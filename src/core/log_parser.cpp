#include "log_parser.h"
#include <QDateTime>
#include <QStringList>
// ANSI 颜色码 → 颜色映射表 (适合深色背景, GitHub Dark 风格)
// 索引: 0-7 标准色, 8-15 亮色
static const QHash<int, QString> ansiColorMap = {
    {0,  "#6E7681"}, // 黑 → 深灰
    {1,  "#F85149"}, // 红
    {2,  "#7EE787"}, // 绿
    {3,  "#D29922"}, // 黄
    {4,  "#79C0FF"}, // 蓝
    {5,  "#D2A8FF"}, // 品红
    {6,  "#56D4DD"}, // 青
    {7,  "#E6EDF3"}, // 白
    {8,  "#8B949E"}, // 亮黑 → 灰
    {9,  "#FF7B72"}, // 亮红
    {10, "#56D364"}, // 亮绿
    {11, "#E3B341"}, // 亮黄
    {12, "#79C0FF"}, // 亮蓝
    {13, "#D2A8FF"}, // 亮品红
    {14, "#56D4DD"}, // 亮青
    {15, "#F0F6FC"}, // 亮白
};

// ── SGR 样式状态 (ansiToHtml 内部使用) ──────────────────

struct SgrState {
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    bool inverse = false;
    int fg = -1;    // -1 = 默认; 0-15 基础色; 16-255 扩展色 (256色)
    int bg = -1;
    bool fgRgb = false;   // 真彩色前景
    bool bgRgb = false;   // 真彩色背景
    int fgRgbVal[3] = {0, 0, 0};
    int bgRgbVal[3] = {0, 0, 0};

    bool operator==(const SgrState& o) const
    {
        return bold == o.bold && dim == o.dim && italic == o.italic &&
               underline == o.underline && strike == o.strike && inverse == o.inverse &&
               fg == o.fg && bg == o.bg && fgRgb == o.fgRgb && bgRgb == o.bgRgb &&
               fgRgbVal[0] == o.fgRgbVal[0] && fgRgbVal[1] == o.fgRgbVal[1] &&
               fgRgbVal[2] == o.fgRgbVal[2] && bgRgbVal[0] == o.bgRgbVal[0] &&
               bgRgbVal[1] == o.bgRgbVal[1] && bgRgbVal[2] == o.bgRgbVal[2];
    }
    bool operator!=(const SgrState& o) const { return !(*this == o); }
};

// 256色索引 → #RRGGBB (标准 xterm 调色板)
static QString ansiIndexToHex(int idx)
{
    if (idx >= 0 && idx < 16) return ansiColorMap.value(idx, "#E6EDF3");
    // 16-231: 6x6x6 色彩立方体 (蓝通道最快, 同标准 xterm)
    if (idx < 232) {
        int n = idx - 16;
        int r = n / 36;        // 0-5
        int g = (n / 6) % 6;   // 0-5
        int b = n % 6;         // 0-5
        auto level = [](int c) { return c == 0 ? 0 : 55 + c * 40; };
        int rv = level(r), gv = level(g), bv = level(b);
        return QString("#%1%2%3")
            .arg(rv, 2, 16, QChar('0'))
            .arg(gv, 2, 16, QChar('0'))
            .arg(bv, 2, 16, QChar('0'));
    }
    // 232-255: 24 级灰度
    if (idx < 256) {
        int gray = 8 + (idx - 232) * 10;
        return QString("#%1%2%3")
            .arg(gray, 2, 16, QChar('0'))
            .arg(gray, 2, 16, QChar('0'))
            .arg(gray, 2, 16, QChar('0'));
    }
    return QStringLiteral("#E6EDF3");
}

static QString rgbToHex(const int* rgb)
{
    return QString("#%1%2%3")
        .arg(qBound(0, rgb[0], 255), 2, 16, QChar('0'))
        .arg(qBound(0, rgb[1], 255), 2, 16, QChar('0'))
        .arg(qBound(0, rgb[2], 255), 2, 16, QChar('0'));
}

