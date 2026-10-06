# Pointing is not selection

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [contextual](../workshop/contextual.md).

**Context.** Every backend delivered button 3 and Workshop dropped it; a weaver pointing at a
thing had no way to ask what could be done with it (`8981b60`, "A maker can ask a pointed thing
what can be done with it"). Opening such a surface must not change what the weaver had chosen.

**Decision.** Pointing names a subject for one request; selection is a state a weaver entered.
Opening captures an identity — a `PaneRef`, an object id, a layout position, or nothing; never a
rectangle, row or handle — and changes no selection, candidate or focus; spend re-asks the
owner. Arrange is the one exception, and only after its target passes admission.
`kContextCatalog` declares rows over the action catalog's ids and owns no power. A row teaches
its shortcut only when its action owns a binding active in the context the weaver returns to.
Spending is one seam per subject kind and paint is not policy. The surface is a mode with first
refusal. A right press in a pane's body is never lost: the pane acts or offers its rows where it
has something there, and the host's pane menu opens everywhere else; a pane's own menu carries
the host's standard rows beneath its rows, and each row is spent by its owner.

**Alternatives considered.**
- *Capturing a rectangle, row or handle* — rejected: a ref outside the setup gets one truthful
  absence sentence, not a geometry refusal; pinned by case `"a captured pane that left the setup
  is refused truthfully"`.
- *Owner predicates on the paint path* — rejected: the menu renders an identity, not an
  existence claim.
- *Annotating every row with its binding* — rejected: the live TUI witness read `^w` beside a
  Close acting on a tab the weaver was not standing on (`2dc7626`); pinned by case `"shortcut
  annotations teach only truthful surrounding bindings"`.
- *Provider-contributed rows, or a second-button `PanePressed`* — not done; pinned by case
  `"a right press over a provider's pane crosses the seam not at all"`.
- *Toggling on a further right press* — rejected: it re-targets.
- *Tried: a body without the door is empty* — replaced: a right press in Flow, Powers, or the
  Editor off its highlight opened nothing, and the weaver could not tell a dead mouse from a
  pane that meant nothing by it.
- *Argued: the host paints its rows under the presenter's lines* — refused: one popup with two
  owners of its cursor and keys. The presenter shows them; the host spends them.
- *Argued: the standard rows inside the pane's own rows, under reserved ids* — refused: the
  requester would be answered with an id it never offered, and the host would not hear the
  choice. They travel apart (`standard`), and the requester is answered unchosen.
- *Argued: the groups flattened into a pane's menu* — refused: a dozen rows under every pane's
  own. A group row opens the host's own menu at that group.

**Consequences.** `load_document()` drops a captured object subject, the one identity-aliasing
door. `manage.remove` (`d`) completed the arranging vocabulary as the same owner arm the menu's
remove row spends. `Order >` is not called `Arrange`, because an `arrange` row one level up would
make that a lie. A live draft holds a contextual delete back. A stale catalog reference is a
compile error.

**Laws supported.** [WL-CTX-01](../workshop/contextual.md),
[WL-CTX-02](../workshop/contextual.md), [WL-CTX-05](../workshop/contextual.md),
[WL-CTX-06](../workshop/contextual.md), [WL-CTX-07](../workshop/contextual.md),
[WL-CTX-08](../workshop/contextual.md), [WL-CTX-09](../workshop/pane-menu.md).
