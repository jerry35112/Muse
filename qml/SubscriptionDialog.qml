import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Add/edit dialog for a subscription. The URL is masked by default and the
// validation error reported by the backend is shown inside the dialog.
Dialog {
    id: dialog
    property int editIndex: -1

    function edit(index) {
        const subscription = backend.subscription(index)
        editIndex = index
        nameInput.text = subscription.name
        urlInput.text = subscription.url
        intervalInput.text = String(subscription.interval)
        backend.clearMessage()
        open()
    }
    function submit() {
        backend.saveSubscription(editIndex, nameInput.text, urlInput.text, Number(intervalInput.text))
    }

    anchors.centerIn: parent
    width: 510
    modal: true
    padding: 28
    closePolicy: Popup.CloseOnEscape
    onOpened: nameInput.forceActiveFocus()

    background: Rectangle {
        color: Theme.card
        radius: 18
        border.color: Theme.line
    }

    header: Item {
        implicitHeight: 72
        Copy {
            x: 28; y: 27
            text: dialog.editIndex < 0 ? "添加订阅" : "编辑订阅"
            font.pixelSize: 21
            font.weight: Font.DemiBold
        }
    }

    contentItem: ColumnLayout {
        spacing: 12
        Copy { text: "订阅名称"; color: Theme.inkSoft; font.pixelSize: Theme.pageSubtitleSize }
        Field {
            id: nameInput
            placeholderText: "例如：我的日常订阅"
            Layout.fillWidth: true
            maximumLength: 128
            onAccepted: dialog.submit()
        }
        Copy {
            text: "订阅链接"
            color: Theme.inkSoft
            font.pixelSize: Theme.pageSubtitleSize
            Layout.topMargin: 7
        }
        Field {
            id: urlInput
            placeholderText: "https://"
            echoMode: TextInput.Password
            Layout.fillWidth: true
            maximumLength: 4096
            onAccepted: dialog.submit()
        }
        Copy {
            text: "Clash / mihomo 格式，链接将加密保存在本机。"
            font.pixelSize: Theme.captionSize
            color: Theme.muted
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 9
            Copy {
                text: "更新间隔（秒）"
                color: Theme.inkSoft
                Layout.fillWidth: true
                font.pixelSize: Theme.pageSubtitleSize
            }
            Field {
                id: intervalInput
                Layout.preferredWidth: 120
                validator: IntValidator { bottom: 300; top: 604800 }
                onAccepted: dialog.submit()
            }
        }
        Copy {
            visible: backend.error
            text: backend.message
            color: Theme.dangerText
            font.pixelSize: Theme.pageSubtitleSize
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 14
            Item { Layout.fillWidth: true }
            Command { text: "取消"; onClicked: dialog.close() }
            Command {
                text: "保存订阅"
                primary: true
                enabled: !backend.locked
                onClicked: dialog.submit()
            }
        }
    }
}
