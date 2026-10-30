import QtQuick
import "Theme.js" as Theme

// Vector icons drawn on a 24x24 grid, so they stay sharp at every scale and DPI
// without shipping image assets.
Item {
    id: glyph
    property string name: "home"
    property color stroke: Theme.ink
    implicitWidth: 20
    implicitHeight: 20

    function repaint() { canvas.requestPaint() }
    onStrokeChanged: repaint()
    onNameChanged: repaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onWidthChanged: glyph.repaint()
        onHeightChanged: glyph.repaint()
        onPaint: {
            const c = getContext("2d")
            c.reset()
            c.scale(width / 24, height / 24)
            c.strokeStyle = glyph.stroke
            c.lineWidth = 1.6
            c.lineCap = "round"
            c.lineJoin = "round"
            function line(x1, y1, x2, y2) { c.moveTo(x1, y1); c.lineTo(x2, y2) }
            c.beginPath()
            switch (glyph.name) {
            case "home":
                c.moveTo(3, 11); c.lineTo(12, 3); c.lineTo(21, 11)
                c.moveTo(5, 10); c.lineTo(5, 21); c.lineTo(10, 21); c.lineTo(10, 15)
                c.lineTo(14, 15); c.lineTo(14, 21); c.lineTo(19, 21); c.lineTo(19, 10)
                break
            case "subscriptions":
                c.rect(4, 4, 16, 6); c.rect(4, 14, 16, 6)
                line(8, 7, 8.1, 7); line(8, 17, 8.1, 17); line(13, 7, 17, 7); line(13, 17, 17, 17)
                break
            case "apps":
                c.rect(3, 3, 7, 7); c.rect(14, 3, 7, 7); c.rect(3, 14, 7, 7); c.rect(14, 14, 7, 7)
                break
            case "settings":
                c.arc(12, 12, 8, 0, Math.PI * 2); c.moveTo(15, 12); c.arc(12, 12, 3, 0, Math.PI * 2)
                line(12, 1, 12, 4); line(12, 20, 12, 23); line(1, 12, 4, 12); line(20, 12, 23, 12)
                break
            case "power":
                c.arc(12, 13, 8, -Math.PI * .29, Math.PI * 1.29); line(12, 2, 12, 12)
                break
            case "plus":
                line(12, 5, 12, 19); line(5, 12, 19, 12)
                break
            case "close":
                line(6, 6, 18, 18); line(18, 6, 6, 18)
                break
            case "minimize":
                line(5, 13, 19, 13)
                break
            case "maximize":
                c.rect(5, 5, 14, 14)
                break
            case "check":
                c.moveTo(5, 12); c.lineTo(10, 17); c.lineTo(19, 7)
                break
            case "arrow":
                c.moveTo(9, 5); c.lineTo(16, 12); c.lineTo(9, 19)
                break
            case "refresh":
                c.arc(12, 12, 8, .4, Math.PI * 1.8)
                c.moveTo(17, 3); c.lineTo(18, 8); c.lineTo(13, 7)
                break
            case "shield":
                c.moveTo(12, 2); c.lineTo(20, 5); c.lineTo(20, 12)
                c.quadraticCurveTo(20, 18, 12, 22); c.quadraticCurveTo(4, 18, 4, 12)
                c.lineTo(4, 5); c.closePath()
                break
            }
            c.stroke()
        }
    }
}
