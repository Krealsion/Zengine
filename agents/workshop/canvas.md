# Workshop law — pane-local canvas

Register `WL-CANVAS`: a pane's own picture in its window area, and the pointer there.
Router: [`../workshop.md`](../workshop.md). The wire contract is in [`../panes.md`](../panes.md).

Retired: WL-CANVAS-01, WL-CANVAS-03, WL-CANVAS-04, WL-CANVAS-06, WL-CANVAS-07

## WL-CANVAS-02 — A room is the window's, given to the area's holder

LAW — A pane's room is what the window that shows its area gives that area's owner role (`AreaRoom`); a holder arriving with nothing asks it (`RoomAsked`), and keeps no room across a reload.

MEANS
- a new room or a new window starts a drawing afresh: what the pane drew for another room is not shown in it;
- the room says the window's text measure, so a face that is not a cell holds its own count of characters.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-05 — A canvas press may carry a value out

LAW — A primary press in a pane's area is its holder's to continue: under the window's number for it the holder may carry a value out, and once the carry begins the press's hold ends.

MEANS
- the press's number approves one acquisition, the press begins a value drag, and the desk stands in front while it is carried;
- motion and the release after the carry begins are the carry's, in Workshop's own area, never the pane's;
- an unmoved click carries nothing.

WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## Do not assume

- That a window's number for a press proves physical presentation time; it orders Loom deliveries.
- That a reloaded holder keeps its room: it asks the window for it.
- That a pointer gesture means any particular edit; only the pane knows its content.
- That a window reports the pointer leaving it: a hover the pointer left through the window's
  edge stays until the next motion inside the window.

## Proven by

### WL-CANVAS-02
`graphics/vocabulary.hpp` AreaRoom RoomAsked
`pane/seat.hpp` PaneCanvasSeat
`tests/test_workshop_panes_canvas.cpp` "a view asks for its size in pixels and its pane grants
exactly that room: to the pixel in a window, to the cells that hold it in a terminal"
`tests/test_workshop_desk.cpp` "a pane's names survive the canvas: its next image draws a picture
under the names its rows had, and each is pressed where the picture draws it"

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
