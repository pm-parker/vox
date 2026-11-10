import QtQuick
import QtQuick.Controls
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons
import qs.Ui

Panel {
  id: root
  moduleName: "vox"
  ipcTarget: "vox"
  manageIpc: false

  property bool centredOnBar: false

  // Theme, the way the Superwhisper panel reads it, with the bar's own values
  // when the widget sits in the bar and shell fallbacks when it does not.
  readonly property color foreground: bar ? bar.foreground : Commons.Color.foreground
  readonly property color urgent: bar ? bar.urgent : Commons.Color.urgent
  readonly property color dim: Qt.darker(foreground, 1.55)
  readonly property string fontFamily: bar ? bar.fontFamily : Style.font.family
  readonly property real iconSize: Math.max(8, Math.min(24, Number(setting("iconSize", 20))))
  readonly property real barGlyphSize: Style.bar.iconSize > 0
    ? Style.bar.iconSize * iconSize / 20
    : Style.space(iconSize)

  // idle reads dim while the daemon is off, plain while it waits; listening
  // shouts in the urgent color; processing pulses.
  readonly property bool live: sw.running
  readonly property color glyphColor: !live ? dim
    : sw.state === "listening" ? urgent
    : foreground

  function inMiddleOfBar() {
    var window = root.QsWindow.window
    if (!window) return false
    var vertical = root.bar && root.bar.vertical
    var at = root.mapToItem(null, root.width / 2, root.height / 2)
    var span = vertical ? window.height : window.width
    var middle = vertical ? at.y : at.x
    return span > 0 && middle > span / 3 && middle < span * 2 / 3
  }

  function openPanel() {
    root.centredOnBar = root.inMiddleOfBar()
    root.open()
    if (root.centredOnBar) Qt.callLater(function() {
      if (root.opened && root.bar) root.bar.setCenterHoverRevealSuppressed(true)
    })
  }

  function togglePanel() {
    if (root.opened) root.close()
    else root.openPanel()
  }

  onOpenedChanged: if (!opened && centredOnBar && bar) {
    bar.setCenterHoverRevealSuppressed(false)
  } else if (opened) {
    sw.refresh()
    Qt.callLater(function() { keyCatcher.forceActiveFocus() })
  }

  Service {
    id: sw
  }

  IpcHandler {
    target: root.ipcTarget
    function open(): void { root.openPanel() }
    function close(): void { root.close() }
    function show(): void { root.openPanel() }
    function hide(): void { root.close() }
    function toggle(): void { root.togglePanel() }
    function refresh(): string { sw.refresh(); return "ok" }
    function dictate(): string { sw.toggle(); return sw.state }
  }

  Component.onCompleted: sw.refresh()

  // The mic, drawn as vector strokes so it follows the theme color and the
  // bar's icon size; an emoji glyph would bring its own palette.
  component MicGlyph: Item {
    property color color
    property real size: 20
    property bool pulse: false
    width: size
    height: size

    SequentialAnimation on opacity {
      running: pulse
      loops: Animation.Infinite
      NumberAnimation { to: 0.35; duration: 650; easing.type: Easing.InOutQuad }
      NumberAnimation { to: 1; duration: 650; easing.type: Easing.InOutQuad }
    }

    Canvas {
      anchors.fill: parent
      onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        // `color` unqualified: this component's own property. `root.` would
        // resolve to the Panel's root id and leave the stroke undefined.
        ctx.strokeStyle = Qt.rgba(color.r, color.g, color.b, 1)
        ctx.lineWidth = Math.max(1, parent.width / 13)
        ctx.lineCap = "round"
        var w = parent.width, h = parent.height
        // Capsule body.
        ctx.beginPath()
        ctx.roundedRect(w * 0.34, h * 0.10, w * 0.32, h * 0.42, w * 0.16, w * 0.16)
        ctx.stroke()
        // Cradle arc.
        ctx.beginPath()
        ctx.moveTo(w * 0.24, h * 0.42)
        ctx.arc(w * 0.5, h * 0.42, w * 0.26, Math.PI, 0, false)
        ctx.stroke()
        // Stem and base.
        ctx.beginPath()
        ctx.moveTo(w * 0.5, h * 0.68)
        ctx.lineTo(w * 0.5, h * 0.82)
        ctx.moveTo(w * 0.36, h * 0.86)
        ctx.lineTo(w * 0.64, h * 0.86)
        ctx.stroke()
      }
    }
    onColorChanged: repaintCanvas()
    onPulseChanged: repaintCanvas()

    function repaintCanvas() {
      if (children[0] && children[0].requestPaint)
        children[0].requestPaint()
    }
  }

  component VoxButton: Rectangle {
    property string label
    property string hint
    signal activated()

    radius: Style.space(8)
    color: area.containsMouse ? Qt.rgba(foreground.r, foreground.g, foreground.b, 0.10)
                              : Qt.rgba(foreground.r, foreground.g, foreground.b, 0.05)
    border.color: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.14)
    border.width: 1
    implicitHeight: Style.space(34)

    Text {
      anchors.left: parent.left
      anchors.leftMargin: Style.space(12)
      anchors.verticalCenter: parent.verticalCenter
      text: parent.label
      color: foreground
      font.family: root.fontFamily
      font.pixelSize: 12
    }
    Text {
      anchors.right: parent.right
      anchors.rightMargin: Style.space(12)
      anchors.verticalCenter: parent.verticalCenter
      text: parent.hint
      color: dim
      font.family: root.fontFamily
      font.pixelSize: 11
    }
    MouseArea {
      id: area
      anchors.fill: parent
      hoverEnabled: true
      cursorShape: Qt.PointingHandCursor
      onClicked: parent.activated()
    }
  }

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    iconComponent: Component { Item {} }

    MicGlyph {
      x: (button.width - width) / 2
      y: (button.height - height) / 2
      size: root.barGlyphSize
      color: root.glyphColor
      pulse: root.live && sw.state === "processing"
    }

    onPressed: function(buttonCode) {
      if (buttonCode === Qt.RightButton) sw.showWindow()
      else root.togglePanel()
    }
  }

  KeyboardPanel {
    id: panel
    anchorItem: button
    owner: root
    bar: root.bar
    open: root.opened
    centerOnBar: root.centredOnBar
    focusTarget: keyCatcher
    contentWidth: panel.fittedContentWidth(Style.space(360))
    contentHeight: panel.fittedContentHeight(column.implicitHeight, Style.space(420))

    PanelKeyCatcher {
      id: keyCatcher
      anchors.fill: parent
      onCloseRequested: root.close()
      onTextKey: function(t) {
        var key = t.toLowerCase()
        if (key === "d") sw.toggle()
        else if (key === "c") sw.copyLast()
        else if (key === "o") { sw.showWindow(); root.close() }
        else if (key === "r") sw.refresh()
      }

      Flickable {
        id: panelFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        interactive: contentHeight > height
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
          id: column
          width: parent.width
          spacing: Style.space(12)

          // Hero: glyph, name, live state.
          Item {
            width: parent.width
            height: Style.space(40)
            MicGlyph {
              id: heroGlyph
              x: Style.space(2)
              anchors.verticalCenter: parent.verticalCenter
              size: Style.space(30)
              color: root.glyphColor
              pulse: root.live && sw.state === "processing"
            }
            Column {
              anchors.left: heroGlyph.right
              anchors.leftMargin: Style.space(12)
              anchors.verticalCenter: parent.verticalCenter
              spacing: Style.space(2)
              Text {
                text: "Vox"
                color: foreground
                font.family: root.fontFamily
                font.pixelSize: 13
                font.weight: Font.DemiBold
              }
              Text {
                text: !root.live ? "Not running"
                  : sw.state === "listening" ? "Listening — release or press D to stop"
                  : sw.state === "processing" ? "Processing…"
                  : "Idle — press D to dictate"
                color: dim
                font.family: root.fontFamily
                font.pixelSize: 11
              }
            }
          }

          Rectangle {
            width: parent.width
            height: 1
            color: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.12)
          }

          Text {
            text: "LAST TRANSCRIPTION"
            color: dim
            font.family: root.fontFamily
            font.pixelSize: 10
            font.letterSpacing: 0.8
          }

          Text {
            width: parent.width
            text: sw.error !== "" ? sw.error
              : sw.lastText !== "" ? sw.lastText
              : "Nothing dictated yet."
            color: sw.error !== "" ? urgent
              : sw.lastText !== "" ? foreground : dim
            wrapMode: Text.WordWrap
            maximumLineCount: 6
            elide: Text.ElideRight
            font.family: root.fontFamily
            font.pixelSize: 12
          }

          VoxButton {
            width: parent.width
            label: "Copy last transcription"
            hint: "C"
            activated: sw.copyLast
          }
          VoxButton {
            width: parent.width
            label: root.live && sw.state === "listening" ? "Stop dictating" : "Start dictating"
            hint: "D"
            activated: sw.toggle
          }
          VoxButton {
            width: parent.width
            label: "Open the vox window"
            hint: "O"
            activated: function() { sw.showWindow(); root.close() }
          }
        }
      }
    }
  }
}
