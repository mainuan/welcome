import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A compact "label: value" card with a line icon (qml/icons).
Rectangle {
    id: tile

    property string icon: ""
    property string label: ""
    property string value: ""
    property color accent: "#3daee9"
    property color surfaceColor: "#ffffff"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"

    implicitHeight: row.implicitHeight + 28
    radius: 12
    color: surfaceColor
    border.color: borderColor
    Accessible.role: Accessible.StaticText
    Accessible.name: label + ": " + value

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 14
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            radius: 10
            color: Qt.rgba(tile.accent.r, tile.accent.g, tile.accent.b, 0.14)
            Kirigami.Icon {
                anchors.centerIn: parent
                width: 22
                height: 22
                source: tile.icon.length > 0 ? "qrc:/icons/" + tile.icon + ".svg" : ""
                isMask: true
                color: tile.accent
                Accessible.ignored: true
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Controls.Label {
                Layout.fillWidth: true
                text: tile.label
                color: tile.mutedColor
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Controls.Label {
                Layout.fillWidth: true
                text: tile.value.length > 0 ? tile.value : "—"
                color: tile.textColor
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }
}
