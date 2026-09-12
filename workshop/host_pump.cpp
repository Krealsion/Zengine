// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The body of `host_pump.hpp`, compiled once into `zengine-workshop-logic` so the host and
// the suites serve the bus through one seam.

#include "host_pump.hpp"

#include <exception>
#include <utility>

namespace zengine::workshop {

namespace {

/// THE LAST FAILURE FACT THE BUS ANNOUNCED IN THIS TURN, read off the tap. Two kinds are
/// telling: a delivery refused `ApplicationFailed` (a showing failed, or a held weave was
/// reached -- the record says which) and a handler that did not complete. Whichever came
/// last is what an exception escaping the turn is about.
struct LastFact {
    enum class Kind : std::uint8_t { kNone, kApplicationFailed, kHandlerFailed };
    Kind kind = Kind::kNone;
    loom::WeaveId target{};
};

/// A tap for exactly one turn, removed on every exit path.
class TurnTap {
public:
    TurnTap(loom::Switchboard& bus, LastFact& last) : bus_(bus) {
        id_ = bus_.add_observer([&last](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::HandlerFailed) {
                last.kind = LastFact::Kind::kHandlerFailed;
                last.target = ev.target;
            } else if (ev.kind == loom::EventKind::Refused &&
                       ev.refusal.reason == loom::RefusalReason::ApplicationFailed) {
                last.kind = LastFact::Kind::kApplicationFailed;
                last.target = ev.target;
            }
        });
    }
    ~TurnTap() { bus_.remove_observer(id_); }
    TurnTap(const TurnTap&) = delete;
    TurnTap& operator=(const TurnTap&) = delete;

private:
    loom::Switchboard& bus_;
    loom::ObserverId id_ = 0;
};

std::string spelled(loom::WeaveId id) { return std::to_string(id.value); }

} // namespace

ServedTurn serve_until_idle(loom::Switchboard& bus,
                            const std::function<void(const std::string&)>& tell) {
    ServedTurn turn;
    LastFact last;
    std::exception_ptr escaped;
    {
        const TurnTap tap(bus, last);
        try {
            bus.drain_until_idle();
        } catch (...) {
            escaped = std::current_exception();
        }
    }
    if (!escaped) {
        return turn;
    }
    // ATTRIBUTED FROM LOOM'S FACTS, NEVER FROM THE EXCEPTION. The showing that failed refused
    // its delivery `ApplicationFailed` and recorded the participant Failed before the
    // exception was re-raised; the record must still say so now, or this is not that.
    if (last.kind != LastFact::Kind::kApplicationFailed || !last.target.valid() ||
        !bus.has_failed_application(last.target)) {
        std::rethrow_exception(escaped);
    }
    turn.outcome = ServedTurn::Outcome::kHeldParticipant;
    turn.held = last.target;
    turn.office = bus.role_of(last.target);
    std::string what;
    try {
        std::rethrow_exception(escaped);
    } catch (const std::exception& e) {
        what = e.what();
    } catch (...) {
        what = "(not a std::exception)";
    }
    turn.diagnostic = "a published claim could not be applied by " +
                      (turn.office.empty() ? "weave " + spelled(turn.held)
                                           : turn.office + " (weave " + spelled(turn.held) + ")") +
                      " -- it is held until it is reloaded or removed, and the desk says which "
                      "open; its own words: " +
                      what;
    if (tell) {
        tell(turn.diagnostic);
    }
    return turn;
}

} // namespace zengine::workshop
