# Workshop law — geometry

Register `WL-GEO`: the composition in canvas cells, the reserved column, the fine lattice and the
unit a face reports. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-GEO-01 — One geometry draws a thing and hits it

LAW — The geometry that draws a thing and the geometry that hits it are one resolved geometry; Workshop has no click-bounds beside a paint-bounds.

MEANS
- `terminal_input_place` is the editable line resolved once; painter, caret and press call it;
- `completion_first_shown` is the list's one windowing, lifted out of `completion_rows`;
- what a panel is painted at and what it occupies are one resolved truth, on both media.

DOES NOT MEAN
- that a press may not have its own inverse — it may, if the inverse reads the painter's place.

PROVEN BY — `workshop/screen.hpp` `terminal_input_place`, `completion_first_shown`,
`paint_terminal`; `workshop/weave.hpp` `terminal_press`; `tests/test_workshop_screen.cpp` case
`"HD-3: the click reads the SAME window the rows were drawn with"`, case `"HD-3: hit geometry
follows presentation geometry across a resize"`, case `"what a panel is painted at and what it
occupies are one resolved truth"`.
WHY — `agents/decisions/one-geometry-draws-and-hits.md`

## WL-GEO-02 — The terminal pane's right edge is the workspace's

LAW — The Terminal pane wants `kTerminalWantW + (w - kScreenMinW)/2` columns, the room is the ceiling, and `terminal_x + terminal_w == room_w`: the pane ends at the workspace's edge.

MEANS
- at 78–80 columns the pane gets the room; want and room agree from 94 columns up;
- the pane shares no cell with the side region at any extent or metric, asserted at `kMinScreen`.

DOES NOT MEAN
- that `kTerminalWantW` is a floor — it is a want, and eight cells at the minimum is the price.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `kTerminalWantW`, `kScreenMinW`,
`Screen::room_w`, `Screen::terminal_x`, `Screen::terminal_w`; `tests/test_workshop_screen.cpp`
case `"HD-10: the want is unchanged and the room is the ceiling"`, case `"HD-10: the pane and the
side region share no cell, at any extent or metric"`.
WHY — `agents/decisions/the-reserved-column.md`

## WL-GEO-03 — The reservation is the screen's, not the pane's

LAW — `screen_of` subtracts the side column and the top rows whether or not a pane stands in them; a pane's presence, place, size or removal changes no room.

MEANS
- `room_w` and `room_h` are the screen's facts; what stands in a reserved place is the maker's;
- hide Info and the column stays empty; move or remove Layouts and the rows stay empty.

DOES NOT MEAN
- that the reserved rows are the Layouts pane's — `placement_bounds` merely defaults it there.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `Screen::room_w`, `Screen::room_h`, `kTopRows`,
`kBottomRows`, `placement_bounds`, `kPanelCols`, `kSideY`; `workshop/panel.hpp` `kTopBand`,
`placement::kSideRegion`; `tests/test_workshop_screen.cpp` case `"HD-10: the reservation is the
SCREEN's, and holds with no panel in it"`, case `"WUX-12/SC-9: the reservation does not follow the
Layouts pane"`.
WHY — `agents/decisions/the-reserved-column.md`

## WL-GEO-04 — Overlaps stay inside one owner's room

LAW — Presentations may overlap inside the room one owner has; none reaches into a column or row the screen reserved for another.

MEANS
- the completion list over the transcript, the picker over a slot, the pane over the workspace;
- the pane and the overlay stack meet only at the shortest screens, by a measured, bounded amount.

DOES NOT MEAN
- that a test may forbid overlap generally — it would forbid the three intentional ones.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `Screen::panel_x`, `kPanelGap`, `kStackRows`,
`kMinSide`; `tests/test_workshop_screen.cpp` case `"HD-10: what the pane DOES cover is unchanged,
and is on purpose"`, case `"HD-10/QR-14: the pane and the overlay stack meet only at the shortest
screens"`, case `"WIND-1: the stack/pane overlap grew by a bounded amount, and stayed in the
room"`.
WHY — `agents/decisions/the-reserved-column.md`

## WL-GEO-05 — The composition is settled in cells before any metric

LAW — `screen_of` answers in canvas cells with no text metric consulted; a metric only changes how much prose fits inside a placement it did not choose.

MEANS
- the same composition truth holds in a medium that sets type;
- a resize recomputes all of it, and nothing about it is remembered from one screen.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `kMinScreen`, `Screen`;
`tests/test_workshop_screen.cpp` case `"HD-10: the composition is DERIVED, and a resize recomputes
all of it"`, case `"HD-10: the same composition truth in a medium that sets type"`, case `"the
pane's interior follows the metric, and its placement does not"`.
WHY — `agents/decisions/the-reserved-column.md`

## WL-GEO-06 — Pane rectangles are sub-units of the canvas lattice

LAW — A pane rectangle is a fine rectangle in sub-units of a cell, 48 to the cell, a type distinct from the cell rectangle; a cell value enters the lattice by one multiply, at projection.

MEANS
- conversion is only `fine_of_cells` / `cells_covered`; nothing passes through `workspace_cell_x`;
- screen furniture, the document and every placement default stay whole cells;
- a one-pixel drag moves a pane by exactly one pixel of lattice; the file keeps every sub-unit.

PROVEN BY — `workshop/screen.hpp` `FineRect`, `fine_of_cells`, `cells_covered`, `project_pane`,
`workspace_cell_x`; `surface/vocabulary.hpp` `kCellSubs`; `ui/layout.hpp` `Rect`;
`workshop/setup.hpp` `kSubcells`, `kPaneSubMin`; `tests/test_workshop_screen.cpp` case `"WUX-2: a
one-pixel drag moves a pane by exactly one pixel of lattice"`, case `"WUX-2: fine geometry
survives the setup file without losing a sub-unit"`; `tests/test_surface.cpp` case `"WUX-2: the
sub-cell conversions are exact, floored, and total"`.
WHY — `agents/decisions/the-fine-lattice.md`

