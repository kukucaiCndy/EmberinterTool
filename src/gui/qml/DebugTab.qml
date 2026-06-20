import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EmberInter 1.0
import EmberDesign 1.0

/// 调试 Tab — ST-Link / J-Link 调试面板
Rectangle {
    id: root
    color: DesignSystem.bgPrimary

    property var tabPage: null

    // 当前激活的右侧面板
    property int rightPanelIndex: 0 // 0=寄存器, 1=内存, 2=调用栈

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ══════════════════════════════════════════════
        // 状态栏 (h=32)
        // ══════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: DesignSystem.bgSecondary

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: DesignSystem.spaceSm
                anchors.rightMargin: DesignSystem.spaceSm
                spacing: DesignSystem.spaceMd

                // 调试器类型标识
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: DesignSystem.warning
                }
                Text {
                    text: root.tabPage ? root.tabPage.debuggerType : "调试"
                    color: DesignSystem.warning
                    font.family: DesignSystem.fontBody
                    font.pixelSize: DesignSystem.fontSizeSm
                    font.bold: true
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 18
                    color: DesignSystem.border
                }

                // 目标芯片
                Text {
                    text: root.tabPage && root.tabPage.targetChip ? root.tabPage.targetChip : "未识别"
                    color: DesignSystem.textSecondary
                    font.family: DesignSystem.fontMono
                    font.pixelSize: DesignSystem.fontSizeSm
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 18
                    color: DesignSystem.border
                }

                // HALT/RUN 状态
                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: (root.tabPage && root.tabPage.halted) ? DesignSystem.error : DesignSystem.success
                }
                Text {
                    text: (root.tabPage && root.tabPage.halted) ? "HALT" : "RUN"
                    color: (root.tabPage && root.tabPage.halted) ? DesignSystem.error : DesignSystem.success
                    font.family: DesignSystem.fontMono
                    font.pixelSize: DesignSystem.fontSizeSm
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                // 连接状态
                Text {
                    text: root.tabPage && root.tabPage.connected ? "已连接" : "未连接"
                    color: DesignSystem.textSecondary
                    font.family: DesignSystem.fontBody
                    font.pixelSize: DesignSystem.fontSizeXs
                }
            }
        }

        // ══════════════════════════════════════════════
        // 调试控制工具栏 (h=36)
        // ══════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: DesignSystem.bgPrimary

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: DesignSystem.spaceSm
                anchors.rightMargin: DesignSystem.spaceSm
                spacing: 4

                DebugCtrlBtn { text: "\u25A0"; label: "Halt";   accent: DesignSystem.error;   onClicked: { if (root.tabPage) root.tabPage.halt() } }
                DebugCtrlBtn { text: "\u25B6"; label: "Run";    accent: DesignSystem.success;  onClicked: { if (root.tabPage) root.tabPage.run() } }
                DebugCtrlBtn { text: "\u2913"; label: "Step";   accent: DesignSystem.accent;  onClicked: { if (root.tabPage) root.tabPage.step() } }
                DebugCtrlBtn { text: "\u21BB"; label: "Reset";  accent: DesignSystem.warning; onClicked: { if (root.tabPage) root.tabPage.reset() } }

                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 22; color: DesignSystem.border }

                DebugCtrlBtn { text: "\u2600"; label: "断点";   accent: DesignSystem.textSecondary; onClicked: { if (root.tabPage) root.tabPage.toggleBreakpoint() } }
                DebugCtrlBtn { text: "\u21E7"; label: "单步跳"; accent: DesignSystem.textSecondary; onClicked: { if (root.tabPage) root.tabPage.stepOver() } }
                DebugCtrlBtn { text: "\u21F2"; label: "跳出";   accent: DesignSystem.textSecondary; onClicked: { if (root.tabPage) root.tabPage.stepOut() } }

                Item { Layout.fillWidth: true }

                Text {
                    text: root.tabPage && root.tabPage.programCounter
                          ? "PC: " + root.tabPage.programCounter
                          : "PC: ----"
                    color: DesignSystem.textSecondary
                    font.family: DesignSystem.fontMono
                    font.pixelSize: DesignSystem.fontSizeSm
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: DesignSystem.border
        }

        // ══════════════════════════════════════════════
        // 主内容区: 输出区 + 命令行 (左) | 寄存器/内存/调用栈 (右)
        // ══════════════════════════════════════════════
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── 左侧: 输出区 + 命令行 ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // 输出区标题
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    color: DesignSystem.bgSecondary

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: DesignSystem.spaceSm
                        anchors.rightMargin: DesignSystem.spaceSm
                        spacing: DesignSystem.spaceSm

                        Text {
                            text: "输出"
                            color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody
                            font.pixelSize: DesignSystem.fontSizeXs
                            font.bold: true
                            font.letterSpacing: 0.5
                        }

                        Item { Layout.fillWidth: true }

                        // 清空按钮
                        Text {
                            text: DesignSystem.iconClose
                            color: clearOutputHover.containsMouse ? DesignSystem.error : DesignSystem.textSecondary
                            font.pixelSize: 11
                            font.bold: true

                            MouseArea {
                                id: clearOutputHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: outputArea.clear()
                            }
                        }
                    }
                }

                // 输出区
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: DesignSystem.bgTertiary
                    border.width: 1
                    border.color: DesignSystem.border

                    Flickable {
                        id: flickable
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        flickableDirection: Flickable.VerticalFlick
                        boundsBehavior: Flickable.StopAtBounds

                        TextArea.flickable: TextArea {
                            id: outputArea
                            color: DesignSystem.textPrimary
                            font.family: DesignSystem.fontMono
                            font.pixelSize: DesignSystem.fontSizeMd
                            readOnly: true
                            background: null
                            textFormat: TextEdit.PlainText
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            padding: DesignSystem.spaceSm

                            text: root.tabPage
                                ? "[调试输出] " + root.tabPage.debuggerType + " — 就绪\n"
                                : "[调试输出] 未连接 — 请先连接调试器\n"

                            Connections {
                                target: root.tabPage
                                function onOutputReceived(txt) { outputArea.append(txt) }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 6
                                color: DesignSystem.border
                                radius: 3
                            }
                        }
                    }
                }

                // 命令行输入
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: DesignSystem.bgSecondary
                    border.width: 1
                    border.color: DesignSystem.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: DesignSystem.spaceSm
                        anchors.rightMargin: DesignSystem.spaceSm
                        spacing: 6

                        Text {
                            text: DesignSystem.iconChevron
                            color: DesignSystem.warning
                            font.family: DesignSystem.fontMono
                            font.pixelSize: DesignSystem.fontSizeLg
                            font.bold: true
                        }

                        TextField {
                            id: cmdInput
                            Layout.fillWidth: true
                            color: DesignSystem.textPrimary
                            font.family: DesignSystem.fontMono
                            font.pixelSize: DesignSystem.fontSizeMd
                            placeholderText: "输入调试命令..."
                            placeholderTextColor: DesignSystem.textSecondary
                            selectByMouse: true
                            background: null

                            onAccepted: {
                                if (root.tabPage && text.trim() !== "") {
                                    root.tabPage.runCommand(text)
                                    text = ""
                                }
                            }
                        }
                    }
                }
            }

            // 分隔线
            Rectangle {
                width: 1
                Layout.fillHeight: true
                color: DesignSystem.border
            }

            // ── 右侧面板: 寄存器 / 内存 / 调用栈 ──
            Rectangle {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                color: DesignSystem.bgSecondary

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // 面板切换标签
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        spacing: 0

                        Repeater {
                            model: ["寄存器", "内存", "栈"]
                            DebugPanelTab {
                                text: modelData
                                checked: index === root.rightPanelIndex
                                onClicked: root.rightPanelIndex = index
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // 面板内容 (StackLayout)
                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: root.rightPanelIndex

                        // ── 寄存器面板 ──
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: DesignSystem.spaceSm
                            spacing: DesignSystem.spaceXs

                            // 读取按钮
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                radius: DesignSystem.radiusSm
                                color: regReadHover.containsMouse ? DesignSystem.bgTertiary : "transparent"
                                border.width: 1
                                border.color: DesignSystem.border

                                Text {
                                    anchors.centerIn: parent
                                    text: "读取寄存器"
                                    color: DesignSystem.textSecondary
                                    font.family: DesignSystem.fontBody
                                    font.pixelSize: DesignSystem.fontSizeSm
                                }

                                MouseArea {
                                    id: regReadHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (root.tabPage) root.tabPage.readRegisters() }
                                }
                            }

                            // 寄存器列表 (可滚动)
                            ListView {
                                id: regList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 2
                                model: root.tabPage ? root.tabPage.registerModel : null

                                delegate: RowLayout {
                                    width: regList.width
                                    height: 20
                                    spacing: DesignSystem.spaceSm

                                    Text {
                                        text: model.name
                                        color: DesignSystem.accentHover
                                        font.family: DesignSystem.fontMono
                                        font.pixelSize: DesignSystem.fontSizeSm
                                        font.bold: true
                                        Layout.preferredWidth: 30
                                    }
                                    Text {
                                        text: model.value
                                        color: DesignSystem.textPrimary
                                        font.family: DesignSystem.fontMono
                                        font.pixelSize: DesignSystem.fontSizeSm
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }

                                // 空状态
                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    visible: regList.count === 0
                                    z: -1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "点击上方按钮读取"
                                        color: DesignSystem.textSecondary
                                        font.family: DesignSystem.fontBody
                                        font.pixelSize: DesignSystem.fontSizeXs
                                        opacity: 0.6
                                    }
                                }
                            }
                        }

                        // ── 内存面板 ──
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: DesignSystem.spaceSm
                            spacing: DesignSystem.spaceXs

                            // 地址输入
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: DesignSystem.spaceXs

                                TextField {
                                    id: memAddr
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    color: DesignSystem.textPrimary
                                    font.family: DesignSystem.fontMono
                                    font.pixelSize: DesignSystem.fontSizeSm
                                    placeholderText: "0x08000000"
                                    placeholderTextColor: DesignSystem.textSecondary
                                    selectByMouse: true
                                    background: Rectangle {
                                        color: DesignSystem.bgPrimary
                                        radius: DesignSystem.radiusSm
                                        border.width: 1
                                        border.color: DesignSystem.border
                                    }
                                }

                                Rectangle {
                                    width: 36; height: 26
                                    radius: DesignSystem.radiusSm
                                    color: memReadHover.containsMouse ? DesignSystem.accentHover : DesignSystem.bgTertiary
                                    border.width: 1
                                    border.color: DesignSystem.border

                                    Text {
                                        anchors.centerIn: parent
                                        text: "读"
                                        color: DesignSystem.textSecondary
                                        font.family: DesignSystem.fontBody
                                        font.pixelSize: DesignSystem.fontSizeXs
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: memReadHover
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (root.tabPage) {
                                                var addr = parseInt(memAddr.text) || 0x08000000
                                                root.tabPage.readMemory(addr, 64)
                                            }
                                        }
                                    }
                                }
                            }

                            // 读取长度
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: DesignSystem.spaceXs

                                Text {
                                    text: "长度:"
                                    color: DesignSystem.textSecondary
                                    font.family: DesignSystem.fontBody
                                    font.pixelSize: DesignSystem.fontSizeXs
                                }

                                Repeater {
                                    model: ["16", "32", "64", "128", "256"]
                                    Rectangle {
                                        Layout.preferredWidth: 28
                                        Layout.preferredHeight: 20
                                        radius: DesignSystem.radiusSm
                                        color: memSizeHover.containsMouse ? DesignSystem.bgTertiary : "transparent"
                                        border.width: 1
                                        border.color: DesignSystem.border

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: DesignSystem.textSecondary
                                            font.family: DesignSystem.fontMono
                                            font.pixelSize: 9
                                        }

                                        MouseArea {
                                            id: memSizeHover
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (root.tabPage) {
                                                    var addr = parseInt(memAddr.text) || 0x08000000
                                                    root.tabPage.readMemory(addr, parseInt(modelData))
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // 内存数据显示
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: DesignSystem.radiusSm
                                color: DesignSystem.bgTertiary
                                border.width: 1
                                border.color: DesignSystem.border

                                Flickable {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    clip: true
                                    contentHeight: memText.implicitHeight
                                    boundsBehavior: Flickable.StopAtBounds

                                    Text {
                                        id: memText
                                        width: parent.width
                                        text: root.tabPage ? root.tabPage.memoryDump : ""
                                        color: DesignSystem.textPrimary
                                        font.family: DesignSystem.fontMono
                                        font.pixelSize: DesignSystem.fontSizeXs
                                        lineHeight: 1.3
                                    }

                                    ScrollBar.vertical: ScrollBar {
                                        policy: ScrollBar.AsNeeded
                                        contentItem: Rectangle {
                                            implicitWidth: 4; radius: 2
                                            color: DesignSystem.border
                                        }
                                    }
                                }
                            }
                        }

                        // ── 调用栈面板 ──
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: DesignSystem.spaceSm
                            spacing: DesignSystem.spaceXs

                            // 刷新按钮
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                radius: DesignSystem.radiusSm
                                color: stackReadHover.containsMouse ? DesignSystem.bgTertiary : "transparent"
                                border.width: 1
                                border.color: DesignSystem.border

                                Text {
                                    anchors.centerIn: parent
                                    text: "刷新调用栈"
                                    color: DesignSystem.textSecondary
                                    font.family: DesignSystem.fontBody
                                    font.pixelSize: DesignSystem.fontSizeSm
                                }

                                MouseArea {
                                    id: stackReadHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (root.tabPage) root.tabPage.readCallStack() }
                                }
                            }

                            ListView {
                                id: stackList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 2
                                model: root.tabPage ? root.tabPage.callStackModel : null

                                delegate: ColumnLayout {
                                    width: stackList.width
                                    spacing: 1

                                    Text {
                                        text: "#" + model.index + " " + model.function
                                        color: DesignSystem.textPrimary
                                        font.family: DesignSystem.fontMono
                                        font.pixelSize: DesignSystem.fontSizeSm
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: "    " + model.address
                                        color: DesignSystem.textSecondary
                                        font.family: DesignSystem.fontMono
                                        font.pixelSize: DesignSystem.fontSizeXs
                                    }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    visible: stackList.count === 0
                                    z: -1
                                    Text {
                                        anchors.centerIn: parent
                                        text: "Halt 后查看调用栈"
                                        color: DesignSystem.textSecondary
                                        font.family: DesignSystem.fontBody
                                        font.pixelSize: DesignSystem.fontSizeXs
                                        opacity: 0.6
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════
    // 内联组件
    // ═══════════════════════════════════════════════

    /// 调试控制按钮
    component DebugCtrlBtn: Rectangle {
        property string text: ""
        property string label: ""
        property color accent: DesignSystem.accent
        signal clicked()

        width: 32; height: 26
        radius: DesignSystem.radiusSm
        color: hover.containsMouse ? DesignSystem.selected : "transparent"
        Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

        Text {
            anchors.centerIn: parent
            text: parent.text
            color: parent.accent
            font.pixelSize: 13
            font.bold: true
        }
        MouseArea {
            id: hover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
        ToolTip {
            visible: hover.containsMouse
            text: label
            delay: 500
        }
    }

    /// 右侧面板切换标签
    component DebugPanelTab: Rectangle {
        property string text: ""
        property bool checked: false
        signal clicked()

        width: tabLabel.implicitWidth + 20
        height: 30
        color: checked ? DesignSystem.bgPrimary : "transparent"

        // 底部激活指示线
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width; height: 2
            color: checked ? DesignSystem.warning : "transparent"
        }

        Text {
            id: tabLabel
            anchors.centerIn: parent
            text: parent.text
            color: checked ? DesignSystem.warning : DesignSystem.textSecondary
            font.family: DesignSystem.fontBody
            font.pixelSize: DesignSystem.fontSizeSm
            font.bold: checked
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}