# The Editor is the custodian

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [editor](../workshop/editor.md).

**Context.** The Editor was the last built-in the pane-weave arc set out to move, and it was
unlike the five before it: Files left the project root with the host, the Builder left the tool,
Attention left the conditions, Info left the object document, the Terminal left the participant —
each presented a subject somebody else held. The Editor's subject was the buffer a maker types
into, and it was `Session::editor`: the exit read its dirty flag, the paste pinned its epoch, the
Files and Builder asks landed at the host's own office, and four `Act` values, a `KeyContext`, a
`PasteOwner`, a drag place, a painter and three handler bodies were compiled into the host for
it. A pane that only presented a buffer the host kept would have crossed the seam twice per
keystroke and made a second mutable copy of the same bytes.

**Decision.** The Editor weave holds the one open document — path, bytes, saved copy, line
convention, epoch, caret, anchor, history, viewport — and `editor.hpp` moved with it whole. The
host keeps room, focus, membership and the exit DECISION, and learns what it may about the
document by asking or by being told. Five ordinary protocol shapes carry that: `PaneDragged`
(the sweep, unclamped, no release), `PaneRevealRequested` / `PaneRevealAnswered` (the pane asks
to be shown and learns what the desk did), `PaneQuitRequested` / `PaneQuitAnswered` (the host
publishes, counts Loom's accepters, holds every gesture until the last answer, and replays them
on a refusal). `OpenSourceRequested` is answered at `zengine.editor`; the recipe-name half moved
to the read-only project door as `RecipeSourceSaid`, so the Builder walks two doors and no host
relays a source toward the Editor's office. A same-shape reload carries the document as
`EditorPaneState`, which the pane keeps current so Loom's own `snapshot` and `zen.PokeRead` read
one truth. `document.save` is `kUnlessOwned`, and the Editor's `editor.save` row DECLARES that
it stands in for it.

**Alternatives considered.**
- *The host keeps the document and the pane presents it* — refused: every keystroke would cross
  twice, and the pane's rows would be a second copy of bytes the host already held; the prompt's
  floor was one authoritative document.
- *A session-lived owner behind value-based doors* — a viable starting point and not a mandated
  layout; the weave IS such an owner, session-lived by the plan row, and the doors are the ones
  above. Nothing was gained by a second object between them.
- *The host reading the dirty flag through a synchronous query shape* — refused: there is no
  synchronous ask on Loom, and a cached clean flag would be a second authority; pinned by cases
  `"EDIT-W23: a forged quit answer moves nothing -- only Loom's answer to the host's ask
  decides"` and `"EDIT-W24: an edit racing the exit check is judged at the answer, and a
  refused quit costs no keystroke"`.
- *A `PaneReleased` shape* — refused: a pane resolves a sweep from the positions it was given and
  needs no sentence saying the hand let go; a pane that lost its seat ends the record host-side.
- *Clamping the drag into the body host-side* — refused: a row past the edge is exactly the
  fact the Editor steps its window on, and the pane's own coordinate system was the granted one.
- *Carrying the document as a list of lines* — refused: Loom's decode budget counts cells, and a
  hundred thousand lines is a refused reload of a file the Editor admits; pinned by the
  four-megabyte reload case.
- *Making `document.save` a no-text row* — TRIED, and rejected by the founder: it disabled the
  object document's save wherever any pane, a layout name or a draft held input, which is a
  capability makers had. What replaced it is the class `kNoEditor` was really expressing, with
  the exception DECLARED by the pane (`PaneActionRow::supersedes`) instead of compiled into this
  host: `kUnlessOwned`. Supersession is by id, so rebinding either row moves neither meaning,
  and the collision law admits the Editor's `^s` because the two are one meaning in two scopes.
- *Installing the document and asking to be shown afterwards* — TRIED, and rejected: a screen
  with no room then left the source open in a pane nobody could see while the requester had been
  told it succeeded. An acquisition is one transaction: judge, ask the desk, commit only if the
  desk said yes, and re-judge at commitment because a maker can type while it decides.
- *Building the reload snapshot on demand from the live buffer* — TRIED, and rejected: Loom
  answers `zen.PokeRead` from `state_` before any handler runs, so the pane advertised fields it
  answered empty. The pane mirrors into `state_` at each composition instead, rebuilding the two
  expensive Texts only when the bytes moved. A cheaper shape needs a substrate hook there is
  none of.
- *A host relay from `RecipeSourceRequested` to the Editor's office* — refused: the host would
  have to name a pane's office, which is the coupling the extraction exists to remove.

**Consequences.** The host's presentation sources name no Editor identifier (pinned by a source
read). The empty Editor takes the keys, as every runtime pane does. A pointer gesture composes
no new rows, so the picture a press was measured against survives the motions behind it. An accepter that never
answers the quit ask holds the process open, and that is written rather than solved. Process
death still loses drafts. The Editor's outgoing operations — the answer to the open, the reveal,
the quit answer, the project-root ask, the paste ask, the copy — are the next fate phase's list,
and none of them is answered here beyond what Loom already reports.

**Laws supported.** [WL-EDIT-01](../workshop/editor.md), [WL-EDIT-03](../workshop/editor.md),
[WL-EDIT-12](../workshop/editor.md), [WL-EDIT-13](../workshop/editor.md),
[WL-EDIT-14](../workshop/editor.md), [WL-EDIT-15](../workshop/editor.md),
[WL-EDIT-16](../workshop/editor.md).
