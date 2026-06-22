/// Unix PTY 实现 (Linux / macOS / BSD)
/// 使用 POSIX openpty() / fork() / execvp()

#include "pty_process.h"

#ifndef Q_OS_WIN

#include <QSocketNotifier>
#include <QTimer>
#include <spdlog/spdlog.h>

#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstdio>
#include <cstring>

// ── PtyProcessUnix ───────────────────────────────────────

class PtyProcessUnix : public PtyProcess {
    Q_OBJECT
public:
    explicit PtyProcessUnix(QObject* parent = nullptr);
    ~PtyProcessUnix() override;

    bool start(const QString& program, const QStringList& args,
               const QString& cwd = {},
               const QProcessEnvironment& env = {}) override;
    qint64 write(const QByteArray& data) override;
    QByteArray readAll() override;
    void resize(int cols, int rows) override;
    void terminate() override;
    bool isRunning() const override;

private:
    void onMasterReadyRead(int fd);
    void checkExit();

    int         masterFd_;
    pid_t       childPid_;
    QByteArray  readBuffer_;
    bool        running_;
    int         exitCode_;

    QSocketNotifier* notifier_;
    QTimer*          exitTimer_;
};

// ── 工厂 ─────────────────────────────────────────────────

std::unique_ptr<PtyProcess> PtyProcess::create(QObject* parent)
{
    return std::unique_ptr<PtyProcess>(new PtyProcessUnix(parent));
}

// ── 构造/析构 ────────────────────────────────────────────

PtyProcessUnix::PtyProcessUnix(QObject* parent)
    : PtyProcess(parent)
    , masterFd_(-1)
    , childPid_(-1)
    , running_(false)
    , exitCode_(0)
    , notifier_(nullptr)
    , exitTimer_(nullptr)
{
}

PtyProcessUnix::~PtyProcessUnix()
{
    terminate();
}

// ── start ────────────────────────────────────────────────

bool PtyProcessUnix::start(const QString& program, const QStringList& args,
                            const QString& cwd, const QProcessEnvironment& env)
{
    if (running_) return false;

    int slaveFd;
    if (openpty(&masterFd_, &slaveFd, nullptr, nullptr, nullptr) < 0) {
        spdlog::error("PTY: openpty() failed, errno={}", errno);
        emit errorOccurred(PtyError, "openpty() failed");
        return false;
    }

    // 设置终端窗口大小
    struct winsize ws = {};
    ws.ws_col = 80;
    ws.ws_row = 24;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ioctl(masterFd_, TIOCSWINSZ, &ws);

    childPid_ = fork();
    if (childPid_ < 0) {
        spdlog::error("PTY: fork() failed, errno={}", errno);
        close(masterFd_);
        close(slaveFd);
        masterFd_ = -1;
        emit errorOccurred(LaunchFailed, "fork() failed");
        return false;
    }

    if (childPid_ == 0) {
        // ── 子进程 ─────────────────────────────────────
        close(masterFd_);

        // 创建新会话
        setsid();

        // 将 slave 设为控制终端
        ioctl(slaveFd, TIOCSCTTY, nullptr);

        // 复制 slave → stdin/stdout/stderr
        dup2(slaveFd, STDIN_FILENO);
        dup2(slaveFd, STDOUT_FILENO);
        dup2(slaveFd, STDERR_FILENO);
        if (slaveFd > STDERR_FILENO)
            close(slaveFd);

        // 切换工作目录
        if (!cwd.isEmpty())
            chdir(cwd.toLocal8Bit().constData());

        // 设置环境变量
        if (!env.isEmpty()) {
            const QStringList pairs = env.toStringList();
            for (const auto& p : pairs) {
                QByteArray ba = p.toLocal8Bit();
                // environ is accessible here; use putenv equivalent
                // putenv expects a mutable string
                char* envStr = strdup(ba.constData());
                putenv(envStr);
                // Note: intentional leak — putenv strings must persist
            }
        }

        // 构建 argv
        QByteArray progBytes = program.toLocal8Bit();
        std::vector<char*> argv;
        argv.push_back(progBytes.data());

        std::vector<QByteArray> argBytes;
        argBytes.reserve(args.size());
        for (const auto& a : args) {
            argBytes.push_back(a.toLocal8Bit());
            argv.push_back(argBytes.back().data());
        }
        argv.push_back(nullptr);

        execvp(progBytes.constData(), argv.data());

        // execvp 失败: 向 stderr (已重定向到 PTY) 报告错误后退出
        std::fprintf(stderr, "execvp failed: %s: %s\n",
                     progBytes.constData(), std::strerror(errno));
        std::fflush(stderr);
        _exit(127);
    }

    // ── 父进程 ─────────────────────────────────────────
    close(slaveFd);

    // 主 fd 设为非阻塞
    int flags = fcntl(masterFd_, F_GETFL);
    fcntl(masterFd_, F_SETFL, flags | O_NONBLOCK);

    running_ = true;
    exitCode_ = 0;

    // 使用 QSocketNotifier 监听可读事件
    notifier_ = new QSocketNotifier(masterFd_, QSocketNotifier::Read, this);
    connect(notifier_, &QSocketNotifier::activated,
            this, [this](int fd) { onMasterReadyRead(fd); });

    // 定时检查子进程退出
    exitTimer_ = new QTimer(this);
    connect(exitTimer_, &QTimer::timeout, this, &PtyProcessUnix::checkExit);
    exitTimer_->start(200);

    emit started();
    spdlog::info("PTY: started {} (pid={})", program.toStdString(), childPid_);
    return true;
}

