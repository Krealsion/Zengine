# The keys go where the maker last pressed

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [focus](../workshop/focus.md).

**Context.** External panes could be typed into (`a153f67`, the commit that let a weave offer
Workshop a pane), which made the question of where a keystroke goes a maker-visible one, and the
measured lie was keystrokes landing somewhere the screen did not name. While the Editor was the
only built-in that took keys, the routing layer simply named it; at the second such built-in
that spelling became a disjunction somebody must remember to extend (`5a302ae`).

**Decision.** `Panels::keyboard` is a pointing's memory, and `keyboard_pane` resolves fresh at
every spend. Candidacy is declared on the catalog row (`takes_keyboard`); readiness is resolved
live and stored nowhere. One reading at the top of the pressed branch decides the selection and
the candidate. The press that points the keys is not an act in the pane. The candidate is never
cleared and the target never stored. The modes above never reach that line. The pane gets every
bare key, `q` included. `^c` follows the keyboard. The screen says where typing goes in two
places, in characters. Pane titles are a preference, and the keyboard's pane always keeps its
title.

**Alternatives considered.**
- *A focus framework or registration* — none; the declaration moved to the catalog row.
- *Activating a Files row on the first press* — rejected: a maker aiming at a cold pane whose
  cursor rests on the pointed row would open a file, or meet the dirty refusal, having done
  nothing but look; two presses from cold is the price, and a double-click was not available
  because the wire carries no click count; pinned by case `"EDIT-1: the first press into a cold
  pane selects and never activates"`.
- *Deciding the candidate in the routing arms* — rejected: four decisions about one fact, and
  the fourth is the one nobody adds.
- *A hand-kept mirror predicate for the `^c` gate* — replaced by
  `context_takes_text(keyboard_context(...))` (`7b64b73`).
- *Hiding the keyboard's pane's title with the preference* — refused: it would recreate the
  measured lie; pinned by case `"WUX-1/SC-5+SC-6: hiding titles returns the row; the keyboard's
  pane keeps its own"`.
- *Bare printables as globals* — rejected: admission refuses a bare printable on a global row,
  which is why typing `p` into a field does not open the picker.

**Consequences.** A pane that closes, stops resolving or loses its room stops being the answer
with nothing to clear, and gets the keyboard back when it returns. The band's typing-goes-to row
is generated from the keymap's global rows, so the chords it advertises are the ones that work. A
hidden title returns its row to the provider through the ordinary grant door.

**Laws supported.** [WL-FOCUS-01](../workshop/focus.md), [WL-FOCUS-02](../workshop/focus.md),
[WL-FOCUS-03](../workshop/focus.md), [WL-FOCUS-04](../workshop/focus.md),
[WL-FOCUS-05](../workshop/focus.md), [WL-FOCUS-06](../workshop/focus.md),
[WL-FOCUS-08](../workshop/focus.md), [WL-FOCUS-09](../workshop/focus.md),
[WL-FOCUS-10](../workshop/focus.md), [WL-FOCUS-11](../workshop/focus.md).
