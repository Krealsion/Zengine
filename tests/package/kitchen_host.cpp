// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The stranger's host: an ordinary program owning the bus, the loader and the loop. It loads two
// artifacts by path from the directory argv[1] names: the Timer service, from the installed
// package, and the oven this project built. It installs a refusal observer, since a sender cannot
// see its send's fate: `kitchen-host <dir>` must bake and print no refusal, and
// `kitchen-host <dir> --no-timer` must name the refused order's shape and office. Either arm
// alone speaks only to volume; together they speak to meaning.

#include "kitchen.hpp"

#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/admission.hpp>
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

/// The host's own weave: it commands the loads, sends the order, hears the result. It matches each
/// `zen.Result`, `zen.Ack` or `zen.Refused` against its own outstanding load by correlation and by
/// bus-stamped sender (the standing rule of `zen/weave/standard_shapes.hpp`), through Loom's
/// `loom::AskBook`; what stays here is about this program: whether a load succeeded, why one was
/// refused, and whether the bake came out of the oven.
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
        const loom::AskOpened opened =
            loads_.open(manager, loom::LoadWeave::zen_name, loom::LoadWeave::zen_version);
        current_ = opened.id;
        return opened.correlation;
    }
    /// IS MY LOAD CONVERSATION STILL OPEN? Never "was anything delivered this turn".
    bool awaiting() const { return loads_.awaiting(); }

    /// I have stopped waiting, so I stop tracking, locally: Loom has no cancellation vocabulary,
    /// the Manager was never asked to stop, and a late answer matches no record here. What this
    /// must not leave is a conversation open on the books that nothing will look at again.
    void stopped_waiting() { (void)loads_.forget(current_); current_ = 0; }

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
    /// Which of my conversations does this arrival settle? Settling CLOSES it, so a
    /// duplicate or a late copy of the same answer is inert -- there is no longer a
    /// record for it to close.
    bool mine(const loom::Mail& mail) {
        if (!loads_.settle(mail.correlation(), mail.sender())) {
            return false;
        }
        current_ = 0; // closed by its answer: there is nothing left to stop waiting for
        return true;
    }

    /// TWO OPEN AT ONCE IS MORE THAN THIS PROGRAM EVER HAS -- it asks, then waits --
    /// but the bound is the owner's to state, and a book that could grow without limit
    /// is not a record, it is a leak.
    loom::AskBook loads_{2};
    /// WHICH conversation this program is waiting on, so it can stop tracking exactly
    /// that one. `settle` finds an arrival's own record by correlation; giving up needs
    /// the local handle instead, because no arrival is naming it.
    std::uint64_t current_ = 0;
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
    loom::Kernel kernel(
        bus, loom::trust_every_artifact("this witness loads only what it just built"));
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
        const std::uint64_t correlation = waiter->asking(manager);
        bus.send_as(waiter_id, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{
                                      stem, dir + "/" + stem + kArtifactSuffix, role}),
                                  waiter_id, waiter_id, correlation));
        // A load is answered, not merely started: is_loaded turns true while the Result naming
        // the weave is still queued, so this turns the dispatch crank until this waiter's own
        // answer arrives. The 64 is a hang guard, not what settles the load; do not stop on an
        // empty turn, since `pending()` is one instant's queue and a respondent may hold an answer.
        for (int turn = 0; turn < 64 && waiter->awaiting(); ++turn) {
            bus.pump_pending();
        }
        if (!waiter->answered) {
            // THIS PROGRAM HAS STOPPED WAITING, AND SO IT STOPS TRACKING. There is no
            // continuation here -- `load` returns false and the caller exits -- so
            // keeping the conversation on the books would record an interest nothing
            // has. Local only: nothing was sent, nothing was cancelled, and the
            // Manager's answer right is exactly what it was a moment ago.
            waiter->stopped_waiting();
            std::printf("  %s: no answer arrived before this program's local guard "
                        "expired; it stopped waiting and no longer tracks that "
                        "conversation (nothing was cancelled)\n",
                        stem.c_str());
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

    // The host loop: with the Timer service loaded the bus is never idle (the service seeds its
    // next beat inside every beat's handler), so `drain_until_idle()` here would never return.
    // `pump_pending()` dispatches what was waiting and hands control back, so the condition below
    // is checked at all. The 4000 is this fixture's patience, not a dispatch budget.
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
