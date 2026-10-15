import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    property var appModel
    property var pkgService
    property var installer
    property var system
    property string pageTitle: ""
    property string pageDescription: ""
    property bool dark: false
    property color accent: "#3daee9"
    property color pageColor: "#f3f5f9"
    property color surfaceColor: "#ffffff"
    property color elevatedColor: "#f5f7fb"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property color successColor: "#2da65a"

    signal installRequested(string profileId)

    title: page.pageTitle
    padding: 0
    background: Rectangle { color: page.pageColor }

    PageContainer {
        contentSpacing: 14

        Kirigami.Heading { text: page.pageTitle; level: 1 }
        Controls.Label {
            Layout.fillWidth: true
            text: page.pageDescription
            color: page.mutedColor
            wrapMode: Text.WordWrap
        }
        Controls.Label {
            visible: !page.pkgService.flatpakAvailable
            Layout.fillWidth: true
            text: "Flatpak não foi encontrado. A detecção e a instalação estão indisponíveis neste sistema."
            color: page.mutedColor
            wrapMode: Text.WordWrap
        }
        Controls.Label {
            visible: page.pkgService.lastMessage.length > 0
            Layout.fillWidth: true
            text: page.pkgService.lastMessage
            color: page.pkgService.lastMessage.indexOf("Falha") === 0 ? "#c94444" : page.mutedColor
            wrapMode: Text.WordWrap
        }

        Flow {
            id: cardFlow
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(300, Math.ceil(appRepeater.count / 3) * 300 + Math.max(0, Math.ceil(appRepeater.count / 3) - 1) * 14)
            Layout.minimumHeight: Layout.preferredHeight
            spacing: 14
            Repeater {
                id: appRepeater
                model: page.appModel
                delegate: AppCard {
                    width: Math.max(220, Math.min(310, (cardFlow.width - 28) / 3))
                    appModel: page.appModel
                    system: page.system
                    row: index
                    dark: page.dark
                    accent: page.accent
                    surfaceColor: page.surfaceColor
                    elevatedColor: page.elevatedColor
                    borderColor: page.borderColor
                    textColor: page.textColor
                    mutedColor: page.mutedColor
                    successColor: page.successColor
                    appName: model.name
                    appDescription: model.description
                    appIcon: model.iconSource
                    installed: model.installed
                    busy: model.busy
                    available: model.available
                    installState: model.installState
                    webUrl: model.webUrl
                    installProfile: model.installProfile
                    installerBusy: page.installer.busy
                    onInstallRequested: (profileId) => page.installRequested(profileId)
                }
            }
        }
        Item { Layout.preferredHeight: 20 }
    }
}
