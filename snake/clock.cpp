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
// Its first breath is its OWN ACTIVATION (R2A-2): the Loom's control door tells
// this incarnation it is live, and it asks for the beat then. That is what makes
// load order stop mattering — loaded before the Timer or long after it, the
// adapter arranges its own time either way. `TimerReady` covers the one case its
// activation cannot: being loaded first, so that the activation-time ask refused
// into a role nothing held yet. Both paths ask the same thing, and asking twice
// is free — every ask is an upsert.

#include "vocabulary.hpp"

#include "activation/activation.hpp"
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
                             loom::Accept<loom::Activated, timer::TimerReady, timer::TimerFired>,
                             loom::Emit<timer::StartTimer, SnakeTick>> {
public:
    /// This incarnation is live: ask for world time. A duplicate or replayed
    /// activation asks for nothing.
    void on(const loom::Activated& a, loom::Mail& mail) {
        if (!activation_.accept(mail.sender(), a.sequence)) {
            return;
        }
        ask(mail);
    }

    /// The Timer became available — the load-order case our own activation
    /// cannot cover, and the one a replaced service needs.
    void on(const timer::TimerReady&, loom::Mail& mail) { ask(mail); }

    void on(const timer::TimerFired& f, loom::Mail& mail) {
        if (f.id != kTickTimerId) {
            return; // not our ask; someone else's time is not our business
        }
        ++state_.ticks;
        mail.send_to_role(kWorldRole, SnakeTick{});
    }

private:
    void ask(loom::Mail& mail) {
        mail.send_to_role(timer::kTimerRole,
                          timer::StartTimer{kTickTimerId, kTickMs, /*repeat=*/true});
    }

    zengine::ActivationCursor activation_; ///< per-incarnation, never state
};

} // namespace

ZEN_EXPORT_WEAVE(SnakeClock)
