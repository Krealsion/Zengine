# `workshop/` — Workshop, the host: working on it

The router for `workshop/`, the host: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| part | what it is |
|---|---|
| `desktop-pane/` | the Pane Manager and Hotkeys: `zengine-desktop-pane`, a loadable weave; its law: desktop, desktop-presenting, keymap-edit, keyboard |
| `menu-presenter/` | shows a pane's menu: `zengine-menu-presenter`, a loadable weave; its law: pane-menu, and the second button in `agents/panes.md` |
| `palette/` | the Command Palette: `zengine-palette`, a loadable weave listing every action and pane by name and asking Workshop to run one (`command_vocabulary.hpp`); its law: WL-DESK-18 in `agents/workshop/desktop.md` |
| `workshop.cpp` | the host's `main`: `zengine-workshop` |
| `weave.hpp`, `weave_*.cpp` | Workshop's own weave: the session, input bound to gestures; it composes the Terminal's door, `terminal/door.hpp` |
| `desk_read.hpp`, `weave_desk_read.cpp` | the desk read by message: where words and parts stand and how a reading pages (the header); the weave's answers; its law: desk-read |
| `weave_inspection.cpp` | a pane as an inspector's subject, and one write through its rows' owner; its law: info-body |
| `screen.hpp`, `screen_*.cpp` | the screen: session facts and gestures, composed into a canvas |
| `panes.hpp` | the pane catalog |
| `keymap.hpp`, with its parts `gesture_grammar.hpp` and `keymap_join.hpp` | the action catalog and the keymap; how a key is written and read; override admission and the joins of a pane's and an application's rows |
| `setup.hpp`, with its part `setup_reconcile.hpp` | the setup: its value, its checks and its edits; what the live panes make of it |
| `layouts/` | the desk's owner, `zengine.layouts`, mounted in-process: the run of layouts and its store (`run.hpp`, `store.hpp`), the office (`owner.hpp`), its vocabulary, and the session it alone writes (`session_persist.hpp`, `session_history.hpp`); its law: layouts, tab-run, session-restore, settings, setting-locks |
| `pane_vocabulary.hpp` | the pane protocol, installed: `zengine-pane-vocabulary`, `zengine::pane` |
| `*_seam_vocabulary.hpp`, `pane_doors.hpp` | the seams to loaded panes and guests; the doors |
| `*_persist.hpp` | the files: setup, keymap, preferences, recipes, load plan, and Workshop's window file |
| `load_*.hpp`, `*-load-plan.json` | load plan and executor (`zengine-workshop-load`); the plans |
| `guests.hpp`, `guest_door.hpp` | the guests file and the guest door: `zengine-workshop-guests` |
| `develop.cpp`, `prepare-runtime.cmake` | the development launch: `zengine-workshop-develop` |

The vocabulary is header-only (`zengine-workshop-vocabulary`), its bodies compiled once into
`zengine-workshop-logic`; `session_migration_provider.cpp` is `zengine-workshop-session-history`.
The pane protocol's law is `agents/panes.md`, which the root routes.

## Law

