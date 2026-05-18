# EmberInterDebugTool (尘智) 项目规则

## 项目概述

嵌入式跨平台调试工具，支持串口终端、SSH 远程终端、本地命令行终端以及 ST-Link/J-Link 等调试工具。

## 技术栈

- **语言**: C++17
- **框架**: Qt5 (MSYS2 mingw64, 动态链接 LGPLv3)
- **构建系统**: CMake ≥ 3.16
- **第三方库**: spdlog, fmt, RapidJSON
- **许可**: 自有许可，避免 GPL 传染性库

## 架构总览

```
src/
├── core/                          # 核心静态库 (serialmonitor_core)
│   ├── tab_type.h                 # Tab 类型枚举 (新增)
│   ├── ansi_parser.h/cpp          # ANSI 转义序列解析器 (新增)
│   ├── serial_engine.h/cpp        # 串口通信引擎
│   ├── log_buffer.h/cpp           # 线程安全日志环形缓冲
│   ├── log_parser.h/cpp           # 日志解析/行拆分/HEX格式化
│   ├── log_exporter.h/cpp         # JSON日志导出
│   ├── config_manager.h/cpp       # JSON配置读写
│   └── ipc_protocol.h/cpp         # IPC消息协议
│
├── gui/                           # GUI可执行程序 (serial-monitor)
│   ├── tab_page.h                 # Tab页抽象基类 (新增)
│   ├── serial_tab_page.h/cpp      # 串口终端Tab (新增, 重构)
│   ├── terminal_view.h/cpp        # 交互式终端控件 (新增)
│   ├── serial_tab_widget.h/cpp    # Tab容器控件 (改造)
│   ├── log_view.h/cpp             # 只读日志显示面板
│   ├── send_panel.h/cpp           # 数据发送面板
│   ├── main_window.h/cpp          # 主窗口 (精简)
│   ├── status_bar.h/cpp           # 状态栏
│   ├── settings_dialog.h/cpp      # 设置对话框
│   ├── serial_port_dialog.h/cpp   # 串口配置对话框
│   └── ipc_server.h/cpp           # IPC服务端
│
└── cli/                           # CLI可执行程序 (serial-monitor-cli)
    ├── cli_app.h/cpp
    └── ipc_client.h/cpp
```

## Tab 类型扩展计划

### 设计原则

1. **Tab = 独立功能单元**：每个 Tab 拥有独立的视图、引擎和数据缓冲
2. **TabPage 抽象基类**：统一定义生命周期接口和类型标识
3. **MainWindow 精简**：只做调度和协调，不管理具体 Tab 内容
4. **CLI/GUI 同步**：IPC 协议携带 tab_type 字段，CLI 可感知和操作不同类型 Tab
5. **自研优先**：不引入 GPL 许可的第三方终端库

### Tab 类型枚举 (src/core/tab_type.h)

```cpp
enum class TabType {
    Serial,     // 串口终端 — 日志查看 + 数据发送
    SSH,        // SSH 远程终端 — 交互式 shell
    CMD,        // 本地命令行终端 — cmd/bash/powershell
    STLink,     // ST-Link 调试 — 寄存器/堆栈/调用栈分析
    JLink,      // J-Link 调试 — 寄存器/堆栈/调用栈分析
};
```

### TabPage 抽象基类 (src/gui/tab_page.h)

```cpp
class TabPage : public QWidget {
    Q_OBJECT
public:
    virtual TabType tabType() const = 0;
    virtual QString tabTitle() const = 0;
    virtual bool isConnected() const = 0;
    virtual void connectTo(const QJsonObject& params) = 0;
    virtual void disconnect() = 0;

signals:
    void rxBytesChanged(qint64 bytes);
    void txBytesChanged(qint64 bytes);
    void statusChanged(bool connected);
};
```

### 实施阶段

#### Phase 1: 类型枚举 + 抽象基类 (最小侵入)

- [ ] 新增 `src/core/tab_type.h` — TabType 枚举
- [ ] 新增 `src/gui/tab_page.h` — TabPage 抽象基类
- [ ] 改造 `SerialTabWidget` — 管理 `TabPage*` 替代 `TabInfo`
- [ ] 改造 `TabConfig` — 增加 `TabType type` 字段
- [ ] 改造 IPC 协议 — 增加 `tab_type` 字段

#### Phase 2: 重构 SerialTabPage (核心重构)

