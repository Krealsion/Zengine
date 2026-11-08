// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The host's turn of the bus (`host_pump.hpp`): one seam for the host and the suites.

#include "host_pump.hpp"

#include <utility>

namespace zengine::workshop {

namespace {

std::string spelled(loom::WeaveId id) { return std::to_string(id.value); }

/// TELL WHAT THE BOUNDARIES WROTE, each failure against Loom's record as it stands now: the
/// owner is named by the role it holds, and "held" is said only while the record says so --
/// an owner reloaded or removed later in the same turn is not held any more.
ServedTurn tell_failures(loom::Switchboard& bus, ShowingFailures& book,
                         const std::function<void(const std::string&)>& tell) {
    ServedTurn turn;
    for (ShowingFailure& failure : book.take()) {
        ServedTurn::Failed one;
        one.owner = failure.owner;
        one.role = bus.role_of(failure.owner);
        one.held = bus.has_failed_application(failure.owner);
        one.diagnostic =
            "a published claim could not be applied by " +
            (one.role.empty() ? "weave " + spelled(one.owner)
                                : one.role + " (weave " + spelled(one.owner) + ")") +
            (one.held ? " -- it is held until it is reloaded or removed"
                      : " -- it is not held now") +
            "; its own words: " + failure.words;
        if (tell) {
            tell(one.diagnostic);
        }
        turn.failed.push_back(std::move(one));
    }
    return turn;
}

/// Run one turn of the bus and tell what the boundaries wrote during it.
template <class Turn>
ServedTurn served(loom::Switchboard& bus, ShowingFailures& book,
                  const std::function<void(const std::string&)>& tell, Turn turn) {
    try {
        turn();
    } catch (...) {
        // NOT CAUGHT TO BE EXPLAINED. Nothing a boundary captured leaves the turn as an
        // exception, so this one is a handler's, an observer's or an owner's thrown outside
        // any boundary, and it is described by nobody here. What the boundaries DID capture
        // during the turn is told first -- a host that ends on this exception has still said
        // which owner failed and in what words -- and then it leaves exactly as it came.
        (void)tell_failures(bus, book, tell);
        throw;
    }
    return tell_failures(bus, book, tell);
}

} // namespace

// WL-OPEN-09 -- agents/workshop/opening.md
ServedTurn serve_until_idle(loom::Switchboard& bus, ShowingFailures& book,
                            const std::function<void(const std::string&)>& tell) {
    return served(bus, book, tell, [&bus] { bus.drain_until_idle(); });
}

// WL-OPEN-09 -- agents/workshop/opening.md
ServedTurn serve_turn(loom::Switchboard& bus, ShowingFailures& book,
                      const std::function<void(const std::string&)>& tell) {
    return served(bus, book, tell, [&bus] { (void)bus.pump_pending(); });
}

} // namespace zengine::workshop
