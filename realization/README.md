# Realization

**What makes a project run: its load plan, the plan's file, and the realization owner, the one
participant that performs the plan and says where it is.**

A load plan names the artifacts a project is made of and how each takes part. The realization owner
performs its rows in the order they were written -- mount an artifact's operators, then ask the
Weave Manager to load its weave into its role -- and waits at a row whose artifact is not built yet.
It is a weave in the role `zengine.realization`: Workshop and the app host each mount one, and what it
knows -- where the plan is, what it waits on, the rows that gave way, the roles still to come, and
which authored choice holds each switchable role -- it claims as that role, for whoever is granted
to read it.

## What is in this folder

| | |
|---|---|
| `load_plan.hpp` | the typed plan and its law |
| `load_persist.hpp` | the plan's file: read, written and checked |
| `owner.hpp` | the realization owner, and how a host mounts it |
| `vocabulary.hpp` | what the owner claims |
| `docs/zengine.realization.md` | the owner's manual |
| `CMakeLists.txt` | the build: `zengine-realization`, header-only |

The plan's format: [the load plan](../workshop/docs/load-plan.md). Working on it:
[AGENTS.md](AGENTS.md).
