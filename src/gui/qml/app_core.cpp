#include "app_core.h"
#include <QSerialPortInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QGuiApplication>
#include <QWindow>
#include <QProcessEnvironment>
#include <spdlog/spdlog.h>

// 平台默认 shell
static QString defaultShell()
{
#ifdef Q_OS_WIN
    return QStringLiteral("cmd.exe");
#else
    return QProcessEnvironment::systemEnvironment().value("SHELL", "/bin/bash");
#endif
}

AppCore::AppCore(QObject* parent)
    : QObject(parent)
    , ipcServer_(nullptr)
    , hasActiveTab_(false)
    , currentTabIdx_(-1)
    , rxBytes_(0)
    , txBytes_(0)
    , networkManager_(nullptr)
    , downloadReply_(nullptr)
    , uptimeTimer_(nullptr)
{
    statusText_ = QString::fromUtf8("就绪");
    portConfigText_ = "";
    uptimeText_ = "";

    uptimeTimer_ = new QTimer(this);
    uptimeTimer_->setInterval(1000);
    connect(uptimeTimer_, &QTimer::timeout, this, &AppCore::updateStatus);
}

void AppCore::init(QQmlApplicationEngine* engine)
{
    Q_UNUSED(engine);

    // 启动 IPC 服务器
    ipcServer_ = new IPCServer(this);
    ipcServer_->start();
    connect(ipcServer_, &IPCServer::commandReceived,
            this, &AppCore::onIpcCommand);

    // 加载已保存会话
    savedPortModel_.load();
    refreshSavedPortConnected();

    // 初始串口扫描
    refreshSerialPorts();

    spdlog::info("AppCore initialized");
}

// ── 已使用端口跟踪 ───────────────────────────────────────

bool AppCore::isPortUsed(const QString& port) const
{
    return usedPorts_.contains(port);
}

void AppCore::refreshSerialPorts()
{
    QStringList ports;
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        if (!isPortUsed(info.portName())) {
            ports.append(info.portName());
        }
    }
    if (availablePorts_ != ports) {
        availablePorts_ = ports;
        emit availableSerialPortsChanged();
    }
}

void AppCore::refreshSavedPortConnected()
{
    const auto& savedPorts = ConfigManager::instance().config().savedPorts;
    QVector<bool> conn(savedPorts.size(), false);
    for (int i = 0; i < savedPorts.size(); ++i) {
        int tabIdx = findTabForSavedPort(i);
        if (tabIdx >= 0) {
            TabPage* page = tabModel_.tabAt(tabIdx);
            conn[i] = page && page->isConnected();
        }
    }
    savedPortModel_.refreshConnected(conn);
}

// ── Tab 管理 ─────────────────────────────────────────────

void AppCore::addTab(TabPage* page)
{
    spdlog::debug("AppCore::addTab: type={}, title={}",
                  static_cast<int>(page->tabType()), page->tabTitle().toStdString());

    connect(page, &TabPage::rxBytesChanged, this, &AppCore::onTabRxChanged);
    connect(page, &TabPage::txBytesChanged, this, &AppCore::onTabTxChanged);
    connect(page, &TabPage::statusChanged, this, &AppCore::onTabStatusChanged);

    // 终端 Tab: 连接 dataReceived 信号, 转发到 IPC 订阅客户端
    if (page->tabType() == TabType::CMD || page->tabType() == TabType::SSH) {
        auto* tp = qobject_cast<TerminalTabPage*>(page);
        if (tp) {
            int newTabIndex = tabModel_.count();  // 即将插入的索引
            connect(tp, &TerminalTabPage::dataReceived, this, [this, tp, newTabIndex](const QByteArray& data) {
                if (ipcServer_) {
                    // 查找当前实际索引 (防止 Tab 顺序变化)
                    int idx = -1;
                    for (int i = 0; i < tabModel_.count(); ++i) {
                        if (tabModel_.tabAt(i) == tp) { idx = i; break; }
                    }
                    if (idx >= 0) {
                        ipcServer_->sendTerminalOutput(idx, data, tp->tabType());
                    }
                }
            });
        }
    }

    tabModel_.addTab(page);
    currentTabIdx_ = tabModel_.count() - 1;
    spdlog::debug("AppCore::addTab: tabModel count={}, currentTabIdx={}",
                  tabModel_.count(), currentTabIdx_);
    emit currentTabIndexChanged();
    updateStatus();
    if (!uptimeTimer_->isActive())
        uptimeTimer_->start();
}

void AppCore::removeTab(TabPage* page)
{
    spdlog::debug("AppCore::removeTab: type={}, title={}",
                  static_cast<int>(page->tabType()), page->tabTitle().toStdString());

    // 先断开所有信号, 防止 closeConnection/deleteLater 期间信号触发回调
    disconnect(page, nullptr, this, nullptr);

    if (page->tabType() == TabType::Serial) {
        auto* sp = qobject_cast<SerialTabPage*>(page);
        if (sp) {
            usedPorts_.remove(sp->portName());
            refreshSerialPorts();
        }
    }
    tabConnParams_.remove(page);
    tabConnectTimes_.remove(page);
}

QObject* AppCore::getTabPage(int index)
{
    auto* page = tabModel_.tabAt(index);
    spdlog::debug("AppCore::getTabPage: index={}, page={}", index,
                  page ? page->tabTitle().toStdString() : "null");
    return page;
}

void AppCore::createConnection()
{
    refreshSerialPorts();
    emit wizardRequested();
}

void AppCore::showWizard()
{
    refreshSerialPorts();
    emit wizardRequested();
}

