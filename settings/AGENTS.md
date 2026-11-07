# `settings/` — Settings: working on it

The router for `settings/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `theme_persist.hpp` | the `zengine-theme` file version 2 (`ThemeColor`, `ThemeFont`, `ThemeSize`, `ThemeSlots`, `ThemeSetting`, `ThemeKey`, `ThemeFile`), version 1 (`v1::`) widened, its form, `from_text`/`to_text`, `load_file`/`save_file`, `resolve_theme` and its fingerprint, `list_themes` |
| `apply.hpp` | `plan_apply` (every entry judged, merge and reset), `judge_waiting`, `look_over` and `default_palette` |
| `slots.hpp` | `SlotsDeclared`, `SlotsAsked` and `kSettingsRole`: what a pane with slots speaks |
| `vocabulary.hpp` | office `zengine.settings`, its apply modes and its doors' shapes, `ThemesPlaced` and `ThemeSkipsSaid` |
| `weave.cpp` | loadable weave `zengine-settings`, holding the office |
| `CMakeLists.txt` | `zengine-settings-vocabulary`: INTERFACE, over `loom::core`, the pane protocol, `zengine-input-vocabulary` and `zengine-persist`; the weave, carrying its manual |

The theme file reads nothing of Workshop's: a setting's key and kinds are the pane protocol's
(`workshop/pane_settings.hpp`), a key's spelling the input library's (`input/gesture_grammar.hpp`),
and the bounded read, the safe write and `Written` the persist library's (`persist/`). The weave
speaks to the layouts' owner and the keymap by their seam's words.

## Law

- [themes](../agents/workshop/themes.md) `WL-THEME` — how a theme applies: judged whole first, merge
  and reset, locks a final layer, slots that wait; witnessed by `settings_themes`, then
  `workshop_desk`

Which palette an area wears is Workshop's settings law, WL-SETTING-13; a theme's setting is judged
by the rules Workshop's settings keep (`agents/workshop/settings.md`), and a theme's key by the
keymap's (`agents/workshop/keyboard.md`, `agents/workshop/key-spelling.md`), routed by
[workshop/AGENTS.md](../workshop/AGENTS.md). The office's manual is
[`zengine.settings`](docs/zengine.settings.md).

## Suites

- `settings_themes` — a theme's form, version 2's look and version 1 read, its file read and written,
  the two-hands guard on a rewrite, resolution with its includes, the folder's listing, how a theme
  applies as values (each entry judged, merge and reset, keys, waiting slots, the palette), Flow's
  declared slots against its `slots.json`, and every shape's round trip;
  `tests/test_settings_themes.cpp`, `tests/test_settings_apply.cpp`

Its cases in a shared suite: `workshop_desk` (`tests/test_workshop_desk.cpp`), the weave loaded
beside a live Workshop -- a theme edited on disk and applied, an entry skipped or refusing, merge and
reset, a lock kept, a planned office's slots waiting, several gestures and a version-1 file.

Its shapes are pinned by `workshop_shapes` (`tests/census_headers.hpp`, `tests/shapes.txt`).

## Host side

- Loading and staging: `workshop/default-load-plan.json` loads `zengine-settings` as
  `zengine.settings`; `workshop/CMakeLists.txt` stages it and the shipped themes in `themes/`
  beside the host; `cmake/ZengineInstall.cmake` installs them beside Workshop.
- Told: `workshop/weave_run.cpp` (`place_themes`) sends `ThemesPlaced` whenever another weave holds
  the office or anything told changes, and when asked; the folders are `workshop/workshop.cpp`'s.
- Worn: `workshop/weave_run.cpp` (`worn_theme`, `arrange_graphics`) chooses each area's palette by
  its row's `workshop.theme`, else the live layout's baseline, else Workshop's own, and offers the
  three shipped palettes as the fallback.
- Said: Workshop says a `ThemeSkipsSaid` on its band.
- Slots: Flow declares its slots (`flow/flow-pane/look.hpp`, `hue::declared`).

## Pages

- [Settings](README.md), the front page: what a theme holds, how it resolves and applies, and its
  bounds
- [`zengine.settings`](docs/zengine.settings.md), the office's manual

## Practices

None of its own yet.
