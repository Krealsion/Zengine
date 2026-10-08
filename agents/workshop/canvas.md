# Workshop law — pane-local canvas

Register `WL-CANVAS`: optional local graphics and pointer custody.
Router: [`../workshop.md`](../workshop.md). The wire contract is in [`../panes.md`](../panes.md).

## WL-CANVAS-01 — A picture occupies only its granted body

LAW — A canvas provider draws in local canvas pixels inside the pane body; Workshop clips before translating, below its title, on the pane's existing plane.

MEANS
- bounded rectangles, fixed cell labels and measured one-line text, below the title;
- shared fit and hit bounds; whole glyph/row clipping keeps every glyph and caret inside, and a run's padding unless the run stands on the lattice;
- provider-owned meaning and hit testing; malformed content is refused whole, and said as WL-ATTN-04 says.

PROVEN BY — `workshop/pane_canvas.hpp` `canvas_content_problem`;
`workshop/screen_canvas.hpp` `canvas_body_place`, `canvas_clip_rect`, `paint_pane_canvas`;
`workshop/pane_canvas_text.hpp` `canvas_text_metrics`, `clip_canvas_text`, `clip_canvas_run`,
`canvas_text_region`;
`workshop/screen_external.cpp` `paint_external`; `tests/test_workshop_panes_canvas.cpp` case
"pane canvas rejects malformed pictures whole and budgets data before rendering", case
"pane canvas clips every primitive at its local boundary before translating", case
"pane canvas grants fenced room and keeps a good picture after a refused update", case
"a canvas picture refused for what it holds is said in its pane and in Attention until a picture of
the pane is admitted, and one that came late is answered to its pane alone", case
"pane canvas measured text shares its fit with existing surface type and preserves labels", case
"pane canvas text clipping preserves surviving positions through both edges", case
"an unpadded run stands its first character at its own place, and runs a line apart hold as many
rows as a prose body".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-02 — A room is bound to its provider

LAW — A canvas room is granted to the current provider identity, and a new room or re-offer invalidates its predecessor's content admission and held gestures.

MEANS
- a current-role lookup fences admission; new room or re-offer ends predecessor input;
- text metrics renew room; providers do not retain grants or gestures across reload;
- resizing may show a marked, input-free preview only while owner and metrics agree.

DOES NOT MEAN
- that a holder of only the earlier doors is refused: it is answered, and judged, in its units.

PROVEN BY — `workshop/weave.hpp` `HostContext::role_holder`;
`workshop/weave_canvas.cpp` `canvas_owner_current`, `refresh_canvas_rooms`,
`on(PaneCanvasContent)`, `on(v2::PaneCanvasContent)`, `on(v4::PaneCanvasContent)`;
`workshop/pane_canvas_vocabulary.hpp`
`kPaneCanvasLegacySubs`, `canvas_content_of_legacy`, `canvas_content_as_said`;
`tests/test_workshop_panes_canvas.cpp` case
"pane canvas grants turn over on reoffer and old content and capture cannot survive", case
"pane canvas provider replacement cannot acquire its predecessor's held gesture", case
"pane canvas text metric changes renew the grant even when its body stays fixed", case
"pane canvas resize preview keeps only the same provider's picture and never its input", case
"a canvas provider that speaks only the earlier doors is answered in them", case
"an older canvas picture is judged by its own rules, and a rect that floors to nothing is
dropped, not the picture".
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

## WL-CANVAS-04 — A carried item lands on a canvas as a local place

LAW — An item dropped on a canvas reaches a provider accepting its door (`PaneCanvasValueDrop`, `PaneCanvasDrop`) as local pixels in its room and aimed picture; another is sent nothing.

MEANS
- the provider hit-tests the place in the picture it drew and owns what the drop means;
- a drop begins no canvas custody; a value keeps v2's attribution, a reference crosses as `PaneDrop` does; one refused is said not delivered;
- a drag's drop names the picture and place its release met, or is refused in words; unsent, a clicked item stays held, a dragged one is let go.

