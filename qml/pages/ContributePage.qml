import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    property var system
    property bool dark: false
    property color accent: "#3daee9"
    property color pageColor: "#f3f5f9"
    property color surfaceColor: "#ffffff"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property color successColor: "#2da65a"

    readonly property bool hasPix: system.pixKey.length > 0

    title: "Contribuir"
    padding: 0
    background: Rectangle { color: page.pageColor }

    Timer {
        id: copiedTimer
        interval: 2500
    }

    PageContainer {
        contentSpacing: 14

        Kirigami.Heading { text: "Contribuir"; level: 1 }
        Controls.Label {
            Layout.fillWidth: true
            text: "O Mainuan é feito em comunidade. Você pode ajudar com uma doação, com código ou relatando problemas."
            color: page.mutedColor
            wrapMode: Text.WordWrap
        }

        // Pix
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: pixLayout.implicitHeight + 40
            radius: 16
            color: page.surfaceColor
            border.color: page.borderColor
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: page.surfaceColor }
                GradientStop { position: 1; color: Qt.rgba(page.accent.r, page.accent.g, page.accent.b, page.dark ? 0.20 : 0.12) }
            }

            GridLayout {
                id: pixLayout
                anchors.fill: parent
                anchors.margins: 20
                columns: width > 560 ? 2 : 1
                columnSpacing: 24
                rowSpacing: 16

                // The QR code is rendered locally from the configured key or payload.
                Rectangle {
                    visible: page.hasPix && page.system.pixQrSource.length > 0
                    Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
                    Layout.preferredWidth: 196
                    Layout.preferredHeight: 196
                    radius: 12
                    color: "#ffffff"
                    border.color: page.borderColor
                    Image {
                        anchors.fill: parent
                        anchors.margins: 8
                        source: page.system.pixQrSource
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        Accessible.role: Accessible.Graphic
                        Accessible.name: (page.system.pixQrIsPayload ? "QR Code Pix" : "QR Code da chave Pix")
                                         + ". Chave: " + page.system.pixKey
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    RowLayout {
                        spacing: 10
                        Kirigami.Icon {
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            source: "qrc:/icons/heart.svg"
                            isMask: true
                            color: page.accent
                            Accessible.ignored: true
                        }
                        Kirigami.Heading {
                            text: "Apoie o Mainuan"
                            level: 2
                            color: page.textColor
                        }
                    }
                    Controls.Label {
                        Layout.fillWidth: true
                        text: "Sua doação ajuda a manter o desenvolvimento, a infraestrutura e a comunidade."
                        color: page.mutedColor
                        wrapMode: Text.WordWrap
                    }

                    Controls.Label {
                        Layout.topMargin: 6
                        text: "Chave Pix"
                        color: page.mutedColor
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        visible: page.hasPix
                    }
                    Rectangle {
                        visible: page.hasPix
                        Layout.fillWidth: true
                        implicitHeight: keyText.implicitHeight + 20
                        radius: 8
                        color: page.pageColor
                        border.color: page.borderColor
                        TextEdit {
                            id: keyText
                            anchors.fill: parent
                            anchors.margins: 10
                            text: page.system.pixKey
                            readOnly: true
                            selectByMouse: true
                            color: page.textColor
                            selectionColor: page.accent
                            font.pixelSize: 15
                            verticalAlignment: Text.AlignVCenter
                            Accessible.role: Accessible.EditableText
                            Accessible.name: "Chave Pix"
                        }
                    }
                    RowLayout {
                        visible: page.hasPix
                        spacing: 10
                        Controls.Button {
                            text: "Copiar chave Pix"
                            icon.name: "edit-copy"
                            highlighted: true
                            onClicked: if (page.system.copyText(page.system.pixKey)) copiedTimer.restart()
                        }
                        Kirigami.Icon {
                            visible: copiedTimer.running
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            source: "checkmark"
                            color: page.successColor
                            Accessible.ignored: true
                        }
                        Controls.Label {
                            visible: copiedTimer.running
                            text: "Chave Pix copiada."
                            color: page.successColor
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.StaticText
                        }
                    }
                    Controls.Label {
                        visible: page.hasPix
                        Layout.fillWidth: true
                        text: page.system.pixQrIsPayload
                              ? "Leia o QR Code no aplicativo do seu banco, na opção Pix."
                              : "O QR Code contém a chave Pix. No aplicativo do banco, use a opção Pix com a chave copiada ou lida pelo código."
                        color: page.mutedColor
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    Controls.Label {
                        visible: !page.hasPix
                        Layout.fillWidth: true
                        text: "A chave Pix ainda não foi configurada nesta instalação."
                        color: page.mutedColor
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Controls.Label {
            Layout.topMargin: 6
            text: "Outras formas de ajudar"
            color: page.mutedColor
            font.pixelSize: 12
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }
        ActionCard {
            Layout.fillWidth: true
            iconName: "github"
            title: "Contribuir com código"
            description: "Código, traduções e melhorias no GitHub."
            accent: page.accent; surfaceColor: page.surfaceColor; borderColor: page.borderColor
            textColor: page.textColor; mutedColor: page.mutedColor
            onClicked: page.system.openUrl("https://github.com/mainuanos/mainuan")
        }
        ActionCard {
            Layout.fillWidth: true
            iconName: "bug"
            title: "Reportar um problema"
            description: "Descreva o que aconteceu; anexar o relatório da máquina (em Sobre) ajuda muito."
            accent: page.accent; surfaceColor: page.surfaceColor; borderColor: page.borderColor
            textColor: page.textColor; mutedColor: page.mutedColor
            onClicked: page.system.openUrl("https://github.com/mainuanos/mainuan/issues/new")
        }
        ActionCard {
            Layout.fillWidth: true
            iconName: "users"
            title: "Comunidade no Telegram"
            description: "Converse com usuários e desenvolvedores do Mainuan."
            accent: page.accent; surfaceColor: page.surfaceColor; borderColor: page.borderColor
            textColor: page.textColor; mutedColor: page.mutedColor
            onClicked: page.system.openUrl("https://t.me/mainuanos")
        }
        Controls.Label {
            Layout.fillWidth: true
            text: "Os links abrem no navegador só quando você clica. O QR Code é gerado no próprio computador, sem internet."
            color: page.mutedColor
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
