# Introspection — the `Loaded` pane

> **What is this Loom actually running?**

Introspection is a Zengine package (`introspection/`) that builds one loadable weave,
`zengine-introspection`. It holds the office `zengine.introspection` and offers Workshop one
read-only pane through the [external pane protocol](../guides/make-a-workshop-tool.md#part-b--an-office-authored-external-pane):

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

There is no refresh button, because an external pane receives no input of any kind. Closing and
reopening the pane, or resizing it enough to move its prose capacity, re-reads.

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

CANNOT (never sent, and not in its declared Emit set)
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
introspection/vocabulary.hpp   the durable PaneRef halves and the two picker lines
introspection/loaded.hpp       the pure core: parse the Manager's answer, spend the budget
introspection/introspection.cpp the weave: when to observe, and whom to believe
```

`loaded.hpp` links nothing and knows no bus, so what a reading *means* is provable over a value.
The suite's INTR-0 tier does both halves: the pure projection over a swept domain of populations
and budgets, and the real `zengine-introspection.so` loaded through the real Kernel and Manager
with its rows read off a published canvas.
