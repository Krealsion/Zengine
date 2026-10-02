# Workshop law — pane-local canvas

Register `WL-CANVAS`: optional local graphics and pointer custody.
Router: [`../workshop.md`](../workshop.md). The wire contract is in [`../panes.md`](../panes.md).

## WL-CANVAS-01 — A picture occupies only its granted body

LAW — A canvas provider draws in local subunits inside the pane body; Workshop clips before translating, below its title, on the pane's existing plane.

MEANS
- bounded rectangles, fixed cell labels and measured one-line text, below the title;
- shared fit and hit bounds; whole glyph/row clipping keeps the padded region inside;
- provider-owned meaning and hit testing; malformed content is refused whole.

PROVEN BY — `workshop/pane_canvas.hpp` `canvas_content_problem`;
`workshop/screen_canvas.hpp` `canvas_body_place`, `canvas_clip_rect`, `paint_pane_canvas`;
`workshop/pane_canvas_text.hpp` `canvas_text_metrics`, `clip_canvas_text`, `canvas_text_region`;
`workshop/screen_external.cpp` `paint_external`; `tests/test_workshop_panes_canvas.cpp` case
"pane canvas rejects malformed pictures whole and budgets data before rendering", case
"pane canvas clips every primitive at its local boundary before translating", case
"pane canvas grants fenced room and keeps a good picture after a refused update", case
"pane canvas measured text shares its fit with existing surface type and preserves labels", case
"pane canvas text clipping preserves surviving positions through both edges".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-02 — A room is bound to its provider

LAW — A canvas room is granted to the current provider identity, and a new room or re-offer invalidates its predecessor's content admission and held gestures.

MEANS
- a current-role lookup fences admission; new room or re-offer ends predecessor input;
- text metrics renew room; providers do not retain grants or gestures across reload;
- resizing may show a marked, input-free preview only while owner and metrics agree.

PROVEN BY — `workshop/weave.hpp` `HostContext::role_holder`;
`workshop/weave_canvas.cpp` `canvas_owner_current`, `refresh_canvas_rooms`,
`on(PaneCanvasContent)`;
`tests/test_workshop_panes_canvas.cpp` case
"pane canvas grants turn over on reoffer and old content and capture cannot survive", case
"pane canvas provider replacement cannot acquire its predecessor's held gesture", case
"pane canvas text metric changes renew the grant even when its body stays fixed", case
"pane canvas resize preview keeps only the same provider's picture and never its input".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-03 — A canvas gesture ends explicitly

LAW — A canvas gesture retains its press's grant, picture and identity through motion until release or loss, even when the drag repaints the pane.

MEANS
- press and wheel use the existing medium fence to identify their picture;
- a release reaches its held owner outside the pane and through modal surfaces;
- room, owner or mode changes end custody; a secondary press may ask for a menu or be handed back.

PROVEN BY — `workshop/weave_canvas.cpp` `canvas_press`, `canvas_motion`, `canvas_release`,
`canvas_wheel`, `lose_canvas_hold`, `end_canvas_holds`;
`workshop/weave_external.cpp` `end_refused_button`; `tests/test_workshop_panes_canvas.cpp` case
"pane canvas capture keeps the press picture through repaint motion and outside release", case
"a right press a canvas picture hands back opens the host's pane menu at the press; one it keeps
opens nothing", case
"pane canvas resize loses capture and wheel names a local point in the latest picture".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-04 — A carried value lands on a canvas as a local place

LAW — A value dropped on a canvas reaches a provider accepting `PaneCanvasValueDrop` as local subunits in its granted room and aimed picture; another provider is sent nothing and the value stays held.

MEANS
- the provider hit-tests the place in the picture it drew and owns what the drop means;
- a drop begins no canvas custody; v2's attribution rides with the copy;
- a drag's drop names the picture and place its release met, or is refused in words.

PROVEN BY — `workshop/pane_carry.hpp` `PaneCanvasValueDrop`; `workshop/weave_operation.cpp`
`drop_on_canvas`, `drop_carry`; `workshop/weave.hpp` `CanvasRelease`;
`tests/test_workshop_inventory_info.cpp` cases "a value dragged from Inventory onto Flow's canvas
reaches Flow as a canvas drop where it was released, and Flow offers what it can become" and "a
value released on Flow's canvas before its carry is answered names the picture it was released
on, though Flow repainted meanwhile".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-05 — A canvas press may carry a value out

LAW — A primary canvas press is its provider's to continue as a prose press is: under its number the provider may carry a value out, and once the carry begins the press's hold ends as lost.

MEANS
- the press's number approves one acquisition, and the press begins a value drag;
- motion and the release after the carry begins are the carry's, never the canvas's;
- a release before the carry is retained, as for a prose drag; an unmoved click carries nothing.

PROVEN BY — `workshop/weave_canvas.cpp` `canvas_press`; `workshop/weave_pointer.cpp`
`on(PointerButton)`; `workshop/weave_operation.cpp` `accept_carry`, `begin_value_drag`;
`flow-pane/pane.cpp` `carry`; `tests/test_workshop_inventory_info.cpp` case "a press on a canvas
pane drags a value out as a prose press does: the hold ends as lost when the carry begins, the
value lands where the hand lets go, and a click carries nothing"; `tests/test_flow_pane.cpp` case
"a press on a declared message asks under that press to drag its shape out, and still opens it; a
right press on a found operator is handed back".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-06 — A canvas hears where the pointer rests

LAW — A provider that accepts `PaneCanvasHover` is told where a pointer holding no button rests on its canvas, and once when it leaves; resting moves no selection, focus, key or gesture.

MEANS
- the canvas on top under the pointer, from geometry Workshop holds, once per local place;
- a leave when the pointer moves off its body, a mode, menu or sweep takes it, or a press holds;
- a carried value over a canvas is told with `carrying`; a fresh room puts the hover down unsaid.

PROVEN BY — `workshop/pane_canvas_vocabulary.hpp` `PaneCanvasHover`; `workshop/weave_canvas.cpp`
`canvas_hover`, `leave_canvas_hover`, `refresh_canvas_rooms`; `workshop/weave_pointer.cpp`
`on(PointerMoved)`; `tests/test_workshop_panes_canvas.cpp` case "a canvas that accepts the hover
door hears where the pointer rests and that it left; resting selects, focuses and presses
nothing", case "a described view offers its own pane through the view host, Workshop seats and
draws it, a press reaches it, and a stop leaves a picture that says so".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## Do not assume

- That a picture fence proves physical presentation time; it orders Loom deliveries.
- That a reloaded provider may retain grants: it must wait for its fresh room before input.
- That a pointer gesture means any particular edit; only the provider knows its content.
- That a window reports the pointer leaving it: a hover over a canvas the pointer left through
  the window's edge stays until the next motion inside the window.
