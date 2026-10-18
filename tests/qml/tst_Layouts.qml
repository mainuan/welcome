import QtQuick
import QtQuick.Controls as Controls
import QtTest
import "components" as App

Item {
    id: window
    visible: true
    width: 1120
    height: 760
    QtObject {
        id: mockSystem
        property var accentProfiles: []
        property string lastMessage: ""
        property bool visualStyleAvailable: false
        property bool busy: false
        property string visualStyle: "solid"
        property var installedVisualStyles: []
        property string accentId: ""
    }
    QtObject {
        id: manager
        property string activeLayout: ""
        property bool available: true
        property bool busy: false
        property bool hasBackup: false
        property string message: ""
        property bool messageIsError: false
        property string requested: ""
        function refresh() {}
        function apply(id) { requested = id }
    }
    ListModel {
        id: layoutEntries
        ListElement { layoutId: "plasma-default"; name: "Clássico — 5 barras"; description: "Cinco painéis" }
        ListElement { layoutId: "three-panels"; name: "3 barras"; description: "Três painéis" }
        ListElement { layoutId: "single-bar"; name: "Barra única"; description: "Barra inferior flutuante" }
        ListElement { layoutId: "tiling"; name: "Tiling"; description: "Blocos KWin" }
        ListElement { layoutId: "panel-top"; name: "Painel superior"; description: "TahoeLauncher" }
        ListElement { layoutId: "unity"; name: "Lomiri"; description: "Orbit.Start.Menu" }
        ListElement { layoutId: "minimal"; name: "Barra W7"; description: "Barra não flutuante" }
    }
    App.AppearancePage {
        id: page
        anchors.fill: parent
        system: mockSystem
        layouts: layoutEntries
        layoutManager: manager
    }
    TestCase {
        name: "LayoutCards"
        when: windowShown
        function test_cardsOrderingNamesAndAccessibility() {
            compare(layoutEntries.count, 7)
            compare(findChild(page, "layoutCard-floating"), null)
            let previous = null
            for (let i = 0; i < layoutEntries.count; ++i) {
                const entry = layoutEntries.get(i)
                const card = findChild(page, "layoutCard-" + entry.layoutId)
                const button = findChild(page, "applyLayout-" + entry.layoutId)
                verify(card !== null)
                verify(button !== null)
                compare(card.name, entry.name)
                compare(button.Accessible.name, "Aplicar layout " + entry.name)
                compare(button.Accessible.description, entry.description)
                verify(button.enabled)
                if (previous !== null)
                    verify(card.y > previous.y || (card.y === previous.y && card.x > previous.x))
                button.clicked()
                compare(manager.requested, entry.layoutId)
                manager.activeLayout = entry.layoutId
                compare(button.text, "Aplicado")
                compare(button.Accessible.name, "Layout em uso: " + entry.name)
                compare(card.border.width, 2)
                verify(!button.enabled)
                manager.activeLayout = ""
                manager.busy = true
                verify(!button.enabled)
                manager.busy = false
                previous = card
            }
            manager.available = false
            verify(!findChild(page, "applyLayout-unity").enabled)
            manager.available = true
        }
        function test_newPreviews() {
            const component = Qt.createComponent("components/LayoutPreview.qml")
            compare(component.status, Component.Ready)
            const three = component.createObject(page, {layoutId: "three-panels"})
            compare(three.shapes.length, 3)
            verify(three.shapes.every(s => s.y > 0.8 && s.pill))
            three.destroy()
            const single = component.createObject(page, {layoutId: "single-bar"})
            compare(single.shapes.length, 1)
            verify(single.shapes[0].x > 0 && single.shapes[0].x + single.shapes[0].w < 1)
            single.destroy()
            const legacy = component.createObject(page, {layoutId: "floating"})
            compare(legacy.shapes.length, 0)
            legacy.destroy()
        }
    }
}
