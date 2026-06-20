# EmberInterDebugTool (尘智) v3.0

## 产品需求文档 (PRD)

> 版本: v3.0  
> 日期: 2026-06-10  
> 状态: 需求确认  
> 受众: 产品 + 研发  

---

## 1. 产品概述

### 1.1 产品定位

**尘智 (EmberInterDebugTool)** 是一款面向嵌入式开发的 **人机协同 Agent 工作台**。

与传统的"串口调试助手"不同，尘智的核心价值不在于功能堆砌，而在于 **打通人与 AI 的协作边界**：

- **开发者** 通过 GUI 连接设备、查看日志、发送指令
- **AI Agent** 通过 CLI 连接同一会话、读取日志、发送指令
- 双方共享同一份配置、同一份日志缓冲、同一套状态视图
- 人能看到 AI 的操作，AI 能感知人的操作 — 透明协作

```
┌──────────────────────────────────────────────┐
│              尘智 - Agent 工作台               │
│                                               │
│  开发者 (GUI)  ←──  共享会话  ──→  AI Agent (CLI) │
│       │              │              │          │
│       │        日志 / 配置 / 状态          │          │
│       └──────────────┼──────────────┘          │
│                      │                         │
│              嵌入式设备 (串口/SSH/终端)           │
└──────────────────────────────────────────────┘
```

### 1.2 核心价值

| 价值点 | 描述 |
|--------|------|
| **人机协同** | 人和 AI 共享同一会话，操作实时可见，互不干扰 |
| **透明协作** | AI 发送的命令出现在 GUI 日志中（TX 标记），人可审查；人操作 CLI 也能感知 |
| **统一数据面** | 单一日志缓冲 + 配置持久化，GUI/CLI 数据一致 |
| **嵌入式原生** | 串口/SSH/本地终端一站式覆盖从裸机到 Linux 的调试场景 |
| **跨平台** | Windows/Linux/macOS 统一体验，自研终端渲染不依赖外部模拟器 |

### 1.3 目标用户

| 用户角色 | 使用方式 | 典型场景 |
|----------|----------|----------|
| 嵌入式固件开发者 | GUI 图形界面 | 连接开发板串口，查看日志输出，手动发送 AT 命令 |
| 硬件测试工程师 | GUI + CLI | GUI 监控日志，CLI 脚本批量发送测试指令 |
| AI Agent / 自动化脚本 | CLI 命令行 | 自动化调试流程：烧录→连接→发送命令→读取日志→判断结果 |
| DevOps 流水线 | CLI 非交互模式 | 固件 CI 中通过 `--send-file` 推送固件，`--get-logs` 验证启动日志 |

### 1.4 产品边界

**包含：**
- 串口终端（日志查看 + 数据收发）
- SSH 远程终端（通过系统 ssh 命令 + PTY 桥接）
- 本地命令行终端（cmd / PowerShell / bash）
- CLI + GUI 双模式，IPC 实时通信
- 会话配置持久化（JSON）

**不包含：**
- ST-Link / J-Link 硬件调试器集成（已作废，不做）
- 图形化数据绘图/示波器
- Modbus / CAN 等工业协议解析
- 多设备并行固件烧录

---

## 2. 功能需求

### 2.1 会话类型

系统支持 3 种会话类型：

#### FR-1 串口会话 (Serial)

**FR-1.1 连接参数**
- 串口号：自动侦测（已占用端口置灰不可选）
- 波特率：9600 / 19200 / 38400 / 57600 / 115200 / 230400 / 460800 / 921600（支持手动输入）
- 数据位：5 / 6 / 7 / 8（默认 8）
- 校验位：无校验 / 偶校验 / 奇校验 / 标记 / 空格
- 停止位：1 / 1.5 / 2

**FR-1.2 自动重连**
- 串口异常断开后自动重试，间隔 1s → 2s → 3s → 5s（上限）
- 可在配置中开关

**FR-1.3 日志显示**
- 彩色日志：TX(发送)绿色、ERROR 红色、WARN 黄色、INFO 绿色、DEBUG 蓝色、TRACE 灰色
- 时间戳显示（可开关）：格式 `HH:MM:SS.mmm`
- HEX 模式显示（可切换）
- 关键字过滤（实时）
- 自动滚动 / 暂停

