# EmberInterDebugTool (尘智) 项目规则

## 项目概述

嵌入式跨平台调试工具，支持串口终端、SSH 远程终端、本地命令行终端以及 ST-Link/J-Link 等调试工具。

## 技术栈

- **语言**: C++17
- **框架**: Qt6 QML (MSYS2 mingw64, 动态链接 LGPLv3)
- **构建系统**: CMake ≥ 3.16
- **第三方库**: spdlog, fmt, RapidJSON
- **许可**: 自有许可，避免 GPL 传染性库

## 架构总览

```
src/
├── core/                          # 核心静态库 (serialmonitor_core)
│   ├── tab_type.h                 # Tab 类型枚举
│   ├── ansi_parser.h/cpp          # ANSI 转义序列解析器
│   ├── serial_engine.h/cpp        # 串口通信引擎
│   ├── log_buffer.h/cpp           # 线程安全日志环形缓冲
│   ├── log_parser.h/cpp           # 日志解析/行拆分/HEX格式化
│   ├── log_exporter.h/cpp         # JSON日志导出
│   ├── config_manager.h/cpp       # JSON配置读写
│   ├── ipc_protocol.h/cpp         # IPC消息协议
│   └── pty_process.h              # ConPTY/PTY 抽象基类
│
├── gui/                           # GUI可执行程序 (serial-monitor)
│   ├── ipc_server.h/cpp           # IPC服务端
│   ├── main.cpp                   # 入口: QGuiApplication + QQmlApplicationEngine
│   └── qml/                       # QML UI + C++ 逻辑
│       ├── tab_page.h             # TabPage 抽象基类 (QObject)
│       ├── app_core.h/cpp         # AppCore 主逻辑 (TabModel, SavedPortModel)
│       ├── serial_tab_page.h/cpp  # 串口终端 Tab (纯逻辑)
│       ├── terminal_tab_page.h/cpp # 终端/SSH Tab (纯逻辑)
│       ├── terminal_view.h/cpp    # QQuickPaintedItem 终端控件
│       ├── debug_page.h/cpp       # ST-Link/J-Link 调试页
│       ├── main.qml               # 主窗口
│       ├── SerialTab.qml          # 串口 Tab
│       ├── TerminalTab.qml        # 终端 Tab
│       ├── DebugTab.qml           # 调试 Tab
│       ├── ConnectionWizard.qml   # 连接向导 (串口/终端/SSH/ST-Link/J-Link)
│       ├── SettingsDialog.qml     # 设置对话框
│       ├── AboutDialog.qml        # 关于对话框
│       └── EmberDesign/           # Design System 模块
│           ├── DesignSystem.qml   # 设计令牌 (颜色/字体/间距)
│           └── qmldir
│
└── cli/                           # CLI可执行程序 (serial-monitor-cli)
    ├── cli_app.h/cpp
    ├── ipc_client.h/cpp
    └── main.cpp
```

## Qt6 环境 (MSYS2)

| 包名 | 用途 |
|------|------|
| `mingw-w64-x86_64-qt6-base` | Qt6 核心 (QGuiApplication, QSerialPort, etc.) |
| `mingw-w64-x86_64-qt6-declarative` | QML/QtQuick 引擎 |
| `mingw-w64-x86_64-qt6-serialport` | 串口通信 |
| `mingw-w64-x86_64-qt6-tools` | qmlscene, qmllint 等工具 |

## 设计原则

1. **core/ 不变**: 所有核心逻辑（引擎、解析、缓冲、协议）纯 C++/Qt6 Core
2. **cli/ 轻量**: CLI 仅依赖 Qt6::Core + Qt6::Network
3. **gui/ QML 驱动**: UI 由 QML 负责，C++ 类为纯逻辑 QObject
4. **IPC 兼容**: 协议不变，GUI/CLI 通信使用 QLocalServer/QLocalSocket
5. **TabPage = QObject**: QML 集成，无 QWidget 依赖

## Tab 类型枚举 (src/core/tab_type.h)

```cpp
enum class TabType {
    Serial,     // 串口终端 — 日志查看 + 数据发送
    SSH,        // SSH 远程终端 — 交互式 shell
    CMD,        // 本地命令行终端 — cmd/bash/powershell
    STLink,     // ST-Link 调试 — 寄存器/堆栈/调用栈分析
    JLink,      // J-Link 调试 — 寄存器/堆栈/调用栈分析
};
```

## 构建命令

```bash
# 安装 Qt6 (一次性)
pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-declarative \
          mingw-w64-x86_64-qt6-serialport mingw-w64-x86_64-qt6-tools

# 配置构建
mkdir -p build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug

# 编译
mingw32-make -j$(nproc)
```

## 语法检查约定

- 每次修改完成后运行 `mingw32-make -j$(nproc)` 确保编译通过
- 提交前确保无 warning（`-Wall -Wextra`）
- 头文件使用 `#pragma once` 或传统的 `#ifndef` 防护