void AppCore::confirmConnection(const QJsonObject& params)
{
    QString connType = params["type"].toString();
    QString name = params["name"].toString().trimmed();
    spdlog::debug("AppCore::confirmConnection: type={}, name={}", connType.toStdString(), name.toStdString());

    SavedPort sp;
    sp.name = name;

    if (connType == "serial") {
        sp.type = TabType::Serial;
        sp.port = params["port"].toString().trimmed();
        sp.baudrate = params["baud"].toInt(115200);
        sp.databits = static_cast<QSerialPort::DataBits>(params["data_bits"].toInt(8));
        sp.parity = static_cast<QSerialPort::Parity>(params["parity"].toInt(0));
        sp.stopbits = static_cast<QSerialPort::StopBits>(params["stop_bits"].toInt(1));

        auto* page = new SerialTabPage(this);
        page->setPortName(sp.port);
        addTab(page);

        QJsonObject connParams;
        connParams["port"] = sp.port;
        connParams["baud"] = sp.baudrate;
        connParams["data_bits"] = static_cast<int>(sp.databits);
        connParams["parity"] = static_cast<int>(sp.parity);
        connParams["stop_bits"] = static_cast<int>(sp.stopbits);
        // 先显示"连接中"
        statusText_ = QString::fromUtf8("○ %1 连接中...").arg(sp.port);
        emit statusTextChanged();

        page->connectTo(connParams);

        usedPorts_.insert(sp.port);
        refreshSerialPorts();
        tabConnParams_[page] = connParams;
        tabConnectTimes_[page] = QDateTime::currentDateTime();

    } else if (connType == "terminal") {
        sp.type = TabType::CMD;
        sp.port = params["shell"].toString();
        sp.extra["shell"] = params["shell"].toString();

        QString shell = params["shell"].toString();
        spdlog::debug("AppCore::confirmConnection: creating TerminalTabPage(CMD), shell={}", shell.toStdString());
        auto* page = new TerminalTabPage(TabType::CMD, this);
        addTab(page);

        QJsonObject connParams;
        connParams["shell"] = shell;
        spdlog::debug("AppCore::confirmConnection: calling page->connectTo, view_={}", (void*)nullptr);
        page->connectTo(connParams);

        statusText_ = QString::fromUtf8("○ %1 连接中...").arg(shell);
        emit statusTextChanged();
        tabConnParams_[page] = connParams;
        tabConnectTimes_[page] = QDateTime::currentDateTime();

    } else if (connType == "ssh") {
        sp.type = TabType::SSH;
        sp.extra["host"] = params["host"].toString().trimmed();
        sp.extra["port"] = params["ssh_port"].toInt(22);
        sp.extra["user"] = params["user"].toString().trimmed();
        sp.extra["password"] = params["password"].toString();

        QString host = sp.extra["host"].toString();
        QString user = sp.extra["user"].toString();
        sp.port = params["ssh_port"].toString();

        auto* page = new TerminalTabPage(TabType::SSH, this);
        addTab(page);

        QJsonObject connParams;
        connParams["host"] = host;
        connParams["port"] = sp.extra["port"].toInt(22);
        connParams["user"] = user;
        connParams["password"] = sp.extra["password"].toString();
        page->connectTo(connParams);

        statusText_ = QString::fromUtf8("○ %1@%2 连接中...").arg(user, host);
        emit statusTextChanged();
        tabConnParams_[page] = connParams;
        tabConnectTimes_[page] = QDateTime::currentDateTime();
    } else {
        spdlog::warn("AppCore::confirmConnection: unknown connection type '{}'", connType.toStdString());
        statusText_ = QString::fromUtf8("未知连接类型: %1").arg(connType);
        emit statusTextChanged();
        return;
    }

    // 保存到配置文件
    auto& config = ConfigManager::instance().config();
    config.savedPorts.append(sp);
    ConfigManager::instance().save();
    savedPortModel_.load();

    emit statusTextChanged();
    emit portConfigTextChanged();
    emit uptimeTextChanged();
    refreshSavedPortConnected();
    spdlog::info("Connection created: type={}, name={}", connType.toStdString(), name.toStdString());
}

