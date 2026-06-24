#!/bin/bash
# v1.3.0 Release 打包脚本 (使用 windeployqt6 自动收集依赖)
# 在 MSYS2 bash 终端内执行
set -e

PROJECT_DIR="/f/work/software/serial-monitor"
BUILD_DIR="$PROJECT_DIR/build-release"
DEPLOY_DIR="$PROJECT_DIR/deploy/emberInter"
DIST_DIR="$PROJECT_DIR/dist"
MINGW_DIR="/mingw64"

echo "================================================"
echo "  EmberInterDebugTool v1.3.1 Release 打包"
echo "================================================"
echo ""

# 1. Release 模式构建
echo "=== [1/6] Release 模式构建 ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
echo "--- 开始编译 ---"
mingw32-make -j$(nproc) 2>&1 | tail -10
cd "$PROJECT_DIR"

echo ""
echo "=== [2/6] 创建部署目录 ==="
rm -rf "$DEPLOY_DIR" "$DIST_DIR"
mkdir -p "$DEPLOY_DIR"
mkdir -p "$DIST_DIR"

echo "=== [3/6] 复制可执行文件 ==="
cp "$BUILD_DIR/bin/serial-monitor.exe" "$DEPLOY_DIR/"
cp "$BUILD_DIR/bin/serial-monitor-cli.exe" "$DEPLOY_DIR/"

echo "=== [4/6] 使用 windeployqt6 收集 Qt 依赖 ==="
# windeployqt6 会自动复制 Qt DLL、插件、QML 模块等
cd "$DEPLOY_DIR"
"$MINGW_DIR/bin/windeployqt6.exe" serial-monitor.exe --qmldir "$PROJECT_DIR/src/gui/qml" 2>&1 | tail -20
echo "--- CLI 依赖 ---"
"$MINGW_DIR/bin/windeployqt6.exe" serial-monitor-cli.exe 2>&1 | tail -10
cd "$PROJECT_DIR"

echo "=== [5/6] 复制运行时 DLL 和资源 ==="
# 复制 MinGW 运行时 DLL (windeployqt 可能遗漏)
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
    if [ -f "$MINGW_DIR/bin/$dll" ] && [ ! -f "$DEPLOY_DIR/$dll" ]; then
        cp "$MINGW_DIR/bin/$dll" "$DEPLOY_DIR/"
        echo "  复制: $dll"
    fi
done

# 复制 spdlog DLL
if [ -f "$MINGW_DIR/bin/libspdlog-1.15.dll" ] && [ ! -f "$DEPLOY_DIR/libspdlog-1.15.dll" ]; then
    cp "$MINGW_DIR/bin/libspdlog-1.15.dll" "$DEPLOY_DIR/"
    echo "  复制: libspdlog-1.15.dll"
fi

# 复制 fmt DLL (spdlog 依赖)
for fmt_dll in libfmt.dll; do
    if [ -f "$MINGW_DIR/bin/$fmt_dll" ] && [ ! -f "$DEPLOY_DIR/$fmt_dll" ]; then
        cp "$MINGW_DIR/bin/$fmt_dll" "$DEPLOY_DIR/"
        echo "  复制: $fmt_dll"
    fi
done

# 复制图标
mkdir -p "$DEPLOY_DIR/icons"
cp "$PROJECT_DIR/resources/icons/logo.png" "$DEPLOY_DIR/icons/" 2>/dev/null || true
cp "$PROJECT_DIR/resources/icons/app.ico" "$DEPLOY_DIR/icons/" && echo "  复制: icons/app.ico"

# 启动脚本
cat > "$DEPLOY_DIR/emberInter.bat" << 'BAT'
@echo off
chcp 65001 > nul
set PATH=%~dp0;%~dp0platforms;%PATH%
start "" "%~dp0serial-monitor.exe"
BAT

cat > "$DEPLOY_DIR/emberInter-cli.bat" << 'BAT'
@echo off
chcp 65001 > nul
set PATH=%~dp0;%~dp0platforms;%PATH%
"%~dp0serial-monitor-cli.exe" %*
BAT

echo ""
echo "=== [6/6] 统计部署结果 ==="
echo "目录: $DEPLOY_DIR"
echo ""
echo "可执行文件:"
ls -lh "$DEPLOY_DIR/serial-monitor.exe" "$DEPLOY_DIR/serial-monitor-cli.exe"
echo ""
DLL_COUNT=$(ls "$DEPLOY_DIR"/*.dll 2>/dev/null | wc -l)
echo "DLL 数量: $DLL_COUNT"
echo ""
echo "子目录:"
for d in "$DEPLOY_DIR"/*/; do
    [ -d "$d" ] && echo "  $(basename $d)/ ($(ls "$d" | wc -l) 文件)"
done
echo ""
echo "================================================"
echo "  部署目录准备完成"
echo "================================================"
echo ""
echo "=== 下一步: 执行 Inno Setup 编译安装包 ==="
