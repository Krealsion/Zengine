# A pane declares its actions

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [keyboard](../workshop/keyboard.md).

**Context.** Every pane a maker sees is to be a participating weave (the pane-weave arc,
2026-09-06), and the first thing every migration needs is what no pane weave had: its keys in
the legend and its rows under a maker's keymap file. The Powers pane matched four raw scancodes
against numbers, so `Tab`, `Up`, `Down` and `Return` sat in no legend and no override could move
them; a Files weave would have brought ten such rows and every maker's authored `files.up` with
them. The host's one binding truth — id, label, context, default gesture in `kActionCatalog`,
the collision law over the effective map — stopped at the seam.

**Decision.** A pane declares rows of that same catalog — id, label, default gesture as
`PaneKey`'s two numbers — in one shape beside its offer (`PaneActions`), judged whole under the
office stamp exactly as the offer is. The host joins them at admission: the maker's authored
overrides are applied to the ids through the file's own grammar, and the collision law runs over
the effective map in the words the file meets. A pane's context is its runtime handle: its rows
are active exactly while `keyboard_pane` resolves to it, meet the globals and the no-editor rows
(what is active while a text-taking pane holds the keys), and never another pane's. When the
keyboard pane holds the keys and the pressed gesture is one of its effective bindings, the
**resolved id** crosses (`PaneActionRequested`) instead of the raw key, and the character the
keystroke produced is swallowed; every other key crosses as `PaneKey`, so a `p` typed into a
field is still a `p`. The keymap file's load re-joins every pane's retained declaration, and a
pane the file now collides with is the party refused, so both load orders end the same way.

**Alternatives considered.**
- *Tried: forwarding the raw key and letting the pane match its own table* — refused: a maker's
  override never reaches the pane and the declaration is decorative; pinned by case `"a maker's
  override moves a Powers action, and the key it left no longer acts"`.
- *Argued: a `KeyContext` value per pane* — refused: the enum is closed and
  `contexts_intersect(kPane, kPane)` is true, so two panes declaring one bare key would collide
  falsely; the handle is
  already the integer the catalog keys runtime panes by (`kFirstRuntimeKind`), and comparing
  handles refuses the false collision by construction.
- *Argued: refusing the maker's file when a pane's rows collide with it* — refused: the outcome
  would depend on which party arrived second; the file is the maker's and always wins, and the
  pane is judged in both orders.
- *Argued: rows on `PaneOffered`* — refused: the descriptor is frozen at version 1 (GATE-04),
  and a second version would make every older provider's offer a different shape.
- *Argued: the contextual surface listing a pane's rows* — deferred: it declares over
  `kActionCatalog` ids at compile time, and a runtime join on the pane subject is a second
  consumer with its own law.

**Consequences.** Eleven shapes, not eight; `Act` and `KeyContext` are untouched — a pane row
has no dispatch site in the host. An unknown id in a keymap file is still preserved byte for
byte, which is what lets an override authored before a pane loads apply the moment it does. A
pane's declared rows are bounded (`kMaxPaneActionRows`) and its ids may not be Workshop's own,
so one authored row never names two things. "No shape in which a provider says it wants keys"
stands: declaring rows points no keyboard and holds none.

**Laws supported.** [WL-KEY-15](../workshop/keyboard.md).
