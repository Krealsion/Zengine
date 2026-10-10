# `workshop/` — Workshop, the host: working on it

The router for `workshop/`, the host: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| part | what it is |
|---|---|
| `desktop-pane/` | the Pane Manager and Hotkeys: `zengine-desktop-pane`, a loadable weave; its law: desktop, desktop-presenting, keymap-edit, keyboard |
| `menu-presenter/` | shows a pane's menu: `zengine-menu-presenter`, a loadable weave; its law: pane-menu, and the second button in `agents/panes.md` |
| `workshop.cpp` | the host's `main`: `zengine-workshop` |
| `weave.hpp`, `weave_*.cpp` | Workshop's own weave: the session, input bound to gestures |
| `screen.hpp`, `screen_*.cpp` | the screen: session facts and gestures, composed into a canvas |
| `panes.hpp`, `keymap.hpp`, `setup.hpp` | pane catalog; action catalog and keymap; the setup |
| `pane_vocabulary.hpp` | the pane protocol, installed: `zengine-pane-vocabulary`, `zengine::pane` |
| `*_seam_vocabulary.hpp`, `pane_doors.hpp` | the seams to loaded panes and guests; the doors |
| `*_persist.hpp` | the files: setup, session, keymap, preferences, recipes, load plan |
| `load_*.hpp`, `*-load-plan.json` | load plan and executor (`zengine-workshop-load`); the plans |
| `guests.hpp`, `guest_door.hpp` | the guests file and the guest door: `zengine-workshop-guests` |
| `develop.cpp`, `prepare-runtime.cmake` | the development launch: `zengine-workshop-develop` |

The vocabulary is header-only (`zengine-workshop-vocabulary`), its bodies compiled once into
`zengine-workshop-logic`; `session_migration_provider.cpp` is `zengine-workshop-session-history`.
The pane protocol's law is `agents/panes.md`, which the root routes.

## Law

