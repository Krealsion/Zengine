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

**Declared vocabulary is agreed at admission; authority is granted separately.** Loom checks
accepted, emitted, claimed and persisted shapes and their nested schema closure. `Emit` is not
an exhaustive send list and declaring a shape grants no reach. The actual destination, shape
and version still meet the sender's grant at delivery, including replies. A convenience layer
can reuse these mechanisms but cannot authorize itself. See Loom's
[admission contract](https://github.com/Krealsion/Loom/blob/main/docs/decisions/declared-vocabulary-is-agreed-at-admission.md).

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

**Extract from repeated working behaviour, never from a list of widgets.** A piece joins the
component package when working tools already carry the same behaviour and extracting it deletes
their copies; a piece that would only rename one tool's code is not extracted.

**Refuse rather than clamp, and say why.** A refused edit leaves both stored coordinates
untouched. Resolution, by contrast, is **total for every value the type can hold** — authored
content arrives from the wire and from a poke as well as from a checked edit, so an out-of-range
share is clamped there and an absurd span divides before it multiplies.

**Say what a green means.** A test population is a written file, not a derivation, because only
a written expectation knows what is *missing*. See
[build and test](../contributing/build-and-test.md#verification).

## Cross-pane interaction

> This section exists because a cross-pane semantic gesture — dragging an object out of one pane
> and into another — crosses several ownership boundaries. A value or a reference is carried
> between panes today; **nothing here designs more.** It is a map of who owns what today, so a
> future decision can be made from evidence.

### What makes it hard today

**An external pane receives pointer and keyboard input only as sentences about its own room.**
The protocol — every shape listed in
[`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp) and
[`workshop/pane_canvas_vocabulary.hpp`](../../workshop/pane_canvas_vocabulary.hpp), described in
[A weave may offer a pane](../reference/workshop-panes.md#a-weave-may-offer-a-pane) — is
deliberately thin: Workshop grants a pane a lattice of prose rows and columns, or a canvas room in pixels it draws its own picture on, tells it *a weaver pressed here in your room*, tells it
*a key went down and you have the keyboard* (or, for an action the pane declared, *a weaver asked
for this action of yours*), and tells it the wheel turned over its body. **A pane never says it
consumed a gesture**: it may hand a press back (`PanePassRequested`, *that press was not mine*)
or say an Escape was unspent (`PaneEscapeUnspent`), each under that gesture's own correlation, and
Workshop's own meaning then runs; a pane that says neither keeps the gesture. On a canvas a press
is held for its provider: its motion and its release are told to it under the press's room,
picture and number until the press ends, and Workshop ends it as lost when a mode, a menu, a new
room or a carry takes it (the
[pane-local canvas](../reference/workshop-panes.md#optional-pane-local-canvas)).
A pane may ask Workshop to carry a value or a reference
([`workshop/pane_carry.hpp`](../../workshop/pane_carry.hpp)) under the gesture that approved it —
a press, an action, a shortcut or a menu choice. A value dragged out under a primary press lands
where the hand lets go; any other carry Workshop holds until the actor clicks a receiving pane,
and Escape gives it up. It lands on a pane accepting a drop of that kind, as a place in that
pane's rows or pixels; what it means there is the receiving pane's. There is no target
negotiation: a pane with no door for what is carried is sent nothing. The other questions the
protocol carries are scoped conversations with explicit answers: a pane asking to be seated,
Workshop asking whether it may quit.

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
| hit testing over authored objects | the **`ui` package**: `hit(scene, cx, cy)` answers the **authored id** under a cell. Workshop does not call it: its own furniture is hit-tested region by region, in the next row | [`ui/layout.hpp`](../../ui/layout.hpp) |
| hit testing over Workshop's own furniture | **Workshop's screen module**, per-region: a layout tab, the contextual menu, a pane's rectangle and the row of its body a press names. Each is its own predicate | [`workshop/screen.hpp`](../../workshop/screen.hpp) |
| pane rectangles (placement and size) | **Workshop's arrangement/setup module** — an authored place and size per pane, resolved against the screen | [`workshop/setup.hpp`](../../workshop/setup.hpp), [`workshop/arrangement.hpp`](../../workshop/arrangement.hpp) |
| pane order (depth) | the **setup's rank permutation**. Front is *painted later*; there is no numeric z | [`workshop/setup.hpp`](../../workshop/setup.hpp) |
| pointer routing | **Workshop's weave**, in one place: the `PointerButton` and `PointerMoved` handlers, which decide by mode and then by region | [`workshop/weave.hpp`](../../workshop/weave.hpp) |
| whether a press was consumed | **Workshop's weave**, as a local `bool` per region check. There is no cross-participant disposition type | [`workshop/weave.hpp`](../../workshop/weave.hpp) |
| drag / move while held | **Workshop's weave**, as *held gestures* it can end. Mid-drag state is Workshop's, and never authored until it settles; a carried item is Workshop's until it is placed | [`workshop/weave.hpp`](../../workshop/weave.hpp) |
| pointer capture | **a canvas press's hold**: its provider hears its motion and release under the press's room, picture and number, and Workshop ends it as lost. A prose pane hears a sweep's motion and no release | [`workshop/pane_canvas_vocabulary.hpp`](../../workshop/pane_canvas_vocabulary.hpp) |
| keyboard focus | **Workshop's weave**, per mode (`keyboard_context`: the arrangement scopes, the contextual surface, a layout's name line, then a pane holding the keys, else command). A loaded pane's own cursor is the pane's. A canvas has no focus and never did | [`workshop/weave.hpp`](../../workshop/weave.hpp), [`workshop/screen.hpp`](../../workshop/screen.hpp) |
| keyboard possession across the pane seam | **Workshop**, as a spend: granted to a pane, revoked by a press anywhere else | [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp) |
| the subject an inspector reads | **Workshop's session**, named per inspection and *published as a fact* — a named subject, not a pointer, which selecting or focusing another pane does not move. (Selection of an authored object held this row until the object canvas retired.) | [`workshop/inspection_seam_vocabulary.hpp`](../../workshop/inspection_seam_vocabulary.hpp) |
| the external pane protocol | **`workshop/pane_vocabulary.hpp`** — the shapes it lists, prose one way, a bounded budget, input as places, keys and resolved ids, answered only by a press or an Escape handed back, and explicit answers only where it asks a question (the reveal, the quit) | [`workshop/pane_vocabulary.hpp`](../../workshop/pane_vocabulary.hpp) |
| what a pane may draw | **its own picture on the canvas room it is granted**, bounded and admitted whole, or rows Workshop composes into the canvas | [`workshop/screen.hpp`](../../workshop/screen.hpp) |

### Where a cross-pane drag crosses a boundary

Reading the map, a semantic drag from one pane to another crosses four places, and none of them
is a rendering change; three have an owner today:

1. **A shape for "a gesture is in progress and it carries this object."** The carry requests and
   their answer ([`workshop/pane_carry.hpp`](../../workshop/pane_carry.hpp)): Workshop takes a
   carry only under the gesture that approved it, and holds the item until it is placed, put down
   or given up.
2. **A disposition on the pane protocol** — absent. Workshop tells a pane about a press and asks
   nothing; the pane may hand a press back or say an Escape was unspent, never that it consumed
   one, and a drop target cannot say *yes, I will take that* before the drop: it is sent the drop
   and answers in its own words. A target that can answer is a target that can refuse,
   which is a protocol change rather than an addition.
3. **A capture concept.** A canvas press's hold, told to its provider through motion and release
   while Workshop keeps the power to end it; a drag's carry takes its motion and release once it
   begins.
4. **An owner for "what does this pane accept".** The drop doors a pane's holder accepts
   (`PaneValueDrop`, `PaneDrop`, `PaneCanvasValueDrop`, `PaneCanvasDrop`), read off its
   accept-set; what a value means where it lands is the pane's own typed answer.

Two things worth stating so they are not mistaken for a plan: **a second weave publishing input
is not a second UI region** — published input has no arbitration, so two publishers do not
divide a screen between them — and **a pane takes no part in deciding where input goes**: the
routing and the arbitration are Workshop's.

## Large source units

Two Workshop modules are large. Judged by what lives in them rather than by their size:

| module | lines | judgement |
|---|---|---|
| [`workshop/screen.hpp`](../../workshop/screen.hpp), with its bodies in thirteen `workshop/screen_<subject>.cpp` files | ~1,900 in the header (the composition, its `static_assert`s, the constants, the constexpr functions and every declaration); ~3,200 of bodies across the subject files | **Composition of one screen.** Everything in it answers "where does this go, and what does it look like": the fixed composition and its `static_assert`s, per-region placement, and one painter per region. Several independent machines *have* accreted here — the screen composition, per-region hit predicates, and the painters — but they share one invariant (a single resolved screen every consumer reads), and the subject files keep it: each holds bodies, not a boundary. Read [the ownership note](#ownership-map) before moving anything |
| [`workshop/weave.hpp`](../../workshop/weave.hpp), with its bodies in fifteen `workshop/weave_<subject>.cpp` files | ~1,500 in the header (the class, its state and every declaration); ~5,900 of bodies across the subject files | **The mode machine.** Genuinely several state machines — command, naming, the contextual surface, the arrangement scopes, and the desktop seam's doors (launch, close, inspect, commit, make) — plus the routing that puts them in order. (The object canvas's editing, the picker and the terminal overlay were modes here too, and retired.) This is where a semantic split is most nearly earned, and the natural seam is *one mode per unit* with the routing table left behind. It is not earned yet: the modes share the session state and the refusal channel through one class, and the routing order between them is itself load-bearing |

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
