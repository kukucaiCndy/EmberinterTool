#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QSerialPort>
#include "serial_engine.h"

struct SavedPort {
    QString name;
    QString port;
    int baudrate = 115200;
    QSerialPort::DataBits databits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopbits = QSerialPort::OneStop;

    QString summary() const {
        return port.isEmpty() ? name : QString("%1 - %2").arg(port, name);
    }

    SerialConfig toSerialConfig() const {
        SerialConfig cfg;
        cfg.port = port;
        cfg.baudrate = baudrate;
        cfg.databits = databits;
        cfg.parity = parity;
        cfg.stopbits = stopbits;
        return cfg;
    }
};

struct TabConfig {
    QString port;
    QString name;
};

struct DisplayConfig {
    QString theme = "dark";
    bool showTimestamp = true;
    bool hexMode = false;
    int fontSize = 12;
    int bufferSize = 10000;
    bool autoScroll = true;
    bool autoReconnect = true;
};

struct SendConfig {
    QStringList history;
    QString append = "CRLF";
};

struct AppConfig {
    QString port;
    int baudrate = 115200;
    int databits = 8;
    QString parity = "N";
    int stopbits = 1;
    QString flowcontrol = "N";
    DisplayConfig display;
    QStringList filterHistory;
    SendConfig send;
    int windowWidth = 1000;
    int windowHeight = 700;
    QVector<TabConfig> tabs;
    QVector<SavedPort> savedPorts;
    QString ipcName = "serial_monitor_ipc";
};

class ConfigManager {
public:
    static ConfigManager& instance();

    bool load(const QString& path = "");
    bool save(const QString& path = "");
    AppConfig& config();
    QString configPath() const;
    void resetToDefault();

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    AppConfig config_;
    QString configPath_;
    QString defaultConfigPath() const;
};

#endif
