#include "terminal_view.h"
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <QClipboard>
#include <QGuiApplication>
#include <QFontMetricsF>
#include <spdlog/spdlog.h>

// --- FBO 渲染器 (在渲染线程执行) ---
///
/// 线程模型 (Qt6 QQuickFramebufferObject):
/// - synchronize(): 在 GUI 线程执行, 渲染线程阻塞等待. 可以安全读取 item 数据.
///   但 OpenGL 上下文不一定当前, 所以不能调用任何 GL 函数!
/// - render(): 在渲染线程执行, OpenGL 上下文当前. 所有 GL 操作必须在这里执行.
///
class TerminalFboRenderer : public QQuickFramebufferObject::Renderer,
                             protected QOpenGLFunctions {
public:
    TerminalFboRenderer()
    {
        // 注意: initializeOpenGLFunctions() 需要 GL 上下文, 不能在构造函数中调用
        // 因为构造函数执行时 GL 上下文可能还不当前
    }

    /// synchronize: 在 GUI 线程执行, 只做数据复制, 不调用任何 GL 函数
    void synchronize(QQuickFramebufferObject* item) override
    {
        view_ = static_cast<TerminalView*>(item);
        auto* view = view_;
        if (!view) return;

        // ── 记录是否需要初始化/重建 ──
        needsGLInit_ = !rendererInitialized_ || view->atlas().needsRebuild();

        // QQuickFramebufferObject 的 FBO 尺寸是物理像素 (已乘 devicePixelRatio),
        // 因此快照/渲染坐标也需要使用物理像素, 避免内容被缩放。
        double dpr = view->window() ? view->window()->devicePixelRatio() : 1.0;
        if (dpr <= 0.0) dpr = 1.0;
        snapshot_.devicePixelRatio = dpr;

        // ── 设置字体参数 (不需要 GL) ──
        if (view->atlas().needsRebuild()) {
            spdlog::info("[FboRenderer::sync] atlas needs rebuild, setting up font: cellSize={}x{} dpr={}",
                         view->cellWidth(), view->cellHeight(), dpr);
            view->atlas().setupFont(view->font(),
                                     static_cast<int>(view->cellWidth() * dpr),
                                     static_cast<int>(view->cellHeight() * dpr));
        }

        // ── 构建快照 ──
        snapshot_.viewportWidth = static_cast<int>(view->width() * dpr);
        snapshot_.viewportHeight = static_cast<int>(view->height() * dpr);
        snapshot_.scrollOffset = static_cast<float>(view->scrollOffset() * dpr);
        snapshot_.scrollbackLines = view->model().buffer().scrollbackSize();
        snapshot_.cursorVisible = view->cursorVisible() && view->cursorBlink()
                                  ? view->cursorBlinkState() : view->cursorVisible();
        snapshot_.cursorStyle = view->cursorStyle();
        snapshot_.cellWidth = static_cast<float>(view->cellWidth() * dpr);
        snapshot_.cellHeight = static_cast<float>(view->cellHeight() * dpr);
        snapshot_.cursorRow = view->model().buffer().cursorRow();
        snapshot_.cursorCol = view->model().buffer().cursorCol();

        // 选区信息 (转换为全局行索引)
        const auto& sel = view->model().buffer().selection();
        snapshot_.hasSelection = sel.active;
        if (sel.active) {
            int sb = view->model().buffer().scrollbackSize();
            int sLine = sb + sel.startRow;
            int eLine = sb + sel.endRow;
            int sCol = sel.startCol;
            int eCol = sel.endCol;
            // 确保 start 在 end 之前
            if (sLine > eLine || (sLine == eLine && sCol > eCol)) {
                std::swap(sLine, eLine);
                std::swap(sCol, eCol);
            }
            snapshot_.selStartLine = sLine;
            snapshot_.selStartCol = sCol;
            snapshot_.selEndLine = eLine;
            snapshot_.selEndCol = eCol;
        } else {
            snapshot_.selStartLine = -1;
            snapshot_.selStartCol = -1;
            snapshot_.selEndLine = -1;
            snapshot_.selEndCol = -1;
        }

        // 颜色转 ABGR 格式: (A<<24)|(B<<16)|(G<<8)|R
        // 在小端内存上布局为 [R, G, B, A]，与 GL_UNSIGNED_BYTE 兼容
        QColor cc = view->cursorColor();
        snapshot_.cursorColor = (255u << 24) |
                                (static_cast<uint32_t>(cc.blue()) << 16) |
                                (static_cast<uint32_t>(cc.green()) << 8) |
                                static_cast<uint32_t>(cc.red());
        QColor bg = view->backgroundColor();
        snapshot_.defaultBg = (255u << 24) |
                              (static_cast<uint32_t>(bg.blue()) << 16) |
                              (static_cast<uint32_t>(bg.green()) << 8) |
                              static_cast<uint32_t>(bg.red());

        // ── 复制行数据 + 预取字形 (不需要 GL) ──
        // 只复制可见行范围 (包括 scrollback), 避免全量复制
        const auto& buffer = view->model().buffer();
        const auto& palette = view->model().palette();
        int numCols = buffer.columns();
        float ch = snapshot_.cellHeight;
        int totalLines = buffer.scrollbackSize() + buffer.rows();

        // 计算可见行范围 (全局行索引)
        int firstVisible = static_cast<int>(snapshot_.scrollOffset / ch);
        int lastVisible = static_cast<int>((snapshot_.scrollOffset + snapshot_.viewportHeight) / ch) + 1;
        firstVisible = std::max(0, firstVisible);
        lastVisible = std::min(totalLines - 1, lastVisible);

        int visibleCount = lastVisible - firstVisible + 1;
        if (visibleCount <= 0) visibleCount = 0;

        snapshot_.firstLineIdx = firstVisible;
        snapshot_.rows.resize(visibleCount);

        // 辅助函数: 获取全局行索引对应的行数据
        auto getLine = [&](int lineIdx) -> const std::vector<TerminalCell>* {
            int sb = buffer.scrollbackSize();
            if (lineIdx < sb) {
                return &buffer.scrollback()[lineIdx];
            }
            int screenRow = lineIdx - sb;
            if (screenRow < buffer.rows()) {
                return &buffer.line(screenRow);
            }
            return nullptr;
        };

        for (int i = 0; i < visibleCount; i++) {
            int lineIdx = firstVisible + i;
            const auto* srcRow = getLine(lineIdx);
            auto& dstRow = snapshot_.rows[i];
            dstRow.cells.resize(numCols);
            if (!srcRow) continue;

            for (int c = 0; c < numCols; c++) {
                const auto& cell = (c < static_cast<int>(srcRow->size())) ? srcRow->at(c) : TerminalCell{};
                auto& dst = dstRow.cells[c];
                dst.ch = cell.ch;
                dst.width = cell.width;
                dst.fgColor = palette.cellFg(cell);
                dst.bgColor = palette.cellBg(cell);
                dst.invisible = cell.invisible;
                dst.bold = cell.bold;
                dst.italic = cell.italic;
                dst.inverse = cell.inverse;
                dst.u0 = dst.v0 = dst.u1 = dst.v1 = 0;

                // 预取字形并缓存纹理坐标 (光栅化到 QImage, 不需要 GL)
                if (cell.width != CellWidth::Cont &&
                    cell.ch != ' ' && cell.ch != 0 && !cell.invisible) {
                    GlyphKey key;
                    key.ch = cell.ch;
                    key.bold = cell.bold;
                    key.italic = cell.italic;
                    key.wide = (cell.width == CellWidth::Wide);

                    const GlyphInfo* glyph = view->atlas().getGlyph(key);
                    if (glyph) {
                        dst.u0 = glyph->u0;
                        dst.v0 = glyph->v0;
                        dst.u1 = glyph->u1;
                        dst.v1 = glyph->v1;
                    }
                }
            }
        }

        // ── 保存 atlas 指针 (render 中用于上传纹理) ──
        atlasPtr_ = &view->atlas();
    }

    /// render: 在渲染线程执行, OpenGL 上下文当前, 所有 GL 操作在这里
    void render() override
    {
        // 首次调用时初始化 GL 函数
        if (!glFunctionsInitialized_) {
            initializeOpenGLFunctions();
            glFunctionsInitialized_ = true;
            spdlog::info("[FboRenderer] GL functions initialized");
        }

        // ── 初始化/重建 GL 资源 ──
        if (needsGLInit_ && atlasPtr_) {
            spdlog::info("[FboRenderer] initGL: atlas needsRebuild={}, cellSize={}x{}",
                         atlasPtr_->needsRebuild(), atlasPtr_->cellWidth(), atlasPtr_->cellHeight());
            atlasPtr_->initGL(this);
            renderer_.setFontMetrics(atlasPtr_->cellWidth(), atlasPtr_->cellHeight());
            if (!rendererInitialized_) {
                renderer_.init(this);
                rendererInitialized_ = true;
                spdlog::info("[FboRenderer] renderer initialized, textureId={}", atlasPtr_->textureId());
            }
            needsGLInit_ = false;
        }

        if (!rendererInitialized_) {
            spdlog::warn("[FboRenderer] render called but renderer not initialized");
            return;
        }

        // ── 上传待上传的字形纹理 ──
        if (atlasPtr_ && atlasPtr_->hasPendingUploads()) {
            atlasPtr_->uploadPending(this);
        }

        // ── 设置视口为 FBO 尺寸 ──
        glViewport(0, 0, snapshot_.viewportWidth, snapshot_.viewportHeight);

        // ── 清除背景 ──
        // ABGR 格式: (A<<24)|(B<<16)|(G<<8)|R
        float bgr = (snapshot_.defaultBg & 0xFF) / 255.0f;
        float bgg = ((snapshot_.defaultBg >> 8) & 0xFF) / 255.0f;
        float bgb = ((snapshot_.defaultBg >> 16) & 0xFF) / 255.0f;
        float bga = ((snapshot_.defaultBg >> 24) & 0xFF) / 255.0f;
        glClearColor(bgr, bgg, bgb, bga);
        glClear(GL_COLOR_BUFFER_BIT);

        // ── 渲染终端内容 ──
        GLuint texId = atlasPtr_ ? atlasPtr_->textureId() : 0;
        renderer_.render(this, snapshot_, texId);

        // 重置 OpenGL 状态 (QML 场景图要求)
        glDisable(GL_BLEND);
    }

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override
    {
        double dpr = (view_ && view_->window()) ? view_->window()->devicePixelRatio() : 1.0;
        spdlog::info("[FboRenderer] createFBO requested={}x{} viewLogical={}x{} dpr={}",
                     size.width(), size.height(),
                     view_ ? static_cast<int>(view_->width()) : -1,
                     view_ ? static_cast<int>(view_->height()) : -1,
                     dpr);
        return new QOpenGLFramebufferObject(size);
    }

private:
    TerminalView* view_ = nullptr;
    bool glFunctionsInitialized_ = false;
    bool rendererInitialized_ = false;
    bool needsGLInit_ = true;
    TerminalSnapshot snapshot_;
    TerminalRenderer renderer_;
    GlyphAtlas* atlasPtr_ = nullptr;  // 非拥有指针, 仅在 render 中使用
};

