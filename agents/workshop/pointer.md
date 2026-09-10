# Workshop law — pointer

Register `WL-PTR`: two presses as one gesture. One law per heading; cite by ID.
Router: [`../workshop.md`](../workshop.md).

**Reading past a fitted row was here and is retired** — see the note below WL-PTR-03.

## WL-PTR-01 — Two presses are one gesture, and time is an argument

LAW — A double-click is Workshop's own interpretation: `doubles_a_click` is pure and total — armed, same line, same draft, same word, within `kDoubleClickMs` — and time is its argument.

MEANS
- `input::PointerButton` carries no click count or timestamp on either backend;
- `HostContext::interaction_now` is the one clock reading: steady, never persisted, never wired;
- the interval is a product constant, not a preference: one gesture means one thing everywhere.

PROVEN BY — `workshop/interaction_time.hpp` `interaction_now_ms`; `workshop/screen.hpp`
`kDoubleClickMs`, `ClickMemory`, `Session::click`; `workshop/screen_arrange.cpp`
`doubles_a_click`; `workshop/weave.hpp` `HostContext::interaction_now`;
`tests/test_workshop_screen.cpp` case `"WUX-7: what makes two presses one double-click, and what
does not"`.
WHY — `agents/decisions/time-is-an-argument.md`

## WL-PTR-02 — One seam, both editable lines

LAW — One word-selecting press serves every editable line this host still holds, so a double-click selects the word under it in the Pane Manager's draft.

MEANS
- it served Info's property draft too, until that draft became a loaded image's own.

DOES NOT MEAN
- that the Editor's multiline machinery or the Composer's fields were taught it — neither was;
- that a PANE gets it: a pane sweeps from the positions it is sent (WL-TEXT-14), never a word.

**The live witness went with the Terminal's line, and is not replaced.** It was driven over
the terminal overlay's own row; the Pane Manager's draft spends the same call and has no
live case, so what is pinned now is the PURE qualification and not the spend. Named rather
than counted away.

PROVEN BY — `workshop/weave_seam.cpp` `press_selects_word`;
`workshop/weave_pane_editor.cpp` `pane_editor_press`; `tests/test_workshop_screen.cpp` case
`"WUX-7: what makes two presses one double-click, and what does not"`.
WHY — `agents/decisions/time-is-an-argument.md`

## WL-PTR-03 — The record arms on the way out, and the completing press spends it

LAW — The click that first lands in a word is an ordinary press with an arming beside it; the completing press spends the arming, so there is no triple-click.

MEANS
- a modifier-bearing press neither doubles nor arms, and its ordinary behaviour is untouched.

**The three live subcases went with the Terminal's line (WL-PTR-02).** The spend is still
written where it happens, and the qualification it consults is pinned pure and total; what
no case drives any more is a third press, a modifier-bearing press and a spent arming
against a real box.

PROVEN BY — `workshop/screen.hpp` `ClickMemory`; `workshop/screen_arrange.cpp`
`doubles_a_click`; `workshop/weave_seam.cpp` `press_selects_word`;
`tests/test_workshop_screen.cpp` subcase `"nothing armed is nothing to double"`, subcase
`"a different DRAFT of the same line is a different box"`.
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

## Do not assume
