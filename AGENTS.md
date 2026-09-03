# AGENTS.md — Zengine

The contract for automated collaborators working in this tree. Docs router:
**`docs/README.md`** — every public page and its reader purpose, written for external readers
(`docs/contributing/repository-conventions.md` owns the external-reader rule, what may not
appear in a public page, and `doc_links`). Substrate truth belongs to the Loom:
<https://github.com/Krealsion/Loom/blob/main/docs/README.md>, machine router
<https://github.com/Krealsion/Loom/blob/main/docs/CONTEXT.md>; with a sibling checkout those
are `../Loom/docs/README.md` and `../Loom/docs/CONTEXT.md`.

## Read this, then route

This file is the one mandatory read before touching this repository. Everything else is
routed: read a surface's document when the task touches that surface, not before.

| the task touches… | read |
|---|---|
| lanes, suites, populations, CI, what a green means, platform build traps | [agents/verification.md](agents/verification.md) |
| `surface/` — the drawing vocabulary, grounds, planes, the TUI/SDL media, input backends | [agents/surface.md](agents/surface.md) |
| `workshop/` or `component/` — Workshop's law, one register per owner under `agents/workshop/` | [agents/workshop.md](agents/workshop.md), the router |
| the external pane protocol, `introspection/`, `composer/` | [agents/panes.md](agents/panes.md) |
| `operator/` — named rules, the catalog, the host/consumer seam, providers | [agents/operators.md](agents/operators.md) |
| load plans, realization, the load conversation, `builder/` | [agents/realization.md](agents/realization.md) |
| `cmake/ZengineInstall.cmake`, any public header, any exported target's link line | [agents/packaging.md](agents/packaging.md) |
| Timer semantics | [docs/laws/timer-laws.md](docs/laws/timer-laws.md) (TIMER-01..05) and the `docs/reference/timer-*.md` pages |
| the Input, UI or Snake packages | [docs/reference/input.md](docs/reference/input.md) · [docs/reference/ui.md](docs/reference/ui.md) · [docs/reference/snake.md](docs/reference/snake.md) |

**Where law lives (the re-accretion guard).** Surface law belongs in the routed surface
document that owns it; this core gains a rule only when the rule is genuinely cross-cutting —
needed for essentially any Zengine task. Routed documents hold **current law, rewritten in
place**: a phase updates the sentences its change made false rather than appending a
phase-titled account, and the evidence trail stays in Git history and the phase record, not
here. A phase that edits this core rechecks its budget: **this file stays at or under 20 KB.**
Executor/internal material (this file and `agents/`) stays out of the public documentation
index and out of the installed package.

**The register rules.** `law_register` enforces the form and the names; the router, the
procedure. **a** a claim lives in LAW, MEANS or DOES NOT MEAN, never the heading. **b** a span
a maker types or sees is a spelling, not an identifier. **c** PROVEN BY names functions, types,
constants and members under their declaring file. **d** a residue claim is LAW when it is the
invariant, else DOES NOT MEAN. **e** what a change did not touch is a change note, not a law.
**f** LAW text in a table escapes `|`. **g** no new phase tags; a tag in a `TEST_CASE` literal
is a fossil. **h** a record's Alternatives split tried (evidence inline) from argued.
**i** records wrap at 98 bytes, ~1.5 KB, over 4 KB flagged. **j** a record may link another.
**k** Laws supported is generated from the WHY lines: edit the WHY. **l** a record over ten laws
is suspected of being two. **m** an owner identifier is a whole token in the named file's code,
comments stripped. **n** a `// WL-` pointer is PROVEN BY inverted: every law on it names the
declaration beneath. **o** a mass edit is sheet → applier → proof, regenerated from the start
commit. **p** after a compaction, re-measure everything the report states. **q** a tree-reading
check self-tests and treats an empty population as red. **r** a check that reads the tree quotes
its filesystem with its wall clock. **s** a pointer carries only ids whose law names the
declaration; a member is `Struct::member`, an overload `name(Type)`. **t** a per-line check's
sheet carries the whole pointer group; an index over a file is re-resolved after every write.

## Build / test (canonical: WSL; consumes an *installed* Loom)

```bash
# once, in ../Loom:  cmake --build build -j && cmake --install build --prefix build/_install
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install"
cmake --build build -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build -P tests/verify.cmake        # the official lane

# sanitizer lane: the same suites under ASan + UBSan
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install" -DZENGINE_SANITIZE=ON
cmake --build build-san -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build-san -P tests/verify.cmake

# installed-package witness: install, then build an unrelated project against the
# prefix alone, OUTSIDE this repository
cmake -DZEN_BUILD_DIR=build -DZEN_WORK=/tmp/zengine-package -P tests/package/run.cmake
```

- **Quote `tests/verify.cmake`, never a bare `ctest`** — a bare run cannot say whether the
  population that ran is the population this repository meant to run.
