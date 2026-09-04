# Agent law — Verification

Routed detail behind [`AGENTS.md`](../AGENTS.md) — the lanes past the commands, the population
contract in full, and the repository's self-checks. The lane commands themselves are in the
core [`AGENTS.md`](../AGENTS.md#build--test-canonical-wsl-consumes-an-installed-loom); public
reference: [`../docs/contributing/build-and-test.md`](../docs/contributing/build-and-test.md).
Phase tags like (W-3a) are provenance markers into this repository's history; the law here is
current.

## The lanes, and what each can see

- **Run `tests/verify.cmake`, not a bare `ctest`**, whenever a result is going to be quoted. A
  bare `ctest` still works and still runs the tests; what it cannot tell you is whether the
  population that ran is the population this repository meant to run. Pass extra CTest flags
  with `-DZEN_CTEST_ARGS=-V`.
- **Stranger-by-default is deliberate** (`ZEN_LOOM_DEV=OFF`): an unexported-surface mistake
  must fail on every machine, not only in CI.
- **Per-repo green**: Zengine's lane never re-runs Loom's suite — state which repo's green you
  proved, which configuration, which compiler.
- **Windows is two standard libraries, and a Windows claim names the one it was measured on.**
  MinGW-w64/libstdc++ is the maker's daily build; its lane (`Zengine / Windows / MinGW-w64
  Loom stranger`) is REQUIRED. MSVC's STL is the toolchain released Windows users are expected
  to build with; its lane is ADVISORY (`continue-on-error`) until it can be proven locally as
  a matter of course. Both are supported in the long run, and neither is evidence for the
  other: measured on the Files browser, libstdc++ reports a directory junction as `directory`
  unfollowed and does not implement `create_directory_symlink`, MSVC's STL reports the
  junction — which is why the Windows branch asks the host (WL-FILES-04); and on a dangling
  junction MSVC's STL fails the kind-ask while libstdc++ answers `directory` from the cached
  entry and never asks — measured in the comment on the WL-FILES-14 second-clause case in
  `tests/test_workshop_files.cpp`, not in the register, whose MEANS names neither library. A
  job under `continue-on-error` is red only in the run's jobs list, never in its conclusion,
  so read the jobs. "Green on Windows" is not a result; "green on Windows/MinGW-w64 GCC
  13.1, Debug, SDL off" is.
- **CI runs on pull requests and `main`; a branch push alone gets no run.**
- **Weave libraries go through `zengine_weave()`**, which delegates the reloadable lifetime to
  the Loom's `loom_weave_build_contract()` (KERN-05). Do not reintroduce a private compiler
  flag for it here. A provider that is not a weave goes through `zengine_provider()`
  ([`operators.md`](operators.md)).
- **The package is a third kind of evidence, and neither ordinary lane can ask its question.**
  Both reach Zengine through its own build tree, where every header is on the include path and
  every artifact is staged; a requirement the *package* fails to carry is invisible there. Run
  `tests/package/run.cmake` after touching any public header, any exported target's link line,
  or `cmake/ZengineInstall.cmake`. It is not a CTest entry and `verify.cmake` will not run it
  for you. See [`packaging.md`](packaging.md).
- Suites are separate binaries (`zengine-timer-tests` etc.); ctest runs them all, plus
  compile-negative targets judged on their diagnostics.
- **The lane may be run in parallel on Linux/GCC** — `-DZEN_CTEST_ARGS=-j<n>`; measured
  56.2 s → 9.4 s at `-j24`, same proofs (ZOOM-P1, halved again by ZOOM-P2). **Windows/MSVC
  stays serial**: `timer` fails there under `-j16` on `REQUIRE(swapped != nullptr)`
  (`tests/test_timer.cpp`), 2 of 6 runs on current main and 4 of 6 on the commit before the
  compile-test repair — an open defect in weave loading under concurrent processes, measured
  as untouched by that repair rather than caused by it. Serial is green there.
- **That same defect reaches Linux too, and what decides it is the BUILD TREE'S FILESYSTEM,
  not the platform (QR-13).** The identical assertion at `tests/test_timer.cpp:1721` fails
  4 of 4 parallel runs of the sanitizer lane at `-j24` when the tree is on `/mnt/g`, and
  0 of 3 on an ext4 tree built from the same source — and it still fails 3 of 3 with the
  `workshop_panes` entry excluded, so it is the defect and not whatever suite is nearby. It
  is load-sensitive rather than absolute (`-j8`: one fail, one pass), and **serial is green
  on both**. So "Linux/GCC parallel is safe" is a claim about a FAST tree; quote the
  filesystem with the parallel number, and fall back to serial before suspecting your own
  change. A `/mnt/g` tree is also ~2x slower to build than ext4 (measured 3m20s vs 1m29s,
  same source, same `-j24`), which is reason enough to keep the working tree off it.
- **A parallel lane's floor is its LONGEST ENTRY, not its total.** What the parallel number
  buys is bounded by whichever single entry runs longest, so the thing worth watching is a
  suite growing past its neighbours — and the thing worth knowing before optimizing is which
  entry that is. Ask CTest, not the suite count.

## The Workshop family is seven entries, and the split is semantic (ZOOM-P2)

`tests/test_workshop.cpp` had reached thirty thousand lines behind one CTest entry: thirty
seconds of compiling before an assertion could run, and a nineteen-second test the scheduler
could only ever hand to one worker — a floor neither the machine nor the population explained.
It was cut into six sources along the headings the file already had, and each later phase
that opened a genuinely new area has added one:

```text
workshop_document      the authored material and the maker's hands on it
workshop_screen        composition and geometry -- what is painted where
workshop_panels        the panels Workshop ships, and the attention surface
workshop_panes         the external pane seam, from both sides
workshop_persistence   what survives a process
workshop_load          which artifacts are in the room at all
workshop_editor        the built-in source editor and its document
workshop_files         where source comes from: the project browser, what a
                       project-relative recipe source means to editor and build alike,
                       and who owns the completed recipe catalog for the session
```

- **Pick the one your change can falsify** and build that target alone: a Workshop test edit
  costs one suite's compile now, not the whole file's.
- **A SUITE IS NOT A FILE (QR-13).** One CTest entry runs one binary; how many translation
  units that binary is built from is a compiler question, not a population one. `panes` is
  five sources — `_seam`, `_window`, `_input`, `_introspection`, `_sampling` — under the one
  `workshop_panes` entry and the one floor, because one MinGW Debug object could no longer
  name all of its instantiations (see the platform traps below). Sources go in the suite's
  `SOURCES` list, which is fail-closed: they share the target's definitions, includes and
  link line by construction, and a source left out of it is not compiled — its cases then go
  missing from the population, which the floor catches and the build does not.
- **`tests/workshop_support.hpp` holds what more than one of them needs** — fixtures, canvas
  readers, the `Live` and `PaneRig` rigs. A helper one suite uses stays in that suite's file.
  It is not free: measured, the header adds ~0.4 s of parse to a translation unit that only
  reads it, and roughly 5–8 s to one that USES it, because every suite emits the helpers it
  calls. Editing it rebuilds five suites. Moving a helper into it is a decision.
- **A temporary directory belongs to the suite that made it, in the process that made it.**
  The suites run at once and every `TempDir` counter starts at zero, so the root carries
  `ZENGINE_WORKSHOP_SUITE` and the process id — two processes of one suite (an MSVC build and
  a MinGW build side by side) otherwise sweep each other's directories; a case in
  `workshop_persistence` asserts both rather than trusting them. The sweep is the fixture's
  own `remove_tree`, which asks `leaves_the_tree` of every entry and removes a link by name
  without entering it: libstdc++ on Windows walks a directory junction as a directory,
  emptying the target through it and leaving a dangling junction behind when the target went
  first, which cost every second local MinGW run its junction witness. A root a crashed run
  left behind is another process's and is left alone. Nothing else in the family shares a
  filesystem path — the load-plan stage directory is `workshop_load`'s alone.
- **The six original floors sum to what the one entry's was**, and the editor's floor is its
  own new evidence on top — per-area numbers instead of one, so a deleted case names the
  area it went missing from.

## The compile-judged entries build in a tree of their own (ZOOM-P1)

A compile test's evidence is a build, so the entry is a WRITER of whatever build tree it names.
All eight of them used to name `${CMAKE_BINARY_DIR}`, which made eight independent proofs into
eight competing owners of the one tree the whole repository shares — invisible serially, and
reproducibly destructive under `ctest -j` whenever CMake owed that tree a regeneration
(measured: 5–7 of 22 entries failing, 3 of 3).

`tests/compile_negative/CMakeLists.txt` holds the fixture targets, and `tests/CMakeLists.txt`
configures them into `<build>/tests/compile-fixtures` — this same project, entered under
`ZENGINE_COMPILE_FIXTURES`, with `BUILD_TESTING`, `ZENGINE_SDL_SKIN` and `ZENGINE_INSTALL` off
and `CMAKE_SUPPRESS_REGENERATION` on. Three consequences worth knowing before touching it:

- **The fixtures compile exactly as an in-tree target would**, because the tree they are in is
  this project: one `zengine-warnings`, one `zengine-sanitize`, one vocabulary per package, and
  the compiler, its launcher, the flags, the build type, the sanitizer switch and the resolved
  Loom all passed through. A private fixture project would have needed a second copy of all
  three, and a second copy of a contract is a second answer.
- **No FetchContent lives in that tree**, so no compile test can inherit a dependency
  population or leave one half-extracted. That was the second half of the defect and it is why
  the SDL skin is off there.
- **It cannot re-run CMake** (no `RERUN_CMAKE` edge is generated), and one entry writes it at a
  time (`RESOURCE_LOCK`, free here: the eight serialize into ~9 s against an ~19 s critical
  path). The two protect different halves — a lock alone leaves the entries owning the tree
  they lock, which is what makes the regeneration cost unbounded rather than absent.

It is configured during **configure**, not as a build step, because this lane has no target a
build-time dependency could hang from: configure, `--build --target <one suite>`, run a compile
test, and the entry would fail on `compile-fixtures is not a directory`.

**The same rule reaches inside one test process (BQR-0).** A build tree has ONE owner while a
build is in it, and that is not only a `ctest -j` question — the Builder suite has a case that
starts two builds at once on purpose, and both of them named the same fixture tree. Ninja's
`.ninja_log` recompaction writes one fixed temp name per directory, so two builds entered
concurrently there can collide and one aborts before it reaches its target: measured 2 of 400
children on a settled log, 9 of 40 with the log over the recompaction threshold, and 0 of 400
once each operation had a tree of its own (`tests/CMakeLists.txt` configures `build-fixture` and
`build-fixture-b`). Unix Makefiles showed 0 of 120 — the hazard is the generator's private
bookkeeping, so a suite that is green under one generator says nothing about another. **Ask how
many builds a case runs at once, not only how many entries CTest runs at once.**

## The sanitizer lane is a SECOND kind of evidence (W-3a)

`-DZENGINE_SANITIZE=ON` (ASan + UBSan, non-recovering) runs the same population under
instrumentation; CI runs it on pull requests and on pushes to `main`. It is not a second
correctness lane and it does not replace `verify.cmake` — the ordinary lane asks whether the
intended population ran and passed, which cannot see a defect whose symptom is that *no answer
changes*. Measured, both ways round:

```text
a Placed bound into a temporary Scene   ordinary PASSES   ASan  heap-use-after-free
resolve_extent without its guard        ordinary PASSES   UBSan signed integer overflow
```

- Both defects shipped green on the ordinary lane before this lane existed — one in committed
  test code, one signed-overflow UB in shared `ui/` code on the ordinary press path. That is
  why this lane exists.
- **A new target lists `zengine-sanitize` beside `zengine-warnings`.** Omitting it does not
  fail anything — it silently drops that target out of the witness, which is the one way this
  lane degrades.
- It instruments what this repo AUTHORS, not the Loom it consumes (the Loom runs the same lane
  over itself). ASan's allocator is process-wide, so a Loom allocation misused by Zengine code
  is still caught.
- It runs the **full** population, SDL skin included — the verifier prints
  `gates active: always;sdl`, and every floor is the one the ordinary lane clears. Do not
  lower a floor or drop a gate to buy instrumentation. The `sdl` gate opens the embedded
  typeface through SDL_ttf and FreeType against a real renderer, so this lane is where a leak
  or a misuse in that lifetime would be named.

## The population contract (C4, POP-01/POP-02)

A green here means the intended test population existed and ran. Four things have to hold, and
`tests/verify.cmake` requires all four:

```text
the declared CTest entries exist, exactly — no more, no fewer
each doctest surface selects at least its declared case floor
a doctest run that selects zero cases is a FAILURE
the tests themselves pass
```

- `tests/test_population.txt` is the **expectation** — entry, kind, gate, and either a case
  floor or the diagnostic a compile-negative test must be judged on. It lives in the source
  tree precisely so that deleting a registration cannot delete the expectation with it.
  Adding, renaming or removing a CTest entry is a deliberate edit to that file.
- Register tests through `zengine_doctest_test()` / `zengine_compile_test()` /
  `zengine_program_test()` (top-level `CMakeLists.txt`), never a bare `add_test()`. The
  helpers record what kind of evidence the entry is; the verifier refuses an entry it finds
  registered and unrecorded.
- `tests/doctest_main.cpp` is the one `main()` every runtime suite links. Stock doctest exits
  **0** on `--test-case=<no match>` ("Status: SUCCESS!"); this one exits **70** and says
  `EMPTY TEST POPULATION`. The verifier re-proves that on every run, per binary, with a filter
  that matches nothing.
- Floors are **minimums** anchored to a measured baseline. Additions are free; a deletion is a
  red. A phase that adds cases raises the floor to the measured count, so its own cases are
  under the contract from the commit that added them. Do not lower a floor to make a deletion
  pass. The per-suite values live in `tests/test_population.txt` and nowhere else — a
  convenience copy of them kept here went stale twice before that rule was made. A second copy
  of a contract is not a convenience; it is a second answer.
- **Moving evidence between suites does not lower the source suite's floor.** When a
  vocabulary relocates to another package, what its operations DO becomes the new suite's
  claim, and the old suite keeps a case for each proving its own answers come from there — so
  the old floor rises or holds. A relocation that made the old floor fall would have moved the
  guarantee out of watch, not out of the file.
- **Assertion totals are evidence to report, never a population, never an acceptance oracle,
  and not coverage.** The figure is configuration-dependent (gated suites carry fewer cases
  where SDL is off), so it travels with the lane it was measured on, and it is dated because
  nothing enforces it: no contract file holds assertion counts, and a phase that adds a case
  moves the number without anything noticing. The repository's history shows every failure
  mode of reading it as more: convenience copies of it decayed in place (twice, including the
  arithmetic); a single property-sweep case can move it by ten thousand while pinning ONE law;
  a phase of distinct laws can move it by hundreds while changing far more of the repository;
  and cases moving between suites make one suite's total fall while the repository's rises.
  Report the total with its date and lane; read the per-suite floors in
  `tests/test_population.txt` as the contract.
- Configuration-dependent populations are **declared**, not absorbed: the SDL-gated cases are
  their own manifest rows, so a suite's floor is the SUM of the rows whose gate is active and
  the Windows lane's `-DZENGINE_SDL_SKIN=OFF` simply drops the `sdl` row, with no slack in
  either configuration.
- The verifier verifies the **configured build tree it is handed**; producing a current one is
  the job of whoever configures and builds.

## The suites need a Loom that can host weaves (POP-03)

Every suite but `smoke` drives real weave libraries through the real kernel, so
`BUILD_TESTING=ON` (the default) **requires a Loom exporting `loom::kernel`** — always present
on Linux; on Windows only under the Loom's opt-in `LOOM_ENABLE_WINDOWS_KERNEL`, which
`-DZEN_LOOM_DEV=ON` sets for you.

Against a kernel-less package, `tests/` **fails configuration** with an actionable message. It
used to `return()` quietly, and `ctest` then printed "100% tests passed" over the one surviving
smoke test — from the *supported default* Windows Loom package, not from a typo. See
`../Loom/docs/laws/population-laws.md` (sibling checkout; substrate truth is the Loom's).

`-DBUILD_TESTING=OFF` is the supported library-only configuration: it gates the tests and
nothing else. Against a kernel-full Loom it still builds every weave library and registers no
tests; against a kernel-less one it configures and builds the kernel-independent surface (the
activation cursor and the header-only vocabularies).

## The repository checks its own documentation, vocabulary and law

- **`doc_links`** (kind `script`; `tests/check_doc_links.cmake`). Every relative link in a
  current-facing `*.md` and its `#anchor`, plus every repository-relative `*.md` path written
  in a first-party C/C++ comment under any package directory or `tests/`, must resolve — a
  broken one is a RED in the official lane. Markdown is swept from the repository root, so a
  new documentation folder is covered the moment it exists; a comment's reference is resolved
  against the **repository root**, because a comment moves with its code. Excluded by written
  rule: `docs/history/` (frozen), `reference/` (the pre-Zen quarry), vendored and build trees.
  A reference above the repository root — including anything under `../Loom/` — is counted and
  declined: this repository is verified as a standalone clone. The same entry reads every
  current-facing file of a text kind — documentation, source, CMake, manifests, workflows —
  whole, for a path outside the repository (the maintainers' workspace, its drive or mount, a
  tool's scratch directory, a home): one is a RED, the remedy is words, and the spellings it
  looks for are declared in the check itself, which with the package witness's forbidden-word
  list is the only file allowed to carry them.
- **`package_vocabulary`** (kind `script`; `tests/check_package_vocabulary.cmake`). An
  **artifact** is the physical loadable file; a **weave** and a **provider** are runtime
  surfaces one may expose, and the installed package holds both kinds, so its public variables
  name the physical thing. The retired spellings may appear in exactly one file — the checker
  that declares them — and the check asserts both halves of that. It does **not** police the
  word *weave*: `zengine_weave()`, `WeaveId`, the weave ABI and weave-only guides all mean
  weave and must not be renamed. Its self-test makes the predicate say **yes** to a token in
  the tree and **no** to one that is absent before it answers.
- **`law_register`** (kind `script`; `tests/check_law_register.cmake`). Workshop's law is
  written once, in the registers under `agents/workshop/`, and this entry keeps the form and
  the names honest: every `##` is a `WL-` entry or the one `## Do not assume`; ids are unique
  under `agents/`; LAW is one line of at most 210 bytes, MEANS at most 3 and DOES NOT MEAN at
  most 2, no SINCE line; every PROVEN BY path exists, every backticked identifier occurs in the
  file named before it, every quoted witness is a `TEST_CASE`/`SUBCASE` literal under `tests/`;
  every WHY target exists and each record's **Laws supported** is exactly the laws whose WHY
  names it; every `// WL-…` pointer names entries of the register on its line and every
  `// Workshop law:` header names existing files; registers ≤ 16 KB, the router ≤ 8 KB,
  `AGENTS.md` ≤ 20 KB; a law that writes `witness: none`, or `UNWITNESSED — <clause>` for the
  one clause no case pins, is repeated under its register's `## Do not assume`, and only such a
  law is. Two stricter checks — an identifier must occur
  as a whole token in the named file's *code*, comments stripped, where a member spelled
  `Struct::member` and an overload spelled `name(Type)` are read as their parts; and the
  declaration under a pointer must be named by **every** law on that line — sit behind
  `LAW_REGISTER_STRICT`, ON by default since their lists were cleared;
  `-DLAW_REGISTER_STRICT=OFF` prints the lists and a count without failing, the setting for a
  phase working one down. It cannot know that a phase edited a witnessed test; that rule is
  procedural and lives in the router ([`workshop.md`](workshop.md)). By hand:
  `cmake -P tests/check_law_register.cmake`.

## A check that reads the tree, and a pass that rewrites it

- **Grep code, not comments.** A whole-file grep for an identifier is satisfied by a mention
  in a comment: 24 wrong attributions survived three register steps that way, and 14 remained
  once comments were stripped. A check that asks whether a file *declares* or *spends* a name
  strips `//` and `/* */` first and matches a whole token — `Rect` is not inside
  `SurfaceRect`. `law_register`'s rule m is the model.
- **A detector keys on a marker's VALUE, never on a name's presence.** A mechanism that
  recognises an authored marker — a macro-emitted flag member, a sentinel field, a naming
  convention, an attribute, an environment flag — asks what the marker says, not whether it
  exists: a presence check fails OPEN, and in the widening direction. The Loom's
  `shape_access_bits<T>()` (`include/zen/weave/shape.hpp`) is the model: the presence check
  `requires { T::zen_expose_all; }` was satisfied by a hand-written `zen_expose_all = false`
  and by a state field that merely bore the name, exposing every field for writing; the nested
  `requires { requires T::zen_expose_all; }` demands a true constant expression, and the Loom's
  `tests/test_poke.cpp` pins the `= false` twin that must NOT trigger. Write that negative twin
  beside every marker check. `TerminalSize::measured()` ([`surface.md`](surface.md)) is the same
  posture on a number, and `law_register`'s self-test — a commented-out identifier refused — is
  the same posture on a grep.
- **A CMake script that splits text into a list has four characters to fear** — `;`, `[`,
  `]` and `\` — and the swap that neutralises them is stated once, in the header of
  `tests/check_law_register.cmake`; read it before writing another tree-reading check. Two
  companions from the same script: `if(x MATCHES …)` inside a nested loop rewrites
  `CMAKE_MATCH_n` for the enclosing scope, so copy a match into a variable before looping; and
  an empty population is a red, never a quiet pass.
- **A tree-reading check's wall clock is its filesystem's.** The same `law_register` script
  measured 9.75 s on a 9p-mounted source tree, 1.99 s on an ext4 copy and 2.5 s on the
  Windows host, so a number quoted without its filesystem says nothing. Quote the tree's
  filesystem with the number, as the parallel-lane note above already does.
- **A mass edit over the tree goes sheet → applier → proof, regenerated from the start
  commit.** The sheet is the decisions, one row per declaration, and it is the review; the
  applier applies them from a clean checkout of the base commit every time, so a rerun
  converges; the proof is mechanical and travels with the commit — for a source, strip
  comments and blank lines and `diff` against the base (empty), and extract the string
  literals (byte-identical); for a register, everything outside the paragraph edited is
  byte-identical. Two lessons from the register passes: a sheet for a per-line check carries
  the **whole pointer group** above a declaration, not the flagged line, because a flagged
  line's neighbour is a section banner or a content citation and the decision is about the
  group; and an index over a file — entry line ranges, a cached parse — is **stale at the
  file's first rewrite**, so resolve fresh on every lookup or re-index after every write. The
  symptom of a stale index is judgment-shaped (good lines marked for dropping), not
  tool-shaped.

## Which suite witnesses which area

The registers cite their witnesses by exact case name, so the answer to "which suite pins
this law" is a grep over `agents/workshop/`; this table is the current shape of that answer,
most-cited suite first, for choosing where a new case goes and which target to build. Keymap
and clipboard law is witnessed in the *document* suite, keyboard focus in *panes_input*, the
Info grounds in *document* — the suite is the subject the case proves, not the file the law
names.

| register | witnessed by |
|---|---|
| arrangement, chrome, geometry, planes, pointer, tab-run, terminal | `workshop_screen`, then `workshop_panes` (window) and `workshop_panels` |
| attention, info-body, info-controls, pane-manager | `workshop_panels`, then `workshop_document` |
| catalog, panes-and-windows, setup-file | `workshop_panes` (seam, window), then `workshop_screen` |
| contextual, press-chain | `workshop_panels`, `workshop_screen`, `workshop_document` |
| document | `workshop_document`; the file half (document-file) in `workshop_persistence` |
| editor, files, project | `workshop_editor` and `workshop_files`; project also `workshop_panels` |
| focus | `workshop_panes` (input), then `workshop_editor`, `workshop_files`, `workshop_document` |
| keyboard, text-box | `workshop_document`; text-box also `component` |
| layouts, migration, session | `workshop_persistence`, then `workshop_screen` and `workshop_panels` |
| maker-pane | `workshop_panels` (the creator source) |
| regions | `workshop_screen`, `workshop_document` |

## Driving a live graphical witness

A suite proves a law against a fixture; a live witness proves it against the real window, and
its harness is where the defects hide. The method, each rule bought by a witness that read a
working product as broken:

- **Control target first.** Before believing any gesture, make one whose effect you already
  know and assert it; a witness that cannot say what state it started in cannot say what it
  measured. Unstick every modifier and send Escape twice at the start — a modifier whose
  key-up an earlier run lost stays down for the whole desktop, and the next run's activation
  click arrives as a chord.
- **Launch with `--isolated`** (WL-SESSION-02): the per-user roots resolve to the designed
  absence, so a witness reads and writes nothing of the maker's own state.
- **The channel lies silently.** Build the input structs in compiled code, never by indexed
  assignment into a value-type array in a shell — the write is dropped, every event goes out
  all-zero, and the injection call still reports the full count. Prove the foreground with
  the window handle you were given, not with a thread's own queue facts; a screenshot of a
  window's rectangle photographs whatever is on top of it.
- **Aim lies too.** A prose row is the face's line height, not the canvas cell; an arrow key
  needs its scancode form; a keystroke's bounding box spans the notice row above the line, so
  crop every calibration diff to the region being calibrated. A TUI frame needs a cursor model
  (cursor addressing, carriage return and line feed, erase), after which the painted text is
  the best oracle either medium has.
- **Three oracles, none of them "did the process exit":** the window title for text, a
  before/after frame diff with a tolerance and the notice band cropped out for pixels, and a
  pixel count along a row for a claim that is a number about ink. Ask of every oracle what it
  would say if the act had no effect, and whether the ink you are measuring is produced by the
  act itself — an addressed pane draws handles on the rows a search was watching. Order the
  hypothesis-destroying reads last: a step that reads through the product's own channel may
  also write it.
- Working harnesses for both media are kept with the phase records, outside this repository,
  and are copied rather than rebuilt.

## The mutation harness

A green suite is nothing complained; the mutation matrix is the evidence that a case would
have. Rules, each learned by a harness that fooled itself:

- **Canary first**: run one mutation known to be caught before trusting the matrix; a matrix
  whose every row reads CAUGHT against a stale binary is the dangerous kind of wrong.
- **Snapshot the bytes at the start and restore by rewriting them.** A metadata-preserving
  copy (`copy2`, `cp -p`) restores an older mtime and rebuilds nothing, so mutation N runs
  against mutation N−1's objects — the restore must move mtimes. `git checkout -- <file>`
  over uncommitted work restores the predecessor's file, not yours. Key backups by full path:
  this repository has four `weave.hpp`.
- **Hash the artifact the mutation lands in**, not the one that runs — a weave library is
  loaded, so the test binary is byte-identical across its mutations — and refuse a verdict
  when it did not change; make the harness hash the source before and after the edit and abort
  when the pattern matched nothing. Build the whole tree for a library mutation.
- **A CAUGHT names the assertion that moved**, never the suite: a mutation to `weave.hpp`
  whose evidence is a `surface` assertion is visibly absurd instead of quietly plausible.
- **A `static_assert` that refuses a mutant is a catch of a different kind.** Relax it and
  mutate again, or the report cannot tell a doubled guard from a sole one.
- **A mask is usually a hole in an older suite.** A mutation that comes back green because
  every arrangement already satisfies the property is closed by arranging the missing
  condition — measured as the cheapest audit of past work available. A mutation that removes
  redundancy rather than a property is a third verdict, unexpressible, and is respelled at the
  property. A case that lands on a handler's deliberate `false` path proves nothing about the
  handler.
- **Never run a restoring harness beside another lane** on the same tree: a source-tree
  check reddens for a reason that is not your change.

## Platform traps

- **MinGW objects need `-mbig-obj` on `workshop.cpp`'s object, and that is not optional.** It
  instantiates enough templates that its `-g` debug COMDATs sit near COFF's 32-bit
  section-relative limit; one more inline function in a header it includes fails the link with
  `relocation truncated to fit: IMAGE_REL_AMD64_SECREL against .debug_frame$...`, which reads
  like a broken repository and is not one. `zengine-warnings` carries the flag under
  `if(MINGW)`. Do not remove it because a build happens to link without it today.
- **COFF has a SECOND, lower ceiling and `-mbig-obj` does not lift it (QR-13).** A section
  name over eight bytes lives in the string table, and the eight-byte name field holds `/`
  plus at most seven decimal digits — so the section-name string table stops at 9,999,999
  bytes, whatever the section count is. `-mbig-obj` widens the section NUMBER, not the name
  spelling. The two say different things, and the diagnostic tells you which one you met:
  `string table overflow at offset 10000097` / `file too big` is this one;
  `too many sections (N)` is the count. **Read the sentence before reaching for a flag** —
  the flag was already present when panes met this. **And it is the assembler's ceiling, not
  the format's**: the same GCC 13.1 assembly of `workshop.cpp` that binutils 2.40 (CLion
  2025.2's bundled MinGW) refuses as `string table overflow at offset 10000011` / `file too
  big`, binutils 2.45 assembles into a 50 MB object (measured 2026-09-02, unchanged source).
  The MinGW lane's runner carries a newer binutils, so it builds `workshop.cpp` while a CLion
  2025.2 toolchain does not, and internal linkage does not buy the object back under 2.40's
  ceiling (measured: a `static inline` predicate still overflowed).
- **What spends that table is instantiations per object, four names each.** Every
  vague-linkage function gets a COMDAT, and `-g` mirrors it into `.text$`, `.pdata$`,
  `.xdata$` and `.debug_frame$` — the same mangled suffix stored four times. Measured, a TU
  that includes `tests/workshop_support.hpp` and only constructs `PaneRig` and `Live` spends
  **69%** of the table before it asserts anything, and the Workshop objects run 63–86%. So
  the remedy is fewer instantiations per object — **another source under the same suite**,
  not a flag and not fewer cases. Splitting has a floor it cannot go below; do not expect a
  finer cut to buy proportionally more room.
