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
    void handleCommand(const QString& cmd);
    void onLogReceived(const QJsonObject& log);
    void onStatusChanged(const QJsonObject& status);
    void onResponseReceived(const QString& id, bool success, const QJsonObject& data);
    void addPending();
    void donePending();
    QString nextReqId();

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
};

#endif