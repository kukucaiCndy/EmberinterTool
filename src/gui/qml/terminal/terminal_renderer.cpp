#include "terminal_renderer.h"
#include <QOpenGLContext>

// --- 背景着色器 ---
static const char* bgVertexShader = R"(
attribute vec2 aPosition;
attribute vec4 aColor;
uniform vec2 uResolution;
varying vec4 vColor;
void main() {
    vec2 clip = (aPosition / uResolution) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, clip.y, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* bgFragmentShader = R"(
varying vec4 vColor;
void main() {
    gl_FragColor = vColor;
}
)";

// --- 文本着色器 ---
static const char* textVertexShader = R"(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
attribute vec4 aFgColor;
uniform vec2 uResolution;
varying vec2 vTexCoord;
varying vec4 vFgColor;
void main() {
    vec2 clip = (aPosition / uResolution) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, clip.y, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vFgColor = aFgColor;
}
)";

static const char* textFragmentShader = R"(
uniform sampler2D uAtlas;
varying vec2 vTexCoord;
varying vec4 vFgColor;
void main() {
    float alpha = texture2D(uAtlas, vTexCoord).a;
    gl_FragColor = vec4(vFgColor.rgb, vFgColor.a * alpha);
}
)";

TerminalRenderer::TerminalRenderer() = default;

TerminalRenderer::~TerminalRenderer()
{
    // OpenGL 资源应在 cleanup() 中释放
}

void TerminalRenderer::init(QOpenGLFunctions* gl)
{
    if (initialized_) return;

    // 编译背景着色器
    bgProgram_ = new QOpenGLShaderProgram();
    bgProgram_->addShaderFromSourceCode(QOpenGLShader::Vertex, bgVertexShader);
    bgProgram_->addShaderFromSourceCode(QOpenGLShader::Fragment, bgFragmentShader);
    bgProgram_->link();

    // 编译文本着色器
    textProgram_ = new QOpenGLShaderProgram();
    textProgram_->addShaderFromSourceCode(QOpenGLShader::Vertex, textVertexShader);
    textProgram_->addShaderFromSourceCode(QOpenGLShader::Fragment, textFragmentShader);
    textProgram_->link();

    // 创建 VBO
    vbo_.create();
    vbo_.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    initialized_ = true;
}

void TerminalRenderer::cleanup(QOpenGLFunctions* gl)
{
    if (!initialized_) return;

    delete bgProgram_;
    bgProgram_ = nullptr;
    delete textProgram_;
    textProgram_ = nullptr;

    if (vbo_.isCreated()) {
        vbo_.destroy();
    }

    initialized_ = false;
}

void TerminalRenderer::setFontMetrics(int cellWidth, int cellHeight)
{
    cellWidth_ = cellWidth;
    cellHeight_ = cellHeight;
}

void TerminalRenderer::render(QOpenGLFunctions* gl,
                               const TerminalSnapshot& snapshot,
                               GLuint atlasTextureId)
{
    if (!initialized_) return;

    // 构建顶点数据
    bgVertices_.clear();
    textVertices_.clear();
    cursorVertices_.clear();

    buildBackgroundVertices(snapshot);
    buildTextVertices(snapshot);

    if (snapshot.cursorVisible) {
        buildCursorVertices(snapshot);
    }

    // 启用混合
    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 绘制背景
    drawBackgroundBatch(gl, snapshot.viewportWidth, snapshot.viewportHeight);

    // 绘制文本
    drawTextBatch(gl, snapshot.viewportWidth, snapshot.viewportHeight, atlasTextureId);

    // 绘制光标
    if (snapshot.cursorVisible && !cursorVertices_.empty()) {
        drawCursorBatch(gl, snapshot.viewportWidth, snapshot.viewportHeight);
    }

    gl->glDisable(GL_BLEND);
}

