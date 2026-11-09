import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Item {
    id: root
    property bool active: true
    property int rev: Ops.revision
    property var orbitModel: []
    property string selectedId: ""
    property int minutes: 60
    property bool playing: false
    property int frame: 0
    property real yaw: 0
    property real distance: 5.8
    property bool dragged: false
    property real flow: 0
    property real breath: 1
    property real alertPulse: 1
    signal openDetail(string moduleId)
    signal openLogs(string moduleId)
    signal openAlerts(string moduleId)

    property int measured: Ops.healthyCount + Ops.warningCount + Ops.criticalCount + Ops.disconnectedCount
    property string coreTone: {
        root.rev
        if (Ops.criticalCount > 0 || Ops.hataVar) return "red"
        if (Ops.warningCount > 0 || Ops.disconnectedCount > 0) return "amber"
        if (root.measured > 0 && Ops.healthyCount > 0) return "cyan"
        return "gray"
    }

    function nodeTone(moduleId) {
        root.rev
        const info = Ops.moduleMap(moduleId)
        const kind = info.healthClass || ""
        if (kind === "healthy") return "green"
        if (kind === "warning") return "amber"
        if (kind === "critical" || kind === "disconnected") return "red"
        return "gray"
    }
    function toneColor(tone) {
        if (tone === "green") return Theme.green
        if (tone === "amber") return Theme.amber
        if (tone === "red") return Theme.critical
        if (tone === "cyan") return "#0891b2"
        return Theme.gray
    }
    function lineColor(tone) {
        if (tone === "red") return "#fb7185"
        if (tone === "amber") return "#fbbf24"
        if (tone === "green") return "#2dd4bf"
        return "#67e8f9"
    }
    function coreFill() {
        if (root.coreTone === "red") return "#e11d48"
        if (root.coreTone === "amber") return "#d97706"
        if (root.coreTone === "gray") return "#94a3b8"
        return "#06b6d4"
    }
    function orbit() {
        root.rev
        const catalog = [
            { id: "tg-api", name: "Backend API", icon: "▣" },
            { id: "gochat-api", name: "Go-Chat API", icon: "◉" },
            { id: "tg-postgres", name: "PostgreSQL", icon: "▤" },
            { id: "tg-redis", name: "Redis", icon: "▦" },
            { id: "tg-workers", name: "Worker servisleri", icon: "⚙" },
            { id: "tg-iyzico", name: "İyzico ödeme", icon: "▢" },
            { id: "tg-netgsm", name: "NetGSM SMS", icon: "◎" },
            { id: "tg-ferry", name: "Feribot bileti satış", icon: "⌂" }
        ]
        const out = []
        for (let i = 0; i < catalog.length; ++i) {
            const spec = catalog[i]
            const info = Ops.moduleMap(spec.id)
            const known = info && info.moduleId
            out.push({
                moduleId: known ? info.moduleId : "",
                name: spec.name,
                icon: spec.icon,
                tone: known ? root.nodeTone(spec.id) : "gray",
                latency: known ? (info.latency || "Ölçüm alınamadı") : "Ölçüm yok",
                statusLabel: known ? (info.statusLabel || "") : "Ölçüm alınamadı"
            })
        }
        return out
    }
    function reloadOrbit() { root.orbitModel = root.orbit() }
    function slot(i, w, h) {
        const row = i % 4
        const left = i < 4
        const gap = Math.max(58, (h - 16) / 4)
        return Qt.point(left ? 4 : Math.max(190, w - 184), 2 + row * gap)
    }
    function frames() {
        root.rev
        if (!root.selectedId) return []
        return Ops.latencyTimeline(root.selectedId, root.minutes) || []
    }
    function currentFrame() {
        const rows = root.frames()
        if (rows.length === 0) return null
        return rows[Math.max(0, Math.min(root.frame, rows.length - 1))]
    }
    function choose(moduleId) {
        if (!moduleId || moduleId === "core") return
        root.selectedId = moduleId
        root.playing = false
        root.frame = 0
    }
    function selected() {
        root.rev
        if (!root.selectedId) return null
        const info = Ops.moduleMap(root.selectedId)
        return info && info.moduleId ? info : null
    }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.max(statsCol.implicitHeight, titleCapsule.height)
        Column {
            id: statsCol
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(240, parent.width * 0.28)
            spacing: 1
            Text { text: "Ölçülen " + root.measured; color: Theme.title; font.pixelSize: 13; font.weight: Font.Medium; width: parent.width; elide: Text.ElideRight }
            Text { text: "Uyarı " + Ops.warningCount; color: Theme.amber; font.pixelSize: 13; font.weight: Font.Medium; width: parent.width; elide: Text.ElideRight }
            Text { text: "Kritik " + Ops.criticalCount; color: Theme.critical; font.pixelSize: 13; font.weight: Font.Medium; width: parent.width; elide: Text.ElideRight }
            Text { text: root.selected() ? ("Son ölçüm " + (root.selected().lastCheck || "Ölçüm alınamadı")) : "Son ölçüm yok"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.Medium; width: parent.width; elide: Text.ElideRight }
            Text { text: root.coreTone === "gray" ? "Ölçüm alınamadı. Çekirdek sağlıklı gösterilmez." : "Çekirdek canlı sayaçtır. Parçacık istek hacmi değildir."; color: Theme.amber; font.pixelSize: 11; font.weight: Font.Medium; width: parent.width; wrapMode: Text.WordWrap }
        }
        Rectangle {
            id: titleCapsule
            anchors.verticalCenter: parent.verticalCenter
            x: Math.max(statsCol.width + 12, (parent.width - width) / 2)
            width: titleCol.implicitWidth + 28
            height: titleCol.implicitHeight + 12
            radius: 12
            color: Theme.card
            border.color: "#7dd3fc"
            border.width: 1
            Column {
                id: titleCol
                anchors.centerIn: parent
                spacing: 1
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "OPERASYON ÇEKİRDEĞİ"
                    color: Theme.title
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "SİSTEM İZLEME MOTORU"
                    color: Theme.subtitle
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
            }
        }
    }

    Item {
        id: stage
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.topMargin: 6
        anchors.bottom: controls.top
        anchors.bottomMargin: 4
        scale: Math.max(0.72, Math.min(1.28, 5.8 / root.distance))
        transformOrigin: Item.Center
        rotation: Math.max(-8, Math.min(8, root.yaw * 0.06))

        Rectangle {
            width: 460
            height: 460
            radius: 230
            anchors.centerIn: sphere
            color: root.coreTone === "gray" ? "#e2e8f0" : "#d5f6ff"
            opacity: root.coreTone === "red" ? 0.45 * root.alertPulse : 0.55
        }
        Rectangle {
            id: ringA
            width: 300
            height: 300
            radius: 150
            anchors.centerIn: sphere
            color: "transparent"
            border.color: root.coreTone === "gray" ? Theme.line : "#7dd3fc"
            border.width: 2
            opacity: 0.7
            RotationAnimation on rotation { from: 0; to: 360; duration: 42000; loops: Animation.Infinite; running: root.active && Ops.animationsEnabled && root.measured > 0 }
        }
        Rectangle {
            width: 250
            height: 78
            radius: 39
            anchors.horizontalCenter: sphere.horizontalCenter
            anchors.top: sphere.bottom
            anchors.topMargin: -6
            color: "transparent"
            border.color: "#bae6fd"
            border.width: 6
            rotation: -8
        }
        Rectangle {
            width: 190
            height: 28
            radius: 14
            anchors.horizontalCenter: sphere.horizontalCenter
            anchors.top: sphere.bottom
            anchors.topMargin: 18
            color: "#e0f2fe"
            border.color: "#38bdf8"
            border.width: 1
        }

        Canvas {
            id: links
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const cx = width / 2
                const cy = height * 0.46
                const nodes = root.orbitModel || []
                for (let i = 0; i < nodes.length; ++i) {
                    const p = root.slot(i, width, height)
                    const x = p.x + (i < 4 ? 168 : 8)
                    const y = p.y + 26
                    const tone = nodes[i].tone
                    ctx.beginPath()
                    ctx.moveTo(cx, cy)
                    const bend = i < 4 ? 28 : -28
                    ctx.quadraticCurveTo((cx + x) / 2, (cy + y) / 2 + bend, x, y)
                    ctx.strokeStyle = tone === "gray" ? "#cbd5e1" : root.lineColor(tone)
                    ctx.globalAlpha = tone === "gray" ? 0.45 : 0.9
                    ctx.lineWidth = root.selectedId && root.selectedId === nodes[i].moduleId ? 2.6 : 1.5
                    if (root.active && Ops.animationsEnabled && root.measured > 0 && tone !== "gray") {
                        ctx.setLineDash([4, 10])
                        ctx.lineDashOffset = -root.flow
                    } else {
                        ctx.setLineDash([])
                    }
                    ctx.stroke()
                }
                ctx.globalAlpha = 1
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        Rectangle {
            id: sphere
            width: 188 * root.breath
            height: width
            radius: width / 2
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -8
            color: root.coreFill()
            border.color: "#f0fdff"
            border.width: 3
            opacity: root.coreTone === "gray" ? 0.72 : (root.coreTone === "red" ? root.alertPulse : 1)
            Rectangle {
                width: parent.width * 0.42
                height: width
                radius: width / 2
                color: "#ffffff"
                opacity: 0.28
                anchors.left: parent.left
                anchors.leftMargin: parent.width * 0.12
                anchors.top: parent.top
                anchors.topMargin: parent.height * 0.1
            }
            Column {
                anchors.centerIn: parent
                width: parent.width * 0.72
                spacing: 3
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 2
                    Repeater {
                        model: 11
                        Rectangle {
                            required property int index
                            width: 3
                            height: 8 + ((index * 5 + Math.floor(root.flow / 6)) % 7) * (root.measured > 0 && Ops.animationsEnabled ? 1.4 : 0.6)
                            radius: 1
                            color: "#ecfeff"
                            opacity: 0.9
                        }
                    }
                }
            }
        }

        Repeater {
            model: root.orbitModel
            delegate: Rectangle {
                id: capsule
                required property var modelData
                required property int index
                width: 176
                height: 58
                radius: 16
                x: root.slot(index, stage.width, stage.height).x
                y: root.slot(index, stage.width, stage.height).y
                color: "#ffffff"
                border.width: root.selectedId === modelData.moduleId && modelData.moduleId.length > 0 ? 2 : 1
                border.color: root.selectedId === modelData.moduleId && modelData.moduleId.length > 0 ? "#0891b2" : "#d7e4ee"
                Row {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    Rectangle {
                        width: 34
                        height: 34
                        radius: 10
                        color: modelData.tone === "gray" ? Theme.surface : "#e0f7fb"
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            anchors.centerIn: parent
                            text: modelData.icon
                            color: root.toneColor(modelData.tone)
                            font.pixelSize: 16
                        }
                    }
                    Column {
                        width: 112
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text {
                            width: parent.width
                            text: modelData.name
                            color: Theme.title
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Row {
                            spacing: 6
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: root.toneColor(modelData.tone)
                                opacity: modelData.tone === "red" && Ops.animationsEnabled ? root.alertPulse : 1
                            }
                            Row {
                                spacing: 2
                                height: 14
                                anchors.verticalCenter: parent.verticalCenter
                                Repeater {
                                    model: 5
                                    Rectangle {
                                        required property int index
                                        width: 3
                                        radius: 1
                                        color: root.toneColor(modelData.tone)
                                        opacity: modelData.tone === "gray" ? 0.35 : 0.9
                                        property real bar: modelData.tone === "gray" || !Ops.animationsEnabled ? 4 : 4 + ((index * 3 + Math.floor(root.flow / 8)) % 5) * 2
                                        height: bar
                                        y: 14 - bar
                                    }
                                }
                            }
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: modelData.moduleId.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: root.choose(modelData.moduleId)
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            acceptedButtons: Qt.LeftButton
            property real startX: 0
            property real startYaw: 0
            onPressed: (mouse) => { root.dragged = false; startX = mouse.x; startYaw = root.yaw }
            onPositionChanged: (mouse) => {
                if (Math.abs(mouse.x - startX) > 6) {
                    root.dragged = true
                    root.yaw = Math.max(-40, Math.min(40, startYaw + (mouse.x - startX) * 0.15))
                }
            }
            onWheel: (wheel) => { root.distance = Math.max(3.6, Math.min(8.2, root.distance - wheel.angleDelta.y / 480)) }
        }

    }

    Column {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 4
        Flow {
            width: parent.width
            spacing: 6
            ComboBox {
                model: ["Canlı", "Son 5 dakika", "Son 15 dakika", "Son 1 saat", "Son 6 saat", "Son 24 saat"]
                currentIndex: 0
                onActivated: {
                    root.playing = false
                    root.frame = 0
                    if (currentIndex > 0) root.minutes = [60, 5, 15, 60, 360, 1440][currentIndex]
                }
            }
            Button { text: "Oynat"; enabled: root.frames().length > 1; onClicked: root.playing = true }
            Button { text: "Duraklat"; onClicked: root.playing = false }
            Button { text: "Geri sar"; onClicked: { root.playing = false; root.frame = 0 } }
            Button { text: "Canlıya dön"; onClicked: { root.playing = false; root.minutes = 60; root.frame = Math.max(0, root.frames().length - 1) } }
            Button { text: "Yakınlaştır"; onClicked: root.distance = Math.max(3.6, root.distance - 0.4) }
            Button { text: "Uzaklaştır"; onClicked: root.distance = Math.min(8.2, root.distance + 0.4) }
            Button { text: "Görünümü sıfırla"; onClicked: { root.yaw = 0; root.distance = 5.8 } }
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            color: root.frames().length > 1 ? Theme.cyan : Theme.amber
            font.pixelSize: 11
            text: {
                const rows = root.frames()
                const point = root.currentFrame()
                if (!root.selectedId) return "Zaman çizgisi için bir düğüm seçin. Geçmiş yoksa sahte kare üretilmez."
                if (rows.length < 2) return "Geçmiş ölçüm yok. Sahte zaman ekseni çizilmedi."
                return "Zaman " + (root.frame + 1) + "/" + rows.length + " · " + point.at + " · " + Math.round(point.latency) + " ms"
            }
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.text
            font.pixelSize: 11
            text: "1 Motor kontrolü sıraya alır.  2 HTTPS sağlık isteği gider.  3 Süre ölçülür.  4 Sözleşme doğrulanır.  5 Durum güncellenir.  6 Eşik aşılırsa olay açılır."
        }
        Text {
            visible: root.selected() !== null
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.title
            font.pixelSize: 12
            text: {
                const info = root.selected()
                if (!info) return ""
                const probe = Ops.moduleProbe(info.moduleId)
                const point = root.playing || root.frame > 0 ? root.currentFrame() : null
                return info.name + " · " + info.statusLabel + " · " + (point ? (Math.round(point.latency) + " ms oynatılıyor") : info.latency)
                    + " · " + (probe.endpoint || "Ölçüm kaynağı mevcut değil")
                    + " · " + (info.evidence || "")
            }
        }
        Flow {
            width: parent.width
            spacing: 6
            visible: root.selected() !== null
            Button { text: "Detay"; onClicked: root.openDetail(root.selectedId) }
            Button { text: "Modüle git"; onClicked: root.openDetail(root.selectedId) }
            Button { text: "Loglar"; onClicked: root.openLogs(root.selectedId) }
            Button { text: "Uyarılar"; onClicked: root.openAlerts(root.selectedId) }
        }
    }

    Timer {
        interval: 120
        repeat: true
        running: root.active && Ops.animationsEnabled && root.measured > 0
        onTriggered: root.flow = (root.flow + 2) % 40
    }
    SequentialAnimation on breath {
        running: root.active && Ops.animationsEnabled && root.coreTone !== "gray"
        loops: Animation.Infinite
        NumberAnimation { to: 1.035; duration: 1800; easing.type: Easing.InOutQuad }
        NumberAnimation { to: 1; duration: 1800; easing.type: Easing.InOutQuad }
    }
    SequentialAnimation on alertPulse {
        running: root.active && Ops.animationsEnabled && root.coreTone === "red"
        loops: Animation.Infinite
        NumberAnimation { to: 0.55; duration: 700; easing.type: Easing.InOutQuad }
        NumberAnimation { to: 1; duration: 700; easing.type: Easing.InOutQuad }
    }
    onFlowChanged: if (links.available) links.requestPaint()
    onOrbitModelChanged: if (links.available) links.requestPaint()
    onSelectedIdChanged: if (links.available) links.requestPaint()
    Connections {
        target: Ops
        function onChanged() { root.reloadOrbit() }
    }
    Component.onCompleted: root.reloadOrbit()
    Timer {
        interval: 700
        repeat: true
        running: root.active && root.playing && root.frames().length > 1
        onTriggered: root.frame = (root.frame + 1) % root.frames().length
    }
}
