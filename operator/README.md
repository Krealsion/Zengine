# Operators

**An operator is a named, typed rule that several parts of a program must agree about, such as
`math.max(lhs, rhs) -> result`. A program keeps its operators in one catalog and asks that catalog
to evaluate a rule by name, so no two tools hold two copies of the same rule.**

A rule is a C++ function or a composition of other operators. A host gets its rules from
providers, shared libraries it opens; a basic one ships here. A tool the host loads can ask the
host to evaluate an operator without holding the catalog.

A program links `zengine::operator` and writes `#include "operator/catalog.hpp"`; a loaded tool
that only asks links `zengine::operator-consumer` and writes `#include "operator/host.hpp"`.

## Pages

- [Operator providers — where a host's powers come from](docs/operator-providers.md): how a
  library supplies operators to a host, and how one is replaced.
- [The operator host — how a loaded tool spends the host's operator truth](docs/operator-host.md):
  how a loaded tool asks its host to evaluate an operator.
- [Sources — the catalog entries you can spend with nothing in hand](docs/operator-sources.md):
  operators that take no inputs, and how to sample one.

## What is in this folder

| | |
|---|---|
| `operator.hpp`, `catalog.hpp`, `fold.hpp` | what an operator is, the catalog, the evaluator |
| `primitives.hpp` | the basic rules: `math.max`, `math.add` and others |
| `reference.hpp` | a reference to an operator, as a value a program can keep |
| `source.hpp` | Sources: operators that take no inputs |
| `migration.hpp` | conversions that bring a file saved by an older version up to date |
| `host.hpp`, `host_abi.h` | a loaded tool's side of asking its host |
| `host_surface.hpp`, `image.hpp` | the host's side of that |
| `provider.hpp`, `provider_abi.h` | how a library supplies operators |
| `provider_host.hpp` | how a host mounts one |
| `basic_provider.cpp` | the basic provider, `zengine-operators-basic` |
| `CMakeLists.txt` | the build targets |
| `docs/` | these pages |

Working on it: [AGENTS.md](AGENTS.md).