void TerminalRenderer::buildBackgroundVertices(const TerminalSnapshot& snapshot)
{
    float pixelOffset = snapshot.scrollOffset;
    float cw = snapshot.cellWidth;
    float ch = snapshot.cellHeight;
    int numRows = static_cast<int>(snapshot.rows.size());

    // 选区高亮颜色 (半透明白色)
    uint32_t selColor = 0x40FFFFFF;  // ABGR: A=64, B=255, G=255, R=255

    // snapshot.rows 只包含可见行, firstLineIdx 是第一行的全局行索引
    for (int i = 0; i < numRows; i++) {
        int lineIdx = snapshot.firstLineIdx + i;
        const auto& row = snapshot.rows[i];
        float y = lineIdx * ch - pixelOffset;
        // 最后一行选区高亮延伸到 viewport 底部, 避免高度不是 cellHeight 整数倍时
        // 点击底部空白无法看到选区反馈。
        float selectionRowHeight = (i == numRows - 1)
            ? std::max(ch, static_cast<float>(snapshot.viewportHeight) - y)
            : ch;

        for (int col = 0; col < static_cast<int>(row.cells.size()); col++) {
            const auto& cell = row.cells[col];
            if (cell.width == CellWidth::Cont) continue;

            // 检查是否在选区内
            bool inSelection = false;
            if (snapshot.hasSelection) {
                if (lineIdx > snapshot.selStartLine && lineIdx < snapshot.selEndLine) {
                    inSelection = true;  // 中间行全选
                } else if (lineIdx == snapshot.selStartLine && lineIdx == snapshot.selEndLine) {
                    inSelection = (col >= snapshot.selStartCol && col <= snapshot.selEndCol);
                } else if (lineIdx == snapshot.selStartLine) {
                    inSelection = (col >= snapshot.selStartCol);
                } else if (lineIdx == snapshot.selEndLine) {
                    inSelection = (col <= snapshot.selEndCol);
                }
            }

            uint32_t bg = cell.bgColor;
            bool drawBg = (bg != snapshot.defaultBg || cell.inverse);

            if (inSelection) {
                // 选区: 绘制选区高亮背景
                float x = col * cw;
                float w = cw * (cell.width == CellWidth::Wide ? 2.0f : 1.0f);
                float h = selectionRowHeight;

                Vertex v0 = {x,     y,     0, 0, selColor, 0};
                Vertex v1 = {x + w, y,     0, 0, selColor, 0};
                Vertex v2 = {x,     y + h, 0, 0, selColor, 0};
                Vertex v3 = {x + w, y,     0, 0, selColor, 0};
                Vertex v4 = {x + w, y + h, 0, 0, selColor, 0};
                Vertex v5 = {x,     y + h, 0, 0, selColor, 0};

                bgVertices_.push_back(v0);
                bgVertices_.push_back(v1);
                bgVertices_.push_back(v2);
                bgVertices_.push_back(v3);
                bgVertices_.push_back(v4);
                bgVertices_.push_back(v5);
            } else if (drawBg) {
                // 非选区: 绘制单元格背景色
                float x = col * cw;
                float w = cw * (cell.width == CellWidth::Wide ? 2.0f : 1.0f);
                float h = ch;

                Vertex v0 = {x,     y,     0, 0, bg, 0};
                Vertex v1 = {x + w, y,     0, 0, bg, 0};
                Vertex v2 = {x,     y + h, 0, 0, bg, 0};
                Vertex v3 = {x + w, y,     0, 0, bg, 0};
                Vertex v4 = {x + w, y + h, 0, 0, bg, 0};
                Vertex v5 = {x,     y + h, 0, 0, bg, 0};

                bgVertices_.push_back(v0);
                bgVertices_.push_back(v1);
                bgVertices_.push_back(v2);
                bgVertices_.push_back(v3);
                bgVertices_.push_back(v4);
                bgVertices_.push_back(v5);
            }
        }
    }
}

void TerminalRenderer::buildTextVertices(const TerminalSnapshot& snapshot)
{
    float pixelOffset = snapshot.scrollOffset;
    float cw = snapshot.cellWidth;
    float ch = snapshot.cellHeight;
    int numRows = static_cast<int>(snapshot.rows.size());

    for (int i = 0; i < numRows; i++) {
        int lineIdx = snapshot.firstLineIdx + i;
        const auto& row = snapshot.rows[i];
        float y = lineIdx * ch - pixelOffset;

        for (int col = 0; col < static_cast<int>(row.cells.size()); col++) {
            const auto& cell = row.cells[col];
            if (cell.width == CellWidth::Cont) continue;
            if (cell.ch == ' ' || cell.ch == 0 || cell.invisible) continue;

            float x = col * cw;
            float w = cw * (cell.width == CellWidth::Wide ? 2.0f : 1.0f);
            float h = ch;

            uint32_t fg = cell.fgColor;

            // 使用预计算的纹理坐标
            Vertex v0 = {x,     y,     cell.u0, cell.v0, fg, 0};
            Vertex v1 = {x + w, y,     cell.u1, cell.v0, fg, 0};
            Vertex v2 = {x,     y + h, cell.u0, cell.v1, fg, 0};
            Vertex v3 = {x + w, y,     cell.u1, cell.v0, fg, 0};
            Vertex v4 = {x + w, y + h, cell.u1, cell.v1, fg, 0};
            Vertex v5 = {x,     y + h, cell.u0, cell.v1, fg, 0};

            textVertices_.push_back(v0);
            textVertices_.push_back(v1);
            textVertices_.push_back(v2);
            textVertices_.push_back(v3);
            textVertices_.push_back(v4);
            textVertices_.push_back(v5);
        }
    }
}

