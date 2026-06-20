import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EmberDesign 1.0

/// 新建会话连接向导 — 串口 / 终端 / SSH 三页
Popup {
    id: root
    width: 480; height: 440
    x: (root.parent.width - width) / 2
    y: (root.parent.height - height) / 2
    modal: true; closePolicy: Popup.CloseOnEscape; padding: 0

    background: Rectangle {
        color: DesignSystem.bgSecondary; radius: DesignSystem.radiusLg
        border.width: 1; border.color: DesignSystem.border
    }

    // ── 向导状态 ──
    property int    currentPage: 0
    property string wizardName: ""
    property string wizardSerialPort: ""
    property string wizardBaud: "115200"
    property int    wizardDataBits: 8
    property int    wizardParity: 0
    property int    wizardStopBits: 1
    property string wizardShell: "cmd.exe"
    property string wizardSshHost: ""
    property string wizardSshPort: "22"
    property string wizardSshUser: ""
    property string wizardSshPassword: ""

    function resetWizard() {
        currentPage = 0; wizardName = ""; wizardSerialPort = ""
        wizardBaud = "115200"; wizardDataBits = 8; wizardParity = 0; wizardStopBits = 1
        wizardShell = "cmd.exe"; wizardSshHost = ""; wizardSshPort = "22"
        wizardSshUser = ""; wizardSshPassword = ""
    }

    onOpened: resetWizard()

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // ── 标题栏 (h=48) ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 48
            color: DesignSystem.bgSecondary

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: DesignSystem.spaceLg
                anchors.rightMargin: DesignSystem.spaceLg

                Text {
                    text: "新建会话"; color: DesignSystem.textPrimary
                    font.family: DesignSystem.fontHead
                    font.pixelSize: DesignSystem.fontSize2xl
                    font.weight: DesignSystem.fontWeightMedium
                }
                Item { Layout.fillWidth: true }

                // 关闭按钮
                Rectangle {
                    width: 28; height: 28; radius: DesignSystem.radiusSm
                    color: DesignSystem.bgTertiary
                    Text {
                        anchors.centerIn: parent
                        text: DesignSystem.iconClose
                        color: DesignSystem.textSecondary
                        font.pixelSize: DesignSystem.fontSizeMd
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: DesignSystem.border }

        // ── 类型选择 (h=40) ──
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 40
            Layout.leftMargin: DesignSystem.spaceSm; Layout.rightMargin: DesignSystem.spaceSm
            spacing: DesignSystem.spaceSm

            TypeChip {
                text: "串口"; iconText: "\u26A1"
                checked: currentPage === 0
                onClicked: currentPage = 0
            }
            TypeChip {
                text: "终端"; iconText: "\u2328"
                checked: currentPage === 1
                onClicked: currentPage = 1
            }
            TypeChip {
                text: "SSH"; iconText: "\u25CE"
                checked: currentPage === 2
                onClicked: currentPage = 2
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20
            height: 1; color: DesignSystem.border
        }

        // ── 页面内容 ──
        StackLayout {
            id: wizardStack
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.topMargin: 8; Layout.margins: 20
            currentIndex: root.currentPage

            // ── 页0: 串口 ──
            ColumnLayout {
                spacing: DesignSystem.spaceSm; Layout.fillWidth: true

                WizardField {
                    label: "会话名称"; placeholder: "如: STM32调试串口"
                    onFieldText: (t) => root.wizardName = t
                }

                ColumnLayout {
                    spacing: 4; Layout.fillWidth: true
                    Text {
                        text: "串口号"; color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeMd
                    }
                    ComboBox {
                        id: serialPortCombo
                        Layout.fillWidth: true; Layout.preferredHeight: 32
                        editable: true; currentIndex: -1
                        displayText: root.wizardSerialPort || "选择串口..."
                        model: appCore.availableSerialPorts
                        background: Rectangle {
                            color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                            border.width: 1; border.color: DesignSystem.border
                        }
                        contentItem: Text {
                            leftPadding: 10; rightPadding: 10
                            text: parent.displayText
                            color: root.wizardSerialPort ? DesignSystem.textPrimary : DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeLg
                            verticalAlignment: Text.AlignVCenter
                        }
                        onActivated: root.wizardSerialPort = editText
                        onEditTextChanged: root.wizardSerialPort = editText
                    }
                }

                GridLayout {
                    columns: 2; rowSpacing: 8; columnSpacing: 10; Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 4; Layout.fillWidth: true
                        Text {
                            text: "波特率"; color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeMd
                        }
                        ComboBox {
                            Layout.fillWidth: true; Layout.preferredHeight: 32
                            editable: true; currentIndex: 0
                            model: ["115200","921600","460800","230400","57600","38400","19200","9600"]
                            background: Rectangle {
                                color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                                border.width: 1; border.color: DesignSystem.border
                            }
                            contentItem: Text {
                                leftPadding: 10; rightPadding: 10
                                text: parent.displayText
                                color: DesignSystem.textPrimary
                                font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeLg
                                verticalAlignment: Text.AlignVCenter
                            }
                            onCurrentTextChanged: root.wizardBaud = currentText
                        }
                    }

                    ColumnLayout {
                        spacing: 4; Layout.fillWidth: true
                        Text {
                            text: "数据位"; color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeMd
                        }
                        ComboBox {
                            Layout.fillWidth: true; Layout.preferredHeight: 32
                            currentIndex: 0
                            model: ["8","7","6","5"]
                            background: Rectangle {
                                color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                                border.width: 1; border.color: DesignSystem.border
                            }
                            contentItem: Text {
                                leftPadding: 10; rightPadding: 10
                                text: parent.displayText
                                color: DesignSystem.textPrimary
                                font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeLg
                                verticalAlignment: Text.AlignVCenter
                            }
                            onCurrentTextChanged: root.wizardDataBits = parseInt(currentText)
                        }
                    }

                    ColumnLayout {
                        spacing: 4; Layout.fillWidth: true
                        Text {
                            text: "校验位"; color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeMd
                        }
                        ComboBox {
                            Layout.fillWidth: true; Layout.preferredHeight: 32
                            currentIndex: 0
                            model: ["无", "偶", "奇", "标记", "空格"]
                            background: Rectangle {
                                color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                                border.width: 1; border.color: DesignSystem.border
                            }
                            contentItem: Text {
                                leftPadding: 10; rightPadding: 10
                                text: parent.displayText
                                color: DesignSystem.textPrimary
                                font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeLg
                                verticalAlignment: Text.AlignVCenter
                            }
                            onCurrentIndexChanged: root.wizardParity = currentIndex
                        }
                    }

                    ColumnLayout {
                        spacing: 4; Layout.fillWidth: true
                        Text {
                            text: "停止位"; color: DesignSystem.textSecondary
                            font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeMd
                        }
                        ComboBox {
                            Layout.fillWidth: true; Layout.preferredHeight: 32
                            currentIndex: 0
                            model: ["1","1.5","2"]
                            background: Rectangle {
                                color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                                border.width: 1; border.color: DesignSystem.border
                            }
                            contentItem: Text {
                                leftPadding: 10; rightPadding: 10
                                text: parent.displayText
                                color: DesignSystem.textPrimary
                                font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeLg
                                verticalAlignment: Text.AlignVCenter
                            }
                            onCurrentTextChanged: { root.wizardStopBits = parseInt(currentText) || 1 }
                        }
                    }
                }
            }

            // ── 页1: 终端 ──
            ColumnLayout {
                spacing: DesignSystem.spaceSm; Layout.fillWidth: true

                WizardField {
                    label: "会话名称"; placeholder: "如: 本地 Bash"
                    onFieldText: (t) => root.wizardName = t
                }

                ColumnLayout {
                    spacing: 4; Layout.fillWidth: true
                    Text {
                        text: "Shell"; color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeMd
                    }
                    ComboBox {
                        Layout.fillWidth: true; Layout.preferredHeight: 32
                        currentIndex: 0
                        model: ["cmd.exe", "PowerShell", "bash (Git/MSYS2)"]
                        background: Rectangle {
                            color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                            border.width: 1; border.color: DesignSystem.border
                        }
                        contentItem: Text {
                            leftPadding: 10; rightPadding: 10
                            text: parent.displayText
                            color: DesignSystem.textPrimary
                            font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeLg
                            verticalAlignment: Text.AlignVCenter
                        }
                        onCurrentIndexChanged: {
                            root.wizardShell = ["cmd.exe","powershell.exe","bash.exe"][currentIndex]
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                RowLayout { spacing: 6
                    Text { text: DesignSystem.iconInfo; color: DesignSystem.accent; font.pixelSize: DesignSystem.fontSizeMd }
                    Text {
                        text: "自动检测系统可用的 Shell 环境"
                        color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeSm
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                }
            }

            // ── 页2: SSH ──
            ColumnLayout {
                spacing: DesignSystem.spaceSm; Layout.fillWidth: true

                WizardField {
                    label: "会话名称"; placeholder: "如: 生产服务器"
                    onFieldText: (t) => root.wizardName = t
                }
                WizardField {
                    label: "主机地址"; placeholder: "192.168.1.100"
                    onFieldText: (t) => root.wizardSshHost = t
                }

                RowLayout { spacing: 10
                    WizardField {
                        label: "端口"; placeholder: "22"
                        onFieldText: (t) => root.wizardSshPort = t
                        Layout.fillWidth: true
                    }
                    WizardField {
                        label: "用户名"; placeholder: "root"
                        onFieldText: (t) => root.wizardSshUser = t
                        Layout.fillWidth: true
                    }
                }

                WizardField {
                    label: "密码"; placeholder: "留空使用密钥认证"
                    onFieldText: (t) => root.wizardSshPassword = t
                }

                Item { Layout.fillHeight: true }

                RowLayout { spacing: 6
                    Text { text: DesignSystem.iconWarning; color: DesignSystem.warning; font.pixelSize: DesignSystem.fontSizeMd }
                    Text {
                        text: "密码留空时使用 SSH 密钥认证 (~/.ssh/id_rsa)"
                        color: DesignSystem.textSecondary
                        font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeSm
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                }
            }
        }

        // ── 底部按钮 ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 56
            color: DesignSystem.bgSecondary

            Rectangle {
                anchors.top: parent.top; width: parent.width; height: 1
                color: DesignSystem.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: DesignSystem.spaceLg
                anchors.rightMargin: DesignSystem.spaceLg
                spacing: DesignSystem.spaceMd

                Item { Layout.fillWidth: true }

                // 取消
                Rectangle {
                    width: 72; height: 32; radius: DesignSystem.radiusMd
                    color: "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: DesignSystem.iconClose
                        color: DesignSystem.textSecondary
                        font.pixelSize: DesignSystem.fontSizeLg
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }

                // 创建会话
                Rectangle {
                    width: 96; height: 32; radius: DesignSystem.radiusMd

                    property bool canCreate: {
                        if (root.currentPage === 0) return root.wizardSerialPort !== ""
                        if (root.currentPage === 2) return root.wizardSshHost !== ""
                        return true
                    }

                    color: createHover.containsMouse ? DesignSystem.accentHover : DesignSystem.accent
                    opacity: canCreate ? 1.0 : 0.4

                    Text {
                        anchors.centerIn: parent
                        text: "创建会话"; color: DesignSystem.textInverse
                        font.family: DesignSystem.fontBody
                        font.pixelSize: DesignSystem.fontSizeLg
                        font.weight: DesignSystem.fontWeightMedium
                    }
                    MouseArea {
                        id: createHover; anchors.fill: parent
                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        enabled: parent.canCreate
                        onClicked: {
                            var p = {}
                            if (root.currentPage === 0) {
                                p["type"] = "serial"; p["port"] = root.wizardSerialPort
                                p["baud"] = parseInt(root.wizardBaud) || 115200
                                p["data_bits"] = root.wizardDataBits
                                p["parity"] = root.wizardParity
                                p["stop_bits"] = root.wizardStopBits
                            } else if (root.currentPage === 1) {
                                p["type"] = "terminal"; p["shell"] = root.wizardShell
                            } else if (root.currentPage === 2) {
                                p["type"] = "ssh"; p["host"] = root.wizardSshHost
                                p["ssh_port"] = parseInt(root.wizardSshPort) || 22
                                p["user"] = root.wizardSshUser
                                p["password"] = root.wizardSshPassword
                            }
                            p["name"] = root.wizardName
                            appCore.confirmConnection(p)
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

    component TypeChip: Rectangle {
        property string text: ""
        property string iconText: ""
        property bool checked: false
        signal clicked()

        Layout.fillWidth: true
        height: 32; radius: DesignSystem.radiusMd
        color: checked ? DesignSystem.accent : "transparent"
        Behavior on color { ColorAnimation { duration: DesignSystem.animNormal } }

        RowLayout {
            anchors.centerIn: parent
            spacing: 6

            Text {
                text: parent.parent.iconText
                color: parent.parent.checked ? DesignSystem.textInverse : DesignSystem.textSecondary
                font.pixelSize: 14
                visible: text !== ""
            }
            Text {
                text: parent.parent.text
                color: parent.parent.checked ? DesignSystem.textInverse : DesignSystem.textSecondary
                font.family: DesignSystem.fontBody
                font.pixelSize: DesignSystem.fontSizeLg
                font.weight: checked ? DesignSystem.fontWeightMedium : DesignSystem.fontWeightNormal
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    component WizardField: ColumnLayout {
        property string label: ""
        property string placeholder: ""
        signal fieldText(string t)
        spacing: 4; Layout.fillWidth: true

        Text {
            text: label; color: DesignSystem.textSecondary
            font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeMd
        }
        TextField {
            Layout.fillWidth: true; Layout.preferredHeight: 32
            placeholderText: placeholder
            color: DesignSystem.textPrimary
            font.family: DesignSystem.fontBody; font.pixelSize: DesignSystem.fontSizeLg
            background: Rectangle {
                color: DesignSystem.bgTertiary; radius: DesignSystem.radiusSm
                border.width: 1; border.color: DesignSystem.border
            }
            onTextChanged: fieldText(text)
        }
    }
}