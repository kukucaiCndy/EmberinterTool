#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QSerialPortInfo>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>
#include "serial_tab_widget.h"
#include "log_view.h"
#include "send_panel.h"
#include "status_bar.h"
#include "ipc_server.h"
#include "serial_engine.h"
#include "log_buffer.h"
#include "config_manager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    struct PendingConnectRequest {
        QString clientId;
        QString requestId;
        QString port;
        int baudrate;
    };

private slots:
    void onSettings();
    void onAbout();
    void onClear();
    void onExport();
    void onPauseToggled(bool paused);
    void onFilterChanged(const QString& text);
    void onTabChanged(int index);
    void onTabClosed(int index);
    void onSerialDataReceived(const QByteArray& data, const QString& port);
    void onSerialStatusChanged(const QString& port, bool connected, const SerialConfig& config);

    void onAddSavedPort();
    void onEditSavedPort();
    void onDeleteSavedPort();
    void onDisconnectAction();
    void onConnectAction();
    void onSavedPortClicked(QListWidgetItem* item);
    void onSavedPortContextMenu(const QPoint& pos);

private:
    void setupUi();
    void setupSidebar(QWidget* sidebar);
    void setupMainArea(QWidget* mainArea);
    void setupConnections();
    void loadConfig();
    void saveConfig();
    void loadTheme();
    void closeEvent(QCloseEvent* event) override;

    void connectSavedPort(int savedIdx);
    void disconnectCurrentPort();

    QSplitter* splitter_;

    QWidget* sidebar_;
    QListWidget* savedPortList_;

    SerialTabWidget* tabWidget_;
    LogView* logView_;
    QLineEdit* filterEdit_;
    QComboBox* displayModeCombo_;
    QPushButton* pauseBtn_;
    QPushButton* clearBtn_;
    QPushButton* exportBtn_;
    SendPanel* sendPanel_;
    StatusBar* statusBar_;
    IPCServer* ipcServer_;
    LogBuffer* logBuffer_;
    QTimer* statusTimer_;
    QElapsedTimer connectionTimer_;
    bool connectionTimerRunning_;
    QMap<QString, PendingConnectRequest> pendingConnects_;

    QVector<SavedPort> savedPorts_;

    qint64 rxBytes_;
    qint64 txBytes_;
};

#endif