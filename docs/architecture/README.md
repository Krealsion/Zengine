# Architecture notes

**Design material.** Why Zengine is shaped the way it is, and where the seams are. Reading this
is an intentional choice — nothing here is needed to use the library or Workshop.

If you want to *use* something, go to [getting started](../getting-started.md), the
[cheat sheet](../../cheat_sheet.md) or the [reference pages](../README.md). If you want to know
what does not work yet, go to [limitations](../workshop/limitations.md).

## The three layers

```text
Loom       the substrate. Values, schemas, the admission gate, the switchboard,
           the Kernel. Everyone's, and it cannot see Zengine.
Zengine    one opinionated set of weaves built on it. Take it, replace it
           piece by piece, or ignore it.
your work  weaves that sit alongside Zengine's as peers, not as plugins into it.
```

Zengine is a **separate repository from the Loom on purpose**, and it consumes the Loom exactly
as a stranger would. The dependency arrow is structurally un-invertible: the Loom's build cannot
see Zengine. So every rough edge in the Loom's public surface is felt here before it is felt by
a guest — which is the point, and is why the
[installed-package path is the default](../contributing/build-and-test.md#the-default-an-installed-loom-package-zen_loom_devoff).

## Recurring principles

These are not aspirations; each one is enforced somewhere, and the enforcement is named.

**A weave's manifest is its accept and emit lists.** There is no wildcard acceptance, no
undeclared emission and no ambient registry. A convenience layer may compose a wider contract
than an author writes by hand, but it composes it *into the manifest* — the ceremony is hidden
from the author, never the conversation from Loom.

**A grant bounds what a weave may say, never what it may touch.** An in-process weave shares
the host's address space, so any code compiled into the binary could call the same platform
functions. What a grant buys is one reviewable place where authority lives and one refusal to
test. Calling that containment would be an overclaim, and
`Kernel::containment_note()` refuses to make it at run time.

**Address a slot, not an instance.** A role-addressed send reaches whoever holds the role at
delivery, which is what lets a service be replaced while its consumers keep working. Where a
promise differs — this incarnation's beat versus the slot's beat — the two get **different
names** rather than an inferred mode, because they are different promises about who hears it and
who may cancel it.

**Authored is not resolved, and the result is cached nowhere.** What a person wrote down and
what a viewport makes of it are separate values. The authored type has no field able to hold a
resolved rectangle, and adding one is a compile error with a test that proves the fence fires.

**One party measures in a sizing conversation.** The medium measures its own face once and
publishes the *result*; the application does the arithmetic. Both call the same function, so
"how much fits" has one answer in the process. A medium with no answer publishes **nothing**
rather than zeroes — "I have no opinion" and "there is no room" are different sentences.

**Absence is said, not guessed.** A chain that cannot reach the root is not placed at all. A
pane reference this build cannot present is *unresolved*, never *unavailable* — silence is not
evidence of absence. A sentinel for "none" is negative where the positive space belongs to real
values, so a later vocabulary cannot collide with it silently and in the widening direction.

**Extract from repeated working behaviour, never from a list of widgets.** The component package
has one component, and it was declined once before it was earned: at the first attempt the two
consumers shared only free functions, so extracting would have renamed a class and deleted
nothing.

**Refuse rather than clamp, and say why.** A refused edit leaves both stored coordinates
untouched. Resolution, by contrast, is **total for every value the type can hold** — authored
content arrives from the wire and from a poke as well as from a checked edit, so an out-of-range
share is clamped there and an absurd span divides before it multiplies.

**Say what a green means.** A test population is a written file, not a derivation, because only
a written expectation knows what is *missing*. See
[build and test](../contributing/build-and-test.md#verification).

## Cross-pane interaction

> This section exists because a cross-pane semantic gesture — dragging an object out of one pane
> and into another — is a thing people ask for, and the honest answer is that it would cross
> several existing ownership boundaries. **Nothing here is a design.** It is a map of who owns
> what today, so a future decision can be made from evidence.

### What makes it hard today

**An external pane receives no pointer or keyboard input as a first-class participant.** The
protocol is four shapes and it is deliberately thin: Workshop grants a pane a lattice of prose
rows and columns, tells it *a maker pressed at this row and column in your room*, and tells it
*a key went down and you have the keyboard*. Workshop **asks nothing back**. There is no
disposition, no "I consumed it", no drag lifecycle, no target negotiation and no capture. A pane
cannot say "I am now the source of a drag", because there is no shape with which to say it.

**Keyboard possession is a spend, and any press elsewhere takes it away.** That is enough for a
pane that wants typed input in its own room, and it is not a capture model.

### Ownership map

A `where` that names `workshop/screen.hpp` or `workshop/weave.hpp` names the declaration; the
bodies compile once from `workshop/screen_<subject>.cpp` and `workshop/weave_<subject>.cpp`
beside them, one file per subject the header's section banners name
(`workshop/CMakeLists.txt`, the logic target).

| system | owner today | where |
|---|---|---|
| authored geometry → resolved rectangles | the **`ui` package**, as pure arithmetic. No viewport is remembered and no result is cached | [`ui/layout.hpp`](../../ui/layout.hpp) |
| hit testing over authored objects | the **`ui` package**: `hit(scene, cx, cy)` answers the **authored id** under a cell | [`ui/layout.hpp`](../../ui/layout.hpp) |
| hit testing over Workshop's own furniture | **Workshop's screen module**, per-region: a terminal input row, a property row, a pane's rectangle. Each is its own predicate | [`workshop/screen.hpp`](../../workshop/screen.hpp) |
| pane rectangles (placement and size) | **Workshop's arrangement/setup module** — an authored place and size per pane, resolved against the screen | [`workshop/setup.hpp`](../../workshop/setup.hpp), [`workshop/arrangement.hpp`](../../workshop/arrangement.hpp) |
| pane order (depth) | the **setup's rank permutation**. Front is *painted later*; there is no numeric z | [`workshop/setup.hpp`](../../workshop/setup.hpp) |
| pointer routing | **Workshop's weave**, in one place: the `PointerButton` and `PointerMoved` handlers, which decide by mode and then by region | [`workshop/weave.hpp`](../../workshop/weave.hpp) |
| whether a press was consumed | **Workshop's weave**, as a local `bool` per region check. There is no cross-participant disposition type | [`workshop/weave.hpp`](../../workshop/weave.hpp) |
| drag / move while held | **Workshop's weave**, as *held gestures* it can end. Mid-drag state is Workshop's, and never authored until it settles | [`workshop/weave.hpp`](../../workshop/weave.hpp) |
| pointer capture | **does not exist as a concept.** A held gesture is Workshop's own state, not a grant to a participant | — |
| keyboard focus | **Workshop's weave**, per mode, plus the inspector's own row focus. A canvas has no focus and never did | [`workshop/weave.hpp`](../../workshop/weave.hpp), [`workshop/screen.hpp`](../../workshop/screen.hpp) |
| keyboard possession across the pane seam | **Workshop**, as a spend: granted to a pane, revoked by a press anywhere else | [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp) |
| selection of an authored object | **Workshop's document/session state**, and it is *published as a fact* — an identity, not a pointer | [`workshop/document.hpp`](../../workshop/document.hpp) |
| the external pane protocol | **`workshop/pane_vocabulary.hpp`** — four shapes, read-only prose, a bounded budget, no answers | [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp) |
| what a pane may draw | **nothing directly.** It publishes rows; Workshop composes them into the canvas | [`workshop/screen.hpp`](../../workshop/screen.hpp) |

### Where a cross-pane drag would cross a boundary

Reading the map, a semantic drag from one pane to another would need something new at four
places, and none of them is a rendering change:

1. **A shape for "a gesture is in progress and it carries this object."** Selection is already
   published as a *fact* rather than a pointer, which is the right precedent; a drag is the same
   kind of fact with a lifetime.
2. **A disposition on the pane protocol.** Today Workshop tells a pane about a press and asks
   nothing. A drop target has to be able to say *yes, I will take that* — and a target that can
   answer is a target that can refuse, which is a protocol change rather than an addition.
3. **A capture concept.** Held-gesture state is Workshop's. For a pane to be a drag source or
   target across a whole gesture, the arbitration has to move somewhere both parties can see.
4. **An owner for "what does this pane accept".** A drop needs a typed answer to "may this
   object land here", which is a describe/accept question of the kind the composer's schema
   surface already answers for messages — and reusing it means deciding whether a *pane* has an
   accept-set at all.

Two things worth stating so they are not mistaken for a plan: **a second weave publishing input
is not a second UI region** — published input has no arbitration, so two publishers do not
divide a screen between them — and **a pane is a presentation, not a participant** in Workshop's
input conversation today.

## Large source units

Two Workshop modules are large. Judged by what lives in them rather than by their size:

| module | lines | judgement |
|---|---|---|
| [`workshop/screen.hpp`](../../workshop/screen.hpp), with its bodies in fourteen `workshop/screen_<subject>.cpp` files | ~2,800 in the header (the composition, its `static_assert`s, the constants, the constexpr functions and every declaration); ~5,100 of bodies across the subject files | **Composition of one screen.** Everything in it answers "where does this go, and what does it look like": the fixed composition and its `static_assert`s, per-region placement, and one painter per region. Several independent machines *have* accreted here — the screen composition, per-region hit predicates, and the painters — but they share one invariant (a single resolved screen every consumer reads), and the subject files keep it: each holds bodies, not a boundary. Read [the ownership note](#ownership-map) before moving anything |
| [`workshop/weave.hpp`](../../workshop/weave.hpp), with its bodies in ten `workshop/weave_<subject>.cpp` files | ~1,200 in the header (the class, its state and every declaration); ~5,200 of bodies across the subject files | **The mode machine.** Genuinely several state machines — command, editing, picker, naming, management, terminal — plus the routing that puts them in order. This is where a semantic split is most nearly earned, and the natural seam is *one mode per unit* with the routing table left behind. It is not earned yet: the modes share the session state and the refusal channel through one class, and the routing order between them is itself load-bearing |

The split that did happen is not a semantic one. The bodies moved out of both headers into
subject files that compile once (`workshop/CMakeLists.txt`, the logic target), because a body
in a header is emitted in every translation unit that reaches it, and thirteen of them did;
the declarations, and therefore the shape of both modules, are exactly where they were. The
pressure that would decide a real seam is the cross-pane work above: it adds a participant to
the routing conversation, and that is the change that would make the mode machine's seam worth
paying for.

## Further reading

Per-subject design detail lives with the subject:

- [Timer protocol](../reference/timer-protocol.md), [continuity](../reference/timer-continuity.md),
  [the binding layer](../reference/timer-binding.md), [laws](../laws/timer-laws.md), and
  [why durations rather than deadlines](../decisions/timer-continuity-carries-remaining-duration.md).
- [Operator providers](../reference/operator-providers.md) — how an artifact supplies rules to a
  host, and how one power may be shadowed and revealed again.
- [The operator host surface](../reference/operator-host.md) — how a loaded weave asks a host to
  evaluate a rule it did not compile with.
- [Load plans](../reference/load-plan.md) — the execution law, and what a failed artifact rolls
  back.
- [Introspection](../reference/introspection.md) — why three panes rather than one table, and why
  two of them deliberately disagree.
- [Pointer spaces](../reference/pointer-spaces.md) — where a reported position lands, and which
  package owns each step.
- [The Surface package](../reference/surface.md) — the depth model, and the two kinds of text.
- [The UI package](../reference/ui.md) — the authored/resolved fence.

Frozen material describing an earlier tree is in [`docs/history/`](../history/pre-r2c/README.md)
and is not maintained against the current one.
