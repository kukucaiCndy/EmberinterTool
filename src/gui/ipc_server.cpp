#include "ipc_server.h"
#include "ipc_protocol.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <spdlog/spdlog.h>

IPCServer::IPCServer(QObject* parent)
    : QObject(parent)
    , server_(nullptr)
{
}

IPCServer::~IPCServer()
{
    stop();
}

bool IPCServer::start(const QString& name)
{
    server_ = new QLocalServer(this);
    QLocalServer::removeServer(name);

    if (!server_->listen(name)) {
        spdlog::error("IPC server failed to listen: {}", server_->errorString().toStdString());
        return false;
    }

    connect(server_, &QLocalServer::newConnection, this, &IPCServer::onNewConnection);
    spdlog::info("IPC server started on {}", name.toStdString());
    return true;
}

void IPCServer::stop()
{
    if (!server_) return;

    for (auto* client : clients_) {
        client->disconnectFromServer();
    }

    server_->close();
    spdlog::info("IPC server stopped");
}

void IPCServer::broadcastLog(const LogEntry& entry)
{
    QByteArray message = IpcProtocol::buildMessage(
        "log_entry", IpcProtocol::buildLogEntryMessage(entry));

    for (auto* client : clients_) {
        if (client && client->isOpen()) {
            sendToClient(client, message);
        }
    }
}

void IPCServer::broadcastStatus(const QString& port, bool connected,
                                 const SerialConfig& config,
                                 qint64 rxBytes, qint64 txBytes, qint64 uptimeSeconds)
{
    QByteArray message = IpcProtocol::buildMessage(
        "status_update",
        IpcProtocol::buildStatusMessage(port, connected, config,
                                        rxBytes, txBytes, uptimeSeconds));

    for (auto* client : clients_) {
        if (client && client->isOpen()) {
            sendToClient(client, message);
        }
    }
}

int IPCServer::clientCount() const
{
    return clients_.size();
}

void IPCServer::onNewConnection()
{
    while (server_->hasPendingConnections()) {
        QLocalSocket* client = server_->nextPendingConnection();
        clients_.append(client);
        readBuffers_.append(QByteArray());

        connect(client, &QLocalSocket::disconnected, this, &IPCServer::onDisconnected);
        connect(client, &QLocalSocket::readyRead, this, &IPCServer::onReadyRead);

        QString clientId = QString::number(reinterpret_cast<quintptr>(client));
        spdlog::info("IPC client connected: {}", clientId.toStdString());
        emit clientConnected(clientId);

        QJsonObject welcomePayload;
        welcomePayload["server_version"] = "2.0";
        QByteArray welcome = IpcProtocol::buildMessage("welcome", welcomePayload);
        sendToClient(client, welcome);
    }
}

void IPCServer::onDisconnected()
{
    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) return;

    int index = clients_.indexOf(client);
    if (index >= 0) {
        QString clientId = QString::number(reinterpret_cast<quintptr>(client));
        spdlog::info("IPC client disconnected: {}", clientId.toStdString());
        emit clientDisconnected(clientId);

        clients_.removeAt(index);
        readBuffers_.removeAt(index);
    }
    client->deleteLater();
}

void IPCServer::onReadyRead()
{
    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) return;

    int index = clients_.indexOf(client);
    if (index < 0) return;

    processClientData(client, index);
}

void IPCServer::processClientData(QLocalSocket* client, int clientIndex)
{
    readBuffers_[clientIndex].append(client->readAll());

    while (true) {
        int newlinePos = readBuffers_[clientIndex].indexOf('\n');
        if (newlinePos < 0) break;

        QByteArray line = readBuffers_[clientIndex].left(newlinePos).trimmed();
        readBuffers_[clientIndex].remove(0, newlinePos + 1);

        if (line.isEmpty()) continue;

        QJsonObject msg = IpcProtocol::parseMessage(line);
        if (msg.isEmpty()) continue;

        QString type = msg["type"].toString();
        QString id = msg["id"].toString();
        QJsonObject payload = msg["payload"].toObject();
        QString clientId = QString::number(reinterpret_cast<quintptr>(client));

        if (type == "hello" || type == "goodbye") {
            continue;
        }

        emit commandReceived(clientId, type, payload, id);

        if (type == "get_status") {
            QJsonObject data;
            data["message"] = "ok";
            QByteArray response = IpcProtocol::buildResponse(id, true, data);
            sendToClient(client, response);
        }
    }
}

void IPCServer::sendToClient(QLocalSocket* client, const QByteArray& message)
{
    if (client && client->isOpen()) {
        client->write(message);
        client->flush();
    }
}
