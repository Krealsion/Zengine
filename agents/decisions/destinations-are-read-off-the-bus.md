# Destinations are read off the bus

**Decision record.** One decision, its alternatives, and why this one. The law it supports is in
[terminal](../workshop/terminal.md).

**Context.** A maker addressing a line had to know an id or an office already: the completer
offered `#<id>` and `@<office>` as forms, because the participant holds no directory of weaves or
roles. The founder asked for real, currently discoverable destinations, through their owner, with
enough identity to choose deliberately -- and without granting the Terminal a tap or keeping a
second registry.

**Decision.** The owner of which weaves exist and which offices they hold is the Switchboard. The
host that holds it answers the completion ask, so when the line is at its address the host reads
`list_weaves`, `sealed`, `role_of`, `alive` and `accepted_schemas` at that moment and hands the
completer a value for one answer. The list is `*`, the offices held now by name, and the weaves
registered now by id, each saying what it is. Nothing keeps the reading.

**Alternatives considered.**
- *The Loaded pane's `zen.ListLoaded`* -- refused: it names kernel libraries only, so Workshop, the
  participant and every in-process weave would be missing.
- *A directory kept current by watching the bus* -- refused: a second registry and a tap; a weave
  replaced between two asks would be listed by a copy.
- *Handing the participant or the pane the bus* -- refused: the pane may not reach a participant
  (WL-TERM-08), and the participant's grant is narrow on purpose.
- *Marking which destinations the line may reach* -- not possible at the address: a grant is per
  shape, and the shape is the next word.

**Consequences.** A removed id comes back as Loom's `NoSuchTarget` on the participant's own record;
an office reaches whoever holds it at delivery; a host that lists nothing still offers the forms.
A Loom door answering the same question for a loaded asker is the seam an inspection tool would use.

**Laws supported.** [WL-TERM-16](../workshop/terminal.md).
