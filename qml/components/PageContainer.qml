import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

// Scrollable body shared by every page: a centered column of at most
// `maxWidth` pixels, so the content starts at the same place on every page.
Controls.ScrollView {
    id: container

    property int maxWidth: 820
    property int margin: 24
    property alias contentSpacing: column.spacing
    default property alias content: column.data

    anchors.fill: parent
    clip: true
    contentWidth: availableWidth
    contentHeight: column.implicitHeight + 2 * margin

    ColumnLayout {
        id: column
        width: Math.max(0, Math.min(container.availableWidth - 2 * container.margin, container.maxWidth))
        x: Math.max(container.margin, (container.availableWidth - width) / 2)
        y: container.margin
        spacing: 14
    }
}
