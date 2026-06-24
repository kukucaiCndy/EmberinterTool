#include "cli_app.h"
#include "ipc_protocol.h"
#include "log_parser.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QRegularExpression>
#include <iostream>
#include <cstdio>
#ifdef Q_OS_WIN
#  include <io.h>
#else
#  include <unistd.h>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[91m"
#define COLOR_GREEN   "\033[92m"
#define COLOR_YELLOW  "\033[93m"
#define COLOR_CYAN    "\033[96m"
#define COLOR_GRAY    "\033[90m"

static void printColored(QTextStream& out, const QString& text, const QString& level)
{
    QString color;
    if (level == "ERROR")      color = COLOR_RED;
    else if (level == "WARN")  color = COLOR_YELLOW;
    else if (level == "INFO")  color = COLOR_GREEN;
    else if (level == "DEBUG") color = COLOR_CYAN;
    else if (level == "TRACE") color = COLOR_GRAY;
    else                       color = COLOR_RESET;
    out << color << text << COLOR_RESET << Qt::endl;
}

CLIApp::CLIApp(const QString& ipcName)
    : ipc_(new IPCClient(this))
    , ipcName_(ipcName)
    , jsonMode_(false)
    , interactiveMode_(false)
    , hexMode_(false)
    , showTimestamp_(true)
    , pendingRequestCount_(0)
    , shouldQuit_(false)
{
    connect(ipc_, &IPCClient::logReceived, this, &CLIApp::onLogReceived);
    connect(ipc_, &IPCClient::statusChanged, this, &CLIApp::onStatusChanged);
    connect(ipc_, &IPCClient::responseReceived,
            this, &CLIApp::onResponseReceived);
    connect(ipc_, &IPCClient::terminalOutputReceived,
            this, &CLIApp::onTerminalOutput);
    connect(ipc_, &IPCClient::errorOccurred, [](const QString& err) {
        QTextStream out(stdout);
        out << COLOR_RED << "[ERROR] " << err << COLOR_RESET << Qt::endl;
    });
}

