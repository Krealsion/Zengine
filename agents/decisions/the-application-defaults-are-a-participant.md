# The application's defaults are a participant

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws
it supports are in [desktop](../workshop/desktop.md).

**Context.** Four unrelated-looking things were compiled into the Workshop host and could not be
replaced: the `Ctrl+t` global that opened the Terminal overlay (retired with the overlay, so the
chord a maker's hand knew named nothing), the `p` picker — a mode that owned the keyboard whole
and TOGGLED a pane's participation — the Escape-to-deselect line at the end of `on(KeyPressed)`,
and the prototype object canvas standing where a desk's own floor belongs. None of them is a
fact about ROOM, FOCUS, REALIZATION or ROOTS, the four the host keeps (VD-19). The founder's
direction of 2026-09-17 selects making them replaceable, with the running Workshop as the
consumer. P-WORK-19 named the shape the launch bindings need: usable without the tool already
holding focus, with precedence, overrides and conflicts explicit.

**Decision.** A participating weave holds the office `zengine.desktop` and declares APPLICATION
actions: rows that are requestable wherever the maker is standing, joined into the one keymap
under the one collision law, and dispatched back to the declarer as a resolved id. Each row
declares one of two precedence classes, and there are exactly two because the chain has exactly
two places an application row can be answered: ABOVE THE MODES, before the keys cross to a pane
(what a launch binding needs), and DEFAULT, where the resolved context claimed nothing and no
pane took the keys (where the host's Escape line already sat). A pane stands in for an
application row by naming its id on its own row, the accepted `supersedes` mechanism unchanged.
Launching is a request the host performs against the ONE inventory: a pane already on the desk
is FOCUSED, never closed and never unloaded, and a pane whose office nobody holds now is refused.
The room's floor is rows the desktop says and the host paints. The inventory is said when it
changes and answered to a presenter that arrives. A plan row may declare itself optional, and a
refused optional row is an unavailable tool rather than a refused project.

**Alternatives considered.**
- *One precedence class* — rejected, and it is the decision's core. A single class either puts
  Escape above every pane (Neovim never sees it) or puts `Ctrl+t` below every mode (it works
  only when nothing is happening). Both were traced in the chain before the second class was
  added; the first is the defect QR-18 was written to prevent.
- *Answering an application row where no pane replied* — refused. The seam carries no
  `consumed` and a pane's silence settles nothing (WL-ARR-15); inferring non-consumption from
  quiet is the exact failure the accepted Escape correlation repair exists to end.
- *A launch-only registry beside `PaneActions`* — rejected: it would leave the same ownership
  problem in the existing action path, and BL-WORK-04 would then be owed twice. One refusal
  shape answers both declaration surfaces.
- *Letting the desktop clear the selection itself* — rejected: the selection is `Panels::selected`
  and belongs to the party that owns the room. The desktop owns WHEN, the host owns the act, and
  the answer echoes the number the ask went out under or acts on nothing.
- *Keeping Escape-to-deselect hard-wired behind the declared row* — refused, on the founder's
  word: "let makers replace or disable application defaults, proving this for Escape-to-deselect
  as well". A default with a compiled-in copy behind it is not replaceable. The recovery route
  is the independent host/console, not a second copy of the policy.
- *Making `none` mean "delete the row"* — rejected: the row stays declared, listed and
  nameable; what it has is no gesture. Four rows shipped unbound already, so the state existed
  and only the authoring did not.
- *A blanket "continue past a failed plan row"* — refused. `allow_any()` and skip-what-fails are
  the two things the optional field is written to not be: a row is stepped over only because a
  maker WROTE that it may be, authored order is unchanged, and the refusal is reported by name.
- *A bool on the existing artifact row shape* — refused: adding a field changes the content-id,
  so the row and its envelope are versioned and versions 1 and 2 are read against their own
  retained shapes (Loom GATE-04's rule, as `v2::PaneActions` took it).

**Consequences.** A Workshop whose desktop did not load has no application defaults: `Ctrl+t`
does nothing, Escape sheds no selection, and the floor is empty. That is the honest reading of a
replaceable shell and it is diagnosable from three surfaces at once — the boot's own stdout, the
standing conditions the Attention pane lists, and the launcher's `[gone]` rows. Every suite that
asserts the deselect now supplies the declarer, which is stronger evidence than the line it
replaced: it exercises the whole relocated path. The `p` picker and the object canvas are still
standing; their retirement waits on Info gaining a real pane subject, which is the arc's other
half.

**Laws supported.** [WL-DESK-01](../workshop/desktop.md),
[WL-DESK-02](../workshop/desktop.md), [WL-DESK-03](../workshop/desktop.md),
[WL-DESK-04](../workshop/desktop.md), [WL-DESK-05](../workshop/desktop.md),
[WL-DESK-07](../workshop/desktop.md), [WL-DESK-08](../workshop/desktop.md),
[WL-DESK-09](../workshop/desktop.md), [WL-DESK-10](../workshop/desktop.md).