// 变暗: 向黑色混合, 模拟 dim 属性
static QString dimColor(const QString& hex)
{
    int r = hex.mid(1, 2).toInt(nullptr, 16);
    int g = hex.mid(3, 2).toInt(nullptr, 16);
    int b = hex.mid(5, 2).toInt(nullptr, 16);
    r = r * 3 / 5; g = g * 3 / 5; b = b * 3 / 5;
    return QString("#%1%2%3")
        .arg(r, 2, 16, QChar('0'))
        .arg(g, 2, 16, QChar('0'))
        .arg(b, 2, 16, QChar('0'));
}

// 将 SGR 状态转为 CSS 样式串
static QString sgrStyle(const SgrState& st)
{
    // 反色: 前后景互换
    bool inverse = st.inverse;
    int fgIdx = inverse ? st.bg : st.fg;
    int bgIdx = inverse ? st.fg : st.bg;
    bool fgIsRgb = inverse ? st.bgRgb : st.fgRgb;
    bool bgIsRgb = inverse ? st.fgRgb : st.bgRgb;
    const int* fgRgb = inverse ? st.bgRgbVal : st.fgRgbVal;
    const int* bgRgb = inverse ? st.fgRgbVal : st.bgRgbVal;

    QString fgHex, bgHex;
    if (fgIdx >= 0) fgHex = fgIsRgb ? rgbToHex(fgRgb) : ansiIndexToHex(fgIdx);
    if (bgIdx >= 0) bgHex = bgIsRgb ? rgbToHex(bgRgb) : ansiIndexToHex(bgIdx);

    QStringList css;
    if (!fgHex.isEmpty()) {
        if (st.dim) fgHex = dimColor(fgHex);
        css << "color:" + fgHex;
    } else if (st.dim) {
        css << "color:#8B949E";  // 仅 dim 无前景色: 使用次要灰
    }
    if (!bgHex.isEmpty()) css << "background-color:" + bgHex;
    if (st.bold) css << "font-weight:bold";
    if (st.italic) css << "font-style:italic";
    QStringList deco;
    if (st.underline) deco << "underline";
    if (st.strike) deco << "line-through";
    if (!deco.isEmpty()) css << "text-decoration:" + deco.join(' ');
    return css.join(';');
}

static bool sgrActive(const SgrState& s)
{
    return s.fg >= 0 || s.bg >= 0 || s.bold || s.dim || s.italic ||
           s.underline || s.strike || s.inverse;
}

