#pragma once

#include <QByteArray>
#include <QKeyEvent>
#include <QMouseEvent>

/// 终端输入处理器 - 将键盘/鼠标事件转换为 VT 序列
class TerminalInput {
public:
    /// 鼠标跟踪模式
    enum MouseTracking {
        None = 0,
        X10,        // 基本点击 (ESC[?9h)
        Normal,     // 点击+释放+修饰键 (ESC[?1000h)
        Button,     // 点击+释放+拖动 (ESC[?1002h)
        Any,        // 任何移动 (ESC[?1003h)
    };

    /// 鼠标编码模式
    enum MouseEncoding {
        Default,    // X10 编码
        SGR,        // SGR 编码 (ESC[?1006h)
        URXVT,      // URXVT 编码 (ESC[?1015h)
    };

    TerminalInput();

    /// 处理键盘事件, 返回要发送给 PTY 的数据
    QByteArray handleKey(QKeyEvent* event);

    /// 处理鼠标按下
    QByteArray handleMousePress(QMouseEvent* event, int col, int row);
    /// 处理鼠标释放
    QByteArray handleMouseRelease(QMouseEvent* event, int col, int row);
    /// 处理鼠标移动
    QByteArray handleMouseMove(QMouseEvent* event, int col, int row);

    /// 设置鼠标跟踪模式
    void setMouseTracking(MouseTracking mode) { mouseTracking_ = mode; }
    MouseTracking mouseTracking() const { return mouseTracking_; }

    /// 设置鼠标编码模式
    void setMouseEncoding(MouseEncoding enc) { mouseEncoding_ = enc; }
    MouseEncoding mouseEncoding() const { return mouseEncoding_; }

    /// 设置应用程序光标键模式
    void setApplicationCursorKeys(bool app) { appCursorKeys_ = app; }
    bool applicationCursorKeys() const { return appCursorKeys_; }

    /// 设置应用程序键盘模式
    void setApplicationKeypad(bool app) { appKeypad_ = app; }
    bool applicationKeypad() const { return appKeypad_; }

    /// 设置 Bracket Paste Mode
    void setBracketPasteMode(bool enabled) { bracketPaste_ = enabled; }
    bool bracketPasteMode() const { return bracketPaste_; }

    /// 包装粘贴内容 (如果 bracket paste mode 启用)
    QByteArray wrapPaste(const QByteArray& data) const;

private:
    QByteArray encodeMouse(int button, int modifiers, int action, int col, int row);

    MouseTracking mouseTracking_ = None;
    MouseEncoding mouseEncoding_ = Default;
    bool appCursorKeys_ = false;
    bool appKeypad_ = false;
    bool bracketPaste_ = false;
    int lastMouseButton_ = 0;
};
