#include "ipc_protocol.h"

QByteArray IpcProtocol::buildMessage(const QString& type, const QJsonObject& payload,
                                     const QString& id)
{
    QJsonObject msg;
    msg["type"] = type;
    msg["payload"] = payload;
    if (!id.isEmpty()) {
        msg["id"] = id;
    }
    QJsonDocument doc(msg);
    return doc.toJson(QJsonDocument::Compact) + "\n";
}

QByteArray IpcProtocol::buildResponse(const QString& id, bool success,
                                      const QJsonObject& data)
{
    QJsonObject payload = data;
    payload["success"] = success;
    return buildMessage("response", payload, id);
}

QJsonObject IpcProtocol::parseMessage(const QByteArray& data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}

QJsonObject IpcProtocol::buildLogEntryMessage(const LogEntry& entry)
{
    QJsonObject payload;
    payload["port"] = entry.portName;
    payload["timestamp"] = entry.timestamp;
    payload["level"] = entry.level;
    payload["text"] = entry.text;
    payload["raw_hex"] = QString::fromLatin1(entry.rawBytes.toHex());
    return payload;
}

QJsonObject IpcProtocol::buildStatusMessage(const QString& port, bool connected,
                                            const SerialConfig& config,
                                            qint64 rxBytes, qint64 txBytes,
                                            qint64 uptimeSeconds)
{
    QJsonObject stats;
    stats["bytes_received"] = rxBytes;
    stats["bytes_sent"] = txBytes;
    stats["uptime_seconds"] = uptimeSeconds;

    QJsonObject payload;
    payload["port"] = port;
    payload["connected"] = connected;
    payload["baudrate"] = config.baudrate;
    payload["stats"] = stats;
    return payload;
}

QJsonObject IpcProtocol::buildPortInfo(const QString& name, const QString& description,
                                        const QString& vid, const QString& pid,
                                        bool recommended)
{
    QJsonObject port;
    port["name"] = name;
    port["description"] = description;
    port["vid"] = vid;
    port["pid"] = pid;
    port["recommended"] = recommended;
    return port;
}

QJsonObject IpcProtocol::buildError(const QString& code, const QString& message,
                                     const QString& port)
{
    QJsonObject payload;
    payload["code"] = code;
    payload["message"] = message;
    if (!port.isEmpty()) {
        payload["port"] = port;
    }
    return payload;
}
