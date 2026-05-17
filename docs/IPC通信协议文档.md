# IPC 通信协议文档

> 项目: EmberIntel Serial Monitor v2.0  
> 日期: 2026-05-17  
> 版本: 1.0  

---

## 1. 概述

GUI 进程与 CLI 进程之间通过 **QLocalSocket / QLocalServer** 进行本地进程间通信（IPC）。

- **传输层**：Windows Named Pipe / Unix Domain Socket
- **消息格式**：JSON（每行一个完整的 JSON 对象，以 `\n` 分隔）
- **通信模式**：双向异步消息
- **服务端**：GUI 进程（`serial-monitor.exe`）
- **客户端**：CLI 进程（`serial-monitor-cli.exe`）

---

## 2. 连接管理

### 2.1 服务名称

```
服务名: "serial_monitor_ipc"
Windows 实际 Pipe 名: \\.\pipe\serial_monitor_ipc
Unix 实际 Socket 路径: /tmp/serial_monitor_ipc
```

### 2.2 连接流程

```
CLI Client                          GUI Server
    │                                    │
    │──── connectToServer() ────────────►│
    │                                    │ onNewConnection()
    │◄─── connected() signal ────────────│
    │                                    │
    │──── {"type":"hello","version":"2.0"} ──►│
    │◄── {"type":"welcome","server_version":"2.0"} ──│
    │                                    │
    │◄═══ 日志流开始推送 ═══════════════│
```

### 2.3 断开处理

| 场景 | CLI 行为 | GUI 行为 |
|------|----------|----------|
| CLI 主动断开 | 发送 `goodbye` 消息后关闭连接 | 清理客户端，不影响串口 |
| GUI 主动关闭 | 发送 `server_shutdown` 给所有客户端 | 等待客户端确认后关闭 |
| 网络异常断开 | 尝试重连 3 次（间隔 1s） | 检测断开，清理客户端 |

---

## 3. 消息格式

### 3.1 通用消息结构

所有消息均为单行 JSON，以 `\n` 结尾：

```json
{
  "type": "消息类型",
  "id": "可选的消息ID（用于请求-响应匹配）",
  "timestamp": "2026-05-17T10:23:45.123",
  "payload": { }
}
```

### 3.2 消息类型一览

#### CLI → GUI（客户端请求）

| type | 说明 | 需要响应 |
|------|------|----------|
| `hello` | 握手，声明协议版本 | 是 |
| `goodbye` | 断开连接 | 否 |
| `list_ports` | 列出可用串口 | 是 |
| `connect` | 连接指定串口 | 是 |
| `disconnect` | 断开指定串口 | 是 |
| `send_text` | 发送文本数据 | 是 |
| `send_hex` | 发送 HEX 数据 | 是 |
| `set_filter` | 设置筛选关键字 | 是 |
| `get_logs` | 获取最近日志 | 是 |
| `clear_logs` | 清空日志缓冲区 | 是 |
| `get_status` | 获取连接状态 | 是 |
| `set_display_mode` | 设置显示模式 | 是 |
| `export_logs` | 导出日志到文件 | 是 |

#### GUI → CLI（服务端推送/响应）

| type | 说明 |
|------|------|
| `welcome` | 握手响应 |
| `response` | 通用命令响应 |
| `log_entry` | 实时日志推送 |
| `status_update` | 连接状态变更 |
| `error` | 错误通知 |
| `server_shutdown` | 服务端即将关闭 |

---

## 4. 消息详细定义

### 4.1 握手

**CLI → GUI:**
```json
{
  "type": "hello",
  "payload": {
    "version": "2.0",
    "client_type": "cli"
  }
}
```

**GUI → CLI:**
```json
{
  "type": "welcome",
  "payload": {
    "server_version": "2.0",
    "connected_ports": ["COM5"],
    "config": { }
  }
}
```

### 4.2 列出串口

**CLI → GUI:**
```json
{
  "type": "list_ports",
  "id": "req_001"
}
```

**GUI → CLI:**
```json
{
  "type": "response",
  "id": "req_001",
  "payload": {
    "success": true,
    "ports": [
      {
        "name": "COM5",
        "description": "USB Serial Device (COM5)",
        "vid": "10C4",
        "pid": "EA60",
        "recommended": true
      },
      {
        "name": "COM6",
        "description": "JLink CDC UART Port (COM6)",
        "vid": "1366",
        "pid": "1015",
        "recommended": true
      }
    ]
  }
}
```

### 4.3 连接串口

**CLI → GUI:**
```json
{
  "type": "connect",
  "id": "req_002",
  "payload": {
    "port": "COM5",
    "baudrate": 115200,
    "databits": 8,
    "parity": "N",
    "stopbits": 1,
    "flowcontrol": "N"
  }
}
```

**GUI → CLI（成功）:**
```json
{
  "type": "response",
  "id": "req_002",
  "payload": {
    "success": true,
    "message": "已连接到 COM5 @ 115200bps"
  }
}
```

**GUI → CLI（失败）:**
```json
{
  "type": "response",
  "id": "req_002",
  "payload": {
    "success": false,
    "error": "PermissionError",
    "message": "拒绝访问 COM5，需要管理员权限"
  }
}
```

### 4.4 发送数据

**发送文本:**
```json
{
  "type": "send_text",
  "id": "req_003",
  "payload": {
    "port": "COM5",
    "data": "reboot",
    "append": "CRLF"
  }
}
```

