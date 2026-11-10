import QtQuick
import Quickshell
import Quickshell.Io

// Client for the duckvox daemon's control socket by way of its CLI: every reply
// is one JSON line from `duckvox --status`. Holds no knowledge of vox internals —
// if the daemon renames a field, this file is the only thing to touch.
QtObject {
  id: root

  readonly property string cli: "duckvox"
  // idle | listening | processing — "" when the daemon is not running.
  property string state: ""
  property string lastText: ""
  property string error: ""
  property bool running: false
  property bool busy: statusProcess.running || commandProcess.running

  property bool _refreshAgain: false

  function refresh() {
    if (statusProcess.running) {
      _refreshAgain = true
      return
    }
    statusProcess.command = [cli, "--status"]
    statusProcess.running = true
  }

  function command(flag) {
    if (commandProcess.running)
      return
    commandProcess.command = [cli, flag]
    commandProcess.running = true
  }

  function toggle() {
    command("--toggle")
  }

  function copyLast() {
    command("--copy-last")
  }

  function showWindow() {
    command("--show")
  }

  function readReply(text) {
    // "duckvox is not running" prints to stderr and exits 1 — no JSON to read.
    if (!text || text.trim() === "")
      return
    try {
      const reply = JSON.parse(text)
      root.running = true
      root.state = reply.state || ""
      root.lastText = reply.lastText || ""
      root.error = reply.error || ""
    } catch (e) {
      // A half-written line is not a state change; the next poll fixes it.
    }
  }

  Process {
    id: statusProcess
    stdout: StdioCollector {
      onStreamFinished: root.readReply(text)
    }
    onExited: function(exitCode) {
      if (exitCode !== 0) {
        root.running = false
        root.state = ""
      }
      if (root._refreshAgain) {
        root._refreshAgain = false
        root.refresh()
      }
    }
  }

  Process {
    id: commandProcess
    stdout: StdioCollector {
      onStreamFinished: root.readReply(text)
    }
    onExited: function(exitCode) {
      if (exitCode !== 0) {
        root.running = false
        root.state = ""
      }
    }
  }

  Timer {
    interval: 2000
    running: true
    repeat: true
    triggeredOnStart: true
    onTriggered: root.refresh()
  }

  Component.onCompleted: root.refresh()
}
