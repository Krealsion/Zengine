# Each surface spells a key as the weaver chose

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [key-spelling](../workshop/key-spelling.md).

**Context.** Two spellings of one gesture exist: the band's compact one (`B`, `^p`, `enter`) and a
keymap file's (`shift+b`, `ctrl+p`, `return`), which the Hotkeys view prints. A weaver reading the
band, the list of keys and a pane's manual should be able to choose how each reads.

**Decision.** Two presentation preferences, kept beside pane titles in the weaver's preferences
file and toggled from a pane's gear: how the band spells a key, short by default, and how the
Hotkeys view spells one, full by default. Each changes its own surface only; a pane's manual spells
as the Hotkeys view does, because it is read beside it. Every spelling is still the keymap's own
function of the gesture. The host spells the Hotkeys view's keys on the value it says
(`KeymapShown`), beside the file's spelling, which stays the binding's identity. A file of the
version before the spellings is read with both at their defaults.

**Alternatives considered.**
- *Argued: one spelling for every surface* — refused: the founder asked for the two apart.
- *Argued: the Hotkeys view's spelling as the Hotkeys pane's own setting* — refused: a pane's
  settings are kept per layout row, so the manual's spelling would depend on which layout holds the
  Hotkeys pane.
- *Argued: the desktop spells from a flag* — refused: two places would spell one gesture.

**Consequences.** `KeymapShown` and the preferences file each took a new version; a reader of the
older file keeps the weaver's titles.

**Laws supported.** [WL-KEY-16](../workshop/key-spelling.md).
