import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TicketGo.Ops

Dialog {
    id: dialog
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    z: 100
    width: 460
    padding: 18
    title: dialog.showMfa ? "E-POSTA DOĞRULAMA" : "TICKET-GO BACKEND\nİZLEME HESABI GİRİŞİ"
    closePolicy: Popup.CloseOnEscape
    palette.window: Theme.card
    palette.windowText: Theme.title
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    background: Rectangle {
        color: Theme.card
        border.color: Theme.line
        border.width: 1
        radius: 12
        layer.enabled: true
    }

    property bool preferLogin: false
    readonly property bool showMfa: Ops.logSessionPhase === "mfa" && !preferLogin

    function begin() {
        if (dialog.opened) return
        dialog.preferLogin = false
        userField.text = ""
        passwordField.text = ""
        codeField.text = ""
        dialog.open()
        if (dialog.showMfa) codeField.forceActiveFocus()
        else userField.forceActiveFocus()
    }

    function submitLogin() {
        if (userField.text.trim().length === 0 || passwordField.text.length === 0) {
            Ops.openLogSession("", "")
            return
        }
        const password = passwordField.text
        passwordField.text = ""
        dialog.preferLogin = false
        Ops.openLogSession(userField.text.trim(), password)
    }

    onClosed: passwordField.text = ""

    Connections {
        target: Ops
        function onChanged() {
            if (!dialog.opened) return
            if (Ops.logSessionPhase === "open") dialog.close()
            if (Ops.logSessionPhase === "mfa") dialog.preferLogin = false
        }
    }

    ColumnLayout {
        width: dialog.availableWidth
        spacing: 10
        Label {
            visible: !dialog.showMfa
            text: "Sistem loglarına erişmek için backend izleme hesabınızla giriş yapın."
            color: Theme.text
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            visible: dialog.showMfa
            text: "Doğrulama kodunu girin. Kod kaydedilmez." + (Ops.logMfaHint.length > 0 ? "\n" + Ops.logMfaHint : "")
            color: Theme.text
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        TextField {
            id: userField
            visible: !dialog.showMfa
            placeholderText: "Kullanıcı adı / e-posta"
            Layout.fillWidth: true
            color: Theme.title
            placeholderTextColor: Theme.muted
            Keys.onReturnPressed: dialog.submitLogin()
            Keys.onEnterPressed: dialog.submitLogin()
        }
        RowLayout {
            visible: !dialog.showMfa
            Layout.fillWidth: true
            TextField {
                id: passwordField
                echoMode: reveal.checked ? TextInput.Normal : TextInput.Password
                placeholderText: "Şifre"
                Layout.fillWidth: true
                color: Theme.title
                placeholderTextColor: Theme.muted
                Keys.onReturnPressed: dialog.submitLogin()
                Keys.onEnterPressed: dialog.submitLogin()
            }
            CheckBox { id: reveal; text: "Göster" }
        }
        TextField {
            id: codeField
            visible: dialog.showMfa
            placeholderText: "Doğrulama kodu"
            Layout.fillWidth: true
            inputMethodHints: Qt.ImhDigitsOnly
            color: Theme.title
            placeholderTextColor: Theme.muted
            maximumLength: 12
        }
        BusyIndicator { visible: Ops.logSessionBusy; running: Ops.logSessionBusy; Layout.alignment: Qt.AlignHCenter }
        Label {
            visible: Ops.logSessionMessage.length > 0 && Ops.logSessionPhase !== "open"
            text: Ops.logSessionMessage
            color: Ops.logSessionPhase === "error" ? Theme.red : Theme.text
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
        Button {
            visible: dialog.showMfa
            text: "GERİ"
            enabled: !Ops.logSessionBusy
            onClicked: dialog.preferLogin = true
        }
        Button {
            visible: dialog.showMfa
            text: "YENİDEN GÖNDER"
            enabled: !Ops.logSessionBusy
            onClicked: {
                codeField.text = ""
                Ops.resendLogMfa()
            }
        }
        Button {
            text: "İPTAL"
            onClicked: dialog.close()
        }
        Button {
            visible: dialog.showMfa
            text: "DOĞRULA"
            highlighted: true
            enabled: !Ops.logSessionBusy
            onClicked: {
                const code = codeField.text.trim()
                codeField.text = ""
                Ops.submitLogMfa(code)
            }
        }
        Button {
            visible: !dialog.showMfa
            text: "GİRİŞ YAP"
            highlighted: true
            enabled: !Ops.logSessionBusy
            onClicked: dialog.submitLogin()
        }
    }
}
