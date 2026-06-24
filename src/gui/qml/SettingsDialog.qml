import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EmberDesign 1.0

/// 设置对话框 — 外观、日志、更新、快捷键
Popup {
    id: root
    width: 520
    height: 460
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    // 居中显示: 使用 Overlay.overlay (弹出层所在的实际父项)
    // onOpened 时 Overlay 已就绪, 此时计算居中位置最准确
    onOpened: {
        var p = parent
        if (p) {
            root.x = (p.width - root.width) / 2
            root.y = (p.height - root.height) / 2
        }
        console.log("[SettingsDialog] opened, loading settings...")
        try {
            var s = appCore.loadSettings()
            if (s) {
                // 字体大小映射: 10->0, 12->1, 14->2, 16->3, 18->4
                var fs = s.fontSize || 12
                var idx = 1
                if (fs <= 10) idx = 0
                else if (fs <= 12) idx = 1
                else if (fs <= 14) idx = 2
                else if (fs <= 16) idx = 3
                else idx = 4
                root.fontSizeIdx = idx
                root.maxLogLines = s.maxLogLines || 10000
                root.autoScroll = s.autoScroll !== undefined ? s.autoScroll : true
                root.autoCheckUpdate = s.autoCheckUpdate !== undefined ? s.autoCheckUpdate : true
                fontSizeSlider.value = root.fontSizeIdx
                autoScrollSwitch.checked = root.autoScroll
                updateSwitch.checked = root.autoCheckUpdate
                // 应用字体缩放
                DesignSystem.fontScale = root.fontSizeIdx - 1
                console.log("[SettingsDialog] settings loaded: fontSize=" + root.fontSizeIdx)
            }
        } catch (e) {
            console.log("[SettingsDialog] ERROR loading settings: " + e)
        }
    }

    background: Rectangle {
        color: DesignSystem.bgSecondary
        radius: DesignSystem.radiusLg
        border.width: 1
        border.color: DesignSystem.border
    }

    property int fontSizeIdx: 1       // 默认 12px
    property int maxLogLines: 10000
    property bool autoScroll: true
    property bool autoCheckUpdate: false

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
                    text: DesignSystem.iconSettings + " 设置"
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
            contentHeight: contentCol.implicitHeight
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
                id: contentCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 20
                spacing: DesignSystem.spaceMd

                // ── 外观 ──
                SectionHeader { text: "外观" }

                SettingRow {
                    label: "主题"
                    ComboBox {
                        id: themeCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        model: ["暗黑 (Default)", "明亮 (Light)"]
                        currentIndex: DesignSystem.theme
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeMd
                        background: SettingCBoxBg {}
                        contentItem: SettingCBoxText { t: parent.displayText }
                        indicator: SettingCBoxIndicator {}
                        onCurrentIndexChanged: {
                            DesignSystem.setTheme(currentIndex)
                        }
                    }
                }

                SettingRow {
                    label: "字体大小"
                    Slider {
                        id: fontSizeSlider
                        Layout.fillWidth: true
                        from: 0; to: 4
                        stepSize: 1
                        value: root.fontSizeIdx
                        onValueChanged: {
                            root.fontSizeIdx = value
                            // 实时生效: 字体大小索引映射到 fontScale (0=10px, 1=12px, 2=14px, 3=16px, 4=18px)
                            // DesignSystem.fontSizeMd 基准是 12+fontScale, 所以 fontScale = fontSizeIdx - 1
                            DesignSystem.fontScale = value - 1
                        }

                        background: Rectangle {
                            x: fontSizeSlider.leftPadding
                            y: fontSizeSlider.topPadding + fontSizeSlider.availableHeight / 2 - 2
                            implicitWidth: 200
                            implicitHeight: 4
                            width: fontSizeSlider.availableWidth
                            height: 4
                            radius: 2
                            color: DesignSystem.bgTertiary

                            Rectangle {
                                width: fontSizeSlider.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: DesignSystem.accent
                            }
                        }
                        handle: Rectangle {
                            x: fontSizeSlider.leftPadding + fontSizeSlider.visualPosition * (fontSizeSlider.availableWidth - width)
                            y: fontSizeSlider.topPadding + fontSizeSlider.availableHeight / 2 - height / 2
                            implicitWidth: 16; implicitHeight: 16
                            radius: 8
                            color: DesignSystem.accent
                            border.width: 2
                            border.color: DesignSystem.bgPrimary
                        }
                    }
                    Text {
                        text: [10, 12, 14, 16, 18][root.fontSizeIdx] + "px"
                        color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontMono
                        font.pixelSize: DesignSystem.fontSizeMd
                        Layout.preferredWidth: 36
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: DesignSystem.border
                }

                // ── 日志 ──
                SectionHeader { text: "日志" }

                SettingRow {
                    label: "最大行数"
                    TextField {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        text: root.maxLogLines.toString()
                        color: DesignSystem.textPrimary
                        font.family: DesignSystem.fontMono
                        font.pixelSize: DesignSystem.fontSizeMd
                        selectByMouse: true
                        validator: IntValidator { bottom: 100; top: 1000000 }
                        onTextChanged: {
                            var v = parseInt(text)
                            if (!isNaN(v)) root.maxLogLines = v
                        }
                        background: Rectangle {
                            color: DesignSystem.bgPrimary
                            radius: DesignSystem.radiusMd
                            border.width: 1
                            border.color: DesignSystem.border
                        }
                    }
                }

                SettingRow {
                    label: "自动滚动"
                    Switch {
                        id: autoScrollSwitch
                        checked: root.autoScroll
                        Layout.alignment: Qt.AlignVCenter
                        onCheckedChanged: root.autoScroll = checked
                        indicator: SettingSwitchIndicator {}
                    }
                    Text {
                        text: root.autoScroll ? "开启" : "关闭"
                        color: root.autoScroll ? DesignSystem.success : DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeSm
                    }
                    Item { Layout.fillWidth: true }
                }

                SettingRow {
                    label: "日志导出"
                    ComboBox {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        model: ["JSON", "纯文本"]
                        currentIndex: 0
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeMd
                        background: SettingCBoxBg {}
                        contentItem: SettingCBoxText { t: parent.displayText }
                        indicator: SettingCBoxIndicator {}
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: DesignSystem.border
                }

                // ── 更新 ──
                SectionHeader { text: "更新" }

                SettingRow {
                    label: "自动检查更新"
                    Switch {
                        id: updateSwitch
                        checked: root.autoCheckUpdate
                        Layout.alignment: Qt.AlignVCenter
                        onCheckedChanged: {
                            root.autoCheckUpdate = checked
                            appCore.saveAutoCheckUpdate(checked)
                        }
                        indicator: SettingSwitchIndicator {}
                    }
                    Text {
                        text: root.autoCheckUpdate ? "开启" : "关闭"
                        color: root.autoCheckUpdate ? DesignSystem.success : DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeSm
                    }
                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: DesignSystem.border
                }

                // ── 快捷键 ──
                SectionHeader { text: "快捷键" }

                Repeater {
                    model: [
                        { key: "Ctrl+T", desc: "新建会话" },
                        { key: "Ctrl+W", desc: "关闭当前 Tab" },
                        { key: "Ctrl+1~9", desc: "切换到第 N 个 Tab" },
                        { key: "Escape", desc: "关闭当前弹窗" },
                        { key: "Ctrl+Enter", desc: "发送面板发送数据" }
                    ]
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: DesignSystem.spaceMd
                        Rectangle {
                            Layout.preferredWidth: shortcutKey.implicitWidth + 12
                            Layout.preferredHeight: 22
                            radius: DesignSystem.radiusSm
                            color: DesignSystem.bgTertiary
                            border.width: 1
                            border.color: DesignSystem.border
                            Text {
                                id: shortcutKey
                                anchors.centerIn: parent
                                text: modelData.key
                                color: DesignSystem.accentHover
                                font.family: DesignSystem.fontMono
                                font.pixelSize: DesignSystem.fontSizeXs
                                font.bold: true
                            }
                        }
                        Text {
                            text: modelData.desc
                            color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody
                            font.pixelSize: DesignSystem.fontSizeSm
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        // ══ 底部按钮 ══
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "transparent"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: DesignSystem.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: DesignSystem.spaceSm

                // 重置默认
                Rectangle {
                    width: resetText.implicitWidth + 24
                    height: 32
                    radius: DesignSystem.radiusMd
                    color: resetHover.containsMouse ? DesignSystem.bgTertiary : "transparent"
                    border.width: 1
                    border.color: DesignSystem.border
                    Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

                    Text {
                        id: resetText
                        anchors.centerIn: parent
                        text: "重置默认"
                        color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeLg
                    }
                    MouseArea {
                        id: resetHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.fontSizeIdx = 1
                            root.maxLogLines = 10000
                            root.autoScroll = true
                            root.autoCheckUpdate = false
                            fontSizeSlider.value = 1
                            autoScrollSwitch.checked = true
                            updateSwitch.checked = false
                            // 重置主题和字体
                            DesignSystem.setTheme(0)
                            DesignSystem.fontScale = 0
                            themeCombo.currentIndex = 0
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // 保存
                Rectangle {
                    width: saveText.implicitWidth + 32
                    height: 32
                    radius: DesignSystem.radiusMd
                    color: saveHover.containsMouse ? DesignSystem.accentHover : DesignSystem.accent
                    Behavior on color { ColorAnimation { duration: DesignSystem.animFast } }

                    Text {
                        id: saveText
                        anchors.centerIn: parent
                        text: "保存"
                        color: DesignSystem.textInverse
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeLg
                        font.bold: true
                    }
                    MouseArea {
                        id: saveHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // 字体大小: 索引 0-4 对应 10,12,14,16,18 px
                            var fontSize = [10, 12, 14, 16, 18][root.fontSizeIdx]
                            appCore.saveSettings(fontSize, root.maxLogLines, root.autoScroll)
                            root.close()
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════
    // 内联组件
    // ═══════════════════════════════════════════════

    component SectionHeader: Text {
        color: DesignSystem.textPrimary
        font.family: DesignSystem.fontBody
        font.pixelSize: DesignSystem.fontSizeXl
        font.bold: true
        Layout.topMargin: DesignSystem.spaceSm
    }

    component SettingRow: RowLayout {
        property string label: ""
        spacing: 10

        Text {
            text: parent.label
            color: DesignSystem.textSecondary
            font.family: DesignSystem.fontBody
            font.pixelSize: DesignSystem.fontSizeMd
            Layout.preferredWidth: 100
        }
    }

    component SettingCBoxBg: Rectangle {
        color: DesignSystem.bgPrimary
        radius: DesignSystem.radiusMd
        border.width: 1
        border.color: DesignSystem.border
    }

    component SettingCBoxText: Text {
        property string t: ""
        leftPadding: 8
        text: t
        color: DesignSystem.textPrimary
        font.family: DesignSystem.fontBody
        font.pixelSize: DesignSystem.fontSizeMd
        verticalAlignment: Text.AlignVCenter
    }

    component SettingCBoxIndicator: Text {
        x: parent.width - 14
        y: (parent.height - 10) / 2
        text: "\u25BE"
        color: DesignSystem.textSecondary
        font.pixelSize: 10
    }

    component SettingSwitchIndicator: Rectangle {
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