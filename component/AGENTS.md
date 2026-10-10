# `component/` — The pieces a pane is built from: working on it

The router for `component/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `CMakeLists.txt` | `zengine-component` (`zengine::component`): header-only, links nothing |
| `text_box.hpp` | `TextBox`, `Clipboard`, the character and word rules, `kEditingVocabulary` |
| `list_window.hpp` | `ListWindow`: what a bounded place shows of a list, and what it omitted |
| `row_map.hpp` | `RowMap<Meaning>`: what each published row and column span means |
| `held_choice.hpp` | `HeldChoice<Key>`: a choice kept by identity across a list that moves |
| `control_strip.hpp` | `ControlStrip`, `pack_controls`: labelled controls packed into a width |
| `columns.hpp` | `Column`, `layout_columns`: a table's columns, laid out once for every row |
| `motion.hpp` | `motion::Path`: linear and cubic Bezier sampling, elapsed-time `progress` |
| `docs/component.md` | the reference page |

## Law

- [text-box](../agents/workshop/text-box.md) — the text box; witnessed by `component`, then
  `workshop_document` and `workshop_panes` (terminal, info, input)

The controls a pane draws for a hand, over `control_strip.hpp`, `list_window.hpp` and `row_map.hpp`,
are `agents/workshop/pane-controls.md` (`WL-HAND`), which [workshop/AGENTS.md](../workshop/AGENTS.md)
routes.

## Suites

- `component` — `TextBox`, `ListWindow`, `RowMap`, `HeldChoice` and columns as values:
  `tests/test_component.cpp`, its own executable linking only `zengine-component`

Its cases in shared suites: `workshop_panes` (`tests/test_workshop_panes_terminal.cpp`, `_info.cpp`
and `_input.cpp` for the text box; `_builder.cpp`, `_files.cpp`, `_output.cpp` and `_canvas.cpp` for
the controls), `workshop_document` (`tests/test_workshop_document.cpp`: the name editor and the
Composer's fields) and `input` (`tests/test_input.cpp` pins `text_box.hpp`'s key
spellings to the wire's). Outside CTest, `tests/package/run.cmake` builds
`tests/package/public_surface.cpp` and `tests/package/pane_menu_consumer.cpp` against the
installed headers.

## Host side

- `workshop/CMakeLists.txt` links it into `zengine-workshop-vocabulary`;
  `cmake/ZengineInstall.cmake` installs its headers and exports `zengine::component`.
- `workshop/screen.hpp` holds the session's `Clipboard`; `workshop/setup.hpp` `LayoutNaming` and
  `workshop/weave_session.cpp` `naming_key` are the name editor's `TextBox`.
- `workshop/pane_parts.hpp` reads a `RowMap`; `workshop/keymap.hpp` reads `kEditingVocabulary`.
- Consumers: `builder/builder-pane/`, `editor/editor-pane/`, `files/`, `flow/flow-pane/`,
  `info/`, `inventory/inventory-pane/`, `terminal/`, `workshop/desktop-pane/`,
  `workshop/menu-presenter/`, `composer/draft.hpp`, `view/`, `introspection/powers.hpp`;
  `input/input_weave.hpp` uses `motion.hpp`.

## Pages

- [The Component package](docs/component.md)

## Practices

- [Components](../docs/contributing/best-practices.md#components-component)
