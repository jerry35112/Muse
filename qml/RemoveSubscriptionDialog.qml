import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Confirmation for removing a local subscription entry only.
Dialog {
    id: dialog
    property int targetIndex: -1

    anchors.centerIn: parent
    width: 400
    modal: true
    padding: 25
    closePolicy: Popup.CloseOnEscape

    background: Rectangle {
        color: Theme.card
        radius: Theme.radiusCard
        border.color: Theme.line
    }

    header: Item {
        implicitHeight: 56
        Copy {
            x: 25; y: 22
            text: "移除订阅？"
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }
    }

    contentItem: ColumnLayout {
        spacing: 20
        Copy {
            text: "仅移除本机配置，不会取消服务商的订阅。"
            color: Theme.muted
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Command { text: "保留"; onClicked: dialog.close() }
            Command {
                text: "确认移除"
                primary: true
                enabled: !backend.locked
                onClicked: {
                    backend.removeSubscription(dialog.targetIndex)
                    dialog.close()
                }
            }
        }
    }
}
