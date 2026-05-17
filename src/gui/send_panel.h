#ifndef SEND_PANEL_H
#define SEND_PANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>

class SendPanel : public QWidget {
    Q_OBJECT
public:
    explicit SendPanel(QWidget* parent = nullptr);

    void setHexMode(bool enabled);
    void addSendHistory(const QString& text);

signals:
    void sendText(const QString& data, const QString& append);
    void sendHex(const QByteArray& data);

private slots:
    void onSendClicked();
    void onModeChanged(int index);

private:
    QLineEdit* senderEdit_;
    QPushButton* sendBtn_;
    QComboBox* modeCombo_;
    QComboBox* appendCombo_;
    bool hexSendMode_;
};

#endif
