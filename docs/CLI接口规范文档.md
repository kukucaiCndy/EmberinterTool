# CLI 接口规范文档

> 项目: EmberInterDebugTool v1.2.0  
> 日期: 2026-06-21  
> 版本: 1.1  

---

## 1. 概述

`serial-monitor-cli.exe` 是一个轻量级命令行工具，通过 IPC 与 GUI 服务进程通信，供 AI Agent 和自动化脚本使用。

**核心设计原则：**
- CLI 不直接操作串口，所有串口操作委托给 GUI 进程
- CLI 可随时启动/关闭，不影响串口数据接收
- 输出格式对脚本友好（可选的机器可读模式）

---

## 2. 命令行参数

```
serial-monitor-cli.exe [选项]

选项:
  -p, --port <PORT>         串口号 (如 COM5, /dev/ttyUSB0)
  -b, --baudrate <RATE>     波特率 (默认: 115200)
  -f, --filter <KEYWORD>    筛选关键字
  -o, --output <FILE>       保存日志到文件
  --hex                     HEX 显示模式
  --no-timestamp            禁用时间戳
  --clear                   启动时清空日志
  --list                    列出可用串口
  --cli                     交互式 CLI 模式
  --json                    机器可读 JSON 输出模式
  --ipc <NAME>              IPC 服务名 (默认: serial_monitor_ipc)
  --send <DATA>             发送文本数据后退出
  --send-hex <HEX>          发送 HEX 数据后退出
  --send-file <FILE>        发送二进制文件后退出
  --connect <PORT>          连接串口
  --get-logs <COUNT>        获取最近 N 条日志
  --get-status              获取连接状态
  --pause                   暂停日志输出
  --resume                  恢复日志输出
  --activate-window         激活 GUI 窗口
  -h, --help                显示帮助
  -v, --version             显示版本
```

---

## 3. 运行模式

### 3.1 单次命令模式（非交互）

执行单个操作后立即退出，适合脚本调用：

```bash
# 列出可用串口
serial-monitor-cli.exe --list

# 连接串口并持续显示日志（Ctrl+C 退出）
serial-monitor-cli.exe -p COM5 -b 115200

# 连接串口，筛选 ERROR，保存到文件
serial-monitor-cli.exe -p COM5 -f ERROR -o error.log

# 发送命令后退出
serial-monitor-cli.exe -p COM5 --send "reboot"

# JSON 输出模式（机器可读）
serial-monitor-cli.exe --list --json
```

### 3.2 交互模式（--cli）

进入交互式命令行，支持实时命令输入：

```bash
serial-monitor-cli.exe -p COM5 --cli
```

---

## 4. 交互命令

### 4.1 命令列表

| 命令 | 别名 | 说明 |
|------|------|------|
| `help` | `h`, `?` | 显示帮助 |
| `quit` | `q`, `exit` | 退出 CLI（不影响 GUI） |
| `clear` | `c` | 清空日志缓冲区 |
| `status` | `s` | 显示连接状态 |
| `list` | `ls` | 列出可用串口 |
| `send <text>` | | 发送文本数据 |
| `sendhex <hex>` | | 发送 HEX 数据 |
| `sendfile <file>` | | 发送二进制文件 |
| `filter <keyword>` | | 设置筛选关键字 (空=取消) |
| `hex` | | 切换 HEX 显示模式 |
| `text` | | 切换文本显示模式 |
| `timestamp` | `ts` | 切换时间戳显示 |
| `export <file>` | | 导出日志到 JSON 文件 |
| `connect <port> [baud]` | | 连接串口 |
| `disconnect` | `disc` | 断开当前串口 |

### 4.2 命令示例

```
> connect COM5 115200
[SYSTEM] 已连接到 COM5 @ 115200bps

> send reboot
>>> 发送: reboot

> sendhex 01 02 03 FF
>>> 发送 HEX: 01 02 03 FF

> filter ERROR
[SYSTEM] 筛选关键字: ERROR

> status
[SYSTEM] 端口: COM5 | 状态: 已连接 | 115200-8-N-1
[SYSTEM] 接收: 12.5KB | 发送: 256B | 运行时间: 1h23m

> export C:\logs\debug.json
[SYSTEM] 已导出 10000 条日志到 C:\logs\debug.json

> quit
[SYSTEM] CLI 已断开（GUI 服务继续运行）
```

