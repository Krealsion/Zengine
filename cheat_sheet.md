# Zengine cheat sheet

An index: a line per topic, each linking the page that owns it. Nothing is stated here; the page a
line links says it, and the checks hold that page to the code. [docs/](docs/README.md) maps every
folder's pages, and [best practices](docs/contributing/best-practices.md) says what good work in
this repository looks like.

## Build and test

- Build and install Loom, build Zengine against it, run the official lane:
  [build and test](docs/contributing/build-and-test.md#the-default-an-installed-loom-package-zen_loom_devoff).
- A sibling Loom source instead of an installed one, `ZEN_LOOM_DEV=ON`:
  [the override](docs/contributing/build-and-test.md#the-override-a-sibling-loom-source-tree-zen_loom_devon).
- Why `tests/verify.cmake` and never a bare `ctest`:
  [verification](docs/contributing/build-and-test.md#run-testsverifycmake-not-a-bare-ctest).
- Which repository's green you proved, and what a green means:
  [per-repo green](docs/contributing/build-and-test.md#per-repo-green),
  [what a green means](docs/contributing/build-and-test.md#what-a-green-means).
- The same suites under ASan and UBSan, `ZENGINE_SANITIZE=ON`:
  [the sanitizer lane](docs/contributing/build-and-test.md#the-sanitizer-lane).
- The installed package, built against from outside the tree:
  [the installed-package witness](docs/contributing/build-and-test.md#the-installed-package-witness).
- A change that touches no compiled line, verified by the documentation lane:
  [the checks whose green counts](docs/contributing/taking-an-issue.md#8-the-checks-whose-green-counts).
- Compilers and platforms, MSVC and MinGW-w64, the Windows kernel:
  [supported toolchains](docs/contributing/supported-toolchains.md#the-matrix),
  [MSVC](docs/contributing/supported-toolchains.md#msvc),
  [MinGW](docs/contributing/supported-toolchains.md#mingw).
- No SDL, the terminal window alone, `ZENGINE_TUI_ONLY=ON`:
  [SDL](docs/contributing/supported-toolchains.md#sdl).

## Using Zengine from another project

- `find_package(zengine)`, linking a capability, and `loom_weave_build_contract()`:
  [getting started](docs/getting-started.md#using-zengine-from-another-project).
- The exported targets and what linking each grants: the same section.
- The loadable artifacts, `ZENGINE_ARTIFACT_DIR` and `ZENGINE_RUNTIME_ARTIFACTS`:
  [run it](docs/getting-started.md#run-it).
- The reloadable-weave build contract:
  [supported toolchains](docs/contributing/supported-toolchains.md#the-reloadable-weave-build-contract).
- What the developer package leaves out, and why:
  [limitations](workshop/docs/limitations.md#the-installable-package-does-not-cover-everything-zengine-builds).
- The package a stranger downloads, ready to run: [download and run](README.md#download-and-run).

## A weave in C++

- A shape, and its version as part of its identity: [a shape](docs/getting-started.md#a-shape).
- A weave: what it accepts and emits, its state, its handlers:
  [a weave](docs/getting-started.md#a-weave).
- An authored rhythm or a delay that is data:
  [`TimedWeave` and the raw protocol](docs/getting-started.md#timedweave-and-the-raw-protocol--which-one).
- A host: its loop, `pump_pending()` and `drain_until_idle()`, and asking with `loom::AskBook`:
  [a host](docs/getting-started.md#a-host).
- It loads and nothing happens, and seeing a refusal:
  [when it does not work](docs/getting-started.md#when-it-does-not-work).
- What a sender learns about its own send:
  [limitations](workshop/docs/limitations.md#a-sender-sees-its-own-sends-fate-only-if-it-asks).
- Publishing, sending, sending to a role, speaking as an office, asking and answering:
  [Loom's messaging reference](https://github.com/Krealsion/Loom/blob/main/docs/reference/messaging.md).

## Libraries

- The Timer: [ordering a timer](timer/docs/timers.md), [an authored rhythm](timer/docs/timed-weaves.md),
  [the binding's factories and handles](timer/docs/timer-binding.md#model),
  [its office](timer/docs/zengine.timer.md), [its laws](timer/docs/timer-laws.md).
- Key, text and pointer moments: [input](input/docs/input.md),
  [pointer spaces](workshop/docs/pointer-spaces.md).
- Drawing in a window's area, the windows and their skins: [graphics](graphics/docs/graphics.md).
- Places and sizes in words, solved in whole pixels: [ui](ui/docs/ui.md).
- Text editing, list and table arithmetic, a control strip, motion: [component](component/docs/component.md).
- Reading your own activation: [activation](activation/docs/activation.md).
- Typed rules supplied by artifacts: [operator providers](operator/docs/operator-providers.md),
  [the operator host](operator/docs/operator-host.md), [sources](operator/docs/operator-sources.md).
- A weave made from a definition: [maker](maker/docs/maker-weave.md); typed value forms and
  drafts: [message drafts](message-draft/docs/message-drafts.md); a weave's manual:
  [manual](manual/docs/manual.md).

## Workshop

- Launching it, in a window or a terminal, and its arguments:
  [launch it](workshop/docs/getting-started.md#launch-it),
  [arguments](workshop/docs/getting-started.md#arguments).
- Your first five minutes: [getting started](workshop/docs/getting-started.md#your-first-five-minutes).
- Every default key, where it is answered: [the default keys](workshop/docs/hotkeys.md#the-default-keys).
- The keys the desktop supplies, and taking them away:
  [hotkeys](workshop/docs/hotkeys.md#keys-the-application-supplies--and-how-to-take-them-away).
- The Hotkeys pane, changing a binding, the band's legend, how a key is spelled:
  [hotkeys](workshop/docs/hotkeys.md).
- The Pane Manager: [panes](workshop/docs/panes.md#showing-going-to-and-hiding--the-pane-manager),
  [the desktop's manual](workshop/docs/zengine.desktop.md).
- Arranging panes: [arrange](workshop/docs/panes.md#moving-resizing-and-ordering--arrange).
- The context menu and the second button:
  [the context menu](workshop/docs/panes.md#the-context-menu--what-can-i-do-with-this),
  [the second button](workshop/docs/panes.md#the-second-button--the-panes-first).
- A pane's size and a bigger pane: [pane geometry](workshop/docs/panes.md#pane-geometry).
- A pane as Info's subject: [Info](workshop/docs/panes.md#a-pane-as-a-subject--info).
- A pane of your own, a view: [a view](workshop/docs/panes.md#a-pane-of-your-own--a-view).
- A pane's manual and its gear: [panes](workshop/docs/panes.md#a-panes-manual-and-its-gear).
- Layouts, setups and the last session: [setups](workshop/docs/setups.md),
  [workspace continuity](workshop/docs/setups.md#workspace-continuity).
- Which artifacts a run is made of: [load plans](workshop/docs/load-plans.md), and the file's
  reference, [the load plan](workshop/docs/load-plan.md).
- Building, and loading what you built: [the Builder pane](builder/docs/zengine.builder-pane.md),
  [recipes](builder/docs/recipes.md),
  [a built artifact loaded again](workshop/docs/load-plans.md#when-a-built-artifact-is-loaded-again).
- Changing a pane while it runs: [edit a running pane](builder/docs/edit-a-running-pane.md),
  [develop Workshop](workshop/docs/develop-workshop.md).
- What is true right now: [Attention](attention/docs/zengine.attention.md).
- What a running system is made of: [Loaded, Project and Powers](introspection/docs/zengine.introspection.md).
- The Editor, standard or Neovim: [the source editor](editor/docs/editor.md),
  [Neovim](editor/docs/neovim.md), [its manual](editor/docs/zengine.editor.md).
- The other panes, each by its manual: [Files](files/docs/zengine.files.md),
  [the Terminal](terminal/docs/zengine.terminal.md), [Compose](composer/docs/zengine.composer.md),
  [Flow](flow/docs/zengine.flow.md), [Inventory](inventory/docs/zengine.inventory-pane.md),
  [Info](info/docs/zengine.info.md), [the View Builder](view/docs/zengine.view.builder.md).
- Workshop driven from another host: [external host](external-host/docs/external-host.md).
- What loads, reloads and unloads: [lifecycle](workshop/docs/limitations.md#lifecycle).
- What does not work yet: [limitations](workshop/docs/limitations.md).

## Words

- Loom's words, weave, shape, role, office, grant, Sense and the rest:
  [Loom's terminology](https://github.com/Krealsion/Loom/blob/main/docs/terminology.md).
- Artifact, weave and provider, three words and not synonyms:
  [run it](docs/getting-started.md#run-it).
- Operator and power: [operator providers](operator/docs/operator-providers.md).
- Load plan: [load plans](workshop/docs/load-plans.md).
- Window, directory and skin: [graphics](graphics/docs/graphics.md).
- Pane, and closing one against unloading its tool: [panes](workshop/docs/panes.md).
- Setup against session: [setups](workshop/docs/setups.md#last-session-versus-named-setup).
- Desktop: [the desktop's manual](workshop/docs/zengine.desktop.md).
- Authored against resolved:
  [a pane's geometry](workshop/docs/panes.md#reading-a-panes-geometry--and-whose-number-it-is).
