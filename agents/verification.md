# Agent law — Verification (router)

Routed behind [`../AGENTS.md`](../AGENTS.md): how this repository is verified. The method lives
in the registers under [`verification/`](verification/): one method per `##`, a `VM-<AREA>-<NN>`
id that is permanent, a `METHOD` of one line, a `BECAUSE` of at most three, and a `SEEN` naming
where the method is applied in this tree — a test file and case, a script, a lane step — or
`SEEN — nowhere yet`, a debt of the same kind as an unwitnessed law. The lane commands are in
[`../AGENTS.md`](../AGENTS.md#build--test-canonical-wsl-consumes-an-installed-loom); the public
reference is [`../docs/contributing/build-and-test.md`](../docs/contributing/build-and-test.md).

## The lanes, in order

1. **The ordinary lane** — `cmake -DZEN_BUILD_DIR=<build> -P tests/verify.cmake`, never a bare
   `ctest`. Parallel on Linux/GCC with `-DZEN_CTEST_ARGS=-j<n>` on a fast filesystem; serial on
   Windows.
2. **The sanitizer lane** — the same population under ASan + UBSan (`-DZENGINE_SANITIZE=ON`): a
   second kind of evidence, for defects whose symptom is that no answer changes.
3. **The package witness** — `tests/package/run.cmake`, not a CTest entry: after any public
   header, exported link line or `cmake/ZengineInstall.cmake`.
4. **The live witness** — the real window or terminal, when a claim is about the screen; owed
   and named, never assumed.
5. **The mutation matrix** — the evidence that a case would have complained; canary first.

CI runs on pull requests and on pushes to `main`, and a run is read job by job, because a job
under `continue-on-error` is red only in the jobs list. Windows is two standard libraries:
MinGW-w64/libstdc++ is REQUIRED, MSVC's STL is ADVISORY, and a Windows claim names the one it was
measured on.

## What a green means

A green means nothing complained. It names the repository, the configuration and the compiler —
"green on Windows/MinGW-w64 GCC 13.1, Debug, SDL off", never "green on Windows" — and it quotes
`tests/verify.cmake`. Proven means a regression test asserts it; everything else is true by
construction and not yet pinned. A count is a population, never an acceptance oracle.

## The population contract

A green means the intended test population existed and ran: the declared CTest entries exist
exactly, each doctest surface clears its declared case floor, a selection of zero cases is a
FAILURE, and the tests pass. `tests/test_population.txt` is the expectation and the only home of
the floors, and a test is registered through the helpers, never a bare `add_test()`. Every suite
but `smoke` needs a Loom exporting `loom::kernel`, and against a kernel-less package `tests/`
fails configuration out loud.

## The tree-reading checks

Three `script` entries read the source tree rather than a build, ride the official lane, and
self-test before they answer:

- **`doc_links`** (`tests/check_doc_links.cmake`) — every relative link and anchor in a
  current-facing document, every repository-relative `.md` path in a first-party comment, and no
  path outside the repository in any current-facing text file.
- **`package_vocabulary`** (`tests/check_package_vocabulary.cmake`) — the installed package's
  public variables name the physical thing; a retired spelling appears only in the checker.
- **`law_register`** (`tests/check_law_register.cmake`) — the registers under `agents/workshop/`
  (the WL form) and `agents/verification/` (the VM form): ids unique under `agents/`, every path,
  identifier and witness resolving, every record reciprocal with the WHY lines that name it,
  every `// WL-` pointer naming the declaration beneath it, and every file under `agents/` within
  its byte budget — a register 16,384, a router 8,192, `AGENTS.md` 20,480.

## Where the method is

| the work touches… | read |
|---|---|
| running a lane, CI, the compile-judged entries and the tree they write | [lanes](verification/lanes.md) `VM-LANE` |
| a floor, a CTest entry, the Workshop suites, a temp root, a split | [population](verification/population.md) `VM-POP` |
| a mutation harness: the canary, the snapshot, the build, the restore, the hashes | [mutation](verification/mutation.md) `VM-MUT` |
| reading a matrix: a green, a mask, a crash, a refusal at compile time | [mutation-verdicts](verification/mutation-verdicts.md) `VM-MUT` |
| a case that can fail: a bound, an oracle, a repeat, custody, a durable format, a key | [fixtures](verification/fixtures.md) `VM-FIX` |
| a static_assert, a compile-negative entry, a source tripwire | [walls](verification/walls.md) `VM-WALL` |
| driving the real window or terminal | [witnesses](verification/witnesses.md) `VM-WIT` |
| reading what the witness shows: a diff, a title, a pixel count | [witnesses-reading](verification/witnesses-reading.md) `VM-WIT` |
| a runtime probe, a timing, a reproduction, a moved build | [probes](verification/probes.md) `VM-PROBE` |
| a check that reads the tree, a mass edit over it | [checks](verification/checks.md) `VM-CHECK` |
| MinGW, MSVC, the COFF ceilings, a 9p mount, two CMakes | [platforms](verification/platforms.md) `VM-PLAT` |
| judging a dependency, a package option, a toolchain | [dependencies](verification/dependencies.md) `VM-DEP` |

Which suite witnesses which Workshop area is the table at the end of
[population](verification/population.md#where-a-case-goes). The working harnesses for the live
witness and the mutation matrix are kept with the phase records, outside this repository
(VM-WIT-24); a register restates the method, never the program.
