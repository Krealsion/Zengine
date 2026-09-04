# Verification method — population

Register `VM-POP`: the population contract, the floors, the Workshop suites and what a split
creates. One method per heading; cite by ID. Which suite witnesses which Workshop area is the
table at the end. Router: [`../verification.md`](../verification.md).

## VM-POP-01 — A green means the intended population existed and ran

METHOD — A green means the intended population existed and ran: the declared entries exist exactly, each doctest surface clears its floor, a zero-case selection is a FAILURE, and the tests pass.
BECAUSE — the tests once returned quietly against a kernel-less package and the runner printed a
full pass over the one surviving smoke test; each of the four is a way that green was false.
SEEN — `tests/verify.cmake`; `tests/check_population.cmake`.

## VM-POP-02 — The population file is the only home of the floors

METHOD — `tests/test_population.txt` is the expectation and the only home of the floors; adding, renaming or removing an entry is a deliberate edit there, and a convenience copy elsewhere is a second answer.
BECAUSE — it lives in the source tree so deleting a registration cannot delete the expectation
with it; a convenience copy of the floors kept in a document went stale twice, arithmetic
included, before that rule was made.
SEEN — `tests/test_population.txt`.

## VM-POP-03 — Register through the helpers, never a bare add_test

METHOD — Register through `zengine_doctest_test()` / `zengine_compile_test()` / `zengine_program_test()`, never a bare `add_test()`: the helper records the kind, and the verifier refuses an unrecorded entry.
BECAUSE — the kind says what evidence the entry is, a case floor or a diagnostic; an entry with
no recorded kind is one the verifier cannot judge, so it is refused rather than passed over.
SEEN — `CMakeLists.txt` `zengine_doctest_test`, `zengine_compile_test`, `zengine_program_test`,
`zengine_record_test`; `tests/check_population.cmake`.

## VM-POP-04 — One main exits 70 on an empty selection

METHOD — One `main()` for every runtime suite exits 70 and says `EMPTY TEST POPULATION` on a selection that matches nothing; the verifier re-proves that per binary on every run.
BECAUSE — stock doctest exits zero and reports success on a filter that matched nothing, which
is how a suite whose cases were all deleted stays green.
SEEN — `tests/doctest_main.cpp`; `tests/check_population.cmake`.

## VM-POP-05 — Floors are minimums anchored to a measured baseline

METHOD — Floors are minimums anchored to a measured baseline: a phase that adds cases raises the floor to the measured count in the same commit; never lower one to make a deletion pass.
BECAUSE — additions are free and a deletion is a red only while the floor sits on the measured
count; a floor left below it is slack that hides exactly that many deletions.
SEEN — `tests/test_population.txt`.

## VM-POP-06 — Moving evidence between suites does not lower the source floor

METHOD — Moving evidence between suites does not lower the source suite's floor: the old suite keeps a case per relocated claim proving its answers now come from there.
BECAUSE — a relocation that made the old floor fall would have moved the guarantee out of watch,
not out of the file; the old suite's case per claim is what keeps its answers its own.
SEEN — nowhere yet

## VM-POP-07 — An assertion total is evidence, never a population

METHOD — An assertion total is evidence to report with its date and lane, never a population, an acceptance oracle or coverage; a queue size or a delivery count is the same kind of number.
BECAUSE — nothing enforces it and it is configuration-dependent: convenience copies of it
decayed in place twice, one property sweep moves it by ten thousand while pinning one law, and a
case moving between suites makes one total fall while the repository's rises.
SEEN — nowhere yet

## VM-POP-08 — A configuration-dependent population is declared

METHOD — A configuration-dependent population is DECLARED: the gated cases are their own manifest rows, and a suite's floor is the sum of the rows whose gate is active, with no slack either way.
BECAUSE — the Windows lane's `-DZENGINE_SDL_SKIN=OFF` simply drops the `sdl` row, so the same
green means the same thing on both lanes and no slack is left in either.
SEEN — `tests/test_population.txt`; `tests/verify.cmake`.

## VM-POP-09 — The verifier verifies the tree it is handed

METHOD — The verifier verifies the configured build tree it is handed; producing a current one is the job of whoever configures and builds.
BECAUSE — a verifier that configured or built would own the tree it judges, and a judge that is
also a writer can be handed a regeneration it did not ask for.
SEEN — `tests/verify.cmake` `ZEN_BUILD_DIR`.

## VM-POP-10 — Every suite but smoke needs a Loom that can host weaves

METHOD — Every suite but `smoke` needs a Loom exporting `loom::kernel`; against a kernel-less package `tests/` fails configuration out loud, and `-DBUILD_TESTING=OFF` is the library-only configuration.
BECAUSE — every suite but `smoke` drives real weave libraries through the real kernel; against a
kernel-less package the tests used to return quietly and the lane printed a full pass over the one
surviving smoke test, from the supported default Windows package.
SEEN — `tests/CMakeLists.txt` `BUILD_TESTING`.

## VM-POP-11 — The Workshop family is split by subject

