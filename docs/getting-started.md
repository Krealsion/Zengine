# Getting started with Zengine

This page takes a C++ developer from an empty directory to a running program made of two
loadable weaves, one of which asks Zengine's Timer service to wake it up. Workshop is not
involved and is not needed.

Everything here was compiled and run before it was written down.

## What you need

- A C++20 compiler. GCC 11 or newer on Linux/WSL is the fully-supported configuration; see
  [supported toolchains](contributing/supported-toolchains.md) for Windows.
- CMake 3.16 or newer.
- An **installed** Loom and an **installed** Zengine. Nothing below needs either project's
  source tree or build tree once they are installed.

## Using Zengine from another project

Both projects install as ordinary CMake packages, so an external project finds them the same
way it finds anything else.

```sh
git clone https://github.com/Krealsion/Loom
cmake -S Loom -B Loom/build -DCMAKE_BUILD_TYPE=Debug
cmake --build Loom/build -j
cmake --install Loom/build --prefix "$PWD/deps"

git clone https://github.com/Krealsion/Zengine
cmake -S Zengine -B Zengine/build -DCMAKE_PREFIX_PATH="$PWD/deps"
cmake --build Zengine/build -j
cmake --install Zengine/build --prefix "$PWD/deps"
```

Your own project's `CMakeLists.txt` then says:

```cmake
cmake_minimum_required(VERSION 3.16)
project(kitchen LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(zengine 0.1 CONFIG REQUIRED)   # resolves Zengine's Loom dependency too

add_library(oven SHARED oven.cpp)           # your weave, as a loadable artifact
target_link_libraries(oven PRIVATE zengine::timer loom::switchboard)
loom_weave_build_contract(oven)             # <-- see below. Do not skip this.
set_target_properties(oven PROPERTIES PREFIX "")

add_executable(kitchen-host kitchen_host.cpp)
target_link_libraries(kitchen-host PRIVATE zengine::timer loom::switchboard loom::kernel)
```

Configure it with `-DCMAKE_PREFIX_PATH=<the prefix you installed into>`.

**You do not call `find_package(loom)`.** Zengine's package config resolves the Loom itself,
because the Loom is a *public* dependency of it: Zengine's headers include `<zen/...>` and its
exported targets name `loom::core`, `loom::switchboard` and — for a host — `loom::kernel`.

**Link the capability, not the library.** Each exported target grants one thing:

| target | what linking it grants |
|---|---|
| `zengine::timer` | order one-shots and repeats; declare an authored rhythm |
| `zengine::surface` | publish drawing intent; cells, regions, pointing, terminal size |
| `zengine::input` | receive key, text and pointer moments; translate a byte stream |
| `zengine::ui` | author placement and extent; read what a viewport resolved |
| `zengine::component` | a medium-independent editable text box |
| `zengine::activation` | read your own activation as a cursor |
| `zengine::operator` | hold and evaluate named typed rules; mount a provider |
| `zengine::operator-consumer` | spend a host's rules from inside a loaded artifact |

Every one of them carries its own include path and its own Loom dependencies, so linking one
is the whole of what you write.

**`loom_weave_build_contract()` is not optional and forgetting it is silent.** It applies
whatever the current platform needs for a loadable image's static lifetime to actually end
when the image is unloaded. Without it, on ELF with GCC, unload reports success and the image
stays resident — so the next load of a *different* library sharing the same vocabulary header
binds to the dead image's statics. The Loom exports the function so this is a mechanism rather
than a sentence in a guide.

## A shape

A shape is a struct that carries its own schema. It is how everything travels.

```cpp
// kitchen.hpp
#ifndef KITCHEN_HPP
#define KITCHEN_HPP

#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace kitchen {

/// "Put this in the oven for `minutes` minutes."
struct BakeOrder {
    std::string dish;
    std::int64_t minutes = 0;
    ZEN_SHAPE(BakeOrder, 1, ZEN_FIELD(dish), ZEN_FIELD(minutes));
};

/// "It is done."
struct BakeDone {
    std::string dish;
    ZEN_SHAPE(BakeDone, 1, ZEN_FIELD(dish));
};

} // namespace kitchen

#endif
```

