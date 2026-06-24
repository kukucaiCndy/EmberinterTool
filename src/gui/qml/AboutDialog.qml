import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EmberDesign 1.0

/// 关于 / 版本 / 自动更新
Popup {
    id: root
    width: 540
    height: 540
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
    property string latestVersion: ""
    property string currentVersion: "v1.3.1"
    property string buildDate: "2026.06.24"
    property bool updateAvailable: false
    property bool autoCheckEnabled: true
    property string downloadUrl: ""
    property string releaseNotes: ""
    property string errorMsg: ""

    // 打开时: 居中 + 加载配置 + 自动检查更新
    onOpened: {
        var p = parent
        if (p) {
            root.x = (p.width - root.width) / 2
            root.y = (p.height - root.height) / 2
        }
        var s = appCore.loadSettings()
        autoCheckEnabled = s.autoCheckUpdate !== undefined ? s.autoCheckUpdate : true
        autoCheckSwitch.checked = autoCheckEnabled
        currentVersion = appCore.currentVersion()
        if (autoCheckEnabled) {
            doCheckUpdate()
        }
    }

    function doCheckUpdate() {
        root.checking = true
        root.errorMsg = ""
        appCore.checkUpdate()
    }

    // 下载状态
    property bool downloading: false
    property int downloadPercent: 0
    property string downloadSizeText: ""

    function doDownloadUpdate() {
        if (root.downloadUrl === "") {
            root.errorMsg = "无下载链接，请前往 Release 页面手动下载"
            return
        }
        root.downloading = true
        root.downloadPercent = 0
        root.downloadSizeText = ""
        appCore.downloadUpdate(root.downloadUrl)
    }

    // 接收 C++ 检查结果
    Connections {
        target: appCore
        function onUpdateCheckResult(result) {
            root.checking = false
            if (result.error) {
                root.errorMsg = result.errorMsg || "网络错误"
                root.updateAvailable = false
            } else {
                root.latestVersion = result.latestVersion || ""
                root.updateAvailable = result.hasUpdate
                root.downloadUrl = result.downloadUrl || ""
                root.releaseNotes = result.releaseNotes || ""
                root.errorMsg = ""
            }
        }
        // 下载进度
        function onUpdateDownloadProgress(received, total, percent) {
            root.downloadPercent = percent
            var mb = 1024 * 1024
            if (total > 0) {
                root.downloadSizeText = (received / mb).toFixed(1) + " / " + (total / mb).toFixed(1) + " MB"
            } else {
                root.downloadSizeText = (received / mb).toFixed(1) + " MB"
            }
        }
        // 下载完成
        function onUpdateDownloadFinished(filePath, errorMsg) {
            root.downloading = false
            if (filePath === "") {
                root.errorMsg = "下载失败: " + errorMsg
            } else {
                root.downloadPercent = 100
                root.downloadSizeText = "下载完成，安装程序已启动..."
            }
        }
    }

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

                    Image {
                        anchors.centerIn: parent
                        width: 56; height: 56
                        source: "qrc:/icons/logo.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
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
                            text: "Qt 6.x | MSYS2 mingw64 | C++17"
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
                        onCheckedChanged: {
                            root.autoCheckEnabled = checked
                            appCore.saveAutoCheckUpdate(checked)
                        }
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
                    color: checkBtnHover.containsMouse ? DesignSystem.hover : DesignSystem.bgTertiary
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
                        id: checkBtnHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!root.checking) {
                                root.doCheckUpdate()
                            }
                        }
                    }
                }

                // 错误提示
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.errorMsg !== "" ? 40 : 0
                    visible: root.errorMsg !== ""
                    clip: true
                    radius: DesignSystem.radiusSm
                    color: DesignSystem.error15
                    border.width: 1
                    border.color: DesignSystem.error

                    Text {
                        anchors.centerIn: parent
                        text: root.errorMsg
                        color: DesignSystem.error
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeSm
                    }

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: DesignSystem.animFast }
                    }
                }

                // 已是最新版本提示
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: (!root.updateAvailable && !root.checking && root.latestVersion !== "" && root.errorMsg === "") ? 40 : 0
                    visible: Layout.preferredHeight > 0
                    clip: true
                    radius: DesignSystem.radiusSm
                    color: DesignSystem.success15
                    border.width: 1
                    border.color: "#333FB950"

                    Text {
                        anchors.centerIn: parent
                        text: DesignSystem.iconCheck + " 当前已是最新版本 (" + root.currentVersion + ")"
                        color: DesignSystem.success
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeSm
                    }

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: DesignSystem.animFast }
                    }
                }

                // 更新状态卡片 (有新版本时显示)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.updateAvailable ? 220 : 0
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
                            Item { Layout.fillWidth: true }
                        }

                        // Release Notes (截断显示)
                        Text {
                            Layout.fillWidth: true
                            text: root.releaseNotes !== "" ? root.releaseNotes : "修复若干已知问题，优化性能，增加新功能。"
                            color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody
                            font.pixelSize: DesignSystem.fontSizeMd
                            wrapMode: Text.WordWrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }

                        // 立即更新按钮 / 下载进度
                        Rectangle {
                            Layout.fillWidth: true
                            height: 36
                            radius: DesignSystem.radiusSm
                            color: root.downloading ? DesignSystem.bgTertiary
                                 : (downloadBtnHover.containsMouse ? DesignSystem.accentHover : DesignSystem.success)
                            visible: !root.downloading

                            Text {
                                anchors.centerIn: parent
                                text: "立即更新"
                                color: DesignSystem.textInverse
                                font.family: DesignSystem.fontBody
                                font.pixelSize: DesignSystem.fontSizeMd
                                font.bold: true
                            }
                            MouseArea {
                                id: downloadBtnHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.doDownloadUpdate()
                            }
                        }

                        // 下载进度条
                        Rectangle {
                            Layout.fillWidth: true
                            height: 36
                            radius: DesignSystem.radiusSm
                            color: DesignSystem.bgTertiary
                            border.width: 1
                            border.color: DesignSystem.border
                            visible: root.downloading

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                // 进度条
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 8
                                    radius: 4
                                    color: DesignSystem.bgPrimary

                                    Rectangle {
                                        width: parent.width * (root.downloadPercent / 100)
                                        height: parent.height
                                        radius: 4
                                        color: DesignSystem.success
                                        Behavior on width { NumberAnimation { duration: 200 } }
                                    }
                                }

                                // 百分比
                                Text {
                                    text: root.downloadPercent >= 0 ? root.downloadPercent + "%" : "..."
                                    color: DesignSystem.textPrimary
                                    font.family: DesignSystem.fontMono
                                    font.pixelSize: DesignSystem.fontSizeSm
                                    font.bold: true
                                    Layout.preferredWidth: 36
                                }
                            }
                        }

                        // 下载大小文本
                        Text {
                            Layout.fillWidth: true
                            visible: root.downloading && root.downloadSizeText !== ""
                            text: root.downloadSizeText + " - 正在下载..."
                            color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontMono
                            font.pixelSize: DesignSystem.fontSizeXs
                            horizontalAlignment: Text.AlignHCenter
                        }

                        // 下载完成后提示
                        Text {
                            Layout.fillWidth: true
                            visible: !root.downloading && root.downloadPercent === 100
                            text: "下载完成，安装程序已启动，请按提示完成安装"
                            color: DesignSystem.success
                            font.family: DesignSystem.fontBody
                            font.pixelSize: DesignSystem.fontSizeSm
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
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