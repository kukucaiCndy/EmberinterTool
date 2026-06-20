#include "debug_page.h"
#include <spdlog/spdlog.h>

DebugPage::DebugPage(TabType type, QObject* parent)
    : TabPage(parent)
    , type_(type)
    , connected_(false)
    , halted_(false)
    , programCounter_("--------")
{
    debuggerType_ = (type == TabType::STLink) ? "ST-Link" : "J-Link";
    targetChip_ = "未识别";
}

QString DebugPage::tabTitle() const
{
    return QString("[%1] %2").arg(debuggerType_, targetChip_);
}

void DebugPage::connectTo(const QJsonObject& params)
{
    Q_UNUSED(params);
    connected_ = false;  // 实际连接待后续实现
    spdlog::info("DebugPage: {} connect (placeholder)", debuggerType_.toStdString());
}

void DebugPage::disconnect()
{
    connected_ = false;
    halted_ = false;
    emit connectedChanged();
    emit haltedChanged();
    emit statusChanged(false);
}

void DebugPage::runCommand(const QString& cmd)
{
    spdlog::info("DebugPage::runCommand: {}", cmd.toStdString());
    emit outputReceived(QString("[%1] 命令: %2\n").arg(debuggerType_, cmd));
}

void DebugPage::readRegisters()
{
    regModel_.clearAll();
    QString out = QString("[%1] 寄存器读取\n").arg(debuggerType_);
    out += "R0=--------  R1=--------  R2=--------  R3=--------\n";
    out += "R4=--------  R5=--------  R6=--------  R7=--------\n";
    out += "R8=--------  R9=--------  R10=-------- R11=--------\n";
    out += "R12=-------- SP=--------  LR=--------  PC=--------\n";
    out += "\n(连接真实调试器后自动读取寄存器)";
    emit outputReceived(out);
}

void DebugPage::readMemory(uint32_t addr, int len)
{
    memoryDump_ = QString("[%1] 内存 @ 0x%2 (%3 bytes)\n")
        .arg(debuggerType_)
        .arg(addr, 8, 16, QChar('0'))
        .arg(len);
    // 生成占位字节数据
    for (int i = 0; i < len; i += 16) {
        int rowLen = qMin(16, len - i);
        memoryDump_ += QString("0x%1  ").arg(addr + i, 8, 16, QChar('0'));
        for (int j = 0; j < 16; ++j) {
            if (j < rowLen)
                memoryDump_ += "?? ";
            else
                memoryDump_ += "   ";
            if (j == 7) memoryDump_ += " ";
        }
        memoryDump_ += " |";
        for (int j = 0; j < rowLen; ++j)
            memoryDump_ += '.';
        memoryDump_ += "|\n";
    }
    memoryDump_ += " (连接真实调试器后自动读取内存)";
    emit memoryDumpChanged();
    emit outputReceived(memoryDump_);
}

void DebugPage::halt()
{
    halted_ = true;
    emit haltedChanged();
    emit outputReceived(QString("[%1] Halt\n").arg(debuggerType_));
    readRegisters();
}

void DebugPage::resume()
{
    halted_ = false;
    emit haltedChanged();
    emit outputReceived(QString("[%1] Run\n").arg(debuggerType_));
}

void DebugPage::step()
{
    emit outputReceived(QString("[%1] Step\n").arg(debuggerType_));
    readRegisters();
}

void DebugPage::run()
{
    resume();
}

void DebugPage::reset()
{
    halted_ = true;
    programCounter_ = "--------";
    emit haltedChanged();
    emit programCounterChanged();
    emit outputReceived(QString("[%1] Reset\n").arg(debuggerType_));
}

void DebugPage::toggleBreakpoint()
{
    emit outputReceived(QString("[%1] 断点切换 (待实现)\n").arg(debuggerType_));
}

void DebugPage::stepOver()
{
    emit outputReceived(QString("[%1] Step Over (待实现)\n").arg(debuggerType_));
    readRegisters();
}

void DebugPage::stepOut()
{
    emit outputReceived(QString("[%1] Step Out (待实现)\n").arg(debuggerType_));
    readRegisters();
}

void DebugPage::readCallStack()
{
    QVector<QPair<QString, QString>> frames = {
        {"main()", "0x08000400"},
        {"HAL_Init()", "0x08000380"},
        {"SystemClock_Config()", "0x080002F0"},
    };
    stackModel_.rebuild(frames);
    emit outputReceived(QString("[%1] 调用栈 (%2 帧) — 连接真实调试器后获取实际数据\n")
        .arg(debuggerType_).arg(frames.size()));
}
