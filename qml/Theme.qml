pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#F5F7FA"
    readonly property color panel: "#E9EFF5"
    readonly property color card: "#FFFFFF"
    readonly property color field: "#FFFFFF"
    readonly property color fieldHover: "#F8FAFC"
    readonly property color fieldBorder: "#CBD5E1"
    readonly property color selection: "#BFDBFE"
    readonly property color selectionText: "#172B4D"
    readonly property color surface: "#EEF2F6"
    readonly property color chart: "#E7F1FA"
    readonly property color text: "#243247"
    readonly property color title: "#172B4D"
    readonly property color subtitle: "#475569"
    readonly property color muted: "#64748B"
    readonly property color line: "#DCE3EB"
    readonly property color cyan: "#2563EB"
    readonly property color hover: "#E2ECFA"
    readonly property color selected: "#D8E7FC"
    readonly property color green: "#16835D"
    readonly property color amber: "#A16207"
    readonly property color red: "#C62828"
    readonly property color critical: "#B91C1C"
    readonly property color gray: "#64748B"
    readonly property color onAccent: "#FFFFFF"
    readonly property color criticalSurface: "#FDECEC"
    readonly property color warningSurface: "#FEF6E6"
    readonly property color successSurface: "#E7F6EF"
    readonly property color unknownSurface: "#EEF2F6"
    readonly property color shadow: "#1A172B4D"

    function toneColor(tone) {
        if (tone === "green") return green
        if (tone === "amber") return amber
        if (tone === "red") return red
        if (tone === "cyan") return cyan
        return gray
    }
}
