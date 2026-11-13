# Workshop law — pointer

Register `WL-PTR`: two presses as one gesture. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md).

Retired: WL-PTR-02, WL-PTR-03, WL-PTR-04, WL-PTR-05, WL-PTR-06, WL-PTR-08.

## WL-PTR-01 — Two presses are one gesture by the window's count

LAW — A double-click is the window's count, which a reader reads: `doubles_a_tab_click` is pure and total — the press counted 2 or more, the last press armed on the same tab — and no reader times a press.

MEANS
- the window counts each hand's presses by input's `input.double-click-ms`, said in `PointerTimingSaid`; two hands' presses never make one;
- Flow reads the same count (`flow_pane::doubles`); Double Shift is keys, timed by that same said time (WL-KEY-19);
- what Workshop still times -- Shift's taps, the desk reads' rate -- it times by the time the host's one wait hands it (`loom.host.Waited`), and reads no clock.

DOES NOT MEAN
- that the time is a constant of Workshop's: it is input's role-wide setting, 200 to 2000 ms.

WHY — `agents/decisions/the-double-click-time-is-inputs.md`

## WL-PTR-09 — A POSIX terminal cannot report a hover

LAW — On a POSIX terminal the medium asks for button-event tracking (`1002`), so an idle pointer reaches nobody there; it is a documented medium fact, not a defect to repair with `1003`.

MEANS
- the Win32 console reports an idle pointer's moves (`ENABLE_MOUSE_INPUT`), so there a hover reaches whoever hears the pointer.

WHY — `agents/decisions/the-row-is-its-own-scrub-track.md`

## WL-PTR-10 — `on(PointerWheel)` is Workshop's one wheel router

LAW — `on(PointerWheel)` routes every wheel Workshop hears: modes keep their ownership, the topmost occupancy decides by front order; a pane the window shows hears its own, and nothing under another scrolls.

MEANS
- an external pane's area: the window gives it the notches (`AreaWheelTurned`); the Editor scrolls, caret still;
- the picker's and the host Pane Manager's lists scrolled here, and retired with them;
- in a graphics window, the wheel in Workshop's own area (`on(AreaWheelTurned)`); a pane the window shows hears its own (`AreaWheelTurned`).

DOES NOT MEAN
- that there is a scroll framework, a scrollbar, a global offset map or a persisted position.

WHY — `agents/decisions/the-first-multiline-consumer.md`

## Do not assume

- That "… N more" is unreachable — the wheel reaches it wherever a cursor is
  (WL-PTR-10).

## Proven by

### WL-PTR-01
`workshop/screen.hpp` TabClickMemory Session::tab_click
`workshop/screen_arrange.cpp` doubles_a_tab_click
`workshop/weave.hpp` on(Waited)
`workshop/weave_run.cpp` on(Waited)
`graphics/window/clicks.hpp` clicks_of ClickCounter
`flow/flow-pane/gesture.hpp` doubles
`tests/test_workshop_screen.cpp` "what makes two presses on a tab one double-click, and what does
not"

### WL-PTR-09
`graphics/window/terminal_io.hpp` kTuiPointerOn
`workshop/docs/limitations.md` hover
`tests/test_graphics.cpp` "the terminal's ink and its pointer modes are the graphics package's: a colour's word, else the nearest of its sixteen inks, never its default"
`input/terminal_reader.hpp` ENABLE_MOUSE_INPUT win32_mouse_to_events

### WL-PTR-10
`workshop/weave_pointer.cpp` pointer_wheel
`editor/editor-pane/pane.cpp` wheel
`tests/test_workshop_panes_editor.cpp` "the wheel scrolls the body, moves no caret, and elsewhere
reaches nothing"
`tests/test_workshop_panes_input.cpp` "a wheel over an external pane's body reaches it unchanged from
the window, follows the pointer, and the header sends nothing"
`workshop/weave_handlers.cpp` on(AreaWheelTurned)
`pane/facets.hpp` PaneRowsFacet
`tests/test_workshop_panes_files.cpp` "WL-HAND-06: Files names its entries by name and its controls by
operation"
