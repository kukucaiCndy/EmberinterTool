#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
EmberIntel Serial Monitor
人机协同串口调试工具 - CLI 版本

功能：
- AI 通过 CLI 控制：启动、配置、清空、筛选
- 人通过终端查看：彩色日志、时间戳、自动滚动
- 自动连接和断线重连
- 大缓冲区（10000行）防止日志丢失
- 支持 UTF-8 编码

使用方法：
  # 启动并连接（AI 使用）
  python serial_monitor.py -p COM5
  
  # 清空日志并连接（AI 下载固件后使用）
  python serial_monitor.py -p COM5 --clear
  
  # 筛选 ERROR 日志
  python serial_monitor.py -p COM5 -f ERROR
  
  # 保存到文件
  python serial_monitor.py -p COM5 -o log.txt
  
  # 列出串口
  python serial_monitor.py --list
  
  # CLI 交互模式
  python serial_monitor.py -p COM5 --cli
"""

import sys
import os

# 设置 Windows 控制台为 UTF-8 编码
if sys.platform == 'win32':
    try:
        import ctypes
        ctypes.windll.kernel32.SetConsoleOutputCP(65001)
        ctypes.windll.kernel32.SetConsoleCP(65001)
    except:
        pass

import serial
import serial.tools.list_ports
import threading
import datetime
import argparse
import time
from collections import deque
from pathlib import Path

# 颜色代码
class Colors:
    RESET = '\033[0m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GRAY = '\033[90m'
    WHITE = '\033[97m'

# 日志级别颜色映射
LOG_COLORS = {
    'ERROR': Colors.RED,
    'WARN': Colors.YELLOW,
    'WARNING': Colors.YELLOW,
    'INFO': Colors.GREEN,
    'DEBUG': Colors.CYAN,
    'TRACE': Colors.GRAY,
}

class SerialMonitor:
    """串口监控核心类"""
    
    # 大缓冲区：10000行日志
    LOG_BUFFER_SIZE = 10000
    
    def __init__(self, port=None, baudrate=115200, filter_str=None,
                 save_file=None, timestamp=True, auto_reconnect=True):
        self.port = port
        self.baudrate = baudrate
        self.filter_str = filter_str
        self.save_file = save_file
        self.timestamp = timestamp
        self.auto_reconnect = auto_reconnect
        
        self.serial_conn = None
        self.is_running = False
        self.is_connected = False
        self.read_thread = None
        self.buffer = ""
        
        # 日志缓冲区（双端队列，固定大小）
        self.log_buffer = deque(maxlen=self.LOG_BUFFER_SIZE)
        self.log_lock = threading.Lock()
        
        # 输出文件
        self.log_fp = None
        
        # 清空标志
        self.clear_requested = False
        
    def list_ports(self):
        """列出可用串口"""
        ports = serial.tools.list_ports.comports()
        if not ports:
            print("未找到可用串口")
            return []
        
        print("\n可用串口列表:")
        print("-" * 60)
        for i, p in enumerate(ports, 1):
            print(f"{i}. {p.device}")
            print(f"   描述: {p.description}")
            if 'USB' in p.description.upper() or 'nRF' in p.description.upper():
                print(f"   [推荐]")
        print()
        return ports
    
    def auto_select_port(self):
        """自动选择串口"""
        ports = serial.tools.list_ports.comports()
        if not ports:
            return None
        
        # 优先选择 USB/nRF 串口
        for p in ports:
            desc = p.description.upper()
            if 'USB' in desc or 'NRF' in desc or 'JLINK' in desc:
                return p.device
        
        return ports[0].device
    
    def clear_logs(self):
        """清空日志缓冲区"""
        with self.log_lock:
            self.log_buffer.clear()
            self.buffer = ""
        
        # 清屏
        os.system('cls' if os.name == 'nt' else 'clear')
        
        print(self.colorize("=" * 70, Colors.CYAN))
        print(self.colorize("  EmberIntel Serial Monitor", Colors.CYAN))
        print(self.colorize(f"  Port: {self.port} @ {self.baudrate}bps", Colors.GRAY))
        print(self.colorize("=" * 70, Colors.CYAN))
        print()
        print(self.colorize("[SYSTEM] 日志已清空，准备接收新日志", Colors.GREEN))
        print()
    
    def colorize(self, text, color):
        """添加颜色"""
        return f"{color}{text}{Colors.RESET}"
    
    def get_log_level(self, line):
        """获取日志级别"""
        upper_line = line.upper()
        for level in LOG_COLORS.keys():
            if level in upper_line:
                return level
        return None
    
    def colorize_line(self, line):
        """为日志行添加颜色"""
        level = self.get_log_level(line)
        if level and level in LOG_COLORS:
            return self.colorize(line, LOG_COLORS[level])
        return line
    
    def add_log(self, line):
        """添加日志到缓冲区并输出"""
        if not line:
            return
        
        # 过滤
        if self.filter_str and self.filter_str not in line:
            return
        
        # 添加时间戳
        if self.timestamp:
            ts = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
            display_line = f"[{ts}] {line}"
        else:
            display_line = line
        
        # 添加到缓冲区
        with self.log_lock:
            self.log_buffer.append(display_line)
        
        # 终端输出（带颜色）
        print(self.colorize_line(display_line))
        
        # 保存到文件
        if self.log_fp:
            self.log_fp.write(display_line + '\n')
            self.log_fp.flush()
    
    def read_loop(self):
        """读取串口数据的线程"""
        reconnect_delay = 1
        max_reconnect_delay = 5
        
        while self.is_running:
            # 检查清空请求
            if self.clear_requested:
                self.clear_logs()
                self.clear_requested = False
            
            # 连接串口
            if not self.is_connected:
                if not self._connect():
                    if self.auto_reconnect:
                        time.sleep(reconnect_delay)
                        reconnect_delay = min(reconnect_delay + 1, max_reconnect_delay)
                        continue
                    else:
                        break
            
            # 读取数据
            try:
                if self.serial_conn and self.serial_conn.in_waiting > 0:
                    data = self.serial_conn.read(self.serial_conn.in_waiting)
                    text = data.decode('utf-8', errors='replace')
                    self.buffer += text
                    
                    # 按行处理
                    while '\n' in self.buffer:
                        line, self.buffer = self.buffer.split('\n', 1)
                        line = line.rstrip('\r')
                        self.add_log(line)
                        
            except serial.SerialException as e:
                self._disconnect()
                self.add_log(f"[SYSTEM] 串口断开: {e}")
                if self.auto_reconnect:
                    self.add_log(f"[SYSTEM] {reconnect_delay}秒后重连...")
                    time.sleep(reconnect_delay)
                else:
                    break
            except Exception as e:
                self.add_log(f"[SYSTEM] 错误: {e}")
            
            reconnect_delay = 1
    
    def _connect(self):
        """连接串口"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1
            )
            self.is_connected = True
            self.add_log(f"[SYSTEM] 已连接到 {self.port} @ {self.baudrate}bps")
            return True
        except serial.SerialException as e:
            error_msg = str(e)
            self.add_log(f"[SYSTEM] 连接 {self.port} 失败: {e}")
            
            # 检查是否是权限错误
            if "PermissionError" in error_msg or "拒绝访问" in error_msg or "Access is denied" in error_msg:
                self._show_permission_error()
            
            return False
    
    def _show_permission_error(self):
        """显示权限错误提示并自动重启"""
        print()
        print(self.colorize("=" * 70, Colors.RED))
        print(self.colorize("  ⚠️  需要管理员权限访问串口", Colors.RED))
        print(self.colorize("=" * 70, Colors.RED))
        print()
        print("  正在尝试以管理员身份自动重启...")
        print()
        
        # 自动以管理员身份重启
        self._restart_as_admin()
    
    def _restart_as_admin(self):
        """以管理员身份重启程序"""
        import subprocess
        import sys
        import tempfile
        
        # 构建命令行参数
        args = [sys.executable, __file__]
        if self.port:
            args.extend(['-p', self.port])
        if self.baudrate != 115200:
            args.extend(['-b', str(self.baudrate)])
        if self.filter_str:
            args.extend(['-f', self.filter_str])
        if self.save_file:
            args.extend(['-o', self.save_file])
        if not self.timestamp:
            args.append('--no-timestamp')
        if not self.auto_reconnect:
            args.append('--no-reconnect')
        
        # 创建临时批处理文件来启动管理员权限
        try:
            # 创建临时 bat 文件
            bat_content = '@echo off\n'
            bat_content += 'chcp 65001 >nul\n'
            bat_content += f'cd /d "{os.path.dirname(__file__)}"\n'
            bat_content += ' '.join([f'"{arg}"' for arg in args]) + '\n'
            bat_content += 'pause\n'
            
            with tempfile.NamedTemporaryFile(mode='w', suffix='.bat', delete=False, encoding='utf-8') as f:
                f.write(bat_content)
                bat_file = f.name
            
            print(f"  创建启动脚本: {bat_file}")
            print()
            
            # 使用 PowerShell 启动 bat 文件（管理员权限）
            ps_cmd = f'Start-Process "cmd" -ArgumentList "/k","{bat_file}" -Verb RunAs'
            
            print(f"  正在请求管理员权限...")
            print()
            
            # 启动新进程
            subprocess.Popen(['powershell', '-Command', ps_cmd], shell=True)
            
            # 退出当前进程
            print(self.colorize("  当前程序将在 2 秒后退出，请在新窗口中查看日志...", Colors.YELLOW))
            time.sleep(2)
            sys.exit(0)
            
        except Exception as e:
            print(self.colorize(f"  自动重启失败: {e}", Colors.RED))
            print()
            print("  请手动以管理员身份运行：")
            print(f"  python tools/serial_monitor.py -p {self.port}")
            print()
    
    def _disconnect(self):
        """断开串口"""
        self.is_connected = False
        if self.serial_conn and self.serial_conn.is_open:
            try:
                self.serial_conn.close()
            except:
                pass
    
    def start(self):
        """启动监控"""
        # 打开输出文件
        if self.save_file:
            self.log_fp = open(self.save_file, 'a', encoding='utf-8')
        
        # 自动选择串口
        if not self.port:
            self.port = self.auto_select_port()
            if not self.port:
                print(self.colorize("[SYSTEM] 未找到可用串口", Colors.RED))
                return False
        
        # 清屏并显示标题
        os.system('cls' if os.name == 'nt' else 'clear')
        print(self.colorize("=" * 70, Colors.CYAN))
        print(self.colorize("  EmberIntel Serial Monitor", Colors.CYAN))
        print(self.colorize(f"  Port: {self.port} @ {self.baudrate}bps", Colors.GRAY))
        print(self.colorize("=" * 70, Colors.CYAN))
        print()
        
        self.is_running = True
        
        # 启动读取线程
        self.read_thread = threading.Thread(target=self.read_loop, daemon=True)
        self.read_thread.start()
        
        return True
    
    def stop(self):
        """停止监控"""
        self.is_running = False
        self._disconnect()
        
        if self.read_thread:
            self.read_thread.join(timeout=2)
        
        if self.log_fp:
            self.log_fp.close()
    
    def request_clear(self):
        """请求清空日志（线程安全）"""
        self.clear_requested = True
    
    def send_data(self, data):
        """发送数据到串口"""
        if self.serial_conn and self.is_connected:
            try:
                self.serial_conn.write(data.encode() + b'\r\n')
                self.add_log(f">>> 发送: {data}")
                return True
            except:
                return False
        return False
    
    def get_recent_logs(self, count=100):
        """获取最近的日志（AI 可以调用获取日志内容）"""
        with self.log_lock:
            return list(self.log_buffer)[-count:]


