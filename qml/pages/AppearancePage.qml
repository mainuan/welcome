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

    readonly property var accentPalette: [
        { color: "#d08040", name: "Laranja" },
        { color: "#e8177d", name: "Rosa" },
        { color: "#3daee9", name: "Azul" },
        { color: "#3dd425", name: "Verde" },
        { color: "#aab6b9", name: "Cinza" },
        { color: "#a588cb", name: "Lilás" }
    ]

    title: "Aparência"
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

        Kirigami.Heading { text: "Aparência"; level: 1 }
        Controls.Label {
            Layout.fillWidth: true
            text: "Personalize as cores, a transparência e a disposição da sua área de trabalho."
            color: page.mutedColor
            wrapMode: Text.WordWrap
        }

        MessageBanner {
            Layout.fillWidth: true
            text: page.system.lastMessage
            dark: page.dark
            accent: page.accent
            textColor: page.textColor
        }

        Controls.Label {
            Layout.topMargin: 12
            text: "Aparência do sistema"
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
                    title: "Estilo visual"
                    subtitle: page.system.visualStyleAvailable
                              ? "Muda o visual completo do Plasma: painéis, janelas, ícones e papel de parede."
                              : "Disponível apenas no KDE Plasma."
                    iconName: "preferences-desktop-effects"
                    accent: page.accent; textColor: page.textColor; mutedColor: page.mutedColor
                    compactWidth: 720

                    SegmentedChoice {
                        enabled: page.system.visualStyleAvailable && !page.system.busy
                        groupName: "Estilo visual"
                        current: page.system.visualStyle
                        availableValues: page.system.installedVisualStyles
                        options: [
                            { value: "blur", label: "Desfocado", icon: "blur", tooltip: "Tema Dream: painéis translúcidos com desfoque" },
                            { value: "glass", label: "Vítreo", icon: "window", tooltip: "Tema Tahoe: painéis claros com efeito de vidro" },
                            { value: "solid", label: "Sólido", icon: "object-fill", tooltip: "Tema Breeze: o visual padrão do KDE, sem transparência" }
                        ]
                        onActivated: (value) => page.system.setVisualStyle(value)
                    }
                }

                Separator { color: page.borderColor }

                SettingRow {
                    Layout.fillWidth: true
                    title: "Tema do sistema"
                    subtitle: "Cores claras ou escuras em todo o sistema."
                    iconName: "preferences-desktop-theme"
                    accent: page.accent; textColor: page.textColor; mutedColor: page.mutedColor

                    SegmentedChoice {
                        enabled: !page.system.busy
                        groupName: "Tema do sistema"
                        current: page.dark ? "dark" : "light"
                        options: [
                            { value: "light", label: "Claro", icon: "weather-clear" },
                            { value: "dark", label: "Escuro", icon: "weather-clear-night" }
                        ]
                        onActivated: (value) => page.system.setTheme(value)
                    }
                }

                Separator { color: page.borderColor }

                SettingRow {
                    Layout.fillWidth: true
                    title: "Cor de destaque"
                    subtitle: "Cor usada em botões, seleções e elementos ativos. A cor escolhida é mantida ao trocar o estilo visual."
                    iconName: "color-picker"
                    accent: page.accent; textColor: page.textColor; mutedColor: page.mutedColor

                    Row {
                        spacing: 8
                        Accessible.role: Accessible.Grouping
                        Accessible.name: "Cor de destaque"
                        Repeater {
                            model: page.accentPalette
                            delegate: Controls.AbstractButton {
                                id: swatch
                                required property var modelData
                                readonly property bool selected: page.accent.toString().toLowerCase() === modelData.color
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
                                        page.system.setAccent(modelData.color);
                                }
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: "Cor de destaque " + modelData.name
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
            text: "Layout do desktop"
            color: page.mutedColor
            font.pixelSize: 12
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }
        Controls.Label {
            Layout.fillWidth: true
            text: page.layoutManager.available
                  ? "Escolha como os painéis ficam na tela. Antes de cada troca o Welcome guarda uma cópia do layout atual."
                  : "Os layouts só podem ser aplicados dentro de uma sessão do KDE Plasma."
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
                                text: "Em uso"
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
                            Layout.alignment: Qt.AlignRight
                            text: layoutCard.active ? "Aplicado" : "Aplicar"
                            icon.name: layoutCard.active ? "checkmark" : "dialog-ok-apply"
                            enabled: page.layoutManager.available && !page.layoutManager.busy && !layoutCard.active
                            onClicked: page.layoutManager.apply(layoutCard.layoutId)
                            Accessible.name: (layoutCard.active ? "Layout em uso: " : "Aplicar layout ") + layoutCard.name
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
                text: "Volta ao layout que estava em uso antes da última troca. O Plasma é reiniciado por alguns segundos."
                color: page.mutedColor
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Controls.Button {
                text: "Restaurar layout anterior"
                icon.name: "edit-undo"
                enabled: !page.layoutManager.busy
                onClicked: restoreDialog.open()
            }
        }
    }

    Kirigami.PromptDialog {
        id: restoreDialog
        parent: page.Controls.Overlay.overlay
        preferredWidth: Kirigami.Units.gridUnit * 28
        title: "Restaurar layout anterior?"
        subtitle: "Os painéis voltam a ser como estavam antes da última troca de layout. O Plasma será reiniciado por alguns segundos."
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: "Restaurar"
                icon.name: "edit-undo"
                onTriggered: {
                    restoreDialog.close();
                    page.layoutManager.restoreBackup();
                }
            },
            Kirigami.Action {
                text: "Cancelar"
                icon.name: "dialog-cancel"
                onTriggered: restoreDialog.close()
            }
        ]
    }
}
