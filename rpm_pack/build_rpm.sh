#!/bin/bash
# EmberInter RPM 打包脚本
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
DIST_DIR="$PROJECT_DIR/release"
VERSION="1.2.0"
RELEASE="1"
ARCH="x86_64"
RPM_NAME="emberinter-${VERSION}-${RELEASE}.${ARCH}"

# 1. 确认已编译
if [ ! -f "$BUILD_DIR/bin/serial-monitor" ]; then
    echo "错误: 未找到编译产物，请先执行 cmake 构建"
    echo "  mkdir -p $BUILD_DIR && cd $BUILD_DIR"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "  cmake --build . -j\$(nproc)"
    exit 1
fi

# 2. 安装 rpm 构建工具
if ! command -v rpmbuild &> /dev/null; then
    echo "=== 安装 rpm 构建依赖 ==="
    sudo apt-get update -qq
    sudo apt-get install -y -qq rpm
fi

# 3. 清理并创建构建目录
rm -rf "$SCRIPT_DIR/rpmbuild"
mkdir -p "$DIST_DIR"

# RPM 构建目录结构
mkdir -p "$SCRIPT_DIR/rpmbuild/BUILD"
mkdir -p "$SCRIPT_DIR/rpmbuild/RPMS"
mkdir -p "$SCRIPT_DIR/rpmbuild/SOURCES"
mkdir -p "$SCRIPT_DIR/rpmbuild/SPECS"
mkdir -p "$SCRIPT_DIR/rpmbuild/SRPMS"

# 4. 创建安装根目录 (buildroot)
BUILDROOT="$SCRIPT_DIR/rpmbuild/BUILDROOT/${RPM_NAME}"
rm -rf "$BUILDROOT"
mkdir -p "$BUILDROOT/opt/emberinter/bin"
mkdir -p "$BUILDROOT/opt/emberinter/resources/icons"
mkdir -p "$BUILDROOT/opt/emberinter/resources/styles"
mkdir -p "$BUILDROOT/usr/share/applications"
mkdir -p "$BUILDROOT/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$BUILDROOT/usr/share/doc/emberinter"
mkdir -p "$BUILDROOT/usr/local/bin"

echo "=== 复制二进制文件 ==="
cp "$BUILD_DIR/bin/serial-monitor" "$BUILDROOT/opt/emberinter/bin/"
cp "$BUILD_DIR/bin/serial-monitor-cli" "$BUILDROOT/opt/emberinter/bin/"
chmod 755 "$BUILDROOT/opt/emberinter/bin/"*

echo "=== 复制资源文件 ==="
cp "$PROJECT_DIR/resources/styles/dark_theme.qss" "$BUILDROOT/opt/emberinter/resources/styles/"
cp "$PROJECT_DIR/resources/icons/logo.png" "$BUILDROOT/opt/emberinter/resources/icons/"
cp "$PROJECT_DIR/resources/icons/close.svg" "$BUILDROOT/opt/emberinter/resources/icons/"
cp "$PROJECT_DIR/resources/icons/logo.png" "$BUILDROOT/usr/share/icons/hicolor/256x256/apps/emberinter.png"

echo "=== 创建 .desktop 文件 ==="
cat > "$BUILDROOT/usr/share/applications/emberinter.desktop" << 'EOF'
[Desktop Entry]
Name=EmberInter
Name[zh_CN]=尘智串口调试工具
Comment=Cross-platform Serial Monitor Debug Tool
Comment[zh_CN]=跨平台串口监控调试工具
Exec=/opt/emberinter/bin/serial-monitor
Icon=emberinter
Terminal=false
Type=Application
Categories=Development;Debugger;Electronics;
Keywords=serial;debug;embedded;monitor;串口;调试;嵌入式;
StartupNotify=true
EOF

# 5. 创建 spec 文件
cat > "$SCRIPT_DIR/rpmbuild/SPECS/emberinter.spec" << SPECEOF
Name:           emberinter
Version:        ${VERSION}
Release:        ${RELEASE}%{?dist}
Summary:        EmberInter Debug Tool - 尘智串口调试工具
License:        MIT
URL:            https://github.com/kukucaiCndy/serial-monitor
Vendor:         kukucaiCndy

