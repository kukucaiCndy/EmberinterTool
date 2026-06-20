#include "serial_tab_page.h"
#include "log_parser.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <spdlog/spdlog.h>

// ═══════════════════════════════════════════════════════════
// LogListModel
// ═══════════════════════════════════════════════════════════

LogListModel::LogListModel(QObject* parent)
    : QAbstractListModel(parent)
    , maxEntries_(10000)
{
}

int LogListModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return entries_.size();
}

QVariant LogListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};

    const auto& e = entries_[index.row()];
    switch (role) {
    case DisplayRole:   return LogParser::formatDisplay(e, true);
    case TimestampRole: return e.timestamp;
    case LevelRole:     return e.level;
    case ColorRole:     return LogParser::levelColorHex(e.level);
    case RawHexRole:    return QString::fromUtf8(e.rawBytes.toHex(' ').toUpper());
    }
    return {};
}

QHash<int, QByteArray> LogListModel::roleNames() const
{
    return {
        {DisplayRole,   "display"},
        {TimestampRole, "timestamp"},
        {LevelRole,     "level"},
        {ColorRole,     "color"},
        {RawHexRole,    "rawHex"}
    };
}

void LogListModel::appendBatch(const QVector<LogEntry>& entries)
{
    if (entries.isEmpty()) return;

    int oldCount = entries_.size();
    int newCount = oldCount + entries.size();

    // 裁剪超出部分
    int overflow = newCount - maxEntries_;
    if (overflow > 0) {
        int removeCount = qMin(overflow, oldCount);
        beginRemoveRows({}, 0, removeCount - 1);
        entries_.remove(0, removeCount);
        endRemoveRows();
        oldCount -= removeCount;
        newCount -= removeCount;
    }

    beginInsertRows({}, oldCount, newCount - 1);
    entries_.append(entries);
    endInsertRows();
}

void LogListModel::rebuild(const QVector<LogEntry>& entries)
{
    beginResetModel();
    entries_ = entries.mid(qMax(0, entries.size() - maxEntries_));
    endResetModel();
}

void LogListModel::clear()
{
    beginResetModel();
    entries_.clear();
    endResetModel();
}

void LogListModel::setMaxEntries(int n)
{
    maxEntries_ = qMax(100, n);
}

// ═══════════════════════════════════════════════════════════
// SerialTabPage
// ═══════════════════════════════════════════════════════════

SerialTabPage::SerialTabPage(QObject* parent)
    : TabPage(parent)
    , connected_(false)
    , hexMode_(false)
    , paused_(false)
    , showTimestamp_(true)
    , autoScroll_(true)
    , rxBytes_(0)
    , txBytes_(0)
{
    spdlog::debug("SerialTabPage::ctor: this={}", (void*)this);
    flushTimer_ = new QTimer(this);
    flushTimer_->setInterval(50);  // 50ms 批量刷新
    flushTimer_->setSingleShot(true);
    connect(flushTimer_, &QTimer::timeout, this, &SerialTabPage::onFlushTimer);

    connect(&engine_, &SerialEngine::dataReceived,
            this, &SerialTabPage::onDataReceived);
    connect(&engine_, &SerialEngine::statusChanged,
            this, &SerialTabPage::onEngineStatusChanged);
    connect(&engine_, &SerialEngine::dataSent,
            this, &SerialTabPage::onDataSent);
}

SerialTabPage::~SerialTabPage()
{
    disconnect();
}

QString SerialTabPage::tabTitle() const
{
    return QString::fromUtf8("[串口] %1").arg(portName_);
}

void SerialTabPage::connectTo(const QJsonObject& params)
{
    portName_ = params["port"].toString();
    emit portNameChanged();
    emit tabTitleChanged();

    SerialConfig cfg;
    cfg.port       = portName_;
    cfg.baudrate   = params["baud"].toInt(115200);
    cfg.databits   = static_cast<QSerialPort::DataBits>(params["data_bits"].toInt(8));
    cfg.parity     = static_cast<QSerialPort::Parity>(params["parity"].toInt(0));
    cfg.stopbits   = static_cast<QSerialPort::StopBits>(params["stop_bits"].toInt(1));

    connectionTimer_.start();
    engine_.open(cfg);

    spdlog::info("SerialTabPage: connecting to {}", portName_.toStdString());
}

void SerialTabPage::disconnect()
{
    flushPending();
    engine_.close();
    connected_ = false;
    emit connectedChanged();
    emit statusChanged(false);
}

void SerialTabPage::clearContent()
{
    QMutexLocker lock(&pendingMutex_);
    pendingEntries_.clear();
    logBuffer_.clear();
    logModel_.clear();
    remainder_.clear();
    rxBytes_ = 0;
    txBytes_ = 0;
    emit rxBytesChanged(0);
    emit txBytesChanged(0);
}

void SerialTabPage::exportContent(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        spdlog::error("SerialTabPage: failed to export to {}", path.toStdString());
        return;
    }

    QTextStream stream(&file);
    for (const auto& entry : logBuffer_.getAll()) {
        stream << LogParser::formatDisplay(entry, showTimestamp_) << "\n";
    }

    spdlog::info("SerialTabPage: exported {} entries to {}",
                 logBuffer_.size(), path.toStdString());
}

// ── QML 可调用 ──────────────────────────────────────────

