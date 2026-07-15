import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import EmberDesign 1.0

/// 串口 Tab — 日志查看 + 数据发送面板
/// 设计对齐: docs/design.pen 中的 SerialTerminalPage
Rectangle {
    id: root
    anchors.fill: parent
    color: DesignSystem.bgPrimary

    property var tabPage: null
    property bool selecting: false
    property int selStartIdx: -1
    property int selEndIdx: -1

    // 断开状态下，回车键重新连接
    focus: true
    Keys.onReturnPressed: {
        if (root.tabPage && !root.tabPage.connected) {
            root.tabPage.reconnect()
        }
    }

    // 隐藏的 TextArea 用于复制选中文本到剪贴板
    TextArea {
        id: clipboardHelper
        visible: false
        width: 0; height: 0
    }

    onTabPageChanged: {
        console.log("[SerialTab] onTabPageChanged: tabPage=", tabPage)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ─────────────────────────────────────────────
        // 工具栏 (h=36)
        // ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: DesignSystem.toolbarHeight
            color: DesignSystem.bgPrimary

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: DesignSystem.spaceSm
                anchors.rightMargin: DesignSystem.spaceSm
                spacing: DesignSystem.spaceSm

                // 过滤器输入
                Rectangle {
                    Layout.preferredWidth: 180; Layout.preferredHeight: 26
                    radius: DesignSystem.radiusSm; color: DesignSystem.bgTertiary
                    border.width: 1; border.color: DesignSystem.border

                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 6; spacing: 4
                        Text {
                            text: DesignSystem.iconSearch; color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontMono; font.pixelSize: 10
                        }
                        TextInput {
                            id: filterInput
                            Layout.fillWidth: true; Layout.fillHeight: true
                            verticalAlignment: TextInput.AlignVCenter
                            color: DesignSystem.textPrimary
                            font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeSm
                            Text {
                                anchors.fill: parent
                                text: "过滤关键字..."; color: DesignSystem.textSecondary
                                font: parent.font; visible: !filterInput.text
                                verticalAlignment: Text.AlignVCenter
                            }
                            onTextChanged: {
                                if (root.tabPage) root.tabPage.setFilter(text)
                            }
                        }
                    }
                }

                // 分隔线
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: DesignSystem.border }

                // HEX 切换
                ToggleBtn {
                    text: "HEX"
                    checked: root.tabPage && root.tabPage.hexMode
                    onClicked: {
                        console.log("[SerialTab] HEX clicked, tabPage=", root.tabPage, "hexMode=", root.tabPage ? root.tabPage.hexMode : "N/A")
                        if (root.tabPage) root.tabPage.setHexMode(!root.tabPage.hexMode)
                    }
                    tooltip: "十六进制模式"
                }

                // 时间戳切换
                ToggleBtn {
                    text: "TS"
                    checked: root.tabPage ? root.tabPage.showTimestamp : true
                    onClicked: {
                        console.log("[SerialTab] TS clicked, tabPage=", root.tabPage, "showTimestamp=", root.tabPage ? root.tabPage.showTimestamp : "N/A")
                        if (root.tabPage) root.tabPage.setShowTimestamp(!root.tabPage.showTimestamp)
                    }
                    tooltip: "显示时间戳"
                }

                // 暂停/继续
                ToggleBtn {
                    text: root.tabPage && root.tabPage.paused ? DesignSystem.iconPlay : DesignSystem.iconPause
                    checked: root.tabPage && root.tabPage.paused
                    onClicked: {
                        console.log("[SerialTab] Pause clicked, tabPage=", root.tabPage)
                        if (root.tabPage) root.tabPage.setPaused(!root.tabPage.paused)
                    }
                    tooltip: root.tabPage && root.tabPage.paused ? "继续" : "暂停"
                    warnColor: true
                }

                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 20; color: DesignSystem.border }

                // 清空
                ToolBtn {
                    text: DesignSystem.iconClose; tooltip: "清空日志 (不可撤销)"
                    onClicked: {
                        console.log("[SerialTab] Clear clicked, tabPage=", root.tabPage)
                        if (root.tabPage) root.tabPage.clearLogs()
                    }
                    dangerColor: true
                }

                // 导出
                ToolBtn {
                    text: DesignSystem.iconExport; tooltip: "导出日志 JSON"
                    onClicked: {
                        if (root.tabPage) {
                            exportDialog.open()
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // 端口名称 + 连接状态
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 6; height: 6; radius: 3
                        color: (root.tabPage && root.tabPage.connected) ? DesignSystem.success : DesignSystem.textSecondary
                        // 呼吸脉冲 (与 TerminalTab 统一参数)
                        SequentialAnimation on opacity {
                            running: root.tabPage && root.tabPage.connected
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 0.5; duration: 1000; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 0.5; to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
                        }
                    }
                    Text {
                        text: root.tabPage ? root.tabPage.portName : ""
                        color: DesignSystem.textPrimary
                        font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeSm
                    }
                    Text {
                        text: root.tabPage && root.tabPage.connected ? "已连接" : "未连接"
                        color: (root.tabPage && root.tabPage.connected) ? DesignSystem.success : DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeXs
                    }
                }
            }
        }

        // 工具栏底部边框
        Rectangle { Layout.fillWidth: true; height: 1; color: DesignSystem.border }

        // ─────────────────────────────────────────────
        // 日志视图 (ListView)
        // ─────────────────────────────────────────────
        ListView {
            id: logList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.tabPage ? root.tabPage.logModel : null
            spacing: 0
            cacheBuffer: 4000
            pixelAligned: true

            // 自动滚动: 仅在非选择状态且 autoScroll 时滚动到底部
            Connections {
                target: logList.model
                function onRowsInserted() {
                    if (root.tabPage && root.tabPage.autoScroll && !root.selecting)
                        logList.positionViewAtEnd()
                }
            }

            delegate: Item {
                width: logList.width
                height: DesignSystem.logLineHeight

                // 选中高亮背景
                Rectangle {
                    anchors.fill: parent
                    color: {
                        if (root.selStartIdx < 0) return "transparent"
                        var s = Math.min(root.selStartIdx, root.selEndIdx)
                        var e = Math.max(root.selStartIdx, root.selEndIdx)
                        return (index >= s && index <= e) ? DesignSystem.accent15 : "transparent"
                    }
                }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 6
                    text: model.display
                    color: model.color
                    font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeMd
                    elide: Text.ElideNone
                    verticalAlignment: Text.AlignVCenter
                    renderType: Text.NativeRendering
                }
            }

            // 鼠标选择处理 (覆盖在 ListView 上，阻止 Flickable 拦截拖动)
            MouseArea {
                id: selectionArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.IBeamCursor
                preventStealing: true
                propagateComposedEvents: false

                onPressed: function(mouse) {
                    var y = mouse.y + logList.contentY
                    var idx = logList.indexAt(mouse.x, y)
                    // indexAt 可能返回 -1，用手动计算兜底
                    if (idx < 0 && logList.count > 0) {
                        idx = Math.floor(y / DesignSystem.logLineHeight)
                        idx = Math.max(0, Math.min(idx, logList.count - 1))
                    }
                    if (idx >= 0) {
                        root.selecting = true
                        root.selStartIdx = idx
                        root.selEndIdx = idx
                    }
                }
                onPositionChanged: function(mouse) {
                    if (!root.selecting) return
                    var y = mouse.y + logList.contentY
                    var idx = logList.indexAt(mouse.x, y)
                    if (idx < 0 && logList.count > 0) {
                        idx = Math.floor(y / DesignSystem.logLineHeight)
                        idx = Math.max(0, Math.min(idx, logList.count - 1))
                    }
                    if (idx >= 0) root.selEndIdx = idx
                }
                onReleased: {
                    if (root.selecting) {
                        var s = Math.min(root.selStartIdx, root.selEndIdx)
                        var e = Math.max(root.selStartIdx, root.selEndIdx)
                        var lines = []
                        for (var i = s; i <= e; i++) {
                            var text = logList.model.getText(i)
                            if (text) lines.push(text)
                        }
                        if (lines.length > 0) {
                            clipboardHelper.text = lines.join("\n")
                            clipboardHelper.selectAll()
                            clipboardHelper.copy()
                        }
                        root.selecting = false
                    }
                }
                onWheel: function(wheel) {
                    // 滚轮事件转发给 ListView
                    wheel.accepted = false
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AlwaysOn
                contentItem: Rectangle {
                    implicitWidth: 6; radius: 3
                    color: DesignSystem.border; opacity: 0.8
                }
            }

            // 空状态
            Rectangle {
                anchors.fill: parent; color: DesignSystem.bgPrimary
                visible: logList.count === 0
                ColumnLayout {
                    anchors.centerIn: parent; spacing: DesignSystem.spaceSm
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.tabPage && root.tabPage.connected ? "等待数据..." : "会话未连接"
                        color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody; font.pixelSize: 13
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.tabPage && root.tabPage.connected
                            ? "串口已打开，等待接收数据"
                            : "请通过连接向导创建串口会话"
                        color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeSm
                        opacity: 0.5
                    }
                }
            }
        }

        // ─────────────────────────────────────────────
        // 发送面板 (h=80, 多行输入)
        // ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: DesignSystem.sendPanelHeight
            color: DesignSystem.bgSecondary

            Rectangle {
                anchors.top: parent.top; width: parent.width; height: 1
                color: DesignSystem.border
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: DesignSystem.spaceSm
                spacing: DesignSystem.spaceSm

                // 换行符选择 (固定宽度，左侧)
                ComboBox {
                    id: appendCombo
                    Layout.preferredWidth: 70; Layout.preferredHeight: 32
                    model: ["CRLF", "LF", "CR", "\u2205"]
                    currentIndex: 0

                    background: Rectangle {
                        color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                        border.width: 1; border.color: DesignSystem.border
                    }
                    contentItem: Text {
                        leftPadding: 6; text: appendCombo.displayText
                        color: DesignSystem.textPrimary
                        font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeSm
                        verticalAlignment: Text.AlignVCenter
                    }
                    indicator: Text {
                        x: appendCombo.width - 14
                        y: (appendCombo.height - 10) / 2
                        text: "\u25BE"; color: DesignSystem.textSecondary; font.pixelSize: 10
                    }
                }

                // 多行输入框
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    radius: DesignSystem.radiusMd; color: DesignSystem.bgTertiary
                    border.width: 1
                    border.color: (root.tabPage && root.tabPage.connected) ? DesignSystem.accent : DesignSystem.border

                    ScrollView {
                        anchors.fill: parent; anchors.margins: 4
                        clip: true; ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        TextArea {
                            id: sendInput
                            width: parent.width
                            placeholderText: "输入要发送的数据... Ctrl+Enter 发送"
                            placeholderTextColor: DesignSystem.textSecondary
                            color: DesignSystem.textPrimary
                            font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeLg
                            wrapMode: TextEdit.WrapAnywhere
                            selectByMouse: true
                            background: null

                            Keys.onPressed: function(event) {
                                if (event.key === Qt.Key_Return
                                    && (event.modifiers & Qt.ControlModifier)) {
                                    sendData(); event.accepted = true
                                }
                            }
                        }
                    }
                }

                // 发送按钮 (固定宽度，右侧)
                Rectangle {
                    Layout.preferredWidth: 64; Layout.preferredHeight: 32
                    radius: DesignSystem.radiusMd
                    color: {
                        if (!(root.tabPage && root.tabPage.connected))
                            return DesignSystem.bgTertiary
                        return sendBtnHover.containsMouse ? DesignSystem.accentHover : DesignSystem.accent
                    }
                    Behavior on color { ColorAnimation { duration: DesignSystem.animNormal } }

                    Text {
                        anchors.centerIn: parent
                        text: "发送"; color: DesignSystem.textInverse
                        font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeMd; font.bold: true
                    }
                    MouseArea {
                        id: sendBtnHover; anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: sendData()
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════
    // 导出文件对话框
    // ═══════════════════════════════════════════════
    FileDialog {
        id: exportDialog
        title: "导出日志"
        defaultSuffix: "json"
        fileMode: FileDialog.SaveFile
        currentFolder: StandardPaths.writableLocation(StandardPaths.DesktopLocation)
        onAccepted: {
            var path = currentFile.toString().replace("file://", "")
            var ok = root.tabPage.exportLogs(path)
            if (ok) {
                toastMsg.text = "已导出: " + path
                toastMsg.color = DesignSystem.success
            } else {
                toastMsg.text = "导出失败: " + path
                toastMsg.color = DesignSystem.error
            }
            toastMsg.show()
        }
    }

    // 导出成功提示
    Rectangle {
        id: toastMsg
        property string text: ""
        property bool visible_: false
        function show() {
            visible_ = true
            toastTimer.restart()
        }
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 90
        width: toastText.implicitWidth + 32
        height: 36
        radius: DesignSystem.radiusMd
        color: DesignSystem.success
        opacity: visible_ ? 1.0 : 0.0
        visible: visible_
        z: 100

        Behavior on opacity { NumberAnimation { duration: 200 } }

        Text {
            id: toastText
            anchors.centerIn: parent
            text: toastMsg.text
            color: DesignSystem.textInverse
            font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeMd
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Timer {
            id: toastTimer
            interval: 3000
            onTriggered: toastMsg.visible_ = false
        }
    }

    // ═══════════════════════════════════════════════
    // 方法
    // ═══════════════════════════════════════════════
    function sendData() {
        if (!root.tabPage || !root.tabPage.connected) return
        var text = sendInput.text
        if (text.length === 0) return
        root.tabPage.sendText(text, appendCombo.currentText)
        sendInput.clear()
    }

    // ═══════════════════════════════════════════════
    // 内联组件
    // ═══════════════════════════════════════════════

    /// 工具栏切换按钮
    component ToggleBtn: Rectangle {
        property string text: ""
        property bool checked: false
        property string tooltip: ""
        property bool warnColor: false
        signal clicked()

        width: label.implicitWidth + 16; height: 26; radius: DesignSystem.radiusSm
        color: checked ? DesignSystem.accent15 : (btnHover.containsMouse ? DesignSystem.bgTertiary : "transparent")
        border.width: checked ? 1 : 0
        border.color: checked ? DesignSystem.accent : "transparent"
        Behavior on color { ColorAnimation { duration: DesignSystem.animNormal } }

        Text {
            id: label; anchors.centerIn: parent; text: parent.text
            color: parent.checked
                ? (parent.warnColor ? DesignSystem.warning : DesignSystem.accentHover)
                : DesignSystem.textSecondary
            font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeSm; font.bold: true
        }
        MouseArea {
            id: btnHover; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                console.log("[ToggleBtn] mouse clicked, text=", text)
                parent.clicked()
            }
        }
        ToolTip {
            visible: btnHover.containsMouse && tooltip
            text: tooltip; delay: 500
        }
    }

    /// 工具栏操作按钮
    component ToolBtn: Rectangle {
        property string text: ""; property string tooltip: ""
        property bool dangerColor: false
        signal clicked()

        width: 30; height: 26; radius: DesignSystem.radiusSm
        color: btnHover.containsMouse
            ? (dangerColor ? DesignSystem.error15 : DesignSystem.bgTertiary) : "transparent"
        Behavior on color { ColorAnimation { duration: DesignSystem.animNormal } }

        Text {
            anchors.centerIn: parent; text: parent.text
            color: parent.dangerColor
                ? (btnHover.containsMouse ? DesignSystem.error : DesignSystem.textSecondary)
                : DesignSystem.textSecondary
            font.family: DesignSystem.fontMono; font.pixelSize: DesignSystem.fontSizeMd; font.bold: true
        }
        MouseArea {
            id: btnHover; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                console.log("[ToolBtn] mouse clicked, text=", text)
                parent.clicked()
            }
        }
        ToolTip {
            visible: btnHover.containsMouse && tooltip
            text: tooltip; delay: 500
        }
    }
}