# The maker's state is a first-class Loom schema

**Decision record.** One decision, its alternatives, and why this one. Not a how-to — the law it
supports is in [definition](../maker/definition.md) and [weave](../maker/weave.md).

**Context.** Every weave in this repository had a `ZEN_SHAPE` state and a class; the Switchboard
claims a weave's state schema from its first snapshot and gates every reload against it. A maker
at the Workshop's editor produces no class. Something must still be a `loom::Weave`, and its
state must still be a Value the gate admits, the registry resolves, the poke doors describe and
`serialize` writes.

**Decision.** The definition carries the state schema as a `zen.SchemaDesc`, rebuilt at admission
through the kernel's own codec; the interpreter is one raw `loom::Weave` whose snapshot is the
maker's Value at that schema, so registration claims the data-built schema exactly as it claims a
compiled one. The state file is that Value's own native bytes in its own envelope, no wrapper. A
trigger is one composition over the host's catalog, spent over a pack of the state's fields then
the message's, and its one answer lands in a named field through the catalog's output gate. The
emit is a field-wise write to a declared shape, published under the weave's own grant. The
definition carries no author or signature field.

**Alternatives considered.**
- *A generic `MakerState` shape holding a list of tagged cells* — argued and rejected: the
  registry would resolve one shape for every maker, the gate would check nothing a maker meant,
  and two makers' states could not be told apart at a door; the descriptor route keeps the
  substrate's own identity check.
- *A wrapper envelope around the state* (`{ author, state : Bytes }`) — rejected: it would put
  an unsigned claim beside the value and make every reload a two-step admission; the identity
  check the research ran shows a later identity as a v2 nesting this v1 with one edge, so nothing
  is lost by waiting.
- *JSON for the two files* — rejected: the compat codec is lossy of byte-canonicality and carries
  no mandatory content id, so a reader could not challenge a claim before decoding it; pinned by
  case `"FC-8: the definition and the state are two native files written by one process, and a
  fresh process reads them back with high == 7"`.
- *Copying the catalog into the weave at registration* — tried as a mutation and caught: an
  overlay under the trigger became invisible; pinned by case `"FC-4: the body is a composition
  spent through the host's catalog -- a power overlaid underneath moves the trigger, and revealing
  it moves it back"`.
- *Writing the answer by position* — tried as a mutation and caught by a state that lists another
  field first; pinned by case `"FC-4: the answer lands in the named state field, and an answer of
  another kind is refused with the state unchanged"`.

**Consequences.** A maker weave is inspectable through the substrate's doors like any other, is
namespaced by its name so two makers cannot collide at the registry, and needs no export of a
shape header. A behaviour edit is the substrate's own `swap_state`. The trigger's operators are
resolved at spend, so the package caches nothing and a replaced power reaches every maker weave.

**Laws supported.** [MW-DEF-01](../maker/definition.md), [MW-DEF-02](../maker/definition.md),
[MW-DEF-05](../maker/definition.md), [MW-DEF-06](../maker/definition.md),
[MW-WEAVE-01](../maker/weave.md), [MW-WEAVE-02](../maker/weave.md),
[MW-WEAVE-03](../maker/weave.md), [MW-WEAVE-04](../maker/weave.md),
[MW-WEAVE-05](../maker/weave.md), [MW-WEAVE-06](../maker/weave.md),
[MW-WEAVE-07](../maker/weave.md), [MW-WEAVE-08](../maker/weave.md).
