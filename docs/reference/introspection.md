# Introspection — the `Loaded` pane

> **What is this Loom actually running?**

Introspection is a Zengine package (`introspection/`) that builds one loadable weave,
`zengine-introspection`. It holds the office `zengine.introspection` and offers Workshop one pane
through the [external pane protocol](../guides/make-a-workshop-tool.md#part-b--an-office-authored-external-pane):

```text
PaneRef      zengine.introspection / loaded
picker row   Loaded    closed    what the kernel has loaded, and each one's role
pane header  Loaded @zengine.introspection
```

`zengine-workshop` boots it beside the Skin, the input reader and the Timer, so a maker opens it
from the panel picker (`p`) and can keep it in a saved setup like any other pane.

## What it shows

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

## Where each fact comes from

| fact | authoritative owner | how Introspection gets it | current or retained | absence means |
|---|---|---|---|---|
| which libraries are loaded | the **Kernel**'s live map (`Kernel::loaded()`) | `zen.ListLoaded` → the Weave Manager → `zen.ListLibraries` → the control door → `zen.Result` | a **snapshot**, re-read on each room grant | the map was empty when it was read |
| the role each holds | the **Kernel** (`Kernel::role_of()`), same answer | same message | same snapshot | that library was bound to no role |
| that a weave is *not* listed | nobody — **not observed** | — | — | it is not in the kernel's map. It may still be running |

That last row is why the pane always carries `in-process weaves are not in the kernel's map`.
Workshop's own weave, the boot weave, the control door, the Weave Manager, the Builder tool, the
build runner and the terminal participant are all live participants that `zen.ListLoaded` does not
enumerate and cannot speak about. A count without that sentence would be an honest number leaving
a false picture, so the sentence is reserved out of the pane's row budget **before** the list is
offered anything but its first row.

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

## Currency: a snapshot, and the pane says so

Loom gives a participant no arrival or departure event — `zen.Activated` is directed at the
activated weave alone, and the bus tap is a host facility rather than a participant door. So there
is nothing to subscribe to, and a consumer that polled for one would be a consumer polling.

Introspection re-reads the map on exactly one beat: **Workshop granting the pane its room.** That
happens when the pane opens, when a valid re-offer refreshes it, and when the resolved prose
capacity changes. Between two grants the rows are a reading, not a feed, and the last line of the
pane says which.

There is no refresh button. A pane can be pressed (below), but a press is a gesture *about a row*
and this tool does not read one as "go and look again" — that would be a second beat with no
sentence saying so. Closing and reopening the pane, or resizing it enough to move its prose
capacity, re-reads.

## Selecting a row

A maker can press one of the entry rows. The pressed row is marked, and the pane publishes an
ordinary Loom message saying which entry was selected:

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
  offer Workshop one pane                     PaneOffered, as its own office
  publish rows inside the grant it was given  PaneContent, as its own office
  state which row a maker selected            LoadedSelected, as its own office

CANNOT (never sent, and not in its declared Emit set)
  send the SELECTED weave anything            naming a thing is not reaching it
  load, reload, swap or unload anything       zen.LoadWeave / SwapWeave / ReloadWeave /
                                              UnloadLibrary / UnloadRole
  reach the kernel's control door directly
  publish a canvas, a text slot or any screen
  read or write the document, the setup, or any file
  start a process, open a socket, hold a timer, read a Sense
  grant itself anything
```

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

The pane's default room is 8 prose rows, which holds a heading, four weaves, a blank row and the
two notes — the shape above, exactly. Beyond that the list is **windowed, never truncated**: entries
are shown until the budget runs out and the remainder is counted on its own row (`... 17 more`), so
every weave is either named or counted. A name too long for the granted columns is cut with `...`.

A maker who wants to see more authors a bigger window for the pane (`w`, then `s`); that is a room
change, which is a fresh reading.

## Reading the source

```text
introspection/vocabulary.hpp   the durable PaneRef halves, the two picker lines, LoadedSelected
introspection/loaded.hpp       the pure core: parse the Manager's answer, spend the budget,
                               map each row back to the entry it names, move the mark
introspection/introspection.cpp the weave: when to observe, whom to believe, what a press means
```

`loaded.hpp` links nothing and knows no bus, so what a reading *means* is provable over a value —
and the row-to-entry map is returned by the function that *builds* the rows, so there is no second
calculation for a press to disagree with.

The suite's INTR-0 and SEL-0 tiers do both halves: the pure projection over a swept domain of
populations and budgets, and the real `zengine-introspection.so` loaded through the real Kernel and
Manager, pressed through the real input path, with its rows read off a published canvas and its
selection fact heard by an independent listener.
