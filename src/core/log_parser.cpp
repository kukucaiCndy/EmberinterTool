#include "log_parser.h"
#include <QDateTime>
#include <QStringList>
#include <QRegularExpression>

// ANSI 颜色码 → 颜色映射表 (适合深色背景)
static const QHash<int, QString> ansiColorMap = {
    {0,  "#E6EDF3"}, // 重置/默认 (textPrimary)
    {30, "#6E7681"}, // 黑色 → 深灰
    {31, "#F85149"}, // 红色
    {32, "#7EE787"}, // 绿色
    {33, "#D29922"}, // 黄色
    {34, "#79C0FF"}, // 蓝色
    {35, "#D2A8FF"}, // 品红
    {36, "#56D4DD"}, // 青色
    {37, "#E6EDF3"}, // 白色
    {90, "#8B949E"}, // 亮黑 → 灰
    {91, "#FF7B72"}, // 亮红
    {92, "#56D364"}, // 亮绿
    {93, "#E3B341"}, // 亮黄
    {94, "#79C0FF"}, // 亮蓝
    {95, "#D2A8FF"}, // 亮品红
    {96, "#56D4DD"}, // 亮青
    {97, "#F0F6FC"}, // 亮白
};

QString LogParser::stripAnsi(const QString& text)
{
    static const QRegularExpression ansiRe("\033\\[[0-9;]*m");
    QString result = text;
    result.remove(ansiRe);
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

    // 解析 ANSI 转义序列，转为 <span style="color:..."> 标签
    static const QRegularExpression ansiRe("\033\\[([0-9;]*)m");
    QRegularExpressionMatchIterator it = ansiRe.globalMatch(escaped);
    
    QString result;
    int lastEnd = 0;
    QStringList openSpans; // 栈追踪已打开的 span
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        // 添加转义码之前的文本
        result += escaped.mid(lastEnd, match.capturedStart() - lastEnd);
        lastEnd = match.capturedEnd();
        
        QString codes = match.captured(1);
        if (codes.isEmpty()) {
            // \033[0m 重置
            for (int i = openSpans.size() - 1; i >= 0; --i) {
                result += "</span>";
            }
            openSpans.clear();
        } else {
            QStringList parts = codes.split(';', Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                bool ok;
                int code = part.toInt(&ok);
                if (!ok) continue;
                
                if (code == 0) {
                    // 重置
                    for (int i = openSpans.size() - 1; i >= 0; --i) {
                        result += "</span>";
                    }
                    openSpans.clear();
                } else if (code == 1) {
                    // 粗体: 用亮色
                    if (!openSpans.isEmpty()) {
                        result += "</span>";
                        openSpans.removeLast();
                    }
                    result += "<span style=\"font-weight:bold;color:#F0F6FC\">";
                    openSpans.append("bold");
                } else if (ansiColorMap.contains(code)) {
                    // 颜色码: 关闭上一个 span 再开新的
                    if (!openSpans.isEmpty()) {
                        result += "</span>";
                        openSpans.removeLast();
                    }
                    result += QString("<span style=\"color:%1\">").arg(ansiColorMap.value(code));
                    openSpans.append("color");
                }
            }
        }
    }
    
    // 添加剩余文本
    result += escaped.mid(lastEnd);
    
    // 关闭所有未关闭的 span
    for (int i = 0; i < openSpans.size(); ++i) {
        result += "</span>";
    }
    
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
    if (startsWithLevel("TRACE") || containsBracketLevel("TRACE")) {
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