METHOD — The Workshop family is eight entries split by SUBJECT; pick the one your change can falsify and build that target alone.
BECAUSE — one file behind one entry had reached thirty thousand lines: thirty seconds of
compiling before an assertion could run, and a nineteen-second test the scheduler could hand to
one worker, a floor neither the machine nor the population explained.
SEEN — `tests/CMakeLists.txt` `workshop_document`, `workshop_files`;
`tests/test_population.txt`.

## VM-POP-12 — A suite is not a file

METHOD — A SUITE IS NOT A FILE: sources in the entry's `SOURCES` list share its definitions by construction, and a source left out drops cases, which the floor catches and the build does not.
BECAUSE — `panes` is five sources under one entry and one floor because one MinGW Debug object
could no longer name all of its instantiations; a source dropped from the list builds and links
green, and only the floor said it was gone.
SEEN — `tests/CMakeLists.txt` `workshop_panes`; `tests/test_population.txt`.

## VM-POP-13 — The shared support header costs emission

METHOD — `tests/workshop_support.hpp` holds what more than one suite needs; a helper costs ~0.4 s of parse to read and 5–8 s of emission per suite that uses it, so moving one in is a decision.
BECAUSE — a suite pays for what it uses, not what it reads: the includes alone cost 0.4 s more
to parse, and the emission of the helpers a suite calls cost five to eight seconds per unit, so
splitting the header buys nothing and out-of-lining does.
SEEN — `tests/workshop_support.hpp`.

## VM-POP-14 — A temporary directory belongs to its suite and its process

METHOD — A temporary directory belongs to the suite and the process that made it: the root carries the suite name and the process id, a case asserts both, and a root a crashed run left is another process's.
BECAUSE — the suites run at once and every `TempDir` counter starts at zero, so two processes of
one suite would otherwise sweep each other's directories, and the failure would not be a red.
SEEN — `tests/test_workshop_persistence.cpp` case `"a temporary directory belongs to the suite
that made it"`; `tests/workshop_support.hpp` `ZENGINE_WORKSHOP_SUITE`.

## VM-POP-15 — A sweep removes a link by name without entering it

METHOD — A sweep asks `leaves_the_tree` of every entry and removes a link by name without entering it, because libstdc++ on Windows walks a directory junction as a directory.
BECAUSE — the target emptied through the junction and the dangling junction left behind cost
every second local MinGW run its junction witness.
SEEN — `tests/workshop_support.hpp` `remove_tree`, `leaves_the_tree`.

## VM-POP-16 — When a suite splits, re-baseline every floor

METHOD — When a suite splits, re-baseline every floor to what actually runs so a deleted case names its area; a redistributed old total keeps the slack the split was meant to remove.
BECAUSE — one floor over a big suite carried fourteen counts of slack, so deleting a case was a
green; measured floors summing to the same total catch one deletion and name its area.
SEEN — `tests/test_population.txt`.

## VM-POP-17 — Before splitting a binary, audit what becomes a cross-process contract

METHOD — Before splitting a binary, take per-entry and per-case timings and audit every process-local convention that becomes a cross-process contract: temp paths, state roots, env, files, sockets, display, children.
BECAUSE — a temp path built from a tag and a counter was safe for years inside one process; six
processes at once each start the counter at zero, and one suite deleting another's files mid-case
is not a red, because the constructor removes before it creates.
SEEN — nowhere yet

## VM-POP-18 — Prove a mechanical move of cases by hashing

METHOD — Prove a mechanical move of cases by hashing each case's full text old against new — N of N byte-identical, none gone, none duplicated — never by eyeballing the diff.
BECAUSE — a thirty-thousand-line diff cannot be read; a hash of each case's full text old
against new is twenty lines of script and a stronger claim than an assertion total.
SEEN — nowhere yet

## VM-POP-19 — Absence is modelled by gate, not by environment

METHOD — A population contract models absence BY GATE, not by environment: an environmental fact a lane needs (a host font, a display) is not modelled by the gate that models an option.
BECAUSE — a suite declared absent when its option is off went red on a host with no font at the
path it opened; the answer here was to embed the typeface, so no lane depends on a host font.
SEEN — nowhere yet

## VM-POP-20 — A doctest filter splits on commas

METHOD — A doctest `--test-case=` filter splits on commas, and a partial match still looks like a result: run comma-bearing titles one at a time and compare the selected count to the number asked for.
BECAUSE — eight titles in one filter, four holding a comma, selected four and reported a
cheerful pass over the rest; nothing said half the request had been discarded.
SEEN — nowhere yet

## Where a case goes

The registers cite their witnesses by exact case name, so the answer to "which suite pins this
law" is a grep over `agents/workshop/`; this table is the current shape of that answer, most-cited
suite first, for choosing where a new case goes and which target to build. Keymap and clipboard
law is witnessed in the *document* suite, keyboard focus in *panes_input*, the Info grounds in
*document* — the suite is the subject the case proves, not the file the law names.

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
| session-restore | `workshop_persistence`, then `surface` and `workshop_files` |
