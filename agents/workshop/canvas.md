# Workshop law — pane-local canvas

Register `WL-CANVAS`: a pane's own picture in its window area, and the pointer there.
Router: [`../workshop.md`](../workshop.md). The wire contract is in [`../panes.md`](../panes.md).

## WL-CANVAS-01 — A picture occupies only its area

LAW — A pane draws its picture in its own window area, in that area's pixels and the room the window gives it; the window cuts it at the area's edge, and a drawing past its bounds is the window's to refuse.

MEANS
- bounded rectangles, labels and measured text, and the parts a press names, as `canvas_sketch` draws a `CanvasPicture` for a `CanvasRoom`;
- a picture past what a window keeps is drawn as its ground and that sentence, never sent to be refused;
- a drawing the window refuses (`DrawnRefused`) is told its drawer alone, which says it in its own picture.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-02 — A room is the window's, given to the area's holder

LAW — A pane's room is what the window that shows its area gives that area's owner office (`AreaRoom`); a holder arriving with nothing asks it (`RoomAsked`), and keeps no room across a reload.

MEANS
- a new room or a new window starts a drawing afresh: what the pane drew for another room is not shown in it;
- the room says the window's text measure, so a face that is not a cell holds its own count of characters.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-03 — A canvas gesture ends explicitly

LAW — A press in a pane's area is held there: its moves and its release reach the area under the window's number for the press until the release or the window's word that the hold ended (`AreaHoldEnded`).

MEANS
- a release reaches its held area outside it; an area withdrawn while it is held hears no release, one handed to another owner is released to that owner;
- an area placed to hear the pointer in front of where the press was made ends the hold, and the release goes where it lands;
- a secondary press may ask for a menu or be handed back, under the window's number.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-04 — A carried item lands at a point of a pane's area

LAW — An item dropped on a pane reaches a holder accepting its door (`PaneCanvasValueDrop`, `PaneCanvasDrop`) as the area it landed in and a point of it, in its pixels; another is sent nothing.

MEANS
- the pane owns what the drop means, judging its point by the window's word of what it drew there (WL-PRESS-07);
- a drop begins no hold; a value keeps its source's attribution; one refused is said not delivered;
- unsent, a clicked item stays held and a dragged one is let go.

DOES NOT MEAN
- that a drop with no word from the window is refused: it is read against the pane's newest drawing.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-05 — A canvas press may carry a value out

LAW — A primary press in a pane's area is its holder's to continue: under the window's number for it the holder may carry a value out, and once the carry begins the press's hold ends.

MEANS
- the press's number approves one acquisition, the press begins a value drag, and the desk stands in front while it is carried;
- motion and the release after the carry begins are the carry's, in Workshop's own area, never the pane's;
- an unmoved click carries nothing.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-06 — A pane hears where the pointer rests, and where a carried item would land

LAW — A pane hears from the window where a pointer holding no button rests in its area; while an item is carried over it, a holder accepting `PaneCanvasHover` is told where it would land.

MEANS
- the window's words are `AreaPointerMoved` and `AreaPointerLeft`; resting moves no selection, focus, key or gesture;
- a hover is put down when the hand leaves the area, the carry ends or a press begins;
- a pane facet hands a pane that hears rests (`kHearsRest`) the cell or part under one once each change and the leave once; a drag is no rest, and a pane not hearing rests accepts no leave.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-07 — A pane's rows lie on the medium's own ground

LAW — A pane drawing its rows in its area lays them on the medium's own ground, as a prose body was.

MEANS
- the ground is a window's own background and a terminal's default, dark or light: no colour of the palette;
- text drawn in the ground is no ink: it paints as plain text, in a terminal its own text colour.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## Do not assume

- That a window's number for a press proves physical presentation time; it orders Loom deliveries.
- That a reloaded holder keeps its room: it asks the window for it.
- That a pointer gesture means any particular edit; only the pane knows its content.
- That a window reports the pointer leaving it: a hover the pointer left through the window's
  edge stays until the next motion inside the window.

## Proven by

### WL-CANVAS-01
`workshop/pane_canvas_drawing.hpp` canvas_sketch canvas_window_room CanvasSketch
`workshop/pane_picture.hpp` CanvasPicture CanvasRoom
`workshop/pane_drawing.hpp` WindowDrawing
`graphics/vocabulary.hpp` DrawnRefused
`tests/test_workshop_panes_canvas.cpp` "pane canvas text clipping preserves surviving positions
through both edges" "a described view of the greatest size its rules allow asks for a pane
Workshop admits, and is seated and drawn cut to it"
`tests/test_workshop_desk.cpp` "a picture of as many parts as it may name gives each a point a
press there gives it, or none"

