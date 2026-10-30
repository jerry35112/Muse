import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Subscription list with provider-reported quota. Values that the provider does
// not supply are labelled as such instead of being guessed.
ColumnLayout {
    id: root
    spacing: 18

    property bool locked: false
    signal addRequested()
    signal editRequested(int index)
    signal removeRequested(int index)

    RowLayout {
        Layout.fillWidth: true
        Copy {
            text: backend.subscriptions.length + " 个订阅"
            color: Theme.muted
        }
        Item { Layout.fillWidth: true }
        Command {
            text: backend.loaded ? "更新订阅" : "读取节点与用量"
            iconName: "refresh"
            enabled: !backend.busy && backend.subscriptions.length > 0
            onClicked: backend.loaded ? backend.updateSubscriptions() : backend.loadNodes()
        }
        Command {
            text: "添加订阅"
            iconName: "plus"
            primary: true
            enabled: !root.locked
            onClicked: root.addRequested()
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            visible: backend.subscriptions.length === 0
            anchors.centerIn: parent
            spacing: 16
            Rectangle {
                width: 68
                height: 68
                radius: 20
                color: Theme.greenSoft
                Layout.alignment: Qt.AlignHCenter
                Glyph {
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    name: "subscriptions"
                    stroke: Theme.emptyGlyph
                }
            }
            Copy {
                text: "留个位置，给你的第一个订阅。"
                font.pixelSize: 17
                font.weight: Font.Medium
                Layout.alignment: Qt.AlignHCenter
            }
            Copy {
                text: "粘贴 Clash / mihomo 格式的订阅链接即可开始。"
                color: Theme.muted
                font.pixelSize: Theme.pageSubtitleSize
                Layout.alignment: Qt.AlignHCenter
            }
            Command {
                text: "添加订阅"
                primary: true
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 6
                enabled: !root.locked
                onClicked: root.addRequested()
            }
        }

        ListView {
            anchors.fill: parent
            anchors.margins: 14
            clip: true
            spacing: 10
            model: backend.subscriptions
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: card
                required property var modelData
                required property int index
                width: ListView.view.width
                height: modelData.usageKnown ? 174 : 136
                radius: Theme.radiusRow
                color: Theme.cardSoft
                border.color: Theme.line

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 18
                    height: 48
                    spacing: 14
                    Glyph {
                        name: "subscriptions"
                        stroke: Theme.cardGlyph
                        width: 22
                        height: 22
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Copy {
                            text: modelData.name
                            font.weight: Font.DemiBold
                            font.pixelSize: 14
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Copy {
                            text: modelData.host + "  ·  每 " + Math.round(modelData.interval / 60) + " 分钟更新"
                            color: Theme.muted
                            font.pixelSize: Theme.captionSize
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }
                    }
                    Switch {
                        checked: modelData.enabled
                        enabled: !root.locked
                        onClicked: backend.enableSubscription(index, checked)
                    }
                    Command {
                        text: "编辑"
                        implicitWidth: 64
                        enabled: !root.locked
                        onClicked: root.editRequested(index)
                    }
                    Button {
                        text: "移除"
                        flat: true
                        enabled: !root.locked
                        font.pixelSize: 12
                        onClicked: root.removeRequested(index)
                    }
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 18
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Copy {
                            text: modelData.usageKnown ? "剩余 " + modelData.remaining : modelData.usageText
                            font.pixelSize: 15
                            color: Theme.green
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }
                        Copy {
                            text: modelData.expiry
                            color: Theme.muted
                            font.pixelSize: Theme.captionSize
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 5
                        radius: 3
                        color: Theme.meterTrack
                        visible: modelData.usageKnown
                        Rectangle {
                            width: parent.width * modelData.fraction
                            height: parent.height
                            radius: 3
                            color: modelData.fraction > .9 ? Theme.meterHigh : Theme.meterFill
                        }
                    }
                    Copy {
                        text: modelData.usageKnown
                              ? modelData.usageText + " · 服务商提供的订阅用量"
                              : "用量信息依赖订阅服务商提供，不会用本机统计代替。"
                        color: Theme.muted
                        font.pixelSize: Theme.microSize
                    }
                }
            }
        }
    }

    Copy {
        text: "加载后自动同步用量，无需先开启代理。未加载时保留本次最后读取的数据；订阅链接在本机加密保存。"
        color: Theme.muted
        font.pixelSize: Theme.captionSize
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}
