// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The clock adapter: snake's own time binding, as a weave. The Timer package speaks timers and
// the world speaks world-time (`SnakeTick`) and never learns where it comes from: this asks the
// Timer for a 120 ms repeating beat and relays each firing as a `SnakeTick` to whoever holds
// `snake.world` at delivery, so time survives the world being swapped.
// Reference: docs/reference/snake.md.

// A replaceable policy -- a pause, slow-motion, replay or turn-based driver can take the slot --
// and not the Timer protocol's ceremony, which is `timer/binding.hpp`'s (TIMER-05). Its own beat
// is requester-addressed: a successor asks on its own activation, and a predecessor's beat dies
// against a never-reused WeaveId.

#include "vocabulary.hpp"

#include "timer/binding.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

#include <chrono>
#include <cstdint>

namespace {

using namespace zengine::snake;
namespace timer = zengine::timer;

/// One honest counter: how many beats this clock has turned into world time.
struct ClockState {
    std::int64_t ticks = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(ClockState, 1, ZEN_FIELD(ticks));
};

class SnakeClock : public timer::TimedWeave<SnakeClock, ClockState, loom::Accept<>,
                                            loom::Emit<SnakeTick>> {
public:
    /// The whole binding: a repeating beat delivered back to this weave, which
    /// this weave turns into world time. Declaring it sends nothing — the
    /// binding is desired local state, reconciled when this incarnation is
    /// activated and again whenever the Timer service says it is available.
    SnakeClock()
        : tick_(timers().repeat(kTickTimerId, std::chrono::milliseconds(kTickMs),
                                &SnakeClock::on_tick)) {}

private:
    /// The policy this weave exists for, and now the only thing in it: a real
    /// firing becomes world time, addressed BY ROLE so it survives the world
    /// being swapped mid-game.
    void on_tick(const timer::TimerFired&, loom::Mail& mail) {
        ++state_.ticks;
        mail.send_to_role(kWorldRole, SnakeTick{});
    }

    Handle tick_;
};

} // namespace

ZEN_EXPORT_WEAVE(SnakeClock)
