#!/bin/bash
# make_macos.sh - EmberInterDebugTool macOS 构建脚本
# 用法:
#   ./make_macos.sh [all|clean|rebuild|deploy|dmg|release]
#   默认 Debug 模式，release 命令使用 Release 模式
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build/macos"
ARCH="$(uname -m)"

cmd="${1:-all}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"

case "$cmd" in
    all)
        echo "==> 配置 macOS 构建 ($BUILD_TYPE, $ARCH)..."
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake ../.. -G "Unix Makefiles" \
            -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
            -DCMAKE_OSX_ARCHITECTURES="$ARCH"

        echo "==> 编译..."
        make -j"$(sysctl -n hw.ncpu)"

        echo "==> 构建完成: $BUILD_DIR/bin/serial-monitor.app"
        ;;

    clean)
        if [ -d "$BUILD_DIR" ]; then
            echo "==> 清理构建产物..."
            rm -rf "$BUILD_DIR"
        fi
        echo "==> 清理完成"
        ;;

    rebuild)
        "$0" clean
        "$0" all
        # 确保开发模式不残留 macdeployqt 部署的 Qt 库
        rm -rf "$BUILD_DIR/bin/serial-monitor.app/Contents/Frameworks" \
               "$BUILD_DIR/bin/serial-monitor.app/Contents/PlugIns" \
               "$BUILD_DIR/bin/serial-monitor.app/Contents/Resources/qt.conf"
        ;;

    deploy)
        APP="$BUILD_DIR/bin/serial-monitor.app"
        if [ ! -d "$APP" ]; then
            echo "错误: $APP 不存在，请先运行 ./make_macos.sh all"
            exit 1
        fi

        # 清理旧部署产物，避免两套 Qt 库冲突
        echo "==> 清理旧部署产物..."
        rm -rf "$APP/Contents/Frameworks" \
               "$APP/Contents/PlugIns" \
               "$APP/Contents/Resources/qt.conf"

        echo "==> 使用 macdeployqt 打包..."
        macdeployqt "$APP" \
            -qmldir="$PROJECT_DIR/src/gui/qml" \
            -libpath="/opt/homebrew/lib"

        # macdeployqt 复制的 dylib/framework 会保留原始签名导致
        # macOS Code Signature Invalid 崩溃，需要用 ad-hoc 签名覆盖
        echo "==> 重新签名 (ad-hoc)..."
        codesign --force --deep --sign - "$APP" 2>/dev/null || true
        # 验证签名
        codesign --verify --deep --strict "$APP" 2>&1 && echo "签名验证通过" || echo "签名验证失败 (不阻塞)"

        echo "==> 打包完成: $APP"
        ;;

    dmg)
        APP="$BUILD_DIR/bin/serial-monitor.app"
        if [ ! -d "$APP" ]; then
            echo "错误: $APP 不存在，请先运行 ./make_macos.sh deploy"
            exit 1
        fi

        DMG_DIR="$BUILD_DIR/dmg"
        DMG_NAME="EmberInterDebugTool.dmg"
        DMG_PATH="$BUILD_DIR/bin/$DMG_NAME"

        echo "==> 创建 DMG 安装包..."

        # 清理临时目录
        rm -rf "$DMG_DIR"
        mkdir -p "$DMG_DIR"

        # 复制 app 和 Applications 链接
        cp -R "$APP" "$DMG_DIR/"
        ln -s /Applications "$DMG_DIR/Applications"

        # 删除旧的 DMG
        rm -f "$DMG_PATH"

        # 创建 DMG
        hdiutil create -volname "EmberInterDebugTool" \
            -srcfolder "$DMG_DIR" \
            -ov -format UDZO \
            "$DMG_PATH"

        # 清理临时目录
        rm -rf "$DMG_DIR"

        echo "==> DMG 安装包生成完成: $DMG_PATH"
        echo "    大小: $(du -sh "$DMG_PATH" | cut -f1)"
        ;;

    release)
        BUILD_TYPE=Release "$0" clean
        BUILD_TYPE=Release "$0" all
        BUILD_TYPE=Release "$0" deploy
        BUILD_TYPE=Release "$0" dmg
        ;;

    *)
        echo "用法: $0 {all|clean|rebuild|deploy|dmg|release}"
        echo ""
        echo "命令说明:"
        echo "  all      - Debug 编译 (用 Homebrew Qt 运行，无需打包)"
        echo "  clean    - 清理构建产物"
        echo "  rebuild  - 清理后重新编译 (自动清理旧 Frameworks)"
        echo "  deploy   - macdeployqt 打包 Qt 库到 .app (分发前用)"
        echo "  dmg      - 生成 DMG 安装包 (需先 deploy)"
        echo "  release  - 一键 Release: clean + build + deploy + dmg"
        exit 1
        ;;
esac