`ZEN_SHAPE(Type, version, fields...)`. The **version** is part of the shape's wire identity:
`BakeOrder` v1 and v2 are different shapes to the admission gate, so bump it when the fields
change. Every message is checked against its schema before your handler is called.

## A weave

A weave is a class that says three things: what state it owns, what it accepts, and what it
may emit.

```cpp
// oven.cpp
#include "kitchen.hpp"

#include "timer/binding.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <string>

namespace {

namespace timer = zengine::timer;
using kitchen::BakeDone;
using kitchen::BakeOrder;

struct OvenState {
    std::string baking;      ///< the dish currently in the oven, or empty
    std::int64_t served = 0; ///< how many have come out
    ZEN_EXPOSE();
    ZEN_SHAPE(OvenState, 1, ZEN_FIELD(baking), ZEN_FIELD(served));
};

constexpr const char* kBakeTimer = "kitchen.oven.bake";

class Oven : public timer::TimedWeave<Oven, OvenState, loom::Accept<BakeOrder>,
                                      loom::Emit<BakeDone>> {
public:
    using TimedWeave::on; // required: keeps the layer's own handlers reachable

    /// An order arrives. Place a one-shot for the ordered delay.
    void on(const BakeOrder& order, loom::Mail& mail) {
        state_.baking = order.dish;
        mail.send_to_role(timer::kTimerRole,
                          timer::StartTimer{kBakeTimer, order.minutes, /*repeat=*/false});
    }

    /// The oven's own timer fired. Serve it.
    void on(const timer::TimerFired& fired, loom::Mail& mail) {
        if (fired.id != kBakeTimer || state_.baking.empty()) {
            return;
        }
        ++state_.served;
        mail.publish(BakeDone{state_.baking});
        state_.baking.clear();
    }
};

} // namespace

ZEN_EXPORT_WEAVE(Oven)
```

Five things to notice, because each is a rule rather than a style:

- **`Accept<>` and `Emit<>` are the manifest.** They are not documentation — Loom refuses a
  shape you did not declare, in either direction, and a loaded weave's undeclared emission is
  rejected at the artifact boundary.
- **`state_` is the only state.** `WeaveBase` gives it to you; it is a shape, so it can be
  inspected and serialized. A weave with state somewhere else is a weave that cannot be
  replaced.
- **`send_to_role` addresses a slot, not an instance.** The oven never learns which weave is
  the Timer. That is what lets the Timer be replaced while the oven keeps working.
- **`using TimedWeave::on;` is required** as soon as you declare any `on` of your own.
  `WeaveBase` dispatches through the *derived* type, and a derived `on` hides every
  base-class one. Forgetting it is a compile error that names the fix.
- **The delay is runtime data, so this uses the raw Timer protocol.** See the next section.

### `TimedWeave` and the raw protocol — which one

`TimedWeave` covers *authored rhythms*: cadences that are part of what a weave is.

```cpp
class Pulse : public timer::TimedWeave<Pulse, PulseState, loom::Accept<>, loom::Emit<>> {
public:
    Pulse() : tick_(timers().repeat("pulse.tick", std::chrono::milliseconds(50),
                                    &Pulse::on_tick)) {}
    using TimedWeave::on;
    void on_tick(const timer::TimerFired&, loom::Mail&) { ++state_.beats; }
private:
    Handle tick_;
};
```

Declaring a binding sends nothing — it records desire. The layer re-places every declared
order at the two moments that matter: your activation, and the Timer service announcing
itself (including after it has been replaced). That is the ceremony you are not writing.

A binding's **delay is fixed at declaration**, and there is no way to add one at run time and
have it take effect before the next `TimerReady`. So when the delay is *data* — it arrived in
a message, a person typed it, a job carries its own cadence — speak the raw protocol as the
oven does: declare `timer::TimerFired` in your accept set and `timer::StartTimer` in your emit
set, send, and filter firings by the id you chose. That split is deliberate
([TIMER-05](laws/timer-laws.md)); it is also the first thing most authors meet.

For domain work that belongs to your first breath, implement `on_timed_activation` — it runs
*after* your bindings are reconciled, and only for an activation the layer accepted:

