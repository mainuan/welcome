import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Card used on the home page: a setting row with a discreet status badge and
// up to two actions (e.g. "Instalar" and "Configurar").
Rectangle {
    id: root

    property string title: ""
    property string description: ""
    property string iconName: ""
    property string statusText: ""
    property string statusKind: "neutral" // "ok", "warn" or "neutral"
    property string statusTooltip: ""

    property string primaryText: ""
    property string primaryIcon: ""
    property bool primaryEnabled: true
    property bool primaryHighlighted: true
    property string secondaryText: ""
    property string secondaryIcon: ""
    property bool secondaryEnabled: true

    property color surfaceColor: "#ffffff"
    property color borderColor: "#d9deea"
    property color accent: "#3daee9"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property color successColor: "#2da65a"
    property color warningColor: "#b7791f"

    signal primaryClicked()
    signal secondaryClicked()

    implicitHeight: row.implicitHeight
    radius: 12
    color: surfaceColor
    border.color: borderColor

    SettingRow {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        title: root.title
        subtitle: root.description
        iconName: root.iconName
        accent: root.accent
        textColor: root.textColor
        mutedColor: root.mutedColor

        RowLayout {
            visible: root.statusText.length > 0
            Layout.rightMargin: 4
            spacing: 5
            Kirigami.Icon {
                visible: root.statusKind !== "neutral"
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                source: root.statusKind === "ok" ? "checkmark" : "dialog-warning"
                color: root.statusKind === "ok" ? root.successColor : root.warningColor
                Accessible.ignored: true
            }
            Controls.Label {
                Layout.maximumWidth: 210
                text: root.statusText
                color: root.statusKind === "ok" ? root.successColor
                     : (root.statusKind === "warn" ? root.warningColor : root.mutedColor)
                font.pixelSize: 12
                font.weight: root.statusKind === "ok" ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                Accessible.name: root.title + ": " + root.statusText
                Accessible.description: root.statusTooltip
                HoverHandler { id: statusHover }
                Controls.ToolTip.visible: root.statusTooltip.length > 0 && statusHover.hovered
                Controls.ToolTip.text: root.statusTooltip
                Controls.ToolTip.delay: 300
            }
        }

        Controls.Button {
            visible: root.primaryText.length > 0
            Layout.minimumWidth: 112
            text: root.primaryText
            icon.name: root.primaryIcon
            enabled: root.primaryEnabled
            highlighted: root.primaryHighlighted && enabled
            onClicked: root.primaryClicked()
            Accessible.name: root.title + ": " + root.primaryText
        }

        Controls.Button {
            visible: root.secondaryText.length > 0
            Layout.minimumWidth: 112
            text: root.secondaryText
            icon.name: root.secondaryIcon
            enabled: root.secondaryEnabled
            onClicked: root.secondaryClicked()
            Accessible.name: root.title + ": " + root.secondaryText
        }
    }
}
