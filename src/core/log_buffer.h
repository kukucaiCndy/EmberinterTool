#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QMutex>
#include <deque>

struct LogEntry {
    QString timestamp;
    QString level;
    QString text;
    QByteArray rawBytes;
    QString portName;

    LogEntry() = default;
    LogEntry(const QString& ts, const QString& lvl, const QString& txt,
             const QByteArray& raw, const QString& port)
        : timestamp(ts), level(lvl), text(txt), rawBytes(raw), portName(port) {}
};

class LogBuffer {
public:
    explicit LogBuffer(int maxSize = 10000);

    void append(const LogEntry& entry);
    void clear();
    QVector<LogEntry> getAll() const;
    QVector<LogEntry> getRecent(int count) const;
    QVector<LogEntry> getFiltered(const QString& keyword, int limit = -1) const;
    int size() const;
    void setMaxSize(int size);
    int maxSize() const;

private:
    std::deque<LogEntry> buffer_;
    mutable QMutex mutex_;
    int maxSize_;
};

#endif