// --- TerminalView ---

TerminalView::TerminalView(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
{
    setFlag(ItemHasContents, true);
    setFlag(ItemIsFocusScope, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    // FBO 坐标系由着色器处理翻转

    font_ = QFont(fontFamily_, fontSize_);
    font_.setStyleHint(QFont::Monospace);
    font_.setHintingPreference(QFont::PreferFullHinting);
    font_.setStyleStrategy(QFont::PreferAntialias);

    recalcCellSize();

    // 初始化调色板默认颜色 (与 fgColor_/bgColor_ 保持一致)
    model_.palette().defaultFg = (255u << 24) |
                                 (static_cast<uint32_t>(fgColor_.blue()) << 16) |
                                 (static_cast<uint32_t>(fgColor_.green()) << 8) |
                                 static_cast<uint32_t>(fgColor_.red());
    model_.palette().defaultBg = (255u << 24) |
                                 (static_cast<uint32_t>(bgColor_.blue()) << 16) |
                                 (static_cast<uint32_t>(bgColor_.green()) << 8) |
                                 static_cast<uint32_t>(bgColor_.red());

    // 光标闪烁定时器
    cursorBlinkTimer_.setInterval(530);
    connect(&cursorBlinkTimer_, &QTimer::timeout, this, &TerminalView::onCursorBlink);
    cursorBlinkTimer_.start();

    // 模型信号
    connect(&model_, &TerminalModel::contentChanged, this, &TerminalView::onContentChanged);
    connect(&model_, &TerminalModel::shellStarted, this, &TerminalView::shellStarted);
    connect(&model_, &TerminalModel::shellExited, this, &TerminalView::shellExited);
    connect(&model_, &TerminalModel::titleChanged, this, &TerminalView::titleChanged);
    connect(&model_, &TerminalModel::bell, this, &TerminalView::bell);
}

TerminalView::~TerminalView() = default;

QQuickFramebufferObject::Renderer* TerminalView::createRenderer() const
{
    return new TerminalFboRenderer();
}

void TerminalView::setFontFamily(const QString& f)
{
    if (fontFamily_ == f) return;
    fontFamily_ = f;
    font_.setFamily(f);
    font_.setStyleHint(QFont::Monospace);
    font_.setHintingPreference(QFont::PreferFullHinting);
    font_.setStyleStrategy(QFont::PreferAntialias);
    recalcCellSize();
    updateTerminalSize();
    atlas_.markNeedsRebuild();
    emit fontFamilyChanged();
}

void TerminalView::setFontSize(int s)
{
    if (fontSize_ == s) return;
    fontSize_ = s;
    font_.setPointSize(s);
    recalcCellSize();
    updateTerminalSize();
    atlas_.markNeedsRebuild();
    emit fontSizeChanged();
}

void TerminalView::setForegroundColor(const QColor& c)
{
    if (fgColor_ == c) return;
    fgColor_ = c;
    model_.palette().defaultFg = (255u << 24) |
                                 (static_cast<uint32_t>(c.blue()) << 16) |
                                 (static_cast<uint32_t>(c.green()) << 8) |
                                 static_cast<uint32_t>(c.red());
    update();
    emit fgColorChanged();
}

void TerminalView::setBackgroundColor(const QColor& c)
{
    if (bgColor_ == c) return;
    bgColor_ = c;
    model_.palette().defaultBg = (255u << 24) |
                                 (static_cast<uint32_t>(c.blue()) << 16) |
                                 (static_cast<uint32_t>(c.green()) << 8) |
                                 static_cast<uint32_t>(c.red());
    update();
    emit bgColorChanged();
}

void TerminalView::setCursorColor(const QColor& c)
{
    if (cursorColor_ == c) return;
    cursorColor_ = c;
    update();
    emit cursorColorChanged();
}

void TerminalView::setCursorStyle(int s)
{
    if (cursorStyle_ == s) return;
    cursorStyle_ = s;
    update();
    emit cursorStyleChanged();
}

void TerminalView::setCursorBlink(bool b)
{
    if (cursorBlink_ == b) return;
    cursorBlink_ = b;
    if (b) cursorBlinkTimer_.start();
    else { cursorBlinkTimer_.stop(); cursorBlinkState_ = true; }
    emit cursorBlinkChanged();
}

void TerminalView::setCursorVisible(bool v)
{
    if (cursorVisible_ == v) return;
    cursorVisible_ = v;
    update();
    emit cursorVisibleChanged();
}

void TerminalView::setShellName(const QString& name)
{
    if (shellName_ == name) return;
    shellName_ = name;
    emit shellNameChanged();
}

void TerminalView::setScrollOffset(int offset)
{
    int maxOff = maxScrollOffset();
    offset = std::clamp(offset, 0, maxOff);
    if (scrollOffset_ == offset) return;
    scrollOffset_ = offset;
    autoScroll_ = (offset >= maxOff - static_cast<int>(cellHeight_));
    update();
    emit scrollOffsetChanged();
}

int TerminalView::maxScrollOffset() const
{
    int totalLines = model_.buffer().scrollbackSize() + model_.buffer().rows();
    int maxOff = totalLines * static_cast<int>(cellHeight_) - static_cast<int>(height());
    return std::max(0, maxOff);
}

void TerminalView::startShell(const QString& program, const QStringList& args)
{
    model_.startShell(program, args);
}

void TerminalView::writeInput(const QByteArray& data)
{
    model_.writeToPty(data);
}

void TerminalView::terminate()
{
    model_.terminate();
}

bool TerminalView::isRunning() const
{
    return model_.isRunning();
}

void TerminalView::keyPressEvent(QKeyEvent* event)
{
    // 快捷键拦截
    if (event->modifiers() & Qt::ControlModifier &&
        event->modifiers() & Qt::ShiftModifier) {
        if (event->key() == Qt::Key_C) {
            QString text = model_.buffer().selectedText();
            if (!text.isEmpty()) {
                QGuiApplication::clipboard()->setText(text);
            }
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_V) {
            QString text = QGuiApplication::clipboard()->text();
            if (!text.isEmpty()) {
                QByteArray data = text.toUtf8();
                data = model_.input().wrapPaste(data);
                model_.writeToPty(data);
            }
            event->accept();
            return;
        }
    }

    model_.handleKeyEvent(event);
    event->accept();
}

void TerminalView::keyReleaseEvent(QKeyEvent* event)
{
    event->accept();
}

void TerminalView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF localPos = mapFromScene(event->scenePosition());
        int col = pixelToCol(localPos.x());
        int row = pixelToRow(localPos.y());
        spdlog::debug("[MousePress] scene=({:.1f},{:.1f}) local=({:.1f},{:.1f}) col={} row={} scrollOffset={} scrollback={}",
                      event->scenePosition().x(), event->scenePosition().y(),
                      localPos.x(), localPos.y(), col, row, scrollOffset_,
                      model_.buffer().scrollbackSize());
        QPointF sceneTopLeft = mapToScene(QPointF(0, 0));
        spdlog::debug("[MousePress] item geom x={} y={} w={} h={} sceneTopLeft=({},{})",
                      x(), y(), width(), height(), sceneTopLeft.x(), sceneTopLeft.y());

        if (model_.input().mouseTracking() != TerminalInput::None) {
            model_.handleMousePress(event, col, row);
            return;
        }

        selecting_ = true;
        selStartCol_ = col;
        selStartRow_ = row;
        auto& sel = model_.buffer().selection();
        sel.startCol = col;
        sel.startRow = row;
        sel.endCol = col;
        sel.endRow = row;
        sel.active = true;
        update();
    }

    event->accept();
}

