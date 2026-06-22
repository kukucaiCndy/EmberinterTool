#include "log_buffer.h"

LogBuffer::LogBuffer(int maxSize)
    : maxSize_(maxSize)
{
}

void LogBuffer::append(const LogEntry& entry)
{
    QMutexLocker locker(&mutex_);
    buffer_.push_back(entry);
    while (static_cast<int>(buffer_.size()) > maxSize_) {
        buffer_.pop_front();
    }
}

void LogBuffer::clear()
{
    QMutexLocker locker(&mutex_);
    buffer_.clear();
}

QVector<LogEntry> LogBuffer::getAll() const
{
    QMutexLocker locker(&mutex_);
    QVector<LogEntry> result;
    result.reserve(static_cast<int>(buffer_.size()));
    for (const auto& entry : buffer_) {
        result.append(entry);
    }
    return result;
}

QVector<LogEntry> LogBuffer::getRecent(int count) const
{
    QMutexLocker locker(&mutex_);
    QVector<LogEntry> result;
    if (buffer_.empty() || count <= 0) {
        return result;
    }
    int actual = qMin(count, static_cast<int>(buffer_.size()));
    result.reserve(actual);
    auto it = buffer_.end() - actual;
    for (; it != buffer_.end(); ++it) {
        result.append(*it);
    }
    return result;
}

QVector<LogEntry> LogBuffer::getFiltered(const QString& keyword, int limit) const
{
    QMutexLocker locker(&mutex_);
    QVector<LogEntry> result;
    for (const auto& entry : buffer_) {
        if (entry.text.contains(keyword, Qt::CaseInsensitive)) {
            result.append(entry);
            if (limit > 0 && result.size() >= limit) {
                break;
            }
        }
    }
    return result;
}

int LogBuffer::size() const
{
    QMutexLocker locker(&mutex_);
    return static_cast<int>(buffer_.size());
}

void LogBuffer::setMaxSize(int size)
{
    QMutexLocker locker(&mutex_);
    maxSize_ = size;
    while (static_cast<int>(buffer_.size()) > maxSize_) {
        buffer_.pop_front();
    }
}

int LogBuffer::maxSize() const
{
    QMutexLocker locker(&mutex_);
    return maxSize_;
}
