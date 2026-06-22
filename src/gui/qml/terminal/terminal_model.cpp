#include "terminal_model.h"
#include <QKeyEvent>
#include <QClipboard>
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

TerminalModel::TerminalModel(QObject* parent)
    : QObject(parent)
{
    parser_.setHandler([this](const VtParser::Event& ev) {
        onVtEvent(ev);
    });

    // 帧合并定时器: 16ms (~60fps)
    frameTimer_.setSingleShot(true);
    frameTimer_.setInterval(16);
    connect(&frameTimer_, &QTimer::timeout, this, [this]() {
        if (contentChangedPending_) {
            contentChangedPending_ = false;
            emit contentChanged();
        }
    });
}

TerminalModel::~TerminalModel()
{
    if (pty_) {
        pty_->terminate();
    }
}

void TerminalModel::init(int cols, int rows)
{
    buffer_.resize(cols, rows);
}

bool TerminalModel::startShell(const QString& program, const QStringList& args,
                                const QString& cwd,
                                const QProcessEnvironment& env)
{
    if (pty_) {
        pty_->terminate();
        pty_.reset();
    }

    pty_ = PtyProcess::create();
    connect(pty_.get(), &PtyProcess::readyRead, this, &TerminalModel::onPtyReadyRead);
    connect(pty_.get(), &PtyProcess::finished, this, &TerminalModel::onPtyFinished);
    connect(pty_.get(), &PtyProcess::started, this, &TerminalModel::shellStarted);

    bool ok = pty_->start(program, args, cwd, env);
    if (ok) {
        pty_->resize(buffer_.columns(), buffer_.rows());
    }
    return ok;
}

void TerminalModel::writeToPty(const QByteArray& data)
{
    spdlog::debug("TerminalModel::writeToPty: size={} pty={} running={}",
                  data.size(), (void*)pty_.get(), pty_ ? pty_->isRunning() : false);
    if (pty_ && pty_->isRunning()) {
        pty_->write(data);
        emit dataSent(data);
    }
}

void TerminalModel::handleKeyEvent(QKeyEvent* event)
{
    spdlog::debug("TerminalModel::handleKeyEvent: key={}", event->key());
    QByteArray data = input_.handleKey(event);
    spdlog::debug("TerminalModel::handleKeyEvent: data size={}", data.size());
    if (!data.isEmpty()) {
        writeToPty(data);
    }
}

void TerminalModel::handleMousePress(QMouseEvent* event, int col, int row)
{
    QByteArray data = input_.handleMousePress(event, col, row);
    if (!data.isEmpty()) writeToPty(data);
}

void TerminalModel::handleMouseRelease(QMouseEvent* event, int col, int row)
{
    QByteArray data = input_.handleMouseRelease(event, col, row);
    if (!data.isEmpty()) writeToPty(data);
}

void TerminalModel::handleMouseMove(QMouseEvent* event, int col, int row)
{
    QByteArray data = input_.handleMouseMove(event, col, row);
    if (!data.isEmpty()) writeToPty(data);
}

void TerminalModel::resize(int cols, int rows)
{
    buffer_.resize(cols, rows);
    if (pty_ && pty_->isRunning()) {
        pty_->resize(cols, rows);
    }
    notifyChanged();
}

void TerminalModel::terminate()
{
    if (pty_) {
        pty_->terminate();
    }
}

bool TerminalModel::isRunning() const
{
    return pty_ && pty_->isRunning();
}

void TerminalModel::onPtyReadyRead()
{
    if (!pty_) return;
    QByteArray data = pty_->readAll();
    if (data.isEmpty()) return;

    // 调试: 记录原始 PTY 输出 (转义不可打印字符)
    if (spdlog::default_logger_raw() && spdlog::default_logger_raw()->should_log(spdlog::level::debug)) {
        std::string dbg;
        int limit = std::min<int>(data.size(), 256);
        for (int i = 0; i < limit; ++i) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            if (c == 0x1b) dbg += "<ESC>";
            else if (c == '\r') dbg += "<CR>";
            else if (c == '\n') dbg += "<LF>";
            else if (c == '\t') dbg += "<TAB>";
            else if (c < 0x20) dbg += fmt::format("<{:02X}>", c);
            else dbg += static_cast<char>(c);
        }
        spdlog::debug("[PTY] {} bytes: {}", data.size(), dbg);
    }

    emit dataReceived(data);
    parser_.parse(data.constData(), data.size());
    notifyChanged();
}

void TerminalModel::onPtyFinished(int exitCode)
{
    emit shellExited(exitCode);
}

