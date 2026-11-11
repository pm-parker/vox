---@diagnostic disable: undefined-global
-- DuckVox: a 430x940 utility window. Tiled, the 720-tall screen clamps it to
-- ~328 px - the settings list then scrolls in a slit and looks half-broken.
-- Omarchy loads every file in default/hypr/apps, so this floats it full-size
-- near the top-right (the pip.lua pattern: tag, then a tag rule).
o.window({ class = "duckvox$" }, { tag = "+duckvox" })
o.window({ tag = "duckvox" }, {
  float = true,
  size = { 430, 940 },
  move = { "(monitor_w-w-40)", "(monitor_h*0.04)" },
  no_initial_focus = true, -- hotkey-first app: never steal focus at autostart
})
