import QtQuick
import QtQuick.Window

// Invisible edge and corner strips. Native resize gestures keep the window
// borderless while behaving like a real resizable window.
Item {
    id: handles
    property var controller: null
    property bool maximized: false

    Repeater {
        model: [Qt.TopEdge, Qt.BottomEdge, Qt.LeftEdge, Qt.RightEdge,
                Qt.TopEdge | Qt.LeftEdge, Qt.TopEdge | Qt.RightEdge,
                Qt.BottomEdge | Qt.LeftEdge, Qt.BottomEdge | Qt.RightEdge]
        delegate: MouseArea {
            required property int modelData
            property bool leftEdge: (modelData & Qt.LeftEdge) !== 0
            property bool rightEdge: (modelData & Qt.RightEdge) !== 0
            property bool topEdge: (modelData & Qt.TopEdge) !== 0
            property bool bottomEdge: (modelData & Qt.BottomEdge) !== 0

            x: rightEdge ? handles.width - 6 : leftEdge ? 0 : 6
            y: bottomEdge ? handles.height - 6 : topEdge ? 0 : 6
            width: leftEdge || rightEdge ? 6 : handles.width - 12
            height: topEdge || bottomEdge ? 6 : handles.height - 12
            enabled: !handles.maximized
            cursorShape: (leftEdge || rightEdge) && (topEdge || bottomEdge)
                         ? (leftEdge === topEdge ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor)
                         : (leftEdge || rightEdge) ? Qt.SizeHorCursor : Qt.SizeVerCursor
            onPressed: if (handles.controller) handles.controller.startSystemResize(modelData)
        }
    }
}