int CLIApp::run(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::off);

    QCoreApplication app(argc, argv);
    app_ = &app;
    app.setApplicationName("EmberInterDebugTool-cli");
    app.setApplicationVersion("1.3.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("EmberInterDebugTool - 尘智串口调试工具 CLI");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(QCommandLineOption(QStringList({"p", "port"}), "串口名称", "PORT"));
    parser.addOption(QCommandLineOption(QStringList({"b", "baudrate"}), "波特率 (默认: 115200)", "RATE", "115200"));
    parser.addOption(QCommandLineOption(QStringList({"f", "filter"}), "过滤关键词", "KEYWORD"));
    parser.addOption(QCommandLineOption(QStringList({"o", "output"}), "保存日志到文件", "FILE"));
    parser.addOption(QCommandLineOption("hex", "HEX显示模式"));
    parser.addOption(QCommandLineOption("no-timestamp", "不显示时间戳"));
    parser.addOption(QCommandLineOption("clear", "启动时清空日志"));
    parser.addOption(QCommandLineOption("list", "列出可用串口"));
    parser.addOption(QCommandLineOption("cli", "交互CLI模式"));
    parser.addOption(QCommandLineOption("json", "JSON输出模式"));
    parser.addOption(QCommandLineOption("ipc", "IPC服务名", "NAME", "serial_monitor_ipc"));
    parser.addOption(QCommandLineOption("send", "发送数据后退出", "DATA"));
    parser.addOption(QCommandLineOption("send-hex", "发送HEX数据后退出", "HEX"));
    parser.addOption(QCommandLineOption("send-file", "发送二进制文件后退出", "FILE"));
    parser.addOption(QCommandLineOption("connect", "连接串口", "PORT"));
    parser.addOption(QCommandLineOption("get-logs", "获取最近N条日志", "COUNT"));
    parser.addOption(QCommandLineOption("get-status", "获取连接状态"));
    parser.addOption(QCommandLineOption("pause", "暂停日志输出"));
    parser.addOption(QCommandLineOption("resume", "恢复日志输出"));
    parser.addOption(QCommandLineOption("activate-window", "激活 GUI 窗口"));

    // ── 终端管理命令 (新增) ──
    parser.addOption(QCommandLineOption("create-terminal",
        "创建本地终端 Tab (shell 可选: cmd.exe/powershell.exe/bash), 返回 tab index", "SHELL", "cmd.exe"));
    parser.addOption(QCommandLineOption("create-ssh",
        "创建 SSH 终端 Tab, 格式: user@host 或单独用 --ssh-host/--ssh-user", "TARGET"));
    parser.addOption(QCommandLineOption("ssh-host", "SSH 主机", "HOST"));
    parser.addOption(QCommandLineOption("ssh-user", "SSH 用户名", "USER"));
    parser.addOption(QCommandLineOption("ssh-port", "SSH 端口 (默认 22)", "PORT", "22"));
    parser.addOption(QCommandLineOption("list-tabs", "列出所有 Tab"));
    parser.addOption(QCommandLineOption("switch-tab", "切换活动 Tab", "INDEX"));
    parser.addOption(QCommandLineOption("close-tab", "关闭指定 Tab", "INDEX"));
    parser.addOption(QCommandLineOption("terminal-input",
        "向终端 Tab 发送文本 (配合 --tab-index)", "TEXT"));
    parser.addOption(QCommandLineOption("tab-index",
        "指定目标 Tab 索引 (默认当前)", "INDEX"));
    parser.addOption(QCommandLineOption("subscribe-tab",
        "订阅终端 Tab 输出并进入交互模式 (像人一样持续操作)", "INDEX"));
    parser.addOption(QCommandLineOption("raw-output",
        "终端输出原始字节模式 (不尝试解析, 配合 --subscribe-tab)"));

    parser.process(app);

    jsonMode_ = parser.isSet("json");
    hexMode_ = parser.isSet("hex");
    showTimestamp_ = !parser.isSet("no-timestamp");
    filter_ = parser.value("f");
    outputFile_ = parser.value("o");
    ipcName_ = parser.value("ipc");
    listenPort_ = parser.value("p");

    bool needsIpc = parser.isSet("list") || parser.isSet("get-status") ||
                    parser.isSet("get-logs") || parser.isSet("send") ||
                    parser.isSet("send-hex") ||
                    parser.isSet("send-file") ||
                    parser.isSet("connect") || parser.isSet("cli") ||
                    parser.isSet("pause") || parser.isSet("resume") ||
                    parser.isSet("activate-window") ||
                    parser.isSet("create-terminal") || parser.isSet("create-ssh") ||
                    parser.isSet("list-tabs") || parser.isSet("switch-tab") ||
                    parser.isSet("close-tab") || parser.isSet("terminal-input") ||
                    parser.isSet("subscribe-tab") ||
                    !parser.value("p").isEmpty();

    if (!needsIpc) {
        printUsage();
        return 1;
    }

    if (!ipc_->connectToServer(ipcName_)) {
        QTextStream err(stderr);
        err << "错误: GUI 服务未运行 (IPC: " << ipcName_ << ")" << Qt::endl;
        err << "请先启动 GUI 程序" << Qt::endl;
        return 2;
    }

    if (parser.isSet("clear")) {
        ipc_->sendCommand("clear_logs", QJsonObject());
    }

    // 非交互命令最多等待 5 秒响应, 避免响应延迟时提前退出
    QTimer::singleShot(5000, [this]() {
        if (pendingRequestCount_ == 0 && !interactiveMode_) {
            shouldQuit_ = true;
            QCoreApplication::quit();
        }
    });

    if (parser.isSet("list")) {
        QJsonObject params;
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("list_ports", params, reqId);
    }

    else if (parser.isSet("get-status")) {
        QJsonObject params;
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("get_status", params, reqId);
    }

    else if (parser.isSet("get-logs")) {
        bool ok = false;
        int count = parser.value("get-logs").toInt(&ok);
        if (!ok || count <= 0) {
            QTextStream err(stderr);
            err << "错误: --get-logs 需要正整数参数" << Qt::endl;
            return 1;
        }
        QJsonObject params;
        params["count"] = count;
        params["filter"] = parser.value("f");
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("get_logs", params, reqId);
    }

    else if (parser.isSet("send")) {
        QString data = parser.value("send");
        QJsonObject params;
        params["data"] = data;
        params["port"] = parser.value("p");
        params["append"] = "CRLF";
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("send_text", params, reqId);
    }

    else if (parser.isSet("send-file")) {
        QString filePath = parser.value("send-file");
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QTextStream err(stderr);
            err << "错误: 无法打开文件 " << filePath << Qt::endl;
            return 3;
        }
        QByteArray binaryData = file.readAll();
        file.close();
        QJsonObject params;
        params["data"] = QString::fromLatin1(binaryData.toBase64());
        params["port"] = parser.value("p");
        params["filename"] = QFileInfo(filePath).fileName();
        params["size"] = binaryData.size();
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("send_file", params, reqId);
    }

    else if (parser.isSet("send-hex")) {
        QString hexStr = parser.value("send-hex");
        hexStr.remove(QRegularExpression("\\s"));
        static const QRegularExpression hexRe("^[0-9A-Fa-f]+$");
        if (hexStr.isEmpty() || hexStr.length() % 2 != 0 || !hexRe.match(hexStr).hasMatch()) {
            QTextStream err(stderr);
            err << "错误: --send-hex 参数必须是偶数长度的十六进制字符串" << Qt::endl;
            return 1;
        }
        QJsonObject params;
        params["data"] = hexStr;
        params["port"] = parser.value("p");
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("send_hex", params, reqId);
    }

    else if (parser.isSet("connect")) {
        QString port = parser.value("connect");
        bool ok = false;
        int baud = parser.value("baudrate").toInt(&ok);
        if (!ok || baud <= 0) {
            QTextStream err(stderr);
            err << "错误: --baudrate 需要正整数参数" << Qt::endl;
            return 1;
        }
        QJsonObject params;
        params["port"] = port;
        params["baudrate"] = baud;
        QString reqId = nextReqId();
        addPending();
        ipc_->sendCommand("connect", params, reqId);
    }

    else if (parser.isSet("pause")) {
        addPending();
        ipc_->sendCommand("pause", QJsonObject(), nextReqId());
    }

    else if (parser.isSet("resume")) {
        addPending();
        ipc_->sendCommand("resume", QJsonObject(), nextReqId());
    }

    else if (parser.isSet("activate-window")) {
        addPending();
        ipc_->sendCommand("activate_window", QJsonObject(), nextReqId());
    }

    // ── 终端管理命令 ──

    else if (parser.isSet("list-tabs")) {
        addPending();
        ipc_->sendCommand("list_tabs", QJsonObject(), nextReqId());
    }

    else if (parser.isSet("switch-tab")) {
        bool ok = false;
        int index = parser.value("switch-tab").toInt(&ok);
        if (!ok || index < 0) {
            QTextStream err(stderr);
            err << "错误: --switch-tab 需要非负整数参数" << Qt::endl;
            return 1;
        }
        QJsonObject params;
        params["index"] = index;
        addPending();
        ipc_->sendCommand("switch_tab", params, nextReqId());
    }

    else if (parser.isSet("close-tab")) {
        bool ok = false;
        int index = parser.value("close-tab").toInt(&ok);
        if (!ok || index < 0) {
            QTextStream err(stderr);
            err << "错误: --close-tab 需要非负整数参数" << Qt::endl;
            return 1;
        }
        QJsonObject params;
        params["index"] = index;
        addPending();
        ipc_->sendCommand("close_tab", params, nextReqId());
    }

    else if (parser.isSet("create-terminal")) {
        QString shell = parser.value("create-terminal");
        QJsonObject params;
        params["type"] = "cmd";
        params["shell"] = shell;
        addPending();
        ipc_->sendCommand("create_tab", params, nextReqId());
    }

    else if (parser.isSet("create-ssh")) {
        QString target = parser.value("create-ssh");
        QString host = parser.value("ssh-host");
        QString user = parser.value("ssh-user");
        int port = parser.value("ssh-port").toInt();

        // 支持 user@host 格式
        if (!target.isEmpty() && target.contains('@')) {
            int at = target.indexOf('@');
            user = target.left(at);
            host = target.mid(at + 1);
        } else if (!target.isEmpty() && host.isEmpty()) {
            host = target;
        }

        if (host.isEmpty()) {
            QTextStream err(stderr);
            err << "错误: --create-ssh 需要 user@host 或 --ssh-host 参数" << Qt::endl;
            return 1;
        }

        QJsonObject params;
        params["type"] = "ssh";
        params["host"] = host;
        params["user"] = user;
        params["port"] = port;
        addPending();
        ipc_->sendCommand("create_tab", params, nextReqId());
    }

    else if (parser.isSet("terminal-input")) {
        QString text = parser.value("terminal-input");
        int tabIndex = -1;
        if (parser.isSet("tab-index")) {
            tabIndex = parser.value("tab-index").toInt();
        }
        QJsonObject params;
        if (tabIndex >= 0) params["tab_index"] = tabIndex;
        // 自动追加 \r\n, 否则命令不会被执行
        params["text"] = text + "\r\n";
        addPending();
        ipc_->sendCommand("terminal_input", params, nextReqId());
    }

    else if (parser.isSet("subscribe-tab")) {
        bool ok = false;
        int index = parser.value("subscribe-tab").toInt(&ok);
        if (!ok || index < 0) {
            QTextStream err(stderr);
            err << "错误: --subscribe-tab 需要非负整数参数" << Qt::endl;
            return 1;
        }
        terminalRawMode_ = parser.isSet("raw-output");
        // 进入终端交互模式 (不退出)
        QTimer::singleShot(0, [this, index]() { startTerminalInteractive(index); });
        return app.exec();
    }

    else if (parser.isSet("cli")) {
        QString port = parser.value("p");
        if (port.isEmpty()) {
            QTextStream out(stdout);
            out << "错误: --cli 需要 --port 参数" << Qt::endl;
            return 1;
        }
        QTimer::singleShot(0, [this, port]() { startInteractive(port); });
        return app.exec();
    }

    else if (!parser.value("p").isEmpty()) {
        interactiveMode_ = true;
        QTimer::singleShot(0, [this]() {
            QTextStream out(stdout);
            out << COLOR_GREEN << "[系统] 正在监听串口日志，按 Ctrl+C 停止。"
                << COLOR_RESET << Qt::endl;
        });
        QObject::connect(&app, &QCoreApplication::aboutToQuit, [this]() {
            QTextStream out(stdout);
            out << COLOR_GRAY << "[系统] CLI 已断开 (GUI 服务继续运行)"
                << COLOR_RESET << Qt::endl;
        });
        return app.exec();
    }

    else {
        printUsage();
        return 1;
    }

    int result = app.exec();
    return result;
}

