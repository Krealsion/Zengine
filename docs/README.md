# Zengine documentation

Zengine documents what it owns: its packages, and Workshop. Substrate truth — messaging,
lifecycle, replacement, capabilities — belongs to the Loom and lives in
[Loom's documentation](https://github.com/Krealsion/Loom/blob/main/docs/README.md).

Every page has one reader purpose, named. A folder's own pages — its guides, reference pages,
laws and decisions — are listed on its front page, its `README.md`; this page is the map of the
front pages, and the index of the pages no one folder owns.

## Start here

| page | purpose | for |
|---|---|---|
| [../README.md](../README.md) | **orientation** | what Zengine is, whether it is mature enough for you, how to build it, and the map of its folders |
| [getting-started.md](getting-started.md) | **getting started** | a C++ developer, from nothing to a running weave that uses the Timer |
| [../cheat_sheet.md](../cheat_sheet.md) | **cheat sheet** | looking something up while you work |
| [../workshop/docs/getting-started.md](../workshop/docs/getting-started.md) | **getting started** | a weaver, launching and using Workshop |
| [../workshop/docs/limitations.md](../workshop/docs/limitations.md) | **limitations** | what does not work yet, in one place |

## The map

**Features** — what a weaver meets by name in Workshop, each with its parts and its pages:

| front page | what it is |
|---|---|
| [attention/](../attention/README.md) | what needs your attention, at a glance and in a pane of its own |
| [builder/](../builder/README.md) | the Builder: build a recipe, then load or reload what it made |
| [composer/](../composer/README.md) | Compose: fill in a typed message and send it |
| [editor/](../editor/README.md) | the Editor, standard or held by a Neovim, and the source material it carries |
| [external-host/](../external-host/README.md) | Workshop driven from another Loom host, and the Connections pane |
| [files/](../files/README.md) | Files: browse the project you launched in and open a file |
| [flow/](../flow/README.md) | Flow: author a stateful weave as a graph, edit it live, generate native C++ |
| [info/](../info/README.md) | Info: independent views that inspect and edit a value |
| [introspection/](../introspection/README.md) | Loaded, Project and Powers: what a running system is made of |
| [inventory/](../inventory/README.md) | Inventory: stored typed values, portable toolboxes and hotkeys |
| [terminal/](../terminal/README.md) | the Terminal: type a command to the weaves on the bus |
| [view/](../view/README.md) | a view described as data, the view host, and the View Builder |

**The host** — [workshop/](../workshop/README.md): Workshop, the interactive environment for
weavers, with the Pane Manager, Hotkeys and the menus.

**Libraries** — what a program links or a weave builds on by name:

| front page | what it owns |
|---|---|
| [timer/](../timer/README.md) | the clock, the only sleep, one beat chain per activation; the Timer laws |
| [input/](../input/README.md) | the sole producer of key, text and pointer moments |
| [surface/](../surface/README.md) | drawing intent, and the skins that paint it |
| [ui/](../ui/README.md) | authored placement and extent, resolved against a viewport |
| [component/](../component/README.md) | reusable pieces of a tool: text editing, list and table arithmetic, a control strip, motion sampling |
| [activation/](../activation/README.md) | reading your own activation, once, without replay |
| [operator/](../operator/README.md) | typed reusable rules, supplied by artifacts |
| [maker/](../maker/README.md) | a weave built from a weaver's definition, edited live |
| [message-draft/](../message-draft/README.md) | typed value forms, unfinished drafts and named presets |

**Examples** — [examples/](../examples/README.md): worked examples, among them a game whose parts
are separate weaves and a small game made from inside Workshop.

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
