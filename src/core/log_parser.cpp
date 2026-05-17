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
    QString upper = line.toUpper();
    if (upper.contains("ERROR") || upper.contains("ERR]") || upper.contains("FAIL")) {
        return "ERROR";
    }
    if (upper.contains("WARN") || upper.contains("WARNING")) {
        return "WARN";
    }
    if (upper.contains("INFO")) {
        return "INFO";
    }
    if (upper.contains("DEBUG") || upper.contains("DBG]")) {
        return "DEBUG";
    }
    if (upper.contains("TRACE")) {
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
    if (showTimestamp) {
        if (entry.level.isEmpty()) {
            return QString("[%1] %2").arg(entry.timestamp, entry.text);
        }
        return QString("[%1] [%2] %3").arg(entry.timestamp, entry.level, entry.text);
    }
    if (entry.level.isEmpty()) {
        return entry.text;
    }
    return QString("[%1] %2").arg(entry.level, entry.text);
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

QColor LogParser::levelColor(const QString& level)
{
    if (level == "ERROR") return QColor("#D32F2F");
    if (level == "WARN")  return QColor("#F9A825");
    if (level == "INFO")  return QColor("#388E3C");
    if (level == "DEBUG") return QColor("#0288D1");
    if (level == "TRACE") return QColor("#757575");
    return QColor("#333333");
}

QString LogParser::levelColorHex(const QString& level)
{
    if (level == "ERROR") return "#D32F2F";
    if (level == "WARN")  return "#F9A825";
    if (level == "INFO")  return "#388E3C";
    if (level == "DEBUG") return "#0288D1";
    if (level == "TRACE") return "#757575";
    return "#333333";
}
