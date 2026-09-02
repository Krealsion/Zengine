# Workshop law — pointer

Register `WL-PTR`: two presses as one gesture, and reading past a fitted row under the pointer.
One law per heading; cite by ID. Router: [`../workshop.md`](../workshop.md).

## WL-PTR-01 — Two presses are one gesture, and time is an argument

LAW — A double-click is Workshop's own interpretation: `doubles_a_click` is pure and total — armed, same line, same draft, same word, within `kDoubleClickMs` — and time is its argument.

MEANS
- `input::PointerButton` carries no click count or timestamp on either backend;
- `HostContext::interaction_now` is the one clock reading: steady, never persisted, never wired;
- the interval is a product constant, not a preference: one gesture means one thing everywhere.

PROVEN BY — `workshop/interaction_time.hpp` `interaction_now_ms`; `workshop/screen.hpp`
`kDoubleClickMs`, `doubles_a_click`, `ClickMemory`, `click`; `workshop/weave.hpp`
`interaction_now`; `tests/test_workshop_screen.cpp` case `"WUX-7: what makes two presses one
double-click, and what does not"`.
WHY — `agents/decisions/time-is-an-argument.md`

## WL-PTR-02 — One seam, both editable lines

LAW — One word-selecting press serves both editable lines, so a double-click selects the word under it on the Terminal's line and in a property draft alike.

DOES NOT MEAN
- that the Editor's multiline machinery or the Composer's fields were taught it — neither was.

PROVEN BY — `workshop/weave.hpp` `press_selects_word`, `terminal_press`, `info_press`;
`tests/test_workshop_screen.cpp` case `"WUX-7: a double-click on the Terminal's line selects the
word under it"`; `tests/test_workshop_panels.cpp` case `"WUX-7: a double-click in a property
draft selects the word under it"`.
WHY — `agents/decisions/time-is-an-argument.md`

## WL-PTR-03 — The record arms on the way out, and the completing press spends it

LAW — The click that first lands in a word is an ordinary press with an arming beside it; the completing press spends the arming, so there is no triple-click.

MEANS
- a modifier-bearing press neither doubles nor arms, and its ordinary behaviour is untouched.

PROVEN BY — `workshop/screen.hpp` `ClickMemory`, `doubles_a_click`; `workshop/weave.hpp`
`press_selects_word`; `tests/test_workshop_screen.cpp` subcase `"a third press is an ordinary
press again -- there is no triple-click"`, subcase `"a modifier-bearing press neither doubles
nor arms"`, subcase `"the arming is spent by the gesture it completed"`.
WHY — `agents/decisions/time-is-an-argument.md`

## WL-PTR-04 — A fitted row may be read past, and only under the pointer

LAW — `Session::reveal` is presentation only, and `detail::reveal_shown` returns the revealed window only when surface, item, a non-zero offset and the string all agree.

MEANS
- the guard is the reset: there is no clearing path anywhere;
- no file, setup, document, provider or value is touched, and nothing durable holds it.

DOES NOT MEAN
- that the reveal is asked while a mode or a held gesture owns the pointer — it is empty then.

PROVEN BY — `workshop/screen.hpp` `Revealed`, `reveal`, `detail::reveal_shown`, `revealed_row`;
`tests/test_workshop_screen.cpp` case `"WUX-7: four things must agree before a row is scrolled
at all"`, case `"WUX-7: a revealed row is a window over the same string, never a wider row"`.
WHY — `agents/decisions/the-row-is-its-own-scrub-track.md`

## WL-PTR-05 — The item is the identity, never the prose row

LAW — A reveal is bound to the item, not the row it is painted on, and eligibility is `rest != full`: a value that fits never moves, and a provider's shortened text is not recovered.

PROVEN BY — `workshop/screen.hpp` `reveal_at`, `reveal_for`, `detail::reveal_shown`, `RevealAt`;
`tests/test_workshop_panels.cpp` case `"WUX-7: hovering a clipped object row reads past its
ellipsis, and nothing else"`; `tests/test_workshop_files.cpp` case `"WUX-7: a SCROLLED listing
reveals the row it is showing, not the row it is at"`.
WHY — `agents/decisions/the-row-is-its-own-scrub-track.md`

## WL-PTR-06 — The pointer's column is the offset

LAW — The left edge of the row is the value's start, the right edge its end, everything between is proportional and monotone, and the head is marked the way the tail is.

MEANS
- `revealed_row` clamps the offset itself, so "a value that fits never moves" is the projection's.

PROVEN BY — `workshop/screen.hpp` `reveal_offset_at_column`, `reveal_max_offset`,
`revealed_row`; `tests/test_workshop_screen.cpp` case `"WUX-7: the pointer's column is the
offset, monotonically and totally"`, subcase `"the head is marked, and the furthest offset shows
the true tail"`.
WHY — `agents/decisions/the-row-is-its-own-scrub-track.md`

## WL-PTR-08 — The first consumer set is four rows

LAW — The Files pane's location header and listed names, and the Info panel's object rows and resting property rows, reveal; a fifth is one `reveal_shown` call and one resolver arm.

DOES NOT MEAN
- that a live draft reveals — it is windowed against its own caret and is excluded;
- that this is a registry — it must not become one.

PROVEN BY — `workshop/screen.hpp` `reveal_place`, `detail::reveal_shown`, `reveal_for`;
`tests/test_workshop_files.cpp` case `"WUX-7: hovering the browser's location reads the path it
could not show"`, case `"WUX-7: hovering a listed name reads the rest of it, and only that
row"`; `tests/test_workshop_panels.cpp` case `"WUX-7: hovering a clipped object row reads past
its ellipsis, and nothing else"`.
WHY — `agents/decisions/the-row-is-its-own-scrub-track.md`

## WL-PTR-09 — The terminal cannot report a hover

LAW — The terminal medium asks for button-event tracking (`1002`), so an idle pointer reaches nobody there; it is a documented medium fact, not a defect to repair with `1003`.

PROVEN BY — `surface/skin_tui.hpp` `kTuiPointerOn`; `docs/workshop/limitations.md` `hover`;
`tests/test_surface.cpp` case `"the Skin's terminal claim includes pointer reporting, and leave
undoes enter"`.
WHY — `agents/decisions/the-row-is-its-own-scrub-track.md`

## Do not assume

- That a mode's pointer ownership over the reveal has a witness — it does not (WL-PTR-04).