void CLIApp::addPending()
{
    pendingRequestCount_++;
}

void CLIApp::donePending()
{
    pendingRequestCount_--;
    if (pendingRequestCount_ <= 0 && shouldQuit_ && app_) {
        QTimer::singleShot(0, app_, &QCoreApplication::quit);
    }
}

QString CLIApp::nextReqId()
{
    return QString("req_%1").arg(++reqCounter_);
}

int CLIApp::startInteractive(const QString& port)
{
    interactiveMode_ = true;

    QTextStream out(stdout);
    out << COLOR_CYAN << QString(60, '=') << COLOR_RESET << Qt::endl;
    out << COLOR_CYAN << "  EmberInterDebugTool v1.3.1 - 尘智 | 微尘藏星火,终端蕴尘智" << COLOR_RESET << Qt::endl;
    out << COLOR_GRAY << "  Port: " << port << COLOR_RESET << Qt::endl;
    out << COLOR_CYAN << QString(60, '=') << COLOR_RESET << Qt::endl;
    out << Qt::endl;
    out << COLOR_GREEN << "[系统] 输入 'help' 查看命令, 'quit' 退出"
        << COLOR_RESET << Qt::endl << Qt::endl;
    out << "> " << Qt::flush;

    // 使用 QSocketNotifier 异步读取 stdin，避免阻塞 Qt 事件循环
    // 这样 IPC 信号 (logReceived/statusChanged/responseReceived) 才能被处理
    stdinNotifier_ = new QSocketNotifier(fileno(stdin), QSocketNotifier::Read, this);
    connect(stdinNotifier_, &QSocketNotifier::activated,
            this, &CLIApp::onStdinActivated);
    stdinNotifier_->setEnabled(true);
    return 0;
}

void CLIApp::onStdinActivated()
{
    char buf[4096];
    ssize_t n = ::read(fileno(stdin), buf, sizeof(buf));
    if (n <= 0) {
        // EOF 或错误：停止通知并退出
        if (stdinNotifier_) {
            stdinNotifier_->setEnabled(false);
        }
        if (app_) {
            QCoreApplication::quit();
        }
        return;
    }

    stdinBuffer_.append(buf, n);

    // 处理缓冲区中所有完整行
    int idx;
    while ((idx = stdinBuffer_.indexOf('\n')) >= 0) {
        QByteArray line = stdinBuffer_.left(idx);
        stdinBuffer_.remove(0, idx + 1);
        QString cmd = QString::fromLocal8Bit(line).trimmed();
        if (cmd.isEmpty()) {
            continue;
        }
        // 终端交互模式: 不识别 q/quit (避免误触), 用 :quit
        if (terminalMode_) {
            if (cmd == ":q" || cmd == ":quit" || cmd == ":exit") {
                if (stdinNotifier_) stdinNotifier_->setEnabled(false);
                QTextStream out(stdout);
                out << COLOR_GRAY << "[SYSTEM] 终端交互模式退出 (GUI 服务继续运行)"
                    << COLOR_RESET << Qt::endl;
                if (app_) QCoreApplication::quit();
                return;
            }
            handleTerminalCommand(cmd);
            continue;
        }
        // 串口交互模式
        if (cmd == "q" || cmd == "quit" || cmd == "exit") {
            if (stdinNotifier_) {
                stdinNotifier_->setEnabled(false);
            }
            QTextStream out(stdout);
            out << COLOR_GRAY << "[SYSTEM] CLI disconnected (GUI service continues)"
                << COLOR_RESET << Qt::endl;
            if (app_) {
                QCoreApplication::quit();
            }
            return;
        }
        handleCommand(cmd);
    }
}

