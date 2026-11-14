# `realization/` — Realization: working on it

The router for `realization/`, a library: where its law, its suites, the host's side of it and its
practices are. It states no law. The repository's own rules are the root [AGENTS.md](../AGENTS.md).

## Parts

| | |
|---|---|
| `load_plan.hpp` | the typed load plan and its law: `LoadPlan`, `ArtifactIntent`, `ChoiceIntent`, `check_plan`, `plan_roles`, `remember_choices` |
| `load_persist.hpp` | the plan's file, its one codec: `to_text`, `from_text`, `load_file`, `save_file`, `plan_in_force` |
| `owner.hpp` | the realization owner, one weave in `zengine.realization`: `Owner`, `mount`, `Mounted`, and what a row came to (`ResolvedArtifact`, `Executed`, `PassedOver`) |
| `vocabulary.hpp` | what the owner claims as its role: `PlanStatus`, `ChoicesHeld`, and `latest`, a reader's read of one |
| `docs/zengine.realization.md` | the owner's manual, compiled into whatever mounts it |
| `CMakeLists.txt` | `zengine-realization`: INTERFACE, installed and exported as `zengine::realization` |

## Law

| register | witnessed by |
|---|---|
| [realization](../agents/realization.md) | no proofs: the one test its prose names is `tests/test_operator_provider.cpp` (`operator`) |

What the editor switch and the desk read of the owner's claims is Workshop's law
(`agents/workshop/editor-switch.md`, `agents/workshop/desktop.md`), routed by
[workshop/AGENTS.md](../workshop/AGENTS.md).

## Suites

None of its own: `workshop_load` (`tests/test_workshop_load.cpp`) drives the owner over a real
Kernel, `operator` (`tests/test_operator_provider.cpp`) reads its source for the order of a row and the verbs it never spends,
and `app_host` (`tests/test_app_host.cpp`) realizes the app's plan through it.

## Host side

Workshop (`workshop/workshop.cpp`) and the app host (`app/host.hpp`) each mount one with
`realization::mount`, writing its grant, and begin its plan. What it claims, Workshop's desk, guest
door, project door, layouts' owner and editor switch read by their grants.

## Pages

- [Realization](README.md), the front page
- [The owner's manual](docs/zengine.realization.md)

## Practices

None of its own yet.
