# Workshop law — pointer

Register `WL-PTR`: two presses as one gesture. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md).

**Reading past a fitted row was here and is retired** — see the note below WL-PTR-03.

## WL-PTR-01 — Two presses are one gesture, and time is an argument

LAW — A double-click is Workshop's own interpretation: `doubles_a_tab_click` is pure and total — armed, same tab, within `kDoubleClickMs` — and time is its argument.

MEANS
- `input::PointerButton` carries no click count or timestamp on either backend;
- `HostContext::interaction_now` is the one clock reading: steady, never persisted, never wired;
- the interval is a product constant, not a preference: one gesture means one thing everywhere.

PROVEN BY — `workshop/interaction_time.hpp` `interaction_now_ms`; `workshop/screen.hpp`
`kDoubleClickMs`, `TabClickMemory`, `Session::tab_click`; `workshop/screen_arrange.cpp`
`doubles_a_tab_click`; `workshop/weave.hpp` `HostContext::interaction_now`;
`tests/test_workshop_screen.cpp` case `"WUX-7: what makes two presses on a tab one double-click,
and what does not"`.
WHY — `agents/decisions/time-is-an-argument.md`

## WL-PTR-02 — RETIRED: one word-selecting press served every editable line the host held

WHY — `agents/decisions/time-is-an-argument.md`

## WL-PTR-03 — RETIRED: the record armed on the way out, and the completing press spent it

WHY — `agents/decisions/time-is-an-argument.md`

**Retired — WL-PTR-04, WL-PTR-05, WL-PTR-06, WL-PTR-08: reading past a fitted row.**
The feature these four laws were about is gone, and it is a loss rather than a move. A
pointer resting on a truncated OBJECTS or PROPERTIES row scrolled that row under the hand. It
needed the row's UNFITTED text and the item's identity, and both of those are
`Zengine/info-pane/`'s now — a pane sends rows it has already cut, so nothing on this side has
the string to read past. The pane protocol has no hover, and adding one so this host could keep
one feature is exactly the host-mapped route VD-22 refuses.

Retired with it: `Session::reveal`, `Revealed`, `RevealAt`, `reveal_place`, `reveal_at`,
`reveal_for`, `reveal_offset_at_column`, `reveal_max_offset`, `revealed_row` and
`detail::reveal_shown`. `agents/decisions/the-row-is-its-own-scrub-track.md` records the
decision and now records its reversal; WL-PTR-09 below outlives it, because "the terminal
cannot report a hover" is a fact about a medium and not about this feature.


## WL-PTR-09 — The terminal cannot report a hover

LAW — The terminal medium asks for button-event tracking (`1002`), so an idle pointer reaches nobody there; it is a documented medium fact, not a defect to repair with `1003`.

PROVEN BY — `surface/skin_tui.hpp` `kTuiPointerOn`; `docs/workshop/limitations.md` `hover`;
`tests/test_surface.cpp` case `"the Skin's terminal claim includes pointer reporting, and leave
undoes enter"`.
WHY — `agents/decisions/the-row-is-its-own-scrub-track.md`

## WL-PTR-10 — `on(PointerWheel)` is Workshop's one wheel router

LAW — `on(PointerWheel)` routes every wheel: modes keep their ownership, the topmost occupancy decides by front order, and a pane in front is sent the notches; nothing under another pane scrolls.

MEANS
- an external pane's body: the notches cross as `PaneWheel`; the Editor scrolls, caret still;
- the picker's and the host Pane Manager's lists scrolled here, and retired with them.

DOES NOT MEAN
- that there is a scroll framework, a scrollbar, a global offset map or a persisted position.

PROVEN BY — `workshop/weave_pointer.cpp` `on(PointerWheel)`; `workshop/weave_external.cpp`
`external_wheel`; `editor-pane/pane.cpp` `on(PaneWheel)`; `tests/test_workshop_panes_editor.cpp`
case `"EDIT-W39: the wheel scrolls the body, moves no caret, and elsewhere reaches nothing"`.
WHY — `agents/decisions/the-first-multiline-consumer.md`

## Do not assume

- That "… N more" is unreachable — the wheel reaches it wherever a cursor is
  (WL-PTR-10).
