// The Trust-Gate liveness probes (Stage 5 of the audit phase, 2026-07-26;
// ratified onto main by the R1 repair phase, 2026-07-27).
//
// THIS SUITE'S STATUS IS DELIBERATE, and a reader should know which kind of
// test file this is. It does NOT pin behavior anyone designed and wants kept.
// It pins what the substrate MEASURABLY DOES today, including where that is
// unwanted: probe A asserts that swapping the timer service KILLS the beat
// chain. That is a description, not an endorsement — and it is here on main on
// purpose, for two reasons. First, an undocumented behavior that a repair
// phase corrected the prose about should not be free to drift again silently;
// today these cases are the guard on the corrected comments. Second, when the
// open lifecycle question is answered (R2 — re-lighting liveness after a swap,
// a steward that re-winds, a Drive serial), probe A is exactly the case that
// FLIPS: it becomes the guard on the new promise. A red probe A after an R2
// change is expected and good; rewrite it to the new truth rather than
// deleting it, and keep the measured half (which refusal, whose sender) —
// that is the part the comments now depend on.
//
// These settle, by execution, the questions the lane's prose left open — above
// all the timer vocabulary's central survival claim, as it read at the audited
// tips: "the service re-sends [Drive] to its own ROLE forever after — so the
// chain itself survives the service being replaced". Measured FALSE for a
// swap, TRUE for a reload; the vocabulary now says so (see Drive in
// timer/vocabulary.hpp, corrected in R1). Each probe pins what the substrate
// ACTUALLY does; where a probe contradicts a comment, the comment is the thing
// under judgment, not the probe.
//
// Probe map:
//   A. SwapWeave on the live timer role  — does the beat chain survive? Which
//      refusal kills it (CapabilityDenied = sender-death, NoSuchTarget =
//      vacancy)? Does the successor announce? Does a fresh root wind heal?
//   B. ReloadWeave of the live service   — same-id rebind: does the chain
//      survive THAT, and does the reloaded incarnation re-announce?
//   C. Double wind                       — is a stray extra Drive "a harmless
//      extra beat" (vocabulary) or a permanent second chain (conserved)?
//   D. Late load                         — a consumer loaded after the wind:
//      does it ever hear TimerReady / tick at all?
//
// The chain never quiesces while alive, so every open-ended pump carries BOTH
// levers: a stopwatch timer (stops the bus when the chain is alive to fire it)
// and a delivered-Drive watchdog via a bus observer (stops the bus if a chain
// outlives the probe's expectation). A pump that returns with an empty queue
// and no watchdog trip IS the chain-death observation.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include "snake/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

using namespace zengine::timer;
using loom::schema_of;

namespace {

// ---- witness (the tier-3 rig of test_timer.cpp, with swap/reload reach) -----

struct Seen {
    struct Answer {
        std::uint64_t corr = 0;
        int kind = 0; // 0 Result, 1 Ack, 2 Refused
        std::string text;
    };
    std::vector<Answer> answers;
    std::vector<std::string> fired;
    std::int64_t ready = 0;
    std::vector<zengine::snake::SnakeVisual> visuals;
    loom::Switchboard* bus = nullptr;
    std::string stop_id;
    std::int64_t stop_count = 1;
    std::int64_t stop_seen = 0;

    const Answer* find(std::uint64_t corr) const {
        for (const Answer& a : answers) {
            if (a.corr == corr) {
                return &a;
            }
        }
        return nullptr;
    }
};

struct WitnessState {
    std::int64_t noted = 0;
    ZEN_SHAPE(WitnessState, 1, ZEN_FIELD(noted));
};

class Witness
    : public loom::WeaveBase<Witness, WitnessState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused, TimerFired,
                                          TimerReady, zengine::snake::SnakeVisual>,
                             loom::Emit<>> {
public:
    explicit Witness(Seen& seen) : seen_(&seen) {}
    void on(const loom::Result& r, loom::Mail& mail) { note(mail, 0, r.value); }
    void on(const loom::Ack&, loom::Mail& mail) { note(mail, 1, ""); }
    void on(const loom::Refused& r, loom::Mail& mail) { note(mail, 2, r.reason); }
    void on(const TimerFired& f, loom::Mail&) {
        seen_->fired.push_back(f.id);
        if (!seen_->stop_id.empty() && f.id == seen_->stop_id &&
            ++seen_->stop_seen >= seen_->stop_count) {
            seen_->bus->stop();
        }
    }
    void on(const TimerReady&, loom::Mail&) { ++seen_->ready; }
    void on(const zengine::snake::SnakeVisual& v, loom::Mail&) { seen_->visuals.push_back(v); }

private:
    void note(loom::Mail& mail, int kind, std::string text) {
        ++state_.noted;
        seen_->answers.push_back(Seen::Answer{mail.correlation(), kind, std::move(text)});
    }
    Seen* seen_;
};

/// Refusal forensics + chain watchdog, wired as a bus observer per probe.
struct DriveForensics {
    std::int64_t delivered = 0;
    std::int64_t refused_capability = 0;
    std::int64_t refused_no_target = 0;
    std::int64_t refused_other = 0;
    std::int64_t stop_after_delivered = -1; ///< watchdog: stop the bus here
    loom::Switchboard* bus = nullptr;

