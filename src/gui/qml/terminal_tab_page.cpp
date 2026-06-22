#include "terminal_tab_page.h"
#include "terminal/terminal_view.h"
#include <QJsonObject>
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
        QString shell = params["shell"].toString("cmd.exe");
        shellName_ = shell;
        emit shellNameChanged();
        emit tabTitleChanged();

        QStringList args;
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
        }
        spdlog::debug("TerminalTabPage::doConnect: calling view_->startShell({}, {}), view_={}",
                      shell.toStdString(), args.size(), (void*)view_);

        if (isMsysBash) {
            // MSYS2/Git Bash 需要正确的 TERM 和 MSYSTEM 环境变量才能保持交互
            QVariantMap env;
            env["TERM"] = "xterm-256color";
            env["MSYSTEM"] = "MINGW64";
            env["MINGW_PREFIX"] = "/mingw64";
            env["CHERE_INVOKING"] = "1";
            view_->startShellWithEnv(shell, args, env);
        } else {
            view_->startShell(shell, args);
        }
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