PROVEN BY — `workshop/pane_carry.hpp` `PaneCanvasValueDrop`, `PaneCanvasDrop`;
`workshop/weave_operation.cpp` `drop_on_canvas`, `drop_carry`; `workshop/weave.hpp` `CanvasRelease`;
`tests/test_workshop_inventory_info.cpp` cases "a value dragged from Inventory onto Flow's canvas
reaches Flow as a canvas drop where it was released, and Flow offers what it can become", "a
value released on Flow's canvas before its carry is answered names the picture it was released
on, though Flow repainted meanwhile", "a live reference carried from Inventory lands in the
Compose field its place names on Compose's canvas", "a reference placed on a canvas whose pane
has no door for one is sent nothing and stays held" and "a drop whose pane leaves before it is
delivered is said not delivered: a reference or a value, on a canvas";
`tests/test_workshop_editor_transfers.cpp` case "a drop whose pane leaves before it is delivered
is said not delivered: a value on a text pane's rows".
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
- a leave when the pointer moves off its body, a mode, menu or sweep takes it, or a press begins;
- a carried value over a canvas is told with `carrying`; a fresh room puts the hover down unsaid.

PROVEN BY — `workshop/pane_canvas_vocabulary.hpp` `PaneCanvasHover`; `workshop/weave_canvas.cpp`
`canvas_hover`, `leave_canvas_hover`, `refresh_canvas_rooms`; `workshop/weave_pointer.cpp`
`on(PointerMoved)`, `on(PointerButton)`; `tests/test_workshop_panes_canvas.cpp` case "a canvas
that accepts the hover door hears where the pointer rests and that it left; resting selects,
focuses and presses nothing", case "a described view offers its own pane through the view host,
Workshop seats and draws it, a press reaches it, and a stop leaves a picture that says so".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## WL-CANVAS-07 — A pane's rows on its canvas lie on the medium's own ground

LAW — A pane drawing its rows on its canvas lays them on the medium's own ground, as its prose body was; to a host granting no canvas it says them as prose and takes no press, wheel or drop there.

MEANS
- the ground is a window's own background and a terminal's default, dark or light: no colour of the palette;
- what a pane says as prose is what it draws, its caret beside the rows; only its keys act there;
- text drawn in the ground is no ink: it paints as plain text, in a terminal its own text colour.

DOES NOT MEAN — that Workshop grants such a pane no canvas: it always grants one, and the
prose is for a host that cannot.

PROVEN BY — `surface/vocabulary.hpp` `kMediumGround`; `workshop/pane_canvas_rows.hpp`
`rows_picture`; `workshop/pane_canvas.hpp` `canvas_content_problem`; `surface/skin_tui.hpp`
`canvas_body`, `sgr_for_role`, `sgr_for_cell`; `surface/skin_sdl_plan.hpp` `ink_for_role`,
`text_ink_for_role`; `tests/test_surface.cpp` cases "canvas: the medium's own ground covers
material and wears the terminal's own ground", "canvas: a terminal's plain text is the terminal's
own text colour on its own ground and the palette's white on a ground a pane paints, so it reads
on a light terminal or a dark one, a caret in it and a selection over it too" and "canvas plan:
the medium's own ground is the window's own background, and no ink";
`tests/test_workshop_panes_attention.cpp` case "a pane that draws its rows on its
canvas wears the medium's own ground beneath them, as a prose body does, in a window and in a
terminal"; `tests/test_workshop_panes_files.cpp` case "a host that grants Files no canvas is shown
its rows as prose, its presses reach nothing there, and Return still opens";
`tests/test_workshop_panes_input.cpp` case "a host that grants Compose no canvas is shown its rows
and caret as prose, its presses reach nothing there, and its keys still compose";
`tests/test_workshop_panes_info.cpp` case "a host that grants Info no canvas is shown its lists and
a draft's caret as prose, its presses reach nothing there, and its keys still inspect and edit";
`tests/test_workshop_inventory_info.cpp` case "a host that grants Inventory no canvas is shown its
rows and a name line's caret as prose, its presses reach nothing there, and its keys still act";
`tests/test_workshop_panes_editor.cpp` case "a host that grants the Editor no canvas is shown its
rows and caret as prose, its presses reach nothing there, and its keys still edit";
`tests/test_workshop_neovim.cpp` case "a host that grants the Neovim editor no canvas is shown its
rows and caret as prose, its presses reach nothing there, and its keys still reach Neovim".
WHY — `agents/decisions/a-canvas-is-a-pane-picture.md`

## Do not assume

- That a picture fence proves physical presentation time; it orders Loom deliveries.
- That a reloaded provider may retain grants: it must wait for its fresh room before input.
- That a pointer gesture means any particular edit; only the provider knows its content.
- That a window reports the pointer leaving it: a hover over a canvas the pointer left through
  the window's edge stays until the next motion inside the window.
