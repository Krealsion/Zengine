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
| [workshop/docs/getting-started.md](../workshop/docs/getting-started.md) | **getting started** | a weaver, launching and using Workshop |

## Guides — task-shaped

| page | purpose |
|---|---|
| [flow/docs/flow-guide.md](../flow/docs/flow-guide.md) | author a stateful message-driven rule, edit it live, save and generate native C++ |
| [timer/docs/timers.md](../timer/docs/timers.md) | ordering a timer: the shapes, the receipts, the `TimerReady` rule |
| [timer/docs/timed-weaves.md](../timer/docs/timed-weaves.md) | a weave whose rhythm is part of what it is, and where that layer's boundary lies |
| [workshop/docs/make-a-workshop-tool.md](../workshop/docs/make-a-workshop-tool.md) | build a loaded Tally pane, add an authorized typed interaction, recognize its answer/refusal, and diagnose a missing reply grant |

## Workshop — the product

| page | purpose |
|---|---|
| [workshop/docs/getting-started.md](../workshop/docs/getting-started.md) | launch, the screen, the first five minutes, the key map |
| [workshop/docs/panes.md](../workshop/docs/panes.md) | opening, moving, resizing and ordering panes — how a bigger one is actually obtained, and how a pane of your own is made from data |
| [inventory/docs/inventory-compose.md](../inventory/docs/inventory-compose.md) | typed drops, explicit command submission and a timed external-host demo |
| [inventory/docs/inventory-slots.md](../inventory/docs/inventory-slots.md) | portable boxes and strips, disabled duplicate bindings and the live demo |
| [inventory/docs/inventory-folders.md](../inventory/docs/inventory-folders.md) | organize Inventory in named nested folders: open, climb, file, move and keep them in toolboxes, with the organized workbench story |
| [inventory/docs/toolboxes.md](../inventory/docs/toolboxes.md) | save typed Inventory toolboxes across restarts and restore a test fixture with one request |
| [info/docs/info-views.md](../info/docs/info-views.md) | several independent Info views: visible controls, field-to-field transfer, watching a linked entry, sampling a source again, and the inspection-workbench toolbox |
| [workshop/docs/hotkeys.md](../workshop/docs/hotkeys.md) | the one binding truth: the hotkey view, the band legend, and the hand-edited keymap file |
| [attention/docs/attention.md](../attention/docs/attention.md) | what is true right now and worth knowing — the compact indicator, the current-condition view, and why hiding one is not fixing it |
| [workshop/docs/demo-setups.md](../workshop/docs/demo-setups.md) | one-command ready setups: find and describe them, prepare and return, visible Reset, hotkeys a setup turns on, authoring a setup, measurements |
| [workshop/docs/setups.md](../workshop/docs/setups.md) | the three persisted files, saving an arrangement under a name, the last session that comes back on its own, and an explicit verdict on workspace continuity |
| [workshop/docs/load-plans.md](../workshop/docs/load-plans.md) | choosing what a run is made of, from a weaver's side |
| [builder/docs/builder.md](../builder/docs/builder.md) | authored build recipes, the two recipe kinds and a CMake target's editing entry, authoring a recipe from Files, load after build, reload in place, reading what a build said, and loading a built artifact into the plan |
| [flow/docs/flow.md](../flow/docs/flow.md) | author and exercise a stateful weave graphically, retain message examples, and save the workspace |
| [view/docs/view-builder.md](../view/docs/view-builder.md) | make a small panel by hand beside Flow, run it as a participant of its own, and join it to a Flow definition by dragging its shapes |
| [editor/docs/editor.md](../editor/docs/editor.md) | the Editor pane — open a source, edit, save, and back to the build; the pane holds the document |
| [files/docs/files.md](../files/docs/files.md) | the Files pane — browse the project you launched in and open a file from it |
| [terminal/docs/terminal.md](../terminal/docs/terminal.md) | the Terminal pane — type a command to the weaves on this bus, recall one you ran, read back through the record, and choose a destination from what is there |
| [editor/docs/neovim.md](../editor/docs/neovim.md) | **walkthrough**: edit in Neovim inside Workshop, switch the Editor between the standard Editor and Neovim with your unsaved work, and run Neovim from a Loom with no Workshop |
| [builder/docs/edit-a-running-pane.md](../builder/docs/edit-a-running-pane.md) | **walkthrough**: right-click a running pane, edit its code, build and reload it in place, then revert or promote — with the Tally example |
| [workshop/docs/develop-workshop.md](../workshop/docs/develop-workshop.md) | **walkthrough**: change a pane Workshop ships from inside Workshop — one Run that launches it, the development catalog and runtime, a failed build read in the Builder, which panes, and what a setup does not follow |
| [external-host/docs/external-host.md](../external-host/docs/external-host.md) | **walkthrough**: drive Workshop from another Loom host — the guests file that admits one and the Connections pane that shows who is connected, then a reader-intent table routing to whichever path fits: a Loom session's editable Python tools (no compiler, recommended), or a compiled probe weave that opens an input session, presses keys, takes a picture and keeps the exchange in its own history; and the desk read by message, whole, and written as text for an agent |
| [external-host/docs/elh-recipe-journey.md](../external-host/docs/elh-recipe-journey.md) | **walkthrough**: prepare a multi-config fixture, author through Files using ELH tools, verify refusal and saved values, and recover the story from another client |
| [../examples/tower-defense/README.md](../examples/tower-defense/README.md) | **worked example**: a small game made from inside Workshop by an external host -- typed in Neovim, reloaded in place by the Builder after each milestone, its commands kept in Inventory -- and a script that replays how from an empty directory, at a pace you choose |
| [workshop/docs/limitations.md](../workshop/docs/limitations.md) | **what does not work yet**, in one place |

