# Workshop law — pointer

Register `WL-PTR`: two presses as one gesture. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md).

Retired: WL-PTR-02, WL-PTR-03, WL-PTR-04, WL-PTR-05, WL-PTR-06, WL-PTR-08.

## WL-PTR-01 — Two presses are one gesture, and time is an argument

LAW — A double-click is Workshop's own interpretation: `doubles_a_tab_click` is pure and total — armed, same tab, within `kDoubleClickMs` — and time is its argument.

MEANS
- `input::PointerButton` carries no click count or timestamp on either backend;
- `HostContext::interaction_now` is the one clock reading: steady, never persisted, never wired;
- the interval is a product constant, not a preference: one gesture means one thing everywhere.

PROVEN BY — `workshop/interaction_time.hpp` `interaction_now_ms`; `workshop/screen.hpp`
`kDoubleClickMs`, `TabClickMemory`, `Session::tab_click`; `workshop/screen_arrange.cpp`
`doubles_a_tab_click`; `workshop/weave.hpp` `HostContext::interaction_now`;
`tests/test_workshop_screen.cpp` case `"what makes two presses on a tab one double-click, and
what does not"`.
WHY — `agents/decisions/time-is-an-argument.md`

## WL-PTR-09 — The terminal cannot report a hover

LAW — The terminal medium asks for button-event tracking (`1002`), so an idle pointer reaches nobody there; it is a documented medium fact, not a defect to repair with `1003`.

PROVEN BY — `surface/skin_tui.hpp` `kTuiPointerOn`; `workshop/docs/limitations.md` `hover`;
`tests/test_surface.cpp` case `"the Skin's terminal claim includes pointer reporting, and leave
undoes enter"`.
WHY — `agents/decisions/the-row-is-its-own-scrub-track.md`

## WL-PTR-10 — `on(PointerWheel)` is Workshop's one wheel router

LAW — `on(PointerWheel)` routes every wheel: modes keep their ownership, the topmost occupancy decides by front order, and a pane in front is sent the notches; nothing under another pane scrolls.

MEANS
- an external pane's body: the notches cross as `PaneWheel` to a text pane, as a canvas wheel to a canvas pane; the Editor scrolls, caret still;
- the picker's and the host Pane Manager's lists scrolled here, and retired with them.

DOES NOT MEAN
- that there is a scroll framework, a scrollbar, a global offset map or a persisted position.

PROVEN BY — `workshop/weave_pointer.cpp` `on(PointerWheel)`; `workshop/weave_external.cpp`
`external_wheel`; `workshop/weave_canvas.cpp` `canvas_wheel`; `editor/editor-pane/pane.cpp`
`wheel`; `tests/test_workshop_panes_editor.cpp` case `"the wheel scrolls the body, moves
no caret, and elsewhere reaches nothing"`; `tests/test_workshop_panes_input.cpp` case `"a wheel
over an external pane's body crosses unchanged, follows the pointer, and the header sends
nothing"`; `tests/test_workshop_panes_canvas.cpp` case `"pane canvas resize loses capture and
wheel names a local point in the latest picture"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## Do not assume

- That "… N more" is unreachable — the wheel reaches it wherever a cursor is
  (WL-PTR-10).
