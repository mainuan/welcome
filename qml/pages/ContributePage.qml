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
    readonly property var links: system.projectLinks || ({})
    // Address the browser could not open; shown so it can be copied.
    property string failedUrl: ""

    function openLink(url) {
        failedUrl = page.system.openUrl(url) ? "" : url
    }

    title: i18nc("@title navigation page", "Contribute")
    padding: 0
    background: Rectangle { color: page.pageColor }

    Timer {
        id: copiedTimer
        interval: 2500
    }
    Timer {
        id: addressCopiedTimer
        interval: 2500
    }

    PageContainer {
        contentSpacing: 14

        Kirigami.Heading { text: i18nc("@title navigation page", "Contribute"); level: 1 }
        Controls.Label {
            Layout.fillWidth: true
            text: i18n("Mainuan is made by its community. You can help with a donation, with code or by reporting problems.")
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
                        // i18n: Pix is the Brazilian instant payment system; %1 is the Pix key
                        Accessible.name: page.system.pixQrIsPayload ? i18n("Pix QR code. Key: %1", page.system.pixKey)
                                                                    : i18n("Pix key QR code. Key: %1", page.system.pixKey)
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
                            text: i18n("Support Mainuan")
                            level: 2
                            color: page.textColor
                        }
                    }
                    Controls.Label {
                        Layout.fillWidth: true
                        text: i18n("Your donation helps sustain development, infrastructure and the community.")
                        color: page.mutedColor
                        wrapMode: Text.WordWrap
                    }

                    Controls.Label {
                        Layout.topMargin: 6
                        text: i18n("Pix Key")
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
                            Accessible.name: i18n("Pix Key")
                        }
                    }
                    RowLayout {
                        visible: page.hasPix
                        spacing: 10
                        Controls.Button {
                            text: i18n("Copy Pix Key")
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
                            text: i18n("Pix key copied.")
                            color: page.successColor
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.StaticText
                        }
                    }
                    Controls.Label {
                        visible: page.hasPix
                        Layout.fillWidth: true
                        text: page.system.pixQrIsPayload
                              ? i18n("Scan the QR code with your bank's app, using the Pix option.")
                              : i18n("The QR code contains the Pix key. In your bank's app, use the Pix option with the copied key or the one read from the code.")
                        color: page.mutedColor
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    Controls.Label {
                        visible: !page.hasPix
                        Layout.fillWidth: true
                        text: i18n("The Pix key has not been set up on this installation yet.")
                        color: page.mutedColor
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Controls.Label {
            Layout.topMargin: 6
            text: i18n("Other Ways to Help")
            color: page.mutedColor
            font.pixelSize: 12
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }
        ActionCard {
            objectName: "contributeCodeCard"
            Layout.fillWidth: true
            iconName: "github"
            title: i18n("Contribute Code")
            description: i18n("Code, translations and improvements on GitHub.")
            accent: page.accent; surfaceColor: page.surfaceColor; borderColor: page.borderColor
            textColor: page.textColor; mutedColor: page.mutedColor
            Controls.ToolTip.visible: hovered && page.links.code !== undefined
            Controls.ToolTip.text: page.links.code || ""
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: page.openLink(page.links.code)
        }
        ActionCard {
            objectName: "reportProblemCard"
            Layout.fillWidth: true
            iconName: "bug"
            title: i18n("Report a Problem")
            description: i18n("Describe what happened; attaching the system report (in About) helps a lot.")
            accent: page.accent; surfaceColor: page.surfaceColor; borderColor: page.borderColor
            textColor: page.textColor; mutedColor: page.mutedColor
            Controls.ToolTip.visible: hovered && page.links.issues !== undefined
            Controls.ToolTip.text: page.links.issues || ""
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: page.openLink(page.links.issues)
        }
        ActionCard {
            objectName: "communityCard"
            Layout.fillWidth: true
            iconName: "users"
            title: i18n("Telegram Community")
            description: i18n("Chat with Mainuan users and developers.")
            accent: page.accent; surfaceColor: page.surfaceColor; borderColor: page.borderColor
            textColor: page.textColor; mutedColor: page.mutedColor
            Controls.ToolTip.visible: hovered && page.links.community !== undefined
            Controls.ToolTip.text: page.links.community || ""
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: page.openLink(page.links.community)
        }
        // The browser did not open: show the address so it can be copied.
        Rectangle {
            objectName: "openUrlFailure"
            visible: page.failedUrl.length > 0
            Layout.fillWidth: true
            implicitHeight: failureLayout.implicitHeight + 24
            radius: 10
            color: Qt.rgba(0.79, 0.27, 0.27, page.dark ? 0.15 : 0.10)
            border.color: Qt.rgba(0.79, 0.27, 0.27, 0.35)
            ColumnLayout {
                id: failureLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Controls.Label {
                    Layout.fillWidth: true
                    text: i18n("The web browser could not be opened. Copy the address below and open it in your browser:")
                    color: page.textColor
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.AlertMessage
                }
                TextEdit {
                    objectName: "failedUrlText"
                    Layout.fillWidth: true
                    text: page.failedUrl
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: page.textColor
                    selectionColor: page.accent
                    font.pixelSize: 14
                    Accessible.role: Accessible.EditableText
                    Accessible.name: i18n("Page address")
                }
                RowLayout {
                    spacing: 10
                    Controls.Button {
                        objectName: "copyFailedUrl"
                        text: i18n("Copy Address")
                        icon.name: "edit-copy"
                        onClicked: if (page.system.copyUrl(page.failedUrl)) addressCopiedTimer.restart()
                    }
                    Controls.Label {
                        visible: addressCopiedTimer.running
                        text: i18n("Address copied.")
                        color: page.successColor
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.StaticText
                    }
                }
            }
        }
        Controls.Label {
            Layout.fillWidth: true
            text: i18n("Links open in the browser only when you click them. The QR code is generated on this computer, without internet access.")
            color: page.mutedColor
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
