#ifndef SERIAL_ENGINE_H
#define SERIAL_ENGINE_H

#include <QObject>
#include <QSerialPort>
#include <QThread>
#include <QTimer>
#include <QByteArray>
#include <QString>
#include <atomic>

struct SerialConfig {
    QString port;
    qint32 baudrate = 115200;
    QSerialPort::DataBits databits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopbits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowcontrol = QSerialPort::NoFlowControl;
};

Q_DECLARE_METATYPE(SerialConfig)

class SerialEngine : public QObject {
    Q_OBJECT
public:
    explicit SerialEngine(QObject* parent = nullptr);
    ~SerialEngine();

    void open(const SerialConfig& config);
    void close();
    bool isOpen() const;

    qint64 sendText(const QString& data, const QString& append = "CRLF");
    qint64 sendHex(const QByteArray& data);
    qint64 sendRaw(const QByteArray& data);
    void setAutoReconnect(bool enabled);

    QString portName() const;
    SerialConfig config() const;

signals:
    void dataReceived(const QByteArray& data, const QString& port);
    void statusChanged(const QString& port, bool connected, const SerialConfig& config);
    void errorOccurred(const QString& port, const QString& error);
    void dataSent(const QString& port, qint64 bytes);

private:
    QThread* workerThread_ = nullptr;
    QObject* worker_ = nullptr;        // SerialWorker 实例（定义在 cpp 中），通过 invokeMethod 操作
    SerialConfig config_;
    bool autoReconnect_ = false;
    std::atomic<bool> isOpen_{false};

    QByteArray buildAppend(const QString& append) const;
};

#endif
