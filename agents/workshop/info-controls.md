# Workshop law — the Info controls

Register `WL-CTRL`: the grounds Info's structural rows sit on, and a property view that carries no
controls of its own. Info inspects the one pane an office names (WL-INFO-14), and a pane is listed,
launched and closed from the Pane Manager, never from here. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-CTRL-01 — Info's property view is one heading, one list and a front sentence

LAW — Info’s pane-property view has no object controls: one heading, one list and a front sentence, reserved as WL-INFO-08 says.

WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-02 — Info's composition ends at the rows it was granted

LAW — With no footer there is nothing to keep at the end; `mark` and `placed` are still inverses over one composition, and `finish` still truncates to the granted rows (WL-INFO-04).

WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-03 — The draft's refusal is Info's own; every other refusal is the owner's

LAW — The draft's refusal is the pane's own, made before asking, and now holds back another subject (WL-INFO-14); every other refusal is the owner's, answered to the ask.

WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-04 — A row the weaver cannot author is muted, and refused in words

LAW — With no control there is nothing to present as unavailable; a row the weaver cannot author is still said in the muted role and refused in words when Return is pressed on it.

WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-05 — Info owns no act: naming a subject and writing a property are the host's

LAW — Info owns no act still: naming a subject and writing a property are the host's doors (`InspectPaneRequested`, `PaneCommitRequested`), answered by the party that owns the rows.

WHY — `agents/decisions/a-component-is-earned.md`

## WL-CTRL-06 — The subject's heading sits on a ground

LAW — The subject's heading is accent on muted and every other row sits on none; the ground is the row's own background, and it marks where the subject's rows begin, under the front sentence.

MEANS
- the ground crosses the seam as a row's own field, so the pane names it and the host draws it;
- accent ink alone would not do: a section's label inside the list is accent ink too.

WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-07 — The ground is presentation and moves no geometry

LAW — Info lays each row's ground in its own picture on the lattice it reads its presses back through, so the grounded strip is the row a press names, and the ground moves no row or column.

MEANS
- the ground spans the room's width on its row's line, and a press is read back to a row and a column through that same lattice.

WHY — `agents/decisions/a-footer-not-a-third-list.md`

## Proven by

### WL-CTRL-01
`info/property_view.hpp` compose
`tests/test_workshop_panes_info.cpp` "a room too short for the body invents none of it"

### WL-CTRL-02
`info/property_view.hpp` placed finish composed_
`tests/test_workshop_panes_info.cpp` "a press on an Info row while a notice stands names the row
painted there, and a full room keeps its last row under the notice"

### WL-CTRL-03
`info/property_view.hpp` kFinishTheEdit inspect_asked
`tests/test_workshop_panes_info.cpp` "a live draft holds another subject back, and the reason is the
weaver's" "an inspect asked while an Info draft is live is refused, keeping the draft, its text, the
subject and the desk through a new room, and inspecting resumes once the draft ends"

### WL-CTRL-04
`info/property_view.hpp` say_properties not_authored
`tests/test_workshop_panes_info.cpp` "a row the screen makes is refused by the pane, in its own
words"

### WL-CTRL-05
`info/property_view.hpp` ask_inspect ask_commit
`workshop/weave_inspection.cpp` on(InspectPaneRequested) on(PaneCommitRequested)
`tests/test_workshop_panes_info.cpp` "an office's ask inspects a pane through the host's own door,
and the office is answered what the host said"

### WL-CTRL-06
`info/property_view.hpp` compose
`graphics/role.hpp` kAccent kMuted kNone
`workshop/pane_rows.hpp` TextRow TextRow::background
`tests/test_workshop_panes_info.cpp` "the heading and the subject's properties are the pane's rows,
over the host's subject, and the panes are the Pane Manager's to list"

### WL-CTRL-07
`workshop/pane_drawing.hpp` rows_drawing window_cell RowsRoom
`info/pane.cpp` fit_room press
`tests/test_workshop_panes_info.cpp` "Info draws its properties on its canvas on the medium's own
ground, and a draft's caret and selection stand in its row where the weaver types, moving no
character, and go with it"
