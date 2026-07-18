# Zengine

Zen's **default set** — the engine-shaped weaves most projects will want, built on the Loom.

## The three tiers

**The Loom is everyone's, Zengine is the default set, your weaves are yours.**

The Loom is the substrate: values, schemas, the gate, the switchboard. Zengine is one
opinionated set of weaves built on it — a set you can take, replace piece by piece, or ignore
entirely. Your own weaves sit alongside Zengine's as peers, not as plugins into it.

This repository is the middle tier, and it is **separate from the Loom on purpose**. Zengine
consumes the Loom exactly as a stranger would, so every rough edge in the Loom's public surface
hits the house before it hits a guest. The dependency arrow is structurally un-invertible: the
Loom's build cannot see Zengine.

## How this consumes the Loom

**Default — the stranger's path.** `find_package(loom)` against an installed, exported Loom:

```sh
# in Zen/Loom — build and install the Loom
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$PWD/build/_install"

# in Zen/Zengine — consume it
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install"
cmake --build build -j"$(nproc)"
ctest --test-dir build
```

**Dev override — the sibling path.** For when the Loom and Zengine are edited together:

```sh
cmake -S . -B build-dev -DZEN_LOOM_DEV=ON   # add_subdirectory(../Loom)
```

Both paths expose the **same target names** (`loom::core`, `loom::switchboard`) — the Loom's
export sets `EXPORT_NAME` to match its in-tree aliases — so the override is a genuine drop-in
and the two paths cannot silently come to mean different things.

The default is deliberately the less convenient one. In dev mode the *whole* Loom build tree is
reachable, including targets left out of the exported surface on purpose (the UI trio, the
console, the TUI, the bridge, the SDL skin — all Zengine-destined, each moving in its own port
phase). A mistaken dependency on one of those compiles happily in dev mode and breaks for a real
consumer. Defaulting to the exported path means that mistake surfaces here, in the house.

## `reference/` — the read-only quarry

`reference/` holds the **V1 Zen engine**: a quarry to read, not a codebase to port. Nothing in
it is built by this repo. Ports are read-and-rewrite, phase by phase, landing in their proper
home from birth — never a lift-and-shift.

*Provenance:* a plain file import of the V1 repo's **working tree** (taken 2026-07-18, including
changes uncommitted there at the time), not a history-carrying subtree split. Its git history
stays in the original repo (`Krealsion/Zen`). Dropped at import: the Python virtualenv, the
derived `all_code.txt` concatenation, and editor/agent cruft.

## Test discipline

**Per-repo green.** Zengine's lane runs Zengine's tests against its pinned/installed Loom and
**does not re-run the Loom's suite** — a dependency's proof rides its version. Every report-back
states *which repo's green was proven*; "green" must never silently mean "green in one of two."

*Honest today-note:* the Loom is still under active development, so during Loom phases its
delegated-scope suite still runs there, per phase. The don't-re-prove economy arrives as the
Loom stabilizes; the structure is ready for it now.

Today Zengine's green is one smoke test: link the Loom's exported surface, drive a value through
the real gate, and confirm the gate **refuses** a malformed candidate. That refusal is what makes
it a proof instead of a greeting.

## Working in this tree

Zengine and the Loom live side by side under a shared `Zen/` root:

```
Zen/
  Loom/        the substrate — everyone's
  Zengine/     this repo — the default set
  playground/  Josh's own weaves
```

Assistant sessions are launched from the **`Zen/` root**, never from inside a sub-repo: the
memory graph is keyed to that path, so launching from `Zen/Zengine/` silently cold-starts
without it. Run git per-repo (`git -C Loom …`, `git -C Zengine …`).
