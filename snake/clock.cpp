// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The clock adapter — snake's own time binding, as a weave.
//
// The Timer package speaks timers (StartTimer, TimerFired); the world speaks
// world-time (SnakeTick) and deliberately never learns where it comes from.
// This weave is the whole distance between them: it asks the TimerService
// for a 120ms repeating beat and relays each firing into a SnakeTick. It is
// the exact move the controls adapter made for keys, pointed at time — and
// because the binding is a weave and not a line in the host, it is
// REPLACEABLE like everything else here: a slow-motion clock, a pause weave,
// or a replay driver can take its place (or stand beside it, under its own
// timer id) without the world or the Timer package changing a line.
//
// It addresses the world BY ROLE, exactly as the host's old loop did: the
// tick goes to whoever holds snake.world at delivery, so time survives the
// world being swapped mid-game (moment 3 does exactly that). Its OWN timer
// is requester-addressed (the prompt's V1 default) — this adapter is never
// swapped mid-game, and if it ever is, its successor asks on its own
// activation; the predecessor's beat dies against a never-reused WeaveId.
//
// WHAT STAYED AND WHAT LEFT (R2A-3). The adapter weave REMAINS, because the
// time-to-world policy is genuinely replaceable: a pause driver, a slow-motion
// clock, a replay feeder or a turn-based driver can take this slot without the
// world or the Timer package changing a line. What left is the Timer protocol
// CEREMONY — accepting an activation, deduplicating it, accepting TimerReady,
// sending the ask, filtering firing ids — because none of that was ever this
// weave's policy. It was common package vocabulary being retyped, and it now
// lives in timer/binding.hpp. The whole adapter is one declaration and one
// callback.

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
