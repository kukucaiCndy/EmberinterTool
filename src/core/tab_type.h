#ifndef TAB_TYPE_H
#define TAB_TYPE_H

#include <QString>

enum class TabType {
    Serial,
    SSH,
    CMD,
};

inline const char* tabTypeToString(TabType type)
{
    switch (type) {
    case TabType::Serial: return "serial";
    case TabType::SSH:    return "ssh";
    case TabType::CMD:    return "cmd";
    }
    return "unknown";
}

inline TabType tabTypeFromString(const QString& str, bool* ok = nullptr)
{
    if (str == "serial") {
        if (ok) *ok = true;
        return TabType::Serial;
    }
    if (str == "ssh") {
        if (ok) *ok = true;
        return TabType::SSH;
    }
    if (str == "cmd") {
        if (ok) *ok = true;
        return TabType::CMD;
    }
    if (ok) *ok = false;
    return TabType::Serial;
}

inline QString tabTypeDisplayName(TabType type)
{
    switch (type) {
    case TabType::Serial: return QStringLiteral("串口");
    case TabType::SSH:    return QStringLiteral("SSH");
    case TabType::CMD:    return QStringLiteral("终端");
    }
    return QStringLiteral("未知");
}

#endif
