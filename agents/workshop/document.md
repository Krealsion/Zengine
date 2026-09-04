# Workshop law — the document

Register `WL-DOC`, the model half: the maker's document and its operations, written from the
tests; the file's laws are in [`document-file.md`](document-file.md).
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md). The authored and
resolved vocabulary itself is the UI package's,
[`../../docs/reference/ui.md`](../../docs/reference/ui.md).

## WL-DOC-01 — Identity is the id, not the name

LAW — Two objects may share a label; ids are minted from `next_id` and never handed out twice, even after a delete, and a spent mint (`kMaxIdentity`) makes create say so.

PROVEN BY — `workshop/document.hpp` `kFirstIdentity`, `kMaxIdentity`, `can_mint`, `add_default`,
`add`; `tests/test_workshop_document.cpp` case `"identity is the id, not the name: two objects may
be called the same thing"`, case `"an identity is never handed out twice, even after its object is
deleted"`; `tests/test_workshop_persistence.cpp` case `"the mint can be spent, and creating says
so rather than overflowing"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-02 — A property is read through its semantic surface and written by commit

LAW — An unparseable draft writes nothing, a value the property refuses is a different outcome with its reason, and cancel touches nothing; two properties of one type share every line of conversion.

MEANS
- `TextForm<T>` is written once per type: canonical out, the typeable spelling in.

PROVEN BY — `workshop/property.hpp` `Row`, `Commit`, `Written`, `TextForm`, `Property`,
`TextForm::format`, `TextForm::expected`; `workshop/document.hpp` `name_of`, `width_of`,
`height_of`, `context_of`; `tests/test_workshop_document.cpp` case `"a successful commit writes
through the semantic setter"`, case `"an unparseable draft leaves the property untouched and says
so"`, case `"a parseable value the property refuses is a DIFFERENT outcome, with its reason"`,
case `"cancel abandons the draft and never touched the property"`, case `"reuse: two properties of
one type share every line of conversion"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-03 — The name's own refusals are empty and too long

LAW — `check_name` refuses an empty name and one over `kMaxNameLen`; the authored bound is the document's, met from a file exactly as from a hand.

PROVEN BY — `workshop/document.hpp` `check_name`, `kMaxNameLen`, `rename`;
`tests/test_workshop_document.cpp` case `"the name property's own refusals: empty and too
long"`, case `"QR-3: what the authored name bound IS, and what it is not a statement about"`,
case `"QR-3: a name past the authored bound is still refused, from a file as from a hand"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-04 — The extent text form is canonical out and typeable in

LAW — `12` is cells and `70%` a share; `70p` is accepted for `70%`; the empty string, `%`, `banana` and `12x` parse to nothing.

PROVEN BY — `workshop/property.hpp` `TextForm`, `TextForm::parse`, `TextForm::expected`;
`tests/test_workshop_document.cpp` case `"the extent text form: canonical out, and the typeable
spelling in"`, case `"a maker types `70%` through the canonical text route, and 70p is history"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-05 — Authored and resolved are different facts, and only one of them moves

LAW — A resolved row cannot be edited because it has nothing to write to, and the painted rectangle is the resolved rectangle, never the panel's.

PROVEN BY — `workshop/screen_bindings.cpp` `workspace_scene`, `inspector_rows`;
`workshop/property.hpp` `Row::show`; `tests/test_workshop_document.cpp` case `"a resolved row
cannot be edited, because it has nothing to write to"`, case `"authored and resolved are different
facts, and only one of them moves"`, case `"the painted rectangle IS the resolved rectangle, and
the panel is not"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-06 — A move is one authored change

LAW — `doc::move` is the only position write and a refused move writes neither coordinate; the root starts at 0, and an offset in a frame may be negative.

MEANS
- the inspector and the maker's hand write through the one operation.

PROVEN BY — `workshop/document.hpp` `move`, `set_x`, `set_y`, `check_coord`, `kFirstCell`;
`workshop/weave_document.cpp` `move_notice`; `workshop/screen_gestures.cpp` `nudge`, `place`;
`tests/test_workshop_document.cpp` case `"a move is ONE authored change: a refused move writes
neither coordinate"`, case `"the inspector and the maker's hand write through ONE position
operation"`; `tests/test_workshop_screen.cpp` case `"a coordinate is a workspace cell at the root
and an OFFSET in a frame"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-07 — A resize is one authored change