**FR-1.4 数据发送**
- 文本发送：支持追加 CR / LF / CRLF / 无换行
- HEX 发送：`01 02 03` 或 `010203` 格式
- 发送历史（配置持久化）

**FR-1.5 日志导出**
- JSON 格式结构化导出，字段：`timestamp` / `level` / `text` / `raw_bytes` / `port`

#### FR-2 SSH 会话 (SSH)

**FR-2.1 连接参数**
- 主机地址（IP 或域名）
- 端口（默认 22）
- 用户名
- 密码（可选，留空则使用密钥认证）

**FR-2.2 终端功能**
- 通过系统 `ssh` 命令 + PTY 桥接实现
- 完整 ANSI 转义序列支持（16色/256色/RGB真彩、光标控制、清屏）
- 字体缩放（A+/A- 按钮）
- 5 套预设配色方案：Catppuccin Dark / Light / Green Phosphor / Amber / Nord

#### FR-3 本地终端会话 (CMD)

**FR-3.1 终端类型**
- Windows：cmd.exe / PowerShell / bash.exe (MSYS2)
- Linux/macOS：bash / zsh / 用户自定义 shell

**FR-3.2 终端功能**
- 通过 PTY（Win: ConPTY，Unix: openpty）实现原生终端体验
- 完整 ANSI 支持，与 SSH 终端共享同一渲染组件
- shell 退出后自动关闭 Tab

### 2.2 CLI + GUI 双模式协作

```
┌─────────────────────────────────────────────────┐
│  GUI 进程 (serial-monitor.exe)                   │
│  ┌──────────┐  ┌──────────────────────────────┐  │
│  │  会话列表  │  │    Tab 标签页 (QML)           │  │
│  │          │  │  ┌────────────────────────┐  │  │
│  │ Session1 │  │  │  日志 / 终端            │  │  │
│  │ Session2 │  │  │  人操作可见             │  │  │
│  │   ...    │  │  └────────────────────────┘  │  │
│  │  [+]-+   │  │                              │  │
│  └──────────┘  │  QLocalServer (IPC)          │  │
│                │  ▲ 接收 CLI 命令              │  │
│                │  ▼ 推送日志/状态到 CLI        │  │
│                └──────────────────────────────┘  │
└──────────────────────┬──────────────────────────┘
                       │ QLocalSocket
┌──────────────────────┴──────────────────────────┐
│  CLI 进程 (serial-monitor-cli.exe)               │
│  - AI Agent 通过参数调用                          │
│  - 接收实时日志流                                  │
│  - 发送命令到 GUI 执行                             │
│  - 操作会反映在 GUI 上（TX 标记）                   │
└─────────────────────────────────────────────────┘
```

**FR-4.1 透明协作机制**
- CLI 发送数据 → GUI 日志区显示为 TX（绿色），人类可见
- GUI 手动发送 → CLI 日志流中同样可见
- GUI 连接/断开 → CLI 收到 statusChanged 实时同步
- 双方共享配置文件和日志缓冲区

**FR-4.2 CLI 命令行参数**

| 参数 | 说明 |
|------|------|
| `-p, --port PORT` | 串口号 |
| `-b, --baudrate RATE` | 波特率 |
| `-f, --filter KW` | 过滤关键字 |
| `-o, --output FILE` | 保存日志到文件 |
| `--hex` | HEX 显示模式 |
| `--no-timestamp` | 禁用时间戳 |
| `--cli` | 交互 CLI 模式 |
| `--json` | JSON 输出模式（机器可读） |
| `--ipc NAME` | IPC 服务名（默认: serial_monitor_ipc） |
| `--connect PORT` | 连接指定串口 |
| `--send DATA` | 发送文本数据 |
| `--send-file FILE` | 发送二进制文件（Base64 编码） |
| `--list` | 列出可用串口 |
| `--get-status` | 显示连接状态 |
| `--get-logs N` | 获取最近 N 条日志 |

**FR-4.3 CLI 交互模式命令**

