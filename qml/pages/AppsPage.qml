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
            text: "Flatpak não foi encontrado. Os aplicativos Flatpak estão indisponíveis. O FreeOffice e os serviços online continuam independentes do Flatpak."
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

        MessageBanner {
            Layout.fillWidth: true
            text: page.system.lastMessage
            dark: page.dark
            accent: page.accent
            textColor: page.textColor
            error: text.indexOf("Erro:") === 0
        }

        Flow {
            id: cardFlow
            objectName: "applicationFlow"
            readonly property int columns: Math.max(1, Math.min(3, Math.floor((width + spacing) / (220 + spacing))))
            Layout.fillWidth: true
            Layout.preferredHeight: Math.ceil(appRepeater.count / columns) * 300
                                    + Math.max(0, Math.ceil(appRepeater.count / columns) - 1) * spacing
            Layout.minimumHeight: Layout.preferredHeight
            spacing: 14
            Repeater {
                id: appRepeater
                model: page.appModel
                delegate: AppCard {
                    width: Math.max(0, Math.min(310, (cardFlow.width - (cardFlow.columns - 1) * cardFlow.spacing) / cardFlow.columns))
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
                    actionType: model.actionType
                    packageId: model.packageId
                    installProfile: model.installProfile
                    installerBusy: page.installer.busy
                    onInstallRequested: (profileId) => page.installRequested(profileId)
                }
            }
        }
        Item { Layout.preferredHeight: 20 }
    }
}