void SerialTabPage::sendText(const QString& text, const QString& append)
{
    if (!connected_) return;

    qint64 sent = engine_.sendText(text, append);
    txBytes_ += sent;
    emit txBytesChanged(txBytes_);

    // 记录发送数据到日志
    QByteArray rawData = text.toUtf8();
    if (append == "CRLF") rawData += "\r\n";
    else if (append == "LF") rawData += "\n";
    else if (append == "CR") rawData += "\r";

    LogEntry entry = LogParser::parseLine(rawData, portName_);
    entry.level = "TX";
    logBuffer_.append(entry);

    {
        QMutexLocker lock(&pendingMutex_);
        pendingEntries_.push_back(entry);
    }
    if (!flushTimer_->isActive())
        flushTimer_->start();
}

void SerialTabPage::sendHex(const QString& hex)
{
    if (!connected_) return;

    QByteArray data = QByteArray::fromHex(hex.toUtf8().replace(' ', ""));
    qint64 sent = engine_.sendRaw(data);
    txBytes_ += sent;
    emit txBytesChanged(txBytes_);
}

void SerialTabPage::setFilter(const QString& keyword)
{
    filter_ = keyword;
    emit filterChanged();

    if (filter_.isEmpty()) {
        rebuildModel();
    } else {
        auto filtered = logBuffer_.getFiltered(filter_);
        logModel_.rebuild(filtered);
    }
}

void SerialTabPage::setHexMode(bool enabled)
{
    hexMode_ = enabled;
    emit hexModeChanged();
    rebuildModel();
}

void SerialTabPage::setPaused(bool p)
{
    paused_ = p;
    emit pausedChanged();
    if (!p) {
        // 恢复时刷新
        flushPending();
        if (!filter_.isEmpty()) {
            auto filtered = logBuffer_.getFiltered(filter_);
            logModel_.rebuild(filtered);
        } else {
            rebuildModel();
        }
    }
}

void SerialTabPage::setShowTimestamp(bool show)
{
    showTimestamp_ = show;
    emit showTimestampChanged();
    rebuildModel();
}

void SerialTabPage::setAutoScroll(bool enabled)
{
    autoScroll_ = enabled;
    emit autoScrollChanged();
}

void SerialTabPage::clearLogs()
{
    clearContent();
}

void SerialTabPage::exportLogs(const QString& path)
{
    exportContent(path);
}

void SerialTabPage::setPortName(const QString& name)
{
    portName_ = name;
    emit portNameChanged();
    emit tabTitleChanged();
}

// ── 内部逻辑 ────────────────────────────────────────────

void SerialTabPage::onDataReceived(const QByteArray& data, const QString& port)
{
    Q_UNUSED(port);
    if (paused_) return;

    rxBytes_ += data.size();
    emit rxBytesChanged(rxBytes_);

    QByteArray combined = remainder_ + data;
    remainder_.clear();

    // 按行分割
    QStringList lines = LogParser::extractLines(combined, remainder_);

    for (const auto& line : lines) {
        QByteArray lineData = line.toUtf8();
        LogEntry entry = LogParser::parseLine(lineData, portName_);
        logBuffer_.append(entry);

        {
            QMutexLocker lock(&pendingMutex_);
            pendingEntries_.push_back(entry);
        }
    }

    if (!flushTimer_->isActive())
        flushTimer_->start();
}

void SerialTabPage::onEngineStatusChanged(const QString& port, bool connected,
                                          const SerialConfig& config)
{
    Q_UNUSED(port);
    Q_UNUSED(config);

    connected_ = connected;
    emit connectedChanged();
    emit statusChanged(connected);

    if (connected) {
        qint64 elapsed = connectionTimer_.elapsed();
        spdlog::info("SerialTabPage: {} connected in {}ms",
                     portName_.toStdString(), elapsed);
    } else {
        spdlog::info("SerialTabPage: {} disconnected", portName_.toStdString());
    }
}

void SerialTabPage::onDataSent(const QString& port, qint64 bytes)
{
    Q_UNUSED(port);
    Q_UNUSED(bytes);
}

void SerialTabPage::onFlushTimer()
{
    flushPending();
}

void SerialTabPage::flushPending()
{
    QVector<LogEntry> batch;
    {
        QMutexLocker lock(&pendingMutex_);
        if (pendingEntries_.empty()) return;
        batch.reserve(pendingEntries_.size());
        while (!pendingEntries_.empty()) {
            batch.append(pendingEntries_.front());
            pendingEntries_.pop_front();
        }
    }

    if (filter_.isEmpty()) {
        logModel_.appendBatch(batch);
    } else {
        // 带过滤: 只添加匹配的行
        QVector<LogEntry> filtered;
        for (const auto& e : batch) {
            if (e.text.contains(filter_, Qt::CaseInsensitive))
                filtered.append(e);
        }
        if (!filtered.isEmpty())
            logModel_.appendBatch(filtered);
    }
}

void SerialTabPage::rebuildModel()
{
    if (filter_.isEmpty()) {
        logModel_.rebuild(logBuffer_.getAll());
    } else {
        logModel_.rebuild(logBuffer_.getFiltered(filter_));
    }
}

QJsonArray SerialTabPage::getRecentLogs(int count) const
{
    QJsonArray arr;
    QVector<LogEntry> entries = logBuffer_.getRecent(count);
    for (const auto& e : entries) {
        QJsonObject obj;
        obj["timestamp"] = e.timestamp;
        obj["level"] = e.level;
        obj["text"] = e.text;
        obj["port"] = e.portName;
        arr.append(obj);
    }
    return arr;
}