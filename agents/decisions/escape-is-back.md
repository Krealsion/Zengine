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
  rejected: the editor's Escape is a pinned no-op (a habitual Esc must not hand the next `d` to
  command mode), and a focused external pane has already been sent the key with no `consumed`
  coming back, so Workshop cannot see it decline; pinned by case `"QR-18/SC-1+SC-2: a focused
  external pane keeps Escape; a press on a pane that takes no text, then Escape, puts the
  selection down"`.

**Consequences.** `unselect_pane` is the press-on-nothing line spent from the keyboard, the
fourth writer of `Panels::selected`. The way out of a typing place is the way in: press a pane
that takes no text (every desk has Layouts) or the workspace, then Escape. The picker remains
how presence changes.

**Laws supported.** [WL-ARR-13](../workshop/arrangement.md),
[WL-ARR-14](../workshop/arrangement.md).