Each register routed here, with the suites its proofs cite, most-cited first (a part of the panes
program with its files); the subjects are in the
[WL router](../agents/workshop.md#where-the-law-is).

| register | witnessed by |
|---|---|
| [workshop](../agents/workshop.md) | the WL router: every register by subject |
| [realization](../agents/realization.md) | no proofs: the one test its prose names is `tests/test_operator_provider.cpp` (`operator`) |
| [action-causes](../agents/workshop/action-causes.md) | `workshop_panes_hand` (actions), `workshop_panes_project` (builder) |
| [arrangement](../agents/workshop/arrangement.md) | `workshop_panes_seam` (window), `workshop_panes_hand` (input), `workshop_screen`, `workshop_host` |
| [arrangement-snap](../agents/workshop/arrangement-snap.md) | `workshop_screen` |
| [canvas](../agents/workshop/canvas.md) | `workshop_panes_seam` (canvas), `workshop_panes_info` (inventory_info), `flow_pane`, `surface` |
| [catalog](../agents/workshop/catalog.md) | `workshop_panes_seam` (seam) |
| [chrome](../agents/workshop/chrome.md) | `workshop_screen`, `workshop_panes_seam` (window), `surface` |
| [code](../agents/workshop/code.md) | `workshop_panes_project` (code), `workshop_files` |
| [contextual](../agents/workshop/contextual.md) | `workshop_host`, `workshop_panes_seam` (button, window), `workshop_screen` |
| [desk-read](../agents/workshop/desk-read.md) | `workshop_desk` |
| [desktop](../agents/workshop/desktop.md) | `workshop_panes_hand` (actions, input), `workshop_load`, `workshop_files` |
| [desktop-presenting](../agents/workshop/desktop-presenting.md) | `workshop_panes_hand` (actions, desktop), `workshop_document` |
| [document](../agents/workshop/document.md) | `workshop_document` |
| [document-file](../agents/workshop/document-file.md) | `workshop_persistence`, `workshop_document` |
| [first-run](../agents/workshop/first-run.md) | `workshop_panes_hand` (first_run) |
| [focus](../agents/workshop/focus.md) | `workshop_panes_hand` (input), `workshop_panes_project` (files), `workshop_desk`, `workshop_document` |
| [geometry](../agents/workshop/geometry.md) | `workshop_screen`, `workshop_desk`, `workshop_panes_seam` (window), `workshop_panes_editor` (opening) |
| [guests](../agents/workshop/guests.md) | `workshop_panes_hand` (guests), `workshop_panes_project` (builder), `workshop_guests`, `workshop_host` |
| [guide](../agents/workshop/guide.md) | `workshop_panes_hand` (guide), `workshop_panes_inspect` (attention), `workshop_guests` |
| [keyboard](../agents/workshop/keyboard.md) | `workshop_document`, `workshop_panes_hand` (actions), `workshop_panes_editor` (editor), `workshop_screen` |
| [keymap-edit](../agents/workshop/keymap-edit.md) | `workshop_panes_hand` (desktop) |
| [key-spelling](../agents/workshop/key-spelling.md) | `workshop_document`, `workshop_panes_hand` (actions), `workshop_panes_project` (builder), `workshop_persistence` |
| [layouts](../agents/workshop/layouts.md) | `workshop_screen`, `workshop_persistence`, `workshop_host` |
| [maker-pane](../agents/workshop/maker-pane.md) | `workshop_panes_hand` (actions), `workshop_host` |
| [migration](../agents/workshop/migration.md) | `workshop_persistence`, `workshop_load` |
| [opening](../agents/workshop/opening.md) | `workshop_panes_editor` (editor, opening) |
| [pane-caret](../agents/workshop/pane-caret.md) | `workshop_panes_seam` (seam) |
| [pane-controls](../agents/workshop/pane-controls.md) | `workshop_panes_project` (builder, files) |
| [pane-manager](../agents/workshop/pane-manager.md) | `workshop_host`, `workshop_panes_settings` (settings) |
| [pane-menu](../agents/workshop/pane-menu.md) | `workshop_panes_seam` (button), `workshop_panes_hand` (desktop) |
| [pane-parts](../agents/workshop/pane-parts.md) | `workshop_desk`, `workshop_panes_hand` (desktop), `workshop_panes_seam` (canvas), `composer` |
| [participants](../agents/workshop/participants.md) | `snake` |
| [panes-and-windows](../agents/workshop/panes-and-windows.md) | `workshop_panes_seam` (window, seam), `workshop_screen`, `workshop_host` |
| [planes](../agents/workshop/planes.md) | `workshop_screen`, `workshop_panes_seam` (window, seam), `workshop_document` |
| [pointer](../agents/workshop/pointer.md) | `workshop_panes_editor` (editor), `workshop_panes_hand` (input), `workshop_screen`, `surface` |
| [press-chain](../agents/workshop/press-chain.md) | `workshop_panes_hand` (input), `workshop_panes_project` (files), `workshop_screen`, `workshop_host` |
| [regions](../agents/workshop/regions.md) | `workshop_screen`, `workshop_host`, `workshop_panes_project` (builder) |
| [session](../agents/workshop/session.md) | `workshop_persistence`, `workshop_panes_editor` (editor), `workshop_document` |
| [session-restore](../agents/workshop/session-restore.md) | `workshop_persistence`, `surface` |
| [window-file](../agents/workshop/window-file.md) | `workshop_persistence`, `graphics` |
| [setting-locks](../agents/workshop/setting-locks.md) | `workshop_panes_settings` (settings), `workshop_panes_hand` (guests), `workshop_panes_terminal` (terminal) |
| [settings](../agents/workshop/settings.md) | `workshop_panes_settings` (settings), `workshop_panes_editor` (opening), `workshop_persistence` |
| [setup-file](../agents/workshop/setup-file.md) | `workshop_panes_seam` (window, seam), `workshop_persistence`, `workshop_screen` |
| [tab-run](../agents/workshop/tab-run.md) | `workshop_screen`, `workshop_host`, `workshop_persistence` |

## Suites

- `workshop_desk` — the desk and its words, read by message; `tests/test_workshop_desk*.cpp`
- `workshop_document` — editable rows, quit policy, the keymap; `tests/test_workshop_document.cpp`
- `workshop_files` — project browser, recipes, development launch; `tests/test_workshop_files.cpp`
- `workshop_guests` — the guest door and guests file on loopback; `tests/test_workshop_guests.cpp`
- `workshop_host` — what Workshop presents around the panes; `tests/test_workshop_host.cpp`
- `workshop_load` — the load plan, its codec and its executor; `tests/test_workshop_load.cpp`
- `workshop_panes_seam` — what an office may offer and who speaks for it, the room, a pane's
  canvas, the second button and the menu presenter, the demo controls;
  `tests/test_workshop_demo.cpp`, `tests/test_workshop_panes_{seam,window,canvas,button}.cpp`
- `workshop_panes_hand` — the weaver's hand crossing the seam: presses and keys, declared
  actions, the Pane Manager and Hotkeys, a guest's hand and a guest that teaches;
  `tests/test_workshop_panes_{input,actions,desktop,guests,guide}.cpp`
- `workshop_panes_inspect` — the panes that read the running system: introspection and what
  sampling costs, Attention and Connections, the View Builder's rows;
  `tests/test_workshop_panes_{introspection,sampling,attention,view_builder}.cpp`
- `workshop_panes_info` — Info and Inventory through their real images;
  `tests/test_workshop_panes_info.cpp`,
  `tests/test_workshop_{inventory_info,inventory_folders,info_views}.cpp`
- `workshop_panes_settings` — a pane's settings across the seam, and one setting end to end;
  `tests/test_workshop_panes_{settings,legend}.cpp`
- `workshop_panes_terminal` — the Terminal as a loaded weave, and finding powers through it;
  `tests/test_workshop_panes_{terminal,powers}.cpp`
- `workshop_panes_project` — the Builder, Files, a pane's code and a build's output, with the
  action census; `tests/test_workshop_panes_{builder,files,code,output}.cpp`
- `workshop_panes_editor` — the Editor and its typed carry, the opening manager;
  `tests/test_workshop_panes_{editor,opening}.cpp`, `tests/test_workshop_editor_transfers.cpp`
- `workshop_persistence` — what survives a process; `tests/test_workshop_persistence.cpp`
- `workshop_screen` — whole screens, and what a press reaches; `tests/test_workshop_screen.cpp`

All but `workshop_guests` are built by `zengine_workshop_suite` (`tests/CMakeLists.txt`); the eight
`workshop_panes_<part>` entries are one program, `zengine-workshop-panes-tests`, built from the
suite's `PART` groups, each group an entry selecting its own sources' cases (VM-POP-22). This
folder's parts' cases are in `workshop_panes_hand` (`tests/test_workshop_panes_desktop.cpp`, the
Pane Manager and Hotkeys) and `workshop_panes_seam` (`tests/test_workshop_panes_button.cpp`, the
menu presenter). `operator`, `builder` and `files_weave` read files here as text; the root's
`workshop_shapes` pins every shape.

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
