import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    visible: false
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    title: "Mainuan — Bem-vindo"
    property bool dark: systemService.darkTheme
    property color accent: systemService.accentColor.length > 0 ? systemService.accentColor : "#3daee9"
    // Kirigami supplies the controls and icon theme; these explicit surfaces keep
    // contrast stable on Plasma sessions whose QPA palette is not synchronized.
    property color pageColor: dark ? "#1b1f26" : "#f2f4f7"
    property color surfaceColor: dark ? "#242a33" : "#ffffff"
    property color elevatedColor: dark ? "#2d3440" : "#f7f8fa"
    property color borderColor: dark ? "#3d4653" : "#d6dbe3"
    property color textColor: dark ? "#f1f4f8" : "#20252d"
    property color mutedColor: dark ? "#aeb8c8" : "#657080"
    property color successColor: dark ? "#7fdda5" : "#258b4b"
    property color warningColor: dark ? "#f2c46d" : "#94600b"
    property color errorColor: dark ? "#ff8f87" : "#c0392b"
    property int currentPage: 0
    property bool sidebarCollapsed: root.width < 1060

    background: Rectangle { color: root.pageColor }

    // A privileged installation cannot be interrupted safely: keep the window
    // open and show the dialog explaining why.
    onClosing: (close) => {
        if (installService.busy) {
            close.accepted = false;
            if (!installDialog.opened)
                installDialog.openFor(installService.profileId);
            installDialog.requestClose();
        }
    }

    function openInstallDialog(profileId) {
        installDialog.openFor(profileId);
    }

    Connections {
        target: installService
        function onLaunchFailed(message) { root.showPassiveNotification(message) }
    }

    Connections {
        target: startupPreference
        function onFailed(message) { root.showPassiveNotification(message) }
    }

    InstallDialog {
        id: installDialog
        installer: installService
        dark: root.dark
        accent: root.accent
        surfaceColor: root.surfaceColor
        elevatedColor: root.elevatedColor
        borderColor: root.borderColor
        textColor: root.textColor
        mutedColor: root.mutedColor
        successColor: root.successColor
        errorColor: root.errorColor
    }

    ListModel {
        id: navigationModel
        ListElement { label: "Início"; iconName: "house" }
        ListElement { label: "Aparência"; iconName: "palette" }
        ListElement { label: "Office"; iconName: "briefcase" }
        ListElement { label: "Navegadores"; iconName: "globe" }
        ListElement { label: "Tutoriais"; iconName: "circle-play" }
        ListElement { label: "Sobre"; iconName: "info" }
        ListElement { label: "Contribuir"; iconName: "heart" }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: root.sidebarCollapsed ? 76 : 232
            color: dark ? "#20252d" : "#e7ebf1"
            border.color: root.borderColor
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
                // Official logo above the menu; a themed icon stands in if the file is missing.
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.sidebarCollapsed ? 52 : 92
                    Layout.topMargin: 6
                    Layout.bottomMargin: 10
                    Image {
                        id: logo
                        anchors.centerIn: parent
                        width: Math.min(parent.width - (root.sidebarCollapsed ? 4 : 32), root.sidebarCollapsed ? 48 : 168)
                        height: parent.height
                        source: systemService.logoSource
                        fillMode: Image.PreserveAspectFit
                        sourceSize.height: 2 * parent.height
                        asynchronous: true
                        smooth: true
                        mipmap: true
                        visible: status === Image.Ready
                        Accessible.role: Accessible.Graphic
                        Accessible.name: "Mainuan"
                    }
                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: root.sidebarCollapsed ? 32 : 48
                        height: width
                        visible: logo.status !== Image.Ready
                        source: "start-here-kde-plasma"
                        fallback: "start-here"
                        Accessible.name: "Mainuan"
                    }
                }
                ListView {
                    id: navigation
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: navigationModel
                    spacing: 4
                    interactive: false
                    delegate: SidebarButton {
                        width: navigation.width
                        collapsed: root.sidebarCollapsed
                        dark: root.dark
                        accent: root.accent
                        mutedColor: root.mutedColor
                        textColor: root.textColor
                        label: model.label
                        iconName: model.iconName
                        selected: root.currentPage === index
                        onClicked: root.currentPage = index
                    }
                }
                Controls.Label {
                    Layout.fillWidth: true
                    text: root.sidebarCollapsed ? "" : "Plasma " + (systemService.plasmaVersion.length > 0 ? systemService.plasmaVersion.replace("plasmashell ", "") : "6")
                    color: root.mutedColor
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                height: 58
                color: root.pageColor
                border.color: root.borderColor
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    Controls.Label { text: navigationModel.get(root.currentPage).label; color: root.textColor; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    Controls.CheckBox {
                        id: startupCheck
                        text: "Mostrar esta tela ao iniciar o sistema"
                        checked: startupPreference.showAtStartup
                        // onToggled is not emitted for accessibility actions (screen readers),
                        // so react to every user change of `checked`.
                        onCheckedChanged: {
                            if (checked === startupPreference.showAtStartup)
                                return;
                            startupPreference.showAtStartup = checked;
                            // A toggle replaces the binding; follow the stored preference again,
                            // which also reverts the box if saving failed.
                            checked = Qt.binding(() => startupPreference.showAtStartup);
                        }
                        Accessible.description: "Abre esta tela automaticamente quando você entra na sua sessão"
                    }
                }
            }
            StackLayout {
                id: pages
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentPage
                HomePage { system: systemService; installer: installService; dark: root.dark; accent: root.accent; pageColor: root.pageColor; surfaceColor: root.surfaceColor; elevatedColor: root.elevatedColor; borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor; successColor: root.successColor; warningColor: root.warningColor; onInstallRequested: (profileId) => root.openInstallDialog(profileId) }
                AppearancePage { system: systemService; layouts: layoutModel; layoutManager: layoutService; dark: root.dark; accent: root.accent; pageColor: root.pageColor; surfaceColor: root.surfaceColor; elevatedColor: root.elevatedColor; borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor; successColor: root.successColor }
                AppsPage { appModel: officeModel; pkgService: packageService; installer: installService; system: systemService; onInstallRequested: (profileId) => root.openInstallDialog(profileId); pageTitle: "Office"; pageDescription: "Instale ferramentas de produtividade ou abra serviços online."; dark: root.dark; accent: root.accent; pageColor: root.pageColor; surfaceColor: root.surfaceColor; elevatedColor: root.elevatedColor; borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor; successColor: root.successColor }
                AppsPage { appModel: browserModel; pkgService: packageService; installer: installService; system: systemService; onInstallRequested: (profileId) => root.openInstallDialog(profileId); pageTitle: "Navegadores"; pageDescription: "Escolha seu navegador preferido para navegar na web."; dark: root.dark; accent: root.accent; pageColor: root.pageColor; surfaceColor: root.surfaceColor; elevatedColor: root.elevatedColor; borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor; successColor: root.successColor }
                TutorialsPage { tutorialModel: videoModel; hostWindow: root; dark: root.dark; accent: root.accent; pageColor: root.pageColor; surfaceColor: root.surfaceColor; elevatedColor: root.elevatedColor; borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor }
                AboutPage { system: systemService; report: systemReport; dark: root.dark; accent: root.accent; pageColor: root.pageColor; surfaceColor: root.surfaceColor; elevatedColor: root.elevatedColor; borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor; successColor: root.successColor }
                ContributePage { system: systemService; dark: root.dark; accent: root.accent; pageColor: root.pageColor; surfaceColor: root.surfaceColor; borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor; successColor: root.successColor }
            }
        }
    }
}
