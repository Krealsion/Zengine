# Sources — the catalog entries you can spend with nothing in hand

**Reference.** What a Source is, how to sample one, how to see what a sample would yield
*without* sampling it, and what a host is allowed to expose about itself.

A Zengine host holds one [operator](../../operator/catalog.hpp) catalog. Most of what is in
it takes arguments: `math.max(lhs, rhs)` is a rule you spend *on* something. Some of it does
not.

> **A Source is a catalog entry with no maker inputs. It is not a new kind of thing — it is
> a question you can ask of a definition you already have.**

```cpp
#include "operator/source.hpp"

op::is_source(*catalog.find("zengine.project.anchor"));   // true
op::is_source(*catalog.find("math.max"));                 // false
```

```text
Operator   one or more unbound maker inputs   evaluated on YOUR arguments
Source     zero unbound maker inputs          evaluated on its own subject
```

There is no `SourceDef`, no Source catalog, no Source ABI, no Source provider protocol and
no Source runtime. There is one store, one registration seam, one evaluator and one
predicate.

## The shape decides — never the name, never the technique

`is_source` reads the definition's input schema and nothing else. An identity spelled
`source.something` that takes an argument is an Operator; an identity spelled
`zengine.recipes.catalog` that takes nothing is a Source. Two producer forms qualify and
they stay as distinguishable as they always were:

| the definition | `is_source` | `is_composite` |
|---|---|---|
| a native body with no parameters | yes | no |
| a composite with every input bound to a constant or another node | yes | yes |
| a composite one binding short | **no** | yes |
| `math.max(lhs, rhs)` | no | no |

The third row is the useful one: binding an operator's last remaining input turns it into a
Source, with nothing re-registered and no second authoring surface. "Operator" and "Source"
are two readings of one store, which is why the store does not need to be two stores.

## Sampling one

```cpp
const op::Evaluation said = op::sample(catalog, "zengine.project.anchor");
if (said) {
    const std::string anchor = said.value().at(0)->as_text();
}
```

`sample` is a convenience over `Catalog::evaluate` and deliberately nothing more. It
resolves the identity in the catalog **at the moment you spend it**, builds the empty
argument pack from *that definition's own* input schema, and hands it to the one evaluator.
Both gate admissions still run — the pack going in, the answer coming out — and every
refusal is still worded by whoever detected it.

Without it, a caller has to `find` the definition first purely to obtain the empty schema
its pack must claim, which is ceremony with a wrong answer available at every step.

| what happened | what you read |
|---|---|
| nothing supplies that identity | `unresolved operator reference '<identity>'` |
| it is an Operator, not a Source | `'math.max' is an operator and not a source: it declares 2 inputs (lhs, rhs), which sampling supplies none of` |
| the body failed | `'<identity>' could not be spent: <its own words>` |
| the body answered the wrong shape | `'<identity>' produced an answer its own output schema refuses: …` |

The second row is a refusal rather than a default. Sampling never manufactures an argument
nobody wrote — a zero, an empty string, a "sensible" value — because an answer computed from
arguments nobody authored is an answer to a question nobody asked.

## What comes back is inert, and it makes no claim about later

A sampled answer is the ordinary schema-admitted `loom::Value` any operator evaluation
yields. It owns no authority, performs nothing by itself, is not a live binding and is not a
subscription. It says exactly one thing:

> this is what this Source returned when it was explicitly sampled.

There is no auto-sampling, no polling, no watcher, no dirty flag, no background refresh and
no cached "current value". A held answer does not move when the world does; a second sample
resolves current truth again. If you want to know whether something changed, sample again.

```cpp
const std::string first = sample_anchor();   // the world at that moment
// ...the owner changes...
const std::string again = sample_anchor();   // the world at this one
```

## Routing is not evaluation

Registration, mounting, `find`, enumeration, description, schema inspection, provenance
inspection and `is_source` itself all touch a **definition** and never a body. Knowing that a
Source exists, who supplies it and what it promises to yield costs zero evaluations —
running one to find out would be a side effect in a view.

An enumeration says so directly. The host's powers projection carries, per contribution:

```text
provider     who currently satisfies this identity
composite    is its body a graph over other identities?
source       could it be spent with nothing supplied?
output       the identity of the schema a sample would answer with:
             name, version, content id
```