    void arm(loom::Switchboard& b) {
        bus = &b;
        b.add_observer([this](const loom::BusEvent& ev) {
            if (ev.schema_name != "Drive") {
                return;
            }
            if (ev.kind == loom::EventKind::Delivered) {
                ++delivered;
                if (stop_after_delivered >= 0 && delivered >= stop_after_delivered) {
                    bus->stop();
                }
            } else if (ev.kind == loom::EventKind::Refused) {
                if (ev.refusal.reason == loom::RefusalReason::CapabilityDenied) {
                    ++refused_capability;
                } else if (ev.refusal.reason == loom::RefusalReason::NoSuchTarget) {
                    ++refused_no_target;
                } else {
                    ++refused_other;
                }
            }
        });
    }
};

struct Rig {
    loom::Switchboard bus;
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    Seen seen;
    loom::WeaveId witness = mount_witness();
    std::uint64_t next_corr = 1;

    loom::WeaveId mount_witness() {
        seen.bus = &bus;
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
        reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
        reach.allow_to_any(StartTimer::zen_name, StartTimer::zen_version);
        return loom::mount_granted<Witness>(bus, std::move(reach), seen);
    }

    template <class Cmd>
    std::uint64_t command(const Cmd& c) {
        const std::uint64_t corr = next_corr++;
        bus.send_as(witness, manager,
                    loom::Message(loom::to_value(c), witness, witness, corr));
        return corr;
    }

    template <class T>
    void ask(loom::WeaveId service, const T& msg) {
        bus.send_as(witness, service, loom::Message(loom::to_value(msg), witness, witness, 0));
    }

    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const std::uint64_t corr = command(loom::LoadWeave{name, path, role});
        bus.pump(); // pre-wind only: with no chain alive this pump drains
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        REQUIRE_MESSAGE(a->kind == 0, "load refused: ", a->text);
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(a->text))};
    }

    void wind() { bus.send_to_role(kTimerRole, loom::Message(loom::to_value(Drive{}))); }

    /// Run the live chain ~ms of real time: stopwatch one-shot as the lever.
    void pump_for(loom::WeaveId service, std::int64_t ms) {
        seen.stop_id = "probe.stopwatch";
        seen.stop_count = 1;
        seen.stop_seen = 0;
        ask(service, StartTimer{"probe.stopwatch", ms, false});
        bus.pump();
        seen.stop_id.clear();
    }

    std::int64_t poke_int(loom::WeaveId service, loom::WeaveId target, const char* field) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(loom::PokeRead{field}), loom::WeaveId{},
                                       witness, corr));
        pump_for(service, 25);
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        REQUIRE(a->kind == 0);
        return std::stoll(a->text);
    }
};

// ---- fake-clock mini-rig for the deterministic double-wind probe ------------

struct FakeHooks {
    std::int64_t now = 0;
    std::int64_t beats = 0;
    std::int64_t stop_after = -1;
    loom::Switchboard* bus = nullptr;
};

struct FakeClock {
    FakeHooks* h = nullptr;
    std::int64_t now_ms() { return h->now; }
    void nap_ms(std::int64_t ms) {
        if (ms > 0) {
            h->now += ms;
        }
        ++h->beats;
        if (h->stop_after >= 0 && h->beats >= h->stop_after) {
            h->bus->stop();
        }
    }
};

using FakeService = TimerServiceT<FakeClock>;

struct FakeRig {
    loom::Switchboard bus;
    FakeHooks hooks;
    loom::WeaveId service{};

    FakeRig() {
        hooks.bus = &bus;
        auto weave = std::make_unique<FakeService>(FakeClock{&hooks});
        FakeService* raw = weave.get();
        loom::Grant grant = loom::emit_default_grant(*raw);
        loom::allow_poke_answers(grant);
        service = bus.register_weave(std::move(weave), std::move(grant), kTimerRole);
        raw->zen_set_self(service);
    }

