#ifndef APP_CORE_H
#define APP_CORE_H

#include <QObject>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QAbstractListModel>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QDateTime>
#include <QTimer>
#include <QSerialPortInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDesktopServices>
#include <spdlog/spdlog.h>
#include "tab_page.h"       // Qt6 QObject-based
#include "ipc_server.h"
#include "config_manager.h"
#include "serial_tab_page.h"
#include "terminal_tab_page.h"

// ── Tab 列表模型 ──────────────────────────────────────────
class TabModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { TitleRole = Qt::UserRole + 1, TabTypeRole, ConnectedRole };

    explicit TabModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override {
        Q_UNUSED(parent);
        return tabs_.size();
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid))
            return {};
        TabPage* tab = tabs_[index.row()];
        switch (role) {
        case TitleRole:   return tab->tabTitle();
        case TabTypeRole: return static_cast<int>(tab->tabType());
        case ConnectedRole: return tab->isConnected();
        }
        return {};
    }

    QHash<int, QByteArray> roleNames() const override {
        return {{TitleRole, "title"}, {TabTypeRole, "tabType"}, {ConnectedRole, "connected"}};
    }

    void addTab(TabPage* page) {
        beginInsertRows({}, tabs_.size(), tabs_.size());
        tabs_.append(page);
        endInsertRows();
        emit countChanged();
        spdlog::debug("TabModel::addTab: count={}, title={}",
                      tabs_.size(), page->tabTitle().toStdString());
    }

    void removeTab(int index) {
        if (index < 0 || index >= tabs_.size()) return;
        spdlog::debug("TabModel::removeTab: index={}, count={}", index, tabs_.size());
        beginRemoveRows({}, index, index);
        tabs_.removeAt(index);
        endRemoveRows();
        emit countChanged();
    }

    TabPage* tabAt(int index) const {
        if (index < 0 || index >= tabs_.size()) return nullptr;
        return tabs_[index];
    }

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    int count() const { return tabs_.size(); }

signals:
    void countChanged();

private:
    QVector<TabPage*> tabs_;
};

// ── 已保存会话列表模型 ────────────────────────────────────
class SavedPortModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { SummaryRole = Qt::UserRole + 1, TypeRole, AvailableRole, NameRole, ConnectedRole };

    explicit SavedPortModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override {
        Q_UNUSED(parent);
        return ports_.size();
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid))
            return {};
        const auto& p = ports_[index.row()];
        switch (role) {
        case SummaryRole:  return p.summary();
        case TypeRole:     return static_cast<int>(p.type);
        case AvailableRole: return isAvailable(p);
        case NameRole:     return p.name;
        case ConnectedRole: return connected_[index.row()];
        }
        return {};
    }

    QHash<int, QByteArray> roleNames() const override {
        return {{SummaryRole, "summary"}, {TypeRole, "type"}, {AvailableRole, "available"},
                {NameRole, "name"}, {ConnectedRole, "connected"}};
    }

    void load() {
        beginResetModel();
        ports_ = ConfigManager::instance().config().savedPorts;
        connected_.fill(false, ports_.size());
        endResetModel();
    }

    void setConnected(int row, bool connected) {
        if (row >= 0 && row < connected_.size()) {
            connected_[row] = connected;
            emit dataChanged(index(row), index(row), {ConnectedRole});
        }
    }

    void refreshConnected(const QVector<bool>& conn) {
        connected_ = conn;
        if (!connected_.isEmpty())
            emit dataChanged(index(0), index(connected_.size() - 1), {ConnectedRole});
    }

    int count() const { return ports_.size(); }

private:
    QVector<SavedPort> ports_;
    QVector<bool> connected_;

    bool isAvailable(const SavedPort& p) const {
        if (p.type != TabType::Serial) return true;
        for (const auto& info : QSerialPortInfo::availablePorts()) {
            if (info.portName() == p.port) return true;
        }
        return false;
    }
};

// ── AppCore: 主应用逻辑 ───────────────────────────────────
class AppCore : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString portConfigText READ portConfigText NOTIFY portConfigTextChanged)
    Q_PROPERTY(QString rxBytesText READ rxBytesText NOTIFY rxBytesTextChanged)
    Q_PROPERTY(QString txBytesText READ txBytesText NOTIFY txBytesTextChanged)
    Q_PROPERTY(QString uptimeText READ uptimeText NOTIFY uptimeTextChanged)
    Q_PROPERTY(bool hasActiveTab READ hasActiveTab NOTIFY hasActiveTabChanged)
    Q_PROPERTY(int currentTabIndex READ currentTabIndex NOTIFY currentTabIndexChanged)
    Q_PROPERTY(QObject* tabModel READ tabModel CONSTANT)
    Q_PROPERTY(QObject* savedPorts READ savedPorts CONSTANT)
    Q_PROPERTY(QStringList availableSerialPorts READ availableSerialPorts NOTIFY availableSerialPortsChanged)

