#ifndef QML_DEBUG_PAGE_H
#define QML_DEBUG_PAGE_H

#include "tab_page.h"
#include <QJsonObject>
#include <QAbstractListModel>
#include <QProcess>
#include <QVector>

/// 寄存器模型 (供 QML ListView 使用)
class RegisterModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { NameRole = Qt::UserRole + 1, ValueRole };

    explicit RegisterModel(QObject* parent = nullptr) : QAbstractListModel(parent) {
        // 默认 ARM Cortex-M 寄存器列表
        QVector<QPair<QString, QString>> regs = {
            {"R0","--------"}, {"R1","--------"}, {"R2","--------"}, {"R3","--------"},
            {"R4","--------"}, {"R5","--------"}, {"R6","--------"}, {"R7","--------"},
            {"R8","--------"}, {"R9","--------"}, {"R10","--------"},{"R11","--------"},
            {"R12","--------"},{"SP","--------"}, {"LR","--------"}, {"PC","--------"},
            {"xPSR","--------"},{"MSP","--------"},{"PSP","--------"},
        };
        for (const auto& r : regs) {
            registers_.append({r.first, r.second});
        }
    }

    int rowCount(const QModelIndex& = {}) const override { return registers_.size(); }
    QVariant data(const QModelIndex& index, int role) const override {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid)) return {};
        switch (role) {
        case NameRole:  return registers_[index.row()].first;
        case ValueRole: return registers_[index.row()].second;
        }
        return {};
    }
    QHash<int, QByteArray> roleNames() const override {
        return {{NameRole, "name"}, {ValueRole, "value"}};
    }

    void setValue(const QString& name, const QString& val) {
        for (int i = 0; i < registers_.size(); ++i) {
            if (registers_[i].first == name) {
                registers_[i].second = val;
                emit dataChanged(index(i), index(i), {ValueRole});
                return;
            }
        }
    }
    void clearAll() {
        for (auto& r : registers_) r.second = "--------";
        emit dataChanged(index(0), index(registers_.size() - 1), {ValueRole});
    }

private:
    QVector<QPair<QString, QString>> registers_;
};

/// 调用栈模型
class CallStackModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { IndexRole = Qt::UserRole + 1, FunctionRole, AddressRole };

    explicit CallStackModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& = {}) const override { return frames_.size(); }
    QVariant data(const QModelIndex& index, int role) const override {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid)) return {};
        switch (role) {
        case IndexRole:    return index.row();
        case FunctionRole: return frames_[index.row()].first;
        case AddressRole:  return frames_[index.row()].second;
        }
        return {};
    }
    QHash<int, QByteArray> roleNames() const override {
        return {{IndexRole, "index"}, {FunctionRole, "function"}, {AddressRole, "address"}};
    }

    void rebuild(const QVector<QPair<QString, QString>>& frames) {
        beginResetModel();
        frames_ = frames;
        endResetModel();
    }
    void clear() {
        beginResetModel();
        frames_.clear();
        endResetModel();
    }

private:
    QVector<QPair<QString, QString>> frames_;
};

/// ST-Link / J-Link 调试 Tab 页
class DebugPage : public TabPage {
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString debuggerType READ debuggerType NOTIFY debuggerTypeChanged)
    Q_PROPERTY(QString targetChip READ targetChip NOTIFY targetChipChanged)
    Q_PROPERTY(bool halted READ halted NOTIFY haltedChanged)
    Q_PROPERTY(QString programCounter READ programCounter NOTIFY programCounterChanged)
    Q_PROPERTY(QObject* registerModel READ registerModel CONSTANT)
    Q_PROPERTY(QObject* callStackModel READ callStackModel CONSTANT)
    Q_PROPERTY(QString memoryDump READ memoryDump NOTIFY memoryDumpChanged)

public:
    explicit DebugPage(TabType type, QObject* parent = nullptr);

    TabType tabType() const override { return type_; }
    QString tabTitle() const override;
    bool isConnected() const override { return connected_; }

    void connectTo(const QJsonObject& params) override;
    void disconnect() override;

    QString debuggerType() const { return debuggerType_; }
    QString targetChip() const { return targetChip_; }
    bool halted() const { return halted_; }
    QString programCounter() const { return programCounter_; }
    QObject* registerModel() { return &regModel_; }
    QObject* callStackModel() { return &stackModel_; }
    QString memoryDump() const { return memoryDump_; }

    Q_INVOKABLE void runCommand(const QString& cmd);
    Q_INVOKABLE void readRegisters();
    Q_INVOKABLE void readMemory(uint32_t addr, int len);
    Q_INVOKABLE void halt();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void step();
    Q_INVOKABLE void run();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void toggleBreakpoint();
    Q_INVOKABLE void stepOver();
    Q_INVOKABLE void stepOut();
    Q_INVOKABLE void readCallStack();

signals:
    void connectedChanged();
    void debuggerTypeChanged();
    void targetChipChanged();
    void haltedChanged();
    void programCounterChanged();
    void memoryDumpChanged();
    void outputReceived(const QString& text);

private:
    TabType type_;
    bool connected_;
    bool halted_;
    QString debuggerType_;
    QString targetChip_;
    QString programCounter_;
    QString memoryDump_;
    RegisterModel regModel_;
    CallStackModel stackModel_;
};

#endif
