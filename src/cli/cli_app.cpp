#include "cli_app.h"
#include "ipc_protocol.h"
#include "log_parser.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QFile>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[91m"
#define COLOR_GREEN   "\033[92m"
#define COLOR_YELLOW  "\033[93m"
#define COLOR_CYAN    "\033[96m"
#define COLOR_GRAY    "\033[90m"

static QString stripAnsi(const QString& text)
{
    QString result = text;
    result.remove(QRegularExpression("\\x1b\\[[0-9;]*m"));
    return result;
}

static void printColored(QTextStream& out, const QString& text, const QString& level)
{
    QString color;
    if (level == "ERROR")      color = COLOR_RED;
    else if (level == "WARN")  color = COLOR_YELLOW;
    else if (level == "INFO")  color = COLOR_GREEN;
    else if (level == "DEBUG") color = COLOR_CYAN;
    else if (level == "TRACE") color = COLOR_GRAY;
    else                       color = COLOR_RESET;

    out << color << text << COLOR_RESET << endl;
}

CLIApp::CLIApp(const QString& ipcName)
    : QObject(nullptr)
    , ipc_(new IPCClient(this))
    , ipcName_(ipcName)
    , jsonMode_(false)
    , interactiveMode_(false)
    , hexMode_(false)
    , showTimestamp_(true)
{
    connect(ipc_, &IPCClient::logReceived, this, &CLIApp::onLogReceived);
    connect(ipc_, &IPCClient::statusChanged, this, &CLIApp::onStatusChanged);
}

