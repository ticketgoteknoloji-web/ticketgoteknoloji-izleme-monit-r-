import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Rectangle {
    id: card
    property var info: ({})
    signal open(string moduleId)
    signal removeRequested(string moduleId, string name)
    radius: 16
    color: Theme.card
    border.color: Theme.toneColor(info.tone || "gray")
    border.width: 1
    implicitWidth: 340
    implicitHeight: body.implicitHeight + 24
    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 6
        RowLayout {
            Layout.fillWidth: true
            MarkImage {
                visible: (info.logo || "").length > 0
                source: info.logo || ""
                markSize: 32
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                Layout.maximumHeight: 32
            }
            Text {
                text: info.name || ""
                color: Theme.title
                font.pixelSize: 15
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Rectangle {
                radius: 9
                color: Theme.toneColor(info.tone || "gray")
                implicitWidth: pill.implicitWidth + 14
                implicitHeight: 22
                Layout.alignment: Qt.AlignVCenter
                Text { id: pill; anchors.centerIn: parent; text: info.classLabel || info.statusLabel || ""; color: Theme.onAccent; font.pixelSize: 11; font.weight: Font.DemiBold }
            }
        }
        Text { text: info.connection || ""; color: Theme.muted; font.pixelSize: 13; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Gecikme " + (info.latency || "—"); color: Theme.cyan; font.pixelSize: 13; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: "Prob " + (info.probes || "—"); color: Theme.muted; font.pixelSize: 13; font.weight: Font.Medium; elide: Text.ElideRight }
            Text { text: info.lastCheck || "—"; color: Theme.muted; font.pixelSize: 13; font.weight: Font.Medium; elide: Text.ElideRight }
        }
        Sparkline { Layout.fillWidth: true; Layout.preferredHeight: 42; Layout.minimumHeight: 42; points: info.spark || [] }
        Text { text: "Hata " + (info.errorRate || "—") + "  Erişim " + (info.availability || "—"); color: Theme.muted; font.pixelSize: 13; font.weight: Font.Medium; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text {
            text: info.lastError && info.lastError !== "—" ? info.lastError : (info.evidence || "")
            color: Theme.text
            font.pixelSize: 13
            font.weight: Font.Medium
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            maximumLineCount: 4
            elide: Text.ElideRight
        }
        Text {
            visible: info.problem === true
            text: (info.causeLabel || "Kanıt") + ": " + (info.causeText || "—")
            color: Theme.text
            font.pixelSize: 13
            font.weight: Font.Medium
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            maximumLineCount: 4
            elide: Text.ElideRight
        }
        Text {
            visible: info.problem === true
            text: "Log: " + (info.logHint || "Log kaynağı bağlı değil")
            color: Theme.cyan
            font.pixelSize: 13
            font.weight: Font.Medium
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            Button {
                text: "Detayları görüntüle"
                font.weight: Font.Medium
                onClicked: card.open(info.moduleId || "")
            }
            Button {
                visible: (info.group || "") === "custom"
                text: "Sil"
                font.weight: Font.Medium
                palette.button: Theme.criticalSurface
                palette.buttonText: Theme.critical
                onClicked: card.removeRequested(info.moduleId || "", info.name || "")
            }
        }
    }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: card.open(info.moduleId || ""); z: -1 }
}
