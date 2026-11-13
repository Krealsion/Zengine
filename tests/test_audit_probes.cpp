// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The liveness probes: what the substrate measurably does to a live Timer's time when the Timer
// is swapped (A), reloaded (B), stimulated twice (C) or joined late (D) -- each keeping its
// MEASURED half, which refusal and whose wake, beside the promise of TIMER-01: one claim of time
// an activated incarnation, nothing of it in flight between wakes. Rewrite a probe to a new truth,
// never drop its measurement. Every pump is the host's loop on a clock that moves only when told.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "role_set.hpp"
#include "doctest.h"

#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include "examples/snake/vocabulary.hpp"

#include "lifecycle_door.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <zen/host/clock.hpp>
#include <zen/host/reactor.hpp>
#include <zen/host/wait_set.hpp>
#include <zen/kernel/control.hpp>
#include <zen/kernel/admission.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

using namespace zengine::timer;
using loom::schema_of;

namespace {

/// THE HOST'S LOOP, as the Timer suite turns it: the bus turned, then the host's one wait -- a
/// look while the bus holds work, else a rest to the soonest time due (on a clock that moves only
/// when told, the wait moves it there), or until `until`. It returns once `done()` holds with the
/// bus empty, once the host is quiet, or at `until` after what is due then was handed over.
template <class Done>
void host_loop(loom::Switchboard& bus, loom::host::Reactor& wait, const loom::host::Clock& clock,
               Done done, std::optional<std::int64_t> until = std::nullopt) {
    for (int turn = 0; turn < 200000; ++turn) {
        bus.pump_pending();
        if (bus.pending() != 0) {
            (void)wait.after_turn(0);
            continue;
        }
        if (done()) {
            return;
        }
        if (until && clock.now_ms() >= *until) {
            (void)wait.after_turn(0);
            if (bus.pending() == 0) {
                return;
            }
            continue;
        }
        (void)(until ? wait.after_turn(*until - clock.now_ms()) : wait.after_turn());
        if (!until && bus.pending() == 0) {
            return;
        }
    }
    FAIL("the host's loop did not come to rest");
}

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

    const Answer* find(std::uint64_t corr) const {
        for (const Answer& a : answers) {
            if (a.corr == corr) {
                return &a;
            }
        }
        return nullptr;
    }
    std::int64_t fired_of(const char* id) const {
        return static_cast<std::int64_t>(std::count(fired.begin(), fired.end(), id));
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
    void on(const TimerFired& f, loom::Mail&) { seen_->fired.push_back(f.id); }
    void on(const TimerReady&, loom::Mail&) { ++seen_->ready; }
    void on(const zengine::snake::SnakeVisual& v, loom::Mail&) { seen_->visuals.push_back(v); }

private:
    void note(loom::Mail& mail, int kind, std::string text) {
        ++state_.noted;
        seen_->answers.push_back(Seen::Answer{mail.correlation(), kind, std::move(text)});
    }
    Seen* seen_;
};

/// THE TIMER'S TRAFFIC, observed: every wake the host's wait delivered and to whom, and every
/// refusal of a wake or a firing, by reason. Between wakes the bus holds nothing for the Timer, so
/// a replacement has nothing in flight to refuse.
struct TimeForensics {
    std::map<std::uint64_t, std::int64_t> wakes_to; ///< due wakes delivered, by weave id
    std::int64_t refused_capability = 0;
    std::int64_t refused_no_target = 0;
    /// The author's life ended before delivery: the refusal a beat of a replaced Timer met when
    /// the Timer kept time by a message on the bus. Kept apart so a probe can say it is zero.
    std::int64_t refused_sender_life = 0;
    std::int64_t refused_other = 0;