// --- VT 事件处理 ---

void TerminalModel::onVtEvent(const VtParser::Event& ev)
{
    switch (ev.type) {
    case VtParser::Event::Print:
        handlePrint(ev);
        break;
    case VtParser::Event::Execute:
        handleExecute(ev);
        break;
    case VtParser::Event::CsiDispatch:
        handleCsiDispatch(ev);
        break;
    case VtParser::Event::OscDispatch:
        handleOscDispatch(ev);
        break;
    case VtParser::Event::EscDispatch:
        handleEscDispatch(ev);
        break;
    case VtParser::Event::DcsHook:
    case VtParser::Event::DcsPut:
    case VtParser::Event::DcsUnhook:
        // DCS 暂不处理 (Sixel 等高级功能)
        break;
    }
}

void TerminalModel::handlePrint(const VtParser::Event& ev)
{
    buffer_.writeChar(ev.ch, static_cast<CellWidth>(ev.width));
}

void TerminalModel::handleExecute(const VtParser::Event& ev)
{
    switch (ev.ctrl) {
    case 0x08: // BS
        buffer_.setCursorCol(std::max(0, buffer_.cursorCol() - 1));
        break;
    case 0x09: // HT
        buffer_.setCursorCol(buffer_.nextTabStop(buffer_.cursorCol()));
        break;
    case 0x0A: // LF
        buffer_.lineFeed();
        break;
    case 0x0B: // VT - 同 LF
        buffer_.lineFeed();
        break;
    case 0x0C: // FF - 同 LF
        buffer_.lineFeed();
        break;
    case 0x0D: // CR
        buffer_.carriageReturn();
        break;
    case 0x07: // BEL
        bellRequested_ = true;
        emit bell();
        break;
    default:
        break;
    }
}

void TerminalModel::handleCsiDispatch(const VtParser::Event& ev)
{
    if (ev.csiPrivate) {
        // DEC 私有模式: CSI ? <params> <final>
        for (int p : ev.csiParams) {
            decSetMode(p, ev.csiFinal == 'h');
        }
        return;
    }

    const auto& params = ev.csiParams;

    switch (ev.csiFinal) {
    case 'A': csiCursorUp(paramValue(params, 0, 1)); break;
    case 'B': csiCursorDown(paramValue(params, 0, 1)); break;
    case 'C': csiCursorForward(paramValue(params, 0, 1)); break;
    case 'D': csiCursorBack(paramValue(params, 0, 1)); break;
    case 'E': csiCursorNextLine(paramValue(params, 0, 1)); break;
    case 'F': csiCursorPrevLine(paramValue(params, 0, 1)); break;
    case 'G': csiCursorHorizontalAbsolute(paramValue(params, 0, 1)); break;
    case 'H': case 'f': csiCursorPosition(paramValue(params, 0, 1), paramValue(params, 1, 1)); break;
    case 'd': csiCursorVerticalAbsolute(paramValue(params, 0, 1)); break;
    case 'J': csiEraseInDisplay(paramValue(params, 0, 0)); break;
    case 'K': csiEraseInLine(paramValue(params, 0, 0)); break;
    case 'L': csiInsertLines(paramValue(params, 0, 1)); break;
    case 'M': csiDeleteLines(paramValue(params, 0, 1)); break;
    case '@': csiInsertCharacters(paramValue(params, 0, 1)); break;
    case 'P': csiDeleteCharacters(paramValue(params, 0, 1)); break;
    case 'X': csiEraseCharacters(paramValue(params, 0, 1)); break;
    case 'S': csiScrollUp(paramValue(params, 0, 1)); break;
    case 'T': csiScrollDown(paramValue(params, 0, 1)); break;
    case 'r': csiSetScrollRegion(paramValue(params, 0, 1), paramValue(params, 1, buffer_.rows())); break;
    case 'I': csiTabForward(paramValue(params, 0, 1)); break;
    case 'Z': csiTabBackward(paramValue(params, 0, 1)); break;
    case 's': csiSaveCursor(); break;
    case 'u': csiRestoreCursor(); break;
    case 'm': csiSGR(params); break;
    case 'l': // Reset Mode (非私有)
        if (!params.empty() && params[0] == 4) {
            // 插入模式关闭
        } else if (!params.empty() && params[0] == 20) {
            // LNM 关闭: LF 仅下移光标
            buffer_.setLineFeedNewLineMode(false);
        }
        break;
    case 'h': // Set Mode (非私有)
        if (!params.empty() && params[0] == 4) {
            // 插入模式开启
        } else if (!params.empty() && params[0] == 20) {
            // LNM 开启: LF = CR + LF
            buffer_.setLineFeedNewLineMode(true);
        }
        break;
    case 'n': // Device Status Report
        if (!params.empty() && params[0] == 6) {
            // 报告光标位置: ESC[row;colR
            QByteArray report = "\033[" +
                QByteArray::number(buffer_.cursorRow() + 1) + ";" +
                QByteArray::number(buffer_.cursorCol() + 1) + "R";
            writeToPty(report);
        } else if (!params.empty() && params[0] == 5) {
            // 报告设备状态: OK
            writeToPty("\033[0n");
        }
        break;
    case 'c': // Device Attributes
        if (params.empty() || params[0] == 0) {
            // 报告为 VT100
            writeToPty("\033[?1;0c");
        }
        break;
    default:
        break;
    }
}

