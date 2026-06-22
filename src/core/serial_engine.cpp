#include "serial_engine.h"
#include <QDateTime>
#include <QMetaObject>
#include <spdlog/spdlog.h>

// ─────────────────────────────────────────────────────────────
// SerialWorker: 在 worker 线程中运行，持有 QSerialPort 和 QTimer
// 所有串口操作都在此对象所在线程执行，确保线程安全
// ─────────────────────────────────────────────────────────────
class SerialWorker : public QObject {
    Q_OBJECT
public:
    QSerialPort* serial_ = nullptr;
    QTimer* reconnectTimer_ = nullptr;
    SerialConfig config_;
    bool autoReconnect_ = true;
    int reconnectDelay_ = 1;
    std::atomic<bool>& isOpenFlag_;

    SerialWorker(std::atomic<bool>& isOpenFlag, QObject* parent = nullptr)
        : QObject(parent)
        , isOpenFlag_(isOpenFlag)
    {
    }

public slots:
    void init()
    {
        serial_ = new QSerialPort(this);
        reconnectTimer_ = new QTimer(this);
        reconnectTimer_->setSingleShot(true);

        connect(serial_, &QSerialPort::readyRead, this, &SerialWorker::onReadyRead);
        connect(serial_, &QSerialPort::errorOccurred, this, &SerialWorker::onError);
        connect(reconnectTimer_, &QTimer::timeout, this, &SerialWorker::tryReconnect);
    }

    void doOpen(const SerialConfig& config)
    {
        config_ = config;
        applyConfig();

        if (serial_->open(QIODevice::ReadWrite)) {
            isOpenFlag_ = true;
            spdlog::info("Serial port {} opened @ {}bps",
                         config_.port.toStdString(), config_.baudrate);
            reconnectDelay_ = 1;
            emit statusChanged(config_.port, true, config_);
        } else {
            spdlog::warn("Failed to open serial port {}: {}",
                         config_.port.toStdString(),
                         serial_->errorString().toStdString());
            emit errorOccurred(config_.port, serial_->errorString());
            if (autoReconnect_) {
                reconnectTimer_->start(reconnectDelay_ * 1000);
            }
        }
    }

    void doClose()
    {
        if (reconnectTimer_) {
            reconnectTimer_->stop();
        }
        if (serial_ && serial_->isOpen()) {
            serial_->close();
        }
        isOpenFlag_ = false;
    }

    qint64 doWrite(const QByteArray& data)
    {
        if (!serial_ || !serial_->isOpen()) {
            return -1;
        }
        qint64 written = serial_->write(data);
        if (written > 0) {
            emit dataSent(config_.port, written);
        }
        return written;
    }

    void setAutoReconnect(bool enabled)
    {
        autoReconnect_ = enabled;
    }

    void onReadyRead()
    {
        QByteArray data = serial_->readAll();
        if (!data.isEmpty()) {
            emit dataReceived(data, config_.port);
        }
    }

    void onError(QSerialPort::SerialPortError error)
    {
        if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError) {
            return;
        }

        spdlog::warn("Serial port {} error: {}",
                     config_.port.toStdString(),
                     serial_->errorString().toStdString());

        if (serial_->isOpen()) {
            serial_->close();
        }
        isOpenFlag_ = false;

        emit errorOccurred(config_.port, serial_->errorString());
        emit statusChanged(config_.port, false, config_);

        if (autoReconnect_) {
            reconnectTimer_->start(reconnectDelay_ * 1000);
        }
    }

    void tryReconnect()
    {
        if (serial_->isOpen()) {
            return;
        }

        applyConfig();

        if (serial_->open(QIODevice::ReadWrite)) {
            isOpenFlag_ = true;
            spdlog::info("Serial port {} reconnected", config_.port.toStdString());
            reconnectDelay_ = 1;
            emit statusChanged(config_.port, true, config_);
        } else {
            spdlog::warn("Reconnect to {} failed, next attempt in {}s",
                         config_.port.toStdString(), reconnectDelay_);
            reconnectDelay_ = qMin(reconnectDelay_ + 1, 5);
            reconnectTimer_->start(reconnectDelay_ * 1000);
        }
    }

private:
    void applyConfig()
    {
        serial_->setPortName(config_.port);
        serial_->setBaudRate(config_.baudrate);
        serial_->setDataBits(config_.databits);
        serial_->setParity(config_.parity);
        serial_->setStopBits(config_.stopbits);
        serial_->setFlowControl(config_.flowcontrol);
    }

signals:
    void dataReceived(const QByteArray& data, const QString& port);
    void statusChanged(const QString& port, bool connected, const SerialConfig& config);
    void errorOccurred(const QString& port, const QString& error);
    void dataSent(const QString& port, qint64 bytes);
};

