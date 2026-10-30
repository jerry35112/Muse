import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Compact state badge: a coloured dot plus a short label.
Item {
    id: pill
    property bool active: false
    property string label: ""
    property string activeColor: Theme.green
    implicitWidth: text.implicitWidth + 24
    implicitHeight: 28

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusPill
        color: pill.active ? Theme.pillOn : Theme.pillOff

        Row {
            anchors.centerIn: parent
            spacing: 7
            Rectangle {
                width: 5
                height: 5
                radius: 3
                color: pill.active ? pill.activeColor : Theme.pillDot
                anchors.verticalCenter: parent.verticalCenter
            }
            Copy {
                id: text
                text: pill.label
                font.pixelSize: Theme.captionSize
                color: pill.active ? pill.activeColor : Theme.pillText
            }
        }
    }
}
