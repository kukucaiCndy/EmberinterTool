#pragma once

#include <QQuickFramebufferObject>
#include <QTimer>
#include <QFont>
#include <QColor>

#include "terminal_model.h"
#include "terminal_renderer.h"
#include "glyph_atlas.h"

/// 终端 QML 控件 - 使用 QQuickFramebufferObject 实现 GPU 加速渲染
class TerminalView : public QQuickFramebufferObject {
    Q_OBJECT

    Q_PROPERTY(int columns READ columns NOTIFY columnsChanged)
    Q_PROPERTY(int rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY fgColorChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY bgColorChanged)
    Q_PROPERTY(QColor cursorColor READ cursorColor WRITE setCursorColor NOTIFY cursorColorChanged)
    Q_PROPERTY(int cursorStyle READ cursorStyle WRITE setCursorStyle NOTIFY cursorStyleChanged)
    Q_PROPERTY(bool cursorBlink READ cursorBlink WRITE setCursorBlink NOTIFY cursorBlinkChanged)
    Q_PROPERTY(bool cursorVisible READ cursorVisible WRITE setCursorVisible NOTIFY cursorVisibleChanged)
    Q_PROPERTY(QString shellName READ shellName WRITE setShellName NOTIFY shellNameChanged)
    Q_PROPERTY(int scrollOffset READ scrollOffset WRITE setScrollOffset NOTIFY scrollOffsetChanged)
    Q_PROPERTY(int maxScrollOffset READ maxScrollOffset NOTIFY maxScrollOffsetChanged)

public:
    explicit TerminalView(QQuickItem* parent = nullptr);
    ~TerminalView() override;

    /// QQuickFramebufferObject: 创建 FBO 渲染器
    Renderer* createRenderer() const override;

    // 属性访问器
    int columns() const { return model_.buffer().columns(); }
    int rows() const { return model_.buffer().rows(); }

    QString fontFamily() const { return fontFamily_; }
    void setFontFamily(const QString& f);
    int fontSize() const { return fontSize_; }
    void setFontSize(int s);

    QColor foregroundColor() const { return fgColor_; }
    void setForegroundColor(const QColor& c);
    QColor backgroundColor() const { return bgColor_; }
    void setBackgroundColor(const QColor& c);
    QColor cursorColor() const { return cursorColor_; }
    void setCursorColor(const QColor& c);
    int cursorStyle() const { return cursorStyle_; }
    void setCursorStyle(int s);
    bool cursorBlink() const { return cursorBlink_; }
    void setCursorBlink(bool b);
    bool cursorVisible() const { return cursorVisible_; }
    void setCursorVisible(bool v);
    bool cursorBlinkState() const { return cursorBlinkState_; }

    QString shellName() const { return shellName_; }
    void setShellName(const QString& name);

    int scrollOffset() const { return scrollOffset_; }
    void setScrollOffset(int offset);
    int maxScrollOffset() const;

    // 进程管理
    Q_INVOKABLE void startShell(const QString& program, const QStringList& args = {});
    Q_INVOKABLE void startShellWithEnv(const QString& program, const QStringList& args,
                                       const QVariantMap& env);
    Q_INVOKABLE void writeInput(const QByteArray& data);
    Q_INVOKABLE void terminate();
    Q_INVOKABLE bool isRunning() const;
    Q_INVOKABLE void setColorScheme(const QString& name);

    /// 获取终端模型 (供 synchronize 使用)
    TerminalModel& model() { return model_; }
    const TerminalModel& model() const { return model_; }

    /// 是否已销毁 (供 renderer 检查)
    bool isDestroyed() const { return destroyed_; }

    GlyphAtlas& atlas() { return atlas_; }

    QFont font() const { return font_; }
    qreal cellWidth() const { return cellWidth_; }
    qreal cellHeight() const { return cellHeight_; }

signals:
    void columnsChanged();
    void rowsChanged();
    void fontFamilyChanged();
    void fontSizeChanged();
    void fgColorChanged();
    void bgColorChanged();
    void cursorColorChanged();
    void cursorStyleChanged();
    void cursorBlinkChanged();
    void cursorVisibleChanged();
    void shellNameChanged();
    void scrollOffsetChanged();
    void maxScrollOffsetChanged();
    void shellStarted();
    void shellExited(int exitCode);
    void titleChanged(const QString& title);
    void bell();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void geometryChange(const QRectF& newGeom, const QRectF& oldGeom) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private slots:
    void onContentChanged();
    void onCursorBlink();

private:
    void recalcCellSize();
    void updateTerminalSize();
    int pixelToRow(int y) const;
    int pixelToCol(int x) const;

    // 模型
    TerminalModel model_;

    // 渲染组件
    GlyphAtlas atlas_;

    // 字体
    QString fontFamily_ = "Cascadia Code";
    int fontSize_ = 14;
    QFont font_;
    qreal cellWidth_ = 8;
    qreal cellHeight_ = 16;

    // 颜色 (MSYS2 暗色终端默认: 浅灰前景 + 黑色背景)
    QColor fgColor_ = QColor(0xCC, 0xCC, 0xCC);
    QColor bgColor_ = QColor(0x00, 0x00, 0x00);
    QColor cursorColor_ = QColor(0xF5, 0xE0, 0xDC);

    // 光标
    int cursorStyle_ = 0;  // 0=Block, 1=Underline, 2=Bar
    bool cursorBlink_ = true;
    bool cursorVisible_ = true;
    bool cursorBlinkState_ = true;
    QTimer cursorBlinkTimer_;

    // 滚动
    int scrollOffset_ = 0;

    // Shell
    QString shellName_;

    // 鼠标选择
    bool selecting_ = false;
    int selStartCol_ = -1, selStartRow_ = -1;

    // 自动滚动到底部
    bool autoScroll_ = true;

    // 标记 view 是否已进入析构
    bool destroyed_ = false;
};
