# Serial Monitor - 嵌入式串口调试工具

一个轻量级的 CLI 串口监控工具，支持人机协同调试。

## 特性

- 🚀 **自动提权** - Windows 下自动申请管理员权限
- 🎨 **彩色日志** - 按日志级别着色（ERROR红、WARN黄、INFO绿等）
- 🔄 **自动重连** - 串口断开后自动重试
- 💾 **大缓冲区** - 10000行日志缓存，防止丢失关键信息
- ⏱️ **时间戳** - 自动添加时间戳
- 🔍 **实时筛选** - 支持关键字过滤

## 快速开始

### 安装

```bash
pip install pyserial
```

### 使用

```bash
# 基本使用
python serial_monitor.py -p COM5

# 清空日志并启动（下载固件后使用）
python serial_monitor.py -p COM5 --clear

# 筛选 ERROR 日志
python serial_monitor.py -p COM5 -f ERROR

# 保存到文件
python serial_monitor.py -p COM5 -o log.txt
```

### 交互命令

运行中可输入：
- `c` / `clear` - 清空日志
- `f <关键字>` - 设置筛选
- `s` / `status` - 显示状态
- `q` / `quit` - 退出
- `help` - 显示帮助

## 系统要求

- Python 3.7+
- Windows/Linux/macOS
- pyserial 库

## 许可证

MIT License
