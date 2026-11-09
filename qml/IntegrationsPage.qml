import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Flickable {
    id: page
    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property int rev: Ops.revision
    property string formMessage: ""
    property string selectedId: ""
    property string editId: ""
    property bool locked: false
    property var issues: ({})
    property string query: ""
    property string healthFilter: ""
    property string monitorFilter: ""
    property string typeFilter: ""
    readonly property var authCodes: ["none", "api_key", "bearer", "basic", "oauth2"]
    readonly property var authLabels: ["Kimlik doğrulamasız", "API Key", "Bearer Token", "Basic Authentication", "OAuth 2.0"]

    function draft() {
        return {
            id: editId,
            name: nameField.text,
            description: descField.text,
            category: appField.text,
            environment: "production",
            baseUrl: urlField.text,
            healthPath: pathField.text,
            method: methodBox.currentText,
            authType: page.authCodes[authBox.currentIndex] || "none",
            apiKeyHeader: headerField.text,
            oauthTokenUrl: oauthUrlField.text,
            oauthClientId: oauthIdField.text,
            timeoutMs: Number(timeoutField.text) * 1000,
            intervalMs: Number(intervalField.text) * 1000,
            expectedStatus: Number(statusField.text),
            latencyWarnMs: Number(latencyField.text),
            failThreshold: Number(failField.text),
            adapter: adapterBox.currentIndex === 1 ? "contract-1.0" : "http",
            allowPrivate: allowPrivate.checked,
            paused: !monitorBox.checked,
            headers: []
        }
    }
    function reloadIssues() {
        page.issues = Ops.integrationIssues(page.draft())
    }
    function issueText(key) {
        const value = page.issues ? page.issues[key] : ""
        return value ? String(value) : ""
    }
    function records() {
        page.rev
        return Ops.integrations || []
    }
    function counts() {
        const rows = page.records()
        const tally = { total: rows.length, active: 0, healthy: 0, warning: 0, connection: 0, unconfigured: 0 }
        for (let i = 0; i < rows.length; ++i) {
            if (!rows[i].paused) tally.active += 1
            if (rows[i].bucket === "healthy") tally.healthy += 1
            if (rows[i].bucket === "warning") tally.warning += 1
            if (rows[i].bucket === "connection") tally.connection += 1
            if (rows[i].bucket === "unconfigured") tally.unconfigured += 1
        }
        return tally
    }
    function visibleRows() {
        const needle = page.query.trim().toLowerCase()
        const rows = page.records()
        const out = []
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            if (needle.length > 0) {
                const blob = (row.name + " " + row.category + " " + row.baseUrl + " " + row.healthPath).toLowerCase()
                if (blob.indexOf(needle) < 0) continue
            }
            if (page.healthFilter.length > 0 && row.bucket !== page.healthFilter) continue
            if (page.monitorFilter === "active" && row.paused) continue
            if (page.monitorFilter === "paused" && !row.paused) continue
            if (page.typeFilter === "rest" && row.apiType !== "REST / HTTPS") continue
            out.push(row)
        }
        return out
    }
    function selected() {
        const rows = page.records()
        for (let i = 0; i < rows.length; ++i) if (rows[i].id === page.selectedId) return rows[i]
        return null
    }
    function relatedIncidents(moduleId) {
        const rows = Ops.incidents || []
        const out = []
        for (let i = 0; i < rows.length; ++i) if (rows[i].moduleId === moduleId && !rows[i].hidden) out.push(rows[i])
        return out.slice(0, 6)
    }
    function clearForm() {
        page.editId = ""
        page.locked = false
        page.issues = ({})
        appField.text = ""
        nameField.text = ""
        descField.text = ""
        urlField.text = "https://"
        pathField.text = "/health"
        methodBox.currentIndex = 0
        authBox.currentIndex = 0
        headerField.text = "X-Api-Key"
        oauthUrlField.text = ""
        oauthIdField.text = ""
        timeoutField.text = "8"
        intervalField.text = "60"
        statusField.text = "200"
        latencyField.text = "1500"
        failField.text = "3"
        adapterBox.currentIndex = 0
        allowPrivate.checked = false
        monitorBox.checked = true
        secretField.text = ""
    }
    function loadForm(item) {
        page.editId = item.id
        page.locked = item.locked === true
        page.issues = ({})
        appField.text = item.category || ""
        nameField.text = item.name || ""
        descField.text = item.description || ""
        urlField.text = item.baseUrl || "https://"
        pathField.text = item.healthPath || "/health"
        methodBox.currentIndex = item.method === "HEAD" ? 1 : 0
        const authIndex = page.authCodes.indexOf(item.authType)
        authBox.currentIndex = authIndex < 0 ? 0 : authIndex
        headerField.text = item.apiKeyHeader || "X-Api-Key"
        oauthUrlField.text = item.oauthTokenUrl || ""
        oauthIdField.text = item.oauthClientId || ""
        timeoutField.text = String(Math.round((item.timeoutMs || 8000) / 1000))
        intervalField.text = String(Math.round((item.intervalMs || 60000) / 1000))
        statusField.text = String(item.expectedStatus || 200)
        latencyField.text = String(item.latencyWarnMs || 1500)
        failField.text = String(item.failThreshold || 3)
        adapterBox.currentIndex = item.adapter === "contract-1.0" ? 1 : 0
        allowPrivate.checked = item.allowPrivate === true
        monitorBox.checked = item.paused !== true
        secretField.text = ""
        formDialog.open()
    }
    function saveForm() {
        page.reloadIssues()
        if (page.issues.summary) {
            page.formMessage = page.issues.summary
            return
        }
        const message = Ops.saveIntegration(page.draft())
        if (message.indexOf("id:") === 0) {
            page.editId = message.substring(3)
            page.formMessage = "Kaydedildi. Sağlık, ilk başarılı ölçümden önce sağlıklı sayılmaz."
            if (secretField.text.length > 0) {
                const secretMessage = Ops.setCredential(page.editId, secretField.text)
                secretField.text = ""
                if (secretMessage && secretMessage.length > 0) page.formMessage = secretMessage
            }
            formDialog.close()
        } else page.formMessage = message
    }

    ColumnLayout {
        id: body
        width: page.width
        spacing: 10
        Label { text: "API VE ENTEGRASYON YÖNETİM MERKEZİ"; color: Theme.title; font.pixelSize: 22; font.bold: true }
        Label { text: "Ticket-Go, Go-Chat ve bağlı uygulamaların API bağlantıları, testleri ve izleme ayarları"; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { visible: page.formMessage.length > 0; text: page.formMessage; color: Theme.cyan; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "Sağlık kontrolü HTTPS GET veya HEAD ile yapılır. ICMP ve TCP yalnızca api.ticket-go.net için ayrı ölçülür ve API sağlığının yerine geçmez."; color: Theme.amber; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        GridLayout {
            columns: 6
            columnSpacing: 8
            rowSpacing: 8
            Layout.fillWidth: true
            Repeater {
                model: [
                    { title: "Toplam Kayıtlı API", value: page.counts().total, color: Theme.cyan },
                    { title: "İzleme Aktif", value: page.counts().active, color: Theme.cyan },
                    { title: "Sağlıklı API", value: page.counts().healthy, color: Theme.green },
                    { title: "Uyarı Veren API", value: page.counts().warning, color: Theme.amber },
                    { title: "Bağlantı Hatası", value: page.counts().connection, color: Theme.red },
                    { title: "Yapılandırılmamış API", value: page.counts().unconfigured, color: Theme.gray }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 72
                    radius: 10
                    color: Theme.card
                    border.color: Theme.line
                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        Label { text: modelData.title; color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width }
                        Label { text: String(modelData.value); color: modelData.color; font.pixelSize: 20; font.bold: true }
                    }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            Button { text: "+ YENİ API EKLE"; highlighted: true; onClicked: { page.clearForm(); formDialog.open() } }
            Button {
                text: "Listeyi yenile"
                onClicked: {
                    page.rev = Ops.revision
                    if (page.selectedId.length > 0) page.formMessage = Ops.probeNow(page.selectedId)
                    else page.formMessage = "Liste yerel kayıtlardan yenilendi. Toplu istek gönderilmedi."
                }
            }
            TextField { id: searchField; placeholderText: "Ara"; onTextChanged: page.query = text; implicitWidth: 160 }
            ComboBox {
                id: healthFilterBox
                model: ["Sağlık: tümü", "Sağlıklı", "Uyarı", "Bağlantı hatası", "Yapılandırılmamış", "Devre dışı", "Kritik", "Kimlik reddedildi"]
                onActivated: page.healthFilter = ["", "healthy", "warning", "connection", "unconfigured", "disabled", "critical", "auth"][currentIndex]
            }
            ComboBox {
                id: monitorFilterBox
                model: ["İzleme: tümü", "Aktif", "Pasif"]
                onActivated: page.monitorFilter = ["", "active", "paused"][currentIndex]
            }
            ComboBox {
                id: typeFilterBox
                model: ["Tür: tümü", "REST / HTTPS"]
                onActivated: page.typeFilter = ["", "rest"][currentIndex]
            }
            Button {
                text: "Filtreleri temizle"
                onClicked: {
                    searchField.text = ""
                    page.query = ""
                    page.healthFilter = ""
                    page.monitorFilter = ""
                    page.typeFilter = ""
                    healthFilterBox.currentIndex = 0
                    monitorFilterBox.currentIndex = 0
                    typeFilterBox.currentIndex = 0
                    page.formMessage = "Filtreler temizlendi."
                }
            }
        }

        Repeater {
            model: page.visibleRows()
            delegate: Rectangle {
                required property var modelData
                width: page.width
                implicitHeight: cardColumn.implicitHeight + 16
                radius: 12
                color: Theme.card
                border.width: 1
                border.color: Theme.toneColor(modelData.tone)
                Column {
                    id: cardColumn
                    width: parent.width - 20
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 3
                    Label { text: modelData.category + " · " + modelData.name; color: Theme.text; font.bold: true; wrapMode: Text.WordWrap; width: parent.width }
                    Label { text: modelData.baseUrl + modelData.healthPath; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; width: parent.width }
                    Label { text: modelData.apiType + " · " + modelData.authLabel + " · " + (modelData.paused ? "İzleme kapalı" : "İzleme açık"); color: Theme.cyan; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width }
                    Label { text: modelData.statusLabel + " · " + modelData.latency + " · HTTP " + modelData.httpCode; color: Theme.toneColor(modelData.tone); font.pixelSize: 12; wrapMode: Text.WordWrap; width: parent.width }
                    Label { text: "Son ölçüm " + modelData.lastCheck + " · Son başarı " + modelData.lastSuccess + " · Son hata " + modelData.lastFailure; color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width }
                    Flow {
                        width: parent.width
                        spacing: 6
                        Button { text: "DETAY"; onClicked: { page.selectedId = modelData.id; detailDialog.open() } }
                        Button { text: "TEST ET"; onClicked: { page.selectedId = modelData.id; page.formMessage = Ops.probeNow(modelData.id) } }
                        Button { text: "DÜZENLE"; onClicked: page.loadForm(modelData) }
                        Button {
                            text: modelData.paused ? "İZLEMEYİ AÇ" : "İZLEMEYİ DURDUR"
                            onClicked: {
                                Ops.setPaused(modelData.id, !modelData.paused)
                                page.formMessage = modelData.paused ? "İzleme açıldı. Yeni ölçüm bekleniyor. API kapatılmadı." : "İzleme durduruldu. Geçmiş duruyor. API kapatılmadı."
                            }
                        }
                        Button {
                            visible: modelData.locked !== true
                            text: "SİL"
                            palette.button: Theme.criticalSurface
                            palette.buttonText: Theme.critical
                            onClicked: window.askDelete("integration", modelData.id, modelData.name, "Base URL: " + modelData.baseUrl + modelData.healthPath + "\nSilme kapsamı: yalnızca bu bilgisayardaki kullanıcı entegrasyonu.\nModül listesindeki bu kart kalkar. Production API, uzak veritabanı ve geçmiş olay kaynağı silinmez.")
                        }
                        Button {
                            visible: modelData.locked === true
                            text: "Yerleşik kayıt korunur"
                            onClicked: page.formMessage = "Ticket-Go ve Go-Chat yerleşik kayıtları silinmez. İzleme durdurulabilir."
                        }
                    }
                }
            }
        }
        Label { visible: page.visibleRows().length === 0; text: "Bu filtrede kayıt yok."; color: Theme.muted }
        Item { Layout.preferredHeight: 12 }
    }

    Dialog {
        id: formDialog
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
        title: page.editId.length > 0 ? "Entegrasyonu düzenle" : "Yeni API"
        padding: 16
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape
        contentItem: Flickable {
            implicitHeight: Math.min(520, formColumn.implicitHeight)
            contentHeight: formColumn.implicitHeight
            clip: true
            Column {
                id: formColumn
                width: 520
                spacing: 6
                Label { text: page.formMessage; color: Theme.amber; wrapMode: Text.WordWrap; width: parent.width; visible: page.formMessage.length > 0 }
                Label { text: "Kayıt, bağlantının başarılı olmasını beklemez. Test ayrı sonuçtur."; color: Theme.muted; wrapMode: Text.WordWrap; width: parent.width }
                TextField { id: appField; width: parent.width; enabled: !page.locked; placeholderText: "Uygulama adı" }
                Label { visible: page.issueText("application").length > 0; text: page.issueText("application"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                TextField { id: nameField; width: parent.width; enabled: !page.locked; placeholderText: "Entegrasyon adı" }
                Label { visible: page.issueText("name").length > 0; text: page.issueText("name"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                TextField { id: descField; width: parent.width; enabled: !page.locked; placeholderText: "Açıklama" }
                TextField { id: urlField; width: parent.width; enabled: !page.locked; placeholderText: "https://alan.adi"; text: "https://" }
                Label { visible: page.issueText("baseUrl").length > 0; text: page.issueText("baseUrl"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                TextField { id: pathField; width: parent.width; enabled: !page.locked; placeholderText: "/health veya aynı hostun tam URL'si"; text: "/health" }
                Label { visible: page.issueText("healthPath").length > 0; text: page.issueText("healthPath"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                Label { visible: page.issueText("duplicate").length > 0; text: page.issueText("duplicate"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                ComboBox { id: methodBox; width: parent.width; enabled: !page.locked; model: ["GET", "HEAD"] }
                ComboBox { id: adapterBox; width: parent.width; enabled: !page.locked; model: ["HTTP durum", "Sözleşme 1.0"] }
                Label { width: parent.width; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 11; text: "HTTP 200, beklenen durumla ve seçilen sözleşmeyle birlikte değerlendirilir. Sözleşme seçilmeden gövde sağlıklı kabul edilmez." }
                ComboBox { id: authBox; width: parent.width; enabled: !page.locked; model: page.authLabels }
                TextField { id: headerField; width: parent.width; enabled: !page.locked; visible: page.authCodes[authBox.currentIndex] === "api_key"; text: "X-Api-Key"; placeholderText: "API anahtarı başlığı" }
                Label { visible: page.issueText("apiKeyHeader").length > 0; text: page.issueText("apiKeyHeader"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                TextField { id: oauthUrlField; width: parent.width; enabled: !page.locked; visible: page.authCodes[authBox.currentIndex] === "oauth2"; placeholderText: "OAuth token URL" }
                TextField { id: oauthIdField; width: parent.width; enabled: !page.locked; visible: page.authCodes[authBox.currentIndex] === "oauth2"; placeholderText: "OAuth istemci kimliği" }
                Label { visible: page.issueText("oauthTokenUrl").length + page.issueText("oauthClientId").length > 0; text: page.issueText("oauthTokenUrl") || page.issueText("oauthClientId"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                TextField { id: secretField; width: parent.width; enabled: !page.locked; echoMode: TextInput.Password; placeholderText: page.authCodes[authBox.currentIndex] === "basic" ? "kullanıcı:parola" : "Gizli değer, anahtar zincirine yazılır" }
                Label { text: "Gizli değer bu forma geri yazılmaz, SQLite'a ve loga geçmez."; color: Theme.muted; font.pixelSize: 11; width: parent.width; wrapMode: Text.WordWrap }
                TextField { id: intervalField; width: parent.width; text: "60"; placeholderText: "Kontrol sıklığı, saniye" }
                Label { visible: page.issueText("intervalMs").length > 0; text: page.issueText("intervalMs"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                TextField { id: timeoutField; width: parent.width; text: "8"; placeholderText: "Timeout, saniye" }
                Label { visible: page.issueText("timeoutMs").length > 0; text: page.issueText("timeoutMs"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                TextField { id: latencyField; width: parent.width; text: "1500"; placeholderText: "Kritik gecikme eşiği, ms" }
                TextField { id: statusField; width: parent.width; enabled: !page.locked; text: "200"; placeholderText: "Beklenen HTTP durumu" }
                TextField { id: failField; width: parent.width; text: "3"; placeholderText: "Kaç hatalı ölçümden sonra uyarı" }
                Label { visible: page.issueText("failThreshold").length > 0; text: page.issueText("failThreshold"); color: Theme.red; width: parent.width; wrapMode: Text.WordWrap }
                CheckBox { id: monitorBox; text: "İzleme aktif"; checked: true }
                CheckBox { id: allowPrivate; enabled: !page.locked; text: "İzin listesi dışındaki hosta açık izin ver" }
                Label { width: parent.width; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 11; text: "Metadata adresleri her durumda engellenir. TLS doğrulaması kapanmaz." }
            }
        }
        footer: Flow {
            spacing: 8
            width: parent.width
            Button { text: "Bağlantıyı test et"; onClicked: { page.reloadIssues(); if (page.issues.summary) page.formMessage = page.issues.summary; else { Ops.testDraft(page.draft(), secretField.text); page.formMessage = "Test gönderildi. Kayıt yazılmadı." } } }
            Button { text: "Kaydet"; highlighted: true; onClicked: page.saveForm() }
            Button { text: "İptal"; onClicked: formDialog.close() }
            Button {
                text: "Gizli değeri kaldır"
                enabled: page.editId.length > 0 && !page.locked
                onClicked: { Ops.clearCredential(page.editId); secretField.text = ""; page.formMessage = "Anahtar zinciri kaydı kaldırıldı. API kaydı duruyor." }
            }
        }
    }

    Dialog {
        id: detailDialog
        modal: true
        parent: Overlay.overlay
        palette.window: Theme.card
        palette.windowText: Theme.title
        palette.text: Theme.text
        palette.button: Theme.surface
        palette.buttonText: Theme.text
        background: Rectangle { color: Theme.card; border.color: Theme.line; border.width: 1; radius: 12 }
        anchors.centerIn: parent
        width: 620
        title: "Entegrasyon detayı"
        padding: 16
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: page.selectedId = page.selectedId
        contentItem: Flickable {
            implicitHeight: 460
            contentHeight: detailColumn.implicitHeight
            clip: true
            Column {
                id: detailColumn
                width: 580
                spacing: 4
                Label { text: page.selected() ? page.selected().id : ""; color: Theme.muted; font.pixelSize: 11; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? page.selected().category + " · " + page.selected().name : "Kayıt yok"; color: Theme.text; font.bold: true; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? page.selected().baseUrl + page.selected().healthPath : ""; color: Theme.cyan; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? page.selected().apiType + " · " + page.selected().authLabel + " · " + (page.selected().credential ? "Kimlik anahtar zincirinde" : "Kimlik yok") : ""; color: Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? page.selected().statusLabel + " · " + (page.selected().paused ? "İzleme kapalı" : "İzleme açık") : ""; color: page.selected() ? Theme.toneColor(page.selected().tone) : Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? "Yanıt " + page.selected().latency + " · HTTP " + page.selected().httpCode : ""; color: Theme.text; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? "Son ölçüm " + page.selected().lastCheck : ""; color: Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? "Son başarılı kontrol " + page.selected().lastSuccess : ""; color: Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? "Son başarısız kontrol " + page.selected().lastFailure : ""; color: Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() && page.selected().lastFailureText ? page.selected().lastFailureText : "Son hata açıklaması yok"; color: Theme.amber; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? "Aralık " + Math.round(page.selected().intervalMs / 1000) + " sn · timeout " + Math.round(page.selected().timeoutMs / 1000) + " sn · uyarı eşiği " + page.selected().failThreshold + " ölçüm" : ""; color: Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? page.selected().evidence : ""; color: Theme.text; width: parent.width; wrapMode: Text.WordWrap }
                Label { text: page.selected() ? Ops.latencyStats(page.selected().moduleId, 60).count + " ölçüm · ortalama " + Ops.latencyStats(page.selected().moduleId, 60).avg + " · son " + Ops.latencyStats(page.selected().moduleId, 60).last : ""; color: Theme.cyan; width: parent.width; wrapMode: Text.WordWrap }
                Repeater {
                    model: page.selected() ? page.relatedIncidents(page.selected().moduleId) : []
                    delegate: Label {
                        required property var modelData
                        width: 580
                        wrapMode: Text.WordWrap
                        color: Theme.amber
                        text: modelData.level + " · " + modelData.flow + " · " + modelData.message
                    }
                }
                Label { visible: page.selected() && page.relatedIncidents(page.selected().moduleId).length === 0; text: "Bu entegrasyon için açık olay yok."; color: Theme.muted; width: parent.width; wrapMode: Text.WordWrap }
            }
        }
        footer: Flow {
            spacing: 8
            width: parent.width
            Button { text: "Test Et"; enabled: page.selected() !== null; onClicked: page.formMessage = Ops.probeNow(page.selected().id) }
            Button { text: "Düzenle"; enabled: page.selected() !== null; onClicked: { const item = page.selected(); detailDialog.close(); if (item) page.loadForm(item) } }
            Button { text: "Performansı Aç"; enabled: page.selected() !== null; onClicked: { const id = page.selected().moduleId; detailDialog.close(); window.openPerformance(id) } }
            Button { text: "Uyarıları Gör"; enabled: page.selected() !== null; onClicked: { const id = page.selected().moduleId; detailDialog.close(); window.openAlerts(id) } }
            Button { text: "Olay Geçmişi"; enabled: page.selected() !== null; onClicked: { const name = page.selected().name; detailDialog.close(); window.openHistory(name) } }
            Button { text: "Raporlar"; enabled: page.selected() !== null; onClicked: { const id = page.selected().moduleId; detailDialog.close(); window.openReports(id) } }
            Button { text: "Kapat"; onClicked: detailDialog.close() }
        }
    }

    Connections {
        target: Ops
        function onChanged() { page.rev = Ops.revision }
        function onTestFinished(summary) { page.formMessage = summary; if (secretField.text.length > 0) secretField.text = "" }
    }
}
