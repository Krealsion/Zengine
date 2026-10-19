# `settings/` — Settings: working on it

The router for `settings/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `theme_persist.hpp` | the `zengine-theme` file (`ThemeSetting`, `ThemeKey`, `ThemeFile`), its form, `from_text`/`to_text`, `load_file`/`save_file`, `resolve_theme` and its fingerprint, `list_themes` |
| `vocabulary.hpp` | office `zengine.settings` (`kSettingsRole`), its apply modes and its doors' shapes |
| `CMakeLists.txt` | `zengine-settings-vocabulary`: INTERFACE, over `loom::core` and `zengine-workshop-vocabulary` |

## Law

No register of its own under `agents/`. A theme's setting is judged by the rules Workshop's
settings keep, `agents/workshop/settings.md`, and a theme's key by the keymap's,
`agents/workshop/keyboard.md` and `agents/workshop/key-spelling.md`; all are routed by
[workshop/AGENTS.md](../workshop/AGENTS.md).

## Suites

- `settings_themes` — a theme's form, its file read and written, the two-hands guard on a
  rewrite, resolution with its includes (later wins, cycles, bounds, a broken include), the
  folder's listing, and every shape's round trip; `tests/test_settings_themes.cpp`

Its shapes are pinned by `workshop_shapes` (`tests/census_headers.hpp`, `tests/shapes.txt`).

## Host side

None: no weave holds `zengine.settings`, and Workshop reads no theme. A theme's settings and
keys are judged by `workshop/pane_settings.hpp` and `workshop/gesture_grammar.hpp`, and its file is
read and written through `workshop/persist.hpp`.

## Pages

- [Settings](README.md), the front page: what a theme holds, how it resolves, and its bounds

## Practices

None of its own yet.
