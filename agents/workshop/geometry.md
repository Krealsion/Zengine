# Workshop law — geometry

Register `WL-GEO`: the composition in canvas pixels, the right column, the whole pixel and the
unit a face reports. One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-GEO-01 — One geometry draws a thing and hits it

LAW — The geometry that draws a thing and the geometry that hits it are one resolved geometry; Workshop has no click-bounds beside a paint-bounds.

MEANS
- `external_body_place` grants a pane's room and locates a press in it: one call, both ways;
- a pane's caret is merged with the same header offset a press subtracts (`WL-CARET-01`);
- what a pane is painted at and what it occupies are one resolved truth, on both media.

DOES NOT MEAN
- that a press may not have its own inverse — it may, read from the origin the painter drew at.

PROVEN BY — `workshop/screen_external.cpp` `external_body_place`, `paint_external`;
`workshop/screen_gestures.cpp` `prose_at`; `surface/pointing.hpp` `prose_column_of_pixel`,
`prose_row_of_pixel`, `floor_to_grain`; `workshop/weave_inspection.cpp` `cell_center`;
`tests/test_workshop_panes_terminal.cpp` case `"a press on the input row places the caret where
the weaver aimed"`; `tests/test_workshop_screen.cpp` case
`"what a pane is painted at and what it occupies are one resolved truth"`;
`tests/test_workshop_panes_input.cpp` case `"text a window sets in cells, in a body too short for
a row of its face, is drawn, pressed and aimed at from one origin"`.
WHY — `agents/decisions/one-geometry-draws-and-hits.md`

## WL-GEO-02 — `screen_of` sizes no tool's rectangle

LAW — The screen answers the room, the bands, the right column's PLACE and the text metric, and nothing else: no presentation has a rectangle here, and a pane's is the arrangement's.

MEANS
- the four constants that sized the terminal overlay left with it;
- so did the six `Screen` fields that carried its corner, its extent and its interior.

DOES NOT MEAN
- that overlap was patched. It ENDED: what it pinned is a pane over a pane, with a boundary.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `kScreenMinW`, `Screen::room_w`,
`Screen::side_x`; `tests/test_workshop_screen.cpp` case `"a pane may lie over a pane, and the
boundary is what makes it legible"`, case `"the screen's extent is TOTAL over whatever
a medium published"`.
WHY — `agents/decisions/the-room-is-the-screen.md`

## WL-GEO-03 — The room is the surface, and the right column stands on it

LAW — `room_w` is the screen's whole width; only the top and bottom bands come off the height. The right column is a PLACE `kSideCols` cells from the right edge, reserved out of nothing.

MEANS
- a pane's presence, place, size or removal changes no room, and neither does the column;
- take the pane off the desk and the weaver gets thirty columns of workspace, not of nothing.

DOES NOT MEAN
- that the bands are a pane's — `placement_bounds` merely defaults the Layouts pane to them.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `Screen::room_w`, `Screen::room_h`, `kTopRows`,
`kBottomRows`, `placement_bounds`, `kSideCols`, `Screen::room_y`; `workshop/panes.hpp` `kTopBand`,
`placement::kSideRegion`; `tests/test_workshop_screen.cpp` case `"the screen's furniture cannot
see a pane, open or closed"`, case `"the reservation does not follow the Layouts pane"`.
WHY — `agents/decisions/the-room-is-the-screen.md`

## WL-GEO-04 — Overlaps are measured, not forbidden

LAW — Presentations may overlap; every overlap this composition makes is measured exactly, and none of them may leave the room.

MEANS
- a pane over the workspace, a pane over another pane (the picker over a slot, while it was);
- the stack's slot reaching into the right column, counted at every extent.

DOES NOT MEAN
- that a test may forbid overlap generally — it would forbid every one of them.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `Screen::side_x`, `kStackRows`, `kMinSide`;
`tests/test_workshop_screen.cpp` case `"a pane may lie over a pane, and the boundary is what makes
it legible"`, case `"an overlapping pane is painted where it is hit, in
both front orders"`.
WHY — `agents/decisions/the-room-is-the-screen.md`