- **The lane runs in parallel on Linux/GCC**: add `-DZEN_CTEST_ARGS=-j<n>` (measured 56.2 s →
  9.4 s at `-j24`, 27/27). Same proofs. It is safe because no CTest entry writes
  this build tree — the compile-judged ones build their fixtures in a tree of their own
  ([agents/verification.md](agents/verification.md)); an entry that took custody of
  `${CMAKE_BINARY_DIR}` would put that back, and the registration helper refuses one that tries.
  What sets the floor now is the longest single entry, so a suite that grows past its
  neighbours is the thing to watch, not the suite count.
  **Not yet on Windows/MSVC**: `timer` fails there under `-j16` about a third to two thirds of
  runs on a weave-load assertion, measured at the same rate before this was true of the compile
  tests, so it is a separate open defect and not this seam. Windows stays serial.
- **Stranger-by-default is deliberate** (`ZEN_LOOM_DEV=OFF`): an unexported-surface mistake
  must fail on every machine, not only in CI. `-DZEN_LOOM_DEV=ON` is the sibling override.
- **Per-repo green:** Zengine's lane never re-runs Loom's suite — state which repo's green you
  proved, which configuration, which compiler; never a bare "green", never a bare "Windows".
  Machine-specific facts — which tree, which filesystem, how many cores, what a given machine
  runs serially — live outside the repository.
- **The sanitizer lane is a second KIND of evidence**: the same full population under
  ASan+UBSan, for defects whose symptom is that *no answer changes* (lifetimes, extents,
  retained references). A new target lists `zengine-sanitize` beside `zengine-warnings`;
  omitting it fails nothing and silently drops the target out of the witness — that is the one
  way this lane degrades.
- **The package witness is a third kind**: both lanes above reach Zengine through its own
  build tree, so a requirement the *package* fails to carry is invisible to them. Run it after
  touching any public header, exported link line, or `cmake/ZengineInstall.cmake`; it is not a
  CTest entry and `verify.cmake` will not run it for you.
- Weave libraries go through `zengine_weave()` (which applies the Loom's reloadable-lifetime
  contract); providers that are not weaves go through `zengine_provider()`.

## The population contract

A green means the intended test population existed and ran. `tests/verify.cmake` requires all
four: the declared CTest entries exist exactly; each doctest surface selects at least its
declared case floor; a run that selects zero cases is a FAILURE; the tests pass.

- `tests/test_population.txt` is the expectation and the only home of the per-suite floors.
  Adding, renaming or removing a CTest entry is a deliberate edit to that file; floors are
  minimums, and lowering one to make a deletion pass is the thing this contract forbids.
- Register tests through `zengine_doctest_test()` / `zengine_compile_test()` /
  `zengine_program_test()`, never a bare `add_test()`.
- Every suite but `smoke` needs a Loom exporting `loom::kernel` (on Windows: the Loom's
  opt-in `LOOM_ENABLE_WINDOWS_KERNEL`); against a kernel-less package `tests/` fails
  configuration out loud. `-DBUILD_TESTING=OFF` is the supported library-only configuration.
- Three entries read the source tree rather than a build: `doc_links` (every repo-local
  documentation reference must resolve, anchors included), `package_vocabulary` (the
  installed package's nouns) and `law_register` (Workshop's registers: the form, and every
  name they make). Full contract detail: [agents/verification.md](agents/verification.md).

## Ownership and dependency direction

This repository owns Zengine's packages and Workshop; the Loom owns substrate truth
(messaging, lifecycle, replacement, capabilities), and Zengine consumes it as a stranger —
`find_package(loom)` against an installed prefix. Every Loom law applies here; see
`../Loom/docs/laws/`. The installed-package boundary (which targets are exported, which
headers install, which artifacts ride along) is owned by `cmake/ZengineInstall.cmake` alone.

## Do not assume

- Loom laws stop applying here — they all do, and substrate questions (bus semantics, grants,
  answer provenance) are answered by Loom's docs, not re-derived here.
- `Switchboard` has a bounded drain — `drain_until_idle()` is unbounded by contract, and it
  never returns on a process with the Timer service loaded (a live Timer re-arms its own beat
  inside its own handler). A host that wants control between turns wants `pump_pending()`.
- The delay a maker authors is the delay that is scheduled — it is normalized
  (`timer.normalize_delay`, [agents/operators.md](agents/operators.md)), and in an
  operator-hosting process the rule is resolved through the HOST's catalog, not the Timer's
  own image.
- A count is a population — assertion totals, delivery counts and queue sizes are evidence to
  report, never an acceptance oracle ([agents/verification.md](agents/verification.md)).
- A green build produced its artifact, a load made a weave live, a send was delivered — each
  is TWO facts wearing one word; the second half has its own owner and its own witness
  ([agents/realization.md](agents/realization.md)).
