# The operator host — how a loaded tool spends the host's operator truth

A Zengine host may hold an [operator](../../operator/catalog.hpp) catalog: named,
typed, composable semantic rules that more than one surface needs to agree
about. `timer.normalize_delay` is the first one, and
[timer-protocol.md](timer-protocol.md) says what it means.

This page is about the other half: how a **dynamically loaded weave** — one the
host opened with the Kernel, built with `create(void)`, and never handed a single
C++ object — asks that host to spend a rule.

It is a seam, not a plugin system. It carries operators and nothing else.

## What a consumer writes

Two lines, and neither of them is about the seam.

```cpp
#include "operator/host.hpp"

class MyTool : public loom::WeaveBase<MyTool, MyState, /* ... */> {
public:
    MyTool() : operators_(zengine::op::OperatorHost::offered()) {}

    void on(const SomeAsk& ask, loom::Mail& mail) {
        const auto rule = operators_.describe("timer.normalize_delay");
        if (!rule.ok()) {
            return;                       // no host, or no such operator
        }
        loom::Value pack(rule.inputs);    // the HOST's own input schema
        pack.set("delay_ms", loom::Cell::integer(ask.delay));
        pack.set("repeat", loom::Cell::boolean(ask.repeat));

        const auto answer = operators_.evaluate(rule, pack);
        if (answer.ok()) {
            use(answer.value->at(0)->as_int());
        }
    }

private:
    zengine::op::OperatorHost operators_;
};

ZENGINE_OPERATOR_CONSUMER();               // at namespace scope, once
```

`ZENGINE_OPERATOR_CONSUMER()` is the whole of the opt-in. It exports one optional
symbol saying *this image can receive an operator host*; a host that does not
know about operators never looks for it, and a weave that never writes it is an
ordinary weave forever.

Link `zengine-operator-consumer`. **Do not link `zengine-operator`** — that is
the host's package, and a loaded consumer that has one has a catalog of its own
to disagree with.

## What a host writes

```cpp
op::Catalog catalog = timer::standard_operators();
op::OperatorHostSurface operators(catalog);      // declared BEFORE the Kernel
loom::Kernel kernel(bus);
// ...
{
    op::OperatorOffer offer(operators, path_to("my-tool"));
    boot("my-tool", kMyRole);                    // an ordinary zen.LoadWeave
    bus.pump();
}                                                // withdrawn here
```

Three things about that, and each is load-bearing.

**The offer brackets the load.** A weave's first legitimate need for an operator
is inside `create()`, which the Kernel calls and which no host can get between —
and on the real path the load happens deep inside a delivery, several messages
away. So the offer goes up before the command is sent and comes down after the
pump has drained. `create()` runs inside that window whichever route took it
there.

**The offer is scoped, not held.** Between those braces the host holds one share
of the artifact's image, purely so a second exported symbol can be resolved.
Outside them it holds nothing, which is why an unload closes exactly what it
would have closed for a weave that never heard of operators.

**The surface must outlive the Kernel.** Every consumer that accepted an offer
holds a copy of the table, and the copy names the surface. Declaring the surface
before the Kernel is what makes reverse-order destruction correct: the Kernel
goes first and takes its artifacts with it. Nothing crosses the ABI to enforce
this, because a lifetime the host already controls does not need a refcount —
it needs to be got right.

**And the catalog is the host arrangement's, not a singleton.** It is a local of
the host's own `main` — no static, no process-wide registry, no accessor. A
second Zengine host in one process owns a second one, correctly, and neither is
"the" catalog. What makes a catalog canonical is that every consumer expected to
agree with the others was handed *that* one during its load.

## Every `create()` needs its own offer, including a reload

`Kernel::load` is not the only place the Kernel builds an instance;
`Kernel::reload_from` is the other. A hot reload snapshots the incumbent, calls
`create()` on the new image, and revives — so a replacement instance is offered
nothing unless the host brackets the reload exactly as it brackets a load:

```cpp
{
    op::OperatorOffer offer(operators, path_to("my-tool"));
    send(manager, loom::ReloadWeave{"my-tool", path_to("my-tool")});
    pump_turns();
}
```

No new mechanism: the same object, around the other command. A host that omits
it gets a replacement that is unbound, which for a consumer with a fallback means
a silent change of semantics — the host's error, and the same error as omitting
the bracket at load time.