## Reference — exact contracts

| page | purpose |
|---|---|
| [input/docs/input.md](../input/docs/input.md) | the Input package: what each shape preserves, and which backend produces it |
| [surface/docs/surface.md](../surface/docs/surface.md) | the drawing vocabulary, the rule for choosing between its text shapes, the depth model |
| [ui/docs/ui.md](../ui/docs/ui.md) | authored versus resolved geometry, and the fence between them |
| [component/docs/component.md](../component/docs/component.md) | reusable editing, list and motion helpers |
| [activation/docs/activation.md](../activation/docs/activation.md) | the activation cursor: when a weave acts on a `zen.Activated`, and why the Loom's attestation comes before the weave's own lineage |
| [builder/docs/builder-reference.md](../builder/docs/builder-reference.md) | the Builder package: authored recipes, the generated single-source project, process custody, and the seam to realization |
| [examples/snake/README.md](../examples/snake/README.md) | a worked example whose parts are genuinely separate weaves |
| [timer/docs/timer-protocol.md](../timer/docs/timer-protocol.md) | exact Timer semantics |
| [timer/docs/timer-continuity.md](../timer/docs/timer-continuity.md) | what a schedule does across the service's own replacement |
| [timer/docs/timer-binding.md](../timer/docs/timer-binding.md) | the `TimedWeave` model and its boundary |
| [workshop/docs/load-plan.md](../workshop/docs/load-plan.md) | the authored load plan: format, execution law, rollback |
| [workshop/docs/workshop-panes.md](../workshop/docs/workshop-panes.md) | the contracts a Workshop tool author builds on: the pane system, named setups, the pane a weave offers and its protocol, presses, menus, authorized input, the desk and each pane's words read by message, and the desk that comes back |
| [editor/docs/editor-switch.md](../editor/docs/editor-switch.md) | switching the Editor's office: a plan's choices, the four messages and their answers, the handoff, and exactly what crosses, resets, needs consent or is refused |
| [introspection/docs/introspection.md](../introspection/docs/introspection.md) | `Loaded`, `Project`, `Powers` — what each shows, where each fact's authority lives, and why two of them deliberately disagree |
| [operator/docs/operator-host.md](../operator/docs/operator-host.md) | how a loaded weave asks a host to evaluate a rule it did not compile with, and the five ways it can fail |
| [operator/docs/operator-providers.md](../operator/docs/operator-providers.md) | how an artifact supplies operator definitions, how one power may be shadowed then revealed, and how a contribution becomes the conversion that reads an older file |
| [flow/docs/flow-reference.md](../flow/docs/flow-reference.md) | Flow authoring, graph workspaces, persistence, native generation, recovery and host lifetimes |
| [message-draft/docs/message-drafts.md](../message-draft/docs/message-drafts.md) | typed value editing, unfinished drafts, named presets, schema closure and compatibility |
| [inventory/docs/inventory.md](../inventory/docs/inventory.md) | owned typed entries, metadata, portable views, contextual hotkeys and lifetime |
| [editor/docs/source-transfer.md](../editor/docs/source-transfer.md) | the text, command and file-location material the Editors carry to Inventory and take back: shapes, byte laws, the Terminal line, generated C++ |
| [flow/docs/flow-runtime.md](../flow/docs/flow-runtime.md) | Flow sessions on an existing host: authority, live editing, dispatch observations and custody |
| [view/docs/view.md](../view/docs/view.md) | a view described as data: the description and its format, the view host, and the renderer that draws it on a pane's canvas |
| [maker/docs/maker-weave.md](../maker/docs/maker-weave.md) | the maker weave: the two artifacts a definition and a state are, what a trigger is, and the two ways a live definition is edited |
| [operator/docs/operator-sources.md](../operator/docs/operator-sources.md) | the catalog entries you can spend with nothing in hand: what a Source is, sampling one, seeing what a sample would yield without sampling it |
| [workshop/docs/pointer-spaces.md](../workshop/docs/pointer-spaces.md) | where a reported pointer position lands, and which package owns each step |

## Invariants and decisions

| page | purpose |
|---|---|
| [timer/docs/timer-laws.md](../timer/docs/timer-laws.md) | TIMER-01..05 |
| [timer/docs/timer-continuity-carries-remaining-duration.md](../timer/docs/timer-continuity-carries-remaining-duration.md) | why durations rather than deadlines |

## Contributing

| page | purpose |
|---|---|
| [contributing/taking-an-issue.md](contributing/taking-an-issue.md) | from an issue labelled `ready` to a pull request ready to merge, with nothing outside this repository's own documentation |
| [contributing/best-practices.md](contributing/best-practices.md) | **best practices**: what good work looks like here, surface by surface, each practice pointing at the law or page that owns it |
| [contributing/build-and-test.md](contributing/build-and-test.md) | every configuration, the verification lanes, and what a green means |
| [contributing/testing-workshop-panes.md](contributing/testing-workshop-panes.md) | arrange loaded-pane gestures, grants, delayed replies and layout without mistaking fixture behavior for a product defect |
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
