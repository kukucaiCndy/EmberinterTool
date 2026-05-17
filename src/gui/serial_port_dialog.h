#ifndef SERIAL_PORT_DIALOG_H
#define SERIAL_PORT_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSerialPortInfo>
#include "config_manager.h"

class SerialPortDialog : public QDialog {
    Q_OBJECT
public:
    explicit SerialPortDialog(QWidget* parent = nullptr);

    void setSavedPort(const SavedPort& sp);
    SavedPort savedPort() const;

private:
    QComboBox* portCombo_;
    QComboBox* baudCombo_;
    QComboBox* dataBitsCombo_;
    QComboBox* parityCombo_;
    QComboBox* stopBitsCombo_;
    QLineEdit* nameEdit_;

    void setupUi();
};

#endif