```
connect <port> [baud]    连接串口
disconnect / disc        断开当前串口
status                   显示连接状态
list                     列出可用串口
send <data>              发送文本数据（自动追加 CRLF）
sendhex <hex>            发送 HEX 数据
sendfile <file>          发送二进制文件
clear                    清空日志缓存
filter <keyword>         设置过滤关键字
hex / text               切换 HEX/文本 显示模式
timestamp / ts           切换时间戳显示
export <file>            导出日志为 JSON 文件
help / ?                 显示帮助
quit / q / exit          退出 CLI（GUI 继续运行）
```

### 2.3 会话管理

**FR-5.1 连接向导**
- 通过侧边栏 `[+]` 按钮弹出
- 三页向导：串口 / 终端 / SSH
- 填写参数后"创建会话"，存入 SavedPorts 列表

**FR-5.2 会话列表**
- 侧边栏显示已保存会话
- 按类型显示彩色图标：S(串口 蓝色) / T(终端 绿色) / H(SSH 橙色)
- 单击会话直接连接

**FR-5.3 Tab 管理**
- 每个已连接会话对应一个 Tab
- TabBar 显示标题 + 连接状态
- `[×]` 关闭当前 Tab + 断开连接

### 2.4 配置持久化

**FR-6.1 配置文件位置**
- Windows: `%APPDATA%/EmberInter/EmberInterDebugTool/config.json`
- Linux: `~/.config/EmberInter/EmberInterDebugTool/config.json`

**FR-6.2 保存内容**

```json
{
  "version": "3.0",
  "display": {
    "theme": "dark",
    "showTimestamp": true,
    "hexMode": false,
    "autoScroll": true,
    "autoReconnect": true
  },
  "filterHistory": ["ERROR", "BLE"],
  "send": {
    "history": ["reboot", "status"],
    "append": "CRLF"
  },
  "savedPorts": [
    {
      "type": "serial",
      "name": "STM32调试",
      "port": "COM3",
      "baudrate": 115200,
      "databits": 8,
      "parity": "N",
      "stopbits": 1
    },
    {
      "type": "ssh",
      "name": "开发板",
      "extra": {
        "host": "192.168.1.100",
        "port": 22,
        "user": "root"
      }
    },
    {
      "type": "cmd",
      "name": "本地Bash",
      "extra": {
        "shell": "bash.exe"
      }
    }
  ]
}
```

---

## 3. 非功能需求

### 3.1 性能

| 指标 | 要求 | 说明 |
|------|------|------|
| 串口读取延迟 | < 10ms | 从硬件发来到界面显示 |
| 日志缓冲区容量 | 10000 行（可配置） | O(1) 追加，不随行数增加变慢 |
| GUI 刷新率 | 60fps | ListView 虚拟化渲染，不卡顿 |
| 最大波特率 | 3Mbps | 高速串口不丢数据 |
| 同时连接数 | ≥ 4 个会话 | 独立线程，互不阻塞 |
| CLI 启动时间 | < 1s | 连接到 IPC 服务 |

### 3.2 兼容性

| 维度 | 要求 |
|------|------|
| 操作系统 | Windows 10/11、Linux (Ubuntu 20.04+)、macOS 12+ |
| 编译器 | MinGW-w64 gcc 13+、MSVC 2022+、GCC 11+ / Clang 14+ |
| Qt 版本 | Qt6.5+ (QML 模式) |
| 串口芯片 | CP210x、FTDI、CH340、CDC ACM 通用 |
| 终端 | Win: ConPTY (Win10 19041+)、Unix: openpty |

### 3.3 可靠性

| 场景 | 处理策略 |
|------|----------|
| 串口异常断开 | 自动重连，状态栏提示，日志不丢失 |
| 配置文件损坏 | 恢复默认配置，日志警告通知 |
| CLI 进程异常退出 | GUI 清理客户端连接，不影响串口 |
| GUI 进程异常退出 | CLI 检测断开，提示用户 |
| 日志文件写入失败 | 不中断程序，错误通知 |

### 3.4 安全性

| 项目 | 策略 |
|------|------|
| SSH 密码 | 内存中持有，不写入配置文件 |
| IPC 通信 | 仅限本机（QLocalSocket），不对外暴露 |
| 配置文件 | 存储于用户目录，不包含敏感信息 |

---

## 4. 系统架构

### 4.1 分层架构