// 应用 SGR 参数, 样式变化时关闭旧 span 并打开新 span
static void applySgr(const QString& rawParams, SgrState& st, QString& html)
{
    SgrState next = st;
    if (rawParams.isEmpty()) {
        next = SgrState{};  // \033[m 等价于重置
    } else {
        QString norm = rawParams;
        norm.replace(':', ';');  // 兼容 38:5:n 冒号分隔写法
        const QStringList parts = norm.split(';', Qt::SkipEmptyParts);
        for (int i = 0; i < parts.size(); ++i) {
            bool ok = false;
            const int code = parts[i].toInt(&ok);
            if (!ok) continue;
            switch (code) {
            case 0: next = SgrState{}; break;
            case 1: next.bold = true; break;
            case 2: next.dim = true; break;
            case 3: next.italic = true; break;
            case 4: next.underline = true; break;
            case 7: next.inverse = true; break;
            case 9: next.strike = true; break;
            case 22: next.bold = next.dim = false; break;
            case 23: next.italic = false; break;
            case 24: next.underline = false; break;
            case 27: next.inverse = false; break;
            case 29: next.strike = false; break;
            case 30: case 31: case 32: case 33:
            case 34: case 35: case 36: case 37:
                next.fg = code - 30; next.fgRgb = false; break;
            case 38:  // 扩展前景: 38;5;n (256色) / 38;2;r;g;b (真彩色)
                if (i + 1 < parts.size() && parts[i + 1] == "5" && i + 2 < parts.size()) {
                    next.fg = parts[i + 2].toInt(); next.fgRgb = false; i += 2;
                } else if (i + 1 < parts.size() && parts[i + 1] == "2" && i + 4 < parts.size()) {
                    next.fg = 0;  // 占位: 实际颜色取 fgRgbVal
                    next.fgRgb = true;
                    next.fgRgbVal[0] = parts[i + 2].toInt();
                    next.fgRgbVal[1] = parts[i + 3].toInt();
                    next.fgRgbVal[2] = parts[i + 4].toInt();
                    i += 4;
                }
                break;
            case 39: next.fg = -1; next.fgRgb = false; break;
            case 40: case 41: case 42: case 43:
            case 44: case 45: case 46: case 47:
                next.bg = code - 40; next.bgRgb = false; break;
            case 48:  // 扩展背景: 48;5;n / 48;2;r;g;b
                if (i + 1 < parts.size() && parts[i + 1] == "5" && i + 2 < parts.size()) {
                    next.bg = parts[i + 2].toInt(); next.bgRgb = false; i += 2;
                } else if (i + 1 < parts.size() && parts[i + 1] == "2" && i + 4 < parts.size()) {
                    next.bg = 0;  // 占位: 实际颜色取 bgRgbVal
                    next.bgRgb = true;
                    next.bgRgbVal[0] = parts[i + 2].toInt();
                    next.bgRgbVal[1] = parts[i + 3].toInt();
                    next.bgRgbVal[2] = parts[i + 4].toInt();
                    i += 4;
                }
                break;
            case 49: next.bg = -1; next.bgRgb = false; break;
            case 90: case 91: case 92: case 93:
            case 94: case 95: case 96: case 97:
                next.fg = code - 90 + 8; next.fgRgb = false; break;
            case 100: case 101: case 102: case 103:
            case 104: case 105: case 106: case 107:
                next.bg = code - 100 + 8; next.bgRgb = false; break;
            default: break;
            }
        }
    }

    if (next == st) return;
    if (sgrActive(st)) html += "</span>";
    st = next;
    if (sgrActive(st)) html += "<span style=\"" + sgrStyle(st) + "\">";
}

// 扫描从 start 开始 (start 指向 ESC 之后的字符) 的一个转义序列。
// 返回消耗的字符数; isSgr 为 true 表示 SGR (m) 序列, sgrParams 为参数串。
static int scanEscape(const QString& text, int start, bool& isSgr, QString& sgrParams)
{
    isSgr = false;
    sgrParams.clear();
    if (start >= text.size()) return 0;
    const QChar c = text.at(start);

    if (c == '[') {
        // CSI: ESC [ <参数> <中间字节> <最终字节>
        int i = start + 1;
        QString params;
        while (i < text.size()) {
            const QChar cc = text.at(i);
            if (cc >= '0' && cc <= '9') { params += cc; ++i; }
            else if (cc == ';' || cc == ':' || cc == '?') { params += cc; ++i; }
            else if (cc >= ' ' && cc <= '/') { ++i; }  // 中间字节
            else if (cc >= '@' && cc <= '~') {  // 最终字节
                isSgr = (cc == 'm');
                sgrParams = params;
                return i + 1 - start;
            } else {
                return 0;  // 非法字节, 序列中止
            }
        }
        return 0;  // 序列未完成
    }

    if (c == ']') {
        // OSC: ESC ] <数据> (BEL 或 ESC \ 结束)
        int i = start + 1;
        while (i < text.size()) {
            const QChar cc = text.at(i);
            if (cc == '\007') return i + 1 - start;
            if (cc == '\033') {
                if (i + 1 < text.size() && text.at(i + 1) == '\\') return i + 2 - start;
                return i + 1 - start;
            }
            ++i;
        }
        return 0;
    }

    // ESC + 中间字节 + 最终字节 (如 ESC ( B 等)
    int i = start;
    while (i < text.size() && text.at(i) >= ' ' && text.at(i) <= '/') ++i;
    if (i >= text.size()) return 0;
    return i + 1 - start;
}

