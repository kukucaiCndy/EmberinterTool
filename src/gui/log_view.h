#ifndef LOG_VIEW_H
#define LOG_VIEW_H

#include <QAbstractScrollArea>
#include <QVector>
#include <QScrollBar>
#include "log_buffer.h"

class LogView : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit LogView(QWidget* parent = nullptr);

    void appendEntry(const LogEntry& entry);
    void setEntries(const QVector<LogEntry>& entries);
    void clear();
    void setHexMode(bool enabled);
    void setShowTimestamp(bool enabled);
    void setAutoScroll(bool enabled);
    void setFilter(const QString& keyword);

    bool hexMode() const;
    bool showTimestamp() const;
    bool autoScroll() const;
    QString filter() const;

public slots:
    void scrollToBottom();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QVector<LogEntry> entries_;
    bool hexMode_;
    bool showTimestamp_;
    bool autoScroll_;
    QString filter_;

    int lineHeight() const;
    int visibleStartIndex() const;
    int visibleEndIndex() const;
    void updateScrollBar();
    void drawLine(QPainter& painter, int index, int y);
};

#endif
