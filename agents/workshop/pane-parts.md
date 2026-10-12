# Workshop law — the names a pane gives its parts

Register `WL-HAND`, the naming half: the names a pane gives the parts a hand acts on, and where
Workshop says them. The controls' own laws are in [`pane-controls.md`](pane-controls.md), and the
ids are one series. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-HAND-06 — A pane names the parts a hand acts on, and Workshop says them where they are drawn

LAW — A pane names each row, control and element a weaver acts on from what it means, kept across redraws; Workshop says each name with the part's words, place and point, naming only its own surfaces' parts.

MEANS
- the names ride with the picture they name, judged with it: a name once, in the order the pane reads a press, a place it names nothing unnamed;
- a text part covers the cells showing its columns, a canvas part its rectangle as shown, each saying the characters drawn in it; its point is a place of its own, or none;
- a pane redrawn as a picture keeps the names its rows had, so a name an agent wrote still holds.

DOES NOT MEAN
- that Workshop reads a name: it judges a name's form and carries it as the pane said it, and says no unnamed place;
- that a part the body does not show or a cover hides is said (WL-READ-05), that one with no place of its own is given another's, or that pressing one is anything but input.

WHY — `agents/decisions/a-pane-names-its-parts.md`

## WL-HAND-07 — A control is named by the action it performs, and that action is in force

LAW — A part that performs an action its pane declares is named `control:<id>`, the id as declared, and a part named `control:<x>` names an action `x` the pane has in force when the picture is drawn.

MEANS
- a control whose press performs no declared action has one declared, with no key, so a part pressed by its name and an action asked by its id are one control;
- a control drawn while its action is not in force, a press on it doing nothing, is named nothing;
- the suites hold it: each picture a case reads is judged against the rows its pane has in force then.

DOES NOT MEAN
- that Workshop reads a part's name, or finds an action from a part (WL-HAND-06);
- that a declared action has a key: a row a control performs needs none until a keymap file gives one.

WHY — `agents/decisions/a-pane-names-its-parts.md`

## Proven by

