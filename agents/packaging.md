# Agent law — Packaging

Routed detail behind [`AGENTS.md`](../AGENTS.md), for tasks touching `cmake/ZengineInstall.cmake`,
any public header, or any exported target's link line. The installed-package witness lane is in
[`verification.md`](verification.md).

## Zengine is a package a stranger installs

`cmake/ZengineInstall.cmake` is the whole public consumer surface, in one file: which targets
are exported, which headers are installed, which artifacts ride along, and the reason each
package is or is not in the set. Change the boundary there, not in a package's own
`CMakeLists.txt`.

```cmake
find_package(zengine 0.1 CONFIG REQUIRED)     # resolves Zengine's Loom dependency too
target_link_libraries(my-weave PRIVATE zengine::timer loom::switchboard)
```

**Exported capability targets**, `EXPORT_NAME`d to match their in-tree `zengine::` aliases so the
house and a guest spell them identically: `activation`, `timer`, `surface`, `input`, `ui`,
`component` (the text box, the four list mechanics `list_window`, `row_map`, `held_choice` and
`columns`, the control strip, and pure `motion::Path` sampling: every header
`zengine_public_headers_component` lists), `operator`, `operator-consumer`, `pane` (the protocol
header `workshop/pane_vocabulary.hpp`, `workshop/pane_canvas_vocabulary.hpp`, and the optional
helpers beside them, `workshop/pane_menu.hpp`, `workshop/pane_canvas_text.hpp`,
`workshop/pane_text_fit.hpp` and `workshop/pane_canvas_rows.hpp`,
plus `workshop/pane_operation.hpp`, `workshop/pane_carry.hpp`, `workshop/pane_settings.hpp`,
`workshop/pane_view.hpp`, `workshop/pane_shortcuts.hpp` and `workshop/setup_control.hpp`,
installed under `include/zengine/workshop/`), `neovim` (the one
header `editor/neovim-editor/vocabulary.hpp`: the asks a console sends the Neovim-backed Editor, and
none of the hosting library behind them), `maker` (data-authored weaves and succession),
`message-draft` (`draft.hpp`, `library.hpp` and `transfer.hpp`: typed editing, schema closure and reusable
value persistence), `inventory` (`codec.hpp`, `vocabulary.hpp`, `grant.hpp` and
`inventory/inventory-pane/vocabulary.hpp`: the inventory
item+metadata envelope, its wire shapes and an explicit bounded host grant; decoding needs no
compiled knowledge of the item's or a metadata entry's schema; portable-view configuration is
owned separately by the pane), `source-transfer` (the one header
`editor/source-transfer/vocabulary.hpp`: the text, selection, location and location-context shapes the
Editors carry, so a stranger's pane can make or recognize them; the byte laws, the Terminal line
and the C++ generator stay internal), `graphics` (the vocabulary an owner draws with -- a node,
an element record, the seven operators, the roles and a canvas -- with the eight elements, a
drawing weave's helper, the painter code a skin runs for characters and for light, the two seams a
skin or an element's code is written against, `element_abi.h` and `painter_abi.h` with their
export helpers, and the terminal's claim and size, `window/terminal_io.hpp`: every header
`zengine_public_headers_graphics`, `zengine_public_headers_graphics/elements` and
`zengine_public_headers_graphics/window` list, installed under `include/zengine/graphics/` and
its `elements/` and `window/`; the directory and the windows are artifacts, below), and `flow` (standalone
authoring, `graph_edit.hpp`, `workspace.hpp`,
persistence, generated-runtime support, and the public control vocabularies
`flow/flow-host/vocabulary.hpp` and `flow/flow-pane/vocabulary.hpp`). The optional
`zengine-flow` executable installs to `bin` when a Loom kernel is available. A plain hyphenated name on a link line means the
target is internal, and that difference is the boundary made visible.

