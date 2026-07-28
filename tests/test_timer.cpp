// The Timer suite — the Timer package V1, proven headless.
//
// Four tiers, deliberately ordered:
//   1. CONTRACT pins — the ZEN_SHAPE spellings in timer/vocabulary.hpp derive
//      schemas content-id-identical to the phase prompt's locked shapes (and
//      the three named additions are frozen the same way). A drift is a red
//      test, not an opinion.
//   2. THE SERVICE over a fake clock, through a real bus — every schedule
//      (one-shot, repeat, upsert, clamps, cancel, role delivery, succession,
//      vacancy, requester death) pinned deterministically: the fake clock
//      advances virtual time and counts beats, so not one tier-2 assertion
//      waits on a wall clock.
//   3. THE REAL LIBRARY through the real Kernel — zengine-timer.so loads into
//      its role, the control door ACTIVATES it, and the chain it authors from
//      that runs on a REAL monotonic clock. Nothing winds anything. The
//      suite's stop levers are a one-shot STOPWATCH timer it asks for itself
//      and a delivered-Drive budget: the package is its own test harness.
//   4. THE MIGRATION chains — the world ticks, the input weave polls, and the
//      skin services its medium with NOBODY pumping them: the three
//      obligations the host used to carry, each proven moved into an ask.
//
// The beat chain never quiesces by design, so every pump here carries a stop
// plan (a fake-clock beat budget, a listener stop-on-fire, or the stopwatch).
// A stopped pump PARKS the queue mid-chain — the tail (that beat's firings
// and the next Drive) delivers on the next pump. The tier-2 cases are timed
// with that in mind, and two of them pin the visible consequence honestly:
// an ask or cancel arriving behind a parked beat takes effect one firing
// late, because the in-flight beat already spoke.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include "input/vocabulary.hpp"
#include "snake/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
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

// ---- tier 2 rig: the fake clock and its ears ---------------------------------

/// The test's hand inside the beat: virtual now, beat counting, and a bus
/// stop after a beat budget (the backstop that makes "nothing fires"
/// provable without waiting for it not to happen).
struct FakeHooks {
    std::int64_t now = 0;
    std::int64_t beats = 0;
    std::int64_t stop_after = -1; ///< stop the bus once beats reaches this
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

/// What the ears heard, shared with the test. One instance can serve several
/// ears — the ids and stamps say who was addressed.
struct Heard {
    std::vector<std::string> fired;     ///< TimerFired ids, in arrival order
    std::vector<std::int64_t> fired_at; ///< virtual now at each arrival
    std::int64_t ready = 0;             ///< TimerReady hellos
    const FakeHooks* hooks = nullptr;
    loom::Switchboard* bus = nullptr;
    std::string stop_on_id; ///< stop the bus when this id arrives (if set)
};

struct EarState {
    std::int64_t count = 0;
    ZEN_SHAPE(EarState, 1, ZEN_FIELD(count));
};

/// An ordinary timer consumer. Its Emit list is what lets the test speak AS
/// it: send_as authorizes against the mounted grant, and mount() derives
/// that grant from exactly this list.
///
/// `Drive` is in its Emit list ON PURPOSE, and it is not a convenience: the
/// hostile frames this suite must forge — a beat from a FOREIGN stamped sender,
/// a beat carrying someone else's activation key — have to be SAYABLE through
/// the honest API, or the pin would be testing the grant model rather than the
/// service's ownership check. An ordinary weave really can emit a Drive here;
/// what it cannot do is make one count.
class Ear : public loom::WeaveBase<Ear, EarState, loom::Accept<TimerFired, TimerReady>,
                                   loom::Emit<StartTimer, StartRoleTimer, CancelTimer,
                                              CancelAllMyTimers, Drive>> {
public:
    explicit Ear(Heard& heard) : heard_(&heard) {}
    void on(const TimerFired& f, loom::Mail&) {
        ++state_.count;
        heard_->fired.push_back(f.id);
        heard_->fired_at.push_back(heard_->hooks != nullptr ? heard_->hooks->now : -1);
        if (!heard_->stop_on_id.empty() && f.id == heard_->stop_on_id) {
            heard_->bus->stop();
        }
    }
    void on(const TimerReady&, loom::Mail&) { ++heard_->ready; }

private:
    Heard* heard_;
};

/// mount(), plus a role binding (the sugar in zen/weave.hpp has no role
/// parameter — role-bound weaves normally arrive through the kernel; here a
/// mounted ear must HOLD a role so role-addressed beats have a target).
template <class W, class... Args>
loom::WeaveId mount_role(loom::Switchboard& bus, std::string role, Args&&... args) {
    auto weave = std::make_unique<W>(std::forward<Args>(args)...);
    W* raw = weave.get();
    loom::Grant grant = loom::emit_default_grant(*raw);
    loom::allow_poke_answers(grant);
    const loom::WeaveId id =
        bus.register_weave(std::move(weave), std::move(grant), std::move(role));
    raw->zen_set_self(id);
    return id;
}

/// A stand-in for the Loom's control door: the thing whose stamped sender and
/// sequence make up an activation key. It sends nothing on its own — the rig
/// speaks AS it — but mounting it is what gives those sends a real, gated,
/// bus-stamped identity instead of a root injection.
struct DoorState {
    std::int64_t nothing = 0;
    ZEN_SHAPE(DoorState, 1, ZEN_FIELD(nothing));
};
class Door : public loom::WeaveBase<Door, DoorState, loom::Accept<>,
                                    loom::Emit<loom::Activated>> {};

/// The tier-2 world: a real bus, the service on a fake clock, one ear, and a
/// stand-in door to activate from.
struct FakeRig {
    loom::Switchboard bus;
    FakeHooks hooks;
    Heard heard;
    loom::WeaveId service{};
    loom::WeaveId ear{};
    loom::WeaveId door{};
    loom::WeaveId other_door{}; ///< a SECOND lifecycle operator: a different lineage
    std::int64_t next_sequence = 1;

