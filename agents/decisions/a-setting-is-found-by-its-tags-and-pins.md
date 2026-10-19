# A setting is found by its tags and pins

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [settings](../workshop/settings.md).

**Context.** Panes declare more settings than a weaver wants to scan, each with tags and the few it
pins by default (`v2::PaneSettingRow`). An editor of settings needs the few that matter first and a
way to the rest, without the host deciding what a tag means.

**Decision.** Workshop keeps each pane's pin list, edited by message (`PaneSettingPinRequested`):
Workshop-wide by default, or per layout, falling back to the declaration's own pins, kept in its
settings file and never handed to the pane. A reading of a pane's row (`PaneSettingsShown`) says the
list in pin order. Finding a setting by its tags is the editor's: `component/tab_strip.hpp` derives
the tabs -- pinned, all, each tag, kept -- and what a width shows of them, a window grown outward
from the active tab, counting what it leaves out. The editor is the settings pane, not the
inspector: Info shows a pane's rows and does not filter them.

**Alternatives considered.**
- *Tried: the strip and the pin keys in Info* -- withdrawn at review: editing settings belongs to the
  pane made for it, and Info inspects.
- *Argued: a scroll offset kept for a strip* -- refused: a re-declaration that removes a tag would
  leave it pointing at nothing; a window derived from the active tab cannot go stale.
- *Argued: pins as a pane's own setting* -- refused: they arrange an editor's view and mean nothing
  to the pane.

**Consequences.** A per-layout list is keyed by the layout's name until the desk carries its own
pins.

**Laws supported.** [WL-SETTING-12](../workshop/settings.md).
