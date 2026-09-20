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
- room, owner or mode changes end custody; secondary presses may ask the existing menu presenter.

PROVEN BY — `workshop/weave_canvas.cpp` `canvas_press`, `canvas_motion`, `canvas_release`,
`canvas_wheel`, `lose_canvas_hold`, `end_canvas_holds`;
`workshop/weave_external.cpp` `end_refused_button`; `tests/test_workshop_panes_canvas.cpp` case
"pane canvas capture keeps the press picture through repaint motion and outside release", case
"pane canvas resize loses capture and wheel names a local point in the latest picture".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## Do not assume

- That a picture fence proves physical presentation time; it orders Loom deliveries.
- That a reloaded provider may retain grants: it must wait for its fresh room before input.
- That a pointer gesture means any particular edit; only the provider knows its content.
