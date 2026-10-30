import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Blocks input while the shutdown sequence restores the network settings.
Rectangle {
    id: overlay
    anchors.fill: parent
    color: Theme.overlay
    z: 50
    visible: backend.closing

    Column {
        anchors.centerIn: parent
        spacing: 16
        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: overlay.visible
        }
        Copy {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "正在安全退出…"
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }
        Copy {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "正在完成当前请求并恢复网络设置"
            color: Theme.muted
        }
    }
    MouseArea { anchors.fill: parent }
}
