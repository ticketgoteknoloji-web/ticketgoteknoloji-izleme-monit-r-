import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Flickable {
    id: page
    signal openHistory()
    property int rev: Ops.revision
    property string rankFilter: "active"
    property string moduleFocus: ""
    property string note: ""
    property string detailId: ""
    contentWidth: width
    contentHeight: column.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    function showModule(moduleId) {
        page.moduleFocus = moduleId || ""
        page.rankFilter = "all"
        page.detailId = ""
        page.note = moduleId ? "Seçilen entegrasyonun olayları. Yeni sahte olay yok." : "Aktif olaylar gösteriliyor."
    }
    function showAlarm(mode) {
        page.moduleFocus = ""
        page.rankFilter = mode || "active"
        page.detailId = ""
        page.note = mode === "critical" ? "Kritik olaylar öne alındı. Gösterge olayı onaylamadı." : (mode === "warning" ? "Aktif uyarılar gösteriliyor." : "Aktif olaylar gösteriliyor.")
    }

    function rows() {
        page.rev
        const list = []
        const source = Ops.incidents || []
        for (let i = 0; i < source.length; ++i) list.push(source[i])
        list.sort(function(a, b) {
            function rank(row) {
                const closed = row.state === "resolved" ? 20 : 0
                if (row.severity === "critical" && row.kind !== "connection_lost") return closed
                if (row.severity === "critical") return closed + 1
                if (row.severity === "warning") return closed + 2
                return closed + 3
            }
            return rank(a) - rank(b) || (b.lastMs || 0) - (a.lastMs || 0)
        })
        return list
    }

    function visibleRows() {
        const list = page.rows()
        const out = []
        for (let i = 0; i < list.length; ++i) {
            const row = list[i]
            const open = row.state !== "resolved"
            if (page.moduleFocus && row.moduleId !== page.moduleFocus) continue
            if (page.rankFilter === "critical" && !(open && row.severity === "critical")) continue
            if (page.rankFilter === "warning" && !(open && row.severity === "warning")) continue
            if (page.rankFilter === "disconnected" && !(open && row.kind === "connection_lost")) continue
            if (page.rankFilter === "active" && !open) continue
            out.push(row)
        }
        return out
    }

    function count(kind) {
        page.rev
        let total = 0
        const now = Date.now()
        const source = Ops.incidents || []
        for (let i = 0; i < source.length; ++i) {
            const row = source[i]
            const open = row.state !== "resolved"
            if (kind === "critical" && open && row.severity === "critical") total += 1
            if (kind === "warning" && open && row.severity === "warning") total += 1
            if (kind === "down" && open && row.kind === "connection_lost") total += 1
            if (kind === "ack" && row.state === "open") total += 1
            if (kind === "day" && row.lastMs && now - row.lastMs <= 86400000) total += 1
        }
        return total
    }

    function liveFaults() {
        page.rev
        const list = Ops.modules || []
        const out = []
        for (let i = 0; i < list.length; ++i) {
            const cls = list[i].healthClass
            if (cls === "warning" || cls === "critical" || cls === "disconnected" || cls === "disabled") out.push(list[i])
        }
        return out
    }

    function selected() {
        const list = page.rows()
        for (let i = 0; i < list.length; ++i) if (list[i].id === page.detailId) return list[i]
        return null
    }

    function levelText(row) {
        if (row.kind === "connection_lost") return "DISCONNECTED"
        if (row.severity === "critical") return "CRITICAL"
        if (row.severity === "warning") return "WARNING"
        return row.level || row.severity || "Veri yok"
    }

    function sourceLine(row) {
        const probe = Ops.moduleProbe(row.moduleId)
        const endpoint = probe.endpoint && probe.endpoint !== "—" ? probe.endpoint : ""
        const logName = Ops.logSourceFor(row.moduleId)
        const verified = row.causeLabel === "Kanıt" || row.causeLabel === "Alt servis"
        let text = "Uygulama: " + (row.application || "Veri yok")
            + "\nModül: " + (row.moduleName || "Veri yok")
            + "\nServis: " + (row.service || "Veri yok")
            + "\nEndpoint / işlem: " + (endpoint || "Veri yok")
            + (probe.note ? "\nÖlçüm notu: " + probe.note : "")
            + "\nHata kodu: Veri yok"
            + "\nLog kaynağı: " + (logName || "ayrı log kaynağı yok")
        if (verified && row.causeText) text += "\nDoğrulanmış bulgu: " + row.causeText
        else text += "\nKesin hata kaynağı doğrulanamadı"
        if (row.causeLabel === "Olası neden" && row.causeText) text += "\nOlası neden, teşhis değil: " + row.causeText
        return text
    }

    Column {
        id: column
        width: page.width
        spacing: 10
        Label { text: "ANLIK UYARILAR VE HATA TESPİT MERKEZİ"; color: Theme.title; font.pixelSize: 22; font.bold: true }
        Label { text: "Ticket-Go, Go-Chat ve bağlı sistemlerin gerçek zamanlı arıza ve uyarı takibi"; color: Theme.muted; wrapMode: Text.WordWrap; width: parent.width }
        Label { visible: page.note.length > 0; text: page.note; color: Theme.cyan; wrapMode: Text.WordWrap; width: parent.width }
        Label { text: "Masaüstü bildirimi ancak sessiz mod kapalı, bildirim ayarı açık ve sistem tepsisi mesaj destekliyorsa istenir. İzin yoksa bildirim görünmez. Susturulan olay çözülmez."; color: Theme.muted; wrapMode: Text.WordWrap; width: parent.width; font.pixelSize: 11 }

        Flow {
            width: parent.width
            spacing: 8
            Repeater {
                model: [
                    { name: "Aktif kritik", kind: "critical", tone: Theme.red },
                    { name: "Aktif uyarı", kind: "warning", tone: Theme.amber },
                    { name: "Bağlantı koptu", kind: "down", tone: Theme.red },
                    { name: "Onay bekleyen", kind: "ack", tone: Theme.cyan },
                    { name: "Son 24 saat", kind: "day", tone: Theme.text }
                ]
                delegate: Rectangle {
                    required property var modelData
                    width: 180
                    height: 64
                    radius: 8
                    color: Theme.card
                    border.color: Theme.line
                    Column {
                        anchors.centerIn: parent
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.name; color: Theme.muted; font.pixelSize: 11 }
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: page.count(modelData.kind); color: modelData.tone; font.pixelSize: 20; font.bold: true }
                    }
                }
            }
        }

        Flow {
            width: parent.width
            spacing: 6
            Button { text: "Aktif"; onClicked: page.rankFilter = "active" }
            Button { text: "Kritik"; onClicked: page.rankFilter = "critical" }
            Button { text: "Uyarı"; onClicked: page.rankFilter = "warning" }
            Button { text: "Bağlantı"; onClicked: page.rankFilter = "disconnected" }
            Button { text: "Tümü"; onClicked: page.rankFilter = "all" }
            Button { text: "Olay geçmişi"; onClicked: page.openHistory() }
        }

        Label { text: "Canlı ölçüm"; color: Theme.title; font.pixelSize: 16; font.weight: Font.DemiBold }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.muted
            text: page.liveFaults().length > 0 ? "Son ölçümdeki uyarı, kritik, kopuk bağlantı ve devre dışı kayıtlar. Ölçülemeyen sistem burada hata değildir." : "Son ölçümde raporlanacak hata yok. Dinleme sürüyor."
        }
        Repeater {
            model: page.liveFaults()
            delegate: Rectangle {
                required property var modelData
                width: column.width
                implicitHeight: faultText.implicitHeight + 16
                radius: 10
                color: Theme.card
                border.color: modelData.healthClass === "critical" || modelData.healthClass === "disconnected" ? Theme.critical : Theme.amber
                Text {
                    id: faultText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    wrapMode: Text.WordWrap
                    color: Theme.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    text: modelData.name + " · " + (modelData.classLabel || modelData.statusLabel) + " · " + (modelData.lastCheck || "Ölçüm yok") + "\n" + (modelData.evidence || "Kanıt yok")
                }
            }
        }

        Label { visible: page.visibleRows().length === 0; text: "Bu filtrede aktif olay yok."; color: Theme.muted; width: parent.width }

        Repeater {
            model: page.visibleRows()
            delegate: Rectangle {
                required property var modelData
                width: column.width
                implicitHeight: card.implicitHeight + 16
                radius: 10
                color: Theme.card
                border.color: modelData.severity === "critical" ? Theme.red : (modelData.state === "resolved" ? Theme.green : Theme.amber)
                Column {
                    id: card
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 4
                    Row {
                        spacing: 8
                        MarkImage { visible: modelData.application === "Go-Chat"; source: Ops.goChatLogo; markSize: 28 }
                        Label { text: page.levelText(modelData) + "  " + modelData.flow; color: modelData.severity === "critical" ? Theme.red : Theme.amber; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                    }
                    Label { text: (modelData.application || "Veri yok") + " · " + (modelData.moduleName || "Veri yok") + " · " + (modelData.service || "Veri yok"); color: Theme.text; font.bold: true; width: parent.width; wrapMode: Text.WordWrap }
                    Label { text: modelData.message || "Veri yok"; color: Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
                    Label { text: "Olay " + modelData.id; color: Theme.muted; font.pixelSize: 11; width: parent.width; elide: Text.ElideRight }
                    Label { text: "İlk " + modelData.firstSeen + " · Son " + modelData.lastSeen + " · Tekrar " + modelData.repeatCount + (modelData.silenced ? " · Bildirim susturuldu" : ""); color: Theme.cyan; font.pixelSize: 11; width: parent.width; wrapMode: Text.WordWrap }
                    Label { text: "HATA KAYNAĞI"; color: Theme.text; font.pixelSize: 11; font.bold: true }
                    Label { text: page.sourceLine(modelData); color: Theme.muted; font.pixelSize: 11; width: parent.width; wrapMode: Text.WordWrap }
                    Label { text: "Son sağlık: " + ((Ops.moduleMap(modelData.moduleId).classLabel) || "Veri yok") + " · HTTP " + ((Ops.moduleMap(modelData.moduleId).httpCode) || "Veri yok"); color: Theme.muted; font.pixelSize: 11; width: parent.width; wrapMode: Text.WordWrap }
                    Flow {
                        width: parent.width
                        spacing: 6
                        Button { text: "Detay"; onClicked: page.detailId = modelData.id }
                        Button {
                            text: "Logları gör"
                            onClicked: {
                                const source = Ops.logSourceFor(modelData.moduleId)
                                const http = modelData.httpStatus > 0 ? String(modelData.httpStatus) : ""
                                logsPage.showIncident(source, "", "", http, "", "", "")
                                logsPage.exportNote = source ? "Modül log kaynağı süzüldü. Request ve correlation kimliği olayda yok." : "Bu olay için ayrı log kaynağı yok. Sahte kayıt yok."
                                window.goPage(6)
                            }
                        }
                        Button { text: "Modüle git"; onClicked: { window.goPage(1); window.selectedId = modelData.moduleId } }
                        Button {
                            text: "Onayla"
                            enabled: modelData.state === "open"
                            onClicked: {
                                const result = Ops.acknowledge(modelData.id)
                                page.note = result && result.length > 0 ? result : "Onaylandı. Hata çözülmedi. Kritik gösterge açık kalır."
                            }
                        }
                        Button {
                            text: "Sustur"
                            enabled: modelData.state !== "resolved"
                            onClicked: {
                                const result = Ops.silenceIncident(modelData.id, 30)
                                page.note = result && result.length > 0 ? result : "30 dakika bildirim sustu. Alarm ve sağlık durumu değişmedi."
                            }
                        }
                    }
                }
            }
        }

        Label { text: "Log gözlemleri"; color: Theme.cyan; font.pixelSize: 16; font.bold: true }
        Label { visible: page.alertRows().length === 0; text: "Gösterilecek ERROR veya CRITICAL log kaydı yok."; color: Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
        Repeater {
            model: page.alertRows()
            delegate: Rectangle {
                required property var modelData
                width: column.width
                implicitHeight: logBody.implicitHeight + 16
                radius: 10
                color: Theme.card
                border.color: Theme.red
                Column {
                    id: logBody
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 4
                    Label { text: modelData.level + " · " + modelData.module + " · " + modelData.time; color: Theme.text; font.bold: true; width: parent.width; wrapMode: Text.WordWrap }
                    Label { text: modelData.message; color: Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
                    Label { text: modelData.note; color: Theme.amber; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 11 }
                    Button { text: "Bildirimi kapat"; onClicked: Ops.dismissLogAlert(modelData.id) }
                }
            }
        }
        Item { height: 8 }
    }

    function alertRows() {
        page.rev
        return Ops.logAlerts()
    }

    Dialog {
        id: detail
        parent: Overlay.overlay
        modal: true
        palette.window: Theme.card
        palette.windowText: Theme.title
        palette.text: Theme.text
        palette.button: Theme.surface
        palette.buttonText: Theme.text
        background: Rectangle { color: Theme.card; border.color: Theme.line; border.width: 1; radius: 12 }
        title: "Olay ayrıntısı"
        visible: page.detailId.length > 0
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        onClosed: page.detailId = ""
        contentItem: Flickable {
            width: 520
            height: 360
            contentHeight: detailText.implicitHeight
            clip: true
            Label {
                id: detailText
                width: 500
                wrapMode: Text.WordWrap
                color: Theme.text
                text: {
                    const row = page.selected()
                    if (!row) return ""
                    const probe = Ops.moduleProbe(row.moduleId)
                    const current = Ops.moduleMap(row.moduleId)
                    return "Olay: " + row.id
                        + "\nUygulama: " + (row.application || "Veri yok")
                        + "\nModül: " + (row.moduleName || "Veri yok")
                        + "\nServis: " + (row.service || "Veri yok")
                        + "\nHata kodu: Veri yok"
                        + "\nHTTP: " + (row.httpStatus > 0 ? row.httpStatus : (current.httpCode || "Veri yok"))
                        + "\nZaman aşımı: " + (probe.timeoutMs > 0 ? probe.timeoutMs + " ms kontrol süresi" : "Veri yok")
                        + "\nRequest ID: Veri yok"
                        + "\nCorrelation ID: Veri yok"
                        + "\nLog kaynağı: " + (Ops.logSourceFor(row.moduleId) || row.logSource || "ayrı log kaynağı yok")
                        + "\nİlk: " + row.firstSeen + "\nSon: " + row.lastSeen
                        + "\nTekrar: " + row.repeatCount
                        + "\nEtkilenen sistem: " + (row.moduleId || "Veri yok")
                        + "\n\n" + page.sourceLine(row)
                        + "\n\nÖnerilen kontrol: " + (row.recommendation || "Veri yok")
                        + "\nOnay ve susturma hatayı çözmez."
                }
            }
        }
    }
}
