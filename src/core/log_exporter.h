#ifndef LOG_EXPORTER_H
#define LOG_EXPORTER_H

#include <QString>
#include <QVector>
#include "log_buffer.h"

class LogExporter {
public:
    static bool exportToJson(
        const QVector<LogEntry>& entries,
        const QString& sourcePort,
        const QString& filePath);

    static QString generateJson(
        const QVector<LogEntry>& entries,
        const QString& sourcePort);

private:
    LogExporter() = default;
};

#endif
