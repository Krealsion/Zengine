# The maker weave — a weave from a definition

**Reference.** The two artifacts a maker weave is made of, what a trigger is, and the two ways a
live definition is edited. The package is `maker/`, header-only, in-tree; the substrate it spends
is the Loom's, unchanged.

Every other weave in this repository is C++: a shape for its state, a class for its handlers, a
build. A maker at the Workshop's editor has no compiler in the loop, so what they make is **data**
— and the maker package is the interpreter that registers one Loom weave per definition and runs
it as any native weave runs.

> **A definition is the maker's stable dotted name, a state schema, the shapes it accepts and
> emits, and its triggers — each one composition over the host's operator catalog, writing one
> named state field. The state is the maker's own value at its own schema.**

## The two artifacts

Both are native Zen bytes — an envelope with a mandatory content id — and never JSON.

**The definition** is a value of `zengine.maker.Definition v1`:

| field | kind | what it is |
|---|---|---|
| `format`, `format_version` | Text, Int | the word `zengine-maker-definition` and the version, inside the value and tied to the envelope's version; a file of another version is refused by its number before a field is read, and a value whose field disagrees with its envelope is a forgery |
| `name`, `revision` | Text, Int | the maker's stable dotted name (`hw`), which namespaces the maker's shapes, and the edit counter |
| `referenced` | List of `zen.SchemaDesc`, optional | every schema the state, the accepted and emitted shapes and the conversion nest, listed before anything that references it — the manifest's own section, through the manifest's own codec |
| `state` | `zen.SchemaDesc` | the state schema; its name must begin `<name>.` |
| `accepts`, `emits` | Lists of `zen.SchemaDesc` | the shapes delivered to the weave, and the shapes it publishes |
| `on` | List of `zengine.maker.On` | the triggers |
| `conversion` | `zengine.maker.Conversion`, optional | present on a schema edit's successor: the predecessor's state schema, how each successor field is written, and which predecessor fields are dropped |

**The state** is the maker's value at its own schema — `hw.State v1 { high : Int }` in its own
envelope, no wrapper. It is read back only at that schema: a state of another version is refused
by name, and nothing converts a state file in this phase.

There is no author, signature or provenance field of any kind. A later identity is a v2 wrapping
this v1 as a nested message, with one conversion edge.

### The seven kinds close the maker path

A definition's shapes are the seven Loom kinds and no eighth. A keyed table is a list of entry
messages; a one-of is several optional fields; optionality is the field's `required` bit.
`required` is the default. An optional state field is admitted only where no trigger binds it,
and it is absent in the default state.

## A trigger

```text
zengine.maker.On v1
  message_name, message_version   an accepted shape
  body                            zengine.OperatorComposition v1 — the operator seam's wire form
  output                          a state field
  emit                            a list of { emitted shape, fields : how each is written }
```

When the message arrives the host builds the **pack** — the state's fields in declared order,
then the message's — and spends the body through the host's one catalog. Every operator is
resolved at spend: a power overlaid underneath moves the trigger, and revealing it moves it back.
The body answers on one port whose type is the target field's own, so the catalog's output gate
is the kind check, and the answer lands in the named field. Then each emit is written field by
field from the new state — a same-named or renamed state field, or a scalar constant — and
published under the weave's own grant. A body that cannot be spent leaves the state, answers
`zen.Refused` with the deepest layer's words, and counts.

A field name both the state and the message carry is refused at admission, as are a trigger on an
unaccepted message, an output the state does not declare, an undeclared emit, and an emit or
conversion the write cannot plan from the schemas alone.

## Registration and inspection

`register_definition` mounts the revision's bodies under `zengine.maker.<name>.r<revision>`,
constructs the weave at its default state, mints a grant from the emits, and registers it bound
to the name as its role. The first snapshot claims the data-built state schema, so the registry
resolves `hw.State v1` by name. The accept-set is the definition's shapes plus the doors every
maker weave answers: the four poke doors, `zen.Activated`, and the package's own ceremony shapes.

`zen.PokeDescribe` names the state schema and every field; `zen.PokeRead` reads a scalar;
`zen.PokeWrite` and `zen.PokeResetState` are refused by name — a maker weave's state is written by
its triggers.

## The two edits

**A behaviour edit** keeps the state schema. `apply_behaviour_edit` mounts the successor
revision's bodies beside the incumbent's, hands the live weave its new definition, unmounts the
old bodies, and calls `swap_state` — same WeaveId, incarnation bumped, `Revived` announced, state
kept. A definition whose state schema differs is refused here: that is a schema edit.

**A schema edit** is a succession — a prepared replacement, exactly as the Loom's handoff garden
performs one, with the conversion authored as data:

1. the maker authors the successor: revision +1, the new state schema, a `conversion` from the
   predecessor's state;
2. `begin_schema_edit` registers the candidate unbound, seals it to the coordinator, begins the
   transaction around it, and sends `Quiesce` to the incumbent — the FIFO boundary;
3. the incumbent quiesces and answers its final value, exact because nothing further changes it;
4. the coordinator spends the conversion: it was mounted at the successor's registration as the
   conventional edge `zengine.migrate.hw.State.v1-to-v2`, so the incumbent's bytes are admitted at
   the predecessor's schema by identity and the answer at the successor's — or the edge refuses,
   the transaction aborts, `Resume` reaches the incumbent, and no candidate is touched;
5. `Adopt` is the transaction's one preparation ask; the candidate admits the converted bytes at
   its own state schema and answers `Adopted` for itself;
6. the host commits and pumps; the role moves, the successor is told `zen.Activated`, and the host
   retires the predecessor and writes the two files anew.

A `hw.Sample` sent during the edit is handled before the boundary, refused by name after it while
the incumbent holds the role, and handled by the successor after the role moves — never lost.

### What the conversion may say

Each successor field is written from one source: a same-named or renamed predecessor field of the
same kind, or one scalar constant. Every predecessor field is copied or named in `drops`; loss is
authored, never inferred. The schemas are judged at admission — a target the shape lacks, a source
the shape lacks, a kind mismatch, two sources, a constant a field cannot hold, a required target
with neither. One refusal waits for the value: a required target whose source is an optional field
that happens to be absent.

## What is deliberately not here

- No `hw.*` in C++: the forcing case is authored in the suite through `SchemaBuilder`, the
  operator `Builder` and the composition wire form, and the fresh-process case reads the two
  files back in another process.
- No export: the package is in-tree until the panel phase, which decides.
- No conversion of a stale state file at load: the edge exists in the catalog once a successor
  is registered, and the reader does not take it in this phase.
- No panel: showing a maker weave on the Workshop's screen is the next phase.
