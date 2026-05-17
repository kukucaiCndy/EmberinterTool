#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void loadFromConfig();
    void saveToConfig();

private:
    QComboBox* themeCombo_;
    QSpinBox* fontSizeSpin_;
    QSpinBox* bufferSizeSpin_;
    QCheckBox* timestampCheck_;
    QCheckBox* autoScrollCheck_;
    QCheckBox* autoReconnectCheck_;
    QLineEdit* ipcNameEdit_;

    void setupUi();
};

#endif