// ─────────────────────────────────────────────────────────────
// SerialEngine: 主线程接口，通过 invokeMethod 转发到 worker 线程
// ─────────────────────────────────────────────────────────────

SerialEngine::SerialEngine(QObject* parent)
    : QObject(parent)
    , workerThread_(nullptr)
    , worker_(nullptr)
    , autoReconnect_(true)
    , isOpen_(false)
{
    // 注册 metatype 以支持队列连接
    qRegisterMetaType<SerialConfig>("SerialConfig");
}

SerialEngine::~SerialEngine()
{
    close();
}

void SerialEngine::open(const SerialConfig& config)
{
    close();
    config_ = config;

    workerThread_ = new QThread(this);
    auto* worker = new SerialWorker(isOpen_);
    worker->autoReconnect_ = autoReconnect_;
    worker->moveToThread(workerThread_);
    worker_ = worker;

    // 转发 worker 信号到 SerialEngine（自动 QueuedConnection，跨线程）
    // worker_ 声明为 QObject* (因为 SerialWorker 定义在 cpp 中), 需要转换以使用信号槽
    auto* sw = static_cast<SerialWorker*>(worker_);
    connect(sw, &SerialWorker::dataReceived,
            this, &SerialEngine::dataReceived);
    connect(sw, &SerialWorker::statusChanged,
            this, &SerialEngine::statusChanged);
    connect(sw, &SerialWorker::errorOccurred,
            this, &SerialEngine::errorOccurred);
    connect(sw, &SerialWorker::dataSent,
            this, &SerialEngine::dataSent);

    // 线程结束时自动清理 worker
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);

    workerThread_->start();

    // 在 worker 线程初始化并打开串口
    QMetaObject::invokeMethod(worker_, "init", Qt::QueuedConnection);
    QMetaObject::invokeMethod(worker_, "doOpen", Qt::QueuedConnection,
                              Q_ARG(SerialConfig, config));
}

void SerialEngine::close()
{
    if (worker_) {
        // 在 worker 线程关闭串口（同步等待，确保关闭完成）
        QMetaObject::invokeMethod(worker_, "doClose", Qt::BlockingQueuedConnection);
    }
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(3000);
        delete workerThread_;
        workerThread_ = nullptr;
        worker_ = nullptr;  // worker_ 由 deleteLater 在线程结束后释放
    }
    isOpen_ = false;

    if (!config_.port.isEmpty()) {
        emit statusChanged(config_.port, false, config_);
    }
}

bool SerialEngine::isOpen() const
{
    return isOpen_.load();
}

qint64 SerialEngine::sendText(const QString& data, const QString& append)
{
    if (!worker_ || !isOpen()) {
        return -1;
    }

    QByteArray payload = data.toUtf8();
    payload.append(buildAppend(append));

    qint64 result = -1;
    // BlockingQueuedConnection 确保在 worker 线程执行并获取返回值
    // serial->write() 只写入内部缓冲区，非常快，不会长时间阻塞主线程
    QMetaObject::invokeMethod(worker_, "doWrite", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(qint64, result),
                              Q_ARG(QByteArray, payload));
    return result;
}

qint64 SerialEngine::sendHex(const QByteArray& data)
{
    if (!worker_ || !isOpen()) {
        return -1;
    }

    qint64 result = -1;
    QMetaObject::invokeMethod(worker_, "doWrite", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(qint64, result),
                              Q_ARG(QByteArray, data));
    return result;
}

qint64 SerialEngine::sendRaw(const QByteArray& data)
{
    if (!worker_ || !isOpen()) {
        return -1;
    }

    qint64 result = -1;
    QMetaObject::invokeMethod(worker_, "doWrite", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(qint64, result),
                              Q_ARG(QByteArray, data));
    return result;
}

void SerialEngine::setAutoReconnect(bool enabled)
{
    autoReconnect_ = enabled;
    if (worker_) {
        QMetaObject::invokeMethod(worker_, "setAutoReconnect",
                                  Qt::QueuedConnection, Q_ARG(bool, enabled));
    }
}

QString SerialEngine::portName() const
{
    return config_.port;
}

SerialConfig SerialEngine::config() const
{
    return config_;
}

QByteArray SerialEngine::buildAppend(const QString& append) const
{
    // 行尾追加模式: CRLF (默认), CR, LF, NONE
    if (append == QLatin1String("CR"))       return QByteArray("\r");
    if (append == QLatin1String("LF"))       return QByteArray("\n");
    if (append == QLatin1String("NONE"))     return QByteArray();
    if (append == QLatin1String("CRLF"))     return QByteArray("\r\n");
    // 未知模式回退到 CRLF
    return QByteArray("\r\n");
}

// SerialWorker 定义在 .cpp 中，需要包含 moc 生成文件
#include "serial_engine.moc"
