# Repository conventions

**Contributing.** How this repository is organised, and the conventions a contributor is
expected to keep. This describes working on the public project.

## Layout

```text
README.md            orientation
cheat_sheet.md       dense operational reference
AGENTS.md            the contract for automated collaborators working in this tree
agents/              routed detail behind AGENTS.md, by surface — internal working law for
                     automated collaborators. Not user documentation, not indexed by
                     docs/README.md, and not part of the installed package
CONTRIBUTING.md      contribution terms
LICENSING.md         the plain-language licence boundary

docs/                all documentation; docs/README.md is the index
  getting-started.md
  guides/            how-to, task-shaped
  reference/         exact contracts, one per package or subject
  workshop/          Workshop as a product, for a weaver
  contributing/      this directory
  architecture/      why it is shaped this way
  laws/              numbered invariants
  decisions/         one decision per file, with its alternatives
  history/           frozen. Describes the tree it was written against

<package>/           one directory per package; see below
examples/            small sources a weaver copies into a project of their own; compiled by tests/
tests/               every suite, fixture and check
reference/           the pre-Zen V1 engine, kept as a quarry. NOT built
```

A page under `docs/contributing/` routes to the registers under `agents/` that own its rules.

## Packages

Each package is a directory holding a `CMakeLists.txt`, one or more `vocabulary.hpp` headers,
and its implementation. The shape is deliberate:

- **A `vocabulary.hpp` is a header-only INTERFACE target.** Consumers spell
  `#include "timer/vocabulary.hpp"` — an include path rooted at the repository, so a package
  reference means the same thing from anywhere in the tree. Shapes are just `ZEN_SHAPE` structs,
  so a vocabulary exists on every configuration, including ones with no kernel.
