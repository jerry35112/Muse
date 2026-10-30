import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Hover hint for values that do not fit, such as full application paths.
// Sized to its content and capped so a long path cannot cover the window.
ToolTip {
    id: tip
    delay: 850
    padding: 12
    width: Math.min(contentItem.implicitWidth + 2 * padding, 400)

    background: Rectangle {
        radius: Theme.radiusChip
        color: Theme.tipBackground
    }

    contentItem: Copy {
        text: tip.text
        color: Theme.tipText
        font.pixelSize: Theme.captionSize
        wrapMode: Text.WrapAnywhere
    }
}