### WL-CANVAS-02
`graphics/vocabulary.hpp` AreaRoom RoomAsked
`workshop/pane_seat.hpp` PaneCanvasSeat
`tests/test_workshop_panes_canvas.cpp` "a view asks for its size in pixels and its pane grants
exactly that room: to the pixel in a window, to the cells that hold it in a terminal"
`tests/test_workshop_desk.cpp` "a pane's names survive the canvas: its next image draws a picture
under the names its rows had, and each is pressed where the picture draws it"

### WL-CANVAS-03
`graphics/window/window.hpp` end_hold end_holds end_covered
`workshop/weave_handlers.cpp` on(AreaHoldEnded)
`graphics/vocabulary.hpp` AreaHoldEnded
`workshop/pane_drawing.hpp` WindowPointer
`editor/editor-pane/transfers.hpp` on_window
`tests/test_graphics.cpp` "a press held for an area ends with no release when the area is
withdrawn, handed to another owner, or covered where it was made by an area placed to hear the
pointer in front of it, and the window tells the area's owner, and its arranger when another office,
that it ended -- and no one else"
`tests/test_workshop_panes_editor.cpp` "in a graphics window that gives the Editor its pointer, a
press there, its moves under its number and its release sweep a selection as on Workshop's canvas,
and a hold the window ends is the press lost"
`tests/test_flow_pane.cpp` "WL-HAND-06: Flow names its controls, its nodes and ports by their place,
and every place a press means something"

### WL-CANVAS-04
`workshop/pane_carry.hpp` PaneCanvasValueDrop PaneCanvasDrop
`workshop/weave_operation.cpp` drop_carry pane_area_at
`tests/test_workshop_inventory_info.cpp` "a value dragged from Inventory onto Flow's canvas reaches
Flow as a canvas drop where it was released, and Flow offers what it can become" "a live reference
carried from Inventory lands in the Compose field its place names on Compose's canvas" "a drop
whose pane leaves before it is delivered is said not delivered: a reference or a value, on a
canvas"
`tests/test_workshop_editor_transfers.cpp` "a drop aimed at a picture the text has since left is
refused and changes nothing"

### WL-CANVAS-05
`workshop/weave_handlers.cpp` on(AreaPressed) area_pressed
`workshop/weave_operation.cpp` accept_carry begin_value_drag release_value_drag
`workshop/weave_run.cpp` desk_in_front_of_panes
`flow/flow-pane/pane.cpp` carry
`tests/test_workshop_inventory_info.cpp` "a press on a canvas pane drags a value out as a prose
press does: the hold ends as lost when the carry begins, the value lands where the hand lets go, and
a click carries nothing"
`tests/test_flow_pane.cpp` "a press on a declared message asks under that press to drag its shape
out, and still opens it; a right press on a found operator is handed back"
`tests/test_workshop_desk.cpp` "a value the weaver drags out of a pane is carried under the hand in
Workshop's overlay in the window, the desk in front of the panes' areas while it is carried, and
goes when the hand lets go"

### WL-CANVAS-06
`graphics/vocabulary.hpp` AreaPointerMoved AreaPointerLeft
`workshop/pane_carry.hpp` PaneCanvasHover
`workshop/weave_operation.cpp` carry_hover leave_carry_hover
`workshop/weave_handlers.cpp` on(AreaPointerLeft)
`view/view-builder/pane.cpp` rest
`tests/test_view_builder.cpp` "the pointer resting on an element marks it and its row; a carried
value marks the label it would land on; leaving puts the marks down"
`workshop/pane_drawing.hpp` kHearsRest
`workshop/pane_facets.hpp` PaneRowsFacet PaneDrawingFacet
`workshop/pane_seat.hpp` MultiPaneFacet PaneCanvasSeat
`tests/test_graphics.cpp` "a pane on the drawing facet is handed the part the pointer rests on, once
each time that part changes, and its leave once a rest; a move a held press carries is no rest"
`tests/test_workshop_panes_files.cpp` "in a window that reports a resting pointer, Files' shut places
peek over the rows under their header and take no room, the pointer on what the peek shows keeps it,
a press there opens them, a move a held press carries is no rest, and the leave ends the peek"

### WL-CANVAS-07
`graphics/role.hpp` kMediumGround
`workshop/pane_drawing.hpp` WindowDrawing
`tests/test_workshop_inventory_info.cpp` "Inventory draws its rows on its canvas on the medium's own
ground, and a name line's caret and selection stand in the line, writing nothing into it, each move
of them drawn" "the rows two canvas panes show are each read off the pane's own plane where their
bodies share a corner, the one laying the medium's ground and the one laying black"
