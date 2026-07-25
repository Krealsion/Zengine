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
// swapped mid-game, and if it ever is, its successor re-asks on the next
// TimerReady; the predecessor's beat dies against a never-reused WeaveId.
//
// Its first breath is the TimerService's hello: the host loads it before the
// wind, TimerReady reaches it, and it asks for the beat. Re-hearing the
// hello (a replaced TimerService re-announces) re-asks — an upsert, so the
// schedule is replaced, never doubled.

#include "vocabulary.hpp"

#include "timer/vocabulary.hpp"

#include <zen/kernel/export.hpp>
#include <zen/weave.hpp>

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

class SnakeClock
    : public loom::WeaveBase<SnakeClock, ClockState,
                             loom::Accept<timer::TimerReady, timer::TimerFired>,
                             loom::Emit<timer::StartTimer, SnakeTick>> {
public:
    void on(const timer::TimerReady&, loom::Mail& mail) {
        mail.send_to_role(timer::kTimerRole,
                          timer::StartTimer{kTickTimerId, kTickMs, /*repeat=*/true});
    }

    void on(const timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != kTickTimerId) {
            return; // not our ask; someone else's time is not our business
        }
        ++state_.ticks;
        mail.send_to_role(kWorldRole, SnakeTick{});
    }
};

} // namespace

ZEN_EXPORT_WEAVE(SnakeClock)