void AppCore::connectSavedPort(int index)
{
    const auto& ports = ConfigManager::instance().config().savedPorts;
    if (index < 0 || index >= ports.size()) {
        spdlog::warn("AppCore::connectSavedPort: invalid index={}, size={}", index, ports.size());
        return;
    }

    const SavedPort& sp = ports[index];
    spdlog::debug("AppCore::connectSavedPort: index={}, type={}, name={}",
                  index, static_cast<int>(sp.type), sp.name.toStdString());

    // ── 去重: 检查是否已有相同配置的 Tab，有则切换 ──
    for (int i = 0; i < tabModel_.count(); ++i) {
        TabPage* existing = tabModel_.tabAt(i);
        if (!existing || existing->tabType() != sp.type) continue;

        auto it = tabConnParams_.find(existing);
        if (it == tabConnParams_.end()) continue;
        const QJsonObject& cp = it.value();

        bool match = false;
        switch (sp.type) {
        case TabType::Serial:
            match = (cp["port"].toString() == sp.port);
            break;
        case TabType::CMD:
            match = (cp["shell"].toString() == sp.extra["shell"].toString(defaultShell()));
            break;
        case TabType::SSH:
            match = (cp["host"].toString() == sp.extra["host"].toString() &&
                     cp["user"].toString() == sp.extra["user"].toString());
            break;
        default: break;
        }

        if (match) {
            spdlog::info("Tab already exists, switching to index={}", i);
            setCurrentTab(i);
            // 如果 Tab 已断开，重新连接
            if (!existing->isConnected()) {
                existing->connectTo(it.value());
                spdlog::info("Reconnecting tab index={}", i);
            }
            return;
        }
    }

    switch (sp.type) {
    case TabType::Serial: {
        if (isPortUsed(sp.port)) {
            statusText_ = QString::fromUtf8("端口 %1 已连接").arg(sp.port);
            emit statusTextChanged();
            return;
        }

        // 检查端口是否存在于系统中
        bool portExists = false;
        for (const auto& info : QSerialPortInfo::availablePorts()) {
            if (info.portName() == sp.port) {
                portExists = true;
                break;
            }
        }
        if (!portExists) {
            statusText_ = QString::fromUtf8("端口 %1 不存在").arg(sp.port);
            emit statusTextChanged();
            return;
        }

        auto* page = new SerialTabPage(this);
        page->setPortName(sp.port);
        addTab(page);

        QJsonObject connParams;
        connParams["port"] = sp.port;
        connParams["baud"] = sp.baudrate;
        connParams["data_bits"] = static_cast<int>(sp.databits);
        connParams["parity"] = static_cast<int>(sp.parity);
        connParams["stop_bits"] = static_cast<int>(sp.stopbits);

        // 先显示"连接中"
        statusText_ = QString::fromUtf8("○ %1 连接中...").arg(sp.port);
        emit statusTextChanged();

        page->connectTo(connParams);

        usedPorts_.insert(sp.port);
        refreshSerialPorts();
        tabConnParams_[page] = connParams;
        tabConnectTimes_[page] = QDateTime::currentDateTime();
        break;
    }
    case TabType::CMD: {
        QString shell = sp.extra["shell"].toString(defaultShell());
        auto* page = new TerminalTabPage(TabType::CMD, this);
        addTab(page);

        QJsonObject connParams;
        connParams["shell"] = shell;
        page->connectTo(connParams);

        statusText_ = QString::fromUtf8("● %1").arg(shell);
        tabConnParams_[page] = connParams;
        tabConnectTimes_[page] = QDateTime::currentDateTime();
        break;
    }
    case TabType::SSH: {
        QString host = sp.extra["host"].toString();
        QString user = sp.extra["user"].toString();
        int port = sp.extra["port"].toInt(22);

        auto* page = new TerminalTabPage(TabType::SSH, this);
        addTab(page);

        QJsonObject connParams;
        connParams["host"] = host;
        connParams["port"] = port;
        connParams["user"] = user;
        connParams["password"] = sp.extra["password"].toString();
        page->connectTo(connParams);

        statusText_ = QString::fromUtf8("● %1@%2").arg(user, host);
        tabConnParams_[page] = connParams;
        tabConnectTimes_[page] = QDateTime::currentDateTime();
        break;
    }
    default:
        break;
    }
    emit statusTextChanged();
    emit portConfigTextChanged();
    emit uptimeTextChanged();
}

void AppCore::removeSavedPort(int index)
{
    auto& config = ConfigManager::instance().config();
    if (index < 0 || index >= config.savedPorts.size()) {
        spdlog::warn("AppCore::removeSavedPort: invalid index={}, size={}", index, config.savedPorts.size());
        return;
    }

    spdlog::debug("AppCore::removeSavedPort: index={}, name={}",
                  index, config.savedPorts[index].name.toStdString());

    // 先关闭对应的 Tab
    closeTabForSavedPort(index);

    config.savedPorts.removeAt(index);
    ConfigManager::instance().save();
    savedPortModel_.load();

    spdlog::info("Saved port removed: index={}", index);
}

void AppCore::setCurrentTab(int index)
{
    if (index < 0 || index >= tabModel_.count()) return;
    currentTabIdx_ = index;
    emit currentTabIndexChanged();

    TabPage* page = tabModel_.tabAt(currentTabIdx_);
    if (page) {
        // 恢复当前 tab 的 statusText
        switch (page->tabType()) {
        case TabType::Serial: {
            auto it = tabConnParams_.find(page);
            QString port = it != tabConnParams_.end() ? it.value()["port"].toString() : page->tabTitle();
            statusText_ = QString::fromUtf8("● %1").arg(port);
            break;
        }
        case TabType::CMD: {
            auto it = tabConnParams_.find(page);
            QString shell = it != tabConnParams_.end() ? it.value()["shell"].toString() : page->tabTitle();
            statusText_ = QString::fromUtf8("● %1").arg(shell);
            break;
        }
        case TabType::SSH: {
            auto it = tabConnParams_.find(page);
            QString user = it != tabConnParams_.end() ? it.value()["user"].toString() : "";
            QString host = it != tabConnParams_.end() ? it.value()["host"].toString() : "";
            if (!user.isEmpty() && !host.isEmpty())
                statusText_ = QString::fromUtf8("● %1@%2").arg(user, host);
            else
                statusText_ = QString::fromUtf8("● %1").arg(page->tabTitle());
            break;
        }
        default:
            statusText_ = QString::fromUtf8("● %1").arg(page->tabTitle());
            break;
        }
    }
    updateStatus();
    emit statusTextChanged();
}

void AppCore::closeCurrentTab()
{
    closeTab(currentTabIdx_);
}

