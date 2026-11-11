---@diagnostic disable: undefined-global
-- DuckVox is a 440-wide utility panel that sizes itself (it clamps its own
-- height to the screen). Omarchy loads every file in default/hypr/apps.
-- Deliberately NO `size` here: an explicit 430x940 used to override the
-- window's own geometry and Hyprland then stretched it over the full screen
-- height, which buried the settings list.
o.window({ class = "duckvox$" }, { tag = "+duckvox" })
o.window({ tag = "duckvox" }, {
  float = true,
  move = { "(monitor_w-w-24)", "(monitor_h*0.16)" },
  no_initial_focus = true, -- hotkey-first app: never steal focus at autostart
})
