#include "log_exporter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <spdlog/spdlog.h>

bool LogExporter::exportToJson(
    const QVector<LogEntry>& entries,
    const QString& sourcePort,
    const QString& filePath)
{
    QString json = generateJson(entries, sourcePort);

    QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        spdlog::error("Cannot open export file: {}", filePath.toStdString());
        return false;
    }

    file.write(json.toUtf8());
    file.close();

    spdlog::info("Exported {} entries to {}", entries.size(), filePath.toStdString());
    return true;
}

QString LogExporter::generateJson(
    const QVector<LogEntry>& entries,
    const QString& sourcePort)
{
    QJsonObject root;
    root["export_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["source"] = sourcePort;
    root["entry_count"] = entries.size();

    QJsonArray entriesArr;
    for (const auto& entry : entries) {
        QJsonObject obj;
        obj["timestamp"] = entry.timestamp;
        obj["level"] = entry.level;
        obj["text"] = entry.text;
        obj["raw_hex"] = QString::fromLatin1(entry.rawBytes.toHex());
        obj["port"] = entry.portName;
        entriesArr.append(obj);
    }
    root["entries"] = entriesArr;

    QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString LogExporter::escapeJson(const QString& str)
{
    QString result;
    for (const QChar& c : str) {
        switch (c.unicode()) {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b";  break;
        case '\f': result += "\\f";  break;
        case '\n': result += "\\n";  break;
        case '\r': result += "\\r";  break;
        case '\t': result += "\\t";  break;
        default:   result += c;
        }
    }
    return result;
}
