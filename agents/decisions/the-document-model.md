# The document model

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [document](../workshop/document.md) and
[document-file](../workshop/document-file.md); the authored/resolved vocabulary itself is
[the UI reference](../../docs/reference/ui.md).

**Context.** The first slice: a person opens Workshop, sees an ordinary authored rectangle,
selects it, inspects a real property through a typed connection, changes it, and sees an invalid
change refuse — with no shadow scene graph and no editor framework (`a83fd86`, the first
Workshop slice). Zengine had no visual primitive, and Loom's semantic tree was claimed to be the
right vocabulary one packaging decision away; source-tracing measured that false in both halves
— four in-tree Loom consumers cannot follow it here, and its model fences out the authored
placement and absolute extent Workshop authors — so the vocabulary moved up out of Workshop
rather than across out of Loom (`0ec265c`, the commit that gave Zengine the authored/resolved
vocabulary). Creation, move and delete (`ee54706`), size (`84e00e9`), persistence (`fddec6e`)
and frames (`7949ac1`) each forced one of the rules below.

**Decision.** Identity is the id: `next_id` never rewinds and is in the file because it cannot
be reconstructed. A property is read through its semantic surface and written by commit, with
`TextForm<T>` written once per type and three commit outcomes. `doc::move` and `doc::resize` are
each the one operation writing a position or a size, and a refused proposal writes neither
half. The authored minimum is the document's; a hand stops at a boundary and a written value is
refused, told apart. A context is authored by identity and a broken chain is not placed at all.
Composition re-resolves and rewrites nothing. The file carries what a maker made and no resolved
geometry; loading is a transaction and saving observes. The file doors refuse before they harm.
`^s` and `^o` go through the real message path. The workspace is the root frame. One scene feeds
the canvas, the list and the inspector.

**Alternatives considered.**
- *Loom's `loom::Widget` + `px_layout` as the vocabulary* — measured false (`0ec265c`).
- *Three call sites doing their own extent arithmetic* (`doc::resolve`, `doc::pick`) — retired:
  one `ui::Scene` from `workspace_scene()`, agreeing because there is one.
- *Two setters for a move* — rejected: a diagonal drag into the corner would slide down the edge
  while reporting a refusal; pinned by case `"a move is ONE authored change: a refused move
  writes neither coordinate"`.
- *Share-authored positions* — rejected: the resolver clamps and floors and is not invertible
  for extents, while placement is a sum that inverts exactly (`ee54706`, `7949ac1`).
- *`max(surviving) + 1` as the mint* — rejected: a maker who made #3, deleted it and came back
  would find the next object wearing a dead one's number; pinned by case `"an identity is never
  handed out twice, even after its object is deleted"`.
- *Nearest rounding for a share resize* — rejected: it sends 28 cells to 58% and resolves back
  to 27, so grabbing an edge would shrink the object; the smallest share that fits, chosen by
  asking the resolver over its candidates (`84e00e9`).
- *Clamping inside `doc::`* — refused: the hand's clamp lives in the gesture layer in the
  document's own limits; pinned by case `"a hand STOPS at a boundary and a written value is
  REFUSED, and they are told apart"`.
- *A parser written here, or persisting the weave's state* — rejected: Loom's own codec, so a
  document and a message are refused by one gate; three small shapes, so renaming a member
  cannot change a maker's file (`fddec6e`).
- *Keeping the selection id across a load* — rejected: it would alias whatever new object
  carried that number.
- *Accepting `70p` for `70%`* — kept only while `%` could not be typed from scancodes; retired
  when text arrived as text (`15f173a`), and case `"a maker types `70%` through the canonical
  text route, and 70p is history"` says so.
- *Parent/child containment* — refused: a frame says what values are measured against and
  nothing about containment, ownership, clipping or lifetime; Workshop's one policy over it is
  that a source with dependents is not deletable (`7949ac1`).
- *Guessing the root for a chain that cannot reach it* — refused: an absence, never a guess.

**Consequences.** Save under a 48-cell workspace and load under a 36-cell one: the authored 61%
is byte-identical while Resolved reads 29x6 before and 21x6 after. Save → load → save is
byte-identical; unknown fields are refused, not dropped. `doc::add` can refuse, because a file
can say its mint is at the top of the number line. The size handle is Workshop furniture, and
the four resize keys were deleted in favour of Shift with the movement keys once the wire could
carry a modifier. Signed arithmetic over poked values was repaired twice on the way, in
`resolve_extent` and in `Rect::contains`.

`kMaxNameLen` is 64 bytes, up from 32: the narrowest reader of a name, the OBJECTS body, is 28
columns at the 78x22 minimum on a character medium, so a 32-byte name already came back cut, and a
suite case needs a 43-byte name (case
`"QR-3: what the authored name bound IS, and what it is not a statement about"`).

`kMaxChainChars` cuts a broken chain by characters, not links: measured live, a two-object cycle
printed one character too long and lost its closing bracket, and a fixed link count failed the
same way one identity later, because an int64 identity is twenty characters.

The safe write's rename replaces an existing destination on both lanes, measured: POSIX
`rename(2)`, and on Windows libstdc++'s `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` (case
`"a detected write failure leaves the last good save readable and unchanged"`).

**Laws supported.** [WL-DOC-13](../workshop/document-file.md),
[WL-DOC-14](../workshop/document-file.md), [WL-DOC-15](../workshop/document-file.md),
[WL-DOC-16](../workshop/document-file.md), [WL-DOC-19](../workshop/document-file.md),
[WL-DOC-01](../workshop/document.md), [WL-DOC-02](../workshop/document.md),
[WL-DOC-03](../workshop/document.md), [WL-DOC-04](../workshop/document.md),
[WL-DOC-05](../workshop/document.md), [WL-DOC-06](../workshop/document.md),
[WL-DOC-07](../workshop/document.md), [WL-DOC-08](../workshop/document.md),
[WL-DOC-09](../workshop/document.md), [WL-DOC-10](../workshop/document.md),
[WL-DOC-11](../workshop/document.md), [WL-DOC-12](../workshop/document.md),
[WL-DOC-17](../workshop/document.md), [WL-DOC-18](../workshop/document.md).
