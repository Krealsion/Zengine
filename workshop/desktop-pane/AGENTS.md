# `workshop/desktop-pane/` — the Desktop weave: working on it

The router for `workshop/desktop-pane/`, the part of the host built as `zengine-desktop-pane`: which
file holds which piece of it. It states no law. Its law, its suites and its pages are routed by
the host's router, [`workshop/AGENTS.md`](../AGENTS.md).

## Files

| file | what it holds |
|---|---|
| `pane.cpp` | `DesktopWeave`, the weave: it offers the three panes, declares their rows and the application's own (`app_rows`), numbers every declaration and every launch, and hands each message to the pane it is about |
| `desktop_common.hpp` | what the weave and its panes share: the offices and the tools they name, `launch` and `close`, a menu's subject, `from_workshop`, `refused_ask`, `PanePress`, `Declared`, `PaneCanvas` |
| `launcher_view.hpp` | `LauncherView`, the Pane Manager: the inventory list, its marker held by identity, the mark that shows or hides, a row's menu, and `n` for the View Builder |
| `hotkeys_view.hpp` | `HotkeysView`, the Hotkeys pane: the keymap in force as a table, its cursor, a binding's menu, key capture and the spelling line |
| `manual_view.hpp` | `ManualView`, the Manual: the ask for a pane's manual and the window over its lines |
| `manual_reader.hpp` | a manual's words set beside the keys in force (`manual_lines`) |
| `shortcuts.hpp` | the application rows other providers propose (`Shortcuts`) |
| `vocabulary.hpp` | the durable names: the office, the panes, the action ids, and `DesktopState` |

## How the pieces meet

- A view owns its pane's room, picture, cursor, notice and asks. The weave owns what more than one
  pane reads: the reload shape (`DesktopState`), the declarations and their attempt counter, and
  the one counter every launch, close and toggle goes out under. It passes each view those.
- A pane's mode is the weave's to declare: the Hotkeys pane asks for it through `DeclareKeys`,
  so the declaration goes out before what the act sends next.
- The Manual reads the keymap the Hotkeys pane holds, handed over by the weave; no view reads
  another.
