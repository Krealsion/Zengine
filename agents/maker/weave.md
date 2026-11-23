# Maker law — the weave

Register `MW-WEAVE`: what the maker runtime and interpreter do with a definition —
registration, delivery, the
emit, inspection, the behaviour edit. One law per heading; cite by ID. Router:
[`../maker.md`](../maker.md).

Retired: MW-WEAVE-01, MW-WEAVE-05, MW-WEAVE-11

## MW-WEAVE-02 — The accept-set is the definition's, plus the doors

LAW — The accept-set is the definition's accepted shapes, Loom's poke doors, `zen.Activated` and the package's `Quiesce`, `Resume` and `Adopt`; an unlisted shape is refused `NotAccepted` at delivery.

MEANS
- Loom's poke doors are `loom::poke_door_schemas()`, every one of them, whatever their count;
- `zen.Activated` is on every maker weave so a candidate passes prevalidation at commit;
- the accept-set is fixed at registration, as the substrate says; an edit never widens it.

WHY — `agents/decisions/the-maker-weaves-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-03 — The pack is the state, then the message

LAW — A trigger is spent over a pack whose fields are the state's in declared order, then the message's, each present field under its own name; a name both carry is refused at admission.

MEANS
- the host fills the pack; a body reads its arguments as `Input` bindings by name.

WHY — `agents/decisions/the-maker-weaves-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-04 — The one answer lands in the named state field

LAW — A trigger's body answers on one port, `value`, whose type is the target field's own, so the catalog's output gate is the kind check; the answer is written to the named field and nothing else moves.

MEANS
- an answer of another kind is refused in the gate's words, the state unchanged;
- the field is written by name: a state that lists another field first is untouched there.

WHY — `agents/decisions/the-maker-weaves-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-06 — A refused spend leaves the state and refuses by name

LAW — A body that cannot be spent leaves the state unchanged, answers `zen.Refused` to the sender with the deepest layer's own words, and counts.

MEANS
- the reply goes to `reply_to` if given, else the stamped sender; neither means silence;
- to the stamped sender it is Loom's answer to that delivery (`answers_ask`); elsewhere, ordinary;
- the deepest layer names the node; the identity heading it is `<definition> on <message>`.

WHY — `agents/decisions/the-maker-weaves-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-07 — The emit is published under the weave's own grant

LAW — After the write-back each emit is written field-wise from the new state and published with the weave as sender under its own grant; ungranted, the publication is `CapabilityDenied` on the tap.

MEANS
- an emit that cannot be written is refused by name and counted; the state stays written;
- emitted schemas join admission before listeners; with none, publication reaches nobody;
- reply_to is empty on outgoing messages: the stamped sender is the default return address.

WHY — `agents/decisions/the-maker-weaves-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-08 — Inspection: every field named, fields read, nothing written

LAW — `zen.PokeDescribe` names the state schema and every field, none hidden, none writable; `zen.PokeRead` reads a field, or a path at version 2; `zen.PokeWrite` and `zen.PokeResetState` are refused by name.

MEANS
- version 1 answers a scalar as text, and version 2 a place by its path as a `zen.PokeTree`;
- version 2 is Loom's walk of the state's value (`loom::poke_read_tree`) under an access model that hides nothing;
- a maker weave's state is written by its triggers, and the refusal says so.

WHY — `agents/decisions/the-maker-weaves-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-09 — A behaviour edit is `swap_state`

LAW — A behaviour edit keeps the state schema: the successor's bodies mount beside the incumbent's, the weave takes the definition, the old bodies unmount, and `swap_state` announces `Revived`.

MEANS
- a definition whose state schema differs is refused here as a succession (MW-SUCC-01);
- accepted and emitted schema contracts must remain identical to the registered surface;
- a mount refusal changes nothing; the revision must bump, or the provider is already mounted.

WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## MW-WEAVE-10 — The ceremony doors trust the sender the host armed, not the shape

LAW — `Quiesce` and `Resume` are honoured only from the coordinator and token the host armed on the weave it holds, `Adopt` only by an unbound candidate from that coordinator; any other is refused by name.