```cpp
void on_timed_activation(const loom::Activated&, loom::Mail& mail) { mail.publish(ImAlive{}); }
```

Declaring a raw `on(const loom::Activated&, ...)` on a `TimedWeave` is refused by name: it
would replace the layer's reconciliation instead of extending it, and every declared timer
would silently never be ordered.

## A host

Something has to own the bus, the loader and the loop. That is a host, and it is an ordinary
program.

```cpp
// kitchen_host.cpp
#include "kitchen.hpp"

#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace {

namespace timer = zengine::timer;
using kitchen::BakeDone;
using kitchen::BakeOrder;

struct WaiterState {
    ZEN_EXPOSE();
    ZEN_SHAPE(WaiterState, 1);
};

/// The host's own weave: it commands the loads, sends the order, hears the result.
/// It keeps one fact about each load -- which conversation it is waiting on -- because
/// the three answer shapes are a vocabulary every participant shares. Loom ships that
/// record as `loom::AskBook`, so this program does not write one.
class Waiter
    : public loom::WeaveBase<Waiter, WaiterState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused, BakeDone>,
                             loom::Emit<loom::LoadWeave, BakeOrder>> {
public:
    bool answered = false;
    bool refused = false;
    bool served = false;
    std::string reason;

    /// Open a load conversation with `manager`, and return the correlation to send.
    std::uint64_t asking(loom::WeaveId manager) {
        answered = false;
        refused = false;
        return loads_.open(manager, loom::LoadWeave::zen_name, loom::LoadWeave::zen_version)
            .correlation;
    }
    /// Is MY load conversation still open? Never "was anything delivered this turn".
    bool awaiting() const { return loads_.awaiting(); }

    void on(const loom::Result&, loom::Mail& mail) { answered |= mine(mail); }
    void on(const loom::Ack&, loom::Mail& mail) { answered |= mine(mail); }
    void on(const loom::Refused& r, loom::Mail& mail) {
        if (!mine(mail)) {
            return;
        }
        answered = true;
        refused = true;
        reason = r.reason;
    }
    void on(const BakeDone& d, loom::Mail&) {
        served = true;
        std::printf("  the oven served: %s\n", d.dish.c_str());
    }

private:
    /// Which of my conversations does this arrival settle -- by correlation (which
    /// conversation) and by bus-stamped sender (the weave actually asked)? Settling
    /// closes it, so a duplicate of the same answer is inert.
    bool mine(const loom::Mail& mail) {
        return loads_.settle(mail.correlation(), mail.sender()).has_value();
    }

    /// This program never has two open at once -- it asks, then waits -- but the bound
    /// is the owner's to state and `loom::AskBook` has no default.
    loom::AskBook loads_{2};
};

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";

    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

    // The waiter's grant: it may command lifecycle, and it may order a bake.
    loom::Grant grant;
    grant.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
    grant.allow_to_any(BakeOrder::zen_name, BakeOrder::zen_version);
    auto owned = std::make_unique<Waiter>();
    Waiter* waiter = owned.get();
    const loom::WeaveId waiter_id = bus.register_weave(std::move(owned), std::move(grant));
    waiter->zen_set_self(waiter_id);

    const auto load = [&](const std::string& stem, const std::string& role) {
        const std::uint64_t correlation = waiter->asking(manager);
        bus.send_as(waiter_id, manager,
                    loom::Message(loom::to_value(
                                      loom::LoadWeave{stem, dir + "/" + stem + ".so", role}),
                                  waiter_id, waiter_id, correlation));
        // Turn the crank until THIS conversation's answer arrives. The 64 is only a
        // hang guard for this example -- see the note below the listing.
        for (int turn = 0; turn < 64 && waiter->awaiting(); ++turn) {
            bus.pump_pending();
        }
        if (!waiter->answered) {
            std::printf("  %s: the Weave Manager has not answered yet\n", stem.c_str());
            return false;
        }
        if (waiter->refused) {
            std::printf("  %s: refused -- %s\n", stem.c_str(), waiter->reason.c_str());
            return false;
        }
        std::printf("  loaded %s as %s\n", stem.c_str(), role.c_str());
        return true;
    };

    std::printf("kitchen: %s\n", kernel.containment_note());
    std::fflush(stdout);
    if (!load("zengine-timer", timer::kTimerRole)) {
        return 1;
    }
    if (!load("oven", "kitchen.oven")) {
        return 1;
    }

    bus.publish_as(waiter_id, loom::Message(loom::to_value(BakeOrder{"sourdough", 40}),
                                            waiter_id, loom::WeaveId{0}));

    // The host loop. `pump_pending()` dispatches what was waiting and hands control
    // back, so this condition is checked every turn while the Timer service keeps
    // beating. The 4000 is this program's patience, not a dispatch budget.
    for (int turn = 0; turn < 4000 && !waiter->served; ++turn) {
        bus.pump_pending();
    }

    std::printf("kitchen: %s\n", waiter->served ? "the bake completed" : "NOTHING WAS SERVED");
    return waiter->served ? 0 : 1;
}
```

