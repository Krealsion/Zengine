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
//      its role, the root winds it once, and the chain runs on a REAL
//      monotonic clock. The suite's stop lever is a one-shot STOPWATCH timer
//      it asks for itself: the package is its own test harness.
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
class Ear : public loom::WeaveBase<Ear, EarState, loom::Accept<TimerFired, TimerReady>,
                                   loom::Emit<StartTimer, StartRoleTimer, CancelTimer,
                                              CancelAllMyTimers>> {
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

/// The tier-2 world: a real bus, the service on a fake clock, one ear.
struct FakeRig {
    loom::Switchboard bus;
    FakeHooks hooks;
    Heard heard;
    loom::WeaveId service{};
    loom::WeaveId ear{};

    FakeRig() {
        hooks.bus = &bus;
        heard.hooks = &hooks;
        heard.bus = &bus;
        // Into its ROLE — the wind is role-addressed, exactly as the host's is.
        service = mount_role<FakeService>(bus, kTimerRole, FakeClock{&hooks});
        ear = loom::mount<Ear>(bus, heard);
    }

    /// Speak AS a weave (stamped sender, authorized against ITS grant) — how
    /// every ask reaches the service in this tier.
    template <class T>
    void ask_as(loom::WeaveId who, const T& msg) {
        bus.send_as(who, service, loom::Message(loom::to_value(msg), who, who, 0));
    }

    /// Wind (or resume) the chain and let it run at most `n` more beats
    /// (fewer if a stop_on_id fires first). The interrupted beat's tail
    /// parks; the next call resumes it — the host's own stop/quit mechanics.
    void run_beats(std::int64_t n, bool wind = false) {
        hooks.stop_after = hooks.beats + n;
        if (wind) {
            bus.send_to_role(kTimerRole, loom::Message(loom::to_value(Drive{})));
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

    loom::WeaveId mount_witness() {
        seen.bus = &bus;
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow_to_any(StartTimer::zen_name, StartTimer::zen_version);
        reach.allow_to_any(StartRoleTimer::zen_name, StartRoleTimer::zen_version);
        reach.allow_to_any(CancelTimer::zen_name, CancelTimer::zen_version);
        reach.allow_to_any(CancelAllMyTimers::zen_name, CancelAllMyTimers::zen_version);
        return loom::mount_granted<Witness>(bus, std::move(reach), seen);
    }

    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const std::uint64_t corr = next_corr++;
        bus.send_as(witness, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{name, path, role}), witness,
                                  witness, corr));
        bus.pump(); // pre-wind only: with no chain alive this pump drains
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

    void wind() { bus.send_to_role(kTimerRole, loom::Message(loom::to_value(Drive{}))); }

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
    CHECK(schema_of<Drive>()->content_id() == SchemaBuilder("Drive", 1).build()->content_id());

    // The address and the beat cap are contract too.
    CHECK(std::string(kTimerRole) == "zengine.timer");
    CHECK(kBeatCapMs == 10);
}

// ============================================================================
// Tier 2 — the service over a fake clock, through a real bus
// ============================================================================

TEST_CASE("the wind announces once per incarnation, and the chain lives") {
    FakeRig r;
    r.run_beats(3, /*wind=*/true);
    CHECK(r.heard.ready == 1); // hello on the first beat...
    CHECK(r.hooks.beats == 3); // ...and the service kept itself alive after it
    r.run_beats(2);
    CHECK(r.heard.ready == 1); // never again, however long it runs
    CHECK(r.hooks.beats == 5);
}

TEST_CASE("a one-shot fires once, on time, and is gone") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"once", 25, false});
    r.run_beats(6, /*wind=*/true);
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
    r.run_beats(4, /*wind=*/true);
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
    r.run_beats(4, /*wind=*/true);
    // Beat n naps to t=10n and fires; the interrupted 4th beat's firing is
    // parked, so exactly three have ARRIVED.
    REQUIRE(r.heard.fired.size() == 3);
    CHECK(r.heard.fired_at[0] == 10);
    CHECK(r.heard.fired_at[1] == 20);
    CHECK(r.heard.fired_at[2] == 30);
}

