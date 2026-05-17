#include <QApplication>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "main_window.h"
#include "config_manager.h"

int main(int argc, char* argv[])
{
    auto fileLogger = spdlog::basic_logger_mt("app", "serial-monitor.log");
    spdlog::set_default_logger(fileLogger);
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Serial Monitor GUI starting");

    QApplication app(argc, argv);
    app.setApplicationName("EmberIntel Serial Monitor");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("EmberIntel");

    ConfigManager::instance().load();

    MainWindow mainWindow;
    mainWindow.show();

    int result = app.exec();

    spdlog::info("Serial Monitor GUI exiting");
    return result;
}