**发送 HEX:**
```json
{
  "type": "send_hex",
  "id": "req_004",
  "payload": {
    "port": "COM5",
    "data": "01 02 03 04 FF"
  }
}
```

**响应:**
```json
{
  "type": "response",
  "id": "req_003",
  "payload": {
    "success": true,
    "bytes_sent": 8
  }
}
```

### 4.5 实时日志推送

**GUI → CLI（每条日志实时推送）:**
```json
{
  "type": "log_entry",
  "payload": {
    "port": "COM5",
    "timestamp": "10:23:45.123",
    "level": "INFO",
    "text": "Device initialized",
    "raw_hex": "5B494E464F5D2044657669636520696E697469616C697A6564"
  }
}
```

### 4.6 获取日志

**CLI → GUI:**
```json
{
  "type": "get_logs",
  "id": "req_005",
  "payload": {
    "port": "COM5",
    "count": 100,
    "filter": "ERROR"
  }
}
```

**GUI → CLI:**
```json
{
  "type": "response",
  "id": "req_005",
  "payload": {
    "success": true,
    "total_count": 10000,
    "returned_count": 3,
    "entries": [
      {
        "timestamp": "10:23:46.001",
        "level": "ERROR",
        "text": "Connection timeout!",
        "raw_hex": ""
      }
    ]
  }
}
```

### 4.7 状态推送

**GUI → CLI（连接状态变更时主动推送）:**
```json
{
  "type": "status_update",
  "payload": {
    "port": "COM5",
    "connected": true,
    "baudrate": 115200,
    "stats": {
      "bytes_received": 12345,
      "bytes_sent": 256,
      "uptime_seconds": 3600
    }
  }
}
```

### 4.8 导出日志

**CLI → GUI:**
```json
{
  "type": "export_logs",
  "id": "req_006",
  "payload": {
    "port": "COM5",
    "file_path": "C:\\Users\\user\\Desktop\\logs.json",
    "format": "json"
  }
}
```

**GUI → CLI:**
```json
{
  "type": "response",
  "id": "req_006",
  "payload": {
    "success": true,
    "file_path": "C:\\Users\\user\\Desktop\\logs.json",
    "entry_count": 10000
  }
}
```

### 4.9 错误通知

**GUI → CLI（非请求相关的错误）:**
```json
{
  "type": "error",
  "payload": {
    "code": "SERIAL_DISCONNECTED",
    "message": "串口 COM5 已断开",
    "port": "COM5"
  }
}
```

---

## 5. 错误码定义

| 错误码 | 说明 |
|--------|------|
| `SERIAL_DISCONNECTED` | 串口意外断开 |
| `SERIAL_PERMISSION_DENIED` | 串口权限不足 |
| `SERIAL_NOT_FOUND` | 串口不存在 |
| `SERIAL_ALREADY_OPEN` | 串口已被占用 |
| `INVALID_COMMAND` | 无效命令 |
| `INVALID_PARAMS` | 参数无效 |
| `PORT_NOT_CONNECTED` | 指定端口未连接 |
| `EXPORT_FAILED` | 导出失败 |
| `INTERNAL_ERROR` | 内部错误 |

---

## 6. 通信时序示例

### 6.1 典型 CLI 交互流程

```
CLI                              GUI
 │                                │
 │──── hello ────────────────────►│
 │◄─── welcome ──────────────────│
 │                                │
 │──── connect(COM5,115200) ─────►│
 │◄─── response(success) ────────│
 │                                │
 │◄─── log_entry ────────────────│  ← 实时日志开始推送
 │◄─── log_entry ────────────────│
 │◄─── log_entry ────────────────│
 │                                │
 │──── send_text("reboot") ──────►│
 │◄─── response(success) ────────│
 │                                │
 │◄─── log_entry ────────────────│
 │◄─── log_entry ────────────────│
 │                                │
 │──── get_logs(count=50) ───────►│
 │◄─── response(entries) ────────│
 │                                │
 │──── goodbye ──────────────────►│
 │                                │  ← CLI 关闭，GUI 继续运行
```

### 6.2 多 CLI 客户端场景

```
CLI-1                    GUI                     CLI-2
 │                        │                        │
 │── hello ──────────────►│                        │
 │◄─ welcome ────────────│                        │
 │                        │◄── hello ─────────────│
 │                        │── welcome ────────────►│
 │                        │                        │
 │◄─ log_entry ──────────│── log_entry ──────────►│  ← 广播
 │◄─ log_entry ──────────│── log_entry ──────────►│
 │                        │                        │
 │── goodbye ────────────►│                        │  ← CLI-1 关闭
 │                        │── log_entry ──────────►│  ← CLI-2 继续接收
```

---

## 7. 实现注意事项

1. **消息边界**：每条 JSON 消息以 `\n` 结尾，接收端按行分割
2. **消息大小**：单条消息不超过 64KB（日志条目通常 < 1KB）
3. **心跳机制**：暂不实现心跳，依赖 QLocalSocket 底层连接检测
4. **重连策略**：CLI 断开后自动重连 3 次，间隔 1s
5. **线程安全**：IPCServer 在主线程运行，通过信号槽与工作线程通信
6. **JSON 库**：服务端使用 QJsonDocument，客户端使用 rapidjson（CLI 不依赖 Qt）