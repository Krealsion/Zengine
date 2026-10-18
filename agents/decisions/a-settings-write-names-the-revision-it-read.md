# A settings write names the revision it read

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [settings](../workshop/settings.md).

**Context.** A pane's settings are edited as rows of its subject in Info
([rows](settings-are-rows-of-a-panes-subject.md)). A script, an agent, the Terminal and later
editors need the same row by message, and a write must not land on a row that moved since it was
read: a switch, another editor's write or a re-declaration.

**Decision.** Workshop keeps, for each row of the live layout, a revision minted afresh whenever
what it stands for moved: the desk put live, the row's settings, the counted declaration, the
office's holder. A reading answers it; a write names it, and any other is refused with nothing
written. The write goes through the setter Info's rows use, so both doors judge alike. A holder
taking `v2::PaneSettings` is handed the revision, and names it when it says a value it cannot
use; a word about another revision is late. The doors answer an office, the Terminal the host
widened and a guest whose row has `settings`, and are answered to the asker alone.

**Alternatives considered.**
- *Argued: a counter bumped at each write site* — refused: a write site added later and missed
  would let a stale reading through; a revision minted from what it stands for misses none.
- *Argued: a write naming the value it replaces* — refused: two writers seeing one value would
  both succeed, and a switch to a layout keeping the same value would not be noticed.
- *Argued: the subject's name as the revision* — refused: it covers Info's rows, which move with
  places and sizes a settings writer never read.

**Consequences.** A revision names no layout and is never persisted; a restart starts again. A
v2 holder is handed again at each desk put live, equal settings included.

**Laws supported.** [WL-SETTING-07](../workshop/settings.md),
[WL-SETTING-08](../workshop/settings.md), [WL-SETTING-09](../workshop/settings.md).
