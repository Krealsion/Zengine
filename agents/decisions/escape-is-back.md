# Escape is back, not cancel

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [arrangement](../workshop/arrangement.md).

**Context.** Every immediate-commit gesture in this application is reversible only by
performing the inverse, and there is no undo, so the help says `esc back`. A desk whose panes
cover every usable cell had no way to reach `selection = none`, because the only clearing
gesture was a press on nothing (`8c2fc05`, "Let hidden rows be reachable, and let Escape put
the selected pane down").

**Decision.** Escape's last meaning is to put the selected pane down. Every mode, overlay and
draft answers Escape with its own row first; a bare Escape that reaches a context where a list or
nothing holds the keys, with no binding claiming it, sheds `Panels::selected` and the keyboard
candidate — exactly the press-elsewhere gesture's two lines — and moves nothing else. It is
asked after the resolved context has had the key, and it is not a keymap action. A place a maker
types into keeps Escape while it holds the keys.

**Alternatives considered.**
- *Escape as a keymap action* — refused: a recovery gesture must not be authorable into a
  lockout, the hotkey view's own reason; the ordering is pinned by case `"QR-18/SC-2: every
  more-specific Escape meaning answers first, and deselection waits"`.
- *Escape closing the pane, or touching rank, geometry, the Pane Manager's subject, provider
  state or a file* — rejected: it writes only `kNoPaneKind`; pinned by case `"QR-18/SC-1+SC-3:
  Escape clears the ordinary selection last, and the Pane Editor's subject stands"`.
- *Shedding the selection while the source editor or an external pane holds the keys* —
  rejected, and still rejected for the editors: their Escape is a pinned no-op or Neovim's own (a
  habitual Esc must not hand the next `d` to command mode). What changed is that a pane may now
  SAY the Escape it was sent was unspent (`PaneEscapeUnspent`, WL-ARR-15) and be put down for it,
  while that Escape is still the maker's latest gesture, and that an Escape whose holder has no
  key door crosses as nothing and is answered here (WL-ARR-16). Neither reads silence as
  permission; pinned by case `"QR-18/SC-1+SC-2: a focused external pane keeps Escape; a press on a
  pane that takes no text, then Escape, puts the selection down"` and by case `"a pane that takes
  keys keeps Escape until it says the Escape was unspent"`.

**Consequences.** `unselect_pane` is the press-on-nothing line spent from the keyboard, the
fourth writer of `Panels::selected`. A pane that takes text keeps Escape unless it says otherwise:
the Terminal sheds its list, then its line, then itself, and the two editors keep every Escape, so
the way out of them is the way in -- press a pane that takes no text (every desk has Layouts) or
the workspace. A pane whose holder takes no keys at all is put down by Escape where it used to
swallow it at Loom's gate. The picker remains how presence changes.

**Laws supported.** [WL-ARR-13](../workshop/arrangement.md),
[WL-ARR-14](../workshop/arrangement.md), [WL-ARR-15](../workshop/arrangement.md),
[WL-ARR-16](../workshop/arrangement.md).
