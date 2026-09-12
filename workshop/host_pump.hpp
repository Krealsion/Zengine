// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_HOST_PUMP_HPP
#define ZENGINE_WORKSHOP_HOST_PUMP_HPP

// THE HOST'S TURN OF THE BUS, AND THE ONE SEAM LOOM HANDS THE HOST AT IT.
//
// A joint publication is shown to each of its owners before that owner runs again. A NATIVE
// owner whose showing throws is recorded by Loom first -- the participant is held, the
// delivery that met it is refused `ApplicationFailed` in the journal and on the tap, the
// operator is told once -- and the owner's own exception is then re-raised to the host,
// exactly as a handler's would be (Loom MSG-10: recorded, then never swallowed; the
// joint-publication reference, "Host exception responsibility at the pump seam"). The
// exception leaves `drain_until_idle()` at that delivery; the bus is not poisoned, the record
// already protects the held weave, and the next turn delivers. WHAT HAPPENS NEXT IS THE
// HOST'S, and this is what this host does: it reads Loom's facts for the turn -- the tap, not
// the exception -- and when the last thing the bus said was that a participant's showing
// failed, it names that participant where a maker and a repair tool can read it and serves
// on. The opening manager's own notice then settles the open in words, the desk says which
// owner is held, and the repair is the owner's reload or removal, as it is for a loaded owner.
//
// ⚠ AN EXCEPTION LOOM'S RECORD DOES NOT EXPLAIN IS NOT SWALLOWED. A handler that threw is
// announced as `HandlerFailed` before it is rethrown, and a participant held since an earlier
// turn is not a reason for anything: only a showing failure recorded IN THIS TURN, as the last
// fact the bus announced before the exception, is attributed. Everything else propagates out
// of `serve_until_idle` unexamined and untranslated, which is the host policy it always had.

#include <zen/switchboard/switchboard.hpp>

#include <functional>
#include <string>

namespace zengine::workshop {

/// WHAT ONE TURN OF THE HOST LOOP CAME TO.
struct ServedTurn {
    enum class Outcome : std::uint8_t {
        kIdle,           ///< the bus went idle on its own, or `stop()` ended the turn
        kHeldParticipant ///< a participant's showing failed, natively; it is held and named
    };
    Outcome outcome = Outcome::kIdle;
    loom::WeaveId held{};  ///< the participant Loom's record says is held
    std::string office;    ///< the office it holds, if any
    std::string diagnostic; ///< the sentence written for the maker and the journal
};

/// SERVE THE BUS UNTIL IT IS IDLE, owning the pump seam above. `tell` receives the
/// diagnostic sentence when a participant is held (a journal line, a printed line); it is
/// never called otherwise. An exception the turn's facts do not attribute propagates.
ServedTurn serve_until_idle(loom::Switchboard& bus,
                            const std::function<void(const std::string&)>& tell);

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_HOST_PUMP_HPP
