#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>
#include <QIcon>
#include <QStandardPaths>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "config_manager.h"
#include "qml/app_core.h"
#include "qml/terminal/terminal_view.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    // 释放继承的控制台，避免 ConPTY 子进程受到父进程控制台（如 MSYS2 pty）的干扰
    FreeConsole();
    // 清除继承的标准句柄，防止 MSYS2 pty 句柄影响 ConPTY 子进程
    SetStdHandle(STD_INPUT_HANDLE, INVALID_HANDLE_VALUE);
    SetStdHandle(STD_OUTPUT_HANDLE, INVALID_HANDLE_VALUE);
    SetStdHandle(STD_ERROR_HANDLE, INVALID_HANDLE_VALUE);
#endif

    // ── 单实例检查 ──────────────────────────────────────
    ConfigManager::instance().load();
    QString ipcName = ConfigManager::instance().config().ipcName;

    QLocalServer singleLock;
    QLocalServer::removeServer("emberinter_instance_lock");
    if (!singleLock.listen("emberinter_instance_lock")) {
        QLocalSocket sock;
        sock.connectToServer(ipcName);
        if (!sock.waitForConnected(2000)) {
            return 0;
        }

        QJsonObject msg;
        msg["type"] = "activate_window";
        QJsonDocument doc(msg);
        QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
        sock.write(data);
        sock.waitForBytesWritten(1000);
        sock.disconnectFromServer();
        return 0;
    }

    // 双 sink: stdout (彩色) + 文件
    // 日志文件写入用户可写目录，避免从 DMG 只读卷运行时崩溃
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(logDir);
    QString logPath = logDir + "/serial-monitor.log";

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto fileSink    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.toStdString(), true);
    std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
    auto logger = std::make_shared<spdlog::logger>("app", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %s:%# | %v");
    spdlog::flush_on(spdlog::level::debug);
    spdlog::info("EmberInterDebugTool GUI starting");

    // 捕获所有 Qt 消息 (包括 QGuiApplication 构造期间的警告)
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& ctx,
                               const QString& msg) {
        if (type == QtDebugMsg)
            spdlog::debug("Qt: {}", msg.toStdString());
        else if (type == QtInfoMsg)
            spdlog::info("Qt: {}", msg.toStdString());
        else if (type == QtWarningMsg) {
            std::string loc = ctx.file ? (std::string(ctx.file) + ":" +
                std::to_string(ctx.line)) : "";
            spdlog::warn("Qt [{}]: {}", loc, msg.toStdString());
        } else if (type == QtCriticalMsg || type == QtFatalMsg) {
            std::string loc = ctx.file ? (std::string(ctx.file) + ":" +
                std::to_string(ctx.line)) : "";
            spdlog::error("Qt [{}/{}]: {}", loc, ctx.function ? ctx.function : "",
                          msg.toStdString());
        }
    });

#ifdef Q_OS_WIN
    // 强制使用 OpenGL RHI 后端 (QQuickFramebufferObject + QOpenGLFunctions 需要)
    // Qt6 默认在 Windows 上使用 D3D11, 会导致 OpenGL FBO 渲染静默失败
    // 必须在 QGuiApplication 构造之前设置!
    qputenv("QSG_RHI_BACKEND", "opengl");
#endif

    QGuiApplication app(argc, argv);
    app.setApplicationName("EmberInterDebugTool");
    app.setApplicationVersion("1.4.1");
    app.setOrganizationName("EmberInter");
    app.setWindowIcon(QIcon(":/icons/app.ico"));

    // 注册自定义 QML 类型
    qmlRegisterType<TerminalView>("EmberInter", 1, 0, "TerminalView");

    // 强制使用 Fusion 样式，避免原生样式不支持自定义控件
    QQuickStyle::setStyle("Fusion");

    // AppCore 必须在 QQmlApplicationEngine 之前构造、之后销毁，
    // 否则程序退出时 QML 对象仍持有对 appCore 的绑定，访问已销毁对象会产生 TypeError。
    AppCore appCore;

    QQmlApplicationEngine engine;

    // QML 导入路径: 始终注册 qrc 路径 + 开发模式文件系统路径
    engine.addImportPath("qrc:/qml");

    QString qmlMainPath = "qrc:/qml/main.qml";
    bool useQrc = true;

    // 开发模式: 如果文件系统存在 QML 源码，也添加文件系统导入路径
    QString fsQmlDir = QDir::currentPath() + "/src/gui/qml";
    if (!QDir(fsQmlDir).exists())
        fsQmlDir = QDir::cleanPath(
            QCoreApplication::applicationDirPath() + "/../../src/gui/qml");
    if (QDir(fsQmlDir).exists()) {
        engine.addImportPath(fsQmlDir);
        spdlog::info("Also registered filesystem QML path: {}", fsQmlDir.toStdString());
    }

    // 如果文件系统 main.qml 存在, 优先使用 (开发模式)
    {
        QString fsMainPath = QDir::currentPath() + "/src/gui/qml/main.qml";
        if (!QFile::exists(fsMainPath))
            fsMainPath = QDir::cleanPath(
                QCoreApplication::applicationDirPath() + "/../../src/gui/qml/main.qml");
        if (QFile::exists(fsMainPath)) {
            qmlMainPath = fsMainPath;
            useQrc = false;
            spdlog::info("Loading QML from filesystem: {}", qmlMainPath.toStdString());
        } else {
            spdlog::info("Loading QML from Qt Resource System");
        }
    }

    // 添加 EmberDesign 文件系统路径 (如果存在)
    {
        QString emberFs = QDir::currentPath() + "/src/gui/qml/EmberDesign";
        if (!QDir(emberFs).exists())
            emberFs = QDir::cleanPath(
                QCoreApplication::applicationDirPath() + "/../../src/gui/qml/EmberDesign");
        if (QDir(emberFs).exists())
            engine.addImportPath(emberFs);
    }

    appCore.init(&engine);

    engine.rootContext()->setContextProperty("appCore", &appCore);

    engine.load(useQrc ? QUrl(qmlMainPath) : QUrl::fromLocalFile(qmlMainPath));

    if (engine.rootObjects().isEmpty()) {
        spdlog::error("Failed to load QML");
        return -1;
    }

    int result = app.exec();
    spdlog::info("EmberInterDebugTool GUI exiting");
    return result;
}
