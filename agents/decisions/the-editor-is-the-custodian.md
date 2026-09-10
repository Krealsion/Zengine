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
to be seated when its act needs nothing more, holding its own gestures until the answer; the
desk seats, selects and focuses it in the delivery that answers, or refuses with nothing moved),
`PaneQuitRequested` / `PaneQuitAnswered` (the host publishes, counts Loom's accepters, holds
every gesture until the last answer, and replays them on a refusal). `OpenSourceRequested` is answered at `zengine.editor`; the recipe-name half moved
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
  told it succeeded.
- *Moving the desk when the reveal is ANSWERED, with the pane re-judging afterwards* — TRIED,
  and rejected: a keystroke delivered while the pane waited could make the replacement a loss,
  and the desk was then holding an authored row, a selection and the keyboard for an operation
  that never happened. Measured with a held paste answer and a removed pane.
- *Three statements, with the desk moving at the pane's SETTLE* — TRIED, and rejected by the
  founder: the capacity answer was an instant's fact, and a screen that shrank between it and
  the settle left the document replaced in a pane the screen no longer showed, reported as a
  success. Naming that a residual and pinning it with a case did not make it the agreed
  outcome. What replaced both: the pane freezes its own eligibility — it holds every gesture
  and clipboard answer from its ask to the answer, Workshop's quit discipline one seam over —
  and the desk seats, selects and focuses it in the one delivery that answers, which is the
  commitment point. A shrink before that delivery is a refused open; a shrink after it is an
  ordinary presentation change to a pane on the desk.
- *A reservation that HOLDS a slot between the two* — refused: it would need capacity accounting
  the seat law does not have, and holding a resize until one pane answered would make every
  pane depend on that pane. Freezing the asker's own fact costs nobody else anything.
- *A fourth statement confirming the seat* — refused: another unprotected round trip moves the
  race rather than closing it.
- *Building the reload snapshot on demand from the live buffer* — TRIED, and rejected: Loom
  answers `zen.PokeRead` from `state_` before any handler runs, so the pane advertised fields it
  answered empty. The pane mirrors into `state_` at each composition instead.
- *Keying that mirror on the buffer's revision* — TRIED, and rejected: that revision moves when
  the CARET moves, because a pending paste must notice its position went stale, so a
  four-megabyte mirror was replaced whole by an arrow key, a press and every motion of a drag.
  The buffer now answers two questions — `revision` for movement, `content_revision` for bytes —
  and the mirror asks the second. What an edit still pays is one materialization of the WHOLE
  document per typed byte -- measured at the bound: the join is the larger part of a keystroke,
  and the history's snapshot is per typing GROUP rather than per byte, so the two are not of
  one order as an earlier writing said; `EditorPaneState::text_builds` counts the
  materializations, so the claim is checkable. A cheaper mirror needs a substrate hook there is
  none of, and asking Loom for one is a decision of its own rather than a premise of this one.
- *Adding `supersedes` to the published `PaneActionRow` v1* — TRIED, and rejected: a published
  `(name, version)` is frozen and its identity is the content-id derived from the shape, so the
  field changed the identity of that row AND of the `PaneActions` v1 enclosing it, and a pane
  built against the old header could no longer register with this host. Ownership is version
  two; version one is what it was.
- *A host relay from `RecipeSourceRequested` to the Editor's office* — refused: the host would
  have to name a pane's office, which is the coupling the extraction exists to remove.

**Consequences.** The host's presentation sources name no Editor identifier (pinned by a source
read). The empty Editor takes the keys, as every runtime pane does. A pointer gesture composes
no new rows, so the picture a press was measured against survives the motions behind it. Asking
for the open source again is a reveal and moves no view. Input's owner is the resolved context
and the remembered pane together, so a menu over a pane is the menu's. An accepter that never
answers the quit ask holds the process open, and that is written rather than solved; a desk that
never answers a reveal leaves the Editor holding its gestures to a bound and refusing the next
open in words, and no other pane waits on it. A reload of the Editor between its ask and the
answer ends the flight: the prior document comes back whole, the desk's seat stands, and the
requester is never told — sender fate's outcome, not this transaction's. Process death still
loses drafts. The Editor's outgoing operations — the answer to the open, the reveal,
the quit answer, the project-root ask, the paste ask, the copy — are the next fate phase's list,
and none of them is answered here beyond what Loom already reports.

**Laws supported.** [WL-EDIT-01](../workshop/editor.md), [WL-EDIT-03](../workshop/editor.md),
[WL-EDIT-12](../workshop/editor.md), [WL-EDIT-13](../workshop/editor.md),
[WL-EDIT-14](../workshop/editor.md), [WL-EDIT-15](../workshop/editor.md),
[WL-EDIT-16](../workshop/editor.md).
