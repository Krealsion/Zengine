# Maker law — the weave

Register `MW-WEAVE`: what the interpreter does with a definition — registration, delivery, the
emit, inspection, the behaviour edit. One law per heading; cite by ID. Router:
[`../maker.md`](../maker.md).

## MW-WEAVE-01 — Registration claims the data-built schema

LAW — `register_definition` mounts the revision's bodies, builds the weave at its default state, mints the grant and registers it bound to its name as role; the first snapshot claims the data-built schema.

MEANS
- a mount refusal registers nothing; a registration refusal unmounts, because the weave dies;
- a candidate for a succession registers unbound, since a sealed weave may hold no role.

DOES NOT MEAN
- that a definition is loaded: nothing here touches the Kernel, and no artifact exists.

PROVEN BY — `maker/weave.hpp` `Weave`, `Weave::snapshot`, `register_definition`, `Registered`;
`maker/write.hpp` `default_value`; `tests/test_maker.cpp` case `"FC-2: the state schema is built
from data, the registry resolves it by name, and its content id is the descriptor's"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-02 — The accept-set is the definition's, plus the doors

LAW — The accept-set is the definition's accepted shapes, the four poke doors, `zen.Activated` and the package's `Quiesce`, `Resume` and `Adopt`; an unlisted shape is refused `NotAccepted` at delivery.

MEANS
- `zen.Activated` is on every maker weave so a candidate passes prevalidation at commit;
- the accept-set is fixed at registration, as the substrate says; an edit never widens it.

PROVEN BY — `maker/weave.hpp` `Weave::accepted_schemas`; `maker/vocabulary.hpp` `Quiesce`,
`Resume`, `Adopt`; `tests/test_maker.cpp` case `"FC-3: the accept-set is the definition's;
hw.Sample is delivered and an unlisted shape is refused NotAccepted"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-03 — The pack is the state, then the message

LAW — A trigger is spent over a pack whose fields are the state's in declared order, then the message's, each present field under its own name; a name both carry is refused at admission.

MEANS
- the host fills the pack; a body reads its arguments as `Input` bindings by name.

PROVEN BY — `maker/write.hpp` `pack_schema`, `pack`; `maker/weave.hpp` `Weave::fire`;
`tests/test_maker.cpp` case `"FC-4: the pack is state then message, and a field name both carry
is refused at admission"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-04 — The one answer lands in the named state field

LAW — A trigger's body answers on one port, `value`, whose type is the target field's own, so the catalog's output gate is the kind check; the answer is written to the named field and nothing else moves.

MEANS
- an answer of another kind is refused in the gate's words, the state unchanged;
- the field is written by name: a state that lists another field first is untouched there.

PROVEN BY — `maker/weave.hpp` `definitions_of`, `kAnswerPort`, `Weave::fire`;
`tests/test_maker.cpp` case `"FC-4: the answer lands in the named state field, and an answer of
another kind is refused with the state unchanged"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-05 — The body is spent through the host's catalog, at spend

LAW — A revision's bodies mount under `zengine.maker.<name>.r<rev>` in the host's one catalog and unmount at destruction; delivery spends `Catalog::evaluate`, resolving every operator at spend.

MEANS
- a power overlaid underneath moves the trigger and revealing it moves it back;
- a body whose operator is unresolved at spend refuses (MW-WEAVE-06); nothing is cached.

DOES NOT MEAN
- that a weave holds a catalog: it holds the host's by reference, which outlives the bus.

PROVEN BY — `maker/weave.hpp` `definitions_of`, `Weave::mount`, `Weave::unmount`,
`Weave::fire`; `maker/definition.hpp` `Definition::provider`, `Definition::trigger_identity`;
`tests/test_maker.cpp` case `"FC-4: the body is a composition spent through the host's catalog
-- a power overlaid underneath moves the trigger, and revealing it moves it back"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-06 — A refused spend leaves the state and refuses by name

LAW — A body that cannot be spent leaves the state unchanged, answers `zen.Refused` to the sender with the deepest layer's own words, and counts.

MEANS
- the reply goes to `reply_to` if given, else the stamped sender; neither means silence.

PROVEN BY — `maker/weave.hpp` `Weave::fire`, `Weave::refused`; `tests/test_maker.cpp` case
`"FC-4: a body that cannot be spent leaves the state unchanged and refuses by name"`, case
`"FC-4: the answer lands in the named state field, and an answer of another kind is refused with
the state unchanged"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-07 — The emit is published under the weave's own grant

