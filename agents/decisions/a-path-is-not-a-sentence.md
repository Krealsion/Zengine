# A path is not a sentence

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [project](../workshop/project.md).

**Context.** `detail::fit` cuts a line's tail and marks the cut, which keeps a sentence's useful
half because a sentence front-loads its meaning. A path back-loads it, so the same cut removes the
filename — and once the browser could leave the project, a based spelling with no stated base
had become a wrong-looking name for the right file (`0cf8a94`, "Look somewhere without making it
your project").

**Decision.** `detail::fit_path` is the host's measurer for a path it cuts: enough root cue to say
which filesystem, a mark where the middle was removed, and the tail cut at a component boundary.
`path_root_cue` is purely lexical (`/`, `C:/`, `//server/`). The browser and the Builder are loaded
panes now, and a pane fits its own rows: `pane_text::fit_path` is the browser's measurer, the same
root and mark, and as much of the end as fits, to the character. Neither changes a stored
identity, and no pane widens to avoid a cut.

**Alternatives considered.**
- *`detail::fit` on a path* — measured wrong for the reason above; pinned by case `"fitting a
  path keeps the end that says which file it is"`.
- *Asking the filesystem for the root* — rejected: this runs at every repaint, and the proper
  accessors were measured to throw (`3920bdb`).
- *Widening a pane to avoid the cut* — refused.

**Consequences.** The tab run's setup status reuses the measurer against its own budget, fitting
the path before the verdict words are appended ([the-layouts-pane](the-layouts-pane.md)).

**Laws supported.** [WL-FILES-19](../workshop/files.md), [WL-PROJ-10](../workshop/project.md).
