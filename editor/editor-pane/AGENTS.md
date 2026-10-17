# `editor/editor-pane/` — the standard Editor: working on it

The router for `editor/editor-pane/`, the standard Editor built as `zengine-editor-pane`: which file
holds which piece of it. It states no law. Its law, its suites and its pages are routed by the
feature's router, [`editor/AGENTS.md`](../AGENTS.md).

## Files

| file | what it holds |
|---|---|
| `pane.cpp` | `EditorPaneWeave`, the weave: it declares every member and holds its lifecycle and revival, the room, the keys, the clipboard and the exit, save and discard, and the rows, caret and mirror it composes |
| `common.hpp` | what the weave and its two aspect headers share: the constants, the records of a candidate, a view, a composition, a hold and a transfer in flight, and the pure helpers over them |
| `transfers.hpp` | the pointer on the canvas, the highlight or this file's location carried out, material dropped in (as text, a Terminal line or generated C++), and a dropped location opened |
| `opening_switch.hpp` | the two open doors, relayed and managed, the candidate a publication installs, and a switch's doors as the incumbent and as the sealed candidate |
| `editor.hpp` | the document as values: `EditorBuffer`, `EditorState`, source-byte admission and tab geometry |
| `vocabulary.hpp` | the office `zengine.editor`, the pane key, the action ids, and `EditorPaneState` |
| `CMakeLists.txt` | the loadable weave target, `zengine-editor-pane` |

## How the pieces meet

- `pane.cpp` is the target's one source. It includes `common.hpp`, defines the class, and then
  includes `opening_switch.hpp` and `transfers.hpp`, which define the members it declares; neither
  header compiles in any other place.
- Every record, helper and member `pane.cpp` and its three headers define stands in one unnamed
  namespace; past this translation unit, only the weave's export (`ZEN_EXPORT_WEAVE`) is named.
- `tests/CMakeLists.txt` builds `pane.cpp` six more times under other names: five under one test
  switch each, and one plain; each switch's block stands in the member it changes.
- The `workshop_panes` case "the image that holds a document cannot reach the host" reads
  `pane.cpp` as text and refuses the host's names in it.