void CLIApp::handleCommand(const QString& cmd)
{
    QTextStream out(stdout);

    if (cmd == "h" || cmd == "help" || cmd == "?") {
        printHelp();
        return;
    }
    if (cmd == "c" || cmd == "clear") {
        ipc_->sendCommand("clear_logs");
        out << COLOR_GREEN << "[系统] 日志已清空" << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "s" || cmd == "status") {
        ipc_->sendCommand("get_status", QJsonObject(), nextReqId());
        return;
    }
    if (cmd == "list" || cmd == "ls") {
        ipc_->sendCommand("list_ports", QJsonObject(), nextReqId());
        return;
    }
    // Tab 管理快捷命令
    if (cmd == "tabs") {
        ipc_->sendCommand("list_tabs", QJsonObject(), nextReqId());
        return;
    }
    if (cmd.startsWith("tab ")) {
        bool ok = false;
        int idx = cmd.mid(4).trimmed().toInt(&ok);
        if (ok && idx >= 0) {
            QJsonObject params;
            params["index"] = idx;
            ipc_->sendCommand("switch_tab", params, nextReqId());
            return;
        }
    }
    if (cmd.startsWith("close ")) {
        bool ok = false;
        int idx = cmd.mid(6).trimmed().toInt(&ok);
        if (ok && idx >= 0) {
            QJsonObject params;
            params["index"] = idx;
            ipc_->sendCommand("close_tab", params, nextReqId());
            return;
        }
    }
    if (cmd.startsWith("term")) {
        // term / term bash / term powershell.exe
        QString shell = "cmd.exe";
        if (cmd.startsWith("term ")) {
            shell = cmd.mid(5).trimmed();
            if (shell.isEmpty()) shell = "cmd.exe";
        }
        QJsonObject params;
        params["type"] = "cmd";
        params["shell"] = shell;
        ipc_->sendCommand("create_tab", params, nextReqId());
        return;
    }
    if (cmd.startsWith("ssh ")) {
        // ssh user@host [port]
        QStringList parts = cmd.mid(4).split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            out << COLOR_RED << "[错误] 用法: ssh user@host [port]" << COLOR_RESET << Qt::endl;
            return;
        }
        QString target = parts[0];
        int port = parts.size() > 1 ? parts[1].toInt() : 22;
        QString user, host;
        if (target.contains('@')) {
            int at = target.indexOf('@');
            user = target.left(at);
            host = target.mid(at + 1);
        } else {
            host = target;
        }
        QJsonObject params;
        params["type"] = "ssh";
        params["host"] = host;
        params["user"] = user;
        params["port"] = port;
        ipc_->sendCommand("create_tab", params, nextReqId());
        return;
    }
    if (cmd.startsWith("sub ")) {
        bool ok = false;
        int idx = cmd.mid(4).trimmed().toInt(&ok);
        if (ok && idx >= 0) {
            // 切换到终端交互模式
            terminalMode_ = true;
            terminalTabIndex_ = idx;
            QJsonObject params;
            params["index"] = idx;
            ipc_->sendCommand("subscribe_tab", params, nextReqId());
            out << COLOR_GREEN << "[系统] 已订阅 Tab " << idx
                << ", 进入终端交互模式 (用 :help 查看命令)" << COLOR_RESET << Qt::endl;
            return;
        }
    }
    if (cmd.startsWith("input ")) {
        QString text = cmd.mid(6);
        QJsonObject params;
        params["text"] = text;
        ipc_->sendCommand("terminal_input", params, nextReqId());
        return;
    }
    if (cmd == "hex") {
        hexMode_ = true;
        out << COLOR_GREEN << "[系统] HEX 模式已开启" << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "text") {
        hexMode_ = false;
        out << COLOR_GREEN << "[系统] 文本模式已开启" << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd == "ts" || cmd == "timestamp") {
        showTimestamp_ = !showTimestamp_;
        out << COLOR_GREEN << "[系统] 时间戳: "
            << (showTimestamp_ ? "开" : "关") << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("send ")) {
        QJsonObject params;
        params["data"] = cmd.mid(5);
        params["append"] = "CRLF";
        ipc_->sendCommand("send_text", params);
        out << COLOR_GREEN << ">>> 发送: " << cmd.mid(5) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("sendhex ")) {
        QJsonObject params;
        params["data"] = cmd.mid(8);
        ipc_->sendCommand("send_hex", params);
        out << COLOR_GREEN << ">>> HEX 发送: " << cmd.mid(8) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("sendfile ")) {
        QString filePath = cmd.mid(9).trimmed();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            out << COLOR_RED << "[错误] 无法打开文件: " << filePath << COLOR_RESET << Qt::endl;
            return;
        }
        QByteArray binaryData = file.readAll();
        file.close();
        QJsonObject params;
        params["data"] = QString::fromLatin1(binaryData.toBase64());
        params["filename"] = QFileInfo(filePath).fileName();
        params["size"] = binaryData.size();
        ipc_->sendCommand("send_file", params);
        out << COLOR_GREEN << ">>> 发送文件: " << QFileInfo(filePath).fileName()
            << " (" << binaryData.size() << " 字节)" << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("filter ")) {
        QString kw = cmd.mid(7).trimmed();
        filter_ = kw;
        QJsonObject params;
        params["keyword"] = kw;
        ipc_->sendCommand("set_filter", params);
        out << COLOR_GREEN << "[系统] 过滤: "
            << (kw.isEmpty() ? "无" : kw) << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("export ")) {
        QJsonObject params;
        params["file_path"] = cmd.mid(7).trimmed();
        params["format"] = "json";
        ipc_->sendCommand("export_logs", params);
        out << COLOR_GREEN << "[系统] 正在导出..." << COLOR_RESET << Qt::endl;
        return;
    }
    if (cmd.startsWith("connect ")) {
        QStringList parts = cmd.mid(8).split(' ', Qt::SkipEmptyParts);
        QJsonObject params;
        params["port"] = parts.value(0);
        params["baudrate"] = parts.value(1, "115200").toInt();
        ipc_->sendCommand("connect", params);
        return;
    }
    if (cmd == "disconnect" || cmd == "disc") {
        ipc_->sendCommand("disconnect");
        out << COLOR_GREEN << "[系统] 正在断开..." << COLOR_RESET << Qt::endl;
        return;
    }

    QJsonObject params;
    params["data"] = cmd;
    params["append"] = "CRLF";
    ipc_->sendCommand("send_text", params);
    out << COLOR_GREEN << ">>> 发送: " << cmd << COLOR_RESET << Qt::endl;
}

