import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    property var system
    property var layouts
    property var layoutManager
    property bool dark: false
    property color accent: "#3daee9"
    property color pageColor: "#f3f5f9"
    property color surfaceColor: "#ffffff"
    property color elevatedColor: "#f5f7fb"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property color successColor: "#2da65a"

    readonly property var accentPalette: page.system.accentProfiles

    title: i18nc("@title navigation page", "Appearance")
    padding: 0
    background: Rectangle { color: page.pageColor }
    // Plasma may not have answered yet when the Welcome starts at login.
    onVisibleChanged: if (visible && layoutManager && !layoutManager.busy) layoutManager.refresh()

    component Separator: Rectangle {
        Layout.fillWidth: true
        Layout.leftMargin: 54
        implicitHeight: 1
    }

    PageContainer {
        contentSpacing: 12

        Kirigami.Heading { text: i18nc("@title navigation page", "Appearance"); level: 1 }
        Controls.Label {
            Layout.fillWidth: true
            text: i18n("Customize the colors, transparency and arrangement of your desktop.")
            color: page.mutedColor
            wrapMode: Text.WordWrap
        }

        MessageBanner {
            Layout.fillWidth: true
            text: page.system.lastMessage
            dark: page.dark
            accent: page.accent
            textColor: page.textColor
            error: page.system.lastMessageIsError === true
        }

        Controls.Label {
            Layout.topMargin: 12
            text: i18n("System Appearance")
            color: page.mutedColor
            font.pixelSize: 12
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: systemColumn.implicitHeight
            radius: 12
            color: page.surfaceColor
            border.color: page.borderColor

            ColumnLayout {
                id: systemColumn
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0

                SettingRow {
                    Layout.fillWidth: true
                    title: i18n("Visual Style")
                    subtitle: page.system.visualStyleAvailable
                              ? i18n("Changes the Plasma style and keeps the Mainuan color and your selected icons.")
                              : i18n("Available only on KDE Plasma.")
                    iconName: "preferences-desktop-effects"
                    accent: page.accent; textColor: page.textColor; mutedColor: page.mutedColor
                    compactWidth: 720

                    SegmentedChoice {
                        enabled: page.system.visualStyleAvailable && !page.system.busy
                        groupName: i18n("Visual Style")
                        current: page.system.visualStyle
                        availableValues: page.system.installedVisualStyles
                        options: [
                            { value: "blur", label: i18nc("@option visual style", "Blurred"), icon: "blur", tooltip: i18n("Dream theme: translucent panels with blur") },
                            { value: "glass", label: i18nc("@option visual style", "Glass"), icon: "window", tooltip: i18n("Tahoe theme: light panels with a glass effect") },
                            { value: "solid", label: i18nc("@option visual style", "Solid"), icon: "object-fill", tooltip: i18n("Breeze theme: the default KDE look, without transparency") }
                        ]
                        onActivated: (value) => page.system.setVisualStyle(value)
                    }
                }

                Separator { color: page.borderColor }

                SettingRow {
                    Layout.fillWidth: true
                    title: i18n("System Theme")
                    subtitle: i18n("Light or dark colors across the whole system.")
                    iconName: "preferences-desktop-theme"
                    accent: page.accent; textColor: page.textColor; mutedColor: page.mutedColor

                    SegmentedChoice {
                        enabled: !page.system.busy
                        groupName: i18n("System Theme")
                        current: page.dark ? "dark" : "light"
                        options: [
                            { value: "light", label: i18nc("@option color theme", "Light"), icon: "weather-clear" },
                            { value: "dark", label: i18nc("@option color theme", "Dark"), icon: "weather-clear-night" }
                        ]
                        onActivated: (value) => page.system.setTheme(value)
                    }
                }

                Separator { color: page.borderColor }

                SettingRow {
                    Layout.fillWidth: true
                    title: i18n("Accent Color")
                    subtitle: i18n("Applies the KDE color scheme, the Kora icons, the wallpaper and the avatar. Keeps the choice when you change style or theme.")
                    iconName: "color-picker"
                    accent: page.accent; textColor: page.textColor; mutedColor: page.mutedColor

                    Row {
                        spacing: 8
                        Accessible.role: Accessible.Grouping
                        Accessible.name: i18n("Accent Color")
                        Repeater {
                            model: page.accentPalette
                            delegate: Controls.AbstractButton {
                                id: swatch
                                required property var modelData
                                readonly property bool selected: page.system.accentId === modelData.id
                                implicitWidth: 30
                                implicitHeight: 30
                                enabled: !page.system.busy
                                focusPolicy: Qt.StrongFocus
                                checkable: true
                                checked: selected
                                // As in SegmentedChoice: AT-SPI toggles `checked` without clicked().
                                onCheckedChanged: {
                                    if (checked === selected)
                                        return;
                                    const requested = checked;
                                    checked = Qt.binding(() => selected);
                                    if (requested)
                                        page.system.setAccent(modelData.id);
                                }
                                Accessible.role: Accessible.RadioButton
                                // i18n: %1 is the accent color name
                                Accessible.name: i18n("Accent color %1", modelData.name)
                                Controls.ToolTip.visible: hovered
                                Controls.ToolTip.text: modelData.name
                                Controls.ToolTip.delay: 400

                                background: Rectangle {
                                    radius: width / 2
                                    color: swatch.modelData.color
                                    opacity: swatch.enabled ? 1 : 0.5
                                    border.width: swatch.selected || swatch.visualFocus ? 2 : 1
                                    border.color: swatch.selected || swatch.visualFocus ? page.textColor
                                                                                        : Qt.rgba(0, 0, 0, 0.18)
                                    scale: swatch.hovered && !swatch.selected ? 1.08 : 1
                                    Behavior on scale { NumberAnimation { duration: 120 } }
                                }
                                contentItem: Kirigami.Icon {
                                    source: "checkmark"
                                    color: "#ffffff"
                                    visible: swatch.selected
                                    anchors.centerIn: parent
                                    width: 16
                                    height: 16
                                    Accessible.ignored: true
                                }
                            }
                        }
                    }
                }
            }
        }

        Controls.Label {
            Layout.topMargin: 16
            text: i18n("Desktop Layout")
            color: page.mutedColor
            font.pixelSize: 12
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }
        Controls.Label {
            Layout.fillWidth: true
            text: page.layoutManager.available
                  ? i18n("Choose how the panels are placed on the screen. Before each change the Welcome keeps a copy of the current layout.")
                  : i18n("Layouts can only be applied inside a KDE Plasma session.")
            color: page.mutedColor
            wrapMode: Text.WordWrap
        }

        MessageBanner {
            Layout.fillWidth: true
            text: page.layoutManager.message
            error: page.layoutManager.messageIsError
            dark: page.dark
            accent: page.accent
            textColor: page.textColor
        }

        GridLayout {
            id: layoutGrid
            Layout.fillWidth: true
            readonly property int cardMinimum: 240
            columns: Math.max(1, Math.min(3, Math.floor((width + columnSpacing) / (cardMinimum + columnSpacing))))
            columnSpacing: 14
            rowSpacing: 14

            Repeater {
                model: page.layouts
                delegate: Rectangle {
                    id: layoutCard
                    objectName: "layoutCard-" + layoutId
                    required property string layoutId
                    required property string name
                    required property string description
                    readonly property bool active: page.layoutManager.activeLayout === layoutId

                    Layout.fillWidth: true
                    Layout.fillHeight: true // cards of a row share one height
                    Layout.preferredWidth: layoutGrid.cardMinimum
                    implicitHeight: cardColumn.implicitHeight + 28
                    radius: 14
                    color: page.surfaceColor
                    border.width: active ? 2 : 1
                    border.color: active ? page.accent
                                         : (cardHover.hovered ? Qt.rgba(page.accent.r, page.accent.g, page.accent.b, 0.45) : page.borderColor)
                    Behavior on border.color { ColorAnimation { duration: 120 } }
                    HoverHandler { id: cardHover }

                    ColumnLayout {
                        id: cardColumn
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        LayoutPreview {
                            Layout.fillWidth: true
                            layoutId: layoutCard.layoutId
                            accent: page.accent
                            panelColor: page.dark ? "#d8dde6" : "#4f5a70"
                            borderColor: page.borderColor
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Controls.Label {
                                Layout.fillWidth: true
                                text: layoutCard.name
                                color: page.textColor
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Kirigami.Icon {
                                visible: layoutCard.active
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                source: "checkmark"
                                color: page.successColor
                                Accessible.ignored: true
                            }
                            Controls.Label {
                                visible: layoutCard.active
                                text: i18nc("@info layout status", "In use")
                                color: page.successColor
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(implicitHeight, 2 * fontMetrics.height)
                            text: layoutCard.description
                            color: page.mutedColor
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            FontMetrics { id: fontMetrics; font.pixelSize: 12 }
                        }
                        Item { Layout.fillHeight: true }
                        Controls.Button {
                            objectName: "applyLayout-" + layoutCard.layoutId
                            Layout.alignment: Qt.AlignRight
                            text: layoutCard.active ? i18nc("@action:button layout already applied", "Applied") : i18nc("@action:button", "Apply")
                            icon.name: layoutCard.active ? "checkmark" : "dialog-ok-apply"
                            enabled: page.layoutManager.available && !page.layoutManager.busy && !layoutCard.active
                            onClicked: page.layoutManager.apply(layoutCard.layoutId)
                            Accessible.name: layoutCard.active ? i18n("Layout in use: %1", layoutCard.name) : i18n("Apply layout %1", layoutCard.name)
                            Accessible.description: layoutCard.description
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: page.layoutManager.hasBackup
            spacing: 10
            Controls.Label {
                Layout.fillWidth: true
                text: i18n("Returns to the layout that was in use before the last change. Plasma restarts for a few seconds.")
                color: page.mutedColor
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Controls.Button {
                text: i18n("Restore Previous Layout")
                icon.name: "edit-undo"
                enabled: !page.layoutManager.busy
                onClicked: restoreDialog.open()
            }
        }
    }

    Kirigami.PromptDialog {
        id: restoreDialog
        objectName: "restoreLayoutDialog"
        parent: page.Controls.Overlay.overlay
        preferredWidth: Kirigami.Units.gridUnit * 28
        title: i18n("Restore previous layout?")
        subtitle: i18n("The panels go back to how they were before the last layout change. Plasma will restart for a few seconds.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Restore")
                icon.name: "edit-undo"
                onTriggered: {
                    restoreDialog.close();
                    page.layoutManager.restoreBackup();
                }
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: restoreDialog.close()
            }
        ]
    }
}
