import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    property var system
    property var installer
    property bool dark: false
    property color accent: "#3daee9"
    property color pageColor: "#f3f5f9"
    property color surfaceColor: "#ffffff"
    property color elevatedColor: "#f5f7fb"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property color successColor: "#2da65a"
    property color warningColor: "#b7791f"

    signal installRequested(string profileId)

    title: "Início"
    padding: 0

    background: Rectangle { color: pageColor }

    // Row backed by an InstallService profile: status badge, "Instalar" while
    // something is missing and an optional configuration action.
    component ProfileRow: StatusRow {
        // Inline components do not see the ids of this file.
        property var host
        property string profileId: ""
        property string configureText: ""
        property string installedNote: ""
        readonly property var profileState: host.installer.profiles[profileId] || ({})
        readonly property bool installedNow: profileState.state === "installed"
        readonly property bool runningNow: profileState.running === true

        Layout.fillWidth: true
        surfaceColor: host.surfaceColor; borderColor: host.borderColor; accent: host.accent
        textColor: host.textColor; mutedColor: host.mutedColor
        successColor: host.successColor; warningColor: host.warningColor

        statusText: {
            if (runningNow)
                return "Instalando…";
            if (installedNow)
                return installedNote.length > 0 ? "Instalado · " + installedNote : "Instalado";
            if (profileState.canInstall === false && (profileState.unavailableReason || "").length > 0)
                return "Indisponível neste sistema";
            return profileState.summary || "";
        }
        statusKind: installedNow ? "ok" : (profileState.state === "partial" ? "warn" : "neutral")
        statusTooltip: installedNow ? "" : (profileState.unavailableReason || "")

        primaryText: installedNow ? "" : (runningNow ? "Instalando…" : "Instalar")
        primaryIcon: "download"
        primaryEnabled: profileState.canInstall === true && !host.installer.busy
        onPrimaryClicked: host.installRequested(profileId)

        secondaryText: configureText
        secondaryIcon: "configure"
        secondaryEnabled: profileState.canLaunch === true
        onSecondaryClicked: host.installer.launch(profileId)
    }

    component SectionLabel: Controls.Label {
        Layout.topMargin: 12
        font.pixelSize: 12
        font.weight: Font.DemiBold
        Accessible.role: Accessible.Heading
    }

    PageContainer {
        contentSpacing: 12

        Kirigami.Heading {
            Layout.fillWidth: true
            text: "Bem-vindo ao <font color='" + page.accent + "'>Mainuan</font>"
            level: 1
        }
        Controls.Label {
            Layout.fillWidth: true
            text: "Configure seu sistema à sua maneira, com ferramentas nativas do Plasma."
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

        SectionLabel { text: "Hardware & Mídia"; color: page.mutedColor }

        StatusRow {
            Layout.fillWidth: true
            surfaceColor: page.surfaceColor; borderColor: page.borderColor; accent: page.accent
            textColor: page.textColor; mutedColor: page.mutedColor
            successColor: page.successColor; warningColor: page.warningColor
            title: "Drivers adicionais"
            description: "Gerencie drivers proprietários de vídeo, Wi-Fi e outros dispositivos."
            iconName: "computer"
            statusText: page.system.driversAvailable ? "" : "Gerenciador indisponível"
            primaryText: "Gerenciar"
            primaryIcon: "configure"
            primaryHighlighted: false
            primaryEnabled: page.system.driversAvailable
            onPrimaryClicked: page.system.performAction("drivers")
        }
        ProfileRow {
            host: page
            profileId: "codecs"
            title: "Codecs de mídia"
            description: "Reproduza áudio e vídeo nos formatos mais comuns."
            iconName: "applications-multimedia"
        }

        SectionLabel { text: "Segurança & Privacidade"; color: page.mutedColor }

        ProfileRow {
            host: page
            profileId: "antivirus"
            title: "Antivírus"
            description: "Proteção ClamAV com a interface gráfica ClamUI."
            iconName: "security-high"
            configureText: "Configurar"
        }
        ProfileRow {
            host: page
            profileId: "firewall"
            title: "Firewall"
            description: "Controle as conexões de rede com o UFW e o módulo do KDE Plasma."
            iconName: "security-medium"
            configureText: "Configurar"
            installedNote: page.system.firewallStatus
        }

        Controls.Label {
            Layout.topMargin: 4
            Layout.bottomMargin: 20
            text: "Sessão: " + page.system.sessionType + (page.system.plasmaVersion.length > 0 ? "  ·  " + page.system.plasmaVersion : "")
            color: page.mutedColor
            font.pixelSize: 11
        }
    }
}
