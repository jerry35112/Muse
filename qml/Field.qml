import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Single-line text input. Keeps the focus ring and the placeholder colour in one
// place so every form looks the same.
TextField {
    id: field
    implicitHeight: Theme.fieldHeight
    color: Theme.ink
    placeholderTextColor: Theme.placeholder
    font.pixelSize: Theme.fontSize
    font.family: Theme.fontFamily
    leftPadding: 14
    rightPadding: 14
    selectByMouse: true
    selectionColor: Theme.greenWash
    selectedTextColor: Theme.ink

    background: Rectangle {
        radius: Theme.radiusControl
        color: field.activeFocus ? Theme.card : Theme.field
        border.color: field.activeFocus ? Theme.focus : Theme.line
        Behavior on border.color { ColorAnimation { duration: 150 } }
    }
}
