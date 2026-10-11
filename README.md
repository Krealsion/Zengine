# Zengine

**A C++20 component library for message-passing applications, and an interactive
environment for building them.**

Zengine is a set of ready-made *weaves* — independently loadable participants that talk
to each other by sending typed, schema-checked messages. It gives you time, input,
drawing, layout and process-running as services you compose, rather than as a framework
you inherit from. It is built on [the Loom](https://github.com/Krealsion/Loom), which
provides the substrate: values, schemas, the admission gate, the message switchboard and
the loader.

There are two ways in, and they are separate on purpose:

| | what it is | start here |
|---|---|---|
| **Zengine** | the C++ library. Link it, write a weave, run it. Workshop is not involved. | [docs/getting-started.md](docs/getting-started.md) |
| **Workshop** | an interactive environment *built with* Zengine, for a *weaver*: the person who writes and runs weaves. Optional. | [workshop/docs/getting-started.md](workshop/docs/getting-started.md) |

A developer looking for the library never has to learn Workshop. A weaver who wants
Workshop never has to read the library's internals.

Keep [**cheat_sheet.md**](cheat_sheet.md) open beside your editor.

## What you can do with it today

- **Write a weave** — a class with a state struct, an accept list and an emit list. Loom
  checks every message against its schema before your handler sees it.
- **Order time** — a Timer service owns the only clock and the only sleep in the process.
  A weave asks for one-shots and repeats and gets them back as messages.
- **Read input** — one Input weave is the sole producer of key, text and pointer moments,
  on a POSIX terminal, a Windows console, or an SDL window.
- **Draw** — publish drawing intent (rectangles, labels, bounded text regions) and let a
  replaceable *skin* paint it to a terminal or a window. Your code names no colours and
  touches no terminal.
- **Load and replace weaves at run time** — through the Loom's Kernel, from an authored
  plan file.
- **Run a build** — start a real OS process from a named recipe and follow it without
  blocking the bus.
- **Look at a live system** — panes that show what is loaded, what the plan asked for, and
  which artifact supplies which power.

## Maturity — read this before you depend on it

**Version 0.1.0. Pre-release. Interfaces change without deprecation cycles.**

What is solid: the message/schema contract, the Timer protocol, the drawing vocabulary,
and the verification discipline (see [Test discipline](docs/contributing/build-and-test.md)).

What is not, stated plainly:

- **The installable package covers the library, not Workshop.** `find_package(zengine)`
  exports capability targets — the pane protocol a one-file Workshop pane speaks among them
  — and installs the loadable artifacts they need. Workshop itself and the SDL-backed skin and
  input reader are deliberately not in it — see [Using Zengine from another
  project](docs/getting-started.md#using-zengine-from-another-project).
- **Linux/WSL with GCC is the only fully-supported configuration.** Windows builds a
  documented subset; the Loom's OS sandbox is Linux-only. See [supported
  toolchains](docs/contributing/supported-toolchains.md).
- **Workshop has real limits** — the desk and the window come back at launch but the file you
  were editing does not, a session is written only on an orderly close, the Builder builds only
  what an authored recipe catalog holds, and a rebuilt weave whose shapes changed is refused
  rather than reloaded in place. Each is written down in
  [Workshop limitations](workshop/docs/limitations.md) rather than left to be discovered.

## Build it

Zengine consumes the Loom as an installed package — the same way any third party would.

```sh
# 1. build and install the Loom
git clone https://github.com/Krealsion/Loom
cmake -S Loom -B Loom/build -DCMAKE_BUILD_TYPE=Debug
cmake --build Loom/build -j
cmake --install Loom/build --prefix "$PWD/deps"

# 2. build Zengine against it
cmake -S Zengine -B Zengine/build -DCMAKE_PREFIX_PATH="$PWD/deps"
cmake --build Zengine/build -j

# 3. verify
cmake -DZEN_BUILD_DIR=build -P Zengine/tests/verify.cmake
```

To use Zengine from a project of your own, install it too and find it:

```sh
cmake --install Zengine/build --prefix "$PWD/deps"
```

```cmake
find_package(zengine 0.1 CONFIG REQUIRED)   # resolves Zengine's Loom dependency too
target_link_libraries(my-weave PRIVATE zengine::timer loom::switchboard)
```

The full walkthrough is [Using Zengine from another
project](docs/getting-started.md#using-zengine-from-another-project).

Windows, sanitizers, the no-SDL build and the sibling-source override are in
[docs/contributing/build-and-test.md](docs/contributing/build-and-test.md).

## Use it from C++

A weave declares what it accepts, what it may emit, and what state it owns:

```cpp
#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

struct CounterState {
    std::int64_t seen = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(CounterState, 1, ZEN_FIELD(seen));
};

struct Ping { ZEN_SHAPE(Ping, 1); };
struct Pong { std::int64_t nth = 0; ZEN_SHAPE(Pong, 1, ZEN_FIELD(nth)); };

class Counter : public loom::WeaveBase<Counter, CounterState,
                                       loom::Accept<Ping>, loom::Emit<Pong>> {
public:
    void on(const Ping&, loom::Mail& mail) { mail.publish(Pong{++state_.seen}); }
};

ZEN_EXPORT_WEAVE(Counter)
```

The full walkthrough — including a weave that uses the Timer, and the host program that
loads and runs it — is [docs/getting-started.md](docs/getting-started.md).

## Launch Workshop

```sh
./Zengine/build/workshop/zengine-workshop
```

It needs a terminal at least **78x22**. For the windowed build, pass the graphical plan:

```sh
./Zengine/build/workshop/zengine-workshop --load-plan workshop/graphical-load-plan.json
```

`Ctrl`+`p` opens the Pane Manager (open, close or make a pane), `Ctrl`+`t` the Terminal, `w`
arranges the desk, `q` quits. See [workshop/docs/getting-started.md](workshop/docs/getting-started.md).

## Where to go next

| I want to… | go to |
|---|---|
| write my first weave | [docs/getting-started.md](docs/getting-started.md) |
| look something up fast | [cheat_sheet.md](cheat_sheet.md) |
| read a package's exact contract | its folder's `README.md` ([the map](#the-map)), or [docs/README.md](docs/README.md) — the documentation index |
| use Workshop | [workshop/docs/getting-started.md](workshop/docs/getting-started.md) |
| know what does not work yet | [workshop/docs/limitations.md](workshop/docs/limitations.md) |
| build, test, or contribute | [docs/contributing/build-and-test.md](docs/contributing/build-and-test.md) |
| understand why it is shaped this way | [docs/architecture/README.md](docs/architecture/README.md) |

## The map

Every folder holds what it is about: a feature's pane, its tools and its pages sit in the feature's
folder, and each feature, the host and each library says what it is in its own `README.md`.

**Features** — what a weaver meets by name in Workshop:

| folder | what it is | exported as |
|---|---|---|
| [`attention/`](attention/README.md) | what needs your attention, at a glance and in a pane of its own | not exported |
| [`builder/`](builder/README.md) | the Builder: build a recipe, then load or reload what it made | not exported |
| [`composer/`](composer/README.md) | Compose: fill in a typed message and send it | not exported |
| [`editor/`](editor/README.md) | the Editor, standard or held by a Neovim, and the source material it carries | `zengine::neovim`, `zengine::source-transfer` |
| [`external-host/`](external-host/README.md) | Workshop driven from another Loom host, and the Connections pane | not exported |
| [`files/`](files/README.md) | Files: browse the project you launched in and open a file | not exported |
| [`flow/`](flow/README.md) | Flow: author a stateful weave as a graph, edit it live, generate native C++ | `zengine::flow` |
| [`info/`](info/README.md) | Info: independent views that inspect and edit a value | not exported |
| [`introspection/`](introspection/README.md) | Loaded, Project and Powers: what a running system is made of | not exported |
| [`inventory/`](inventory/README.md) | Inventory: stored typed values, portable toolboxes and hotkeys | `zengine::inventory` |
| [`terminal/`](terminal/README.md) | the Terminal: type a command to the weaves on the bus | not exported |
| [`view/`](view/README.md) | a view described as data, the view host, and the View Builder | not exported |

**The host** — [`workshop/`](workshop/README.md), the interactive environment for weavers, with
the Pane Manager, Hotkeys and the menus; its pane protocol is exported as `zengine::pane`.

**Libraries** — what a program links or a weave builds on by name:

| folder | what it owns | exported as |
|---|---|---|
| [`timer/`](timer/README.md) | the clock, the only sleep, one beat chain per activation | `zengine::timer` |
| [`input/`](input/README.md) | the sole producer of key, text and pointer moments | `zengine::input` |
| [`surface/`](surface/README.md) | drawing intent, and the skins that paint it | `zengine::surface` |
| [`ui/`](ui/README.md) | authored placement and extent, resolved against a viewport | `zengine::ui` |
| [`component/`](component/README.md) | reusable pieces of a tool that own their state and know no medium: text editing, list and table arithmetic, a control strip, motion sampling | `zengine::component` |
| [`activation/`](activation/README.md) | reading your own activation, once, without replay | `zengine::activation` |
| [`operator/`](operator/README.md) | typed reusable rules, supplied by artifacts | `zengine::operator`, `zengine::operator-consumer` |
| [`maker/`](maker/README.md) | a weave built from a weaver's definition — state, triggers and emits as data, edited live | `zengine::maker` |
| [`message-draft/`](message-draft/README.md) | typed value forms, unfinished drafts and named presets | `zengine::message-draft` |
| [`manual/`](manual/README.md) | a weave's manual, compiled into the weave and answered on ask by its pane or office | not exported |

**Examples** — [`examples/`](examples/README.md): worked examples, among them `snake/`, a game
whose parts are separate weaves.

**The repository's own** — `tests/` (every suite, fixture and check), `cmake/` (build helpers),
`tools/` (repository scripts), `docs/` ([the documentation index](docs/README.md), getting
started, contributing, architecture), `agents/` (the law automated collaborators work to) and
`quarry/` (the pre-Zen engine, kept as a quarry and not built).

An exported package's headers install under `include/zengine/` at the path they have here, so
`#include "timer/vocabulary.hpp"` reads the same from either side, and a part's vocabulary is
spelled with its feature: `flow/flow-host/vocabulary.hpp`, `flow/flow-pane/vocabulary.hpp`,
`inventory/inventory-pane/vocabulary.hpp`, `editor/neovim-editor/vocabulary.hpp` and
`editor/source-transfer/vocabulary.hpp`.

## Licence

Zengine is licensed under **MPL-2.0**. See [LICENSING.md](LICENSING.md) for the
plain-language boundary and [LICENSE](LICENSE) for the legal terms. Third-party components
and their licences are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

What you build with Zengine is yours. See [CONTRIBUTING.md](CONTRIBUTING.md).
