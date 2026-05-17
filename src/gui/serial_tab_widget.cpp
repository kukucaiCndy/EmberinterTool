#include "serial_tab_widget.h"

SerialTabWidget::SerialTabWidget(QWidget* parent)
    : QTabWidget(parent)
{
    setTabsClosable(true);
    setMovable(true);

    connect(this, &QTabWidget::currentChanged, this, &SerialTabWidget::tabChanged);
    connect(this, &QTabWidget::tabCloseRequested, this, &SerialTabWidget::removeTab);
}

int SerialTabWidget::addSerialTab(const QString& port, const QString& name)
{
    TabInfo* info = new TabInfo;
    info->port = port;
    info->name = name.isEmpty() ? port : name;
    info->engine = nullptr;
    info->connected = false;
    tabs_.append(info);

    QString label = QString("%1 %2")
        .arg(info->connected ? "●" : "○")
        .arg(info->name.isEmpty() ? port : info->name);
    addTab(new QWidget(this), label);
    return count() - 1;
}

void SerialTabWidget::removeTab(int index)
{
    if (index < 0 || index >= tabs_.size()) {
        return;
    }
    delete tabs_[index];
    tabs_.removeAt(index);
    QTabWidget::removeTab(index);
}

TabInfo* SerialTabWidget::currentTabInfo()
{
    int idx = currentIndex();
    return tabInfo(idx);
}

TabInfo* SerialTabWidget::tabInfo(int index)
{
    if (index < 0 || index >= tabs_.size()) {
        return nullptr;
    }
    return tabs_[index];
}

QVector<TabInfo*> SerialTabWidget::allTabs()
{
    return tabs_;
}

void SerialTabWidget::setTabConnected(int index, bool connected)
{
    if (index < 0 || index >= tabs_.size()) {
        return;
    }
    tabs_[index]->connected = connected;
    QString label = QString("%1 %2")
        .arg(connected ? "●" : "○")
        .arg(tabs_[index]->name.isEmpty() ? tabs_[index]->port : tabs_[index]->name);
    setTabText(index, label);
}

void SerialTabWidget::updateTabName(int index, const QString& name)
{
    if (index < 0 || index >= tabs_.size()) {
        return;
    }
    tabs_[index]->name = name;
    QString label = QString("%1 %2")
        .arg(tabs_[index]->connected ? "●" : "○")
        .arg(name);
    setTabText(index, label);
}
