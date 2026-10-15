import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Controls.Button {
    id: control

    property bool selected: false
    property bool collapsed: false
    property string label: ""
    property string iconName: ""
    property bool dark: false
    property color accent: "#3daee9"
    property color mutedColor: "#697286"
    property color textColor: "#202532"

    // Line icons from qml/icons, drawn as a mask in the text/accent color so the
    // navigation looks the same with any icon theme.
    readonly property int iconSize: collapsed ? 28 : 26

    implicitHeight: 48
    implicitWidth: collapsed ? 58 : 210
    text: collapsed ? "" : label
    display: collapsed ? Controls.AbstractButton.IconOnly : Controls.AbstractButton.TextBesideIcon
    leftPadding: collapsed ? (width - iconSize) / 2 : 14
    rightPadding: collapsed ? 18 : 12
    Accessible.name: label
    Controls.ToolTip.visible: collapsed && hovered
    Controls.ToolTip.text: label

    background: Rectangle {
        radius: 10
        color: control.selected ? Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.20 : 0.12)
                                : (control.hovered ? Qt.rgba(1, 1, 1, dark ? 0.07 : 0.50) : "transparent")
        border.color: control.visualFocus ? accent
                    : (control.selected ? Qt.rgba(accent.r, accent.g, accent.b, 0.45) : "transparent")
        border.width: control.visualFocus ? 2 : (control.selected ? 1 : 0)
    }

    contentItem: Row {
        spacing: 12
        anchors.fill: parent
        anchors.leftMargin: control.leftPadding
        anchors.rightMargin: control.rightPadding
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        layoutDirection: Qt.LeftToRight

        Kirigami.Icon {
            width: control.iconSize
            height: control.iconSize
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/icons/" + control.iconName + ".svg"
            isMask: true
            color: control.selected ? accent : (control.hovered || control.visualFocus ? textColor : mutedColor)
            Behavior on color { ColorAnimation { duration: 120 } }
            Accessible.ignored: true
        }

        Controls.Label {
            visible: !control.collapsed
            width: parent.width - control.iconSize - 12
            anchors.verticalCenter: parent.verticalCenter
            text: control.label
            color: control.selected ? accent : textColor
            elide: Text.ElideRight
            font.weight: control.selected ? Font.DemiBold : Font.Normal
        }
    }
}
