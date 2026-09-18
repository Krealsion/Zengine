# Time is an argument

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [pointer](../workshop/pointer.md).

**Context.** `input::PointerButton` carries no click count and no timestamp on either backend,
so a double-click is Workshop's own interpretation and not a platform's — which is what keeps a
TUI and an SDL Workshop agreeing about a component's grammar (`1bc34e6`, "Let arranging choose
the pane, a double-click choose the word, and a pointer read what a row cut").

**Decision.** `HostContext::interaction_now` is a reading the host may wire, defaulting to a
steady monotonic clock that is never persisted and never on a wire. `Session::click` records
what the last press on an editable line named: which line, which draft of it, which word, and
when. `doubles_a_click` is pure and total — armed, same line, same draft, same word, within
`kDoubleClickMs` (400) — and time is its argument. One seam, `press_selects_word`, serves both
editable lines. The record arms on the way out and the completing press spends it.

**Alternatives considered.**
- *Widening the pointer wire with a click count* — rejected: widening the wire for one
  consumer is the shape this repository refuses; the count would be a platform's reading and the
  two media would disagree.
- *A per-platform or preference interval* — rejected: one gesture would mean two things
  depending on which medium a maker opened.
- *Teaching the Editor's multiline machinery or the Composer's fields* — not done: the Editor
  keeps its own machinery and the pane protocol was not widened.
- *A triple-click* — absent by construction; pinned by subcase `"a third press is an ordinary
  press again -- there is no triple-click"`.
- *Reading the clock inside the predicate* — rejected: with time as an argument every condition
  is falsifiable by a case rather than a stopwatch; `InteractionClock` in the rigs defaults past
  the interval, so two presses are two aims unless a case says `clock.together()`.

**Consequences.** A modifier-bearing press neither doubles nor arms. The tab run got a second
click record of its own, sharing the interval and the arming discipline, because a tab is not a
line, a draft or a word ([the-layouts-pane](the-layouts-pane.md)).

**Since.** The two editable lines this record served were the host's own drafts — Info's
property row and the Pane Manager's — and both left the host: Info is a loaded pane, and the
host's Pane Manager retired, its subject rows committed through Info's door. `Session::click`,
`doubles_a_click` and `press_selects_word` left with their last caller, and WL-PTR-02 and
WL-PTR-03 are RETIRED entries. The decision stands where it still has a consumer: the tab run's
record (`Session::tab_click`, `doubles_a_tab_click`) keeps time as an argument, with the same
interval and arming discipline, and that is WL-PTR-01 now.

**Laws supported.** [WL-PTR-01](../workshop/pointer.md), [WL-PTR-02](../workshop/pointer.md),
[WL-PTR-03](../workshop/pointer.md).
