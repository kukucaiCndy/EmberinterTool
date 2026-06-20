#include "terminal_input.h"
#include <QKeyCombination>

TerminalInput::TerminalInput() = default;

QByteArray TerminalInput::handleKey(QKeyEvent* event)
{
    // 仅对修饰键屏蔽自动重复, 方向键等应放行以支持按住连续移动
    if (event->isAutoRepeat()) {
        int key = event->key();
        if (key == Qt::Key_Shift || key == Qt::Key_Control ||
            key == Qt::Key_Alt || key == Qt::Key_Meta ||
            key == Qt::Key_CapsLock || key == Qt::Key_NumLock) {
            return {};
        }
    }

    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    // --- 功能键 ---

    // F1-F12
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        int n = key - Qt::Key_F1;
        static const char* fKeysNormal[] = {
            "\033OP", "\033OQ", "\033OR", "\033OS",          // F1-F4
            "\033[15~", "\033[17~", "\033[18~", "\033[19~",  // F5-F8
            "\033[20~", "\033[21~", "\033[23~", "\033[24~",  // F9-F12
        };
        static const char* fKeysModified[] = {
            "\033[1;2P", "\033[1;2Q", "\033[1;2R", "\033[1;2S",
            "\033[15;2~", "\033[17;2~", "\033[18;2~", "\033[19;2~",
            "\033[20;2~", "\033[21;2~", "\033[23;2~", "\033[24;2~",
        };
        if (mods & Qt::ShiftModifier) return fKeysModified[n];
        return fKeysNormal[n];
    }

    // 方向键
    if (key == Qt::Key_Up || key == Qt::Key_Down ||
        key == Qt::Key_Left || key == Qt::Key_Right) {
        QByteArray seq;
        if (appCursorKeys_) {
            seq = "\033O";
        } else {
            seq = "\033[";
        }
        int modNum = 1;
        if (mods & Qt::ShiftModifier) modNum = 2;
        else if (mods & Qt::AltModifier) modNum = 3;
        else if (mods & Qt::ControlModifier) modNum = 5;

        char dir = 'A';
        if (key == Qt::Key_Up) dir = 'A';
        else if (key == Qt::Key_Down) dir = 'B';
        else if (key == Qt::Key_Right) dir = 'C';
        else if (key == Qt::Key_Left) dir = 'D';

        if (modNum > 1) {
            seq += '1';
            seq += ';';
            seq += QByteArray::number(modNum);
        }
        seq += dir;
        return seq;
    }

    // Home / End
    if (key == Qt::Key_Home) {
        if (appCursorKeys_) return "\033OH";
        if (mods & Qt::ShiftModifier) return "\033[1;2H";
        return "\033[H";
    }
    if (key == Qt::Key_End) {
        if (appCursorKeys_) return "\033OF";
        if (mods & Qt::ShiftModifier) return "\033[1;2F";
        return "\033[F";
    }

    // Insert / Delete
    if (key == Qt::Key_Insert) {
        if (mods & Qt::ShiftModifier) return "\033[2;2~";
        return "\033[2~";
    }
    if (key == Qt::Key_Delete) {
        if (mods & Qt::ShiftModifier) return "\033[3;2~";
        return "\033[3~";
    }

    // Page Up / Page Down
    if (key == Qt::Key_PageUp) {
        if (mods & Qt::ShiftModifier) return "\033[5;2~";
        return "\033[5~";
    }
    if (key == Qt::Key_PageDown) {
        if (mods & Qt::ShiftModifier) return "\033[6;2~";
        return "\033[6~";
    }

    // Enter
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        return "\r";
    }

    // Backspace
    if (key == Qt::Key_Backspace) {
        if (mods & Qt::AltModifier) return "\033\x7f";
        return "\x7f";
    }

    // Tab
    if (key == Qt::Key_Tab) {
        if (mods & Qt::ShiftModifier) return "\033[Z";
        return "\t";
    }

    // Escape
    if (key == Qt::Key_Escape) {
        return "\033";
    }

    // Space
    if (key == Qt::Key_Space) {
        if (mods & Qt::ControlModifier) return "\x00";
        return " ";
    }

    // --- Ctrl + 字母 ---
    if (mods & Qt::ControlModifier) {
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            return QByteArray(1, static_cast<char>(key - Qt::Key_A + 1));
        }
        if (key >= Qt::Key_BracketLeft && key <= Qt::Key_Underscore) {
            // Ctrl+[ \ ] ^ _  => 0x1B-0x1F
            return QByteArray(1, static_cast<char>(key - Qt::Key_BracketLeft + 0x1B));
        }
        // Ctrl+6 => 0x1E
        if (key == Qt::Key_6) return "\x1e";
    }

    // --- Alt + 字符 ---
    if (mods & Qt::AltModifier) {
        QString text = event->text();
        if (!text.isEmpty()) {
            QByteArray data = text.toUtf8();
            return "\033" + data;
        }
    }

    // --- 普通字符 ---
    QString text = event->text();
    if (!text.isEmpty()) {
        return text.toUtf8();
    }

    return {};
}

