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
    disconnect();
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
    view_ = view;
    if (!view_) {
        spdlog::warn("TerminalTabPage::attachView: view is null, aborting");
        return;
    }

    // 连接 View 信号
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
        if (shell == "powershell.exe" || shell == "pwsh.exe") {
            args << "-NoLogo" << "-NoExit";
        }
        spdlog::debug("TerminalTabPage::doConnect: calling view_->startShell({}, {}), view_={}",
                      shell.toStdString(), args.size(), (void*)view_);
        view_->startShell(shell, args);
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
        args << "-o" << "UserKnownHostsFile=/dev/null";

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

void TerminalTabPage::disconnect()
{
    spdlog::debug("TerminalTabPage::disconnect: view_={}, this={}", (void*)view_, (void*)this);
    if (view_) {
        view_->terminate();
    }
    connected_ = false;
    emit connectedChanged();
    emit statusChanged(false);
}

void TerminalTabPage::writeInput(const QByteArray& data)
{
    if (view_)
        view_->writeInput(data);
    else
        spdlog::warn("TerminalTabPage::writeInput: view_ is null, dropping input");
}