LAW — `doc::resize` writes both extents or neither; a cells extent stays cells, and a share authors the smallest share that fits the asked size, so the projection is stable.

MEANS
- the inspector and the maker's hand write through the one operation.

PROVEN BY — `workshop/document.hpp` `resize`, `set_width`, `set_height`, `check_extent`,
`kMaxCells`; `workshop/weave_document.cpp` `size_by`; `workshop/screen_gestures.cpp`
`extent_from_drag`, `size_to`, `grow`; `tests/test_workshop_document.cpp` case `"resizing a cells
extent authors cells, and every reading of the object follows"`, case `"resizing a share authors a
SHARE, and the number is the smallest one that fits"`, case `"the projection is stable: the same
resolved size always names the same share"`, case `"a resize is ONE authored change: a refused
proposal writes neither extent"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-08 — The authored minimum is the document's, and a stop is not a refusal

LAW — `check_extent` sets the minimum (one cell, one percent), not the resolution floor; a hand stops at a boundary (`Handled::clamped()`) and a written value is refused, and they are told apart.

PROVEN BY — `workshop/document.hpp` `check_extent`; `workshop/screen.hpp` `Handled`,
`kAtWholeContext`, `kAtWorkspaceStart`; `workshop/screen_gestures.cpp` `place`;
`workshop/weave_document.cpp` `edge_of`; `tests/test_workshop_document.cpp` case `"the authored
minimum is the DOCUMENT's, not the resolution floor"`, case `"a hand STOPS at a boundary and a
written value is REFUSED, and they are told apart"`, case `"move and resize meet the workspace
with one policy, in two different walls"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-09 — A drag takes hold of what the maker can see

LAW — The grabbed point follows the hand; a press on empty space grabs nothing, and a press grabs from its own position with no motion event anywhere.

MEANS
- the size handle is derived and Workshop's; the four resize keys and the handle are one gesture;
- a body press reaches move, a handle press reaches resize; Shift turns move into resize.