    void arm(loom::Switchboard& b) {
        b.add_observer([this](const loom::BusEvent& ev) {
            const bool wake = ev.schema_name == loom::host::Waited::zen_name;
            if (!wake && ev.schema_name != TimerFired::zen_name) {
                return;
            }
            if (ev.kind == loom::EventKind::Delivered) {
                if (wake && ev.payload != nullptr && ev.payload->get("due") != nullptr &&
                    ev.payload->get("due")->as_bool()) {
                    ++wakes_to[ev.target.value];
                }
            } else if (ev.kind == loom::EventKind::Refused) {
                if (ev.refusal.reason == loom::RefusalReason::CapabilityDenied) {
                    ++refused_capability;
                } else if (ev.refusal.reason == loom::RefusalReason::SenderLifeEnded) {
                    ++refused_sender_life;
                } else if (ev.refusal.reason == loom::RefusalReason::NoSuchTarget) {
                    ++refused_no_target;
                } else {
                    ++refused_other;
                }
            }
        });
    }
    std::int64_t refused() const {
        return refused_capability + refused_no_target + refused_sender_life + refused_other;
    }
    std::int64_t wakes_of(loom::WeaveId who) const {
        const auto it = wakes_to.find(who.value);
        return it == wakes_to.end() ? 0 : it->second;
    }
};

struct Rig {
    loom::Switchboard bus;
    loom::host::ManualClock clock;
    loom::host::Reactor wait{bus, clock};
    loom::Kernel kernel{bus, loom::trust_every_artifact(
        "Zengine's test harness: it loads only this build tree's output")};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    Seen seen;
    loom::WeaveId witness = mount_witness();
    std::uint64_t next_corr = 1;

    /// The host waits on the Timer's role, as Workshop and the app do.
    Rig() { wait.trust(kTimerRole); }

    loom::WeaveId mount_witness() {
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

    /// Ask the Timer's ROLE, as every production consumer does: resolved at delivery.
    template <class T>
    void ask_role(const T& msg) {
        bus.send_as_to_role(witness, kTimerRole,
                            loom::Message(loom::to_value(msg), witness, witness, 0));
    }

    /// The bus turned until it is empty, and no time passing.
    void settle() {
        host_loop(bus, wait, clock, [] { return true; });
    }

    /// The host's loop for `ms` on the clock.
    void run_for(std::int64_t ms) {
        host_loop(bus, wait, clock, [] { return false; }, clock.now_ms() + ms);
    }

    /// The host's loop until the world draws, at most `bound_ms` on the clock.
    void run_until_visual(std::int64_t bound_ms) {
        host_loop(bus, wait, clock, [this] { return !seen.visuals.empty(); },
                  clock.now_ms() + bound_ms);
    }

    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const std::uint64_t corr = command(loom::LoadWeave{name, path, zengine::testing::role_set(role)});
        settle();
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        REQUIRE_MESSAGE(a->kind == 0, "load refused: ", a->text);
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(a->text))};
    }

    /// Read one integer field, no time passing.
    std::int64_t poke_int(loom::WeaveId target, const char* field) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(loom::PokeRead{field}), loom::WeaveId{},
                                       witness, corr));
        settle();
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        REQUIRE(a->kind == 0);
        return std::stoll(a->text);
    }
};

// ---- the deterministic mini-rig for the double-stimulus probe --------------------------------

struct FakeHeard {
    std::int64_t ready = 0;
    std::vector<std::int64_t> fired_at; ///< the host's clock at each firing heard
    const loom::host::Clock* clock = nullptr;
};

struct FakeEarState {
    std::int64_t count = 0;
    ZEN_SHAPE(FakeEarState, 1, ZEN_FIELD(count));
};

/// The probe's consumer. `Waited` is in its Emit list ON PURPOSE: the replayed wake the probe
/// forges must be sayable through the honest API.
class FakeEar : public loom::WeaveBase<FakeEar, FakeEarState, loom::Accept<TimerReady, TimerFired>,
                                       loom::Emit<StartTimer, loom::host::Waited>> {
public:
    explicit FakeEar(FakeHeard& h) : h_(&h) {}
    void on(const TimerReady&, loom::Mail&) { ++h_->ready; }
    void on(const TimerFired&, loom::Mail&) { h_->fired_at.push_back(h_->clock->now_ms()); }

private:
    FakeHeard* h_;
};

using zengine::testing::TestDoor;

struct FakeRig {
    loom::Switchboard bus;
    loom::host::ManualClock clock;
    loom::host::Reactor wait{bus, clock};
    FakeHeard heard;
    loom::WeaveId service{};
    loom::WeaveId ear{};
    loom::WeaveId door{};
    std::int64_t next_sequence = 1;