    FakeRig() {
        hooks.bus = &bus;
        heard.hooks = &hooks;
        heard.bus = &bus;
        // Into its ROLE — the beat is role-addressed, exactly as the .so's is.
        service = mount_role<FakeService>(bus, kTimerRole, FakeClock{&hooks});
        ear = loom::mount<Ear>(bus, heard);
        door = loom::mount<Door>(bus);
        other_door = loom::mount<Door>(bus);
    }

    /// Speak AS a weave (stamped sender, authorized against ITS grant) — how
    /// every ask reaches the service in this tier.
    template <class T>
    void ask_as(loom::WeaveId who, const T& msg) {
        bus.send_as(who, service, loom::Message(loom::to_value(msg), who, who, 0));
    }

    /// Activate the service the way the control door does: a directed,
    /// stamped `zen.Activated` carrying the next sequence of this lineage.
    /// This — not a wind — is what starts time.
    void activate(std::int64_t sequence) {
        activate_as(door, sequence);
    }
    void activate() { activate(next_sequence++); }

    /// Activate from an arbitrary stamped sender — how a "different lineage"
    /// is expressed, and how an invalid one is forged.
    void activate_as(loom::WeaveId who, std::int64_t sequence) {
        bus.send_as(who, service,
                    loom::Message(loom::to_value(loom::Activated{sequence}), who, who, 0));
    }

    /// The activation key's two halves, as they travel on a Drive.
    std::string door_text() const { return std::to_string(door.value); }
    std::int64_t current_sequence() const { return next_sequence - 1; }

    /// Send a Drive AS `who`. The forged-frame door: everything a hostile or
    /// confused participant could actually put on the wire.
    void drive_as(loom::WeaveId who, const std::string& sender_text, std::int64_t sequence,
                  std::int64_t serial) {
        bus.send_as(who, service,
                    loom::Message(loom::to_value(Drive{sender_text, sequence, serial}), who, who,
                                  0));
    }

    /// Start (or resume) the chain and let it run at most `n` more beats
    /// (fewer if a stop_on_id fires first). The interrupted beat's tail
    /// parks; the next call resumes it — the host's own stop/quit mechanics.
    void run_beats(std::int64_t n, bool start = false) {
        hooks.stop_after = hooks.beats + n;
        if (start) {
            activate();
        }
        bus.pump();
        hooks.stop_after = -1;
    }
};

// ---- tiers 3/4 rig: the real kernel, the real clock ---------------------------

/// What the kernel-lane witness has seen: the standard answers, plus
/// everything time makes move in these lanes.
struct Seen {
    struct Answer {
        std::uint64_t corr = 0;
        int kind = 0; // 0 Result, 1 Ack, 2 Refused
        std::string text;
    };
    std::vector<Answer> answers;
    std::vector<std::string> fired;
    std::int64_t ready = 0;
    std::int64_t hellos = 0; ///< SurfaceReady (the skin lane)
    std::vector<zengine::snake::SnakeVisual> visuals;
    loom::Switchboard* bus = nullptr;
    std::string stop_id;           ///< stop once this TimerFired id has arrived...
    std::int64_t stop_count = 1;   ///< ...this many times
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

/// The lane's hand and ears: holds the manager reach, hears the standard
/// answers, and accepts everything these lanes watch move. The REAL clock
/// offers no test hook, so the witness's stop lever is a timer it asks for
/// itself — the stopwatch.
class Witness
    : public loom::WeaveBase<Witness, WitnessState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused, TimerFired,
                                          TimerReady, zengine::surface::SurfaceReady,
                                          zengine::snake::SnakeVisual>,
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
    void on(const zengine::surface::SurfaceReady&, loom::Mail&) { ++seen_->hellos; }
    void on(const zengine::snake::SnakeVisual& v, loom::Mail&) { seen_->visuals.push_back(v); }

private:
    void note(loom::Mail& mail, int kind, std::string text) {
        ++state_.noted;
        seen_->answers.push_back(Seen::Answer{mail.correlation(), kind, std::move(text)});
    }
    Seen* seen_;
};

struct Rig {
    loom::Switchboard bus;
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    Seen seen;
    loom::WeaveId witness = mount_witness();
    std::uint64_t next_corr = 1;

    Rig() { watch_drives(); }

    loom::WeaveId mount_witness() {
        seen.bus = &bus;
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
        reach.allow_to_any(StartTimer::zen_name, StartTimer::zen_version);
        reach.allow_to_any(StartRoleTimer::zen_name, StartRoleTimer::zen_version);
        reach.allow_to_any(CancelTimer::zen_name, CancelTimer::zen_version);
        reach.allow_to_any(CancelAllMyTimers::zen_name, CancelAllMyTimers::zen_version);
        return loom::mount_granted<Witness>(bus, std::move(reach), seen);
    }

    /// The beat watchdog: the one pump lever that works on BOTH sides of the
    /// clock's existence. Before any timer is loaded no Drive is ever
    /// delivered, so it never trips and the pump simply drains; once a chain is
    /// alive it bounds an otherwise endless pump. Since R2A-2 that dual nature
    /// is required rather than convenient — loading the timer service is what
    /// starts time, so even `load()` runs under a live chain from its own
    /// second half onward.
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
        const std::uint64_t corr = next_corr++;
        bus.send_as(witness, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{name, path, role}), witness,
                                  witness, corr));
        // Bounded either way: drains when no clock exists yet, and stops after a
        // few beats when loading the timer itself has just started one.
        pump_beats(6);
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        REQUIRE_MESSAGE(a->kind == 0, "load refused: ", a->text);
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(a->text))};
    }

    /// Ask the service for something AS the witness (stamped + authorized).
    template <class T>
    void ask(loom::WeaveId service, const T& msg) {
        bus.send_as(witness, service, loom::Message(loom::to_value(msg), witness, witness, 0));
    }

    /// Run the LIVE chain for about `ms` of real time: upsert the stopwatch
    /// one-shot, pump, and return when it fires. Every post-wind pump in
    /// these lanes goes through here — the chain never quiesces on its own.
    void pump_for(loom::WeaveId service, std::int64_t ms) {
        seen.stop_id = "suite.stopwatch";
        seen.stop_count = 1;
        seen.stop_seen = 0;
        ask(service, StartTimer{"suite.stopwatch", ms, false});
        bus.pump();
        seen.stop_id.clear();
    }

    /// Poke under a live chain: the answer arrives within the first beats;
    /// the stopwatch bounds the pump.
    template <class Poke>
    Seen::Answer poke_timed(loom::WeaveId service, loom::WeaveId target, const Poke& p) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(p), loom::WeaveId{}, witness, corr));
        pump_for(service, 25);
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        return *a;
    }
};

} // namespace

