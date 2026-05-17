#ifndef SERIAL_TAB_WIDGET_H
#define SERIAL_TAB_WIDGET_H

#include <QTabWidget>
#include <QVector>
#include "serial_engine.h"

struct TabInfo {
    QString port;
    QString name;
    SerialEngine* engine;
    bool connected;
};

class SerialTabWidget : public QTabWidget {
    Q_OBJECT
public:
    explicit SerialTabWidget(QWidget* parent = nullptr);

    int addSerialTab(const QString& port, const QString& name = "");
    void removeTab(int index);
    TabInfo* currentTabInfo();
    TabInfo* tabInfo(int index);
    QVector<TabInfo*> allTabs();

    void setTabConnected(int index, bool connected);
    void updateTabName(int index, const QString& name);

signals:
    void connectRequested(int index, const SerialConfig& config);
    void disconnectRequested(int index);
    void tabChanged(int index);

private:
    QVector<TabInfo*> tabs_;
};

#endif