    FakeRig() {
        heard.clock = &clock;
        wait.trust(kTimerRole);
        auto weave = std::make_unique<TimerWeave>();
        TimerWeave* raw = weave.get();
        loom::Grant grant = loom::emit_default_grant(*raw);
        loom::allow_poke_answers(grant);
        service = bus.register_weave(std::move(weave), std::move(grant), kTimerRole);
        raw->zen_set_self(service);
        ear = loom::mount<FakeEar>(bus, heard);
        door = zengine::testing::mount_door(bus);
    }

    std::int64_t current_sequence() const { return next_sequence - 1; }

    void activate(std::int64_t sequence) {
        zengine::testing::order_activation(bus, door, service, sequence);
    }
    void activate() { activate(next_sequence++); }

    void run_until(std::int64_t ms) {
        host_loop(bus, wait, clock, [] { return false; }, ms);
    }

    /// The lattice a 10 ms repeating timer anchored at 0 has fired by `ms`.
    static std::vector<std::int64_t> lattice_to(std::int64_t ms) {
        std::vector<std::int64_t> out;
        for (std::int64_t t = 10; t <= ms; t += 10) {
            out.push_back(t);
        }
        return out;
    }
};

} // namespace

// ============================================================================
// Probe A — the primary: SwapWeave on the live timer role
// ============================================================================

TEST_CASE("probe A: a hard swap leaves nothing of the old Timer's time in flight — and the "
          "ACTIVATED successor keeps one lattice of its own, with no wind") {
    Rig r;
    TimeForensics f;
    f.arm(r.bus);

    // No wind anywhere in this probe. Loading the service activates it.
    const loom::WeaveId old_service = r.load("zengine-timer", TIMER_SO, kTimerRole);
    r.ask_role(StartTimer{"probe.tick", 10, true});
    r.run_for(40);
    CHECK(r.seen.ready == 1);             // the incumbent announced, on its activation
    CHECK(r.seen.fired_of("probe.tick") == 4);
    CHECK(r.bus.pending() == 0);          // nothing queued to keep time
    const std::int64_t old_wakes = f.wakes_of(old_service);
    CHECK(old_wakes > 0);

    // The swap is HARD on purpose: the service declares zen.PrepareShutdown, so a graceful swap
    // would run the letter ceremony and the successor would inherit the schedule (TIMER-03).
    // This measures the letterless path -- what the SUBSTRATE does to a Timer's time when the
    // Timer goes away -- which no authored inheritance changes; the graceful path is the timer
    // suite's.
    const std::uint64_t corr = r.command(loom::SwapWeave{kTimerRole, "zengine-timer", TIMER_SO,
                                                         /*graceful=*/false});
    r.settle();

    const Seen::Answer* swapped = r.seen.find(corr);
    REQUIRE(swapped != nullptr);
    REQUIRE_MESSAGE(swapped->kind == 0, "swap refused: ", swapped->text);
    const loom::WeaveId successor{static_cast<std::uint64_t>(std::stoll(swapped->text))};
    CHECK(successor.value != old_service.value);

    // THE MEASURED HALF: nothing of the old Timer's time was in flight, so the bus refused
    // nothing -- no wake and no firing met a sender whose life had ended, or anything else. Its
    // claim went with its role, and the wait woke it no more.
    CHECK(f.refused() == 0);
    MESSAGE("post-swap refusals: sender_life=", f.refused_sender_life,
            " capability=", f.refused_capability, " no_target=", f.refused_no_target);
    const std::int64_t settled = f.wakes_of(successor);
    r.run_for(100);
    CHECK(f.wakes_of(old_service) == old_wakes);

    // THE PROMISE: the successor was activated by the control door on the way in and announced
    // (hello #2). A hard swap carried no schedule, so it stands with nothing and is woken by
    // nothing until asked.
    CHECK(r.seen.ready == 2);
    CHECK(f.wakes_of(successor) == settled);

    // ONE LATTICE, measured live: asked again through the role, it fires exactly once a rung --
    // a second schedule would fire twice -- and is woken once to learn the time and once a rung.
    const std::int64_t ticks = r.seen.fired_of("probe.tick");
    r.ask_role(StartTimer{"probe.tick", 10, true});
    r.run_for(60);
    CHECK(r.seen.fired_of("probe.tick") - ticks == 6);
    CHECK(f.wakes_of(successor) - settled == 1 + 6);
    CHECK(r.poke_int(successor, "wakes") == f.wakes_of(successor));
    MESSAGE("one lattice: ticks +", r.seen.fired_of("probe.tick") - ticks, " wakes +",
            f.wakes_of(successor) - settled);
}