    void run_beats(std::int64_t n, bool wind = false) {
        hooks.stop_after = hooks.beats + n;
        if (wind) {
            bus.send_to_role(kTimerRole, loom::Message(loom::to_value(Drive{})));
        }
        bus.pump();
        hooks.stop_after = -1;
    }
};

} // namespace

// ============================================================================
// Probe A — the primary: SwapWeave on the live timer role
// ============================================================================

TEST_CASE("probe A: a SwapWeave on the live timer role KILLS the beat chain "
          "(sender-death, CapabilityDenied), and only a fresh root wind heals it") {
    Rig r;
    DriveForensics f;
    f.arm(r.bus);

    const loom::WeaveId old_service = r.load("zengine-timer", TIMER_SO, kTimerRole);
    r.wind();
    r.pump_for(old_service, 40);
    CHECK(r.seen.ready == 1);        // the incumbent announced
    CHECK(r.bus.pending() > 0);      // the chain is alive and parked
    const std::int64_t beats_before = r.poke_int(old_service, old_service, "beats");
    CHECK(beats_before > 0);

    // The swap. Hard on purpose (the service declares no PrepareShutdown, so
    // graceful would auto-degrade to this anyway). Watchdog: if the chain
    // somehow survives, stop after a bounded number of further beats instead
    // of pumping forever.
    f.stop_after_delivered = f.delivered + 50;
    const std::uint64_t corr = r.command(loom::SwapWeave{kTimerRole, "zengine-timer", TIMER_SO,
                                                         /*graceful=*/false});
    r.bus.pump();
    f.stop_after_delivered = -1;

    const Seen::Answer* swapped = r.seen.find(corr);
    REQUIRE(swapped != nullptr);
    REQUIRE_MESSAGE(swapped->kind == 0, "swap refused: ", swapped->text);
    const loom::WeaveId successor{static_cast<std::uint64_t>(std::stoll(swapped->text))};
    CHECK(successor.value != old_service.value);

    // THE OBSERVATION. The pump returned by QUIESCENCE, not by watchdog: the
    // bus drained — the chain is dead. The kill was the old incarnation's
    // in-flight re-wind failing sender lookup at delivery: CapabilityDenied,
    // not role vacancy (the successor already held the role by then).
    CHECK(r.bus.pending() == 0);
    CHECK(f.refused_capability == 1);
    CHECK(f.refused_no_target == 0);
    CHECK(f.refused_other == 0);
    MESSAGE("post-swap: pending=", r.bus.pending(), " drive refusals: capability=",
            f.refused_capability, " no_target=", f.refused_no_target);

    // The successor never got a first message: no hello, no beats. The
    // vocabulary's "the successor inherits the live beat with the role" did
    // not happen; the self-heal cascade (re-announce -> consumers re-ask) is
    // unreachable from here.
    CHECK(r.seen.ready == 1); // still only the incumbent's hello
    const std::int64_t successor_beats = r.poke_int(old_service, successor, "beats");
    CHECK(successor_beats == 0);

    // A fresh ROOT wind heals everything: the successor announces (hello #2)
    // and beats. Nothing automatic does this today — the host would have
    // already exited on pending()==0 ("time is gone").
    r.wind();
    r.pump_for(successor, 40);
    CHECK(r.seen.ready == 2);
    CHECK(r.poke_int(successor, successor, "beats") > 0);
    CHECK(r.bus.pending() > 0); // a live chain again
}

// ============================================================================
// Probe B — the corollary: ReloadWeave is the succession that DOES work
// ============================================================================