- [ ] 新增 `src/gui/serial_tab_page.h/cpp` — 集成 LogView + SendPanel + SerialEngine + LogBuffer
- [ ] 从 MainWindow 移出全局 `logView_`, `sendPanel_`, `logBuffer_`
- [ ] 每个 SerialTab 拥有独立的 toolbar (过滤/HEX/暂停/清空/导出)
- [ ] 精简 MainWindow，只保留 splitter/sidebar/tabWidget/IPCServer

#### Phase 3: ANSI 解析器 (core 层, CLI/GUI 共享)

- [ ] 新增 `src/core/ansi_parser.h/cpp` — 轻量 ANSI 状态机
- [ ] 支持 SGR select graphic rendition (颜色/粗体/下划线/重置)
- [ ] 支持光标控制 (上下左右/定位/归位)
- [ ] 支持清屏/清行
- [ ] 支持 256 色 (可选)

#### Phase 4: 交互式 TerminalView (GUI 层)

- [ ] 新增 `src/gui/terminal_view.h/cpp` — 基于 QPlainTextEdit
- [ ] 集成 ANSI 解析器渲染输出
- [ ] QProcess 桥接 (stdin 键盘输入 → stdout/stderr ANSI 解析)
- [ ] 输入行编辑 (锁定历史行, 只允许最后一行编辑)
- [ ] 跨平台 PTY 适配 (Windows: QWinEventNotifier + CreateProcess, Linux: QSocketNotifier)

#### Phase 5: CmdTabPage (本地终端)

- [ ] 新增 `src/gui/cmd_tab_page.h/cpp` — QProcess 启动 shell
- [ ] 复用 TerminalView
- [ ] 支持 cmd.exe / powershell.exe / bash 切换

#### Phase 6: SshTabPage (SSH 终端)

- [ ] 新增 `src/gui/ssh_tab_page.h/cpp` — QProcess + ssh 命令
- [ ] 复用 TerminalView
- [ ] 支持 host/user/password 配置

#### Phase 7: ST-Link / J-Link 调试页

- [ ] 新增 `src/gui/stlink_debug_page.h/cpp`
- [ ] 新增 `src/gui/jlink_debug_page.h/cpp`
- [ ] 封装 st-link/jlink CLI 工具调用
- [ ] 寄存器面板、堆栈面板、调用栈分析视图
- [ ] 通过 GDB/MI 协议读取调试信息

### Phase 1~2 前后架构对比

```
改造前:
┌─ MainWindow (上帝类) ─────────────────────────────────┐
│  logView (全局单例)  ← 所有Tab共享同一个日志视图         │
│  sendPanel (全局单例) ← 所有Tab共享同一个发送面板         │
│  logBuffer (全局单例)                                   │
│  SerialTabWidget → TabInfo { port, name, engine }       │
│  Tab切换 → 只更新状态栏, 不切换内容视图                    │
└────────────────────────────────────────────────────────┘

改造后:
┌─ MainWindow (精简) ────────────────────────────────────┐
│  SerialTabWidget → TabPage*                             │
│  ┌─────────────────────────────────────────────────┐   │
│  │ SerialTabPage (独立)                              │   │
│  │  ├ logView_     ← 此Tab独享                      │   │
│  │  ├ sendPanel_   ← 此Tab独享                      │   │
│  │  ├ logBuffer_   ← 此Tab独享                      │   │
│  │  ├ toolbar_     ← 此Tab独享                      │   │
│  │  └ engine_      ← SerialEngine                   │   │
│  ├─────────────────────────────────────────────────┤   │
│  │ CmdTabPage (独立)                                 │   │
│  │  ├ TerminalView  ← 交互式终端                     │   │
│  │  └ QProcess      ← 本地 shell                    │   │
│  ├─────────────────────────────────────────────────┤   │
│  │ SshTabPage (独立)                                 │   │
│  │  ├ TerminalView  ← 交互式终端 (复用)              │   │
│  │  └ QProcess      ← ssh 命令                      │   │
│  └─────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────┘
```

## 构建命令

```bash
# 配置 (MSYS2 mingw64 终端)
mkdir -p build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug

# 编译
mingw32-make -j$(nproc)

# 编译并验证
mingw32-make -j$(nproc) 2>&1 | tee build.log
```

## 语法检查约定

- 每次修改完成后运行 `make -j$(nproc)` 确保编译通过
- 提交前确保无 warning（`-Wall -Wextra`）
- 头文件使用 `#pragma once` 或传统的 `#ifndef` 防护