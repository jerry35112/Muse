import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Landing page: one big power switch, the resulting routing scope and a short
// node summary. Everything here reacts to backend state, nothing is invented.
ColumnLayout {
    id: root
    spacing: 16

    property bool changing: false
    property bool locked: false
    signal navigate(int page)

    Card {
        Layout.fillWidth: true
        Layout.preferredHeight: 198
        Layout.minimumHeight: 190

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Theme.radiusCard - 1
            color: backend.connected ? Theme.greenTintStrong : Theme.cardSoft
            Behavior on color { ColorAnimation { duration: 280 } }
        }

        ColumnLayout {
            anchors.left: parent.left
            anchors.leftMargin: 30
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            RowLayout {
                spacing: 7
                Glyph { name: "shield"; stroke: Theme.heroGlyph; implicitWidth: 14; implicitHeight: 14 }
                Copy {
                    text: "YOUR NETWORK, YOUR WAY"
                    color: Theme.mutedWarm
                    font.pixelSize: Theme.microSize
                    font.letterSpacing: 1.4
                }
            }
            Copy {
                text: root.changing
                      ? (backend.testing ? "正在检测节点延迟…"
                                         : backend.connected ? "正在更新代理状态…" : "正在准备节点与连接…")
                      : backend.connected ? "代理已开启，连接自由。" : "代理已关闭。"
                font.pixelSize: 25
                font.weight: Font.DemiBold
                color: backend.connected ? Theme.greenDeep : Theme.ink
            }
            Copy {
                text: backend.connected
                      ? (backend.currentNode || "正在获取节点")
                      : "点击启动，Muse 会按程序勾选自动决定代理范围。"
                color: Theme.heroSubtitle
                font.pixelSize: Theme.pageSubtitleSize
                elide: Text.ElideRight
                Layout.maximumWidth: 400
            }
            Command {
                text: backend.pendingAction === "切换代理状态" ? "取消等待"
                      : root.changing ? "请稍候…" : backend.connected ? "关闭代理" : "启动代理"
                primary: !backend.connected
                enabled: !backend.closing && !root.changing
                onClicked: backend.connectOrDisconnect()
                Layout.topMargin: 4
            }
        }

        Rectangle {
            id: powerCircle
            width: 132
            height: 132
            radius: 66
            anchors.right: parent.right
            anchors.rightMargin: 43
            anchors.verticalCenter: parent.verticalCenter
            color: "transparent"
            border.width: 1
            border.color: Theme.powerRing

            Rectangle {
                anchors.centerIn: parent
                width: 108
                height: 108
                radius: 54
                color: Theme.powerFill
            }
            Rectangle {
                anchors.centerIn: parent
                width: 84
                height: 84
                radius: 42
                color: backend.connected ? Theme.powerActive : powerMouse.containsMouse ? Theme.powerButtonHover : Theme.powerButton
                scale: powerMouse.pressed ? .94 : 1
                Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                Behavior on color { ColorAnimation { duration: 180 } }
                Glyph {
                    anchors.centerIn: parent
                    width: 34
                    height: 34
                    name: "power"
                    stroke: Theme.card
                }
                MouseArea {
                    id: powerMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !backend.closing && !root.changing
                    cursorShape: Qt.PointingHandCursor
                    onClicked: backend.connectOrDisconnect()
                }
            }
            BusyIndicator {
                anchors.fill: parent
                running: root.changing
                visible: running
                opacity: .5
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Copy { text: "代理范围 · 自动切换"; font.weight: Font.DemiBold }
        Item { Layout.fillWidth: true }
        Copy {
            text: root.locked ? backend.lockReason || "断开后修改程序选择" : "由程序勾选自动决定"
            color: Theme.muted
            font.pixelSize: Theme.captionSize
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.preferredHeight: 80
        color: Theme.greenTint
        border.color: Theme.greenEdge

        RowLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16
            Glyph {
                name: backend.selectedPaths.length ? "apps" : "shield"
                stroke: Theme.green
                implicitWidth: 26
                implicitHeight: 26
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 7
                Copy {
                    text: backend.selectedPaths.length
                          ? "仅代理所选的 " + backend.selectedPaths.length + " 个应用"
                          : "全局代理 · 所有程序"
                    color: Theme.green
                    font.weight: Font.DemiBold
                    font.pixelSize: Theme.sectionTitleSize
                }
                Copy {
                    text: backend.selectedPaths.length
                          ? "其他程序直连，取消所有勾选即可恢复全局。"
                          : "当前未勾选应用，默认代理全机流量。"
                    color: Theme.subtitle
                    font.pixelSize: Theme.captionSize
                }
            }
            Command {
                text: "选择应用"
                iconName: "apps"
                onClicked: root.navigate(2)
            }
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.preferredHeight: 76

        RowLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14
            Rectangle {
                width: 38
                height: 38
                radius: Theme.radiusIcon
                color: Theme.iconPlate
                Glyph {
                    anchors.centerIn: parent
                    name: "subscriptions"
                    stroke: Theme.nodeGlyph
                    width: 18
                    height: 18
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                Copy {
                    text: backend.currentNode || "还没有选择节点"
                    font.weight: Font.Medium
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Copy {
                    text: backend.subscriptions.length
                          ? backend.optimizationSummary
                          : "先添加一个订阅，开启你的第一次连接"
                    color: Theme.muted
                    font.pixelSize: Theme.captionSize
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
            Command {
                visible: backend.subscriptions.length > 0
                text: "节点 / 测速"
                onClicked: root.navigate(4)
            }
            Command {
                visible: backend.subscriptions.length === 0
                text: "添加订阅"
                iconName: "plus"
                onClicked: root.navigate(1)
            }
        }
    }

    Copy {
        text: backend.mode === 0
              ? "当前关闭了 TUN，仅覆盖遵循系统代理的应用；可在设置中开启完整接管。"
              : backend.admin
                ? "TUN 已准备就绪；局域网地址直连。程序选择会自动保存。"
                : "当前是标准模式。启动 TUN / 程序代理前，请右键 Muse → 以管理员身份运行。"
        color: !backend.admin && backend.mode !== 0 ? Theme.warn : Theme.muted
        font.pixelSize: Theme.captionSize
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }

    Item { Layout.fillHeight: true }
}
