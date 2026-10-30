import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Primary/secondary command button. Sizes itself from the label so localisation
// never clips text.
Button {
    id: action
    property bool primary: false
    property string iconName: ""
    implicitHeight: Theme.actionHeight
    implicitWidth: Math.max(108, body.implicitWidth + 32)
    hoverEnabled: true
    opacity: enabled ? 1 : .45

    background: Rectangle {
        radius: Theme.radiusControl
        color: action.primary
               ? (action.down ? Theme.greenPressed : action.hovered ? Theme.greenHover : Theme.green)
               : (action.hovered ? Theme.hover : Theme.card)
        border.color: action.primary ? color : Theme.line
        Behavior on color { ColorAnimation { duration: 140 } }
    }

    contentItem: RowLayout {
        id: body
        spacing: 8
        Glyph {
            visible: action.iconName !== ""
            name: action.iconName
            stroke: action.primary ? Theme.card : Theme.ink
            implicitWidth: 16
            implicitHeight: 16
            Layout.alignment: Qt.AlignVCenter
        }
        Copy {
            text: action.text
            color: action.primary ? Theme.card : Theme.ink
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }
}
