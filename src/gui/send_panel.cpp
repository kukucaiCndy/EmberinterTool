#include "send_panel.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QRegularExpression>

SendPanel::SendPanel(QWidget* parent)
    : QWidget(parent)
    , hexSendMode_(false)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("发送:", this));

    senderEdit_ = new QLineEdit(this);
    senderEdit_->setPlaceholderText("输入数据后按回车发送...");
    layout->addWidget(senderEdit_, 1);

    modeCombo_ = new QComboBox(this);
    modeCombo_->addItem("文本");
    modeCombo_->addItem("HEX");
    layout->addWidget(modeCombo_);

    appendCombo_ = new QComboBox(this);
    appendCombo_->addItem("CRLF");
    appendCombo_->addItem("CR");
    appendCombo_->addItem("LF");
    appendCombo_->addItem("无");
    layout->addWidget(appendCombo_);

    sendBtn_ = new QPushButton("发送", this);
    layout->addWidget(sendBtn_);

    connect(sendBtn_, &QPushButton::clicked, this, &SendPanel::onSendClicked);
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SendPanel::onModeChanged);
    connect(senderEdit_, &QLineEdit::returnPressed, this, &SendPanel::onSendClicked);
}

void SendPanel::setHexMode(bool enabled)
{
    hexSendMode_ = enabled;
    modeCombo_->setCurrentIndex(enabled ? 1 : 0);
}

void SendPanel::addSendHistory(const QString& text)
{
    Q_UNUSED(text);
}

void SendPanel::onSendClicked()
{
    QString text = senderEdit_->text();
    if (text.isEmpty()) {
        return;
    }

    if (hexSendMode_) {
        QString cleaned = text;
        cleaned.remove(QRegularExpression("\\s"));
        if (cleaned.length() % 2 != 0) {
            return;
        }
        QByteArray hexData;
        for (int i = 0; i < cleaned.length(); i += 2) {
            bool ok;
            unsigned char byte = static_cast<unsigned char>(
                cleaned.mid(i, 2).toUInt(&ok, 16));
            if (!ok) return;
            hexData.append(static_cast<char>(byte));
        }
        emit sendHex(hexData);
    } else {
        QString append;
        switch (appendCombo_->currentIndex()) {
        case 0: append = "CRLF"; break;
        case 1: append = "CR";   break;
        case 2: append = "LF";   break;
        case 3: append = "NONE"; break;
        }
        emit sendText(text, append);
    }

    senderEdit_->clear();
}

void SendPanel::onModeChanged(int index)
{
    hexSendMode_ = (index == 1);
    senderEdit_->setPlaceholderText(
        hexSendMode_ ? "输入HEX数据, 如: 01 02 03 FF" : "输入数据后按回车发送...");
}