// ============================================================================
// Probe B — the corollary: ReloadWeave is the succession that DOES work
// ============================================================================

TEST_CASE("probe B: a reload carries the schedule, and the reload's activation keeps it on one "
          "lattice -- nothing of the predecessor's time is in flight to inherit") {
    Rig r;
    TimeForensics f;
    f.arm(r.bus);

    const loom::WeaveId service = r.load("zengine-timer", TIMER_SO, kTimerRole);
    r.ask_role(StartTimer{"probe.tick", 10, true});
    r.run_for(40);
    CHECK(r.seen.ready == 1);
    REQUIRE(r.seen.fired_of("probe.tick") == 4);
    const std::int64_t wakes_before = r.poke_int(service, "wakes");
    CHECK(wakes_before > 0);

    // Reload in place. The measured substrate half: reload rebinds the new library behind the
    // SAME adapter and WeaveId, and the role is never vacated, so nothing the bus held could be
    // refused -- and between wakes it held nothing for the Timer.
    const std::uint64_t corr = r.command(loom::ReloadWeave{"zengine-timer", TIMER_SO});
    r.settle();

    const Seen::Answer* reloaded = r.seen.find(corr);
    REQUIRE(reloaded != nullptr);
    CHECK(reloaded->kind == 1);                           // Ack
    CHECK(r.kernel.weave_id("zengine-timer") == service); // SAME WeaveId
    CHECK(f.refused() == 0);                              // nothing died: no sender was lost

    // Liveness is NOT inherited -- it comes from the reload's own activation, which republished
    // TimerReady (hello #2) and claimed its own time.
    CHECK(r.seen.ready == 2);

    // State transplanted (TimerState rides the gate): the counters, and the standing timer.
    CHECK(r.poke_int(service, "wakes") >= wakes_before);
    CHECK(r.poke_int(service, "active") == 1);
    CHECK(r.poke_int(service, "inherited") == 1);

    // ONE LATTICE across the reload: the carried timer fires once a rung, never twice.
    const std::int64_t ticks = r.seen.fired_of("probe.tick");
    r.run_for(60);
    CHECK(r.seen.fired_of("probe.tick") - ticks == 6);
    MESSAGE("reload: ticks +", r.seen.fired_of("probe.tick") - ticks, ", hellos=", r.seen.ready);
}

// ============================================================================
// Probe C — double stimulus: no second lattice, whatever is re-delivered
// ============================================================================

