import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "Theme.js" as Theme

// Window shell: frame, navigation, page host, status footer and dialogs.
// Page content lives in OverviewPage / SubscriptionsPage / ProgramsPage /
// PreferencesPage / NodesPage; reusable widgets are separate files next to it.
ApplicationWindow {
    id: win
    width: 1120
    height: 760
    minimumWidth: 980
    minimumHeight: 690
    visible: true
    color: "transparent"
    title: "Muse"
    font.family: Theme.fontFamily
    flags: Qt.Window | Qt.FramelessWindowHint

    property int page: 0
    property bool captureMode: false
    // Set once the shutdown sequence has restored the network settings. Until then
    // every close request is only a request to start that sequence.
    property bool readyToQuit: false
    readonly property bool maximized: visibility === Window.Maximized
    readonly property bool locked: backend.locked
    readonly property bool changing: backend.transitioning

    function previewSubscriptionEditor() {
        page = 1
        editor.edit(-1)
    }

    // The window never closes on its own: the shutdown sequence restores the
    // network settings first, then emits readyToQuit which lets the close through.
    onClosing: function(close) {
        if (win.readyToQuit) {
            close.accepted = true
            return
        }
        close.accepted = false
        backend.requestQuit()
    }

    Connections {
        target: backend
        function onReadyToQuit() {
            win.readyToQuit = true
            Qt.quit()
        }
        function onSubscriptionSaved() { editor.close() }
    }

    readonly property var pageTitles: [
        "让连接，简单一点。",
        "你的订阅，都在这里。",
        "你的应用，随时可选。",
        "刚刚好的控制。",
        "每个节点，一目了然。"
    ]
    readonly property var pageSubtitles: [
        "为日常上网留一点从容。",
        "剩余流量、到期时间和订阅状态，集中管理。",
        "已安装或未启动都能选；不勾选时默认全局代理。",
        "常用设置，清晰可见。",
        "查看延迟、刷新列表，选择适合你的连接。"
    ]

    Rectangle {
        id: frame
        anchors.fill: parent
        anchors.margins: win.maximized ? 0 : 1
        radius: win.maximized ? 0 : Theme.radiusWindow
        color: Theme.surface
        border.color: Theme.edge
        clip: true

        AppSidebar {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            currentPage: win.page
            cornerRadius: frame.radius
            onNavigate: function(target) { win.page = target }
        }

        ColumnLayout {
            anchors.left: parent.left
            anchors.leftMargin: Theme.contentLeft
            anchors.right: parent.right
            anchors.rightMargin: Theme.contentRight
            anchors.top: parent.top
            anchors.topMargin: Theme.contentTop
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.contentBottom
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 7
                    Layout.fillWidth: true
                    Copy {
                        text: win.pageTitles[win.page]
                        font.pixelSize: Theme.pageTitleSize
                        font.weight: Font.DemiBold
                        font.letterSpacing: -.4
                    }
                    Copy {
                        text: win.pageSubtitles[win.page]
                        color: Theme.muted
                        font.pixelSize: Theme.pageSubtitleSize
                    }
                }
                StatusPill {
                    visible: win.page === 0
                    active: backend.connected
                    label: win.changing ? "处理中" : backend.connected ? "已启动" : "已关闭"
                }
            }

            StackLayout {
                currentIndex: win.page
                Layout.fillWidth: true
                Layout.fillHeight: true

                OverviewPage {
                    changing: win.changing
                    locked: win.locked
                    onNavigate: function(target) { win.page = target }
                }

                SubscriptionsPage {
                    locked: win.locked
                    onAddRequested: editor.edit(-1)
                    onEditRequested: function(index) { editor.edit(index) }
                    onRemoveRequested: function(index) {
                        removeDialog.targetIndex = index
                        removeDialog.open()
                    }
                }

                ProgramsPage {
                    locked: win.locked
                    captureMode: win.captureMode
                }

                PreferencesPage {
                    locked: win.locked
                }

                NodesPage {}
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: Theme.contentLeft
            anchors.right: parent.right
            anchors.rightMargin: Theme.contentRight
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            height: 34
            radius: Theme.radiusChip
            color: backend.error ? Theme.dangerSoft : "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: backend.error ? 10 : 0
                spacing: 8
                Rectangle {
                    width: 5
                    height: 5
                    radius: 3
                    color: backend.error ? Theme.danger : Theme.footerDot
                }
                Copy {
                    Layout.fillWidth: true
                    text: backend.pendingAction
                          ? "已接收：" + backend.pendingAction + " · 当前任务完成后执行"
                          : backend.message || "保持简单，连接自由。"
                    color: backend.error ? Theme.dangerText : Theme.footerText
                    font.pixelSize: Theme.captionSize
                    elide: Text.ElideRight
                    ToolTip.visible: footerMouse.containsMouse && !win.captureMode
                    ToolTip.text: text
                    ToolTip.delay: 700
                    MouseArea {
                        id: footerMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }
                Copy {
                    text: "MUSE"
                    font.pixelSize: 9
                    font.letterSpacing: 2
                    color: Theme.mutedLine
                }
            }
        }
    }

    TitleBar {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        controller: win
        maximized: win.maximized
        busy: backend.closing
        onCloseRequested: win.close()
        onMinimizeRequested: win.showMinimized()
        onToggleMaximizeRequested: win.maximized ? win.showNormal() : win.showMaximized()
    }

    ClosingOverlay {}

    SubscriptionDialog {
        id: editor
    }

    RemoveSubscriptionDialog {
        id: removeDialog
    }

    WindowHandles {
        anchors.fill: parent
        controller: win
        maximized: win.maximized
    }
}
