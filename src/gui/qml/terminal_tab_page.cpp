#include "terminal_tab_page.h"
#include "terminal/terminal_view.h"
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QProcessEnvironment>
#include <spdlog/spdlog.h>

TerminalTabPage::TerminalTabPage(TabType type, QObject* parent)
    : TabPage(parent)
    , type_(type)
    , view_(nullptr)
    , connected_(false)
{
    spdlog::debug("TerminalTabPage::ctor: type={}, view_=nullptr, this={}",
                  static_cast<int>(type_), (void*)this);
}

TerminalTabPage::~TerminalTabPage()
{
    // 析构时直接终止，不发信号
    if (view_) {
        view_->terminate();
    }
}

QString TerminalTabPage::tabTitle() const
{
    if (!customTitle_.isEmpty()) return customTitle_;
    switch (type_) {
    case TabType::CMD:
        return QString::fromUtf8("[终端] %1").arg(shellName_);
    case TabType::SSH:
        return QString::fromUtf8("[SSH] %1").arg(shellName_);
    default:
        return "Terminal";
    }
}

void TerminalTabPage::attachView(TerminalView* view)
{
    spdlog::debug("TerminalTabPage::attachView: view={}, this={}", (void*)view, (void*)this);

    // 防止重复连接：如果同一个 view 已经附加，直接返回
    if (view_ == view) {
        spdlog::debug("TerminalTabPage::attachView: same view already attached, skipping");
        return;
    }

    view_ = view;
    if (!view_) {
        spdlog::warn("TerminalTabPage::attachView: view is null, aborting");
        return;
    }

    // 连接 View 信号（view_ 是 QPointer，QML 销毁 view 时会自动置 null）
    connect(view_, &TerminalView::shellStarted, this, [this]() {
        spdlog::debug("TerminalTabPage: shellStarted signal received, this={}", (void*)this);
        connected_ = true;
        emit connectedChanged();
        emit statusChanged(true);
    });

    connect(view_, &TerminalView::shellExited, this, [this](int exitCode) {
        spdlog::debug("TerminalTabPage: shellExited signal received, code={}, this={}", exitCode, (void*)this);
        connected_ = false;
        emit connectedChanged();
        emit statusChanged(false);
        emit tabTitleChanged();
    });

    // 转发终端输出数据 (PTY → TerminalModel → 这里 → IPC 订阅客户端)
    // 注意: dataReceived 是 TerminalModel 的信号, 通过 view_->model() 访问
    connect(&view_->model(), &TerminalModel::dataReceived,
            this, &TerminalTabPage::dataReceived);

    // 如果有暂存的连接参数，自动连接
    if (!pendingParams_.isEmpty()) {
        spdlog::debug("TerminalTabPage::attachView: consuming pendingParams, this={}", (void*)this);
        QJsonObject params = pendingParams_;
        pendingParams_ = {};
        doConnect(params);
    } else {
        spdlog::debug("TerminalTabPage::attachView: no pendingParams, this={}", (void*)this);
    }
}

void TerminalTabPage::connectTo(const QJsonObject& params)
{
    spdlog::debug("TerminalTabPage::connectTo: view_={}, this={}", (void*)view_, (void*)this);
    if (!view_) {
        // View 尚未就绪，暂存参数等待 attachView 后自动连接
        pendingParams_ = params;
        spdlog::info("TerminalTabPage: view not ready, deferring connect, pendingParams saved");
        return;
    }

    doConnect(params);
}

