#ifndef ZENGINE_TIMER_VOCABULARY_HPP
#define ZENGINE_TIMER_VOCABULARY_HPP

// The Timer package's message vocabulary — time, message-shaped.
//
// The rule this package installs: games and packages do not read the OS clock
// and do not sleep. A weave that wants time ASKS for it (StartTimer /
// StartRoleTimer) and time ARRIVES like everything else does — as a message
// (TimerFired), delivered by the TimerService weave holding `zengine.timer`.
// The service is the one place in the running system that owns a monotonic
// clock and the one nap; everyone else just hears beats. There is no polling
// API in V1: time is a stream of events, not a state to be asked about.
//
// THE V1 CONTRACT (Vision's phase prompt) is four shapes: StartTimer v1,
// CancelTimer v1, CancelAllMyTimers v1, TimerFired v1 — spelled here exactly
// as locked; the timer suite pins the spellings by content-id.
//
// NAMED ADDITIONS (the contract proved insufficient here; recorded face-up
// per the report-back rule, never silently):
//   - StartRoleTimer v1 — the prompt's own "or to a role if the design makes
//     that cleaner" option, made a distinct shape so StartTimer stays exactly
//     as specified. A requester-addressed timer dies with its requester — and
//     a weave cannot observe another weave's death (the bus shows a sender no
//     outcomes, and no unload broadcast exists), so a heartbeat that must
//     SURVIVE its starter being swapped (the input pump, the skin pump) is
//     addressed to the ROLE: whoever holds the slot at each firing hears the
//     beat. That is also what heals a freshly swapped-in skin on a dead-quiet
//     bus: the beat belongs to the role, so the successor inherits it.
//   - TimerReady v1 — the service's hello (the SurfaceReady precedent),
//     published once per incarnation on its first beat. It is the system's
//     first breath: a weave loaded before the wind hears it and starts its
//     own timers in response — which is how anything gets execution time at
//     all in a world where the host no longer pumps anyone.
//   - Drive v1 — the beat itself. A weave runs only when a message arrives,
//     so the service keeps itself alive by re-sending Drive to itself at the
//     end of every beat; inside the beat it naps to the next deadline (the
//     one sleep in the system) and fires what came due. The HOST winds the
//     clock exactly once at boot by sending the first Drive; it owes nothing
//     per lap. Not a pump: nobody owes the service execution time after the
//     wind, and a stray extra Drive is a harmless extra beat, not a lever.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>

namespace zengine::timer {

/// Ask for time, delivered back to YOU (the sender): TimerFired{id} arrives
/// at the requesting weave after delay_ms, once (repeat=false) or every
/// delay_ms (repeat=true). `id` is the caller's own name for the timer
/// ("snake.tick", "my.cooldown"); it is scoped to the requester, so two
/// weaves using the same id never collide. Asking again with the same id
/// REPLACES the schedule (an upsert — which is also how a cadence changes).
/// A repeating delay below 1ms is clamped to 1ms (a 0ms repeat would be a
/// hot spin wearing a timer's clothes); a negative one-shot delay fires on
/// the next beat. Sent by a root (no weave identity), there is no one to
/// deliver to: dropped, counted on the service's `dropped` poke counter.
struct StartTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    ZEN_SHAPE(StartTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat));
};

/// Ask for time, delivered to a ROLE: TimerFired{id} goes to whoever holds
/// `role` at each firing. The beat outlives any particular holder — a swap's
/// successor inherits it without asking — and an unheld role refuses the
/// delivery cleanly (the beat waits for the next holder; it is the slot's
/// pulse, not a weave's). Upsert key is (role, id), ACROSS requesters: a
/// successor re-asking replaces its predecessor's schedule instead of
/// doubling the beat. Same clamps as StartTimer.
struct StartRoleTimer {
    std::string id;
    std::int64_t delay_ms = 0;
    bool repeat = false;
    std::string role;
    ZEN_SHAPE(StartRoleTimer, 1, ZEN_FIELD(id), ZEN_FIELD(delay_ms), ZEN_FIELD(repeat),
              ZEN_FIELD(role));
};

/// Cancel by id: removes the sender's own (requester, id) timer, and any
/// role timer with this id that the sender itself started. V1 edge, honest:
/// a role timer whose starter is gone is cancellable only by a successor
/// first re-asking (upsert takes ownership) and then cancelling.
struct CancelTimer {
    std::string id;
    ZEN_SHAPE(CancelTimer, 1, ZEN_FIELD(id));
};

/// The convenience for a weave that is going away politely: every timer the
/// sender started — requester-addressed and role-addressed alike — dies.
struct CancelAllMyTimers {
    ZEN_SHAPE(CancelAllMyTimers, 1);
};

/// A timer came due. `id` is the one the requester chose; the consumer
/// obligation applies — match it against YOUR OWN asks and ignore the rest
/// (a role can be aimed at by anyone; an unknown id is data, not a command).
struct TimerFired {
    std::string id;
    ZEN_SHAPE(TimerFired, 1, ZEN_FIELD(id));
};

/// The service's hello: published once per incarnation, on its first beat.
/// Weaves that need standing time listen for it and start their timers when
/// they hear it — the first breath of a system whose host pumps nobody. (A
/// weave loaded LATER than the wind never hears it; it either receives an
/// already-standing role beat, or starts its timers on its own first message
/// — the swapped-in skin does exactly that.)
struct TimerReady {
    ZEN_SHAPE(TimerReady, 1);
};

/// The service's own beat (the named addition — see the header comment). The
/// host sends the FIRST one (the wind); the service re-sends it to its own
/// ROLE forever after — so the chain itself survives the service being
/// replaced (the successor inherits the live beat with the role), and a
/// service loaded without the role simply does not beat: the role is the
/// contract. Empty by design: it carries no question, no answer, and no
/// authority — a spurious one costs one extra beat and nothing else.
struct Drive {
    ZEN_SHAPE(Drive, 1);
};

/// The role slot the TimerService holds: the address "whoever provides
/// time", which outlives any particular implementation being swapped in.
inline constexpr const char* kTimerRole = "zengine.timer";

/// The beat cap: the longest the service will nap when nothing is due
/// sooner, which is also the worst-case lateness of a firing and the arrival
/// bound on a StartTimer being considered. 10ms — the responsiveness the old
/// host loop's nap gave the whole system, now owned by the one weave allowed
/// to sleep.
inline constexpr std::int64_t kBeatCapMs = 10;

} // namespace zengine::timer

#endif // ZENGINE_TIMER_VOCABULARY_HPP
