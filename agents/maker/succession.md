# Maker law — the succession

Register `MW-SUCC`: a schema edit as a prepared replacement with an authored conversion. One law
per heading; cite by ID. Router: [`../maker.md`](../maker.md). The substrate's own laws
(replacement, handoff, answer authority) are the Loom's and apply whole.

## MW-SUCC-01 — A schema edit is a succession

LAW — A definition whose state schema differs is a successor: registered, sealed, prepared around, quiesced into, converted for, asked to adopt and committed; the role moves and the host retires the old.

MEANS
- the candidate admits the bytes at its own schema and answers `Adopted` through the answer door;
- commit is the host's decision and it schedules; the outcome is taken, never assumed.

DOES NOT MEAN
- that anything in the Loom changed: every call is one the handoff garden already makes.

PROVEN BY — `maker/succession.hpp` `begin_schema_edit`, `Begun`, `Coordinator`;
`maker/weave.hpp` `Weave::adopt`; `maker/vocabulary.hpp` `Quiesce`, `Quiesced`, `Adopt`,
`Adopted`; `tests/test_maker.cpp` case `"FC-7: a schema edit is a succession -- v2 authored with
its conversion, prepared, adopted, committed; the role moves, high is still 7, label reads high
water, the predecessor is gone"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## MW-SUCC-02 — Four positions, never lost

LAW — A message sent during a schema edit is handled before the boundary, refused by name after it while the incumbent holds the role, and handled by the successor once the role moves — never lost.

MEANS
- the boundary is `Quiesce`, an ordinary message; the final value is exact, nothing moves it;
- `Resume` un-quiesces an incumbent whose edit aborted.

PROVEN BY — `maker/weave.hpp` `Weave::handle`, `Weave::refused_after_boundary`,
`Weave::quiescing`; `maker/vocabulary.hpp` `Quiesce`, `Resume`; `tests/test_maker.cpp` case
`"FC-7: a hw.Sample is handled before the boundary, refused by name after it while the incumbent
holds the role, and handled by the successor after the role moves -- never lost"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## MW-SUCC-03 — The conversion is data in the successor, mounted as an edge

LAW — The conversion is a field of the successor's definition, mounted under its provider as the edge `zengine.migrate.<state>.v<from>-to-v<to>`, and spent by the coordinator with `op::migrate`.

MEANS
- the edge's input is the predecessor's schema by identity; its output, one port of the successor;
- no Message-constructing operator joins the vocabulary; the body is the field-wise write.

DOES NOT MEAN
- that the migrator is a weave or the candidate's code: a catalog identity, versioned with v2.

PROVEN BY — `maker/weave.hpp` `definitions_of`; `maker/succession.hpp`
`Succession::on_quiesced`; `maker/definition.hpp` `Conversion`, `conversion_schema`;
`tests/test_maker.cpp` case `"FC-7: a schema edit is a succession -- v2 authored with its
conversion, prepared, adopted, committed; the role moves, high is still 7, label reads high
water, the predecessor is gone"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## MW-SUCC-04 — What the write refuses, and a refused conversion reaches no candidate

LAW — The write refuses a target or source the shape lacks, a kind mismatch, two sources, a constant a field cannot hold, a required target with neither, and an edge that neither copies nor drops a field.

MEANS
- the schema half is planned at admission; the value half waits for the edge;
- a refused edge aborts, sends `Resume`, records the reason, and asks nothing of the candidate;

PROVEN BY — `maker/write.hpp` `plan_fields`, `write_fields`, `FieldSource`, `Written`;
`maker/succession.hpp` `Succession::on_quiesced`; `tests/test_maker.cpp` case `"f: the
field-wise write refuses a target with no source, a source the schema lacks, a kind mismatch,
two sources, a constant of a non-scalar kind, and a predecessor field neither copied nor
dropped"`, case `"f: a conversion the write refuses reaches no candidate -- the transaction
aborts, the incumbent is resumed and is still the service"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## MW-SUCC-05 — An aborted succession discards the candidate and resumes the incumbent

LAW — `abort_schema_edit` aborts the transaction, which discards the sealed candidate and its bodies with it, and sends `Resume` to a quiesced incumbent, which is the service again with its state.

PROVEN BY — `maker/succession.hpp` `abort_schema_edit`; `tests/test_maker.cpp` case `"e: an
aborted succession discards the sealed candidate and leaves the incumbent the service with its
state"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## MW-SUCC-06 — The coordinator is one host weave, generic over definitions

LAW — `Succession` is a native weave the host registers once, holding the host's handle; it speaks only the five ceremony shapes and carries state as bytes, so one coordinator serves every definition.

MEANS
- the host owns the handle and the pump; the coordinator owns the conversation.

DOES NOT MEAN
- that the coordinator commits: readiness commits nothing; a handle going out of scope, nothing.

PROVEN BY — `maker/succession.hpp` `Succession`, `register_succession`, `Succession::begin`;
`maker/vocabulary.hpp` `Quiesced`, `Adopted`; `tests/test_maker.cpp` case `"FC-7: a schema edit
is a succession -- v2 authored with its conversion, prepared, adopted, committed; the role
moves, high is still 7, label reads high water, the predecessor is gone"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## Do not assume

- That the conversion's `from` is checked against the live predecessor at admission
  (MW-SUCC-04): it is checked by the gate when the edge is spent, which is where the live bytes
  are.
- That the predecessor leaves on commit (MW-SUCC-01): the host retires it, and its bodies leave
  the catalog with it.
