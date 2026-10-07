# Workshop law — the Info controls

Register `WL-CTRL`: the grounds Info's structural rows sit on, and a property view that carries
no controls of its own. Info inspects panes (WL-INFO-14), and a pane is launched and closed from
the Pane Manager, never from here. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-CTRL-01 — Info's property view is two headings, two lists and a front sentence

LAW — Info’s pane-property view has no object controls: two headings, two lists and a front sentence, reserved and shared as WL-INFO-08 and WL-INFO-07 say.

PROVEN BY — `info-pane/pane.cpp` `say`; `tests/test_workshop_panes_info.cpp` case
`"a room too short for the body invents none of it"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-02 — Info's composition ends at the rows it was granted

LAW — With no footer there is nothing to keep at the end; `mark` and `placed` are still inverses over one composition, and `finish` still truncates to the granted rows (WL-INFO-04).

PROVEN BY — `info-pane/pane.cpp` `placed`, `finish`, `composed_`;
`tests/test_workshop_panes_info.cpp` case `"a press on an Info row while a notice stands names
the row painted there, and a full room keeps its last row under the notice"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-03 — The draft's refusal is Info's own; every other refusal is the owner's

LAW — The draft's refusal is the pane's own, made before asking, and now holds back another subject (WL-INFO-09); every other refusal is the owner's, answered to the ask.

PROVEN BY — `info-pane/pane.cpp` `kFinishTheEdit`, `press_placed`;
`tests/test_workshop_panes_info.cpp` case `"a live draft holds another subject back, and the
reason is the weaver's"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-04 — A row the weaver cannot author is muted, and refused in words

LAW — With no control there is nothing to present as unavailable; a row the weaver cannot author is still said in the muted role and refused in words when Return is pressed on it.

PROVEN BY — `info-pane/pane.cpp` `say_properties`, `not_authored`;
`tests/test_workshop_panes_info.cpp` case `"a row the screen makes is refused by the pane, in
its own words"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-05 — Info owns no act: naming a subject and writing a property are the host's

LAW — Info owns no act still: naming a subject and writing a property are the host's doors (`InspectPaneRequested`, `PaneCommitRequested`), answered by the party that owns the rows.

PROVEN BY — `info-pane/pane.cpp` `ask_inspect`, `ask_commit`;
`workshop/weave_inspection.cpp` `on(InspectPaneRequested)`, `on(PaneCommitRequested)`;
`tests/test_workshop_panes_info.cpp` case `"a press on a pane row inspects it, through the
host's own door"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-CTRL-06 — The subject's heading sits on a ground

LAW — The subject's heading is accent on muted and every other row sits on none; the ground is the row's own background, and it marks where the subject's rows begin under the pane list.

MEANS
- the ground crosses the seam as a row's own field, so the pane names it and the host draws it;
- accent ink alone would not do: the pane row above it is the accent-marked subject.

PROVEN BY — `info-pane/pane.cpp` `say`; `surface/vocabulary.hpp` `kAccent`, `kMuted`, `kNone`,
`SurfaceTextRow`, `SurfaceTextRow::background`; `tests/test_workshop_panes_info.cpp` case
`"the two headings and both lists are the pane's rows, over the host's inventory and subject"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-07 — The ground is presentation and moves no geometry

LAW — Info lays each row's ground in its own picture on the lattice it reads its presses back through, so the grounded strip is the row a press names, and the ground moves no row or column.

MEANS
- the ground spans the room's width on its row's line, and a press is read back to a row and a column through that same lattice;
- to a host granting no canvas the rows go as prose, and a press there reaches nothing.

PROVEN BY — `workshop/pane_canvas_rows.hpp` `rows_picture`, `canvas_rows`, `row_cell_at`,
`CanvasRows::row_y`; `info-pane/pane.cpp` `on(PaneCanvasPointer)`, `fit_room`,
`pane_property_press`; `tests/test_workshop_panes_info.cpp` cases `"Info draws its lists on its
canvas on the medium's own ground, and a draft's caret and selection stand in its row where the
weaver types, moving no character, and go with it"`, `"a press on a pane row inspects it,
through the host's own door"` and `"a host that grants Info no canvas is shown its lists and a
draft's caret as prose, its presses reach nothing there, and its keys still inspect and edit"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`