void TerminalModel::handleOscDispatch(const VtParser::Event& ev)
{
    switch (ev.oscCommand) {
    case 0:   // 设置窗口标题和图标名
    case 2:   // 设置窗口标题
        if (!ev.oscData.empty()) {
            windowTitle_ = QString::fromUtf8(ev.oscData.c_str(), ev.oscData.size());
            emit titleChanged(windowTitle_);
        }
        break;
    case 1:   // 设置图标名 - 忽略
        break;
    case 7:   // 设置工作目录 (OSC 7)
        break;
    case 8:   // 超链接 (OSC 8 ; params ; uri ST)
        break;
    case 52:  // 剪贴板操作
        if (ev.oscData.rfind("c;", 0) == 0) {
            // 请求剪贴板内容 - 暂不实现
        }
        break;
    default:
        break;
    }
}

void TerminalModel::handleEscDispatch(const VtParser::Event& ev)
{
    switch (ev.escFinal) {
    case 'D': // IND - 索引 (向下滚动)
        buffer_.lineFeed();
        break;
    case 'E': // NEL - 下一行
        buffer_.carriageReturn();
        buffer_.lineFeed();
        break;
    case 'H': // HTS - 设置 Tab Stop
        buffer_.setTabStop();
        break;
    case 'M': // RI - 反向索引
        buffer_.reverseLineFeed();
        break;
    case '7': // DECSC - 保存光标
        buffer_.saveCursor();
        break;
    case '8': // DECRC - 恢复光标
        buffer_.restoreCursor();
        break;
    case 'c': // RIS - 完全重置
        buffer_.resize(buffer_.columns(), buffer_.rows());
        buffer_.resetScrollRegion();
        buffer_.clearAllTabStops();
        break;
    default:
        break;
    }
}

// --- CSI 命令实现 ---

