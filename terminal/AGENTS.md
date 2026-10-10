# `terminal/` — The Terminal: working on it

The router for `terminal/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `pane.cpp` | `zengine-terminal-pane`, a loadable weave: record view, line, recall, completion |
| `vocabulary.hpp` | its durable names: office `zengine.terminal`, pane, stem, actions, `legend` |
| `CMakeLists.txt` | builds the weave; skipped where Loom cannot host loadable weaves |
| `docs/terminal.md` | the weaver's page; its screenshots in `docs/images/` |

## Law

- [terminal-pane](../agents/workshop/terminal-pane.md) — the Terminal pane's line and record;
  witnessed by `workshop_panes` (terminal, legend)
- [terminal](../agents/workshop/terminal.md) — the Terminal; witnessed by `workshop_panes`
  (terminal, inventory info, powers), then `workshop_host`

## Suites

No CTest entry of its own. Its cases in shared suites: `workshop_panes`
(`tests/test_workshop_panes_terminal.cpp`, `tests/test_workshop_panes_legend.cpp`, and Terminal
cases in `tests/test_workshop_inventory_info.cpp`, `tests/test_workshop_panes_powers.cpp` and
`tests/test_workshop_panes_guests.cpp`), which also reads `pane.cpp` and `CMakeLists.txt` as
text; `workshop_host` (`tests/test_workshop_host.cpp`, the line's completion vocabulary);
`workshop_load` (`tests/test_workshop_load.cpp`, the shipped plan's row); `workshop_files`
(`tests/test_workshop_files.cpp`, the development catalog's entry).

## Host side

- The participant (`loom::TerminalSession`) is mounted and granted in `workshop/workshop.cpp`;
  its asks beyond the skin are added by `workshop/editor_switch.hpp` and `workshop/powers_door.hpp`.
- The seam: `workshop/terminal_seam_vocabulary.hpp`, the office in
  `workshop/pane_seam_vocabulary.hpp`, the host's grant to say it in `workshop/grant.cpp`.
- The door `workshop/weave_terminal.cpp`, the picture `workshop/screen_terminal.cpp`, the
  completer `workshop/complete.hpp`.
- Staged and catalogued in `workshop/CMakeLists.txt`; its plan rows in
  `workshop/default-load-plan.json` and `workshop/graphical-load-plan.json`.
- A guest's hand toward it: `workshop/weave_operation.cpp` (`refused_toward`); `Ctrl`+`t`: the
  desktop's `desktop.terminal` row, `workshop/desktop-pane/pane.cpp`.

## Pages

- [Terminal](docs/terminal.md)

## Practices

- [The Terminal](../docs/contributing/best-practices.md#the-terminal-terminal)
