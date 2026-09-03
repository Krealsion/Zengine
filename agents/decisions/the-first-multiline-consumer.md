# The first multiline consumer

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [editor](../workshop/editor.md).

**Context.** A built-in source editor closes the maker loop — choose source, edit, save, build,
realize, inspect, edit again — without leaving the application and without a framework arriving
to pay for it (`7ac1d53`, "Workshop becomes a place a maker can stay while changing source").
`component::TextBox` is one-line and exactly its four consumers' width.

**Decision.** `workshop/editor.hpp` owns the machinery; `Session::editor` owns the one open
document — path, saved copy (dirty derives), line ending, `doc_epoch`, viewport — and the
Editor pane is only its presentation. `EditorBuffer` carries lines, caret, anchor, preferred
column, a depth-and-byte-bounded snapshot history, `revision()`, and `consume()` with its gestures
declared in `kEditorVocabulary`. The document being session state is the no-silent-loss floor. The
source-byte law is the media's honest reach. Tabs expand at presentation only. The viewport
reconciles once per repaint. `on(PointerWheel)` is Workshop's one wheel router. The pane paints
one region.

**Alternatives considered.**
- *Widening `component::TextBox` to multiline* — rejected: the extraction trigger is named in
  `editor.hpp` (a second multiline consumer, two simultaneous views, or a replaceable backend);
  the component is byte-identical.
- *`ctrl+shift+letter` for the discard* — rejected: the POSIX wire cannot say it; `ctrl+d` is a
  plain chord, safe because `revert_to` keeps the history so one undo takes a slip back.
- *Flattening non-ASCII on paste* — rejected: refused whole, naming the line; pinned by case
  `"EDIT-0: typed non-ASCII is refused with a sentence, and the keystroke costs nothing"`.
- *Following the caret after the wheel* — rejected: the wheel's whole meaning is looking
  elsewhere; pinned by case `"EDIT-0: the wheel scrolls the editor's body, moves no caret, and is
  consumed there"`.
- *A scroll framework, a scrollbar, a global offset map, a persisted position* — none; each
  consumer spends its own cursor, and the router later reached every surface Workshop windows
  (`8c2fc05`).
- *A second body arithmetic* — rejected: `external_body_place` with `kEditorHeaderRows`.

**Consequences.** Hiding, moving, covering, reordering or removing the pane touches nothing; an
orderly quit refuses over unsaved source naming two ways out; process death still loses drafts
and no crash recovery is claimed. Every list's wheel moves its cursor by `kListWheelRows` (three)
because a list derives its window from it; `... N more` is reachable by wheel everywhere but the
Loaded pane, which holds no cursor. The Composer's fields were not touched.

**Laws supported.** [WL-EDIT-01](../workshop/editor.md), [WL-EDIT-02](../workshop/editor.md),
[WL-EDIT-03](../workshop/editor.md), [WL-EDIT-07](../workshop/editor.md),
[WL-EDIT-08](../workshop/editor.md), [WL-EDIT-09](../workshop/editor.md),
[WL-EDIT-10](../workshop/editor.md), [WL-EDIT-12](../workshop/editor.md).