void TerminalModel::csiSGR(const std::vector<int>& params)
{
    auto& cur = buffer_.cursor();
    if (params.empty() || (params.size() == 1 && params[0] == 0)) {
        // 重置所有样式
        cur.fg = 0; cur.bg = 0;
        cur.fgIsDefault = true; cur.bgIsDefault = true;
        cur.fgIsRgb = false; cur.bgIsRgb = false;
        cur.bold = false; cur.italic = false;
        cur.underline = false; cur.strikethrough = false;
        cur.blink = false; cur.inverse = false;
        cur.invisible = false; cur.dim = false;
        return;
    }

    for (size_t i = 0; i < params.size(); i++) {
        int p = params[i];
        switch (p) {
        case 0: // 重置
            cur.fg = 0; cur.bg = 0;
            cur.fgIsDefault = true; cur.bgIsDefault = true;
            cur.fgIsRgb = false; cur.bgIsRgb = false;
            cur.bold = false; cur.italic = false;
            cur.underline = false; cur.strikethrough = false;
            cur.blink = false; cur.inverse = false;
            cur.invisible = false; cur.dim = false;
            break;
        case 1: cur.bold = true; break;
        case 2: cur.dim = true; break;
        case 3: cur.italic = true; break;
        case 4: cur.underline = true; break;
        case 5: cur.blink = true; break;
        case 7: cur.inverse = true; break;
        case 8: cur.invisible = true; break;
        case 9: cur.strikethrough = true; break;
        case 22: cur.bold = false; cur.dim = false; break;
        case 23: cur.italic = false; break;
        case 24: cur.underline = false; break;
        case 25: cur.blink = false; break;
        case 27: cur.inverse = false; break;
        case 28: cur.invisible = false; break;
        case 29: cur.strikethrough = false; break;
        // 标准 16 色: SGR 30-37 → 索引 0-7
        case 30: case 31: case 32: case 33:
        case 34: case 35: case 36: case 37:
            cur.fg = static_cast<uint16_t>(p - 30); cur.fgIsDefault = false; cur.fgIsRgb = false; break;
        case 39: cur.fgIsDefault = true; cur.fgIsRgb = false; break; // 默认前景
        case 40: case 41: case 42: case 43:
        case 44: case 45: case 46: case 47:
            cur.bg = static_cast<uint16_t>(p - 40); cur.bgIsDefault = false; cur.bgIsRgb = false; break;
        case 49: cur.bgIsDefault = true; cur.bgIsRgb = false; break; // 默认背景
        // 亮色: SGR 90-97 → 索引 8-15
        case 90: case 91: case 92: case 93:
        case 94: case 95: case 96: case 97:
            cur.fg = static_cast<uint16_t>(p - 90 + 8); cur.fgIsDefault = false; cur.fgIsRgb = false; break;
        case 100: case 101: case 102: case 103:
        case 104: case 105: case 106: case 107:
            cur.bg = static_cast<uint16_t>(p - 100 + 8); cur.bgIsDefault = false; cur.bgIsRgb = false; break;
        // 256 色
        case 38:
            if (i + 1 < params.size()) {
                if (params[i + 1] == 5 && i + 2 < params.size()) {
                    // 256 色: 38;5;n
                    cur.fg = static_cast<uint16_t>(params[i + 2]);
                    cur.fgIsDefault = false; cur.fgIsRgb = false;
                    i += 2;
                } else if (params[i + 1] == 2 && i + 4 < params.size()) {
                    // 真彩色: 38;2;r;g;b
                    cur.fgIsRgb = true; cur.fgIsDefault = false;
                    cur.rgbFg[0] = static_cast<uint8_t>(params[i + 2]);
                    cur.rgbFg[1] = static_cast<uint8_t>(params[i + 3]);
                    cur.rgbFg[2] = static_cast<uint8_t>(params[i + 4]);
                    i += 4;
                }
            }
            break;
        case 48:
            if (i + 1 < params.size()) {
                if (params[i + 1] == 5 && i + 2 < params.size()) {
                    // 256 色: 48;5;n
                    cur.bg = static_cast<uint16_t>(params[i + 2]);
                    cur.bgIsDefault = false; cur.bgIsRgb = false;
                    i += 2;
                } else if (params[i + 1] == 2 && i + 4 < params.size()) {
                    // 真彩色: 48;2;r;g;b
                    cur.bgIsRgb = true; cur.bgIsDefault = false;
                    cur.rgbBg[0] = static_cast<uint8_t>(params[i + 2]);
                    cur.rgbBg[1] = static_cast<uint8_t>(params[i + 3]);
                    cur.rgbBg[2] = static_cast<uint8_t>(params[i + 4]);
                    i += 4;
                }
            }
            break;
        default:
            break;
        }
    }
}

void TerminalModel::csiCursorUp(int n)
{
    int minRow = buffer_.cursor().originMode ? buffer_.scrollRegionTop() : 0;
    buffer_.setCursorRow(std::max(minRow, buffer_.cursorRow() - n));
}

void TerminalModel::csiCursorDown(int n)
{
    int maxRow = buffer_.cursor().originMode ? buffer_.scrollRegionBottom() : (buffer_.rows() - 1);
    buffer_.setCursorRow(std::min(maxRow, buffer_.cursorRow() + n));
}

void TerminalModel::csiCursorForward(int n)
{
    buffer_.setCursorCol(std::min(buffer_.columns() - 1, buffer_.cursorCol() + n));
}

void TerminalModel::csiCursorBack(int n)
{
    buffer_.setCursorCol(std::max(0, buffer_.cursorCol() - n));
}

void TerminalModel::csiCursorNextLine(int n)
{
    buffer_.setCursorPos(0, buffer_.cursorRow() + n);
}

void TerminalModel::csiCursorPrevLine(int n)
{
    buffer_.setCursorPos(0, buffer_.cursorRow() - n);
}

void TerminalModel::csiCursorHorizontalAbsolute(int n)
{
    buffer_.setCursorCol(n - 1);
}

void TerminalModel::csiCursorPosition(int row, int col)
{
    int r = row - 1;
    int c = col - 1;
    if (buffer_.cursor().originMode) {
        r += buffer_.scrollRegionTop();
    }
    buffer_.setCursorPos(c, r);
}

void TerminalModel::csiCursorVerticalAbsolute(int n)
{
    buffer_.setCursorRow(n - 1);
}

