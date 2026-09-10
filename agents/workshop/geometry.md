# Workshop law — geometry

Register `WL-GEO`: the composition in canvas cells, the right column, the fine lattice and the
unit a face reports. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-GEO-01 — One geometry draws a thing and hits it

LAW — The geometry that draws a thing and the geometry that hits it are one resolved geometry; Workshop has no click-bounds beside a paint-bounds.

MEANS
- `external_body_place` grants a pane's room and locates a press in it: one call, both ways;
- a pane's caret is merged with the same header offset a press subtracts (`WL-CARET-01`);
- what a panel is painted at and what it occupies are one resolved truth, on both media.

DOES NOT MEAN
- that a press may not have its own inverse — it may, if the inverse reads the painter's place.

PROVEN BY — `workshop/screen_external.cpp` `external_body_place`, `paint_external`;
`tests/test_workshop_panes_terminal.cpp` case `"TERM-W12: a press on the
input row places the caret where the maker aimed"`; `tests/test_workshop_screen.cpp` case
`"what a panel is painted at and what it occupies are one resolved truth"`.
WHY — `agents/decisions/one-geometry-draws-and-hits.md`

## WL-GEO-02 — `screen_of` sizes no tool's rectangle

LAW — The screen answers the room, the bands, the right column's PLACE and the text metric, and nothing else: no presentation has a rectangle here, and a pane's is the arrangement's.

MEANS
- the four constants that sized the terminal overlay left with it (VD-24);
- so did the six `Screen` fields that carried its corner, its extent and its interior.

DOES NOT MEAN
- that HD-10 was patched. It ENDED: what it pinned is a pane over a pane, with a boundary.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `kScreenMinW`, `Screen::room_w`,
`Screen::panel_x`; `tests/test_workshop_screen.cpp` case `"HD-10 is over: a pane over a pane,
and the boundary is what makes it legible"`, case `"the screen's extent is TOTAL over whatever
a medium published"`.
WHY — `agents/decisions/the-room-is-the-screen.md`

## WL-GEO-03 — The room is the surface, and the right column stands on it

LAW — `room_w` is the screen's whole width; only the top and bottom bands come off the height. The right column is a PLACE at `w - kPanelCols`, reserved out of nothing.

MEANS
- a pane's presence, place, size or removal changes no room, and neither does the column;
- take the pane off the desk and the maker gets thirty columns of workspace, not of nothing.

DOES NOT MEAN
- that the bands are a pane's — `placement_bounds` merely defaults the Layouts pane to them.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `Screen::room_w`, `Screen::room_h`, `kTopRows`,
`kBottomRows`, `placement_bounds`, `kPanelCols`, `kSideY`; `workshop/panel.hpp` `kTopBand`,
`placement::kSideRegion`; `tests/test_workshop_screen.cpp` case `"HD-10: the screen's furniture
cannot see a panel, open or closed"`, case `"WUX-12/SC-9: the reservation does not follow the
Layouts pane"`.
WHY — `agents/decisions/the-room-is-the-screen.md`

## WL-GEO-04 — Overlaps are measured, not forbidden

LAW — Presentations may overlap; every overlap this composition makes is measured exactly, and none of them may leave the room.

MEANS
- the picker over a slot, a pane over the workspace, a pane over another pane;
- the stack's slot reaching into the right column, counted at every extent.

DOES NOT MEAN
- that a test may forbid overlap generally — it would forbid every one of them.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `Screen::panel_x`, `kStackRows`, `kMinSide`;
`tests/test_workshop_screen.cpp` case `"HD-10 is over: a pane over a pane, and the boundary is
what makes it legible"`, case `"the picker occupies the slot it opens over, and answers for it
while it is there"`.
WHY — `agents/decisions/the-room-is-the-screen.md`

## WL-GEO-05 — The composition is settled in cells before any metric

LAW — `screen_of` answers in canvas cells with no text metric consulted; a metric only changes how much prose fits inside a placement it did not choose.

MEANS
- the same composition truth holds in a medium that sets type;
- a resize recomputes all of it, and nothing about it is remembered from one screen.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `kMinScreen`, `Screen`;
`tests/test_workshop_screen.cpp` case `"HD-10: the screen's furniture cannot see a panel, open
or closed"`, case `"the screen's extent is TOTAL over whatever a medium published"`.
WHY — `agents/decisions/the-reserved-column.md`

## WL-GEO-06 — Pane rectangles are sub-units of the canvas lattice

LAW — A pane rectangle is a fine rectangle in sub-units of a cell, 48 to the cell, a type distinct from the cell rectangle; a cell value enters the lattice by one multiply, at projection.

MEANS
- conversion is only `fine_of_cells` / `cells_covered`; nothing passes through `workspace_cell_x`;
- screen furniture, the document and every placement default stay whole cells;
- a one-pixel drag moves a pane by exactly one pixel of lattice; the file keeps every sub-unit.

PROVEN BY — `workshop/screen.hpp` `FineRect`, `fine_of_cells`, `cells_covered`;
`workshop/screen_chrome.cpp` `project_pane`; `workshop/screen_gestures.cpp` `workspace_cell_x`;
`surface/vocabulary.hpp` `kCellSubs`; `ui/layout.hpp` `Rect`; `workshop/setup.hpp` `kSubcells`,
`kPaneSubMin`; `tests/test_workshop_screen.cpp` case `"WUX-2: a one-pixel drag moves a pane by
exactly one pixel of lattice"`, case `"WUX-2: fine geometry survives the setup file without losing
a sub-unit"`; `tests/test_surface.cpp` case `"WUX-2: the sub-cell conversions are exact, floored,
and total"`.
WHY — `agents/decisions/the-fine-lattice.md`

## WL-GEO-07 — One quantization law, every consumer, every grain

