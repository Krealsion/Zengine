# Maker law — the definition and the state

Register `MW-DEF`: the two artifacts a maker weave is made of, and what admits one. One law per
heading; cite by ID. Router: [`../maker.md`](../maker.md).

## MW-DEF-01 — Two native files, one format tied

LAW — A maker weave is two native files: the definition at `zengine.maker.Definition v1`, its format tied to the envelope, and the state as the maker's Value at its own schema, no wrapper.

MEANS
- a definition of another envelope version is refused by its number, before a field is read;
- a `format_version` disagreeing with its envelope is a forgery; another word is another file;
- the state file has no wrapper: its envelope is `hw.State v1` itself, content id and all.

DOES NOT MEAN
- that either file is JSON; the compat codec is never written and is refused on read;
- that a later identity breaks the format: it is a v2 wrapping this v1 as a nested Message.

PROVEN BY — `maker/definition.hpp` `kFormat`, `kFormatVersion`, `kDefinitionSchemaVersion`,
`definition_schema`, `definition_bytes`, `read_definition`; `maker/files.hpp` `read_file`,
`write_file`; `tests/test_maker.cpp` case `"b: a definition claiming another version is refused
by its number, and one whose own version field disagrees with its envelope is a forgery"`, case
`"FC-8: the definition and the state are two native files written by one process, and a fresh
process reads them back with high == 7"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-DEF-02 — The name namespaces the state

LAW — A definition's name is the maker's stable dotted name, and its state schema's name begins `<name>.`; a state outside that namespace is refused naming the prefix it needed.

MEANS
- two makers' `State v1` cannot collide at the registry's claim, because neither is `State`;
- the accepted and emitted shapes are the maker's to name; a maker may accept another's shape.

PROVEN BY — `maker/definition.hpp` `admit_definition`, `Definition`; `tests/test_maker.cpp`
case `"FC-2: a state schema outside the definition's namespace is refused, naming the prefix it
needed"`, case `"FC-2: the state schema is built from data, the registry resolves it by name,
and its content id is the descriptor's"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-DEF-03 — The seven kinds close the maker path

LAW — A definition's shapes are the seven Loom kinds and no eighth: a keyed table is a list of entries, a one-of is several optional fields, and a nested Message or List rides `referenced`, post-order.

MEANS
- the encoder collects the closure post-order; admission decodes it before what names it;
- the default state of a nested field is the nested schema's own default; of a list, empty.

DOES NOT MEAN
- that a kind may be appended free: an older reader refuses a kind out of range.

PROVEN BY — `maker/definition.hpp` `encode_definition`, `admit_definition`; `maker/write.hpp`
`default_value`, `default_cell`; `tests/test_maker.cpp` case `"2: a definition whose state nests
a message and a list decodes through its referenced section -- the seven kinds, closed"`.
WHY — `agents/decisions/the-seven-kinds-close-the-maker-path.md`

## MW-DEF-04 — Required is the default; optional only where no trigger binds it

LAW — `required` is a field's default on the maker path as on the shape path; an optional state field is admitted only where no trigger binds it, and it is absent in the default state.

MEANS
- a bound optional field would refuse at spend as `no input named`; admission refuses it first;
- the pack keeps each field's `required` bit, so an absent optional field packs as absent.

PROVEN BY — `maker/definition.hpp` `admit_definition`; `maker/write.hpp` `default_value`,
`pack`; `tests/test_maker.cpp` case `"3: an optional state field bound by a trigger is refused
at admission; an unbound optional field is admitted and absent in the default state"`.
WHY — `agents/decisions/the-seven-kinds-close-the-maker-path.md`

## MW-DEF-05 — No author field

LAW — The definition schema carries no author, signature or provenance field of any kind; the suite reads the schema for the field and `maker/definition.hpp` for the token.

MEANS
- a declared, unsigned name would be a claim nothing verifies; identity is a later phase's v2.

PROVEN BY — `maker/definition.hpp` `definition_schema`; `tests/test_maker.cpp` case `"b: the
definition schema carries no author field, and the file says so"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-DEF-06 — Admission refuses by name

LAW — Admission refuses in one sentence naming the trigger, message or field: an unaccepted message, an undeclared output, a pack name both carry, an undeclared emit, a write it cannot plan.

MEANS
- the conversion is planned against its own `from`; the value half waits for the edge;
- a body's operators are not resolved at admission: the catalog is asked at spend (MW-WEAVE-05).

PROVEN BY — `maker/definition.hpp` `admit_definition`; `maker/write.hpp` `plan_fields`;
`tests/test_maker.cpp` case `"a: a definition is refused when an on names an unaccepted message,
an unknown output field, or an emit field with no source"`, case `"FC-4: the pack is state then
message, and a field name both carry is refused at admission"`.
WHY — `agents/decisions/the-makers-state-is-a-first-class-loom-schema.md`

## MW-DEF-07 — Reload is shape-only this phase

LAW — A state file is read at the definition's state schema and nothing else; another version is refused by name and nothing converts it this phase — the seam is `op::migrate` on a successor's edge.

MEANS
- the arm is one call over the successor's catalog, not taken until a phase exercises it.

DOES NOT MEAN
- that the edge is absent: a registered successor's edge resolves, and the reader still refuses.

PROVEN BY — `maker/definition.hpp` `read_state`; `tests/test_maker.cpp` case `"b: a state file
of another version is refused by name at load, and nothing converts it"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## MW-DEF-08 — Emits are namespaced too

LAW — Each emitted shape's name begins `<name>.`, in the state's own words; a definition cannot speak the package's ceremony shapes or another maker's, and accepts stay free.

MEANS
- every maker weave accepts `Quiesce`, `Resume` and `Adopt`; an emit of one would reach them all;
- a weave listens to what others say: an accepted shape may be anyone's.

PROVEN BY — `maker/definition.hpp` `admit_definition`; `tests/test_maker.cpp` case `"before the
merge: a definition whose emits lie outside its namespace is refused, and so a definition cannot
emit the ceremony shapes"`.
WHY — `agents/decisions/a-schema-edit-is-a-successor.md`

## Do not assume

- That the definition's `accepts` are namespaced as the state is (MW-DEF-02): only the state
  must begin `<name>.`, because only the state is claimed by the registration alone.
- That `read_definition` converts anything (MW-DEF-01): a definition of another version is the
  maker's to re-save; the reader names the number and stops.