void TerminalView::mouseMoveEvent(QMouseEvent* event)
{
    QPointF localPos = mapFromScene(event->scenePosition());
    int col = pixelToCol(localPos.x());
    int row = pixelToRow(localPos.y());

    if (model_.input().mouseTracking() != TerminalInput::None) {
        model_.handleMouseMove(event, col, row);
        return;
    }

    if (selecting_) {
        auto& sel = model_.buffer().selection();
        sel.endCol = col;
        sel.endRow = row;
        update();
    }

    event->accept();
}

void TerminalView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF localPos = mapFromScene(event->scenePosition());
        int col = pixelToCol(localPos.x());
        int row = pixelToRow(localPos.y());

        if (model_.input().mouseTracking() != TerminalInput::None) {
            model_.handleMouseRelease(event, col, row);
            return;
        }

        if (selecting_) {
            auto& sel = model_.buffer().selection();
            sel.endCol = col;
            sel.endRow = row;

            QString text = model_.buffer().selectedText();
            if (!text.isEmpty()) {
                QGuiApplication::clipboard()->setText(text);
            }

            selecting_ = false;
            update();
        }
    }

    event->accept();
}

void TerminalView::wheelEvent(QWheelEvent* event)
{
    int delta = event->angleDelta().y();
    int scrollLines = delta / 120 * 3;

    setScrollOffset(scrollOffset_ - scrollLines * static_cast<int>(cellHeight_));

    event->accept();
}