### WL-HAND-06
`workshop/pane_parts.hpp` pane_part_name_problem row_parts_problem canvas_parts_problem PartNames
row_parts
`component/row_map.hpp` RowMap::press_order
`workshop/pane_canvas_rows.hpp` rows_picture
`workshop/weave_inspection.cpp` visible_parts row_owners row_part_on own_points canvas_part_on
words_inside no_point desk_menu
`workshop/screen_attention.cpp` context_line_names
`workshop/weave_seam.cpp` admit_content on(v4::PaneContent)
`workshop/weave_canvas.cpp` admit_canvas_content on(v4::PaneCanvasContent) on(v5::PaneCanvasContent)
`workshop/weave_external.cpp` admit_menu_lines
`workshop/menu-presenter/presenter.cpp` show
`workshop/desktop-pane/pane.cpp` launcher_part keys_part
`info/pane.cpp` named_rows
`info/value_view.hpp` part_name
`files/files.cpp` part_name
`builder/builder-pane/pane.cpp` part_name
`inventory/inventory-pane/pane.cpp` part_name
`terminal/pane.cpp` named_rows
`composer/view.hpp` part_name
`introspection/introspection.cpp` loaded_parts powers_parts
`editor/neovim-editor/pane.cpp` status_part
`view/view.hpp` named
`view/view-builder/picture.hpp` part_name named
`flow/flow-pane/view.hpp` part_name named
`tests/test_workshop_desk.cpp` "a canvas pane's rows set on its room's lattice are words on its
parts' rows, and a part's text is the characters drawn inside it" "with pane titles hidden, a
refused picture's mark is Workshop's own: the pane view waits while it covers the picture, a press
on it reaches no provider and a right press opens Workshop's menu, and the picture beside it takes a
press as before, a press held or not" "a row map's parts are named by what each span means, and a
name nothing, one the judge would refuse or one taken is left unnamed" "a text pane's named parts
are said under the pane's own names over the cells that show them, and a press at a part's point
lands on it, in a window and in a terminal" "a part keeps its name across its pane's redraws, and is
said and pressed where the redraw put it" "a pane's names are judged with the rows they name: a name
twice, one on a row not said or past the room, or one that is not a name refuses the rows whole,
saying why" "a canvas pane's named parts are said where its body shows them, with the words inside
them, and a press at a part's point lands inside it in the pane's own canvas, in a window and in a
terminal" "a picture's names are judged with it: a name twice or a part with no extent rejects the
picture whole, and the last good one stays" "the desk names the lines of Workshop's own menu by the
action or group each shows, over the line it is drawn on" "a pane's names survive the canvas: its
next image draws a picture under the names its rows had, and each is pressed where the picture draws
it" "every place a named part gives is where the medium draws its words, in a window and in a
terminal" "a part's point is a place of its own: beside a control a row holds, on a row's blank cell
where its text is a control's, and none for a row with no place of its own" "a canvas part's point
is its own: a part with another over its centre is pressed where nothing inside it is, and a part a
terminal paints on no cell is not said there" "two overlapping buttons in a running view are each
pressed where the view gives that button the press" "a row part is pressed where its row map gives
it the press, not on a narrower run over it nor an unnamed one" "a canvas part's own place is sought
over all of it, and a part with none has no point, never another's" "a picture of as many parts as
it may name gives each a point a press there gives it, or none"
`tests/test_workshop_panes_canvas.cpp` "a pane's rows drawn on its canvas stand where its prose rows
would, name the same parts, and a place reads back to its row and column"
`tests/test_workshop_panes_button.cpp` "WL-HAND-06: the shipped presenter names each row's line by
its id, and a press at that name's point chooses it"
`tests/test_workshop_panes_desktop.cpp` "WL-HAND-06: the Pane Manager names each row and its mark by
the pane's reference" "WL-HAND-06: Hotkeys names each binding's row by its identity"
`tests/test_workshop_panes_info.cpp` "WL-HAND-06: Info names its panes by reference and its
properties by label"
`tests/test_workshop_info_views.cpp` "WL-HAND-06: an Info view names its controls by action and its
fields by path"
`tests/test_workshop_panes_files.cpp` "WL-HAND-06: Files names its entries by name and its controls
by operation"
`tests/test_workshop_panes_builder.cpp` "WL-HAND-06: the Builder names its controls by operation and
a recipe's row by the recipe"
`tests/test_workshop_inventory_folders.cpp` "WL-HAND-06: Inventory names its entries, folders,
crumbs and controls by what each is"
`tests/test_workshop_panes_terminal.cpp` "WL-HAND-06: the Terminal names the line being typed and
each candidate by what it says"
`tests/test_workshop_panes_attention.cpp` "WL-HAND-06: Attention names each condition's row by its
key"
`tests/test_workshop_demo.cpp` "WL-HAND-06: the demo controls name their reset row and the state
beneath it"
`tests/test_workshop_panes_introspection.cpp` "WL-HAND-06: Loaded names each loaded weave's row, and
Powers its controls and each power's row"
`tests/test_composer.cpp` "WL-HAND-06: the Composer names a message by its identity, a field by its
name and its two controls by what they do"
`tests/test_workshop_panes_editor.cpp` "WL-HAND-06: the Editor names its status row and each
document line by its number, wherever the window stands"
`tests/test_workshop_neovim.cpp` "WL-HAND-06: the Neovim editor names its own status row, and
nothing of Neovim's screen"
`tests/test_view.cpp` "WL-HAND-06: a running view names each element by its id over the place a
press on it lands, a button saying an intent as the action it performs, and a press there uses it"
`tests/test_view_builder.cpp` "WL-HAND-06: the View Builder names its controls, kinds, boxes, list
rows, elements and handles, an element by its id"
`tests/test_flow_pane.cpp` "WL-HAND-06: Flow names its controls, its nodes and ports by their place,
and every place a press means something"

### WL-HAND-07
`tests/control_parts.hpp` control_parts_problem
`tests/workshop_support.hpp` held_control_parts_problem held_parts
`composer/view.hpp` part_name
`introspection/vocabulary.hpp` kPowersActionSources kPowersActionOperators kPowersActionComposite
`introspection/introspection.cpp` powers_parts show_view toggle_composite
`inventory/inventory-pane/pane.cpp` part_name
`flow/flow-pane/pane.cpp` declare
`view/view-builder/picture.hpp` part_name
`view/view-builder/pane.cpp` more_choices
`external-host/demo-control/control.hpp` offer
`tests/test_workshop_panes_builder.cpp` "the action census: the rows the Builder declares in each
mode, with what each press causes and the mode it enters, and the desk's, are tests/actions.txt"
`tests/test_workshop_panes_actions.cpp` "Powers declares its actions, acts on the resolved id, and
its typing still crosses raw"
`tests/test_flow_pane.cpp` "loaded Flow pane authors runs edits and reopens a graphical project with
reusable examples"
`tests/test_view_builder.cpp` "every field a label could show can be chosen: the choice wraps within
its column, and what the rows cannot hold is reached by More"
`tests/test_workshop_demo.cpp` "WL-HAND-06: the demo controls name their reset row and the state
beneath it"
