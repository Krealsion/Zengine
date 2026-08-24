// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Trust-Gate liveness probes — WHAT THEY MEASURED, AND WHAT THEY NOW
// WITNESS. (Born Stage 5 of the audit phase, 2026-07-26; ratified onto main by
// the R1 repair phase, 2026-07-27; FLIPPED by R2A-2, 2026-07-28.)
//
// These four cases were written as descriptions, not endorsements. They pinned
// what the substrate measurably DID, including where that was unwanted — probe
// A asserted that swapping the timer service KILLED the beat chain — and the
// file said, in as many words, that when the open lifecycle question was
// answered probe A was "exactly the case that FLIPS: it becomes the guard on
// the new promise. A red probe A after an R2 change is expected and good;
// rewrite it to the new truth rather than deleting it, and keep the measured
// half (which refusal, whose sender)."
//
// R2A-2 answered it. Each probe below is now the witness to the earned promise,
// and each keeps its measured half — because the substrate behaviour has not
// changed at all. What changed is the mechanism built on top of it:
//
//   THE LAW: every successfully activated Timer incarnation establishes exactly
//   ONE beat chain. A new activation owns a new chain; stale, duplicate,
//   replayed, inherited or foreign Drives cannot establish another.
//
// Probe map — old truth -> new proof:
//   A. SwapWeave on the live timer role
//        was: the chain dies (CapabilityDenied, sender-death); only a fresh
//             root wind heals it.
//        now: the old chain STILL dies exactly that way — the measurement is
//             preserved and re-asserted — and the successor's own activation
//             authors a new one. No wind, and exactly one chain at the end.
//   B. ReloadWeave of the live service
//        was: the chain survives because the WeaveId does, and the reloaded
//             incarnation re-announces on the beat it rides through.
//        now: the predecessor's parked Drive is INERT (a new instance begins
//             unactivated, and the serial is not one it expects); the reload's
//             own activation publishes TimerReady and authors a fresh chain.
//             The test no longer praises accidental inheritance.
//   C. Duplicate / replay
//        was: a stray second Drive seeds a permanent, conserved second chain.
//        now: neither a replayed already-consumed Drive from the REAL stamped
//             Timer sender, nor a replayed activation, can fork time.
//   D. Late load
//        was: a consumer loaded after the wind never hears TimerReady and is
//             permanently deaf.
//        now: its own activation makes it ask, and it runs — with no new
//             TimerReady published to rescue it.
//
// The chain never quiesces while alive, so every open-ended pump carries a
// lever: a stopwatch timer (stops the bus when the chain is alive to fire it)
// and/or a delivered-Drive watchdog via a bus observer. A pump that returns
// with an empty queue and no watchdog trip IS a chain-death observation.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include "snake/vocabulary.hpp"

#include "lifecycle_door.hpp"

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
        /// HOW MANY DRIVES HAD BEEN DELIVERED WHEN THIS ANSWER ARRIVED.
        ///
        /// Stamped here because the two numbers the one-chain probes compare have
        /// to be read at the SAME point in the dispatch stream, and a poke's
        /// answer is the only shared instant available: the reply carries a
        /// `beats` the service computed in its own handler, and a host statement
        /// after the pump reads a Drive count from minutes of bus time later.
        /// Reading them apart made the comparison a race — see `drives_at_poke`.
        std::int64_t drives_at = 0;
    };
    std::vector<Answer> answers;
    std::vector<std::string> fired;
    std::int64_t ready = 0;
    std::vector<zengine::snake::SnakeVisual> visuals;
    loom::Switchboard* bus = nullptr;
    std::string stop_id;
    std::int64_t stop_count = 1;
    std::int64_t stop_seen = 0;
    /// The rig's live delivered-Drive counter, so an answer can stamp itself.
    const std::int64_t* drives_now = nullptr;

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
        seen_->answers.push_back(
            Seen::Answer{mail.correlation(), kind, std::move(text),
                         seen_->drives_now == nullptr ? 0 : *seen_->drives_now});
    }
    Seen* seen_;
};

/// Refusal forensics + chain watchdog, wired as a bus observer per probe.
struct DriveForensics {
    std::int64_t delivered = 0;
    std::int64_t refused_capability = 0;
    std::int64_t refused_no_target = 0;
    /// The author's life ended before delivery. Distinct from
    /// `refused_capability` on purpose: a vanished sender has no grant to check,
    /// so folding the two together names the authorization term for an event
    /// that is about a life. Same event either way; only one of the two names
    /// sends an operator hunting a grant.
    std::int64_t refused_sender_life = 0;
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
};