public:
    explicit AppCore(QObject* parent = nullptr);

    QString statusText() const { return statusText_; }
    QString portConfigText() const { return portConfigText_; }
    QString rxBytesText() const { return rxBytesText_; }
    QString txBytesText() const { return txBytesText_; }
    QString uptimeText() const { return uptimeText_; }
    bool hasActiveTab() const { return hasActiveTab_; }
    int currentTabIndex() const { return currentTabIdx_; }
    QObject* tabModel() { return &tabModel_; }
    QObject* savedPorts() { return &savedPortModel_; }
    QStringList availableSerialPorts() const { return availablePorts_; }

    void init(QQmlApplicationEngine* engine);

    Q_INVOKABLE void createConnection();
    Q_INVOKABLE void connectSavedPort(int index);
    Q_INVOKABLE void removeSavedPort(int index);
    Q_INVOKABLE void setCurrentTab(int index);
    Q_INVOKABLE void closeCurrentTab();
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void openAbout();
    Q_INVOKABLE void loadTabContent(int index, QObject* container);
    Q_INVOKABLE QObject* getTabPage(int index);

    // Tab 右键菜单操作
    Q_INVOKABLE void renameTab(int index, const QString& name);
    Q_INVOKABLE void toggleConnection(int index);
    Q_INVOKABLE void deleteSavedPortForTab(int index);
    Q_INVOKABLE int findSavedPortForTab(int index) const;

    // 会话列表右键菜单操作
    Q_INVOKABLE int findTabForSavedPort(int savedIndex) const;
    Q_INVOKABLE void toggleSavedPort(int savedIndex);
    Q_INVOKABLE void closeTabForSavedPort(int savedIndex);
    Q_INVOKABLE void renameSavedPort(int savedIndex, const QString& name);

    // 连接向导
    Q_INVOKABLE void showWizard();
    Q_INVOKABLE void confirmConnection(const QJsonObject& params);

    // 设置
    Q_INVOKABLE void saveSettings(int fontSize, int maxLogLines, bool autoScroll);
    Q_INVOKABLE QVariantMap loadSettings();
    Q_INVOKABLE void saveAutoCheckUpdate(bool enabled);

    // 更新检查 (通过 GitHub API)
    Q_INVOKABLE void checkUpdate();
    Q_INVOKABLE QString currentVersion() const;
    // 下载更新包并在 APP 内显示进度, 下载完成后启动安装程序
    Q_INVOKABLE void downloadUpdate(const QString& url);

signals:
    void statusTextChanged();
    void portConfigTextChanged();
    void rxBytesTextChanged();
    void txBytesTextChanged();
    void uptimeTextChanged();
    void hasActiveTabChanged();
    void currentTabIndexChanged();
    void availableSerialPortsChanged();
    void wizardRequested();
    void settingsRequested();
    void aboutRequested();
    // 更新检查结果: latestVersion, downloadUrl, hasUpdate, errorMsg
    void updateCheckResult(const QJsonObject& result);
    // 下载进度: receivedBytes, totalBytes, percent (-1=无法获取大小)
    void updateDownloadProgress(qint64 received, qint64 total, int percent);
    // 下载完成: filePath (空表示失败), errorMsg
    void updateDownloadFinished(const QString& filePath, const QString& errorMsg);

private slots:
    void onIpcCommand(const QString& clientId, const QString& cmd,
                      const QJsonObject& params, const QString& reqId);
    void onTabRxChanged(qint64 bytes);
    void onTabTxChanged(qint64 bytes);
    void onTabStatusChanged(bool connected);

private:
    void addTab(TabPage* page);
    void removeTab(TabPage* page);
    void updateStatus();
    void refreshSerialPorts();
    void refreshSavedPortConnected();
    bool isPortUsed(const QString& port) const;
    QString formatUptime(qint64 seconds) const;

    IPCServer* ipcServer_;
    TabModel tabModel_;
    SavedPortModel savedPortModel_;

    QString statusText_;
    QString portConfigText_;
    QString rxBytesText_;
    QString txBytesText_;
    QString uptimeText_;
    bool hasActiveTab_;
    int currentTabIdx_;
    qint64 rxBytes_;
    qint64 txBytes_;

    QStringList availablePorts_;
    QSet<QString> usedPorts_;   // 当前已打开的串口号

    // 更新检查
    QNetworkAccessManager* networkManager_;
    QNetworkReply* downloadReply_;
    QString downloadFilePath_;

    // 每个 Tab 的连接参数和连接时间（用于状态栏显示）
    QHash<TabPage*, QJsonObject> tabConnParams_;
    QHash<TabPage*, QDateTime> tabConnectTimes_;
    QTimer* uptimeTimer_;
};

#endif