// ============================================================================
// Tier 1 — the locked contract, pinned by content-id
// ============================================================================

TEST_CASE("contract: ZEN_SHAPE spellings derive the locked schemas exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;

    // The prompt's V1 shapes, exactly as locked.
    CHECK(schema_of<StartTimer>()->content_id() == SchemaBuilder("StartTimer", 1)
                                                       .field("id", Kind::Text)
                                                       .field("delay_ms", Kind::Int)
                                                       .field("repeat", Kind::Bool)
                                                       .build()
                                                       ->content_id());
    CHECK(schema_of<CancelTimer>()->content_id() == SchemaBuilder("CancelTimer", 1)
                                                        .field("id", Kind::Text)
                                                        .build()
                                                        ->content_id());
    CHECK(schema_of<CancelAllMyTimers>()->content_id() ==
          SchemaBuilder("CancelAllMyTimers", 1).build()->content_id());
    CHECK(schema_of<TimerFired>()->content_id() == SchemaBuilder("TimerFired", 1)
                                                       .field("id", Kind::Text)
                                                       .build()
                                                       ->content_id());
    // The named additions are frozen the same way the locked four are.
    CHECK(schema_of<StartRoleTimer>()->content_id() == SchemaBuilder("StartRoleTimer", 1)
                                                           .field("id", Kind::Text)
                                                           .field("delay_ms", Kind::Int)
                                                           .field("repeat", Kind::Bool)
                                                           .field("role", Kind::Text)
                                                           .build()
                                                           ->content_id());
    CHECK(schema_of<TimerReady>()->content_id() ==
          SchemaBuilder("TimerReady", 1).build()->content_id());
    // Drive v2 — the beat that carries its own ownership (R2A-2). The sender is
    // TEXT and that is contract, not taste: a WeaveId is unsigned 64-bit and the
    // wire's Int is signed, so an Int field would silently narrow the top half
    // of the range. A drift back to Int is a red test.
    CHECK(schema_of<Drive>()->content_id() == SchemaBuilder("Drive", 2)
                                                  .field("activation_sender", Kind::Text)
                                                  .field("activation_sequence", Kind::Int)
                                                  .field("serial", Kind::Int)
                                                  .build()
                                                  ->content_id());
    CHECK(schema_of<Drive>()->version() == 2);

    // The address and the beat cap are contract too.
    CHECK(std::string(kTimerRole) == "zengine.timer");
    CHECK(kBeatCapMs == 10);
}

// ============================================================================
// Tier 2 — the service over a fake clock, through a real bus
// ============================================================================

// ---- R2A-2: the activation law ----------------------------------------------
// "Every successfully activated incarnation establishes exactly ONE beat chain.
//  A new activation owns a new chain; stale, duplicate, replayed, inherited or
//  foreign Drives cannot establish another."
//
// With no timers standing, every beat naps the cap and a parked pump leaves
// exactly the in-flight Drives in the queue — so `pending()` IS the chain count,
// and `hooks.beats` is the work actually done. Those two instruments prove every
// case below.

TEST_CASE("nothing drives an unactivated incarnation: a Drive before activation is inert") {
    FakeRig r;
    // A perfectly well-formed beat, correctly stamped, arriving at a service
    // that has not been told it is live. THIS is what makes a predecessor's
    // queued Drive harmless after a reload — the new instance begins
    // unactivated, so an inherited beat has nothing to own.
    r.drive_as(r.service, "1", 1, 0);
    r.run_beats(4); // no activation
    CHECK(r.hooks.beats == 0);      // no nap, no work
    CHECK(r.heard.ready == 0);      // and no availability notice
    CHECK(r.bus.pending() == 0);    // it established no chain: the bus drained

    // Then activate for real, and exactly one chain appears.
    r.run_beats(4, /*start=*/true);
    CHECK(r.hooks.beats == 4);
    CHECK(r.heard.ready == 1);
    CHECK(r.bus.pending() == 1);
}

TEST_CASE("an invalid, duplicate, or non-newer activation changes nothing") {
    FakeRig r;

    // Sequence 0 and negatives are not activations — the contract says the
    // sequence is positive, and a service that accepted one would be inventing
    // a lineage the operator never authored.
    r.activate(0);
    r.activate(-7);
    r.run_beats(4);
    CHECK(r.hooks.beats == 0);
    CHECK(r.heard.ready == 0);
    CHECK(r.bus.pending() == 0);

    // A real one starts the chain.
    r.run_beats(4, /*start=*/true); // sequence 1
    CHECK(r.heard.ready == 1);
    CHECK(r.bus.pending() == 1);
    const std::int64_t beats_after_start = r.hooks.beats;

    // THE SAME activation again: a duplicate. It does not republish
    // availability, does not reset the serial, and above all does not seed a
    // second chain.
    r.activate(1);
    r.run_beats(4);
    CHECK(r.heard.ready == 1);   // no second notice
    CHECK(r.bus.pending() == 1); // no second chain
    CHECK(r.hooks.beats > beats_after_start); // the real chain kept working

    // An OLDER sequence from the same sender is a replay: same answer.
    r.activate(1);
    r.run_beats(4);
    CHECK(r.heard.ready == 1);
    CHECK(r.bus.pending() == 1);
}