struct Rig {
    loom::Switchboard bus;
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    Seen seen;
    loom::WeaveId witness = mount_witness();
    std::uint64_t next_corr = 1;

    Rig() {
        watch_drives();
        seen.drives_now = &drives; // so every answer can stamp the Drive count
    }

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

    /// The beat watchdog — the one pump lever that works on both sides of the
    /// clock's existence. Before a timer is loaded no Drive is ever delivered,
    /// so it never trips and the pump drains; once a chain is alive it bounds
    /// an otherwise endless pump. Required, because loading the service is
    /// itself what starts time (TIMER-02).
    std::int64_t drives = 0;
    std::int64_t stop_after_drives = -1;

    void watch_drives() {
        bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.schema_name == "Drive" && ev.kind == loom::EventKind::Delivered) {
                ++drives;
                if (stop_after_drives >= 0 && drives >= stop_after_drives) {
                    bus.stop();
                }
            }
        });
    }

    void pump_beats(std::int64_t n) {
        stop_after_drives = drives + n;
        bus.pump();
        stop_after_drives = -1;
    }

    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const std::uint64_t corr = command(loom::LoadWeave{name, path, role});
        pump_beats(6); // drains when no clock exists; bounded once one does
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        REQUIRE_MESSAGE(a->kind == 0, "load refused: ", a->text);
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(a->text))};
    }

    /// Run the live chain ~ms of real time: stopwatch one-shot as the lever.
    void pump_for(loom::WeaveId service, std::int64_t ms) {
        seen.stop_id = "probe.stopwatch";
        seen.stop_count = 1;
        seen.stop_seen = 0;
        ask(service, StartTimer{"probe.stopwatch", ms, false});
        bus.pump();
        seen.stop_id.clear();
    }

    /// DELIVERED DRIVES AS OF THE LAST `poke_int` ANSWER, and the reason it exists
    /// rather than reading a counter after the pump.
    ///
    /// `poke_int` answers EARLY inside a ~25 ms pump and then keeps beating until
    /// the stopwatch stops the bus, so a Drive count read after it returns is the
    /// count at the END of that window while `beats` is the value from the START.
    /// Differencing two such pairs leaves `tail_b - tail_a` -- the difference of
    /// two independently jittering tails, each several beats long on a loaded
    /// host -- and no fixed tolerance is honest about that. Measured: on a
    /// contended runner the pair drifted by 2 where the probes allow 1.
    ///
    /// Stamped at the answer's delivery, the tolerance becomes DERIVABLE instead.
    /// The service computes `beats` while handling the poke; dispatch is
    /// single-threaded FIFO and a beat seeds its one successor at the tail, so at
    /// most the single parked Drive can slip between that handler and the
    /// answer's arrival. Each reading is therefore off by at most one, in one
    /// direction, and the probes' `+/- 1` is what the bus guarantees rather than
    /// what a window happened to allow.
    std::int64_t drives_at_poke = 0;

    std::int64_t poke_int(loom::WeaveId service, loom::WeaveId target, const char* field) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(loom::PokeRead{field}), loom::WeaveId{},
                                       witness, corr));
        pump_for(service, 25);
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        REQUIRE(a->kind == 0);
        drives_at_poke = a->drives_at;
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

/// What the fake lane's ear heard.
struct FakeHeard {
    std::int64_t ready = 0;
};

struct FakeEarState {
    std::int64_t count = 0;
    ZEN_SHAPE(FakeEarState, 1, ZEN_FIELD(count));
};
class FakeEar : public loom::WeaveBase<FakeEar, FakeEarState, loom::Accept<TimerReady>,
                                       loom::Emit<>> {
public:
    explicit FakeEar(FakeHeard& h) : h_(&h) {}
    void on(const TimerReady&, loom::Mail&) { ++h_->ready; }

private:
    FakeHeard* h_;
};

/// A stand-in for the Loom's control door: the identity an activation key is
/// made of. The rig speaks AS it, so those sends carry a real bus-stamped
/// sender rather than being root injections.
struct FakeDoorState {
    std::int64_t nothing = 0;
    ZEN_SHAPE(FakeDoorState, 1, ZEN_FIELD(nothing));
};
using zengine::testing::TestDoor;

struct FakeRig {
    loom::Switchboard bus;
    FakeHooks hooks;
    FakeHeard heard;
    loom::WeaveId service{};
    loom::WeaveId ear{};
    loom::WeaveId door{};
    std::int64_t next_sequence = 1;

