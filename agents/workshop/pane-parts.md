# Workshop law — the names a pane gives its parts

Register `WL-HAND`, the naming half: the names a pane gives the parts a hand acts on, and where
Workshop says them. The controls' own laws are in [`pane-controls.md`](pane-controls.md), and the
ids are one series. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-HAND-06 — A pane names the parts a hand acts on, and Workshop says them where they are drawn

LAW — A pane names each row, control and element a weaver acts on from what it means, kept across its redraws; Workshop says each name beside the part's words, place and point, naming nothing itself.

MEANS
- the names ride with the picture they name, judged with it: a name once, in the order the pane reads a press, a place it names nothing unnamed;
- a text part covers the cells showing its columns, a canvas part its rectangle as shown; its point is a place of its own, sought over all of it, or none;
- a pane redrawn as a picture keeps the names its rows had, so a name an agent wrote still holds.

DOES NOT MEAN
- that Workshop reads a name: it judges a name's form and carries it as the pane said it, and says no unnamed place;
- that a part the body does not show is said, that one with no place of its own is given another's, or that pressing one is anything but input.

PROVEN BY — `workshop/pane_parts.hpp` `pane_part_name_problem`, `row_parts_problem`,
`canvas_parts_problem`, `PartNames`, `row_parts`; `component/row_map.hpp` `RowMap::press_order`;
`workshop/weave_inspection.cpp` `visible_parts`, `row_owners`, `row_part_on`, `own_points`,
`canvas_part_on`, `no_point`, `desk_menu`;
`workshop/screen_attention.cpp` `context_line_names`; `workshop/weave_seam.cpp` `admit_content`,
`on(v4::PaneContent)`; `workshop/weave_canvas.cpp` `admit_canvas_content`, `on(v4::PaneCanvasContent)`;
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
window and in a terminal"`, case `"a part's point is a place of its own: beside a control a row
holds, on a row's blank cell where its text is a control's, and none for a row with no place of
its own"`, case `"a canvas part's point is its own: a part with another over its centre is pressed
where nothing inside it is, and a part a terminal paints on no cell is not said there"`, case `"two
overlapping buttons in a running view are each pressed where the view gives that button the
press"`, case `"a row part is pressed where its row map gives it the press, not on a narrower run
over it nor an unnamed one"`, case `"a canvas part's own place is sought over all of it, and a part
with none has no point, never another's"`, case `"a picture of as many parts as it may name gives
each a point a press there gives it, or none"`.
WHY — `agents/decisions/a-pane-names-its-parts.md`