```
┌───────────────────────────────────────────────┐
│              表示层 (Presentation)              │
│  GUI: QML (ApplicationWindow + Loader + ListView) │
│  CLI: 终端 I/O (QTextStream + stdin/stdout)      │
├───────────────────────────────────────────────┤
│              业务逻辑层 (Application)           │
│  AppCore (TabModel / SavedPortModel / IPC路由)  │
│  SerialTabPage / TerminalTabPage / DebugPage    │
│  CLIApp (命令解析 / 交互循环)                     │
├───────────────────────────────────────────────┤
│              核心服务层 (Core Services)          │
│  SerialEngine (QSerialPort + QThread)           │
│  PtyProcess (Win ConPTY / Unix openpty)         │
│  TerminalView (QQuickPaintedItem + AnsiParser)   │
│  LogBuffer (QMutex + deque<LogEntry>)            │
│  IPCServer / IPCClient (QLocalSocket)            │
│  ConfigManager (RapidJSON + QJsonDocument)       │
├───────────────────────────────────────────────┤
│              基础设施层 (Infrastructure)         │
│  Qt6 (Core/Quick/SerialPort/Network)            │
│  spdlog (日志) / fmt (格式化) / RapidJSON        │
└───────────────────────────────────────────────┘
```

### 4.2 模块依赖

```
serial-monitor.exe ───────────────┐
  ├── AppCore (TabModel, SavedPortModel, IPC路由)
  ├── SerialTabPage ─────► SerialEngine, LogBuffer, LogListModel
  ├── TerminalTabPage ────► TerminalView, PtyProcess
  └── IPCServer ──────────────────┐
                                  │
serial-monitor-cli.exe ───────────┤
  ├── CLIApp                      │
  └── IPCClient ──────────────────┘
                                  │
libserialmonitor_core.a ◄─────────┘
  ├── SerialEngine, LogBuffer, LogParser, LogExporter
  ├── ConfigManager (RapidJSON)
  ├── AnsiParser (纯算法)
  ├── PtyProcess (Win/Unix)
  └── IpcProtocol (JSON 消息协议)
```

### 4.3 IPC 通信模型

```
CLI Client                              GUI Server
    │                                       │
    │── connectToServer() ─────────────────►│
    │                                       │ onNewConnection()
    │◄── 日志流推送 (log_entry) ────────────│
    │◄── 状态推送 (status) ────────────────│
    │                                       │
    │── {"type":"connect","port":"COM3"} ──►│
    │◄── {"type":"response","ok":true} ─────│
    │                                       │
    │── {"type":"send_text","data":"AT"} ──►│
    │                                       │── 串口写入 ──► 设备
    │◄── 日志流: TX 标记可见 ◄──────────────│
```

- **传输层**: QLocalSocket / QLocalServer (Windows Named Pipe / Unix Domain Socket)
- **消息格式**: JSON，每行一个完整消息，以 `\n` 分隔
- **消息类型**: `log_entry` / `status_change` / `port_list` / `connect` / `disconnect` / `send_text` / `send_hex` / `get_logs` / `export_logs` / `clear_logs` / `pause` / `resume`
- **请求-响应**: 可选 `id` 字段匹配命令与响应

### 4.4 线程模型

| 组件 | 线程 | 同步方式 |
|------|------|----------|
| QML 渲染 | 主线程 | Qt 事件循环 |
| SerialEngine | 独立 QThread | QSerialPort moveToThread + 信号槽 |
| PtyProcess (Win) | 主线程 | QTimer 轮询管道 I/O |
| PtyProcess (Unix) | 主线程 | QSocketNotifier 异步 I/O |
| LogBuffer | 多线程读写 | QMutex |
| IPCServer | 主线程 | QLocalServer 事件驱动 |

---

## 5. 用户交互设计

> **完整的交互设计文档见:** `docs/交互设计文档.md`  
> 包含 L1 概念级（信息架构/导航模型）、L2 页面级（布局线框图/组件编排）、L3 组件级（全部状态/行为）、L4 微交互（动画/反馈/边界处理）。

### 设计原则

| 原则 | 描述 |
|------|------|
| **开发者原生** | 遵循 VS Code / iTerm2 / SecureCRT 交互范式 |
| **人机可感知** | CLI Agent 操作在 GUI 可见，反之亦然 |
| **即时反馈** | 所有操作 <100ms 给出视觉响应 |
| **容错优先** | 配置损坏降级默认，连接失败保留参数 |

