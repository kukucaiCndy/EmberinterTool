#ifndef QML_TAB_PAGE_H
#define QML_TAB_PAGE_H

#include <QObject>
#include <QJsonObject>
#include "tab_type.h"

/// Qt6 QML 专用: QObject 基类 TabPage (无 QWidget 依赖)
class TabPage : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString tabTitle READ tabTitle NOTIFY tabTitleChanged)

public:
    explicit TabPage(QObject* parent = nullptr) : QObject(parent) {}

    virtual TabType tabType() const = 0;
    virtual QString tabTitle() const = 0;
    virtual bool isConnected() const = 0;

    virtual void connectTo(const QJsonObject& params) = 0;
    virtual void closeConnection() = 0;  // 重命名以避免隐藏 QObject::disconnect()

    virtual void clearContent() {}
    virtual void exportContent(const QString& path) { Q_UNUSED(path); }

    virtual qint64 rxBytes() const { return 0; }
    virtual qint64 txBytes() const { return 0; }

signals:
    void rxBytesChanged(qint64 bytes);
    void txBytesChanged(qint64 bytes);
    void statusChanged(bool connected);
    void tabTitleChanged();
};

#endif