#ifndef CLI_APP_H
#define CLI_APP_H

#include <QObject>
#include <QStringList>
#include "ipc_client.h"

class CLIApp : public QObject {
    Q_OBJECT
public:
    CLIApp(const QString& ipcName = "serial_monitor_ipc");

    int run(int argc, char* argv[]);

private:
    void printHelp() const;
    void printUsage() const;
    int runInteractive(const QString& port);
    int runNonInteractive(const QStringList& args);
    int runSingleCommand(const QStringList& args);
    void handleCommand(const QString& cmd);
    void onLogReceived(const QJsonObject& log);
    void onStatusChanged(const QJsonObject& status);

    IPCClient* ipc_;
    QString ipcName_;
    bool jsonMode_;
    bool interactiveMode_;
    bool hexMode_;
    bool showTimestamp_;
    QString filter_;
    QString outputFile_;
};

#endif