Each register routed here, with the suites its PROVEN BY cites, most-cited first (`workshop_panes`
with its files); the case that pins a law is a grep over the register, and the subjects are in the
[WL router](../agents/workshop.md#where-the-law-is).

| register | witnessed by |
|---|---|
| [workshop](../agents/workshop.md) | the WL router: every register by subject |
| [realization](../agents/realization.md) | no PROVEN BY: the one test its prose names is `tests/test_operator_provider.cpp` (`operator`) |
| [arrangement](../agents/workshop/arrangement.md) | `workshop_panes` (window, input), `workshop_screen`, `workshop_host` |
| [arrangement-snap](../agents/workshop/arrangement-snap.md) | `workshop_screen` |
| [canvas](../agents/workshop/canvas.md) | `workshop_panes` (canvas, inventory_info), `flow_pane`, `surface` |
| [catalog](../agents/workshop/catalog.md) | `workshop_panes` (seam) |
| [chrome](../agents/workshop/chrome.md) | `workshop_screen`, `workshop_panes` (window), `surface` |
| [code](../agents/workshop/code.md) | `workshop_panes` (code), `workshop_files` |
| [contextual](../agents/workshop/contextual.md) | `workshop_host`, `workshop_panes` (button, window), `workshop_screen` |
| [desk-read](../agents/workshop/desk-read.md) | `workshop_desk` |
| [desktop](../agents/workshop/desktop.md) | `workshop_panes` (actions, input), `workshop_load`, `workshop_files` |
| [desktop-presenting](../agents/workshop/desktop-presenting.md) | `workshop_panes` (actions, desktop), `workshop_document` |
| [document](../agents/workshop/document.md) | `workshop_document` |
| [document-file](../agents/workshop/document-file.md) | `workshop_persistence`, `workshop_document` |
| [focus](../agents/workshop/focus.md) | `workshop_panes` (input, files), `workshop_desk`, `workshop_document` |
| [geometry](../agents/workshop/geometry.md) | `workshop_screen`, `workshop_desk`, `workshop_panes` (window, opening) |
| [guests](../agents/workshop/guests.md) | `workshop_panes` (guests, builder), `workshop_guests`, `workshop_host` |
| [keyboard](../agents/workshop/keyboard.md) | `workshop_document`, `workshop_panes` (actions, editor), `workshop_screen` |
| [keymap-edit](../agents/workshop/keymap-edit.md) | `workshop_panes` (desktop) |
| [layouts](../agents/workshop/layouts.md) | `workshop_screen`, `workshop_persistence`, `workshop_host` |
| [maker-pane](../agents/workshop/maker-pane.md) | `workshop_panes` (actions), `workshop_host` |
| [migration](../agents/workshop/migration.md) | `workshop_persistence`, `workshop_load` |
| [opening](../agents/workshop/opening.md) | `workshop_panes` (editor, opening) |
| [pane-caret](../agents/workshop/pane-caret.md) | `workshop_panes` (seam) |
| [pane-controls](../agents/workshop/pane-controls.md) | `workshop_panes` (builder, files) |
| [pane-manager](../agents/workshop/pane-manager.md) | `workshop_host`, `workshop_panes` (settings) |
| [pane-menu](../agents/workshop/pane-menu.md) | `workshop_panes` (button, desktop) |
| [pane-parts](../agents/workshop/pane-parts.md) | `workshop_desk`, `workshop_panes` (desktop, canvas), `composer` |
| [panes-and-windows](../agents/workshop/panes-and-windows.md) | `workshop_panes` (window, seam), `workshop_screen`, `workshop_host` |
| [planes](../agents/workshop/planes.md) | `workshop_screen`, `workshop_panes` (window, seam), `workshop_document` |
| [pointer](../agents/workshop/pointer.md) | `workshop_panes` (editor, input), `workshop_screen`, `surface` |
| [press-chain](../agents/workshop/press-chain.md) | `workshop_panes` (input, files), `workshop_screen`, `workshop_host` |
| [regions](../agents/workshop/regions.md) | `workshop_screen`, `workshop_host`, `workshop_panes` (builder) |
| [session](../agents/workshop/session.md) | `workshop_persistence`, `workshop_panes` (editor), `workshop_document` |
| [session-restore](../agents/workshop/session-restore.md) | `workshop_persistence`, `surface` |
| [settings](../agents/workshop/settings.md) | `workshop_panes` (settings, opening), `workshop_persistence` |
| [setup-file](../agents/workshop/setup-file.md) | `workshop_panes` (window, seam), `workshop_persistence`, `workshop_screen` |
| [tab-run](../agents/workshop/tab-run.md) | `workshop_screen`, `workshop_host`, `workshop_persistence` |

## Suites

- `workshop_desk` — the desk and its words, read by message; `tests/test_workshop_desk*.cpp`
- `workshop_document` — editable rows, quit policy, the keymap; `tests/test_workshop_document.cpp`
- `workshop_files` — project browser, recipes, development launch; `tests/test_workshop_files.cpp`
- `workshop_guests` — the guest door and guests file on loopback; `tests/test_workshop_guests.cpp`
- `workshop_host` — what Workshop presents around the panes; `tests/test_workshop_host.cpp`
- `workshop_load` — the load plan, its codec and its executor; `tests/test_workshop_load.cpp`
- `workshop_panes` — the pane seam from both sides; `tests/test_workshop_panes_*.cpp` and the rest
- `workshop_persistence` — what survives a process; `tests/test_workshop_persistence.cpp`
- `workshop_screen` — whole screens, and what a press reaches; `tests/test_workshop_screen.cpp`

All but `workshop_guests` are built by `zengine_workshop_suite` (`tests/CMakeLists.txt`). The
parts' cases are in `workshop_panes`: `tests/test_workshop_panes_desktop.cpp` (the Pane Manager
and Hotkeys) and `tests/test_workshop_panes_button.cpp` (the menu presenter). `operator`,
`builder` and `files_weave` read files here as text; the root's `workshop_shapes` pins every shape.

## Host side

Workshop is the host: each feature's router names the files here that serve it. The top-level
`CMakeLists.txt` adds this folder and its parts; `cmake/ZengineInstall.cmake` installs the pane
protocol's headers and exports `zengine::pane`.

## Pages

Its pages, each with what it is for, are listed on [its front page](README.md#pages).

## Practices

- [The host](../docs/contributing/best-practices.md#the-host-workshop)
- [The Pane Manager and
  Hotkeys](../docs/contributing/best-practices.md#the-pane-manager-and-hotkeys-workshopdesktop-pane)
- [The menu
  presenter](../docs/contributing/best-practices.md#the-menu-presenter-workshopmenu-presenter)
