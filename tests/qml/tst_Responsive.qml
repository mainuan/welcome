import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Window
import QtTest

Item {
    width: 1920
    height: 1080
    visible: true
    QtObject {
        id: systemService
        property bool darkTheme: false
        property string accentColor: "#3daee9"
        property string accentId: "cyan"
        property string colorScheme: "DreamCyanLightColor"
        property var accentProfiles: [
            {id: "brown", name: "Marrom", color: "#d08040"}, {id: "cyan", name: "Ciano", color: "#3daee9"},
            {id: "magenta", name: "Magenta", color: "#e8177d"}, {id: "lime", name: "Lima", color: "#3dd425"},
            {id: "gray", name: "Cinza", color: "#aab6b9"}, {id: "violet", name: "Violeta", color: "#a588cb"}
        ]
        property bool busy: false
        property string lastMessage: ""
        property string visualStyle: "blur"
        property bool visualStyleAvailable: true
        property var installedVisualStyles: ["blur", "glass", "solid"]
        property string driversStatus: "Gestão de Drivers não instalada"
        property bool driversAvailable: false
        property string firewallStatus: "Inativo"
        property string sessionType: "wayland"
        property string plasmaVersion: "plasmashell 6.6"
        property string logoSource: ""
        property string pixKey: "test@example.org"
        property string pixQrSource: ""
        property bool pixQrIsPayload: false
        function setTheme(theme) {}
        function setAccent(id) {}
        function setVisualStyle(id) {}
        function openUrl(url) {}
        function copyText(text) { return true }
        function performAction(action) {}
    }
    QtObject {
        id: installService
        property bool busy: false
        property string profileId: ""
        property string phase: "idle"
        property int percent: -1
        property string resultMessage: "Não foi possível concluir a instalação. Tente novamente."
        property string details: "Detalhes de diagnóstico\n".repeat(30)
        property var profiles: ({antivirus: {state: "missing", canInstall: true, summary: "Não instalado"},
                                  firewall: {state: "partial", canInstall: true, summary: "Falta: UFW"},
                                  codecs: {state: "installed", canInstall: false}})
        signal launchFailed(string message)
        function profileInfo(id) { return {name: "FreeOffice", source: "SoftMaker", sourceNotice: "Este pacote é fornecido pela SoftMaker e poderá solicitar autorização de administrador."} }
        function reset() { busy = false; phase = "idle"; profileId = "" }
        function start(id) { profileId = id; phase = "preparing"; busy = true }
        function launch(id) {}
    }
    QtObject { id: startupPreference; property bool showAtStartup: true; signal failed(string message) }
    QtObject { id: packageService; property bool flatpakAvailable: true; property string lastMessage: "" }
    QtObject {
        id: layoutService
        property bool busy: false
        property bool available: true
        property bool hasBackup: true
        property string activeLayout: "plasma-default"
        property string message: ""
        property bool messageIsError: false
        function refresh() {}
        function apply(id) {}
        function restorePrevious() {}
    }
    QtObject {
        id: systemReport
        property bool busy: false
        property bool ready: true
        property var summary: ({os: "Mainuan", plasma: "Plasma 6", kernel: "Kernel 7.0", architecture: "x86_64", cpu: "CPU de teste", memory: "8 GiB"})
        property var sections: []
        function refresh() {}
        function copyReport() {}
        function saveReport(url) { return true }
    }
    ListModel { id: officeModel; function remove(row) {} }
    ListModel { id: browserModel; function remove(row) {} }
    ListModel { id: layoutModel }
    ListModel { id: videoModel }

    TestCase {
        id: tests
        name: "ResponsiveWelcome"
        when: windowShown
        property var welcome: null
        function initTestCase() {
            const ids = ["plasma-default", "three-panels", "single-bar", "tiling", "panel-top", "unity", "minimal"]
            for (let i = 0; i < ids.length; ++i)
                layoutModel.append({layoutId: ids[i], name: "Layout " + ids[i], description: "Descrição do layout e das opções disponíveis."})
            for (let i = 0; i < 9; ++i) {
                const entry = {name: "Aplicativo " + i, description: "Descrição do programa com texto suficiente para validar a distribuição dos cartões.",
                    iconSource: "", installed: false, busy: false, available: true, installState: "Disponível", webUrl: "",
                    actionType: "flatpak", packageId: "org.example.App" + i, installProfile: "app:org.example.App" + i}
                officeModel.append(entry)
                browserModel.append(entry)
                videoModel.append({title: "Tutorial " + i, fileName: "tutorial.mp4", fileSource: "", iconSource: "", available: false})
            }
            const component = Qt.createComponent("components/WelcomeWindow.qml")
            compare(component.status, Component.Ready, component.errorString())
            welcome = component.createObject(null)
            verify(welcome !== null)
            compare(welcome.width, 870)
            compare(welcome.height, 600)
            compare(welcome.minimumWidth, 760)
            compare(welcome.minimumHeight, 520)
            verify(welcome.maximumWidth > 1920)
            verify(welcome.maximumHeight > 1080)
            // The native startup policy lowers minima only for a smaller screen;
            // allow equivalent small logical viewports in this isolated harness.
            welcome.minimumWidth = 0
            welcome.minimumHeight = 0
            welcome.show()
            wait(50)
        }
        function cleanupTestCase() { if (welcome) { welcome.close(); welcome.destroy() } }
        function descendants(item) {
            let found = []
            for (const child of item.children || []) found = found.concat([child], descendants(child))
            return found
        }
        function test_pages_data() {
            let rows = []
            for (const size of [[870,600], [1024,768], [1280,720], [1366,768], [1920,1080], [760,520], [616,320]]) {
                for (const dark of [false, true]) rows.push({tag: size[0] + "x" + size[1] + "-" + dark, w: size[0], h: size[1], dark: dark})
            }
            return rows
        }
        function test_pages(data) {
            welcome.width = data.w
            welcome.height = data.h
            tryCompare(welcome, "width", data.w)
            tryCompare(welcome, "height", data.h)
            systemService.darkTheme = data.dark
            tryCompare(welcome, "sidebarCollapsed", data.w < 1060)
            for (let page = 0; page < 7; ++page) {
                welcome.currentPage = page
                wait(15)
                const scrolls = descendants(welcome.contentItem).filter(item => item.objectName === "pageScrollView" && item.visible)
                compare(scrolls.length, 1, "One visible page: " + page)
                const scroll = scrolls[0]
                verify(scroll.availableWidth > 0)
                verify(scroll.contentWidth <= scroll.availableWidth + 1, "Horizontal scroll on page " + page)
                const flows = descendants(scroll).filter(item => item.objectName === "applicationFlow" || item.objectName === "tutorialFlow")
                for (const flow of flows) {
                    verify(flow.height >= flow.childrenRect.height - 1, "Flow height must contain every wrapped row")
                    verify(flow.childrenRect.width <= flow.width + 1, "Cards must fit the viewport")
                }
                const buttons = descendants(scroll).filter(item => item instanceof Controls.Button && item.visible && item.width > 0)
                for (const button of buttons) {
                    const point = button.mapToItem(scroll, 0, 0)
                    verify(point.x >= -1 && point.x + button.width <= scroll.width + 1,
                           "Button clipped horizontally: " + page + ": " + button.text)
                    verify(button.height >= button.implicitHeight - 1, "Button height: " + button.text)
                }
                if (scroll.contentHeight > scroll.availableHeight) {
                    scroll.contentItem.contentY = scroll.contentHeight - scroll.availableHeight
                    verify(scroll.contentItem.contentY > 0, "Page can scroll to the last row")
                    scroll.contentItem.contentY = 0
                }
            }
            // Resizing back must restore a complete application grid.
            welcome.width = 870
            welcome.height = 600
        }
        function test_installationFitsSmallWindow() {
            welcome.width = 760
            welcome.height = 320
            tryCompare(welcome, "height", 320)
            welcome.openInstallDialog("freeoffice")
            // Popups are QObject children rather than ordinary visual children.
            const title = findChild(welcome, "installationTitle")
            verify(title !== null)
            const popup = findChild(welcome, "installDialog")
            verify(popup !== null)
            tryCompare(popup, "opened", true)
            verify(popup.height <= welcome.height - 32)
            verify(popup.width <= welcome.width - 48)
            findChild(popup.contentItem, "confirmVendorInstallation").clicked()
            installService.busy = false
            installService.phase = "error"
            findChild(popup.contentItem, "installationDetailsToggle").clicked()
            verify(popup.height <= welcome.height - 32)
            tryVerify(() => popup.contentItem.contentHeight > popup.contentItem.availableHeight)
            popup.contentItem.contentItem.contentY = popup.contentItem.contentHeight - popup.contentItem.availableHeight
            const close = findChild(popup.contentItem, "closeInstallation")
            verify(close.Accessible.name.length > 0 || close.text.length > 0)
            tryVerify(() => close.mapToItem(popup.contentItem, 0, close.height).y <= popup.contentItem.availableHeight + 1)
            close.clicked()
            tryCompare(popup, "opened", false)
            installService.reset()
        }
        function test_windowMaximizesAndRestores() {
            welcome.width = 870
            welcome.height = 600
            tryCompare(welcome, "width", 870)
            tryCompare(welcome, "height", 600)
            welcome.showMaximized()
            tryCompare(welcome, "visibility", Window.Maximized)
            welcome.showNormal()
            tryCompare(welcome, "visibility", Window.Windowed)
            tryCompare(welcome, "width", 870)
            tryCompare(welcome, "height", 600)
        }
        function test_otherDialogsFit() {
            welcome.width = 760
            welcome.height = 520
            for (const entry of [{page: 1, name: "restoreLayoutDialog"}, {page: 5, name: "systemReportDialog"}]) {
                welcome.currentPage = entry.page
                const popup = findChild(welcome, entry.name)
                verify(popup !== null)
                popup.open()
                tryCompare(popup, "opened", true)
                verify(popup.width <= welcome.width)
                verify(popup.height <= welcome.height)
                popup.close()
                tryCompare(popup, "opened", false)
            }
        }
    }
}
