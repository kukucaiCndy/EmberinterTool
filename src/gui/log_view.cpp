#include "log_view.h"
#include "log_parser.h"
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QFontMetrics>
#include <cmath>

LogView::LogView(QWidget* parent)
    : QAbstractScrollArea(parent)
    , hexMode_(false)
    , showTimestamp_(true)
    , autoScroll_(true)
{
    verticalScrollBar()->setSingleStep(20);
    setFont(QFont("Consolas", 11));
    setMinimumHeight(200);
    viewport()->setAutoFillBackground(true);
    viewport()->setPalette(QPalette(Qt::white));
}

void LogView::appendEntry(const LogEntry& entry)
{
    if (!filter_.isEmpty() && !entry.text.contains(filter_, Qt::CaseInsensitive)) {
        return;
    }
    entries_.append(entry);
    updateScrollBar();
    if (autoScroll_) {
        scrollToBottom();
    }
    viewport()->update();
}

void LogView::setEntries(const QVector<LogEntry>& entries)
{
    entries_ = entries;
    updateScrollBar();
    viewport()->update();
}

void LogView::clear()
{
    entries_.clear();
    updateScrollBar();
    viewport()->update();
}

void LogView::setHexMode(bool enabled)
{
    hexMode_ = enabled;
    viewport()->update();
}

void LogView::setShowTimestamp(bool enabled)
{
    showTimestamp_ = enabled;
    viewport()->update();
}

void LogView::setAutoScroll(bool enabled)
{
    autoScroll_ = enabled;
    if (enabled) {
        scrollToBottom();
    }
}

void LogView::setFilter(const QString& keyword)
{
    filter_ = keyword;
    viewport()->update();
}

bool LogView::hexMode() const { return hexMode_; }
bool LogView::showTimestamp() const { return showTimestamp_; }
bool LogView::autoScroll() const { return autoScroll_; }
QString LogView::filter() const { return filter_; }

void LogView::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void LogView::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.setFont(font());
    painter.fillRect(event->rect(), viewport()->palette().base());

    int start = visibleStartIndex();
    int end = visibleEndIndex();
    int lh = lineHeight();
    int y = start * lh;

    for (int i = start; i < end && i < entries_.size(); ++i) {
        drawLine(painter, i, y);
        y += lh;
    }
}

void LogView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBar();
}

int LogView::lineHeight() const
{
    return QFontMetrics(font()).height() + 2;
}

int LogView::visibleStartIndex() const
{
    return verticalScrollBar()->value() / lineHeight();
}

int LogView::visibleEndIndex() const
{
    int lh = lineHeight();
    int visible = (viewport()->height() / lh) + 2;
    return qMin(visibleStartIndex() + visible, entries_.size());
}

void LogView::updateScrollBar()
{
    int lh = lineHeight();
    int totalLines = entries_.size() + (viewport()->height() / lh);
    verticalScrollBar()->setRange(0, qMax(0, entries_.size() * lh - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
}

void LogView::drawLine(QPainter& painter, int index, int y)
{
    const LogEntry& entry = entries_[index];
    int lh = lineHeight();
    QColor color = LogParser::levelColor(entry.level);

    painter.setPen(color);

    if (hexMode_ && !entry.rawBytes.isEmpty()) {
        QString hexText = LogParser::formatHex(entry.rawBytes, 0);
        QStringList hexLines = hexText.split('\n');
        for (int h = 0; h < hexLines.size(); ++h) {
            painter.setPen(color);
            painter.drawText(0, y, viewport()->width(), lh, Qt::AlignLeft | Qt::AlignTop, hexLines[h]);
            y += lh;
        }
    } else {
        QString display = LogParser::formatDisplay(entry, showTimestamp_);
        painter.drawText(4, y, viewport()->width() - 8, lh, Qt::AlignLeft | Qt::AlignTop, display);
    }
}
