import QtQuick
import QtQuick.Controls as Controls
import QtTest
import "components" as App

Item {
    id: window
    width: 1120
    height: 900
    visible: true
    Component.onCompleted: {
        const icons = ["freeOffice.png", "vivaldi.png", "zenBrowser.png"]
        for (let row = 0; row < icons.length; ++row)
            apps.setProperty(row, "iconSource", Qt.resolvedUrl("components/" + icons[row]).toString())
    }
    QtObject {
        id: mockSystem
        property string lastMessage: ""
        property int urls: 0
        function openUrl(url) { urls++ }
        function openDiscover(id) { fail("Unexpected Discover invocation") }
    }
    QtObject {
        id: mockPackages
        property bool flatpakAvailable: true
        property string lastMessage: ""
    }
    QtObject {
        id: mockInstaller
        property bool busy: false
        property string profileId: ""
        property string phase: "idle"
        property int percent: -1
        property int stepCount: 1
        property int step: 1
        property string message: "Preparando"
        property string resultMessage: "Falha simulada"
        property string details: "Detalhes simulados"
        property var profiles: ({freeoffice: {canLaunch: false}})
        property int starts: 0
        property string actionLabel: ""
        property bool launchAvailable: false
        property int launches: 0
        function profileInfo(id) {
            return {name: "FreeOffice", source: "SoftMaker", description: "Descrição longa que deve ficar oculta", actionLabel: actionLabel,
                    sourceNotice: "Origem: SoftMaker. Scripts executados como administrador.",
                    iconSource: Qt.resolvedUrl("components/freeOffice.png").toString()}
        }
        function start(id) { starts++; profileId = id; phase = "preparing"; busy = true; return true }
        function reset() { busy = false; phase = "idle"; profileId = "" }
        function launch(id) { launches++ }
    }
    ListModel {
        id: apps
        ListElement { name: "FreeOffice"; description: "Suíte"; iconSource: "components/freeOffice.png"; installed: false; busy: false; available: true; installState: "Disponível"; webUrl: ""; actionType: "debian"; packageId: "softmaker-freeoffice-2024"; installProfile: "freeoffice" }
        ListElement { name: "Vivaldi"; description: "Navegador"; iconSource: "components/vivaldi.png"; installed: false; busy: false; available: true; installState: "Disponível"; webUrl: ""; actionType: "flatpak"; packageId: "com.vivaldi.Vivaldi"; installProfile: "app:com.vivaldi.Vivaldi" }
        ListElement { name: "Zen Browser"; description: "Navegador"; iconSource: "components/zenBrowser.png"; installed: false; busy: false; available: true; installState: "Disponível"; webUrl: ""; actionType: "flatpak"; packageId: "app.zen_browser.zen"; installProfile: "app:app.zen_browser.zen" }
        ListElement { name: "Google Docs"; description: "Online"; iconSource: ""; installed: false; busy: false; available: true; installState: "Online"; webUrl: "https://docs.google.com/"; actionType: "web"; packageId: ""; installProfile: "" }
    }
    App.AppsPage {
        id: page
        anchors.fill: parent
        appModel: apps
        pkgService: mockPackages
        installer: mockInstaller
        system: mockSystem
        property string requested: ""
        onInstallRequested: (id) => requested = id
    }
    App.InstallDialog {
        id: dialog
        installer: mockInstaller
        parent: window
        surfaceColor: dark ? "#171b23" : "#ffffff"
        textColor: dark ? "#f1f4f8" : "#20252d"
        mutedColor: dark ? "#aeb8c8" : "#657080"
        successColor: dark ? "#7fdda5" : "#258b4b"
    }
    TestCase {
        name: "ApplicationInstallation"
        when: windowShown
        function cleanup() {
            mockInstaller.reset()
            dialog.close()
            tryCompare(dialog, "opened", false)
            mockInstaller.actionLabel = ""
            mockInstaller.launchAvailable = false
            mockInstaller.profiles = {freeoffice: {canLaunch: false}}
            mockInstaller.percent = -1
            mockInstaller.step = 1
            mockInstaller.stepCount = 1
            mockInstaller.message = "Preparando"
            mockInstaller.resultMessage = "Falha simulada"
            mockInstaller.details = "Detalhes simulados"
        }
        function test_cardsUseProfilesAndPreserveIcons() {
            const profiles = ["freeoffice", "app:com.vivaldi.Vivaldi", "app:app.zen_browser.zen"]
            for (let row = 0; row < 3; ++row) {
                const card = findChild(page, "applicationCard-" + row)
                verify(card !== null)
                const button = findChild(card, "installApplicationButton")
                compare(button.text, "Instalar")
                compare(button.Accessible.name, apps.get(row).name + ": Instalar")
                verify(button.enabled)
                button.clicked()
                compare(page.requested, profiles[row])
                verify(card.appIcon.endsWith(row === 0 ? "freeOffice.png" : row === 1 ? "vivaldi.png" : "zenBrowser.png"))
                tryCompare(findChild(card, "applicationArtwork"), "status", Image.Ready)
                mockInstaller.busy = true
                verify(!button.enabled)
                mockInstaller.busy = false
            }
            apps.setProperty(0, "installed", true)
            const freeButton = findChild(findChild(page, "applicationCard-0"), "installApplicationButton")
            compare(freeButton.text, "Instalado")
            verify(!freeButton.enabled)
            apps.setProperty(0, "installed", false)
            const onlineButton = findChild(findChild(page, "applicationCard-3"), "installApplicationButton")
            compare(onlineButton.text, "Abrir")
            onlineButton.clicked()
            compare(mockSystem.urls, 1)
        }
        function test_vendorConsentProgressAndRetry() {
            const previousStarts = mockInstaller.starts
            dialog.openFor("freeoffice")
            tryCompare(dialog, "opened", true)
            verify(dialog.awaitingConfirmation)
            compare(mockInstaller.starts, previousStarts)
            const notice = findChild(dialog.contentItem, "installationSourceNotice")
            verify(notice.text.includes("SoftMaker") && notice.text.includes("administrador"))
            const confirmation = findChild(dialog.contentItem, "confirmVendorInstallation")
            verify(confirmation.visible)
            confirmation.clicked()
            compare(mockInstaller.starts, previousStarts + 1)
            verify(dialog.running)
            verify(!notice.visible)
            mockInstaller.phase = "downloading"
            mockInstaller.percent = 35
            dialog.requestClose()
            verify(dialog.opened)
            verify(dialog.closeBlocked)
            mockInstaller.phase = "error"
            mockInstaller.busy = false
            verify(dialog.failed)
            const retry = findChild(dialog.contentItem, "retryInstallation")
            verify(retry.visible)
            retry.clicked()
            compare(mockInstaller.starts, previousStarts + 2)
            verify(dialog.running)
            mockInstaller.phase = "error"
            mockInstaller.busy = false
            dialog.requestClose()
            tryCompare(dialog, "opened", false)
        }
        function descendants(item) {
            let items = []
            const children = item.children || []
            for (let i = 0; i < children.length; ++i) {
                items.push(children[i])
                items = items.concat(descendants(children[i]))
            }
            return items
        }
        function startSimulatedJob() {
            dialog.openFor("freeoffice")
            tryCompare(dialog, "opened", true)
            findChild(dialog.contentItem, "confirmVendorInstallation").clicked()
            verify(dialog.running)
        }
        function test_minimalProgress_data() {
            return [{tag: "light", dark: false}, {tag: "dark", dark: true}]
        }
        function test_minimalProgress(data) {
            dialog.dark = data.dark
            mockInstaller.step = 2
            mockInstaller.stepCount = 3
            mockInstaller.message = "pmstatus:libcurl4:35:Configuring dependency"
            startSimulatedJob()
            const title = findChild(dialog.contentItem, "installationTitle")
            const source = findChild(dialog.contentItem, "installationSource")
            const progress = findChild(dialog.contentItem, "installationProgress")
            const percentage = findChild(dialog.contentItem, "installationPercentage")
            const safety = findChild(dialog.contentItem, "installationSafetyNotice")
            const details = findChild(dialog.contentItem, "installationDetailsPanel")
            compare(title.text, "Instalando FreeOffice")
            compare(source.text, "Origem: SoftMaker")
            compare(source.Accessible.name, source.text)
            compare(dialog.contentItem.Accessible.role, Accessible.Dialog)
            compare(dialog.contentItem.Accessible.name, title.text)
            compare(safety.text, "Por segurança, a instalação não pode ser cancelada.")
            verify(safety.visible)
            verify(safety.color.r !== dialog.surfaceColor.r)
            const bars = descendants(dialog.contentItem).filter(item => item instanceof Controls.ProgressBar)
            compare(bars.length, 1)
            verify(progress.indeterminate)
            verify(!percentage.visible)
            compare(progress.Accessible.description, "Em andamento")
            const visibleText = descendants(dialog.contentItem).filter(item => item.visible && typeof item.text === "string").map(item => item.text)
            verify(!visibleText.some(text => text.includes("Etapa") || text.includes("libcurl4") || text.includes("pmstatus:")
                || text.includes("Descrição longa") || text.includes("Scripts executados")))
            verify(!details.visible)
            verify(!findChild(dialog.contentItem, "installationDetailsToggle").visible)
            for (const value of [0, 18, 67, 99]) {
                mockInstaller.percent = value
                verify(!progress.indeterminate)
                tryCompare(progress, "value", value)
                verify(percentage.visible)
                compare(percentage.text, value + "%")
                compare(progress.Accessible.description, value + "%")
            }
            mockInstaller.percent = 100 // Download complete, installation still running.
            verify(progress.indeterminate)
            verify(!percentage.visible)
            compare(progress.Accessible.description, "Em andamento")
            mockInstaller.percent = -1
            verify(progress.indeterminate)
            dialog.requestClose()
            verify(dialog.opened)
            compare(safety.text, "Por segurança, a instalação não pode ser cancelada.")
            mockInstaller.phase = "success"
            mockInstaller.busy = false
            verify(dialog.succeeded)
            verify(!progress.indeterminate)
            tryCompare(progress, "value", 100)
            compare(percentage.text, "100%")
            compare(findChild(dialog.contentItem, "installationResultMessage").text, "Instalação concluída com sucesso.")
            compare(title.text, "FreeOffice")
            verify(findChild(dialog.contentItem, "installationResultIcon").visible)
            verify(!safety.visible)
            verify(findChild(dialog.contentItem, "closeInstallation").visible)
        }
        function test_errorsAndDetails_data() {
            return [{tag: "light", dark: false}, {tag: "dark", dark: true}]
        }
        function test_errorsAndDetails(data) {
            dialog.dark = data.dark
            mockInstaller.details = "E: dpkg failed\npmerror:dependency:42:technical failure"
            mockInstaller.resultMessage = "Verifique sua conexão e tente novamente."
            startSimulatedJob()
            mockInstaller.phase = "error"
            mockInstaller.busy = false
            compare(findChild(dialog.contentItem, "installationResultMessage").text, mockInstaller.resultMessage)
            verify(findChild(dialog.contentItem, "retryInstallation").visible)
            verify(findChild(dialog.contentItem, "closeInstallation").visible)
            const panel = findChild(dialog.contentItem, "installationDetailsPanel")
            const toggle = findChild(dialog.contentItem, "installationDetailsToggle")
            verify(!panel.visible)
            verify(toggle.visible)
            compare(toggle.text, "Ver detalhes")
            toggle.clicked()
            verify(panel.visible)
            compare(findChild(dialog.contentItem, "installationTechnicalDetails").text, mockInstaller.details)
            toggle.clicked()
            verify(!panel.visible)
            toggle.clicked()
            findChild(dialog.contentItem, "retryInstallation").clicked()
            verify(dialog.running)
            verify(!panel.visible)
            verify(!dialog.showDetails)
        }
        function test_successActions_data() {
            return [{tag: "open", action: "Abrir"}, {tag: "configure", action: "Configurar"}, {tag: "none", action: ""}]
        }
        function test_successActions(data) {
            mockInstaller.actionLabel = data.action
            mockInstaller.profiles = {freeoffice: {canLaunch: data.action.length > 0}}
            startSimulatedJob()
            mockInstaller.phase = "success"
            mockInstaller.busy = false
            const action = findChild(dialog.contentItem, "installationResultAction")
            compare(action.visible, data.action.length > 0)
            if (data.action.length > 0) {
                compare(action.text, data.action)
                const previous = mockInstaller.launches
                action.clicked()
                compare(mockInstaller.launches, previous + 1)
            }
        }
    }
}