void AppCore::closeTab(int index)
{
    if (index < 0 || index >= tabModel_.count()) {
        spdlog::warn("AppCore::closeTab: invalid index={}, count={}", index, tabModel_.count());
        return;
    }

    TabPage* page = tabModel_.tabAt(index);
    spdlog::debug("AppCore::closeTab: index={}, title={}", index,
                  page ? page->tabTitle().toStdString() : "null");
    if (page) {
        // 1. 断开信号 + 清理引用 (防止回调访问无效状态)
        removeTab(page);
        // 2. 关闭底层连接
        page->closeConnection();
        // 3. 延迟删除对象 (等当前事件循环完成)
        page->deleteLater();
    }
    // 4. 从模型移除 (触发 QML Repeater 移除 Loader)
    tabModel_.removeTab(index);

    // 5. 调整当前索引 (必须在 removeTab 之后, 确保索引有效)
    if (currentTabIdx_ >= tabModel_.count())
        currentTabIdx_ = tabModel_.count() - 1;
    spdlog::debug("AppCore::closeTab: after close, count={}, currentTabIdx={}",
                  tabModel_.count(), currentTabIdx_);
    emit currentTabIndexChanged();
    updateStatus();
    if (tabModel_.count() == 0 && uptimeTimer_->isActive())
        uptimeTimer_->stop();
}

void AppCore::openSettings()
{
    emit settingsRequested();
    statusText_ = QString::fromUtf8("● 设置");
    emit statusTextChanged();
}

void AppCore::openAbout()
{
    // 仅发出信号让 QML 的 AboutDialog 处理显示
    // 移除 QMessageBox::about 调用，避免双重弹窗和 QWidget 依赖
    emit aboutRequested();
}

void AppCore::saveSettings(int fontSize, int maxLogLines, bool autoScroll)
{
    auto& cfg = ConfigManager::instance().config();
    cfg.display.fontSize = fontSize;
    cfg.display.bufferSize = maxLogLines;
    cfg.display.autoScroll = autoScroll;
    ConfigManager::instance().save();
    spdlog::info("Settings saved: fontSize={}, maxLogLines={}, autoScroll={}",
                 fontSize, maxLogLines, autoScroll);
}

QVariantMap AppCore::loadSettings()
{
    auto& cfg = ConfigManager::instance().config();
    QVariantMap map;
    map["fontSize"] = cfg.display.fontSize;
    map["maxLogLines"] = cfg.display.bufferSize;
    map["autoScroll"] = cfg.display.autoScroll;
    map["hexMode"] = cfg.display.hexMode;
    map["showTimestamp"] = cfg.display.showTimestamp;
    map["autoReconnect"] = cfg.display.autoReconnect;
    map["autoCheckUpdate"] = cfg.display.autoCheckUpdate;
    return map;
}

void AppCore::saveAutoCheckUpdate(bool enabled)
{
    auto& cfg = ConfigManager::instance().config();
    cfg.display.autoCheckUpdate = enabled;
    ConfigManager::instance().save();
    spdlog::info("Auto check update: {}", enabled);
}

QString AppCore::currentVersion() const
{
    return QString("v%1").arg(QCoreApplication::applicationVersion());
}

void AppCore::checkUpdate()
{
    if (!networkManager_) {
        networkManager_ = new QNetworkAccessManager(this);
    }

    // GitHub API: 获取最新 release
    // 仓库已迁移到 kukucaiCndy/EmberinterTool
    QNetworkRequest request(QUrl("https://api.github.com/repos/kukucaiCndy/EmberinterTool/releases/latest"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "EmberInterDebugTool-UpdateCheck");
    request.setRawHeader("Accept", "application/vnd.github+json");

    spdlog::info("Checking for updates...");
    QNetworkReply* reply = networkManager_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QJsonObject result;

        if (reply->error() != QNetworkReply::NoError) {
            spdlog::warn("Update check failed: {}", reply->errorString().toStdString());
            result["error"] = true;
            result["errorMsg"] = reply->errorString();
            result["hasUpdate"] = false;
            emit updateCheckResult(result);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject release = doc.object();

        QString latestTag = release["tag_name"].toString();
        QString htmlUrl = release["html_url"].toString();
        QString releaseName = release["name"].toString();
        QString body = release["body"].toString();

        // 提取下载链接 (第一个 exe 资产)
        QString downloadUrl;
        QJsonArray assets = release["assets"].toArray();
        for (const auto& a : assets) {
            QJsonObject asset = a.toObject();
            QString name = asset["name"].toString();
            if (name.endsWith(".exe", Qt::CaseInsensitive)) {
                downloadUrl = asset["browser_download_url"].toString();
                break;
            }
        }

        // 版本比较: 当前版本 vs 最新版本
        QString curVer = currentVersion();  // v1.3.0
        bool hasUpdate = false;
        if (!latestTag.isEmpty()) {
            // 简单字符串比较 (v1.3.0 < v1.4.0)
            hasUpdate = (latestTag != curVer);
            spdlog::info("Update check: current={}, latest={}, hasUpdate={}",
                         curVer.toStdString(), latestTag.toStdString(), hasUpdate);
        }

        result["error"] = false;
        result["hasUpdate"] = hasUpdate;
        result["currentVersion"] = curVer;
        result["latestVersion"] = latestTag;
        result["releaseName"] = releaseName;
        result["releaseNotes"] = body;
        result["downloadUrl"] = downloadUrl;
        result["htmlUrl"] = htmlUrl;

        emit updateCheckResult(result);
    });
}

