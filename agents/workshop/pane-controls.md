# Workshop law — a pane's own controls

Register `WL-HAND`: the labelled controls a pane draws for a hand, and what makes a press on one
mean what the maker aimed it at. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md). The menu those controls open is
[`pane-menu.md`](pane-menu.md); the panes that earned them are [`files.md`](files.md) and
[`project.md`](project.md).

## WL-HAND-01 — A control is a face, a place and an operation, and availability is drawn

LAW — `pack_controls` lays labelled faces into the width and rows a pane spends: `[label]` where it believes the operation applies, `(label)` where it does not, both placed, the rest counted.

MEANS
- a face wider than the pane is dropped rather than cut: half a label is not a control;
- an unavailable face is still a target, and pressing it answers with the operation's refusal;
- the first control is never dropped while a row and the width can hold it, so `[menu]` stays.

DOES NOT MEAN
- that a drawn face is permission: the operation asks its own question again at the spend.

PROVEN BY — `component/control_strip.hpp` `Control`, `PlacedControl`, `ControlStrip`,
`control_face`, `pack_controls`; `files/files.cpp` `browser_controls`, `say_controls`,
`strip_budget`; `builder-pane/pane.cpp` `builder_controls`, `say_controls`, `strip_budget`;
`tests/test_workshop_panes_files.cpp` case `"every control the browser draws is a target, and
pressing it performs that operation"`, case `"in a room too short for every control the strip
says how many are in the menu, and the menu keeps them"`.
WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-02 — The strip's rows come out of the body's budget, before the body is laid out

LAW — A pane asks how many rows its strip needs and subtracts them before composing, so a listing or a fact panel is given what is left; a marker or a fact is dropped, never the strip.

MEANS
- the strip grows with the room and stops: a third of it for a list pane, and at most three rows;
- a marker is said only where the window RESERVED a row for it (`ListWindow::markers`);
- a room too small for one strip row leaves the right press, which opens the same rows.

PROVEN BY — `component/list_window.hpp` `ListWindow::markers`, `cursor_window`;
`files/files.cpp` `body_budget`, `say_entries`, `strip_budget`; `builder-pane/pane.cpp`
`publish`, `output_body_rows`, `strip_budget`; `tests/test_workshop_panes_files.cpp` case `"in a
room too short for every control the strip says how many are in the menu, and the menu keeps
them"`; `tests/test_workshop_panes_output.cpp` case `"WL-OUT-04: a build's own words are opened,
stepped, panned and closed by hand"`.
WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-03 — A press is answered from the picture it was aimed at, or refused in words

LAW — A pane numbering its composition (`component::RowMap`) acts on a press only while the number the host echoed is its current one; an older one is refused in words, never re-resolved.

MEANS
- the number moves exactly when the row-to-meaning map does: a repaint moving no row keeps it;
- a listing window that moves by the least it can is what keeps an ordinary double-click working;
- a press with no number at all (a host that cannot say) is acted on as it always was.

PROVEN BY — `component/row_map.hpp` `RowMap::begin`, `RowMap::settle`, `RowMap::picture`,
`RowMap::current`; `workshop/pane_vocabulary.hpp` `v3::PaneContent`, `v3::PanePressed`;
`files/files.cpp` `kMovedSentence`, `pressed`, `say_entries`; `builder-pane/pane.cpp`
`kMovedSentence`; `tests/test_workshop_panes_files.cpp` case `"P-WORK-25: two queued presses on
the row painted as one entry open THAT entry"`, case `"a press that names a picture Files has
replaced is refused in words and spends nothing"`; `tests/test_workshop_panes_builder.cpp` case
`"BLD-MOUSE: a press that names a picture the Builder has replaced is refused in words"`.
WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-04 — A control names its subject exactly when that subject is not the maker's choice

LAW — A control acting on what the pane shows as chosen reads generically; one whose subject is something else -- the recipe that was BUILT, the artifact STANDING -- carries that name.

MEANS
- `builder.build-realize` keeps both its meanings for the key, and each half has an id of its own;
- so a face reading `load built rocket` cannot arm the next build instead, whatever changed;
- a control's own meaning carries no subject, so moving the selection does not move the picture.

PROVEN BY — `builder-pane/vocabulary.hpp` `kActionArm`, `kActionLoadBuilt`;
`builder-pane/pane.cpp` `load_built`, `arm_only`, `ready_to_load`, `builder_controls`;
`tests/test_workshop_panes_builder.cpp` case `"BLD-MOUSE: the control that loads what was built
names the BUILT recipe, not the choice"`, case `"BLD-MOUSE: while an artifact stands built,
arming the next build is a different answer and says so"`.
WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-05 — A mode a hand can enter is a mode a hand can leave

LAW — Every mode a pane opens draws the controls that finish it and the control that abandons it, and its own menu carries them too; no mode is left reachable by hand and escapable only by a key.

MEANS
- the chooser, the authoring line, the recipe list and the output reader each draw their own;
- a field already answered is stood on again by pressing its row, keeping what it holds;
- the reader draws no build verb at all, so a reader cannot build by a slip of the hand.

PROVEN BY — `files/files.cpp` `chooser_controls`, `authoring_controls`, `edit_field`,
`write_recipe`; `builder-pane/pane.cpp` `list_controls`, `role_controls`, `output_controls`;
`files/vocabulary.hpp` `kActionWriteRecipe`, `kActionMenu`; `builder-pane/vocabulary.hpp`
`kActionRecipes`, `kActionRecipesClose`, `kActionMenu`; `tests/test_workshop_panes_files.cpp`
case `"a maker authors a recipe with the mouse alone: the chooser, every field, and the write"`;
`tests/test_workshop_panes_builder.cpp` case `"BLD-MOUSE: the recipe list chooses by hand, and
looking is not choosing"`.
WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## Do not assume

- That an unavailable control is inert: it is drawn, it is a target, and it answers
  (WL-HAND-01).
- That a picture number is a frame: it is the row-to-meaning map's, and it moves only when
  that map does (WL-HAND-03).
- That the strip is the only route to an operation: what it drops is in the pane's own menu,
  and `[menu]` is never dropped (WL-HAND-01, WL-HAND-02).