MEANS
- the sender is the bus's stamp, never a payload field; `begin_schema_edit` arms both weaves;
- a weave registered to its role is bound; a candidate is bound by its attested `zen.Activated`;
- the armed coordinator's `Resume` disarms; a bound weave's refusal says its triggers write it.

DOES NOT MEAN
- that a stranger's `Quiesce` answers anything but `zen.Refused`: no `Quiesced` leaves.

WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## Do not assume

- That the interpreter is a `WeaveBase` (MW-WEAVE-02): it implements `loom::Weave` raw, so its
  doors are exactly the ones it answers, and it restates the reply rule in one place.
- That a behaviour edit re-consents the grant (MW-WEAVE-09): the grant minted at registration
  stands; every definition edit is a new build identity, and the grant is not re-floored
  for it.

## Proven by

### MW-WEAVE-02
`maker/runtime.hpp` Runtime::accepted_schemas
`maker/vocabulary.hpp` Quiesce Resume Adopt
`tests/test_maker.cpp` "the accept-set is the definition's; hw.Sample is delivered and an unlisted
shape is refused NotAccepted"

### MW-WEAVE-03
`maker/write.hpp` pack_schema pack
`maker/runtime.hpp` Runtime::fire
`tests/test_maker.cpp` "the pack is state then message, and a field name both carry is refused at
admission"

### MW-WEAVE-04
`maker/weave.hpp` definitions_of kAnswerPort
`maker/runtime.hpp` Runtime::fire
`tests/test_maker.cpp` "the answer lands in the named state field, and an answer of another kind is
refused with the state unchanged"

### MW-WEAVE-06
`maker/runtime.hpp` Runtime::fire Runtime::refused Runtime::in_weaver_words Runtime::answer
`tests/test_maker.cpp` "a body that cannot be spent leaves the state unchanged and refuses by name"
"the answer lands in the named state field, and an answer of another kind is refused with the state
unchanged" "a trigger's body may fold: the count is folded through math.add into the state, a
refused fold leaves the state unchanged and says why"
`tests/test_flow.cpp` "Flow missing and reshaped leaves refuse by the same detecting layer"
`tests/test_view.cpp` "a refusal reaches the notice row only when Loom attests it answers the view's
own intent; another participant's at that intent's correlation does not"

### MW-WEAVE-07
`maker/weave.hpp` default_grant
`maker/runtime.hpp` Runtime::fire Runtime::emitted_schemas
`maker/write.hpp` write_fields
`tests/test_maker.cpp` "after the trigger the weave publishes hw.HighWater with the written value
under its own grant; ungranted, the publication is CapabilityDenied on the tap"

### MW-WEAVE-08
`maker/runtime.hpp` Runtime::structure Runtime::read Runtime::handle
`tests/test_maker.cpp` "zen.PokeDescribe names hw.State v1 and every field; zen.PokeRead reads high;
write and reset are refused by name" "zen.PokeRead version 2 reads a maker weave's state by its path:
every leaf below the place, nested and listed, and a refusal naming the step"

### MW-WEAVE-09
`maker/weave.hpp` apply_behaviour_edit Weave::adopt_definition Edited
`tests/test_maker.cpp` "a behaviour edit with the schema unchanged is a swap_state -- same WeaveId,
Revived announced, state kept, the new body spent" "a definition whose state schema differs is
refused as a behaviour edit, and the live weave is untouched"

### MW-WEAVE-10
`maker/runtime.hpp` Runtime::arm Runtime::disarm Runtime::set_bound Runtime::from_armed_coordinator
Runtime::handle Runtime::refused_at_door
`maker/succession.hpp` begin_schema_edit
`tests/test_maker.cpp` "before the merge: Quiesce and Resume are honoured only from the coordinator
the host armed for this boundary; a stranger's are refused by name and the weave keeps serving"
"before the merge: Adopt is honoured only by an unbound candidate and only from its coordinator; a
bound weave refuses it by name and its state stands"