void TerminalModel::csiEraseInDisplay(int mode)
{
    buffer_.eraseInDisplay(mode);
}

void TerminalModel::csiEraseInLine(int mode)
{
    buffer_.eraseInLine(mode);
}

void TerminalModel::csiInsertLines(int n)
{
    buffer_.insertLines(n);
}

void TerminalModel::csiDeleteLines(int n)
{
    buffer_.deleteLines(n);
}

void TerminalModel::csiInsertCharacters(int n)
{
    buffer_.insertCharacters(n);
}

void TerminalModel::csiDeleteCharacters(int n)
{
    buffer_.deleteCharacters(n);
}

void TerminalModel::csiEraseCharacters(int n)
{
    buffer_.eraseCharacters(n);
}

void TerminalModel::csiScrollUp(int n)
{
    buffer_.scrollUp(n);
}

void TerminalModel::csiScrollDown(int n)
{
    buffer_.scrollDown(n);
}

void TerminalModel::csiSetScrollRegion(int top, int bottom)
{
    buffer_.setScrollRegion(top - 1, bottom - 1);
    // 光标回到 Home
    buffer_.setCursorPos(0, buffer_.cursor().originMode ? buffer_.scrollRegionTop() : 0);
}

void TerminalModel::csiTabForward(int n)
{
    int col = buffer_.cursorCol();
    for (int i = 0; i < n; i++) {
        col = buffer_.nextTabStop(col);
    }
    buffer_.setCursorCol(col);
}

void TerminalModel::csiTabBackward(int n)
{
    int col = buffer_.cursorCol();
    for (int i = 0; i < n; i++) {
        col = buffer_.prevTabStop(col);
    }
    buffer_.setCursorCol(col);
}

void TerminalModel::csiSaveCursor()
{
    buffer_.saveCursor();
}

void TerminalModel::csiRestoreCursor()
{
    buffer_.restoreCursor();
}

// --- DEC 私有模式 ---

void TerminalModel::decSetMode(int mode, bool enable)
{
    switch (mode) {
    case 1:   // DECCKM - 应用光标键模式
        input_.setApplicationCursorKeys(enable);
        break;
    case 3:   // DECCOLM - 132 列模式
        if (enable) resize(132, buffer_.rows());
        else resize(80, buffer_.rows());
        break;
    case 5:   // DECSCNM - 反色屏幕模式
        // TODO: 实现反色屏幕
        break;
    case 6:   // DECOM - 原点模式
        buffer_.cursor().originMode = enable;
        if (enable) {
            buffer_.setCursorPos(0, buffer_.scrollRegionTop());
        }
        break;
    case 7:   // DECAWM - 自动换行模式
        // buffer_ 内部处理
        break;
    case 9:   // X10 鼠标跟踪
        input_.setMouseTracking(enable ? TerminalInput::X10 : TerminalInput::None);
        break;
    case 12:  // 光标闪烁
        // 由 UI 层处理
        break;
    case 25:  // DECTCEM - 光标显示
        // 由 UI 层处理
        break;
    case 1000: // 基本鼠标跟踪
        input_.setMouseTracking(enable ? TerminalInput::Normal : TerminalInput::None);
        break;
    case 1002: // 按钮事件鼠标跟踪
        input_.setMouseTracking(enable ? TerminalInput::Button : TerminalInput::None);
        break;
    case 1003: // 任意事件鼠标跟踪
        input_.setMouseTracking(enable ? TerminalInput::Any : TerminalInput::None);
        break;
    case 1006: // SGR 鼠标编码
        input_.setMouseEncoding(enable ? TerminalInput::SGR : TerminalInput::Default);
        break;
    case 1015: // URXVT 鼠标编码
        input_.setMouseEncoding(enable ? TerminalInput::URXVT : TerminalInput::Default);
        break;
    case 1049: // Alternate Screen Buffer
        if (enable) {
            buffer_.useAlternateBuffer();
        } else {
            buffer_.useNormalBuffer();
        }
        break;
    case 2004: // Bracket Paste Mode
        input_.setBracketPasteMode(enable);
        break;
    default:
        break;
    }
}

// --- 辅助方法 ---

int TerminalModel::paramValue(const std::vector<int>& params, size_t index, int defaultValue) const
{
    if (index < params.size() && params[index] != 0) {
        return params[index];
    }
    return defaultValue;
}

void TerminalModel::notifyChanged()
{
    if (!contentChangedPending_) {
        contentChangedPending_ = true;
        frameTimer_.start();
    }
}
