import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Application split tunnelling: pick executable paths, regardless of whether the
// application is running yet.
ColumnLayout {
    id: root
    spacing: 10

    property bool locked: false
    property bool captureMode: false
    property int source: 0            // 0 全部 / 1 运行中 / 2 未启动 / 3 手动添加 / 4 已选
    property string category: "全部应用"
    property string needle: ""        // debounced copy of the search box
    property var selection: ({})
    property real savedScroll: 0

    // A hash lookup instead of scanning the selected list for every card.
    function refreshSelection() {
        const paths = backend.selectedPaths
        const map = Object.create(null)
        for (let index = 0; index < paths.length; ++index) map[paths[index].toLowerCase()] = true
        root.selection = map
    }
    function isPicked(path) {
        return path !== undefined && root.selection[String(path).toLowerCase()] === true
    }
    function showMenu(row) {
        appMenu.app = row
        appMenu.popup()
    }
    function resetScroll() {
        appGrid.positionViewAtBeginning()
    }

    Component.onCompleted: refreshSelection()

    Connections {
        target: backend
        function onSelectionChanged() { root.refreshSelection() }
        function onCatalogWillChange() {
            root.savedScroll = appGrid.visible ? Math.max(0, appGrid.contentY - appGrid.originY) : 0
        }
        function onProcessesChanged() {
            Qt.callLater(function() {
                appGrid.forceLayout()
                const columns = Math.max(1, Math.round(appGrid.width / appGrid.cellWidth))
                const index = Math.min(appGrid.count - 1, Math.floor(root.savedScroll / appGrid.cellHeight) * columns)
                if (index > 0) appGrid.positionViewAtIndex(index, GridView.Beginning)
                else appGrid.positionViewAtBeginning()
            })
        }
    }

    // Search is debounced: filtering a few thousand rows on every keystroke made
    // typing lag behind.
    Timer {
        id: searchDebounce
        interval: 150
        onTriggered: {
            root.needle = searchField.text
            root.resetScroll()
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.preferredHeight: 58
        Layout.minimumHeight: 58
        color: Theme.greenTint
        border.color: Theme.greenEdge

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 13
            Glyph {
                name: backend.selectedPaths.length ? "apps" : "shield"
                stroke: Theme.green
                implicitWidth: 24
                implicitHeight: 24
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Copy {
                    text: backend.selectedPaths.length
                          ? "仅代理 " + backend.selectedPaths.length + " 个已选应用"
                          : "全局代理已就绪"
                    font.weight: Font.DemiBold
                    color: Theme.green
                    font.pixelSize: 14
                }
                Copy {
                    text: backend.selectedPaths.length
                          ? "其他应用直连 · 取消所有勾选恢复全局"
                          : "未启动的应用也可以预先选择"
                    color: Theme.subtitle
                    font.pixelSize: Theme.captionSize
                }
            }
            Command {
                text: "恢复全局"
                enabled: !root.locked && backend.selectedPaths.length > 0
                onClicked: backend.clearProcesses()
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Field {
            id: searchField
            placeholderText: "搜索应用名称、文件名或路径…"
            Layout.fillWidth: true
            implicitHeight: 40
            onTextChanged: searchDebounce.restart()
        }
        Command {
            text: backend.catalogBusy ? "刷新中…" : "刷新应用库"
            iconName: "refresh"
            enabled: !backend.closing
            implicitHeight: 40
            onClicked: backend.refreshProcesses()
        }
        Command {
            text: "添加应用"
            iconName: "plus"
            primary: true
            enabled: !root.locked
            onClicked: backend.browseProgram()
        }
    }

    Row {
        spacing: 5
        Layout.fillWidth: true
        Repeater {
            model: [
                { label: "全部应用", value: 0 },
                { label: "正在运行", value: 1 },
                { label: "未启动", value: 2 },
                { label: "手动添加", value: 3 },
                { label: "已选 " + backend.selectedPaths.length, value: 4 }
            ]
            delegate: FilterChip {
                required property var modelData
                objectName: "source-" + modelData.value
                text: modelData.label
                active: root.source === modelData.value
                onClicked: {
                    root.source = modelData.value
                    root.category = "全部应用"
                    root.resetScroll()
                }
            }
        }
    }

    Flickable {
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        contentWidth: categoryChips.implicitWidth
        contentHeight: 32
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Row {
            id: categoryChips
            spacing: 6
            Repeater {
                model: backend.applicationCategories
                delegate: FilterChip {
                    required property var modelData
                    text: modelData.key + "  " + modelData.count
                    active: root.category === modelData.key
                    onClicked: {
                        root.category = modelData.key
                        root.resetScroll()
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Copy {
            text: (root.source === 4 ? "已选应用" : root.category) + " · " + appGrid.count
            color: Theme.ink
            font.weight: Font.DemiBold
        }
        Item { Layout.fillWidth: true }
        Copy {
            text: backend.catalogBusy ? "后台发现应用中"
                  : backend.savingSelection ? "正在保存选择…"
                  : root.locked ? (backend.lockReason || "断开后可修改")
                  : "选择已自动保存"
            color: backend.savingSelection ? Theme.green : root.locked ? Theme.warn : Theme.muted
            font.pixelSize: Theme.captionSize
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true

        GridView {
            id: appGrid
            objectName: "application-grid"
            anchors.fill: parent
            anchors.margins: 8
            clip: true
            cellWidth: width / Math.max(2, Math.floor(width / 235))
            cellHeight: 120
            reuseItems: true
            currentIndex: -1
            ScrollBar.vertical: ScrollBar {}

            model: backend.processes.filter(function(row) {
                const sourceMatch = root.source === 4
                        ? root.isPicked(row.path)
                        : !row.background && (root.source === 0
                                              || (root.source === 1 && row.running)
                                              || (root.source === 2 && !row.running)
                                              || (root.source === 3 && row.manual))
                if (!sourceMatch) return false
                if (root.category !== "全部应用" && row.category !== root.category) return false
                if (root.needle === "") return true
                return (row.name + row.filename + row.path).toLowerCase().indexOf(root.needle.toLowerCase()) !== -1
            })

            delegate: Item {
                id: cell
                required property var modelData
                width: appGrid.cellWidth
                height: appGrid.cellHeight
                property bool selected: root.isPicked(modelData.path)

                AbstractButton {
                    id: appButton
                    objectName: "application-" + modelData.icon
                    anchors.fill: parent
                    anchors.margins: 6
                    hoverEnabled: true
                    enabled: !root.locked
                    Accessible.role: Accessible.CheckBox
                    Accessible.name: modelData.name + "，" + (cell.selected ? "已选择代理" : "未选择")
                    Accessible.checked: cell.selected
                    onClicked: backend.toggleProcess(modelData.path)

                    PathTip {
                        visible: appButton.hovered && !root.captureMode && !appMenu.opened
                        text: modelData.path
                    }

                    background: Rectangle {
                        radius: 13
                        color: cell.selected ? Theme.greenWash : appButton.hovered ? Theme.hoverSoft : Theme.cardSoft
                        border.width: cell.selected ? 1.5 : 1
                        border.color: cell.selected ? Theme.greenLine : Theme.cardEdge
                        Behavior on color { ColorAnimation { duration: 140 } }
                    }

                    contentItem: Item {
                        Rectangle {
                            x: 12; y: 10
                            width: 38; height: 38
                            radius: Theme.radiusIcon
                            color: Theme.card
                            border.color: Theme.iconPlateEdge
                            Glyph {
                                anchors.centerIn: parent
                                width: 23; height: 23
                                name: "apps"
                                stroke: Theme.iconGlyph
                                visible: appIcon.status !== Image.Ready
                            }
                            Image {
                                id: appIcon
                                anchors.centerIn: parent
                                width: 28; height: 28
                                sourceSize.width: 56
                                sourceSize.height: 56
                                source: "image://apps/" + modelData.icon
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            y: 15
                            width: 18; height: 18
                            radius: 6
                            color: cell.selected ? Theme.green : Theme.card
                            border.color: cell.selected ? Theme.green : Theme.checkEdge
                            Glyph {
                                anchors.centerIn: parent
                                width: 13; height: 13
                                name: "check"
                                stroke: Theme.card
                                visible: cell.selected
                            }
                        }
                        Copy {
                            x: 12; y: 58
                            width: parent.width - 24
                            text: modelData.name
                            font.weight: Font.DemiBold
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Row {
                            x: 12; y: 81
                            spacing: 5
                            Rectangle {
                                width: 5; height: 5
                                radius: 3
                                color: modelData.running ? Theme.dotRunning : Theme.dotIdle
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Copy {
                                text: modelData.category + (modelData.missing ? " · 文件缺失"
                                      : modelData.background ? " · 已选后台程序"
                                      : modelData.running ? " · 运行中" : " · 未启动")
                                width: appButton.width - 72
                                font.pixelSize: Theme.microSize
                                color: modelData.missing ? Theme.missingText : Theme.muted
                                elide: Text.ElideRight
                            }
                        }
                        ToolButton {
                            id: appMore
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            y: 72
                            width: 25; height: 25
                            text: "⋯"
                            font.pixelSize: 17
                            onClicked: root.showMenu(modelData)
                            background: Rectangle {
                                radius: 6
                                color: appMore.down ? Theme.morePressed : appMore.hovered ? Theme.hoverTint : "transparent"
                            }
                        }
                    }
                }
            }

            Copy {
                anchors.centerIn: parent
                visible: appGrid.count === 0
                width: Math.min(parent.width - 40, 420)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: backend.catalogBusy ? "正在后台发现应用，已有选择不会改变…"
                      : root.source === 4 ? "还没有选择应用，当前为全局代理。"
                      : root.needle !== "" ? "未找到匹配应用，试试其他关键词。"
                      : root.category !== "全部应用" ? "该分类暂时没有应用。"
                      : "尚未发现应用，可以点击「添加应用」。"
                color: Theme.muted
            }
        }
    }

    Copy {
        text: "来自开始菜单、桌面快捷方式与应用注册信息。未启动也能选；手动添加会留在应用库。系统服务和辅助进程默认隐藏。"
        color: Theme.muted
        font.pixelSize: Theme.microSize
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }

    Menu {
        id: appMenu
        property var app: ({})
        width: 192
        padding: 5
        background: Card { radius: Theme.radiusControl }
        MenuItem { text: "查看文件位置"; onTriggered: backend.revealApplication(appMenu.app.path) }
        MenuItem { text: "复制应用路径"; onTriggered: backend.copyApplicationPath(appMenu.app.path) }
        MenuSeparator {}
        MenuItem {
            text: root.isPicked(appMenu.app.path || "") ? "取消代理此应用" : "代理此应用"
            enabled: !root.locked
            onTriggered: backend.toggleProcess(appMenu.app.path)
        }
        MenuItem {
            text: "移除手动记录"
            visible: !!appMenu.app.manual
            enabled: !root.locked && !root.isPicked(appMenu.app.path || "")
            onTriggered: backend.forgetApplication(appMenu.app.path)
        }
    }
}
