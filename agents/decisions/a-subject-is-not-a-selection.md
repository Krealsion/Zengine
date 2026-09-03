# A subject is not a selection

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [pane-manager](../workshop/pane-manager.md).

**Context.** Info inspects document objects, and a maker needed the same grammar for an
ordinary Workshop pane. Since the tabs conversion a press into any pane selects that pane, so a
subject derived from the selection would retarget whenever the maker pressed into the editor
itself (`e08d868`, "Let a maker edit a pane by describing it, because the editor's subject is a
pane"). The name `Pane Editor` overclaimed a tool that edits a pane's inside (`df06ac5`).

**Decision.** The Pane Manager is the keyboard-taking built-in whose subject is a pane; its
durable key `pane-editor`, its symbols and its action ids are the older name's, unchanged. The
subject is a `PaneRef` on the session written by `choose_subject` and nothing else, never derived
from `Panels::selected`; it stands through a layout switch, its pane closing and its provider
going away, and one rule clears it at a gesture. Every row reads fresh and nothing writes on
paint. Every write is an existing door. A typed value is refused, never clamped. `kDraft` is one
context for two inspectors. Both lists scroll under the wheel and the wheel moves no keys.

**Alternatives considered.**
- *Deriving the subject from the selection* — rejected: pressing into the manager would retarget
  it, and the manager could not be its own subject; pinned by case `"WUX-13/SC-15: the Pane
  Editor can be its own subject, and its own rows do not retarget it"`.
- *Persisting the subject* — rejected: a presentation preference riding an authored artifact;
  pinned by case `"WUX-13/SC-13: a Pane Editor edit survives a restart through the session, and
  the subject does not"`.
- *An editor-side catalog or copied rows* — rejected: `inventory_rows`, the picker's own
  population, read fresh.
- *A `PaneEditor` setter or a held rectangle* — none; `toggle_participation` is `choose_panel`'s
  body quarried out, and `pane_window_base` is `managed_window_base`'s.
- *Clamping a typed value or converting the other face's word* — refused; pinned by case
  `"WUX-13/SC-7: a typed value that is not admissible is refused, and the authored row is
  untouched"`.
- *Renaming the symbols and the durable key with the product* — refused: a key is a promise to
  every setup, session and keymap file that names it.
- *Clearing the subject on paint, or in `forget_removed_selection`* — rejected: asked at a
  gesture, never on paint.

**Consequences.** Info is untouched: it owns the product's first five minutes and the only typed
doors for Context and a share. `kPickerNameCols` went 10 → 12 → 13 to hold the name whole, at
the measured cost of three summary cells at every width; an earlier widening of that column for
a name had been measured, reverted and recorded. An off-room pane is typeable, because a typed
coordinate needs no rectangle to measure from.

**Laws supported.** [WL-PED-01](../workshop/pane-manager.md),
[WL-PED-02](../workshop/pane-manager.md), [WL-PED-03](../workshop/pane-manager.md),
[WL-PED-04](../workshop/pane-manager.md), [WL-PED-05](../workshop/pane-manager.md),
[WL-PED-06](../workshop/pane-manager.md), [WL-PED-07](../workshop/pane-manager.md),
[WL-PED-08](../workshop/pane-manager.md).
