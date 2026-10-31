# Workshop law — a pane's own controls

Register `WL-HAND`, the controls half: the labelled controls a pane draws for a hand, and what
makes a press on one mean what the weaver aimed it at. The names a pane gives its parts are in
[`pane-parts.md`](pane-parts.md), and the ids are one series. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md). The menu those controls open is
[`pane-menu.md`](pane-menu.md); the panes that draw them are [`files.md`](files.md) and
[`project.md`](project.md).

## WL-HAND-01 — A control is a face, a place and an operation, and availability is drawn

LAW — `pack_controls` lays labelled faces into the width and rows a pane spends: `[label]` where it believes the operation applies, `(label)` where it does not, both placed, the rest counted.

MEANS
- a face wider than the pane is dropped rather than cut: half a label is not a control;
- an unavailable face is still a target, and pressing it answers with the operation's refusal;
- the first control is never dropped while a row and the width can hold it, so `[menu]` stays.

DOES NOT MEAN
- that a drawn face is permission: the operation asks its own question again at the spend.

WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-02 — The strip's rows come out of the body's budget, before the body is laid out

LAW — A pane asks how many rows its strip needs and subtracts them before composing, so a listing or a fact view is given what is left; a marker or a fact is dropped, never the strip.

MEANS
- the strip grows with the room and stops: a third of it for a list pane, and at most three rows;
- a marker is said only where the window RESERVED a row for it (`ListWindow::markers`);
- a room too small for one strip row leaves the right press, which opens the same rows.

WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-03 — A press is answered from the meaning its rows have where it landed

LAW — A pane numbering its composition (`component::RowMap`) reads a press against the rows it drew where the press landed, the meanings its number stands for, and acts on that meaning alone.

MEANS
- the number moves exactly when the row-to-meaning map does: a repaint moving no row keeps it;
- a listing window that moves by the least it can is what keeps an ordinary double-click working;
- a named part is judged by its name and an unnamed row by the number (WL-PRESS-07).

DOES NOT MEAN
- that a press waits for the pane to draw: the window's word on what it drew travels with it.

WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-04 — A face or row that names a subject keeps that name, and is refused when it moves

LAW — What a face or a row NAMES is recorded with it -- in the control's meaning, beside the ask for a menu row -- and established again at the spend; one naming none acts on the shown choice.

MEANS
- `builder.build-realize` keeps both its meanings for the key, and each half has an id of its own;
- the subject and its build operation are the meaning, so a shared artifact stem still moves it;
- a face acting on the pane's own cursor names none, so a double-click's second press stands.

DOES NOT MEAN
- that a press's place stands for its meaning: the meaning is established again at the spend;
- that the artifact name alone is the promise: `target_op_of` checks the operation beside it too.

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

## Proven by

### WL-HAND-01
`component/control_strip.hpp` Control PlacedControl ControlStrip control_face pack_controls
`workshop/pane_facets.hpp` PaneRowsFacet::push_controls
`files/browser_view.hpp` browser_controls say_controls
`builder/builder-pane/view.hpp` builder_controls say_controls
`tests/test_workshop_panes_files.cpp` "every control the browser draws is a target, and pressing it
performs that operation" "in a room too short for every control the strip says how many are in the
menu, and the menu keeps them"

### WL-HAND-02
`component/list_window.hpp` ListWindow::markers cursor_window earlier_marker more_marker
`component/control_strip.hpp` kMaxStripRows strip_budget strip_rows_for
`workshop/pane_facets.hpp` PaneRowsFacet::strip_rows PaneRowsFacet::push_window
`files/browser_view.hpp` body_budget say_entries
`builder/builder-pane/view.hpp` publish
`builder/builder-pane/output_reader.hpp` output_body_rows
`tests/test_component.cpp` "ListWindow: a side's marker is said only where the window reserved that
side a row of its own" "ControlStrip: a strip spends about a third of the room up to its cap, and a
pane's own rows pack as their faces"
`tests/test_workshop_panes_files.cpp` "in a room too short for every control the strip says how many
are in the menu, and the menu keeps them" "scrolled into a long listing, Files says on a row of each
side how many entries the window left out"
`tests/test_workshop_panes_output.cpp` "WL-OUT-04: a build's own words are opened, stepped, panned
and closed by hand"

### WL-HAND-03
`component/row_map.hpp` RowMap::begin RowMap::settle RowMap::picture
`workshop/pane_facets.hpp` PaneRowsFacet
`workshop/pane_drawing.hpp` window_cell
`files/browser_view.hpp` say_entries
`tests/test_workshop_panes_files.cpp` "two queued presses on the row painted as one entry open THAT
entry"
`tests/test_workshop_panes_input.cpp` "a Loaded press the window landed before the wheel moved its
rows selects nothing"

### WL-HAND-04
`builder/builder-pane/vocabulary.hpp` kActionArm kActionLoadBuilt
`builder/builder-pane/pane.cpp` BuilderMeaning::op target_of target_op_of perform_on Offered
load_built arm_only ready_to_load standing
`builder/builder-pane/view.hpp` offered_as builder_controls offer_menu say_controls
`builder/builder-pane/modes.hpp` list_controls
`tests/test_workshop_panes_builder.cpp` "the control that loads what was built names the BUILT
recipe, not the choice" "while an artifact stands built, arming the next build is a different answer
and says so" "an open menu row naming an artifact loads THAT artifact or refuses" "the face drawn
where the older one was is the one a press spends" "the list's own double-click still takes the row
it was aimed at" "a press the window landed on a face naming one build spends that build or is
refused in words: another build named there, or the same name over another build, is refused; the
same build redrawn acts" "a held load menu cannot switch recipes sharing an artifact stem" "rebuilding the same recipe replaces an
offered load, and settling again does not" "the promote control acts on the image it names once a
newer build makes it the one standing" "a held promote or revert menu row cannot switch images sharing an artifact
stem"

### WL-HAND-05
`files/recipe_authoring.hpp` chooser_controls authoring_controls edit_field write_recipe next_field
offer_field say_authoring
`builder/builder-pane/modes.hpp` list_controls role_controls
`builder/builder-pane/output_reader.hpp` output_controls
`builder/builder-pane/view.hpp` offer_menu
`files/vocabulary.hpp` kActionWriteRecipe kActionNextField kActionMenu menu_edit_field
`builder/builder-pane/vocabulary.hpp` kActionRecipes kActionRecipesClose kActionMenu
kMenuOutputOlder kMenuOutputRight
`tests/test_workshop_panes_files.cpp` "a weaver authors a recipe with the mouse alone: the chooser,
every field, and the write" "the unavailable `(next field)` control refuses in its own words and
writes no recipe" "a short Files pane keeps the authoring field being typed into on the screen, and
the menu names the rest" "every control each Files mode draws has a row in that mode's own menu"
(subcase "the authoring line, on its last field" , added when the promise held for every field but
the last one, the review's follow-up finding -- `tests/test_workshop_panes_files.cpp` case "the
authoring menu offers next-field on the last field too, and its refusal writes no recipe" is that
finding's own reproduction)
`tests/test_workshop_panes_builder.cpp` "the recipe list chooses by hand, and looking is not
choosing" "every control each Builder mode draws has a row in that mode's own menu" "a reader
waiting on its first page still offers its whole list in a narrow room" "`edit source` in the recipe
list opens the row the list is standing on, and leaves the choice alone"
`tests/test_workshop_panes_output.cpp` "WL-OUT-04: in a room too small for its strip the reader's
whole list is in its own menu, and every row of it acts"
