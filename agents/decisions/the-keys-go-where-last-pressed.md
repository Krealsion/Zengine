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
the candidate. The press that points the keys is not an act in the pane, and Workshop tells the
pane, on the press, whether ordinary keys were already reaching it. The candidate is never
cleared and the target never stored. The modes above never reach that line. The pane gets every
bare key, `q` included. `^c` follows the keyboard. The title's mark names the pane the keys
return to; the band's first row says where a key goes now. Pane titles are a preference, and the
keyboard's pane always keeps its title.

**Alternatives tried.**
- *A pane-local memory of having had the keys* (`had_keyboard_`, `d4815cc`) — retired: Files
  hears nothing when the keys leave it by a press into the Editor, and an arrow arrives as an id,
  so it opened on a press from the Editor and only selected after a title press; pinned by case
  `"the keys leave Files by a press into the Editor and Files is told nothing, so only Workshop
  can say a later press on Files' selected row came from elsewhere"`.
- *Reading the keys, or the pressed row, after the press wrote the keyboard* — measured by
  mutation: a press back into Files read as already there, and a hidden-titles press named the
  row under a title that was not painted.
- *A hand-kept mirror predicate for the `^c` gate* — replaced by
  `context_takes_text(keyboard_context(...))` (`7b64b73`).
- *Hiding the keyboard's pane's title with the preference* — refused: it would recreate the
  measured lie; pinned by case `"WUX-1/SC-5+SC-6: hiding titles returns the row; the keyboard's
  pane keeps its own"`.

**Alternatives argued.**
- *A focus framework, a registration, or a focus-changed notification* — none: a notice would
  fire at every write and every change in resolution, and a missed one activates.
- *Activating a Files row on the first press* — rejected: a maker aiming at a cold pane whose
  cursor rests on the pointed row would open a file, or meet the dirty refusal, having done
  nothing but look; the wire carries no click count.
- *Choosing the press's version by refusal, by sending both, or from a provider's list* — a
  fallback reorders gestures, both is two presses, a list is a second accept-set.
- *Deciding the candidate in the routing arms* — rejected: four decisions about one fact.
- *Bare printables as globals* — rejected: admission refuses a bare printable on a global row.

**Consequences.** A pane that closes, stops resolving or loses its room stops being the answer
with nothing to clear, and gets the keyboard back when it returns. A holder changed before
delivery takes the press or, lacking its door, refuses it; nothing retries. The band's typing row
comes from the keymap's global rows. A hidden title returns its row to the provider through the
grant.

**Laws supported.** [WL-FOCUS-01](../workshop/focus.md), [WL-FOCUS-02](../workshop/focus.md),
[WL-FOCUS-03](../workshop/focus.md), [WL-FOCUS-04](../workshop/focus.md),
[WL-FOCUS-05](../workshop/focus.md), [WL-FOCUS-06](../workshop/focus.md),
[WL-FOCUS-08](../workshop/focus.md), [WL-FOCUS-09](../workshop/focus.md),
[WL-FOCUS-10](../workshop/focus.md), [WL-FOCUS-11](../workshop/focus.md).