LAW — A presenter of device grain g shows a fine span [L,R) on device units [floor(L/g), floor(R/g)) and hit-tests by the same floor: the first painted unit answers the hand.

MEANS
- the device unit before a fractional edge does not answer — what you see is what you can grab;
- the TUI quantizes at its projection and never writes back; a thousand frames rewrite nothing.

PROVEN BY — `workshop/screen.hpp` `sub_span_contains`, `FineRect::contains_at`, `PointedAt`,
`PointedAt::sub`; `workshop/screen_arrange.cpp` `pane_edge_at`; `surface/skin_tui.hpp`
`canvas_body`; `surface/pointing.hpp` `sub_span_contains`; `tests/test_workshop_screen.cpp` case
`"WUX-2: the hand meets exactly the pixels a fine pane paints"`, case `"WUX-2: the TUI projects a
fine pane onto its covered cells and rewrites nothing"`; `tests/test_surface.cpp` case `"WUX-2:
one quantization law -- a span lands on device units by flooring both edges"`.
WHY — `agents/decisions/the-fine-lattice.md`

## WL-GEO-08 — The unit is the medium's answer, never Workshop's

LAW — `Session::cell_px` is the device unit the medium reported on `SurfaceExtent`; zero means the cell is the device unit. Workshop derives no unit, and a change of unit alone is a change.

MEANS
- every terminal, and any run no medium has spoken to, reads cells;
- a window that opens its canvas late does not leave a maker reading cells until something moves.

DOES NOT MEAN
- that Workshop may hold one Skin's layout number — correct only while there is one medium.

PROVEN BY — `workshop/screen_bindings.cpp` `adopt_screen`; `workshop/screen.hpp`
`Session::cell_px`, `Session::text_advance_px`, `Session::screen_w`, `Session::screen_h`,
`Screen::cell_px`, `Screen::text_advance_px`; `surface/vocabulary.hpp` `SurfaceExtent`;
`workshop/weave_handlers.cpp` `on(SurfaceExtent)`; `tests/test_workshop_screen.cpp` case `"WUX-6:
the canvas's device unit is the medium's answer, never Workshop's"`; `tests/test_surface.cpp` case
`"WUX-6: each medium reports the device unit its own canvas is laid out at"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-09 — Geometry is spelled in the face's unit by one derivation

LAW — A pane's geometry has one spelling path — one unit, one amount, one rect — with no per-medium table and no second conversion constant.

MEANS
- there is no unit type in Workshop;
- an axis authored in `pixels` keeps its own inline `px` whatever the face (`483x220px px`).

PROVEN BY — `workshop/screen_pane_state.cpp` `geometry_unit`, `geometry_spelling`,
`geometry_amount_text`, `fine_rect_text`, `pane_window_text`; `workshop/screen.hpp`
`GeometrySpelling`; `surface/region.hpp` `device_of_subs`; `tests/test_workshop_screen.cpp` case
`"WUX-6: one authored value, spelled in whatever unit the active face reported"`;
`tests/test_surface.cpp` case `"WUX-6: a medium's own device unit, and whether it can say a value
exactly"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-10 — A projection wears `~` and names the reason once

LAW — A value not exact in the active face's unit is spelled with `~`, and the line says `(~ projected)` once; an exact value carries neither.

MEANS
- a whole-cell value is exact on every medium; a pixel-authored value is exact in pixels only;
- the mark is ASCII, because the shipped face's letterform is 0x20–0x7E.

DOES NOT MEAN
- that a rounded value is ever presented as the stored one — the mark is the distinction.

PROVEN BY — `workshop/screen_pane_state.cpp` `geometry_spelling`, `geometry_amount_text`,
`fine_rect_text`; `workshop/screen.hpp` `GeometrySpelling`; `tests/test_workshop_screen.cpp` case
`"WUX-6: one authored value, spelled in whatever unit the active face reported"`;
`tests/test_workshop_panes_window.cpp` case `"WUX-6/SC-2: the arrangement notice speaks the unit
the FACE reported"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-11 — Looking is not authoring

LAW — No readout path writes: a session that crosses both media reading a geometry no terminal can say writes the same session file byte for byte, and the unit reaches no durable file.

MEANS
- not the spelling, not the notice, not a repaint, not a save;
- a session restore hands this run's unit straight back rather than resetting it to cells.

PROVEN BY — `workshop/weave_arrange.cpp` `arrange_status`; `workshop/session_persist.hpp`
`kFormatVersion`; `tests/test_workshop_persistence.cpp` case `"WUX-6/SC-4: a read-only visit
through the other medium writes the SAME BYTES"`, case `"WUX-6/SC-9: the medium's device unit
reaches no durable file"`; `tests/test_workshop_screen.cpp` case `"WUX-6: the medium's unit
reaches the READOUT and no geometry at all"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-12 — The notice says where an unplaced pane actually is

LAW — A pane with a reactive axis reads `-` for that axis, followed by ` -- now @x,y WxH <unit>` taken from the resolved, unclipped window; a fully authored window carries no such clause.

MEANS
- the clause names the unclipped ask — the rectangle a gesture measures from.

PROVEN BY — `workshop/weave_arrange.cpp` `arrange_status`, `managed_bounds`;
`workshop/screen_pane_state.cpp` `pane_window_partly_default`;
`tests/test_workshop_panes_window.cpp` case `"WUX-6/SC-6: the notice says where a pane the maker
did not place actually is"`; `tests/test_workshop_screen.cpp` case `"WUX-6: which parts of a
pane's window the maker has not authored"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## Do not assume

- That the right column is reserved out of the room — the room is the surface (WL-GEO-03).
- That a metric ever chooses a placement — it chooses how much prose fits (WL-GEO-05).
