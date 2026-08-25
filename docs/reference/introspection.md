# Introspection — the `Loaded`, `Project` and `Powers` panes

**Reference.** What each pane shows, where each fact's authority lives, and why two of them
deliberately disagree.

> **What is this Loom actually running?**

Introspection is a Zengine package (`introspection/`) that builds one loadable weave,
`zengine-introspection`. It holds the office `zengine.introspection` and offers Workshop **three**
panes through the [external pane protocol](../guides/make-a-workshop-tool.md#part-b--an-office-authored-external-pane):

```text
PaneRef      zengine.introspection / loaded
picker row   Loaded    closed    what the kernel has loaded, and each one's role
pane header  Loaded @zengine.introspection

PaneRef      zengine.introspection / arrangement
picker row   Project   closed    what this project asked for, and what resolved
pane header  Project @zengine.introspection

PaneRef      zengine.introspection / powers
picker row   Powers    closed    which operators resolve, and who supplies each
pane header  Powers @zengine.introspection
```

`zengine-workshop` boots it from its [authored load plan](load-plan.md) beside the Skin, the input
reader and the Timer, so a maker opens any of the three from the panel picker (`p`) and can keep
them in a saved setup like any other pane.

## Three questions, three owners, and they disagree on purpose

The panes are separate because their **populations, their authorities and their currencies** are
different. One merged table would have had to invent a row kind that is none of them.

```text
loaded        which WEAVES this Loom's Kernel has loaded    owner: the Kernel's loaded() map
arrangement   which AUTHORED ARTIFACT PARTICIPATIONS this   owner: the host's realization owner
              project asked for, and where each has got to           (authored plan + resolved rows)
powers        which OPERATOR POWERS this host resolves,     owner: the host's op::Catalog
              and whose contribution satisfies each
```

The clearest evidence any of them is honest is where two of them **disagree**.
`zengine-operators-basic` is a provider and not a weave: no Kernel loads it, it has no `WeaveId`
and no role, and the host opens it directly. It is a row of `Project` and it is **absent** from
`Loaded`. A build in which both listed it would be a build in which one of them had started
guessing.

The pane key is `arrangement` and the picker name is `Project`, and the difference is deliberate:
the **key** is the durable half of a `PaneRef` and is what a maker's saved setup names, so it
carries the load plan's own word; the **name** is the ten cells `kPickerNameCols` actually shows,
and `Arrangement` is eleven.

## What the `Loaded` pane shows

```text
loaded weaves -- 4
  zengine-input @zengine.input
  zengine-introspection @zengine.introspection
  zengine-skin-tui-classic @zengine.skin
  zengine-timer @zengine.timer

in-process weaves are not in the kernel's map
snapshot from zen.ListLoaded, on room grant
```

One row per dynamically loaded weave: the name its library was loaded under, and the role it was
bound to at load. A weave the kernel bound no role to reads `name @(no role)` — an **observed**
absence, because `LoadLibrary` carries a role field that may legitimately be empty and the kernel
answers for it either way.

The order is the kernel's own: `Kernel::loaded()` walks a `std::map` keyed by name, so the list is
name-ordered, the same order every run, independent of the host's boot sequence. Introspection does
not sort it.

## What the `Project` pane shows

> **What did this authored project ask to participate, and what resolved from it?**

```text
6 of 6 artifacts resolved -- 2 providers, 5 weaves
  zengine-operators-basic
    authored  provider normal
    resolved  provider zengine.operators.basic, 2 powers
  zengine-timer
    authored  provider normal, weave zengine.timer
    resolved  provider zengine.timer, 1 power
    resolved  weave #11, operator host offered
  zengine-composer
    authored  weave zengine.composer
    resolved  weave #13, operator host not-a-consumer
  ... 3 more

in-process participants are not authored artifacts
plan: <the file the host read>
```

**One row per authored artifact, in authored order, whatever it participates as.** `zengine-timer`
supplies an operator power *and* is loaded as a weave; it is one authored record and it is one
block here, with two `resolved` lines under it. That is the load plan's central result carried into
observation — splitting it would throw away the only place the two are known to be the same
artifact.

**`authored` and `resolved` are separate labelled rows, and never one.** The authored half is what
a person wrote in the plan file and would be true again tomorrow; the resolved half is what this
run's executor and this run's Kernel made of it and would be a lie tomorrow. `zengine.timer` is
both the authored *role* and the resolved *provider identity* — two facts that read alike — so the
labels are what keep a maker from taking them for one.

| field | kind | where it comes from |
|---|---|---|
| the stem | authored | the plan's artifact record |
| `provider normal` / `provider overlay` | authored | the plan's mount mode, spelled by the same function that writes the file |
| `weave <role>` | authored | the plan's weave declaration |
| the resolved provider identity | resolved | what the **artifact declared about itself** at mount — never the stem, never what the plan said |
| the contribution count | resolved | how many powers that mount installed |
| `weave #<n>` | resolved | the `WeaveId` **this** Kernel minted **this run** |
| `operator host <token>` | resolved | how the [operator handoff](operator-host.md) around that weave's load ended |

**There is no resolved role, because nothing observed one.** The executor keeps the authored role
and hands it to `zen.LoadWeave`; the office the Kernel actually bound lives in the Kernel's own
map, which is what the `Loaded` pane already shows. Two panes, two questions.

**The offer outcome is shown only where a weave was loaded.** A provider-only record had no offer
made around it at all, so the row carries none — reporting the enumeration's default there would be
publishing a default as an observation.

**One row per authored artifact says where realization has got with it**, and there are four
states because there are four different things that can be true:

| the row says | it means |
|---|---|
| the `resolved` lines | every surface this artifact authored has participated |
| `(loading)` | its weave load has been commanded and its answer has not arrived |
| `(refused)` | it was reached and something refused it; its own mount, if it made one, was rolled back |
| `(not reached)` | the plan declares it and realization has not got there |

**At most one row is ever `(loading)`, and at most one is ever `(refused)`.** Authored order is
strict and serial, and the plan stops at the first refusal.

**`(loading)` and `(not reached)` are ordinary states of a project that is working**, so they are
drawn plainly; only `(refused)` is drawn as something the maker must see. A project coming up is
not six problems.

**A `(loading)` row shows no resolved fields even when its provider has already mounted.** Within
one artifact the mount happens before the load, so at that instant the contribution is genuinely
in the catalog — and what came of the *row* is still undecided, because a refusal would roll that
mount back. The `Powers` pane reads the live catalog and shows the contribution immediately;
`Project` answers a different question with a different currency, which is the same reason a
provider is a row here and absent from `Loaded`.

## What the `Powers` pane shows

> **What logical operator powers exist in this host, and who currently satisfies each?**

```text
3 powers resolve here -- from 2 providers
  logic.select_int
      active    zengine.operators.basic
  math.max
      active    zengine.operators.test.min
      shadowed  zengine.operators.basic
  timer.normalize_delay
      active    zengine.timer (composite)

this pane describes this host's operator resolution only
snapshot from zengine.arrangement, on room grant
```

One block per logical [operator](operator-providers.md) identity, **active first**, with every
contribution it is shadowing listed under it. The word is the statement and the ink is the second
signal, so a monochrome terminal reads the same fact a coloured one does.

- **`active` is what an evaluation actually spends.** The pane reads the same layered store
  `Catalog::find` resolves through, so `math.max` reading `active zengine.operators.test.min` and a
  running Timer computing the substituted answer are one fact, not two that agree.
- **`shadowed` appears only where an overlay exists**, and unmounting the overlay reveals the same
  contribution object underneath rather than rebuilding it — so the pane goes back to exactly what
  it said before.
- **`(composite)`** is the definition's own answer (`OperatorDef::is_composite`), not a classifier.
  It is worth reading because it says what a replacement will do: a composite holds its leaves as
  *identities* and resolves them at every spend, so covering `math.max` changes what a composite
  over it computes.
- **`(this host)`** would name a contribution the host published itself. `zengine-workshop` authors
  no operator and cannot, so it never appears there.
- **The pane names no power and no provider.** There is no `math.max` in the projection's source and
  no branch that treats one identity differently from another: a provider mounted later appears
  with nothing edited. That is the feature.

## Where each fact comes from

| fact | authoritative owner | how Introspection gets it | current or retained | absence means |
|---|---|---|---|---|
| which libraries are loaded | the **Kernel**'s live map (`Kernel::loaded()`) | `zen.ListLoaded` → the Weave Manager → `zen.ListLibraries` → the control door → `zen.Result` | a **snapshot**, re-read on each room grant | the map was empty when it was read |
| the role each holds | the **Kernel** (`Kernel::role_of()`), same answer | same message | same snapshot | that library was bound to no role |
| that a weave is *not* listed | nobody — **not observed** | — | — | it is not in the kernel's map. It may still be running |
| what the project authored | the **load plan file**, held by the host | `ArrangementRequested` → `zengine.arrangement` → `ResolvedArrangement` | a **snapshot**, re-read on each room grant | the plan named no such artifact |
| what resolved from it | the host's **realization owner** (its cursor and its resolved rows) | same message | same snapshot | realization has not reached that row |
| which powers resolve | the host's **`op::Catalog`** (the same store `find` resolves through) | `PowersRequested` → `zengine.arrangement` → `ResolvedPowers` | a **snapshot**, re-read on each room grant | nothing supplies that identity here |
| that a power is *not* listed | nobody — **not observed** | — | — | this host's catalog does not resolve it, and the pane says nothing about any other |

The third row is why the pane always carries `in-process weaves are not in the kernel's map`.
Workshop's own weave, the boot weave, the control door, the Weave Manager, the Builder tool, the
build runner and the terminal participant are all live participants that `zen.ListLoaded` does not
enumerate and cannot speak about. A count without that sentence would be an honest number leaving
a false picture, so the sentence is reserved out of the pane's row budget **before** the list is
offered anything but its first row.

The other two panes carry the same kind of sentence for the same reason.
`in-process participants are not authored artifacts` bounds the `Project` count — every weave the
host mounted in-process, including the door that answers this very question, is a live participant
that was never an authored artifact. `this pane describes this host's operator resolution only`
bounds the `Powers` count — it is **one host's** resolution and the pane read one catalog, so the
sentence claims exactly that and stops.

**And it stops there deliberately.** The wording that stood here first —
`a weave that took no offer holds its own catalog` — described the Timer's supported local fallback
and read as a law about every weave that accepts no operator host, which it is not. Whether another
participant owns a private catalog, whether an unbound one can evaluate at all, and what any
particular weave makes of an offer are facts a pane that read one catalog never observed; the
bounding sentence names its own population and settles none of them.

## What it deliberately does not show

- **Not the whole participant population.** There is no participant enumeration in Loom, so there
  is nothing to read. See the table above.
- **Not schemas.** `loom::Registry` has `lookup`, `contains` and `size` — no enumeration, and it is
  a C++ object rather than a message surface, so a participant cannot ask it anything.
- **Not roles by name.** `zen.QueryRole` answers `holder == 0` both for *unheld* and for *held by a
  weave the kernel cannot see into*, and a pane that printed one of those two as the other would be
  inventing a fact.
- **Not message history, traffic, timing or payloads.** Introspection is a view over a registry,
  not a Recorder and not a Logger.
- **Not anything about a weave's internals**, its state, its grants or its Senses.

## How a loaded artifact reads host-side truth

`zengine-introspection` is loaded by the Kernel at run time. The load plan, the executor's
resolved rows and the operator catalog are **locals of the host's `main`**. Nothing but a value may
cross that line — not a pointer, not a reference, not a container, and not a callable that closes
over one.

So the host mounts one small read-only participant, `ArrangementDoor`, in the office
`zengine.arrangement`, and the tool asks it the way it already asks the Kernel:

```text
ArrangementRequested  ->  zengine.arrangement  ->  ResolvedArrangement   (an ANSWER)
PowersRequested       ->  zengine.arrangement  ->  ResolvedPowers        (an ANSWER)
```

This is the seam the `Loaded` pane already spends, pointed at two more facts: `zen.ListLoaded` goes
to the Weave Manager, an in-process weave holding kernel reach, and comes back as a value. It is
deliberately not a second mechanism, not a host API widening, and not a service locator.

**The door derives; it does not remember.** It holds three `const` references it does not own and
answers each question by reading them *at the moment it is asked*. There is no map, no cache, no
mirror, no registry and nothing that has to be updated when something mounts, loads, shadows or
unloads — which is exactly what makes an overlay mounted after the last reading show up in the next
one with nothing having notified anybody.

**It publishes nothing.** Every answer goes through Loom's own answer door to the one weave that
asked, so provider identities and power overlays do not become ambient knowledge for participants
that never asked. And because the answer is Loom's, the asker reads `mail.answers_ask()` — delivery
provenance no payload can write and no sender can choose — which is a stronger bound than the
correlation match `zen.ListLoaded` has to make do with.

**Who may ask:** an **office**, and only an office. The door refuses an ask that was not
deliberately authored as one, and counts the refusal. That rule names nobody — there is no
allow-list and no `zengine.introspection` in it — so a tool added tomorrow asks the same way with
no edit. It is **not containment**: `Kernel::load` binds `allow_any()` to every library it opens
and the plan binds each loaded weave to a role, so any dynamic weave in this process could satisfy
it. What it excludes is anonymous speech and a root send; what it buys is that every answer the
door has ever given went to a named office.

**What the door cannot do:** it answers two shapes and says nothing else, ever. It cannot mount,
unmount, overlay, evaluate, load, unload, reload or replace anything, and its grant is the two
answer shapes and nothing else. *Knowledge of a power is not authority to replace it.*

**A host that mounts no door holds no such office.** An ask addressed to it reaches nobody and the
two panes read `(waiting for the provider)`. That is a correct arrangement rather than a hole:
`zengine-snake` has no authored project to describe and owes no answer about one.

## Currency: a snapshot, and the pane says so

Loom gives a participant no arrival or departure event — `zen.Activated` is directed at the
activated weave alone, and the bus tap is a host facility rather than a participant door. So there
is nothing to subscribe to, and a consumer that polled for one would be a consumer polling.

Introspection re-reads on exactly one beat: **Workshop granting a pane its room.** That happens
when the pane opens, when a valid re-offer refreshes it, and when the resolved prose capacity
changes. Between two grants the rows are a reading, not a feed, and the last line of the pane says
which. **Each of the three panes keeps its own room and its own outstanding question**, so a maker
with all three open never has one pane's grant decide how another is drawn.

There is no arrival event for a weave, and there is no mount event for a provider either — so there
is nothing to subscribe to for any of the three, and nothing here polls. The `Powers` pane is the
one whose subject genuinely changes mid-run; an overlay mounted since the last grant is in the next
reading, with nobody having been told.

There is no refresh button. A pane can be pressed (below), but a press is a gesture *about a row*
and this tool does not read one as "go and look again" — that would be a second beat with no
sentence saying so. Closing and reopening the pane, or resizing it enough to move its prose
capacity, re-reads.

## Selecting a row

**Only the `Loaded` pane has a gesture.** `Project` and `Powers` are read-only projections: they
carry no selection, publish nothing, and hold no row map a press could be resolved against.
Pressing one is consumed by the pane, as a press in any pane's room is, and produces no sentence.
**No row of any of the three carries a control** — there is no unmount, replace, reload, disable or
activate anywhere, and no pane message mutates load or provider state. The maker gets the knowledge
first.

A maker can press one of the `Loaded` pane's entry rows. The pressed row is marked, and the pane
publishes an ordinary Loom message saying which entry was selected:

```text
loaded weaves -- 2
  zengine-introspection @zengine.introspection
> zengine-workshop-hello @zengine.test.workshop-hello       <- pressed

    ->  LoadedSelected { pane: "loaded",
                         library: "zengine-workshop-hello",
                         role: "zengine.test.workshop-hello" }
```

- **Published, not addressed.** It is a fact stated into the room, reaching every weave that
  accepts the shape — today, none. Nothing in this build reacts to it: no pane opens, nothing is
  inspected, and the selected weave is sent nothing.
- **The identity is the loaded library's NAME**, which is the key of the kernel's own map. It is
  not a `WeaveId`, not a participant identity, not a package or publisher, and not proof that
  anything is alive now — the pane is a snapshot and this fact is about what the snapshot *showed*.
  An empty `role` is the same observed absence the pane writes as `(no role)`.
- **The press is read against the rows on screen**, never against a fresh reading. Interpreting a
  press asks the Weave Manager nothing, so a maker who presses a row always selects the entry that
  row was showing — including one the kernel has since unloaded.
- **Selecting is an occurrence, not a state change.** Pressing the same row again publishes again;
  the picture does not change, because the mark is already there.
- **Only entry rows select.** The heading, the caveat, the snapshot-source line, the blank
  separator and the omission marker publish nothing — `... 17 more` is a *population fact*, not a
  stand-in for one hidden weave. A press on any of them is still consumed by the pane and changes
  nothing, including the current selection.
- **The selection is this pane's, and transient.** There is no Workshop-wide or setup-wide "current
  weave": it lives in the provider, is not snapshotted, is not saved, and is gone if the provider
  unloads. It is held as a *name*, so it survives a resize that windows the entry out of sight and
  the mark returns with the entry.
- **It clears when the absence is observed**, which is the next room grant's reading and not a
  moment earlier — and clearing publishes nothing, because a library going away is not a maker's
  gesture.
- **The fact carries no authority.** A listener that hears a library name and a role has learned
  two strings a maker was already looking at. It cannot thereby message, interrogate, load, unload
  or impersonate the thing named; a Loom grant is per `(shape, version, target)` and a value in a
  message is not one. *Values may flow; authority must not flow implicitly with them.*

## When the provider disappears

Workshop is told nothing — the protocol has no unload notification and manufacturing one out of
silence would be a claim nobody observed. So:

```text
provider unloads          the catalog row stays, the pane stays open, and the rows a
                          maker is looking at are the last valid ones
the next room grant       Workshop clears its cache before every grant, so the pane
                          reads `(waiting for the provider)` -- never `unavailable`
provider reloads          its attested activation re-offers the same PaneRef, which
                          refreshes the descriptor and clears the grant, so the next
                          repaint grants room again and the view returns
a fresh process           a saved setup naming zengine.introspection/loaded that this
                          run cannot resolve is `unresolved`, kept and named
```

## Authority

```text
CAN
  ask the Weave Manager one question          zen.ListLoaded
  ask the host's door two questions           ArrangementRequested, PowersRequested,
                                              as its own office
  offer Workshop its three panes              PaneOffered, as its own office
  publish rows inside the grants it was given PaneContent, as its own office
  state which row a maker selected            LoadedSelected, as its own office

CANNOT (never sent, and not in its declared Emit set)
  send the SELECTED weave anything            naming a thing is not reaching it
  load, reload, swap or unload anything       zen.LoadWeave / SwapWeave / ReloadWeave /
                                              UnloadLibrary / UnloadRole
  mount, unmount, overlay or evaluate a power there is no shape for any of it, in
                                              either direction
  reach the kernel's control door directly
  publish a canvas, a text slot or any screen
  read or write the document, the setup, or any file
  start a process, open a socket, hold a timer, read a Sense
  grant itself anything
```

Naming every provider in the process is the sharpest case of that rule in this tool. The answers
are inert values; a value arriving in a message has never been a grant, and no shape exists on
either side of the seam that would let this weave change what any of them supplies.

Being able to ask what is loaded is not being able to load anything: a Loom grant is per
`(shape, version, target)`, so `zen.ListLoaded` to the Manager is exactly that one question, and the
Manager holds the dangerous grant on its own behalf. The suite pins this from the bus rather than
from the declaration — a tap records every shape the weave sends across a whole life and compares
the set.

**And the loader is wider than any of that, which is reported rather than hidden.** `Kernel::load`
binds `Grant{}.allow_any()` to every library it opens, and a declared `Emit<...>` is informational
rather than enforced. So the list above is a fact about what this weave *does* and what the pane
protocol *reaches* — it is not a containment claim about the loader. An in-process dynamic weave
shares this process's address space; that predates this tool and is identically true of the Skin,
the reader and the Timer the same host boots.

## Density

A pane's default room is **8 prose rows** (`kStackRows` is nine, one of which is Workshop's header)
and 48 columns on the default terminal composition. Every pane is **windowed, never truncated**:
material is shown until the budget runs out and the remainder is counted on its own row
(`... 17 more`), so nothing is ever hidden without being counted. Text too long for the granted
columns is cut with `...`.

