import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Item {
    id: page
    property int rev: Ops.revision
    property string sourceId: "all"
    property string level: ""
    property string range: ""
    property string logType: ""
    property string outcome: ""
    property string exportNote: ""
    property bool followTail: true
    property var sections: []
    property bool ready: false

    function levelColor(level) {
        if (level === "DEBUG") return Theme.muted
        if (level === "INFO") return Theme.cyan
        if (level === "WARN") return Theme.amber
        if (level === "ERROR") return Theme.red
        if (level === "CRITICAL") return Theme.critical
        return Theme.muted
    }

    function showIncident(source, level, text, http, errorCode, service, requestId) {
        page.sourceId = source && source.length > 0 ? source : "all"
        page.level = level || ""
        page.range = "24h"
        rangeBox.currentIndex = 4
        searchField.text = text || ""
        httpField.text = http && http !== "0" ? http : ""
        errorField.text = errorCode || ""
        requestField.text = requestId || ""
        serviceField.text = service || ""
        page.exportNote = page.sourceId === "all" ? "Bu olay için ayrı log kaynağı yok. Sahte kayıt yok." : "Olay filtresi uygulandı. Eşleşme yoksa liste boş kalır."
        page.apply()
    }

    function apply() {
        if (!page.ready) return
        Ops.applyLogFilter(sourceId, level, range, searchField.text, httpField.text, errorField.text, requestField.text, serviceField.text, page.logType, fromField.text, toField.text, userField.text, ipField.text, endpointField.text, page.outcome, actionField.text)
    }
    function density() {
        page.rev
        if (!Ops.logHubConnected) return []
        const rows = Ops.logRows || []
        const buckets = {}
        for (let i = 0; i < rows.length; ++i) {
            const key = String(rows[i].clock || "").slice(0, 2)
            if (key.length < 2) continue
            buckets[key] = (buckets[key] || 0) + 1
        }
        const keys = Object.keys(buckets).sort()
        const points = []
        for (let i = 0; i < keys.length; ++i) points.push(buckets[keys[i]])
        return points
    }

    Component.onCompleted: { page.sections = Ops.logSections(); page.ready = true; Ops.refreshLogs() }

    LogLoginDialog { id: logLogin }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label { text: "Merkezi log analizi"; color: Theme.title; font.pixelSize: 22; font.bold: true }
            Item { Layout.fillWidth: true }
            Repeater {
                model: [
                    { name: "Toplam", value: Ops.logConnectedSources === 0 && Ops.logTotalCount === 0 ? "—" : Ops.logTotalCount },
                    { name: "INFO", value: Ops.logConnectedSources === 0 && Ops.logTotalCount === 0 ? "—" : Ops.logInfoCount },
                    { name: "WARNING", value: Ops.logConnectedSources === 0 && Ops.logTotalCount === 0 ? "—" : Ops.logWarnCount },
                    { name: "ERROR", value: Ops.logConnectedSources === 0 && Ops.logTotalCount === 0 ? "—" : Ops.logErrorCount },
                    { name: "CRITICAL", value: Ops.logConnectedSources === 0 && Ops.logTotalCount === 0 ? "—" : Ops.logCriticalCount },
                    { name: "Aktif kaynak", value: Ops.logConnectedSources }
                ]
                Rectangle {
                    required property var modelData
                    Layout.preferredWidth: 108
                    Layout.preferredHeight: 48
                    radius: 8
                    color: Theme.panel
                    border.color: Theme.line
                    Column {
                        anchors.centerIn: parent
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: parent.parent.modelData.name; color: Theme.muted; font.pixelSize: 10 }
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: String(parent.parent.modelData.value); color: Theme.text; font.pixelSize: 16; font.bold: true }
                    }
                }
            }
        }

        Label { text: "Backend Bağlantı ve Oturum Yönetimi"; color: Theme.text; font.bold: true }
        Label { text: Ops.logSessionPhase === "open" ? "Oturum aktif" : "OTURUM YOK"; color: Ops.logSessionPhase === "open" ? Theme.green : Theme.red; font.bold: true }
        Button {
            id: sessionLink
            visible: Ops.logSessionPhase !== "open"
            text: "Oturum yok. İç yönetim hesabıyla oturum açın."
            flat: true
            leftPadding: 8
            rightPadding: 8
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            Accessible.name: text
            Accessible.role: Accessible.Button
            onClicked: logLogin.begin()
            background: Rectangle {
                color: sessionLink.down ? "#E8EEF8" : (sessionLink.hovered ? "#F4F7FB" : "transparent")
                radius: 6
            }
            contentItem: Text {
                text: sessionLink.text
                color: (sessionLink.hovered || sessionLink.down) ? Theme.title : Theme.cyan
                font.underline: true
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                enabled: false
            }
            MouseArea {
                anchors.fill: parent
                z: 2
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: logLogin.begin()
            }
        }
        RowLayout {
            visible: Ops.logSessionPhase === "open"
            Layout.fillWidth: true
            Label { text: "Oturum aktif"; color: Theme.cyan }
            Label { text: "Kullanıcı: " + (Ops.logSessionAccount.length > 0 ? Ops.logSessionAccount : "doğrulandı"); color: Theme.text }
            Label { text: "Bağlantı: " + (Ops.logHubConnected ? "açık" : "yok"); color: Theme.text }
            Label { text: "Son doğrulama: " + (Ops.logSessionVerifiedAt.length > 0 ? Ops.logSessionVerifiedAt : "Veri yok"); color: Theme.text }
            Button { text: "OTURUMU KAPAT"; onClicked: Ops.closeLogSession() }
        }
        Label { text: Ops.logAccessNote; color: Theme.amber; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "Kaynak: Ticket-Go System Logs Hub · Bağlantı " + (Ops.logHubConnected ? "açık" : "yok") + " · Canlı takip " + (Ops.logLive ? "açık, " + Ops.logPollSeconds + " sn" : "kapalı") + " · Son senkron " + Ops.logRefreshedAt + " · Son kayıt " + Ops.logLastRecordAt + " · Toplam " + (Ops.logHubTotal < 0 ? "alınamadı" : Ops.logHubTotal); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { visible: Ops.logStale; text: "Görünen kayıtlar son başarılı okumadandır. Bağlantı kopukken yeni kayıt sayılmaz."; color: Theme.amber; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: page.exportNote; color: Theme.cyan; visible: page.exportNote.length > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                model: page.sections
                textRole: "label"
                onActivated: (index) => {
                    const section = page.sections[index]
                    if (!section) return
                    page.logType = section.code === "errors" ? "" : (section.logType || "")
                    if (section.code === "errors") page.level = "ERROR"
                    page.apply()
                }
            }
            ComboBox {
                id: rangeBox
                model: ["Tüm tarihler", "Son 5 dakika", "Son 15 dakika", "Son 1 saat", "Son 24 saat", "Son 7 gün"]
                currentIndex: 0
                onActivated: { page.range = ["", "5m", "15m", "1h", "24h", "7d"][currentIndex]; page.apply() }
            }
            ComboBox {
                model: ["Tüm seviyeler", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"]
                onActivated: { page.level = currentIndex === 0 ? "" : currentText; page.apply() }
            }
            TextField { id: searchField; placeholderText: "Metin"; Layout.fillWidth: true; onEditingFinished: page.apply() }
            TextField { id: httpField; placeholderText: "HTTP"; Layout.preferredWidth: 70; onEditingFinished: page.apply() }
            TextField { id: errorField; placeholderText: "Hata kodu"; Layout.preferredWidth: 100; onEditingFinished: page.apply() }
            TextField { id: requestField; placeholderText: "Request / correlation"; Layout.preferredWidth: 150; onEditingFinished: page.apply() }
            TextField { id: serviceField; placeholderText: "Servis"; Layout.preferredWidth: 90; onEditingFinished: page.apply() }
            Button { text: Ops.logLive ? "Canlı izlemeyi durdur" : "Canlı log takibi"; onClicked: Ops.setLogLive(!Ops.logLive) }
            Button { text: "Yenile"; onClicked: Ops.refreshLogs() }
            Button { text: "Yeniden dene"; onClicked: Ops.refreshLogs() }
            Button { text: "Dışa aktar"; onClicked: page.exportNote = Ops.exportVisibleLogs() }
            Button { text: "Yerel dosya"; onClicked: { Ops.pickLogFile(); Ops.refreshLogs() } }
            Button { text: "Bağlantıyı doğrula"; onClicked: page.exportNote = Ops.validateLogSession() }
            Button { text: "Oturumu yenile"; onClicked: page.exportNote = Ops.refreshLogSession() }
            TextField { id: fromField; placeholderText: "Başlangıç yyyy-MM-dd"; Layout.preferredWidth: 150; onEditingFinished: page.apply() }
            TextField { id: toField; placeholderText: "Bitiş yyyy-MM-dd"; Layout.preferredWidth: 140; onEditingFinished: page.apply() }
            TextField { id: userField; placeholderText: "Kullanıcı"; Layout.preferredWidth: 120; onEditingFinished: page.apply() }
            TextField { id: ipField; placeholderText: "IP"; Layout.preferredWidth: 110; onEditingFinished: page.apply() }
            TextField { id: endpointField; placeholderText: "/endpoint"; Layout.preferredWidth: 140; onEditingFinished: page.apply() }
            TextField { id: actionField; placeholderText: "İşlem"; Layout.preferredWidth: 100; onEditingFinished: page.apply() }
            ComboBox {
                model: ["Tüm sonuçlar", "Başarılı", "Hatalı", "Bekleyen"]
                onActivated: { page.outcome = ["", "success", "failed", "pending"][currentIndex]; page.apply() }
            }
            TextField { id: pollField; placeholderText: "Canlı sn"; text: String(Ops.logPollSeconds); Layout.preferredWidth: 70; onEditingFinished: Ops.setLogPollSeconds(Number(text) || 5) }
            CheckBox { text: "Otomatik kaydır"; checked: page.followTail; onToggled: page.followTail = checked }
            Label { text: "Yeni " + Ops.logNewCount; color: Theme.cyan }
            Button { text: "Yerel bağı kaldır"; onClicked: window.askDelete("logfile", "log_file", "Yerel log dosyası bağı", "Yalnız bu bilgisayardaki dosya seçimi kalkar. Dosya ve backend logları silinmez.") }
            Button { text: "Jetonu sil"; onClicked: window.askDelete("token", "system-logs", "logs.read jetonu", "Anahtar zincirindeki jeton silinir. Backend log kayıtları silinmez.") }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                color: Theme.panel
                radius: 10
                border.color: Theme.line
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Label { text: "Sistem log kaynakları"; color: Theme.text; font.bold: true }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Ops.logSources
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 112
                            color: page.sourceId === modelData.id ? Theme.selected : "transparent"
                            Column {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 2
                                Row {
                                    spacing: 6
                                    MarkImage {
                                        visible: modelData.id === "gochat" || modelData.id === "gochat-ws"
                                        source: Ops.goChatLogo
                                        markSize: 28
                                    }
                                    Label { text: modelData.name; color: Theme.text; font.pixelSize: 12; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                                }
                                Label { text: modelData.accessLabel; color: modelData.access === "connected" ? Theme.green : Theme.red; font.pixelSize: 11 }
                                Label { text: "Son: " + modelData.lastTime + "  Toplam: " + modelData.total; color: Theme.muted; font.pixelSize: 10 }
                                Label { text: "INFO " + modelData.infos + "  Uyarı " + modelData.warnings; color: Theme.muted; font.pixelSize: 10 }
                                Label { text: "ERROR " + modelData.errors + "  CRITICAL " + modelData.criticals; color: Theme.muted; font.pixelSize: 10 }
                            }
                            MouseArea { anchors.fill: parent; onClicked: { page.sourceId = modelData.id; page.apply() } }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.panel
                radius: 10
                border.color: Theme.line
                ListView {
                    id: logList
                    anchors.fill: parent
                    anchors.margins: 6
                    anchors.bottomMargin: 46
                    clip: true
                    model: Ops.logRows
                    onCountChanged: if (page.followTail && Ops.logLive) positionViewAtEnd()
                    spacing: 4
                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: 92
                        radius: 6
                        color: Ops.logDetail.id === modelData.id ? Theme.selected : Theme.card
                        border.color: Theme.line
                        Row {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 8
                            Rectangle { width: 8; height: parent.height; color: page.levelColor(modelData.level) }
                            Column {
                                width: parent.width - 168
                                spacing: 2
                                Label { text: (modelData.date || "Veri yok") + "  " + (modelData.clock || "Veri yok") + "  " + modelData.level; color: page.levelColor(modelData.level); font.pixelSize: 11 }
                                Label { text: (modelData.module || "Veri yok") + " · " + (modelData.service || "Veri yok") + " · " + (modelData.action || "Veri yok"); color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; width: parent.width }
                                Label { width: parent.width; text: modelData.message || "Veri yok"; color: Theme.text; font.pixelSize: 12; elide: Text.ElideRight }
                                Label { text: "Hata " + (modelData.errorCode || "Veri yok") + "  HTTP " + (modelData.httpCode || "Veri yok"); color: Theme.muted; font.pixelSize: 10 }
                            }
                            Column {
                                spacing: 4
                                Button { text: "Detay"; onClicked: Ops.selectLog(modelData.id) }
                                Button {
                                    text: "Sil"
                                    palette.button: Theme.criticalSurface
                                    palette.buttonText: Theme.critical
                                    onClicked: window.askDelete("logview", modelData.id, modelData.id, "Modül: " + (modelData.module || "Veri yok") + "\nTarih: " + (modelData.date || "Veri yok") + " " + (modelData.clock || "") + "\nSeviye: " + modelData.level + "\n\nBu kayıt yalnızca masaüstü izleme görünümünden kaldırılacaktır. Backend sistem logu silinmeyecektir.")
                                }
                            }
                        }
                    }
                }
                Row {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.margins: 8
                    spacing: 8
                    Button { text: "Önceki"; enabled: Ops.logPage() > 1; onClicked: Ops.setLogPage(Ops.logPage() - 1) }
                    Label { text: "Sayfa " + Ops.logPage(); color: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
                    Button { text: "Sonraki"; onClicked: Ops.setLogPage(Ops.logPage() + 1) }
                    Button { text: "Gizlenenleri geri yükle"; onClicked: Ops.restoreHiddenHubLogs() }
                }
            }

            Rectangle {
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                color: Theme.panel
                radius: 10
                border.color: Theme.line
                Flickable {
                    anchors.fill: parent
                    anchors.margins: 10
                    contentHeight: detail.implicitHeight
                    clip: true
                    Column {
                        id: detail
                        width: parent.width
                        spacing: 6
                        Label { text: "Seçilen kayıt"; color: Theme.text; font.bold: true }
                        Button { text: "Kopyala"; enabled: (Ops.logDetail.id || "").length > 0; onClicked: page.exportNote = Ops.copySelectedLog() }
                        Button { text: "İlgili olayı görüntüle"; visible: Ops.logDetail.level === "ERROR" || Ops.logDetail.level === "CRITICAL"; onClicked: window.pageIndex = 5 }
                        Label { width: parent.width; wrapMode: Text.WordWrap; color: Theme.text; text: Ops.logDetail.id ? ("Log ID: " + Ops.logDetail.id + "\nSistem: Ticket-Go\nModül: " + (Ops.logDetail.module || "Veri yok") + "\nTür: " + (Ops.logDetail.service || "Veri yok") + "\nTarih: " + (Ops.logDetail.date || "Veri yok") + " " + (Ops.logDetail.clock || "Veri yok") + "\nSeviye: " + Ops.logDetail.level + "\nİşlem: " + (Ops.logDetail.action || "Veri yok") + "\nYöntem: " + Ops.logDetail.method + "\nSonuç: " + Ops.logDetail.outcome + "\nSüre: " + Ops.logDetail.duration + "\nKullanıcı: " + Ops.logDetail.user + "\nIP: " + Ops.logDetail.ip + "\nHata kodu: " + (Ops.logDetail.errorCode || "Veri yok") + "\nHTTP: " + (Ops.logDetail.httpCode || "Veri yok") + "\nEndpoint: " + (Ops.logDetail.endpoint || "Veri yok") + "\nRequest ID: " + (Ops.logDetail.requestId || "Veri yok") + "\nCorrelation: " + (Ops.logDetail.correlationId || "Veri yok") + "\nAçıklama: " + (Ops.logDetail.message || "Veri yok") + "\n" + (Ops.logDetail.stack || "")) : "Kayıt seçilmedi." }
                        Sparkline { width: parent.width; height: 48; points: page.density(); visible: page.density().length > 1 }
                        Label { width: parent.width; wrapMode: Text.WordWrap; color: Theme.muted; text: Ops.logHubConnected ? (page.density().length > 1 ? "Yoğunluk, bu sayfadaki gerçek saat dağılımıdır." : "Bu sayfada grafik için yeterli gerçek kayıt yok.") : "Veri alınamadı. Grafik üretilmedi." }
                        Label { width: parent.width; wrapMode: Text.WordWrap; color: Theme.amber; text: Ops.logDetail.analysis || "" }
                        Label { width: parent.width; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 11; text: "Go-Chat mesaj içeriği, OTP, parola, kart ve jeton bu ekranda gösterilmez." }
                    }
                }
            }
        }
    }
}
