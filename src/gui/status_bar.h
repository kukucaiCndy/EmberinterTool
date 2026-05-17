#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>

class StatusBar : public QWidget {
    Q_OBJECT
public:
    explicit StatusBar(QWidget* parent = nullptr);

    void setPortInfo(const QString& port, int baudrate);
    void setConnectionStatus(bool connected);
    void setStats(qint64 rxBytes, qint64 txBytes, qint64 uptimeSeconds);
    void setIpcClientCount(int count);

private:
    QLabel* portLabel_;
    QLabel* statusLabel_;
    QLabel* statsLabel_;
    QLabel* ipcLabel_;
};

#endif
