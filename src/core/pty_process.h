#ifndef PTY_PROCESS_H
#define PTY_PROCESS_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QProcessEnvironment>

/// 跨平台 PTY 进程抽象基类
/// 统一 Windows ConPTY 和 Unix PTY 的接口
class PtyProcess : public QObject {
    Q_OBJECT

public:
    enum Error { NoError, LaunchFailed, PtyError, NotSupported };
    Q_ENUM(Error)

    /// 工厂方法：根据平台创建具体实现
    static PtyProcess* create(QObject* parent = nullptr);

    explicit PtyProcess(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~PtyProcess() = default;

    /// 启动进程并附加 PTY
    /// @param program  可执行文件路径
    /// @param args     命令行参数
    /// @param cwd      工作目录（空 = 继承当前）
    /// @param env      环境变量（空 = 继承当前）
    /// @return true 启动成功
    virtual bool start(const QString& program,
                       const QStringList& args = {},
                       const QString& cwd = {},
                       const QProcessEnvironment& env = {}) = 0;

    /// 写入数据到进程标准输入
    virtual qint64 write(const QByteArray& data) = 0;

    /// 读取进程标准输出
    virtual QByteArray readAll() = 0;

    /// 调整 PTY 窗口大小
    virtual void resize(int cols, int rows) = 0;

    /// 终止进程
    virtual void terminate() = 0;

    /// 进程是否正在运行
    virtual bool isRunning() const = 0;

signals:
    /// 进程已启动
    void started();
    /// 有数据可读
    void readyRead();
    /// 进程已退出
    void finished(int exitCode);
    /// 发生错误
    void errorOccurred(Error error, const QString& message);
};

#endif // PTY_PROCESS_H