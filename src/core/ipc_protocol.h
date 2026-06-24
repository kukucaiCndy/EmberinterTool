#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QString>
#include <QByteArray>
#include "log_buffer.h"
#include "tab_type.h"

struct SerialConfig;

class IpcProtocol {
public:
    static constexpr const char* IPC_NAME = "serial_monitor_ipc";

    static QByteArray buildMessage(const QString& type, const QJsonObject& payload,
                                   const QString& id = "");

    static QByteArray buildResponse(const QString& id, bool success,
                                    const QJsonObject& data);

    static QJsonObject parseMessage(const QByteArray& data);

    static QJsonObject buildLogEntryMessage(const LogEntry& entry,
                                          TabType tabType = TabType::Serial);

    static QJsonObject buildStatusMessage(const QString& port, bool connected,
                                          const SerialConfig& config,
                                          qint64 rxBytes, qint64 txBytes,
                                          qint64 uptimeSeconds,
                                          TabType tabType = TabType::Serial);

    /// 构建终端输出消息 (GUI→CLI 定向推送)
    /// tabIndex: 目标 Tab 索引
    /// data: PTY 原始输出字节
    static QJsonObject buildTerminalOutputMessage(int tabIndex,
                                                   const QByteArray& data,
                                                   TabType tabType);

    static QJsonObject buildPortInfo(const QString& name, const QString& description,
                                     const QString& vid, const QString& pid,
                                     bool recommended);

    static QJsonObject buildError(const QString& code, const QString& message,
                                  const QString& port = "");
};

#endif
