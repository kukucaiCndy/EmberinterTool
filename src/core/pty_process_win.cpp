/// Windows ConPTY 实现 - 异步 I/O 版本
/// 使用后台读取线程替代轮询, 显著降低 CPU 占用和延迟

#include "pty_process.h"

#ifdef Q_OS_WIN

#include <windows.h>
#include <QDir>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <spdlog/spdlog.h>

// ── ConPTY API 类型和函数声明 ──

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

typedef HRESULT (WINAPI *PFN_CreatePseudoConsole_T)(
    COORD size, HANDLE hInput, HANDLE hOutput,
    DWORD dwFlags, HPCON* phPC);

typedef HRESULT (WINAPI *PFN_ResizePseudoConsole_T)(HPCON hPC, COORD size);
typedef VOID    (WINAPI *PFN_ClosePseudoConsole_T)(HPCON hPC);

// ── 读取线程 ──

class PtyReadThread : public QThread {
    Q_OBJECT
public:
    PtyReadThread(HANDLE hOutRead, QObject* parent = nullptr)
        : QThread(parent), hOutRead_(hOutRead), stopping_(false)
    {}

    void stop() {
        stopping_ = true;
        // 取消阻塞的 ReadFile (关闭句柄)
        // 注意: 不在这里关闭, 由主线程关闭
        wait(3000);
    }

    QByteArray takeData() {
        QMutexLocker locker(&mutex_);
        QByteArray data;
        data.swap(pendingData_);
        return data;
    }

signals:
    void dataReady();

protected:
    void run() override {
        char buf[8192];
        while (!stopping_) {
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(hOutRead_, buf, sizeof(buf) - 1, &bytesRead, nullptr);
            if (!ok || bytesRead == 0) {
                // 管道关闭或错误
                if (!stopping_) {
                    spdlog::debug("ConPTY: read thread exiting (pipe closed or error={})", GetLastError());
                }
                break;
            }

            {
                QMutexLocker locker(&mutex_);
                pendingData_.append(buf, static_cast<int>(bytesRead));
            }
            emit dataReady();
        }
    }

private:
    HANDLE hOutRead_;
    std::atomic<bool> stopping_;
    QMutex mutex_;
    QByteArray pendingData_;
};

// ── PtyProcessWin ──

class PtyProcessWin : public PtyProcess {
    Q_OBJECT
public:
    explicit PtyProcessWin(QObject* parent = nullptr);
    ~PtyProcessWin() override;

    bool start(const QString& program, const QStringList& args,
               const QString& cwd = {},
               const QProcessEnvironment& env = {}) override;
    qint64 write(const QByteArray& data) override;
    QByteArray readAll() override;
    void resize(int cols, int rows) override;
    void terminate() override;
    bool isRunning() const override;

private:
    bool loadConPtyApi();
    bool createConPtyPipes(int cols, int rows);
    void cleanup();
    void checkExit();

    // ConPTY API 函数指针
    static PFN_CreatePseudoConsole_T  s_fnCreate_;
    static PFN_ResizePseudoConsole_T  s_fnResize_;
    static PFN_ClosePseudoConsole_T   s_fnClose_;
    static bool                       s_apiLoaded_;

    HPCON       hPC_;
    HANDLE      hInWrite_;      // 我们写入 → PTY stdin
    HANDLE      hOutRead_;      // PTY stdout → 读取线程
    HANDLE      hChildProcess_;
    HANDLE      hChildThread_;
    QByteArray  readBuffer_;
    bool        running_;
    int         exitCode_;

    // 异步读取线程
    PtyReadThread* readThread_ = nullptr;

    // 退出检查定时器
    QTimer*     exitTimer_ = nullptr;
};

PFN_CreatePseudoConsole_T  PtyProcessWin::s_fnCreate_ = nullptr;
PFN_ResizePseudoConsole_T  PtyProcessWin::s_fnResize_ = nullptr;
PFN_ClosePseudoConsole_T   PtyProcessWin::s_fnClose_  = nullptr;
bool                       PtyProcessWin::s_apiLoaded_ = false;

// ── 工厂方法 ──

PtyProcess* PtyProcess::create(QObject* parent)
{
    return new PtyProcessWin(parent);
}

// ── 构造/析构 ──

PtyProcessWin::PtyProcessWin(QObject* parent)
    : PtyProcess(parent)
    , hPC_(nullptr)
    , hInWrite_(INVALID_HANDLE_VALUE)
    , hOutRead_(INVALID_HANDLE_VALUE)
    , hChildProcess_(INVALID_HANDLE_VALUE)
    , hChildThread_(INVALID_HANDLE_VALUE)
    , running_(false)
    , exitCode_(0)
{
}

