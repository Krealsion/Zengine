# Workshop law — the Info controls

Register `WL-CTRL`: the grounds Info's structural rows sit on, and the footer of controls it
retired. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

**What retired.** `[ Create ]` and `[ Delete ]` were two pressable rows under the object
inspector, asking the object document for the operations `n` and `d` were bound to. The object
document retired with its canvas, and Info inspects panes (WL-INFO-14): a pane is launched and
closed from the Pane Manager, never from here. The five laws about the footer are kept below as
RETIRED entries so an id cited elsewhere still says what became of it.

## WL-CTRL-01 — RETIRED: the last two rows were a footer of controls

LAW — Info says no controls: its body is two headings, two lists and a front sentence, reserved and shared as WL-INFO-08 and WL-INFO-07 say.

PROVEN BY — `info-pane/pane.cpp` `say`; `tests/test_workshop_panes_info.cpp` case
`"INFO-WEAVE: a room too short for the body invents none of it"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-02 — RETIRED: spare rows were written blank above a footer

LAW — With no footer there is nothing to keep at the end; `mark` and `placed` are still inverses over one composition, and `finish` still truncates to the granted rows (WL-INFO-04).

PROVEN BY — `info-pane/pane.cpp` `placed`, `finish`, `composed_`;
`tests/test_workshop_panes_info.cpp` case `"a press on an Info row while a notice stands names
the row painted there, and a full room keeps its last row under the notice"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-03 — RETIRED: availability was two reasons, one bit, two owners

LAW — The draft's refusal is the pane's own, made before asking, and now holds back another subject (WL-INFO-09); every other refusal is the owner's, answered to the ask.

PROVEN BY — `info-pane/pane.cpp` `kFinishTheEdit`, `press_placed`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a live draft holds another subject
back, and the reason is the maker's"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-04 — RETIRED: unavailable was said in characters

LAW — With no control there is nothing to present as unavailable; a row the maker cannot author is still said in the muted role and refused in words when Return is pressed on it.

PROVEN BY — `info-pane/pane.cpp` `say_properties`, `not_authored`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a row the screen makes is refused by
the pane, in its own words"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-05 — RETIRED: the controls did not own the acts

LAW — Info owns no act still: naming a subject and writing a property are the host's doors (`InspectPaneRequested`, `PaneCommitRequested`), answered by the party that owns the rows.

PROVEN BY — `info-pane/pane.cpp` `ask_inspect`, `ask_commit`;
`workshop/weave_pane_editor.cpp` `on(InspectPaneRequested)`, `on(PaneCommitRequested)`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a press on a pane row inspects it,
through the host's own door"`.
WHY — `agents/decisions/a-component-is-earned.md`

## WL-CTRL-06 — The subject's heading sits on a ground

LAW — The subject's heading is accent on muted and every other row sits on none; the ground is the row's own background, and it marks where the subject's rows begin under the pane list.

MEANS
- the ground crosses the seam as a row's own field, so the pane names it and the host draws it;
- accent ink alone would not do: the pane row above it is the accent-marked subject.

PROVEN BY — `info-pane/pane.cpp` `say`; `surface/vocabulary.hpp` `kAccent`, `kMuted`, `kNone`,
`SurfaceTextRow`, `SurfaceTextRow::background`; `tests/test_workshop_panes_info.cpp` case
`"INFO-WEAVE: the two headings and both lists are the pane's rows, over the host's inventory
and subject"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`

## WL-CTRL-07 — The ground is presentation and moved no geometry

LAW — A row's ground is the host's to draw at the row the pane published it on, so the grounded strip is exactly the prose row the press inverse inverts, and no row index or hit mapping moved for it.

PROVEN BY — `workshop/screen_external.cpp` `paint_external`; `surface/pointing.hpp`
`prose_row_of_pixel`; `surface/region.hpp` `kTextInsetPx`;
`tests/test_workshop_panes_info.cpp` case `"INFO-WEAVE: a press on a pane row inspects it,
through the host's own door"`.
WHY — `agents/decisions/a-footer-not-a-third-list.md`
