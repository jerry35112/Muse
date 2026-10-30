import QtQuick
import "Theme.js" as Theme

// Frameless title bar: the left part drags the window, the right part holds the
// caption buttons. Kept as an overlay so the page content can use the full height.
Item {
    id: bar
    property var controller: null   // the ApplicationWindow this bar belongs to
    property bool maximized: false
    property bool busy: false
    height: 48

    signal closeRequested()
    signal minimizeRequested()
    signal toggleMaximizeRequested()

    MouseArea {
        x: Theme.sidebarWidth
        y: 0
        width: Math.max(0, bar.width - Theme.sidebarWidth - 132)
        height: bar.height
        onPressed: if (bar.controller) bar.controller.startSystemMove()
        onDoubleClicked: bar.toggleMaximizeRequested()
    }

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 9
        anchors.rightMargin: 10
        spacing: 3

        Repeater {
            model: ["minimize", "maximize", "close"]
            delegate: Rectangle {
                required property string modelData
                width: 34
                height: 30
                radius: 7
                color: captionMouse.containsMouse
                       ? (modelData === "close" ? Theme.windowButtonClose : Theme.windowButton)
                       : "transparent"

                Glyph {
                    anchors.centerIn: parent
                    name: modelData
                    stroke: captionMouse.containsMouse && modelData === "close" ? Theme.captionClose : Theme.captionIcon
                    width: 14
                    height: 14
                }

                MouseArea {
                    id: captionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !bar.busy
                    onClicked: {
                        if (modelData === "close") bar.closeRequested()
                        else if (modelData === "minimize") bar.minimizeRequested()
                        else bar.toggleMaximizeRequested()
                    }
                }
            }
        }
    }
}