TEST_CASE("cancel before the wind: the timer never fires at all") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"gone", 10, true});
    r.ask_as(r.ear, CancelTimer{"gone"});
    r.run_beats(5, /*wind=*/true);
    CHECK(r.heard.fired.empty());
}

TEST_CASE("cancel under a live chain: at most the in-flight firing lands, then silence") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.heard.stop_on_id = "beat";
    r.run_beats(8, /*wind=*/true); // stops as the first firing arrives
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
    r.run_beats(8, /*wind=*/true);
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
    r.run_beats(8, /*wind=*/true);

    CHECK(r.heard.fired.empty()); // both of mine died before firing
    REQUIRE(other_heard.fired.size() == 1);
    CHECK(other_heard.fired[0] == "theirs"); // theirs lived
    CHECK(other_heard.fired_at[0] == 40);
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

    r.run_beats(3, /*wind=*/true);
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
    r.run_beats(4, /*wind=*/true);
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

    r.run_beats(2, /*wind=*/true);
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
    r.run_beats(6, /*wind=*/true);
    CHECK(r.heard.fired.empty());
}

// ============================================================================
// Tier 3 — the real library through the real Kernel, on the real clock
// ============================================================================

TEST_CASE("the timer .so loads into its role, winds once, and re-winds itself") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);

    // A root ask first: dropped, and honestly counted (the tier-2 pin's
    // poke-visible half lives here, where pokes exist).
    r.bus.send(timer_so, loom::Message(loom::to_value(StartTimer{"nobody", 5, true})));

    // The witness asks for a repeating beat, then winds. Everything after
    // runs on the REAL monotonic clock; the third firing stops the bus.
    r.ask(timer_so, StartTimer{"suite.tick", 15, true});
    r.seen.stop_id = "suite.tick";
    r.seen.stop_count = 3;
    r.seen.stop_seen = 0;
    r.wind();
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

TEST_CASE("a wind into a vacant role refuses cleanly; a later wind still starts the clock") {
    // Found live, pinned here: loading is a CONVERSATION (the Manager answers
    // LoadWeave by asking the kernel door, one delivery later), so a wind
    // queued behind un-pumped boot sends resolves the role before any load
    // ran — and dies into the vacancy. The host boot-pumps before winding
    // now; this pins both halves: the early wind refuses cleanly (nothing
    // wedges, nothing leaks), and a wind AFTER the load starts the clock.
    Rig r;
    std::int64_t refused_winds = 0;
    r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Refused && ev.schema_name == "Drive") {
            CHECK(ev.refusal.reason == loom::RefusalReason::NoSuchTarget);
            ++refused_winds;
        }
    });

    r.wind(); // nobody holds zengine.timer yet
    r.bus.pump();
    CHECK(refused_winds == 1);
    CHECK(r.bus.pending() == 0); // refused into the vacancy, no chain, no wedge

    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    r.wind();
    r.pump_for(timer_so, 30);
    CHECK(r.seen.ready == 1);    // the clock announced...
    CHECK(r.bus.pending() > 0);  // ...and the chain is alive and parked
    CHECK(refused_winds == 1);   // the recovery cost nothing further
}

// ============================================================================
// Tier 4 — the migration chains: what the host used to carry, now asked for
// ============================================================================

TEST_CASE("world time comes from the timer path: ticks with nobody sending SnakeTick") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    (void)r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);
    const loom::WeaveId clock_so = r.load("snake-clock", SNAKE_CLOCK_SO, "");

    r.wind();
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

TEST_CASE("the input package arranges its own execution: it polls with nobody pumping") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    const loom::WeaveId input_so =
        r.load("zengine-input", INPUT_SO, zengine::input::kInputRole);

    r.wind();
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

    r.wind();
    r.pump_for(timer_so, 80);

    // The service's hello was the skin's first breath: it said ITS hello and
    // asked for the role beat, which has been servicing its medium since —
    // and not one PumpSurface was sent in this entire lane.
    CHECK(r.seen.hellos == 1);
    const Seen::Answer a = r.poke_timed(timer_so, skin_so, loom::PokeRead{"pumps"});
    CHECK(a.kind == 0);
    CHECK(std::stoll(a.text) >= 3);
}
