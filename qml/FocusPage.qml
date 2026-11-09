import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Item {
    id: page
    property string group: "ticket-go"
    property string headline: ""
    property int rev: Ops.revision
    signal openModule(string moduleId)
    property var shown: {
        page.rev
        page.group
        const list = Ops.modules || []
        const out = []
        for (let i = 0; i < list.length; ++i) {
            if (list[i] && list[i].group === page.group) out.push(list[i])
        }
        return out
    }
    function columnCount(w) {
        if (w >= 340 * 3 + 32) return 3
        if (w >= 340 * 2 + 16) return 2
        return 1
    }
    property int cardWidth: {
        const w = Math.max(0, flow.width)
        const cols = page.columnCount(w)
        const gap = 16 * (cols - 1)
        return Math.max(300, Math.floor((w - gap) / cols))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            MarkImage {
                visible: page.group === "gochat"
                source: Ops.goChatLogo
                markSize: 48
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                Layout.maximumHeight: 48
            }
            MarkImage {
                visible: page.group === "ticket-go"
                source: Ops.logoFor("tg-api", "ticket-go")
                markSize: 48
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                Layout.maximumHeight: 48
            }
            Text {
                text: page.headline
                color: Theme.title
                font.pixelSize: 18
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.muted
            font.pixelSize: 13
            font.weight: Font.Medium
            text: page.group === "gochat"
                  ? "Go-Chat, Ticket-Go sağlık ucundan bağımsız izlenir. FCM, TURN ve SOS ayrı değerlendirilir. Mesaj, OTP ve konum gösterilmez."
                  : "Tek /health yanıtı alt servislerin sağlıklı olduğu anlamına gelmez. Alanı olmayan modüller bağlantı yapılandırılmadı olarak kalır."
        }
        Flickable {
            id: scroller
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            contentWidth: width
            contentHeight: flow.implicitHeight + 12
            Flow {
                id: flow
                width: scroller.width
                spacing: 16
                Repeater {
                    model: page.shown
                    delegate: ModuleCard {
                        required property var modelData
                        width: page.cardWidth
                        height: implicitHeight
                        info: modelData
                        onOpen: (id) => page.openModule(id)
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {
                policy: scroller.contentHeight > scroller.height + 2 ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }
        }
    }
}
