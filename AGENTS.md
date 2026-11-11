# DuckVox — project memory & agent guide

Omarchy-native voice dictation (C++17/Qt6 Quick, qmake). Fork-spirit of the user's
crostivoice ChromeOS extension, rebuilt as a desktop app. Repo: ~/Work/vox →
github.com/pm-parker/vox (branch main). Installed version: **2.0.1-1** (pacman),
brand color duck yellow `#FFD338`, tray/app icon is a yellow duck.

## Architecture (src/, flat per omarchy-app skill)
- `main.cpp` — CLI dispatch (--toggle/--hold-start/--hold-stop/--status/--copy-last/
  --show/--minimized) forwards to control socket; daemon mode = QQuickView + context
  props `Backend`, `Settings` (Store). `qml.load(QUrl("qrc:/vox/ui/Main.qml"))`.
- `backend.{h,cpp}` — states idle/listening/processing; `toggle/holdStart/holdStop/
  repeatLast` (hold ignores silence auto-stop, ends on release).
- `recorder.{h,cpp}` — parec warm keepalive between takes (warmUp at startup,
  killQuiet), 16k mono s16 WAV.
- `engine.{h,cpp}` — sherpa-onnx SenseVoice int8, model dir
  `~/.local/share/vox/models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-…-int8/`.
- `store.{h,cpp}` — settings.json (autoInsert, autoStop, punctStyle, insertMethod,
  model, mic source, duckYellow…); `setSetting()` fires settingsChanged.
- `theme.{h,cpp}` — reads Omarchy theme css; `duckYellow=true` (default) overrides
  accent with `#FFD338` (`Theme::accent()`).
- `controlserver.{h,cpp}` — newline-JSON Unix socket for CLI/panel IPC.
- `statusreporter.{h,cpp}` — JSON status for the bar panel (poll every 2 s).
- `insertor.{h,cpp}` — wtype direct / wl-clipboard+paste / copy-only.
- `ui/Main.qml` — 430×680, THREE TABS (历史/识别/音频 — settings were unreachable
  when they were bottom-of-column; do not regress). Recognize tab: model ComboBox,
  Shortcuts card (4 hotkeys), Start at login. qrc via qmake RESOURCES (no qmldir —
  loadFromModule unusable).
- `tools/screenshot_window.cpp` — offscreen README renderer (NOT in qmake SOURCES;
  built manually with `-I. -Isrc`; covered by compile_flags.txt).

## Omarchy integration contracts (verified)
- Dictation adapter: `/usr/bin/omarchy-dictation-duckvox` (pkgbuild/omarchy-dictation-vox,
  guards/checked JSONC schema), activate: `omarchy dictation use duckvox`.
- Default hotkeys owned by Omarchy: `/usr/share/omarchy/default/hypr/bindings/dictation.lua`
  (ALT+SPACE toggle, ALT+Alt_R hold via o.bind_hold). Custom keys: write
  `~/.config/hypr/dictation/shortcuts.lua` — loaded by dictation-backend.lua
  (paths.config_home + "/" + backend + "/shortcuts.lua").
- Bar panel: ~/.config/omarchy/plugins/vox-panel/ (manifest.json, Service.qml, Panel.qml),
  mounted in ~/.config/omarchy/shell.json as `vox.panel`.
- Menu hooks: ~/.config/omarchy/extensions/omarchy/ (Setup▸Defaults▸Dictation entry).
- Autostart: ~/.config/hypr/autostart.lua → `o.launch_on_start("duckvox --minimized")`.

## Build / install / ship
- Build: `qmake6 && make` in repo root (rc=0 is the authority for C++ truth — stale
  clangd session-cache 🔴s on theme.* have been disproven twice by clean builds).
- Package: `makepkg -fsi` in pkgbuild/ (PKGBUILD version bumps per release).
- Install without TTY sudo: `pkexec pacman -U …` pops a GUI polkit dialog on the
  user's screen (may need the user to click it).
- Push: plain `git push` hits `SSL_read: unexpected eof`; use
  `git -c http.version=HTTP/1.1 push origin main` (recurring network issue).

## Release history
1.1.0 global push-to-talk + bar panel · 1.2.0 settings panel · 1.3.0 model picker +
Shortcuts card + start-at-login · 2.0.0 DuckVox rename, yellow brand, tabbed settings,
tray · 2.0.1 notification dedup + recall-last-notification, build fixes.
