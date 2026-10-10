# `operator/` — Operators: working on it

The router for `operator/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `operator.hpp` | what an operator is: `OperatorDef`, `make_operator`, `Composite`, `Refusal` |
| `catalog.hpp`, `fold.hpp` | `op::Catalog`, the store and one evaluator; the fold, its one form |
| `primitives.hpp` | the scalar powers: `math.max`, `math.add`, `logic.select_int`, ... |
| `reference.hpp` | `op::OperatorRef`, a reference as a typed value (`zengine.OperatorRef v1`) |
| `source.hpp` | Sources, the zero-input reading of the catalog: `is_source`, `sample` |
| `migration.hpp` | conversions as operators: `migration_identity`, `migrate` |
| `host_abi.h`, `host.hpp` | the consumer half of the host seam: the C table, `op::OperatorHost` |
| `host_surface.hpp` | the host half: `OperatorHostSurface`, `OperatorOffer` |
| `image.hpp` | `ImageShare`, one share of an open image, for an offer or a provider mount |
| `provider_abi.h`, `provider.hpp` | the provider seam: its C table, codec and one-line macro |
| `provider_host.hpp` | mounting a provider into a catalog: `op::mount_provider` |
| `basic_provider.cpp` | `zengine-operators-basic`: loadable, not a weave; supplies the primitives |

`CMakeLists.txt` builds two header-only INTERFACE libraries over `loom::core`: `zengine-operator`
(`zengine::operator`) and `zengine-operator-consumer` (`zengine::operator-consumer`), the consumer
half alone; the provider builds only where the Loom exports `loom::kernel`.

## Law

- [operators](../agents/operators.md) — operators: the catalog, the host seam, providers,
  Sources and conversions

## Suites

- `operator` — what an operator is, the host seam across a real loaded image, one live catalog
  shared with the Timer, providers, Sources and conversions: `tests/test_operator.cpp`,
  `tests/test_operator_host.cpp`, `tests/test_operator_canonical.cpp`,
  `tests/test_operator_provider.cpp`, `tests/test_operator_source.cpp`,
  `tests/test_operator_migration.cpp`, with `tests/operator_stranger.cpp`,
  `tests/weavelib/operator_consumer_weave.cpp` and the provider fixtures in `tests/weavelib/`

Its cases in shared suites: `workshop_panes` (`tests/test_workshop_panes_powers.cpp`,
`tests/test_workshop_panes_sampling.cpp`), `workshop_load` (`tests/test_workshop_load.cpp`:
providers mounted and consumers offered from a plan) and `timer` (`tests/test_timer.cpp`).
Outside CTest, `tests/package/public_surface.cpp` compiles its installed headers.

## Host side

- `workshop/workshop.cpp` holds the run's one `op::Catalog`, mounts the host's own Sources
  (`workshop/host_sources.hpp`) and offers it through an `op::OperatorHostSurface`.
- `workshop/load_execute.hpp` mounts each plan provider (`op::mount_provider`) and scopes an
  `op::OperatorOffer` around a load; `workshop/load_plan.hpp` spells `op::MountMode`.
- `workshop/default-load-plan.json` and `workshop/graphical-load-plan.json` name
  `zengine-operators-basic`; `workshop/CMakeLists.txt` stages it; `cmake/ZengineInstall.cmake`
  installs it and the headers.
- Doors over the catalog: `zengine.powers` (`workshop/powers_door.hpp`) and `zengine.sources`
  (`workshop/sample_door.hpp`). `workshop/session_persist.hpp` looks conversions up, and
  `workshop/session_migration_provider.cpp` supplies them.
- Other consumers: the Timer (`timer/normalize.hpp`, `timer/timer.cpp`), `flow/`, `maker/` and
  `introspection/introspection.cpp`.

## Pages

- [The operator host — how a loaded tool spends the host's operator truth](docs/operator-host.md)
- [Operator providers — where a host's powers come from](docs/operator-providers.md)
- [Sources — the catalog entries you can spend with nothing in hand](docs/operator-sources.md)

## Practices

- [Operators](../docs/contributing/best-practices.md#operators-operator)
