import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

// A small group of mutually exclusive buttons ("Claro | Escuro"). The current
// value comes from the system: a click only requests a change, and the checked
// state (exposed to screen readers) always follows `current`.
RowLayout {
    id: choice

    // [{ value: "light", label: "Claro", icon: "weather-clear", tooltip: "…" }, …]
    property var options: []
    // When set, options whose value is not listed are disabled.
    property var availableValues: null
    property string current: ""
    property string groupName: ""
    signal activated(string value)

    spacing: 6
    Accessible.role: Accessible.Grouping
    Accessible.name: groupName

    Repeater {
        model: choice.options
        delegate: Controls.Button {
            required property var modelData
            readonly property bool selected: choice.current === modelData.value
            text: modelData.label
            icon.name: modelData.icon
            enabled: choice.availableValues === null || choice.availableValues.indexOf(modelData.value) >= 0
            highlighted: selected
            checkable: true
            checked: selected
            // Mouse, keyboard and AT-SPI all change `checked`, but only the first
            // two emit clicked/toggled; react to the change itself.
            onCheckedChanged: {
                if (checked === selected)
                    return;
                const requested = checked;
                checked = Qt.binding(() => selected);
                if (requested)
                    choice.activated(modelData.value);
            }
            Accessible.role: Accessible.RadioButton
            Accessible.name: choice.groupName + ": " + modelData.label
            Accessible.description: modelData.tooltip || ""
            Controls.ToolTip.visible: hovered && (modelData.tooltip || "").length > 0
            Controls.ToolTip.text: modelData.tooltip || ""
            Controls.ToolTip.delay: 500
        }
    }
}
