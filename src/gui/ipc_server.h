#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QVector>
#include "log_buffer.h"
#include "serial_engine.h"
#include "tab_type.h"

class IPCServer : public QObject {
    Q_OBJECT
public:
    explicit IPCServer(QObject* parent = nullptr);
    ~IPCServer();

    bool start(const QString& name = "serial_monitor_ipc");
    void stop();
    void broadcastLog(const LogEntry& entry,
                   TabType tabType = TabType::Serial);
    void broadcastStatus(const QString& port, bool connected,
                         const SerialConfig& config, qint64 rxBytes,
                         qint64 txBytes, qint64 uptimeSeconds);
    void sendResponse(const QString& clientId, const QString& requestId,
                      bool success, const QJsonObject& data);
    int clientCount() const;

    /// 终端输出订阅: 客户端订阅指定 Tab 的输出流
    /// 订阅后, 仅该客户端收到对应 Tab 的 terminal_output 消息
    void subscribeTab(const QString& clientId, int tabIndex);
    void unsubscribeTab(const QString& clientId);
    bool isSubscribed(const QString& clientId, int tabIndex) const;

    /// 定向推送终端输出给所有订阅了该 Tab 的客户端
    void sendTerminalOutput(int tabIndex, const QByteArray& data, TabType tabType);

signals:
    void clientConnected(const QString& clientId);
    void clientDisconnected(const QString& clientId);
    void commandReceived(const QString& clientId, const QString& command,
                         const QJsonObject& params, const QString& requestId);

private slots:
    void onNewConnection();
    void onDisconnected();
    void onReadyRead();

private:
    QLocalServer* server_;
    QVector<QLocalSocket*> clients_;
    QVector<QByteArray> readBuffers_;
    int nextClientId_ = 1;

    /// 终端输出订阅表: clientId → 订阅的 tabIndex
    /// 一个客户端同一时间只订阅一个 Tab (符合"AI 持久操作一个终端"场景)
    QHash<QString, int> tabSubscriptions_;

    static int clientIdOf(QLocalSocket* client);

    void sendToClient(QLocalSocket* client, const QByteArray& message);
    void processClientData(QLocalSocket* client, int clientIndex);
};

#endif
