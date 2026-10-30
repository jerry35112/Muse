import QtQuick
import "Theme.js" as Theme

// Body text. Native rendering keeps the Chinese glyphs crisp at 13 px.
Text {
    color: Theme.ink
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSize
    renderType: Text.NativeRendering
    textFormat: Text.PlainText
}
