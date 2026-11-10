import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import TicketGo.Ops

ApplicationWindow {
    id: window
    width: 1440
    height: 900
    minimumWidth: 1180
    minimumHeight: 720
    visible: true
    title: "TICKET GO TEKNOLOJİ AŞ. — Sistem İzleme ve Takip Görüntüleme Sistemi"
    color: Theme.bg
    palette.window: Theme.bg
    palette.windowText: Theme.text
    palette.base: Theme.field
    palette.alternateBase: Theme.fieldHover
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.highlight: Theme.cyan
    palette.highlightedText: Theme.onAccent
    palette.placeholderText: Theme.muted
    palette.mid: Theme.fieldBorder
    palette.light: Theme.fieldHover
    palette.dark: Theme.fieldBorder
    palette.toolTipBase: Theme.card
    palette.toolTipText: Theme.text
    palette.disabled.buttonText: Theme.muted
    palette.disabled.text: Theme.muted
    palette.disabled.windowText: Theme.muted
    font.family: ".AppleSystemUIFont"
    font.weight: Font.Medium

    property int pageIndex: 0
    property bool settingsDirty: false
    property int pendingPage: -1
    function goPage(index) {
        if (window.pageIndex === 10 && index !== 10 && window.settingsDirty) {
            window.pendingPage = index
            unsaved.open()
            return
        }
        window.pageIndex = index
    }
    Component.onCompleted: {
        if (Ops.prefOn("start_fullscreen", false)) window.visibility = Window.FullScreen
        if (Ops.prefOn("remember_page", true)) {
            const saved = Ops.prefInt("last_page", 0)
            if (saved >= 0 && saved < window.pages.length) window.pageIndex = saved
        }
    }
    onPageIndexChanged: {
        if (Ops.prefOn("remember_page", true)) Ops.setLastPage(window.pageIndex)
        const y = window.pageIndex * 42
        if (y < navFlick.contentY) navFlick.contentY = y
        else if (y + 36 > navFlick.contentY + navFlick.height) navFlick.contentY = Math.max(0, y + 36 - navFlick.height)
    }
    property int rev: Ops.revision
    property string selectedId: ""
    property string deleteKind: ""
    property string deleteId: ""
    function openPerformance(moduleId) {
        performancePage.focusModule(moduleId)
        window.goPage(7)
    }
    function openAlerts(moduleId) {
        alertsPage.showModule(moduleId)
        window.goPage(5)
    }
    function openHistory(moduleName) {
        incidentsPage.showModule(moduleName)
        window.goPage(8)
    }
    function openReports(moduleId) {
        reportsPage.focusPerformance(moduleId, 60)
        window.goPage(9)
    }
    function askDelete(kind, id, name, detail) {
        window.deleteKind = kind
        window.deleteId = id
        confirm.recordName = name
        confirm.detail = detail
        confirm.open()
    }
    function selectedCard() { window.rev; return Ops.moduleMap(window.selectedId) }
    property var pages: [
        "Genel Bakış", "Modül İzleme", "Ticket-Go API", "Go-Chat API",
        "API Ekle ve Entegrasyonlar", "Anlık Uyarılar", "Log Analizi",
        "Sistem Performansı", "Olay Geçmişi", "Raporlar", "Ayarlar",
        "API Merkezi"
    ]

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.preferredWidth: 248
            Layout.fillHeight: true
            color: Theme.panel
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                MarkImage {
                    source: Ops.programLogo
                    markSize: 100
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredHeight: 100
                    Layout.maximumWidth: 200
                }
                Text {
                    text: "Ticketgo Teknoloji Gözlem Merkezi"
                    color: Theme.text
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                Flickable {
                    id: navFlick
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.VerticalFlick
                    contentWidth: width
                    contentHeight: navColumn.implicitHeight
                    Column {
                        id: navColumn
                        width: navFlick.width
                        spacing: 6
                        Repeater {
                            model: window.pages
                            delegate: Rectangle {
                                id: navItem
                                required property string modelData
                                required property int index
                                width: navColumn.width
                                height: Math.max(34, navLabel.implicitHeight + 12)
                                radius: 8
                                color: window.pageIndex === index ? Theme.selected : (navHover.containsMouse ? Theme.hover : "transparent")
                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    spacing: 8
                                    Canvas {
                                        id: apiNavIcon
                                        width: modelData === "API Merkezi" ? 16 : 0
                                        height: 16
                                        visible: modelData === "API Merkezi"
                                        onPaint: {
                                            const ctx = getContext("2d")
                                            ctx.clearRect(0, 0, width, height)
                                            ctx.strokeStyle = window.pageIndex === index ? Theme.title : Theme.cyan
                                            ctx.lineWidth = 1.4
                                            ctx.beginPath()
                                            ctx.moveTo(3, 8)
                                            ctx.lineTo(13, 3)
                                            ctx.moveTo(3, 8)
                                            ctx.lineTo(13, 13)
                                            ctx.stroke()
                                            ctx.fillStyle = ctx.strokeStyle
                                            ctx.beginPath(); ctx.arc(3, 8, 2.1, 0, Math.PI * 2); ctx.fill()
                                            ctx.beginPath(); ctx.arc(13, 3, 2.1, 0, Math.PI * 2); ctx.fill()
                                            ctx.beginPath(); ctx.arc(13, 13, 2.1, 0, Math.PI * 2); ctx.fill()
                                        }
                                        Connections {
                                            target: window
                                            function onPageIndexChanged() { apiNavIcon.requestPaint() }
                                        }
                                    }
                                    Text {
                                        id: navLabel
                                        width: parent.width - (modelData === "API Merkezi" ? 24 : 0)
                                        text: modelData
                                        color: window.pageIndex === index ? Theme.title : Theme.text
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        wrapMode: Text.WordWrap
                                    }
                                }
                                Accessible.role: Accessible.Button
                                Accessible.name: modelData
                                MouseArea {
                                    id: navHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: window.goPage(index)
                                }
                                Accessible.onPressAction: window.goPage(index)
                            }
                        }
                    }
                    ScrollBar.vertical: ScrollBar {
                        id: navBar
                        policy: navFlick.contentHeight > navFlick.height + 2 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                        contentItem: Rectangle {
                            implicitWidth: 4
                            radius: 2
                            color: Theme.cyan
                            opacity: navBar.pressed ? 1 : 0.45
                        }
                    }
                }
                Text { text: "v" + Updates.currentVersion; color: Theme.muted; font.pixelSize: 11 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 84
                color: Theme.card
                border.color: Theme.line
                border.width: 1
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    ColumnLayout {
                        spacing: 2
                        Text { text: "TICKET GO TEKNOLOJİ AŞ."; color: Theme.title; font.pixelSize: 22; font.bold: true }
                        Text { text: "Sistem İzleme ve Takip Görüntüleme Sistemi"; color: Theme.cyan; font.pixelSize: 13 }
                    }
                    Item { Layout.fillWidth: true }
                    Item {
                        id: updateCluster
                        readonly property bool updateOffer: Updates.phase === "UPDATE_AVAILABLE" || Updates.phase === "DOWNLOADING" || Updates.phase === "VERIFYING" || Updates.phase === "READY_TO_INSTALL" || Updates.phase === "INSTALLING"
                        readonly property string statusLine: {
                            if (Updates.phase === "ERROR") return Updates.errorMessage.length > 0 ? Updates.errorMessage : "Güncelleme denetlenemedi"
                            if (Updates.phase === "CHECKING") return "Denetleniyor..."
                            if (Updates.phase === "DOWNLOADING") return Updates.progressPercent >= 0 ? ("İndiriliyor... %" + Updates.progressPercent) : "İndiriliyor..."
                            if (Updates.phase === "VERIFYING") return "Doğrulanıyor..."
                            if (Updates.phase === "INSTALLING") return "Yükleniyor..."
                            if (updateOffer && Updates.latestVersion.length > 0) return "Yeni sürüm: v" + Updates.latestVersion + " (Mevcut: v" + Updates.currentVersion + ")"
                            if (Updates.phase === "UP_TO_DATE" || Updates.phase === "NO_RELEASE") return "Uygulamanız güncel"
                            if (Updates.phase === "UNSUPPORTED_PLATFORM") return "Bu Mac için paket yok"
                            return "Mevcut: v" + Updates.currentVersion
                        }
                        readonly property color statusColor: Updates.phase === "ERROR" ? "#B91C1C" : (Updates.phase === "UP_TO_DATE" || Updates.phase === "NO_RELEASE" ? "#047857" : (updateOffer ? "#1D4ED8" : "#64748B"))
                        Layout.preferredWidth: 400
                        Layout.minimumWidth: 300
                        Layout.maximumWidth: 430
                        Layout.preferredHeight: 50
                        Layout.alignment: Qt.AlignVCenter
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34
                                spacing: 8
                                Button {
                                    id: checkUpdateButton
                                    flat: true
                                    padding: 0
                                    hoverEnabled: true
                                    focusPolicy: Qt.StrongFocus
                                    enabled: !Updates.busy
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 164
                                    Layout.minimumWidth: 96
                                    Layout.preferredHeight: 34
                                    scale: checkUpdateButton.down ? 0.98 : (checkUpdateButton.hovered ? 1.02 : 1)
                                    transform: Translate { y: checkUpdateButton.hovered && !checkUpdateButton.down ? -1 : 0 }
                                    Behavior on scale { NumberAnimation { duration: 90 } }
                                    Accessible.name: "Güncelleme Denetle"
                                    Accessible.description: updateCluster.statusLine
                                    ToolTip.visible: hovered || activeFocus
                                    ToolTip.text: "GitHub Releases denetler"
                                    ToolTip.delay: 250
                                    onClicked: Updates.checkForUpdates()
                                    background: Item {
                                        Rectangle {
                                            anchors.fill: parent
                                            anchors.topMargin: 2
                                            radius: 14
                                            color: "#93C5FD"
                                            opacity: checkUpdateButton.hovered ? 0.7 : 0.4
                                        }
                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 14
                                            border.width: checkUpdateButton.activeFocus ? 2 : 1
                                            border.color: "#2563EB"
                                            gradient: Gradient {
                                                orientation: Gradient.Vertical
                                                GradientStop { position: 0.0; color: checkUpdateButton.hovered ? "#FFFFFF" : "#F8FBFF" }
                                                GradientStop { position: 1.0; color: checkUpdateButton.hovered ? "#DBEAFE" : "#E7F1FF" }
                                            }
                                        }
                                    }
                                    contentItem: Item {
                                    Row {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 6
                                        Item {
                                            width: 16
                                            height: 16
                                            anchors.verticalCenter: parent.verticalCenter
                                            property real spinAngle: 0
                                            rotation: Updates.iconSpins ? spinAngle : 0
                                            NumberAnimation on spinAngle {
                                                running: Updates.iconSpins
                                                from: 0
                                                to: 360
                                                duration: 900
                                                loops: Animation.Infinite
                                            }
                                            Canvas {
                                                id: refreshIcon
                                                anchors.fill: parent
                                                property color ink: checkUpdateButton.enabled ? "#1D4ED8" : "#94A3B8"
                                                onInkChanged: requestPaint()
                                                onVisibleChanged: requestPaint()
                                                Component.onCompleted: requestPaint()
                                                onPaint: {
                                                    const ctx = getContext("2d")
                                                    ctx.clearRect(0, 0, width, height)
                                                    ctx.strokeStyle = ink
                                                    ctx.lineWidth = 1.6
                                                    ctx.lineCap = "round"
                                                    ctx.lineJoin = "round"
                                                    ctx.beginPath()
                                                    ctx.arc(8, 8, 5, Math.PI * 0.9, Math.PI * 1.82)
                                                    ctx.stroke()
                                                    ctx.beginPath()
                                                    ctx.moveTo(3.4, 5.2)
                                                    ctx.lineTo(6.6, 3.2)
                                                    ctx.lineTo(6.8, 6.6)
                                                    ctx.stroke()
                                                    ctx.beginPath()
                                                    ctx.arc(8, 8, 5, Math.PI * 1.9, Math.PI * 0.82)
                                                    ctx.stroke()
                                                    ctx.beginPath()
                                                    ctx.moveTo(12.6, 10.8)
                                                    ctx.lineTo(9.4, 12.8)
                                                    ctx.lineTo(9.2, 9.4)
                                                    ctx.stroke()
                                                }
                                            }
                                        }
                                        Text {
                                            text: "Güncelleme Denetle"
                                            color: checkUpdateButton.enabled ? "#1E3A8A" : "#94A3B8"
                                            font.pixelSize: 12
                                            font.bold: true
                                            font.family: Qt.application.font.family
                                            anchors.verticalCenter: parent.verticalCenter
                                            elide: Text.ElideRight
                                            width: Math.max(40, parent.width - 22)
                                        }
                                    }
                                    }
                                }
                                Button {
                                    id: installUpdateButton
                                    readonly property string label: {
                                        if (Updates.phase === "DOWNLOADING" && Updates.progressPercent >= 0) return "İndiriliyor... %" + Updates.progressPercent
                                        if (Updates.phase === "DOWNLOADING") return "İndiriliyor..."
                                        if (Updates.phase === "VERIFYING") return "Doğrulanıyor..."
                                        if (Updates.phase === "INSTALLING") return "Yükleniyor..."
                                        if (Updates.phase === "READY_TO_INSTALL") return "Kuruluma hazır"
                                        return "Güncellemeyi İndir ve Yükle"
                                    }
                                    flat: true
                                    padding: 0
                                    hoverEnabled: true
                                    focusPolicy: Qt.StrongFocus
                                    enabled: updateCluster.updateOffer
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 220
                                    Layout.minimumWidth: 108
                                    Layout.preferredHeight: 34
                                    scale: installUpdateButton.down ? 0.98 : (installUpdateButton.hovered && installUpdateButton.enabled ? 1.02 : 1)
                                    transform: Translate { y: installUpdateButton.hovered && installUpdateButton.enabled && !installUpdateButton.down ? -1 : 0 }
                                    Behavior on scale { NumberAnimation { duration: 90 } }
                                    Accessible.name: label
                                    Accessible.description: updateCluster.statusLine
                                    ToolTip.visible: hovered || activeFocus
                                    ToolTip.text: updateCluster.updateOffer ? "Doğrulanan güncellemeyi indirir ve kurar" : "Güncelleme yok"
                                    ToolTip.delay: 250
                                    onClicked: {
                                        if (Updates.busy) return
                                        if (Updates.phase === "READY_TO_INSTALL") Updates.confirmInstall()
                                        else if (Updates.phase === "UPDATE_AVAILABLE") Updates.confirmDownload()
                                    }
                                    background: Item {
                                        Rectangle {
                                            anchors.fill: parent
                                            anchors.topMargin: 2
                                            radius: 14
                                            color: installUpdateButton.enabled ? "#6EE7B7" : "transparent"
                                            opacity: installUpdateButton.hovered ? 0.75 : 0.45
                                        }
                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 14
                                            border.width: installUpdateButton.activeFocus ? 2 : 1
                                            border.color: installUpdateButton.enabled ? "#047857" : "#D1D5DB"
                                            gradient: Gradient {
                                                orientation: Gradient.Horizontal
                                                GradientStop { position: 0.0; color: !installUpdateButton.enabled ? "#F3F4F6" : (installUpdateButton.down ? "#047857" : (installUpdateButton.hovered ? "#34D399" : "#10B981")) }
                                                GradientStop { position: 1.0; color: !installUpdateButton.enabled ? "#E5E7EB" : (installUpdateButton.down ? "#065F46" : (installUpdateButton.hovered ? "#10B981" : "#059669")) }
                                            }
                                        }
                                        Rectangle {
                                            visible: updateCluster.updateOffer
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: "#DC2626"
                                            border.color: "#FFFFFF"
                                            border.width: 1
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.rightMargin: 4
                                            anchors.topMargin: 4
                                        }
                                    }
                                    contentItem: Item {
                                    Row {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 6
                                        Canvas {
                                            width: 16
                                            height: 16
                                            anchors.verticalCenter: parent.verticalCenter
                                            property color ink: installUpdateButton.enabled ? "#FFFFFF" : "#9CA3AF"
                                            onInkChanged: requestPaint()
                                            Component.onCompleted: requestPaint()
                                            onPaint: {
                                                const ctx = getContext("2d")
                                                ctx.clearRect(0, 0, width, height)
                                                ctx.strokeStyle = ink
                                                ctx.lineWidth = 1.7
                                                ctx.lineCap = "round"
                                                ctx.lineJoin = "round"
                                                ctx.beginPath()
                                                ctx.moveTo(8, 2)
                                                ctx.lineTo(8, 9)
                                                ctx.stroke()
                                                ctx.beginPath()
                                                ctx.moveTo(4.4, 6.2)
                                                ctx.lineTo(8, 10.2)
                                                ctx.lineTo(11.6, 6.2)
                                                ctx.stroke()
                                                ctx.beginPath()
                                                ctx.moveTo(3, 13.2)
                                                ctx.lineTo(13, 13.2)
                                                ctx.stroke()
                                            }
                                        }
                                        Text {
                                            text: installUpdateButton.label
                                            color: installUpdateButton.enabled ? "#FFFFFF" : "#9CA3AF"
                                            font.pixelSize: 12
                                            font.bold: true
                                            font.family: Qt.application.font.family
                                            anchors.verticalCenter: parent.verticalCenter
                                            elide: Text.ElideRight
                                            width: Math.max(48, parent.width - 22)
                                        }
                                    }
                                    }
                                }
                            }
                            Text {
                                text: updateCluster.statusLine
                                color: updateCluster.statusColor
                                font.pixelSize: 11
                                font.family: Qt.application.font.family
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.preferredHeight: 14
                            }
                        }
                    }
                    Item {
                        id: alarmButton
                        property int rev: Ops.revision
                        property int criticalCount: {
                            alarmButton.rev
                            let count = 0
                            const rows = Ops.incidents || []
                            for (let i = 0; i < rows.length; ++i) {
                                if (rows[i].state !== "resolved" && rows[i].severity === "critical") count += 1
                            }
                            return count
                        }
                        property int warningCount: {
                            alarmButton.rev
                            let count = 0
                            const rows = Ops.incidents || []
                            for (let i = 0; i < rows.length; ++i) {
                                if (rows[i].state !== "resolved" && rows[i].severity === "warning") count += 1
                            }
                            return count
                        }
                        property int measured: {
                            alarmButton.rev
                            let count = 0
                            const rows = Ops.modules || []
                            for (let i = 0; i < rows.length; ++i) if (rows[i].lastCheck && rows[i].lastCheck !== "—") count += 1
                            return count
                        }
                        readonly property int shownCritical: Math.max(criticalCount, Ops.criticalCount + Ops.disconnectedCount)
                        readonly property int shownWarning: Math.max(warningCount, Ops.warningCount)
                        readonly property string mode: shownCritical > 0 ? "critical" : (shownWarning > 0 ? "warning" : (measured === 0 ? "unknown" : "normal"))
                        readonly property int badge: mode === "critical" ? shownCritical : (mode === "warning" ? shownWarning : 0)
                        property real pulse: 1
                        SequentialAnimation on pulse {
                            running: alarmButton.mode === "critical" && Ops.animationsEnabled
                            loops: Animation.Infinite
                            NumberAnimation { from: 1; to: 0.45; duration: 520 }
                            NumberAnimation { from: 0.45; to: 1; duration: 520 }
                        }
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 46
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: mode === "critical" ? "Kritik alarm" : (mode === "warning" ? "Uyarı" : (mode === "unknown" ? "Alarm durumu bilinmiyor" : "Aktif alarm yok"))
                        Accessible.description: badge + " olay. Anlık Uyarılar açılır. Olay onaylanmaz."
                        function openAlerts() {
                            alertsPage.showAlarm(alarmButton.mode === "critical" ? "critical" : (alarmButton.mode === "warning" ? "warning" : "active"))
                            window.goPage(5)
                        }
                        Keys.onReturnPressed: openAlerts()
                        Keys.onEnterPressed: openAlerts()
                        Keys.onSpacePressed: openAlerts()
                        HoverHandler { id: alarmHover }
                        TapHandler { onTapped: alarmButton.openAlerts() }
                        ToolTip.visible: alarmHover.hovered || alarmButton.activeFocus
                        ToolTip.text: Accessible.name + " · " + badge
                        Rectangle {
                            id: alarmGlow
                            anchors.fill: parent
                            radius: 10
                            border.width: parent.activeFocus ? 2 : 1
                            border.color: parent.activeFocus ? Theme.cyan : (parent.mode === "critical" ? Theme.critical : (parent.mode === "warning" ? Theme.amber : Theme.line))
                            color: parent.mode === "critical" ? Theme.criticalSurface : (parent.mode === "warning" ? Theme.warningSurface : (parent.mode === "unknown" ? Theme.unknownSurface : Theme.successSurface))
                            opacity: alarmButton.mode === "critical" && Ops.animationsEnabled ? alarmButton.pulse : 1
                        }
                        Row {
                            anchors.centerIn: parent
                            spacing: 8
                            Text {
                                text: "⚠"
                                color: alarmButton.mode === "critical" ? Theme.red : (alarmButton.mode === "warning" ? Theme.amber : (alarmButton.mode === "unknown" ? Theme.gray : Theme.green))
                                font.pixelSize: 18
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: alarmButton.badge
                                color: Theme.text
                                font.pixelSize: 16
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: alarmButton.mode === "critical" ? "KRİTİK" : (alarmButton.mode === "warning" ? "UYARI" : (alarmButton.mode === "unknown" ? "BİLİNMİYOR" : "NORMAL"))
                                color: alarmButton.mode === "critical" ? Theme.red : (alarmButton.mode === "warning" ? Theme.amber : (alarmButton.mode === "unknown" ? Theme.gray : Theme.green))
                                font.pixelSize: 11
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                    Column {
                        spacing: 2
                        Layout.preferredWidth: 206
                        Layout.maximumWidth: 214
                        Layout.minimumWidth: 180
                        Text {
                            width: parent.width
                            text: "İstasyon saati " + clock.text
                            color: Theme.text
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: Ops.hataVar ? "Kritik olay" : "Canlı izleme"
                            color: Ops.hataVar ? Theme.red : Theme.green
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Text { id: clock; text: Qt.formatDateTime(new Date(), Ops.clock24h ? "dd.MM.yyyy HH:mm:ss" : "dd.MM.yyyy hh:mm:ss AP"); color: "transparent"; Timer { interval: 1000; running: true; repeat: true; onTriggered: clock.text = Qt.formatDateTime(new Date(), Ops.clock24h ? "dd.MM.yyyy HH:mm:ss" : "dd.MM.yyyy hh:mm:ss AP") } }
                    }
                    Button {
                        text: window.visibility === Window.FullScreen ? "Pencere" : "Tam ekran"
                        Layout.preferredWidth: 88
                        Layout.maximumWidth: 96
                        onClicked: window.visibility = window.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
                    }
                }
            }
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 14
                currentIndex: window.pageIndex
                OverviewPage {
                    sceneActive: window.pageIndex === 0
                    onOpenModule: (id) => { window.selectedId = id }
                    onOpenLogs: (id) => { logsPage.showIncident(id, "", "", "", "", "", ""); window.goPage(6) }
                    onOpenAlerts: (id) => window.openAlerts(id)
                }
                ModulesPage { onOpenModule: (id) => { window.selectedId = id } }
                FocusPage { group: "ticket-go"; headline: Ops.ticketGoLine; onOpenModule: (id) => window.selectedId = id }
                FocusPage { group: "gochat"; headline: "GO-CHAT API — CANLI İZLEME"; onOpenModule: (id) => window.selectedId = id }
                IntegrationsPage {}
                AlertsPage { id: alertsPage; onOpenHistory: window.goPage(8) }
                LogsPage { id: logsPage }
                PerformancePage { id: performancePage; sceneActive: window.pageIndex === 7 }
                IncidentsPage { id: incidentsPage }
                ReportsPage { id: reportsPage }
                SettingsPage { id: settingsPage }
                ApiCenterPage { id: apiCenterPage }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                color: Theme.panel
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    Text { text: "Ortam " + Ops.environmentText; color: Theme.muted; font.pixelSize: 11 }
                    Text { text: "Bölge " + Ops.regionText; color: Theme.muted; font.pixelSize: 11 }
                    Text { text: "Süre " + Ops.uptimeText; color: Theme.muted; font.pixelSize: 11 }
                    Text { text: "Sağlık " + Ops.healthRatioText; color: Theme.cyan; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }
                    Text { text: "Ticket-Go " + Ops.ticketGoLine; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
                }
            }
        }
    }

    ModuleDetail {
        visible: window.selectedId.length > 0
        anchors.fill: parent
        anchors.margins: 24
        moduleId: window.selectedId
        onClose: window.selectedId = ""
    }
    Text {
        id: notice
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 36
        color: Theme.cyan
        font.pixelSize: 13
        visible: text.length > 0
    }
    Connections {
        target: Ops
        function onNotice(text) { notice.text = text; noticeTimer.restart() }
    }
    Timer { id: noticeTimer; interval: 4000; onTriggered: notice.text = "" }
    Dialog {
        id: confirm
        modal: true
        anchors.centerIn: parent
        width: 440
        title: "Kaydı sil"
        padding: 16
        palette.window: Theme.card
        palette.windowText: Theme.title
        palette.text: Theme.text
        palette.button: Theme.surface
        palette.buttonText: Theme.text
        background: Rectangle { color: Theme.card; border.color: Theme.line; border.width: 1; radius: 12 }
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property string recordName: ""
        property string detail: ""
        signal confirmed()
        Column {
            width: 400
            spacing: 8
            Label { width: parent.width; wrapMode: Text.WordWrap; text: confirm.recordName; color: Theme.text; font.bold: true }
            Label { width: parent.width; wrapMode: Text.WordWrap; text: confirm.detail; color: Theme.muted }
            Label { width: parent.width; wrapMode: Text.WordWrap; text: "Bu kaydı silmek istediğinize emin misiniz?"; color: Theme.text }
        }
        footer: Row {
            spacing: 8
            Button { text: "İptal"; onClicked: confirm.close() }
            Button {
                text: "Sil"
                palette.button: Theme.criticalSurface
                palette.buttonText: Theme.critical
                onClicked: { confirm.confirmed(); confirm.close() }
            }
        }
        onConfirmed: {
            let message = ""
            if (window.deleteKind === "integration") message = Ops.removeIntegration(window.deleteId)
            else if (window.deleteKind === "module") message = Ops.removeCustomModule(window.deleteId)
            else if (window.deleteKind === "logfile") {
                Ops.clearLogFile()
                message = "Yerel dosya bağı kaldırıldı. Dosya silinmedi."
            } else if (window.deleteKind === "token") {
                Ops.clearLogToken()
                message = "Jeton silindi. Backend logları duruyor."
            } else if (window.deleteKind === "incidentview") {
                message = Ops.hideIncident(window.deleteId)
                if (!message || message.length === 0) message = "Olay yalnızca bu görünümden kaldırıldı. Kaynak log ve kayıt duruyor."
            } else if (window.deleteKind === "report") {
                message = Ops.deleteSavedReport(window.deleteId)
                if (!message || message.length === 0) message = "Yerel rapor kaydı silindi. Ölçüm ve olay verisi duruyor."
            } else if (window.deleteKind === "logview") {
                Ops.hideHubLog(window.deleteId)
                message = "Kayıt yalnızca masaüstü görünümünden kaldırıldı. Backend sistem logu silinmedi."
            }
            notice.text = message && message.length > 0 ? message : "Silindi."
            noticeTimer.restart()
        }
    }
    UpdateDialog { id: updateDialog }
    Dialog {
        id: unsaved
        modal: true
        anchors.centerIn: parent
        width: 460
        title: "Kaydedilmemiş değişiklikler var"
        padding: 16
        palette.window: Theme.card
        palette.windowText: Theme.title
        palette.text: Theme.text
        palette.button: Theme.surface
        palette.buttonText: Theme.text
        background: Rectangle { color: Theme.card; border.color: Theme.line; border.width: 1; radius: 12 }
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Label { width: 420; wrapMode: Text.WordWrap; color: Theme.text; text: "Ayarlar kaydedilmedi. Kaydet, vazgeç veya bu sayfada kal." }
        footer: Row {
            spacing: 8
            Button { text: "Kal"; onClicked: { window.pendingPage = -1; unsaved.close() } }
            Button { text: "Vazgeç"; onClicked: { settingsPage.load(); window.pageIndex = window.pendingPage; window.pendingPage = -1; unsaved.close() } }
            Button { text: "Kaydet"; highlighted: true; onClicked: { if (settingsPage.save()) { window.pageIndex = window.pendingPage; window.pendingPage = -1; unsaved.close() } } }
        }
    }
}