void CLIApp::onLogReceived(const QJsonObject& log)
{
    QTextStream out(stdout);
    QString text = log["text"].toString();
    QString level = log["level"].toString();
    QString port = log["port"].toString();

    if (!listenPort_.isEmpty() && port != listenPort_) {
        return;
    }

    if (!filter_.isEmpty() && !text.contains(filter_, Qt::CaseInsensitive)) {
        return;
    }

    if (jsonMode_) {
        QJsonDocument doc(log);
        out << doc.toJson(QJsonDocument::Compact) << Qt::endl;
        return;
    }

    if (hexMode_) {
        QByteArray raw = QByteArray::fromHex(log["raw_hex"].toString().toLatin1());
        if (!raw.isEmpty()) {
            out << LogParser::formatHex(raw, 0) << Qt::endl;
        }
        return;
    }

    LogEntry entry(log["timestamp"].toString(), level, text,
                   QByteArray(), log["port"].toString());
    QString display = LogParser::formatDisplay(entry, showTimestamp_);
    printColored(out, display, level);

    if (!outputFile_.isEmpty()) {
        QFile f(outputFile_);
        if (f.open(QIODevice::Append)) {
            f.write(display.toUtf8() + "\n");
            f.close();
        }
    }
}

void CLIApp::onStatusChanged(const QJsonObject& status)
{
    QTextStream out(stdout);
    QString port = status["port"].toString();
    bool connected = status["connected"].toBool();

    if (jsonMode_) {
        QJsonDocument doc(status);
        out << doc.toJson(QJsonDocument::Compact) << Qt::endl;
        return;
    }

    out << COLOR_GREEN << "[系统] " << port << " "
        << (connected ? "已连接" : "已断开") << COLOR_RESET << Qt::endl;
}

void CLIApp::onResponseReceived(const QString& id, bool success, const QJsonObject& data)
{
    Q_UNUSED(id);
    donePending();

    QTextStream out(stdout);

    if (data.contains("ports")) {
        QJsonArray ports = data["ports"].toArray();
        if (ports.isEmpty()) {
            out << COLOR_GRAY << "[系统] 未找到串口设备" << COLOR_RESET << Qt::endl;
        } else {
            for (const auto& val : ports) {
                QJsonObject p = val.toObject();
                QString marker = p["recommended"].toBool() ? " *" : "  ";
                out << COLOR_GREEN << marker << COLOR_RESET
                    << p["name"].toString().leftJustified(8)
                    << COLOR_GRAY << p["description"].toString()
                    << COLOR_RESET << Qt::endl;
            }
        }
        return;
    }

    // list_tabs 响应
    if (data.contains("tabs")) {
        QJsonArray tabs = data["tabs"].toArray();
        if (tabs.isEmpty()) {
            out << COLOR_GRAY << "[系统] 当前无 Tab" << COLOR_RESET << Qt::endl;
        } else {
            out << COLOR_CYAN << "  Idx  Type   Conn   Title" << COLOR_RESET << Qt::endl;
            for (const auto& val : tabs) {
                QJsonObject t = val.toObject();
                int idx = t["index"].toInt();
                QString type = t["type"].toString();
                QString title = t["title"].toString();
                bool conn = t["connected"].toBool();
                bool isCur = t["is_current"].toBool();
                QString marker = isCur ? QString(COLOR_GREEN) + ">" : " ";
                out << marker
                    << QString(" %1  ").arg(idx, 3)
                    << type.leftJustified(6)
                    << (conn ? "ON  " : "OFF ")
                    << title
                    << (isCur ? COLOR_RESET : "")
                    << Qt::endl;
            }
        }
        return;
    }

    // create_tab 响应: 显示返回的 tab_index
    if (data.contains("tab_index") && success) {
        int tabIndex = data["tab_index"].toInt();
        QString tabType = data["tab_type"].toString();
        out << COLOR_GREEN << "[OK] " << data["message"].toString()
            << " (tab_index=" << tabIndex << ", type=" << tabType << ")"
            << COLOR_RESET << Qt::endl;
        return;
    }

    if (data.contains("total_count")) {
        int total = data["total_count"].toInt();
        int returned = data["returned_count"].toInt();
        QJsonArray entries = data["entries"].toArray();

        out << COLOR_GRAY << "[系统] 日志缓冲区: " << total
            << " 条, 显示 " << returned << COLOR_RESET << Qt::endl;

        for (const auto& val : entries) {
            QJsonObject entry = val.toObject();
            QString timestamp = entry["timestamp"].toString();
            QString level = entry["level"].toString();
            QString text = entry["text"].toString();

            if (jsonMode_) {
                QJsonDocument doc(entry);
                out << doc.toJson(QJsonDocument::Compact) << Qt::endl;
            } else {
                LogEntry e(timestamp, level, text, QByteArray(), entry["port"].toString());
                QString display = LogParser::formatDisplay(e, showTimestamp_);
                printColored(out, display, level);
            }
        }
        return;
    }

    if (data.contains("port") && !data.contains("entries")) {
        out << COLOR_GREEN << "[系统] 串口: " << data["port"].toString()
            << " | 连接: " << (data["connected"].toBool() ? "是" : "否")
            << " | 缓冲: " << data["buffer_size"].toInt()
            << " | RX: " << data["rx_bytes"].toDouble()
            << " | TX: " << data["tx_bytes"].toDouble()
            << COLOR_RESET << Qt::endl;
        return;
    }

    out << (success ? COLOR_GREEN : COLOR_RED)
        << "[" << (success ? "OK" : "FAIL") << "] "
        << data["message"].toString()
        << COLOR_RESET << Qt::endl;
}