TEST_CASE("a newer activation replaces the chain; a different sender begins a new lineage — "
          "either way, exactly one chain") {
    FakeRig r;
    r.run_beats(4, /*start=*/true); // sequence 1
    REQUIRE(r.heard.ready == 1);
    REQUIRE(r.bus.pending() == 1);

    // A NEWER activation from the same lineage. It republishes availability
    // (the notice is once per ACCEPTED activation) and re-seeds from serial 0 —
    // and the old chain's parked beat, now carrying a stale key, is ignored
    // when it lands. One chain in, one chain out.
    r.activate(); // sequence 2
    r.run_beats(6);
    CHECK(r.heard.ready == 2);
    CHECK(r.bus.pending() == 1);

    // A DIFFERENT SENDER — a second lifecycle operator — is a new lineage. It
    // is accepted (at this altitude every weave in the process is trusted code;
    // see ActivationCursor on why this is lineage and not authentication), and
    // note its sequence is 1: LOWER than the current one, which is exactly the
    // point. Sequences are only comparable WITHIN a lineage, so a new sender's
    // 1 is not a replay of the old sender's 2. It too leaves one chain.
    r.activate_as(r.other_door, /*sequence=*/1);
    r.run_beats(6);
    CHECK(r.heard.ready == 3);
    CHECK(r.bus.pending() == 1);
}

TEST_CASE("only the current chain's next beat advances time: foreign sender, wrong key, "
          "and old/future/replayed serials are all ignored") {
    FakeRig r;
    r.run_beats(5, /*start=*/true);
    REQUIRE(r.bus.pending() == 1);
    const std::int64_t beats_before = r.hooks.beats;
    const std::string key = r.door_text();
    const std::int64_t seq = r.current_sequence();

    // EACH FORGERY MUST ISOLATE ITS OWN DEFECT, or the case proves less than it
    // says. The chain is parked with one real Drive ahead of these in the
    // queue, and that beat advances the expected serial by one before any of
    // them is delivered — so a forgery built with today's serial would be
    // rejected for being STALE, and would tell us nothing about the check it
    // was written for. They therefore carry the serial the chain will actually
    // be expecting when they arrive; the only thing wrong with each is the one
    // thing it is testing. (The two that ARE about serials carry deliberately
    // wrong ones.)
    const std::int64_t next_serial = r.hooks.beats + 1;
    r.drive_as(r.ear, key, seq, next_serial);            // FOREIGN stamped sender, else valid
    r.drive_as(r.service, "999999", seq, next_serial);   // wrong activation sender, else valid
    r.drive_as(r.service, key, seq + 5, next_serial);    // wrong activation sequence
    r.drive_as(r.service, key, seq, 0);                  // a REPLAYED consumed serial
    r.drive_as(r.service, key, seq, next_serial + 99);   // a fabricated future serial
    r.run_beats(5);

    // HONEST SCOPE OF THIS CASE. Four of those five are DISCRIMINATED here —
    // removing the key check or the serial check makes this case red. The
    // FOREIGN-SENDER one is not: an honoured foreign beat displaces the real
    // one rather than forking the chain, so every instrument below reads the
    // same either way. It is asserted as behaviour, not claimed as proof; the
    // service header states its true-by-construction status and why the term
    // stays anyway.

    // Still one chain, and the work done is the chain's own: five more beats,
    // not ten. (Every valid Drive produces exactly one beat; these produced
    // none.)
    CHECK(r.bus.pending() == 1);
    CHECK(r.hooks.beats == beats_before + 5);
    CHECK(r.heard.ready == 1); // and nothing re-announced
}

TEST_CASE("the chain's arithmetic boundary refuses to wrap") {
    // PROOF LEVEL, stated: this is a DIRECT pin on the guard, and the
    // behavioural path through it is true-by-construction — reaching the
    // boundary would take 2^63 beats, and the serial is a per-incarnation plain
    // member by design, so no revival path can place a chain near its end. The
    // guard is extracted precisely so the boundary is assertable at all.
    CHECK(can_advance_serial(0));
    CHECK(can_advance_serial(1));
    CHECK(can_advance_serial(std::numeric_limits<std::int64_t>::max() - 1));
    // At the last representable serial the chain STOPS rather than wrapping: a
    // wrapped serial would re-issue one already spent, and a duplicated serial
    // is indistinguishable from a replay — which would fork time.
    CHECK_FALSE(can_advance_serial(std::numeric_limits<std::int64_t>::max()));
    // A negative serial has no valid continuation either; it is not normalized.
    CHECK_FALSE(can_advance_serial(-1));
}

TEST_CASE("an activation announces availability and authors exactly one chain") {
    FakeRig r;
    r.run_beats(3, /*start=*/true);
    CHECK(r.heard.ready == 1); // hello on the first beat...
    CHECK(r.hooks.beats == 3); // ...and the service kept itself alive after it
    r.run_beats(2);
    CHECK(r.heard.ready == 1); // never again, however long it runs
    CHECK(r.hooks.beats == 5);
}

TEST_CASE("a one-shot fires once, on time, and is gone") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"once", 25, false});
    r.run_beats(6, /*start=*/true);
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired[0] == "once");
    CHECK(r.heard.fired_at[0] == 25); // nap-to-deadline, not nap-past-it
    r.run_beats(6);
    CHECK(r.heard.fired.size() == 1); // spent means spent
}

TEST_CASE("an immediate one-shot fires on the first beat; a 0ms repeat is clamped") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"asap", -5, false}); // negative: next beat, now
    r.ask_as(r.ear, StartTimer{"fast", 0, true});   // 0ms repeat: clamped to 1ms
    r.run_beats(4, /*start=*/true);
    REQUIRE(r.heard.fired.size() >= 3);
    CHECK(r.heard.fired[0] == "asap");
    CHECK(r.heard.fired_at[0] == 0); // due already: the beat naps 0 and fires
    // The clamp is what makes a "0ms" repeat a schedule instead of a hot
    // spin: virtual time ADVANCES 1ms per firing (a nap that stays 0 forever
    // would loop at t=0 and never satisfy these stamps).
    CHECK(r.heard.fired[1] == "fast");
    CHECK(r.heard.fired_at[1] == 1);
    CHECK(r.heard.fired[2] == "fast");
    CHECK(r.heard.fired_at[2] == 2);
}