    FakeRig() {
        hooks.bus = &bus;
        auto weave = std::make_unique<FakeService>(FakeClock{&hooks});
        FakeService* raw = weave.get();
        loom::Grant grant = loom::emit_default_grant(*raw);
        loom::allow_poke_answers(grant);
        service = bus.register_weave(std::move(weave), std::move(grant), kTimerRole);
        raw->zen_set_self(service);
        ear = loom::mount<FakeEar>(bus, heard);
        door = zengine::testing::mount_door(bus);
    }

    /// The activation key's two halves, as a consumer of the wire would see it.
    std::string door_text() const { return std::to_string(door.value); }
    std::int64_t current_sequence() const { return next_sequence - 1; }

    void activate(std::int64_t sequence) {
        zengine::testing::order_activation(bus, door, service, sequence);
    }
    void activate() { activate(next_sequence++); }

    void run_beats(std::int64_t n, bool start = false) {
        hooks.stop_after = hooks.beats + n;
        if (start) {
            activate();
        }
        bus.pump();
        hooks.stop_after = -1;
    }
};

} // namespace

// ============================================================================
// Probe A — the primary: SwapWeave on the live timer role
// ============================================================================

TEST_CASE("probe A: the old chain still dies honestly on a swap — and the ACTIVATED "
          "successor authors a new one, with no wind") {
    Rig r;
    DriveForensics f;
    f.arm(r.bus);

    // No wind anywhere in this probe. Loading the service activates it.
    const loom::WeaveId old_service = r.load("zengine-timer", TIMER_SO, kTimerRole);
    r.pump_for(old_service, 40);
    CHECK(r.seen.ready == 1);   // the incumbent announced, on its activation
    CHECK(r.bus.pending() > 0); // the chain is alive and parked
    const std::int64_t beats_before = r.poke_int(old_service, old_service, "beats");
    CHECK(beats_before > 0);

    // The swap. HARD on purpose, and that is a real choice rather than the only
    // available one: the service declares zen.PrepareShutdown, so a graceful swap
    // would run the letter ceremony and the successor would inherit the standing
    // schedule (TIMER-03). This probe deliberately measures the
    // letterless path — what the SUBSTRATE does to an in-flight beat when its
    // sender goes away — which is exactly the fact the audit recorded and which
    // no amount of authored inheritance changes. (The graceful path, and the
    // continuity it buys, is the timer suite's.)
    const std::uint64_t corr = r.command(loom::SwapWeave{kTimerRole, "zengine-timer", TIMER_SO,
                                                         /*graceful=*/false});
    r.pump_beats(40);

    const Seen::Answer* swapped = r.seen.find(corr);
    REQUIRE(swapped != nullptr);
    REQUIRE_MESSAGE(swapped->kind == 0, "swap refused: ", swapped->text);
    const loom::WeaveId successor{static_cast<std::uint64_t>(std::stoll(swapped->text))};
    CHECK(successor.value != old_service.value);

    // THE MEASURED HALF, PRESERVED. The substrate's BEHAVIOUR has not changed and
    // this is still exactly how the old chain ends: the incumbent's parked re-seed
    // fails at delivery because its SENDER is gone — sender-death, not role vacancy
    // (the successor already held the role by then). Keeping this assertion is the
    // point of rewriting rather than replacing the probe.
    //
    // The reason names the LIFE and not the grant, which is what this comment has
    // always said the event is. `CapabilityDenied` would be the right outcome down
    // the wrong road — a vanished sender has no grant to check, so the
    // authorization term is the one that fails — and an operator reading that
    // reason would go hunting a grant.
    CHECK(f.refused_sender_life == 1);
    CHECK(f.refused_capability == 0);
    CHECK(f.refused_no_target == 0);
    CHECK(f.refused_other == 0);
    MESSAGE("post-swap drive refusals: sender_life=", f.refused_sender_life,
            " capability=", f.refused_capability, " no_target=", f.refused_no_target);

    // THE NEW PROMISE. The old chain died — and the bus did NOT drain, because
    // the successor was activated by the control door on the way in and
    // authored a chain of its own. It announced (hello #2) and it is beating.
    CHECK(r.bus.pending() > 0);
    CHECK(r.seen.ready == 2);
    const std::int64_t successor_beats = r.poke_int(successor, successor, "beats");
    CHECK(successor_beats > 0);

    // EXACTLY ONE CHAIN, measured live. Every VALID delivered Drive produces
    // exactly one beat, and an invalid one produces none — so over a window,
    // "Drives delivered" and "beats lived" move together iff there is a single
    // chain. A second chain would deliver roughly twice as many Drives as it
    // produced beats, because one of each pair would be refused ownership.
    // (Probe C proves the same law deterministically on the fake clock; this is
    // its live corollary, tolerant by one for the beat in flight at each read --
    // and both numbers are read at the same instant, which is what makes that
    // one a bound rather than a hope. See Rig::drives_at_poke.)
    const std::int64_t beats_a = r.poke_int(successor, successor, "beats");
    const std::int64_t drives_a = r.drives_at_poke; // the SAME instant as beats_a
    r.pump_for(successor, 60);
    const std::int64_t beats_b = r.poke_int(successor, successor, "beats");
    const std::int64_t drives_b = r.drives_at_poke;
    CHECK(beats_b > beats_a); // time is genuinely running, unattended
    const std::int64_t beat_delta = beats_b - beats_a;
    const std::int64_t drive_delta = drives_b - drives_a;
    REQUIRE(beat_delta >= 3); // enough window for the comparison to mean something
    CHECK(drive_delta <= beat_delta + 1);
    CHECK(drive_delta >= beat_delta - 1);
    MESSAGE("one chain: beats +", beat_delta, " drives delivered +", drive_delta);
}

