import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Icon, title and subtitle on the left with arbitrary controls on the right.
// When the row is narrow the controls move below the text.
Item {
    id: row

    property string title: ""
    property string subtitle: ""
    property string iconName: ""
    property color accent: "#3daee9"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property int compactWidth: 620
    default property alias trailing: trailingRow.data

    readonly property bool compact: width < compactWidth

    implicitHeight: grid.implicitHeight + 28

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        columns: row.compact ? 2 : 3
        columnSpacing: 14
        rowSpacing: 10

        Kirigami.Icon {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 2
            source: row.iconName
            color: row.accent
            Accessible.ignored: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Controls.Label {
                Layout.fillWidth: true
                text: row.title
                color: row.textColor
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }
            Controls.Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: row.subtitle
                color: row.mutedColor
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            id: trailingRow
            Layout.column: row.compact ? 1 : 2
            Layout.row: row.compact ? 1 : 0
            Layout.alignment: row.compact ? Qt.AlignLeft : (Qt.AlignRight | Qt.AlignVCenter)
            spacing: 8
        }
    }
}
