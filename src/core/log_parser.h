#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include "log_buffer.h"

class LogParser {
public:
    static QStringList extractLines(const QByteArray& data, QByteArray& remainder);

    static QString detectLevel(const QString& line);

    static LogEntry parseLine(const QByteArray& rawData, const QString& portName);

    static QString formatDisplay(const LogEntry& entry, bool showTimestamp);

    /// 将 ANSI 转义码转为 HTML <span> 颜色标签 (用于 QML RichText)
    static QString ansiToHtml(const QString& text);

    /// 剥离 ANSI 转义码 (纯文本)
    static QString stripAnsi(const QString& text);

    static QString formatHex(const QByteArray& data, int baseOffset = 0);

    static QString levelColorHex(const QString& level);

    /// 判断日志行是否为 Zephyr 格式 (含 <inf>/<wrn>/<err>/<dbg>/<trc> 级别标签)
    static bool isZephyrLine(const QString& text);

    /// Zephyr 级别规范色 (inf=绿, wrn=黄, err=红, dbg=青, trc=灰)
    static QString zephyrColorHex(const QString& level);

private:
    LogParser() = default;
};

#endif
