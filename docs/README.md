# Zengine documentation

Zengine documents what it owns: its packages, and Workshop. Substrate truth — messaging,
lifecycle, replacement, capabilities — belongs to the Loom and lives in
[Loom's documentation](https://github.com/Krealsion/Loom/blob/main/docs/README.md).

Every page below has one reader purpose, named.

## Start here

| page | purpose | for |
|---|---|---|
| [../README.md](../README.md) | **orientation** | what Zengine is, whether it is mature enough for you, how to build it |
| [getting-started.md](getting-started.md) | **getting started** | a C++ developer, from nothing to a running weave that uses the Timer |
| [../cheat_sheet.md](../cheat_sheet.md) | **cheat sheet** | looking something up while you work |
| [workshop/getting-started.md](workshop/getting-started.md) | **getting started** | a maker, launching and using Workshop |

## Guides — task-shaped

| page | purpose |
|---|---|
| [guides/timers.md](guides/timers.md) | ordering a timer: the shapes, the receipts, the `TimerReady` rule |
| [guides/timed-weaves.md](guides/timed-weaves.md) | a weave whose rhythm is part of what it is, and where that layer's boundary lies |
| [guides/make-a-workshop-tool.md](guides/make-a-workshop-tool.md) | adding a Workshop tool, sorted into its two authoring paths: a **compiled-in panel** (identity, granted room, publishing rows, one pointer-and-hotkey operation, where each kind of state belongs) or an **office-authored external pane** (four shapes, a prose budget, no input, no installation story yet) |

## Workshop — the product

| page | purpose |
|---|---|
| [workshop/getting-started.md](workshop/getting-started.md) | launch, the screen, the first five minutes, the key map |
| [workshop/panes.md](workshop/panes.md) | opening, moving, resizing and ordering panes — how a bigger one is actually obtained, and how a pane of your own is made from data |
| [workshop/hotkeys.md](workshop/hotkeys.md) | the one binding truth: the hotkey view, the band legend, and the hand-edited keymap file |
| [workshop/attention.md](workshop/attention.md) | what is true right now and worth knowing — the compact indicator, the current-condition view, and why hiding one is not fixing it |
| [workshop/setups.md](workshop/setups.md) | the three persisted files, saving an arrangement under a name, the last session that comes back on its own, and an explicit verdict on workspace continuity |
| [workshop/load-plans.md](workshop/load-plans.md) | choosing what a run is made of, from a maker's side |
| [workshop/builder.md](workshop/builder.md) | authored build recipes, the two recipe kinds, and build & realize |
| [workshop/editor.md](workshop/editor.md) | the built-in source editor — open a source, edit, save, and back to the build |
| [workshop/files.md](workshop/files.md) | the Files pane — browse the project you launched in and open a file from it |
| [workshop/limitations.md](workshop/limitations.md) | **what does not work yet**, in one place |

## Reference — exact contracts

| page | purpose |
|---|---|
| [reference/input.md](reference/input.md) | the Input package: what each shape preserves, and which backend produces it |
| [reference/surface.md](reference/surface.md) | the drawing vocabulary, the rule for choosing between its text shapes, the depth model |
| [reference/ui.md](reference/ui.md) | authored versus resolved geometry, and the fence between them |
| [reference/component.md](reference/component.md) | the Component package, and why it has exactly one component |
| [reference/builder.md](reference/builder.md) | the Builder package: authored recipes, the generated single-source project, process custody, and the seam to realization |
| [reference/snake.md](reference/snake.md) | a worked example whose parts are genuinely separate weaves |
| [reference/timer-protocol.md](reference/timer-protocol.md) | exact Timer semantics |
| [reference/timer-continuity.md](reference/timer-continuity.md) | what a schedule does across the service's own replacement |
| [reference/timer-binding.md](reference/timer-binding.md) | the `TimedWeave` model and its boundary |
| [reference/load-plan.md](reference/load-plan.md) | the authored load plan: format, execution law, rollback |
| [reference/introspection.md](reference/introspection.md) | `Loaded`, `Project`, `Powers` — what each shows, where each fact's authority lives, and why two of them deliberately disagree |
| [reference/operator-host.md](reference/operator-host.md) | how a loaded weave asks a host to evaluate a rule it did not compile with, and the five ways it can fail |
| [reference/operator-providers.md](reference/operator-providers.md) | how an artifact supplies operator definitions, how one power may be shadowed then revealed, and how a contribution becomes the conversion that reads an older file |
| [reference/maker-weave.md](reference/maker-weave.md) | the maker weave: the two artifacts a definition and a state are, what a trigger is, and the two ways a live definition is edited |
| [reference/operator-sources.md](reference/operator-sources.md) | the catalog entries you can spend with nothing in hand: what a Source is, sampling one, seeing what a sample would yield without sampling it |
| [reference/pointer-spaces.md](reference/pointer-spaces.md) | where a reported pointer position lands, and which package owns each step |

## Invariants and decisions

| page | purpose |
|---|---|
| [laws/timer-laws.md](laws/timer-laws.md) | TIMER-01..05 |
| [decisions/timer-continuity-carries-remaining-duration.md](decisions/timer-continuity-carries-remaining-duration.md) | why durations rather than deadlines |

## Contributing

| page | purpose |
|---|---|
| [contributing/build-and-test.md](contributing/build-and-test.md) | every configuration, the verification lanes, and what a green means |
| [contributing/supported-toolchains.md](contributing/supported-toolchains.md) | the platform matrix, and the reloadable-weave build contract |
| [contributing/repository-conventions.md](contributing/repository-conventions.md) | layout, package shape, documentation and comment conventions |

Automated collaborators working in this tree start at [AGENTS.md](../AGENTS.md).

## Architecture

| page | purpose |
|---|---|
| [architecture/README.md](architecture/README.md) | why it is shaped this way; the recurring principles; the cross-pane interaction ownership map; the large-source-unit judgement |

## Frozen

[history/pre-r2c/README.md](history/pre-r2c/README.md) describes the tree it was written
against and is **not** maintained against the current one. It is kept because it is a fuller
account of the Timer package's design than the reference pages carry, not because it is
current. Do not cite it as current.