void AppCore::downloadUpdate(const QString& url)
{
    if (url.isEmpty()) {
        emit updateDownloadFinished("", "下载链接为空");
        return;
    }

    if (!networkManager_) {
        networkManager_ = new QNetworkAccessManager(this);
    }

    // 取消已有下载
    if (downloadReply_) {
        downloadReply_->abort();
        downloadReply_->deleteLater();
        downloadReply_ = nullptr;
    }

    // 下载到临时目录
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    // 从 URL 提取文件名
    QString fileName = url.section('/', -1);
    if (fileName.isEmpty() || !fileName.endsWith(".exe", Qt::CaseInsensitive)) {
        fileName = "emberInter-Setup.exe";
    }
    downloadFilePath_ = tempDir + "/" + fileName;

    spdlog::info("Downloading update: {} -> {}", url.toStdString(), downloadFilePath_.toStdString());

    QUrl downloadUrl(url);
    QNetworkRequest request(downloadUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EmberInterDebugTool-UpdateDownloader");

    downloadReply_ = networkManager_->get(request);

    // 下载进度
    connect(downloadReply_, &QNetworkReply::downloadProgress,
            this, [this](qint64 received, qint64 total) {
        int percent = -1;
        if (total > 0) {
            percent = static_cast<int>(received * 100 / total);
        }
        emit updateDownloadProgress(received, total, percent);
    });

    // 下载完成
    connect(downloadReply_, &QNetworkReply::finished, this, [this]() {
        if (downloadReply_->error() != QNetworkReply::NoError) {
            spdlog::warn("Download failed: {}", downloadReply_->errorString().toStdString());
            emit updateDownloadFinished("", downloadReply_->errorString());
            downloadReply_->deleteLater();
            downloadReply_ = nullptr;
            return;
        }

        // 保存文件
        QFile file(downloadFilePath_);
        if (!file.open(QIODevice::WriteOnly)) {
            emit updateDownloadFinished("", "无法写入临时文件: " + downloadFilePath_);
            downloadReply_->deleteLater();
            downloadReply_ = nullptr;
            return;
        }
        file.write(downloadReply_->readAll());
        file.close();

        spdlog::info("Download complete: {}", downloadFilePath_.toStdString());
        emit updateDownloadFinished(downloadFilePath_, "");

        downloadReply_->deleteLater();
        downloadReply_ = nullptr;

        // 启动安装程序 (异步, 不阻塞退出)
        bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(downloadFilePath_));
        if (ok) {
            spdlog::info("Installer launched: {}", downloadFilePath_.toStdString());
            // 提示用户安装程序已启动, 当前程序可退出
            QTimer::singleShot(2000, qApp, &QCoreApplication::quit);
        } else {
            spdlog::error("Failed to launch installer: {}", downloadFilePath_.toStdString());
        }
    });
}

void AppCore::loadTabContent(int index, QObject* container)
{
    Q_UNUSED(index);
    Q_UNUSED(container);
    // Phase 3: QML Loader 按 tabType 自动加载
    // TabPage 引用通过 getTabPage() 获取
}

QString AppCore::formatUptime(qint64 seconds) const
{
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    return QString::fromUtf8("运行 %1:%2:%3")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}

void AppCore::updateStatus()
{
    TabPage* page = tabModel_.tabAt(currentTabIdx_);
    hasActiveTab_ = (page != nullptr);

    if (page) {
        rxBytesText_ = QString("RX: %1").arg(page->rxBytes());
        txBytesText_ = QString("TX: %1").arg(page->txBytes());

        // 端口配置文本
        auto it = tabConnParams_.find(page);
        if (it != tabConnParams_.end()) {
            const QJsonObject& cp = it.value();
            if (page->tabType() == TabType::Serial) {
                int baud = cp["baud"].toInt(115200);
                int dataBits = cp["data_bits"].toInt(8);
                int parity = cp["parity"].toInt(0);
                int stopBits = cp["stop_bits"].toInt(1);
                const char* parityChars[] = {"N", "O", "E"};
                const char* parityStr = (parity >= 0 && parity <= 2) ? parityChars[parity] : "N";
                portConfigText_ = QString("%1 %2%3%4")
                    .arg(baud).arg(dataBits).arg(parityStr).arg(stopBits);
            } else {
                portConfigText_ = "";
            }
        } else {
            portConfigText_ = "";
        }

        // 运行时长
        auto itt = tabConnectTimes_.find(page);
        if (itt != tabConnectTimes_.end()) {
            qint64 elapsed = itt.value().secsTo(QDateTime::currentDateTime());
            uptimeText_ = formatUptime(elapsed);
        } else {
            uptimeText_ = "";
        }
    } else {
        rxBytesText_ = "RX: 0";
        txBytesText_ = "TX: 0";
        portConfigText_ = "";
        uptimeText_ = "";
        statusText_ = QString::fromUtf8("就绪");
    }

    emit hasActiveTabChanged();
    emit rxBytesTextChanged();
    emit txBytesTextChanged();
    emit portConfigTextChanged();
    emit uptimeTextChanged();
    if (!hasActiveTab_)
        emit statusTextChanged();
}

// ── Tab 回调 ──────────────────────────────────────────────

void AppCore::onTabRxChanged(qint64 bytes)
{
    // 仅当信号来自当前 Tab 时才更新状态栏，避免多 Tab 场景下显示错乱
    TabPage* page = qobject_cast<TabPage*>(sender());
    if (page && page == tabModel_.tabAt(currentTabIdx_)) {
        rxBytes_ = bytes;
        emit rxBytesTextChanged();
    }
}

