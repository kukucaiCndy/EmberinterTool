#pragma once

#include "terminal_buffer.h"
#include "vt_parser.h"
#include "terminal_input.h"
#include "terminal_cell.h"
#include "../pty_process.h"

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QFont>
#include <QTimer>
#include <memory>

/// 终端模型 - 连接 VT 解析器、缓冲区、PTY 的核心逻辑层
/// 不依赖任何 UI 组件, 可独立测试
class TerminalModel : public QObject {
    Q_OBJECT

public:
    explicit TerminalModel(QObject* parent = nullptr);
    ~TerminalModel() override;

    /// 初始化终端
    void init(int cols = 80, int rows = 24);

    /// 启动 shell 进程
    bool startShell(const QString& program, const QStringList& args = {},
                    const QString& cwd = {},
                    const QProcessEnvironment& env = {});

    /// 写入数据到 PTY
    void writeToPty(const QByteArray& data);

    /// 处理键盘输入
    void handleKeyEvent(QKeyEvent* event);

    /// 处理鼠标按下
    void handleMousePress(QMouseEvent* event, int col, int row);
    /// 处理鼠标释放
    void handleMouseRelease(QMouseEvent* event, int col, int row);
    /// 处理鼠标移动
    void handleMouseMove(QMouseEvent* event, int col, int row);

    /// 调整终端大小
    void resize(int cols, int rows);

    /// 终止进程
    void terminate();

    /// 获取缓冲区
    TerminalBuffer& buffer() { return buffer_; }
    const TerminalBuffer& buffer() const { return buffer_; }

    /// 获取调色板
    ColorPalette& palette() { return palette_; }
    const ColorPalette& palette() const { return palette_; }

    /// 获取输入处理器
    TerminalInput& input() { return input_; }

    /// 进程是否在运行
    bool isRunning() const;

    /// 获取窗口标题
    QString windowTitle() const { return windowTitle_; }

    /// 获取 bell 请求
    bool hasBell() const { return bellRequested_; }
    void clearBell() { bellRequested_ = false; }

signals:
    /// 终端内容已更新 (需要重绘)
    void contentChanged();
    /// 光标位置变化
    void cursorChanged(int col, int row);
    /// Shell 已启动
    void shellStarted();
    /// Shell 已退出
    void shellExited(int exitCode);
    /// 窗口标题变化
    void titleChanged(const QString& title);
    /// Bell 请求
    void bell();
    /// 进程输出 (用于日志等)
    void dataReceived(const QByteArray& data);
    /// 输入数据已发送
    void dataSent(const QByteArray& data);

private slots:
    void onPtyReadyRead();
    void onPtyFinished(int exitCode);

private:
    // VT 事件处理
    void onVtEvent(const VtParser::Event& ev);
    void handlePrint(const VtParser::Event& ev);
    void handleExecute(const VtParser::Event& ev);
    void handleCsiDispatch(const VtParser::Event& ev);
    void handleOscDispatch(const VtParser::Event& ev);
    void handleEscDispatch(const VtParser::Event& ev);

    // CSI 命令处理
    void csiSGR(const std::vector<int>& params);          // SGR (m)
    void csiCursorUp(int n);                               // CUU (A)
    void csiCursorDown(int n);                             // CUD (B)
    void csiCursorForward(int n);                          // CUF (C)
    void csiCursorBack(int n);                             // CUB (D)
    void csiCursorNextLine(int n);                         // CNL (E)
    void csiCursorPrevLine(int n);                         // CPL (F)
    void csiCursorHorizontalAbsolute(int n);               // CHA (G)
    void csiCursorPosition(int row, int col);              // CUP (H/f)
    void csiCursorVerticalAbsolute(int n);                 // VPA (d)
    void csiEraseInDisplay(int mode);                      // ED (J)
    void csiEraseInLine(int mode);                         // EL (K)
    void csiInsertLines(int n);                            // IL (L)
    void csiDeleteLines(int n);                            // DL (M)
    void csiInsertCharacters(int n);                       // ICH (@)
    void csiDeleteCharacters(int n);                       // DCH (P)
    void csiEraseCharacters(int n);                        // ECH (X)
    void csiScrollUp(int n);                               // SU (S)
    void csiScrollDown(int n);                             // SD (T)
    void csiSetScrollRegion(int top, int bottom);          // DECSTBM (r)
    void csiTabForward(int n);                             // CHT (I)
    void csiTabBackward(int n);                            // CBT (Z)
    void csiSaveCursor();                                  // DECSC (s)
    void csiRestoreCursor();                               // DECRC (u)

    // DEC 私有模式
    void decSetMode(int mode, bool enable);

    // 辅助方法
    int paramValue(const std::vector<int>& params, size_t index, int defaultValue) const;
    void notifyChanged();

    TerminalBuffer buffer_;
    VtParser parser_;
    TerminalInput input_;
    ColorPalette palette_;
    std::unique_ptr<PtyProcess> pty_;

    QString windowTitle_;
    bool bellRequested_ = false;

    // 帧合并: 同一帧内多次 contentChanged 只通知一次
    bool contentChangedPending_ = false;
    QTimer frameTimer_;
};
