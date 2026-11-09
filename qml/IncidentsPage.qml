import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Flickable {
    id: page
    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    property int rev: Ops.revision
    property bool live: true
    property var snapshot: []
    property string application: ""
    property string moduleName: ""
    property string level: ""
    property string flow: ""
    property string kind: ""
    property string rangeKey: "all"
    property string query: ""
    property int pageNo: 0
    property string message: ""
    property var selected: ({})
    readonly property int pageSize: 12

    function showModule(name) {
        page.moduleName = name || ""
        page.query = ""
        page.pageNo = 0
        page.message = name ? name + " olayları süzüldü. Kaynak kayıtlar duruyor." : "Tüm olaylar."
    }
    function sourceRows() {
        page.rev
        return page.live ? Ops.incidents : page.snapshot
    }
    function matches(row) {
        if (!row || row.hidden) return false
        if (page.application && row.application !== page.application) return false
        if (page.moduleName && row.moduleName !== page.moduleName) return false
        if (page.level && row.level !== page.level) return false
        if (page.flow && row.flow !== page.flow) return false
        if (page.kind && row.kind !== page.kind) return false
        const now = Date.now()
        const span = page.rangeKey === "1h" ? 3600000 : (page.rangeKey === "24h" ? 86400000 : (page.rangeKey === "7d" ? 7 * 86400000 : (page.rangeKey === "30d" ? 30 * 86400000 : 0)))
        if (span > 0 && (now - Number(row.lastMs || 0)) > span) return false
        if (page.query.length > 0) {
            const blob = (row.id + " " + row.moduleName + " " + row.message + " " + row.kind + " " + row.application).toLowerCase()
            if (blob.indexOf(page.query.toLowerCase()) < 0) return false
        }
        return true
    }
    function rows() {
        const all = page.sourceRows().filter(page.matches)
        all.sort((a, b) => Number(b.lastMs || 0) - Number(a.lastMs || 0))
        return all
    }
    function pageRows() {
        const all = page.rows()
        return all.slice(page.pageNo * page.pageSize, (page.pageNo + 1) * page.pageSize)
    }
    function count(flow) {
        return page.rows().filter(row => !flow || row.flow === flow).length
    }
    function countLevel(level) {
        return page.rows().filter(row => row.level === level).length
    }
    function recentCount() {
        const now = Date.now()
        return page.rows().filter(row => now - Number(row.firstMs || 0) <= 86400000).length
    }
    function ids() {
        return page.rows().map(row => row.id).join("\n")
    }
    function levelColor(level) {
        if (level === "INFO") return Theme.cyan
        if (level === "WARNING") return Theme.amber
        if (level === "ERROR") return Theme.red
        if (level === "CRITICAL") return Theme.critical
        return Theme.muted
    }
    function openLogs(row) {
        const source = Ops.logSourceFor(row.moduleId)
        const level = row.level === "WARNING" ? "WARN" : (row.level === "CRITICAL" ? "CRITICAL" : "")
        logsPage.showIncident(source, level, row.moduleName, String(row.httpStatus || ""))
        window.pageIndex = 6
        page.message = source ? "Log Analizi bu modülün kaynağına süzüldü." : "Ayrı log kaynağı yok. Log ekranında yalnızca modül adı aranır. Kimlik yoksa oturum doğrulanamadı yazar."
    }

    ColumnLayout {
        id: body
        width: page.width
        spacing: 8
        Label { text: "SİSTEM OLAYLARI VE HATA GEÇMİŞİ"; color: Theme.title; font.pixelSize: 22; font.bold: true }
        Label { text: "Ticket-Go, Go-Chat ve bağlı sistemlerin olay kayıtları ve operasyon geçmişi"; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { visible: page.message.length > 0; text: page.message; color: Theme.cyan; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        GridLayout {
            columns: 4
            columnSpacing: 8
            rowSpacing: 8
            Layout.fillWidth: true
            Repeater {
                model: [
                    { label: "Kayıtlı olay", value: page.rows().length },
                    { label: "Aktif", value: page.count("ACTIVE") },
                    { label: "Kritik", value: page.countLevel("CRITICAL") },
                    { label: "Uyarı", value: page.countLevel("WARNING") },
                    { label: "Çözülen", value: page.count("RESOLVED") },
                    { label: "Onaylı", value: page.count("ACKNOWLEDGED") },
                    { label: "Son 24 saat", value: page.recentCount() },
                    { label: "Kapsam", value: page.rangeKey === "all" ? "tüm kayıtlar" : page.rangeKey }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 58
                    radius: 8
                    color: Theme.card
                    border.color: Theme.line
                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        Label { text: modelData.label; color: Theme.muted; font.pixelSize: 11 }
                        Label { text: String(modelData.value); color: Theme.text; font.pixelSize: 16; font.bold: true }
                    }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            ComboBox { model: ["Tüm sistemler", "Ticket-Go", "Go-Chat"]; onActivated: { page.application = index === 0 ? "" : currentText; page.pageNo = 0 } }
            ComboBox { model: ["Tüm seviyeler", "INFO", "WARNING", "ERROR", "CRITICAL"]; onActivated: { page.level = index === 0 ? "" : currentText; page.pageNo = 0 } }
            ComboBox { model: ["Tüm durumlar", "ACTIVE", "ACKNOWLEDGED", "RESOLVED"]; onActivated: { page.flow = index === 0 ? "" : currentText; page.pageNo = 0 } }
            ComboBox { model: ["Tüm süre", "Son 1 saat", "Son 24 saat", "Son 7 gün", "Son 30 gün"]; onActivated: { page.rangeKey = ["all", "1h", "24h", "7d", "30d"][index]; page.pageNo = 0 } }
            TextField { placeholderText: "Olay no, modül veya metin"; onEditingFinished: { page.query = text; page.pageNo = 0 } }
            Button { text: "Filtreleri temizle"; onClicked: { page.application = ""; page.moduleName = ""; page.level = ""; page.flow = ""; page.kind = ""; page.rangeKey = "all"; page.query = ""; page.pageNo = 0; page.message = "Filtreler temizlendi." } }
            CheckBox { text: "Canlı"; checked: page.live; onToggled: { if (!checked) page.snapshot = Ops.incidents; page.live = checked } }
            Button { text: "Yenile"; onClicked: { page.rev = Ops.revision; page.message = "Liste güncellendi. " + page.rows().length + " kayıt." } }
            Button { text: "Gizlenenleri geri yükle"; onClicked: { const result = Ops.restoreHiddenIncidents(); page.message = result && result.length > 0 ? result : "Gizlenen olaylar geri geldi. Kaynak değişmedi." } }
            Button { text: "CSV"; onClicked: page.message = Ops.exportIncidents("csv", page.ids()) }
            Button { text: "PDF"; onClicked: page.message = Ops.exportIncidents("pdf", page.ids()) }
        }

        ListView {
            Layout.fillWidth: true
            implicitHeight: Math.min(520, Math.max(80, count * 78))
            clip: true
            spacing: 6
            model: page.pageRows()
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 72
                radius: 8
                color: Theme.card
                border.color: page.levelColor(modelData.level)
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Rectangle { width: 8; height: parent.height; color: page.levelColor(modelData.level) }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: modelData.level + " · " + modelData.flow + " · " + modelData.application + " / " + modelData.moduleName; color: page.levelColor(modelData.level); elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: (modelData.lastSeen || "Veri yok") + " · tekrar " + modelData.repeatCount + " · " + (modelData.message || "Veri yok"); color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    Button { text: "Detay"; onClicked: { page.selected = modelData; detail.open() } }
                    Button { text: "Loglar"; onClicked: page.openLogs(modelData) }
                    Button { text: "Onayla"; enabled: modelData.flow === "ACTIVE"; onClicked: { const result = Ops.acknowledge(modelData.id); page.message = result && result.length > 0 ? result : "Onaylandı. Hata çözüldü sayılmadı." } }
                    Button { text: "Sil"; palette.button: Theme.criticalSurface; palette.buttonText: Theme.critical; onClicked: window.askDelete("incidentview", modelData.id, modelData.id, "Bu olay izleme görünümünden kaldırılacaktır. Kaynak sistemdeki loglar ve hata kayıtları silinmeyecektir.") }
                }
            }
        }
        Label { visible: page.rows().length === 0; text: "Bu filtrede kayıtlı olay yok."; color: Theme.muted }
        RowLayout {
            Button { text: "Önceki"; enabled: page.pageNo > 0; onClicked: page.pageNo -= 1 }
            Label { text: "Sayfa " + (page.pageNo + 1) + " / " + Math.max(1, Math.ceil(page.rows().length / page.pageSize)); color: Theme.muted }
            Button { text: "Sonraki"; enabled: (page.pageNo + 1) * page.pageSize < page.rows().length; onClicked: page.pageNo += 1 }
        }
    }

    Dialog {
        id: detail
        modal: true
        parent: Overlay.overlay
        palette.window: Theme.card
        palette.windowText: Theme.title
        palette.text: Theme.text
        palette.button: Theme.surface
        palette.buttonText: Theme.text
        background: Rectangle { color: Theme.card; border.color: Theme.line; border.width: 1; radius: 12 }
        anchors.centerIn: parent
        width: 560
        title: "Olay ayrıntısı"
        padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Flickable {
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
                    const row = page.selected
                    if (!row || !row.id) return "Olay seçilmedi."
                    const timeline = ["İlk tespit: " + (row.firstSeen || "Veri yok")]
                    if (Number(row.repeatCount) > 1) timeline.push("Tekrar tespit: " + row.repeatCount + " · son " + (row.lastSeen || "Veri yok"))
                    if (row.flow === "ACKNOWLEDGED") timeline.push("Kullanıcı onayı: " + (row.ackAt || "zaman kaydı yok") + ". Çözülme değildir.")
                    if (row.flow === "RESOLVED") timeline.push("Sağlık kontrolüyle çözüldü: " + (row.resolvedAt || "Veri yok"))
                    return [
                        "Olay ID: " + row.id,
                        "Uygulama: " + (row.application || "Veri yok"),
                        "Modül: " + (row.moduleName || "Veri yok"),
                        "Servis: " + (row.service || "Veri yok"),
                        "Tür: " + (row.kind || "Veri yok"),
                        "Seviye: " + (row.level || "Veri yok"),
                        "Durum: " + (row.flow || "Veri yok"),
                        "İlk: " + (row.firstSeen || "Veri yok"),
                        "Son: " + (row.lastSeen || "Veri yok"),
                        "Tekrar: " + row.repeatCount,
                        "HTTP: " + (row.httpStatus > 0 ? row.httpStatus : "Veri yok"),
                        "Hata kodu: Veri yok",
                        "Endpoint: Veri yok",
                        "Request ID: Veri yok",
                        "Correlation ID: Veri yok",
                        "Açıklama: " + (row.message || "Veri yok"),
                        "Log kaynağı: " + (row.logSource || "Veri yok"),
                        (row.causeLabel || "Olası neden") + ": " + (row.causeText || "Veri yok"),
                        "Kontrol: " + (row.recommendation || "Veri yok"),
                        "",
                        timeline.join("\n")
                    ].join("\n")
                }
            }
        }
        footer: Row {
            spacing: 6
            Button { text: "Kapat"; onClicked: detail.close() }
            Button { text: "Loglar"; onClicked: { detail.close(); page.openLogs(page.selected) } }
            Button { text: "Modül"; onClicked: { detail.close(); window.selectedId = page.selected.moduleId || "" } }
            Button { text: "Onayla"; enabled: page.selected.flow === "ACTIVE"; onClicked: { const result = Ops.acknowledge(page.selected.id); page.message = result && result.length > 0 ? result : "Onaylandı. Hata çözüldü sayılmadı."; detail.close() } }
            Button { text: "Kopyala"; onClicked: { const result = Ops.copyIncident(page.selected.id); page.message = result && result.length > 0 ? result : "Maskelenmiş olay metni kopyalandı." } }
        }
    }
}