PROVEN BY — `workshop/screen.hpp` `Drag`, `kHandleGlyph`, `Handle`;
`workshop/screen_gestures.cpp` `size_handle`, `begin_drag`, `drag_to`;
`workshop/weave_document.cpp` `move_by`; `tests/test_workshop_document.cpp` case `"a drag takes
hold of what the maker can see, and the grabbed point follows"`, case `"a press on empty space
grabs nothing, and a drag against the edge slides along it"`, case `"a press grabs from ITS OWN
position, with no motion event anywhere"`, case `"the four resize keys and the handle are ONE
gesture, reached two ways"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-10 — Creating mints, deleting removes exactly one identity

LAW — Creating mints a fresh identity at `kNewX`/`kNewY` with the default extent and cannot author a refused state; delete removes exactly one identity; deleting nothing is a readable refusal.

MEANS
- the post-delete selection rule: the one that took its place, then the last, then none;
- create after empty: the document comes back, and the tool never left.

PROVEN BY — `workshop/document.hpp` `add_default`, `remove`, `kNewX`, `kNewWidthCells`,
`kNewLabel`; `workshop/weave_document.cpp` `create_object`, `delete_object`, `deleted_notice`;
`workshop/screen_gestures.cpp` `create`, `delete_selected`; `tests/test_workshop_document.cpp`
case `"creating mints a fresh identity, and the identity is not the label or the index"`, case
`"delete removes exactly one identity, and the other duplicate label survives"`, case `"the
post-delete selection rule: the one that took its place, then the last, then none"`, case
`"deleting nothing is a refusal a maker can read, not a crash or a silence"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-11 — A context is authored by identity, and a bad one is refused by name

LAW — A relationship that cannot mean anything (itself, nothing, a loop) is refused and says which; a rewire is one authored act, and changing a context rewrites none of the values whose meaning it changed.

MEANS
- `check_document` states the relationship law once, and a poke cannot smuggle one past it.

PROVEN BY — `workshop/document.hpp` `set_context`, `check_context`, `chain_text`,
`check_document`, `dependents_of`, `kMaxChainChars`; `workshop/property.hpp` `ContextRef`,
`TextForm::parse`, `TextForm::expected`; `tests/test_workshop_screen.cpp` case `"a context is
authored BY IDENTITY, and an identity is not a position"`, case `"a relationship that cannot mean
anything is refused, and says which"`, case `"a rewire is ONE authored act: a refused one writes
neither half"`, case `"changing a context does not rewrite the values whose meaning it changed"`,
case `"the relationship law is the document law, and a poke cannot smuggle one past it"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-12 — Composition re-resolves and rewrites nothing

LAW — Moving or resizing a source re-resolves what measures against it, rewriting none; document order is paint, hit and list order, not dependency order; a dependent may spill past its source.

PROVEN BY — `workshop/screen_bindings.cpp` `workspace_scene`; `tests/test_workshop_screen.cpp`
case `"moving a source moves what measures against it, and rewrites none of it"`, case `"resizing
a source re-resolves a share and leaves an authored cell count alone"`, case `"document order is
not dependency order, and stays paint, hit and list order"`, case `"a dependent may spill past its
source, and nothing clips, owns or reorders it"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-17 — The workspace is the root frame

LAW — Taking the room refits the workspace and says whether anything moved; the narrow and widen actions (`[`/`]`) resize it; a share follows the workspace and a cells extent does not.

MEANS
- a maker's authored work keeps its place while the surface grows;
- a run no medium measures is exactly the run Workshop had before.

PROVEN BY — `workshop/screen_bindings.cpp` `adopt_screen`; `workshop/screen.hpp` `kWorkspaceW`,
`kWorkspaceH`; `workshop/weave_document.cpp` `resize_workspace`; `workshop/weave_handlers.cpp`
`on(SurfaceExtent)`; `workshop/keymap.hpp` `workspace.narrower`, `workspace.wider`;
`tests/test_workshop_panels.cpp` case `"taking the room refits the workspace, and says whether
anything moved"`, case `"a maker's authored work keeps its place while the surface grows"`, case
`"`]` reaches the room a bigger surface gave, and `[` still narrows"`;
`tests/test_workshop_document.cpp` case `"a share keeps following the workspace after a resize;
cells do not"`.
WHY — `agents/decisions/the-document-model.md`

## WL-DOC-18 — One scene: canvas, list and inspector read one document

LAW — Canvas, object list and inspector agree after every gesture, through the message path as through a direct call; the pointer lands where the Skin drew the workspace, on either medium.

MEANS
- a pointer in a space Workshop does not speak is ignored, not mis-placed.

PROVEN BY — `workshop/screen_compose.cpp` `paint`; `workshop/screen_bindings.cpp`
`workspace_scene`; `tests/test_workshop_document.cpp` case `"canvas, object list and inspector
agree after every gesture in a session"`, case `"the semantic operations are still the only
authority, through the message path"`, case `"the pointer lands where the Skin actually drew the
workspace"`, case `"the SAME object is under the pointer whichever medium reported it"`, case `"a
pointer in a space Workshop does not speak is ignored, not mis-placed"`.
WHY — `agents/decisions/the-document-model.md`

## Do not assume

- That the document is read at launch: the desk, the window, the keymap and the prefs are; the
  document still is not (WL-SESSION-01).