TEST_CASE("probe B: a ReloadWeave of the live timer service PRESERVES the chain "
          "(same WeaveId), and the reloaded incarnation re-announces") {
    Rig r;
    DriveForensics f;
    f.arm(r.bus);

    const loom::WeaveId service = r.load("zengine-timer", TIMER_SO, kTimerRole);
    r.wind();
    r.pump_for(service, 40);
    CHECK(r.seen.ready == 1);
    const std::int64_t beats_before = r.poke_int(service, service, "beats");
    CHECK(beats_before > 0);

    // Reload in place. The chain's parked Drive was sent by THIS WeaveId, and
    // reload rebinds the new library behind the SAME adapter/WeaveId — the
    // sender lookup keeps succeeding, the role is never vacated, the chain
    // rides straight through. Watchdog-stop after a few more beats (the chain
    // surviving means this pump would otherwise never return).
    f.stop_after_delivered = f.delivered + 5;
    const std::uint64_t corr = r.command(loom::ReloadWeave{"zengine-timer", TIMER_SO});
    r.bus.pump();
    f.stop_after_delivered = -1;

    const Seen::Answer* reloaded = r.seen.find(corr);
    REQUIRE(reloaded != nullptr);
    CHECK(reloaded->kind == 1); // Ack
    CHECK(r.bus.pending() > 0); // ALIVE: the watchdog stopped a live chain
    CHECK(f.refused_capability == 0);
    CHECK(f.refused_no_target == 0);

    // The reloaded incarnation re-announced on its first beat (announced_ is
    // a plain member, reset by reload) — so the self-heal story (hello #2 ->
    // standing-beat owners re-ask) actually runs HERE, where the chain lives.
    CHECK(r.seen.ready == 2);

    // The beats counter is TimerState — it rode the reload transplant; the
    // schedule table (plain members) did not, which is reload's documented
    // shape (state carries, non-state resets).
    const std::int64_t beats_after = r.poke_int(service, service, "beats");
    CHECK(beats_after > beats_before);
    MESSAGE("reload: beats ", beats_before, " -> ", beats_after, ", hellos=", r.seen.ready);
}

// ============================================================================
// Probe C — double wind: conserved second chain, not "a harmless extra beat"
// ============================================================================

TEST_CASE("probe C: a second Drive is a PERMANENT second chain — conserved, "
          "never coalesced (though beat throughput stays nap-bound)") {
    // Deterministic fake-clock form. With no timers standing, every beat naps
    // kBeatCapMs, and a parked pump leaves exactly the in-flight Drives in the
    // queue: pending() IS the chain count.
    FakeRig single;
    single.run_beats(6, /*wind=*/true);
    CHECK(single.hooks.beats == 6);
    CHECK(single.bus.pending() == 1); // one chain: one parked Drive

    FakeRig dual;
    dual.bus.send_to_role(kTimerRole, loom::Message(loom::to_value(Drive{}))); // stray extra
    dual.run_beats(6, /*wind=*/true);                                          // + the wind
    CHECK(dual.hooks.beats == 6);
    CHECK(dual.bus.pending() == 2); // TWO parked Drives: the second chain is real

    // The nuance the vocabulary missed in BOTH directions: the extra chain is
    // not "one extra beat" (it never goes away) — but it also does not double
    // the beat rate (each beat naps, so the two chains SHARE the nap-bound
    // throughput: same virtual time for the same beat count, halved cadence
    // per chain).
    CHECK(dual.hooks.now == single.hooks.now); // 6 beats -> 60ms, chains or no

    // And it is permanent: many beats later the queue still carries two, and
    // the beat rate is still nap-bound (14 beats -> 140ms).
    dual.run_beats(8);
    CHECK(dual.bus.pending() == 2);
    CHECK(dual.hooks.now == 140);
}

// ============================================================================
// Probe D — late-load deafness
// ============================================================================

TEST_CASE("probe D: a consumer loaded AFTER the wind never hears TimerReady and "
          "never ticks — the hello is once per incarnation, not a standing offer") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    (void)r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);

    r.wind();
    r.pump_for(timer_so, 60); // the hello happened here, before the clock existed
    CHECK(r.seen.ready == 1);

    // Load the latecomer WITHOUT load()'s bare pump: the chain is live now, so
    // the pump must carry the stopwatch lever (the probe-harness lesson — an
    // unlevered pump under a live chain never returns).
    const std::uint64_t corr = r.command(loom::LoadWeave{"snake-clock", SNAKE_CLOCK_SO, ""});
    r.pump_for(timer_so, 400); // load answers within the first beats; then ~3 tick periods
    const Seen::Answer* loaded = r.seen.find(corr);
    REQUIRE(loaded != nullptr);
    REQUIRE_MESSAGE(loaded->kind == 0, "late load refused: ", loaded->text);
    const loom::WeaveId clock_so{static_cast<std::uint64_t>(std::stoll(loaded->text))};

    // The latecomer is deaf: it never heard the (already-spent) hello, so it
    // never asked for the tick timer, so the world never moved — visuals would
    // have arrived at the witness if it had (test_timer's early-load twin case
    // proves >=2 by 400ms).
    CHECK(r.poke_int(timer_so, clock_so, "ticks") == 0);
    CHECK(r.seen.visuals.empty());
}