`output` is an **identity, not a structure**: enough to say *which* shape comes back, and
deliberately not enough to decode one. Those three facts travel together because they are
compared together — a name alone cannot tell two different shapes apart.

## Sources and Senses are both honest, and they are not the same

Zen has two ways to obtain information synchronously, and the difference is who runs code
when you read:

```text
SOURCE   an explicit sample runs the evaluator NOW, on this call
SENSE    a read returns the owner's already-stored latest claim; no owner code runs,
         and the claim carries its own authorship and freshness
```

The owner *tells* you a Sense; you *ask* a Source. Neither substitutes for the other and
neither is being merged into the other. A Sense read may hand you something stamped a while
ago; a Source sample cannot be stale, because it did not exist until you asked.

## Sampling is not participation

A Source's body may compute, and it may read state the process already holds. What it may
not do is need somebody else to act. Sampling never loads or realizes anything, never asks
another weave to answer, never waits for a delivery or a frame, never schedules later work
and never writes anything durable.

> **If another participant must act for the answer to exist, it is not a Source from your
> seat — it is a message.**

## What a host may expose about itself

A host does not author operators: its catalog starts empty and its powers arrive from
[providers](operator-providers.md). That rule is about *manufacturing meaning*, and it has
one deliberate boundary rather than an exception:

> **A host may describe itself. It may not invent provider power.**

So a host may expose zero-input **Sources** over state or synchronous observations it
legitimately owns, and may not use that allowance to author parameterized application power,
provider behaviour, participation authority, or a hidden maker input dressed as a default.
The boundary is a mechanism and not a promise: the host's one door into its own catalog
judges every definition in the batch and refuses anything that would take an argument,
leaving the catalog exactly as it was.

`zengine-workshop` exposes two:

| identity | what it answers | whose state |
|---|---|---|
| `zengine.project.anchor` | the project-relative semantic anchor this Workshop was launched into, as `zengine.ProjectAnchor` | the host's own launch capture |
| `zengine.recipes.catalog` | which authored [recipe](builder.md) catalog is currently in force and how many completed recipes it holds, as `zengine.RecipeCatalog` | the session's current recipe owner |

Both **read their owner at the moment of the sample**, which is the whole difference between
a route and a copy: choose a different recipe catalog while Workshop is running and the next
sample reports the new one, with nothing re-registered. A choice that was *refused* leaves
the owner untouched, so it leaves the sampled answer untouched too.

Both preserve their owner's own absence rather than inventing something plausible. An empty
anchor means this process could not say where it was launched from; an empty catalog path
means no recipe catalog is in force, which is an ordinary state — a project with nothing to
build is a project.

## Two namespaces, kept apart

```text
zengine.project.anchor      which routable information source do I mean?
zengine.ProjectAnchor       what admitted meaning does a sample yield?
```

Sources are routed by **identity only**. There is no type-directed lookup anywhere in the
catalog, so two Sources whose answers happen to share a structure are not interchangeable
and cannot be substituted for one another — a schema is a gate, never a route. That is why a
project anchor and a browsing location would carry different schema *names* even if both were
one line of text: a schema's content id is computed over its name, so the gate separates them
everywhere, automatically.

## Exposure is deliberate

The catalog describes what a composition **chose** to make addressable, not everything that
happens to be true. Nothing is auto-wrapped: no field, getter, pane, preference, buffer or
schema becomes routable merely by existing, and a host does not become reflectable by
exposing two facts about itself. The clipboard's contents, an editor's unsaved buffer, a
maker's keymap and preferences, the session file and the substrate's grant ledger are all
real facts this process can reach, and none of them is a door.

## What this is not

No subscriptions, no watchers, no reactive propagation, no auto-sampling and no background
evaluation. No cached answers and no freshness state. No durable reference to a resolved
Source — a resolve is the concrete contribution found for one synchronous operation, and
holding one would be holding a copy of a truth the catalog re-decides. No visual authoring,
no dataflow graph beside the composite representation, and no code generation. Those are
other questions, and some of them are questions nobody has asked yet.

Read this beside [`operator/source.hpp`](../../operator/source.hpp) (the predicate and the
sample seam) and [`operator/catalog.hpp`](../../operator/catalog.hpp) (the one store and the
one evaluator). Where powers come from is [operator-providers.md](operator-providers.md);
how a loaded weave spends a host's is [operator-host.md](operator-host.md); what a maker sees
of a host's resolution is [introspection.md](introspection.md).
