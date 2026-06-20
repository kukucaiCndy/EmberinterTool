#!/bin/bash
# run.sh - 启动尘智 EmberInterDebugTool (Qt6)
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 添加 MSYS2 Qt6 / mingw64 库路径
export PATH="/mingw64/bin:$PATH"

cd "$SCRIPT_DIR/build/bin"
./serial-monitor.exe
