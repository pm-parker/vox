# vox

Voice input for Omarchy — press Space, speak, and your words land in whatever
window has focus. Entirely offline.

vox is an Omarchy-native desktop app built from the ideas of
[crostivoice](https://github.com/david-heinemeier-hansson/crostivoice), a
ChromeOS extension, rebuilt in C++/Qt Quick as a Hyprland/Wayland citizen.

## How it works

- **Space to dictate** — one big mic button and a keyboard-first window.
  Space toggles capture, `R` inserts the last take into the focused window
  (via `wtype`, falling back to `wl-copy`), `C` copies it, `M` downloads the
  model. All plain text — no clipboard managers, no cloud, no accounts.
- **SenseVoice, offline** — recognition runs on the local machine through
  [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) with the
  zh/en/ja/ko/yue SenseVoice model (~230 MB, downloaded once from inside the
  app; a system `sherpa-onnx-offline` is used if present).
- **Omarchy look & feel** — the accent color and corner radius follow the
  live Omarchy theme (`omarchy-theme` output and Hyprland rounding), and the
  interface language follows Omarchy's chosen language (Chinese and English
  catalogs ship in the box).
- **crostivoice niceties, carried over** — silence auto-stop, full-width
  `，。` or half-width `, .` punctuation, a hot-words dictionary applied
  longest-first (`gpt four → GPT-4` wins over `four → 4`), auto-insert into
  the focused window, and a 50-item history you can recall from.

## Install

```sh
bin/install
```

That builds an Arch package and installs it, so `vox` shows up in the Omarchy
app launcher. First launch: press `M` to fetch the model, then Space to talk.

For a debug build without installing: `bin/build`, binary at `build/vox`.
Run the unit tests with `bin/test` (12 cases: store settings/dictionary/
history, theme fallback + Hyprland rounding, translator catalogs and plurals,
backend idle safety).

## Keys

| Key | Action |
| --- | ------ |
| `Space` | Start / stop dictation |
| `R` | Insert last text into the focused window |
| `C` | Copy last text |
| `M` | Download / retry the speech model |

## Dependencies

`qt6-base` `qt6-declarative` `qt6-wayland` `libpulse` (parec)
`wtype` `wl-clipboard` `curl` — the model itself (~230 MB) is fetched on
first run and stored under `~/.local/share/vox/models`.

## License

MIT.