// ── 终端交互模式 ──

int CLIApp::startTerminalInteractive(int tabIndex)
{
    terminalMode_ = true;
    terminalTabIndex_ = tabIndex;
    interactiveMode_ = true;

    // 先订阅 Tab 输出
    QJsonObject subParams;
    subParams["index"] = tabIndex;
    ipc_->sendCommand("subscribe_tab", subParams, nextReqId());

    QTextStream out(stdout);
    out << COLOR_CYAN << QString(60, '=') << COLOR_RESET << Qt::endl;
    out << COLOR_CYAN << "  终端交互模式 - Tab #" << tabIndex << COLOR_RESET << Qt::endl;
    out << COLOR_GRAY << "  已订阅终端输出, 输入命令直接发送到终端" << COLOR_RESET << Qt::endl;
    out << COLOR_GRAY << "  特殊命令: :help :quit :tabs :tab N :close N :sub N :unsub :raw" << COLOR_RESET << Qt::endl;
    out << COLOR_CYAN << QString(60, '=') << COLOR_RESET << Qt::endl;
    out << Qt::endl;

    // 异步读取 stdin
    stdinNotifier_ = new QSocketNotifier(fileno(stdin), QSocketNotifier::Read, this);
    connect(stdinNotifier_, &QSocketNotifier::activated,
            this, &CLIApp::onStdinActivated);
    stdinNotifier_->setEnabled(true);

    QObject::connect(app_, &QCoreApplication::aboutToQuit, [this]() {
        // 退出时取消订阅
        if (terminalTabIndex_ >= 0) {
            ipc_->sendCommand("unsubscribe_tab", QJsonObject());
        }
        QTextStream out(stdout);
        out << COLOR_GRAY << "[系统] 终端交互模式已退出 (GUI 服务继续运行)"
            << COLOR_RESET << Qt::endl;
    });

    return 0;
}

void CLIApp::handleTerminalCommand(const QString& cmd)
{
    QTextStream out(stdout);

    // 特殊命令以 : 开头
    if (cmd.startsWith(":")) {
        QString sub = cmd.mid(1).trimmed();
        if (sub == "q" || sub == "quit" || sub == "exit") {
            if (stdinNotifier_) stdinNotifier_->setEnabled(false);
            if (app_) QCoreApplication::quit();
            return;
        }
        if (sub == "h" || sub == "help" || sub == "?") {
            out << COLOR_CYAN << "终端交互命令:" << COLOR_RESET << Qt::endl;
            out << "  :help / :h        显示此帮助" << Qt::endl;
            out << "  :quit / :q        退出终端交互模式" << Qt::endl;
            out << "  :tabs             列出所有 Tab" << Qt::endl;
            out << "  :tab N            切换并订阅 Tab N" << Qt::endl;
            out << "  :close N          关闭 Tab N" << Qt::endl;
            out << "  :sub N            订阅 Tab N 输出" << Qt::endl;
            out << "  :unsub            取消订阅" << Qt::endl;
            out << "  :raw              切换原始字节输出模式" << Qt::endl;
            out << COLOR_GRAY << "其他输入: 直接发送到当前终端 (自动追加 \\r\\n)" << COLOR_RESET << Qt::endl;
            return;
        }
        if (sub == "tabs") {
            ipc_->sendCommand("list_tabs", QJsonObject(), nextReqId());
            return;
        }
        if (sub.startsWith("tab ")) {
            bool ok = false;
            int idx = sub.mid(4).trimmed().toInt(&ok);
            if (!ok || idx < 0) {
                out << COLOR_RED << "[错误] 无效 Tab 索引" << COLOR_RESET << Qt::endl;
                return;
            }
            // 切换 + 订阅
            QJsonObject swParams;
            swParams["index"] = idx;
            ipc_->sendCommand("switch_tab", swParams, nextReqId());
            QJsonObject subParams;
            subParams["index"] = idx;
            ipc_->sendCommand("subscribe_tab", subParams, nextReqId());
            terminalTabIndex_ = idx;
            out << COLOR_GREEN << "[系统] 已切换并订阅 Tab " << idx << COLOR_RESET << Qt::endl;
            return;
        }
        if (sub.startsWith("close ")) {
            bool ok = false;
            int idx = sub.mid(6).trimmed().toInt(&ok);
            if (!ok || idx < 0) {
                out << COLOR_RED << "[错误] 无效 Tab 索引" << COLOR_RESET << Qt::endl;
                return;
            }
            QJsonObject params;
            params["index"] = idx;
            ipc_->sendCommand("close_tab", params, nextReqId());
            return;
        }
        if (sub.startsWith("sub ")) {
            bool ok = false;
            int idx = sub.mid(4).trimmed().toInt(&ok);
            if (!ok || idx < 0) {
                out << COLOR_RED << "[错误] 无效 Tab 索引" << COLOR_RESET << Qt::endl;
                return;
            }
            QJsonObject params;
            params["index"] = idx;
            ipc_->sendCommand("subscribe_tab", params, nextReqId());
            terminalTabIndex_ = idx;
            out << COLOR_GREEN << "[系统] 已订阅 Tab " << idx << COLOR_RESET << Qt::endl;
            return;
        }
        if (sub == "unsub") {
            ipc_->sendCommand("unsubscribe_tab", QJsonObject(), nextReqId());
            out << COLOR_GREEN << "[系统] 已取消订阅" << COLOR_RESET << Qt::endl;
            return;
        }
        if (sub == "raw") {
            terminalRawMode_ = !terminalRawMode_;
            out << COLOR_GREEN << "[系统] 原始字节模式: "
                << (terminalRawMode_ ? "开" : "关") << COLOR_RESET << Qt::endl;
            return;
        }
        out << COLOR_RED << "[错误] 未知命令: :" << sub << " (用 :help 查看)" << COLOR_RESET << Qt::endl;
        return;
    }

    // 普通输入: 发送到当前终端 (追加 \r\n)
    if (terminalTabIndex_ >= 0) {
        sendTerminalInput(terminalTabIndex_, (cmd + "\r\n").toUtf8());
    } else {
        out << COLOR_RED << "[错误] 未订阅任何 Tab, 用 :sub N 订阅" << COLOR_RESET << Qt::endl;
    }
}