- **A loadable weave goes through `zengine_weave()`.** See
  [supported toolchains](supported-toolchains.md#the-reloadable-weave-build-contract).
- **A package that needs a kernel gates on `if(TARGET loom::kernel)`** and reports out loud that
  it is skipped and why, rather than failing to configure.
- **A package links what it uses, on its own line.** A transitive edge that happens to work is
  not a declaration. An artifact that both supplies and consumes a surface names both.

`zengine-component` links **nothing** — not even `loom::core`. A `TextBox` has no wire form,
nothing serializes it and nothing hosts it, and the absence of that link is the enforcement of
"a component is not content".

**A `zengine::` on a link line means the target is public.** The packages an external project
can consume are exported by [`cmake/ZengineInstall.cmake`](../../cmake/ZengineInstall.cmake)
with `EXPORT_NAME`s matching their in-tree `zengine::` aliases, so the house and a stranger
spell them identically and the two cannot drift into meaning different things. An internal
target keeps its plain hyphenated name, and a link line therefore reads as a boundary:

```cmake
target_link_libraries(zengine-workshop PRIVATE zengine::surface            # exported
                                               zengine-workshop-vocabulary) # internal
```

An exported target carries its include path twice — `$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>`
and `$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zengine>` — which is what keeps
`#include "timer/vocabulary.hpp"` the same sentence from either side. Two rules follow:

- **Exported means the headers are installed one at a time**, by name, in
  `ZengineInstall.cmake`. A package directory holds implementation beside vocabulary, so
  installing a whole directory would ship both. Anything a public header includes must itself
  be installed; `tests/package` is the witness.
- **An exported target links what its public headers use, on its own line.** In this tree a
  missing link is invisible — every include path is the same directory — and from an installed
  package it is the difference between compiling and not.

## Documentation conventions

**One document, one recognizable reader purpose.** A page that is both a tutorial and a
reference is two pages.

**The external-reader rule.** Every public document assumes a reader who knows nothing about
the maintainers, their machines, their directory layout, their tooling habits or the project's
development process. Public documentation must remain coherent if all of that disappeared.

Concretely, none of the following belongs in a public document: personal directory layouts,
absolute paths on somebody's machine, private tooling workflows, development-phase names used
as explanation, or references to files that exist only in a maintainer's tree. A durable
technical fact stays; the excavation that found it does not. A public repository
names no path outside itself — not the workspace it is checked out in, not a sibling, not a
drive, not a tool's scratch directory — and `doc_links` reads every current-facing file for one.

- **Bad:** "phase X's repair taught us that …"
- **Good:** "On MSVC, exported ABI declarations use the existing export macro …"

**A document states the present.** A current-facing page says what is true of the tree it ships
with, not what the tree was, what retired or what a change did; that story is Git's and
`docs/history/`'s. `law_register` refuses the few forms that told such a story every time they
were read by hand — a heading marked retired, a note of what a thing was before or what it was
called, a bold note opening on the past — and the development process used as a unit of time.
It names each form in its own words; history put in other words is a reviewer's to catch.

**References are checked.** Citing a reference page from a law, a test from a reference page, or
a `.md` from a source comment is the convention, and `doc_links` verifies every one of them on
the official lane, `#anchor` included. A repository-relative `.md` path is the accepted form
inside a source comment, because a comment has no stable directory to be relative to. See
[build and test](build-and-test.md#doc_links-because-documentation-is-verified-here-too).

**Examples are compiled.** A code example that claims to compile should have been compiled.
Avoid ellipses inside a supposedly-complete minimal example; make an intentional omission
obvious.

**`docs/history/` is frozen.** It describes the tree it was written against and is not
maintained against the current one. Do not fix it and do not cite it as current.

**`reference/` is a quarry, not a codebase.** It holds the pre-Zen V1 engine as material to
read. Nothing in it is built by this repository, and ports out of it are read-and-rewrite —
landing in their proper home from birth, never lift-and-shift. Its provenance is a plain file
import of that project's working tree rather than a history-carrying subtree split, so its
history stays in the original working copy. What is actually in there is indexed by
[reference/QUARRY-CATALOG.md](../../reference/QUARRY-CATALOG.md) — capability by capability,
with source paths, the legacy interaction shape, and where the comparable question is answered
today. It is archaeology, not authority: an entry saying a capability is absent is a statement
about coverage, never a request for work.

### Values the code owns

A version, a limit, a count or a default the code owns is named by its owner — the constant,
the budget, the format version — wherever a sentence needs it: "at most `kMaxHeldKeys` keys",
"the setup file's `kFormatVersion`". A comment names the owner and no more; a number another
repository owns, such as Loom's `loom::kTranscriptCapacity`, is named, never copied. Where a
page's reader needs the number itself, it stands in a marker that renders as nothing:

```markdown
A carried pair is at most <!-- value kMaxCarryBytes KiB -->64<!-- /value --> KiB.
<!-- value kMaxCommandBytes in "longer than {} bytes" -->
```

The first holds the number between its two comments, written as the word after the id says
(`KiB`, `MiB`, `GiB`, `s`, `pow2`, `grouped`, or nothing for the integer); two ids joined by a
comma are one value two owners share, and they must agree. The second holds a spelling on its
own line, or, standing alone, the next line or the fenced block after it: the form for a
number inside code or a transcript. A marker shown inside a fenced block, as here, is an
example and holds nothing, and `law_register` measures its budgets without markers.

`code_values` (`tests/check_code_values.cmake`) reads each value
[`tests/code_values.txt`](../../tests/code_values.txt) registers from its owner, and a marker
that says otherwise is a red; `cmake -DZEN_VALUES_WRITE=ON -P tests/check_code_values.cmake`
rewrites the markers to the owners' values. It also refuses an unmarked copy of a value it
knows: the owner's name with a number beside it, in a page or a comment, and a spelling the
registry names, such as a claim of a file format's current version. A shape's `Name vN` is an
identity, frozen once published, and no copy.

A test count outside the population inventory names, beside it, the commit and the command that
measured it, or it is not written; `code_values` refuses one that names neither. No change
sweeps for copies by hand: when review finds a moving value the registry does not know, it is
registered in the change that found it.

## Naming

**A name carries its terminating condition.** An operation that may never return says so in its
name, and one that returns after a bounded turn says that: the bus's two dispatch turns are
`pump_pending()` and `drain_until_idle()`, and the second is unbounded by contract, which is
what its name is for. A name an ordinary reader takes for the bounded turn is how a host once
chose the call that never returned.

**A retired name is never redefined.** When a name is retired because it misled, the repair is
a new name with the honest promise. The old spelling is not reintroduced as a synonym and is
not given a different meaning later; what it used to mean is in Git history, not in a new
definition.

## Source comment conventions

A comment stays only if, without it, a competent reader with the code and the law registers
open would make a mistake. Prefer a clearer name to a comment; keep a comment to one line where
one line will do, and point to the owner that holds the rest. Be conservative where authority,
custody or lifetime is at stake.

- **What stays.** The file header: the SPDX lines, one line of purpose and, where the package
  has one, its law line (`// Workshop law: …`). A law pointer above its declaration
  (`// WL-… -- agents/workshop/<register>.md`). A section banner that names a subject. A why the
  code cannot say, beside the code it explains.
- **What moves.** Reasoning no owner holds yet, when it would prevent a future mistake, goes
  where a reader first needs it: usually a decision record, sometimes a reference page. Law is
  already in its register; the source keeps the pointer.
- **What goes.** History — a removal note, "was here", what a retired piece did — is Git's to
  keep. A restatement of the code, of a law or of a neighbouring comment goes. So does a
  development-phase name or a private id used as an explanation: if the reason matters, state
  the reason.
- **Public headers are documentation.** A stranger reads an installed header while using the
  API, so it states what a thing **is** and what it deliberately **is not** (the "is not" half
  stops a reader inferring a capability from an architecture), names the law or reference page
  a rule comes from, and says what triggers a check or a wall: a guard whose trigger is
  misdescribed is worse than an undocumented one.
- **Tests are witnesses.** A case's name says what it proves, in words and never by a
  development-phase code or a plan's step letter (`law_register` refuses both), and the register
  that cites the case by name holds the law, so the case does not restate it; a document cites a
  case only by a name a test declares. A law a comment needs is named mid-sentence, because a
  line that begins with an id reads as a pointer, and it is a law a register declares. A case's comments say
  what its name and code cannot -- why a setup, a bound, a repeat or an oracle has its shape, what
  a canary or a mutation found that the case now guards -- wherever a reader would otherwise
  simplify the case into one that passes for the wrong reason. A method a verification register
  states is named by its id.

`source_comments` holds the roots its own list names (`ZEN_COMMENT_ROOTS` in
`tests/check_source_comments.cmake`) to this on the official lane: a comment block over six lines
outside an installed header, a removal note, or a phase name or private id is a red that names
where the text belongs.

## Attribution

A commit is authored under the account of the person who makes it:
`Krealsion <krealsion@gmail.com>` for the project's own work, and an outside contributor's own
account for theirs, never the AI agent either uses. No commit and no pull request carries a
co-author line or an AI credit, for anyone. The reason:

> You will be the one held accountable for the code you open a pull request for, not the agent
> you used. Take care in the work you do, and consider the reasons and outcomes of the choices
> you make.

`tests/check_commit_attribution.cmake` refuses every co-author line and the credit line on every
commit it reads, and its refusal gives the same reason.

## Licensing

MPL-2.0. Every first-party source file carries an SPDX identifier and a copyright line:

```cpp
// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
```

Do not modify `LICENSE`. Third-party material is recorded in
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md), and vendored assets carry their own
provenance file. Details in [LICENSING.md](../../LICENSING.md).

## Contributing changes

See [CONTRIBUTING.md](../../CONTRIBUTING.md). Open an issue before preparing a large core
contribution. Packages and weaves of your own need no permission at all — they are yours.
