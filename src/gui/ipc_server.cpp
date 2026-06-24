#include "ipc_server.h"
#include "ipc_protocol.h"
#include "serial_engine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSerialPortInfo>
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

    QJsonObject payload;
    payload["message"] = "server shutting down";
    QByteArray msg = IpcProtocol::buildMessage("server_shutdown", payload);
    for (auto* client : clients_) {
        if (client && client->isOpen()) {
            sendToClient(client, msg);
            client->disconnectFromServer();
        }
    }

    server_->close();
    spdlog::info("IPC server stopped");
}

void IPCServer::broadcastLog(const LogEntry& entry, TabType tabType)
{
    QByteArray message = IpcProtocol::buildMessage(
        "log_entry", IpcProtocol::buildLogEntryMessage(entry, tabType));

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

int IPCServer::clientIdOf(QLocalSocket* client)
{
    if (!client) return -1;
    return client->property("ipcClientId").toInt();
}

void IPCServer::sendResponse(const QString& clientId, const QString& requestId,
                              bool success, const QJsonObject& data)
{
    bool ok = false;
    int id = clientId.toInt(&ok);
    if (!ok) return;

    QLocalSocket* target = nullptr;
    for (auto* client : clients_) {
        if (clientIdOf(client) == id) {
            target = client;
            break;
        }
    }
    if (!target) return;

    QByteArray response = IpcProtocol::buildResponse(requestId, success, data);
    sendToClient(target, response);
}

int IPCServer::clientCount() const
{
    return clients_.size();
}

void IPCServer::subscribeTab(const QString& clientId, int tabIndex)
{
    tabSubscriptions_[clientId] = tabIndex;
    spdlog::info("IPC client {} subscribed to tab {}", clientId.toStdString(), tabIndex);
}

void IPCServer::unsubscribeTab(const QString& clientId)
{
    tabSubscriptions_.remove(clientId);
    spdlog::info("IPC client {} unsubscribed", clientId.toStdString());
}

bool IPCServer::isSubscribed(const QString& clientId, int tabIndex) const
{
    auto it = tabSubscriptions_.constFind(clientId);
    return it != tabSubscriptions_.constEnd() && it.value() == tabIndex;
}

void IPCServer::sendTerminalOutput(int tabIndex, const QByteArray& data, TabType tabType)
{
    if (tabSubscriptions_.isEmpty()) return;

    QByteArray message = IpcProtocol::buildMessage(
        "terminal_output",
        IpcProtocol::buildTerminalOutputMessage(tabIndex, data, tabType));

    for (auto it = tabSubscriptions_.constBegin(); it != tabSubscriptions_.constEnd(); ++it) {
        if (it.value() != tabIndex) continue;
        bool ok = false;
        int id = it.key().toInt(&ok);
        if (!ok) continue;
        for (auto* client : clients_) {
            if (clientIdOf(client) == id) {
                sendToClient(client, message);
                break;
            }
        }
    }
}

void IPCServer::onNewConnection()
{
    while (server_->hasPendingConnections()) {
        QLocalSocket* client = server_->nextPendingConnection();
        clients_.append(client);
        readBuffers_.append(QByteArray());

        connect(client, &QLocalSocket::disconnected, this, &IPCServer::onDisconnected);
        connect(client, &QLocalSocket::readyRead, this, &IPCServer::onReadyRead);

        int id = nextClientId_++;
        client->setProperty("ipcClientId", id);
        QString clientId = QString::number(id);
        spdlog::info("IPC client connected: {}", clientId.toStdString());
        emit clientConnected(clientId);

        QJsonObject welcomePayload;
        welcomePayload["server_version"] = "1.2.0";
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
        QString clientId = QString::number(clientIdOf(client));
        spdlog::info("IPC client disconnected: {}", clientId.toStdString());
        emit clientDisconnected(clientId);
        // 清理终端输出订阅
        tabSubscriptions_.remove(clientId);
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
        QString clientId = QString::number(clientIdOf(client));

        if (type == "hello" || type == "goodbye" || type == "welcome") {
            continue;
        }

        emit commandReceived(clientId, type, payload, id);
    }
}

void IPCServer::sendToClient(QLocalSocket* client, const QByteArray& message)
{
    if (client && client->isOpen()) {
        client->write(message);
        client->flush();
    }
}