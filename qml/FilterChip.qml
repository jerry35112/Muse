import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Small pill used for filters (application sources, categories, regions).
Button {
    id: chip
    property bool active: false
    implicitHeight: Theme.chipHeight
    implicitWidth: label.implicitWidth + 24
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radiusChip
        color: chip.active ? Theme.chipActive : chip.hovered ? Theme.chipHover : "transparent"
        border.color: chip.active ? Theme.chipActiveEdge : "transparent"
    }

    contentItem: Copy {
        id: label
        text: chip.text
        font.pixelSize: Theme.captionSize
        color: chip.active ? Theme.green : Theme.chipText
        font.weight: chip.active ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
