# Operator providers — where a host's powers come from

**Reference.** How an artifact supplies operator definitions to a host, why a provider need not
be a weave, and how one power may be deliberately shadowed and then revealed again.

A Zengine host holds an [operator](../../operator/catalog.hpp) catalog: named,
typed, composable semantic rules that more than one surface has to agree about.
[operator-host.md](operator-host.md) is about how a **loaded consumer** spends
what is in that catalog.

This page is about the other end: **how anything gets into it.**

> **Providers own implementations and compositions. Hosts own their lifetime and
> current resolution. Consumers own neither.**

It is a seam, not a plugin system. It carries operator definitions and nothing
else — no lifecycle, no discovery, no dependency solving, no versions to resolve.

## What was wrong before

Until PROV-0 a host filled its catalog by *calling a package's authoring
function*:

```cpp
op::Catalog operators = timer::standard_operators();   // gone
```

That reads like configuration and is not. It compiles one artifact's semantic
vocabulary into the host: every power the running system has is a power the
host's own translation unit contains. "A newly loaded Timer brings different
delay semantics" was not a hard problem in that arrangement — it was
inexpressible.

## What a provider writes

```cpp
#include "operator/provider.hpp"

std::vector<zengine::op::OperatorDef> my_powers() {
    std::vector<zengine::op::OperatorDef> defs;
    defs.push_back(zengine::op::make_operator<&max_int>("math.max", {"lhs", "rhs"}, "result"));
    return defs;
}

ZENGINE_OPERATOR_PROVIDER("my.provider", my_powers)   // namespace scope, once
```

That is the whole opt-in. It exports one optional symbol saying *this image
supplies these definitions*; a host that knows nothing about providers never
looks for it, and an artifact that never writes it is an ordinary artifact
forever.

**`"my.provider"` is the PROVIDER's identity, not an operator's.** `math.max` is
`math.max` whoever supplies it — which is exactly what lets somebody else supply
it later without every composition that names it being rewritten. The provider
identity is what a host mounts, unmounts, and reports as the active supplier of a
power.

### A provider is not a weave

`zengine-operators-basic` exports `zengine_operator_provider` and **no
`zen_weave_abi` at all**. No Kernel loads it, it gets no WeaveId, no role, no
grant, no manifest and no row in `zen.ListLoaded`, and it never sees a bus. The
host opens the file itself, resolves one symbol, and reads definitions out of it.

The Timer artifact, next door, exports **three** symbols and is a provider, a
consumer and a participant at once. Those are three independent relationships:

```text
zen_weave_abi                 this image can BE the Timer participant
zengine_operator_provider     this image SUPPLIES timer.normalize_delay
zengine_operator_consumer     this image can SPEND a host's operator truth
```

Build a provider-only artifact with `zengine_provider()` rather than
`zengine_weave()`. The difference on the link line is `loom::switchboard`, and
its absence is the claim: there is no participant in the image to send or receive
anything.

## Native and composite contributions are different

A provider contributes two fundamentally different kinds of thing, and the
difference is what makes replacement propagate at all.

| | what lives in the provider | what crosses | who evaluates it |
|---|---|---|---|
| **native** | the implementation | the contract | the provider, by index, while its image is held |
| **composite** | nothing at run time | the **graph** | the host's own evaluator |

A composite crosses as **structure**. Its nodes still say `math.max` on the far
side, and the host resolves those identities against whatever currently supplies
them, at every spend.

```text
timer.normalize_delay        supplied by  zengine.timer
    floor_zero = math.max(delay_ms, 0)         \
    floor_one  = math.max(floor_zero, 1)        >  supplied by zengine.operators.basic
    effective  = logic.select_int(repeat, ...) /
```

If a composite crossed as an opaque callback into its own image instead, the
provider would be evaluating its own private graph — and a power replaced
underneath could never propagate through it. That is not a detail of the
encoding; it is the reason the encoding exists.

## What a host writes

```cpp
op::Catalog operators;                                    // authors NOTHING
op::mount_provider(operators, dir + "/zengine-operators-basic.so");
op::mount_provider(operators, dir + "/zengine-timer.so");
op::OperatorHostSurface operator_host(operators);         // BEFORE the Kernel
loom::Kernel kernel(bus);
```

Four lines, after which the host's resolution carries `math.max`,
`logic.select_int` and `timer.normalize_delay` — and the host's own translation
unit contains not one of them.

**The mounts come before the offer, and that ordering is load-bearing.** A Timer
that is offered a host *validates* the rule it is being asked to spend, inside
its own constructor. The contribution therefore has to be in the catalog before
the artifact that needs it is created:

```text
mount the provider surface from the timer artifact
    -> timer.normalize_delay enters host resolution
create the OperatorOffer over that now-complete catalog
    -> ordinary zen.LoadWeave
        -> Kernel::load -> create() -> the Timer validates the rule and finds it
withdraw the offer          (the load handoff is over)
the provider mount REMAINS  (its contribution is still installed)
```

An offer is scoped to one load. **A mount is not**: it lasts as long as its
contributions are installed, and it holds the artifact's image open for exactly
that long.

## Layering: active, shadowed, and reversible

An identity does not hold a definition. It holds the **stack of contributions**
eligible to satisfy it, and the last one is active.

```text
math.max
    active     zengine.operators.test.min     <- what find() answers
    shadowed   zengine.operators.basic        <- still resident, still here
```