---

## 5. 输出格式

### 5.1 人类可读模式（默认）

```
[10:23:45.123] [INFO]  Device initialized
[10:23:45.234] [DEBUG] BLE stack started
[10:23:46.001] [ERROR] Connection timeout!
```

带 ANSI 颜色代码（可重定向到文件时自动禁用颜色）。

### 5.2 HEX 模式

```
[10:23:45.123] 00000000  48 65 6C 6C 6F 20 57 6F  72 6C 64 21 0D 0A        |Hello World!..|
[10:23:45.234] 0000000E  5B 49 4E 46 4F 5D 20 44  65 76 69 63 65 20 69 6E  |[INFO] Device in|
[10:23:45.234] 0000001E  69 74 69 61 6C 69 7A 65  64 0D 0A                 |itialized..|
```

### 5.3 JSON 模式（--json）

每行一个 JSON 对象，适合脚本解析：

```json
{"type":"log","port":"COM5","ts":"10:23:45.123","level":"INFO","text":"Device initialized"}
{"type":"log","port":"COM5","ts":"10:23:45.234","level":"DEBUG","text":"BLE stack started"}
{"type":"log","port":"COM5","ts":"10:23:46.001","level":"ERROR","text":"Connection timeout!"}
{"type":"status","port":"COM5","connected":true,"rx_bytes":12345,"tx_bytes":256}
```

---

## 6. 退出码

| 退出码 | 说明 |
|--------|------|
| 0 | 正常退出 |
| 1 | 参数错误 |
| 2 | GUI 服务未运行 |
| 3 | IPC 连接失败 |
| 4 | 串口连接失败 |
| 5 | 未知错误 |

---

## 7. AI Agent 使用场景

### 7.1 固件烧录后验证

```bash
# AI Agent 执行流程
# 1. 检查 GUI 服务是否运行
serial-monitor-cli.exe --list
# 退出码 2 → 提示用户启动 GUI

# 2. 清空旧日志，连接串口
serial-monitor-cli.exe -p COM5 --clear

# 3. 发送重启命令
serial-monitor-cli.exe -p COM5 --send "reboot"

# 4. 等待 3 秒后获取日志
timeout /t 3
serial-monitor-cli.exe -p COM5 --get-logs 50 --json > output.json

# 5. 分析 output.json 中的日志
```

### 7.2 持续监控

```bash
# 后台持续监控并保存日志
serial-monitor-cli.exe -p COM5 -f ERROR -o error.log &

# 随时获取最近日志
serial-monitor-cli.exe --get-logs 100 --json
```

### 7.3 多串口监控

```bash
# 终端1: 监控 COM5
serial-monitor-cli.exe -p COM5 --cli

# 终端2: 监控 COM6
serial-monitor-cli.exe -p COM6 --cli
```

---

## 8. 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `SERIAL_MONITOR_IPC` | IPC 服务名 | `serial_monitor_ipc` |
| `SERIAL_MONITOR_CONFIG` | 配置文件路径 | 系统默认路径 |
| `NO_COLOR` | 设为 1 禁用颜色输出 | 未设置 |

---

## 9. 与 Python 版本 CLI 的差异

| 功能 | Python v1.0 | C++ v2.0 CLI |
|------|-------------|--------------|
| 串口操作 | 直接操作 QSerialPort | 通过 IPC 委托 GUI |
| GUI 依赖 | 无 | 需要 GUI 进程运行 |
| 关闭影响 | 串口断开 | 不影响串口 |
| 多客户端 | 不支持 | 支持多个 CLI 同时连接 |
| HEX 模式 | 不支持 | 支持 |
| JSON 输出 | 不支持 | 支持 `--json` |
| 日志导出 | 仅 TXT | JSON 结构化 |
| 配置持久化 | 无 | 通过 GUI 共享配置 |