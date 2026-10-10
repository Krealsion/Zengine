# `editor/` — The Editor: working on it

The router for `editor/`, a feature: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| part | what it is |
|---|---|
| `editor-pane/` | the standard Editor: `zengine-editor-pane`, a loadable weave |
| `editor-pane/editor.hpp` | the document: buffer, caret and selection, source bytes, tab geometry |
| `editor-pane/vocabulary.hpp` | its office `zengine.editor`, pane key, action ids, reload state |
| `neovim-editor/` | the Neovim-backed Editor: `zengine-neovim-editor`, a loadable weave |
| `neovim-editor/vocabulary.hpp` | its names and three asks; `zengine::neovim`, installed |
| `neovim/` | `zengine-neovim`: header-only Neovim hosting library; not a weave, not installed |
| `neovim/host.hpp` | the owner of one Neovim: its process, RPC, screen grid and Lua module |
| `neovim/document.hpp` | the arithmetic between the Editor's document and Neovim's buffer |
| `neovim/launch.hpp` | which Neovim starts, with which configuration |
| `source-transfer/` | `zengine-source-transfer`: header-only, the material the Editors carry |
| `source-transfer/vocabulary.hpp` | its shapes; `zengine::source-transfer`, installed |
| `source-transfer/material.hpp` | what a drop means to an Editor, and the pairs it hands out |

## Law

- [editor-switch](../agents/workshop/editor-switch.md) — switching the Editor; witnessed by
  `workshop_editor_switch`, then `workshop_load` (the choices, the record)
- [editor-transfers](../agents/workshop/editor-transfers.md) — what the Editor carries out and takes
  in; witnessed by `workshop_panes` (editor transfers), then `source_transfer`; the `open` power in
  `workshop_guests`
- [editor](../agents/workshop/editor.md) — the Editor; witnessed by `workshop_panes` (editor,
  opening), then `workshop_load`; the buffer in `editor`
- [neovim-transfers](../agents/workshop/neovim-transfers.md) — what the Neovim-backed Editor carries
  out and takes in; witnessed by `workshop_neovim` (its transfers source), then `neovim_live`
- [neovim](../agents/workshop/neovim.md) — the Neovim-backed Editor; witnessed by `workshop_neovim`
  (always, and gate `neovim`; its carry in its transfers source), then `neovim` and `neovim_live`

The managed opening both Editors are prepared through, `agents/workshop/opening.md` (`WL-OPEN`), is
routed by [workshop/AGENTS.md](../workshop/AGENTS.md).

## Suites

- `editor` — the buffer (`editor-pane/editor.hpp`) as values, with nothing of the host;
  `tests/test_editor.cpp`
- `neovim` — the hosting library against a scriptable fake Neovim (`tests/neovim_fixture.cpp`):
  arithmetic, framing, custody, every failing start; `tests/test_neovim.cpp`
- `neovim_journey` — gate `session-neovim`: `external-host/tools/workshop/nvim_edit.py` against a
  real Neovim in a real Workshop; `tests/session/neovim_journey.py`
- `neovim_live` — gate `neovim`: the hosting library against a real, sandboxed Neovim;
  `tests/test_neovim_live.cpp`
- `source_transfer` — the material as pure functions: byte rules, the Terminal line, a dropped
  pair, the C++ generator and its goldens; `tests/test_source_transfer.cpp`
- `source_transfer_cpp` — program: the generated C++ goldens compiled against the installed Loom
  and called; `tests/source_transfer_cpp_witness.cpp`
- `workshop_editor_switch` — switching `zengine.editor` in a live Workshop with no Neovim, over
  copies of the standard Editor; `tests/test_workshop_editor_switch.cpp`
- `workshop_neovim` — the Neovim-backed Editor in a live Workshop, always and behind gate `neovim`;
  `tests/test_workshop_neovim.cpp`, `tests/test_workshop_neovim_transfers.cpp`

Its cases in shared suites: `workshop_panes` (`tests/test_workshop_panes_editor.cpp`,
`tests/test_workshop_editor_transfers.cpp`), `workshop_files` (`tests/test_workshop_files.cpp`),
`workshop_load` (`tests/test_workshop_load.cpp`), `workshop_guests`
(`tests/test_workshop_guests.cpp`); its shapes in `workshop_shapes` (`tests/census_headers.hpp`).
Rigs: `tests/editor_transfer_story.hpp`, `tests/workshop_switch_rig.hpp`.

## Host side

- `workshop/workshop.cpp` mounts the opening manager (`workshop/opening.hpp`) and the editor
  switch (`workshop/editor_switch.hpp`, with its grant) for the office `zengine.editor`.
- `workshop/default-load-plan.json` and `workshop/graphical-load-plan.json` load the office and
  author its choices `standard` and `neovim`; `workshop/CMakeLists.txt` stages both weaves.
- Seam vocabulary: `workshop/pane_seam_vocabulary.hpp` (`kEditorRole`, the open),
  `workshop/open_seam_vocabulary.hpp`, `workshop/editor_handoff_vocabulary.hpp`,
  `workshop/editor_switch_vocabulary.hpp`; the carry is `workshop/pane_carry.hpp`.
- Both Editors read files through `workshop/persist.hpp`, and the standard Editor writes through
  it; `workshop/pane_migration.hpp` reads a desk saved under the Editor's old office.

## Pages

- [Editor switch](docs/editor-switch.md)
- [The source editor](docs/editor.md)
- [Neovim in Workshop](docs/neovim.md)
- [Source material: text, commands and file locations between the Editors and
  Inventory](docs/source-transfer.md)

## Practices

- [The Editor](../docs/contributing/best-practices.md#the-editor-editoreditor-pane-editorsource-transfer)
- [Neovim](../docs/contributing/best-practices.md#neovim-editorneovim-editor-editorneovim)
