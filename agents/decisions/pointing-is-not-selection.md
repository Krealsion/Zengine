# Pointing is not selection

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [contextual](../workshop/contextual.md).

**Context.** Every backend delivered button 3 and Workshop dropped it; a maker pointing at a
thing had no way to ask what could be done with it (`8981b60`, "A maker can ask a pointed thing
what can be done with it"). Opening such a surface must not change what the maker had chosen.

**Decision.** Pointing names a subject for one request; selection is a state a maker entered.
Opening captures an identity — a `PaneRef`, an object id, a layout position, or nothing; never a
rectangle, row or handle — and changes no selection, candidate or focus; spend re-asks the
owner. Arrange is the one exception, and only after its target passes admission.
`kContextCatalog` declares rows over the action catalog's ids and owns no power. A row teaches
its shortcut only when its action owns a binding active in the context the maker returns to.
Spending is one seam per subject kind and paint is not policy. The surface is a mode with first
refusal.

**Alternatives considered.**
- *Capturing a rectangle, row or handle* — rejected: a ref outside the setup gets one truthful
  absence sentence, not a geometry refusal; pinned by case `"CTX-0: a captured pane that left
  the setup is refused truthfully"`.
- *Owner predicates on the paint path* — rejected: the menu renders an identity, not an
  existence claim.
- *Annotating every row with its binding* — rejected: the live TUI witness read `^w` beside a
  Close acting on a tab the maker was not standing on (`2dc7626`); pinned by case `"ARR-0:
  shortcut annotations teach only truthful surrounding bindings"`.
- *Provider-contributed rows, or a second-button `PanePressed`* — not done; pinned by case
  `"CTX-0: a right press over a provider's pane crosses the seam not at all"`.
- *Toggling on a further right press* — rejected: it re-targets.

**Consequences.** `load_document()` drops a captured object subject, the one identity-aliasing
door. `manage.remove` (`d`) completed the arranging vocabulary as the same owner arm the menu's
remove row spends. `Order >` was renamed from `Arrange` when an `arrange` row one level up made
that a lie. A live draft holds a contextual delete back. A stale catalog reference is a compile
error.

**Laws supported.** [WL-CTX-01](../workshop/contextual.md),
[WL-CTX-02](../workshop/contextual.md), [WL-CTX-05](../workshop/contextual.md),
[WL-CTX-06](../workshop/contextual.md), [WL-CTX-07](../workshop/contextual.md),
[WL-CTX-08](../workshop/contextual.md).
