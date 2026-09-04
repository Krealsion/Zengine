# Building and testing Zengine

**Contributing.** How to build every configuration this repository supports, how to verify a
change, and what a green result does and does not mean. This is about working *on* the public
project; for using it, see [getting started](../getting-started.md).

## How Zengine gets the Loom

Two paths, and the same target names either way — the Loom's export sets `EXPORT_NAME` to match
its in-tree aliases (`loom::core`, `loom::switchboard`, `loom::kernel`), so the override is a
genuine drop-in and the two paths cannot silently come to mean different things.

### The default: an installed Loom package (`ZEN_LOOM_DEV=OFF`)

`find_package(loom)` against an installed, exported Loom — consumed exactly as a third party
would. Two steps, because a stranger cannot skip the install:

```sh
# in Loom -- build and install
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$PWD/build/_install"

# in Zengine -- consume it
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install"
cmake --build build -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build -P tests/verify.cmake
```

**Why this is the default.** It is what makes a mistaken dependency on an unexported Loom target
fail on *every* developer's machine rather than only in CI. The dev override reaches the whole
Loom build tree, so a target that was never exported still links — the build stays green here
and breaks for the first guest who tries it. That is precisely the class of error this
repository exists to catch first, and a default that hides it moves the discipline from "always
on" to "on wherever someone remembered".

### The override: a sibling Loom source tree (`ZEN_LOOM_DEV=ON`)

`add_subdirectory(../Loom)`, for editing both trees together without an install round-trip. It
expects a Loom checkout at `../Loom` relative to the Zengine source tree — that path is the
only place this option looks, and it is the only place in the build that assumes anything about
where anything is.

```sh
cmake -S . -B build-dev -DZEN_LOOM_DEV=ON
cmake --build build-dev -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build-dev -P tests/verify.cmake
```

