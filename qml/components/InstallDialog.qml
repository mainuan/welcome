import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Standard installation dialog of the Welcome. It renders whatever profile
// InstallService is running; it never decides what is installed.
Controls.Popup {
    id: dialog

    property var installer
    property string profileId: ""
    property var info: ({})
    property bool showDetails: false
    property bool closeBlocked: false

    property bool dark: false
    property color accent: "#3daee9"
    property color surfaceColor: "#ffffff"
    property color elevatedColor: "#f5f7fb"
    property color borderColor: "#d9deea"
    property color textColor: "#202532"
    property color mutedColor: "#697286"
    property color successColor: "#2da65a"
    property color errorColor: "#c62828"

    readonly property bool ownsJob: !!installer && installer.profileId === profileId
    readonly property string phase: ownsJob ? installer.phase : "idle"
    readonly property bool running: ownsJob && installer.busy
    readonly property bool succeeded: phase === "success"
    readonly property bool failed: phase === "error"
    readonly property bool cancelled: phase === "cancelled"
    readonly property bool finished: succeeded || failed || cancelled
    readonly property var profileState: installer && installer.profiles[profileId] ? installer.profiles[profileId] : ({})
    readonly property bool canRunAction: succeeded && (info.actionLabel || "").length > 0 && profileState.canLaunch === true

    // Opens the dialog for `id`; while another installation runs, it shows that one instead.
    function openFor(id) {
        const target = installer.busy ? installer.profileId : id;
        profileId = target;
        info = installer.profileInfo(target);
        showDetails = false;
        closeBlocked = false;
        open();
        if (!installer.busy)
            installer.start(target);
    }

    function requestClose() {
        if (running) {
            closeBlocked = true;
            blockedPulse.restart();
            return;
        }
        close();
    }

    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(540, (parent ? parent.width : 540) - 48)
    modal: true
    focus: true
    padding: 24
    // Escape and outside clicks are handled explicitly so that an installation
    // in progress can explain why the dialog stays open.
    closePolicy: Controls.Popup.NoAutoClose

    onClosed: {
        if (!installer.busy)
            installer.reset();
        profileId = "";
    }
    onSucceededChanged: if (succeeded) Qt.callLater(() => (canRunAction ? actionButton : closeButton).forceActiveFocus())
    onFailedChanged: if (failed) Qt.callLater(() => retryButton.forceActiveFocus())
    onCancelledChanged: if (cancelled) Qt.callLater(() => closeButton.forceActiveFocus())

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 160; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110 }
    }

    Controls.Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, dialog.dark ? 0.55 : 0.35)
        TapHandler { onTapped: dialog.requestClose() }
    }

    background: Kirigami.ShadowedRectangle {
        radius: 14
        color: dialog.surfaceColor
        border.width: 1
        border.color: dialog.borderColor
        shadow.size: 28
        shadow.yOffset: 6
        shadow.color: Qt.rgba(0, 0, 0, dialog.dark ? 0.5 : 0.22)
    }

    contentItem: FocusScope {
        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight
        focus: true
        Keys.onEscapePressed: dialog.requestClose()

        Accessible.role: Accessible.Dialog
        Accessible.name: title.text

        ColumnLayout {
            id: column
            anchors.fill: parent
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Item {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    Layout.alignment: Qt.AlignTop

                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        readonly property color tone: dialog.succeeded ? dialog.successColor
                                                    : (dialog.failed ? dialog.errorColor : dialog.accent)
                        color: Qt.rgba(tone.r, tone.g, tone.b, dialog.dark ? 0.20 : 0.12)
                        Behavior on color { ColorAnimation { duration: 180 } }
                    }
                    Kirigami.Icon {
                        id: productIcon
                        readonly property bool bitmap: (dialog.info.iconSource || "").length > 0
                        anchors.centerIn: parent
                        // Application artwork has generous padding; theme icons do not.
                        width: bitmap ? 44 : 30
                        height: width
                        visible: !dialog.finished
                        source: (dialog.info.iconSource || "").length > 0 ? dialog.info.iconSource
                                                                         : (dialog.info.iconName || "system-software-install")
                        color: dialog.accent
                        Accessible.ignored: true
                    }
                    Kirigami.Icon {
                        id: resultIcon
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        visible: dialog.finished
                        source: dialog.succeeded ? "checkmark" : (dialog.failed ? "dialog-error" : "dialog-cancel")
                        color: dialog.succeeded ? dialog.successColor : (dialog.failed ? dialog.errorColor : dialog.mutedColor)
                        Accessible.ignored: true
                        onVisibleChanged: if (visible) popIn.restart()
                        NumberAnimation on scale {
                            id: popIn
                            running: false
                            from: 0.5
                            to: 1
                            duration: 220
                            easing.type: Easing.OutBack
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Kirigami.Heading {
                        id: title
                        Layout.fillWidth: true
                        level: 3
                        wrapMode: Text.WordWrap
                        color: dialog.textColor
                        text: dialog.succeeded ? (dialog.info.successTitle || "Instalação concluída")
                            : dialog.failed ? "Não foi possível concluir a instalação"
                            : dialog.cancelled ? "Instalação cancelada"
                            : (dialog.info.runningTitle || "Instalando")
                    }
                    Controls.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: dialog.mutedColor
                        text: dialog.finished ? dialog.installer.resultMessage
                            : (dialog.installer.stepCount > 1
                               ? "Etapa " + dialog.installer.step + " de " + dialog.installer.stepCount
                               : (dialog.info.description || ""))
                        Accessible.role: Accessible.StaticText
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: dialog.running || dialog.succeeded
                spacing: 8

                Controls.ProgressBar {
                    id: progress
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    indeterminate: dialog.running && dialog.installer.percent < 0
                    value: dialog.succeeded ? 100 : Math.max(0, dialog.installer.percent)
                    Behavior on value { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                    Accessible.name: "Progresso da instalação"
                    Accessible.description: indeterminate ? "Em andamento" : Math.round(value) + "%"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Controls.Label {
                        Layout.fillWidth: true
                        text: dialog.succeeded ? "Concluído" : dialog.installer.message
                        color: dialog.textColor
                        elide: Text.ElideRight
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }
                    Controls.Label {
                        visible: dialog.succeeded || (dialog.running && dialog.installer.percent >= 0)
                        text: (dialog.succeeded ? 100 : dialog.installer.percent) + "%"
                        color: dialog.mutedColor
                        font.features: { "tnum": 1 }
                    }
                }
            }

            RowLayout {
                id: blockedNotice
                Layout.fillWidth: true
                visible: dialog.running
                spacing: 8
                Kirigami.Icon {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    Layout.alignment: Qt.AlignTop
                    source: "dialog-information"
                    color: dialog.closeBlocked ? dialog.accent : dialog.mutedColor
                    Accessible.ignored: true
                }
                Controls.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    font.weight: dialog.closeBlocked ? Font.DemiBold : Font.Normal
                    color: dialog.closeBlocked ? dialog.textColor : dialog.mutedColor
                    text: dialog.closeBlocked
                          ? "Interromper agora pode deixar o sistema inconsistente. Você poderá fechar esta janela assim que a instalação terminar."
                          : "Você pode continuar usando o computador. A instalação não pode ser interrompida com segurança."
                    Accessible.role: Accessible.StaticText
                }
                SequentialAnimation {
                    id: blockedPulse
                    NumberAnimation { target: blockedNotice; property: "opacity"; to: 0.35; duration: 90 }
                    NumberAnimation { target: blockedNotice; property: "opacity"; to: 1; duration: 160 }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: (dialog.failed || dialog.cancelled) && dialog.installer.details.length > 0
                spacing: 6

                Controls.Button {
                    flat: true
                    text: dialog.showDetails ? "Ocultar detalhes" : "Ver detalhes"
                    icon.name: dialog.showDetails ? "arrow-up" : "arrow-down"
                    onClicked: dialog.showDetails = !dialog.showDetails
                    Accessible.description: "Mostra a saída técnica da instalação"
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 170
                    visible: dialog.showDetails
                    radius: 8
                    color: dialog.elevatedColor
                    border.color: dialog.borderColor

                    Controls.ScrollView {
                        id: detailsScroll
                        anchors.fill: parent
                        anchors.margins: 8
                        // A plain TextEdit: Breeze's TextArea passes itself to a property
                        // typed TextInput and logs a warning every time it is created.
                        TextEdit {
                            width: detailsScroll.availableWidth
                            readOnly: true
                            selectByMouse: true
                            wrapMode: Text.WrapAnywhere
                            text: dialog.installer.details
                            font.family: "monospace"
                            font.pixelSize: 11
                            color: dialog.textColor
                            selectionColor: dialog.accent
                            Accessible.role: Accessible.EditableText
                            Accessible.name: "Detalhes técnicos da instalação"
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: dialog.finished
                spacing: 8
                Item { Layout.fillWidth: true }
                Controls.Button {
                    id: actionButton
                    visible: dialog.canRunAction
                    text: dialog.info.actionLabel || ""
                    icon.name: text === "Abrir" ? "system-run" : "configure"
                    highlighted: true
                    onClicked: {
                        dialog.installer.launch(dialog.profileId);
                        dialog.close();
                    }
                }
                Controls.Button {
                    id: retryButton
                    visible: dialog.failed || dialog.cancelled
                    text: "Tentar novamente"
                    icon.name: "view-refresh"
                    highlighted: true
                    onClicked: {
                        dialog.showDetails = false;
                        dialog.closeBlocked = false;
                        dialog.installer.start(dialog.profileId);
                    }
                }
                Controls.Button {
                    id: closeButton
                    text: "Fechar"
                    icon.name: "window-close"
                    onClicked: dialog.close()
                }
            }
        }
    }
}
