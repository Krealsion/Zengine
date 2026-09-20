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

## WL-HAND-04 — A face or row that names a subject keeps that name, and is refused when it moves

LAW — What a face or a row NAMES is recorded with it -- in the control's meaning, beside the ask for a menu row -- and established again at the spend; one naming none acts on the shown choice.

MEANS
- `builder.build-realize` keeps both its meanings for the key, and each half has an id of its own;
- the named subject is part of the meaning, so two equal-width faces are two pictures;
- a face acting on the pane's own cursor names none, so a double-click's second press stands.

DOES NOT MEAN
- that the picture fence alone answers it: a press echoing no picture is judged by the name too.

PROVEN BY — `builder-pane/vocabulary.hpp` `kActionArm`, `kActionLoadBuilt`;
`builder-pane/pane.cpp` `target_of`, `perform_on`, `advertised_by`, `Offered`, `load_built`,
`arm_only`, `ready_to_load`, `builder_controls`, `list_controls`, `offer_menu`, `say_controls`;
`tests/test_workshop_panes_builder.cpp` case `"BLD-MOUSE: the control that loads what was built
names the BUILT recipe, not the choice"`, case `"BLD-MOUSE: while an artifact stands built,
arming the next build is a different answer and says so"`, case `"BLD-MOUSE: an open menu row
naming an artifact loads THAT artifact or refuses"`, case `"BLD-MOUSE: a numbered control naming
an artifact is refused once that artifact is not what is standing"`, case `"BLD-MOUSE: the face
drawn where the older one was is the one a press spends"`,
case `"BLD-MOUSE: the list's own double-click still takes the row it was aimed at"`.
WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-05 — A mode a hand can enter is a mode a hand can leave

LAW — Every mode a pane opens draws the controls that finish it and the control that abandons it, and its own menu carries them too; no mode is left reachable by hand and escapable only by a key.

MEANS
- the chooser, the authoring line, the recipe list and the output reader each draw their own;
- the mode's own menu carries every control the mode draws, which is `+N in menu`'s promise;
- the reader draws no build verb at all, so a reader cannot build by a slip of the hand.

DOES NOT MEAN
- that a mode keeps a printable menu key: one whose line takes text declares none, and `[menu]`
and the second button are its routes.

PROVEN BY — `files/files.cpp` `chooser_controls`, `authoring_controls`, `edit_field`,
`write_recipe`, `next_field`, `offer_field`, `say_authoring`; `builder-pane/pane.cpp`
`list_controls`, `role_controls`, `output_controls`, `offer_menu`; `files/vocabulary.hpp`
`kActionWriteRecipe`, `kActionNextField`, `kActionMenu`, `menu_edit_field`;
`builder-pane/vocabulary.hpp` `kActionRecipes`, `kActionRecipesClose`, `kActionMenu`,
`kMenuOutputOlder`, `kMenuOutputRight`; `tests/test_workshop_panes_files.cpp` case `"a maker
authors a recipe with the mouse alone: the chooser, every field, and the write"`, case `"the
unavailable `(next field)` control refuses in its own words and writes no recipe"`, case `"a
short Files pane keeps the authoring field being typed into on the screen, and the menu names
the rest"`, case `"every control each Files mode draws has a row in that mode's own menu"`;
`tests/test_workshop_panes_builder.cpp` case `"BLD-MOUSE: the recipe list chooses by hand, and
looking is not choosing"`, case `"BLD-MOUSE: every control each Builder mode draws has a row in
that mode's own menu"`, case `"BLD-MOUSE: a reader waiting on its first page still offers its
whole list in a narrow room"`, case `"BLD-MOUSE: `edit source` in the recipe list opens the row
the list is standing on, and leaves the choice alone"`;
`tests/test_workshop_panes_output.cpp` case `"WL-OUT-04: in a room too small for its strip the
reader's whole list is in its own menu, and every row of it acts"`.
WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## Do not assume

- That an unavailable control is inert: it is drawn, it is a target, and it answers
  (WL-HAND-01).
- That a picture number is a frame: it is the row-to-meaning map's, and it moves only when
  that map does (WL-HAND-03).
- That the strip is the only route to an operation: what it drops is in the pane's own menu,
  which carries the mode's whole list, and `[menu]` is never dropped (WL-HAND-01, WL-HAND-05).
- That a field left behind is abandoned: pressing its row stands the line on it again, keeping
  what it holds, and the mode's menu names every field the room cannot draw (WL-HAND-05).
