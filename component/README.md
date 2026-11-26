# Component

**The pieces a pane (one panel of a tool's window) is built from: an editable line of text, a long
text read and scrolled, a window onto a long list, a record of what each drawn row means, a choice that stays put while
its list changes, a strip of labelled controls, table columns, sections that open and shut, and
paths for things that move. Each piece keeps its own state and knows nothing about the screen that
shows it.**

It is header-only and depends on nothing. A program links the `zengine::component` target and
spells a header from the repository root, for example `#include "component/text_box.hpp"`.

## Pages

- [The Component package](docs/component.md): the reference — each piece, what it owns, and what
  it leaves to the program using it.
- [The text box's laws](docs/component-laws.md): editing as a component no consumer owns, the
  consumer's clipboard and capacity, and a paste that belongs to the draft that asked.

## What is in this folder

| file | what it holds |
|---|---|
| `text.hpp` | what the text pieces share: characters, words, a selection as two places, a copy |
| `text_box.hpp` | a one-line text box: caret, selection, copy and paste, and undo; and what a width of characters shows of it |
| `text_area.hpp` | long text read only: wrapped by width, scrolled, its text selected and copied; and the lines a width shows |
| `list_window.hpp` | which part of a list fits the space a pane has, and how much is left out |
| `row_map.hpp` | what each row and column span a pane drew means, so a press lands on it |
| `held_choice.hpp` | a chosen row remembered by identity while the list around it changes |
| `control_strip.hpp` | a row of labelled controls packed into the width available |
| `columns.hpp` | a table's columns, laid out once so every row lines up |
| `tab_strip.hpp` | a row of named tabs, the active one marked, windowed to the width available; no pane draws it yet |
| `fingerprint.hpp` | a number naming what was held or sent: the same parts give the same number in every build |
| `accordion.hpp` | a section that opens, shuts, and peeks while the pointer rests on its header; a group of them over the open ones their owner keeps; and the rows a tree shows |
| `unicode_width.hpp` | which code points take no column and which take two, written by `tools/unicode-width.py` and never edited by hand; `text.hpp` reads it |
| `motion.hpp` | straight and curved paths, and progress over elapsed time |
| `CMakeLists.txt` | the `zengine-component` library target (`zengine::component`): header-only, linking nothing |
| `docs/` | these pages |

## Working on it

The repository's guide is [CONTRIBUTING.md](../CONTRIBUTING.md); this section is what a change in
`component/` adds to it.

**Law.** Five numbered laws, on [the text box's laws](docs/component-laws.md): editing text is a
component no consumer owns (WL-TEXT-01); the consumer owns the capacity, the clipboard's custody
and what the text means (WL-TEXT-02); Workshop's clipboard is one in-process mirror (WL-TEXT-08); a
paste is a conversation whose answer belongs to the draft that asked (WL-TEXT-09); and the library
links nothing (WL-TEXT-11). Their proofs are listed in `tests/laws/component.txt`. The controls a
pane draws over `control_strip.hpp`, `list_window.hpp` and `row_map.hpp` keep Workshop's laws
(`WL-HAND`, `agents/workshop/pane-controls.md`, which [Workshop's router](../workshop/AGENTS.md)
routes).

**Suites.** Each runs through the official lane, `tests/verify.cmake`.

| suite | what it proves | where |
|---|---|---|
| `component` | `TextBox`, `TextArea`, the text core, `ListWindow`, `RowMap`, `HeldChoice`, columns, control strips, tab strips and accordions as values, in an executable of its own that links only `zengine-component` | `tests/test_component.cpp` |

Its cases in suites other routers claim: `workshop_panes_terminal`, `workshop_panes_info` and
`workshop_panes_hand` (`tests/test_workshop_panes_terminal.cpp`, `_info.cpp` and `_input.cpp`) for
the text box; `workshop_panes_project` (`_builder.cpp`, `_files.cpp`, `_output.cpp`) and
`workshop_panes_seam` (`_canvas.cpp`) for the controls; `workshop_panes_project` (`_files.cpp`, the
places) and `workshop_panes_hand` (`_desktop.cpp`, the Hotkeys pane's sections) for the accordion;
`workshop_document` (`tests/test_workshop_document.cpp`: the name editor and the Composer's
fields); and `input` (`tests/test_input.cpp` pins `text_box.hpp`'s key spellings to the wire's).
Outside CTest, `tests/package/run.cmake` builds `tests/package/public_surface.cpp` and
`tests/package/pane_menu_consumer.cpp` against the installed headers.

**The host's side.**

- `workshop/CMakeLists.txt` links it into `zengine-workshop-vocabulary`, and
  `cmake/ZengineInstall.cmake` installs its headers and exports `zengine::component`.
- `workshop/screen.hpp` holds the session's `Clipboard`; `LayoutNaming` (`workshop/setup.hpp`) and
  `naming_key` (`workshop/weave_session.cpp`) are the name editor's `TextBox`.
- `pane/parts.hpp` reads a `RowMap`, and `workshop/keymap.hpp` reads `kEditingVocabulary`.
- Its consumers are `builder/builder-pane/`, `editor/editor-pane/`, `files/`, `flow/flow-pane/`,
  `info/`, `inventory/inventory-pane/`, `terminal/`, `workshop/desktop-pane/`,
  `workshop/menu-presenter/`, `composer/draft.hpp`, `view/` and `introspection/powers.hpp`;
  `input/input_weave.hpp` uses `motion.hpp`.
- Good practice here is [Components](../docs/contributing/best-practices.md#components-component) in
  the best practices.
