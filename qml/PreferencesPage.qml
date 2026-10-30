import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Preferences: TUN capture, local ports and the manual proxy recovery switch.
ScrollView {
    id: root
    objectName: "settings-scroll"
    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    property bool locked: false

    readonly property int mixedValue: Number(mixedField.text)
    readonly property int apiValue: Number(apiField.text)

    // Validate locally so the user sees the reason next to the field instead of
    // only getting a footer message after a rejected click.
    readonly property string portProblem: {
        if (mixedField.text === "" || apiField.text === "") return "两个端口都必须填写"
        if (!isFinite(root.mixedValue) || !isFinite(root.apiValue)) return "端口必须是数字"
        if (root.mixedValue < 1024 || root.mixedValue > 65535 || root.apiValue < 1024 || root.apiValue > 65535)
            return "端口范围是 1024–65535"
        if (root.mixedValue === root.apiValue) return "两个端口不能相同"
        return ""
    }
    readonly property bool portDirty: root.mixedValue !== backend.mixedPort || root.apiValue !== backend.apiPort

    ColumnLayout {
        width: root.availableWidth
        spacing: 18

        Card {
            Layout.fillWidth: true
            Layout.preferredHeight: 82
            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Copy {
                        text: "TUN 网络接管"
                        font.weight: Font.DemiBold
                        font.pixelSize: 14
                    }
                    Copy {
                        text: "默认开启，接管全机流量；勾选程序时始终使用 TUN。"
                        color: Theme.muted
                        font.pixelSize: Theme.captionSize
                    }
                }
                Switch {
                    id: tunSwitch
                    objectName: "tun-switch"
                    checked: backend.tunEnabled
                    enabled: !root.locked
                    onClicked: if (checked !== backend.tunEnabled) backend.setTun(checked)
                    // Clicking a Switch destroys its binding, so re-sync from the
                    // backend: a refused change must snap back, not lie.
                    Connections {
                        target: backend
                        function onSelectionChanged() { tunSwitch.checked = backend.tunEnabled }
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            Layout.preferredHeight: 246
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 25
                spacing: 16
                Copy {
                    text: "本地端口"
                    font.pixelSize: Theme.sectionTitleSize
                    font.weight: Font.DemiBold
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Copy {
                            text: "HTTP / SOCKS 代理端口"
                            color: Theme.muted
                            font.pixelSize: Theme.pageSubtitleSize
                        }
                        Field {
                            id: mixedField
                            text: String(backend.mixedPort)
                            enabled: !root.locked
                            Layout.fillWidth: true
                            validator: IntValidator { bottom: 1024; top: 65535 }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Copy {
                            text: "本地控制端口"
                            color: Theme.muted
                            font.pixelSize: Theme.pageSubtitleSize
                        }
                        Field {
                            id: apiField
                            text: String(backend.apiPort)
                            enabled: !root.locked
                            Layout.fillWidth: true
                            validator: IntValidator { bottom: 1024; top: 65535 }
                        }
                    }
                }
                RowLayout {
                    spacing: 12
                    Command {
                        text: "保存设置"
                        primary: true
                        enabled: !root.locked && root.portProblem === "" && root.portDirty
                        onClicked: backend.savePorts(Number(mixedField.text), Number(apiField.text))
                    }
                    Copy {
                        Layout.fillWidth: true
                        text: root.portProblem !== "" ? root.portProblem
                              : root.portDirty ? "修改将在下次启动代理时生效。"
                              : "当前端口已保存。"
                        color: root.portProblem !== "" ? Theme.dangerText : Theme.muted
                        font.pixelSize: Theme.captionSize
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            RowLayout {
                anchors.fill: parent
                anchors.margins: 25
                spacing: 20
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Copy {
                        text: "恢复系统代理"
                        font.pixelSize: Theme.sectionTitleSize
                        font.weight: Font.DemiBold
                    }
                    Copy {
                        text: "如果上次未正常退出，可以在这里重试恢复。"
                        color: Theme.muted
                        font.pixelSize: Theme.pageSubtitleSize
                    }
                }
                Command {
                    text: "恢复网络"
                    enabled: !root.locked
                    onClicked: backend.recover()
                }
            }
        }

        Copy {
            text: "Muse " + backend.version + "  ·  Qt Quick\nTUN 和程序分流需要管理员权限。当前版本不提供断线阻断。"
            color: Theme.muted
            font.pixelSize: Theme.captionSize
            lineHeight: 1.7
        }
        Item { Layout.fillHeight: true }
    }
}