### 信息架构

- **App Shell**: 左侧 Sidebar (220px, 可折叠) + 右侧 Tab 内容区 + 底部 StatusBar
- **导航**: 侧边栏会话列表管理已保存会话，TabBar 管理已打开会话，ConnectWizard 模态弹窗创建新会话

### 核心页面

| 页面 | 布局要点 |
|------|----------|
| 主窗口 | 侧边栏 (Logo+会话列表+设置) + StackLayout (动态加载 Tab QML) + 状态栏 |
| SerialTab | 工具栏(过滤+HEX/TS/暂停/清空/导出) + ListView(22px行高) + SendPanel(64px) |
| TerminalTab | 终端状态栏(28px,配色+字体) + TerminalView(80×24字符网格) |
| ConnectionWizard | 480×400 模态弹窗，三页切换(串口/终端/SSH)，表单验证 |

### 交互设计亮点

- CLI Agent 发送的命令在 GUI 日志中以 `[TX] 🤖 CLI` 标记显示
- 自动滚动 + 手动回溯悬浮 [↓ 新消息] 按钮
- 连接状态指示器呼吸脉冲动画
- 终端 5 套独立配色方案（不影响主界面）
- 发送按钮 300ms 绿色反馈脉冲

---

## 6. 接口规范

### 6.1 CLI 命令行完整参数

```
serial-monitor-cli [选项]

监听模式:
  -p, --port <PORT>         串口号
  -b, --baudrate <RATE>     波特率 (默认: 115200)
  -f, --filter <KEYWORD>    过滤关键字
  -o, --output <FILE>       保存日志到文件
  --hex                     HEX 显示模式
  --no-timestamp            禁用时间戳
  --cli                     交互 CLI 模式
  --json                    机器可读 JSON 输出
  --ipc <NAME>              IPC 服务名 (默认: serial_monitor_ipc)

操作命令:
  --connect <PORT>          连接指定串口
  --send <DATA>             发送文本数据（自动追加 CRLF）
  --send-file <FILE>        发送二进制文件
  --list                    列出可用串口
  --get-status              显示连接状态
  --get-logs <N>            获取最近 N 条日志

帮助:
  -h, --help                显示帮助
  -v, --version             显示版本
```

### 6.2 IPC 消息协议

**通用消息结构：**
```json
{
  "type": "消息类型",
  "id": "可选请求ID",
  "payload": { }
}
```

**核心消息类型：**

| type | 方向 | 说明 |
|------|------|------|
| `log_entry` | GUI → CLI | 实时日志推送 |
| `status_change` | GUI → CLI | 连接状态变更 |
| `port_list` | GUI → CLI | 可用串口列表 |
| `connect` | CLI → GUI | 请求连接串口 |
| `disconnect` | CLI → GUI | 请求断开 |
| `send_text` | CLI → GUI | 发送文本数据 |
| `send_hex` | CLI → GUI | 发送 HEX 数据 |
| `get_logs` | CLI → GUI | 获取日志 |
| `export_logs` | CLI → GUI | 导出日志到文件 |
| `clear_logs` | CLI → GUI | 清空日志 |
| `pause` / `resume` | CLI → GUI | 暂停/恢复日志输出 |
| `response` | 双向 | 命令响应 |
| `activate_window` | CLI → GUI | 激活 GUI 窗口 |

### 6.3 配置文件 JSON Schema

