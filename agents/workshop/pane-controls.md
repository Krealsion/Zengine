# Workshop law — a pane's own controls

Register `WL-HAND`: the labelled controls a pane draws for a hand, what makes a press on one
mean what the weaver aimed it at, and the names a pane gives the parts a hand acts on. One law per heading; cite by ID. Router:
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

LAW — A pane asks how many rows its strip needs and subtracts them before composing, so a listing or a fact view is given what is left; a marker or a fact is dropped, never the strip.

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
- zero is only current before a numbered picture is established; it is not a legacy bypass.

PROVEN BY — `component/row_map.hpp` `RowMap::begin`, `RowMap::settle`, `RowMap::picture`,
`RowMap::current`; `workshop/pane_vocabulary.hpp` `v3::PaneContent`, `v3::PanePressed`;
`files/files.cpp` `kMovedSentence`, `pressed`, `say_entries`; `builder-pane/pane.cpp`
`kMovedSentence`; `tests/test_workshop_panes_files.cpp` case `"two queued presses on the row
painted as one entry open THAT entry"`, case `"a press that names a picture Files has
replaced is refused in words and spends nothing"`; `tests/test_workshop_panes_builder.cpp` case
`"a press that names a picture the Builder has replaced is refused in words"`.
WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-04 — A face or row that names a subject keeps that name, and is refused when it moves

LAW — What a face or a row NAMES is recorded with it -- in the control's meaning, beside the ask for a menu row -- and established again at the spend; one naming none acts on the shown choice.

MEANS
- `builder.build-realize` keeps both its meanings for the key, and each half has an id of its own;
- the subject and its build operation are the meaning, so a shared artifact stem still moves it;
- a face acting on the pane's own cursor names none, so a double-click's second press stands.

DOES NOT MEAN
- that a zero-picture press reaches this check after a numbered picture:
`RowMap::current` refuses it first (WL-HAND-03);
- that the artifact name alone is the promise: `target_op_of` checks the operation beside it too.

