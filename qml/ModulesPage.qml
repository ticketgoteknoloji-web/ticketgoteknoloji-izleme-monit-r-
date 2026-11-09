import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Item {
    id: page
    property string groupFilter: "all"
    signal openModule(string moduleId)
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Row {
            spacing: 8
            Repeater {
                model: [
                    { id: "all", title: "Tümü" },
                    { id: "ticket-go", title: "Ticket-Go" },
                    { id: "gochat", title: "Go-Chat" },
                    { id: "custom", title: "Eklenenler" }
                ]
                Button {
                    required property var modelData
                    text: modelData.title
                    checked: page.groupFilter === modelData.id
                    onClicked: page.groupFilter = modelData.id
                }
            }
        }
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: flow.implicitHeight
            clip: true
            Flow {
                id: flow
                width: parent.width
                spacing: 16
                Repeater {
                    model: Ops.modules
                    delegate: ModuleCard {
                        required property var modelData
                        visible: page.groupFilter === "all" || modelData.group === page.groupFilter
                        width: visible ? 340 : 0
                        height: visible ? implicitHeight : 0
                        info: modelData
                        onOpen: (id) => page.openModule(id)
                        onRemoveRequested: (id, name) => window.askDelete("module", id, name, "Bu yerel izleme kartı silinecek. Uzak servis kapanmaz.")
                    }
                }
            }
        }
    }
}