TEST_CASE("a repeating timer holds its lattice") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.run_beats(4, /*start=*/true);
    // Beat n naps to t=10n and fires; the interrupted 4th beat's firing is
    // parked, so exactly three have ARRIVED.
    REQUIRE(r.heard.fired.size() == 3);
    CHECK(r.heard.fired_at[0] == 10);
    CHECK(r.heard.fired_at[1] == 20);
    CHECK(r.heard.fired_at[2] == 30);
}

TEST_CASE("a LATE repeating timer gets one firing, never a burst (the catch-up clamp)") {
    // The no-burst claim was double-claimed and never pinned — the trust gate
    // removed the clamp and this suite stayed green (its M2b). This is the
    // missing pin. The stall is the test's hand on the virtual clock: exactly
    // what a host wedged past several periods does to the lattice.
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.heard.stop_on_id = "beat";
    r.run_beats(8, /*start=*/true); // stops as the first firing arrives (t=10)
    r.heard.stop_on_id.clear();
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired_at[0] == 10); // the lattice's second rung is due at 20

    // ...and now the world stalls until t=75: rungs 20 through 70 are all in
    // the past when the chain resumes. SIX deadlines missed, by construction.
    const std::int64_t resumed = 75;
    r.hooks.now = resumed;
    r.run_beats(4);

    // What arrives is ONE firing at the resuming instant, then a lattice
    // re-anchored to the stall — not six stale rungs delivered back to back.
    // Without the clamp every one of these arrives at t=75 (each beat naps 0
    // because the next rung is still in the past), so the burst is exactly
    // what these stamps refuse.
    REQUIRE(r.heard.fired.size() == 4);
    CHECK(r.heard.fired_at[1] == resumed);                  // the one catch-up firing
    CHECK(r.heard.fired_at[2] == resumed + 10);             // re-anchored to NOW...
    CHECK(r.heard.fired_at[3] == resumed + 20);             // ...and cadence restored
    CHECK(r.heard.fired_at[2] != 80);                       // not the pre-stall lattice
    CHECK(r.hooks.now >= resumed + 20);                     // virtual time MOVED again
}

TEST_CASE("the beat cap IS the lateness bound: idle naps the cap, a nearer deadline shortens it") {
    // kBeatCapMs is pinned as a constant in tier 1; this pins the BEHAVIOR it
    // names — the two halves of the vocabulary's claim that the cap is "the
    // worst-case lateness of a firing and the arrival bound on a StartTimer
    // being considered".
    FakeRig r;
    r.run_beats(3, /*start=*/true);
    CHECK(r.hooks.now == 3 * kBeatCapMs); // nothing standing: each beat naps the cap

    // An ask queued behind a parked beat waits out that beat's whole nap
    // before it is even seen. With an empty table that nap is a full cap — so
    // a 1ms timer asked at t=30 fires at t=41, not t=31. That gap IS the bound.
    r.ask_as(r.ear, StartTimer{"late", 1, false});
    r.run_beats(3);
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired_at[0] == 4 * kBeatCapMs + 1);

    // The other half: a deadline nearer than the cap shortens the nap to it,
    // and a farther one is walked there in capped steps, landing exactly on it.
    FakeRig s;
    s.ask_as(s.ear, StartTimer{"soon", 4, false});
    s.ask_as(s.ear, StartTimer{"far", 25, false});
    s.run_beats(5, /*start=*/true);
    REQUIRE(s.heard.fired.size() == 2);
    CHECK(s.heard.fired_at[0] == 4);  // the deadline, not the cap
    CHECK(s.heard.fired_at[1] == 25); // reached in capped naps, no overshoot
}

TEST_CASE("cancel before the chain starts: the timer never fires at all") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"gone", 10, true});
    r.ask_as(r.ear, CancelTimer{"gone"});
    r.run_beats(5, /*start=*/true);
    CHECK(r.heard.fired.empty());
}

TEST_CASE("cancel under a live chain: at most the in-flight firing lands, then silence") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.heard.stop_on_id = "beat";
    r.run_beats(8, /*start=*/true); // stops as the first firing arrives
    r.heard.stop_on_id.clear();
    REQUIRE(r.heard.fired.size() == 1);

    // The cancel queues BEHIND the parked beat, which has already spoken:
    // one more firing lands, and that is the whole cost — pinned, not hidden.
    r.ask_as(r.ear, CancelTimer{"beat"});
    r.run_beats(6);
    CHECK(r.heard.fired.size() == 2);
    r.run_beats(6);
    CHECK(r.heard.fired.size() == 2);
    CHECK(r.hooks.beats >= 12); // the chain itself never stopped beating
}

TEST_CASE("asking again with the same id REPLACES the schedule (upsert)") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.heard.stop_on_id = "beat";
    r.run_beats(8, /*start=*/true);
    r.heard.stop_on_id.clear();
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired_at[0] == 10);

    r.ask_as(r.ear, StartTimer{"beat", 30, true}); // re-ask: new cadence
    r.run_beats(8);
    // The parked beat fires once more at t=20 (in flight before the re-ask
    // was heard); after that the lattice is 30ms from the re-ask's arrival.
    REQUIRE(r.heard.fired.size() >= 3);
    CHECK(r.heard.fired_at[1] == 20);
    CHECK(r.heard.fired_at[2] == 50);
    if (r.heard.fired.size() > 3) {
        CHECK(r.heard.fired_at[3] == 80);
    }
}