## WL-GEO-05 — The composition is pixels, its bands fitted to the text they hold

LAW — `screen_of` answers in canvas pixels: each band is as tall as the rows it holds in the face's metric, inside one device unit of chrome, or whole cells without type; the room is what is left.

MEANS
- the top band holds the Layouts pane's `kTopRows` rows and the foot `kBottomRows`, on every face;
- a resize or a new metric recomputes all of it, and nothing of it is remembered from one screen;
- pane preferences resolve separately, against the room this answers.

PROVEN BY — `workshop/screen.hpp` `screen_of`, `band_px_for`, `kMinScreen`, `Screen`,
`Screen::room_y`, `Screen::notice_y`; `tests/test_workshop_screen.cpp` case `"the screen's
furniture cannot see a pane, open or closed"`, case `"the screen's extent is TOTAL over whatever
a medium published"`, case `"the notice is a band row, and the SENTENCE is never shortened"`.
WHY — `agents/decisions/the-whole-pixel.md`

## WL-GEO-06 — Workshop's unit is the whole pixel

LAW — A pane rectangle is a `PixelRect` of whole canvas pixels, twelve to the cell, distinct from the cell rectangle; a cell enters by one multiply, and a span leaves only as the cells it covers.

MEANS
- conversion is only `pixels_of_cells` / `cells_covered`; nothing finer than a pixel is held;
- an axis authored in `pixels` is presented at exactly its pixels, on every medium;
- a one-pixel drag moves a pane by exactly one pixel; the file keeps every pixel.

PROVEN BY — `workshop/screen.hpp` `PixelRect`, `pixels_of_cells`, `cells_covered`;
`workshop/screen_chrome.cpp` `project_pane`; `surface/vocabulary.hpp` `kCanvasCellPx`;
`ui/layout.hpp` `Rect`; `workshop/setup.hpp` `kPixels`, `kPanePxMin`;
`tests/test_workshop_screen.cpp` case `"a one-pixel drag moves a pane by exactly one pixel of
lattice"`, case `"pixel geometry survives the setup file without losing a pixel"`;
`tests/test_workshop_panes_window.cpp` case `"an axis in pixels is presented at exactly its
pixels, on every medium"`; `tests/test_surface.cpp` case `"the pixel and cell conversions are
exact, floored, and total"`.
WHY — `agents/decisions/the-whole-pixel.md`

## WL-GEO-07 — One quantization law, every consumer, every grain

LAW — A presenter of grain g shows a pixel span [L,R) on device units [floor(L/g), floor(R/g)) and hits by the same floor: a window's grain is a pixel, a terminal's a cell.

MEANS
- the window draws the picture 1:1; a terminal floors every span to the cells it covers;
- the cell before an edge inside a cell does not answer — what you see is what you can grab;
- the TUI quantizes at its projection and never writes back; a thousand frames rewrite nothing.

PROVEN BY — `workshop/screen.hpp` `PixelRect::contains_at`, `PointedAt`, `PointedAt::px`;
`workshop/screen_arrange.cpp` `pane_edge_at`; `surface/skin_tui.hpp` `canvas_body`;
`surface/pointing.hpp` `px_span_contains`, `kPixelGrainPx`, `kCellGrainPx`;
`tests/test_workshop_screen.cpp` case `"the hand meets exactly the pixels and cells a pane
paints"`, case `"the TUI projects a pane onto its covered cells and rewrites nothing"`;
`tests/test_surface.cpp` case `"one quantization law -- a span lands on device units by flooring
both edges"`.
WHY — `agents/decisions/the-whole-pixel.md`

## WL-GEO-08 — The room and the unit are the medium's answer, never Workshop's

LAW — `SurfaceExtent` says the room in canvas pixels and `cell_px` the device unit; zero means the cell, a terminal's room is its cells times twelve, and Workshop derives neither.

MEANS
- every terminal, and any run no medium has spoken to, reads cells;
- a terminal's geometry is derived from its cells, never measured or written back (WL-GEO-11);
- a change of unit alone is a change: a window opened late does not leave a weaver in cells.

DOES NOT MEAN
- that Workshop may hold one Skin's layout number — correct only while there is one medium.

