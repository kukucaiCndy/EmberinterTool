#!/bin/bash
# build-and-run.sh - 构建并启动尘智 EmberInterDebugTool (Qt6)
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "==> 配置构建..."
mkdir -p build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug

echo "==> 编译..."
cmake --build . -j$(nproc)

echo "==> 启动应用..."

# 添加 MSYS2 Qt6 / mingw64 库路径
export PATH="/mingw64/bin:$PATH"

cd bin
./serial-monitor.exe
