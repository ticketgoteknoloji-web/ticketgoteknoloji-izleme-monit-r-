import QtQuick
import QtQuick.Layouts
import TicketGo.Ops

Item {
    id: page
    property int rev: Ops.revision
    property bool sceneActive: true
    signal openModule(string moduleId)
    signal openLogs(string moduleId)
    signal openAlerts(string moduleId)
    function matches(info) {
        page.rev
        return info.healthClass === "healthy"
            && info.status === "healthy"
            && info.connection === "Bağlı"
            && (info.lastCheck || "") !== ""
            && info.lastCheck !== "—"
    }
    function shownCount() {
        page.rev
        let count = 0
        const list = Ops.modules
        for (let i = 0; i < list.length; ++i) {
            if (page.matches(list[i])) count += 1
        }
        return count
    }
    function listeners() {
        page.rev
        const want = ["tg-api", "gochat-api", "tg-ferry", "tg-web", "tg-admin"]
        const list = Ops.modules || []
        const out = []
        for (let i = 0; i < list.length; ++i) {
            const row = list[i]
            if (want.indexOf(row.moduleId) >= 0 || String(row.moduleId || "").indexOf("custom-") === 0) out.push(row)
        }
        return out
    }
    function faults() {
        page.rev
        const list = Ops.modules || []
        const out = []
        for (let i = 0; i < list.length; ++i) {
            const cls = list[i].healthClass
            if (cls === "warning" || cls === "critical" || cls === "disconnected" || cls === "disabled") out.push(list[i])
        }
        return out
    }
    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        Column {
            id: column
            width: page.width
            spacing: 12
            Text {
                width: parent.width
                color: Theme.cyan
                font.pixelSize: 13
                font.bold: true
                text: "Yalnızca sağlıklı ve çalışan modüller"
            }
            Row {
                width: parent.width
                spacing: 10
                Repeater {
                    model: [
                        { label: "Toplam Sistem", value: Ops.totalCount, note: "Kayıtlı" },
                        { label: "Sağlıklı", value: Ops.healthyCount, note: "Doğrulanan" },
                        { label: "Uyarı", value: Ops.warningCount, note: "Ölçülen" },
                        { label: "Kritik", value: Ops.criticalCount, note: "Ölçülen" },
                        { label: "Bağlantı Kesildi", value: Ops.disconnectedCount, note: "Ölçülen" },
                        { label: "İzlenemeyen", value: Ops.unmonitoredCount, note: "Sağlıklı sayılmaz" }
                    ]
                    Rectangle {
                        width: (column.width - 50) / 6
                        height: 78
                        radius: 12
                        color: Theme.card
                        border.color: Theme.line
                        border.width: 1
                        Column {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2
                            Text { text: modelData.label; color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight; width: parent.width }
                            Text { text: modelData.value; color: Theme.text; font.pixelSize: 18; font.bold: true; elide: Text.ElideRight; width: parent.width }
                            Text { text: modelData.note; color: Theme.cyan; font.pixelSize: 10; elide: Text.ElideRight; width: parent.width }
                        }
                    }
                }
            }
            Row {
                width: parent.width
                spacing: 8
                Repeater {
                    model: [
                        { label: "İstasyon CPU", value: Ops.stationCpu },
                        { label: "İstasyon RAM", value: Ops.stationMem },
                        { label: "İstasyon disk", value: Ops.stationDisk },
                        { label: "Prob gecikmesi", value: Ops.avgLatencyText }
                    ]
                    Rectangle {
                        width: (column.width - 24) / 4
                        height: 52
                        radius: 10
                        color: Theme.card
                        border.color: Theme.line
                        border.width: 1
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            Text { text: modelData.label; color: Theme.muted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: modelData.value; color: Theme.text; font.pixelSize: 13; font.bold: true }
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width
                radius: 12
                color: Theme.card
                border.color: page.faults().length > 0 ? Theme.amber : Theme.line
                border.width: 1
                implicitHeight: liveColumn.implicitHeight + 20
                Column {
                    id: liveColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 6
                    Text { text: "Canlı dinleme"; color: Theme.title; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        text: "Backend, Go-Chat, feribot, web, admin ve eklenen uygulamalar 15 saniyede bir ölçülür. Ölçüm kaynağı olmayan alt sistem hata sayılmaz."
                    }
                    Flow {
                        width: parent.width
                        spacing: 8
                        Repeater {
                            model: page.listeners()
                            delegate: Rectangle {
                                required property var modelData
                                width: Math.min(220, liveColumn.width)
                                implicitHeight: listenText.implicitHeight + 12
                                radius: 8
                                color: Theme.surface
                                border.color: modelData.problem === true ? Theme.amber : Theme.line
                                Text {
                                    id: listenText
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 6
                                    wrapMode: Text.WordWrap
                                    color: Theme.text
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    text: modelData.name + " · " + (modelData.classLabel || modelData.statusLabel) + " · " + (modelData.lastCheck || "Ölçüm yok")
                                }
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: page.faults().length > 0 ? Theme.amber : Theme.green
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        text: page.faults().length > 0 ? "Canlı bulgu var. Olay eşiği ayrıca durur." : "Ölçülen hata yok. Dinleme sürüyor."
                    }
                    Repeater {
                        model: page.faults()
                        delegate: Text {
                            required property var modelData
                            width: liveColumn.width
                            wrapMode: Text.WordWrap
                            color: modelData.healthClass === "critical" || modelData.healthClass === "disconnected" ? Theme.critical : Theme.title
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            text: modelData.name + " · " + (modelData.classLabel || modelData.statusLabel) + " · " + (modelData.lastCheck || "Ölçüm yok") + " · " + (modelData.evidence || "Kanıt yok")
                        }
                    }
                }
            }
            Rectangle {
                width: Math.min(parent.width, 1080)
                anchors.horizontalCenter: parent.horizontalCenter
                height: 760
                radius: 18
                color: Theme.chart
                border.color: (Ops.criticalCount > 0 || Ops.hataVar) ? Theme.red : ((Ops.warningCount > 0 || Ops.disconnectedCount > 0) ? Theme.amber : Theme.cyan)
                border.width: 1
                ServerView {
                    anchors.fill: parent
                    anchors.margins: 8
                    active: page.sceneActive
                    onOpenDetail: (id) => page.openModule(id)
                    onOpenLogs: (id) => page.openLogs(id)
                    onOpenAlerts: (id) => page.openAlerts(id)
                }
            }
            Rectangle {
                width: Math.min(parent.width, 920)
                anchors.horizontalCenter: parent.horizontalCenter
                radius: 16
                color: Theme.card
                border.color: Theme.line
                border.width: 1
                implicitHeight: ratioColumn.implicitHeight + 28
                Column {
                    id: ratioColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    spacing: 4
                    Text { text: "Sağlık oranı"; color: Theme.muted; font.pixelSize: 12 }
                    Text { text: Ops.healthRatioText; color: Theme.title; font.pixelSize: 22; font.bold: true; width: parent.width; wrapMode: Text.WordWrap }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        font.pixelSize: 12
                        text: "Payda yalnız sağlıklı, uyarı, kritik ve bağlantısı kesilen servislerdir. İzlenemeyen ve yapılandırılmamış servisler sağlıklı sayılmaz."
                    }
                    Text {
                        color: Theme.cyan
                        font.pixelSize: 12
                        text: "Sağlıklı " + Ops.healthyCount + " · Uyarı " + Ops.warningCount + " · Kritik " + Ops.criticalCount + " · Bağlantı kesildi " + Ops.disconnectedCount + " · İzlenemeyen " + Ops.unmonitoredCount
                    }
                }
            }
            Rectangle {
                width: parent.width
                visible: page.shownCount() === 0
                implicitHeight: visible ? clearColumn.implicitHeight + 28 : 0
                radius: 16
                color: Theme.surface
                border.color: Theme.line
                border.width: 1
                Column {
                    id: clearColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 8
                    Text { text: "Şu anda doğrulanmış sağlıklı modül yok."; color: Theme.title; font.pixelSize: 22; font.bold: true }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        font.pixelSize: 13
                        text: "Bu ekran yalnız son kontrolü başarılı ve bağlantısı doğrulanmış modülleri gösterir. Uyarı, kritik ve bağlantısı kopan modüller Anlık Uyarılar ekranındadır. İzleme durmaz."
                    }
                    Text {
                        visible: Ops.unmonitoredCount > 0 || Ops.warningCount > 0 || Ops.criticalCount > 0 || Ops.disconnectedCount > 0
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.amber
                        font.pixelSize: 13
                        text: "İzlenemeyen " + Ops.unmonitoredCount + " · Uyarı " + Ops.warningCount + " · Kritik " + Ops.criticalCount + " · Bağlantı kesildi " + Ops.disconnectedCount + ". Bunlar sağlıklı sayılmaz."
                    }
                }
            }
            Flow {
                width: parent.width
                spacing: 12
                Repeater {
                    model: Ops.modules
                    delegate: ModuleCard {
                        required property var modelData
                        visible: page.matches(modelData)
                        width: visible ? 320 : 0
                        height: visible ? (modelData.problem === true ? 304 : 240) : 0
                        info: modelData
                        onOpen: (id) => page.openModule(id)
                        onRemoveRequested: (id, name) => window.askDelete("module", id, name, "Bu yerel izleme kartı silinecek. Uzak servis kapanmaz.")
                    }
                }
            }
            Rectangle {
                width: parent.width
                visible: Ops.hataVar || Ops.criticalCount > 0
                implicitHeight: visible ? alertColumn.implicitHeight + 20 : 0
                radius: 16
                color: Theme.criticalSurface
                border.color: Theme.red
                border.width: 2
                Column {
                    id: alertColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 6
                    Text { text: "HATA VAR"; color: Theme.red; font.pixelSize: 22; font.bold: true }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        font.pixelSize: 12
                        text: "Kritik durum veya eşik aşılmış kritik olay var. Çözülen olay karttan kalkar; olay geçmişi durur."
                    }
                    Repeater {
                        model: Ops.incidents
                        delegate: Text {
                            required property var modelData
                            visible: modelData.state !== "resolved" && modelData.severity === "critical"
                            width: alertColumn.width
                            color: Theme.text
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            text: modelData.moduleName + " · " + modelData.causeLabel + ": " + modelData.message
                        }
                    }
                }
            }
        }
    }
}
