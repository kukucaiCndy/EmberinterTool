#ifndef TAB_TYPE_H
#define TAB_TYPE_H

#include <QString>

enum class TabType {
    Serial,
    SSH,
    CMD,
    STLink,
    JLink,
};

inline const char* tabTypeToString(TabType type)
{
    switch (type) {
    case TabType::Serial: return "serial";
    case TabType::SSH:    return "ssh";
    case TabType::CMD:    return "cmd";
    case TabType::STLink: return "stlink";
    case TabType::JLink:  return "jlink";
    }
    return "unknown";
}

inline TabType tabTypeFromString(const QString& str)
{
    if (str == "serial") return TabType::Serial;
    if (str == "ssh")    return TabType::SSH;
    if (str == "cmd")    return TabType::CMD;
    if (str == "stlink") return TabType::STLink;
    if (str == "jlink")  return TabType::JLink;
    return TabType::Serial;
}

inline QString tabTypeDisplayName(TabType type)
{
    switch (type) {
    case TabType::Serial: return QStringLiteral("串口");
    case TabType::SSH:    return QStringLiteral("SSH");
    case TabType::CMD:    return QStringLiteral("终端");
    case TabType::STLink: return QStringLiteral("ST-Link");
    case TabType::JLink:  return QStringLiteral("J-Link");
    }
    return QStringLiteral("未知");
}

#endif