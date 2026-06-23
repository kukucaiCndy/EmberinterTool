import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EmberInter 1.0
import EmberDesign 1.0

/// 终端/SSH Tab — TerminalView + 状态条 + 命令栏
Rectangle {
    id: root
    anchors.fill: parent
    color: DesignSystem.bgPrimary

    property var tabPage: null

    // tabPage 赋值后立即 attachView
    onTabPageChanged: {
        console.log("[TerminalTab] onTabPageChanged: tabPage=", tabPage, "termView=", termView)
        if (tabPage && termView) {
            console.log("[TerminalTab] calling tabPage.attachView(termView)")
            tabPage.attachView(termView)
        }
    }

    // 终端配色方案
    readonly property var colorSchemes: DesignSystem.terminalSchemes

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // ─────────────────────────────────────────────
        // 状态栏 (h=32)
        // ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: DesignSystem.termStatusHeight
            color: DesignSystem.bgSecondary

            Rectangle {
                anchors.bottom: parent.bottom; width: parent.width; height: 1
                color: DesignSystem.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: DesignSystem.spaceSm; anchors.rightMargin: 6
                spacing: DesignSystem.spaceSm

                // 状态点 + 状态文字 (与 SerialTab 统一风格)
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 6; height: 6; radius: 3
                        color: (root.tabPage && root.tabPage.connected) ? DesignSystem.success : DesignSystem.textSecondary
                        SequentialAnimation on opacity {
                            running: root.tabPage && root.tabPage.connected
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 0.5; duration: 1000; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 0.5; to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
                        }
                    }
                    Text {
                        text: (root.tabPage && root.tabPage.connected) ? "运行中" : "已停止"
                        color: (root.tabPage && root.tabPage.connected) ? DesignSystem.success : DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeSm
                    }
                }

                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 18; color: DesignSystem.border }

                // Shell 名称
                Text {
                    text: termView.shellName
                    color: DesignSystem.accent
                    font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeSm
                    elide: Text.ElideRight; Layout.fillWidth: true
                }

                // 终端尺寸
                Text {
                    text: termView.columns + "\u00D7" + termView.rows
                    color: DesignSystem.textSecondary
                    font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeXs
                }

                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 18; color: DesignSystem.border }

                // 字体缩放
                RowLayout { spacing: 2
                    TermBtn { text: "A-"; onClicked: { if (termView.fontSize > 8) termView.fontSize-- } }
                    Text {
                        text: termView.fontSize + "px"; color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeXs
                        Layout.preferredWidth: 28
                    }
                    TermBtn { text: "A+"; onClicked: { if (termView.fontSize < 48) termView.fontSize++ } }
                }

                // 配色方案选择
                ComboBox {
                    id: colorSchemeCombo
                    Layout.preferredWidth: 120; Layout.preferredHeight: 22
                    model: root.colorSchemes.map(function(s) { return s.name })
                    currentIndex: 0
                    font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeXs

                    background: Rectangle {
                        color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                        border.width: 1; border.color: DesignSystem.border
                    }
                    contentItem: Text {
                        leftPadding: 6; text: parent.displayText
                        color: DesignSystem.textPrimary; font: parent.font; verticalAlignment: Text.AlignVCenter
                    }
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0 && currentIndex < root.colorSchemes.length) {
                            var cs = root.colorSchemes[currentIndex]
                            termView.setColorScheme(cs.name)
                        }
                    }
                }
            }
        }

        // ─────────────────────────────────────────────
        // 终端视图 (内置滚动, 不需要 Flickable)
        // ─────────────────────────────────────────────
        TerminalView {
            id: termView
            Layout.fillWidth: true
            Layout.fillHeight: true
            fontSize: DesignSystem.fontSizeMd
            focus: true
            activeFocusOnTab: true

            Component.onCompleted: {
                console.log("[TerminalTab] termView.Component.onCompleted: tabPage=", root.tabPage)
                if (root.tabPage) {
                    console.log("[TerminalTab] calling tabPage.attachView(termView)")
                    root.tabPage.attachView(termView)
                }
                // 应用初始配色方案 (index 0)
                if (root.colorSchemes.length > 0) {
                    var cs = root.colorSchemes[0]
                    termView.setColorScheme(cs.name)
                }
                // 真正获取焦点，否则键盘事件无法到达终端视图
                termView.forceActiveFocus()
            }
        }

        // ─────────────────────────────────────────────
        // 命令输入栏 (h=36)
        // ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: DesignSystem.termCmdBarHeight
            color: DesignSystem.bgSecondary

            Rectangle {
                anchors.top: parent.top; width: parent.width; height: 1
                color: DesignSystem.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: DesignSystem.spaceSm; anchors.rightMargin: 6
                spacing: DesignSystem.spaceSm

                // 提示符
                Text {
                    text: DesignSystem.iconChevron; color: DesignSystem.success
                    font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeXl; font.bold: true
                }

                // 命令输入
                TextField {
                    id: cmdInput
                    Layout.fillWidth: true; Layout.preferredHeight: 26
                    placeholderText: (root.tabPage && root.tabPage.connected)
                        ? "输入命令 (Enter 发送)..."
                        : "未连接"
                    placeholderTextColor: DesignSystem.textSecondary
                    color: DesignSystem.textPrimary
                    font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeMd
                    selectByMouse: true

                    background: Rectangle {
                        color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                        border.width: 1; border.color: DesignSystem.border
                    }
                    onAccepted: {
                        if (root.tabPage && root.tabPage.connected && text.length > 0) {
                            root.tabPage.writeInput(text + "\r\n"); text = ""
                        }
                    }
                }

                // 发送按钮
                Rectangle {
                    width: sendText.implicitWidth + 16; height: 26; radius: DesignSystem.radiusSm
                    color: sendHover.containsMouse ? DesignSystem.accentHover : DesignSystem.accent
                    opacity: (root.tabPage && root.tabPage.connected) ? 1.0 : 0.4

                    Text {
                        id: sendText; anchors.centerIn: parent
                        text: "发送"; color: DesignSystem.textInverse
                        font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeSm; font.bold: true
                    }
                    MouseArea {
                        id: sendHover; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.tabPage && root.tabPage.connected && cmdInput.text.length > 0) {
                                root.tabPage.writeInput(cmdInput.text + "\r\n"); cmdInput.text = ""
                            }
                        }
                    }
                }
            }
        }
    }

    // ── 终端按钮组件 ──
    component TermBtn: Rectangle {
        property string text: ""; signal clicked()
        width: 26; height: 22; radius: DesignSystem.radiusSm
        color: btnHover.containsMouse ? DesignSystem.bgTertiary : "transparent"
        Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

        Text {
            anchors.centerIn: parent; text: parent.text
            color: DesignSystem.textSecondary
            font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeXs; font.bold: true
        }
        MouseArea {
            id: btnHover; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor; onClicked: parent.clicked()
        }
    }

}