def main():
    parser = argparse.ArgumentParser(
        description='EmberIntel Serial Monitor - 人机协同串口调试工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s -p COM5                    # 启动并连接
  %(prog)s -p COM5 --clear            # 清空日志并连接（下载固件后使用）
  %(prog)s -p COM5 -f ERROR           # 只显示 ERROR 日志
  %(prog)s -p COM5 -o log.txt         # 保存到文件
  %(prog)s --list                     # 列出串口
        """
    )
    
    parser.add_argument('-p', '--port', help='串口号 (如 COM5)')
    parser.add_argument('-b', '--baudrate', type=int, default=115200,
                       help='波特率 (默认: 115200)')
    parser.add_argument('-f', '--filter', help='筛选关键字')
    parser.add_argument('-o', '--output', help='保存日志到文件')
    parser.add_argument('--no-timestamp', action='store_true',
                       help='禁用时间戳')
    parser.add_argument('--no-reconnect', action='store_true',
                       help='禁用自动重连')
    parser.add_argument('--clear', action='store_true',
                       help='清空日志缓冲区（下载固件后使用）')
    parser.add_argument('--list', action='store_true',
                       help='列出可用串口')
    
    args = parser.parse_args()
    
    # 列出串口
    if args.list:
        monitor = SerialMonitor()
        monitor.list_ports()
        return 0
    
    # 检查串口参数
    if not args.port:
        print("错误: 请指定串口号 (-p COM5)")
        print("使用 --list 查看可用串口")
        return 1
    
    # 创建监控器
    monitor = SerialMonitor(
        port=args.port,
        baudrate=args.baudrate,
        filter_str=args.filter,
        save_file=args.output,
        timestamp=not args.no_timestamp,
        auto_reconnect=not args.no_reconnect
    )
    
    # 启动监控
    if not monitor.start():
        return 1
    
    # 如果需要清空，等待连接成功后清空
    if args.clear:
        time.sleep(0.5)  # 等待连接
        monitor.request_clear()
    
    print(monitor.colorize("[SYSTEM] 按 Ctrl+C 停止，输入 'help' 查看命令", Colors.GREEN))
    print()
    
    # 交互式命令循环
    try:
        while monitor.is_running:
            try:
                # 非阻塞输入
                import select
                if sys.platform == 'win32':
                    # Windows 不支持 select on stdin，使用简单延迟
                    time.sleep(0.1)
                    # 检查是否有输入（简单方式）
                    import msvcrt
                    if msvcrt.kbhit():
                        cmd = input().strip()
                        _handle_command(cmd, monitor)
                else:
                    # Unix/Linux/Mac
                    if select.select([sys.stdin], [], [], 0.1)[0]:
                        cmd = input().strip()
                        _handle_command(cmd, monitor)
            except ImportError:
                # 如果无法使用高级输入，使用简单循环
                time.sleep(0.1)
                        
    except KeyboardInterrupt:
        print()
        print(monitor.colorize("[SYSTEM] 用户中断", Colors.YELLOW))
    
    monitor.stop()
    print(monitor.colorize("[SYSTEM] 已断开", Colors.GRAY))
    return 0


def _handle_command(cmd, monitor):
    """处理交互命令"""
    if not cmd:
        return
    
    cmd_lower = cmd.lower()
    
    if cmd_lower in ['q', 'quit', 'exit']:
        monitor.is_running = False
    elif cmd_lower in ['c', 'clear']:
        monitor.request_clear()
    elif cmd_lower in ['h', 'help', '?']:
        _print_help(monitor)
    elif cmd_lower in ['s', 'status']:
        status = "已连接" if monitor.is_connected else "未连接"
        monitor.add_log(f"[SYSTEM] 状态: {status}, 端口: {monitor.port}")
    elif cmd_lower.startswith('send '):
        data = cmd[5:]
        monitor.send_data(data)
    elif cmd_lower.startswith('filter '):
        monitor.filter_str = cmd[7:] or None
        monitor.add_log(f"[SYSTEM] 筛选关键字: {monitor.filter_str or '无'}")
    else:
        # 直接发送输入的内容
        monitor.send_data(cmd)


def _print_help(monitor):
    """打印帮助信息"""
    help_text = """
命令:
  q/quit/exit    - 退出程序
  c/clear        - 清空日志
  s/status       - 显示状态
  h/help/?       - 显示帮助
  send <数据>    - 发送数据到串口
  filter <关键字> - 设置筛选关键字
  <任意文本>     - 直接发送到串口
"""
    print(help_text)


if __name__ == '__main__':
    sys.exit(main())