TEST_CASE("CancelAllMyTimers kills mine and nobody else's") {
    FakeRig r;
    Heard other_heard;
    other_heard.hooks = &r.hooks;
    other_heard.bus = &r.bus;
    const loom::WeaveId other = loom::mount<Ear>(r.bus, other_heard);

    r.ask_as(r.ear, StartTimer{"mine.a", 20, false});
    r.ask_as(r.ear, StartTimer{"mine.b", 30, false});
    r.ask_as(other, StartTimer{"theirs", 40, false});
    r.ask_as(r.ear, CancelAllMyTimers{});
    r.run_beats(8, /*start=*/true);

    CHECK(r.heard.fired.empty()); // both of mine died before firing
    REQUIRE(other_heard.fired.size() == 1);
    CHECK(other_heard.fired[0] == "theirs"); // theirs lived
    CHECK(other_heard.fired_at[0] == 40);
}

TEST_CASE("CancelAllMyTimers is TOTAL — role beats die with the rest, chosen not accidental") {
    // The shape's doc teaches WHEN to send this (a weave being replaced should
    // leave its role beats standing for its successor; one retiring with no
    // heir should cancel) — but the mechanism itself stays total and neutral,
    // because succession here is AUTHORED, never guessed by a shape. This pins
    // the totality as a decision, so it cannot erode into an accident.
    const char* kRole = "suite.pulse.holder";

    // The positive control first: without the cancel, that role beat fires.
    // (An all-empty assertion is only meaningful next to a rig where the same
    // asks DO produce firings.)
    {
        FakeRig c;
        Heard pulse;
        pulse.hooks = &c.hooks;
        pulse.bus = &c.bus;
        (void)mount_role<Ear>(c.bus, kRole, pulse);
        c.ask_as(c.ear, StartTimer{"mine", 20, false});
        c.ask_as(c.ear, StartRoleTimer{"pulse", 10, true, kRole});
        c.run_beats(6, /*start=*/true);
        CHECK(c.heard.fired.size() == 1);  // the requester timer fired...
        CHECK(pulse.fired.size() >= 2);    // ...and the role beat was beating
    }

    FakeRig r;
    Heard pulse;
    pulse.hooks = &r.hooks;
    pulse.bus = &r.bus;
    (void)mount_role<Ear>(r.bus, kRole, pulse);
    r.ask_as(r.ear, StartTimer{"mine", 20, false});
    r.ask_as(r.ear, StartRoleTimer{"pulse", 10, true, kRole});
    r.ask_as(r.ear, CancelAllMyTimers{});
    r.run_beats(6, /*start=*/true);

    CHECK(r.heard.fired.empty()); // the requester-addressed timer: gone
    CHECK(pulse.fired.empty());   // and the ROLE beat with it — that is the point
}

TEST_CASE("a role timer follows the role: holder, successor, and honest vacancy") {
    FakeRig r;
    std::int64_t fired_refused = 0;
    r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Refused && ev.schema_name == "TimerFired") {
            CHECK(ev.refusal.reason == loom::RefusalReason::NoSuchTarget);
            ++fired_refused;
        }
    });

    Heard heard_a;
    heard_a.hooks = &r.hooks;
    heard_a.bus = &r.bus;
    const loom::WeaveId holder_a = mount_role<Ear>(r.bus, "suite.pulse.holder", heard_a);
    r.ask_as(r.ear, StartRoleTimer{"pulse", 10, true, "suite.pulse.holder"});

    r.run_beats(3, /*start=*/true);
    CHECK(heard_a.fired.size() == 2); // t=10, t=20 (the 3rd beat's firing is parked)

    // The holder dies; the beat keeps its address and refuses cleanly into
    // the vacancy (an unheld role degrades like an unknown target)...
    (void)r.bus.unregister_weave(holder_a);
    r.run_beats(3);
    CHECK(fired_refused >= 2);

    // ...and a SUCCESSOR simply inherits it: no re-ask, no handshake.
    Heard heard_b;
    heard_b.hooks = &r.hooks;
    heard_b.bus = &r.bus;
    (void)mount_role<Ear>(r.bus, "suite.pulse.holder", heard_b);
    r.run_beats(3);
    CHECK(heard_b.fired.size() >= 2);
    CHECK(heard_a.fired.size() == 2); // the dead never heard another word
}

TEST_CASE("a successor's re-ask upserts the ROLE beat instead of doubling it") {
    FakeRig r;
    Heard heard_a;
    heard_a.hooks = &r.hooks;
    heard_a.bus = &r.bus;
    const loom::WeaveId holder = mount_role<Ear>(r.bus, "suite.pulse.holder", heard_a);

    // Two askers, same (role, id) — the second replaces the first, so the
    // holder hears ONE lattice, not an interleaving of two.
    r.ask_as(r.ear, StartRoleTimer{"pulse", 7, true, "suite.pulse.holder"});
    r.ask_as(holder, StartRoleTimer{"pulse", 10, true, "suite.pulse.holder"});
    r.run_beats(4, /*start=*/true);
    REQUIRE(heard_a.fired.size() == 3);
    CHECK(heard_a.fired_at[0] == 10);
    CHECK(heard_a.fired_at[1] == 20);
    CHECK(heard_a.fired_at[2] == 30);
}

TEST_CASE("a dead requester's timer fires into clean refusals — the pinned V1 floor") {
    // The service cannot SEE a requester die (a weave gets no delivery
    // outcomes and the bus broadcasts no unloads), so V1's honest behavior
    // is: the beat keeps firing at a WeaveId that no longer exists, each
    // delivery refused NoSuchTarget (ids are never reused — it can never hit
    // a stranger), until someone cancels or the service is replaced. Pinned
    // as documented behavior, not hidden as a surprise.
    FakeRig r;
    std::int64_t fired_refused = 0;
    r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Refused && ev.schema_name == "TimerFired") {
            CHECK(ev.refusal.reason == loom::RefusalReason::NoSuchTarget);
            ++fired_refused;
        }
    });

    Heard orphan_heard;
    orphan_heard.hooks = &r.hooks;
    orphan_heard.bus = &r.bus;
    const loom::WeaveId orphan = loom::mount<Ear>(r.bus, orphan_heard);
    r.ask_as(orphan, StartTimer{"orphan.beat", 10, true});

    r.run_beats(2, /*start=*/true);
    CHECK(orphan_heard.fired.size() == 1); // alive: it hears its beat

    (void)r.bus.unregister_weave(orphan);
    r.run_beats(4);
    CHECK(fired_refused >= 3);            // dead: the beat refuses, visibly
    CHECK(orphan_heard.fired.size() == 1); // and never lands anywhere else
}