void CLIApp::sendTerminalInput(int tabIndex, const QByteArray& data)
{
    QJsonObject params;
    params["tab_index"] = tabIndex;
    params["data"] = QString::fromLatin1(data.toBase64());
    ipc_->sendCommand("terminal_input", params);
}

void CLIApp::onTerminalOutput(int tabIndex, const QByteArray& data, const QString& tabType)
{
    Q_UNUSED(tabType);
    QTextStream out(stdout);

    if (jsonMode_) {
        QJsonObject obj;
        obj["tab_index"] = tabIndex;
        obj["size"] = data.size();
        obj["data"] = QString::fromLatin1(data.toBase64());
        QJsonDocument doc(obj);
        out << doc.toJson(QJsonDocument::Compact) << Qt::endl;
        return;
    }

    if (terminalRawMode_) {
        // 原始字节直接输出 (含 ANSI 控制序列, 适合交互式终端体验)
        out.flush();
        ::fwrite(data.constData(), 1, data.size(), stdout);
        ::fflush(stdout);
        return;
    }

    // 默认模式: 尝试解码为文本输出 (过滤纯控制字符可能影响显示)
    QString text = QString::fromUtf8(data);
    out << text;
    out.flush();
}

void CLIApp::printHelp() const
{
    QTextStream out(stdout);
    out << Qt::endl;
    out << "EmberInterDebugTool CLI v1.3.1" << Qt::endl;
    out << Qt::endl;
    out << "会话管理:" << Qt::endl;
    out << "  connect <port> [baud] - 连接串口" << Qt::endl;
    out << "  disconnect             - 断开当前串口" << Qt::endl;
    out << "  disc                   - 断开当前串口 (简写)" << Qt::endl;
    out << "  status                 - 显示连接状态" << Qt::endl;
    out << "  list                   - 列出可用串口" << Qt::endl;
    out << "  tabs                   - 列出所有 Tab (串口/终端/SSH)" << Qt::endl;
    out << "  tab <index>            - 切换到指定 Tab" << Qt::endl;
    out << "  close <index>          - 关闭指定 Tab" << Qt::endl;
    out << Qt::endl;
    out << "终端管理 (新增):" << Qt::endl;
    out << "  term [shell]           - 创建本地终端 (默认 cmd.exe)" << Qt::endl;
    out << "  ssh <user@host> [port] - 创建 SSH 终端" << Qt::endl;
    out << "  sub <index>            - 订阅终端 Tab 输出 (进入终端交互模式)" << Qt::endl;
    out << "  input <text>           - 向当前终端发送文本" << Qt::endl;
    out << Qt::endl;
    out << "数据发送:" << Qt::endl;
    out << "  send <data>            - 发送文本数据 (自动追加CRLF)" << Qt::endl;
    out << "  sendhex <hex>          - 发送HEX数据" << Qt::endl;
    out << "  sendfile <file>        - 发送二进制文件" << Qt::endl;
    out << Qt::endl;
    out << "日志操作:" << Qt::endl;
    out << "  clear                  - 清空日志缓存" << Qt::endl;
    out << "  filter <keyword>       - 设置过滤关键词 (空=取消过滤)" << Qt::endl;
    out << "  hex / text             - 切换 HEX/文本 显示模式" << Qt::endl;
    out << "  timestamp / ts         - 切换时间戳显示" << Qt::endl;
    out << "  export <file>          - 导出日志为JSON文件" << Qt::endl;
    out << Qt::endl;
    out << "其他:" << Qt::endl;
    out << "  help / ?               - 显示此帮助" << Qt::endl;
    out << "  quit / q / exit        - 退出CLI (GUI继续运行)" << Qt::endl;
    out << Qt::endl;
    out << "提示: 未识别的命令将作为文本数据发送到串口" << Qt::endl;
    out << Qt::endl;
}

