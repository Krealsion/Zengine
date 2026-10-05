# A pane made from data is a view

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [weaver-pane](../workshop/maker-pane.md) and [view](../view.md).

**Context.** Two ways to make a pane from data stood side by side: the Pane Creator, whose
regions Workshop held, painted and edited through Info
([one way a pane can be implemented](../../docs/history/decisions/one-way-a-pane-can-be-implemented.md)),
and described views, which the view host runs and the View Builder makes. For the first,
Workshop held view behaviour of its own.

**Decision.** The Pane Creator retires into described views. The Pane Manager's `n` shows the View
Builder through the host's launch door. Workshop has no weaver kind, namespace or held
definition, and Info edits no regions: every subject's interior is a capture. The Creator's file
is read as a view by the one reader every view door uses -- a label per region, its text at the
pixels Workshop painted -- under its pane's name made a view's by one rule
(`view_name_of_creator_pane`). A desk naming its pane is converted at load to the view's pane,
where the pane stood. The View Builder keeps the view it runs in a project file of its own and
runs it again at a launch without asking to be shown, so the restored desk seats it; a project
with no such file runs the Creator's file.

**Alternatives considered.**
- *A project view file the builder always runs* (`--pane`'s model) -- rejected: one file name
  comes back, and a view the weaver stopped starts again.
- *Workshop's session remembering the view* -- rejected: Workshop would hold view behaviour.
- *The view host remembering* -- rejected: it owns no file policy.
- *A resumed view asking to be shown* -- rejected: a pane the weaver hid would come back,
  depending on whether the reveal or the restore came first.
- *A one-time converter for old files* -- rejected: a file stays old until it runs; read at every
  door, it converts in memory and is written current only when saved.
- *Cutting a long region line to a label's 64 bytes* -- rejected: the label bound rises to 256.

**Consequences.** `--pane` is read so an old launch line starts, and said once. Four Creator key
ids retire, and `pane-creator.new` is read as `launcher.new`. A desk's conversion moves no format
version. The Pane Creator's own record moved to history.

**Laws supported.** [WL-MAKER-11](../workshop/maker-pane.md),
[WL-MAKER-12](../workshop/maker-pane.md), [WL-MAKER-15](../workshop/maker-pane.md).