**Shadowing is intentional.** Two ordinary providers of one power is an ambiguity
nobody authored, so it is refused — load order, filesystem order, lexical order
and map iteration are not policy. Covering an existing contribution takes an
explicit overlay:

```cpp
op::mount_provider(operators, path, op::MountMode::Overlay);
```

**...and only where the ports agree.** An overlay must be structurally the
contract existing compositions were authored against — both port schemas compared
by `loom::same_identity`. `Int -> Int` may not be covered by `Text -> Bool`: that
is a different power wearing the same name.

**Shadowing covers; it does not replace.** The contribution underneath stays
resident, as the same object. So:

```cpp
operators.unmount("zengine.operators.test.min");
```

reveals what was there — not a rebuild of it, and with no re-run of anybody's
authoring. That reversibility is the whole point of the layering.

**And a maker can see all three states.** The `Powers` pane
([reference/introspection.md](introspection.md)) projects this same store: one block
per logical identity, active first, with every shadowed contribution under it. It
derives at every reading rather than keeping a copy, so an overlay mounted or
unmounted at run time is in the next reading with nobody having been notified.

If nothing remains underneath, the logical operator becomes **unresolved**, and
the next evaluation says so by name. Nothing is manufactured to fill the gap.

## Resolution happens at spend

A composite node retains an operator **identity** and the two `ContentId`s it was
authored against. It never retains a provider, a provider-local index, a callable
or a resolved definition.

So this needs no rewrite, no rebinding pass and no notification to anybody:

```text
function.1 -> function.2 -> function.3

    provider B overlays function.3

next evaluation:
    function.1 -> resolve function.2 -> resolve function.3 -> provider B
```

`function.1`'s graph does not even mention `function.3`.

## Custody: what keeps a native contribution callable

```text
ProviderRecord      identity + the C table + ONE SHARE OF THE IMAGE
    held by         every native contribution's callable, and by the catalog,
                    until the provider is unmounted
```

Nothing pointing into a provider image may outlive it. The callable is a closure
over the whole **record** — not over the image, and not over a raw function
pointer — because a hold that covered the image but not the record dangles the
moment the provider is replaced.

Unmount therefore has one order, and the catalog keeps it:

```text
1. stop selecting the provider's contributions   (they leave the store)
2. drop the contribution records                 (the callables become unreachable)
3. release the provider image                    (nothing can call into it now)
```

No statement sequences that. A refcount does.

Do mount and unmount **between** evaluations. Zen is single-threaded here and
nothing in this seam is arranged for anything else.

## How it fails

| what happened | what a host reads |
|---|---|
| the file is not there | `could not open '<path>' to look for operators` |
| an ordinary artifact | `'<path>' exports no operator provider surface` |
| another era's surface | `offers operator provider surface v2; this host speaks v1` |
| an ordinary collision | `'math.max' is already supplied by '<provider>'; mounting '<other>' over it needs an explicit overlay` |
| an incompatible overlay | `would shadow 'math.max' at a different signature (...) than '<provider>' supplies (...)` |
| the provider's authoring failed | `provider '<name>' in '<path>' contributes nothing` |
| a missing dependency at spend | `'timer.normalize_delay' step 0: unresolved operator reference 'math.max'` |
| the provider could not answer | `'math.max' could not be spent: <the provider's own words>` |

Every one of them leaves the catalog exactly as it was. **A mount is all or
nothing**: every contribution in a batch is judged before any of them is
installed.

## Cost

Release, GCC 11.4, one machine, measured over 20,000 evaluations each.

```text
math.max, definition published in-process        136 ns
math.max, definition supplied by a PROVIDER      846 ns
timer.normalize_delay, all in-process            655 ns
timer.normalize_delay, ACROSS two providers     2858 ns

prov.function.1  (2 composites over 4 native crossings)
    baseline                                    3759 ns
    with an overlay ACTIVE                      3715 ns
    after the overlay was REMOVED               3764 ns
```

Two things to read out of that.

**Layering is free.** Choosing which contribution is active is a map lookup and a
`back()`; the three chain rows are the same number three times.

**A native crossing costs about 700 ns**, and it is representation rather than
design — serialize, cross, parse, admit, and the same again for the answer. A
composite crossing costs *nothing extra*, because the host evaluates it. The
Timer's rule is three native crossings above its in-process cost, which is what
that arithmetic predicts.

**No node makes a Loom turn**, reopens an image, or rebuilds the provider stack:
22,000 evaluations of an overlaid chain opened exactly one image, the mount's own.

## What this is not

No discovery, no scanning, no directory of providers — a host still names every
artifact it mounts. No persistence: which providers a project loads is not
answered here. No version solving, no dependency graph, no provider trust policy,
no concurrent replacement. Those are other phases and some of them are other
questions.

Read this beside [`operator/provider_abi.h`](../../operator/provider_abi.h) (the
C table), [`operator/provider.hpp`](../../operator/provider.hpp) (the codec and
the macro), [`operator/provider_host.hpp`](../../operator/provider_host.hpp)
(mounting), and [`operator/catalog.hpp`](../../operator/catalog.hpp) (the layered
store). The consumer half is [operator-host.md](operator-host.md); the rule the
Timer supplies is [timer-protocol.md](timer-protocol.md).