That default suits `Loaded` exactly — a heading, four weaves, a blank row and two notes. It suits
the other two **less well, and that is a real limitation rather than an oversight**. `Project`
spends three or four rows per artifact and `Powers` two or more per power, so a six-artifact
project reads, at the default size:

```text
6 of 6 artifacts resolved -- 2 providers, 5 w...
  zengine-operators-basic
    authored  provider normal
    resolved  provider zengine.operators.basi...
  ... 5 more

in-process participants are not authored arti...
```

The count is exact, the omission is counted, and nothing on the screen is false — but one artifact
is all that fits. **The room is not the terminal's**: `kStackRows` is fixed, so a bigger terminal
buys columns and no rows at all. What buys rows is the maker's own authored pane window (`w`, then
size it), which is a room change, which is a fresh reading. A block-per-entry projection and an
eight-row default are simply in tension, and the honest thing was to count what did not fit rather
than to invent a second, denser layout that says less.

## Reading the source

```text
introspection/vocabulary.hpp   the three durable PaneRef halves, the picker lines, LoadedSelected
introspection/loaded.hpp       the Loaded pane's pure core: parse the Manager's answer, spend the
                               budget, map each row back to the entry it names, move the mark
introspection/resolved.hpp     the other two panes' pure cores, and the ONE budget rule they share
introspection/introspection.cpp the weave: when to observe, whom to believe, what a press means

workshop/arrangement_vocabulary.hpp  the two questions and the two answers, as ordinary shapes
workshop/arrangement.hpp             the two derivations, and the host's read-only door
```

`loaded.hpp` and `resolved.hpp` link nothing and know no bus, so what a reading *means* is provable
over a value — and the `Loaded` pane's row-to-entry map is returned by the function that *builds*
the rows, so there is no second calculation for a press to disagree with. `resolved.hpp` holds both
new projections in one file for exactly one reason: they share the budget rule that an entry and
its omission marker are **one demand**, and that rule is spelled once so the second pane cannot
inherit a subtly different version of it.

The suite's INTR-0, SEL-0 and INTR-1 tiers do all three halves: the pure projections over a swept
domain of populations and budgets; the real derivations driven over a real authored plan, a real
Kernel and a real `op::Catalog`, including an overlay mounted and unmounted at run time; and the
real `zengine-introspection.so` loaded through the real Kernel and Manager, with its rows read off
a published canvas. Workshop's own sources are additionally read **as files**, because "this host
was taught no pane" is a claim every rig would stay green without.
