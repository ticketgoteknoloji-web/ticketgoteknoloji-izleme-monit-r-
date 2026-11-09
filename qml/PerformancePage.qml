import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick3D
import TicketGo.Ops

Flickable {
    id: page
    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property bool sceneActive: true
    property int rev: Ops.revision
    property bool playing: false
    property int frame: 0
    property string moduleId: ""
    property string chartId: "tg-api"
    property int minutes: 60
    property real yaw: 0
    property real cameraZ: 6.2
    property bool sceneLarge: false
    property string message: ""
    property var nodes: [
        { id: "tg-api", name: "Backend API" },
        { id: "gochat-api", name: "Go-Chat API" },
        { id: "tg-postgres", name: "PostgreSQL" },
        { id: "tg-redis", name: "Redis" },
        { id: "tg-workers", name: "Worker" },
        { id: "tg-iyzico", name: "İyzico" },
        { id: "tg-netgsm", name: "NetGSM" }
    ]
    function focusModule(moduleId) {
        page.moduleId = moduleId || ""
        page.chartId = moduleId || "tg-api"
        page.minutes = 60
        page.message = "Seçilen API ölçümleri gösteriliyor. Ping, TCP, CPU ve disk bu API sonucundan türetilmez."
    }
    function tone(moduleId) {
        page.rev
        const info = Ops.moduleMap(moduleId)
        const kind = info.healthClass || ""
        if (kind === "healthy") return "green"
        if (kind === "warning") return "amber"
        if (kind === "critical") return "red"
        return "gray"
    }
    function stats() {
        page.rev
        return page.moduleId.length > 0 ? Ops.latencyStats(page.moduleId, page.minutes) : ({ count: 0, min: "Ölçüm alınamadı", max: "Ölçüm alınamadı", avg: "Ölçüm alınamadı", last: "Ölçüm alınamadı", p95: "yeterli ham veri yok", lastValue: -1 })
    }
    function series() {
        page.rev
        return page.chartId.length > 0 ? Ops.latencySeries(page.chartId, page.minutes) : []
    }
    function visibleSeries() {
        const points = page.series()
        if (!page.playing || points.length < 2) return points
        return points.slice(0, Math.max(2, page.frame + 1))
    }
    function station(value) {
        return value === "ölçülüyor" || value === "—" ? "Ölçüm alınamadı" : value
    }
    function glow(moduleId) {
        const kind = page.tone(moduleId)
        if (kind === "green") return Qt.vector3d(0.01, 0.1, 0.05)
        if (kind === "amber") return Qt.vector3d(0.14, 0.08, 0.01)
        if (kind === "red") return Qt.vector3d(0.16, 0.02, 0.03)
        return Qt.vector3d(0.04, 0.05, 0.06)
    }
    function barScale(slot) {
        const points = page.visibleSeries()
        if (points.length < 2) return 0.02
        let peak = 1
        for (let i = 0; i < points.length; ++i) peak = Math.max(peak, points[i])
        const index = Math.round(slot * (points.length - 1) / 7)
        return Math.max(0.04, points[index] / peak)
    }
    function barColor(slot) {
        const points = page.visibleSeries()
        if (points.length < 2) return Theme.gray
        const index = Math.round(slot * (points.length - 1) / 7)
        const warn = Ops.prefInt("latency_warn_ms", 1500)
        if (points[index] >= warn) return Theme.red
        if (points[index] >= warn / 2) return Theme.amber
        return Theme.green
    }
    function incidentCount() {
        page.rev
        let count = 0
        const rows = Ops.incidents || []
        for (let i = 0; i < rows.length; ++i) if (rows[i].moduleId === page.moduleId && !rows[i].hidden) count += 1
        return page.moduleId.length > 0 ? count : 0
    }

    ColumnLayout {
        id: body
        width: page.width
        spacing: 10
        Label { text: "SİSTEM PERFORMANSI"; color: Theme.title; font.pixelSize: 22; font.bold: true }
        Label { text: "Gerçek Zamanlı Sistem Performansı ve Kaynak Analiz Merkezi"; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "HTTPS süresi sağlık kontrolünün toplam süresidir. Ping ve TCP ayrı ölçülür. ICMP yanıtı servisi düşürmez."; color: Theme.amber; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { visible: page.message.length > 0; text: page.message; color: Theme.cyan; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            ComboBox {
                model: ["Tüm sistemler", "Backend API", "Go-Chat API", "PostgreSQL", "Redis", "Worker", "İyzico", "NetGSM"]
                onActivated: {
                    page.moduleId = ["", "tg-api", "gochat-api", "tg-postgres", "tg-redis", "tg-workers", "tg-iyzico", "tg-netgsm"][currentIndex]
                    if (page.moduleId.length > 0) page.chartId = page.moduleId
                }
            }
            ComboBox {
                model: ["Son 5 dakika", "Son 15 dakika", "Son 1 saat", "Son 6 saat", "Son 24 saat", "Son 7 gün"]
                currentIndex: 2
                onActivated: page.minutes = [5, 15, 60, 360, 1440, 10080][currentIndex]
            }
            Button { text: "Yenile"; onClicked: { page.rev = Ops.revision; page.message = "Grafik mevcut ölçümlerden yenilendi." } }
            Button { text: "Görünümü sıfırla"; onClicked: { page.yaw = 0; page.cameraZ = 6.2; page.sceneLarge = false } }
            Button { text: "Grafiği büyüt"; onClicked: page.sceneLarge = !page.sceneLarge }
            Button { text: "Detay"; onClicked: window.selectedId = page.moduleId }
            Button { text: "Uyarılar"; onClicked: window.pageIndex = 5 }
            Button { text: "Rapor"; onClicked: { reportsPage.focusPerformance(page.moduleId, page.minutes); window.pageIndex = 9 } }
            MarkImage { visible: page.moduleId === "gochat-api"; source: Ops.goChatLogo; markSize: 36 }
        }

        GridLayout {
            columns: 4
            columnSpacing: 8
            rowSpacing: 8
            Layout.fillWidth: true
            Repeater {
                model: [
                    { title: "Backend API gecikmesi", value: Ops.performanceMetric("tg-api", page.minutes).value, source: "GET /health", note: Ops.performanceMetric("tg-api", page.minutes).lastAt, chart: "tg-api", measure: "https-backend" },
                    { title: "Go-Chat API gecikmesi", value: Ops.performanceMetric("gochat-api", page.minutes).value, source: "GET /api/v1/gochat/health", note: Ops.performanceMetric("gochat-api", page.minutes).lastAt, chart: "gochat-api", measure: "https-gochat" },
                    { title: "Sunucu ping süresi", value: Ops.performanceMetric("path:icmp:api.ticket-go.net", page.minutes).value, source: "ICMP api.ticket-go.net", note: "HTTPS ile aynı metrik değil", chart: "path:icmp:api.ticket-go.net", measure: "path" },
                    { title: "TCP bağlantı süresi", value: Ops.performanceMetric("path:tcp:api.ticket-go.net:443", page.minutes).value, source: "TCP 443", note: "Bağlantı API sağlığı değildir", chart: "path:tcp:api.ticket-go.net:443", measure: "path" },
                    { title: "API başarı oranı", value: Ops.performanceMetric(page.chartId.indexOf("path:") === 0 ? "tg-api" : (page.chartId || "tg-api"), page.minutes).successRate, source: "Sağlık kontrolü", note: "Uyarı başarısızlık sayılmaz", chart: "", measure: "" },
                    { title: "API hata oranı", value: Ops.performanceMetric(page.chartId.indexOf("path:") === 0 ? "tg-api" : (page.chartId || "tg-api"), page.minutes).errorRate, source: "Sağlık kontrolü", note: "Kritik ve bağlantı kopması", chart: "", measure: "" },
                    { title: "CPU", value: page.station(Ops.stationCpu), source: "Bu bilgisayar", note: "Uzak sunucu değil", chart: "", measure: "" },
                    { title: "RAM", value: page.station(Ops.stationMem), source: "Bu bilgisayar", note: "Uzak sunucu değil", chart: "", measure: "" },
                    { title: "Disk", value: page.station(Ops.stationDisk), source: "Bu bilgisayar", note: "Uzak sunucu değil", chart: "", measure: "" },
                    { title: "Worker kuyruğu", value: Ops.queueText === "Bağlantı yapılandırılmadı" ? "Ölçüm kaynağı yapılandırılmamış" : Ops.queueText, source: "tg-queues", note: "Ping ile doldurulmaz", chart: "", measure: "" },
                    { title: "Veritabanı gecikmesi", value: "Ölçüm kaynağı yapılandırılmamış", source: "PostgreSQL", note: "Ayrı sorgu süresi yok", chart: "", measure: "" },
                    { title: "Ağ trafiği", value: "Ölçüm kaynağı yapılandırılmamış", source: "yok", note: "Ping ile doldurulmaz", chart: "", measure: "" }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: modelData.measure.length > 0 ? 118 : 92
                    radius: 8
                    color: Theme.card
                    border.color: page.chartId === modelData.chart && modelData.chart.length > 0 ? Theme.cyan : Theme.line
                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2
                        Label { text: modelData.title; color: Theme.muted; font.pixelSize: 11 }
                        Label { text: modelData.value; color: modelData.value === "Ölçüm alınamadı" || modelData.value.indexOf("yapılandırılmamış") >= 0 || modelData.value.indexOf("alınamadı") >= 0 ? Theme.amber : Theme.cyan; font.pixelSize: 15; font.bold: true; elide: Text.ElideRight; width: parent.width }
                        Label { text: modelData.source + " · " + modelData.note; color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight; width: parent.width }
                        Button {
                            visible: modelData.measure.length > 0
                            text: "Şimdi ölç"
                            onClicked: {
                                if (modelData.chart.length > 0) page.chartId = modelData.chart
                                page.message = Ops.measurePerformance(modelData.measure)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: page.sceneLarge ? 460 : 300
            radius: 12
            color: Theme.chart
            border.color: Theme.line
            clip: true
            View3D {
                anchors.fill: parent
                visible: page.sceneActive
                environment: SceneEnvironment {
                    backgroundMode: SceneEnvironment.Color
                    clearColor: Theme.chart
                    antialiasingMode: Ops.graphicsQuality === "low" ? SceneEnvironment.NoAA : SceneEnvironment.MSAA
                }
                PerspectiveCamera { position: Qt.vector3d(0, 1.8, page.cameraZ); eulerRotation.x: -16 }
                DirectionalLight { eulerRotation.x: -40; brightness: 0.85 }
                Node {
                    eulerRotation.y: page.yaw
                    Model {
                        source: "#Sphere"
                        scale: Qt.vector3d(0.42, 0.42, 0.42)
                        materials: PrincipledMaterial { baseColor: Theme.cyan; metalness: 0.55; roughness: 0.32; emissiveFactor: Qt.vector3d(0.02, 0.06, 0.16) }
                    }
                    Node {
                        id: ring
                        NumberAnimation on eulerRotation.y { running: page.sceneActive && Ops.animationsEnabled; from: 0; to: 360; duration: 36000; loops: Animation.Infinite }
                        Node { eulerRotation.y: 0; Model { source: "#Sphere"; position: Qt.vector3d(1.55, 0.05, 0); scale: Qt.vector3d(0.16, 0.16, 0.16); materials: PrincipledMaterial { baseColor: Theme.toneColor(page.tone("tg-api")); emissiveFactor: page.glow("tg-api"); metalness: 0.45; roughness: 0.3 } } }
                        Node { eulerRotation.y: 51; Model { source: "#Sphere"; position: Qt.vector3d(1.55, 0.05, 0); scale: Qt.vector3d(0.16, 0.16, 0.16); materials: PrincipledMaterial { baseColor: Theme.toneColor(page.tone("gochat-api")); emissiveFactor: page.glow("gochat-api"); metalness: 0.45; roughness: 0.3 } } }
                        Node { eulerRotation.y: 102; Model { source: "#Sphere"; position: Qt.vector3d(1.55, 0.05, 0); scale: Qt.vector3d(0.16, 0.16, 0.16); materials: PrincipledMaterial { baseColor: Theme.toneColor(page.tone("tg-postgres")); emissiveFactor: page.glow("tg-postgres"); metalness: 0.45; roughness: 0.3 } } }
                        Node { eulerRotation.y: 153; Model { source: "#Sphere"; position: Qt.vector3d(1.55, 0.05, 0); scale: Qt.vector3d(0.16, 0.16, 0.16); materials: PrincipledMaterial { baseColor: Theme.toneColor(page.tone("tg-redis")); emissiveFactor: page.glow("tg-redis"); metalness: 0.45; roughness: 0.3 } } }
                        Node { eulerRotation.y: 204; Model { source: "#Sphere"; position: Qt.vector3d(1.55, 0.05, 0); scale: Qt.vector3d(0.16, 0.16, 0.16); materials: PrincipledMaterial { baseColor: Theme.toneColor(page.tone("tg-workers")); emissiveFactor: page.glow("tg-workers"); metalness: 0.45; roughness: 0.3 } } }
                        Node { eulerRotation.y: 255; Model { source: "#Sphere"; position: Qt.vector3d(1.55, 0.05, 0); scale: Qt.vector3d(0.16, 0.16, 0.16); materials: PrincipledMaterial { baseColor: Theme.toneColor(page.tone("tg-iyzico")); emissiveFactor: page.glow("tg-iyzico"); metalness: 0.45; roughness: 0.3 } } }
                        Node { eulerRotation.y: 306; Model { source: "#Sphere"; position: Qt.vector3d(1.55, 0.05, 0); scale: Qt.vector3d(0.16, 0.16, 0.16); materials: PrincipledMaterial { baseColor: Theme.toneColor(page.tone("tg-netgsm")); emissiveFactor: page.glow("tg-netgsm"); metalness: 0.45; roughness: 0.3 } } }
                    }
                    Node {
                        visible: page.visibleSeries().length > 1
                        Model { source: "#Cube"; x: -1.4; y: page.barScale(0) * 0.35 - 0.85; scale: Qt.vector3d(0.1, page.barScale(0) * 0.7, 0.1); materials: PrincipledMaterial { baseColor: page.barColor(0); metalness: 0.2; roughness: 0.45 } }
                        Model { source: "#Cube"; x: -1.0; y: page.barScale(1) * 0.35 - 0.85; scale: Qt.vector3d(0.1, page.barScale(1) * 0.7, 0.1); materials: PrincipledMaterial { baseColor: page.barColor(1); metalness: 0.2; roughness: 0.45 } }
                        Model { source: "#Cube"; x: -0.6; y: page.barScale(2) * 0.35 - 0.85; scale: Qt.vector3d(0.1, page.barScale(2) * 0.7, 0.1); materials: PrincipledMaterial { baseColor: page.barColor(2); metalness: 0.2; roughness: 0.45 } }
                        Model { source: "#Cube"; x: -0.2; y: page.barScale(3) * 0.35 - 0.85; scale: Qt.vector3d(0.1, page.barScale(3) * 0.7, 0.1); materials: PrincipledMaterial { baseColor: page.barColor(3); metalness: 0.2; roughness: 0.45 } }
                        Model { source: "#Cube"; x: 0.2; y: page.barScale(4) * 0.35 - 0.85; scale: Qt.vector3d(0.1, page.barScale(4) * 0.7, 0.1); materials: PrincipledMaterial { baseColor: page.barColor(4); metalness: 0.2; roughness: 0.45 } }
                        Model { source: "#Cube"; x: 0.6; y: page.barScale(5) * 0.35 - 0.85; scale: Qt.vector3d(0.1, page.barScale(5) * 0.7, 0.1); materials: PrincipledMaterial { baseColor: page.barColor(5); metalness: 0.2; roughness: 0.45 } }
                        Model { source: "#Cube"; x: 1.0; y: page.barScale(6) * 0.35 - 0.85; scale: Qt.vector3d(0.1, page.barScale(6) * 0.7, 0.1); materials: PrincipledMaterial { baseColor: page.barColor(6); metalness: 0.2; roughness: 0.45 } }
                        Model { source: "#Cube"; x: 1.4; y: page.barScale(7) * 0.35 - 0.85; scale: Qt.vector3d(0.1, page.barScale(7) * 0.7, 0.1); materials: PrincipledMaterial { baseColor: page.barColor(7); metalness: 0.2; roughness: 0.45 } }
                    }
                }
            }
            HoverHandler { id: sceneHover }
            Label {
                visible: sceneHover.hovered
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 10
                text: {
                    const metric = Ops.performanceMetric(page.chartId, page.minutes)
                    return page.chartId + " · " + metric.value + " · HTTP " + metric.http + " · " + metric.lastAt
                }
                color: Theme.text
                font.pixelSize: 12
            }
            Row {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 8
                spacing: 6
                Button { text: "Yaklaş"; onClicked: page.cameraZ = Math.max(4.6, page.cameraZ - 0.4) }
                Button { text: "Uzaklaş"; onClicked: page.cameraZ = Math.min(7.4, page.cameraZ + 0.4) }
                Button { text: "Sola"; onClicked: page.yaw = Math.max(-35, page.yaw - 8) }
                Button { text: "Sağa"; onClicked: page.yaw = Math.min(35, page.yaw + 8) }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: page.nodes
                delegate: Button {
                    required property var modelData
                    text: modelData.name
                    highlighted: page.moduleId === modelData.id
                    onClicked: { page.moduleId = modelData.id; page.message = modelData.name + " seçildi. Durum: " + page.tone(modelData.id) }
                }
            }
        }

        Label { text: "Grafik · ms · " + page.chartId + " · eşik " + Ops.prefInt("latency_warn_ms", 1500) + " ms"; color: Theme.muted }
        Sparkline { Layout.fillWidth: true; Layout.preferredHeight: 140; points: page.visibleSeries(); visible: page.visibleSeries().length > 1 }
        Label { visible: page.series().length < 2; text: "Bu metrik için ölçüm bulunmuyor. Sahte zaman ekseni yok."; color: Theme.amber }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Button { text: "Oynat"; enabled: page.series().length > 1; onClicked: page.playing = true }
            Button { text: "Duraklat"; onClicked: page.playing = false }
            Button { text: "Geri sar"; onClicked: { page.playing = false; page.frame = 0 } }
            Button { text: "Canlıya dön"; onClicked: { page.playing = false; page.frame = Math.max(0, page.series().length - 1) } }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: page.series().length > 1 ? Theme.cyan : Theme.muted
            text: page.series().length > 1 ? ("Zaman " + (page.frame + 1) + "/" + page.series().length + " · " + Math.round(page.series()[Math.max(0, Math.min(page.frame, page.series().length - 1))]) + " ms · CPU, RAM ve disk bu seriden türetilmez") : "Geçmiş ölçüm yok."
        }
        Timer {
            interval: 700
            repeat: true
            running: page.sceneActive && page.playing && page.series().length > 1
            onTriggered: page.frame = (page.frame + 1) % page.series().length
        }
        Label { text: "Ölçüm " + page.stats().count + " · ortalama " + page.stats().avg + " · en düşük " + page.stats().min + " · en yüksek " + page.stats().max + " · P95 " + page.stats().p95; color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "Son ölçüm " + page.stats().lastAt + " · dönem içi değişim " + page.stats().delta + " · ilgili olay " + page.incidentCount(); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "3B sütunlar seçilen serinin 8 gerçek örneğidir. Yükseklik, serinin kendi en yüksek değerine göredir. Boş dönemde sütun çizilmez."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label {
            visible: page.stats().lastValue >= 0 && page.stats().lastValue > (Ops.prefInt ? Ops.prefInt("latency_warn_ms", 1500) : 1500)
            text: "Seçilen yanıt süresi kayıtlı uyarı eşiğinin üstünde. Yeni alarm açılmadı; Anlık Uyarılar mevcut olayları gösterir."
            color: Theme.amber
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Item { Layout.preferredHeight: 8 }
    }
}