TEST_CASE("a root ask has no one to answer: dropped, not misdelivered") {
    FakeRig r;
    // Root sends carry no weave identity; a StartTimer from one has no
    // requester to fire at. The service drops it (counted on its poke
    // counter — pinned through the .so lane) rather than inventing a target.
    r.bus.send(r.service, loom::Message(loom::to_value(StartTimer{"nobody", 5, true})));
    r.run_beats(6, /*start=*/true);
    CHECK(r.heard.fired.empty());
}

// ============================================================================
// Tier 3 — the real library through the real Kernel, on the real clock
// ============================================================================

TEST_CASE("the timer .so authors its chain from its own activation — no wind, ever") {
    Rig r;
    // NOTHING here winds anything. Loading the service is the whole gesture:
    // the control door activates the freshly committed incarnation and the
    // service seeds its own chain from that.
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    CHECK(r.seen.ready == 1);   // available, announced on the activation itself
    CHECK(r.drives > 0);        // and already beating
    CHECK(r.bus.pending() > 0); // on a chain that is alive and parked

    // A root ask: dropped, and honestly counted (the tier-2 pin's poke-visible
    // half lives here, where pokes exist).
    r.bus.send(timer_so, loom::Message(loom::to_value(StartTimer{"nobody", 5, true})));

    // The witness asks for a repeating beat. Everything runs on the REAL
    // monotonic clock; the third firing stops the bus.
    r.ask(timer_so, StartTimer{"suite.tick", 15, true});
    r.seen.stop_id = "suite.tick";
    r.seen.stop_count = 3;
    r.seen.stop_seen = 0;
    r.bus.pump();
    r.seen.stop_id.clear();

    CHECK(r.seen.ready == 1); // the hello reached an accepter
    std::size_t ticks = 0;
    for (const std::string& id : r.seen.fired) {
        if (id == "suite.tick") {
            ++ticks;
        }
    }
    CHECK(ticks == 3);
    CHECK(r.bus.pending() > 0); // the parked chain: it RE-WINDS ITSELF

    // The honest counters, poked under the live chain.
    Seen::Answer a = r.poke_timed(timer_so, timer_so, loom::PokeRead{"dropped"});
    CHECK(a.kind == 0);
    CHECK(a.text == "1");
    a = r.poke_timed(timer_so, timer_so, loom::PokeRead{"active"});
    CHECK(a.kind == 0);
    // suite.tick alone: each poke_timed stopwatch is a one-shot, spent and
    // erased before its pump stops, and THIS call's stopwatch registers
    // after the poke answers (FIFO).
    CHECK(a.text == "1");
    a = r.poke_timed(timer_so, timer_so, loom::PokeRead{"fired"});
    CHECK(a.kind == 0);
    CHECK(std::stoll(a.text) >= 4); // 3 ticks + at least one stopwatch

    // Cancel, then prove silence by outliving a fresh stopwatch window.
    r.ask(timer_so, CancelTimer{"suite.tick"});
    const std::size_t before = r.seen.fired.size();
    r.pump_for(timer_so, 50);
    std::size_t late_ticks = 0;
    for (std::size_t i = before; i < r.seen.fired.size(); ++i) {
        if (r.seen.fired[i] == "suite.tick") {
            ++late_ticks;
        }
    }
    CHECK(late_ticks <= 1); // at most the in-flight firing (the tier-2 pin, live)
}

TEST_CASE("a root Drive establishes nothing: the boot ordering hazard is gone with the wind") {
    // HISTORICAL NOTE, kept because it is why this case exists. Under the old
    // mechanism the host wound the clock with a root Drive, and loading is a
    // CONVERSATION (the Manager answers LoadWeave by asking the kernel door,
    // one delivery later) — so a wind queued behind un-pumped boot sends
    // resolved the timer role before any load had run and died into the
    // vacancy. Found live, on the pilot's very first run; the host had to
    // boot-pump before winding to avoid it.
    //
    // R2A-2 removed the hazard by removing the wind. What is pinned now is the
    // stronger property that replaced it: a root Drive is not a lever at all.
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    CHECK(r.seen.ready == 1);
    const std::int64_t beats_before = std::stoll(
        r.poke_timed(timer_so, timer_so, loom::PokeRead{"beats"}).text);

    // A root Drive — no stamped sender, no activation key, serial 0 — into the
    // live role. It is DELIVERED (the role is held) and does nothing: it names
    // no activation this incarnation is living under.
    const std::int64_t delivered_before = r.drives;
    r.bus.send_to_role(kTimerRole, loom::Message(loom::to_value(Drive{"", 0, 0})));
    r.pump_for(timer_so, 30);
    CHECK(r.drives > delivered_before); // it really did arrive...

    // ...and it forked nothing. One chain still, and the beat count advanced
    // only by the chain's own beats.
    CHECK(r.bus.pending() > 0);
    const std::int64_t beats_after = std::stoll(
        r.poke_timed(timer_so, timer_so, loom::PokeRead{"beats"}).text);
    CHECK(beats_after > beats_before); // the real chain kept running
    CHECK(r.seen.ready == 1);          // and nothing re-announced
}

// ============================================================================
// Tier 4 — the migration chains: what the host used to carry, now asked for
// ============================================================================

TEST_CASE("world time comes from the timer path: ticks with nobody sending SnakeTick") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    (void)r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);
    const loom::WeaveId clock_so = r.load("snake-clock", SNAKE_CLOCK_SO, "");

    r.pump_for(timer_so, 400); // ~3 ticks of the adapter's 120ms ask

    // The world seeded and MOVED — and no test code ever sent a SnakeTick.
    REQUIRE(r.seen.visuals.size() >= 2);
    CHECK(r.seen.visuals.front().snake.front().x == 12); // the seed pin
    CHECK(r.seen.visuals.front().snake.front().y == 8);
    CHECK(r.seen.visuals.back().snake.front().x > 12); // heading right, on time

    const Seen::Answer a = r.poke_timed(timer_so, clock_so, loom::PokeRead{"ticks"});
    CHECK(a.kind == 0);
    CHECK(std::stoll(a.text) >= 2);
}