%description
EmberInter 是一款面向嵌入式开发者的跨平台串口监控调试工具。
同时支持 CLI 命令行模式（供 AI Agent/脚本使用）和 GUI 图形界面模式（供开发者使用）。

功能特性：
 - 串口自动检测与连接
 - 彩色日志显示（自动识别 ERROR/WARN/INFO/DEBUG/TRACE 级别）
 - HEX 十六进制显示模式与混合模式
 - 多串口标签页同时监控
 - 数据发送（文本/HEX/循环发送）
 - 日志筛选过滤与 JSON 格式导出
 - 配置持久化
 - 暗色主题支持

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a %{_topdir}/BUILDROOT/${RPM_NAME}/* %{buildroot}/

%post
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q /usr/share/applications 2>/dev/null || true
fi
ln -sf /opt/emberinter/bin/serial-monitor /usr/local/bin/serial-monitor 2>/dev/null || true
ln -sf /opt/emberinter/bin/serial-monitor-cli /usr/local/bin/emberinter-cli 2>/dev/null || true
echo "EmberInter 尘智串口调试工具已安装成功！"
echo "  GUI: 从应用菜单启动 'EmberInter'，或运行 serial-monitor"
echo "  CLI: 运行 emberinter-cli"

%postun
if [ \$1 -eq 0 ]; then
    rm -f /usr/local/bin/serial-monitor /usr/local/bin/emberinter-cli
    if [ -x /usr/bin/update-desktop-database ]; then
        update-desktop-database -q /usr/share/applications 2>/dev/null || true
    fi
fi

%files
/opt/emberinter/bin/serial-monitor
/opt/emberinter/bin/serial-monitor-cli
/opt/emberinter/resources/icons/close.svg
/opt/emberinter/resources/icons/logo.png
/opt/emberinter/resources/styles/dark_theme.qss
/usr/share/applications/emberinter.desktop
/usr/share/icons/hicolor/256x256/apps/emberinter.png

%doc /usr/share/doc/emberinter

%changelog
* $(date "+%a %b %d %Y") kukucaiCndy <kukucaiCndy@github.com> - ${VERSION}-${RELEASE}
- RPM packaging release
- CLI + GUI dual-mode serial debug tool
SPECEOF

# 6. 创建 changelog 和 copyright
cat > "$BUILDROOT/usr/share/doc/emberinter/changelog" << EOF
emberinter (${VERSION}-${RELEASE}) unstable; urgency=medium

  * RPM packaging release
  * CLI + GUI dual-mode serial debug tool

 -- kukucaiCndy <kukucaiCndy@github.com>  $(date -R)
EOF
gzip -9 -n "$BUILDROOT/usr/share/doc/emberinter/changelog"

cat > "$BUILDROOT/usr/share/doc/emberinter/copyright" << 'EOF'
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: EmberInterDebugTool
Source: https://github.com/kukucaiCndy/serial-monitor

Files: *
Copyright: 2025-2026 kukucaiCndy
License: MIT
EOF

# 7. 构建 RPM
echo ""
echo "=== 构建 RPM 包 ==="
rm -f "$DIST_DIR"/emberinter-*.rpm

rpmbuild -bb \
    --define "_topdir $SCRIPT_DIR/rpmbuild" \
    --buildroot "$BUILDROOT" \
    "$SCRIPT_DIR/rpmbuild/SPECS/emberinter.spec"

# 8. 复制产物
RPM_OUTPUT=$(find "$SCRIPT_DIR/rpmbuild/RPMS" -name "*.rpm" 2>/dev/null | head -1)
if [ -f "$RPM_OUTPUT" ]; then
    cp "$RPM_OUTPUT" "$DIST_DIR/"
else
    echo "错误: RPM 构建失败，未找到 RPM 文件"
    exit 1
fi

echo ""
echo "=== 完成 ==="
ls -lh "$DIST_DIR"/emberinter-*.rpm
echo ""
echo "安装: sudo rpm -i $DIST_DIR/emberinter-${VERSION}-${RELEASE}.${ARCH}.rpm"
echo "卸载: sudo rpm -e emberinter"