void TerminalView::geometryChange(const QRectF& newGeom, const QRectF& oldGeom)
{
    QQuickFramebufferObject::geometryChange(newGeom, oldGeom);
    if (newGeom.size() != oldGeom.size()) {
        updateTerminalSize();
    }
}

void TerminalView::focusInEvent(QFocusEvent* event)
{
    QQuickFramebufferObject::focusInEvent(event);
}

void TerminalView::focusOutEvent(QFocusEvent* event)
{
    QQuickFramebufferObject::focusOutEvent(event);
}

void TerminalView::onContentChanged()
{
    if (autoScroll_) {
        setScrollOffset(maxScrollOffset());
    }
    update();
}

void TerminalView::onCursorBlink()
{
    cursorBlinkState_ = !cursorBlinkState_;
    update();
}

void TerminalView::recalcCellSize()
{
    QFontMetricsF fm(font_);
    cellWidth_ = fm.horizontalAdvance('W');
    cellHeight_ = fm.height();
}

void TerminalView::updateTerminalSize()
{
    if (width() <= 0 || height() <= 0) return;

    int cols = static_cast<int>(width() / cellWidth_);
    int rows = static_cast<int>(height() / cellHeight_);
    cols = std::max(1, cols);
    rows = std::max(1, rows);

    if (cols != model_.buffer().columns() || rows != model_.buffer().rows()) {
        model_.resize(cols, rows);
        emit columnsChanged();
        emit rowsChanged();
        emit maxScrollOffsetChanged();
    }
}

int TerminalView::pixelToRow(int y) const
{
    int row = (y + scrollOffset_) / static_cast<int>(cellHeight_);
    row -= model_.buffer().scrollbackSize();
    return std::clamp(row, 0, model_.buffer().rows() - 1);
}

int TerminalView::pixelToCol(int x) const
{
    int col = static_cast<int>(x / cellWidth_);
    return std::clamp(col, 0, model_.buffer().columns() - 1);
}