// ── 读取 ─────────────────────────────────────────────────

void PtyProcessUnix::onMasterReadyRead(int /*fd*/)
{
    if (!running_) return;

    char buf[4096];
    ssize_t n = read(masterFd_, buf, sizeof(buf));
    if (n > 0) {
        readBuffer_.append(buf, static_cast<int>(n));
        emit readyRead();
    } else if (n == 0) {
        // EOF: 子进程关闭了 PTY slave 端
        // 必须处理，否则 QSocketNotifier 会持续触发导致 CPU 100%
        spdlog::info("PTY: EOF received, child process closed PTY");
        running_ = false;
        emit finished(exitCode_);
    } else if (n < 0 && errno != EAGAIN && errno != EINTR) {
        spdlog::error("PTY: read() failed, errno={}", errno);
    }
}

QByteArray PtyProcessUnix::readAll()
{
    QByteArray result = readBuffer_;
    readBuffer_.clear();
    return result;
}

// ── 写入 ─────────────────────────────────────────────────

qint64 PtyProcessUnix::write(const QByteArray& data)
{
    if (!running_ || masterFd_ < 0) return -1;

    ssize_t n = ::write(masterFd_, data.constData(),
                        static_cast<size_t>(data.size()));
    return (n >= 0) ? static_cast<qint64>(n) : -1;
}

// ── 调整大小 ─────────────────────────────────────────────

void PtyProcessUnix::resize(int cols, int rows)
{
    if (masterFd_ < 0) return;

    struct winsize ws = {};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    if (ioctl(masterFd_, TIOCSWINSZ, &ws) < 0) {
        spdlog::warn("PTY: TIOCSWINSZ failed, errno={}", errno);
    }

    // 发送 SIGWINCH 通知子进程
    if (childPid_ > 0)
        kill(childPid_, SIGWINCH);
}

// ── 退出检查 ─────────────────────────────────────────────

void PtyProcessUnix::checkExit()
{
    if (!running_ || childPid_ <= 0) return;

    int status;
    pid_t result = waitpid(childPid_, &status, WNOHANG);
    if (result == 0) return;  // 还在运行
    if (result < 0) {
        spdlog::error("PTY: waitpid() failed, errno={}", errno);
        return;
    }

    running_ = false;

    if (result > 0) {
        if (WIFEXITED(status))
            exitCode_ = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            exitCode_ = 128 + WTERMSIG(status);
        else
            exitCode_ = -1;
    } else {
        exitCode_ = -1;
    }

    // 停止监听
    if (notifier_) {
        notifier_->setEnabled(false);
        delete notifier_;
        notifier_ = nullptr;
    }
    if (exitTimer_) {
        exitTimer_->stop();
        delete exitTimer_;
        exitTimer_ = nullptr;
    }

    if (masterFd_ >= 0) {
        close(masterFd_);
        masterFd_ = -1;
    }

    childPid_ = -1;
    emit finished(exitCode_);
    spdlog::info("PTY: process exited, code={}", exitCode_);
}

// ── 终止 ─────────────────────────────────────────────────

void PtyProcessUnix::terminate()
{
    running_ = false;

    if (notifier_) {
        notifier_->setEnabled(false);
        delete notifier_;
        notifier_ = nullptr;
    }
    if (exitTimer_) {
        exitTimer_->stop();
        delete exitTimer_;
        exitTimer_ = nullptr;
    }

    if (childPid_ > 0) {
        // 先发 SIGHUP (挂断)
        kill(childPid_, SIGHUP);

        // 非阻塞轮询等待子进程退出, 避免永久阻塞主线程
        int status = 0;
        pid_t result = 0;
        bool killed = false;
        for (int i = 0; i < 20; ++i) {
            result = waitpid(childPid_, &status, WNOHANG);
            if (result != 0) break;
            usleep(100000);  // 100ms, 总计最多 2 秒
        }

        if (result == 0 && !killed) {
            // 强制 SIGKILL
            kill(childPid_, SIGKILL);
            killed = true;
            for (int i = 0; i < 10; ++i) {
                result = waitpid(childPid_, &status, WNOHANG);
                if (result != 0) break;
                usleep(100000);  // 100ms, 总计最多 1 秒
            }
        }

        if (result == 0) {
            spdlog::warn("PTY: failed to reap child process {} after SIGKILL", childPid_);
        } else if (result > 0) {
            if (WIFEXITED(status))
                exitCode_ = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                exitCode_ = 128 + WTERMSIG(status);
        }

        childPid_ = -1;
    }

    if (masterFd_ >= 0) {
        close(masterFd_);
        masterFd_ = -1;
    }

    emit finished(exitCode_);
    spdlog::info("PTY: terminated, exit code={}", exitCode_);
}

bool PtyProcessUnix::isRunning() const
{
    return running_;
}

#endif // !Q_OS_WIN

#include "pty_process_unix.moc"