A dependency's proof rides its version, so the sibling build contributes its libraries and
nothing else: its tests and examples are forced off, as is its own SDL2 UI-tree renderer (which
is a different thing from Zengine's SDL3 skin).

### Where a fetched dependency goes, and that nothing prunes it

The SDL skin fetches SDL3, SDL_ttf and FreeType at configure time
([supported toolchains](supported-toolchains.md#sdl)). Under WSL, building from a
Windows-mounted checkout, those trees cannot live in the build directory — that filesystem cannot
hold the symlinks inside SDL's tarball — so the build sends them to
`~/.cache/zen-fetch/zengine-<name>`, where `<name>` is the build directory's **basename** and
nothing else. Two consequences. A build directory under a new name is a fresh fetch and a fresh
dependency build, so reuse names. And nothing ever prunes that cache: every name you have
configured is still in it, and it grows until you delete `~/.cache/zen-fetch` by hand. Setting
`FETCHCONTENT_BASE_DIR` yourself is respected and replaces the whole arrangement.

## The sanitizer lane

```sh
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install" -DZENGINE_SANITIZE=ON
cmake --build build-san -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build-san -P tests/verify.cmake
```

`-DZENGINE_SANITIZE=ON` builds Zengine's own targets with **AddressSanitizer +
UndefinedBehaviorSanitizer** (`-fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all`), and the verifier then runs the *same* population under them. CI
runs it on every pull request and on every push to `main`; there is nothing to remember.

**Two lanes, two questions, and neither substitutes for the other.** The ordinary verifier asks
whether the population this repository meant to run existed, ran and passed. It cannot ask
whether the code that passed did so while reading freed memory or overflowing a signed integer,
because a wrong answer is not the failure mode — *no answer changing at all* is:

| | ordinary lane | sanitizer lane |
|---|---|---|
| a `Placed` bound into a temporary `Scene` | **passes** | ASan `heap-use-after-free` |
| `resolve_extent` without its overflow guard | **passes** | UBSan `signed integer overflow` |

Both rows are measured, and both are real defects rather than invented hazards — the first
shipped in committed test code, and the second was signed-overflow UB in `ui::Rect::contains`,
shared code on Workshop's ordinary press path. Each was caught by a hand-built sanitizer tree
that existed for one change and was then thrown away, and each had been called green by the
ordinary lane.

The instrumentation reaches the targets **this repository authors**, not the Loom it consumes:
an installed Loom arrives already compiled, and the Loom runs this same lane over itself. ASan's
allocator is process-wide either way, so a Loom allocation freed and then read by Zengine code
is still caught; a fault entirely inside Loom's compiled objects is Loom's lane's job.

The lane runs the **full** population, the SDL skin included — the verifier prints
`gates active: always;sdl`, and every entry the ordinary lane declares is here at the same
floor. No floor is lowered and no gate is turned off to buy the instrumentation.

**Which lane a change obliges follows the surface changed.** Run the sanitizer lane when the
change could hide a defect whose only symptom is that no answer changes — lifetimes, ownership,
retained references, extent arithmetic. Not as a reflex on prose, comments or CMake-only edits.
Whatever you run, say what you ran and what you did not.

## The installed-package witness

```sh
cmake -DZEN_BUILD_DIR=build -DZEN_WORK=/tmp/zengine-package -P tests/package/run.cmake
```

It installs Zengine into an isolated prefix, copies
[`tests/package/`](../../tests/package/CMakeLists.txt) **out of this repository**, and builds
that copy as an unrelated project against the prefix alone. `ZEN_WORK` must be outside the
Zengine tree, and the driver refuses if it is not — otherwise "it does not need the source
tree" would be a claim rather than a fact.

**A third question, which neither of the lanes above can ask.** The verifier and the sanitizer
both reach Zengine through its own build tree, where every header is on the include path and
every artifact is already staged. A requirement the *package* fails to carry is invisible from
there and lands on the first stranger. The Loom keeps the same witness for the same reason.

What it asks, in order:

| step | the failure it discriminates |
|---|---|
| install into an isolated prefix | install rules that do not run, or run into the source tree |
| read the generated package files | an absolute path to the machine that built it |
| read the installed public material | a header assuming this project's development environment |
| configure the copied project | a public dependency the config does not resolve |
| run `witness-surfaces` | an exported target whose headers are not self-contained |
| run `kitchen-host` | a loadable artifact a consumer cannot find or load |
| rebuild against a **moved** prefix | a package that only works where it was installed |
| **canary**: delete one installed header | a consumer secretly reading the source tree |

The last row is what makes the rest mean anything. Zengine's checkout is fully present and
readable throughout, so if the stranger still builds with `timer/vocabulary.hpp` removed from
the prefix, it was never using the package and every green above is void. The driver fails
loudly in that case rather than reporting a pass.

It is deliberately **not** a CTest entry: it installs, relocates and configures a second
unrelated project, and folding that into `ctest` would put a nested CMake build inside a test
binary's population. One command, run by hand and by CI, so the two cannot come to mean
different things.

## Verification

### Run `tests/verify.cmake`, not a bare `ctest`

A bare `ctest` still works and still runs the tests. What it cannot tell you is whether the
population that ran is the population this repository meant to run. Delete a registration and
CTest, the build tree and every derived list agree instantly that there are fewer entries and
all of them passed; a filter that matched nothing exits 0. A list generated from the
registrations cannot notice a registration that is gone.

So the expectation is a **file** — [`tests/test_population.txt`](../../tests/test_population.txt)
— and `verify.cmake` checks it before running anything. Its contract is three fail-closed
claims:

1. **Inventory, exact.** For the gates active in this configuration, the entries named in the
   file must equal, exactly, what `ctest -N` reports. A missing entry fails; an entry registered
   and never declared fails too, and both are named. Adding or renaming a CTest entry is a
   deliberate act with a line in that file.
2. **Floors.** A doctest suite declares a minimum assertion count; a compile-negative entry
   declares the diagnostic pattern that judges it. An entry whose diagnostic quietly went away
   is a red, not a test that has become "the compiler returned non-zero, therefore pass".
3. **Execution.** The entries must actually have run.

Pass extra CTest flags through with `-DZEN_CTEST_ARGS=-V`.

**On Linux the lane is safe to run in parallel.** `-DZEN_CTEST_ARGS=-j<n>` spends the machine on
the suites without changing what any of them proves; on a 12-core desktop it takes the lane from
about a minute to under ten seconds. On Windows keep it serial for now — the `timer` suite
has a separate, still-open failure under concurrent CTest, unrelated to the one below and
present before it was fixed. What made parallel unsafe everywhere for a while was the
compile-judged entries: each of them proves its point by trying to build a fixture, and they
used to build it in this build tree — so eight tests that have nothing to do with each other
were all writers of the tree everything else depends on, and whenever CMake owed the tree a
regeneration they raced through it. They build in a small tree of their own now, prepared by
this tree's configure, which nothing else writes and which cannot re-run CMake at all.

### Per-repo green

Zengine's lane runs Zengine's tests against its pinned or installed Loom and **does not re-run
the Loom's suite** — a dependency's proof rides its version. This is enforced rather than
documented: even under the dev override, `ctest -N` in Zengine lists only Zengine's entries.

**State which repository's green you proved.** "Green" must never silently mean "green in one of
two", and a bare "green" or a bare "Windows" is not a result.

### What is in the lane

The suites are separate binaries (`zengine-timer-tests`, `zengine-input-tests`, …) so each
package's numbers stay its own, plus compile-negative and compile-positive targets judged on
their diagnostics, plus script entries.

| entry | proves |
|---|---|
| `smoke` | a separate repository can link the Loom's exported surface and drive a value through the real gate — including that the gate **refuses** a malformed candidate. The refusal is what makes it a proof rather than a greeting |
| `snake` | the vertical slice headless: a locked contract by content-id, the simulation and a migration as pure math, live evolution through real `.so` weaves and the real kernel, and the negative space — a skinless game writes **zero** bytes to stdout, with a painted-bytes negative control |
| `timer` | the Timer contract by content-id; every schedule over a fake clock through a real bus; the activation law (premature, duplicate, foreign, stale and replayed activations establishing nothing); a real `.so` re-seeding its chain on the real clock; the load-order matrix; and the continuity lane over a *virtual* clock, so "a five-second one-shot had two seconds remaining and the successor fired it two seconds later" is an exact integer nobody had to sleep for |
| `input` | the locked contract and SDL-scancode identity by content-id and literal value; both backends' translations as pure math on every lane; the weave's publish path and self-arranged beat through a real bus |
| `surface` | the contract by content-id; terminal skins as golden bytes; the SDL frame plan as pure math; the hello handshake and the one-owner rule through the real kernel; the general canvas as golden bytes; and, where built, the SDL skin under SDL's dummy video driver |
| `ui`, `component`, `operator`, `composer` | those packages' own contracts |
| `workshop_document` | authored shapes by content-id; identity-is-not-the-name; the typed property connection including both ways a commit can fail; authored-versus-resolved as two facts only one of which moves; the maker's own gestures; editable text as a component; and the keymap that says which gesture a key is |
| `workshop_screen` | whole screens asserted as `SurfaceCanvas` values; hit testing against real authored objects; one object read in another's frame; what a hand can and cannot reach; and the sub-cell lattice a pane arrangement is authored on |
| `workshop_panels` | the panels Workshop itself ships — the terminal, the Builder, Info and its bounded bodies — and the surface that says what is currently true |
| `workshop_panes` | the external pane seam from both sides: an office authors a pane, Workshop grants the room, and a press inside that room reaches the office that owns it — against real loaded libraries through the real kernel |
| `workshop_persistence` | what survives a process and what deliberately does not: the document format and its refusals, the named arrangement, the desk that comes back on its own, and the installed application's per-user roots |
| `workshop_load` | which artifacts are in the room at all: the authored load plan, its codec, and the executor that walks it |
| `builder` | real child processes — every recipe is `cmake -E …` or this repository's own deliberately slow script, so it needs no shell and no assumption about what is installed — plus the regression canary for non-blocking custody |
| `audit_probes` | a different **kind** of suite, kept deliberately: what the substrate measurably does to a live beat chain when the timer service is swapped, reloaded, double-wound or joined late — *including where that was unwanted*. Read its header before changing it |
| the `ui_*` and `timer_*` compile entries | that a fence is a compile error, with its positive control |
| `doc_links` | every repo-local documentation reference and `#anchor` in a current-facing document, and every repository-relative `.md` path written in a first-party source comment, still resolves; and no current-facing file names a path outside the repository |
| `package_vocabulary` | the installed package's public variables still name the *physical* thing they hold. Retired spellings appear in exactly one file — the checker that declares them — and nowhere else |
| `law_register` | Workshop's law is written once, in registers under `agents/workshop/`, and every name a register makes still resolves: each entry is well-formed, every path, identifier and test case it names exists in the tree, every decision record lists exactly the laws that cite it, and every `// WL-…` pointer above a declaration names only laws that name that declaration |

### `doc_links`, because documentation is verified here too

Citing a reference page from a law, a test from a reference page, or a `.md` from a source
comment is this project's convention. What was missing is that a rename broke them silently. So
it rides the official lane: a red reaches whoever moved the file rather than whoever reads the
docs six months later. The same entry reads every current-facing text file for a path outside
the repository — the external-reader rule made mechanical.

It is a CMake script, like every repository-owned check here, because CMake is a dependency this
project already has on every lane by construction — a verifier may not depend on a tool that
merely happens to be installed. It deliberately does not require a comment to carry a
reference, does not reach outside this repository (a standalone clone has no sibling to look at),
and does not police `docs/history/` or the `reference/` quarry. Its self-test makes the real
predicate say **no** to a bad path and a bad anchor before it answers, because a clean tree and
a broken checker produce byte-identical output.

Run it alone from the repository root:

```sh
cmake -P tests/check_doc_links.cmake
```

### `package_vocabulary`, because a public noun can be false

An **artifact** is the physical loadable file a host opens by path. A **weave** and a
**provider** are runtime *surfaces* an artifact may expose — a weave is a participant the
Kernel loads onto the bus; a provider is opened directly by a host and has no participant in it
at all. The installed package carries both kinds in one list, so its public variables are
`ZENGINE_ARTIFACT_DIR` and `ZENGINE_RUNTIME_ARTIFACTS`: they name the physical thing, because
no surface word is true of all of them.

The names PKG-0 shipped were not, and nothing in a build could have said so — a compiler has no
opinion about a word. So the retirement is mechanical: the checker owns the list of spellings
that may no longer be written, and asserts both halves of its own exception — every declared
spelling present in that one file, none in any other. It does **not** police the word *weave*;
`zengine_weave()`, `WeaveId`, the weave ABI and the weave-only guides all mean weave, and
renaming those would be the opposite error.

```sh
cmake -P tests/check_package_vocabulary.cmake
```

## What a green means

**Green means nothing complained.** Proven means a regression test asserts it; everything else
is true-by-construction-and-not-yet-pinned. A green must name a population that actually ran,
which repository it was, which configuration, and which compiler.

## Warnings

Zengine applies its own warning discipline to every target it authors: `-Wall -Wextra
-Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror` on GNU and Clang. The Loom's
exported `loom::warnings` is deliberately link-only so its `-Werror` never leaks onto a
consumer's sources; a consumer that wants the same discipline declares its own, and this one
does.

## Weave libraries

A weave's shared library goes through `zengine_weave()`, which delegates the reloadable lifetime
to the Loom's exported `loom_weave_build_contract()`. Do not reintroduce a private compiler flag
for it here — the point of the Loom exporting that function is that the law travels with the
package instead of being re-derived by each consumer. See
[supported toolchains](supported-toolchains.md#the-reloadable-weave-build-contract).

## Attribution

Commits are authored as `Krealsion <krealsion@gmail.com>`. Do not add co-author trailers.