PROVEN BY — `workshop/screen_bindings.cpp` `adopt_screen`; `workshop/screen.hpp`
`Session::cell_px`, `Session::text_advance_px`, `Session::screen_w`, `Session::screen_h`,
`Screen::cell_px`, `Screen::text_advance_px`; `surface/vocabulary.hpp` `SurfaceExtent`;
`workshop/weave_handlers.cpp` `on(SurfaceExtent)`; `tests/test_workshop_screen.cpp` case `"the
canvas's device unit is the medium's answer, never Workshop's"`; `tests/test_surface.cpp` case
`"each medium reports the device unit its own canvas is laid out at"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-09 — Geometry is spelled in the face's unit by one derivation

LAW — A pane's geometry has one spelling path — one unit, one amount, one rect — with no per-medium table and no second conversion constant.

MEANS
- there is no unit type in Workshop;
- the unit is said once per line, where a number in it was printed (`@75,60 482x12 px f0`).

PROVEN BY — `workshop/screen_pane_state.cpp` `geometry_unit`, `geometry_spelling`,
`geometry_amount_text`, `pixel_rect_text`, `pane_window_text`; `workshop/screen.hpp`
`GeometrySpelling`; `surface/region.hpp` `device_of_px`; `tests/test_workshop_screen.cpp` case
`"one authored value, spelled in whatever unit the active face reported"`;
`tests/test_surface.cpp` case `"a medium's own device unit, and whether it can say
a value exactly"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-10 — A projection wears `~` and names the reason once

LAW — A value not exact in the active face's unit is spelled with `~`, and the line says `(~ projected)` once; an exact value carries neither.

MEANS
- a whole-cell value is exact on every medium; a pixel-authored value is exact in pixels only;
- the mark is ASCII, because the shipped face's letterform is 0x20–0x7E.

DOES NOT MEAN
- that a rounded value is ever presented as the stored one — the mark is the distinction.

PROVEN BY — `workshop/screen_pane_state.cpp` `geometry_spelling`, `geometry_amount_text`,
`pixel_rect_text`; `workshop/screen.hpp` `GeometrySpelling`; `tests/test_workshop_screen.cpp` case
`"one authored value, spelled in whatever unit the active face reported"`;
`tests/test_workshop_panes_window.cpp` case `"the arrangement notice speaks the unit
the FACE reported"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-11 — Looking is not authoring

LAW — No readout path writes: a session that crosses both media reading a geometry no terminal can say writes the same session file byte for byte, and the unit reaches no durable file.

MEANS
- not the spelling, not the notice, not a repaint, not a save;
- a session restore hands this run's unit straight back rather than resetting it to cells.

PROVEN BY — `workshop/weave_arrange.cpp` `arrange_status`; `workshop/session_persist.hpp`
`kFormatVersion`; `tests/test_workshop_persistence.cpp` case `"a read-only visit through the other
medium writes the SAME BYTES"`, case `"the medium's device unit reaches no
durable file"`; `tests/test_workshop_screen.cpp` case `"the medium's unit reaches the
READOUT and no geometry at all"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-12 — The notice says where an unplaced pane actually is

LAW — A pane with a reactive axis reads `-` for that axis, followed by ` -- now @x,y WxH <unit>` taken from the resolved, unclipped window; a fully authored window carries no such clause.

MEANS
- the clause names the unclipped ask — the rectangle a gesture measures from.

PROVEN BY — `workshop/weave_arrange.cpp` `arrange_status`, `managed_bounds`;
`workshop/screen_pane_state.cpp` `pane_window_partly_default`;
`tests/test_workshop_panes_window.cpp` case `"the notice says where a pane the weaver did not
place actually is"`; `tests/test_workshop_screen.cpp` case `"which parts of a pane's
window the weaver has not authored"`.
WHY — `agents/decisions/the-face-reports-the-unit.md`

## Do not assume

- That the right column is reserved out of the room — the room is the surface (WL-GEO-03).
- That a metric chooses where a pane goes — it sizes the two bands, and a default place is
  measured from what they leave (WL-GEO-05).
