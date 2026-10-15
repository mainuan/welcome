import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

Rectangle {
    id: card

    property var appModel
    property var system
    property int row: -1
    property string appName: ""
    property string appDescription: ""
    property string appIcon: ""
    property bool installed: false
    property bool busy: false
    property bool available: false
    property string installState: ""
    property string webUrl: ""
    property string installProfile: ""
    property bool installerBusy: false
    property bool dark: false
    property color accent: "#3daee9"
    property color surfaceColor: "#ffffff"
    property color elevatedColor: "#f5f7fb"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property color successColor: "#2da65a"

    signal installRequested(string profileId)

    implicitHeight: 300
    radius: 14
    color: surfaceColor
    border.color: mouse.containsMouse ? Qt.rgba(accent.r, accent.g, accent.b, 0.65) : borderColor
    border.width: mouse.containsMouse ? 2 : 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 9

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 154
            radius: 10
            color: elevatedColor

            Image {
                anchors.fill: parent
                anchors.margins: 13
                source: card.appIcon
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                sourceSize.width: 220
                sourceSize.height: 220
                mipmap: true
            }
        }

        Controls.Label {
            Layout.fillWidth: true
            text: card.appName
            color: textColor
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Controls.Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: card.appDescription
            color: mutedColor
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            Controls.Label {
                Layout.fillWidth: true
                text: card.installState
                color: card.installed ? successColor : mutedColor
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Controls.Button {
                Layout.preferredWidth: card.webUrl.length > 0 ? 78 : 102
                Layout.minimumWidth: card.webUrl.length > 0 ? 72 : 94
                text: card.webUrl.length > 0 ? "Abrir" : (card.busy ? "Aguarde…" : (card.installed ? "Remover" : "Instalar"))
                icon.name: card.webUrl.length > 0 ? "internet-web-browser" : (card.installed ? "edit-delete" : "download")
                enabled: card.available && !card.busy && (card.webUrl.length > 0 || card.installed || !card.installerBusy)
                highlighted: !card.installed
                onClicked: {
                    if (card.webUrl.length > 0)
                        card.system.openUrl(card.webUrl);
                    else if (card.installed)
                        card.appModel.remove(card.row);
                    else
                        card.installRequested(card.installProfile);
                }
                Accessible.name: card.appName + ": " + text
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        z: -1
    }
}
