import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Node list with real HTTPS latency measured through the core. A manual choice is
// preserved: refreshing the list never silently overrides the user's selection.
ColumnLayout {
    id: root
    spacing: 16

    property string region: "全部地区"
    property int stateFilter: 0      // 0 全部 / 1 可用 / 2 未测速 / 3 超时
    property string needle: ""

    function resetScroll() {
        nodeList.positionViewAtBeginning()
    }

    Timer {
        id: searchDebounce
        interval: 150
        onTriggered: {
            root.needle = nodeSearch.text
            root.resetScroll()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Copy {
            text: backend.optimizationSummary
            color: Theme.green
            Layout.fillWidth: true
            font.pixelSize: Theme.pageSubtitleSize
            elide: Text.ElideRight
        }
        Command {
            text: backend.loaded ? "刷新列表" : "加载节点"
            iconName: "refresh"
            enabled: !backend.busy && backend.subscriptions.length > 0
            onClicked: backend.loaded ? backend.refreshNodes() : backend.loadNodes()
        }
        Command {
            text: backend.testing ? "正在优选…" : "重新优选"
            primary: true
            enabled: backend.loaded && !backend.busy
            onClicked: backend.testNodes()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Field {
            id: nodeSearch
            placeholderText: "搜索节点名称或协议…"
            Layout.fillWidth: true
            implicitHeight: 38
            onTextChanged: searchDebounce.restart()
        }
        ComboBox {
            id: statusBox
            Layout.preferredWidth: 132
            implicitHeight: 38
            font.pixelSize: 12
            leftPadding: 12
            rightPadding: 30
            model: ["全部状态", "可用节点", "尚未测速", "超时节点"]
            currentIndex: root.stateFilter
            onActivated: {
                root.stateFilter = currentIndex
                root.resetScroll()
            }
            background: Rectangle {
                radius: Theme.radiusControl
                color: statusBox.hovered ? Theme.comboHover : Theme.card
                border.color: Theme.line
            }
            contentItem: Copy {
                text: statusBox.displayText
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            indicator: Glyph {
                name: "arrow"
                rotation: 90
                width: 12
                height: 12
                x: statusBox.width - 24
                y: (statusBox.height - height) / 2
                stroke: Theme.comboGlyph
            }
        }
    }

    Flickable {
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        contentWidth: regionChips.implicitWidth
        contentHeight: 32
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Row {
            id: regionChips
            spacing: 6
            Repeater {
                model: backend.nodeRegions
                delegate: FilterChip {
                    required property string modelData
                    text: modelData
                    active: root.region === modelData
                    onClicked: {
                        root.region = modelData
                        root.resetScroll()
                    }
                }
            }
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: nodeList
            anchors.fill: parent
            anchors.margins: 10
            clip: true
            spacing: 7
            ScrollBar.vertical: ScrollBar {}

            model: backend.nodeDetails.filter(function(node) {
                if (root.region !== "全部地区" && node.region !== root.region) return false
                if (root.needle !== "" && (node.name + " " + node.type).toLowerCase().indexOf(root.needle.toLowerCase()) === -1) return false
                if (root.stateFilter === 1) return node.delay > 0
                if (root.stateFilter === 2) return node.delay < 0
                if (root.stateFilter === 3) return node.delay === 0
                return true
            })

            delegate: AbstractButton {
                id: nodeButton
                required property var modelData
                width: ListView.view.width
                height: 76
                hoverEnabled: true
                enabled: backend.loaded && !backend.closing
                property bool active: backend.currentNode === modelData.name
                Accessible.name: modelData.name + "，" + modelData.delayText
                onClicked: backend.selectNode(modelData.name)

                background: Rectangle {
                    radius: Theme.radiusRow
                    color: nodeButton.active ? Theme.nodeActive : nodeButton.hovered ? Theme.nodeHover : Theme.cardSoft
                    border.color: nodeButton.active ? Theme.nodeActiveEdge : Theme.lineSoft
                }

                contentItem: RowLayout {
                    spacing: 14
                    Item { Layout.preferredWidth: 8 }
                    Rectangle {
                        width: 28; height: 28
                        radius: 14
                        color: nodeButton.active ? Theme.nodeIconActive : Theme.nodeIcon
                        Glyph {
                            anchors.centerIn: parent
                            width: 16; height: 16
                            name: nodeButton.active ? "check" : "shield"
                            stroke: Theme.green
                        }
                    }
                    ColumnLayout {
                        spacing: 7
                        Layout.fillWidth: true
                        Copy {
                            text: modelData.name
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Copy {
                            text: modelData.region + "  ·  " + modelData.type + (nodeButton.active ? "  ·  当前节点" : "")
                            font.pixelSize: Theme.microSize
                            color: Theme.muted
                        }
                    }
                    Rectangle {
                        implicitWidth: 80
                        implicitHeight: 29
                        radius: Theme.radiusChip
                        color: modelData.delay > 0 ? Theme.goodSoft : modelData.delay === 0 ? Theme.warnSoft : Theme.neutralSoft
                        Copy {
                            anchors.centerIn: parent
                            text: modelData.delayText
                            font.pixelSize: Theme.captionSize
                            color: modelData.delay > 0 ? Theme.goodText : modelData.delay === 0 ? Theme.warnText : Theme.muted
                        }
                    }
                    Item { Layout.preferredWidth: 8 }
                }
            }

            Column {
                anchors.centerIn: parent
                visible: nodeList.count === 0
                spacing: 14
                Copy {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: backend.nodeDetails.length ? "当前分类下没有匹配节点。"
                          : backend.testing ? "正在加载并优选节点…"
                          : backend.subscriptions.length ? "先加载你的订阅节点。" : "还没有订阅。"
                    font.pixelSize: 17
                    font.weight: Font.Medium
                }
                Copy {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "自动测速与选择节点不会开启系统代理或 TUN。"
                    color: Theme.muted
                    font.pixelSize: Theme.pageSubtitleSize
                }
            }
        }
    }

    Copy {
        text: nodeList.count + " / " + backend.nodeDetails.length
              + " 个节点 · 地区按名称识别，启动后测速优选；全部超时保留原选择，定时刷新不会覆盖手动选择。"
        font.pixelSize: Theme.captionSize
        color: Theme.muted
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}