Four host facts worth having up front:

- **A grant is what a weave may *say*, and to whom — never what it may *touch*.** The waiter
  above may command lifecycle because the host wrote that down. An in-process weave shares the
  host's address space, so this is reviewability, not containment. `Kernel::containment_note()`
  says so out loud; read it literally.
- **`bus.pump_pending()` is the host loop's call.** It dispatches exactly the work that was
  waiting and gives control back, so the loop above can check `waiter->served` every turn while
  the Timer service keeps beating. Its counterpart `bus.drain_until_idle()` keeps going until
  the bus is idle, which is right for settling a world or for a host whose whole program *is*
  the bus — and with the Timer service loaded that bus is never idle, exactly as the name
  says.
- **Loading is answered, not merely started — and answered *to you*.** `Kernel::is_loaded`
  turns true the instant the load returns, while the `zen.Result` naming the new weave is
  still queued, so `load()` above waits for the answer and a refusal stops you instead of
  being missed. Which answer, though, is a second question. `zen.Result`, `zen.Ack` and
  `zen.Refused` are a **shared vocabulary**: any participant your host grants them may send
  one to any weave that accepts them, so "an answer-shaped message arrived" is not "my load
  answered". Loom states the obligation as a standing rule (`zen/weave/standard_shapes.hpp`):
  match each arrival against your own outstanding request **by correlation** — which
  conversation, the number you put on the request and the Manager echoes back — **and by
  bus-stamped sender** — the weave you actually asked, which the bus stamps so nobody can
  claim another's identity. A correlation is *guessable* — the first one any asker opens is
  `1` — so it identifies and never authenticates; the pair is what makes the wall.
