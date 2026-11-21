# Contributing

The guide for everyone who changes Zengine, a person or an agent: one reader with one path. Read
[what any task needs](#what-any-task-needs) before any task; the sections after it are reference,
read when a question needs it. Every page and comment is written to
[the documentation's design](https://github.com/Krealsion/Loom/blob/main/docs/contributing/documentation-design.md),
and [the recommended setup](https://github.com/Krealsion/Loom/blob/main/docs/contributing/recommended-setup.md)
helps you choose a machine, a clone and a harness. Both are Loom's, and so is the half of this
guide the two repositories share, which the sections below link by address.

## What any task needs

### Where things live

Zengine is the application side of the project: its packages and Workshop, the host every feature
is mounted in. It uses Loom, the substrate, as a stranger would, through an installed package, and
every Loom law holds here: [Loom's pages](https://github.com/Krealsion/Loom/blob/main/docs/README.md),
and [its router](https://github.com/Krealsion/Loom/blob/main/docs/CONTEXT.md), which sends each
topic to its reference page, its laws and its tests (with a sibling checkout,
`../Loom/docs/README.md` and `../Loom/docs/CONTEXT.md`). Questions about the bus, grants or where an
answer came from are answered by Loom's pages, not worked out again here.

Each folder's front page, its `README.md`, says what the folder is and what its parts are. Start a
task at the front page of the folder it touches. A folder brought to the documentation's design
ends its front page with "Working on it": its laws, its suites and the host's side of it. A folder
not yet brought to it keeps an `AGENTS.md` beside its front page, the folder's router: its law
(registers under `agents/`), its suites by CTest entry, the host's side of it and its practices;
read it before working there. The map of every folder is [`docs/README.md`](docs/README.md);
[`tests/folders.txt`](tests/folders.txt) names every top-level folder and its kind, and
[`tests/front_page_folders.txt`](tests/front_page_folders.txt) the folders brought to the design.
The project's own words are in [the glossary](docs/glossary.md).

### What a change is judged against

[What a change is judged against](https://github.com/Krealsion/Loom/blob/main/CONTRIBUTING.md#what-a-change-is-judged-against)
is Loom's, and holds here whole: the purpose and the agreed contracts decide whether a change is
right, code and tests show what it does now, and the whole result is judged, not only its cases.

**Write Loom, not host code.** A pane or a tool is a weave that reaches the rest only by messages.
Split it until a piece is too small to be useful alone; that piece becomes an operator other
weaves compose. Code that grows inside the host where a weave could hold it is the mistake to
refuse; a problem that arises inside Loom's architecture is the one worth solving. When a piece
earns being a weave of its own, and when it is a library of another, is
[a decision record](agents/decisions/a-participant-earns-its-place.md) and its law, WL-PART-01.

### Build and test

The official lane runs on Linux, and Zengine builds against an installed Loom:

```bash
# once, in ../Loom:  cmake --build build -j && cmake --install build --prefix build/_install
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install"
cmake --build build -j"$(nproc)"
cmake -DZEN_BUILD_DIR=build -P tests/verify.cmake        # the official lane

# the documentation lane: no build, no Loom
cmake -P tests/documentation_lane.cmake
```

- **One command to a ready build**, on Linux or in MSYS2's MinGW64 shell on Windows, from this
  checkout: `cmake -P tools/ready-build.cmake <build folder>` gets Loom beside it (`../Loom`,
  cloned when it is not there; a second argument names another checkout), builds and installs it
  into the build folder, and configures and builds Zengine there against it. It writes a `ready`
  preset into `CMakeUserPresets.json`, so an IDE that reads CMake presets, CLion among them,
  opens the checkout ready to build. Run it again to move both checkouts forward to their upstream
  branches and rebuild only what changed; a checkout with uncommitted changes, or no upstream, is
  left at what it holds. `-DREADY_JOBS=<n>` before `-P` bounds the build's jobs, for a machine with
  less memory than one heavy unit a core needs.
- **Quote `tests/verify.cmake`, never a bare `ctest`.** A bare run cannot tell whether the tests
  that ran are the tests this repository meant to run. Add `-DZEN_CTEST_ARGS=-j<n>` to run it in
  parallel. The sanitizer lane, the installed-package and build witnesses, and which of them a
  change owes, are [building and testing](docs/contributing/build-and-test.md).
- **A change that touches no compiled line is verified by the documentation lane**: Markdown, an
  image under a `docs/` folder, a text check or the files only text checks read.
  `tests/check_change_kind.cmake` says each changed file's kind against `origin/main` and why,
  and the lane runs every text check `tests/text_checks.cmake` lists, in seconds, passing only
  for a documentation-only change; CI runs those checks on every change and skips the build and
  test jobs for a documentation-only one. A C/C++ file is the official lane's even when only its
  comments changed, because compiled tests read source as text (`tests/text_checks.cmake` says
  which); so is a CMake file, CI, the lane itself, a manual, which is compiled into its weave, and
  a guide a compiled test reads, which that list names.
- **Say which green you proved**: this repository's, in which configuration, with which compiler.
  Zengine's lane never runs Loom's suite. A green is never bare, and never a bare "Windows".
- **Stranger-by-default is deliberate** (`ZEN_LOOM_DEV=OFF`): an unexported-surface mistake must
  fail on every machine, not only in CI. `-DZEN_LOOM_DEV=ON` is the sibling override.
- Weave libraries go through `zengine_weave()` (which applies Loom's reloadable-lifetime contract);
  providers that are not weaves go through `zengine_provider()`.

### What a green means

A green means the intended tests existed and ran. `tests/verify.cmake` requires all four: the
declared CTest entries exist exactly; each doctest suite selects at least its declared number of
cases; a run that selects no case fails; and the tests pass.

- [`tests/test_population.txt`](tests/test_population.txt) is the expectation and the only home of
  the per-suite floors. Adding, renaming or removing a CTest entry is a deliberate edit there. A
  floor is a minimum, and lowering one so that a deletion passes is exactly what the contract
  forbids.
- Register a test with `zengine_doctest_test()`, `zengine_compile_test()` or
  `zengine_program_test()`, never a bare `add_test()`.
- Every suite but `smoke` needs a Loom exporting `loom::kernel` (on Windows: Loom's opt-in
  `LOOM_ENABLE_WINDOWS_KERNEL`); against a kernel-less package `tests/` fails configuration out
  loud. `-DBUILD_TESTING=OFF` is the supported library-only configuration.
- The text checks (`tests/text_checks.cmake` lists them) read the source tree rather than a build;
  what each holds is [the tree-reading checks](agents/verification.md#the-tree-reading-checks), and
  the full contract [agents/verification/population.md](agents/verification/population.md).

### Traps

Do not assume any of these:

- That `Switchboard` has a bounded drain. `drain_until_idle()` is unbounded by contract. A host
  turns its bus with `pump_pending()` and rests between turns in its one wait (Loom's
  `loom::host::Reactor`), which sends the Timer and every source it trusts the time
  ([the host's wait](timer/docs/timers.md#the-hosts-wait)).
- That the delay a weaver authors is the delay that is scheduled. It is normalized
  (`timer.normalize_delay`, [agents/operators.md](agents/operators.md)), and in a process that
  hosts operators the rule is resolved through the host's catalog, not the Timer's own image.
- That a count is a population. Totals of assertions, deliveries or queued messages are evidence to
  report, never the test of acceptance ([agents/verification.md](agents/verification.md)).
- That "built", "loaded" or "sent" names one fact. Each names two: a green build and the artifact it
  produced, a load and a live weave, a send and a delivery. The second fact has its own owner and
  its own witness ([agents/realization.md](agents/realization.md)).
- That a shape's version is its own. A shape that encloses a changed shape is a new version too.
  Loom folds a nested shape's content id into its parent's and refuses a second shape under a name
  and version it already holds, so a component built before the change and one built after could not
  load side by side. Nor may two shapes share a name: Loom claims a library's state and vocabulary
  at load, so two components' shapes of one name meet at the next version either takes.
  `tests/shapes.txt` pins every shape the tree publishes, nested ones included, by content id under
  its name and version, and its census (`tests/test_shapes_census.cpp`) fails a content id that
  moved without a new version, naming each enclosing shape; it writes the census as it stands beside
  the build to diff. `tests/test_workshop_shapes.cpp` claims the shapes as they were published
  beside the current ones and reads every name the tree declares.

### Writing text

[Comments and documents](https://github.com/Krealsion/Loom/blob/main/CONTRIBUTING.md#comments-and-documents)
is Loom's, and holds here whole: names first, and a comment says why, what the code must not
become, or what cannot be seen from its line, in complete sentences. A rule about one declaration
is written in that declaration's comment. A rule that other files rely on is a numbered law: on its
folder's law page in a folder brought to the design (the Timer's are
[`timer/docs/timer-laws.md`](timer/docs/timer-laws.md)), and in a register under `agents/`
elsewhere ([the registers](#the-registers-under-agents)). A comment names it inside a sentence
that says the rule. What may not appear in a public page, and how `doc_links` reads a comment's
link, is [the repository conventions](docs/contributing/repository-conventions.md).

### Making a change

Bounded work is a GitHub issue labelled `ready`. [Taking an
issue](docs/contributing/taking-an-issue.md) goes from one to a pull request ready to merge, and
[best practices](docs/contributing/best-practices.md) says what good work looks like, surface by
surface. [Making a change](https://github.com/Krealsion/Loom/blob/main/CONTRIBUTING.md#making-a-change),
[attribution](https://github.com/Krealsion/Loom/blob/main/CONTRIBUTING.md#attribution) and
[attacking the result](https://github.com/Krealsion/Loom/blob/main/CONTRIBUTING.md#attacking-the-result)
are Loom's, and hold here whole; this repository's attribution check is
`tests/check_commit_attribution.cmake`. Open an issue before preparing a large change to the core.

## What crosses folders

What crosses folders, and the folders with no front page of their own (`tests/`, `cmake/`,
`tools/`, `docs/`, `agents/` and the forge's `.github/` and `.run/`), are routed here:

| the task touches… | read |
|---|---|
| lanes, suites, populations, CI, what a green means, platform build traps, a harness, a witness, a check | [agents/verification.md](agents/verification.md), the router over the method registers: [checks](agents/verification/checks.md) · [dependencies](agents/verification/dependencies.md) · [fixtures](agents/verification/fixtures.md) · [lanes](agents/verification/lanes.md) · [mutation](agents/verification/mutation.md) · [mutation verdicts](agents/verification/mutation-verdicts.md) · [platforms](agents/verification/platforms.md) · [population](agents/verification/population.md) · [probes](agents/verification/probes.md) · [walls](agents/verification/walls.md) · [driving a witness](agents/verification/witnesses.md) · [reading one](agents/verification/witnesses-reading.md) |
| `cmake/ZengineInstall.cmake`, any public header, any exported target's link line | [agents/packaging.md](agents/packaging.md) |
| the pane seam's prose, both its halves, and the tools that arrive through it (its law: `pane/`'s router) | [agents/panes.md](agents/panes.md) |
| Workshop, the host every feature is mounted in | [workshop/AGENTS.md](workshop/AGENTS.md) |
| the line between Workshop and Zengine, `tests/census.txt` | [agents/workshop-line.md](agents/workshop-line.md) |

The suites no one folder holds:

- `doc_links`, `doc_names`, `package_vocabulary`, `law_register`, `source_comments`, `code_values`, `folder_map`, `manual`
  — the checks that read the source tree, each stating its contract in its own header
- `census` — the numbers `tests/census.txt` pins, the line between Workshop and Zengine among them
- `smoke` — the one suite with no kernel, in `tests/smoke/`
- `workshop_shapes` — every shape the tree publishes against `tests/shapes.txt` ([traps](#traps))
- `package_judges` — the judges of the package a stranger downloads, each red on its canary
  (`tests/package/judges/`)
- `configure_switches` — a configure naming a retired switch, refused by name before a compiler
  is sought (`tests/check_configure_switches.cmake`)
- `sanitizer_lane` — the sanitizer lane's shares, pieces, time budget and build memory, each
  refusing or warning on its fixed case (`tests/check_sanitizer_lane.cmake`)

## The registers under `agents/`

A folder not yet brought to the documentation's design keeps its law in registers under `agents/`,
routed by that folder's `AGENTS.md`, and `law_register` holds their form and names. Surface law
belongs in the routed surface document that owns it, current law, rewritten in place: a change
updates the sentences it made false and states the present, never an account of itself. The
registers, every folder's `AGENTS.md` router and `agents/` stay out of the public documentation
index and out of the installed package.

The rules for writing one (`law_register` enforces form and names; the router, procedure):
**a** a claim lives in LAW, MEANS or DOES NOT MEAN, never the heading. **b** what a weaver types
or sees is a spelling, not an identifier. **c** a law's proof stands under `### <id>` in its
register's appendix, `## Proven by`, at the file's end (a method's under `## Seen`): a line per
file, the file in backticks, then its functions, types, constants and members bare and its cases
quoted, a note in parentheses. **d** a residue claim is LAW when it is the invariant, else DOES
NOT MEAN. **e** what a change left alone is a change note, not a law. **f** LAW text in a table
escapes `|`. **g** no phase code in a case name or a current-facing document, no step label
opening a case name; every case a document cites, and every law a comment or document names,
exists. **h** a record's Alternatives split tried (evidence inline) from argued. **i** a line in a
register, a router or a record wraps past <!-- value ZEN_LAW_LINE_BYTES -->196<!-- /value --> bytes, and short of that
where its writer judges; a record runs about 1.5 KB, and over
<!-- value ZEN_LAW_RECORD_FLAG_BYTES KiB -->4<!-- /value --> KB is flagged. **j** a record may link another; its header
links a register file only where that file declares a law the record lists. **k** Laws supported
is generated from WHY lines: edit the WHY; a record no law supports moves to `docs/history/`.
**l** a record over ten laws is suspected of being two. **m** an owner identifier is a whole token
in the named file's code, comments stripped. **n** a `// WL-` or `// MW-` pointer is a proof
inverted: every law on it names the declaration beneath. **o** a mass edit is sheet → applier →
proof, regenerated from the start commit. **p** after a compression, measure the report's claims
again. **q** a tree-reading check self-tests; an empty population is red. **r** a tree-reading
check quotes its filesystem with its wall clock. **s** a pointer carries only ids whose law names
the declaration; a member is `Struct::member`, an overload `name(Type)`. **t** a per-line sheet
carries the whole pointer group; a file index is re-resolved after every write.

The page budgets these registers and routers were held to are paused while the text is rewritten
([the checks](https://github.com/Krealsion/Loom/blob/main/docs/contributing/documentation-design.md#the-checks)).

## Ownership

This repository owns Zengine's packages and Workshop; Loom owns substrate truth (messaging,
lifecycle, replacement, capabilities), and Zengine consumes it as a stranger —
`find_package(loom)` against an installed prefix. Every Loom law applies here; see Loom's
[laws](https://github.com/Krealsion/Loom/tree/main/docs/laws). The installed-package boundary
(which targets are exported, which headers install, which artifacts ride along) is owned by
`cmake/ZengineInstall.cmake` alone.

## Code contributions

Issues, testing, design discussion, reproductions, and feedback are welcome.

Before merging substantial third-party code contributions to the core
repository, the project will publish explicit contributor terms so ownership
and licensing remain clear for contributors and maintainers alike.

Please open an issue before preparing a large core contribution.

This note is deliberately minimal: it is not a CLA, it requires no copyright
assignment, and it sets no terms beyond asking that big core changes start
with a conversation. Experimentation, packages, and weaves of your own need
no permission at all — they are yours.
