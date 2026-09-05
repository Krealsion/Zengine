# The seven kinds close the maker path

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [definition](../maker/definition.md).

**Context.** The Loom's value model has seven kinds — Int, Float, Text, Bool, Bytes, Message and
List — and a field's `required` bit. The shape path (`ZEN_SHAPE`) spells required-only. A maker
authoring a state at an editor will want a keyed table, a one-of, an optional field, and the
research asked whether the maker path should grow a kind or a default for any of them.

**Decision.** The seven kinds close the maker path. A keyed table is a List of entry Messages, a
one-of is several optional fields, optionality is the field's `required` bit, and a state that
nests a Message or a List lists it in the definition's `referenced` section, post-order, decoded
by the manifest's own codec. `required` stays the default on both paths; the maker's tool may
author an optional state field only where no trigger binds it, and an unbound optional field is
absent in the default state.

**Alternatives considered.**
- *An eighth kind (a map, a variant)* — argued and rejected: the codec refuses a kind out of
  range in every older reader, so an appended kind is a format break for every file already
  written; the research measured that as the append-only cost and found no maker need that a
  list of entries does not meet.
- *Optional as a default on the maker path* — rejected: the walk refuses an absent input at spend
  as `no input named`, so an optional field a trigger binds would be a refusal a maker meets
  late; admission refuses it early instead, pinned by case `"3: an optional state field bound by a
  trigger is refused at admission; an unbound optional field is admitted and absent in the default
  state"`.
- *Flattening nested shapes into the state* — rejected: the manifest's `referenced` section
  already carries a nested closure for compiled weaves, and one codec is the whole point; pinned
  by case `"2: a definition whose state nests a message and a list decodes through its referenced
  section -- the seven kinds, closed"`.

**Consequences.** A definition file is self-contained: every schema it nests is in it, in the
order a single forward pass resolves. The default state of a nested Message is that schema's own
default and of a List is empty, so a data-built default always conforms. Nothing in the Loom's
kind table or gate changed.

**Laws supported.** [MW-DEF-03](../maker/definition.md), [MW-DEF-04](../maker/definition.md).
