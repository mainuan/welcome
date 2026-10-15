import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A full-width clickable card: line icon, title, description and an arrow.
Controls.AbstractButton {
    id: card

    property string iconName: ""
    property string title: ""
    property string description: ""
    property color accent: "#3daee9"
    property color surfaceColor: "#ffffff"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"

    implicitHeight: row.implicitHeight + 28
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: description

    background: Rectangle {
        radius: 12
        color: card.down ? Qt.rgba(card.accent.r, card.accent.g, card.accent.b, 0.10) : card.surfaceColor
        border.width: card.visualFocus ? 2 : 1
        border.color: card.visualFocus || card.hovered ? Qt.rgba(card.accent.r, card.accent.g, card.accent.b, card.visualFocus ? 1 : 0.55)
                                                       : card.borderColor
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    contentItem: RowLayout {
        id: row
        spacing: 14
        anchors.fill: parent
        anchors.margins: 14

        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: 10
            color: Qt.rgba(card.accent.r, card.accent.g, card.accent.b, 0.14)
            Kirigami.Icon {
                anchors.centerIn: parent
                width: 22
                height: 22
                source: "qrc:/icons/" + card.iconName + ".svg"
                isMask: true
                color: card.accent
                Accessible.ignored: true
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Controls.Label {
                Layout.fillWidth: true
                text: card.title
                color: card.textColor
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Controls.Label {
                Layout.fillWidth: true
                text: card.description
                color: card.mutedColor
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
        Kirigami.Icon {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            source: "qrc:/icons/arrow-right.svg"
            isMask: true
            color: card.hovered || card.visualFocus ? card.accent : card.mutedColor
            Accessible.ignored: true
            transform: Translate { x: card.hovered ? 2 : 0; Behavior on x { NumberAnimation { duration: 120 } } }
        }
    }
}
