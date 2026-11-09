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
    property string kind: "overview"
    property string rangeKey: "24h"
    property string moduleId: ""
    property string message: ""
    property var preview: ({})
    function focusPerformance(moduleId, minutes) {
        page.moduleId = moduleId || ""
        page.kind = "performance"
        page.rangeKey = minutes <= 60 ? "1h" : (minutes <= 1440 ? "24h" : "7d")
        page.loadPreview()
        page.message = "Performans raporu açıldı. Rapor dönemi 1 saat, 24 saat veya 7 gündür."
    }
    function loadPreview() {
        page.rev
        const doc = Ops.reportDocument(page.kind, page.moduleId, page.rangeKey, fromField.text, toField.text)
        page.preview = doc
        if (doc && doc.ok === false) page.message = doc.error
    }
    function savedRows() { page.rev; return Ops.savedReports() }
    function moduleName(id) {
        const info = Ops.moduleMap(id)
        return info && info.name ? info.name : id
    }
    Component.onCompleted: page.loadPreview()

    ColumnLayout {
        id: body
        width: page.width
        spacing: 10
        Label { text: "SİSTEM RAPORLARI VE ANALİZ MERKEZİ"; color: Theme.title; font.pixelSize: 22; font.bold: true }
        Label { text: "Ticket-Go, Go-Chat ve bağlı sistemlerin sağlık, performans ve olay raporları"; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { visible: page.message.length > 0; text: page.message; color: Theme.cyan; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        GridLayout {
            columns: 4
            columnSpacing: 8
            rowSpacing: 8
            Layout.fillWidth: true
            Repeater {
                model: [
                    { label: "İzlenen", value: (page.preview.healthyNow || 0) + (page.preview.warningNow || 0) + (page.preview.criticalNow || 0) + (page.preview.unmonitoredNow || 0) },
                    { label: "Sağlıklı", value: page.preview.healthyNow ?? "—" },
                    { label: "Kritik anlık", value: page.preview.criticalNow ?? "—" },
                    { label: "Uyarı anlık", value: page.preview.warningNow ?? "—" },
                    { label: "Erişilebilirlik", value: page.preview.availability || "Hesaplanamadı" },
                    { label: "Ortalama yanıt", value: page.preview.avgLatency || "ölçüm yok" },
                    { label: "Başarısız kontrol", value: page.preview.errors ?? "—" },
                    { label: "Dönem kapsamı", value: (page.preview.measured ?? "—") + " ölçüm / " + (page.preview.excluded ?? "—") + " kapsam dışı" }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 64
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

        RowLayout {
            Label { text: "Tür"; color: Theme.text }
            ComboBox {
                id: kindBox
                model: ["Genel sağlık", "Ticket-Go", "Go-Chat", "Performans", "Hata ve olay", "Erişilebilirlik", "Entegrasyon", "Tek modül"]
                onActivated: {
                    const keys = ["overview", "ticketgo", "gochat", "performance", "incidents", "availability", "integrations", "module"]
                    page.kind = keys[index]
                    page.loadPreview()
                }
            }
            Label { text: "Dönem"; color: Theme.text }
            ComboBox {
                model: ["Son 1 saat", "Son 24 saat", "Son 7 gün", "Son 30 gün", "Özel"]
                currentIndex: 1
                onActivated: {
                    const keys = ["1h", "24h", "7d", "30d", "custom"]
                    page.rangeKey = keys[index]
                    page.loadPreview()
                }
            }
            Label { text: "Modül"; color: Theme.text }
            ComboBox {
                id: moduleBox
                model: ["Tümü"].concat(Ops.modules.map(item => item.name))
                onActivated: {
                    page.moduleId = index === 0 ? "" : Ops.modules[index - 1].id
                    page.loadPreview()
                }
            }
            Button { text: "Filtreleri temizle"; onClicked: { kindBox.currentIndex = 0; moduleBox.currentIndex = 0; page.kind = "overview"; page.rangeKey = "24h"; page.moduleId = ""; fromField.text = ""; toField.text = ""; page.loadPreview(); page.message = "Filtreler temizlendi." } }
            Button { text: "Yenile"; onClicked: page.loadPreview() }
            Button { text: "Yeni rapor oluştur"; onClicked: wizard.open() }
        }
        RowLayout {
            visible: page.rangeKey === "custom"
            Label { text: "Başlangıç ISO"; color: Theme.muted }
            TextField { id: fromField; placeholderText: "2026-11-09T00:00:00" }
            Label { text: "Bitiş ISO"; color: Theme.muted }
            TextField { id: toField; placeholderText: "2026-11-09T23:59:00" }
            Button { text: "Dönemi uygula"; onClicked: page.loadPreview() }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 180
            radius: 10
            color: Theme.card
            border.color: Theme.line
            Column {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6
                Label { text: "API gecikme · ms · kaynak: yerel ölçüm"; color: Theme.muted }
                Sparkline { width: parent.width; height: 110; points: page.preview.latency || [] }
                Label { text: "P95: " + (page.preview.p95 || "yeterli ham veri yok") + " · ortalama: " + (page.preview.avgLatency || "ölçüm yok") + " · son nokta: " + ((page.preview.latency && page.preview.latency.length > 0) ? (page.preview.latency[page.preview.latency.length - 1] + " ms") : "ölçüm yok"); color: Theme.text; wrapMode: Text.WordWrap; width: parent.width }
                Row {
                    spacing: 6
                    Rectangle { width: Math.max(4, (page.preview.healthyNow || 0) * 14); height: 14; color: Theme.green }
                    Rectangle { width: Math.max(4, (page.preview.warningNow || 0) * 14); height: 14; color: Theme.amber }
                    Rectangle { width: Math.max(4, (page.preview.criticalNow || 0) * 14); height: 14; color: Theme.red }
                    Rectangle { width: Math.max(4, (page.preview.unmonitoredNow || 0) * 14); height: 14; color: Theme.gray }
                    Label { text: "yeşil sağlıklı, sarı uyarı, kırmızı kritik, gri izlenemeyen"; color: Theme.muted; font.pixelSize: 11 }
                }
            }
        }
        Label { text: page.preview.summary || "Önizleme için tür ve dönem seçin."; color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        Label { text: "KAYDEDİLEN RAPORLAR"; color: Theme.text; font.bold: true }
        ListView {
            Layout.fillWidth: true
            implicitHeight: Math.min(360, Math.max(48, count * 72))
            clip: true
            model: page.savedRows()
            spacing: 6
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 66
                radius: 8
                color: Theme.card
                border.color: Theme.line
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: modelData.name + " · " + modelData.kind; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: modelData.created + " · " + modelData.period + " · " + modelData.format + " · " + modelData.status; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    Button { text: "Detay"; onClicked: { detailText.text = modelData.summary || "Özet yok"; detail.open() } }
                    Button { text: "Aç"; onClicked: { const result = Ops.openSavedReport(modelData.id); page.message = result && result.length > 0 ? result : "Dosya açma isteği kabul edildi." } }
                    Button { text: "Finder"; onClicked: { const result = Ops.revealSavedReport(modelData.id); page.message = result && result.length > 0 ? result : "Finder isteği kabul edildi." } }
                    Button { text: "Sil"; palette.button: Theme.criticalSurface; palette.buttonText: Theme.critical; onClicked: window.askDelete("report", modelData.id, modelData.name, "Yalnızca bu yerel rapor kaydı ve onun PDF/CSV dosyası silinir. Ölçüm, olay, audit ve backend verisi durur.") }
                }
            }
        }
        Label { visible: page.savedRows().length === 0; text: "Kayıtlı rapor yok."; color: Theme.muted }
        Item { Layout.preferredHeight: 8 }
    }

    Dialog {
        id: wizard
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
        title: "Yeni rapor"
        padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property int step: 0
        Column {
            width: 520
            spacing: 8
            Label { color: Theme.text; text: "Adım " + (wizard.step + 1) + " / 4" }
            Label { visible: wizard.step === 0; color: Theme.muted; wrapMode: Text.WordWrap; width: 520; text: "Tür: " + page.kind + ". Modül: " + (page.moduleId || "tümü") + ". Dönem: " + page.rangeKey + "." }
            Label { visible: wizard.step === 1; color: Theme.muted; wrapMode: Text.WordWrap; width: 520; text: "Alanlar: sağlık, erişilebilirlik, gecikme, olay. Log gövdesi ve gizli değer alınmaz." }
            Label { visible: wizard.step === 2; color: Theme.text; wrapMode: Text.WordWrap; width: 520; text: page.preview.summary || "" }
            Row {
                visible: wizard.step === 3
                spacing: 8
                Button { text: "Kaydet"; highlighted: true; onClicked: { const result = Ops.saveReportRecord(page.kind, page.kind, page.preview.period || "", page.preview.system || "", "kayıt", page.preview.summary || ""); page.message = result && result.length > 0 ? result : "Yerel rapor kaydı yazıldı. Dosya üretilmedi."; wizard.close() } }
                Button { text: "PDF"; onClicked: { page.message = Ops.exportReportDocument(page.kind, "pdf", page.moduleId, page.rangeKey, fromField.text, toField.text); wizard.close() } }
                Button { text: "CSV"; onClicked: { page.message = Ops.exportReportDocument(page.kind, "csv", page.moduleId, page.rangeKey, fromField.text, toField.text); wizard.close() } }
            }
        }
        footer: Row {
            spacing: 8
            Button { text: "İptal"; onClicked: wizard.close() }
            Button { text: "Geri"; enabled: wizard.step > 0; onClicked: wizard.step -= 1 }
            Button { text: "İleri"; enabled: wizard.step < 3; onClicked: { page.loadPreview(); if (page.preview.ok === false) page.message = page.preview.error; else wizard.step += 1 } }
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
        title: "Rapor önizleme"
        padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Label { id: detailText; width: 520; wrapMode: Text.WordWrap; color: Theme.text }
        footer: Button { text: "Kapat"; onClicked: detail.close() }
    }
}
