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
| **Workshop** | an interactive maker environment *built with* Zengine. Optional. | [docs/workshop/getting-started.md](docs/workshop/getting-started.md) |

A developer looking for the library never has to learn Workshop. A maker who wants
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
  exports eight capability targets and installs the loadable artifacts they need. Workshop
  itself, the SDL-backed skin and input reader, and the external-pane vocabularies are
  deliberately not in it — see [Using Zengine from another
  project](docs/getting-started.md#using-zengine-from-another-project).
- **Linux/WSL with GCC is the only fully-supported configuration.** Windows builds a
  documented subset; the Loom's OS sandbox is Linux-only. See [supported
  toolchains](docs/contributing/supported-toolchains.md).
- **Workshop has real gaps** — no workspace restore at launch, no text editor, one
  hard-coded build target, no run-time unload or reload. Each is written down in
  [Workshop limitations](docs/workshop/limitations.md) rather than left to be discovered.

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

`p` opens the pane picker, `w` manages pane geometry, `q` quits. See
[docs/workshop/getting-started.md](docs/workshop/getting-started.md).

## Where to go next

| I want to… | go to |
|---|---|
| write my first weave | [docs/getting-started.md](docs/getting-started.md) |
| look something up fast | [cheat_sheet.md](cheat_sheet.md) |
| read a package's exact contract | [docs/README.md](docs/README.md) — the documentation index |
| use Workshop | [docs/workshop/getting-started.md](docs/workshop/getting-started.md) |
| know what does not work yet | [docs/workshop/limitations.md](docs/workshop/limitations.md) |
| build, test, or contribute | [docs/contributing/build-and-test.md](docs/contributing/build-and-test.md) |
| understand why it is shaped this way | [docs/architecture/README.md](docs/architecture/README.md) |

## The packages

Each is independently linkable; most are header-only vocabularies plus one loadable weave.

| package | what it owns | exported as | reference |
|---|---|---|---|
| `timer/` | the clock, the only sleep, one beat chain per activation | `zengine::timer` | [timers](docs/guides/timers.md) · [protocol](docs/reference/timer-protocol.md) |
| `input/` | the sole producer of key, text and pointer moments | `zengine::input` | [input](docs/reference/input.md) |
| `surface/` | drawing intent, and the skins that paint it | `zengine::surface` | [surface](docs/reference/surface.md) |
| `ui/` | authored placement and extent, resolved against a viewport | `zengine::ui` | [ui](docs/reference/ui.md) |
| `component/` | reusable pieces of a tool — currently one: `TextBox` | `zengine::component` | [component](docs/reference/component.md) |
| `activation/` | reading your own activation, once, without replay | `zengine::activation` | [timed weaves](docs/guides/timed-weaves.md) |
| `operator/` | typed reusable rules, supplied by artifacts | `zengine::operator` | [operators](docs/reference/operator-providers.md) |
| `builder/` | starting an OS process from a named recipe | not exported | [builder](docs/reference/builder.md) |
| `introspection/` | panes that show what a running system is made of | not exported | [introspection](docs/reference/introspection.md) |
| `workshop/` | the maker environment | not exported | [Workshop docs](docs/workshop/getting-started.md) |
| `snake/` | a worked example: a game whose parts are separate weaves | not exported | [snake](docs/reference/snake.md) |

## Licence

Zengine is licensed under **MPL-2.0**. See [LICENSING.md](LICENSING.md) for the
plain-language boundary and [LICENSE](LICENSE) for the legal terms. Third-party components
and their licences are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

What you build with Zengine is yours. See [CONTRIBUTING.md](CONTRIBUTING.md).
