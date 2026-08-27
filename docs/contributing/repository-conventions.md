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
  workshop/          Workshop as a product, for a maker
  contributing/      this directory
  architecture/      why it is shaped this way
  laws/              numbered invariants
  decisions/         one decision per file, with its alternatives
  history/           frozen. Describes the tree it was written against

<package>/           one directory per package; see below
tests/               every suite, fixture and check
reference/           the pre-Zen V1 engine, kept as a quarry. NOT built
```

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
technical fact discovered during a phase stays; the excavation does not.

- **Bad:** "phase X's repair taught us that …"
- **Good:** "On MSVC, exported ABI declarations use the existing export macro …"

**References are checked.** Citing a reference page from a law, a test from a reference page, or
a `.md` from a source comment is the convention, and `doc_links` verifies every one of them on
the official lane, `#anchor` included. A repository-relative `.md` path is the accepted form
inside a source comment, because a comment has no stable directory to be relative to. See
[build and test](build-and-test.md#doc_links-because-documentation-is-verified-here-too).

**Examples are compiled.** A code example that claims to compile should have been compiled.
Avoid ellipses inside a supposedly-complete minimal example; make an intentional omission
obvious.

**Numbers and statuses have owners.** A count, a version, a population size or a "currently"
written into prose or a comment will go stale silently. Prefer pointing at the file that owns
it — the population inventory, the source, the plan — over copying it.

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

## Source comment conventions

Public header comments are documentation, and a stranger reads them while using the API. So:

- State what the thing **is** and what it deliberately **is not**. The "is not" half is
  load-bearing: it is what stops a reader inferring a capability from an architecture.
- Name the law or reference page a rule comes from, so the claim is checkable.
- Do not use a development-phase name as an explanation of *why*. If the reason matters, state
  the reason.
- Where a check or a wall exists, say what triggers it — a guard whose trigger is misdescribed
  is worse than an undocumented one.

## Attribution

Commits are authored as `Krealsion <krealsion@gmail.com>`. Do not add co-author trailers of any
kind.

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