QString LogParser::stripAnsi(const QString& text)
{
    QString result;
    result.reserve(text.size());
    const int n = text.size();
    int i = 0;
    while (i < n) {
        if (text.at(i) != '\033') {
            result += text.at(i);
            ++i;
            continue;
        }
        bool isSgr = false;
        QString params;
        const int consumed = scanEscape(text, i + 1, isSgr, params);
        if (consumed > 0) i += consumed + 1;
        else ++i;  // 未完成的转义序列: 丢弃 ESC 本身
    }
    return result;
}

QString LogParser::ansiToHtml(const QString& text)
{
    // HTML 转义
    QString escaped;
    escaped.reserve(text.size() * 2);
    for (const QChar& ch : text) {
        if (ch == '<') escaped += "&lt;";
        else if (ch == '>') escaped += "&gt;";
        else if (ch == '&') escaped += "&amp;";
        else escaped += ch;
    }

    // 逐字符扫描: SGR 序列转 <span> 颜色标签, 其他转义序列直接剥离
    QString result;
    result.reserve(escaped.size() + 64);
    SgrState sgr;
    const int n = escaped.size();
    int i = 0;
    while (i < n) {
        if (escaped.at(i) != '\033') {
            result += escaped.at(i);
            ++i;
            continue;
        }
        bool isSgr = false;
        QString params;
        const int consumed = scanEscape(escaped, i + 1, isSgr, params);
        if (consumed > 0) {
            if (isSgr) applySgr(params, sgr, result);
            i += consumed + 1;
        } else {
            ++i;  // 未完成的转义序列: 丢弃 ESC 本身
        }
    }

    // 关闭未闭合的 span
    if (sgrActive(sgr)) result += "</span>";
    return result;
}

QStringList LogParser::extractLines(const QByteArray& data, QByteArray& remainder)
{
    QStringList lines;
    QByteArray buffer = remainder + data;
    remainder.clear();

    int start = 0;
    for (int i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == '\n') {
            QByteArray line = buffer.mid(start, i - start);
            if (line.endsWith('\r')) {
                line.chop(1);
            }
            lines.append(QString::fromUtf8(line));
            start = i + 1;
        }
    }

    if (start < buffer.size()) {
        remainder = buffer.mid(start);
    }

    return lines;
}

QString LogParser::detectLevel(const QString& line)
{
    QString upper = line.trimmed().toUpper();

    // 1. 检测行首前缀: "LEVEL ...", "[LEVEL] ...", "<LEVEL> ..."
    auto startsWithLevel = [&upper](const QString& prefix) {
        return upper.startsWith(prefix) || upper.startsWith("[" + prefix + "]") ||
               upper.startsWith("<" + prefix + ">");
    };

    // 2. 检测行中任意位置的 [LEVEL] 标签 (支持 "[INFO][其他] message" 这种格式)
    auto containsBracketLevel = [&upper](const QString& prefix) {
        return upper.contains("[" + prefix + "]");
    };

    // 3. 检测 Zephyr RTOS 日志格式: <inf>, <wrn>, <err>, <dbg> (不区分大小写)
    auto containsAngleLevel = [&upper](const QString& prefix) {
        return upper.contains("<" + prefix + ">");
    };

    // ERROR (优先级最高)
    if (startsWithLevel("ERROR") || startsWithLevel("ERR") ||
        containsBracketLevel("ERROR") || containsBracketLevel("ERR") ||
        containsAngleLevel("ERR") ||
        upper.contains("FAIL:") || upper.contains("[FAIL]")) {
        return "ERROR";
    }
    // WARN (支持 WARNG 变体)
    if (startsWithLevel("WARN") || startsWithLevel("WARNING") || startsWithLevel("WARNG") ||
        containsBracketLevel("WARN") || containsBracketLevel("WARNING") ||
        containsBracketLevel("WARNG") ||
        containsAngleLevel("WRN")) {
        return "WARN";
    }
    // INFO
    if (startsWithLevel("INFO") || containsBracketLevel("INFO") ||
        containsAngleLevel("INF")) {
        return "INFO";
    }
    // DEBUG
    if (startsWithLevel("DEBUG") || startsWithLevel("DBG") ||
        containsBracketLevel("DEBUG") || containsBracketLevel("DBG") ||
        containsAngleLevel("DBG")) {
        return "DEBUG";
    }
    // TRACE
    if (startsWithLevel("TRACE") || containsBracketLevel("TRACE") ||
        containsAngleLevel("TRC")) {
        return "TRACE";
    }
    return "";
}

