#!/bin/bash
# start.sh - 尘智 EmberInterDebugTool 启动/停止脚本
# 用法: ./start.sh [start|stop|status] [--no-build]
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN_DIR="$PROJECT_DIR/build/bin"
PID_FILE="$PROJECT_DIR/.serial-monitor.pid"

export PATH="/mingw64/bin:$PATH"

is_running() {
    local pid="$1"
    [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

cmd="${1:-start}"
no_build=false

# 解析 --no-build 参数
for arg in "$@"; do
    case "$arg" in
        --no-build) no_build=true ;;
    esac
done

case "$cmd" in
    start)
        if [ -f "$PID_FILE" ]; then
            OLD_PID="$(cat "$PID_FILE")"
            if is_running "$OLD_PID"; then
                echo "serial-monitor 已在运行 (PID: $OLD_PID)"
                exit 0
            fi
            # PID 文件残留（进程已死），启动时会自动覆盖
            echo "清理残留 PID 文件 ($OLD_PID)"
            > "$PID_FILE" 2>/dev/null || true
        fi

        # 启动前先编译（除非指定了 --no-build）
        if [ "$no_build" = false ]; then
            echo "==> 编译最新代码..."
            "$SCRIPT_DIR/make.sh" all
        fi

        if [ ! -f "$BIN_DIR/serial-monitor.exe" ]; then
            echo "错误: 找不到 $BIN_DIR/serial-monitor.exe"
            echo "请先运行: ./scripts/make.sh all"
            exit 1
        fi

        echo "==> 启动 serial-monitor..."
        cd "$PROJECT_DIR"
        "$BIN_DIR/serial-monitor.exe" &
        NEW_PID=$!
        echo "$NEW_PID" > "$PID_FILE"
        echo "serial-monitor 已后台启动 (PID: $NEW_PID)"
        echo "关闭 GUI 窗口后进程会自动结束"
        echo "如需强制结束，请运行: ./scripts/start.sh stop"
        ;;

    stop)
        if [ ! -f "$PID_FILE" ]; then
            echo "未找到 PID 文件，尝试查找并结束所有 serial-monitor 进程..."
            ps -W | grep -i '/serial-monitor\.exe' | awk '{print $1}' | while read -r pid; do
                taskkill //F //PID "$pid" 2>/dev/null || true
            done
            exit 0
        fi

        PID="$(cat "$PID_FILE")"
        if is_running "$PID"; then
            echo "==> 正在停止 serial-monitor (PID: $PID)..."
            kill "$PID" 2>/dev/null || true
            sleep 1
            if is_running "$PID"; then
                taskkill //F //PID "$PID" 2>/dev/null || true
            fi
            echo "serial-monitor 已停止"
        else
            echo "进程 $PID 已不存在"
        fi
        > "$PID_FILE" 2>/dev/null || true
        ;;

    status)
        if [ -f "$PID_FILE" ] && is_running "$(cat "$PID_FILE")"; then
            echo "serial-monitor 运行中 (PID: $(cat "$PID_FILE"))"
        else
            echo "serial-monitor 未运行"
            > "$PID_FILE" 2>/dev/null || true
        fi
        ;;

    *)
        echo "用法: $0 {start|stop|status} [--no-build]"
        exit 1
        ;;
esac
