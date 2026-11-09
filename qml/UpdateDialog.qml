import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Dialog {
    id: dialog
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 560
    padding: 18
    title: "Uygulama Güncellemesi"
    closePolicy: Popup.CloseOnEscape
    palette.window: Theme.card
    palette.windowText: Theme.title
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    background: Rectangle { color: Theme.card; border.color: Theme.line; border.width: 1; radius: 12 }

    function sync() {
        if (Updates.dialogOpen && !dialog.opened) dialog.open()
        if (!Updates.dialogOpen && dialog.opened && Updates.phase !== "DOWNLOADING" && Updates.phase !== "VERIFYING") dialog.close()
    }
    Connections {
        target: Updates
        function onChanged() { dialog.sync() }
    }

    ColumnLayout {
        width: dialog.availableWidth
        spacing: 8
        Label { text: "TICKET GO TEKNOLOJİ A.Ş."; color: Theme.title; font.bold: true; font.pixelSize: 16 }
        Label { text: "Ticketgo Teknoloji Gözlem Merkezi"; color: Theme.subtitle; font.pixelSize: 12 }
        Label { text: "Mevcut Sürüm: " + Updates.currentVersion; color: Theme.text }
        Label { text: "Yeni Sürüm: " + (Updates.latestVersion.length > 0 ? Updates.latestVersion : "Veri yok"); color: Theme.text }
        Label { text: "Yayın Tarihi: " + (Updates.publishedText.length > 0 ? Updates.publishedText : "Veri yok"); color: Theme.text }
        Label { text: "Paket Boyutu: " + (Updates.packageBytesText.length > 0 ? Updates.packageBytesText : "Veri yok"); color: Theme.text }
        Label { text: "Hedef Platform: " + Updates.platformText; color: Theme.text }
        Label { text: "Sürüm Notları"; color: Theme.title; font.bold: true }
        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            contentHeight: notes.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Text {
                id: notes
                width: parent.width
                text: Updates.releaseNotes.length > 0 ? Updates.releaseNotes : "Sürüm notu yok."
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                color: Theme.text
                font.pixelSize: 13
            }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        }
        Label {
            visible: Updates.progressText.length > 0
            text: Updates.progressText
            color: Theme.cyan
            font.bold: true
        }
        Label {
            visible: Updates.transferText.length > 0
            text: Updates.transferText + (Updates.speedText.length > 0 ? " · " + Updates.speedText : "")
            color: Theme.muted
        }
        ProgressBar {
            Layout.fillWidth: true
            visible: Updates.phase === "DOWNLOADING" && Updates.progressPercent >= 0
            from: 0
            to: 100
            value: Updates.progressPercent
        }
        Label {
            visible: Updates.errorMessage.length > 0
            text: Updates.errorMessage
            color: Theme.red
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            visible: Updates.guidance.length > 0
            text: Updates.guidance
            color: Theme.text
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
        Button {
            text: Updates.phase === "DOWNLOADING" ? "İptal" : "İPTAL"
            onClicked: Updates.dismissDialog()
        }
        Button {
            visible: Updates.phase === "UPDATE_AVAILABLE"
            text: "İNDİR VE GÜNCELLE"
            highlighted: true
            onClicked: Updates.confirmDownload()
        }
        Button {
            visible: Updates.phase === "READY_TO_INSTALL"
            text: "Güncellemeyi Yükle"
            highlighted: true
            onClicked: Updates.confirmInstall()
        }
    }
}
