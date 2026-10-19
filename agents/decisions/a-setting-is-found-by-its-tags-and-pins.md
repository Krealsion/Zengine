# A setting is found by its tags and pins

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the laws it
supports are in [settings](../workshop/settings.md) and [info-body](../workshop/info-body.md).

**Context.** A pane's settings are rows of its subject ([rows](settings-are-rows-of-a-panes-subject.md)).
Once panes declare many, a weaver needs the few that matter first and a way to the rest, without the
host deciding what a tag means.

**Decision.** The host sends every row with how a setting is found: its tags, its place among the
pane's pins, its lock, whether it is kept. Info derives a one-row strip from them -- `pinned`,
`all`, each tag, `kept` -- grown outward from the active tab and counting what it leaves out, and
filters only what it shows. The pin list is the host's, edited by message: Workshop-wide by default,
or per layout, falling back to the declaration's own pins, and never handed to the pane.

**Alternatives considered.**
- *Argued: Info filtering by asking for fewer rows* — refused: the rows are the host's and a commit
  names a row by its place; a filtered picture would move every place.
- *Argued: a scroll offset kept for the strip* — refused: a re-declaration that removes a tag would
  leave it pointing at nothing; a window derived from the active tab cannot go stale.
- *Argued: pins as a pane's own setting* — refused: they arrange an editor's view and mean nothing
  to the pane.

**Consequences.** A per-layout list is keyed by the layout's name until the desk carries its own
pins.

**Laws supported.** [WL-INFO-16](../workshop/info-body.md),
[WL-SETTING-12](../workshop/settings.md).
