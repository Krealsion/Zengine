# Why the coarse step is four

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [arrangement](../workshop/arrangement.md).

**Context.** The pane boundary cost every stack pane two rows: an interior of 46x7 where the slot
is 48x9. Every other consequence was absorbed by a mechanism that already says what it hid, and
one was not — the Compose pane's form no longer fit the default slot — reported rather than
repaired, because which of its rows should yield is that pane's own decision (`65cf9f1`). The
pressure was closed at the desk instead (`35653ad`).

**Decision.** `=` grows the addressed pane and `-` shrinks it, by `kCoarseStepCells` (4) on both
axes, in both scopes, through `arrange_grow` — the same `pane_window_proposal` at `kBottomRight`
into `author_pane_window` that a shifted arrow spends. Four is pinned by a `static_assert`:
`kStackRows + kCoarseStepCells - 2*kChromeCells - 1 >= 8`. `manage.grow` and `manage.shrink` are
catalog rows declared in both scopes.

**Alternatives considered.**
- *A step of two* — measured: a default stack pane's body is `kStackRows - 2*kChromeCells - 1` =
  6 rows, the Compose form needs 8, and a step of two lands on exactly eight with no room to
  spare.
- *Touching the Composer's composition priorities* — rejected: `composer/view.hpp` is
  byte-identical; the decision is that pane's.
- *A second geometry owner, content measurement or collision avoidance* — rejected: one owner,
  one clamping law; pinned by case `"WUX-6/SC-5: a coarse shrink meets the same per-axis
  refusal a fine one does"`.
- *A chord such as ctrl+shift+letter* — rejected: unsayable from a POSIX terminal; `=` and `-`
  are plain printable ASCII a hand already reads as bigger and smaller.
- *Painting the gesture into a pane's chrome* — refused; pinned by case `"WUX-5: no ordinary
  pane spends a row teaching a key the keymap already owns"`.

**Consequences.** One press turns the Compose pane from a Submit control with no form under it
into the whole form — witnessed off the published canvas, with a real `StartTimer` submitted
from the grown pane. The assertion keeps the number true when `kStackRows` or `kChromeCells`
moves. The driven SDL witness for this found the band's legend cutting the new gesture off its
right-hand end.

**Laws supported.** [WL-ARR-10](../workshop/arrangement.md),
[WL-ARR-11](../workshop/arrangement.md), [WL-ARR-12](../workshop/arrangement.md).