**The artifacts `zengine_installable_artifacts` lists** install to `lib/zengine/`, named by
`ZENGINE_RUNTIME_ARTIFACTS` and located by `ZENGINE_ARTIFACT_DIR`: `zengine-app-weave` (an app's
own weave, which `zengine-app` runs), `zengine-timer`, `zengine-input`, `zengine-inventory`,
`zengine-neovim-editor` (whose one runtime need beyond the Loom is a Neovim, found when it
starts one), `zengine-operators-basic`, `zengine-guest-vocabulary` (booted by another
Loom host, a session, to speak Workshop's guest shapes), and the graphics directory and terminal
window (`zengine-graphics`, `zengine-window-terminal`) with the four painters a window opens by
path (`zengine-paint-classic` and `zengine-paint-block` for characters, `zengine-paint-day` and
`zengine-paint-night` for light). They install as FILES, not as
exported targets — an artifact is opened by path and never linked, and an imported target would
offer a link line that must never be written. Beside them, as data, the Workshop tool packages
for a Loom session's run manager: `share/zengine/loom-tools/workshop`, the journeys and their
setups, each feature's tutorials (`<feature>/docs/tutorials/<name>/`) among them, exported whole by
`demo.py export` so the files a tutorial shares from elsewhere in the tree travel with it;
`share/zengine/loom-tools/desk`, the agent's desk guide and resolver; and beside each its
`-machine` package (`workshop-machine`, `desk-machine`), the tools that also reach this machine;
and `share/zengine/loom-tools/accounts-machine`, a checkout's Git and pull requests, which reach the
session user's accounts. An operator approves a package whole, so a package's name says how far
its tools reach. The generic
`zengine-inventory-read` executable installs to `bin` and reads those tools' saved pairs, and so
does `zengine-app`, the host an app runs in with no Workshop (`app/docs/app.md`): it opens the
artifacts from `lib/zengine/` by a path relative to its own, so it names no machine's layout, and
with the OS window out it runs an app whose window is the terminal window.

**One CMake function rides along**, `zengine_code_page(<target>...)`: `cmake/ZengineCodePage.cmake`
and the manifest it gives install beside the package config, which includes the module. A stranger's
program calls it to run in the UTF-8 code page on Windows, as every program this repository
builds does (VM-PLAT-16); elsewhere it does nothing. The installed-package witness calls it for
every program of its own and, on Windows, refuses one whose manifest names no UTF-8 code page;
`public_surface.cpp` checks the code page it runs in.

**ARTIFACT is the noun, and the distinction is load-bearing.** An artifact is the
physical loadable file; *weave* and *provider* are runtime SURFACES an artifact may expose.
Ten of the fifteen above are weaves; `zengine-operators-basic` is a provider and the four
painters are painter code, none of the five a weave (enforced by `zengine_provider()` in the top-level
`CMakeLists.txt`). The public
package variables must therefore name the physical thing: a variable named after one surface
is false of its own contents the moment the list holds another. The `package_vocabulary`
CTest entry keeps the retired spellings from coming back — it owns the list of them, so this
page does not spell them.

**What is deliberately out of the developer package, and why** (each is a limit, not an
oversight): the OS window (`zengine-window-sdl`), because a fetched SDL is a build-tree library
the developer package does not carry; Workshop, because a
build's executable carries `ZENGINE_BUILDER_CMAKE`, the absolute path of the cmake that configured
it, so installing it would ship one machine's layout -- the package below carries both;
the implementation in `flow/flow-host/runtime.hpp` and the Flow pane model/view,
because these currently implement Workshop session policy and presentation; the view package
(`view/`) and the View Builder, because this repository's own hosts, Workshop and `zengine-app`,
compose the view host and are the description format's consumers. Their control
vocabularies are public through `zengine::flow`, so another participant can drive them; the rest of the Workshop/Builder/introspection/composer vocabularies and the
pane weaves that spend them (`zengine-files`, `zengine-builder-pane`), because they are Workshop
being Workshop — a stranger's pane needs only the pane protocol, which is in, since a pane
arrives by an authored load-plan row; the Timer's service headers, because using the Timer is
supported and *being* one has not been measured.

## The package a stranger downloads

`ZENGINE_PACKAGE=ON`, declared in the top-level `CMakeLists.txt` before any package, installs the
developer package above **and Workshop ready to run** into the same prefix
(`cmake/ZengineInstall.cmake`, its one owner): the host and every artifact, program and plan it
stages, read from `workshop/CMakeLists.txt`'s own lists, in `bin/`, where it resolves them beside
itself; the SDL it built and fetched; the launchers, `README.txt`, `BUILD-INFO.txt`, `NOTICE.md`
and each third party's licence text (`workshop/package/`). Under it:

- the version is `ZENGINE_PACKAGE_VERSION`, read from the night's tag, and Loom is named by
  `ZENGINE_PACKAGE_LOOM_COMMIT`; `project(VERSION)` stays the CMake package's compatibility line;
  `zengine-workshop --version` says the package's facts as `BUILD-INFO.txt` does;
- Workshop compiles no CMake, so its Builder refuses in its own words and its banner says it
  builds nothing;
- the launcher's demo service is the package's own `share/zengine/loom-tools/demo.py`, beside the
  tool packages, started against the package by a Python 3.8 or newer found on the PATH, which the
  package does not carry (`launcher/CMakeLists.txt`, [the launcher](launcher.md));
- every compile maps its source, build and prefix folders away (`-ffile-prefix-map`), the fetched
  SDL's included, and on MinGW-w64 every executable links its runtime statically, so an image names
  no build machine and needs nothing beside the package but the system;
- the launchers make the weaver's project outside the package (`%USERPROFILE%\Zengine`,
  `~/Zengine`, `terminal/` inside for the terminal's) and write `zengine-app.path` there, the
  package's `zengine-app`, which an exported app's `run.cmd` and `run.sh` start (`app/folder.hpp`).

It is a Release build with GCC or Clang, refused otherwise at configure, installed with
`cmake --install --strip`; the installed-package witness's package arm
(`tests/package/run.cmake -DZEN_PACKAGE=ON`) installs it so and starts it.

**How the package is made** (`.github/workflows/package.yml`):
- **When.** Each night, from the newest `main` commit whose CI push run passed and is newer than the
  last `v0.1.*` tag. `decide` reads that run's `loom-commit` artifact, which `ci.yml`'s
  `resolve-loom` keeps, and builds at that Loom commit. When nothing moved, the night ends green
  and says so. A pull request touching how the package is made runs the same jobs on its own head,
  at Loom `main`, as a dry run.
- **Built** cold and Release, with the version the next tag's:
  - Linux in Ubuntu 22.04 pinned by digest, its packages from Ubuntu's archive on one named day,
    and SDL with its X11 and Wayland drivers (`cmake/ZengineSdl.cmake` keeps SDL's guard under the
    switch);
  - Windows with WinLibs' GCC UCRT archive, pinned by SHA-256 and checked against the checksum
    WinLibs publishes.
  - Both repositories install `--strip` into one folder, packed by `tests/package/pack.py`, so the
    same files make the same bytes.
- **Judged** on a runner that did not build it, extracted under a folder named with a space and
  letters outside the code page: `--version` naming the commit and a clean tree, then the judges
  (`tests/package/judges/`), then the story (`tests/package/story.py`): the tally setup started
  from the package by the package's own demo service (`demo.py start --package`), its walk
  `export` passed, the exported app's
  launcher naming the package's `zengine-app`, the app answered at its console until every row
  runs, and still running once Workshop has stopped. Its pictures and the app's console are kept on
  the run, red or green. Then `SHA256SUMS`.
- **What a red judge does.** Nothing is published while any judge is red.
- **Published** by a night, or by a dispatch asked to (`publish: true`); a pull request and a
  dispatch that is not asked publish nothing. The workflow's own token alone does it:
  - `attest`, holding `id-token` and `attestations: write` alone, attests each archive and
    `SHA256SUMS`;
  - `publish`, holding `contents: write` alone, refuses a tag `v0.1.N` that already exists and
    takes down a draft of that name a failed night left. It opens a draft with the notes
    (`workshop/package/release-notes.md`, each `@NAME@` the night's), aimed at the packaged
    commit, downloads its assets back and matches their sums, then publishes it. Publishing
    makes the tag, so a night that fails before then leaves no tag naming no release; the tag is
    read back on the packaged commit before anything else is put down.
  - Then every other release is deleted, and its tag stays: the Releases page holds one package,
    and a tag still finds the commit a stranger ran.

Header spelling does not change across the boundary: installed headers land under
`include/zengine/<package>/`, so `#include "timer/vocabulary.hpp"` is the same sentence from
either side.

## Do not assume

- Adding a header to a package makes it public — the installed set is named one file at a
  time in `cmake/ZengineInstall.cmake`, and anything a public header includes must be
  installed with it or the stranger stops compiling.
- An exported target may rely on a link it gets transitively in this tree — in this tree
  every include path is the same directory, so a missing `loom::core` is invisible here and
  fatal from an installed prefix. Exported targets link what their public headers use, on
  their own line.
- `PACKAGE_PREFIX_DIR` still means this package after `find_dependency(...)` — it is an
  ordinary variable, and the dependency's own config overwrites it. `zengineConfig.cmake.in`
  resolves every path of its own **before** it finds the Loom, and says so.
- Installing under `ZEN_LOOM_DEV=ON` produces a usable package — it is refused, because the
  export would name `loom::core` with no installed Loom to resolve it.
