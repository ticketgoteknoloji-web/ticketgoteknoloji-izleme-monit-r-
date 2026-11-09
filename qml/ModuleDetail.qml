import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Rectangle {
    id: page
    property string moduleId: ""
    property int rev: Ops.revision
    signal close
    color: Theme.card
    radius: 16
    border.color: Theme.cyan

    function card() { page.rev; return Ops.moduleMap(page.moduleId) }
    function probe() { return Ops.moduleProbe(page.moduleId) }

    property string level: ""
    property string query: ""
    property int minutes: 60
    property string actionNote: ""
    property string openLogId: ""

    Shortcut { sequence: "Escape"; enabled: page.visible; onActivated: page.close() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            MarkImage { source: page.card().logo || ""; visible: (page.card().logo || "").length > 0; markSize: 48 }
            ColumnLayout {
                Layout.fillWidth: true
                Text { text: (page.card().name || "Modül") + " — Sistem detayları"; color: Theme.text; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { text: (page.card().classLabel || page.card().statusLabel || "—") + " · " + (page.card().connection || "—"); color: Theme.toneColor(page.card().tone || "gray"); font.bold: true }
            }
            Button { text: "Yenile"; onClicked: page.actionNote = Ops.probeModule(page.moduleId) }
            Button { text: "Geri"; onClicked: page.close() }
        }
        Text { text: page.actionNote; color: Theme.amber; visible: page.actionNote.length > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: body.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: body
                width: parent.width
                spacing: 12
                GridLayout {
                    width: parent.width
                    columns: 3
                    columnSpacing: 8
                    rowSpacing: 8
                    Repeater {
                        model: [
                            { k: "Son kontrol", v: page.card().lastCheck || "—" },
                            { k: "Bağlantı", v: page.card().connection || "—" },
                            { k: "HTTP", v: page.card().httpCode || "—" },
                            { k: "Gecikme", v: page.card().latency || "—" },
                            { k: "Prob", v: page.card().probes || "—" },
                            { k: "Ortam", v: Ops.environmentText }
                        ]
                        Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 52
                            radius: 8
                            color: Theme.surface
                            Column {
                                anchors.fill: parent
                                anchors.margins: 8
                                Text { text: modelData.k; color: Theme.muted; font.pixelSize: 10 }
                                Text { text: String(modelData.v); color: Theme.text; font.pixelSize: 13; elide: Text.ElideRight; width: parent.width }
                            }
                        }
                    }
                }
                Text { width: parent.width; wrapMode: Text.WordWrap; color: Theme.muted; text: page.card().evidence || "Kanıt metni yok." }
                Text { width: parent.width; wrapMode: Text.WordWrap; color: Theme.cyan; text: "Son hata: " + (page.card().lastError || "—") }
                Text { text: "Canlı performans"; color: Theme.text; font.bold: true }
                Row {
                    spacing: 6
                    Repeater {
                        model: [
                            { t: "Son 5 dakika", m: 5 },
                            { t: "Son 15 dakika", m: 15 },
                            { t: "Son 1 saat", m: 60 },
                            { t: "Son 24 saat", m: 1440 }
                        ]
                        Button {
                            required property var modelData
                            text: modelData.t
                            checked: page.minutes === modelData.m
                            onClicked: page.minutes = modelData.m
                        }
                    }
                }
                Sparkline {
                    width: parent.width
                    height: 90
                    visible: (Ops.latencySeries(page.moduleId, page.minutes) || []).length > 0
                    points: Ops.latencySeries(page.moduleId, page.minutes)
                }
                Text {
                    visible: (Ops.latencySeries(page.moduleId, page.minutes) || []).length === 0
                    text: "Bu metrik için ölçüm bulunmuyor"
                    color: Theme.amber
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    text: "İstek sayısı: " + (page.card().rpm === "—" ? "ölçüm yok" : page.card().rpm)
                          + " · Hata oranı: " + (page.card().errorRate || "—")
                          + " · Erişilebilirlik: " + (page.card().availability || "—")
                          + ". CPU, RAM, kuyruk derinliği ve aktif bağlantı bu modülün yanıtında yoksa gösterilmez."
                }
                Text { text: "API kontrolü"; color: Theme.text; font.bold: true }
                Text { width: parent.width; wrapMode: Text.WordWrap; color: Theme.text; text: (page.probe().method || "—") + " " + (page.probe().endpoint || "—") }
                Text { width: parent.width; wrapMode: Text.WordWrap; color: Theme.muted; text: (page.probe().note || "") + " Zaman aşımı " + (page.probe().timeoutMs || "—") + " ms. Aralık " + (page.probe().intervalMs || "—") + " ms." }
                Text { text: "Alt servisler"; color: Theme.text; font.bold: true }
                Repeater {
                    model: Ops.relatedModules(page.moduleId)
                    delegate: Text {
                        required property var modelData
                        width: body.width
                        wrapMode: Text.WordWrap
                        color: Theme.toneColor(modelData.tone || "gray")
                        text: modelData.name + " · " + (modelData.classLabel || modelData.statusLabel) + " · " + (modelData.evidence || "Ölçüm yok")
                    }
                }
                Text { visible: Ops.relatedModules(page.moduleId).length === 0; text: "Bu modülün ayrı alt servis ölçümü yok."; color: Theme.muted }
                Text { text: "Modül logları"; color: Theme.text; font.bold: true }
                Text { width: parent.width; wrapMode: Text.WordWrap; color: Theme.amber; text: Ops.moduleLogNote(page.moduleId) }
                Row {
                    spacing: 6
                    ComboBox {
                        model: ["Tümü", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"]
                        onActivated: page.level = currentIndex === 0 ? "" : currentText
                    }
                    TextField { placeholderText: "Metin, hata kodu veya istek"; onEditingFinished: page.query = text }
                    Button { text: "Dışa aktar"; onClicked: page.actionNote = Ops.exportModuleLogs(page.moduleId, page.level, page.query) }
                }
                Repeater {
                    model: Ops.moduleLogs(page.moduleId, page.level, page.query)
                    delegate: Rectangle {
                        required property var modelData
                        width: body.width
                        radius: 8
                        color: Theme.surface
                        implicitHeight: logCol.implicitHeight + 12
                        Column {
                            id: logCol
                            width: parent.width - 16
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 6
                            Text { text: modelData.time + " · " + modelData.level; color: Theme.cyan; font.pixelSize: 11 }
                            Text { width: parent.width; wrapMode: Text.WordWrap; color: Theme.text; text: modelData.message }
                            Text { visible: page.openLogId === modelData.id; width: parent.width; wrapMode: Text.WordWrap; color: Theme.muted; text: (modelData.analysis || "") + "\n" + (modelData.stack || "") }
                        }
                        MouseArea { anchors.fill: parent; onClicked: page.openLogId = page.openLogId === modelData.id ? "" : modelData.id }
                    }
                }
                Text { text: "Hata ve olay analizi"; color: Theme.text; font.bold: true }
                Repeater {
                    model: Ops.incidents
                    delegate: Text {
                        required property var modelData
                        visible: modelData.moduleId === page.moduleId
                        width: body.width
                        wrapMode: Text.WordWrap
                        color: Theme.text
                        text: modelData.state + " · " + modelData.severity + " · tekrar " + modelData.repeatCount
                              + "\nİlk " + modelData.firstSeen + " · Son " + modelData.lastSeen
                              + "\n" + modelData.causeLabel + ": " + modelData.causeText
                              + "\nKontrol: " + modelData.recommendation
                    }
                }
                Text {
                    visible: {
                        const rows = Ops.incidents
                        for (let i = 0; i < rows.length; ++i) if (rows[i].moduleId === page.moduleId) return false
                        return true
                    }
                    text: page.card().problem === true
                          ? "Aktif durum kanıtı var. Ayrı olay kaydı eşiğe ulaşmadan açılmaz. Kök neden henüz doğrulanamadı."
                          : "Bu modül için kayıtlı hata bulunmuyor"
                    color: Theme.muted
                    width: parent.width
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
