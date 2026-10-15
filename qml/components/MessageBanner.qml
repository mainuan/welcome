import QtQuick
import QtQuick.Controls as Controls

// Last result reported by SystemService (theme, accent, style, layouts).
Rectangle {
    id: banner

    property string text: ""
    property bool dark: false
    property color accent: "#3daee9"
    property color textColor: "#202532"
    property bool error: false
    property color errorColor: "#c0392b"
    readonly property color tone: error ? errorColor : accent

    visible: text.length > 0
    implicitHeight: label.implicitHeight + 26
    radius: 10
    color: Qt.rgba(tone.r, tone.g, tone.b, dark ? 0.15 : 0.10)
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.35)

    Controls.Label {
        id: label
        anchors.fill: parent
        anchors.margins: 13
        text: banner.text
        color: banner.textColor
        wrapMode: Text.WordWrap
        verticalAlignment: Text.AlignVCenter
        Accessible.role: Accessible.StaticText
    }
}