// ---- R2A-2: the load-order matrix -------------------------------------------
// Load order used to decide who got to breathe. It decides nothing now, and
// these two cases are the halves that prove it: a consumer AFTER the timer is
// served by its own activation; a consumer BEFORE it is served by the retry.

TEST_CASE("consumer BEFORE the timer: its ask goes nowhere, and TimerReady "
          "is what rescues it") {
    Rig r;
    // The skin is loaded into a world with NO timer service. Its activation
    // fires, it announces, and it asks for its pump beat — and the ask goes
    // NOWHERE.
    //
    // WHERE IT DIES IS WORTH KNOWING, and it is earlier than "refused by an
    // unheld role": a loaded weave's send crosses the library seam as bytes,
    // and the host resolves the claimed schema against the bus registry before
    // routing anything. With no timer service present, NOBODY accepts
    // StartRoleTimer, so the shape is not registered at all and the send is
    // rejected at the seam — no envelope, no delivery, no refusal event. The
    // skin cannot tell, and could not retry on its own if it wanted to. That is
    // precisely why TimerReady is still load-bearing.
    const loom::WeaveId skin_so =
        r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC,
               zengine::surface::kSkinRole);
    r.bus.pump(); // nothing is alive; this drains
    CHECK(r.bus.pending() == 0);

    // Measured, not assumed: it is being serviced by nothing.
    const std::uint64_t before_corr = r.next_corr++;
    r.bus.send(skin_so, loom::Message(loom::to_value(loom::PokeRead{"pumps"}), loom::WeaveId{},
                                      r.witness, before_corr));
    r.bus.pump();
    const Seen::Answer* before = r.seen.find(before_corr);
    REQUIRE(before != nullptr);
    CHECK(before->text == "0");

    // Now the timer arrives. Its activation publishes TimerReady, the skin asks
    // AGAIN — the separated ask, which the old `hello_once` made impossible —
    // and from then on it is serviced.
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    r.pump_for(timer_so, 60);
    CHECK(r.seen.ready >= 1);
    const Seen::Answer pumps = r.poke_timed(timer_so, skin_so, loom::PokeRead{"pumps"});
    CHECK(pumps.kind == 0);
    CHECK(std::stoll(pumps.text) > 0); // it is being serviced, with nobody pumping it
}

TEST_CASE("a swapped-in skin asks from its own activation and does NOT double the role beat") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    (void)r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC,
                 zengine::surface::kSkinRole);
    r.pump_for(timer_so, 60);
    const std::int64_t standing_before =
        std::stoll(r.poke_timed(timer_so, timer_so, loom::PokeRead{"active"}).text);
    CHECK(standing_before >= 1); // the skin's beat is standing

    // Replace the painter. The successor's own activation makes it ask.
    const std::uint64_t corr = r.next_corr++;
    r.bus.send_as(r.witness, r.manager,
                  loom::Message(loom::to_value(loom::SwapWeave{
                                    zengine::surface::kSkinRole, "zengine-skin-tui-block",
                                    SKIN_SO_TUI_BLOCK, /*graceful=*/false}),
                                r.witness, r.witness, corr));
    r.pump_for(timer_so, 80);
    const Seen::Answer* swapped = r.seen.find(corr);
    REQUIRE(swapped != nullptr);
    REQUIRE_MESSAGE(swapped->kind == 0, "skin swap refused: ", swapped->text);
    const loom::WeaveId block{static_cast<std::uint64_t>(std::stoll(swapped->text))};

    // The successor is serviced...
    const Seen::Answer pumps = r.poke_timed(timer_so, block, loom::PokeRead{"pumps"});
    CHECK(std::stoll(pumps.text) > 0);
    // ...and the beat was REPLACED, not added: the role-timer upsert keys on
    // (role, id) ACROSS requesters, so the successor's ask takes the incumbent's
    // schedule instead of standing a second one beside it.
    const std::int64_t standing_after =
        std::stoll(r.poke_timed(timer_so, timer_so, loom::PokeRead{"active"}).text);
    CHECK(standing_after == standing_before);
}

TEST_CASE("the input package arranges its own execution: it polls with nobody pumping") {
    Rig r;
    // Loaded AFTER the timer is already running — the load order that used to
    // make a consumer permanently deaf. Its own activation is its first breath.
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    const loom::WeaveId input_so =
        r.load("zengine-input", INPUT_SO, zengine::input::kInputRole);

    r.pump_for(timer_so, 80); // ~8 beats of the weave's own 10ms ask

    // Headless, so it heard nothing — but it RAN, repeatedly, and not one
    // PumpInput was sent in this entire lane.
    Seen::Answer a = r.poke_timed(timer_so, input_so, loom::PokeRead{"pumped"});
    CHECK(a.kind == 0);
    CHECK(std::stoll(a.text) >= 3);
    a = r.poke_timed(timer_so, input_so, loom::PokeRead{"emitted"});
    CHECK(a.kind == 0);
    CHECK(a.text == "0");
}

TEST_CASE("the skin keeps itself serviced: hello and beats with nobody pumping") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    const loom::WeaveId skin_so =
        r.load("zengine-skin-tui-classic", SKIN_SO_TUI_CLASSIC, zengine::surface::kSkinRole);

    r.pump_for(timer_so, 80);

    // The service's hello was the skin's first breath: it said ITS hello and
    // asked for the role beat, which has been servicing its medium since —
    // and not one PumpSurface was sent in this entire lane.
    CHECK(r.seen.hellos == 1);
    const Seen::Answer a = r.poke_timed(timer_so, skin_so, loom::PokeRead{"pumps"});
    CHECK(a.kind == 0);
    CHECK(std::stoll(a.text) >= 3);
}
