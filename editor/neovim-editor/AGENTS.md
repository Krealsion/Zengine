# `editor/neovim-editor/` — the Neovim-backed Editor: working on it

The router for `editor/neovim-editor/`, the part of the Editor built as `zengine-neovim-editor`: which
file holds which piece of it. It states no law. Its law, its suites and its pages are routed by the
feature's router, [`editor/AGENTS.md`](../AGENTS.md).

## Files

| file | what it holds |
|---|---|
| `pane.cpp` | the weave's one source: `NeovimEditorWeave`, its lifecycle and reload, the beat and the pump, Neovim's start, the keys, the text, the quit and the clipboard, and the pane it composes and claims |
| `common.hpp` | what the three files share: the bounds and names (`kAskMs`, `kChromeRows`, the menu rows), the arithmetic of paths, rows, pictures and the clipboard, and the records of its conversations (`Candidate`, `Snapshot`, `Held`, `Judgement`, `Preview`) |
| `transfers.hpp` | the pointer on Neovim's screen, the selection and this file's location carried out, material dropped in, a dropped location opened, and a change Neovim holds until it answers |
| `opening_switch.hpp` | the managed opening and the old door's relay, both halves of an editor switch, and Neovim started headless from a Loom with no Workshop |
| `vocabulary.hpp` | the installed names: the office, the pane key, the action ids, `NeovimEditorState`, and `NeovimStartRequested`, `NeovimStopRequested` and `NeovimStatusRequested` |
| `CMakeLists.txt` | the loadable weave `zengine-neovim-editor`, and `zengine::neovim`, its vocabulary |

## How the pieces meet

- `pane.cpp` is the one translation unit. It defines the class, declaring every member;
  `opening_switch.hpp` and `transfers.hpp` define the rest of them out of line and are included
  after the class, so neither compiles alone.
- `common.hpp` stands alone, in `zengine::neovim_editor::detail`; `pane.cpp` reads it whole.
- What the standard Editor keeps and says alike -- the relayed open, the hold, the carry and the
  dropped location's steps -- is `editor/office_seam.hpp` (`zengine::editor_office`), which
  `common.hpp` names into `detail` and the other files call as `office::`.
- Neovim itself is `editor/neovim/` (`zengine-neovim`): its process, its RPC, its screen and the
  Lua module every question and change is asked through.
