#ifndef QML_TERMINAL_TAB_PAGE_H
#define QML_TERMINAL_TAB_PAGE_H

#include "tab_page.h"
#include "terminal/terminal_view.h"
#include <QJsonObject>
#include <QPointer>

/// 终端/SSH Tab 页 (纯逻辑)
class TerminalTabPage : public TabPage {
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString shellName READ shellName NOTIFY shellNameChanged)

public:
    explicit TerminalTabPage(TabType type, QObject* parent = nullptr);
    ~TerminalTabPage();

    TabType tabType() const override { return type_; }
    QString tabTitle() const override;
    bool isConnected() const override { return connected_; }

    void connectTo(const QJsonObject& params) override;
    void closeConnection() override;

    // 供 QML TerminalView 使用
    Q_INVOKABLE void attachView(TerminalView* view);
    TerminalView* view() const { return view_; }
    bool connected() const { return connected_; }
    QString shellName() const { return shellName_; }

    Q_INVOKABLE void writeInput(const QString& text);

    /// 向终端写入原始字节 (供 IPC 调用, 不经过 QML)
    void writeRawInput(const QByteArray& data);

signals:
    void connectedChanged();
    void shellNameChanged();
    /// 终端输出数据 (转发自 TerminalModel::dataReceived)
    /// 供 AppCore 转发到 IPC 订阅客户端
    void dataReceived(const QByteArray& data);

private:
    void doConnect(const QJsonObject& params);

    TabType type_;
    QPointer<TerminalView> view_;  // QPointer 防止 QML 销毁 view 后悬垂指针
    bool connected_;
    QString shellName_;
    QJsonObject pendingParams_;  // view_ 未就绪时暂存连接参数
};

#endif
