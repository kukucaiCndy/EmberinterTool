#!/bin/bash
# make.sh - 尘智 EmberInterDebugTool 构建脚本
# 用法: ./make.sh [all|clean|rebuild]
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

# 确保 MSYS2 MinGW64 工具链在 PATH 中
export PATH="/mingw64/bin:$PATH"

cmd="${1:-all}"

case "$cmd" in
    all)
        echo "==> 配置构建..."
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug

        echo "==> 编译..."
        mingw32-make -j"$(nproc)"

        echo "==> 构建完成: $BUILD_DIR/bin/serial-monitor.exe"
        ;;

    clean)
        if [ -d "$BUILD_DIR" ]; then
            echo "==> 清理构建产物..."
            cd "$BUILD_DIR"
            mingw32-make clean 2>/dev/null || true
            rm -rf CMakeCache.txt CMakeFiles Makefile cmake_install.cmake \
                   compile_commands.json .cmake *.dir
        fi
        echo "==> 清理完成"
        ;;

    rebuild)
        "$0" clean
        "$0" all
        ;;

    *)
        echo "用法: $0 {all|clean|rebuild}"
        exit 1
        ;;
esac
