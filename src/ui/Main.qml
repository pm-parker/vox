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
    height: 940
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
        id: pill
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
        Keys.forwardTo: [scroll]
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

        // Scrollable body: settings grow with features, the window need
        // not. Keys.forwardTo (above) feeds PgUp/PgDn/arrows to the bar.
        Flickable {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: body.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: body
                width: scroll.width
                spacing: 12

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
            SwitchRow {
                Layout.fillWidth: true
                label: qsTr("Start at login")
                value: Settings.autostart
                onChange: (checked) => Settings.autostart = checked
            }

                // Recognition card
        Rectangle {
            Layout.fillWidth: true
            radius: root.rad
            color: root.surface
            border.color: root.line
            border.width: 1
            implicitHeight: recognitionColumn.implicitHeight + 24
            ColumnLayout {
                id: recognitionColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.dim
                    }
                    Label {
                        text: qsTr("Recognition")
                        color: root.fg
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Language")
                        color: root.fg
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        spacing: 6
                        Repeater {
                            model: ["auto", "zh", "en", "ja", "ko"]
                            PillButton {
                                required property string modelData
                                text: modelData
                                background: Rectangle {
                                    radius: root.rad
                                    color: Settings.language === modelData ? root.accent : root.surfaceUp
                                    border.color: root.line
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: Settings.language === parent.modelData ? root.accentFg : root.fg
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: Settings.language = modelData
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Number & date normalization")
                        color: root.fg
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                    PillButton {
                        property bool on: Settings.useItn
                        text: on ? qsTr("On") : qsTr("Off")
                        background: Rectangle {
                            radius: root.rad
                            color: parent.on ? root.accent : root.surfaceUp
                            border.color: root.line
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.on ? root.accentFg : root.fg
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: Settings.useItn = !Settings.useItn
                    }
                }
            }
        }

        // Audio card
        Rectangle {
            Layout.fillWidth: true
            radius: root.rad
            color: root.surface
            border.color: root.line
            border.width: 1
            implicitHeight: audioColumn.implicitHeight + 24
            ColumnLayout {
                id: audioColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.dim
                    }
                    Label {
                        text: qsTr("Audio")
                        color: root.fg
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Microphone")
                        color: root.fg
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                    ComboBox {
                        id: micBox
                        Layout.preferredWidth: 220
                        model: Backend.micSources
                        textRole: "desc"
                        valueRole: "name"
                        // Pure function of observable state — the sources list
                        // arrives async from pactl, so a currentIndex binding
                        // would miss the update and leave a blank box.
                        displayText: {
                            for (let i = 0; i < Backend.micSources.length; ++i)
                                if (Backend.micSources[i].name === Settings.microphone)
                                    return Backend.micSources[i].desc
                            return qsTr("System default microphone")
                        }
                        onActivated: (i) => Settings.microphone = Backend.micSources[i].name
                        background: Rectangle {
                            radius: root.rad
                            color: root.surfaceUp
                            border.color: root.line
                        }
                        contentItem: Text {
                            text: micBox.displayText
                            color: root.fg
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            leftPadding: 10
                            rightPadding: 24
                        }
                        indicator: Label {
                            text: "▾"
                            color: root.dim
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        delegate: ItemDelegate {
                            id: micDelegate
                            required property var modelData
                            width: micBox.width
                            contentItem: Text {
                                text: micDelegate.modelData.desc
                                color: root.fg
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            highlighted: micBox.highlightedIndex === micDelegate.index
                            background: Rectangle {
                                radius: 4
                                color: micDelegate.highlighted ? root.accent : root.surface
                            }
                        }
                        popup: Popup {
                            y: micBox.height + 4
                            width: micBox.width
                            padding: 4
                            background: Rectangle {
                                radius: root.rad
                                color: root.surfaceUp
                                border.color: root.line
                            }
                            contentItem: ListView {
                                clip: true
                                implicitHeight: Math.min(contentHeight, 240)
                                model: micBox.delegateModel
                                currentIndex: micBox.highlightedIndex
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Resume after silence")
                        color: root.fg
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                    Slider {
                        id: gapSlider
                        from: 400; to: 3000; stepSize: 100
                        value: Settings.gapMs
                        onMoved: Settings.gapMs = value
                        implicitWidth: 150
                        background: Rectangle {
                            x: gapSlider.leftPadding
                            y: gapSlider.topPadding + gapSlider.availableHeight / 2 - height / 2
                            width: gapSlider.availableWidth
                            height: 4
                            radius: 2
                            color: root.surfaceUp
                            Rectangle {
                                width: gapSlider.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: root.accent
                            }
                        }
                        handle: Rectangle {
                            x: gapSlider.leftPadding + gapSlider.visualPosition * (gapSlider.availableWidth - width)
                            y: gapSlider.topPadding + gapSlider.availableHeight / 2 - height / 2
                            width: 14; height: 14; radius: 7
                            color: root.accent
                            border.color: root.line
                        }
                    }
                    Label {
                        text: (Settings.gapMs / 1000).toFixed(1) + "s"
                        color: root.dim
                        font.pixelSize: 12
                    }
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
                // Model picker — Engine.catalog(), one entry per language
                // family. Picking an uninstalled model flips the card into
                // its download state; picking an installed one is instant.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    ComboBox {
                        id: modelBox
                        Layout.preferredWidth: 240
                        enabled: Speech.phase !== "downloading" && Speech.phase !== "extracting"
                        model: Speech.models
                        textRole: "name"
                        currentIndex: {
                            const ids = Speech.models.map(function (m) { return m.id })
                            const i = ids.indexOf(Speech.model)
                            return i >= 0 ? i : 0
                        }
                        onActivated: (index) => Speech.model = Speech.models[index].id
                        background: Rectangle {
                            implicitWidth: 240; implicitHeight: 32
                            radius: root.rad
                            color: root.surfaceUp
                            border.color: modelBox.activeFocus || modelBox.hovered ? root.accent : root.line
                        }
                        contentItem: Text {
                            leftPadding: 10; rightPadding: 30
                            text: modelBox.displayText
                            color: root.fg
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        indicator: Label {
                            x: modelBox.width - width - 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: "▾"; color: root.dim
                        }
                        delegate: ItemDelegate {
                            width: modelBox.width
                            contentItem: Text {
                                leftPadding: 10
                                text: model.id + (installed ? "  ✓" : "")
                                color: highlighted ? root.accentFg : (installed ? root.fg : root.dim)
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            highlighted: modelBox.highlightedIndex === index
                            background: Rectangle {
                                radius: 4
                                color: highlighted ? root.accent : root.surfaceUp
                            }
                        }
                        popup: Popup {
                            y: modelBox.height + 4
                            width: modelBox.width
                            implicitHeight: contentItem.implicitHeight + 8
                            padding: 2
                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: modelBox.delegateModel
                                currentIndex: modelBox.highlightedIndex
                            }
                            background: Rectangle {
                                radius: root.rad
                                color: root.surface
                                border.color: root.line
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: {
                        const entry = Speech.models.find(function (m) { return m.id === Speech.model })
                        return entry ? entry.detail : ""
                    }
                    color: root.dim
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
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

        // Shortcuts card — global hotkeys, live-applied through Omarchy's
        // dictation-backend hook. Type exactly what Hyprland expects, e.g.
        // "ALT + SPACE" or "SUPER + V".
        Rectangle {
            Layout.fillWidth: true
            radius: root.rad
            color: root.surface
            border.color: root.line
            border.width: 1
            implicitHeight: keysColumn.implicitHeight + 24
            ColumnLayout {
                id: keysColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Label {
                    text: qsTr("Shortcuts")
                    color: root.dim
                    font.pixelSize: 11
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        text: qsTr("Toggle dictation")
                        color: root.fg
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: toggleField
                        Layout.preferredWidth: 170
                        text: Settings.shortcuts.toggleKey || "ALT + SPACE"
                        color: root.fg
                        placeholderText: "ALT + SPACE"
                        font.pixelSize: 13
                        horizontalAlignment: TextInput.AlignHCenter
                        background: Rectangle {
                            radius: root.rad
                            color: root.surfaceUp
                            border.color: toggleField.activeFocus ? root.accent : root.line
                        }
                        onEditingFinished: {
                            const keys = Settings.shortcuts
                            keys.toggleKey = toggleField.text.trim()
                            Settings.shortcuts = keys
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        text: qsTr("Push-to-talk (hold)")
                        color: root.fg
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: pttField
                        Layout.preferredWidth: 170
                        text: Settings.shortcuts.pttKey || "ALT + Alt_R"
                        color: root.fg
                        placeholderText: "ALT + Alt_R"
                        font.pixelSize: 13
                        horizontalAlignment: TextInput.AlignHCenter
                        background: Rectangle {
                            radius: root.rad
                            color: root.surfaceUp
                            border.color: pttField.activeFocus ? root.accent : root.line
                        }
                        onEditingFinished: {
                            const keys = Settings.shortcuts
                            keys.pttKey = pttField.text.trim()
                            Settings.shortcuts = keys
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    PillButton { text: qsTr("Reset to defaults"); onClicked: Backend.resetShortcuts() }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Changes apply immediately — press the keys to try.\nPush-to-talk ignores the silence auto-stop while held.")
                    color: root.dim
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }
            }
        }
    }
}