详见 [FR-6.2 保存内容](#fr-62-保存内容)。

### 6.4 日志导出格式

```json
{
  "export_time": "2026-06-10T10:23:45+08:00",
  "source": "COM3",
  "entries": [
    {
      "timestamp": "10:23:45.123",
      "level": "INFO",
      "text": "Device initialized",
      "raw_bytes": "44657669636520696E697469616C697A6564"
    }
  ]
}
```

---

## 7. 项目阶段规划

### Phase 0-5: 已完成的里程碑

| Phase | 内容 | 状态 |
|-------|------|------|
| Phase 0 | Qt6 环境搭建 (MSYS2 pacman 安装) | ✅ |
| Phase 1 | core 库 Qt6 适配编译 | ✅ |
| Phase 2 | QML 框架骨架 (main.qml + AppCore) | ✅ |
| Phase 3 | Tab 迁移 (SerialTab / TerminalTab) | ✅ |
| Phase 4 | 终端渲染增强 (ANSI / PTY / ConPTY) | ✅ |
| Phase 5 | CLI 回归验证 + DebugTab (调试已作废) | ✅ |

### Phase 6: 当前阶段 — 完善与发布

| 任务 | 优先级 | 说明 |
|------|--------|------|
| PRD 与设计文档完善 | P0 | 本文档的持续更新 |
| Qt5 遗留代码清理 | P1 | 移除 `main_window` / `log_view` / `send_panel` 等旧 Widgets |
| 平台兼容性验证 | P1 | Linux (PTY) + macOS 编译运行测试 |
| ConPTY Win10 19041+ 兼容 | P1 | 低版本 Windows 回退到 QProcess pipe |
| 单元测试建设 | P2 | SerialEngine / LogBuffer / AnsiParser 核心模块 |
| CI/CD 流水线 | P2 | GitHub Actions 自动化构建 + 测试 |
| 打包发布 | P2 | Inno Setup (Win) / AppImage (Linux) / DMG (macOS) |

---

## 8. 验收标准

### 8.1 功能验收

| 验收项 | 标准 | 验证方法 |
|--------|------|----------|
| GUI 启动 | 双击 exe 正常打开窗口，侧边栏 + Tab 区渲染正确 | 手动启动 |
| 串口连接 | 选择端口后点击连接，日志正常显示，状态栏更新 | 连接真实/虚拟串口 |
| 彩色日志 | TX 绿色、ERROR 红色、WARN 黄色、INFO 绿色 | 发送对应级别日志 |
| HEX 模式 | 切换 HEX 后显示十六进制 | 点击 HEX 按钮 |
| 过滤功能 | 输入关键字后仅显示匹配行 | 输入 ERROR |
| 数据发送 | 输入文本 → 发送 → TX 出现在日志中 | 手动发送 |
| SSH 连接 | 通过连接向导创建 SSH 会话，终端可交互 | 连接 Linux 主机 |
| 本地终端 | 创建终端会话，bash/powershell/cmd 可交互 | 启动终端 Tab |
| 会话持久化 | 创建会话 → 关闭程序 → 重启 → 会话列表保留 | 重启验证 |
| CLI 基础功能 | `--list` 列出端口，`--connect` 连接，`--send` 发送 | CLI 调用 |
| CLI 日志流 | `-p COM3` 实时接收日志 | CLI 监听 |
| 人机协同 | CLI 发送命令 → GUI TX 可见；GUI 操作 → CLI 日志可见 | 双开验证 |

### 8.2 性能验收

| 指标 | 标准 |
|------|------|
| 10000 行日志渲染 | ListView 不卡顿，< 16ms/帧 |
| 115200bps 持续接收 | 24h 不丢数据、不崩溃 |
| CLI 启动 | < 1s 连接到 IPC 服务 |
| 内存占用 | 空载 < 80MB，4 会话 < 200MB |

### 8.3 兼容性验收

| 平台 | 状态 |
|------|------|
| Windows 10/11 (MinGW64) | 已验证 ✅ |
| Windows 10/11 (MSVC) | 待验证 |
| Linux (Ubuntu 20.04+) | 待验证 |
| macOS 12+ | 待验证 |

---

## 附录 A: 术语表

| 术语 | 说明 |
|------|------|
| IPC | Inter-Process Communication，进程间通信 |
| PTY | Pseudo-Terminal，伪终端 |
| ConPTY | Windows Pseudo Console API |
| ANSI | ANSI 转义序列标准，控制终端颜色/光标 |
| CLI | Command Line Interface，命令行界面 |
| GUI | Graphical User Interface，图形用户界面 |
| TX | 发送 (Transmit) |
| RX | 接收 (Receive) |
| FIFO | First In First Out，先进先出缓冲 |

## 附录 B: 参考文档

| 文档 | 路径 |
|------|------|
| 项目架构与模块设计 | `docs/项目架构与模块设计文档.md` |
| IPC 通信协议 | `docs/IPC通信协议文档.md` |
| CLI 接口规范 | `docs/CLI接口规范文档.md` |
| 技术可行性分析 | `docs/技术可行性分析报告.md` |
| 项目构建与打包 | `project.md` |
