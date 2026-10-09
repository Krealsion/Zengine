# A setting is a typed value its layout's row keeps

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [settings](../workshop/settings.md).

**Context.** A pane makes choices about how it presents itself — a row shown or hidden, a step, a
mode — and a weaver wants them to differ per layout and to survive a restart. The setup row
kept a pane's place, size and rank and nothing else, so a pane had nowhere to keep such a choice
but its own state, which is global across layouts and lost to a reload.

**Decision.** A setting is a key and exactly one value in its kind's own field: a flag, a number
or a text. Each layout's setup row keeps its pane's settings beside place and size, in key
order, once each, judged by form alone. The kinds are the three a presentation choice needs: a
switch, a count or step, and a word, of which a choice among words is a text the pane's row
restricts. Loom leaves "exactly one present" to the receiver, so every reader judges it, naming
the key. The row moved the setup to version 5, which keeps a version-4 reader, and the session
to version 8, whose version 7 is a conversion's (`session_v7_to_v8`), never the reader's.

**Alternatives considered.**
- *Argued: a word for every setting* — refused: a number or a flag spelled as text is a value
  each reader must parse again, and a word cannot say which kind it is.
- *Argued: a typed value of the pane's own shape* — refused: every setting a pane adds would be a
  version of its shape, a retained struct and a converter, nested in the persisted row.
- *Argued: a real number* — refused: NaN is unequal to itself, so a layout holding one would read
  `modified` forever, and a fifth field costs every setting a decoded cell, which the desk's
  bound cannot spare. A fraction is a whole number of a finer unit.
- *Argued: a list, a colour or a gesture* — refused: a set is one flag per member, a pane draws
  in roles, so a colour is a role's name, and gestures are the keymap's.
- *Argued: an optional list of settings* — refused: absent and `[]` would be two spellings of
  none.

**Consequences.** A kind added later moves the setup and the session versions again. The desk's
bound is set by Loom's decode budget at five cells a setting, and the setup's byte ceiling by the
worst spelling of each. Keys under `workshop.` are reserved for Workshop's own settings, so an
older build refuses a newer file rather than drop what it cannot read. The Terminal's legend is
the first setting a pane declares.

**Laws supported.** [WL-SETTING-01](../workshop/settings.md),
[WL-SETTING-02](../workshop/settings.md), [WL-SETTING-03](../workshop/settings.md),
[WL-SETTING-04](../workshop/settings.md), [WL-SETTING-06](../workshop/settings.md),
[WL-TERM-19](../workshop/terminal-pane.md).
