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

    title: i18nc("@title navigation page", "Home")
    padding: 0

    background: Rectangle { color: pageColor }

    // Row backed by an InstallService profile: status badge, "Instalar" while
    // something is missing and an optional configuration action.
    component ProfileRow: StatusRow {
        objectName: "profileRow-" + profileId
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
                return i18n("Installing…");
            if (installedNow)
                // i18n: %1 is a short note such as "startup: enabled"
                return installedNote.length > 0 ? i18n("Installed · %1", installedNote) : i18nc("@info status", "Installed");
            if (profileState.canInstall === false && (profileState.unavailableReason || "").length > 0)
                return i18n("Unavailable on this system");
            if (profileState.canInstall === true
                    && (profileState.state === "missing" || profileState.state === "partial"))
                return "";
            return profileState.summary || "";
        }
        statusKind: installedNow ? "ok" : (profileState.state === "partial" ? "warn" : "neutral")
        statusTooltip: installedNow ? "" : (profileState.unavailableReason || "")

        primaryText: installedNow ? "" : (runningNow ? i18n("Installing…") : i18nc("@action:button", "Install"))
        primaryIcon: "download"
        primaryEnabled: profileState.canInstall === true && !host.installer.busy
        onPrimaryClicked: host.installRequested(profileId)

        secondaryText: installedNow && profileState.canLaunch === true && !runningNow ? configureText : ""
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
            objectName: "welcomeGreeting"
            Layout.fillWidth: true
            // i18n: %1 is a colour code; keep the <font> markup around the product name
            text: i18n("Welcome to <font color='%1'>Mainuan</font>", String(page.accent))
            level: 1
        }
        Controls.Label {
            Layout.fillWidth: true
            text: i18n("Set up your system your way, with native Plasma tools.")
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

        SectionLabel { text: i18n("Hardware & Media"); color: page.mutedColor }

        StatusRow {
            objectName: "driversRow"
            Layout.fillWidth: true
            surfaceColor: page.surfaceColor; borderColor: page.borderColor; accent: page.accent
            textColor: page.textColor; mutedColor: page.mutedColor
            successColor: page.successColor; warningColor: page.warningColor
            title: i18n("Additional Drivers")
            description: i18n("View and manage the drivers available for your computer.")
            iconName: "computer"
            statusText: page.system.driversAvailable ? "" : page.system.driversStatus
            primaryText: i18nc("@action:button", "Manage")
            primaryIcon: "configure"
            primaryHighlighted: false
            primaryEnabled: page.system.driversAvailable
            onPrimaryClicked: page.system.performAction("drivers")
        }
        ProfileRow {
            host: page
            profileId: "codecs"
            title: i18n("Media Codecs")
            description: i18n("Play audio and video in the most common formats.")
            iconName: "applications-multimedia"
        }

        SectionLabel { text: i18n("Security & Privacy"); color: page.mutedColor }

        ProfileRow {
            host: page
            profileId: "antivirus"
            title: i18n("Antivirus")
            description: i18n("ClamAV protection with the ClamUI graphical interface.")
            iconName: "security-high"
            configureText: i18nc("@action:button", "Configure")
        }
        ProfileRow {
            host: page
            profileId: "firewall"
            title: i18n("Firewall")
            description: i18n("Control network connections with UFW and the KDE Plasma module.")
            iconName: "security-medium"
            configureText: i18nc("@action:button", "Configure")
            // The service reads ufw.conf's boot preference, not live protection.
            // i18n: %1 is the firewall boot preference, e.g. "enabled"/"disabled"
            installedNote: page.system.firewallStatus.length > 0 ? i18n("startup: %1", page.system.firewallStatus.toLowerCase()) : ""
        }

        Controls.Label {
            Layout.topMargin: 4
            Layout.bottomMargin: 20
            // i18n: %1 is the session type (wayland/x11), %2 the Plasma version
            text: page.system.plasmaVersion.length > 0 ? i18n("Session: %1  ·  %2", page.system.sessionType, page.system.plasmaVersion)
                                                       : i18n("Session: %1", page.system.sessionType)
            color: page.mutedColor
            font.pixelSize: 11
        }
    }
}
