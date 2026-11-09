# `theme/` — Themes: working on it

The router for `theme/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `theme_persist.hpp` | a theme's two files: `ThemeFile` (window, version 3), `TerminalFile` (version 1), their form (`check_theme_file`, `check_terminal_file`), `window_from_text`, `terminal_from_text`, `to_text`, `load_theme`, `save_theme`, `resolve_theme_with`, `list_themes`; a version-1 or -2 file refused in words |
| `stack.hpp` | `lay_stack`: a stack laid into one palette, a link of the other medium converted with its warning, own values judged against their roles' declarations |
| `stack_name.hpp` | `stack_palette`, `stack_over`, `kMaxStackThemes`, `kMaxStacksWorn`: what a host names its areas' palettes by, reading no file |
| `vocabulary.hpp` | role `zengine.theme` (`kThemeRole`) and its doors' shapes |
| `weave.cpp` | loadable weave `zengine-theme` |
| `docs/zengine.theme.md` | the role's manual, compiled in |
| `CMakeLists.txt` | `zengine-theme-vocabulary`: INTERFACE, over `loom::core`, `zengine-graphics-vocabulary` and `zengine-persist`; the weave |

## Law

- [themes](../agents/theme.md) — what a theme is, a stack, the other medium's link and a change by
  kind; a pane's own values and the terminal's look are graphics' law, which `graphics/AGENTS.md`
  routes

## Suites

- `theme` — a theme's two files read, written and refused (a version-1 file, which held settings
  and keys; a version-2 one; an unknown member), resolution with its includes per medium (later
  wins, cycles, bounds, a missing include), the folder's listing, a stack laid (over the pool's
  defaults, a link of the other medium converted and warned, own values judged), and the weave
  through the real kernel: the palette offered under the stack's name, laid again when a role
  declares, when the files change and are saved, and kept across a reload; `tests/test_theme.cpp`

Its shapes are pinned by `workshop_shapes` (`tests/census_headers.hpp`, `tests/shapes.txt`).

## Host side

Workshop stages the weave and the shipped themes beside it (`workshop/CMakeLists.txt`), names the
folders and every stack its areas wear (`ThemesPlaced`, `ThemesWorn`), and chooses each area's
palette by its stack's name; its layouts' owner keeps each layout's stack and each row's theme.
The weave names no host. Its files are read and written through `persist/`.

## Pages

- [Themes](README.md), the front page: what a theme's files hold, a stack, and the bounds
- [`zengine.theme`](docs/zengine.theme.md), the role's manual

## Practices

- A theme is the look alone: nothing here reads or writes a setting or a key.
- Nothing here assumes Workshop: a host is whoever says where the themes are and what is worn.