LogEntry LogParser::parseLine(const QByteArray& rawData, const QString& portName)
{
    // 剥离 ANSI 转义码得到纯文本 (用于级别检测、复制、导出)
    QString line = stripAnsi(QString::fromUtf8(rawData)).trimmed();
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString level = detectLevel(line);
    return LogEntry(ts, level, line, rawData, portName);
}

QString LogParser::formatDisplay(const LogEntry& entry, bool showTimestamp)
{
    // 直接显示原始文本 (若包含 [INFO] 等标签则保留，避免重复添加 [level] 前缀)
    if (showTimestamp) {
        return QString("[%1] %2").arg(entry.timestamp, entry.text);
    }
    return entry.text;
}

QString LogParser::formatHex(const QByteArray& data, int baseOffset)
{
    QString result;
    const int bytesPerLine = 16;
    int totalLines = (data.size() + bytesPerLine - 1) / bytesPerLine;

    for (int line = 0; line < totalLines; ++line) {
        int offset = line * bytesPerLine;
        int chunkSize = qMin(bytesPerLine, data.size() - offset);

        result += QString("%1  ").arg(baseOffset + offset, 8, 16, QChar('0'));

        QString hexPart;
        QString asciiPart;

        for (int i = 0; i < bytesPerLine; ++i) {
            if (i == 8) {
                hexPart += ' ';
            }
            if (i < chunkSize) {
                unsigned char byte = static_cast<unsigned char>(data[offset + i]);
                hexPart += QString("%1 ").arg(byte, 2, 16, QChar('0'));
                asciiPart += (byte >= 32 && byte <= 126) ? QChar(byte) : QChar('.');
            } else {
                hexPart += "   ";
                asciiPart += ' ';
            }
        }

        result += hexPart + QString(" |%1|").arg(asciiPart);
        if (line < totalLines - 1) {
            result += '\n';
        }
    }

    return result;
}

QString LogParser::levelColorHex(const QString& level)
{
    // 与 DesignSystem.qml 的日志颜色保持一致 (适合深色背景 #0D1117)
    // 避免使用 #333333 这类在黑底上几乎不可见的颜色
    if (level == "ERROR") return "#F85149";  // 红色 (error)
    if (level == "WARN")  return "#D29922";  // 黄色 (warning)
    if (level == "INFO")  return "#E6EDF3";  // 主文本白 (textPrimary)
    if (level == "DEBUG") return "#8B949E";  // 次要灰 (textSecondary)
    if (level == "TRACE") return "#8B949E";  // 次要灰 (textSecondary)
    if (level == "TX")    return "#7EE787";  // 发送绿
    if (level == "RX")    return "#79C0FF";  // 接收蓝
    return "#E6EDF3";  // 默认使用主文本色，确保可见
}

bool LogParser::isZephyrLine(const QString& text)
{
    const QString upper = text.toUpper();
    return upper.contains("<INF>") || upper.contains("<WRN>") ||
           upper.contains("<ERR>") || upper.contains("<DBG>") ||
           upper.contains("<TRC>");
}

QString LogParser::zephyrColorHex(const QString& level)
{
    // Zephyr log_output 规范色: err=红, wrn=黄, inf=绿, dbg=青, trc=灰
    if (level == "ERROR") return "#F85149";  // <err> 红
    if (level == "WARN")  return "#D29922";  // <wrn> 黄
    if (level == "INFO")  return "#7EE787";  // <inf> 绿
    if (level == "DEBUG") return "#56D4DD";  // <dbg> 青
    if (level == "TRACE") return "#8B949E";  // <trc> 灰
    return "#E6EDF3";
}
