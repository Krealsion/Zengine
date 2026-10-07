# Workshop law — regions

Register `WL-RGN`: semantic text in Workshop's own panes, the Builder's priorities, the foot
band, and the name on a weaver's material. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

Retired: WL-RGN-04, WL-RGN-05.

## WL-RGN-01 — The popup and a pane's body spend `prose_place`

LAW — `prose_place` + `prose_region` is the one call for the popup and a pane's captured body: the prose rows and columns the active medium fits inside the popup's or the body's own rectangle.

PROVEN BY — `workshop/screen_pane_state.cpp` `prose_place`, `prose_region`;
`workshop/screen.hpp` `ProsePlace`; `workshop/screen_attention.cpp` `paint_context`;
`tests/test_workshop_screen.cpp` case `"the popup opens at the press's own cell, and its extent is
its content"`, case `"the contextual surface is its actions, and its width is theirs"`;
`tests/test_workshop_host.cpp` case `"a code-backed subject's interior is a read-only
capture, and an unresolved one is nothing to inspect"`.
WHY — `agents/decisions/semantic-text-owns-its-room.md`

## WL-RGN-02 — The Builder is a region composed by explicit priority

LAW — Each Builder fact carries a distinct priority: the budget keeps the most important, the display order never changes, facts drop whole, and `said…` wraps into exactly the rows that survived.

MEANS
- header, recipe, the `project` frontier while one waits, last, exit, ran, realize, `said…`;
- the realize row has three faces and no second row: armed, the button, or the outcome;
- the pane is a WEAVE and composes into the rows of the room it is granted, not a region it resolved.

PROVEN BY — `builder-pane/pane.cpp` `say_builder`, `publish`, `labelled_block`;
`tests/test_workshop_panes_builder.cpp` case
`"the pane asks the tool what it is on its own room grant, and shows it"`, case
`"the frontier row comes from the host's read-only door"`, case
`"after a plain build that worked, `B` is the button"`.
WHY — `agents/decisions/semantic-text-owns-its-room.md`

## WL-RGN-03 — The foot band is the notice, then the legend

LAW — `band_region` composes the notice first and `budget - 1` legend rows after it; at a budget of one it keeps the notice while there is one, and one legend row otherwise.

MEANS
- the band's height is `kBottomRows` cells; its rows are whatever the face fits in them;
- the layout tabs, the setup status and the workspace fact are the Layouts pane's, not the band's.

DOES NOT MEAN
- that the notice is ever shortened — it is cut with a mark and kept whole in the session.

PROVEN BY — `workshop/screen.hpp` `band_bounds`, `band_fit`, `kBottomRows`;
`workshop/screen_compose.cpp` `band_region`; `workshop/screen_gestures.cpp` `help_rows`;
`tests/test_workshop_document.cpp` case `"two bands compose their budgets, and the selector is row
0"`; `tests/test_workshop_screen.cpp` case `"the notice is a band row, and the
SENTENCE is never shortened"`.
WHY — `agents/decisions/two-bands.md`