// ============================================================================
// Probe B — the corollary: ReloadWeave is the succession that DOES work
// ============================================================================

TEST_CASE("probe B: a reload does not INHERIT the old chain — the predecessor's parked "
          "beat is inert, and the reload's activation authors a fresh one") {
    Rig r;
    DriveForensics f;
    f.arm(r.bus);

    const loom::WeaveId service = r.load("zengine-timer", TIMER_SO, kTimerRole);
    r.pump_for(service, 40);
    CHECK(r.seen.ready == 1);
    const std::int64_t beats_before = r.poke_int(service, service, "beats");
    CHECK(beats_before > 0);
    const std::int64_t drives_before = f.delivered;

    // Reload in place. The measured substrate half is unchanged and still
    // true: reload rebinds the new library behind the SAME adapter/WeaveId, so
    // the predecessor's parked Drive is still DELIVERABLE — its sender lookup
    // keeps succeeding and the role is never vacated. What the activation law
    // adds is that being deliverable is not enough to own anything (TIMER-01).
    const std::uint64_t corr = r.command(loom::ReloadWeave{"zengine-timer", TIMER_SO});
    r.pump_beats(8);

    const Seen::Answer* reloaded = r.seen.find(corr);
    REQUIRE(reloaded != nullptr);
    CHECK(reloaded->kind == 1);                    // Ack
    CHECK(r.kernel.weave_id("zengine-timer") == service); // SAME WeaveId
    CHECK(f.refused_capability == 0);              // nothing died: no sender was lost
    CHECK(f.refused_no_target == 0);

    // THE FLIP. The old queued Drive reached the new instance and did NOTHING:
    // a fresh incarnation begins unactivated, and even once activated the
    // predecessor's serial is not the one this chain expects. Liveness here is
    // NOT inherited — it comes from the reload's own activation, which
    // republished TimerReady (hello #2) and seeded serial 0.
    CHECK(r.seen.ready == 2);
    CHECK(r.bus.pending() > 0); // and a chain is alive and parked

    // State transplanted (TimerState rides the gate); the chain did not (the
    // cursor and serial are plain members, by design).
    const std::int64_t beats_after = r.poke_int(service, service, "beats");
    CHECK(beats_after > beats_before);

    // EXACTLY ONE CHAIN across the reload — the inherited beat did not become a
    // second one. Same live method as probe A: valid Drives and beats move
    // together; a surviving old chain plus a new one would not.
    const std::int64_t beats_a = r.poke_int(service, service, "beats");
    const std::int64_t drives_a = r.drives_at_poke; // the SAME instant as beats_a
    r.pump_for(service, 60);
    const std::int64_t beat_delta = r.poke_int(service, service, "beats") - beats_a;
    const std::int64_t drive_delta = r.drives_at_poke - drives_a;
    REQUIRE(beat_delta >= 3);
    CHECK(drive_delta <= beat_delta + 1);
    CHECK(drive_delta >= beat_delta - 1);
    MESSAGE("reload: beats ", beats_before, " -> ", beats_after, ", hellos=", r.seen.ready,
            ", drives since load=", f.delivered - drives_before);
}

// ============================================================================
// Probe C — double wind: conserved second chain, not "a harmless extra beat"
// ============================================================================

