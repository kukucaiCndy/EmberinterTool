#include "status_bar.h"
#include <QFrame>

StatusBar::StatusBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(28);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(16);

    portLabel_ = new QLabel("COM? | ?bps", this);
    layout->addWidget(portLabel_);

    QFrame* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    layout->addWidget(sep1);

    statusLabel_ = new QLabel("未连接", this);
    layout->addWidget(statusLabel_);

    QFrame* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::VLine);
    layout->addWidget(sep2);

    statsLabel_ = new QLabel("RX: 0B  TX: 0B", this);
    layout->addWidget(statsLabel_);

    QFrame* sep3 = new QFrame(this);
    sep3->setFrameShape(QFrame::VLine);
    layout->addWidget(sep3);

    ipcLabel_ = new QLabel("IPC: 0 客户端", this);
    layout->addWidget(ipcLabel_);

    layout->addStretch();
}

void StatusBar::setPortInfo(const QString& port, int baudrate)
{
    portLabel_->setText(QString("%1 | %2bps").arg(port).arg(baudrate));
}

void StatusBar::setConnectionStatus(bool connected)
{
    statusLabel_->setText(connected ? "已连接" : "未连接");
    statusLabel_->setStyleSheet(connected ?
        "color: #4CAF50; font-weight: bold;" : "color: #F44336;");
}

void StatusBar::setStats(qint64 rxBytes, qint64 txBytes, qint64 uptimeSeconds)
{
    auto fmtSize = [](qint64 bytes) -> QString {
        if (bytes < 1024) return QString("%1B").arg(bytes);
        if (bytes < 1024 * 1024) return QString("%1KB").arg(bytes / 1024.0, 0, 'f', 1);
        return QString("%1MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    };
    auto fmtTime = [](qint64 secs) -> QString {
        if (secs < 60) return QString("%1s").arg(secs);
        if (secs < 3600) return QString("%1m%2s").arg(secs / 60).arg(secs % 60);
        return QString("%1h%2m").arg(secs / 3600).arg((secs % 3600) / 60);
    };
    statsLabel_->setText(QString("RX: %1  TX: %2  运行: %3")
        .arg(fmtSize(rxBytes)).arg(fmtSize(txBytes)).arg(fmtTime(uptimeSeconds)));
}

void StatusBar::setIpcClientCount(int count)
{
    ipcLabel_->setText(QString("IPC: %1 客户端").arg(count));
}
