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

    property int intervalSec: 15
    property int timeoutSec: 8
    property int retryCap: 4
    property int latencyWarnMs: 1500
    property int retentionDays: 1
    property bool monitorEnabled: true
    property bool rememberPage: true
    property bool clock24h: true
    property bool sound: true
    property bool desktopNotify: true
    property bool quiet: false
    property bool animations: true
    property bool lowPower: false
    property bool startFullscreen: false
    property string quality: "high"
    property string savedStamp: ""
    property string message: ""
    property string pendingModule: ""
    property string pendingStamp: ""
    property string secretTarget: ""

    property string formStamp: [intervalSec, timeoutSec, retryCap, latencyWarnMs, retentionDays, monitorEnabled, rememberPage, clock24h, sound, desktopNotify, quiet, animations, lowPower, startFullscreen, quality].join("|")
    function stamp() { return formStamp }
    function draft() {
        return {
            intervalSec: page.intervalSec,
            timeoutSec: page.timeoutSec,
            retryCap: page.retryCap,
            latencyWarnMs: page.latencyWarnMs,
            retentionDays: page.retentionDays,
            monitorEnabled: page.monitorEnabled,
            rememberPage: page.rememberPage,
            clock24h: page.clock24h,
            sound: page.sound,
            desktopNotify: page.desktopNotify,
            quiet: page.quiet,
            animations: page.animations,
            lowPower: page.lowPower,
            startFullscreen: page.startFullscreen,
            quality: page.quality
        }
    }
    function applyMap(values) {
        page.intervalSec = Number(values.intervalSec)
        page.timeoutSec = Number(values.timeoutSec)
        page.retryCap = Number(values.retryCap)
        page.latencyWarnMs = Number(values.latencyWarnMs)
        page.retentionDays = Number(values.retentionDays)
        page.monitorEnabled = values.monitorEnabled === true
        page.rememberPage = values.rememberPage === true
        page.clock24h = values.clock24h === true
        page.sound = values.sound === true
        page.desktopNotify = values.desktopNotify === true
        page.quiet = values.quiet === true
        page.animations = values.animations === true
        page.lowPower = values.lowPower === true
        page.startFullscreen = values.startFullscreen === true
        page.quality = values.quality || "high"
        page.savedStamp = page.stamp()
        window.settingsDirty = false
    }
    function load() { page.applyMap(Ops.preferences()) }
    function save() {
        const result = Ops.savePreferences(page.draft())
        if (result && result.length > 0) {
            page.message = result
            return false
        }
        page.applyMap(Ops.preferences())
        page.message = "Kaydedildi. Yerleşik bağlayıcıların aralığı ve zaman aşımı güncellendi. Kimlik ve olay kayıtları duruyor."
        return true
    }
    function integration(id) {
        const rows = Ops.integrations
        for (let i = 0; i < rows.length; ++i) if (rows[i].id === id) return rows[i]
        return null
    }
    function moduleLine(moduleId) {
        const info = Ops.moduleMap(moduleId)
        if (!info || !info.name) return "Ölçüm yok"
        return (info.connection || "Veri yok") + " · HTTP " + (info.httpCode || "Veri yok") + " · " + (info.lastCheck || "Veri yok")
    }

    Component.onCompleted: page.load()
    onFormStampChanged: window.settingsDirty = page.formStamp !== page.savedStamp
    Connections {
        target: Ops
        function onChanged() {
            if (!page.pendingModule) return
            const info = Ops.moduleMap(page.pendingModule)
            if ((info.lastCheck || "") !== "" && info.lastCheck !== page.pendingStamp) {
                page.message = (info.name || page.pendingModule) + ": " + (info.connection || "Veri yok") + ", HTTP " + (info.httpCode || "Veri yok") + ". " + (info.evidence || "")
                page.pendingModule = ""
            }
        }
    }

    ColumnLayout {
        id: body
        width: page.width
        spacing: 12
        Label { text: "AYARLAR"; color: Theme.title; font.pixelSize: 22; font.bold: true }
        Label { text: "Sistem izleme, bağlantı, bildirim, güvenlik ve uygulama tercihleri"; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { visible: page.message.length > 0; text: page.message; color: Theme.cyan; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: Theme.card
            border.color: Theme.line
            implicitHeight: updates.implicitHeight + 24
            ColumnLayout {
                id: updates
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 6
                Label { text: "Uygulama Güncellemeleri"; color: Theme.text; font.bold: true }
                Label { text: "Kurulu Sürüm: " + Updates.currentVersion; color: Theme.text }
                Label { text: "Son Kontrol Zamanı: " + (Updates.lastCheckedText.length > 0 ? Updates.lastCheckedText : "Henüz kontrol edilmedi"); color: Theme.text }
                Label { text: "Güncelleme Kanalı: " + Updates.channel; color: Theme.text }
                Label { text: "Son Güncelleme Durumu: " + (Updates.lastStatusText.length > 0 ? Updates.lastStatusText : Updates.statusText); color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: Theme.card
            border.color: Theme.line
            implicitHeight: general.implicitHeight + 24
            ColumnLayout {
                id: general
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8
                Label { text: "Genel Ayarlar"; color: Theme.text; font.bold: true }
                Label { text: "Dil: Türkçe. Başka dil paketi yok."; color: Theme.muted }
                Label { text: "Tarih biçimi: gg.aa.yyyy. Başka biçim yok."; color: Theme.muted }
                CheckBox { text: "Son açık sayfayı hatırla"; checked: page.rememberPage; onToggled: page.rememberPage = checked }
                CheckBox { text: "24 saat biçimi"; checked: page.clock24h; onToggled: page.clock24h = checked }
                CheckBox { text: "Tam ekran başlat"; checked: page.startFullscreen; onToggled: page.startFullscreen = checked }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: Theme.card
            border.color: Theme.line
            implicitHeight: watch.implicitHeight + 24
            ColumnLayout {
                id: watch
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8
                Label { text: "İzleme Ayarları"; color: Theme.text; font.bold: true }
                Label { text: "Durdurma yalnızca bu masaüstü izlemeyi keser. Uzak servis kapanmaz. Alt sınır 15 saniye."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                CheckBox { text: "İzleme aktif"; checked: page.monitorEnabled; onToggled: page.monitorEnabled = checked }
                RowLayout {
                    Label { text: "Kontrol aralığı (sn)"; color: Theme.text }
                    SpinBox { from: 15; to: 300; value: page.intervalSec; onValueModified: page.intervalSec = value }
                }
                RowLayout {
                    Label { text: "API zaman aşımı (sn)"; color: Theme.text }
                    SpinBox { from: 2; to: 30; value: page.timeoutSec; onValueModified: page.timeoutSec = value }
                }
                RowLayout {
                    Label { text: "Taşıma hatası bekleme katı"; color: Theme.text }
                    SpinBox { from: 0; to: 4; value: page.retryCap; onValueModified: page.retryCap = value }
                }
                RowLayout {
                    Label { text: "Eklenen API gecikme uyarısı (ms)"; color: Theme.text }
                    SpinBox { from: 100; to: 60000; stepSize: 100; value: page.latencyWarnMs; onValueModified: page.latencyWarnMs = value }
                }
                Label { text: "Kritik olay eşiği sabittir: 2 ardışık gözlem. Uyarı eşiği sabittir: 3. Bu ekrandan değiştirilmez."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: Theme.card
            border.color: Theme.line
            implicitHeight: links.implicitHeight + 24
            ColumnLayout {
                id: links
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8
                Label { text: "API Bağlantıları"; color: Theme.text; font.bold: true }
                Repeater {
                    model: [
                        { id: "ticket-go-backend", moduleId: "tg-api", title: "Ticket-Go Backend" },
                        { id: "go-chat-api", moduleId: "gochat-api", title: "Go-Chat" }
                    ]
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 4
                        Label { text: modelData.title; color: Theme.cyan; font.bold: true }
                        Label { text: page.integration(modelData.id) ? ("Adres: " + page.integration(modelData.id).baseUrl) : "Bağlayıcı kaydı yok"; color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { text: page.integration(modelData.id) ? ("Health: " + page.integration(modelData.id).healthPath + " · zaman aşımı " + page.integration(modelData.id).timeoutMs + " ms") : ""; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { text: "Durum: " + page.moduleLine(modelData.moduleId); color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { text: "Kimlik: " + (Ops.credentialConfigured(modelData.id) ? "kayıtlı, gösterilmez" : "yok"); color: Theme.muted }
                        RowLayout {
                            Button {
                                text: "Bağlantıyı test et"
                                onClicked: {
                                    const item = page.integration(modelData.id)
                                    if (!item) { page.message = "Bağlayıcı kaydı yok."; return }
                                    const info = Ops.moduleMap(modelData.moduleId)
                                    page.pendingStamp = info.lastCheck || ""
                                    const queued = Ops.probeNow(modelData.id)
                                    page.message = queued
                                    page.pendingModule = queued === "Kontrol kuyruğa alındı." ? modelData.moduleId : ""
                                }
                            }
                            Button { text: "Bağlantıyı yenile"; onClicked: page.message = Ops.probeNow(modelData.id) }
                            Button { text: "Entegrasyona git"; onClicked: window.goPage(4) }
                            Button { text: "Kimliği kaydet"; onClicked: { page.secretTarget = modelData.id; secretField.text = ""; secretDialog.open() } }
                            Button { text: "Kimliği kaldır"; onClicked: { page.secretTarget = modelData.id; secretRemove.open() } }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: Theme.card
            border.color: Theme.line
            implicitHeight: notes.implicitHeight + 24
            ColumnLayout {
                id: notes
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8
                Label { text: "Bildirimler ve Alarmlar"; color: Theme.text; font.bold: true }
                CheckBox { text: "Masaüstü bildirimi"; checked: page.desktopNotify; onToggled: page.desktopNotify = checked }
                CheckBox { text: "Bildirim sesi"; checked: page.sound; onToggled: page.sound = checked }
                CheckBox { text: "Sessiz mod"; checked: page.quiet; onToggled: page.quiet = checked }
                Label { text: "Kritik olay bildirimi bu anahtarlardan geçer. Uyarı seviyesi için ayrı masaüstü bildirimi yok. Ses seviyesi macOS tarafından yönetilir."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Button {
                    text: "Bildirimi test et"
                    onClicked: {
                        const result = Ops.testNotification()
                        page.message = result && result.length > 0 ? result : "Bildirim isteği sistem tepsisine iletildi. macOS izni kapalıysa pencere görünmez."
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: Theme.card
            border.color: Theme.line
            implicitHeight: look.implicitHeight + 24
            ColumnLayout {
                id: look
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8
                Label { text: "Görünüm ve Performans"; color: Theme.text; font.bold: true }
                CheckBox { text: "3D animasyon"; checked: page.animations; onToggled: page.animations = checked }
                CheckBox { text: "Düşük güç (animasyon kapalı, istasyon ölçümü 30 sn)"; checked: page.lowPower; onToggled: page.lowPower = checked }
                Label { text: "3D kalite"; color: Theme.text }
                ComboBox {
                    model: ["Düşük", "Orta", "Yüksek"]
                    currentIndex: page.quality === "low" ? 0 : (page.quality === "medium" ? 1 : 2)
                    onActivated: page.quality = index === 0 ? "low" : (index === 1 ? "medium" : "high")
                }
                Label { text: "Arayüz ölçeği sistem çözünürlüğünü kullanır. Ayrı ölçek kaydı yok. Grafik verisi izleme örneklerinden gelir; ayrı yenileme hızı yok."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: Theme.card
            border.color: Theme.line
            implicitHeight: keep.implicitHeight + 24
            ColumnLayout {
                id: keep
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8
                Label { text: "Veri Saklama"; color: Theme.text; font.bold: true }
                Label { text: "Klasör: " + Ops.dataDirectory; color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: "Veritabanı: " + Ops.databaseStatus() + " · Disk: " + Ops.stationDisk; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: "Ölçüm saklama (gün)"; color: Theme.text }
                Slider { id: keepSlider; from: 1; to: 30; stepSize: 1; value: page.retentionDays; onMoved: page.retentionDays = Math.round(value) }
                Label { text: page.retentionDays + " gün. Olay geçmişi ve audit bu süreden silinmez. Yerel log satırları bellekte tutulur, ayrı süre kaydı yok."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    Button {
                        text: "Veri klasörünü aç"
                        onClicked: {
                            const result = Ops.openDataFolder()
                            page.message = result && result.length > 0 ? result : "Finder isteği kabul edildi."
                        }
                    }
                    Button { text: "Önbelleği temizle"; onClicked: cacheConfirm.open() }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: Theme.card
            border.color: Theme.line
            implicitHeight: secure.implicitHeight + 24
            ColumnLayout {
                id: secure
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8
                Label { text: "Güvenlik"; color: Theme.text; font.bold: true }
                Label { text: "Kimlik saklama: " + Ops.keychainNote; color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: "TLS sertifika doğrulaması açık. Kapatma kontrolü yok."; color: Theme.cyan; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: "Ticket-Go kimliği: " + (Ops.credentialConfigured("ticket-go-backend") ? "kayıtlı" : "yok") + " · Go-Chat kimliği: " + (Ops.credentialConfigured("go-chat-api") ? "kayıtlı" : "yok"); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: "Son Ticket-Go kontrolü: " + page.moduleLine("tg-api"); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 10
            color: Theme.card
            border.color: Theme.line
            implicitHeight: about.implicitHeight + 24
            ColumnLayout {
                id: about
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8
                Label { text: "Sistem Bilgileri"; color: Theme.text; font.bold: true }
                Label { text: Ops.systemInfoText(); color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: "İzleme motoru: " + (Ops.monitorEnabled ? "aktif" : "durduruldu"); color: Theme.cyan }
                RowLayout {
                    Button { text: "Veri klasörünü aç"; onClicked: { const result = Ops.openDataFolder(); page.message = result && result.length > 0 ? result : "Finder isteği kabul edildi." } }
                    Button {
                        text: "Bağlantı durumunu test et"
                        onClicked: {
                            const first = Ops.probeNow("ticket-go-backend")
                            const second = Ops.probeNow("go-chat-api")
                            page.message = "Ticket-Go: " + first + " Go-Chat: " + second
                        }
                    }
                    Button {
                        text: "Sistem bilgilerini kopyala"
                        onClicked: {
                            const result = Ops.copySystemInfo()
                            page.message = result && result.length > 0 ? result : "Sistem bilgisi kopyalandı. Anahtar ve jeton yok."
                        }
                    }
                }
            }
        }

        RowLayout {
            Button { text: "Kaydet"; highlighted: true; onClicked: page.save() }
            Button { text: "İptal"; onClicked: { page.load(); page.message = "Kaydedilmemiş değişiklikler geri alındı." } }
            Button {
                text: "Varsayılana dön"
                onClicked: {
                    const result = Ops.savePreferences(Ops.defaultPreferences())
                    if (result && result.length > 0) page.message = result
                    else {
                        page.load()
                        page.message = "Güvenli varsayılanlar yazıldı. API kimlikleri, entegrasyonlar ve olay geçmişi silinmedi."
                    }
                }
            }
        }
        Item { Layout.preferredHeight: 12 }
    }

    Dialog {
        id: cacheConfirm
        modal: true
        parent: Overlay.overlay
        palette.window: Theme.card
        palette.windowText: Theme.title
        palette.text: Theme.text
        palette.button: Theme.surface
        palette.buttonText: Theme.text
        background: Rectangle { color: Theme.card; border.color: Theme.line; border.width: 1; radius: 12 }
        anchors.centerIn: parent
        width: 460
        title: "Ölçüm önbelleği"
        padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Label { width: 420; wrapMode: Text.WordWrap; color: Theme.text; text: "Yalnızca saklama süresinden eski ölçüm örnekleri silinir. Backend logu, olay geçmişi, audit, ödeme ve entegrasyon kayıtları durur." }
        footer: Row {
            spacing: 8
            Button { text: "İptal"; onClicked: cacheConfirm.close() }
            Button { text: "Temizle"; onClicked: { page.message = Ops.clearSampleCache(); cacheConfirm.close() } }
        }
    }
    Dialog {
        id: secretDialog
        modal: true
        parent: Overlay.overlay
        palette.window: Theme.card
        palette.windowText: Theme.title
        palette.text: Theme.text
        palette.button: Theme.surface
        palette.buttonText: Theme.text
        background: Rectangle { color: Theme.card; border.color: Theme.line; border.width: 1; radius: 12 }
        anchors.centerIn: parent
        width: 460
        title: "Kimlik kaydı"
        padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Column {
            spacing: 8
            Label { width: 420; wrapMode: Text.WordWrap; color: Theme.muted; text: "Değer anahtar zincirine yazılır. Ekranda tekrar gösterilmez. Entegrasyon kaydı silinmez." }
            TextField { id: secretField; width: 420; echoMode: TextInput.Password; placeholderText: "Yeni gizli değer" }
        }
        footer: Row {
            spacing: 8
            Button { text: "İptal"; onClicked: { secretField.text = ""; secretDialog.close() } }
            Button {
                text: "Kaydet"
                highlighted: true
                onClicked: {
                    const result = Ops.setCredential(page.secretTarget, secretField.text)
                    secretField.text = ""
                    secretDialog.close()
                    page.message = result && result.length > 0 ? result : "Kimlik anahtar zincirine yazıldı."
                }
            }
        }
    }
    Dialog {
        id: secretRemove
        modal: true
        parent: Overlay.overlay
        palette.window: Theme.card
        palette.windowText: Theme.title
        palette.text: Theme.text
        palette.button: Theme.surface
        palette.buttonText: Theme.text
        background: Rectangle { color: Theme.card; border.color: Theme.line; border.width: 1; radius: 12 }
        anchors.centerIn: parent
        width: 460
        title: "Kimliği kaldır"
        padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Label { width: 420; wrapMode: Text.WordWrap; color: Theme.text; text: "Seçilen gizli değer anahtar zincirinden silinir. Yerleşik Ticket-Go ve Go-Chat bağlayıcı kaydı silinmez." }
        footer: Row {
            spacing: 8
            Button { text: "İptal"; onClicked: secretRemove.close() }
            Button {
                text: "Kaldır"
                onClicked: {
                    Ops.clearCredential(page.secretTarget)
                    secretRemove.close()
                    page.message = "Kimlik kaldırıldı. Bağlayıcı kaydı duruyor."
                }
            }
        }
    }
}
