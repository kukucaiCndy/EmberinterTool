import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EmberDesign 1.0

/// 关于 / 版本 / 自动更新
Popup {
    id: root
    width: 540
    height: 540
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    background: Rectangle {
        color: DesignSystem.bgSecondary
        radius: DesignSystem.radiusLg
        border.width: 1
        border.color: DesignSystem.border
    }

    // 更新状态
    property bool checking: false
    property string updateStatus: ""
    property string latestVersion: "v1.2.0"
    property string currentVersion: "v1.2.0"
    property string buildDate: "2026.06.10"
    property bool updateAvailable: false
    property bool autoCheckEnabled: false

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ══ 标题栏 ══
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 12
                spacing: DesignSystem.spaceSm

                Text {
                    text: DesignSystem.iconInfo + " 关于"
                    color: DesignSystem.textPrimary
                    font.family: DesignSystem.fontBody
                    font.pixelSize: DesignSystem.fontSize2xl
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                RectBtn {
                    width: 28; height: 28
                    icon: DesignSystem.iconClose
                    hoverColor: DesignSystem.error
                    onClicked: root.close()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: DesignSystem.border
        }

        // ══ 滚动内容 ══
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: aboutCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: DesignSystem.border
                }
            }

            ColumnLayout {
                id: aboutCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 24
                spacing: DesignSystem.spaceLg

                // Logo
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 72; height: 72
                    radius: DesignSystem.radiusLg
                    color: DesignSystem.accent10
                    border.width: 1
                    border.color: "#332F81F7"

                    Text {
                        anchors.centerIn: parent
                        text: "\u5C18" // 尘
                        color: DesignSystem.accent
                        font.family: DesignSystem.fontHead
                        font.pixelSize: 32
                        font.bold: true
                    }
                }

                // 应用名
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "EmberInterDebugTool"
                    color: DesignSystem.textPrimary
                    font.family: DesignSystem.fontHead
                    font.pixelSize: DesignSystem.fontSize3xl
                    font.bold: true
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "人机协同 Agent 工作台"
                    color: DesignSystem.textSecondary
                    font.family: DesignSystem.fontBody
                    font.pixelSize: DesignSystem.fontSizeMd
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "微尘藏星火，终端孕尘智\n以尘之微，铸智之核！"
                    color: DesignSystem.accentHover
                    font.family: DesignSystem.fontBody
                    font.pixelSize: DesignSystem.fontSizeLg
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.4
                }

                // 版本信息卡
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    radius: DesignSystem.radiusMd
                    color: DesignSystem.bgTertiary
                    border.width: 1
                    border.color: DesignSystem.border

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: DesignSystem.spaceXs

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: root.currentVersion + "  Build " + root.buildDate
                            color: DesignSystem.textPrimary
                            font.family: DesignSystem.fontBody
                            font.pixelSize: DesignSystem.fontSizeLg
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Qt 6.10.1 | MSYS2 mingw64 | C++17"
                            color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody
                            font.pixelSize: DesignSystem.fontSizeSm
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: DesignSystem.border
                }

                // ── 项目理念 ──
                SectionTitle { text: "项目理念" }

                Text {
                    Layout.fillWidth: true
                    text: "专为嵌入式开发打造的人机协同调试工具，支持串口、终端、SSH、ST-Link/J-Link 等多种调试方式。\n" +
                          "核心采用 C++17 开发，GUI 基于 Qt6 QML 构建，保持现代简约的设计风格。"
                    color: DesignSystem.textSecondary
                    font.family: DesignSystem.fontBody
                    font.pixelSize: DesignSystem.fontSizeMd
                    lineHeight: 1.5
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: DesignSystem.border
                }

                // ── 自动更新 ──
                SectionTitle { text: "自动更新" }

                RowLayout {
                    spacing: DesignSystem.spaceMd
                    Text {
                        text: "自动检查更新"
                        color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeMd
                    }
                    Switch {
                        id: autoCheckSwitch
                        checked: root.autoCheckEnabled
                        Layout.alignment: Qt.AlignVCenter
                        onCheckedChanged: root.autoCheckEnabled = checked
                        indicator: SwitchIndicator {}
                    }
                    Text {
                        text: root.autoCheckEnabled ? "已开启" : "已关闭"
                        color: root.autoCheckEnabled ? DesignSystem.success : DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeSm
                    }
                    Item { Layout.fillWidth: true }
                }

                // 检查更新按钮
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: DesignSystem.radiusMd
                    color: DesignSystem.bgTertiary
                    border.width: 1
                    border.color: DesignSystem.border

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: DesignSystem.spaceSm

                        Item {
                            width: 16; height: 16
                            visible: root.checking

                            RotationAnimator on rotation {
                                running: root.checking
                                from: 0; to: 360
                                duration: 1000
                                loops: Animation.Infinite
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width: 14; height: 14
                                radius: 7
                                color: "transparent"
                                border.width: 2
                                border.color: DesignSystem.accent
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 4; height: 4
                                    radius: 2
                                    color: DesignSystem.accent
                                }
                            }
                        }

                        Text {
                            text: root.checking ? "检查中..." : "检查更新"
                            color: root.checking ? DesignSystem.accentHover : DesignSystem.textPrimary
                            font.family: DesignSystem.fontBody
                            font.pixelSize: DesignSystem.fontSizeLg
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!root.checking) {
                                root.checking = true
                                checkUpdateTimer.start()
                            }
                        }
                    }
                }

                Timer {
                    id: checkUpdateTimer
                    interval: 1500
                    repeat: false
                    onTriggered: {
                        root.checking = false
                        root.updateAvailable = true
                        root.latestVersion = "v3.1.0"
                    }
                }

                // 更新状态卡片
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.updateAvailable ? 120 : 0
                    visible: root.updateAvailable
                    clip: true
                    radius: DesignSystem.radiusMd
                    color: DesignSystem.success15
                    border.width: 1
                    border.color: "#333FB950"

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: DesignSystem.animSlow; easing.type: Easing.InOutQuad }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: DesignSystem.spaceMd
                        spacing: DesignSystem.spaceSm

                        RowLayout {
                            spacing: DesignSystem.spaceSm
                            Text {
                                text: DesignSystem.iconCheck
                                color: DesignSystem.success
                                font.pixelSize: 14
                            }
                            Text {
                                text: "新版本可用: " + root.latestVersion
                                color: DesignSystem.success
                                font.family: DesignSystem.fontBody
                                font.pixelSize: DesignSystem.fontSizeLg
                                font.bold: true
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "修复若干已知问题，优化终端渲染性能，增加新功能。"
                            color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody
                            font.pixelSize: DesignSystem.fontSizeMd
                            wrapMode: Text.WordWrap
                        }

                        Rectangle {
                            width: 100; height: 30
                            radius: DesignSystem.radiusSm
                            color: DesignSystem.success

                            Text {
                                anchors.centerIn: parent
                                text: "立即更新"
                                color: DesignSystem.textInverse
                                font.family: DesignSystem.fontBody
                                font.pixelSize: DesignSystem.fontSizeMd
                                font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: DesignSystem.border
                }

                // ── 许可 ──
                SectionTitle { text: "许可" }

                Text {
                    Layout.fillWidth: true
                    text: "自有许可 — 仅限非商业使用\n" +
                          "Qt6 使用 LGPLv3 动态链接，避免 GPL 传染性。"
                    color: DesignSystem.textSecondary
                    font.family: DesignSystem.fontBody
                    font.pixelSize: DesignSystem.fontSizeMd
                    lineHeight: 1.5
                    wrapMode: Text.WordWrap
                }

                Item { Layout.preferredHeight: DesignSystem.spaceSm }
            }
        }
    }

    // ═══════════════════════════════════════════════
    // 内联组件
    // ═══════════════════════════════════════════════

    component SectionTitle: Text {
        text: parent.text
        color: DesignSystem.textPrimary
        font.family: DesignSystem.fontBody
        font.pixelSize: DesignSystem.fontSizeXl
        font.bold: true
    }

    component SwitchIndicator: Rectangle {
        implicitWidth: 32; implicitHeight: 18
        radius: 9
        color: parent.parent.checked ? DesignSystem.accent : DesignSystem.bgTertiary
        border.width: 1
        border.color: parent.parent.checked ? DesignSystem.accent : DesignSystem.border
        Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

        Rectangle {
            x: parent.parent.checked ? parent.width - width - 2 : 2
            y: (parent.height - height) / 2
            width: 14; height: 14
            radius: 7
            color: DesignSystem.textPrimary
            Behavior on x { NumberAnimation { duration: DesignSystem.animFast; easing.type: Easing.InOutQuad } }
        }
    }

    component RectBtn: Rectangle {
        property string icon: ""
        property color hoverColor: DesignSystem.error
        signal clicked()
        radius: DesignSystem.radiusSm
        color: hover.containsMouse ? hoverColor + "40" : "transparent"
        Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

        Text {
            anchors.centerIn: parent
            text: icon
            color: hover.containsMouse ? hoverColor : DesignSystem.textSecondary
            font.pixelSize: 14
            font.bold: true
        }
        MouseArea {
            id: hover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}