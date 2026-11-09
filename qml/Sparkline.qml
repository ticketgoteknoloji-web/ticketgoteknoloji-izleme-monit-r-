import QtQuick
import TicketGo.Ops

Item {
    id: root
    property var points: []
    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.points || root.points.length < 2) return
            let min = root.points[0]
            let max = root.points[0]
            for (let i = 1; i < root.points.length; ++i) {
                min = Math.min(min, root.points[i])
                max = Math.max(max, root.points[i])
            }
            if (max === min) max = min + 1
            ctx.strokeStyle = Theme.cyan
            ctx.lineWidth = 2
            ctx.beginPath()
            for (let i = 0; i < root.points.length; ++i) {
                const x = i * width / (root.points.length - 1)
                const y = height - ((root.points[i] - min) / (max - min)) * (height - 4) - 2
                if (i === 0) ctx.moveTo(x, y)
                else ctx.lineTo(x, y)
            }
            ctx.stroke()
        }
    }
    Text {
        anchors.centerIn: parent
        visible: !root.points || root.points.length < 2
        text: "Ölçüm yok"
        color: Theme.muted
        font.pixelSize: 11
    }
    onPointsChanged: canvas.requestPaint()
    Component.onCompleted: canvas.requestPaint()
}