int CLIApp::run(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("serial-monitor-cli");
    app.setApplicationVersion("2.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("EmberIntel Serial Monitor - CLI Client");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(QCommandLineOption("p", "串口号", "PORT"));
    parser.addOption(QCommandLineOption("b", "波特率 (默认: 115200)", "RATE", "115200"));
    parser.addOption(QCommandLineOption("f", "筛选关键字", "KEYWORD"));
    parser.addOption(QCommandLineOption("o", "保存日志到文件", "FILE"));
    parser.addOption(QCommandLineOption("hex", "HEX 显示模式"));
    parser.addOption(QCommandLineOption("no-timestamp", "禁用时间戳"));
    parser.addOption(QCommandLineOption("clear", "启动时清空日志"));
    parser.addOption(QCommandLineOption("list", "列出可用串口"));
    parser.addOption(QCommandLineOption("cli", "交互式 CLI 模式"));
    parser.addOption(QCommandLineOption("json", "JSON 输出模式"));
    parser.addOption(QCommandLineOption("ipc", "IPC 服务名 (默认: serial_monitor_ipc)", "NAME", "serial_monitor_ipc"));
    parser.addOption(QCommandLineOption("send", "发送数据后退出", "DATA"));
    parser.addOption(QCommandLineOption("get-logs", "获取最近日志条数", "COUNT"));

    parser.process(app);

    jsonMode_ = parser.isSet("json");
    hexMode_ = parser.isSet("hex");
    showTimestamp_ = !parser.isSet("no-timestamp");
    filter_ = parser.value("f");
    outputFile_ = parser.value("o");
    ipcName_ = parser.value("ipc");

    if (parser.isSet("help") || parser.isSet("version")) {
        return 0;
    }

    if (!ipc_->connectToServer(ipcName_)) {
        QTextStream err(stderr);
        err << "Error: GUI 服务未运行 (IPC: " << ipcName_ << ")" << Qt::endl;
        err << "请先启动 serial-monitor.exe" << Qt::endl;
        return 2;
    }

    if (parser.isSet("list")) {
        QJsonObject params;
        ipc_->sendCommand("list_ports", params);
        QCoreApplication::processEvents();
        QThread::msleep(200);
        QCoreApplication::processEvents();
        return 0;
    }

    if (parser.isSet("send")) {
        QString data = parser.value("send");
        QJsonObject params;
        params["data"] = data;
        params["port"] = parser.value("p");
        params["append"] = "CRLF";
        ipc_->sendCommand("send_text", params);
        QCoreApplication::processEvents();
        QThread::msleep(100);
        QCoreApplication::processEvents();
        return 0;
    }

    if (parser.isSet("cli") && !parser.value("p").isEmpty()) {
        ipcName_ = parser.value("ipc");
        return runInteractive(parser.value("p"));
    }

    if (!parser.value("p").isEmpty()) {
        return runNonInteractive(QStringList() << "--port" << parser.value("p"));
    }

    printUsage();
    return 1;
}

int CLIApp::runNonInteractive(const QStringList& args)
{
    Q_UNUSED(args);
    QTextStream out(stdout);

    QJsonObject params;
    params["port"] = args.size() > 1 ? args[1] : "";
    params["baudrate"] = 115200;
    ipc_->sendCommand("connect", params);

    out << COLOR_GREEN << "[SYSTEM] 正在接收日志，按 Ctrl+C 退出..." << COLOR_RESET << endl;

    QCoreApplication::processEvents();

    return 0;
}

int CLIApp::runInteractive(const QString& port)
{
    interactiveMode_ = true;

    QJsonObject connParams;
    connParams["port"] = port;
    connParams["baudrate"] = 115200;
    ipc_->sendCommand("connect", connParams);

    QTextStream out(stdout);
    out << COLOR_CYAN << "=" << QString(60, '=') << COLOR_RESET << endl;
    out << COLOR_CYAN << "  EmberIntel Serial Monitor CLI" << COLOR_RESET << endl;
    out << COLOR_GRAY << "  Port: " << port << COLOR_RESET << endl;
    out << COLOR_CYAN << "=" << QString(60, '=') << COLOR_RESET << endl;
    out << endl;
    out << COLOR_GREEN << "[SYSTEM] 输入 'help' 查看命令，'quit' 退出" << COLOR_RESET << endl;
    out << endl;

    QTextStream in(stdin);
    while (true) {
        out << "> " << flush;
        QString cmd = in.readLine().trimmed();
        if (cmd.isEmpty()) continue;

        if (cmd == "q" || cmd == "quit" || cmd == "exit") {
            break;
        }

        handleCommand(cmd);
    }

    out << COLOR_GRAY << "[SYSTEM] CLI 已断开（GUI 服务继续运行）" << COLOR_RESET << endl;
    return 0;
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
        return;
    }

    if (cmd == "s" || cmd == "status") {
        ipc_->sendCommand("get_status");
        return;
    }

    if (cmd == "list" || cmd == "ls") {
        ipc_->sendCommand("list_ports");
        return;
    }

    if (cmd == "hex") {
        hexMode_ = true;
        out << COLOR_GREEN << "[SYSTEM] 已切换到 HEX 模式" << COLOR_RESET << endl;
        return;
    }

    if (cmd == "text") {
        hexMode_ = false;
        out << COLOR_GREEN << "[SYSTEM] 已切换到文本模式" << COLOR_RESET << endl;
        return;
    }

    if (cmd == "ts" || cmd == "timestamp") {
        showTimestamp_ = !showTimestamp_;
        out << COLOR_GREEN << "[SYSTEM] 时间戳: "
            << (showTimestamp_ ? "开" : "关") << COLOR_RESET << endl;
        return;
    }

    if (cmd.startsWith("send ")) {
        QJsonObject params;
        params["data"] = cmd.mid(5);
        params["append"] = "CRLF";
        ipc_->sendCommand("send_text", params);
        out << COLOR_GREEN << ">>> 发送: " << cmd.mid(5) << COLOR_RESET << endl;
        return;
    }

    if (cmd.startsWith("sendhex ")) {
        QString hexStr = cmd.mid(8);
        hexStr.remove(QRegularExpression("\\s"));
        QJsonObject params;
        params["data"] = hexStr;
        ipc_->sendCommand("send_hex", params);
        out << COLOR_GREEN << ">>> 发送 HEX: " << cmd.mid(8) << COLOR_RESET << endl;
        return;
    }

    if (cmd.startsWith("filter ")) {
        QString kw = cmd.mid(7).trimmed();
        QJsonObject params;
        params["keyword"] = kw;
        ipc_->sendCommand("set_filter", params);
        out << COLOR_GREEN << "[SYSTEM] 筛选关键字: "
            << (kw.isEmpty() ? "无" : kw) << COLOR_RESET << endl;
        return;
    }

    if (cmd.startsWith("export ")) {
        QJsonObject params;
        params["file_path"] = cmd.mid(7).trimmed();
        params["format"] = "json";
        ipc_->sendCommand("export_logs", params);
        out << COLOR_GREEN << "[SYSTEM] 正在导出..." << COLOR_RESET << endl;
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

    if (cmd == "disc" || cmd == "disconnect") {
        ipc_->sendCommand("disconnect");
        return;
    }

    QJsonObject params;
    params["data"] = cmd;
    params["append"] = "CRLF";
    ipc_->sendCommand("send_text", params);
    out << COLOR_GREEN << ">>> 发送: " << cmd << COLOR_RESET << endl;
}

void CLIApp::onLogReceived(const QJsonObject& log)
{
    QTextStream out(stdout);
    QString timestamp = log["timestamp"].toString();
    QString level = log["level"].toString();
    QString text = log["text"].toString();

    if (!filter_.isEmpty() && !text.contains(filter_, Qt::CaseInsensitive)) {
        return;
    }

    if (jsonMode_) {
        QJsonDocument doc(log);
        out << doc.toJson(QJsonDocument::Compact) << endl;
        return;
    }

    if (hexMode_) {
        QByteArray raw = QByteArray::fromHex(log["raw_hex"].toString().toLatin1());
        if (!raw.isEmpty()) {
            out << LogParser::formatHex(raw, 0) << endl;
        }
        return;
    }

    LogEntry entry(timestamp, level, text, QByteArray(), log["port"].toString());
    QString display = LogParser::formatDisplay(entry, showTimestamp_);
    printColored(out, display, level);

    if (!outputFile_.isEmpty()) {
        QFile f(outputFile_);
        if (f.open(QIODevice::Append)) {
            f.write(stripAnsi(display).toUtf8() + "\n");
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
        out << doc.toJson(QJsonDocument::Compact) << endl;
        return;
    }

    out << COLOR_GREEN << "[SYSTEM] " << port << " "
        << (connected ? "已连接" : "已断开") << COLOR_RESET << endl;
}

void CLIApp::printHelp() const
{
    QTextStream out(stdout);
    out << endl;
    out << "命令:" << endl;
    out << "  q/quit/exit          - 退出 CLI（不影响 GUI）" << endl;
    out << "  c/clear              - 清空日志缓冲区" << endl;
    out << "  s/status             - 显示连接状态" << endl;
    out << "  list                 - 列出可用串口" << endl;
    out << "  connect <port> [baud]- 连接串口" << endl;
    out << "  disc/disconnect      - 断开串口" << endl;
    out << "  send <data>          - 发送文本数据" << endl;
    out << "  sendhex <hex>        - 发送 HEX 数据" << endl;
    out << "  filter <keyword>     - 设置筛选关键字" << endl;
    out << "  hex / text           - 切换显示模式" << endl;
    out << "  timestamp / ts       - 切换时间戳" << endl;
    out << "  export <file>        - 导出日志到 JSON" << endl;
    out << "  help / ?             - 显示帮助" << endl;
    out << endl;
}

void CLIApp::printUsage() const
{
    QTextStream out(stdout);
    out << "Usage: serial-monitor-cli [options]" << endl;
    out << "  -p, --port PORT       串口号" << endl;
    out << "  -b, --baudrate RATE   波特率" << endl;
    out << "  -f, --filter KEYWORD  筛选关键字" << endl;
    out << "  -o, --output FILE     保存日志到文件" << endl;
    out << "  --hex                 HEX 显示模式" << endl;
    out << "  --list                列出可用串口" << endl;
    out << "  --cli                 交互式 CLI 模式" << endl;
    out << "  --json                JSON 输出模式" << endl;
    out << "  --send DATA           发送数据" << endl;
    out << "  --get-logs COUNT      获取最近日志" << endl;
    out << "  --ipc NAME            IPC 服务名" << endl;
    out << "  -h, --help            显示帮助" << endl;
}