void CLIApp::printUsage() const
{
    QTextStream out(stdout);
    out << "EmberInterDebugTool v1.3.1 - 尘智 | 微尘藏星火,终端蕴尘智" << Qt::endl << Qt::endl;
    out << "用法: EmberInterDebugTool-cli [选项]" << Qt::endl << Qt::endl;
    out << "监听模式 (需先启动GUI):" << Qt::endl;
    out << "  -p, --port PORT       指定串口, 实时接收日志 (需先通过GUI连接该串口)" << Qt::endl;
    out << "  --cli                 交互CLI模式, 需配合 -p PORT 使用" << Qt::endl;
    out << "  -f, --filter KW       过滤关键词, 只显示包含关键词的日志" << Qt::endl;
    out << "  -o, --output FILE     同时保存日志到文件" << Qt::endl;
    out << "  --hex                 HEX显示模式" << Qt::endl;
    out << "  --no-timestamp        不显示时间戳" << Qt::endl;
    out << "  --json                JSON输出模式" << Qt::endl;
    out << "  --clear               启动时清空GUI日志缓冲" << Qt::endl;
    out << "  --ipc NAME            IPC服务名称 (默认: serial_monitor_ipc)" << Qt::endl;
    out << Qt::endl;
    out << "操作命令 (需先启动GUI):" << Qt::endl;
    out << "  --connect PORT        连接指定串口, 可用 --baudrate 指定波特率" << Qt::endl;
    out << "  --baudrate RATE       配合 --connect 使用, 设置波特率 (默认: 115200)" << Qt::endl;
    out << "  --send DATA           发送文本数据 (自动追加CRLF), 完成后退出" << Qt::endl;
    out << "  --send-hex HEX        发送HEX数据, 完成后退出" << Qt::endl;
    out << "  --send-file FILE      发送二进制文件 (Base64编码传输), 完成后退出" << Qt::endl;
    out << "  --list                列出可用串口设备" << Qt::endl;
    out << "  --get-status          显示当前连接状态" << Qt::endl;
    out << "  --get-logs N          获取最近N条日志" << Qt::endl;
    out << "  --pause               暂停日志输出" << Qt::endl;
    out << "  --resume              恢复日志输出" << Qt::endl;
    out << "  --activate-window     激活 GUI 窗口" << Qt::endl;
    out << Qt::endl;
    out << "终端管理命令 (新增, 需先启动GUI):" << Qt::endl;
    out << "  --create-terminal [SHELL]  创建本地终端 (cmd.exe/powershell.exe/bash), 返回 tab index" << Qt::endl;
    out << "  --create-ssh TARGET        创建 SSH 终端, TARGET=user@host 或配合 --ssh-host" << Qt::endl;
    out << "  --ssh-host HOST            SSH 主机" << Qt::endl;
    out << "  --ssh-user USER            SSH 用户名" << Qt::endl;
    out << "  --ssh-port PORT            SSH 端口 (默认 22)" << Qt::endl;
    out << "  --list-tabs                列出所有 Tab (串口/终端/SSH)" << Qt::endl;
    out << "  --switch-tab INDEX         切换活动 Tab" << Qt::endl;
    out << "  --close-tab INDEX          关闭指定 Tab" << Qt::endl;
    out << "  --terminal-input TEXT      向终端 Tab 发送文本" << Qt::endl;
    out << "  --tab-index INDEX          指定目标 Tab 索引 (配合 --terminal-input)" << Qt::endl;
    out << "  --subscribe-tab INDEX      订阅终端 Tab 输出并进入交互模式" << Qt::endl;
    out << "  --raw-output               终端输出原始字节模式 (配合 --subscribe-tab)" << Qt::endl;
    out << Qt::endl;
    out << "帮助:" << Qt::endl;
    out << "  -h, --help            显示此帮助信息" << Qt::endl;
    out << "  -v, --version         显示版本信息" << Qt::endl;
    out << Qt::endl;
    out << "示例:" << Qt::endl;
    out << "  EmberInterDebugTool-cli --list                            # 列出所有串口" << Qt::endl;
    out << "  EmberInterDebugTool-cli --connect COM3 --baudrate 9600    # 连接COM3" << Qt::endl;
    out << "  EmberInterDebugTool-cli -p COM3                           # 监听COM3日志" << Qt::endl;
    out << "  EmberInterDebugTool-cli -p COM3 --cli                     # 交互模式监听COM3" << Qt::endl;
    out << "  EmberInterDebugTool-cli --send \"AT+GMR\" -p COM3           # 发送命令并观察回复" << Qt::endl;
    out << "  EmberInterDebugTool-cli --send-hex \"FF AB 03\" -p COM3      # 发送HEX字节" << Qt::endl;
    out << "  EmberInterDebugTool-cli --send-file firmware.bin -p COM3    # 发送二进制固件" << Qt::endl;
    out << "  EmberInterDebugTool-cli --get-logs 50                     # 获取最近50条日志" << Qt::endl;
    out << "  EmberInterDebugTool-cli -p COM3 -f ERROR                  # 只显示含ERROR的日志" << Qt::endl;
    out << "  EmberInterDebugTool-cli -p COM3 -o debug.log              # 监听并保存到文件" << Qt::endl;
    out << "  EmberInterDebugTool-cli --get-status                      # 查看连接状态" << Qt::endl;
    out << Qt::endl;
    out << "终端管理示例 (AI 自动化场景):" << Qt::endl;
    out << "  EmberInterDebugTool-cli --create-terminal bash           # 创建 bash 终端, 返回 tab_index" << Qt::endl;
    out << "  EmberInterDebugTool-cli --create-ssh user@192.168.1.10   # 创建 SSH 终端" << Qt::endl;
    out << "  EmberInterDebugTool-cli --list-tabs                      # 列出所有 Tab" << Qt::endl;
    out << "  EmberInterDebugTool-cli --terminal-input \"ls -la\" --tab-index 0  # 向 Tab 0 发送命令" << Qt::endl;
    out << "  EmberInterDebugTool-cli --subscribe-tab 0                # 订阅 Tab 0 输出, 进入交互模式" << Qt::endl;
    out << "  EmberInterDebugTool-cli --subscribe-tab 0 --raw-output   # 原始字节模式 (完整 ANSI 体验)" << Qt::endl;
    out << Qt::endl;
    out << "AI 持久化操作终端流程:" << Qt::endl;
    out << "  1. --create-terminal bash        # 创建终端, 记录返回的 tab_index" << Qt::endl;
    out << "  2. --subscribe-tab <index>       # 订阅该终端, 持续接收输出" << Qt::endl;
    out << "  3. --terminal-input \"cmd\" --tab-index <index>  # 反复发送命令" << Qt::endl;
    out << "  4. --close-tab <index>           # 完成后关闭 Tab" << Qt::endl;
}