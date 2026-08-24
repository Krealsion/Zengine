# The authored load plan

Which artifacts participate in a project, and how. One durable file, read at startup and
executed in the order it is written.

> **Adding a native artifact to a load plan is an execution-authority decision.**
> A provider row means *allow this native artifact to contribute executable semantic power to
> the host*. A weave row means *allow this native artifact to participate as a Loom weave under
> this role*. This file is explicit precisely so that choice is visible, reviewable and
> diffable. It is not harmless configuration, and nothing in this phase signs, verifies or
> restricts it.

Read beside [`workshop/load_plan.hpp`](../../workshop/load_plan.hpp) (the typed plan and its
law), [`workshop/load_persist.hpp`](../../workshop/load_persist.hpp) (the one codec) and
[`workshop/load_execute.hpp`](../../workshop/load_execute.hpp) (the executor). What a provider
IS and what an operator host IS are [reference/operator-providers.md](operator-providers.md) and
[reference/operator-host.md](operator-host.md).

## The law

```text
An artifact is ONE authored project participant which may expose ZERO OR MORE runtime surfaces.

The project DECLARES participation.  The host PERFORMS it.

LIST, not SCAN.

Order may be authored explicitly before Zen earns dependency solving.

The plan records INTENT; the runtime still discovers what the artifact actually provides.
```

## One record, two optional surfaces

`zengine-timer` is one shared library that participates in two ways: it **supplies**
`timer.normalize_delay`, and it **constructs** the `zengine.timer` weave. Before LOAD-0 the host
held two independent hard-coded lists and the Timer was in both — which could only be maintained
so that the two happened to agree, with the ordering law between them living in a comment.

It appears **once**:

```json
{ "artifact": "zengine-timer",
  "provider": [ { "mode": "normal" } ],
  "weave":    [ { "role": "zengine.timer" } ] }
```

Provider-only (`zengine-operators-basic` is not a weave at all, and mounting it must never make a
Kernel go looking for one):

```json
{ "artifact": "zengine-operators-basic",
  "provider": [ { "mode": "normal" } ],
  "weave":    [] }
```

Weave-only:

```json
{ "artifact": "zengine-composer",
  "provider": [],
  "weave":    [ { "role": "zengine.composer" } ] }
```

**The two surfaces stay separate optional intentions even for an artifact that supports both**,
because *load this participant* and *let this artifact change the host's semantic world* are
different authored decisions. An artifact declared only as a weave does **not** get its provider
surface mounted, even if it exports one; an artifact declared only as a provider is **not**
loaded as a weave, even if it is one. A row requesting neither surface is refused.

## Execution

```text
for artifact in AUTHORED ORDER:
    if provider intent:  mount it                      (op::mount_provider, PROV-0)
    if weave intent:     offer this host's operators   (op::OperatorOffer, OPH-0)
                         load the weave                (zen.LoadWeave -> Weave Manager)
                         withdraw the offer
```

Two orderings, and they are different kinds of fact.

**Between artifacts the order is authored policy.** Nothing infers that one artifact's
composition spends another's primitives. A person writes the rows in the order they must happen,
and the host executes that order. There is no dependency solver, no reordering and no retry.

**Within one artifact the order is semantic law** and is not the plan's to state. A
provider+consumer artifact validates the rule it is about to spend inside its own `create()`
(CAT-0), and `create()` runs several deliveries below the command that starts the load. So the
contribution must be in the catalog before the artifact that needs it is built. A file that could
say *weave, then provider* would be a file that could author a Timer whose semantics depend on
which load happened to be in flight.

## Provider mode

`normal` and `overlay` are `op::MountMode::Ordinary` and `op::MountMode::Overlay` — PROV-0's own
two values and its only spelling of covering a power. An ordinary collision still refuses; a
signature-incompatible overlay still refuses. What LOAD-0 adds is that the intent is now
**durable**: a deliberate semantic substitution can be authored project arrangement rather than
ad hoc runtime test code, and it survives a restart.

## The operator offer

Every weave load is bracketed by an offer of the host's operator resolution, generically — an
ordinary weave need not know operator hosting exists. Three outcomes:

| outcome | what it means | what the executor does |
|---|---|---|
| not a consumer | an ordinary weave; the image exports no consumer surface | load it |
| offered | the artifact took the host's resolution for this one load | load it |
| a failed handoff | the image **does** export a consumer surface and the handoff did not complete | **refuse the artifact** |

The third is CAT-0's correction. A host-sensitive artifact loaded under a failed handoff is not
the same as one that was never offered anything: a Timer falls back to a local catalog when
nothing was offered, so loading it anyway would silently swap the process's semantic authority
for the image's own copy — invisible in every answer until the two disagree.

An image the offer could not **open** at all is not refused here: the load owns that sentence, so
a plan naming an artifact that is not on this disk is refused in the loader's own words.