TEST_CASE("probe C: neither a replayed wake nor a replayed activation can fork time — one "
          "lattice, whatever is re-delivered") {
    // Deterministic form: a 10 ms repeating timer on the host's wait over a clock that moves only
    // when told. The lattice is the instrument: a second schedule, or a wake believed, would put a
    // firing off it or twice on it.
    FakeRig baseline;
    baseline.bus.send_as(baseline.ear, baseline.service,
                         loom::Message(loom::to_value(StartTimer{"tick", 10, true}), baseline.ear,
                                       baseline.ear, 0));
    baseline.activate();
    baseline.run_until(60);
    CHECK(baseline.heard.fired_at == FakeRig::lattice_to(60));
    CHECK(baseline.bus.pending() == 0); // nothing queued to keep time

    // ---- (1) replaying the host's wait's word -----------------------------------
    //
    // The replay is the shape the host's wait sends, due, carrying a time far ahead -- sent by an
    // ordinary weave through the honest API, and sent AS THE TIMER ITSELF. Only the host's own
    // word, which has no sender, is time; these are nothing.
    FakeRig replay;
    replay.bus.send_as(replay.ear, replay.service,
                       loom::Message(loom::to_value(StartTimer{"tick", 10, true}), replay.ear,
                                     replay.ear, 0));
    replay.activate();
    replay.run_until(60);
    REQUIRE(replay.heard.fired_at == FakeRig::lattice_to(60));
    loom::host::Waited forged;
    forged.held = true;
    forged.due = true;
    forged.now_ms = 1000;
    replay.bus.send_as(replay.ear, replay.service,
                       loom::Message(loom::to_value(forged), replay.ear, replay.ear, 0));
    replay.bus.send_as(replay.service, replay.service,
                       loom::Message(loom::to_value(forged), replay.service, replay.service, 0));
    replay.run_until(120);
    // The replays were DELIVERED and established nothing: the lattice ran on, every rung once.
    CHECK(replay.heard.fired_at == FakeRig::lattice_to(120));

    // ---- (2) replaying the same activation -----------------------------------
    //
    // The cursor tells a second stimulus from a first: same sender, non-newer sequence is a
    // duplicate.
    FakeRig again;
    again.bus.send_as(again.ear, again.service,
                      loom::Message(loom::to_value(StartTimer{"tick", 10, true}), again.ear,
                                    again.ear, 0));
    again.activate();
    again.run_until(60);
    REQUIRE(again.heard.fired_at == FakeRig::lattice_to(60));
    const std::int64_t ready_before = again.heard.ready;
    again.activate(again.current_sequence()); // the very same activation, again
    again.run_until(90);
    CHECK(again.heard.fired_at == FakeRig::lattice_to(90)); // still ONE lattice
    CHECK(again.heard.ready == ready_before);               // and no second availability notice

    // A NEWER activation is a different matter: it re-announces, and the lattice still runs
    // once a rung -- an activation is not a second schedule.
    again.activate();
    again.run_until(120);
    CHECK(again.heard.fired_at == FakeRig::lattice_to(120));
    CHECK(again.heard.ready == ready_before + 1); // it did republish
}

// ============================================================================
// Probe D — late-load deafness
// ============================================================================

TEST_CASE("probe D: a consumer loaded LONG AFTER time started begins anyway — its own "
          "activation is its first breath, and nothing was republished to rescue it") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);

    r.run_for(60); // time has been running for a while already
    CHECK(r.seen.ready == 1);
    const std::int64_t hellos_before = r.seen.ready;

    // The latecomer, loaded after TimerReady was spent: the world, whose own activation must make
    // it ask for its tick, with no new TimerReady to rescue it. The pump ends on the evidence, the
    // world's first visual, which only its tick can cause; its bound is ten tick periods on the
    // host's clock, which the load's own turns do not spend.
    const std::uint64_t corr = r.command(
        loom::LoadWeave{"snake-world-v1", WORLD_V1_SO, {zengine::snake::kWorldRole}});
    const std::int64_t bound = 10 * zengine::snake::kTickMs;
    r.run_until_visual(bound);
    const Seen::Answer* loaded = r.seen.find(corr);
    REQUIRE(loaded != nullptr);
    REQUIRE_MESSAGE(loaded->kind == 0, "late load refused: ", loaded->text);
    const loom::WeaveId world_so{static_cast<std::uint64_t>(std::stoll(loaded->text))};

    // IT BEGAN. Its own activation made it ask, the ticks are positive, and the
    // world moved — visuals arrived at the witness with nobody sending a
    // SnakeTick.
    CHECK_MESSAGE(r.poke_int(world_so, "ticks") > 0, "the late world never ticked in ", bound,
                  " ms");
    CHECK_MESSAGE(!r.seen.visuals.empty(), "the world never drew in ", bound, " ms");

    // AND NOTHING WAS REPUBLISHED TO RESCUE IT. This is the half that makes the
    // proof about activation rather than about a lucky notice: the Timer's
    // TimerReady count did not move, so the latecomer was not saved by hearing
    // an availability broadcast — it was saved by being told it was live.
    CHECK(r.seen.ready == hellos_before);
    (void)timer_so;
}
