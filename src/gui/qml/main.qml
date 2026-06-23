import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EmberInter 1.0
import EmberDesign 1.0

/// 尘智 (EmberInterDebugTool) — Qt6 QML 主窗口
/// 布局: 侧边栏(固定) + 内容区(TabBar + StackLayout) + 状态栏(底部)
/// 设计对齐: docs/design.pen + docs/交互设计文档.md
ApplicationWindow {
    id: root
    width: 1366
    height: 768
    minimumWidth: 960
    minimumHeight: 540
    title: "尘智 — EmberInterDebugTool"
    visible: true

    color: DesignSystem.bgPrimary

    // ═══════════════════════════════════════════════
    // 键盘快捷键 (L4 交互规范)
    // ═══════════════════════════════════════════════
    Shortcut { sequence: "Ctrl+T";   onActivated: appCore.createConnection() }
    Shortcut { sequence: "Ctrl+W";   onActivated: appCore.closeCurrentTab() }
    Shortcut { sequence: "Ctrl+1";   onActivated: appCore.setCurrentTab(0) }
    Shortcut { sequence: "Ctrl+2";   onActivated: appCore.setCurrentTab(1) }
    Shortcut { sequence: "Ctrl+3";   onActivated: appCore.setCurrentTab(2) }
    Shortcut { sequence: "Ctrl+4";   onActivated: appCore.setCurrentTab(3) }
    Shortcut { sequence: "Ctrl+5";   onActivated: appCore.setCurrentTab(4) }
    Shortcut { sequence: "Ctrl+6";   onActivated: appCore.setCurrentTab(5) }
    Shortcut { sequence: "Ctrl+7";   onActivated: appCore.setCurrentTab(6) }
    Shortcut { sequence: "Ctrl+8";   onActivated: appCore.setCurrentTab(7) }
    Shortcut { sequence: "Ctrl+9";   onActivated: appCore.setCurrentTab(8) }
    Shortcut { sequence: "Escape";   onActivated: closeTopPopup() }

    function closeTopPopup() {
        if (connectionWizard.opened) connectionWizard.close()
        else if (settingsDialog.opened) settingsDialog.close()
        else if (aboutDialog.opened) aboutDialog.close()
    }

    // ═══════════════════════════════════════════════
    // AppCore 信号连接
    // ═══════════════════════════════════════════════
    Connections {
        target: appCore
        function onWizardRequested()     { connectionWizard.open() }
        function onSettingsRequested()   { settingsDialog.open() }
        function onAboutRequested()      { aboutDialog.open() }
    }

    // ═══════════════════════════════════════════════
    // 主布局: 顶层 ColumnLayout (内容区 + 状态栏)
    // ═══════════════════════════════════════════════
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ───────────────────────────────────────────
        // 内容区: 侧边栏 + 主区域
        // ───────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

        // ───────────────────────────────────────────
        // 侧边栏 (Sidebar)
        // ───────────────────────────────────────────
        Rectangle {
            id: sidebar
            Layout.preferredWidth: 220
            Layout.minimumWidth: 48
            Layout.fillHeight: true
            color: DesignSystem.bgSecondary

            // 右侧薄边框
            Rectangle {
                anchors.right: parent.right
                width: 1; height: parent.height
                color: DesignSystem.border
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Logo 区 (h=48)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: DesignSystem.spaceLg
                        anchors.rightMargin: DesignSystem.spaceSm
                        spacing: DesignSystem.spaceSm

                        // 应用图标
                        Rectangle {
                            width: 32; height: 32; radius: DesignSystem.radiusSm
                            color: DesignSystem.accent
                            Text {
                                anchors.centerIn: parent
                                text: "\u5C18" // 尘
                                color: DesignSystem.textInverse
                                font.family: DesignSystem.fontHead
                                font.pixelSize: 18
                                font.bold: true
                            }
                        }

                        ColumnLayout {
                            spacing: 0
                            Text {
                                text: "尘智"
                                color: DesignSystem.accentHover
                                font.family: DesignSystem.fontHead
                                font.pixelSize: 16
                                font.bold: true
                            }
                            Text {
                                text: "DebugTool"
                                color: DesignSystem.textSecondary
                                font.pixelSize: 9
                                font.letterSpacing: 1.5
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Tab 计数徽章
                        Rectangle {
                            width: 20; height: 20; radius: DesignSystem.radiusSm
                            color: DesignSystem.bgTertiary
                            visible: appCore.tabModel && appCore.tabModel.count > 0
                            Text {
                                anchors.centerIn: parent
                                text: appCore.tabModel ? appCore.tabModel.count : "0"
                                color: DesignSystem.textSecondary
                                font.family: DesignSystem.fontMono
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                    }
                }

                // 分隔线
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: DesignSystem.spaceLg
                    Layout.rightMargin: DesignSystem.spaceLg
                    height: 1
                    color: DesignSystem.border
                }

                // "我的会话" 标题 + 新建按钮
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    Layout.topMargin: DesignSystem.spaceMd
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: DesignSystem.spaceMd
                        anchors.rightMargin: DesignSystem.spaceMd
                        spacing: 0

                        Text {
                            text: "我的会话"
                            color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.5
                        }
                        Item { Layout.fillWidth: true }

                        // 新建会话 [+]
                        Rectangle {
                            width: 20; height: 20; radius: DesignSystem.radiusSm
                            color: addSessionHover.containsMouse ? DesignSystem.selected : DesignSystem.bgTertiary
                            Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }
                            Text {
                                anchors.centerIn: parent
                                text: "\u002B" // +
                                color: DesignSystem.textSecondary
                                font.family: DesignSystem.fontMono
                                font.pixelSize: 14
                                font.bold: true
                            }
                            MouseArea {
                                id: addSessionHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appCore.createConnection()
                            }
                        }
                    }
                }

                // 已保存会话列表
                ListView {
                    id: sessionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: appCore.savedPorts
                    clip: true
                    spacing: 0
                    cacheBuffer: 200

                    // 空状态提示
                    footer: Item {
                        width: ListView.view.width
                        height: sessionList.count === 0 ? 28 : 0
                        visible: sessionList.count === 0
                        Text {
                            anchors.centerIn: parent
                            text: "点击 [+] 创建第一个会话"
                            color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody
                            font.pixelSize: 10
                            opacity: 0.6
                        }
                    }

                    delegate: Rectangle {
                        id: sessionItem
                        width: ListView.view.width
                        height: 36
                        color: sessionHover.containsMouse ? DesignSystem.hover : "transparent"
                        Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }
                        opacity: model.available ? 1.0 : 0.45

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: DesignSystem.spaceMd
                            anchors.rightMargin: DesignSystem.spaceMd
                            spacing: DesignSystem.spaceSm

                            // 类型指示器圆点
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: {
                                    if (!model.available) return DesignSystem.textSecondary
                                    switch(model.type) {
                                        case 0: return DesignSystem.accent   // 串口
                                        case 1: return DesignSystem.warning  // 终端
                                        case 2: return DesignSystem.success  // SSH
                                        default: return DesignSystem.textSecondary
                                    }
                                }
                            }

                            Text {
                                text: model.summary
                                color: model.available ? DesignSystem.textPrimary : DesignSystem.textSecondary
                                font.family: DesignSystem.fontBody
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // 不可用标记
                            Text {
                                text: "不可用"
                                color: DesignSystem.error
                                font.family: DesignSystem.fontBody
                                font.pixelSize: 10
                                visible: !model.available
                            }
                        }

                        MouseArea {
                            id: sessionHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    sessionContextMenu.sessionIndex = index
                                    sessionContextMenu.sessionType = model.type
                                    sessionContextMenu.sessionSummary = model.summary
                                    sessionContextMenu.connected = false
                                    // 检查是否已有对应 Tab 及其连接状态
                                    var tabIdx = appCore.findTabForSavedPort(index)
                                    sessionContextMenu.tabIndex = tabIdx
                                    if (tabIdx >= 0) {
                                        var page = appCore.getTabPage(tabIdx)
                                        sessionContextMenu.connected = page ? page.connected : false
                                    }
                                    sessionContextMenu.popup()
                                } else {
                                    appCore.connectSavedPort(index)
                                }
                            }
                            onDoubleClicked: appCore.connectSavedPort(index)
                        }

                        // 右键菜单
                        Menu {
                            id: sessionContextMenu
                            property int sessionIndex: -1
                            property int sessionType: -1
                            property string sessionSummary: ""
                            property bool connected: false
                            property int tabIndex: -1

                            // 连接/断开 (串口/SSH) 或 打开/关闭 (终端)
                            MenuItem {
                                text: {
                                    if (sessionContextMenu.sessionType === 1) {
                                        return sessionContextMenu.connected ? "关闭终端" : "打开终端"
                                    } else {
                                        return sessionContextMenu.connected ? "断开连接" : "连接"
                                    }
                                }
                                onTriggered: appCore.toggleSavedPort(sessionContextMenu.sessionIndex)
                            }

                            MenuItem {
                                text: "重命名"
                                onTriggered: {
                                    renameDialog.savedPortIndex = sessionContextMenu.sessionIndex
                                    renameDialog.isSavedPort = true
                                    renameDialog.currentName = sessionContextMenu.sessionSummary
                                    renameDialog.open()
                                }
                            }

                            // 关闭 Tab (仅串口/SSH, CMD 终端直接打开/关闭, 没有常驻 Tab)
                            MenuItem {
                                text: "关闭"
                                visible: sessionContextMenu.sessionType === 0 || sessionContextMenu.sessionType === 1
                                height: visible ? implicitHeight : 0
                                onTriggered: appCore.closeTabForSavedPort(sessionContextMenu.sessionIndex)
                            }

                            MenuSeparator {
                                visible: sessionContextMenu.sessionType === 0 || sessionContextMenu.sessionType === 1
                                height: visible ? implicitHeight : 0
                            }

                            MenuItem {
                                text: "删除"
                                onTriggered: appCore.removeSavedPort(sessionContextMenu.sessionIndex)
                            }
                        }

                        ToolTip {
                            visible: sessionHover.containsMouse && !model.available
                            text: "端口未连接或不存在"
                            delay: 300
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // 底部操作按钮
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: DesignSystem.spaceMd
                    Layout.leftMargin: DesignSystem.spaceMd
                    Layout.rightMargin: DesignSystem.spaceMd
                    spacing: 1

                    Rectangle {
                        Layout.fillWidth: true; height: 1
                        color: DesignSystem.border
                        Layout.bottomMargin: DesignSystem.spaceXs
                    }

                    SidebarNavBtn {
                        label: "设置"
                        iconText: DesignSystem.iconSettings
                        onClicked: appCore.openSettings()
                    }
                    SidebarNavBtn {
                        label: "关于"
                        iconText: DesignSystem.iconInfo
                        onClicked: appCore.openAbout()
                    }
                }
            }
        } // 侧边栏结束

        // ═══════════════════════════════════════════
        // 主内容区 (Content Area)
        // ═══════════════════════════════════════════
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── 空状态页 (无 Tab 时显示) ──
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: DesignSystem.bgPrimary
                visible: !appCore.tabModel || appCore.tabModel.count === 0

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: DesignSystem.spaceLg

                    // Logo
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 80; height: 80; radius: DesignSystem.radiusLg
                        color: DesignSystem.accent10
                        border.width: 1
                        border.color: DesignSystem.accent15

                        Text {
                            anchors.centerIn: parent
                            text: "\u5C18" // 尘
                            color: DesignSystem.accent
                            font.family: DesignSystem.fontHead
                            font.pixelSize: 36
                            font.bold: true
                        }
                    }

                    // 标题 + 标语
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "EmberInterDebugTool"
                        color: DesignSystem.textPrimary
                        font.family: DesignSystem.fontHead
                        font.pixelSize: 22
                        font.bold: true
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "微尘藏星火，终端孕尘智"
                        color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody
                        font.pixelSize: 13
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "人机协同 Agent 工作台"
                        color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody
                        font.pixelSize: 12
                        opacity: 0.7
                    }

                    Item { height: DesignSystem.spaceSm }

                    // 操作按钮
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: DesignSystem.spaceMd

                        // 创建会话 (主按钮)
                        Rectangle {
                            id: emptyCreateBtn
                            width: emptyCreateText.implicitWidth + 32
                            height: 36; radius: DesignSystem.radiusMd
                            color: emptyCreateHover.containsMouse ? DesignSystem.accentHover : DesignSystem.accent
                            Behavior on color { ColorAnimation { duration: DesignSystem.animNormal } }

                            Text {
                                id: emptyCreateText
                                anchors.centerIn: parent
                                text: "创建会话"
                                color: DesignSystem.textInverse
                                font.family: DesignSystem.fontBody
                                font.pixelSize: 13
                                font.bold: true
                            }
                            MouseArea {
                                id: emptyCreateHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appCore.createConnection()
                            }
                        }

                        // 打开设置 (次要按钮)
                        Rectangle {
                            id: emptySettingsBtn
                            width: emptySettingsText.implicitWidth + 32
                            height: 36; radius: DesignSystem.radiusMd
                            color: emptySettingsHover.containsMouse ? DesignSystem.hover : DesignSystem.bgTertiary
                            border.width: 1; border.color: DesignSystem.border
                            Behavior on color { ColorAnimation { duration: DesignSystem.animNormal } }

                            Text {
                                id: emptySettingsText
                                anchors.centerIn: parent
                                text: "打开设置"
                                color: DesignSystem.textPrimary
                                font.family: DesignSystem.fontBody
                                font.pixelSize: 13
                            }
                            MouseArea {
                                id: emptySettingsHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appCore.openSettings()
                            }
                        }
                    }

                    Item { height: DesignSystem.spaceLg }

                    // 分隔线
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 360; height: 1
                        color: DesignSystem.border
                    }

                    // 快速开始指南
                    ColumnLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: DesignSystem.spaceSm
                        Layout.topMargin: DesignSystem.spaceSm

                        Repeater {
                            model: [
                                "点击 [+] 创建串口或终端会话",
                                "CLI: serial-monitor-cli --list",
                                "CLI: serial-monitor-cli --connect COM3"
                            ]
                            RowLayout {
                                spacing: DesignSystem.spaceSm
                                Text {
                                    text: "\u276F" // ❯
                                    color: DesignSystem.accent
                                    font.family: DesignSystem.fontMono
                                    font.pixelSize: 11
                                }
                                Text {
                                    text: modelData
                                    color: DesignSystem.textSecondary
                                    font.family: DesignSystem.fontMono
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }
            } // 空状态页结束

            // ── TabBar + 内容区 (有 Tab 时显示) ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                visible: appCore.tabModel && appCore.tabModel.count > 0

                // TabBar (h=36)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: DesignSystem.bgSecondary

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width; height: 1
                        color: DesignSystem.border
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        // 可滚动 Tab 列表
                        ListView {
                            id: tabBarList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: appCore.tabModel
                            orientation: ListView.Horizontal
                            clip: true
                            spacing: 0
                            currentIndex: appCore.currentTabIndex

                            delegate: Rectangle {
                                id: tabDelegate
                                width: Math.max(140, Math.min(220, tabTitle.implicitWidth + 56))
                                height: ListView.view.height
                                color: {
                                    if (index === appCore.currentTabIndex) return DesignSystem.bgPrimary
                                    if (tabHoverArea.containsMouse) return DesignSystem.hover
                                    return "transparent"
                                }
                                Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

                                // 底部激活指示线
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width; height: 2
                                    color: index === appCore.currentTabIndex ? DesignSystem.accent : "transparent"
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: DesignSystem.spaceMd
                                    anchors.rightMargin: DesignSystem.spaceXs
                                    spacing: DesignSystem.spaceSm

                                    // 连接状态指示器
                                    Rectangle {
                                        width: 6; height: 6; radius: 3
                                        color: model.connected ? DesignSystem.success : DesignSystem.textSecondary
                                    }

                                    // Tab 标题
                                    Text {
                                        id: tabTitle
                                        text: model.title
                                        color: index === appCore.currentTabIndex ? DesignSystem.textPrimary : DesignSystem.textSecondary
                                        font.family: DesignSystem.fontBody
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // 关闭按钮 (悬停或选中时显示)
                                    Rectangle {
                                        z: 10
                                        width: 20; height: 20; radius: DesignSystem.radiusSm
                                        color: tabCloseHover.containsMouse ? DesignSystem.error + "1A" : "transparent"
                                        visible: tabHoverArea.containsMouse || index === appCore.currentTabIndex

                                        Text {
                                            anchors.centerIn: parent
                                            text: "\u2715" // ✕
                                            color: tabCloseHover.containsMouse ? DesignSystem.error : DesignSystem.textSecondary
                                            font.pixelSize: 11
                                            font.bold: true
                                        }

                                        MouseArea {
                                            id: tabCloseHover
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            z: 10
                                            cursorShape: Qt.PointingHandCursor
                                            propagateComposedEvents: false
                                            onClicked: {
                                                appCore.closeTab(index)
                                                mouse.accepted = true
                                            }
                                        }
                                    }
                                }

                                // 点击切换 Tab (z:-1 确保不遮挡关闭按钮)
                                MouseArea {
                                    id: tabHoverArea
                                    anchors.fill: parent
                                    z: -1
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    acceptedButtons: Qt.LeftButton
                                    onClicked: appCore.setCurrentTab(index)
                                }
                            }
                        }

                        // 新建 Tab [+]
                        Rectangle {
                            id: newTabBtn
                            Layout.preferredWidth: 36
                            Layout.fillHeight: true
                            color: newTabHover.containsMouse ? DesignSystem.hover : "transparent"
                            Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

                            Text {
                                anchors.centerIn: parent
                                text: "\u002B" // +
                                color: DesignSystem.textSecondary
                                font.family: DesignSystem.fontMono
                                font.pixelSize: 16
                                font.bold: true
                            }
                            MouseArea {
                                id: newTabHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appCore.createConnection()
                            }
                            ToolTip {
                                visible: newTabHover.containsMouse
                                text: "新建会话 (Ctrl+T)"
                                delay: 500
                            }
                        }

                        // 关闭当前 Tab [✕]
                        Rectangle {
                            id: closeTabBtn
                            Layout.preferredWidth: 36
                            Layout.fillHeight: true
                            color: closeTabHover.containsMouse ? DesignSystem.error + "1A" : "transparent"
                            Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

                            Text {
                                anchors.centerIn: parent
                                text: "\u2715" // ✕
                                color: closeTabHover.containsMouse ? DesignSystem.error : DesignSystem.textSecondary
                                font.pixelSize: 14
                            }
                            MouseArea {
                                id: closeTabHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appCore.closeCurrentTab()
                            }
                            ToolTip {
                                visible: closeTabHover.containsMouse
                                text: "关闭当前 (Ctrl+W)"
                                delay: 500
                            }
                        }
                    }
                } // TabBar 结束

                // Tab 内容区 (StackLayout 动态加载)
                StackLayout {
                    id: tabStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: appCore.currentTabIndex

                    Repeater {
                        model: appCore.tabModel

                        Loader {
                            id: tabLoader
                            property int tabIndex: index
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            source: {
                                var src = ""
                                switch(model.tabType) {
                                    case 0: src = "SerialTab.qml"; break
                                    case 1: src = "TerminalTab.qml"; break
                                    case 2: src = "TerminalTab.qml"; break
                                default: src = ""; break
                                }
                                return src
                            }

                            onLoaded: {
                                if (item && item.tabPage !== undefined) {
                                    item.tabPage = appCore.getTabPage(tabIndex)
                                }
                            }
                        }
                    }
                }
            } // TabBar + 内容区结束
        } // 主内容区结束
    } // RowLayout (侧边栏+主内容区) 结束

    // ═══════════════════════════════════════════════
    // 状态栏 (StatusBar — 窗口最底部，仅主内容区宽度)
    // ═══════════════════════════════════════════════
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 24
        color: DesignSystem.bgSecondary

        Rectangle {
            anchors.top: parent.top
            width: parent.width; height: 1
            color: DesignSystem.border
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: DesignSystem.spaceMd
            anchors.rightMargin: DesignSystem.spaceMd
            spacing: 12

            // 端口/状态文本
            Text {
                text: appCore.statusText || "就绪"
                color: appCore.hasActiveTab ? DesignSystem.success : DesignSystem.textSecondary
                font.family: DesignSystem.fontMono
                font.pixelSize: 11
            }

            // 端口配置
            Text {
                text: appCore.portConfigText
                color: DesignSystem.textSecondary
                font.family: DesignSystem.fontMono
                font.pixelSize: 11
                visible: appCore.hasActiveTab && text !== ""
            }

            // RX 统计
            Text {
                text: appCore.rxBytesText
                color: DesignSystem.info
                font.family: DesignSystem.fontMono
                font.pixelSize: 11
                visible: appCore.hasActiveTab
            }

            // TX 统计
            Text {
                text: appCore.txBytesText
                color: DesignSystem.warning
                font.family: DesignSystem.fontMono
                font.pixelSize: 11
                visible: appCore.hasActiveTab
            }

            // 运行时长
            Text {
                text: appCore.uptimeText
                color: DesignSystem.textSecondary
                font.family: DesignSystem.fontMono
                font.pixelSize: 11
                visible: appCore.hasActiveTab && text !== ""
            }
        }
    } // 状态栏结束
    } // 顶层 ColumnLayout 结束

    // ═══════════════════════════════════════════════
    // 弹出层 (Popups)
    // ═══════════════════════════════════════════════

    // 重命名对话框 (支持 Tab 重命名和会话重命名)
    Dialog {
        id: renameDialog
        property int tabIndex: -1
        property int savedPortIndex: -1
        property bool isSavedPort: false
        property string currentName: ""
        anchors.centerIn: parent
        modal: true
        title: isSavedPort ? "重命名会话" : "重命名 Tab"
        background: Rectangle {
            color: DesignSystem.bgSecondary; radius: DesignSystem.radiusLg
            border.width: 1; border.color: DesignSystem.border
        }

        contentItem: ColumnLayout {
            spacing: DesignSystem.spaceMd
            TextField {
                id: renameInput
                text: renameDialog.currentName
                Layout.preferredWidth: 280
                font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeLg
                color: DesignSystem.textPrimary
                selectByMouse: true; focus: true
                background: Rectangle {
                    color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                    border.width: 1; border.color: renameInput.activeFocus ? DesignSystem.accent : DesignSystem.border
                }
                onAccepted: renameDialog.accept()
            }
        }

        footer: RowLayout {
            spacing: DesignSystem.spaceSm
            Layout.alignment: Qt.AlignRight
            Rectangle {
                width: 64; height: 30; radius: DesignSystem.radiusSm
                color: cancelRenameHover.containsMouse ? DesignSystem.hover : DesignSystem.bgTertiary
                border.width: 1; border.color: DesignSystem.border
                Text { anchors.centerIn: parent; text: "取消"; color: DesignSystem.textSecondary; font.pixelSize: 12 }
                MouseArea { id: cancelRenameHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: renameDialog.reject() }
            }
            Rectangle {
                width: 64; height: 30; radius: DesignSystem.radiusSm
                color: confirmRenameHover.containsMouse ? DesignSystem.accentHover : DesignSystem.accent
                Text { anchors.centerIn: parent; text: "确定"; color: DesignSystem.textInverse; font.pixelSize: 12; font.bold: true }
                MouseArea { id: confirmRenameHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: renameDialog.accept() }
            }
        }

        onOpened: { renameInput.text = currentName; renameInput.selectAll(); renameInput.forceActiveFocus() }
        onAccepted: {
            if (renameInput.text.trim().length > 0) {
                if (isSavedPort) {
                    appCore.renameSavedPort(savedPortIndex, renameInput.text)
                } else {
                    appCore.renameTab(tabIndex, renameInput.text)
                }
            }
        }
    }

    // 连接向导
    ConnectionWizard {
        id: connectionWizard
    }

    // 设置对话框
    SettingsDialog {
        id: settingsDialog
    }

    // 关于 / 更新
    AboutDialog {
        id: aboutDialog
    }

    // ═══════════════════════════════════════════════
    // 内联组件 (In-components)
    // ═══════════════════════════════════════════════

    // ── 侧边栏导航按钮 ──
    component SidebarNavBtn: Rectangle {
        property string label: ""
        property string iconText: ""
        signal clicked()

        width: parent ? parent.width : 200
        height: 32; radius: DesignSystem.radiusSm
        color: navHover.containsMouse ? DesignSystem.hover : "transparent"
        Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: DesignSystem.spaceSm
            anchors.rightMargin: DesignSystem.spaceSm
            spacing: DesignSystem.spaceSm

            Text {
                text: parent.parent.label
                color: DesignSystem.textSecondary
                font.family: DesignSystem.fontBody
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            Text {
                text: parent.parent.iconText
                color: DesignSystem.textSecondary
                font.family: DesignSystem.fontBody
                font.pixelSize: 14
                visible: text !== ""
            }
        }
        MouseArea {
            id: navHover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}