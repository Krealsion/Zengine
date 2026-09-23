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
| [guides/flow.md](guides/flow.md) | author a stateful message-driven rule, edit it live, save and generate native C++ |
| [guides/timers.md](guides/timers.md) | ordering a timer: the shapes, the receipts, the `TimerReady` rule |
| [guides/timed-weaves.md](guides/timed-weaves.md) | a weave whose rhythm is part of what it is, and where that layer's boundary lies |
| [guides/make-a-workshop-tool.md](guides/make-a-workshop-tool.md) | build a loaded Tally pane, add an authorized typed interaction, recognize its answer/refusal, and diagnose a missing reply grant |

## Workshop — the product

| page | purpose |
|---|---|
| [workshop/getting-started.md](workshop/getting-started.md) | launch, the screen, the first five minutes, the key map |
| [workshop/panes.md](workshop/panes.md) | opening, moving, resizing and ordering panes — how a bigger one is actually obtained, and how a pane of your own is made from data |
| [workshop/inventory-compose.md](workshop/inventory-compose.md) | typed drops, explicit command submission and a timed external-host demo |
| [workshop/inventory-slots.md](workshop/inventory-slots.md) | portable boxes and strips, disabled duplicate bindings and the live demo |
| [workshop/hotkeys.md](workshop/hotkeys.md) | the one binding truth: the hotkey view, the band legend, and the hand-edited keymap file |
| [workshop/attention.md](workshop/attention.md) | what is true right now and worth knowing — the compact indicator, the current-condition view, and why hiding one is not fixing it |
| [workshop/demo-setups.md](workshop/demo-setups.md) | one-command isolated value and command demos, visible Reset, repeated stories and ceremony measurements |
| [workshop/setups.md](workshop/setups.md) | the three persisted files, saving an arrangement under a name, the last session that comes back on its own, and an explicit verdict on workspace continuity |
| [workshop/load-plans.md](workshop/load-plans.md) | choosing what a run is made of, from a maker's side |
| [workshop/builder.md](workshop/builder.md) | authored build recipes, the two recipe kinds and a CMake target's editing entry, authoring a recipe from Files, load after build, reload in place, reading what a build said, and loading a built artifact into the plan |
| [workshop/flow.md](workshop/flow.md) | author and exercise a stateful weave graphically, retain message examples, and save the workspace |
| [workshop/editor.md](workshop/editor.md) | the Editor pane — open a source, edit, save, and back to the build; the pane holds the document |
| [workshop/files.md](workshop/files.md) | the Files pane — browse the project you launched in and open a file from it |
| [workshop/terminal.md](workshop/terminal.md) | the Terminal pane — type a command to the weaves on this bus, recall one you ran, read back through the record, and choose a destination from what is there |
| [workshop/neovim.md](workshop/neovim.md) | **walkthrough**: edit in Neovim inside Workshop, switch the Editor between the standard Editor and Neovim with your unsaved work, and run Neovim from a Loom with no Workshop |
| [workshop/edit-a-running-pane.md](workshop/edit-a-running-pane.md) | **walkthrough**: right-click a running pane, edit its code, build and reload it in place, then revert or promote — with the Tally example |
| [workshop/develop-workshop.md](workshop/develop-workshop.md) | **walkthrough**: change a pane Workshop ships from inside Workshop — one Run that launches it, the development catalog and runtime, a failed build read in the Builder, which panes, and what a setup does not follow |
| [workshop/external-host.md](workshop/external-host.md) | **walkthrough**: drive Workshop from another Loom host — the guests file that admits one and the Connections pane that shows who is connected, then a reader-intent table routing to whichever path fits: a Loom session's editable Python tools (no compiler, recommended), or a compiled probe weave that opens an input session, presses keys, takes a picture and keeps the exchange in its own history |
| [workshop/elh-recipe-journey.md](workshop/elh-recipe-journey.md) | **walkthrough**: prepare a multi-config fixture, author through Files using ELH tools, verify refusal and saved values, and recover the story from another client |
| [workshop/limitations.md](workshop/limitations.md) | **what does not work yet**, in one place |

## Reference — exact contracts

| page | purpose |
|---|---|
| [reference/input.md](reference/input.md) | the Input package: what each shape preserves, and which backend produces it |
| [reference/surface.md](reference/surface.md) | the drawing vocabulary, the rule for choosing between its text shapes, the depth model |
| [reference/ui.md](reference/ui.md) | authored versus resolved geometry, and the fence between them |
| [reference/component.md](reference/component.md) | reusable editing, list and motion helpers |
| [reference/builder.md](reference/builder.md) | the Builder package: authored recipes, the generated single-source project, process custody, and the seam to realization |
| [reference/snake.md](reference/snake.md) | a worked example whose parts are genuinely separate weaves |
| [reference/timer-protocol.md](reference/timer-protocol.md) | exact Timer semantics |
| [reference/timer-continuity.md](reference/timer-continuity.md) | what a schedule does across the service's own replacement |
| [reference/timer-binding.md](reference/timer-binding.md) | the `TimedWeave` model and its boundary |
| [reference/load-plan.md](reference/load-plan.md) | the authored load plan: format, execution law, rollback |
| [reference/editor-switch.md](reference/editor-switch.md) | switching the Editor's office: a plan's choices, the four messages and their answers, the handoff, and exactly what crosses, resets, needs consent or is refused |
| [reference/introspection.md](reference/introspection.md) | `Loaded`, `Project`, `Powers` — what each shows, where each fact's authority lives, and why two of them deliberately disagree |
| [reference/operator-host.md](reference/operator-host.md) | how a loaded weave asks a host to evaluate a rule it did not compile with, and the five ways it can fail |
| [reference/operator-providers.md](reference/operator-providers.md) | how an artifact supplies operator definitions, how one power may be shadowed then revealed, and how a contribution becomes the conversion that reads an older file |
| [reference/flow.md](reference/flow.md) | Flow authoring, graph workspaces, persistence, native generation, recovery and host lifetimes |
| [reference/message-drafts.md](reference/message-drafts.md) | typed value editing, unfinished drafts, named presets, schema closure and compatibility |
| [reference/inventory.md](reference/inventory.md) | owned typed entries, metadata, portable views, contextual hotkeys and lifetime |
| [reference/flow-runtime.md](reference/flow-runtime.md) | Flow sessions on an existing host: authority, live editing, dispatch observations and custody |
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
