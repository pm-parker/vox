pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Vox

// The one window: a big mic, the last sentence, and just enough settings.
// Omarchy's look — near-black surfaces, the live theme accent, theme radius,
// keyboard-first — rendered from Backend's idle|listening|processing state.
ApplicationWindow {
    id: root
    width: 430
    height: 680
    visible: true
    color: "#161616"
    title: "vox"

    readonly property color surface: "#1E1E1E"
    readonly property color surfaceUp: "#262626"
    readonly property color line: "#2E2E2E"
    readonly property color fg: "#EEEEEE"
    readonly property color dim: "#8A8A8A"
    readonly property color good: "#30D158"
    readonly property color bad: "#FF453A"
    readonly property color accent: Theme.accent
    readonly property color accentFg: Theme.accentForeground
    readonly property real rad: Math.min(Theme.radius > 0 ? Theme.radius : 12, 14)

    readonly property bool listening: Backend.state === "listening"
    readonly property bool processing: Backend.state === "processing"

    component PillButton: Button {
        focusPolicy: Qt.NoFocus
        background: Rectangle {
            radius: root.rad
            color: pill.pressed ? root.accent : root.surfaceUp
            border.color: pill.pressed ? root.accent : root.line
            border.width: 1
        }
        contentItem: Text {
            text: pill.text
            color: pill.pressed ? root.accentFg : root.fg
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        leftPadding: 14
        rightPadding: 14
        topPadding: 7
        bottomPadding: 7
    }

    component SwitchRow: RowLayout {
        required property string label
        required property bool value
        signal change(bool checked)

        Label {
            text: parent.label
            color: root.fg
            font.pixelSize: 13
            Layout.fillWidth: true
        }
        Switch {
            focusPolicy: Qt.NoFocus
            checked: parent.value
            onToggled: parent.change(checked)
            indicator: Rectangle {
                implicitWidth: 38
                implicitHeight: 22
                radius: 11
                color: parent.checked ? root.accent : root.surfaceUp
                border.color: parent.checked ? root.accent : root.line
                Rectangle {
                    x: parent.checked ? parent.width - width - 3 : 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16; height: 16; radius: 8
                    color: parent.checked ? root.accentFg : root.dim
                    Behavior on x { NumberAnimation { duration: 120 } }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        focus: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Space) { Backend.toggle(); event.accepted = true }
            else if (event.key === Qt.Key_R) { Backend.repeatLast(); event.accepted = true }
            else if (event.key === Qt.Key_C) { Backend.copyLast(); event.accepted = true }
            else if (event.key === Qt.Key_M) {
                if (Speech.phase === "not-installed" || Speech.phase === "error")
                    Speech.install()
                event.accepted = true
            }
        }

        // Header
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "vox"
                color: root.accent
                font.pixelSize: 20
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                radius: height / 2
                implicitWidth: stateLabel.implicitWidth + 18
                implicitHeight: 22
                color: root.listening ? root.accent : (root.processing ? root.surfaceUp : root.surface)
                border.color: root.line
                Label {
                    id: stateLabel
                    anchors.centerIn: parent
                    text: root.listening ? qsTr("Speak now")
                          : root.processing ? qsTr("Thinking…")
                          : qsTr("Tap or press Space to dictate")
                    color: root.listening ? root.accentFg : root.dim
                    font.pixelSize: 11
                }
            }
        }

        // Mic stage
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 190

            // Pulsing rings while listening
            Repeater {
                model: 3
                Rectangle {
                    required property int index
                    anchors.centerIn: parent
                    width: 96; height: 96
                    radius: 48
                    color: "transparent"
                    border.color: root.accent
                    border.width: 2
                    opacity: root.listening ? 0.5 - index * 0.15 : 0
                    scale: root.listening ? 1.0 + index * 0.35 : 0.6
                    SequentialAnimation on scale {
                        running: root.listening
                        loops: Animation.Infinite
                        NumberAnimation { to: 2.2 + index * 0.3; duration: 1400 }
                        NumberAnimation { to: 1.0; duration: 0 }
                    }
                }
            }

            Button {
                id: mic
                anchors.centerIn: parent
                focusPolicy: Qt.NoFocus
                width: 96; height: 96
                Accessible.name: qsTr("Tap or press Space to dictate")
                background: Rectangle {
                    radius: 48
                    color: root.listening ? root.bad : (root.processing ? root.surfaceUp : root.accent)
                    border.width: 0
                }
                contentItem: Item {
                    // Waveform bars, echoing the app icon
                    Row {
                        anchors.centerIn: parent
                        spacing: 5
                        Repeater {
                            model: [0.5, 0.9, 1.0, 0.8, 0.55]
                            Rectangle {
                                required property real modelData
                                width: 6
                                radius: 3
                                color: root.listening ? "#FFFFFF"
                                      : root.processing ? root.dim : root.accentFg
                                height: root.listening
                                        ? 8 + 44 * Backend.level * modelData
                                        : (root.processing ? 10 : 36 * modelData)
                                Behavior on height { NumberAnimation { duration: 90 } }
                            }
                        }
                    }
                }
                onClicked: Backend.toggle()
            }

            Label {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    const s = Backend.elapsedMs
                    const mm = Math.floor(s / 60000)
                    const ss = Math.floor((s % 60000) / 1000)
                    return root.listening || root.processing
                            ? `${mm}:${ss < 10 ? "0" + ss : ss}` : ""
                }
                color: root.fg
                font.pixelSize: 22
            }
        }

        // Status / error
        Label {
            Layout.fillWidth: true
            visible: Backend.errorText === "" && Backend.statusLine !== ""
            text: Backend.statusLine
            color: root.dim
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        Rectangle {
            Layout.fillWidth: true
            visible: Backend.errorText !== ""
            radius: root.rad
            color: Qt.rgba(root.bad.r, root.bad.g, root.bad.b, 0.12)
            border.color: root.bad
            border.width: 1
            implicitHeight: errorText.implicitHeight + 20
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                Label {
                    id: errorText
                    text: Backend.errorText
                    color: root.fg
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    text: "✕"
                    color: root.dim
                    font.pixelSize: 13
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Backend.dismissError()
                    }
                }
            }
        }

        // Last text
        Rectangle {
            Layout.fillWidth: true
            visible: Backend.lastText !== ""
            radius: root.rad
            color: root.surface
            border.color: root.line
            border.width: 1
            implicitHeight: lastColumn.implicitHeight + 24
            ColumnLayout {
                id: lastColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Label {
                    text: qsTr("Last text")
                    color: root.dim
                    font.pixelSize: 11
                }
                TextEdit {
                    Layout.fillWidth: true
                    text: Backend.lastText
                    readOnly: true
                    wrapMode: Text.WordWrap
                    color: root.fg
                    font.pixelSize: 14
                    selectionColor: root.accent
                    selectedTextColor: root.accentFg
                }
                RowLayout {
                    spacing: 8
                    PillButton { text: qsTr("Insert"); onClicked: Backend.repeatLast() }
                    PillButton { text: qsTr("Copy"); onClicked: Backend.copyLast() }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // History
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Backend.history.length > 0
            spacing: 6
            Label {
                text: qsTr("History")
                color: root.dim
                font.pixelSize: 11
            }
            ListView {
                id: historyList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: Backend.history
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: historyList.width
                    height: 40
                    radius: root.rad
                    color: historyMouse.containsMouse ? root.surfaceUp : root.surface
                    border.color: root.line
                    border.width: 1
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10
                        Label {
                            Layout.fillWidth: true
                            text: modelData.text
                            color: root.fg
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Label {
                            text: Qt.formatDateTime(new Date(modelData.at * 1000), "MM-dd hh:mm")
                            color: root.dim
                            font.pixelSize: 11
                        }
                    }
                    MouseArea {
                        id: historyMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Backend.useHistoryText(index)
                    }
                }
                ScrollBar.vertical: ScrollBar {}
            }
        }

        // Settings
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Label {
                text: qsTr("Settings")
                color: root.dim
                font.pixelSize: 11
            }
            SwitchRow {
                Layout.fillWidth: true
                label: qsTr("Insert text automatically")
                value: Settings.autoInsert
                onChange: (checked) => Settings.autoInsert = checked
            }
            SwitchRow {
                Layout.fillWidth: true
                label: qsTr("Auto-stop on silence")
                value: Settings.autoStop
                onChange: (checked) => Settings.autoStop = checked
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Punctuation")
                    color: root.fg
                    font.pixelSize: 13
                    Layout.fillWidth: true
                }
                PillButton {
                    text: "，。"
                    background: Rectangle {
                        radius: root.rad
                        color: Settings.punctStyle === "cn" ? root.accent : root.surfaceUp
                        border.color: root.line
                    }
                    contentItem: Text {
                        text: "，。"
                        color: Settings.punctStyle === "cn" ? root.accentFg : root.fg
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: Settings.punctStyle = "cn"
                }
                PillButton {
                    text: ", ."
                    background: Rectangle {
                        radius: root.rad
                        color: Settings.punctStyle === "en" ? root.accent : root.surfaceUp
                        border.color: root.line
                    }
                    contentItem: Text {
                        text: ", ."
                        color: Settings.punctStyle === "en" ? root.accentFg : root.fg
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: Settings.punctStyle = "en"
                }
            }
        }

        // Model card
        Rectangle {
            Layout.fillWidth: true
            radius: root.rad
            color: root.surface
            border.color: root.line
            border.width: 1
            implicitHeight: modelColumn.implicitHeight + 24
            ColumnLayout {
                id: modelColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: Speech.phase === "ready" ? root.good
                             : Speech.phase === "error" ? root.bad
                             : Speech.phase === "downloading" || Speech.phase === "extracting" ? root.accent
                             : root.dim
                    }
                    Label {
                        text: qsTr("Speech model")
                        color: root.fg
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                    Label {
                        text: Speech.phase === "ready" ? qsTr("Ready — offline")
                              : Speech.phase === "downloading" ? qsTr("Downloading %1%").arg(Speech.progress)
                              : Speech.phase === "extracting" ? qsTr("Extracting…")
                              : Speech.phase === "error" ? qsTr("Failed — press M to retry")
                              : qsTr("Not installed")
                        color: Speech.phase === "ready" ? root.good : root.dim
                        font.pixelSize: 12
                    }
                }
                ProgressBar {
                    Layout.fillWidth: true
                    visible: Speech.phase === "downloading"
                    from: 0; to: 100; value: Speech.progress
                    indeterminate: Speech.phase === "extracting"
                    background: Rectangle {
                        radius: 3; color: root.surfaceUp
                        implicitHeight: 6
                    }
                    contentItem: Item {
                        Rectangle {
                            width: parent.width * parent.visualPosition
                            height: 6; radius: 3
                            color: root.accent
                        }
                    }
                }
                RowLayout {
                    visible: Speech.phase !== "ready" && Speech.phase !== "downloading" && Speech.phase !== "extracting"
                    Layout.fillWidth: true
                    PillButton {
                        text: Speech.phase === "error" ? qsTr("Retry download") : qsTr("Download model (~230 MB, once)")
                        onClicked: Speech.install()
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        visible: Speech.phase === "ready"
                        text: ""
                    }
                }
                RowLayout {
                    visible: Speech.phase === "ready"
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    PillButton { text: qsTr("Remove model"); onClicked: Speech.remove() }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
