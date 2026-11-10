import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Item {
    id: page
    property int rev: Ops.revision
    property var center: {
        page.rev
        return Ops.apiCenter()
    }
    property string selectedId: ""
    property string endpointKey: ""
    property string query: ""
    property string methodFilter: "Tümü"
    property string actionNote: ""
    property string refreshingId: ""
    property int refreshPending: 0
    readonly property var methods: ["Tümü", "GET", "POST", "PUT", "PATCH", "DELETE"]
    readonly property string docTip: "Dokümantasyon henüz tanımlanmadı."

    function currentApi() {
        const list = (page.center && page.center.apis) ? page.center.apis : []
        const count = list.length || 0
        for (let i = 0; i < count; ++i) {
            const row = list[i]
            if (row && row.id === page.selectedId) return row
        }
        return null
    }
    function shownVersion(api) {
        if (!api || !api.version || api.version === "—") return "Bilgi mevcut değil"
        return api.version
    }
    function filteredEndpoints(api) {
        const rows = (api && api.endpoints) ? api.endpoints : []
        const needle = page.query.trim().toLowerCase()
        const apiName = api && api.name ? String(api.name).toLowerCase() : ""
        const out = []
        const count = rows.length || 0
        for (let i = 0; i < count; ++i) {
            const row = rows[i]
            if (!row) continue
            if (page.methodFilter !== "Tümü" && row.method !== page.methodFilter) continue
            const blob = (apiName + " " + row.method + " " + row.path + " " + row.summary).toLowerCase()
            if (needle.length > 0 && blob.indexOf(needle) < 0) continue
            out.push(row)
        }
        return out
    }
    function selectedEndpoint(api) {
        const rows = page.filteredEndpoints(api)
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            if ((row.method + " " + row.path) === page.endpointKey) return row
        }
        const all = (api && api.endpoints) ? api.endpoints : []
        const count = all.length || 0
        for (let j = 0; j < count; ++j) {
            const row = all[j]
            if (row && (row.method + " " + row.path) === page.endpointKey) return row
        }
        return null
    }
    function methodColor(method) {
        if (method === "GET") return Theme.green
        if (method === "POST") return Theme.cyan
        if (method === "PUT" || method === "PATCH") return Theme.amber
        if (method === "DELETE") return Theme.red
        return Theme.gray
    }
    function columnsFor(w) {
        if (w >= 980) return 3
        if (w >= 640) return 2
        return 1
    }
    function openApi(id) {
        page.endpointKey = ""
        page.query = ""
        page.methodFilter = "Tümü"
        if (searchField) searchField.text = ""
        page.selectedId = id
    }
    function closeDetail() {
        page.endpointKey = ""
        page.selectedId = ""
    }
    function note(message) {
        page.actionNote = message || ""
    }
    function refreshOne(id) {
        if (!id) {
            page.note("API durumu alınamadı")
            Ops.postNotice("API durumu alınamadı")
            return
        }
        if (page.refreshingId === id || (page.refreshingId === "all" && page.refreshPending > 0)) return
        const message = Ops.refreshApi(id)
        if (message === "Test gönderildi.") {
            page.note("Durum kontrolü başladı.")
            page.refreshingId = id
            page.refreshPending = 1
        } else page.note(message)
    }
    function refreshListed() {
        if (page.refreshPending > 0) return
        const api = page.currentApi()
        if (api) {
            page.refreshOne(api.integrationId)
            return
        }
        const list = (page.center && page.center.apis) ? page.center.apis : []
        const count = list.length || 0
        let started = 0
        let last = ""
        for (let i = 0; i < count; ++i) {
            const message = Ops.refreshApi(list[i].integrationId)
            last = message
            if (message === "Test gönderildi.") started += 1
        }
        page.note(started > 0 ? "Durum kontrolü başladı." : last)
        page.refreshPending = started
        page.refreshingId = started > 0 ? "all" : ""
    }
    function probeFailed(text) {
        return text.indexOf("zaman aşımı") >= 0 || text.indexOf("kurulamadı") >= 0 || text.indexOf("alınamadı") >= 0 || text.indexOf("bulunamadı") >= 0
    }

    Shortcut {
        sequences: ["Escape"]
        enabled: page.visible && (page.selectedId.length > 0 || page.endpointKey.length > 0)
        onActivated: {
            if (page.endpointKey.length > 0) page.endpointKey = ""
            else page.closeDetail()
        }
    }
    Connections {
        target: Ops
        function onTestFinished(summary) {
            if (page.refreshPending <= 0) return
            page.refreshPending -= 1
            if (page.refreshPending <= 0) {
                page.refreshPending = 0
                page.refreshingId = ""
            }
            const text = summary && summary.length > 0 ? summary : "API durumu alınamadı"
            page.note(text)
            Ops.postNotice(page.probeFailed(text) ? text : ("Durum güncellendi. " + text))
        }
    }

    component ActionButton: Button {
        id: control
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        implicitHeight: 36
        leftPadding: 12
        rightPadding: 12
        Accessible.name: control.text
        HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor }
        ToolTip.delay: 400
        ToolTip.visible: control.hovered && control.ToolTip.text.length > 0
    }

    component StatCard: Rectangle {
        property string title: ""
        property string value: ""
        property string note: ""
        radius: 16
        color: Theme.card
        border.color: Theme.line
        border.width: 1
        implicitHeight: statBody.implicitHeight + 28
        ColumnLayout {
            id: statBody
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            spacing: 4
            Text { text: title; color: Theme.muted; font.pixelSize: 12; font.weight: Font.Medium; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Text { text: value; color: Theme.title; font.pixelSize: 28; font.weight: Font.DemiBold }
            Text { text: note; color: Theme.subtitle; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        contentWidth: width
        contentHeight: view.implicitHeight
        ColumnLayout {
            id: view
            width: scroller.width
            height: implicitHeight
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        text: page.selectedId.length > 0 ? (page.currentApi() ? page.currentApi().name : "API Merkezi") : "API Merkezi"
                        color: Theme.title
                        font.pixelSize: 26
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        text: page.selectedId.length > 0
                              ? "Ölçülen adres, sağlık ve doğrulanmış uçlar"
                              : "API servisleri, entegrasyonlar ve iş ortakları yönetimi"
                        color: Theme.subtitle
                        font.pixelSize: 14
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
                BusyIndicator {
                    running: page.refreshPending > 0
                    visible: running
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                }
                ActionButton {
                    visible: page.selectedId.length === 0
                    text: "Durumu kontrol et"
                    enabled: page.refreshPending === 0
                    ToolTip.text: "Üç API grubunun sağlık kontrolünü başlatır"
                    onClicked: page.refreshListed()
                }
                ActionButton {
                    visible: page.selectedId.length > 0
                    text: "Geri"
                    Accessible.name: "API Merkezi geri"
                    ToolTip.text: "API Merkezi ana ekranına döner"
                    onClicked: page.closeDetail()
                }
            }
            Text {
                visible: page.actionNote.length > 0
                text: page.actionNote
                color: Theme.text
                font.pixelSize: 13
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            GridLayout {
                visible: page.selectedId.length === 0
                Layout.fillWidth: true
                columns: page.columnsFor(width)
                columnSpacing: 12
                rowSpacing: 12
                StatCard { Layout.fillWidth: true; title: "API Grupları"; value: String(page.center.groupCount !== undefined ? page.center.groupCount : "—"); note: "Katalogdaki grup sayısı" }
                StatCard { Layout.fillWidth: true; title: "Aktif API'ler"; value: String(page.center.activeCount !== undefined ? page.center.activeCount : "—"); note: "Yalnız ölçülmüş ve sağlıklı API'ler" }
                StatCard { Layout.fillWidth: true; title: "İş Ortakları"; value: page.center.partnerText || "—"; note: "Henüz kullanılamıyor" }
            }

            GridLayout {
                visible: page.selectedId.length === 0
                Layout.fillWidth: true
                columns: page.columnsFor(width)
                columnSpacing: 14
                rowSpacing: 14
                Repeater {
                    model: (page.center && page.center.apis) ? page.center.apis : []
                    delegate: Rectangle {
                        id: apiCard
                        required property var modelData
                        required property int index
                        property bool pointerInside: false
                        Layout.fillWidth: true
                        radius: 18
                        color: pointerInside ? Theme.hover : Theme.card
                        border.width: 1
                        border.color: pointerInside ? Theme.cyan : Theme.line
                        implicitHeight: cardBody.implicitHeight + 28
                        Behavior on border.color { ColorAnimation { duration: Ops.animationsEnabled ? 140 : 0 } }

                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 4
                            radius: 18
                            color: Theme.shadow
                            opacity: 0.12
                            z: -1
                            enabled: false
                        }
                        MouseArea {
                            anchors.fill: parent
                            z: 1
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: page.openApi(apiCard.modelData.id)
                            onEntered: apiCard.pointerInside = true
                            onExited: apiCard.pointerInside = false
                        }
                        ColumnLayout {
                            id: cardBody
                            z: 2
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 14
                            spacing: 8
                            enabled: true
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Item {
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 48
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 12
                                        color: Theme.surface
                                        enabled: false
                                        visible: brandImage.status !== Image.Ready
                                        Text { anchors.centerIn: parent; text: apiCard.modelData.initials || ""; color: Theme.title; font.pixelSize: 14; font.weight: Font.DemiBold }
                                    }
                                    Image {
                                        id: brandImage
                                        anchors.fill: parent
                                        source: apiCard.modelData.logo || ""
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        mipmap: true
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: apiCard.modelData.name || ""; color: Theme.title; font.pixelSize: 16; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                    Text { text: apiCard.modelData.summary || ""; color: Theme.subtitle; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
                                }
                            }
                            Rectangle {
                                radius: 10
                                enabled: false
                                color: Theme.toneColor(apiCard.modelData.tone || "gray")
                                implicitWidth: statusText.implicitWidth + 16
                                implicitHeight: 24
                                Text { id: statusText; anchors.centerIn: parent; text: apiCard.modelData.statusLabel || "Ölçüm yok"; color: Theme.onAccent; font.pixelSize: 12; font.weight: Font.DemiBold }
                            }
                            Text { text: "Adres  " + (apiCard.modelData.baseUrl || "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { text: "Sürüm  " + page.shownVersion(apiCard.modelData); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { text: "Bağlantı  " + (apiCard.modelData.connection || "Ölçüm yok"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { text: "Son kontrol  " + (apiCard.modelData.lastCheck || "—"); color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 8
                                ActionButton {
                                    text: "API detayları"
                                    Accessible.name: (apiCard.modelData.name || "API") + " detayları"
                                    ToolTip.text: (apiCard.modelData.name || "API") + " detayını açar"
                                    onClicked: page.openApi(apiCard.modelData.id)
                                }
                                Item {
                                    implicitWidth: docButton.implicitWidth
                                    implicitHeight: docButton.implicitHeight
                                    ActionButton {
                                        id: docButton
                                        text: "Dokümantasyon"
                                        enabled: apiCard.modelData.docAvailable === true
                                        Accessible.name: (apiCard.modelData.name || "API") + " dokümantasyon"
                                        ToolTip.text: page.docTip
                                        onClicked: page.note(Ops.openApiDoc(apiCard.modelData.id))
                                    }
                                    HoverHandler { id: docHover; cursorShape: Qt.ForbiddenCursor }
                                    ToolTip.visible: docHover.hovered && !docButton.enabled
                                    ToolTip.text: page.docTip
                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: !docButton.enabled
                                        cursorShape: Qt.ForbiddenCursor
                                        onClicked: page.note(Ops.openApiDoc(apiCard.modelData.id))
                                    }
                                }
                                ActionButton {
                                    text: "Adresi kopyala"
                                    Accessible.name: (apiCard.modelData.name || "API") + " adresini kopyala"
                                    ToolTip.text: "Paylaşılabilir base URL panoya kopyalanır"
                                    onClicked: page.note(Ops.copyApiText(apiCard.modelData.baseUrl || ""))
                                }
                                ActionButton {
                                    text: "Yenile"
                                    Accessible.name: (apiCard.modelData.name || "API") + " yenile"
                                    enabled: page.refreshingId !== apiCard.modelData.integrationId
                                    ToolTip.text: "Yalnız bu API'nin sağlık kontrolünü başlatır"
                                    onClicked: page.refreshOne(apiCard.modelData.integrationId)
                                }
                                BusyIndicator {
                                    running: page.refreshingId === apiCard.modelData.integrationId
                                    visible: running
                                    implicitWidth: 28
                                    implicitHeight: 28
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: page.selectedId.length === 0
                Layout.fillWidth: true
                radius: 16
                color: Theme.card
                border.color: Theme.line
                border.width: 1
                implicitHeight: partnerBody.implicitHeight + 28
                ColumnLayout {
                    id: partnerBody
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    spacing: 6
                    Text { text: "İş ortaklarına API"; color: Theme.title; font.pixelSize: 16; font.weight: Font.DemiBold }
                    Text { text: "Henüz kullanılamıyor"; color: Theme.muted; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Text { text: page.center.partnerNote || ""; color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: ["Firma kaydı", "Erişim talebi", "Firma yetkisi", "API ürünü", "Anahtar", "Kota", "Hız sınırı", "IP kısıtı", "Sandbox", "Kullanım raporu", "Erişim kaydı", "Sözleşme"]
                            delegate: Rectangle {
                                required property string modelData
                                radius: 8
                                color: Theme.surface
                                border.color: Theme.line
                                implicitWidth: gapText.implicitWidth + 16
                                implicitHeight: 28
                                enabled: false
                                Text { id: gapText; anchors.centerIn: parent; text: modelData + " · Henüz kullanılamıyor"; color: Theme.muted; font.pixelSize: 11 }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                visible: page.selectedId.length > 0 && page.currentApi() !== null
                Layout.fillWidth: true
                spacing: 14
                Rectangle {
                    Layout.fillWidth: true
                    radius: 18
                    color: Theme.card
                    border.color: Theme.line
                    implicitHeight: detailHead.implicitHeight + 28
                    ColumnLayout {
                        id: detailHead
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Item {
                                Layout.preferredWidth: 56
                                Layout.preferredHeight: 56
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 12
                                    color: Theme.surface
                                    enabled: false
                                    visible: detailLogo.status !== Image.Ready
                                    Text { anchors.centerIn: parent; text: page.currentApi() ? page.currentApi().initials : ""; color: Theme.title; font.weight: Font.DemiBold }
                                }
                                Image {
                                    id: detailLogo
                                    anchors.fill: parent
                                    source: page.currentApi() ? (page.currentApi().logo || "") : ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    mipmap: true
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: page.currentApi() ? page.currentApi().name : ""; color: Theme.title; font.pixelSize: 20; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                Text { text: page.currentApi() ? page.currentApi().summary : ""; color: Theme.subtitle; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            }
                            Rectangle {
                                radius: 10
                                enabled: false
                                color: Theme.toneColor(page.currentApi() ? page.currentApi().tone : "gray")
                                implicitWidth: detailStatus.implicitWidth + 16
                                implicitHeight: 26
                                Text { id: detailStatus; anchors.centerIn: parent; text: page.currentApi() ? page.currentApi().statusLabel : "Ölçüm yok"; color: Theme.onAccent; font.pixelSize: 12; font.weight: Font.DemiBold }
                            }
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: page.width >= 760 ? 2 : 1
                            columnSpacing: 16
                            rowSpacing: 6
                            Text { text: "Base URL  " + (page.currentApi() && page.currentApi().baseUrl ? page.currentApi().baseUrl : "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { text: "Sürüm  " + page.shownVersion(page.currentApi()); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { text: "Ortam  " + (page.currentApi() && page.currentApi().environment ? page.currentApi().environment : "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { text: "Bağlantı  " + (page.currentApi() && page.currentApi().connection ? page.currentApi().connection : "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { text: "Son kontrol  " + (page.currentApi() && page.currentApi().lastCheck ? page.currentApi().lastCheck : "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { text: "Kimlik  " + (page.currentApi() && page.currentApi().auth ? page.currentApi().auth : "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { text: "Dokümantasyon  " + page.docTip; color: Theme.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        }
                        Text { text: page.currentApi() && page.currentApi().evidence ? page.currentApi().evidence : "Bilgi mevcut değil"; color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            ActionButton { text: "Geri"; Accessible.name: "Detay geri"; ToolTip.text: "API Merkezi ana ekranına döner"; onClicked: page.closeDetail() }
                            Item {
                                implicitWidth: detailDoc.implicitWidth
                                implicitHeight: detailDoc.implicitHeight
                                ActionButton {
                                    id: detailDoc
                                    text: "Dokümantasyon"
                                    enabled: page.currentApi() && page.currentApi().docAvailable === true
                                    Accessible.name: "Detay dokümantasyon"
                                    ToolTip.text: page.docTip
                                    onClicked: page.note(Ops.openApiDoc(page.currentApi() ? page.currentApi().id : ""))
                                }
                                HoverHandler { id: detailDocHover; cursorShape: Qt.ForbiddenCursor }
                                ToolTip.visible: detailDocHover.hovered && !detailDoc.enabled
                                ToolTip.text: page.docTip
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: !detailDoc.enabled
                                    cursorShape: Qt.ForbiddenCursor
                                    onClicked: page.note(Ops.openApiDoc(page.currentApi() ? page.currentApi().id : ""))
                                }
                            }
                            ActionButton {
                                text: "Adresi kopyala"
                                Accessible.name: "Detay adresi kopyala"
                                ToolTip.text: "Paylaşılabilir base URL panoya kopyalanır"
                                onClicked: page.note(Ops.copyApiText(page.currentApi() && page.currentApi().baseUrl ? page.currentApi().baseUrl : ""))
                            }
                            ActionButton {
                                text: "Yenile"
                                Accessible.name: "Detay yenile"
                                enabled: page.refreshPending === 0
                                ToolTip.text: "Yalnız bu API'nin sağlık kontrolünü başlatır"
                                onClicked: page.refreshOne(page.currentApi() ? page.currentApi().integrationId : "")
                            }
                            BusyIndicator { running: page.refreshPending > 0 && page.selectedId.length > 0; visible: running; implicitWidth: 28; implicitHeight: 28 }
                        }
                    }
                }

                Rectangle {
                    visible: page.currentApi() && page.currentApi().id === "ferry"
                    Layout.fillWidth: true
                    radius: 16
                    color: Theme.card
                    border.color: Theme.line
                    implicitHeight: ferryBody.implicitHeight + 28
                    ColumnLayout {
                        id: ferryBody
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 14
                        spacing: 6
                        Text { text: "Satış sağlayıcıları"; color: Theme.title; font.pixelSize: 16; font.weight: Font.DemiBold }
                        Text { text: page.currentApi() && page.currentApi().providerNote ? page.currentApi().providerNote : "Bilgi mevcut değil"; color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: "Liknoss  " + (page.currentApi() && page.currentApi().liknossLabel ? page.currentApi().liknossLabel : "Ölçüm yok"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: page.currentApi() && page.currentApi().liknossNote ? page.currentApi().liknossNote : ""; color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: "Yapılandırma ucu  " + (page.currentApi() && page.currentApi().configLabel ? page.currentApi().configLabel : "Ölçüm yok"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: (page.currentApi() && page.currentApi().capabilities) ? page.currentApi().capabilities : []
                                delegate: Rectangle {
                                    required property var modelData
                                    radius: 8
                                    color: Theme.surface
                                    border.color: Theme.line
                                    enabled: false
                                    implicitWidth: capText.implicitWidth + 16
                                    implicitHeight: 28
                                    Text { id: capText; anchors.centerIn: parent; text: modelData.name + " · " + modelData.state; color: Theme.muted; font.pixelSize: 12 }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: page.selectedEndpoint(page.currentApi()) !== null
                    Layout.fillWidth: true
                    radius: 16
                    color: Theme.card
                    border.color: Theme.cyan
                    implicitHeight: endpointDetail.implicitHeight + 28
                    ColumnLayout {
                        id: endpointDetail
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 14
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Endpoint detayı"; color: Theme.title; font.pixelSize: 16; font.weight: Font.DemiBold; Layout.fillWidth: true }
                            ActionButton { text: "Kapat"; Accessible.name: "Endpoint detayını kapat"; ToolTip.text: "Endpoint detayını kapatır"; onClicked: page.endpointKey = "" }
                        }
                        Text { text: page.selectedEndpoint(page.currentApi()) ? (page.selectedEndpoint(page.currentApi()).method + " " + page.selectedEndpoint(page.currentApi()).path) : ""; color: Theme.title; font.pixelSize: 14; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: "Açıklama  " + (page.selectedEndpoint(page.currentApi()) && page.selectedEndpoint(page.currentApi()).summary ? page.selectedEndpoint(page.currentApi()).summary : "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: "Parametre  " + (page.selectedEndpoint(page.currentApi()) && page.selectedEndpoint(page.currentApi()).params ? page.selectedEndpoint(page.currentApi()).params : "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: "Kimlik  " + (page.selectedEndpoint(page.currentApi()) && page.selectedEndpoint(page.currentApi()).auth ? page.selectedEndpoint(page.currentApi()).auth : "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: "Yanıt kodları  " + (page.selectedEndpoint(page.currentApi()) && page.selectedEndpoint(page.currentApi()).responses ? page.selectedEndpoint(page.currentApi()).responses : "Bilgi mevcut değil"); color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: "Örnek istek  " + (page.selectedEndpoint(page.currentApi()) && page.selectedEndpoint(page.currentApi()).exampleRequest ? page.selectedEndpoint(page.currentApi()).exampleRequest : "Bilgi mevcut değil"); color: Theme.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: "Örnek yanıt  " + (page.selectedEndpoint(page.currentApi()) && page.selectedEndpoint(page.currentApi()).exampleResponse ? page.selectedEndpoint(page.currentApi()).exampleResponse : "Bilgi mevcut değil"); color: Theme.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text {
                            visible: page.selectedEndpoint(page.currentApi()) && page.selectedEndpoint(page.currentApi()).method !== "GET"
                            text: "Bu ekran POST, PUT, PATCH veya DELETE çağrısı başlatmaz."
                            color: Theme.amber
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 16
                    color: Theme.card
                    border.color: Theme.line
                    implicitHeight: catalogBody.implicitHeight + 28
                    ColumnLayout {
                        id: catalogBody
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 14
                        spacing: 8
                        Text { text: "Endpoint kataloğu"; color: Theme.title; font.pixelSize: 16; font.weight: Font.DemiBold }
                        Text {
                            text: page.currentApi() ? (page.currentApi().endpointCount + " doğrulanmış uç. Satır detayı açar. Canlı işlem başlatılmaz.") : ""
                            color: Theme.muted
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: "API, uç veya metot ara"
                            Accessible.name: "Endpoint ara"
                            onTextChanged: page.query = text
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: page.methods
                                delegate: ActionButton {
                                    required property string modelData
                                    text: modelData
                                    highlighted: page.methodFilter === modelData
                                    Accessible.name: "Metot " + modelData
                                    ToolTip.text: modelData === "Tümü" ? "Metot süzgecini kaldırır" : (modelData + " uçlarını gösterir")
                                    onClicked: page.methodFilter = modelData
                                }
                            }
                            ActionButton {
                                text: "Temizle"
                                enabled: page.query.length > 0 || page.methodFilter !== "Tümü"
                                Accessible.name: "Süzgeci temizle"
                                ToolTip.text: "Aramayı ve metot süzgecini temizler"
                                onClicked: { searchField.text = ""; page.methodFilter = "Tümü" }
                            }
                        }
                        Text {
                            visible: page.filteredEndpoints(page.currentApi()).length === 0
                            text: "Eşleşen endpoint bulunamadı."
                            color: Theme.muted
                            font.pixelSize: 13
                        }
                        Repeater {
                            model: page.filteredEndpoints(page.currentApi())
                            delegate: Rectangle {
                                id: endpointRow
                                required property var modelData
                                required property int index
                                property bool pointerInside: false
                                Layout.fillWidth: true
                                radius: 12
                                color: page.endpointKey === (modelData.method + " " + modelData.path) ? Theme.selected : (pointerInside ? Theme.hover : Theme.surface)
                                border.color: pointerInside ? Theme.cyan : Theme.line
                                implicitHeight: endpointBody.implicitHeight + 20
                                MouseArea {
                                    anchors.fill: parent
                                    z: 1
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.endpointKey = endpointRow.modelData.method + " " + endpointRow.modelData.path
                                    onEntered: endpointRow.pointerInside = true
                                    onExited: endpointRow.pointerInside = false
                                }
                                ColumnLayout {
                                    id: endpointBody
                                    z: 2
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 10
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Rectangle {
                                            radius: 6
                                            enabled: false
                                            color: page.methodColor(endpointRow.modelData.method)
                                            implicitWidth: methodLabel.implicitWidth + 12
                                            implicitHeight: 22
                                            Text { id: methodLabel; anchors.centerIn: parent; text: endpointRow.modelData.method || ""; color: Theme.onAccent; font.pixelSize: 11; font.weight: Font.DemiBold }
                                        }
                                        Text { text: endpointRow.modelData.path || ""; color: Theme.title; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                        Rectangle {
                                            radius: 6
                                            enabled: false
                                            color: endpointRow.modelData.shareable ? Theme.successSurface : Theme.unknownSurface
                                            implicitWidth: audienceLabel.implicitWidth + 12
                                            implicitHeight: 22
                                            Text { id: audienceLabel; anchors.centerIn: parent; text: endpointRow.modelData.audienceLabel || ""; color: Theme.title; font.pixelSize: 11 }
                                        }
                                    }
                                    Text { text: endpointRow.modelData.summary || "Bilgi mevcut değil"; color: Theme.text; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                    ActionButton {
                                        text: "Yolu kopyala"
                                        enabled: endpointRow.modelData.shareable === true
                                        Accessible.name: (endpointRow.modelData.method || "") + " " + (endpointRow.modelData.path || "") + " kopyala"
                                        ToolTip.text: enabled ? "Paylaşılabilir yolu panoya kopyalar" : "İç izleme yolu kopyalanmaz."
                                        onClicked: page.note(Ops.copyApiText(endpointRow.modelData.shareable === true ? (endpointRow.modelData.path || "") : ""))
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    }
}