PtyProcessWin::~PtyProcessWin()
{
    cleanup();
}

// ── API 加载 ──

bool PtyProcessWin::loadConPtyApi()
{
    if (s_apiLoaded_)
        return (s_fnCreate_ && s_fnClose_);

    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (hKernel) {
        s_fnCreate_ = reinterpret_cast<PFN_CreatePseudoConsole_T>(
            GetProcAddress(hKernel, "CreatePseudoConsole"));
        s_fnResize_ = reinterpret_cast<PFN_ResizePseudoConsole_T>(
            GetProcAddress(hKernel, "ResizePseudoConsole"));
        s_fnClose_ = reinterpret_cast<PFN_ClosePseudoConsole_T>(
            GetProcAddress(hKernel, "ClosePseudoConsole"));
    }
    s_apiLoaded_ = true;

    if (!s_fnCreate_ || !s_fnClose_) {
        spdlog::error("ConPTY API not available (requires Windows 10 1809+)");
        return false;
    }
    spdlog::info("ConPTY API loaded successfully");
    return true;
}

// ── 管道创建 ──

bool PtyProcessWin::createConPtyPipes(int cols, int rows)
{
    HANDLE hInRead, hInWriteTmp;
    HANDLE hOutReadTmp, hOutWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    if (!CreatePipe(&hInRead, &hInWriteTmp, &sa, 0)) {
        spdlog::error("ConPTY: CreatePipe(in) failed, err={}", GetLastError());
        return false;
    }

    if (!CreatePipe(&hOutReadTmp, &hOutWrite, &sa, 0)) {
        spdlog::error("ConPTY: CreatePipe(out) failed, err={}", GetLastError());
        CloseHandle(hInRead);
        CloseHandle(hInWriteTmp);
        return false;
    }

    COORD size = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
    HRESULT hr = s_fnCreate_(size, hInRead, hOutWrite, 0, &hPC_);
    if (FAILED(hr)) {
        spdlog::error("ConPTY: CreatePseudoConsole failed, hr=0x{:X}",
                      static_cast<unsigned>(hr));
        CloseHandle(hInRead);
        CloseHandle(hInWriteTmp);
        CloseHandle(hOutReadTmp);
        CloseHandle(hOutWrite);
        return false;
    }

    hInWrite_  = hInWriteTmp;
    hOutRead_  = hOutReadTmp;
    return true;
}

// ── 清理 ──

void PtyProcessWin::cleanup()
{
    running_ = false;

    // 停止读取线程
    if (readThread_) {
        readThread_->stop();
        // 关闭读取端句柄以解除 ReadFile 阻塞
        if (hOutRead_ != INVALID_HANDLE_VALUE) {
            HANDLE h = hOutRead_;
            hOutRead_ = INVALID_HANDLE_VALUE;
            CloseHandle(h);
        }
        readThread_->wait(3000);
        delete readThread_;
        readThread_ = nullptr;
    }

    if (exitTimer_) {
        exitTimer_->stop();
        delete exitTimer_;
        exitTimer_ = nullptr;
    }

    if (hChildProcess_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(hChildProcess_, 1);
        CloseHandle(hChildProcess_);
        hChildProcess_ = INVALID_HANDLE_VALUE;
    }
    if (hChildThread_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hChildThread_);
        hChildThread_ = INVALID_HANDLE_VALUE;
    }

    if (hPC_ && s_fnClose_) {
        s_fnClose_(hPC_);
        hPC_ = nullptr;
    }

    if (hInWrite_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hInWrite_);
        hInWrite_ = INVALID_HANDLE_VALUE;
    }
    // hOutRead_ 已在读取线程停止时关闭
}

// ── start ──

