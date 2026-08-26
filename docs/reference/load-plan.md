# The authored load plan

**Reference.** The plan file's format, the execution law, and what a failed artifact rolls
back. A maker's view is [choosing what a run is made of](../workshop/load-plans.md).

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

## Realization takes the host's own turns

**Loading a weave is a conversation, so a plan is not performed in one breath.** A provider mount
is a synchronous, host-native step and finishes where it stands. A weave load is a `zen.LoadWeave`
sent to the Weave Manager, and its answer comes back several deliveries later — through the
control door, which is where a loaded weave is *activated*.

So the host **begins** realizing the project and then goes back to being a host:

```text
begin the plan
    mount what can be mounted
    command the first weave load
    return

the ordinary host loop
    ...delivers everything, including that load's answer...

the answer arrives
    finish that row, command the next one, return
```

A row settles only when **its own** answer arrives, matched by correlation and by the bus-stamped
weave that was asked. Until then that row is *loading*, the rows after it have not been attempted,
and **the rest of the host is running normally** — messages are delivered, participants act, and
nothing is blocked waiting for a file to open.

**Authored order is unchanged by this.** Artifact *N+1* does not begin until artifact *N* has
settled: there is one conversation open at a time, no dependency solver, no reordering, no retry,
no parallelism and no deadline. An unanswered load stays unanswered rather than becoming a
refusal, because a refusal is something a layer actually said.

**A row is done when the load is answered — not when the artifact is registered.** The two are
different instants and the second is not the useful one: a weave the Kernel has loaded but that
has not been activated is registered and routable and never breathes. The answer is the fact that
implies the whole sequence.

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

## A row may be WAITING ON THE MAKER, and it is a BARRIER

An artifact a project intends to run may not be on this disk yet, because **this project is
where it gets built**. Refusing the plan over it — which is what *stops rather than skips* would
do — makes the one Workshop a maker could have built it in refuse to start.

So realization asks the **host**, per row, one question: *is this row waiting on the maker?*
The host answers it from two facts neither of which is realization's — whether the artifact file
is there (the host owns the rule that spells a stem as a file) and whether some authored
[build recipe](builder.md) can produce that stem. A yes means **realization stops at that row**;
nothing is mounted, opened or commanded for it, the rows behind it are not reached, and the
executor never learns *why* the answer was yes.

```text
zengine-workshop - waiting to be built: zengine-oven (build it, and its authored
                   participation is performed then -- every authored row after it is
                   waiting on this one)
```

- **A waiting row is a barrier, not a hole.** Authored plan order **is** realization order —
  that is this format's whole dependency model, and it is why there is no `after:` field and no
  solver. A walk that stepped over a row it could not perform would have replaced it with
  *eligibility* order: whatever happened to be on disk first. That is not a smaller promise,
  it is a different one, and it is wrong in a way the plan cannot express. An
  [overlay](#provider-mode) row authored **before** the ordinary
  provider it covers is a bad plan and the catalog says so; step that row over because its
  artifact is not built yet and the overlay arrives last, where it is *valid*. The absence of a
  file would have repaired an authored order the file still gets wrong.
- **Buildability is permission to wait, not permission to reorder.** What the host's answer buys
  is that a missing artifact this project builds does not refuse the Workshop a maker would have
  built it in. It buys nothing about order, because order was never realization's to decide.
- **It is not "skip what is missing".** An artifact that is not on this disk and that **nothing
  here can build** still refuses the plan by name, exactly as it did before. What changed is
  only the case where the project itself says how the file is made.
- **It is not build-on-missing.** Nothing starts a build, asks for one, or remembers to.
- **It is not a retry**, and nothing polls. A waiting row waits forever unless a maker asks for
  it; realization asks the host once, when it reaches the row, and never looks at a disk itself.
- **The plan has NOT completed.** `Complete` means every authored row settled. A plan stopped at
  a waiting row is *waiting*: not finished, and nothing refused — which the host reads as
  `ok == false` with an **empty refusal** and the waiting row **named**.
- **It is `pending` in the `Project` pane** — a fifth token beside `authored`, `loading`,
  `resolved` and `refused`. It publishes no resolved field at all, because nothing was done for
  it, and every row after it reads `authored`. At most one row is ever `pending`.

### Realizing the waiting row, later

`PlanExecutor::realize(stem)` performs **the row realization is waiting on**, with the same
three steps in the same order, at a moment a maker chose. In Workshop that is `Shift+b` in the
Builder pane; the fact reaches the realization owner as `builder::ArtifactBuilt` and its answer
comes back as `builder::ArtifactRealized`. When the row settles, the frontier moves on by one
and the walk resumes from exactly the next authored row.

Every eligibility rule is about the **authored plan**:

| refused when | because |
|---|---|
| a realization is already in flight | one is not interruptible, and queueing one would make this a scheduler |
| the row is already **resolved** | **this is where hot reload is refused** — nothing unloads, replaces or migrates, so an artifact already live is told so in words |
| the plan does not name the artifact | a build can produce a file; only the project's plan can say how it participates |
| it is not the row being waited on | authored order is realization order — a later row may be **built** now, and it participates when the rows in front of it have. The refusal names the row it is behind |

Nothing in that path consults a build, a recipe, a file or a timestamp; if the artifact is not
on disk the load refuses in the loader's own words exactly as it always would. An ineligible ask
changes **nothing at all** — no state, no row, and above all not the project, which a maker who
asked too early has not lost. And a refusal here does **not** fail the arrangement: the row's own
mount is rolled back, the frontier goes back to exactly where the ask found it so a corrected
build is a retry, and the host keeps running.

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

**A maker can see both halves side by side.** The `Project` pane
([reference/introspection.md](introspection.md)) pairs each authored row with what this run made
of it, labelled `authored` and `resolved` so the two are never one. It reads the plan for the
authored half — a resolved row does not know whether its mount was an overlay — and the executor's
rows for the other, and it keeps neither.

**Restart persistence exists; clean-build persistence does not.** A fresh process reconstructs
the same provider and weave arrangement from the same file with no source change between runs.
Recreating the artifacts themselves is **build intent**, it is a separate file with a separate
owner ([the Builder](builder.md)), and the only thing joining the two is the artifact stem — a
plan row carries no source path, package prefix, compiler flag or build tree, and a recipe
carries no role, mount mode or load order.
