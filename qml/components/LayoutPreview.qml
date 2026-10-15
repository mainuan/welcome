import QtQuick

// Miniature screen showing where a desktop layout puts its panels.
Rectangle {
    id: preview

    property string layoutId: ""
    property color accent: "#3daee9"
    property color panelColor: "#4f5a70"
    property color borderColor: "#d9deea"

    // Shapes in fractions of the screen: x, y, w, h; "pill" rounds the ends,
    // "tile" draws an outlined KWin tile.
    readonly property var shapes: ({
        "plasma-default": [
            { x: 0.02, y: 0.85, w: 0.07, h: 0.10, pill: true }, { x: 0.11, y: 0.85, w: 0.15, h: 0.10, pill: true },
            { x: 0.35, y: 0.85, w: 0.30, h: 0.10, pill: true }, { x: 0.69, y: 0.85, w: 0.17, h: 0.10, pill: true },
            { x: 0.88, y: 0.85, w: 0.10, h: 0.10, pill: true }],
        "panel-top": [{ x: 0, y: 0, w: 1, h: 0.10 }],
        "floating": [{ x: 0.28, y: 0.83, w: 0.44, h: 0.12, pill: true }],
        "minimal": [{ x: 0, y: 0.935, w: 1, h: 0.065 }],
        "unity": [{ x: 0, y: 0, w: 1, h: 0.075 }, { x: 0, y: 0.075, w: 0.08, h: 0.925 }],
        "tiling": [
            { x: 0, y: 0, w: 1, h: 0.075 },
            { x: 0.03, y: 0.12, w: 0.455, h: 0.84, tile: true },
            { x: 0.515, y: 0.12, w: 0.455, h: 0.405, tile: true },
            { x: 0.515, y: 0.555, w: 0.455, h: 0.405, tile: true }]
    })[layoutId] || []

    implicitHeight: 92
    radius: 8
    clip: true
    color: Qt.rgba(accent.r, accent.g, accent.b, 0.14)
    border.color: borderColor
    Accessible.ignored: true

    Repeater {
        model: preview.shapes
        delegate: Rectangle {
            required property var modelData
            x: modelData.x * preview.width
            y: modelData.y * preview.height
            width: modelData.w * preview.width
            height: modelData.h * preview.height
            radius: modelData.pill ? height / 2 : (modelData.tile ? 3 : 0)
            color: modelData.tile ? Qt.rgba(preview.accent.r, preview.accent.g, preview.accent.b, 0.10)
                                  : Qt.rgba(preview.panelColor.r, preview.panelColor.g, preview.panelColor.b, 0.92)
            border.width: modelData.tile ? 1 : 0
            border.color: preview.accent
        }
    }
}
