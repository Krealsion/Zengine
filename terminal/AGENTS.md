# `terminal/` — The Terminal: working on it

The router for `terminal/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| path | what it is |
|---|---|
| `pane.cpp` | `zengine-terminal-pane`, a loadable weave: its offer, setting, keys, presses, pickup and act |
| `view.hpp` | the record wrapped into rows, the view onto them, and the pane's rows and caret composed |
| `history_and_completion.hpp` | the commands recalled from the record, and the completion ask and its list's cursor |
| `vocabulary.hpp` | its durable names: role `zengine.terminal`, pane, stem, actions, `legend` |
| `CMakeLists.txt` | builds the weave; skipped where Loom cannot host loadable weaves |
| `docs/zengine.terminal.md` | the pane's manual, compiled into the weave and answered when asked |
| `docs/terminal.md` | the weaver's how-to; its screenshots in `docs/images/` |

The host's door onto the participant, its completer and the picture of its record are Workshop's,
`workshop/terminal/`: Workshop's weave composes the door and compiles all three, and the pane
includes none of them (WL-TERM-08). `view.hpp` and
`history_and_completion.hpp` are the pane's: they define members of the class `pane.cpp` declares,
and only `pane.cpp` includes them, after the class.

## Law

- [terminal-pane](../agents/workshop/terminal-pane.md) — the Terminal pane's line and record;
  witnessed by `workshop_panes_terminal` (terminal), `workshop_panes_settings` (legend)
- [terminal](../agents/workshop/terminal.md) — the Terminal pane and its seam; witnessed by
  `workshop_panes_terminal` (terminal); the participant's door and completer are Workshop's law

## Suites

No CTest entry of its own. Its cases in shared suites: `workshop_panes_terminal`
(`tests/test_workshop_panes_terminal.cpp`, which also reads `pane.cpp` and `CMakeLists.txt` as text,
and Terminal cases in `tests/test_workshop_panes_powers.cpp`), `workshop_panes_settings`
(`tests/test_workshop_panes_legend.cpp`), and Terminal cases in `workshop_panes_info`
(`tests/test_workshop_inventory_info.cpp`) and `workshop_panes_hand`
(`tests/test_workshop_panes_guests.cpp`); `workshop_host` (`tests/test_workshop_host.cpp`, the
line's completion vocabulary); `workshop_load` (`tests/test_workshop_load.cpp`, the shipped plan's
row); `workshop_files` (`tests/test_workshop_files.cpp`, the development catalog's entry).

## Host side

- The participant (`loom::TerminalSession`) is mounted and granted in `workshop/workshop.cpp`;
  its asks are added by `workshop/editor_switch.hpp` and `workshop/powers_door.hpp`.
- The seam: `workshop/terminal_seam_vocabulary.hpp`, the role in
  `workshop/pane_seam_vocabulary.hpp`, the host's grant to say it in `workshop/grant.cpp`.
- The door `workshop/terminal/door.hpp`, composed onto Workshop's weave in `workshop/weave.hpp`;
  the picture `workshop/terminal/transcript.hpp`; the completer `workshop/terminal/complete.hpp`;
  their law `agents/workshop/terminal-door.md`, routed by Workshop's router. A record's value is captured for a gesture in
  `workshop/weave_operation.cpp` (`TerminalValueRequested`).
- Staged and catalogued in `workshop/CMakeLists.txt`; its plan row in
  `workshop/default-load-plan.json`.
- A guest's hand toward it: `workshop/weave_operation.cpp` (`refused_toward`); `Ctrl`+`t`: the
  desktop's `desktop.terminal` row, `workshop/desktop-pane/pane.cpp`.

## Pages

- [`zengine.terminal`](docs/zengine.terminal.md)
- [Terminal](docs/terminal.md)

## Practices

- [The Terminal](../docs/contributing/best-practices.md#the-terminal-terminal)
