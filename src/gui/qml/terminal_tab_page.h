#ifndef QML_TERMINAL_TAB_PAGE_H
#define QML_TERMINAL_TAB_PAGE_H

#include "tab_page.h"
#include <QJsonObject>

class TerminalView;

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
    void disconnect() override;

    // 供 QML TerminalView 使用
    Q_INVOKABLE void attachView(TerminalView* view);
    TerminalView* view() const { return view_; }
    bool connected() const { return connected_; }
    QString shellName() const { return shellName_; }

    Q_INVOKABLE void writeInput(const QByteArray& data);

signals:
    void connectedChanged();
    void shellNameChanged();

private:
    void doConnect(const QJsonObject& params);

    TabType type_;
    TerminalView* view_;
    bool connected_;
    QString shellName_;
    QJsonObject pendingParams_;  // view_ 未就绪时暂存连接参数
};

#endif
