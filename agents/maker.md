# Agent law — Maker (router)

Routed behind [`../AGENTS.md`](../AGENTS.md), for tasks touching `maker/`: a Loom weave built from
a maker's definition rather than from a class. The law lives in the registers under
[`maker/`](maker/): one law per `##`, an `MW-<AREA>-<NN>` id that is permanent, a `LAW` of one
line, `MEANS`, `DOES NOT MEAN`, a `PROVEN BY` naming the owner identifiers and the exact witness
cases, and a `WHY` naming one decision record under `decisions/`. Authority when they disagree:
tests > code > register > decision record. The whole of the law in one screen is
`grep -h '^LAW' agents/maker/*.md`.

Substrate law is the Loom's and applies whole: a maker weave is an ordinary `loom::Weave`, its
schema edit an ordinary prepared replacement, its conversion an ordinary operator in the
convention [`operators.md`](operators.md) states. What the Workshop will do with a definition —
the panel, the editor's gesture that registers one — is the panel phase's, not written here.

## Where the law is

| the task touches… | read |
|---|---|
| the two artifacts, the format tie, the namespace, the seven kinds, `required`, admission, the state reader | [definition](maker/definition.md) `MW-DEF` |
| registration, the accept-set, the pack, the write-back, the body's spend, the emit, inspection, the behaviour edit | [weave](maker/weave.md) `MW-WEAVE` |
| the schema edit: the ceremony, the four positions, the conversion edge, what the write refuses, an abort, the coordinator | [succession](maker/succession.md) `MW-SUCC` |

**Where the code is.** `maker/definition.hpp` (the schemas, `Definition`, encode, admit, the two
readers), `maker/write.hpp` (the default, the pack, the field-wise write), `maker/weave.hpp` (the
interpreter, registration, the behaviour edit), `maker/succession.hpp` (the coordinator, the
schema edit), `maker/vocabulary.hpp` (the five ceremony shapes), `maker/files.hpp` (the two files
on disk). A `// MW-` pointer sits above a declaration a law names.

**Where a case goes.** One suite, `maker` (`tests/test_maker.cpp`), authoring High-water as data
through `tests/maker_fixture.hpp`; the fresh-process witness is `tests/maker_author.cpp`.

## Ongoing rules

The seven rules of [`workshop.md`](workshop.md#ongoing-rules) apply to these registers unchanged,
with the family spelled `MW` and the pointer `// MW-… -- agents/maker/<file>`.

## Do not assume

- That High-water exists in C++. It is data authored by the suite through `SchemaBuilder`, the
  operator `Builder` and the composition wire form; the interpreter is the only class.
- That a definition edit is one thing. A behaviour edit with the schema unchanged is `swap_state`;
  a schema edit is a succession with an authored conversion, and the two are refused into each
  other's path by name.
- That a stale state file is converted at load. Reload is shape-only this phase; the edge a
  successor mounts is the seam, and the reader does not take it (MW-DEF-07).
- That the package is exported. It is in-tree only until the panel phase decides.