## WL-GEO-07 — One quantization law, every consumer, every grain

LAW — A presenter of device grain g shows a fine span [L,R) on device units [floor(L/g), floor(R/g)) and hit-tests by the same floor: the first painted unit answers the hand.

MEANS
- the device unit before a fractional edge does not answer — what you see is what you can grab;
- the TUI quantizes at its projection and never writes back; a thousand frames rewrite nothing.

PROVEN BY — `workshop/screen.hpp` `sub_span_contains`, `FineRect::contains_at`, `PointedAt`,
`PointedAt::sub`, `pane_edge_at`; `surface/skin_tui.hpp` `canvas_body`; `surface/pointing.hpp`
`sub_span_contains`; `tests/test_workshop_screen.cpp` case `"WUX-2: the hand meets exactly the
pixels a fine pane paints"`, case `"WUX-2: the TUI projects a fine pane onto its covered cells and
rewrites nothing"`; `tests/test_surface.cpp` case `"WUX-2: one quantization law -- a span lands on
device units by flooring both edges"`.
WHY — `agents/decisions/the-fine-lattice.md`

## WL-GEO-08 — The unit is the medium's answer, never Workshop's

LAW — `Session::cell_px` is the device unit the medium reported on `SurfaceExtent`; zero means the cell is the device unit. Workshop derives no unit, and a change of unit alone is a change.

MEANS
- every terminal, and any run no medium has spoken to, reads cells;
- a window that opens its canvas late does not leave a maker reading cells until something moves.

DOES NOT MEAN
- that Workshop may hold one Skin's layout number — correct only while there is one medium.

PROVEN BY — `workshop/screen.hpp` `adopt_screen`, `Session::cell_px`,
`Session::text_advance_px`, `Session::screen_w`, `Session::screen_h`; `surface/vocabulary.hpp`
`SurfaceExtent`; `workshop/weave.hpp` `on(SurfaceExtent)`; `tests/test_workshop_screen.cpp` case
`"WUX-6: the canvas's device unit is the medium's answer, never Workshop's"`;
`tests/test_surface.cpp` case `"WUX-6: each medium reports the device unit its own canvas is laid
out at"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-09 — Geometry is spelled in the face's unit by one derivation

LAW — A pane's geometry has one spelling path — one unit, one amount, one rect — with no per-medium table and no second conversion constant.

MEANS
- there is no unit type in Workshop;
- an axis authored in `pixels` keeps its own inline `px` whatever the face (`483x220px px`).

PROVEN BY — `workshop/screen.hpp` `geometry_unit`, `geometry_spelling`, `geometry_amount_text`,
`fine_rect_text`, `pane_window_text`, `GeometrySpelling`; `surface/region.hpp` `device_of_subs`;
`tests/test_workshop_screen.cpp` case `"WUX-6: one authored value, spelled in whatever unit the
active face reported"`; `tests/test_surface.cpp` case `"WUX-6: a medium's own device unit, and
whether it can say a value exactly"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-10 — A projection wears `~` and names the reason once

LAW — A value not exact in the active face's unit is spelled with `~`, and the line says `(~ projected)` once; an exact value carries neither.

MEANS
- a whole-cell value is exact on every medium; a pixel-authored value is exact in pixels only;
- the mark is ASCII, because the shipped face's letterform is 0x20–0x7E.

DOES NOT MEAN
- that a rounded value is ever presented as the stored one — the mark is the distinction.

PROVEN BY — `workshop/screen.hpp` `geometry_spelling`, `geometry_amount_text`, `fine_rect_text`,
`GeometrySpelling`; `tests/test_workshop_screen.cpp` case `"WUX-6: one authored value, spelled in
whatever unit the active face reported"`; `tests/test_workshop_panes_window.cpp` case
`"WUX-6/SC-2: the arrangement notice speaks the unit the FACE reported"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-11 — Looking is not authoring

LAW — No readout path writes: a session that crosses both media reading a geometry no terminal can say writes the same session file byte for byte, and the unit reaches no durable file.

MEANS
- not the spelling, not the notice, not a repaint, not a save;
- a session restore hands this run's unit straight back rather than resetting it to cells.

PROVEN BY — `workshop/weave.hpp` `arrange_status`; `workshop/session_persist.hpp`
`kFormatVersion`; `tests/test_workshop_persistence.cpp` case `"WUX-6/SC-4: a read-only visit
through the other medium writes the SAME BYTES"`, case `"WUX-6/SC-9: the medium's device unit
reaches no durable file"`; `tests/test_workshop_screen.cpp` case `"WUX-6: the medium's unit
reaches the READOUT and no geometry at all"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-12 — The notice says where an unplaced pane actually is

LAW — A pane with a reactive axis reads `-` for that axis, followed by ` -- now @x,y WxH <unit>` taken from the resolved, unclipped window; a fully authored window carries no such clause.

MEANS
- the clause names the unclipped ask — the rectangle a gesture measures from.

PROVEN BY — `workshop/weave.hpp` `arrange_status`, `managed_bounds`; `workshop/screen.hpp`
`pane_window_partly_default`; `tests/test_workshop_panes_window.cpp` case `"WUX-6/SC-6: the notice
says where a pane the maker did not place actually is"`; `tests/test_workshop_screen.cpp` case
`"WUX-6: which parts of a pane's window the maker has not authored"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## Do not assume

- That the reserved column is about overlap — it is about the reservation (WL-GEO-04).
- That a metric ever chooses a placement — it chooses how much prose fits (WL-GEO-05).
