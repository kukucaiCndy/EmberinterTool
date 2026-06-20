#ifndef QML_SERIAL_TAB_PAGE_H
#define QML_SERIAL_TAB_PAGE_H

#include "tab_page.h"
#include "serial_engine.h"
#include "log_buffer.h"
#include <QAbstractListModel>
#include <QTimer>
#include <QMutex>
#include <QElapsedTimer>
#include <deque>

// ── 日志列表模型 (供 QML ListView 使用) ─────────────────
class LogListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        DisplayRole = Qt::UserRole + 1,
        TimestampRole,
        LevelRole,
        ColorRole,
        RawHexRole
    };

    explicit LogListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendBatch(const QVector<LogEntry>& entries);
    void rebuild(const QVector<LogEntry>& entries);
    void clear();

    int maxEntries() const { return maxEntries_; }
    void setMaxEntries(int n);

private:
    QVector<LogEntry> entries_;
    int maxEntries_;
};

// ── 串口 Tab 页 (纯逻辑, 无 QWidget) ────────────────────
class SerialTabPage : public TabPage {
    Q_OBJECT

    Q_PROPERTY(QObject* logModel READ logModel CONSTANT)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool hexMode READ hexMode NOTIFY hexModeChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(bool showTimestamp READ showTimestamp NOTIFY showTimestampChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll NOTIFY autoScrollChanged)
    Q_PROPERTY(QString filter READ filter NOTIFY filterChanged)
    Q_PROPERTY(QString portName READ portName NOTIFY portNameChanged)

public:
    explicit SerialTabPage(QObject* parent = nullptr);
    ~SerialTabPage();

    TabType tabType() const override { return TabType::Serial; }
    QString tabTitle() const override;
    bool isConnected() const override { return connected_; }
    qint64 rxBytes() const override { return rxBytes_; }
    qint64 txBytes() const override { return txBytes_; }

    void connectTo(const QJsonObject& params) override;
    void disconnect() override;
    void clearContent() override;
    void exportContent(const QString& path) override;

    // QML 可访问属性
    QObject* logModel() { return &logModel_; }
    bool connected() const { return connected_; }
    bool hexMode() const { return hexMode_; }
    bool paused() const { return paused_; }
    bool showTimestamp() const { return showTimestamp_; }
    bool autoScroll() const { return autoScroll_; }
    QString filter() const { return filter_; }
    QString portName() const { return portName_; }

    // QML 可调用方法
    Q_INVOKABLE void sendText(const QString& text, const QString& append);
    Q_INVOKABLE void sendHex(const QString& hex);
    Q_INVOKABLE void setFilter(const QString& keyword);
    Q_INVOKABLE void setHexMode(bool enabled);
    Q_INVOKABLE void setPaused(bool p);
    Q_INVOKABLE void setShowTimestamp(bool show);
    Q_INVOKABLE void setAutoScroll(bool enabled);
    Q_INVOKABLE void clearLogs();
    Q_INVOKABLE void exportLogs(const QString& path);

    // 内部接口
    void setPortName(const QString& name);
    QJsonArray getRecentLogs(int count) const;

signals:
    void connectedChanged();
    void hexModeChanged();
    void pausedChanged();
    void showTimestampChanged();
    void autoScrollChanged();
    void filterChanged();
    void portNameChanged();

private slots:
    void onDataReceived(const QByteArray& data, const QString& port);
    void onEngineStatusChanged(const QString& port, bool connected,
                               const SerialConfig& config);
    void onDataSent(const QString& port, qint64 bytes);
    void onFlushTimer();

private:
    void processData(const QByteArray& data);
    void flushPending();
    void rebuildModel();

    SerialEngine engine_;
    LogBuffer logBuffer_;
    LogListModel logModel_;
    QTimer* flushTimer_;

    QString portName_;
    bool connected_;
    bool hexMode_;
    bool paused_;
    bool showTimestamp_;
    bool autoScroll_;
    QString filter_;
    qint64 rxBytes_;
    qint64 txBytes_;
    QByteArray remainder_;

    // 批量处理: 待刷新的条目
    mutable QMutex pendingMutex_;
    std::deque<LogEntry> pendingEntries_;
    QElapsedTimer connectionTimer_;
};

#endif