LAW — After the write-back each emit is written field-wise from the new state and published with the weave as sender under its own grant; ungranted, the publication is `CapabilityDenied` on the tap.

MEANS
- an emit that cannot be written is refused by name and counted; the state stays written;
- an emitted shape nobody accepts publishes to nobody, which is every native weave's fate.

PROVEN BY — `maker/weave.hpp` `default_grant`, `Weave::fire`; `maker/write.hpp`
`write_fields`; `tests/test_maker.cpp` case `"FC-5: after the trigger the weave publishes
hw.HighWater with the written value under its own grant; ungranted, the publication is
CapabilityDenied on the tap"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-08 — Inspection: every field named, scalars read, nothing written

LAW — `zen.PokeDescribe` answers the state schema and every field, none hidden, none writable; `zen.PokeRead` answers a scalar as text; `zen.PokeWrite` and `zen.PokeResetState` are refused by name.

MEANS
- a maker weave's state is written by its triggers, and the refusal says so.

PROVEN BY — `maker/weave.hpp` `Weave::structure`, `Weave::read`, `Weave::handle`;
`tests/test_maker.cpp` case `"FC-8: zen.PokeDescribe names hw.State v1 and every field;
zen.PokeRead reads high; write and reset are refused by name"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-WEAVE-09 — A behaviour edit is `swap_state`

LAW — A behaviour edit keeps the state schema: the successor's bodies mount beside the incumbent's, the weave takes the definition, the old bodies unmount, and `swap_state` announces `Revived`.

MEANS
- a definition whose state schema differs is refused here as a succession (MW-SUCC-01);
- a mount refusal changes nothing; the revision must bump, or the provider is already mounted.

PROVEN BY — `maker/weave.hpp` `apply_behaviour_edit`, `Weave::adopt_definition`, `Edited`;
`tests/test_maker.cpp` case `"e: a behaviour edit with the schema unchanged is a swap_state --
same WeaveId, Revived announced, state kept, the new body spent"`, case `"e: a definition whose
state schema differs is refused as a behaviour edit, and the live weave is untouched"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## MW-WEAVE-10 — The ceremony doors trust the sender the host armed, not the shape

LAW — `Quiesce` and `Resume` are honoured only from the coordinator and token the host armed on the weave it holds, `Adopt` only by an unbound candidate from that coordinator; any other is refused by name.

MEANS
- the sender is the bus's stamp, never a payload field; `begin_schema_edit` arms both weaves;
- a weave registered to its role is bound; a candidate is bound by its attested `zen.Activated`;
- the armed coordinator's `Resume` disarms; a bound weave's refusal says its triggers write it.

DOES NOT MEAN
- that a stranger's `Quiesce` answers anything but `zen.Refused`: no `Quiesced` leaves.

PROVEN BY — `maker/weave.hpp` `Weave::arm`, `Weave::disarm`, `Weave::set_bound`,
`Weave::from_armed_coordinator`, `Weave::handle`, `Weave::refused_at_door`;
`maker/succession.hpp` `begin_schema_edit`; `tests/test_maker.cpp` case `"before the merge:
Quiesce and Resume are honoured only from the coordinator the host armed for this boundary; a
stranger's are refused by name and the weave keeps serving"`, case `"before the merge: Adopt is
honoured only by an unbound candidate and only from its coordinator; a bound weave refuses it by
name and its state stands"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## Do not assume

- That the interpreter is a `WeaveBase` (MW-WEAVE-02): it implements `loom::Weave` raw, so its
  doors are exactly the ones it answers, and it restates the reply rule in one place.
- That a behaviour edit re-consents the grant (MW-WEAVE-09): the grant minted at registration
  stands; every definition edit is a new build identity, and re-flooring is the identity
  phase's rule.
