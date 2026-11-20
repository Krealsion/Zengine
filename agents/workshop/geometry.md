# Workshop law — geometry

Register `WL-GEO`: the composition in canvas pixels, the whole pixel, the unit a face reports,
and the desk and its words read in Workshop's numbers. One law per heading;
cite by ID. Router: [`../workshop.md`](../workshop.md).

Retired: WL-GEO-02, WL-GEO-03, WL-GEO-04, WL-GEO-05, WL-GEO-06, WL-GEO-09, WL-GEO-10, WL-GEO-12, WL-GEO-13

## WL-GEO-01 — One geometry draws a thing and hits it

LAW — The geometry that draws a thing and the geometry that hits it are one resolved geometry; Workshop has no click-bounds beside a paint-bounds.

MEANS
- `external_body_place` grants a pane's room and locates a press in it: one call, both ways;
- what a pane is painted at and what it occupies are one resolved truth, on both media.

DOES NOT MEAN
- that a press may not have its own inverse — it may, read from the origin the painter drew at.
- that a caret moves a press: it moves no character from its cell, so a cell names what it shows.

WHY — `agents/decisions/one-geometry-draws-and-hits.md`

## WL-GEO-07 — One quantization law, every consumer, every grain

LAW — A presenter of grain g shows a pixel span [L,R) on device units [floor(L/g), floor(R/g)) and hits by the same floor: a window's grain is a pixel, a terminal's a cell.

MEANS
- the window draws the picture 1:1; a terminal floors every span to the cells it covers;
- the cell before an edge inside a cell does not answer — what you see is what you can grab;
- the TUI quantizes at its projection and never writes back; a thousand frames rewrite nothing.

WHY — `agents/decisions/the-whole-pixel.md`

## WL-GEO-08 — The room and the unit are the medium's answer, never Workshop's

LAW — The window's offer (`WindowOffered`) says the room in canvas pixels and `cell_px` the device unit; zero means the cell, a terminal's room is its cells times twelve, and Workshop derives neither.

MEANS
- every terminal, and any run no medium has spoken to, reads cells;
- a terminal's geometry is derived from its cells, never measured or written back (WL-GEO-11);
- a change of unit alone is a change: a window opened late does not leave a weaver in cells.

DOES NOT MEAN
- that Workshop may hold one window's layout number — correct only while there is one window.

WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-11 — Looking is not authoring

LAW — No readout path writes: a session that crosses both media reading a geometry no terminal can say writes the same session file byte for byte, and the unit reaches no durable file.

MEANS
- not the spelling, not the notice, not a repaint, not a save;
- a session restore hands this run's unit straight back rather than resetting it to cells.

WHY — `agents/decisions/the-face-reports-the-unit.md`

## WL-GEO-14 — A dock keeps a strip, and its open pane covers the room

LAW — Each edge a shown row is docked on keeps a strip its whole length, open or shut, and the room is narrower by it; an open docked pane stands against the strip, the room's length, over it.

MEANS
- a strip is a text column on a left or right edge and a line on a top or bottom one, in whole cells;
- the pane's width across its edge is its own words' answer, floored to the grain; it is in front;
- docking a pane moves what stands against that edge once, by the strip; its open and its shut move nothing.

WHY — `agents/decisions/docks-cover-the-room.md`

## Do not assume

- That any part of the room is reserved for a pane — the room is the surface less the strips its
  docks keep (WL-GEO-14).
- That a metric chooses where a pane goes — it sizes the two bands, and a default place is
  measured from what they leave.

## Proven by

### WL-GEO-01
`workshop/screen_external.cpp` external_body_place paint_external external_press_at
`workshop/screen_gestures.cpp` prose_at
`workshop/screen.hpp` prose_column_of_pixel prose_row_of_pixel
`graphics/pixels.hpp` floor_to_grain
`workshop/desk_read.hpp` glyph_point
`tests/test_workshop_panes_terminal.cpp` "a press on the input row places the caret where the weaver
aimed"
`tests/test_workshop_screen.cpp` "what a pane is painted at and what it occupies are one resolved
truth"
`tests/test_workshop_panes_input.cpp` "text a window sets in cells, in a body too short for a row of
its face, is drawn, pressed and aimed at from one origin"
`tests/test_workshop_desk.cpp` "a character's point is the cell showing it, a caret moving none, and
a press on a cell reaches the pane as the column of the character it shows"
`tests/test_workshop_panes_button.cpp` "WL-GEO-01: in a terminal, a right press and its release name
the column of the character each cell shows, a caret moving none"
`tests/test_workshop_panes_editor.cpp` "in a terminal, a press on a character past the caret puts
the caret before that character, and a sweep ends before the character under the hand"

### WL-GEO-07
`workshop/screen.hpp` PixelRect::contains_at PointedAt PointedAt::px
`workshop/screen_arrange.cpp` pane_edge_at
`graphics/pixels.hpp` px_span_contains kPixelGrainPx kCellGrainPx
`tests/test_workshop_screen.cpp` "the hand meets exactly the pixels and cells a pane paints" "the
TUI projects a pane onto its covered cells and rewrites nothing"
`tests/test_graphics.cpp` "one quantization law -- a span lands on device units by flooring both
edges"

### WL-GEO-08
`workshop/screen_bindings.cpp` adopt_screen
`workshop/screen.hpp` Session::cell_px Session::text_advance_px Session::screen_w Session::screen_h
Screen::cell_px Screen::text_advance_px
`graphics/vocabulary.hpp` WindowOffered
`workshop/weave_handlers.cpp` on(WindowOffered)
`tests/test_workshop_screen.cpp` "the canvas's device unit is the medium's answer, never Workshop's"

### WL-GEO-11
`workshop/weave_arrange.cpp` arrange_status
`workshop/layouts/session_persist.hpp` kFormatVersion
`tests/test_workshop_persistence.cpp` "a read-only visit through the other medium writes the SAME
BYTES" "the medium's device unit reaches no durable file"
`tests/test_workshop_screen.cpp` "the medium's unit reaches the READOUT and no geometry at all"

### WL-GEO-14
`workshop/screen.hpp` Screen::strip_left strip_px with_strips strip_rect screen_of canvas_of_room
room_of_canvas
`workshop/screen_chrome.cpp` docked_rect solve_desk add_desk_frames
`workshop/setup_reconcile.hpp` presentation_order effective_pane_order
`tests/test_workshop_screen.cpp` "a dock's strip narrows the room by its width, open or shut, and an
open dock covers what is under it and moves no pane" "in a terminal, a dock's strip is a column or a
row of cells, its arrow a character, its open pane over cells"
