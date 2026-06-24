#!/bin/bash
# EmberInterDebugTool v1.3.1 一键打包脚本 (bash)
# 用法: bash deploy/pack_v1.3.1.sh
# 前提: build-release 目录已完成 Release 编译
set -e

PROJECT_DIR="/f/work/software/serial-monitor"
BUILD_DIR="$PROJECT_DIR/build-release"
DEPLOY_DIR="$PROJECT_DIR/deploy/emberInter"
DIST_DIR="$PROJECT_DIR/dist"
MINGW_DIR="/mingw64"
ISCC="/c/Program Files (x86)/Inno Setup 6/ISCC.exe"

echo "================================================"
echo "  EmberInterDebugTool v1.3.1 打包"
echo "================================================"
echo ""

# 检查编译产物
if [ ! -f "$BUILD_DIR/bin/serial-monitor.exe" ]; then
    echo "错误: 未找到 serial-monitor.exe, 请先执行 Release 编译"
    exit 1
fi

# 1. 清理并创建部署目录
echo "[1/4] 准备部署目录..."
mkdir -p "$DEPLOY_DIR" "$DIST_DIR"
# 清理旧文件 (不用 rm -rf, 兼容安全策略)
find "$DEPLOY_DIR" -mindepth 1 -delete 2>/dev/null || true

# 2. 复制可执行文件
echo "[2/4] 复制可执行文件..."
cp "$BUILD_DIR/bin/serial-monitor.exe" "$DEPLOY_DIR/"
cp "$BUILD_DIR/bin/serial-monitor-cli.exe" "$DEPLOY_DIR/"
echo "  serial-monitor.exe ($(ls -lh "$BUILD_DIR/bin/serial-monitor.exe" | awk '{print $5}'))"
echo "  serial-monitor-cli.exe ($(ls -lh "$BUILD_DIR/bin/serial-monitor-cli.exe" | awk '{print $5}'))"

# 3. 收集 DLL 依赖
echo "[3/4] 收集 DLL 依赖..."

SYSTEM_DLLS="kernel32.dll user32.dll gdi32.dll advapi32.dll shell32.dll comdlg32.dll ole32.dll oleaut32.dll winspool.drv comctl32.dll winmm.dll ws2_32.dll rpcrt4.dll imm32.dll msvcrt.dll shlwapi.dll uxtheme.dll dwmapi.dll version.dll setupapi.dll cfgmgr32.dll secur32.dll crypt32.dll wldap32.dll dnsapi.dll iphlpapi.dll bcrypt.dll ncrypt.dll powrprof.dll profapi.dll netapi32.dll mpr.dll userenv.dll dxgi.dll d3d11.dll usp10.dll dwrite.dll"

is_system_dll() {
    local name="$1"
    local lower=$(echo "$name" | tr '[:upper:]' '[:lower:]')
    for sys in $SYSTEM_DLLS; do
        [ "$lower" = "$sys" ] && return 0
    done
    case "$lower" in
        msvcp*.dll|vcruntime*.dll) return 0 ;;
    esac
    return 1
}

copied_dlls=""

copy_dll_recursive() {
    local target="$1"
    local dlls=$(objdump -p "$target" 2>/dev/null | grep "DLL Name:" | awk '{print $3}')
    for dll in $dlls; do
        [ -z "$dll" ] && continue
        is_system_dll "$dll" && continue
        echo "$copied_dlls" | grep -qi "$dll" && continue
        local found=$(find "$MINGW_DIR/bin" -maxdepth 1 -iname "$dll" -print -quit 2>/dev/null)
        if [ -z "$found" ]; then
            found=$(find "$MINGW_DIR" -maxdepth 3 -iname "$dll" -print -quit 2>/dev/null)
        fi
        if [ -n "$found" ]; then
            echo "  $dll"
            cp "$found" "$DEPLOY_DIR/"
            copied_dlls="$copied_dlls"$'\n'"$dll"
            copy_dll_recursive "$found"
        fi
    done
}

copy_dll_recursive "$BUILD_DIR/bin/serial-monitor.exe"
copy_dll_recursive "$BUILD_DIR/bin/serial-monitor-cli.exe"

# Qt 插件
echo "  platforms/qwindows.dll"
mkdir -p "$DEPLOY_DIR/platforms"
cp "$MINGW_DIR/share/qt6/plugins/platforms/qwindows.dll" "$DEPLOY_DIR/platforms/" 2>/dev/null || true
cp "$MINGW_DIR/share/qt6/plugins/platforms/qdirect2d.dll" "$DEPLOY_DIR/platforms/" 2>/dev/null || true
cp "$MINGW_DIR/share/qt6/plugins/platforms/qminimal.dll" "$DEPLOY_DIR/platforms/" 2>/dev/null || true
cp "$MINGW_DIR/share/qt6/plugins/platforms/qoffscreen.dll" "$DEPLOY_DIR/platforms/" 2>/dev/null || true

echo "  imageformats"
mkdir -p "$DEPLOY_DIR/imageformats"
cp "$MINGW_DIR/share/qt6/plugins/imageformats/"*.dll "$DEPLOY_DIR/imageformats/" 2>/dev/null || true

echo "  QML 模块"
cp -r "$MINGW_DIR/share/qt6/qml" "$DEPLOY_DIR/" 2>/dev/null || true

# 图标和启动脚本
mkdir -p "$DEPLOY_DIR/icons"
cp "$PROJECT_DIR/resources/icons/logo.png" "$DEPLOY_DIR/icons/" 2>/dev/null || true
cp "$PROJECT_DIR/resources/icons/app.ico" "$DEPLOY_DIR/icons/"

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

DLL_COUNT=$(ls "$DEPLOY_DIR"/*.dll 2>/dev/null | wc -l)
echo "  DLL 总数: $DLL_COUNT"

# 4. Inno Setup 编译
echo ""
echo "[4/4] Inno Setup 编译安装包..."
cd "$PROJECT_DIR"
"$ISCC" "deploy/emberInter.iss" 2>&1 | tail -5

# 结果
echo ""
echo "================================================"
echo "  打包完成!"
echo "================================================"
SETUP=$(ls "$DIST_DIR"/*.exe 2>/dev/null | head -1)
if [ -n "$SETUP" ]; then
    SIZE=$(ls -lh "$SETUP" | awk '{print $5}')
    echo "安装包: $(basename "$SETUP")  ($SIZE)"
    echo "路径: $SETUP"
fi
