# The window is a choice in the plan, and the launch's fallback

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [project](../workshop/project.md).

**Context.** Workshop shipped two plans that differed in one row, the window: the default in the
TUI window, and a graphical plan in the GUI window, staged only where SDL was built and passed
by the window launchers. A project's plan copies the plan in force, so a project begun in one
window never tried the other, and a GUI window that could not open stayed loaded with no window,
composing a picture nothing showed while the desk waited for it.

**Decision.** One shipped plan, whose window row loads the GUI window, `otherwise` the terminal
window (load-plan format version 4). A row's artifact whose load is refused gives way to the next
it names, said once on the desk; one the installation does not hold gives way unsaid, so a build
without the GUI window starts in the TUI window and says nothing. The GUI window that cannot
open refuses its own making, so nothing of it stays loaded. `--tui` takes the TUI window alone
among the plan's windows and refuses a plan offering none. The choice is the launch's, and a
project's plan carries the row as read.

**Alternatives considered.**
- *Argued: the windows as `choices`* — refused: choices switch one role and a session remembers
  them; the two windows are two roles, and nothing switches or remembers a window.
- *Argued: a `windows` list outside the rows* — refused: the window's place in authored order,
  between the directory and the input row, is the plan's meaning.
- *Argued: the window loaded, then unloaded when it says it has no window* — refused: the plan's
  booter holds no unload, and a window that loads and then leaves is one a desk can have offered
  itself to; pinned by case `"the GUI window SDL cannot open, told a video driver it lacks, is
  refused at its load and leaves no window, no SDL, no role held and no claim on SDL's queue"`.

**Consequences.** One plan, one launcher per window, one project for both; a fallback is said
where a weaver reads it, in SDL's own words, which the refused making carries through the Loom. A
plan naming one window behaves as it always has.

**Laws supported.** [WL-PROJ-17](../workshop/project.md).
