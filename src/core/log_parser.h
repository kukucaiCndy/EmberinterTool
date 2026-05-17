#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QColor>
#include "log_buffer.h"

class LogParser {
public:
    static QStringList extractLines(const QByteArray& data, QByteArray& remainder);

    static QString detectLevel(const QString& line);

    static LogEntry parseLine(const QByteArray& rawData, const QString& portName);

    static QString formatDisplay(const LogEntry& entry, bool showTimestamp);

    static QString formatHex(const QByteArray& data, int baseOffset = 0);

    static QColor levelColor(const QString& level);
    static QString levelColorHex(const QString& level);

private:
    LogParser() = default;
};

#endif
