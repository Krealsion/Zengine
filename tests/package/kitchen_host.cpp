// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
//
// The stranger's host: it owns the bus, the loader and the loop. An ordinary program.
//
// It loads TWO artifacts by path from one directory: the Timer service, which arrived from
// the installed Zengine package, and the oven, which this project built. Neither path is
// written here -- the directory is argv[1], and the build rule that put both files in it
// names ZENGINE_ARTIFACT_DIR, never a Zengine build tree.
//
// The refusal observer is not decoration. A sender cannot see its own send's fate, so a
// message addressed to a role nobody holds simply does not arrive; the host is the only party
// that can see why, and installing this observer is how "it loaded and nothing happened"
// becomes a sentence instead of a silence.
//
// IT RUNS TWO WAYS, AND NEITHER ARM MEANS ANYTHING WITHOUT THE OTHER (FRIC-0).
//
//   kitchen-host <dir>              the ordinary success. It bakes, AND it fails if the
//                                   observer saw anything at all -- a working program that
//                                   prints a refusal is the defect this arm exists for.
//   kitchen-host <dir> --no-timer   the nearby genuine failure. The oven is loaded and the
//                                   Timer service is not, so the order really does go
//                                   nowhere. This arm passes only when the observer NAMED
//                                   it: the shape, and the role it was addressed to.
//
// Read alone the first arm would say no more than "no output is good output", and a
// diagnostic system that had simply stopped working would satisfy it. The second is what
// makes the pair a claim about semantics rather than about volume.

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

#if defined(_WIN32)
constexpr const char* kArtifactSuffix = ".dll";
#else
constexpr const char* kArtifactSuffix = ".so";
#endif

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    bool with_timer = true;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--no-timer") {
            with_timer = false;
        }
    }

    loom::Switchboard bus;
    loom::Kernel kernel(bus);
    // WHAT THE OBSERVER SAW, kept so the program can answer for it rather than leaving a
    // human to read stderr. Nothing here interprets: it counts refusals and remembers the
    // last one's shape and address exactly as Loom reported them.
    int refusals = 0;
    std::string refused_shape;
    std::string refused_role;
    bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused) {
            ++refusals;
            refused_shape = e.schema_name;
            refused_role = e.addressed_role;
            std::fprintf(stderr, "  refused %s -> '%s' : %s\n", e.schema_name.c_str(),
                         e.addressed_role.c_str(), e.refusal.message().c_str());
        }
    });

    const loom::WeaveId control = loom::mount_control(kernel, bus);
    const loom::WeaveId manager = loom::mount_manager(control, bus);

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
                    loom::Message(loom::to_value(loom::LoadWeave{
                                      stem, dir + "/" + stem + kArtifactSuffix, role}),
                                  waiter_id, waiter_id));
        // A load is ANSWERED, not merely started: is_loaded turns true while the Result
        // naming the new weave is still queued, so keep taking turns until the answer or a
        // refusal is missed.
        for (int turn = 0; turn < 64 && !waiter->answered; ++turn) {
            if (bus.pump_pending() == 0) {
                break; // nothing left to dispatch: no answer is coming
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
    if (with_timer && !load("zengine-timer", timer::kTimerRole)) {
        return 1;
    }
    if (!load("oven", "kitchen.oven")) {
        return 1;
    }

    bus.publish_as(waiter_id, loom::Message(loom::to_value(BakeOrder{"sourdough", 40}),
                                            waiter_id, loom::WeaveId{0}));

    // THE HOST LOOP, AND THE POINT OF IT (FRIC-1). With the Timer service loaded this bus
    // is never idle -- the service seeds its one successor beat inside every beat's handler
    // -- so a host that asked for `drain_until_idle()` here would never reach the next line
    // and this program would print nothing at all. `pump_pending()` dispatches exactly what
    // was waiting and hands control back, which is what lets the condition below be checked
    // at all. The 4000 is this fixture's patience, not a dispatch budget: nothing in Loom
    // takes a number.
    int laps = 0;
    for (; laps < 4000 && !waiter->served; ++laps) {
        bus.pump_pending();
    }

    if (!with_timer) {
        // THE GENUINE-FAILURE ARM. The order was placed and there is no Timer service to
        // hear it, so nothing can be served -- and the only question worth asking is
        // whether the program said WHY. `StartTimer` cannot resolve, because the artifact
        // that defines the Timer vocabulary is the one that was not loaded, so the seam
        // refuses the emission and names the office the oven addressed.
        if (waiter->served) {
            std::printf("kitchen: SERVED WITHOUT A TIMER\n");
            return 1;
        }
        std::printf("kitchen: nothing was served\n");
        if (refusals == 0) {
            std::printf("kitchen: AND NOTHING SAID WHY -- the failure was silent\n");
            return 1;
        }
        if (refused_shape != "StartTimer" || refused_role != timer::kTimerRole) {
            std::printf("kitchen: the diagnostic did not say where it was going: "
                        "%s -> %s\n", refused_shape.c_str(), refused_role.c_str());
            return 1;
        }
        std::printf("kitchen: the failure was reported, and it named its destination\n");
        return 0;
    }

    std::printf("kitchen: %s\n", waiter->served ? "the bake completed" : "NOTHING WAS SERVED");
    if (!waiter->served) {
        return 1;
    }
    // THE RECURRING-WORK WITNESS. Two facts, and neither is worth anything without the
    // other: the host got control back many times over, AND the service that made that
    // interesting is still going. A run where the chain had died would satisfy the first
    // alone, and would prove only that a quiet bus can be pumped.
    std::printf("kitchen: the host loop kept control for %d turn(s)\n", laps);
    if (laps < 2) {
        std::printf("kitchen: THE HOST LOOP NEVER REALLY LOOPED\n");
        return 1;
    }
    if (bus.pending() == 0) {
        std::printf("kitchen: THE BUS WENT IDLE -- the Timer chain is not alive, so this run "
                    "proves nothing about servicing a live one\n");
        return 1;
    }
    std::printf("kitchen: the bus is still busy (%zu queued) -- quiescence was never coming, "
                "and the host never asked for it\n", bus.pending());
    // AN ORDINARY SUCCESS LOOKS LIKE ONE. Not a preference about volume: every refusal
    // this run could produce would be a real one, so a single line here means either the
    // program is broken or the runtime is presenting a non-event as a failure. It was the
    // second, and this is the arm that keeps it from coming back.
    if (refusals != 0) {
        std::printf("kitchen: THE BAKE SUCCEEDED AND THE RUN REPORTED %d REFUSAL(S)\n", refusals);
        return 1;
    }
    return 0;
}