TEST_CASE("probe C: neither a replayed Drive nor a replayed activation can fork time — "
          "one chain, whatever is re-delivered") {
    // Deterministic fake-clock form. With no timers standing, every beat naps
    // kBeatCapMs, and a parked pump leaves exactly the in-flight Drives in the
    // queue: pending() IS the chain count. That is the same instrument the old
    // probe used to MEASURE a second chain being created; here it is what
    // proves one cannot be.
    FakeRig baseline;
    baseline.run_beats(6, /*start=*/true);
    CHECK(baseline.hooks.beats == 6);
    CHECK(baseline.bus.pending() == 1); // one chain: one parked Drive

    // ---- (1) replaying a valid, already-consumed Drive -----------------------
    //
    // The replay is sent AS THE TIMER ITSELF, with the live activation key and
    // a serial the chain really did consume. A root or alien sender being
    // ignored is necessary but NOT sufficient proof — this is the hostile frame
    // that actually matters, and it is fully sayable through the honest API.
    FakeRig replay;
    replay.run_beats(6, /*start=*/true);
    REQUIRE(replay.bus.pending() == 1);
    const std::int64_t consumed_serial = replay.hooks.beats - 1; // one already spent
    replay.bus.send_as(replay.service, replay.service,
                       loom::Message(loom::to_value(Drive{replay.door_text(),
                                                          replay.current_sequence(),
                                                          consumed_serial}),
                                     replay.service, replay.service, 0));
    replay.run_beats(6);
    // The replay was DELIVERED (the role is held, the sender is real) and it
    // established nothing: still one parked Drive, and the beat count advanced
    // only by the chain's own beats.
    CHECK(replay.bus.pending() == 1);
    CHECK(replay.hooks.beats == 12);

    // ---- (2) replaying the same activation -----------------------------------
    //
    // The old mechanism had no way to tell a second stimulus from a first. The
    // cursor does: same sender, non-newer sequence is a duplicate.
    FakeRig again;
    again.run_beats(6, /*start=*/true);
    REQUIRE(again.bus.pending() == 1);
    const std::int64_t ready_before = again.heard.ready;
    again.activate(again.current_sequence()); // the very same activation, again
    again.run_beats(6);
    CHECK(again.bus.pending() == 1);          // still ONE chain
    CHECK(again.heard.ready == ready_before); // and no second availability notice

    // A NEWER activation is a different matter: it REPLACES the chain rather
    // than adding one. Still exactly one parked Drive afterwards.
    again.activate();
    again.run_beats(6);
    CHECK(again.bus.pending() == 1);
    CHECK(again.heard.ready == ready_before + 1); // it did republish
}

// ============================================================================
// Probe D — late-load deafness
// ============================================================================

TEST_CASE("probe D: a consumer loaded LONG AFTER time started begins anyway — its own "
          "activation is its first breath, and nothing was republished to rescue it") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    (void)r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);

    r.pump_for(timer_so, 60); // time has been running for a while already
    CHECK(r.seen.ready == 1);
    const std::int64_t hellos_before = r.seen.ready;

    // The latecomer. Under the old mechanism this weave was permanently deaf:
    // TimerReady was the only first breath and it had been spent before this
    // weave existed, so it never asked for a tick timer and the world never
    // moved. Its ticks stayed 0 forever, measured.
    const std::uint64_t corr = r.command(loom::LoadWeave{"snake-clock", SNAKE_CLOCK_SO, ""});
    r.pump_for(timer_so, 400); // load answers within the first beats; then ~3 tick periods
    const Seen::Answer* loaded = r.seen.find(corr);
    REQUIRE(loaded != nullptr);
    REQUIRE_MESSAGE(loaded->kind == 0, "late load refused: ", loaded->text);
    const loom::WeaveId clock_so{static_cast<std::uint64_t>(std::stoll(loaded->text))};

    // IT BEGAN. Its own activation made it ask, the ticks are positive, and the
    // world moved — visuals arrived at the witness with nobody sending a
    // SnakeTick.
    CHECK(r.poke_int(timer_so, clock_so, "ticks") > 0);
    CHECK_FALSE(r.seen.visuals.empty());

    // AND NOTHING WAS REPUBLISHED TO RESCUE IT. This is the half that makes the
    // proof about activation rather than about a lucky notice: the Timer's
    // TimerReady count did not move, so the latecomer was not saved by hearing
    // an availability broadcast — it was saved by being told it was live.
    CHECK(r.seen.ready == hellos_before);
}