PROVEN BY — `builder-pane/vocabulary.hpp` `kActionArm`, `kActionLoadBuilt`;
`builder-pane/pane.cpp` `BuilderMeaning::op`, `target_of`, `target_op_of`, `perform_on`,
`offered_as`, `Offered`, `load_built`, `arm_only`, `ready_to_load`, `standing`,
`builder_controls`, `list_controls`, `offer_menu`, `say_controls`;
`tests/test_workshop_panes_builder.cpp` case `"the control that loads what was built names the
BUILT recipe, not the choice"`, case `"while an artifact stands built, arming the next
build is a different answer and says so"`, case `"an open menu row naming an
artifact loads THAT artifact or refuses"`, case `"a numbered control naming an artifact
is refused once that artifact is not what is standing"`, case `"the face drawn where
the older one was is the one a press spends"`,
case `"the list's own double-click still takes the row it was aimed at"`,
case `"a held load menu cannot switch recipes sharing an artifact stem"`,
case `"a numbered load control preserves the build behind a shared artifact stem"`,
case `"rebuilding the same recipe replaces an offered load, and settling again does not"`, case
`"the promote control acts on the image it names once a newer build makes it the one
standing"`, case `"the promote and revert controls refuse a stale
press across a shared artifact stem"`, case `"a held promote or revert menu row
cannot switch images sharing an artifact stem"`.
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
`kMenuOutputOlder`, `kMenuOutputRight`; `tests/test_workshop_panes_files.cpp` case `"a weaver
authors a recipe with the mouse alone: the chooser, every field, and the write"`, case `"the
unavailable `(next field)` control refuses in its own words and writes no recipe"`, case `"a
short Files pane keeps the authoring field being typed into on the screen, and the menu names
the rest"`, case `"every control each Files mode draws has a row in that mode's own menu"`
(subcase `"the authoring line, on its last field"`, added when the promise held for every field
but the last one, the review's follow-up finding -- `tests/test_workshop_panes_files.cpp` case
`"the authoring menu offers next-field on the last field too, and its refusal writes no recipe"`
is that finding's own reproduction);
`tests/test_workshop_panes_builder.cpp` case `"the recipe list chooses by hand, and looking is not
choosing"`, case `"every control each Builder mode draws has a row in that mode's
own menu"`, case `"a reader waiting on its first page still offers its whole list in a
narrow room"`, case `"`edit source` in the recipe list opens the row the list is
standing on, and leaves the choice alone"`;
`tests/test_workshop_panes_output.cpp` case `"WL-OUT-04: in a room too small for its strip the
reader's whole list is in its own menu, and every row of it acts"`.
WHY — `agents/decisions/a-pane-draws-its-own-controls.md`

## WL-HAND-06 — A pane names the parts a hand acts on, and Workshop says them where they are drawn

LAW — A pane names each row, control and element a weaver acts on from what it means, kept across its redraws; Workshop says each name beside the part's words, place and point, naming nothing itself.

MEANS
- the names ride with the picture they name and are judged with it: a name once, on what it says;
- a text part covers the cells showing its columns, a caret's glyph among them, a canvas part its rectangle as shown; its point is one no part inside it covers;
- a pane redrawn as a picture keeps the names its rows had, so a name an agent wrote still holds.

DOES NOT MEAN
- that Workshop reads a name: it judges a name's form and carries it as the pane said it;
- that a part the body does not show is said, or that pressing one is anything but input.

PROVEN BY — `workshop/pane_parts.hpp` `pane_part_name_problem`, `row_parts_problem`,
`canvas_parts_problem`, `PartNames`, `row_parts`; `workshop/weave_inspection.cpp` `visible_parts`,
`row_part_on`, `canvas_part_on`, `inside_part`, `own_point`, `desk_menu`; `workshop/screen_attention.cpp`
`context_line_names`; `workshop/weave_seam.cpp` `admit_content`, `on(v4::PaneContent)`;
`workshop/weave_canvas.cpp` `admit_canvas_content`, `on(v4::PaneCanvasContent)`;
`workshop/weave_external.cpp` `admit_menu_lines`; `menu-presenter/presenter.cpp` `show`;
`desktop-pane/pane.cpp` `launcher_part`, `keys_part`; `info-pane/pane.cpp` `named_rows`;
`info-pane/value_view.hpp` `part_name`; `files/files.cpp` `part_name`; `builder-pane/pane.cpp`
`part_name`; `inventory-pane/pane.cpp` `part_name`; `terminal-pane/pane.cpp` `named_rows`;
`composer/view.hpp` `part_name`; `introspection/introspection.cpp` `loaded_parts`, `powers_parts`;
`neovim-editor/pane.cpp` `status_part`; `view/view.hpp` `named`; `view-builder/picture.hpp`
`part_name`, `named`; `flow-pane/view.hpp` `part_name`, `named`;
`tests/test_workshop_panes_button.cpp` case `"WL-HAND-06:
the shipped presenter names each row's line by its id, and a press at that name's point chooses
it"`; `tests/test_workshop_panes_desktop.cpp` case `"WL-HAND-06: the Pane Manager names each row
and its mark by the pane's reference"`, case `"WL-HAND-06: Hotkeys names each binding's row by its
identity"`; `tests/test_workshop_panes_info.cpp` case `"WL-HAND-06: Info names its panes by
reference and its properties by label"`; `tests/test_workshop_info_views.cpp` case `"WL-HAND-06:
an Info view names its controls by action and its fields by path"`;
`tests/test_workshop_panes_files.cpp` case `"WL-HAND-06: Files names its entries by name and its
controls by operation"`; `tests/test_workshop_panes_builder.cpp` case `"WL-HAND-06: the Builder
names its controls by operation and a recipe's row by the recipe"`;
`tests/test_workshop_inventory_folders.cpp` case `"WL-HAND-06: Inventory names its entries,
folders, crumbs and controls by what each is"`; `tests/test_workshop_panes_terminal.cpp` case
`"WL-HAND-06: the Terminal names the line being typed and each candidate by what it says"`;
`tests/test_workshop_panes_attention.cpp` case `"WL-HAND-06: Attention names each condition's row
by its key"`; `tests/test_workshop_demo.cpp` case `"WL-HAND-06: the demo controls name their reset
row and the state beneath it"`; `tests/test_workshop_panes_introspection.cpp` case `"WL-HAND-06:
Loaded names each loaded weave's row, and Powers its controls and each power's row"`;
`tests/test_composer.cpp` case `"WL-HAND-06: the Composer names a message by its identity, a field
by its name and its two controls by what they do"`; `tests/test_workshop_panes_editor.cpp` case
`"WL-HAND-06: the Editor names its status row and each document line by its number, wherever the
window stands"`; `tests/test_workshop_neovim.cpp` case `"WL-HAND-06: the Neovim editor names its
own status row, and nothing of Neovim's screen"`; `tests/test_view.cpp` case `"WL-HAND-06: a running
view names each element by its id over the place a press on it lands, and a press there uses it"`;
`tests/test_view_builder.cpp` case `"WL-HAND-06: the View Builder names its controls, kinds, boxes,
list rows, elements and handles, an element by its id"`; `tests/test_flow_pane.cpp` case
`"WL-HAND-06: Flow names its controls, its nodes and ports by their place, and every place a press
means something"`;
`tests/test_workshop_desk.cpp`
case `"a row
map's parts are named by what each span means, and a name nothing, one the judge would refuse or
one taken is left unnamed"`, case `"a text
pane's named parts are said under the pane's own names over the cells that show them, and a press
at a part's point lands on it, in a window and in a terminal"`, case `"a part keeps its name
across its pane's redraws, and is said and pressed where the redraw put it"`, case `"a pane's
names are judged with the rows they name: a name twice, one on a row not said or past the room,
or one that is not a name refuses the rows whole, saying why"`, case `"a canvas pane's named parts
are said where its body shows them, with the words inside them, and a press at a part's point
lands inside it in the pane's own canvas, in a window and in a terminal"`, case `"a picture's
names are judged with it: a name twice or a part with no extent rejects the picture whole, and
the last good one stays"`, case `"the desk names the lines of Workshop's own menu by the action or
group each shows, over the line it is drawn on"`, case `"a pane's names survive the canvas: its
next image draws a picture under the names its rows had, and each is pressed where the picture
draws it"`, case `"every place a named part gives is where the medium draws its words, in a
window and in a terminal"`, case `"a part's point is a character of its own: a row with a control
inside it is pressed beside the control, and the control on itself, in a window and in a
terminal"`, case `"a canvas part's point is its own: a part with another over its centre is pressed
where nothing inside it is, and a part a terminal paints on no cell is not said there"`.
WHY — `agents/decisions/a-pane-names-its-parts.md`

## Do not assume

- That an unavailable control is inert: it is drawn, it is a target, and it answers
  (WL-HAND-01).
- That a picture number is a frame: it is the row-to-meaning map's, and it moves only when
  that map does (WL-HAND-03).
- That the strip is the only route to an operation: what it drops is in the pane's own menu,
  which carries the mode's whole list, and `[menu]` is never dropped (WL-HAND-01, WL-HAND-05).
- That a field left behind is abandoned: pressing its row stands the line on it again, keeping
  what it holds, and the mode's menu names every field the room cannot draw (WL-HAND-05).