- **You do not write that record yourself.** `loom::AskBook` is it: `open` gives you the
  correlation to send and remembers who may answer it, `settle` says which of *your*
  conversations an arrival closed, `awaiting()` is your loop condition, and `forget` stops
  tracking one locally without claiming anything was cancelled at the far end. It knows
  nothing about `zen.Result` or what "refused" means — that part stays yours, which is why
  `answered` / `refused` / `reason` are still fields of `Waiter`. See
  [the asker's own book](https://github.com/Krealsion/Loom/blob/main/docs/reference/messaging.md#the-askers-own-book).
- **The turn ceiling in `load()` is a hang guard, not the thing that settles the load.** The
  conversation settles when its own correlated answer arrives; the 64 only stops this example
  from spinning forever if no answer ever comes, and reaching it would mean exactly that and
  nothing more — not that the Manager refused, and not that an answer became impossible.
  **Do not stop the loop when a turn dispatches nothing.** `bus.pending()` is the size of the
  queue at one instant; it says nothing about what may be queued next, and a respondent that
  defers its answer holds it off the queue entirely. An empty turn is not a settlement.

## Run it

The Timer service is a Zengine artifact, and the package says where its own artifacts are:
`ZENGINE_ARTIFACT_DIR` is the directory and `ZENGINE_RUNTIME_ARTIFACTS` lists the stems in it.
Copy the one you need beside your host as part of the build, so running is one command:

```cmake
add_custom_command(TARGET kitchen-host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ZENGINE_ARTIFACT_DIR}/zengine-timer${CMAKE_SHARED_LIBRARY_SUFFIX}"
            "$<TARGET_FILE_DIR:kitchen-host>")
```

**Artifact, weave, provider — three words, and they are not synonyms.** An *artifact* is the
physical loadable file. A *weave* and a *provider* are runtime surfaces an artifact may expose:
a weave is a participant the Kernel loads onto the bus and addresses by role; a provider is
opened directly by a host to contribute operator definitions, and has no participant in it at
all. The package names the physical things, because one list holds both kinds:

| artifact | what it exposes |
|---|---|
| `zengine-timer` | weave — the Timer service you loaded above |
| `zengine-input` | weave — the sole producer of key, text and pointer moments |
| `zengine-skin-tui-classic`, `zengine-skin-tui-block` | weave — terminal skins that paint drawing intent |
| `zengine-operators-basic` | **provider** — typed operator definitions, mounted by a host, never loaded onto the bus |

An artifact could expose both surfaces, or a surface that does not exist yet; `zengine::operator`
and [operator providers](reference/operator-providers.md) are where the provider side is
written down.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/deps"
cmake --build build -j
./build/kitchen-host build
```

```text
kitchen: in-process; trusted; no OS sandbox (out-of-process isolation is the isolation host's job)
  loaded zengine-timer as zengine.timer
  loaded oven as kitchen.oven
  the oven served: sourdough
kitchen: the bake completed
```

## When it does not work

| what you see | what it means |
|---|---|
| `open failed: ./oven.so: cannot open shared object file` | the path in `LoadWeave` is wrong, or the artifact was not built |
| `library create() returned null` | your weave's constructor threw. A duplicate binding id, an empty id, or an empty role on a `*_to_role` binding all do this deliberately |
| it loads and **nothing happens** | most often: the service you are sending to is not loaded. See below |
| the program hangs | you asked `bus.drain_until_idle()` of a process with the Timer service loaded — that bus is never idle. A host loop wants `bus.pump_pending()` |
| walls of template errors from `WeaveBase` | you declared an `on` and forgot `using TimedWeave::on;` — look for the `static_assert` sentence |

**"It loads and nothing happens" deserves its own paragraph**, because it is the failure a
stranger hits first and the one the substrate is quietest about. A sender cannot observe its
own send's fate; the refusal is real, and it is visible to the *host*. Install an observer:

```cpp
bus.add_observer([](const loom::BusEvent& e) {
    if (e.kind == loom::EventKind::Refused) {
        std::fprintf(stderr, "refused %s -> '%s' : %s\n", e.schema_name.c_str(),
                     e.addressed_role.c_str(), e.refusal.message().c_str());
    }
});
```

Run the oven with no Timer service loaded and that prints:

```text
refused StartTimer -> 'zengine.timer' : the shape claimed across the library seam is not
registered in this Loom; nothing was queued
```

Read it as two facts. `SeamUnresolved` means *a loaded weave reached out with a shape nothing
in this process has ever declared* — and a shape is declared by being in some weave's
`Accept<...>`, not by `#include`ing the header that defines it and not by an `Emit<...>`. So
the Timer service is not in the process, and therefore neither is its vocabulary. The role is
the address the oven named: `zengine.timer`, the office it was reaching for. Together they are
the whole diagnosis — what was said, and where it was going.

`loom::name_of(RefusalReason)` gives the reason's name and `Refusal::message()` gives the text.

**A successful run prints none of this.** That is worth saying because it briefly did not: a
publication that reaches nobody used to be reported here as a refusal, so the Timer's ordinary
startup announcement — spoken before any consumer is loaded, and heard by nobody, exactly as
intended — looked like a failure in a program that was working perfectly. It no longer does. A
publication has no addressee, so being unheard is not a failure; a *send* to a role nobody
holds still is, which is the line above.

## Where to go next

- [cheat_sheet.md](../cheat_sheet.md) — the same material, dense and searchable.
- [Ordering a timer](guides/timers.md) and [a weave with an authored rhythm](guides/timed-weaves.md).
- [Reading input](reference/input.md) and [drawing](reference/surface.md), if your program has
  a face.
- [snake](reference/snake.md) — a worked example whose parts are genuinely separate weaves.
- [Workshop](workshop/getting-started.md), if you want the interactive environment.