void AppCore::onTabTxChanged(qint64 bytes)
{
    // 仅当信号来自当前 Tab 时才更新状态栏
    TabPage* page = qobject_cast<TabPage*>(sender());
    if (page && page == tabModel_.tabAt(currentTabIdx_)) {
        txBytes_ = bytes;
        emit txBytesTextChanged();
    }
}

void AppCore::onTabStatusChanged(bool connected)
{
    // 根据当前 Tab 的连接状态更新状态栏文本
    TabPage* page = qobject_cast<TabPage*>(sender());
    if (!page) return;

    // IPC 广播: 状态变更
    if (ipcServer_ && ipcServer_->clientCount() > 0) {
        SerialConfig cfg;
        QString port = page->tabTitle();
        if (page->tabType() == TabType::Serial) {
            auto* sp = qobject_cast<SerialTabPage*>(page);
            if (sp) {
                port = sp->portName();
            }
        }
        qint64 uptime = 0;
        auto itt = tabConnectTimes_.find(page);
        if (itt != tabConnectTimes_.end())
            uptime = itt.value().secsTo(QDateTime::currentDateTime());
        ipcServer_->broadcastStatus(port, connected, cfg,
                                    page->rxBytes(), page->txBytes(), uptime);
    }

    if (page->tabType() == TabType::Serial) {
        auto it = tabConnParams_.find(page);
        QString port = it != tabConnParams_.end() ? it.value()["port"].toString() : page->tabTitle();
        if (connected) {
            statusText_ = QString::fromUtf8("● %1").arg(port);
        } else {
            statusText_ = QString::fromUtf8("○ %1 已断开").arg(port);
        }
        emit statusTextChanged();
    }

    updateStatus();
    refreshSavedPortConnected();
}

