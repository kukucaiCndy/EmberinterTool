#include "log_parser.h"
#include <QDateTime>
#include <QStringList>

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

    // ERROR (优先级最高)
    if (startsWithLevel("ERROR") || startsWithLevel("ERR") ||
        containsBracketLevel("ERROR") || containsBracketLevel("ERR") ||
        upper.contains("FAIL")) {
        return "ERROR";
    }
    // WARN (支持 WARNG 变体)
    if (startsWithLevel("WARN") || startsWithLevel("WARNING") || startsWithLevel("WARNG") ||
        containsBracketLevel("WARN") || containsBracketLevel("WARNING") ||
        containsBracketLevel("WARNG")) {
        return "WARN";
    }
    // INFO
    if (startsWithLevel("INFO") || containsBracketLevel("INFO")) {
        return "INFO";
    }
    // DEBUG
    if (startsWithLevel("DEBUG") || startsWithLevel("DBG") ||
        containsBracketLevel("DEBUG") || containsBracketLevel("DBG")) {
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
    QString line = QString::fromUtf8(rawData).trimmed();
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
