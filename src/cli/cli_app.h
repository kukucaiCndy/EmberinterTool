#ifndef CLI_APP_H
#define CLI_APP_H

#include <QObject>
#include <QStringList>
#include <QCoreApplication>
#include <QJsonObject>
#include <QSocketNotifier>
#include "ipc_client.h"

class CLIApp : public QObject {
    Q_OBJECT
public:
    CLIApp(const QString& ipcName = "serial_monitor_ipc");

    int run(int argc, char* argv[]);

private:
    void printHelp() const;
    void printUsage() const;
    int startInteractive(const QString& port);
    /// 终端交互模式: 订阅指定 Tab, 持续接收输出 + 发送输入
    int startTerminalInteractive(int tabIndex);
    void handleCommand(const QString& cmd);
    void handleTerminalCommand(const QString& cmd);
    void onLogReceived(const QJsonObject& log);
    void onStatusChanged(const QJsonObject& status);
    void onResponseReceived(const QString& id, bool success, const QJsonObject& data);
    void onTerminalOutput(int tabIndex, const QByteArray& data, const QString& tabType);
    void addPending();
    void donePending();
    QString nextReqId();
    /// 发送终端输入 (Base64 编码保留原始字节)
    void sendTerminalInput(int tabIndex, const QByteArray& data);

private slots:
    void onStdinActivated();

private:
    IPCClient* ipc_;
    QString ipcName_;
    bool jsonMode_;
    bool interactiveMode_;
    bool hexMode_;
    bool showTimestamp_;
    QString filter_;
    QString outputFile_;
    QString listenPort_;
    QCoreApplication* app_ = nullptr;
    int pendingRequestCount_ = 0;
    int reqCounter_ = 0;
    bool shouldQuit_ = false;
    QSocketNotifier* stdinNotifier_ = nullptr;
    QByteArray stdinBuffer_;

    // 终端交互模式状态
    bool terminalMode_ = false;        // 是否处于终端交互模式
    int terminalTabIndex_ = -1;        // 当前操作的 Tab 索引
    bool terminalRawMode_ = false;     // 原始字节输出模式 (不解析 ANSI)
};

#endif