## Failure, and what it leaves behind

A refusal names **which artifact**, **which participation step**, and **why** — the `why` being
the deepest layer's own sentence, quoted rather than reworded:

```text
artifact 'zengine-timer': provider mount refused: 'math.max' is already supplied by ...
artifact 'zengine-composer': weave load refused: open failed: ... No such file or directory
```

**One artifact is the atomic unit.** A record whose provider mounted and whose weave then failed
unmounts its own contribution before reporting: an artifact that is not participating leaves
nothing behind that says it is. A record whose provider mount failed does not go on to attempt
its weave.

**A transaction across the whole plan was deliberately not built.** Artifacts that succeeded
before the failure stay, and the host says how many did. Execution stops at the first refusal:
carrying on would mean running a project nobody authored while reporting the one that was asked
for.

**The durable plan is never rewritten to agree with a failed runtime.** Authored intent and
resolved state are different truths — the same law an unresolvable `PaneRef` already lives under.
Unresolved intent remains intent.

## Where the plan lives

| | |
|---|---|
| shipped default | `workshop/default-load-plan.json`, staged beside the host binary |
| shipped graphical | `workshop/graphical-load-plan.json`, staged only where both SDL artifacts were built |
| default lookup | `<directory of the executable>/default-load-plan.json` |
| override | `--load-plan <path>`, used exactly as given |
| missing | refused by path, and the host exits without mounting or loading anything |

There is **no compiled-in fallback plan**. A host that could manufacture an arrangement when the
file is missing would make the file decorative, and would tell a maker their project loaded when
what actually ran was the host's own opinion of one.

`--load-plan` replaced `--skin` and `--input`. Those flags existed because *building* a Skin and
*choosing* one are different acts, and choosing one by editing a literal in the host is an
experiment nobody else can repeat — which is exactly what a plan file makes repeatable. The
graphical arrangement is therefore a second shipped plan; it differs from the default in exactly
the two rows that name the medium and the reader, and presentation and input remain two
independent rows rather than one deduced from the other.

## Artifact names, and why they are stems

A row names a **stem**: `zengine-timer`, not `zengine-timer.so` and not a path. The host owns the
one rule that turns a stem into a file — a directory, a separator and the platform's suffix
(`HostContext::so`) — which is what makes one plan legal on Linux and on Windows with no platform
matrix, no per-OS field and no package locator. Neither shipped plan contains `.so`, `.dll` or a
path separator, and a test asserts it.

A stem carries no path separator and no `..`. That is an authority rule rather than tidiness: a
stem that could climb out of the host's artifact directory would make *which files may run in
this process* a question about the plan's text rather than about the directory the host was
deployed into.

## The file format

JSON through the Loom's compat codec — the same gate the live bus uses, so an unknown field, a
field of the wrong kind, a bad integer or invalid UTF-8 is refused by the codec rather than by
something hand-written. One codec; no JSON is read anywhere else in Workshop's startup.

```json
{
  "zen": 1,
  "schema": "WorkshopLoadFile",
  "version": 1,
  "fields": {
    "format": "zengine-workshop-load-plan",
    "format_version": "1",
    "artifacts": [ ... ]
  }
}
```

Two things a hand-author needs to know:

- **`content_id` is optional on the way in.** The canonical writer emits one; a file written by
  a person may omit it. If it is present it is checked against the schema.
- **An integer is written as a quoted string** (`"format_version": "1"`). That is the codec's
  encoding, not this format's choice.

Whitespace and indentation are free — the shipped plans are indented for reading. Re-serializing
a loaded plan produces the canonical one-line form, and a second write of a loaded plan is
byte-identical to the first.

Refused, with the reason: another `format_version` (by its number, before the rows are judged);
a `format` that is not this one; more than one provider or weave surface on one artifact; an
unrecognised mode word (naming both what was found and what would have worked); a weave
declaration with no role; an artifact requesting neither surface; a stem that is empty, too long,
spaced, or traversing; the same stem declared twice; a file larger than a plan can be.

## What is deliberately not here

No version constraint, no dependency, no hash, no description, no trust level, no capability
list, no restart or reload policy, no platform matrix, no metadata — and no **operator
identity**. `math.max` and `timer.normalize_delay` belong to the providers that supply them; a
plan that copied them into the project file would be a host authoring semantics again, one
indirection further out. What is written down is *mount this provider artifact*, never *these are
the powers it has*.

The plan holds no resolved truth either: no `WeaveId`, no mounted provider identity, no
contribution count, no outcome. Those exist only at runtime, in the executor's
`ResolvedArtifact`, and are never written back.

**Restart persistence exists; clean-build persistence does not.** A fresh process reconstructs
the same provider and weave arrangement from the same file with no source change between runs.
Recreating the artifacts themselves from a clean tree is build intent and is a separate,
unstarted question.