QByteArray TerminalInput::handleMousePress(QMouseEvent* event, int col, int row)
{
    if (mouseTracking_ == None) return {};

    Qt::MouseButton button = event->button();
    int btn = 0;
    if (button == Qt::LeftButton) btn = 0;
    else if (button == Qt::MiddleButton) btn = 1;
    else if (button == Qt::RightButton) btn = 2;
    else return {};

    lastMouseButton_ = btn;
    return encodeMouse(btn, event->modifiers(), 0, col, row);
}

QByteArray TerminalInput::handleMouseRelease(QMouseEvent* event, int col, int row)
{
    if (mouseTracking_ == None) return {};

    int btn = lastMouseButton_;
    return encodeMouse(btn, event->modifiers(), 2, col, row);
}

QByteArray TerminalInput::handleMouseMove(QMouseEvent* event, int col, int row)
{
    if (mouseTracking_ < Button) return {};
    if (!(event->buttons() & (Qt::LeftButton | Qt::MiddleButton | Qt::RightButton))) return {};

    int btn = 0;
    if (event->buttons() & Qt::LeftButton) btn = 0;
    else if (event->buttons() & Qt::MiddleButton) btn = 1;
    else if (event->buttons() & Qt::RightButton) btn = 2;

    return encodeMouse(btn, event->modifiers(), 1, col, row);
}

QByteArray TerminalInput::wrapPaste(const QByteArray& data) const
{
    if (bracketPaste_) {
        return "\033[200~" + data + "\033[201~";
    }
    return data;
}

QByteArray TerminalInput::encodeMouse(int button, int modifiers, int action, int col, int row)
{
    // col/row 是 0-based, VT 序列中用 1-based
    int c = col + 1;
    int r = row + 1;

    if (mouseEncoding_ == SGR) {
        // SGR 编码: ESC[<Cb;Cx;CyM (按下) 或 ESC[<Cb;Cx;Cym (释放)
        int cb = button + (action == 1 ? 32 : 0); // 拖动加 32
        if (modifiers & Qt::ShiftModifier) cb += 4;
        if (modifiers & Qt::AltModifier) cb += 8;
        if (modifiers & Qt::ControlModifier) cb += 16;

        QByteArray seq = "\033[<" + QByteArray::number(cb) + ";" +
                         QByteArray::number(c) + ";" + QByteArray::number(r);
        if (action == 2) {
            seq += 'm';  // 释放
        } else {
            seq += 'M';  // 按下/移动
        }
        return seq;
    }

    // X10 / Default 编码
    int cb = 0;
    if (action == 0) cb = button;        // 按下
    else if (action == 1) cb = button + 32;  // 拖动
    else if (action == 2) cb = 3;        // 释放

    if (modifiers & Qt::ShiftModifier) cb += 4;
    if (modifiers & Qt::AltModifier) cb += 8;
    if (modifiers & Qt::ControlModifier) cb += 16;

    cb += 32;  // 偏移使可打印
    c += 32;
    r += 32;

    // 限制坐标 <= 255 (X10 编码限制)
    if (c > 255) c = 255;
    if (r > 255) r = 255;

    return QByteArray("\033[M", 3) + static_cast<char>(cb) +
           static_cast<char>(c) + static_cast<char>(r);
}