void TerminalRenderer::buildCursorVertices(const TerminalSnapshot& snapshot)
{
    int cursorLine = snapshot.scrollbackLines + snapshot.cursorRow;
    float y = cursorLine * snapshot.cellHeight - snapshot.scrollOffset;
    float x = snapshot.cursorCol * snapshot.cellWidth;

    uint32_t color = snapshot.cursorColor;

    float w, h, yOffset = 0;
    switch (snapshot.cursorStyle) {
    case 0: // Block
        w = snapshot.cellWidth;
        h = snapshot.cellHeight;
        break;
    case 1: // Underline
        w = snapshot.cellWidth;
        h = 2;
        yOffset = snapshot.cellHeight - 2;
        break;
    case 2: // Bar
        w = 2;
        h = snapshot.cellHeight;
        break;
    default:
        w = snapshot.cellWidth;
        h = snapshot.cellHeight;
        break;
    }

    y += yOffset;

    Vertex v0 = {x,     y,     0, 0, color, 0};
    Vertex v1 = {x + w, y,     0, 0, color, 0};
    Vertex v2 = {x,     y + h, 0, 0, color, 0};
    Vertex v3 = {x + w, y,     0, 0, color, 0};
    Vertex v4 = {x + w, y + h, 0, 0, color, 0};
    Vertex v5 = {x,     y + h, 0, 0, color, 0};

    cursorVertices_.push_back(v0);
    cursorVertices_.push_back(v1);
    cursorVertices_.push_back(v2);
    cursorVertices_.push_back(v3);
    cursorVertices_.push_back(v4);
    cursorVertices_.push_back(v5);
}

void TerminalRenderer::drawBackgroundBatch(QOpenGLFunctions* gl, int viewportWidth, int viewportHeight)
{
    if (bgVertices_.empty()) return;

    bgProgram_->bind();
    bgProgram_->setUniformValue("uResolution", QVector2D(viewportWidth, viewportHeight));

    vbo_.bind();
    vbo_.allocate(bgVertices_.data(), bgVertices_.size() * sizeof(Vertex));

    int posLoc = bgProgram_->attributeLocation("aPosition");
    int colorLoc = bgProgram_->attributeLocation("aColor");

    gl->glEnableVertexAttribArray(posLoc);
    gl->glEnableVertexAttribArray(colorLoc);

    gl->glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                               reinterpret_cast<void*>(offsetof(Vertex, x)));
    gl->glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                               reinterpret_cast<void*>(offsetof(Vertex, color)));

    gl->glDrawArrays(GL_TRIANGLES, 0, bgVertices_.size());

    gl->glDisableVertexAttribArray(posLoc);
    gl->glDisableVertexAttribArray(colorLoc);

    vbo_.release();
    bgProgram_->release();
}

void TerminalRenderer::drawTextBatch(QOpenGLFunctions* gl, int viewportWidth, int viewportHeight,
                                      GLuint atlasTextureId)
{
    if (textVertices_.empty()) return;

    // 绑定图集纹理
    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, atlasTextureId);

    textProgram_->bind();
    textProgram_->setUniformValue("uResolution", QVector2D(viewportWidth, viewportHeight));
    textProgram_->setUniformValue("uAtlas", 0);

    vbo_.bind();
    vbo_.allocate(textVertices_.data(), textVertices_.size() * sizeof(Vertex));

    int posLoc = textProgram_->attributeLocation("aPosition");
    int texLoc = textProgram_->attributeLocation("aTexCoord");
    int fgLoc  = textProgram_->attributeLocation("aFgColor");

    gl->glEnableVertexAttribArray(posLoc);
    gl->glEnableVertexAttribArray(texLoc);
    gl->glEnableVertexAttribArray(fgLoc);

    gl->glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                               reinterpret_cast<void*>(offsetof(Vertex, x)));
    gl->glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                               reinterpret_cast<void*>(offsetof(Vertex, u)));
    gl->glVertexAttribPointer(fgLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                               reinterpret_cast<void*>(offsetof(Vertex, color)));

    gl->glDrawArrays(GL_TRIANGLES, 0, textVertices_.size());

    gl->glDisableVertexAttribArray(posLoc);
    gl->glDisableVertexAttribArray(texLoc);
    gl->glDisableVertexAttribArray(fgLoc);

    vbo_.release();
    textProgram_->release();
}

void TerminalRenderer::drawCursorBatch(QOpenGLFunctions* gl, int viewportWidth, int viewportHeight)
{
    if (cursorVertices_.empty()) return;

    bgProgram_->bind();
    bgProgram_->setUniformValue("uResolution", QVector2D(viewportWidth, viewportHeight));

    vbo_.bind();
    vbo_.allocate(cursorVertices_.data(), cursorVertices_.size() * sizeof(Vertex));

    int posLoc = bgProgram_->attributeLocation("aPosition");
    int colorLoc = bgProgram_->attributeLocation("aColor");

    gl->glEnableVertexAttribArray(posLoc);
    gl->glEnableVertexAttribArray(colorLoc);

    gl->glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                               reinterpret_cast<void*>(offsetof(Vertex, x)));
    gl->glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                               reinterpret_cast<void*>(offsetof(Vertex, color)));

    gl->glDrawArrays(GL_TRIANGLES, 0, cursorVertices_.size());

    gl->glDisableVertexAttribArray(posLoc);
    gl->glDisableVertexAttribArray(colorLoc);

    vbo_.release();
    bgProgram_->release();
}
