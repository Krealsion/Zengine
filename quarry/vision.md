# Zen — Vision and Architecture

## The Pitch

Zen is a game and simulation engine built around three properties that no existing production engine combines: **hot-replaceable at every level, uncrashable by construction, and bit-deterministically replayable.** Each property exists somewhere in the field — Erlang has hot-reload and fault tolerance, lockstep RTS engines have determinism, Smalltalk has image-based development — but no shipping game engine has all three integrated. Zen is the integration. The novelty isn't in any individual concept; it's in the combination, and the bet is that the combination is large enough to matter even though no single piece is new.

## The Three Load-Bearing Properties

**Hot-replaceable at every level.** Any module — input driver, physics, renderer, UI widget, gameplay logic, the IDE itself — can be recompiled and swapped into a running process without stopping or restarting. State persists across the swap. Code can change ten times a minute; the simulation never pauses.

**Uncrashable.** A fault inside any module is contained to that module. The rest of the world continues running. The crashed module is paused, surfaced to the developer for repair, restarted, and rejoins the running simulation. The process itself never dies.

**Replayable.** Every simulation is deterministic. A save is an input log and a seed, not a state dump. Bugs are reproducible by construction. Time-travel debugging is a query, not a research project. A 4KB save file is a perfect bug report.

## The Architectural Spine

Two execution spaces, strictly separated.

**The Harness** is a minimal C++ kernel. Compiled once, rarely changed. It owns nothing interesting — no game logic, no rendering code, no assumptions about hardware. Its responsibilities are memory allocation, message routing, crash containment, time advancement, and supervisor decisions. The Harness is the steel plate everything else stands on.

**Shards** are dynamically-loaded libraries containing all actual logic — from low-level hardware drivers to high-level gameplay. Shards are hot-swappable in sub-second intervals. A Shard never holds a raw pointer to another Shard's data; all cross-Shard communication goes through the Harness.

Shards communicate through two complementary primitives.

**The Switchboard** is the async message bus. Shards emit typed messages targeted at IDs; the Harness routes them to inboxes in deterministic order. If the target Shard was unloaded or crashed, the message bounces safely — no dangling pointer dereferences. The Switchboard is for events, state changes, cross-system commands. It is the slow, flexible, decoupled communication mode.

**Senses** are zero-copy reads of named values exposed by other Shards. A Shard publishes `TimeDriver.delta_time` or `Camera.position`; other Shards bind to it by name and read directly at native C++ speed. Reads are epoch-protected — if the source Shard is severed or crashes mid-read, the read is discarded rather than returning corrupted memory. Senses are the fast, tight, throwaway communication mode.

You need both. Picking one ends in either slow systems or fragile ones.

## How "Uncrashable" Actually Works

Each Shard owns a private memory arena managed by the Harness. The Shard's allocations live exclusively in its arena. No Shard holds a pointer into another Shard's arena — cross-Shard references are stable IDs the Harness translates.

When a Shard faults — segfault, divide-by-zero, exception, hang — the Harness's signal handler catches the fault, identifies the active arena, and:

1. Marks every Sense pointing into that arena as severed (atomic flip; the next read fails safely).
2. Drops every pending Switchboard message addressed to the dead Shard.
3. Notifies the Shard's supervisor.
4. The supervisor decides: restart with persisted state, restart fresh, freeze for human inspection, or escalate the crash to its own parent.

Crash containment lives in the memory isolation, not in the signal handler. The signal handler is a tripwire on top of an isolation discipline. This is why "uncrashable" works at all — the corruption has nowhere to spread to.

## How Hot-Reload Works Without Losing State

Every Shard declares a **Domain** — the persistent memory schema for its state. The Harness tags the Domain with a hash of its structural definition.

When a Shard recompiles and reloads:

- Same Domain hash → the new Shard inherits the existing memory block. State survives.
- Different Domain hash → the Harness invokes the Shard's migration function, or wipes and reinitializes if no migration is declared.

The Shard's code is volatile and reloadable. The Shard's state is owned by the Harness and persists independently. State outlives the code that produced it.

## How Replay Works

Time is not ambient. `now()` is not callable from inside Shard logic. Every tick, the Harness provides `dt` as input. Every random number generator is seeded per-Shard and lives in the Shard's Domain. Every message has a deterministic ordering key.

Given the same seed and the same input log, the simulation produces bit-identical output every time.

The consequences ripple outward:
- A save file is `(seed, input_log)`. Kilobytes, not megabytes.
- A bug report is a save — it reproduces the crash exactly.
- The debugger rewinds by replaying from an earlier snapshot.
- Multiplayer netcode is lockstep-by-default, with rollback available as an optimization.

