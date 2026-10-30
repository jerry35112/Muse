import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Left navigation rail: brand, page list and the runtime badge.
Rectangle {
    id: sidebar
    property int currentPage: 0
    property int cornerRadius: Theme.radiusWindow
    signal navigate(int page)

    width: Theme.sidebarWidth
    color: Theme.chrome
    radius: cornerRadius

    // Square off the right edge so only the window corners stay rounded.
    Rectangle {
        anchors.right: parent.right
        width: 20
        height: parent.height
        color: parent.color
    }
    Rectangle {
        width: 1
        anchors.right: parent.right
        height: parent.height
        color: Theme.line
    }

    RowLayout {
        x: 26; y: 34
        spacing: 12
        Image {
            source: "qrc:/assets/muse.png"
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            sourceSize.width: 76
            sourceSize.height: 76
        }
        Copy {
            text: "Muse"
            font.pixelSize: 25
            font.weight: Font.DemiBold
            font.letterSpacing: -.7
        }
    }

    Column {
        x: 14; y: 124
        spacing: 8
        width: 186
        Repeater {
            model: [
                { label: "概览", icon: "home", page: 0 },
                { label: "订阅管理", icon: "subscriptions", page: 1 },
                { label: "节点与测速", icon: "refresh", page: 4 },
                { label: "程序分流", icon: "apps", page: 2 },
                { label: "偏好设置", icon: "settings", page: 3 }
            ]
            delegate: AbstractButton {
                id: navigation
                objectName: "nav-" + modelData.page
                required property var modelData
                required property int index
                width: 186
                height: 46
                hoverEnabled: true
                Accessible.name: modelData.label
                onClicked: sidebar.navigate(modelData.page)
                background: Rectangle {
                    radius: Theme.radiusControl
                    color: sidebar.currentPage === modelData.page ? Theme.greenSoft
                           : navigation.hovered ? Theme.navHover : "transparent"
                    border.color: navigation.activeFocus ? Theme.navFocus : "transparent"
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 13
                    Glyph {
                        name: modelData.icon
                        stroke: sidebar.currentPage === modelData.page ? Theme.green : Theme.navIcon
                        implicitWidth: 19
                        implicitHeight: 19
                    }
                    Copy {
                        text: modelData.label
                        color: sidebar.currentPage === modelData.page ? Theme.green : Theme.inkSoft
                        font.weight: sidebar.currentPage === modelData.page ? Font.DemiBold : Font.Normal
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 23
        anchors.horizontalCenter: parent.horizontalCenter
        width: 166
        height: 57
        radius: 11
        color: Theme.cardTint
        RowLayout {
            anchors.centerIn: parent
            spacing: 10
            Rectangle {
                width: 7
                height: 7
                radius: 4
                color: backend.connected ? Theme.green : Theme.statusDot
            }
            Column {
                spacing: 3
                Copy {
                    text: backend.admin ? "管理员模式" : "标准模式"
                    font.pixelSize: 12
                    color: Theme.inkSoft
                }
                Copy {
                    text: "MUSE  /  " + backend.version
                    font.pixelSize: 9
                    font.letterSpacing: 1.3
                    color: Theme.mutedFaint
                }
            }
        }
    }
}
