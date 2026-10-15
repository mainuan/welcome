import QtQuick
import QtQuick.Controls as Controls
import QtCore
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// "Relatório da máquina": a fastfetch-style summary followed by one card per
// section, with actions to refresh, copy and save the text report.
Controls.Popup {
    id: dialog

    property var report
    property string logoSource: ""
    property string feedback: ""
    property bool feedbackError: false

    property bool dark: false
    property color accent: "#3daee9"
    property color surfaceColor: "#ffffff"
    property color elevatedColor: "#f5f7fb"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property color successColor: "#2da65a"
    property color errorColor: "#c0392b"

    function showFeedback(message, error) {
        feedback = message;
        feedbackError = error;
        feedbackTimer.restart();
    }

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(900, (parent ? parent.width : 900) - 48)
    height: Math.min(760, (parent ? parent.height : 760) - 48)
    modal: true
    focus: true
    padding: 0
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

    onOpened: {
        feedback = "";
        if (!report.ready && !report.busy)
            report.refresh();
        closeButton.forceActiveFocus();
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutCubic }
    }

    Controls.Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, dialog.dark ? 0.55 : 0.35) }

    background: Kirigami.ShadowedRectangle {
        radius: 16
        color: dialog.elevatedColor
        border.width: 1
        border.color: dialog.borderColor
        shadow.size: 28
        shadow.yOffset: 6
        shadow.color: Qt.rgba(0, 0, 0, dialog.dark ? 0.5 : 0.22)
    }

    Timer {
        id: feedbackTimer
        interval: 3000
        onTriggered: dialog.feedback = ""
    }

    Dialogs.FileDialog {
        id: saveDialog
        title: "Salvar relatório da máquina"
        fileMode: Dialogs.FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: ["Texto (*.txt)"]
        onAccepted: dialog.report.saveReport(selectedFile)
                    ? dialog.showFeedback("Relatório salvo.", false)
                    : dialog.showFeedback("Não foi possível salvar o relatório.", true)
    }

    contentItem: ColumnLayout {
        spacing: 0
        Accessible.role: Accessible.Dialog
        Accessible.name: "Relatório da máquina"

        // Header: summary and actions.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: header.implicitHeight + 36
            radius: 16
            color: dialog.surfaceColor
            Rectangle { // square the bottom corners against the body
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 16
                color: dialog.surfaceColor
            }
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: dialog.borderColor
            }

            RowLayout {
                id: header
                anchors.fill: parent
                anchors.margins: 18
                spacing: 18

                Image {
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 88
                    Layout.alignment: Qt.AlignTop
                    source: dialog.logoSource
                    visible: status === Image.Ready
                    fillMode: Image.PreserveAspectFit
                    sourceSize.width: 176
                    sourceSize.height: 176
                    smooth: true
                    mipmap: true
                    Accessible.ignored: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 14
                    rowSpacing: 3
                    Accessible.role: Accessible.List
                    Accessible.name: "Resumo do sistema"

                    Kirigami.Heading {
                        Layout.columnSpan: 2
                        level: 2
                        text: dialog.report.summary.os || "Relatório da máquina"
                        color: dialog.textColor
                    }
                    Repeater {
                        model: [
                            { label: "Desktop", value: dialog.report.summary.plasma },
                            { label: "Kernel", value: (dialog.report.summary.kernel || "").replace("Kernel ", "") },
                            { label: "CPU", value: dialog.report.summary.cpu },
                            { label: "GPU", value: dialog.report.summary.gpu },
                            { label: "Memória", value: dialog.report.summary.memory },
                            { label: "Ligado há", value: dialog.report.summary.uptime }
                        ]
                        delegate: Controls.Label {
                            required property var modelData
                            required property int index
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            visible: (modelData.value || "").length > 0
                            textFormat: Text.StyledText
                            text: "<b><font color='" + dialog.accent + "'>" + modelData.label + "</font></b>&nbsp;&nbsp;" + modelData.value
                            color: dialog.textColor
                            elide: Text.ElideRight
                            font.pixelSize: 13
                            Accessible.name: modelData.label + ": " + modelData.value
                        }
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: 6
                    Controls.Button {
                        Layout.fillWidth: true
                        text: "Copiar relatório"
                        icon.name: "edit-copy"
                        highlighted: true
                        enabled: dialog.report.ready
                        onClicked: dialog.report.copyReport()
                                   ? dialog.showFeedback("Relatório copiado.", false)
                                   : dialog.showFeedback("Não foi possível copiar o relatório.", true)
                    }
                    Controls.Button {
                        Layout.fillWidth: true
                        text: "Salvar relatório"
                        icon.name: "document-save"
                        enabled: dialog.report.ready
                        onClicked: {
                            // Suggested name, set when opening: Qt 6.10 warns about a
                            // relative or not-yet-existing file given at creation.
                            const folder = StandardPaths.writableLocation(StandardPaths.DocumentsLocation);
                            saveDialog.currentFolder = folder;
                            saveDialog.selectedFile = folder + "/mainuan-system-report.txt";
                            saveDialog.open();
                        }
                    }
                    Controls.Button {
                        Layout.fillWidth: true
                        text: dialog.report.busy ? "Atualizando…" : "Atualizar"
                        icon.name: "view-refresh"
                        enabled: !dialog.report.busy
                        onClicked: dialog.report.refresh()
                    }
                    Controls.Button {
                        id: closeButton
                        Layout.fillWidth: true
                        text: "Fechar"
                        icon.name: "window-close"
                        onClicked: dialog.close()
                    }
                }
            }
        }

        Controls.Label {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.topMargin: 10
            visible: dialog.feedback.length > 0
            text: dialog.feedback
            color: dialog.feedbackError ? dialog.errorColor : dialog.successColor
            font.weight: Font.DemiBold
            Accessible.role: Accessible.StaticText
        }

        Controls.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 24
            visible: !dialog.report.ready
            running: visible
        }

        // Body: one card per section.
        Controls.ScrollView {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            contentHeight: sectionGrid.implicitHeight + 28

            GridLayout {
                id: sectionGrid
                width: body.availableWidth - 36
                x: 18
                y: 14
                columns: width > 640 ? 2 : 1
                columnSpacing: 12
                rowSpacing: 12

                Repeater {
                    model: dialog.report.sections
                    delegate: Rectangle {
                        id: sectionCard
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        implicitHeight: sectionColumn.implicitHeight + 28
                        radius: 12
                        color: dialog.surfaceColor
                        border.color: dialog.borderColor

                        ColumnLayout {
                            id: sectionColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 14
                            spacing: 8

                            RowLayout {
                                spacing: 8
                                Kirigami.Icon {
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    source: "qrc:/icons/" + sectionCard.modelData.icon + ".svg"
                                    isMask: true
                                    color: dialog.accent
                                    Accessible.ignored: true
                                }
                                Controls.Label {
                                    text: sectionCard.modelData.title
                                    color: dialog.textColor
                                    font.weight: Font.DemiBold
                                    Accessible.role: Accessible.Heading
                                }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 12
                                rowSpacing: 4
                                Repeater {
                                    model: sectionCard.modelData.items
                                    delegate: Item {
                                        id: entry
                                        required property var modelData
                                        Layout.columnSpan: 2
                                        Layout.fillWidth: true
                                        implicitHeight: Math.max(entryLabel.implicitHeight, entryValue.implicitHeight)
                                        Accessible.role: Accessible.StaticText
                                        Accessible.name: modelData.label + ": " + modelData.value
                                        Controls.Label {
                                            id: entryLabel
                                            width: Math.min(150, parent.width * 0.4)
                                            text: entry.modelData.label
                                            color: dialog.mutedColor
                                            font.pixelSize: 12
                                            wrapMode: Text.WordWrap
                                        }
                                        Controls.Label {
                                            id: entryValue
                                            x: entryLabel.width + 10
                                            width: parent.width - x
                                            text: entry.modelData.value
                                            color: dialog.textColor
                                            font.pixelSize: 12
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