## What Other Engines Cannot Do

The following scenarios are not currently possible in any single shipping system. They are the reason Zen exists.

**The Live Patch.** A multiplayer game is running on a Friday night. A bug surfaces — a damage calculation is wrong. The developer fixes the formula on the server and pushes the new Shard. Connected clients verify the new hash, the new logic activates on the next tick, the match continues uninterrupted. Nobody disconnects. Nobody loses progress. Nobody knows a patch happened except via the changelog. Erlang upgrades telco switches this way. No game engine offers it.

**The Untrusted Mod.** A player installs a mod from a stranger on the internet. The mod is a Shard with its own arena. It can do anything it wants *inside that arena*. It cannot crash the host, cannot scribble on other Shards' memory, cannot read other state except via published Senses, cannot hold a pointer to anything it could corrupt. Modding becomes safe by construction — the way iOS apps became safe through sandboxing. This changes the economics of mod marketplaces.

**The Reproducible Crash.** A QA tester attaches a 4KB save file to a bug ticket. The developer loads it. The engine replays the exact input sequence at exact tick boundaries with the exact RNG seed. The bug reproduces on frame 8,341. The developer rewinds to frame 8,330, steps forward, sees the corruption enter on frame 8,338, fixes the code, hot-reloads, rewinds again, confirms the fix, ships. No "works on my machine." No flaky tests. No "we couldn't reproduce it." Every bug is a save file.

**The Self-Editing IDE.** The developer is using the editor and notices the layout panel is annoying. They open the editor's own Shard from inside the editor, edit the layout code, save. The layout Shard hot-reloads. The annoyance is gone. They never left their editing session. The Smalltalk-1976 demo, with modern hardware, on a real game engine.

**The Shared Time Slice.** Two developers debugging the same multiplayer crash. One is rewound to tick 8,200 inspecting why an entity was in a bad position. The other is at tick 8,341 inspecting the actual crash. They are in the same simulation, looking at it from different time slices, leaving comments anchored to specific frames. When the fix lands, both timelines re-derive forward and the bug is gone in both.

**The Frozen Subtree.** During gameplay, an enemy AI Shard throws an exception. The supervisor freezes that AI, surrounds it in a red glow in the running world, surfaces the stack to the developer. Physics, the player, other AIs all continue at 60 FPS. The developer fixes the AI's code; the supervisor restarts the AI with its persisted state; it rejoins the world without anyone else noticing. In Unity this is a crash dialog. In Zen it is a Tuesday.

## Deliberately Out of Scope for V1

- **The Zen language.** A custom DSL is a long-term goal but a multi-year project on its own. V1 ships with C++ Shards and a thin codegen layer for ergonomics. The language grows once the ABI is pressure-tested by real subsystems.
- **VR / spatial IDE.** The Iron Man interface is the long-term north star. V1 ships a flat 2D self-hosted IDE that demonstrates live-edit and replay. VR is a presentation layer added once the underlying system is real.
- **Content-addressed code distribution.** Hash-based dependency stability is in the design but not in V1. V1 uses conventional versioning.
- **A modern 3D rendering backend.** SDL3 is sufficient for V1 demos. Vulkan/DX12 comes post-V1.

The discipline of saying no to these now is what makes V1 shippable.

## The First Build Target

V1 is the smallest demonstration that proves the architecture works.

- The Harness, with crash containment, deterministic tick loop, and supervisor tree.
- The kernel ABI: Sense publish/subscribe with epoch protection, Switchboard send/route with deterministic ordering, Domain allocate/migrate with schema hashing, supervisor register/notify.
- Three Shards migrated from the existing zengine codebase: Input, Renderer, and one Game Shard.
- The cookie clicker app, ported to run as a Game Shard.
- Demo flow A: induce a crash in the Game Shard. See it freeze in red. Edit the code. Hot-reload. See it resume — all while Input and Renderer never stopped running.
- Demo flow B: record a session. Replay it. Verify bit-identical output across runs.

If both demos work convincingly, Zen has a foundation that earns the right to grow toward the long-term vision. If they don't, the problem is discovered before another 10,000 lines get written on a flawed assumption.

## Why This Project Exists

Most game engines optimize for building games efficiently. Zen optimizes for the developer's relationship with the system while it's running. The bet is that an engine where you can fix a crashed AI mid-game, push a patch without disconnecting players, send a 4KB bug reproducer, and edit the IDE from inside the IDE — is a fundamentally different tool than what currently exists. The difference is large enough to matter even if no single piece is novel in isolation.

The novelty is the integration. The thesis is that nobody has integrated these pieces because the integration is harder than it looks, not because the pieces don't fit. Zen is the bet that the integration is worth doing.
