import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    property var tutorialModel
    property bool dark: false
    property color accent: "#3daee9"
    property color pageColor: "#f3f5f9"
    property color surfaceColor: "#ffffff"
    property color elevatedColor: "#f5f7fb"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"

    property bool playerVisible: false
    property string currentTitle: ""
    property var hostWindow

    title: "Tutoriais"
    padding: 0
    background: Rectangle { color: page.pageColor }

    property var mediaPlayer: playerLoader.item ? playerLoader.item.mediaPlayer : null
    property var audioOutput: playerLoader.item ? playerLoader.item.audioOutput : null

    PageContainer {
        contentSpacing: 14

        Kirigami.Heading { text: "Tutoriais em vídeo"; level: 1 }
        Controls.Label {
            Layout.fillWidth: true
            text: "Aprenda o essencial do Mainuan. Vídeos ausentes ficam desativados sem impedir a navegação."
            color: page.mutedColor
            wrapMode: Text.WordWrap
        }

        Rectangle {
            visible: page.playerVisible
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(430, page.height * 0.52)
            color: "#090b10"
            radius: 14
            clip: true
            Loader {
                id: playerLoader
                    anchors.fill: parent
                    active: page.playerVisible
                    sourceComponent: Component {
                        Item {
                        anchors.fill: parent
                        property alias mediaPlayer: mediaPlayerObject
                        property alias audioOutput: audioOutputObject

                        MediaPlayer {
                            id: mediaPlayerObject
                            videoOutput: output
                            audioOutput: audioOutputObject
                            onErrorOccurred: page.playerVisible = true
                        }
                        AudioOutput { id: audioOutputObject }
                        VideoOutput { id: output; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit }
                    }
                }
            }
            Controls.Button {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 10
                text: "Fechar"
                onClicked: { if (page.mediaPlayer) page.mediaPlayer.stop(); page.playerVisible = false }
            }
            Controls.Label {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 14
                text: page.currentTitle
                color: "white"
                font.weight: Font.DemiBold
            }
        }

        RowLayout {
            visible: page.playerVisible
            Layout.fillWidth: true
            Controls.Button { text: page.mediaPlayer && page.mediaPlayer.playbackState === MediaPlayer.PlayingState ? "Pausar" : "Reproduzir"; onClicked: { if (page.mediaPlayer) page.mediaPlayer.playbackState === MediaPlayer.PlayingState ? page.mediaPlayer.pause() : page.mediaPlayer.play() } }
            Controls.Slider {
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, page.mediaPlayer ? page.mediaPlayer.duration : 1)
                value: page.mediaPlayer ? page.mediaPlayer.position : 0
                onMoved: if (page.mediaPlayer) page.mediaPlayer.position = value
            }
            Controls.Slider { implicitWidth: 120; from: 0; to: 1; value: page.audioOutput ? page.audioOutput.volume : 1; onMoved: if (page.audioOutput) page.audioOutput.volume = value; Accessible.name: "Volume" }
            Controls.Button { text: "Tela cheia"; onClicked: page.hostWindow.visibility = page.hostWindow.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen }
        }

        Flow {
            id: videoFlow
            visible: !page.playerVisible
            Layout.fillWidth: true
            Layout.preferredHeight: 550
            Layout.minimumHeight: 550
            spacing: 14
            Repeater {
                id: videoRepeater
                model: page.tutorialModel
                delegate: Rectangle {
                    width: Math.max(220, Math.min(310, (videoFlow.width - 28) / 3))
                    height: 268
                    radius: 14
                    color: page.surfaceColor
                    border.color: model.available ? page.borderColor : Qt.rgba(page.borderColor.r, page.borderColor.g, page.borderColor.b, 0.55)
                    opacity: model.available ? 1 : 0.62

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 9
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 158
                            radius: 10
                            color: page.elevatedColor
                            Image { anchors.fill: parent; anchors.margins: 16; source: model.iconSource; fillMode: Image.PreserveAspectFit; asynchronous: true }
                            Controls.Button {
                                anchors.centerIn: parent
                                text: model.available ? "Reproduzir" : "Indisponível"
                                icon.name: model.available ? "media-playback-start" : "dialog-warning"
                                display: Controls.AbstractButton.TextBesideIcon
                                implicitWidth: model.available ? 126 : 120
                                implicitHeight: 38
                                enabled: model.available
                                onClicked: {
                                    page.currentTitle = model.title
                                    page.playerVisible = true
                                    Qt.callLater(function() {
                                        if (page.mediaPlayer) {
                                            page.mediaPlayer.source = model.fileSource
                                            page.mediaPlayer.play()
                                        }
                                    })
                                }
                                Accessible.name: model.available ? "Reproduzir " + model.title : model.title + ": vídeo ausente"
                            }
                        }
                        Controls.Label { Layout.fillWidth: true; text: model.title; color: page.textColor; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
                        Controls.Label { Layout.fillWidth: true; text: model.available ? model.fileName : "Arquivo não encontrado"; color: page.mutedColor; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideMiddle }
                    }
                }
            }
        }

        Controls.Label {
            visible: !page.playerVisible && videoRepeater.count === 0
            Layout.fillWidth: true
            text: "Nenhum tutorial foi cadastrado."
            color: page.mutedColor
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
