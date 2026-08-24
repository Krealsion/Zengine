# Getting started with Zengine

This page takes a C++ developer from an empty directory to a running program made of two
loadable weaves, one of which asks Zengine's Timer service to wake it up. Workshop is not
involved and is not needed.

Everything here was compiled and run before it was written down.

## What you need

- A C++20 compiler. GCC 11 or newer on Linux/WSL is the fully-supported configuration; see
  [supported toolchains](contributing/supported-toolchains.md) for Windows.
- CMake 3.16 or newer.
- An **installed** Loom, and a Zengine **source tree** plus its **build tree**. The next
  section explains why you need all three.

## Using Zengine from another project

> **Current limitation.** Zengine ships no `install()` rules, so there is no
> `find_package(zengine)` and no exported target set. An external project reaches Zengine's
> header-only vocabularies by pointing an include directory at a Zengine source tree, and
> gets its loadable artifacts (`zengine-timer.so`, the skins, the input weave) from a Zengine
> build tree. This is the honest current state; it is recorded as the first item in
> [the usability backlog](workshop/limitations.md#the-library-itself).

The Loom, by contrast, *is* a proper package:

```sh
git clone https://github.com/Krealsion/Loom
cmake -S Loom -B Loom/build -DCMAKE_BUILD_TYPE=Debug
cmake --build Loom/build -j
cmake --install Loom/build --prefix "$PWD/Loom/build/_install"
```

Then build Zengine against it, which is also what produces the artifacts you will load:

```sh
git clone https://github.com/Krealsion/Zengine
cmake -S Zengine -B Zengine/build -DCMAKE_PREFIX_PATH="$PWD/Loom/build/_install"
cmake --build Zengine/build -j
```

Your own project's `CMakeLists.txt` then needs three things:

```cmake
cmake_minimum_required(VERSION 3.16)
project(kitchen LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(loom 0.1 REQUIRED)          # 1. the substrate, as a real package

add_library(zengine-headers INTERFACE)   # 2. Zengine's header-only vocabularies
target_include_directories(zengine-headers INTERFACE ${ZENGINE_DIR})

add_library(oven SHARED oven.cpp)        # 3. your weave, as a loadable artifact
target_link_libraries(oven PRIVATE loom::core loom::switchboard zengine-headers)
loom_weave_build_contract(oven)          # <-- see below. Do not skip this.
set_target_properties(oven PROPERTIES PREFIX "")

add_executable(kitchen-host kitchen_host.cpp)
target_link_libraries(kitchen-host PRIVATE loom::core loom::switchboard loom::kernel
                                           zengine-headers)
```

Configure it with `-DZENGINE_DIR=<path to a Zengine source tree>`.

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
class Waiter
    : public loom::WeaveBase<Waiter, WaiterState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused, BakeDone>,
                             loom::Emit<loom::LoadWeave, BakeOrder>> {
public:
    bool answered = false;
    bool refused = false;
    bool served = false;
    std::string reason;

    void on(const loom::Result&, loom::Mail&) { answered = true; }
    void on(const loom::Ack&, loom::Mail&) { answered = true; }
    void on(const loom::Refused& r, loom::Mail&) {
        answered = true;
        refused = true;
        reason = r.reason;
    }
    void on(const BakeDone& d, loom::Mail&) {
        served = true;
        std::printf("  the oven served: %s\n", d.dish.c_str());
    }
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
        waiter->answered = false;
        waiter->refused = false;
        bus.send_as(waiter_id, manager,
                    loom::Message(loom::to_value(
                                      loom::LoadWeave{stem, dir + "/" + stem + ".so", role}),
                                  waiter_id, waiter_id));
        for (int turn = 0; turn < 64 && !waiter->answered; ++turn) {
            if (bus.pump_pending() == 0) {
                break;
            }
        }
        if (!waiter->answered) {
            std::printf("  %s: the Weave Manager never answered\n", stem.c_str());
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

    for (int turn = 0; turn < 4000 && !waiter->served; ++turn) {
        bus.pump_pending();
    }

    std::printf("kitchen: %s\n", waiter->served ? "the bake completed" : "NOTHING WAS SERVED");
    return waiter->served ? 0 : 1;
}
```

Three host facts worth having up front:

- **A grant is what a weave may *say*, and to whom — never what it may *touch*.** The waiter
  above may command lifecycle because the host wrote that down. An in-process weave shares the
  host's address space, so this is reviewability, not containment. `Kernel::containment_note()`
  says so out loud; read it literally.
- **`bus.pump()` drains to quiescence, and a live Timer beat chain never quiesces.** A host
  that wants to check anything between turns uses `bus.pump_pending()` — the bounded turn.
  Calling `pump()` with the Timer service loaded is a program that appears to hang.
- **Loading is answered, not merely started.** `Kernel::is_loaded` turns true the instant the
  load returns, while the `zen::Result` naming the new weave is still queued. Wait for the
  answer, as `load()` above does, so a refusal stops you instead of being missed.

## Run it

The Timer service is a Zengine artifact, so put it beside your own:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/../Loom/build/_install" \
                    -DZENGINE_DIR="$PWD/../Zengine"
cmake --build build -j
cp ../Zengine/build/timer/zengine-timer.so build/    # or from any staged copy
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
| the program hangs | you called `bus.pump()` with a live Timer chain. Use `pump_pending()` |
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
refused StartTimer -> '' : ...   (RefusalReason::SeamUnresolved)
```

`SeamUnresolved` means *a loaded weave emitted a shape nothing in this process has ever
declared*. Including `timer/vocabulary.hpp` in your host does not declare anything — only a
weave's `Accept`/`Emit` does. So the reason is precise: the Timer service is not in the
process, and therefore neither is its vocabulary. `loom::name_of(RefusalReason)` gives the
name and `Refusal::message()` gives the text.

## Where to go next

- [cheat_sheet.md](../cheat_sheet.md) — the same material, dense and searchable.
- [Ordering a timer](guides/timers.md) and [a weave with an authored rhythm](guides/timed-weaves.md).
- [Reading input](reference/input.md) and [drawing](reference/surface.md), if your program has
  a face.
- [snake](reference/snake.md) — a worked example whose parts are genuinely separate weaves.
- [Workshop](workshop/getting-started.md), if you want the interactive environment.
