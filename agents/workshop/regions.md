# Workshop law — regions

Register `WL-RGN`: semantic text in Workshop's own panels, the Builder's priorities, the foot
band, and the name on a maker's material. One law per heading; cite by ID. Router:
[`../workshop.md`](../workshop.md).

## WL-RGN-01 — A panel whose rectangle is its own spends `panel_prose_place`

LAW — `panel_prose_place` + `panel_prose_region` is the one call for the picker, the popup and an external pane: the prose rows and columns the active medium fits inside the panel's own rectangle.

PROVEN BY — `workshop/screen_pane_state.cpp` `panel_prose_place`, `panel_prose_region`;
`workshop/screen.hpp` `PanelProsePlace`; `tests/test_workshop_screen.cpp` case `"TYPE-0: the
picker is ONE bounded region, and its cells are what it used to write"`, case `"TYPE-0: the picker
spends the ACTIVE medium's rows, and says what it omitted"`.
WHY — `agents/decisions/semantic-text-owns-its-room.md`

## WL-RGN-02 — The Builder is a region composed by explicit priority

LAW — Each Builder fact carries a distinct priority: the budget keeps the most important, the display order never changes, facts drop whole, and `said…` wraps into exactly the rows that survived.

MEANS
- header, recipe, the `project` frontier while one waits, last, exit, ran, realize, `said…`;
- the catalog row costs one `said` row, and only where it is present.

PROVEN BY — `workshop/screen_pane_state.cpp` `paint_builder`, `panel_block`;
`tests/test_workshop_document.cpp` case `"WUX-1/SC-4: the Builder keeps the facts a maker acts on,
by explicit priority"`, case `"PROJ-1: the catalog row costs one `said` row, and only where it is
present"`.
WHY — `agents/decisions/semantic-text-owns-its-room.md`

## WL-RGN-03 — The foot band is the notice, then the legend

LAW — `band_region` composes the notice first and `budget - 1` legend rows after it; at a budget of one it keeps the notice while there is one, and one legend row otherwise.

MEANS
- the band's height is `kBottomRows` cells; its rows are whatever the face fits in them;
- the layout tabs, the setup status and the workspace fact are the Layouts pane's, not the band's.

DOES NOT MEAN
- that the notice is ever shortened — it is cut with a mark and kept whole in the session.

PROVEN BY — `workshop/screen.hpp` `band_bounds`, `band_fit`, `kBottomRows`;
`workshop/screen_pane_editor.cpp` `band_region`; `workshop/screen_gestures.cpp` `help_rows`;
`tests/test_workshop_document.cpp` case `"QR-14/SC-2+SC-7: two bands compose their budgets, and
the selector is row 0"`; `tests/test_workshop_screen.cpp` case `"TYPE-0/WUX-1: the notice is a
band row, and the SENTENCE is never shortened"`.
WHY — `agents/decisions/two-bands.md`

## WL-RGN-04 — The workspace object's name is a `kGroundBeneath` region over its own rectangle

LAW — The name is set in the medium's own type on the object's material: not an ordinary region, which erases the material, and not a role carried as a ground, which replaces `glyph_for_role`'s `#`.

PROVEN BY — `workshop/screen_pane_editor.cpp` `kGroundBeneath`; `surface/skin_tui.hpp`
`glyph_for_role`; `tests/test_workshop_screen.cpp` case `"TYPE-1: the object's name is set in the
medium's own type, ON its material"`, case `"TYPE-1: the character medium's picture did not move,
and its `#` is why"`, case `"TYPE-1: the name is over every object's material and under nothing it
should be"`.
WHY — `agents/decisions/semantic-text-owns-its-room.md`

## WL-RGN-05 — A name is bounded by the material it names

LAW — The name's bounds are `min(object width, workspace right edge - x)` by the object's height, each floored at one cell, so a name that does not fit is marked by `detail::fit` rather than fading.

MEANS
- a one-cell object falls back to cells with no `if (h < N)` written anywhere;
- the authored name is untouched; widening the object reveals more of the same bytes.

PROVEN BY — `workshop/screen_bindings.cpp` `detail::fit`; `workshop/screen_pane_editor.cpp`
`paint`; `tests/test_workshop_screen.cpp` case `"QR-3: the name's bound is the OBJECT'S resolved
width, clipped by the workspace"`, case `"QR-3: no part of a name is drawn outside the material it
names"`, case `"TYPE-1: a tiny object shows its name in CELLS, and no rule was written to say
so"`.
WHY — `agents/decisions/semantic-text-owns-its-room.md`
