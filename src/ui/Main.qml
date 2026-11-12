pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import Vox

// The window is a utility panel, not a page: a compact two-hundred-odd pixels
// of chrome, three tabs, and every list scrolls inside its own tab. Nothing
// grows to fill the screen — the mic is the only thing that wants air.
ApplicationWindow {
    id: root
    width: 440
    // Compact: a utility panel, not a page. On a 720-tall screen this lands
    // around 520 px, and the settings list scrolls inside its own tab instead
    // of the window growing to swallow the display.
    height: Math.min(620, Math.max(480, Screen.height - 200))
    visible: true
    color: "#161616"
    title: "DuckVox"

    readonly property color surface: "#1E1E1E"
    readonly property color surfaceUp: "#262626"
    readonly property color line: "#2E2E2E"
    readonly property color fg: "#EEEEEE"
    readonly property color dim: "#8A8A8A"
    readonly property color good: "#30D158"
    readonly property color bad: "#FF453A"
    // Brand yellow while the signature mode is on; the live Omarchy theme
    // accent otherwise. One switch, every chrome surface follows.
    readonly property color accent: Settings.duckYellow ? Theme.brand : Theme.accent
    readonly property color accentFg: Settings.duckYellow ? Theme.brandForeground : Theme.accentForeground
    readonly property real rad: Math.min(Theme.radius > 0 ? Theme.radius : 12, 14)

    readonly property bool listening: Backend.state === "listening"
    readonly property bool processing: Backend.state === "processing"

    // 0 dictate, 1 settings, 2 history. History earns its own tab: keeping the
    // whole list under the mic is what starved the settings of space.
    property int page: 0
    readonly property int recentCount: Math.min(3, Backend.history.length)

    // Test hook for tools/screenshot_window: the settings page must be
    // reachable to its last card, so the shot tool flips this and grabs the
    // bottom. Harmless in the app — nothing sets it there.
    property bool scrollToEnd: false
    property bool scrollToLanguage: false
    onScrollToEndChanged: if (scrollToEnd) settingsScroll.contentY = Math.max(0, settingsScroll.contentHeight - settingsScroll.height)
    // Screenshot harness: land one card above the fold end so the
    // Interface-language card is in frame (bottom-relative, never overshoots).
    onScrollToLanguageChanged: if (scrollToLanguage) settingsScroll.contentY = Math.max(0, settingsScroll.contentHeight - settingsScroll.height - 480)

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
        leftPadding: 12
        rightPadding: 12
        topPadding: 6
        bottomPadding: 6
    }

    // Segmented tab: the pill look, with a persistent active fill.
    component Tab: Button {
        id: tab
        required property bool active
        focusPolicy: Qt.NoFocus
        background: Rectangle {
            radius: root.rad
            color: tab.active ? root.accent : (tab.hovered ? root.surfaceUp : "transparent")
            border.color: tab.active ? root.accent : root.line
            border.width: 1
        }
        contentItem: Text {
            text: tab.text
            color: tab.active ? root.accentFg : root.fg
            font.pixelSize: 12
            font.bold: tab.active
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        leftPadding: 14
        rightPadding: 14
        topPadding: 5
        bottomPadding: 5
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
            id: control
            focusPolicy: Qt.NoFocus
            checked: parent.value
            onToggled: {
                parent.change(control.checked)
                // The widget toggles `checked` internally, which drops the
                // `parent.value` binding; restore it so a later settings
                // change still moves the switch.
                control.checked = Qt.binding(() => parent.value)
            }
            indicator: Rectangle {
                implicitWidth: 38
                implicitHeight: 22
                radius: 11
                color: control.checked ? root.accent : root.surfaceUp
                border.color: control.checked ? root.accent : root.line
                Rectangle {
                    // `control.checked`, not `parent.checked`: the knob's
                    // parent is this track Rectangle, which has no `checked`,
                    // so the old binding read undefined and the knob never
                    // moved off its left seat.
                    x: control.checked ? parent.width - width - 3 : 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16; height: 16; radius: 8
                    color: control.checked ? root.accentFg : root.dim
                    Behavior on x { NumberAnimation { duration: 120 } }
                }
            }
        }
    }

    // One card shell: title line plus whatever is declared inside. Keeps the
    // settings page dense — five cards, one look, no repeated boilerplate.
    component Card: Rectangle {
        id: card
        property string title: ""
        default property alias content: inner.data
        Layout.fillWidth: true
        radius: root.rad
        color: root.surface
        border.color: root.line
        border.width: 1
        implicitHeight: inner.implicitHeight + 16
        ColumnLayout {
            id: inner
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6
            Label {
                Layout.fillWidth: true
                text: card.title
                visible: card.title !== ""
                color: root.dim
                font.pixelSize: 11
            }
        }
    }

    // One history row, shared by the recent list and the full list.
    component HistoryRow: Rectangle {
        id: row
        required property var entry
        signal activate()
        readonly property bool hasDelta: entry.delta !== undefined && entry.delta !== "" && entry.delta !== entry.text
        Layout.fillWidth: true
        implicitHeight: 40
        radius: root.rad
        color: rowMouse.containsMouse ? root.surfaceUp : "transparent"
        border.color: root.line
        border.width: 1
        // First child so the row below stays hoverable/clickable; the inner
        // widgets sit on top and handle their own clicks.
        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: row.activate()
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: row.entry.text
                color: root.fg
                font.pixelSize: 12
                elide: Text.ElideRight
                maximumLineCount: 1
                verticalAlignment: Text.AlignVCenter
            }
            // Per-item delta chip: what this take appended to the previous
            // one. Older entries (pre-2.2.2) have no delta and hide the chip.
            Rectangle {
                visible: row.hasDelta
                Layout.maximumWidth: Math.round(row.width * 0.42)
                implicitWidth: deltaText.implicitWidth + 14
                implicitHeight: 22
                radius: 11
                color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.14)
                Label {
                    id: deltaText
                    anchors.fill: parent
                    anchors.leftMargin: 7
                    anchors.rightMargin: 7
                    text: "+ " + (row.entry.delta || "")
                    color: root.accent
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Label {
                text: Qt.formatDateTime(new Date(row.entry.at * 1000), "MM-dd hh:mm")
                color: root.dim
                font.pixelSize: 10
                verticalAlignment: Text.AlignVCenter
            }
            Item {
                visible: row.hasDelta
                Layout.preferredWidth: 26
                Layout.preferredHeight: 22
                Rectangle {
                    anchors.fill: parent
                    radius: 11
                    color: copyNewMouse.containsMouse ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.18) : "transparent"
                    border.color: copyNewMouse.containsMouse ? root.accent : root.line
                    border.width: 1
                }
                Label {
                    anchors.centerIn: parent
                    text: "\u29C9"
                    color: copyNewMouse.containsMouse ? root.accent : root.dim
                    font.pixelSize: 12
                }
                MouseArea {
                    id: copyNewMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Backend.copyText(row.entry.delta || row.entry.text)
                }
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Copy the new part")
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10
        focus: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Space && root.page === 0) { Backend.toggle(); event.accepted = true }
            else if (event.key === Qt.Key_Left) { root.page = (root.page + 2) % 3; event.accepted = true }
            else if (event.key === Qt.Key_Right) { root.page = (root.page + 1) % 3; event.accepted = true }
            else if (event.key === Qt.Key_1) { root.page = 0; event.accepted = true }
            else if (event.key === Qt.Key_2) { root.page = 1; event.accepted = true }
            else if (event.key === Qt.Key_3) { root.page = 2; event.accepted = true }
            else if (event.key === Qt.Key_R) { Backend.repeatLast(); event.accepted = true }
            else if (event.key === Qt.Key_C) { Backend.copyLast(); event.accepted = true }
            else if (event.key === Qt.Key_M) {
                if (Speech.phase === "not-installed" || Speech.phase === "error")
                    Speech.install()
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) { root.hide(); event.accepted = true }
        }

        // Header — one line: name, live state, dismiss.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: "DuckVox"
                color: root.accent
                font.pixelSize: 17
                font.bold: true
            }
            Rectangle {
                radius: height / 2
                implicitWidth: stateLabel.implicitWidth + 16
                implicitHeight: 20
                color: root.listening ? root.accent : root.surface
                border.color: root.listening ? root.accent : root.line
                Label {
                    id: stateLabel
                    anchors.centerIn: parent
                    text: root.listening ? qsTr("Speak now")
                          : root.processing ? qsTr("Thinking…")
                          : qsTr("Idle")
                    color: root.listening ? root.accentFg : root.dim
                    font.pixelSize: 10
                }
            }
            Item { Layout.fillWidth: true }
            PillButton {
                text: "✕"
                Accessible.name: qsTr("Hide window")
                onClicked: root.hide()
            }
        }

        // Tabs — Left/Right or 1/2/3.
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Tab { text: qsTr("Dictate"); active: root.page === 0; onClicked: root.page = 0 }
            Tab { text: qsTr("Settings"); active: root.page === 1; onClicked: root.page = 1 }
            Tab {
                text: Backend.history.length > 0
                      ? qsTr("History %1").arg(Backend.history.length)
                      : qsTr("History")
                active: root.page === 2
                onClicked: root.page = 2
            }
            Item { Layout.fillWidth: true }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.page

            // ---------- Dictate ----------
            // A plain layout, not a scroll area: recent history is capped at
            // three rows, so this page always fits. The mic block takes the
            // slack and centres itself, which is what turns the dead black
            // space under the mic into breathing room.
            ColumnLayout {
                id: dictateBody
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 156

                        Repeater {
                            model: 3
                            Rectangle {
                                required property int index
                                anchors.centerIn: parent
                                width: 84; height: 84
                                radius: 42
                                color: "transparent"
                                border.color: root.accent
                                border.width: 2
                                opacity: root.listening ? 0.5 - index * 0.15 : 0
                                scale: root.listening ? 1.0 + index * 0.35 : 0.6
                                SequentialAnimation on scale {
                                    running: root.listening
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 2.1 + index * 0.3; duration: 1400 }
                                    NumberAnimation { to: 1.0; duration: 0 }
                                }
                            }
                        }

                        Button {
                            id: micButton
                            anchors.centerIn: parent
                            focusPolicy: Qt.NoFocus
                            width: 84; height: 84
                            Accessible.name: qsTr("Tap or press Space to dictate")
                            background: Rectangle {
                                radius: 42
                                color: root.listening ? root.bad : (root.processing ? root.surfaceUp : root.accent)
                                border.width: 0
                            }
                            contentItem: Item {
                                // A Button assigns its contentItem geometry; anchors
                                // fight that and the row drifts to the top-left.
                                // Centre the bars through a filling Item instead.
                                Row {
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Repeater {
                                        model: [0.5, 0.9, 1.0, 0.8, 0.55]
                                        Rectangle {
                                            required property real modelData
                                            width: 5
                                            radius: 2.5
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: root.listening ? "#FFFFFF"
                                                  : root.processing ? root.dim : root.accentFg
                                            height: root.listening
                                                    ? 7 + 38 * Backend.level * modelData
                                                    : (root.processing ? 9 : 30 * modelData)
                                            Behavior on height { NumberAnimation { duration: 90 } }
                                        }
                                    }
                                }
                            }
                            onClicked: Backend.toggle()
                        }

                        Label {
                            anchors.top: micButton.bottom
                            anchors.topMargin: 16
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: Math.min(implicitWidth, parent.width - 24)
                            visible: Backend.history.length === 0
                                  && Backend.errorText === "" && Backend.statusLine === ""
                            text: qsTr("Nothing dictated yet — press Space or the mic.")
                            color: root.dim
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
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
                            font.pixelSize: 18
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: Backend.errorText === "" && Backend.statusLine !== ""
                        text: Backend.statusLine
                        color: root.dim
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        visible: Backend.errorText !== ""
                        radius: root.rad
                        color: Qt.rgba(root.bad.r, root.bad.g, root.bad.b, 0.12)
                        border.color: root.bad
                        border.width: 1
                        implicitHeight: errorText.implicitHeight + 18
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 9
                            spacing: 8
                            Label {
                                id: errorText
                                text: Backend.errorText
                                color: root.fg
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            Label {
                                text: "✕"
                                color: root.dim
                                font.pixelSize: 12
                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -8
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Backend.dismissError()
                                }
                            }
                        }
                    }

                    Card {
                        title: qsTr("Last text")
                        visible: Backend.lastText !== ""

                        TextEdit {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(contentHeight, 130)
                            text: Backend.lastText
                            readOnly: true
                            activeFocusOnPress: false
                            wrapMode: Text.WordWrap
                            color: root.fg
                            font.pixelSize: 13
                            selectionColor: root.accent
                            selectedTextColor: root.accentFg
                            clip: true
                        }
                        RowLayout {
                            spacing: 7
                            PillButton { text: qsTr("Insert"); onClicked: Backend.repeatLast() }
                            PillButton { text: qsTr("Copy"); onClicked: Backend.copyLast() }
                            Item { Layout.fillWidth: true }
                        }
                    }

                    // Recent only — the full list lives in its own tab.
                    RowLayout {
                        Layout.fillWidth: true
                        visible: Backend.history.length > 0
                        Label {
                            text: qsTr("Recent")
                            color: root.dim
                            font.pixelSize: 11
                            Layout.fillWidth: true
                        }
                        PillButton {
                            text: qsTr("All %1 →").arg(Backend.history.length)
                            onClicked: root.page = 2
                        }
                    }
                    Repeater {
                        model: root.recentCount
                        HistoryRow {
                            required property int index
                            entry: Backend.history[index]
                            onActivate: Backend.useHistoryText(index)
                        }
                    }
                    Item { Layout.preferredHeight: 2 }
            }

            // ---------- Settings ----------
            Flickable {
                id: settingsScroll
                contentWidth: width
                contentHeight: settingsBody.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ColumnLayout {
                    id: settingsBody
                    width: settingsScroll.width
                    spacing: 8

                    Card {
                        title: qsTr("Dictation")
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
                        SwitchRow {
                            Layout.fillWidth: true
                            label: qsTr("Start at login")
                            value: Settings.autostart
                            onChange: (checked) => Settings.autostart = checked
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 7
                            Label {
                                text: qsTr("Punctuation")
                                color: root.fg
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            Tab {
                                text: "，。"
                                active: Settings.punctStyle === "cn"
                                onClicked: Settings.punctStyle = "cn"
                            }
                            Tab {
                                text: ", ."
                                active: Settings.punctStyle === "en"
                                onClicked: Settings.punctStyle = "en"
                            }
                        }
                    }

                    Card {
                        title: qsTr("Interface language")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                text: "🌐"
                                font.pixelSize: 14
                            }
                            Label {
                                text: Settings.uiLanguage === "auto" ? qsTr("Following the system language") : ""
                                color: root.dim
                                font.pixelSize: 11
                                Layout.fillWidth: true
                            }
                            Tab {
                                text: qsTr("System")
                                active: Settings.uiLanguage === "auto"
                                onClicked: Settings.uiLanguage = "auto"
                            }
                            Tab {
                                text: "EN"
                                active: Settings.uiLanguage === "en"
                                onClicked: Settings.uiLanguage = "en"
                            }
                            Tab {
                                text: "中文"
                                active: Settings.uiLanguage === "zh-CN"
                                onClicked: Settings.uiLanguage = "zh-CN"
                            }
                        }
                    }

                    Card {
                        title: qsTr("Appearance")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Rectangle {
                                width: 14; height: 14; radius: 7
                                color: "#FFD338"
                                border.color: root.line
                            }
                            Label {
                                text: qsTr("Duck yellow accent")
                                color: root.fg
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            Tab {
                                text: Settings.duckYellow ? qsTr("On") : qsTr("Off")
                                active: Settings.duckYellow
                                onClicked: Settings.duckYellow = !Settings.duckYellow
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Off follows the live Omarchy theme accent.")
                            color: root.dim
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }

                    Card {
                        title: qsTr("Recognition")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 7
                            Label {
                                text: qsTr("Language")
                                color: root.fg
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            Repeater {
                                model: ["auto", "zh", "en", "ja", "ko"]
                                Tab {
                                    required property string modelData
                                    text: modelData
                                    active: Settings.language === modelData
                                    onClicked: Settings.language = modelData
                                }
                            }
                        }
                        SwitchRow {
                            Layout.fillWidth: true
                            label: qsTr("Number & date normalization")
                            value: Settings.useItn
                            onChange: (checked) => Settings.useItn = checked
                        }
                    }

                    Card {
                        title: qsTr("Audio")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                text: qsTr("Microphone")
                                color: root.fg
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            ComboBox {
                                id: micBox
                                Layout.preferredWidth: 200
                                model: Backend.micSources
                                textRole: "desc"
                                valueRole: "name"
                                // Pure function of observable state — the
                                // sources arrive async from pactl.
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
                            spacing: 8
                            Label {
                                text: qsTr("Resume after silence")
                                color: root.fg
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            Slider {
                                id: gapSlider
                                from: 400; to: 3000; stepSize: 100
                                value: Settings.gapMs
                                onMoved: Settings.gapMs = value
                                implicitWidth: 130
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
                                font.pixelSize: 11
                            }
                        }
                    }

                    Card {
                        title: qsTr("Speech model")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                Layout.alignment: Qt.AlignVCenter
                                color: Speech.phase === "ready" ? root.good
                                     : Speech.phase === "error" ? root.bad
                                     : Speech.phase === "downloading" || Speech.phase === "extracting" ? root.accent
                                     : root.dim
                            }
                            Label {
                                Layout.fillWidth: true
                                text: Speech.phase === "ready" ? qsTr("Ready — offline")
                                      : Speech.phase === "downloading" ? qsTr("Downloading %1%").arg(Speech.progress)
                                      : Speech.phase === "extracting" ? qsTr("Extracting…")
                                      : Speech.phase === "error" ? qsTr("Failed — press M to retry")
                                      : qsTr("Not installed")
                                color: Speech.phase === "ready" ? root.good : root.dim
                                font.pixelSize: 11
                            }
                        }
                        ComboBox {
                            id: modelBox
                            Layout.fillWidth: true
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
                                implicitHeight: 30
                                radius: root.rad
                                color: root.surfaceUp
                                border.color: modelBox.activeFocus || modelBox.hovered ? root.accent : root.line
                            }
                            contentItem: Text {
                                leftPadding: 10; rightPadding: 30
                                text: modelBox.displayText
                                color: root.fg
                                font.pixelSize: 12
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
                                    font.pixelSize: 12
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
                                implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
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
                        Label {
                            Layout.fillWidth: true
                            text: {
                                const entry = Speech.models.find(function (m) { return m.id === Speech.model })
                                return entry ? entry.detail : ""
                            }
                            color: root.dim
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            visible: Speech.phase === "downloading"
                            from: 0; to: 100; value: Speech.progress
                            indeterminate: Speech.phase === "extracting"
                            background: Rectangle { radius: 3; color: root.surfaceUp; implicitHeight: 6 }
                            contentItem: Item {
                                Rectangle {
                                    width: parent.width * parent.visualPosition
                                    height: 6; radius: 3
                                    color: root.accent
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 7
                            visible: Speech.phase !== "ready" && Speech.phase !== "downloading" && Speech.phase !== "extracting"
                            PillButton {
                                text: Speech.phase === "error" ? qsTr("Retry download") : qsTr("Download model (~230 MB, once)")
                                onClicked: Speech.install()
                            }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: Speech.phase === "ready"
                            Item { Layout.fillWidth: true }
                            PillButton { text: qsTr("Remove model"); onClicked: Speech.remove() }
                        }
                    }

                    Card {
                        title: qsTr("Shortcuts")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                text: qsTr("Toggle dictation")
                                color: root.fg
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            TextField {
                                id: toggleField
                                Layout.preferredWidth: 150
                                text: Settings.shortcuts.toggleKey || "ALT + SPACE"
                                color: root.fg
                                placeholderText: "ALT + SPACE"
                                font.pixelSize: 12
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
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            TextField {
                                id: pttField
                                Layout.preferredWidth: 150
                                text: Settings.shortcuts.pttKey || "ALT + Alt_R"
                                color: root.fg
                                placeholderText: "ALT + Alt_R"
                                font.pixelSize: 12
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
                            spacing: 7
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Applied immediately — press the keys to try.")
                                color: root.dim
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                            }
                            PillButton { text: qsTr("Reset"); onClicked: Backend.resetShortcuts() }
                        }
                    }

                    Card {
                        title: qsTr("About")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Offline voice input for Omarchy")
                                color: root.fg
                                font.pixelSize: 12
                            }
                            // Build-injected release version (vox.pro VERSION),
                            // the same number the package and the GitHub tag use.
                            Label {
                                text: "v" + Backend.version
                                color: root.dim
                                font.pixelSize: 11
                            }
                        }
                    }
                    Item { Layout.preferredHeight: 2 }
                }
            }

            // ---------- History ----------
            ColumnLayout {
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: Backend.history.length > 0
                              ? qsTr("%1 entries — click one to insert").arg(Backend.history.length)
                              : qsTr("History")
                        color: root.dim
                        font.pixelSize: 11
                    }
                    PillButton {
                        text: qsTr("Clear all")
                        visible: Backend.history.length > 0
                        onClicked: Backend.clearHistory()
                    }
                }
                ListView {
                    id: historyList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: Backend.history
                    ScrollBar.vertical: ScrollBar {}
                    delegate: HistoryRow {
                        required property int index
                        required property var modelData
                        entry: modelData
                        // HistoryRow's Layout.fillWidth only works under the
                        // Dictate page's ColumnLayout. A ListView host ignores
                        // layout attachments, so without an explicit width the
                        // row shrinks to its content and the timestamp lands
                        // right next to the text.
                        width: historyList.width
                        onActivate: Backend.useHistoryText(index)
                    }
                }
                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    visible: Backend.history.length === 0
                    text: qsTr("Nothing dictated yet.\nPress Space or the mic, then come back.")
                    color: root.dim
                    font.pixelSize: 12
                }
            }
        }
    }
}
