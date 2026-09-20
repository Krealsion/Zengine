# Workshop law — pane-local canvas

Register `WL-CANVAS`: optional local graphics and pointer custody.
Router: [`../workshop.md`](../workshop.md). The wire contract is in [`../panes.md`](../panes.md).

## WL-CANVAS-01 — A picture occupies only its granted body

LAW — A canvas provider draws in local subunits inside the pane body; Workshop clips before translating, below its title, on the pane's existing plane.

MEANS
- bounded rectangles and fixed-size labels, no whole-screen publication;
- pan, zoom, hit testing and drawing meaning belong to the provider;
- invalid content is refused whole and the last good picture remains.

PROVEN BY — `workshop/pane_canvas.hpp` `canvas_content_problem`;
`workshop/screen_canvas.hpp` `canvas_body_place`, `canvas_clip_rect`, `paint_pane_canvas`;
`workshop/screen_external.cpp` `paint_external`; `tests/test_workshop_panes_canvas.cpp` case
"pane canvas rejects malformed pictures whole and budgets data before rendering", case
"pane canvas clips every primitive at its local boundary before translating", case
"pane canvas grants fenced room and keeps a good picture after a refused update".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-02 — A room is bound to its provider

LAW — A canvas room is granted to the current provider identity, and a new room or re-offer invalidates its predecessor's content and held gestures.

MEANS
- the current office holder is read from the host's Loom callback at each admission;
- direct sends and echoed grants prevent a successor inheriting old gestures;
- a provider keeps grants and gesture state outside its reload-kept state.

PROVEN BY — `workshop/weave.hpp` `HostContext::role_holder`;
`workshop/weave_canvas.cpp` `canvas_owner_current`, `refresh_canvas_rooms`,
`on(PaneCanvasContent)`;
`tests/test_workshop_panes_canvas.cpp` case
"pane canvas grants turn over on reoffer and old content and capture cannot survive", case
"pane canvas provider replacement cannot acquire its predecessor's held gesture".
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