bool PtyProcessWin::start(const QString& program, const QStringList& args,
                           const QString& cwd, const QProcessEnvironment& env)
{
    if (running_) return false;

    if (!loadConPtyApi()) {
        emit errorOccurred(PtyError, "ConPTY API not available");
        return false;
    }

    if (!createConPtyPipes(80, 24)) {
        emit errorOccurred(PtyError, "Failed to create ConPTY");
        return false;
    }

    // 构建命令行
    QString cmdLine = '"' + QDir::toNativeSeparators(program) + '"';
    for (const auto& arg : args) {
        cmdLine += ' ';
        if (arg.contains(' ') || arg.contains('"'))
            cmdLine += '"' + arg + '"';
        else
            cmdLine += arg;
    }

    // STARTUPINFOEX + 属性列表
    STARTUPINFOEXW siEx = {};
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    siEx.lpAttributeList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attrSize));

    if (!siEx.lpAttributeList || !InitializeProcThreadAttributeList(
            siEx.lpAttributeList, 1, 0, &attrSize)) {
        if (siEx.lpAttributeList)
            HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
        cleanup();
        emit errorOccurred(LaunchFailed, "Failed to init proc thread attributes");
        return false;
    }

    UpdateProcThreadAttribute(siEx.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC_, sizeof(HPCON),
        nullptr, nullptr);

    // 命令行缓冲区
    std::wstring cmdW = cmdLine.toStdWString();
    std::vector<wchar_t> cmdBuf(cmdW.begin(), cmdW.end());
    cmdBuf.push_back(L'\0');

    // 工作目录
    std::wstring cwdW;
    LPCWSTR cwdPtr = nullptr;
    if (!cwd.isEmpty()) {
        cwdW = cwd.toStdWString();
        cwdPtr = cwdW.c_str();
    }

    // 环境变量
    std::vector<wchar_t> envBlock;
    LPWSTR envPtr = nullptr;
    QProcessEnvironment useEnv = env.isEmpty()
        ? QProcessEnvironment::systemEnvironment() : env;
    if (!useEnv.isEmpty()) {
        const QStringList pairs = useEnv.toStringList();
        for (const auto& p : pairs) {
            auto ws = p.toStdWString();
            envBlock.insert(envBlock.end(), ws.begin(), ws.end());
            envBlock.push_back(L'\0');
        }
        envBlock.push_back(L'\0');
        envPtr = envBlock.data();
    }

    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessW(
        nullptr, cmdBuf.data(),
        nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        envPtr, cwdPtr,
        &siEx.StartupInfo, &pi);

    DeleteProcThreadAttributeList(siEx.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);

    if (!ok) {
        DWORD err = GetLastError();
        cleanup();
        spdlog::error("ConPTY: CreateProcessW failed, err={}", err);
        emit errorOccurred(LaunchFailed,
                           QString("CreateProcessW error %1").arg(err));
        return false;
    }

    hChildProcess_ = pi.hProcess;
    hChildThread_  = pi.hThread;
    running_ = true;
    exitCode_ = 0;

    // 启动异步读取线程
    readThread_ = new PtyReadThread(hOutRead_, this);
    connect(readThread_, &PtyReadThread::dataReady, this, [this]() {
        readBuffer_.append(readThread_->takeData());
        emit readyRead();
    }, Qt::QueuedConnection);
    readThread_->start();

    // 检查进程退出
    exitTimer_ = new QTimer(this);
    connect(exitTimer_, &QTimer::timeout, this, &PtyProcessWin::checkExit);
    exitTimer_->start(200);

    emit started();
    spdlog::info("ConPTY: started {}", program.toStdString());
    return true;
}

// ── 检查退出 ──

void PtyProcessWin::checkExit()
{
    if (!running_ || hChildProcess_ == INVALID_HANDLE_VALUE) return;

    DWORD code = STILL_ACTIVE;
    if (!GetExitCodeProcess(hChildProcess_, &code)) {
        running_ = false;
        exitCode_ = -1;
        emit finished(-1);
        return;
    }

    if (code != STILL_ACTIVE) {
        running_ = false;
        exitCode_ = static_cast<int>(code);

        // 排空残留数据
        if (readThread_) {
            readBuffer_.append(readThread_->takeData());
        }

        if (exitTimer_) exitTimer_->stop();

        emit finished(exitCode_);
        spdlog::info("ConPTY: process exited, code={}", exitCode_);
    }
}

// ── I/O ──

qint64 PtyProcessWin::write(const QByteArray& data)
{
    if (!running_ || hInWrite_ == INVALID_HANDLE_VALUE)
        return -1;

    DWORD written = 0;
    if (!WriteFile(hInWrite_, data.constData(),
                   static_cast<DWORD>(data.size()), &written, nullptr)) {
        return -1;
    }
    return static_cast<qint64>(written);
}

QByteArray PtyProcessWin::readAll()
{
    QByteArray result = readBuffer_;
    readBuffer_.clear();
    return result;
}

// ── 调整大小 ──

void PtyProcessWin::resize(int cols, int rows)
{
    if (hPC_ && s_fnResize_) {
        COORD size = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
        s_fnResize_(hPC_, size);
    }
}

// ── 终止 ──

void PtyProcessWin::terminate()
{
    if (hChildProcess_ != INVALID_HANDLE_VALUE) {
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        if (WaitForSingleObject(hChildProcess_, 2000) == WAIT_TIMEOUT) {
            TerminateProcess(hChildProcess_, 1);
        }
    }
    cleanup();
}

bool PtyProcessWin::isRunning() const
{
    return running_;
}

#endif // Q_OS_WIN

#include "pty_process_win.moc"
