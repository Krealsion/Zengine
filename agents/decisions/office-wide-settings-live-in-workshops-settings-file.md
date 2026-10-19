# Office-wide settings live in Workshop's settings file

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [settings](../workshop/settings.md).

**Context.** Some settings belong to no layout's row: how Workshop draws its own chrome, which pin
list an editor edits, a skin's face. They need one value for the whole Workshop, a door to read and
write them like a pane's row, and a file that keeps them across runs without becoming a second,
silent writer.

**Decision.** An office declares its office-wide settings by message, judged by a pane
declaration's rules; Workshop declares its own in code. Both are read and written by the settings
doors' askers at a revision, and kept in Workshop's preferences file, which grows to version 3:
office settings, pin lists and locks as typed key-values, a default kept as its absence.
Versions 1 and 2 are read as Workshop's own three settings. The file is read once and written by
the edit doors only while its bytes are the ones this host read or wrote; a file it could not read
refuses every edit and is never written.

**Alternatives considered.**
- *Argued: a field per preference, as version 2 had* — refused: every new Workshop setting would be
  a format version.
- *Argued: office settings in the session* — refused: they follow the weaver, not the desk, and the
  session's decode budget is spent.
- *Argued: writing on every edit whatever the file holds* — refused: two Workshops, or a hand
  editing the file, would lose each other's settings without a word.

**Consequences.** The lock door writes a lock into the file, beside one authored by hand; every
write door refuses a locked key.

**Laws supported.** [WL-SETTING-10](../workshop/settings.md),
[WL-SETTING-11](../workshop/settings.md), [WL-SETTING-13](../workshop/settings.md).