void AppCore::onIpcCommand(const QString& clientId, const QString& cmd,
                           const QJsonObject& params, const QString& reqId)
{
    QJsonObject data;
    TabPage* page = tabModel_.tabAt(currentTabIdx_);

    // ── 无状态命令 ──
    if (cmd == "activate_window") {
        if (QWindow* w = QGuiApplication::focusWindow()) {
            w->requestActivate();
        } else if (!QGuiApplication::topLevelWindows().isEmpty()) {
            QGuiApplication::topLevelWindows().first()->requestActivate();
        }
        return;
    }

    if (cmd == "list_ports") {
        QJsonArray ports;
        for (const auto& info : QSerialPortInfo::availablePorts()) {
            QJsonObject port;
            port["name"] = info.portName();
            port["description"] = info.description();
            port["manufacturer"] = info.manufacturer();
            // 标记有厂商信息的物理设备为推荐
            port["recommended"] = !info.manufacturer().isEmpty();
            ports.append(port);
        }
        data["ports"] = ports;
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    // ── Tab 管理命令 (新增) ──

    if (cmd == "list_tabs") {
        QJsonArray tabs;
        for (int i = 0; i < tabModel_.count(); ++i) {
            TabPage* t = tabModel_.tabAt(i);
            if (!t) continue;
            QJsonObject tab;
            tab["index"] = i;
            tab["type"] = tabTypeToString(t->tabType());
            tab["title"] = t->tabTitle();
            tab["connected"] = t->isConnected();
            tab["is_current"] = (i == currentTabIdx_);
            tabs.append(tab);
        }
        data["tabs"] = tabs;
        data["current_index"] = currentTabIdx_;
        data["count"] = tabModel_.count();
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    if (cmd == "switch_tab") {
        int index = params["index"].toInt(-1);
        if (index < 0 || index >= tabModel_.count()) {
            data["message"] = QString("Invalid tab index: %1").arg(index);
            ipcServer_->sendResponse(clientId, reqId, false, data);
            return;
        }
        setCurrentTab(index);
        data["message"] = QString("Switched to tab %1").arg(index);
        data["current_index"] = currentTabIdx_;
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    if (cmd == "close_tab") {
        int index = params["index"].toInt(currentTabIdx_);
        if (index < 0 || index >= tabModel_.count()) {
            data["message"] = QString("Invalid tab index: %1").arg(index);
            ipcServer_->sendResponse(clientId, reqId, false, data);
            return;
        }
        closeTab(index);
        data["message"] = QString("Tab %1 closed").arg(index);
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    // create_tab: 创建 CMD/SSH 终端 Tab
    if (cmd == "create_tab") {
        QString type = params["type"].toString().toLower();
        QJsonObject connParams;

        if (type == "cmd" || type == "terminal") {
            QString shell = params.value("shell").toString(defaultShell());
            connParams["shell"] = shell;
            auto* tp = new TerminalTabPage(TabType::CMD, this);
            addTab(tp);
            tp->connectTo(connParams);
            tabConnParams_[tp] = connParams;
            tabConnectTimes_[tp] = QDateTime::currentDateTime();

            data["message"] = QString("Terminal tab created: %1").arg(shell);
            data["tab_index"] = tabModel_.count() - 1;
            data["tab_type"] = tabTypeToString(TabType::CMD);
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        if (type == "ssh") {
            QString host = params.value("host").toString();
            int port = params.value("port").toInt(22);
            QString user = params.value("user").toString();
            if (host.isEmpty()) {
                data["message"] = "SSH requires 'host' parameter";
                ipcServer_->sendResponse(clientId, reqId, false, data);
                return;
            }
            connParams["host"] = host;
            connParams["port"] = port;
            connParams["user"] = user;
            connParams["password"] = params.value("password").toString();

            auto* tp = new TerminalTabPage(TabType::SSH, this);
            addTab(tp);
            tp->connectTo(connParams);
            tabConnParams_[tp] = connParams;
            tabConnectTimes_[tp] = QDateTime::currentDateTime();

            data["message"] = QString("SSH tab created: %1@%2:%3").arg(user, host).arg(port);
            data["tab_index"] = tabModel_.count() - 1;
            data["tab_type"] = tabTypeToString(TabType::SSH);
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        data["message"] = QString("Unknown tab type: %1 (use cmd/ssh)").arg(type);
        ipcServer_->sendResponse(clientId, reqId, false, data);
        return;
    }

    // 终端输出订阅
    if (cmd == "subscribe_tab") {
        int index = params["index"].toInt(-1);
        if (index < 0 || index >= tabModel_.count()) {
            data["message"] = QString("Invalid tab index: %1").arg(index);
            ipcServer_->sendResponse(clientId, reqId, false, data);
            return;
        }
        ipcServer_->subscribeTab(clientId, index);
        data["message"] = QString("Subscribed to tab %1").arg(index);
        data["tab_index"] = index;
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    if (cmd == "unsubscribe_tab") {
        ipcServer_->unsubscribeTab(clientId);
        data["message"] = "Unsubscribed";
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    // 终端输入: 向指定 Tab (默认当前) 的终端发送原始数据
    if (cmd == "terminal_input") {
        int index = params.value("tab_index").toInt(currentTabIdx_);
        if (index < 0 || index >= tabModel_.count()) {
            data["message"] = QString("Invalid tab index: %1").arg(index);
            ipcServer_->sendResponse(clientId, reqId, false, data);
            return;
        }
        TabPage* target = tabModel_.tabAt(index);
        auto* tp = qobject_cast<TerminalTabPage*>(target);
        if (!tp) {
            data["message"] = QString("Tab %1 is not a terminal tab").arg(index);
            ipcServer_->sendResponse(clientId, reqId, false, data);
            return;
        }
        // data 字段: 优先用 base64 (保留原始字节), 否则用 text 字符串
        QByteArray input;
        if (params.contains("data")) {
            input = QByteArray::fromBase64(params["data"].toString().toLatin1());
        } else if (params.contains("text")) {
            input = params["text"].toString().toUtf8();
        }
        if (input.isEmpty()) {
            data["message"] = "No input data provided (use 'data' base64 or 'text')";
            ipcServer_->sendResponse(clientId, reqId, false, data);
            return;
        }
        tp->writeRawInput(input);
        data["message"] = QString("Sent %1 bytes to tab %2").arg(input.size()).arg(index);
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    if (cmd == "get_status") {
        data["connected"] = page ? page->isConnected() : false;
        data["rx_bytes"] = rxBytes_;
        data["tx_bytes"] = txBytes_;
        data["buffer_size"] = 0;
        if (page) {
            data["port"] = page->tabTitle();
            data["tab_type"] = tabTypeToString(page->tabType());
            data["tab_index"] = currentTabIdx_;
        }
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    if (cmd == "clear" || cmd == "clear_logs") {
        if (page) page->clearContent();
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    // ── 需要活动串口 Tab 的命令 ──
    // connect 命令单独处理: 无活动串口 Tab 时自动创建
    if (cmd == "connect") {
        QString port = params["port"].toString();
        int baud = params["baudrate"].toInt(115200);
        QJsonObject c;
        c["port"] = port;
        c["baud"] = baud;
        c["data_bits"] = params.value("data_bits").toInt(8);
        c["parity"] = params.value("parity").toInt(0);
        c["stop_bits"] = params.value("stop_bits").toInt(1);

        auto* sp = qobject_cast<SerialTabPage*>(page);
        if (!sp) {
            // 无活动串口 Tab, 自动创建
            sp = new SerialTabPage(this);
            sp->setPortName(port);
            addTab(sp);
            usedPorts_.insert(port);
            refreshSerialPorts();
            tabConnParams_[sp] = c;
            tabConnectTimes_[sp] = QDateTime::currentDateTime();
        }
        sp->connectTo(c);
        data["message"] = "Connecting...";
        data["tab_index"] = currentTabIdx_;
        ipcServer_->sendResponse(clientId, reqId, true, data);
        return;
    }

    if (cmd == "send_text" || cmd == "send_hex" || cmd == "send_file" ||
        cmd == "disconnect" ||
        cmd == "get_logs" || cmd == "set_filter" || cmd == "export_logs" ||
        cmd == "pause" || cmd == "resume") {

        auto* sp = qobject_cast<SerialTabPage*>(page);
        if (!sp) {
            data["message"] = "No active serial tab";
            ipcServer_->sendResponse(clientId, reqId, false, data);
            return;
        }

        if (cmd == "send_text") {
            QString text = params["data"].toString();
            sp->sendText(text, params["append"].toString("CRLF"));
            data["message"] = "Sent";
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        if (cmd == "send_hex") {
            QString hexStr = params["data"].toString();
            sp->sendHex(hexStr);
            data["message"] = "HEX sent";
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        if (cmd == "send_file") {
            // CLI 发送 Base64 编码的文件内容
            QString b64 = params["data"].toString();
            QByteArray raw = QByteArray::fromBase64(b64.toLatin1());
            sp->sendRaw(raw);
            data["message"] = QString("Sent %1 bytes").arg(raw.size());
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        if (cmd == "disconnect") {
            sp->closeConnection();
            data["message"] = "Disconnected";
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        if (cmd == "get_logs") {
            int count = params["count"].toInt(100);
            QJsonArray entries = sp->getRecentLogs(qMin(count, 500));
            data["total_count"] = entries.size();
            data["returned_count"] = entries.size();
            data["entries"] = entries;
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        if (cmd == "set_filter") {
            QString kw = params["keyword"].toString();
            sp->setFilter(kw);
            data["message"] = "Filter set";
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        if (cmd == "export_logs") {
            QString path = params["file_path"].toString();
            sp->exportContent(path);
            data["message"] = QString("Exported to %1").arg(path);
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        if (cmd == "pause") {
            sp->setPaused(true);
            data["message"] = "Log paused";
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }

        if (cmd == "resume") {
            sp->setPaused(false);
            data["message"] = "Log resumed";
            ipcServer_->sendResponse(clientId, reqId, true, data);
            return;
        }
    }

    ipcServer_->sendResponse(clientId, reqId, false, data);
}

// ── Tab 右键菜单操作 ──────────────────────────────────────

void AppCore::renameTab(int index, const QString& name)
{
    TabPage* page = tabModel_.tabAt(index);
    if (!page) return;
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return;
    page->setTabTitle(trimmed);
    // 触发 TabModel 数据更新 (标题角色)
    QModelIndex idx = tabModel_.index(index);
    emit tabModel_.dataChanged(idx, idx, {TabModel::TitleRole});
    spdlog::info("Tab renamed: index={}, name={}", index, trimmed.toStdString());
}

void AppCore::toggleConnection(int index)
{
    TabPage* page = tabModel_.tabAt(index);
    if (!page) return;

    if (page->isConnected()) {
        page->closeConnection();
        spdlog::info("Connection toggled off: index={}", index);
    } else {
        // 重新连接: 使用保存的连接参数
        auto it = tabConnParams_.find(page);
        if (it != tabConnParams_.end()) {
            page->connectTo(it.value());
            spdlog::info("Connection toggled on: index={}", index);
        }
    }
}

int AppCore::findSavedPortForTab(int index) const
{
    TabPage* page = tabModel_.tabAt(index);
    if (!page) return -1;

    auto it = tabConnParams_.constFind(page);
    if (it == tabConnParams_.constEnd()) return -1;
    const QJsonObject& cp = it.value();

    const auto& ports = ConfigManager::instance().config().savedPorts;
    for (int i = 0; i < ports.size(); ++i) {
        const SavedPort& sp = ports[i];
        if (sp.type != page->tabType()) continue;

        bool match = false;
        switch (sp.type) {
        case TabType::Serial:
            match = (cp["port"].toString() == sp.port);
            break;
        case TabType::CMD:
            match = (cp["shell"].toString() == sp.extra["shell"].toString(defaultShell()));
            break;
        case TabType::SSH:
            match = (cp["host"].toString() == sp.extra["host"].toString() &&
                     cp["user"].toString() == sp.extra["user"].toString());
            break;
        default: break;
        }
        if (match) return i;
    }
    return -1;
}

void AppCore::deleteSavedPortForTab(int index)
{
    int savedIdx = findSavedPortForTab(index);
    if (savedIdx >= 0) {
        removeSavedPort(savedIdx);
        spdlog::info("Saved port deleted for tab: index={}, savedIdx={}", index, savedIdx);
    }
}

// ── 会话列表右键菜单操作 ──────────────────────────────────

int AppCore::findTabForSavedPort(int savedIndex) const
{
    const auto& ports = ConfigManager::instance().config().savedPorts;
    if (savedIndex < 0 || savedIndex >= ports.size()) return -1;

    const SavedPort& sp = ports[savedIndex];
    for (int i = 0; i < tabModel_.count(); ++i) {
        TabPage* page = tabModel_.tabAt(i);
        if (!page || page->tabType() != sp.type) continue;

        auto it = tabConnParams_.constFind(page);
        if (it == tabConnParams_.constEnd()) continue;
        const QJsonObject& cp = it.value();

        bool match = false;
        switch (sp.type) {
        case TabType::Serial:
            match = (cp["port"].toString() == sp.port);
            break;
        case TabType::CMD:
            match = (cp["shell"].toString() == sp.extra["shell"].toString(defaultShell()));
            break;
        case TabType::SSH:
            match = (cp["host"].toString() == sp.extra["host"].toString() &&
                     cp["user"].toString() == sp.extra["user"].toString());
            break;
        default: break;
        }
        if (match) return i;
    }
    return -1;
}

void AppCore::toggleSavedPort(int savedIndex)
{
    int tabIdx = findTabForSavedPort(savedIndex);
    if (tabIdx >= 0) {
        // Tab 已存在, 切换连接状态
        toggleConnection(tabIdx);
    } else {
        // Tab 不存在, 创建并连接
        connectSavedPort(savedIndex);
    }
}

void AppCore::closeTabForSavedPort(int savedIndex)
{
    int tabIdx = findTabForSavedPort(savedIndex);
    if (tabIdx >= 0) {
        closeTab(tabIdx);
    }
}

void AppCore::renameSavedPort(int savedIndex, const QString& name)
{
    auto& config = ConfigManager::instance().config();
    if (savedIndex < 0 || savedIndex >= config.savedPorts.size()) return;
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return;

    config.savedPorts[savedIndex].name = trimmed;
    ConfigManager::instance().save();
    savedPortModel_.load();

    // 如果对应的 Tab 已打开, 也重命名 Tab
    int tabIdx = findTabForSavedPort(savedIndex);
    if (tabIdx >= 0) {
        renameTab(tabIdx, trimmed);
    }
    spdlog::info("Saved port renamed: index={}, name={}", savedIndex, trimmed.toStdString());
}