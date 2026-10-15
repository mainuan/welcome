import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    property var system
    property var report
    property bool dark: false
    property color accent: "#3daee9"
    property color pageColor: "#f3f5f9"
    property color surfaceColor: "#ffffff"
    property color elevatedColor: "#f5f7fb"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property color successColor: "#2da65a"

    readonly property var summary: report ? report.summary : ({})

    title: "Sobre"
    padding: 0
    background: Rectangle { color: page.pageColor }

    // Collected the first time the page is shown, never at startup.
    onVisibleChanged: if (visible && !report.ready && !report.busy) report.refresh()

    component Chip: Rectangle {
        property string text: ""
        property color tone: "#3daee9"
        property color textColor: "#202532"
        visible: text.length > 0
        implicitWidth: chipLabel.implicitWidth + 20
        implicitHeight: chipLabel.implicitHeight + 8
        radius: height / 2
        color: Qt.rgba(tone.r, tone.g, tone.b, 0.14)
        Controls.Label {
            id: chipLabel
            anchors.centerIn: parent
            text: parent.text
            color: parent.textColor
            font.pixelSize: 12
        }
    }

    PageContainer {
        contentSpacing: 14

        // Hero
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: hero.implicitHeight + 40
            radius: 16
            color: page.surfaceColor
            border.color: page.borderColor
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Qt.rgba(page.accent.r, page.accent.g, page.accent.b, page.dark ? 0.22 : 0.14) }
                GradientStop { position: 0.6; color: page.surfaceColor }
            }

            RowLayout {
                id: hero
                anchors.fill: parent
                anchors.margins: 20
                spacing: 22

                Image {
                    id: heroLogo
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 112
                    source: page.system.logoSource
                    visible: status === Image.Ready
                    fillMode: Image.PreserveAspectFit
                    sourceSize.width: 224
                    sourceSize.height: 224
                    smooth: true
                    mipmap: true
                    Accessible.role: Accessible.Graphic
                    Accessible.name: "Logotipo do Mainuan"
                }
                Kirigami.Icon {
                    visible: !heroLogo.visible
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 96
                    source: "start-here-kde-plasma"
                    fallback: "start-here"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Kirigami.Heading {
                        text: "Mainuan"
                        level: 1
                        color: page.textColor
                    }
                    Controls.Label {
                        Layout.fillWidth: true
                        text: page.summary.version ? "Mainuan " + page.summary.version : (page.summary.os || "")
                        color: page.mutedColor
                        wrapMode: Text.WordWrap
                    }
                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        spacing: 6
                        Chip { text: page.summary.plasma || ""; tone: page.accent; textColor: page.textColor }
                        Chip { text: page.summary.kernel || ""; tone: page.accent; textColor: page.textColor }
                        Chip { text: page.summary.architecture || ""; tone: page.accent; textColor: page.textColor }
                    }
                    RowLayout {
                        Layout.topMargin: 8
                        spacing: 8
                        Controls.Button {
                            text: "Relatório da máquina"
                            icon.name: "view-list-details"
                            highlighted: true
                            onClicked: reportDialog.open()
                            Accessible.description: "Abre as informações de hardware e software para suporte técnico"
                        }
                        Controls.Button {
                            text: "Repositório"
                            icon.name: "internet-web-browser"
                            onClicked: page.system.openUrl("https://github.com/ruscher/welcome5")
                        }
                    }
                }
            }
        }

        Controls.Label {
            Layout.topMargin: 6
            text: "Este computador"
            color: page.mutedColor
            font.pixelSize: 12
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width > 560 ? 2 : 1
            columnSpacing: 12
            rowSpacing: 12

            Repeater {
                model: [
                    { icon: "cpu", label: "Processador", value: page.summary.cpu },
                    { icon: "monitor", label: "Gráficos", value: page.summary.gpu },
                    { icon: "memory-stick", label: "Memória em uso", value: page.summary.memory },
                    { icon: "hard-drive", label: "Armazenamento (sistema)", value: page.summary.storage },
                    { icon: "calendar-clock", label: page.summary.installEstimated ? "Instalação (estimada)" : "Instalação", value: page.summary.installDate },
                    { icon: "power", label: "Ligado há", value: page.summary.uptime }
                ]
                delegate: InfoTile {
                    required property var modelData
                    Layout.fillWidth: true
                    icon: modelData.icon
                    label: modelData.label
                    value: page.report.ready ? (modelData.value || "") : "…"
                    accent: page.accent
                    surfaceColor: page.surfaceColor
                    borderColor: page.borderColor
                    textColor: page.textColor
                    mutedColor: page.mutedColor
                }
            }
        }

        Controls.Label {
            Layout.topMargin: 6
            text: "Créditos"
            color: page.mutedColor
            font.pixelSize: 12
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }
        StatusRow {
            Layout.fillWidth: true
            surfaceColor: page.surfaceColor; borderColor: page.borderColor; accent: page.accent
            textColor: page.textColor; mutedColor: page.mutedColor
            title: "KDE Plasma"
            description: "Ambiente de desktop e componentes de integração."
            iconName: "kde"
            primaryText: "kde.org"
            primaryIcon: "internet-web-browser"
            primaryHighlighted: false
            onPrimaryClicked: page.system.openUrl("https://kde.org")
        }
        StatusRow {
            Layout.fillWidth: true
            surfaceColor: page.surfaceColor; borderColor: page.borderColor; accent: page.accent
            textColor: page.textColor; mutedColor: page.mutedColor
            title: "Ubuntu / Kubuntu"
            description: "Base da distribuição e repositórios de pacotes."
            iconName: "distributor-logo-ubuntu"
            primaryText: "ubuntu.com"
            primaryIcon: "internet-web-browser"
            primaryHighlighted: false
            onPrimaryClicked: page.system.openUrl("https://ubuntu.com")
        }
        Controls.Label {
            Layout.fillWidth: true
            text: "Software livre. A licença e o histórico do projeto estão no repositório."
            color: page.mutedColor
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }

    ReportDialog {
        id: reportDialog
        report: page.report
        logoSource: page.system.logoSource
        dark: page.dark
        accent: page.accent
        surfaceColor: page.surfaceColor
        elevatedColor: page.elevatedColor
        borderColor: page.borderColor
        textColor: page.textColor
        mutedColor: page.mutedColor
        successColor: page.successColor
    }
}