**There is no way for the artifact to catch that**, and it is worth saying which
shape the general fix has rather than leaving it implied: the Kernel would have
to be tellable that an artifact must always be offered something, so that every
one of its `create()` sites is covered rather than each host remembering. That is
a loader question (LOAD-0's shape from a third direction), not an operator one.

## What actually crosses

Not the catalog. Not a callable. Not an index into one.

```text
host  -> consumer    two function pointers and an opaque context
consumer -> host     an operator IDENTITY, as a string
host  -> consumer    the operator's real input/output SCHEMAS, as descriptor bytes
consumer -> host     an argument pack, as serialized value bytes
host  -> consumer    the answer, as serialized value bytes -- or a refusal, as prose
```

Both descriptions and answers are re-admitted through `loom::admit` on the
receiving side, exactly as a message is: this is another boundary the one gate
guards. The descriptor is `zengine.OperatorDesc v1`, which is `zen.Manifest`'s
shape — a post-order `referenced` closure of every nested schema, then the two
ports, all in `zen.SchemaDesc v1`. There is no second schema language here.

**The description and the evaluation come from the same `OperatorDef`.** There is
no hand-written descriptor beside an operator, and there must never be one: a
description that could disagree with the thing it describes is the second copy
the whole operator substrate exists to remove.

**Evaluation resolves at spend.** The consumer holds an identity and two schemas
it built for itself — never a pointer, an index or a callable into the host — and
the host looks its own definition up on every call. So a rule that changed
underneath is spent as it is now, not as it was described.

## Why `evaluate` takes the contract and not the name

Turning bytes into a `loom::Value` needs a door, so an `evaluate("id", pack)`
would have to fetch the description on every call: a second crossing per
evaluation, hidden inside a spelling that looks free. Describe once, spend many
times — which is also what a real consumer does, since a form is built from a
contract and then a maker types into it.

## It is not a message

An operator call enqueues nothing. No send, no publish, no correlation, no answer
authority, no role, and no pump generation — sixteen evaluations inside one
delivery cost the bus exactly what one does. That is not an optimisation: a
rule spelled as conversation would be one pump generation per node, so a
three-node rule would be three sequential turns of the bus for arithmetic.

It is also not authority. Evaluating `max(-500, 0)` is computation over values
the caller already holds, and nothing about arriving from a loaded artifact makes
it a capability question. The honest boundary is LOAD — a weave loaded in-process
shares this address space, and `Kernel::containment_note()` says what that means.

## Five failures, five answers

| status | what happened |
|---|---|
| `ZENGINE_OP_ERR_NO_HOST` | nothing was ever offered to this instance. An ordinary weave in an ordinary host, and not an error |
| `ZENGINE_OP_ERR_ABI` | the two sides do not agree on the seam's version. Nothing was handed over and nothing was called |
| `ZENGINE_OP_ERR_NOT_FOUND` | the host publishes no operator under that identity |
| `ZENGINE_OP_ERR_REFUSED` | the operator refused — a bad pack, an unresolved step, a step that is not the authored signature. The reason carries the gate's or the catalog's own words, verbatim |
| `ZENGINE_OP_ERR_MALFORMED` | the bytes were not a well-formed Zen envelope at all |

A missing host and a missing operator are different troubles and send a reader to
different places, which is why they are two answers and not one silence.

## The version, and why it rides in a field

`ZENGINE_OPERATOR_ABI_VERSION` is carried by **both** tables and checked by
**both** sides, because each side is the only one that knows what it was compiled
against. It is a field rather than a suffix on the symbol name for the reason
`zen_weave_abi` puts it in one: an absent symbol already means something else
here — *an ordinary weave, load it normally* — and a mismatch arriving as a
failed lookup would be indistinguishable from that. Two different facts must not
arrive as the same silence.

## What this deliberately is not

No operator enumeration (a consumer names the rule it was authored against), no
authoring door (nothing loaded may publish into the host's catalog), no callable
accessor, no subscription, no second injected capability. If a future capability
needs this exact invariant it may earn a shared component; today it has not, and
a `PluginServices` grown ahead of its second customer is a framework nobody
asked for.

## Who consumes this today

The shipped **Timer** (`zengine-timer`) and OPH-0's stranger fixture. The Timer is
the first artifact to be both halves of this seam at once: it AUTHORS the delay
vocabulary its package publishes, and it CONSUMES whatever operator surface its
host offered the instance. Those are different roles and the package keeps them
apart — `timer::standard_operators()` is what a HOST publishes, and what a Timer
spends is decided per instance. See
[timer-protocol.md](timer-protocol.md#what-the-timer-makes-of-a-delay).

A consumer with its own fallback owes one more thing than the stranger does:
**an accepted offer must be checked before it is accepted.** The Timer describes
`timer.normalize_delay` across the seam at construction and compares both port
schemas against the ones its own package authors (`loom::same_identity`, which is
`Schema::content_id()` doing the versioning it already does). A host that
publishes no such rule, or publishes it at another signature, is refused — the
constructor throws, `create()` returns null, and the Kernel refuses the load.
There is deliberately no path from "the host could not serve it" to "evaluate our
own copy": a consumer that fell back there would diverge from its host at exactly
the moment the host became inconsistent.

## Tests

Zengine suite `operator`. In
[`tests/test_operator_host.cpp`](../../tests/test_operator_host.cpp): an untouched
pre-existing artifact meeting the offer path, the same fixture source built three
ways (aware / ordinary / stale-version), the offer reaching `create()`, the
described contract, the whole normalization matrix across a real module boundary,
the bus-turn count, unload and reload, the withdrawal, two instances of one
image, and a primitive replaced in the **host** moving what the loaded artifact
answers, with its binary unchanged.

In [`tests/test_operator_canonical.cpp`](../../tests/test_operator_canonical.cpp):
the same substitution moving a **real loaded Timer** and that stranger together,
in one process, off one catalog, with neither artifact rebuilt — plus a Timer
offered nothing staying where it was, the two refusals above, the bracketed and
unbracketed reloads, and the traffic count for eight host-backed schedules.