void TerminalTabPage::doConnect(const QJsonObject& params)
{
    spdlog::debug("TerminalTabPage::doConnect: type={}, this={}", static_cast<int>(type_), (void*)this);
    switch (type_) {
    case TabType::CMD: {
        QString shell = params["shell"].toString();

        // 平台默认 shell
        if (shell.isEmpty()) {
#ifdef Q_OS_WIN
            shell = "cmd.exe";
#else
            // macOS/Linux: 使用用户登录 shell
            shell = QProcessEnvironment::systemEnvironment().value("SHELL", "/bin/bash");
#endif
        }

        QStringList args;
#ifdef Q_OS_WIN
        bool isMsysBash = false;
        if (shell == "powershell.exe" || shell == "pwsh.exe") {
            args << "-NoLogo" << "-NoExit";
        } else if (shell.endsWith("bash.exe", Qt::CaseInsensitive) ||
                   shell.endsWith("bash", Qt::CaseInsensitive) ||
                   shell.endsWith("sh.exe", Qt::CaseInsensitive) ||
                   shell.endsWith("sh", Qt::CaseInsensitive) ||
                   shell.endsWith("zsh.exe", Qt::CaseInsensitive) ||
                   shell.endsWith("zsh", Qt::CaseInsensitive)) {
            // Git Bash / MSYS2 bash 在 ConPTY 下需要显式进入交互式模式，
            // 否则无控制终端时会立刻退出。
            args << "--login" << "-i";
            isMsysBash = true;

            // 关键修复: 当 shell 是 "bash" (无完整路径) 时, 系统 PATH 里
            // Git Bash (C:/Program Files/Git/bin/bash) 可能优先于 MSYS2,
            // 导致启动的是 Git Bash 而非 MSYS2 bash, /mingw64 挂载点缺失,
            // mingw32-make 等工具找不到。
            // 解决: 探测常见的 MSYS2 安装路径, 优先使用 MSYS2 的 bash。
            if (QFileInfo(shell).isRelative() &&
                (shell == "bash" || shell == "bash.exe")) {
                // 候选 MSYS2 bash 路径 (按优先级排序)
                static const QStringList msysBashCandidates = {
                    "C:/msys64/usr/bin/bash.exe",
                    "C:/msys2/usr/bin/bash.exe",
                    "D:/msys64/usr/bin/bash.exe",
                    "D:/msys2/usr/bin/bash.exe",
                };
                for (const auto& candidate : msysBashCandidates) {
                    if (QFile::exists(candidate)) {
                        spdlog::info("TerminalTabPage: resolved 'bash' to MSYS2: {}",
                                     candidate.toStdString());
                        shell = candidate;
                        break;
                    }
                }
            }
        }
#else
        // macOS/Linux: shell 交互模式
        if (shell.endsWith("bash") || shell.endsWith("zsh") || shell.endsWith("sh")) {
            args << "--login" << "-i";
        }
#endif

        shellName_ = shell;
        emit shellNameChanged();
        emit tabTitleChanged();

        spdlog::debug("TerminalTabPage::doConnect: calling view_->startShell({}, {}), view_={}",
                      shell.toStdString(), args.size(), (void*)view_);

#ifdef Q_OS_WIN
        if (isMsysBash) {
            // MSYS2/Git Bash 需要正确的 TERM 和 MSYSTEM 环境变量才能保持交互
            // 关键: 必须设置 PATH 包含 MSYS2 的 bin 目录, 否则 bash --login
            // 无法找到 /etc/profile 依赖的基础工具 (printenv/cygpath 等),
            // 导致 PATH 不会被正确初始化, mingw32-make 等命令找不到。
            //
            // 探测 MSYS2 安装根目录 (bash 通常位于 <msys_root>/usr/bin/bash.exe)
            QString msysRoot;
            QString bashDir = QFileInfo(shell).absolutePath();
            if (bashDir.endsWith("/usr/bin", Qt::CaseInsensitive) ||
                bashDir.endsWith("\\usr\\bin", Qt::CaseInsensitive)) {
                msysRoot = QDir::cleanPath(bashDir + "/../..");
            }
            // 若 bash 来自 mingw64/bin (少见), 取上两级
            if (msysRoot.isEmpty() &&
                (bashDir.endsWith("/mingw64/bin", Qt::CaseInsensitive) ||
                 bashDir.endsWith("\\mingw64\\bin", Qt::CaseInsensitive))) {
                msysRoot = QDir::cleanPath(bashDir + "/../..");
            }

            QVariantMap env;
            env["TERM"] = "xterm-256color";
            env["MSYSTEM"] = "MINGW64";
            env["MINGW_PREFIX"] = "/mingw64";
            env["CHERE_INVOKING"] = "1";
            env["MSYS2_PATH_TYPE"] = "inherit";
            // 预设 MSYS2 标准路径 (Windows 格式, 供 bash 启动阶段使用)
            // bash -- login 读取 /etc/profile 后会用 MSYS2 格式路径覆盖
            if (!msysRoot.isEmpty()) {
                QString msysRootWin = QDir::toNativeSeparators(msysRoot);
                env["PATH"] = msysRootWin + "\\mingw64\\bin;" +
                              msysRootWin + "\\usr\\local\\bin;" +
                              msysRootWin + "\\usr\\bin;" +
                              msysRootWin + "\\bin;" +
                              QProcessEnvironment::systemEnvironment().value("PATH");
                spdlog::info("TerminalTabPage: MSYS2 root={}, PATH prefix set",
                             msysRoot.toStdString());
            }
            view_->startShellWithEnv(shell, args, env);
        } else {
            view_->startShell(shell, args);
        }
#else
        view_->startShell(shell, args);
#endif
        view_->setShellName(shell);
        spdlog::info("TerminalTabPage: starting local shell {}", shell.toStdString());
        break;
    }
    case TabType::SSH: {
        QString host   = params["host"].toString();
        int port       = params["port"].toInt(22);
        QString user   = params["user"].toString();

        shellName_ = QString("%1@%2").arg(user, host);
        emit shellNameChanged();
        emit tabTitleChanged();

        QStringList args;
        args << "-p" << QString::number(port);
        if (!user.isEmpty())
            args << QString("%1@%2").arg(user, host);
        else
            args << host;

        args << "-o" << "StrictHostKeyChecking=accept-new";
#ifdef Q_OS_WIN
        // Windows 没有 /dev/null，使用 NUL 设备
        args << "-o" << "UserKnownHostsFile=NUL";
#else
        args << "-o" << "UserKnownHostsFile=/dev/null";
#endif

        spdlog::debug("TerminalTabPage::doConnect: calling view_->startShell(ssh, {}), view_={}",
                      args.size(), (void*)view_);
        view_->startShell("ssh", args);
        view_->setShellName(QString("%1@%2").arg(user, host));
        spdlog::info("TerminalTabPage: starting SSH {}@{}:{}",
                     user.toStdString(), host.toStdString(), port);
        break;
    }
    default:
        break;
    }
}

void TerminalTabPage::closeConnection()
{
    spdlog::debug("TerminalTabPage::closeConnection: view_={}, this={}", (void*)view_.data(), (void*)this);
    if (view_) {
        // 先断开 view 的所有信号, 防止 terminate() 触发 shellExited
        // 回调访问正在被销毁的 this
        disconnect(view_, nullptr, this, nullptr);
        view_->terminate();
    }
    connected_ = false;
    emit connectedChanged();
    emit statusChanged(false);
}

void TerminalTabPage::writeInput(const QString& text)
{
    spdlog::debug("TerminalTabPage::writeInput: text='{}', view_={}, connected={}",
                  text.toStdString(), (void*)view_.data(), connected_);
    if (view_)
        view_->writeInput(text.toUtf8());
    else
        spdlog::warn("TerminalTabPage::writeInput: view_ is null, dropping input");
}

void TerminalTabPage::writeRawInput(const QByteArray& data)
{
    if (view_) {
        view_->writeInput(data);
    } else {
        spdlog::warn("TerminalTabPage::writeRawInput: view_ is null, dropping {} bytes", data.size());
    }
}
