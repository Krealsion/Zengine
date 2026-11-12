// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The Timer suite -- the Timer package proven headless: the vocabulary pinned by content id; the
// service through a real bus, handed its time by the host's one wait (`loom::host::Reactor`) on a
// clock that moves only when told; the real library through the real Kernel under that same wait;
// and the world it ticks with nobody sending a tick. Every pump is the host's loop: the bus turned,
// then the wait -- a look while the bus holds work, else a rest until the soonest time due, which
// moves the clock there. The bus drains between wakes, so a pump ends when the host is quiet, when
// what it waits for came, or at the time it waits until.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "role_set.hpp"
#include "doctest.h"

#include "timer/binding.hpp"
#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "operator_fixture.hpp"
#include "operator_stranger.hpp"
#include "timer/normalize.hpp"

#include "input/vocabulary.hpp"
#include "role_manual.hpp"
#include "probe_vocabulary.hpp"
#include "lifecycle_door.hpp"
#include "examples/snake/vocabulary.hpp"
#include "graphics/window/window.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
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

// ---- the host's loop, as every rig here turns it ----------------------------------

/// THE HOST'S LOOP: the bus turned, then the host's one wait -- a look while the bus holds work,
/// else a rest until the soonest time a trusted source is due (on a clock that moves only when
/// told, the wait moves it there), or until `until` when one is given. It returns once `done()`
/// holds with the bus empty, once the host is quiet -- the bus empty and nothing to wait for --
/// or at `until`, after what is due then has been handed over. `at_once` returns as soon as
/// `done()` holds after a turn, the bus as that turn left it (a tap that stops the bus mid-turn
/// leaves the rest queued). The bound is a hang guard: a loop that does not come to rest fails the
/// case.
template <class Done>
void host_loop(loom::Switchboard& bus, loom::host::Reactor& wait, const loom::host::Clock& clock,
               Done done, std::optional<std::int64_t> until = std::nullopt, bool at_once = false) {
    for (int turn = 0; turn < 200000; ++turn) {
        bus.pump_pending();
        if (at_once && done()) {
            return;
        }
        if (bus.pending() != 0) {
            (void)wait.after_turn(0);
            continue;
        }
        if (done()) {
            return;
        }
        if (until && clock.now_ms() >= *until) {
            (void)wait.after_turn(0); // what is due at that time, handed over
            if (bus.pending() == 0) {
                return;
            }
            continue;
        }
        (void)(until ? wait.after_turn(*until - clock.now_ms()) : wait.after_turn());
        if (!until && bus.pending() == 0) {
            return; // quiet: nothing claimed a time, nothing is ready
        }
    }
    FAIL("the host's loop did not come to rest");
}

/// Whether a delivery is the host's wait waking `who` because its time came.
bool due_wake_of(const loom::BusEvent& ev, loom::WeaveId who) {
    return ev.kind == loom::EventKind::Delivered && ev.target == who && ev.payload != nullptr &&
           ev.schema_name == loom::host::Waited::zen_name && !ev.sender.valid() &&
           ev.schema_version == loom::host::Waited::zen_version && ev.payload->get("due") != nullptr &&
           ev.payload->get("due")->as_bool();
}

/// What the ears heard, shared with the test. One instance can serve several
/// ears — the ids and stamps say who was addressed.
struct Heard {
    std::vector<std::string> fired;     ///< TimerFired ids, in arrival order
    std::vector<std::int64_t> fired_at; ///< the host's clock at each arrival
    std::vector<TimerResolution> resolutions; ///< receipts, in arrival order
    std::int64_t ready = 0;             ///< TimerReady hellos
    const loom::host::Clock* clock = nullptr;
};

struct EarState {
    std::int64_t count = 0;
    ZEN_SHAPE(EarState, 1, ZEN_FIELD(count));
};

/// An ordinary timer consumer; its Emit list is what lets the test speak AS it (send_as
/// authorizes against the grant mount() derives from exactly this list). `Waited` is in it ON
/// PURPOSE: the hostile frame this suite forges -- the host's wait's word from a participant --
/// must be SAYABLE through the honest API, or the pin would test the grant model rather than the
/// service's check that only the host's own word is time.
class Ear
    : public loom::WeaveBase<Ear, EarState,
                             loom::Accept<TimerFired, TimerReady, TimerResolution>,
                             loom::Emit<StartTimer, StartRoleTimer, EnsureTimer, EnsureRoleTimer,
                                        CancelTimer, CancelAllMyTimers, loom::host::Waited>> {
public:
    explicit Ear(Heard& heard) : heard_(&heard) {}
    void on(const TimerResolution& t, loom::Mail&) { heard_->resolutions.push_back(t); }
    void on(const TimerFired& f, loom::Mail&) {
        ++state_.count;
        heard_->fired.push_back(f.id);
        heard_->fired_at.push_back(heard_->clock != nullptr ? heard_->clock->now_ms() : -1);
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
using zengine::testing::Impostor;
using zengine::testing::TestDoor;

/// A letter written to a DIFFERENT VERSION of the handoff shape.
///
/// Hand-written registration so the wire name is `TimerHandoff` with version 2
/// — the whole point is that it is the same NAME and a different VERSION, which
/// is the case a label-trusting reader would wave through and the gate does not.
/// No honest predecessor in this tree can produce one, so the test forges it.
struct HandoffV2 {
    std::vector<TimerHandoffEntry> entries;
    using ZenSelf = HandoffV2;
    static constexpr const char* zen_name = "TimerHandoff";
    static constexpr std::uint32_t zen_version = 2;
    static auto zen_fields() { return std::make_tuple(ZEN_FIELD(entries)); }
};

/// A stand-in steward: holds `zen.manager`, answers a claim however the test tells it to, and
/// keeps whatever letter it is handed -- so the tier-2 pins drive both ends of the handoff
/// without a kernel, including the ends a real steward never would: a silence that never
/// answers, a letter to the wrong version, one over the published bound.
struct Letters {
    enum class Answer { Letter, Refuse, Silence, UnattestedLetter };
    Answer answer = Answer::Refuse;
    std::vector<loom::Bytes> items; ///< what a Letter answer hands over
    std::int64_t claims = 0;        ///< how many claims arrived
    std::vector<loom::Bytes> received; ///< letters the steward was given
    std::uint64_t last_claim_corr = 0;
};

struct StewardState {
    std::int64_t claims = 0;
    ZEN_SHAPE(StewardState, 1, ZEN_FIELD(claims));
};

class FakeSteward
    : public loom::WeaveBase<FakeSteward, StewardState,
                             loom::Accept<loom::ClaimBequest, loom::Bequest>,
                             loom::Emit<loom::PrepareShutdown, loom::Bequest, loom::Refused>> {
public:
    explicit FakeSteward(Letters& box) : box_(&box) {}

    void on(const loom::ClaimBequest& c, loom::Mail& mail) {
        ++state_.claims;
        ++box_->claims;
        box_->last_claim_corr = mail.correlation();
        switch (box_->answer) {
        case Letters::Answer::Silence:
            return; // the steward that never speaks: the bootstrap must not hang
        case Letters::Answer::Refuse:
            // ANSWERED, not merely sent. "There is nothing for you" is
            // exactly as load-bearing as a letter — it is what ends the heir's
            // bounded wait — so it carries Loom's word too.
            mail.answer(loom::Refused{"no bequest is held for you"});
            return;
        case Letters::Answer::Letter: {
            loom::Bequest b;
            b.role = c.role;
            b.items = box_->items;
            mail.answer(b);
            return;
        }
        case Letters::Answer::UnattestedLetter: {
            // The same bytes, the same correlation, the same sender — sent the
            // ordinary way. This is what an impersonator can produce, and what
            // the legitimate steward looks like if it ever answers OUTSIDE the
            // delivery that asked. Either way the heir must not adopt it.
            loom::Bequest plain;
            plain.role = c.role;
            plain.items = box_->items;
            mail.send(mail.sender(), plain, mail.correlation());
            return;
        }
        }
    }

    /// The predecessor's answer to PrepareShutdown lands here.
    void on(const loom::Bequest& letter, loom::Mail&) {
        for (const loom::Bytes& item : letter.items) {
            box_->received.push_back(item);
        }
    }

private:
    Letters* box_;
};

/// The tier-2 world: a real bus, the service in its role, the host's one wait trusting that role
/// on a clock that moves only when told, one ear, and a stand-in door to activate from.
struct FakeRig {
    loom::Switchboard bus;
    loom::host::ManualClock clock;
    loom::host::Reactor wait{bus, clock};
    Heard heard;
    Letters letters;
    std::vector<std::string> tape; ///< delivered schema names, in order
    loom::WeaveId service{};
    loom::WeaveId ear{};
    loom::WeaveId door{};
    loom::WeaveId other_door{}; ///< a SECOND lifecycle initiator: a different lineage
    loom::WeaveId impostor{};   ///< an ordinary weave that merely knows the shape
    loom::WeaveId steward{};    ///< mounted only where a test wants one
    std::int64_t next_sequence = 1;

    FakeRig() : FakeRig(zengine::timer::fallback_vocabulary()) {}

    /// A rig whose service spends a catalog the CASE chose. Everything else is
    /// identical; the only difference under test is which operator truth the
    /// running Timer executes.
    explicit FakeRig(zengine::op::Catalog operators) {
        heard.clock = &clock;
        wait.trust(kTimerRole);
        // Into its ROLE: the host's wait reads the role's claim, exactly as the .so's is read.
        service = mount_role<TimerWeave>(bus, kTimerRole, std::move(operators));
        ear = loom::mount<Ear>(bus, heard);
        door = zengine::testing::mount_door(bus);
        other_door = zengine::testing::mount_door(bus);
        impostor = loom::mount<Impostor>(bus);
        bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered) {
                return;
            }
            tape.push_back(ev.schema_name);
            if (ev.schema_name == "StartTimer") {
                ask_now = clock.now_ms(); // the instant a schedule was asked for
            }
            if (ev.schema_name == loom::PrepareShutdown::zen_name) {
                // The time the predecessor was last handed: its letter's `now`.
                letter_now = clock.now_ms();
            }
            if (due_wake_of(ev, service)) {
                ++wakes;
            }
        });
    }

    std::int64_t wakes = 0;       ///< the host's wakes the service was handed, its time come
    std::int64_t letter_now = -1; ///< the clock when the predecessor was asked to write
    std::int64_t ask_now = -1;    ///< the clock when a StartTimer was last delivered

    /// The bus turned until it is empty, and no time passing: what is queued now, delivered.
    void settle() {
        host_loop(bus, wait, clock, [] { return true; });
    }

    /// Put a steward in the `zen.manager` slot. Deliberately NOT part of the
    /// default world: most tier-2 pins are about a service with no steward at
    /// all, which is also the direct-load case.
    void mount_steward(Letters::Answer answer) {
        letters.answer = answer;
        steward = mount_role<FakeSteward>(bus, loom::kManagerRole, letters);
    }

    /// Where a schema first appears on the tape (or -1).
    std::int64_t first(const std::string& schema) const {
        for (std::size_t i = 0; i < tape.size(); ++i) {
            if (tape[i] == schema) {
                return static_cast<std::int64_t>(i);
            }
        }
        return -1;
    }

    /// One of the service's own counters, off its real snapshot.
    std::int64_t count(const char* field) {
        loom::Unverified u = loom::parse(bus.snapshot_bytes(service));
        loom::Admission a = loom::admit(u, loom::schema_of<TimerState>());
        REQUIRE(a.ok());
        return a.value().get(field)->as_int();
    }

    /// Ask the service to write its letter, as the steward would, and decode it: the bus turned with
    /// no wait between, so no time passes and nothing fires between the asks before it and it.
    std::vector<TimerHandoffEntry> prepare_shutdown() {
        REQUIRE(steward.valid());
        const std::size_t before = letters.received.size();
        bus.send_as(steward, service,
                    loom::Message(loom::to_value(loom::PrepareShutdown{}), steward, steward, 77));
        bus.drain_until_idle(); // delivered in this turn: no wake between the asks and the letter
        REQUIRE(letters.received.size() == before + 1);
        const std::optional<TimerHandoff> h =
            loom::claim_item<TimerHandoff>(letters.received.back());
        REQUIRE(h.has_value());
        return h->entries;
    }

    /// Speak AS a weave (stamped sender, authorized against ITS grant) — how
    /// every ask reaches the service in this tier.
    template <class T>
    void ask_as(loom::WeaveId who, const T& msg) {
        bus.send_as(who, service, loom::Message(loom::to_value(msg), who, who, 0));
    }

    /// Activate the service the way the control door does: a directed,
    /// stamped `zen.Activated` carrying the next sequence of this lineage.
    void activate(std::int64_t sequence) {
        activate_as(door, sequence);
    }
    void activate() { activate(next_sequence++); }

    /// Activate from an arbitrary stamped sender — how a "different lineage"
    /// is expressed, and how an invalid one is forged.
    void activate_as(loom::WeaveId who, std::int64_t sequence) {
        zengine::testing::order_activation(bus, who, service, sequence);
    }

    /// The same gesture with the two sequences DISAGREEING: Loom is asked to
    /// attest one number while the payload states another.
    void activate_mismatched(std::int64_t announce, std::int64_t claim) {
        zengine::testing::order_activation(bus, door, service, announce, claim);
    }

    /// A perfectly-shaped activation from an ordinary weave holding nothing but
    /// the grant — the forgery the attestation exists to refuse.
    void forge_activation(std::int64_t sequence) {
        zengine::testing::order_activation(bus, impostor, service, sequence);
    }

    /// The host's loop until the service has been handed `n` more wakes, or the host is quiet --
    /// a Timer with nothing standing claims no time, so nothing wakes it. What the last wake set
    /// off is delivered before it returns.
    void run_wakes(std::int64_t n, bool start = false) {
        if (start) {
            activate();
        }
        const std::int64_t target = wakes + n;
        host_loop(bus, wait, clock, [&] { return wakes >= target; });
    }

    /// The host's loop until the clock reads `ms`, and what is due then delivered.
    void run_until(std::int64_t ms) {
        host_loop(bus, wait, clock, [] { return false; }, ms);
    }

    /// The host's loop until `id` has been heard `times` more, or the host is quiet.
    void run_until_fired(const std::string& id, std::size_t times = 1) {
        const auto heard_of = [&] {
            return static_cast<std::size_t>(std::count(heard.fired.begin(), heard.fired.end(), id));
        };
        const std::size_t target = heard_of() + times;
        host_loop(bus, wait, clock, [&] { return heard_of() >= target; });
    }
};

// ---- tiers 3/4 rig: the real kernel, under the host's wait -------------------------

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
    std::vector<zengine::snake::SnakeVisual> visuals;
    std::vector<zengine::probe::ProbeReport> reports; ///< the continuity lane's answers
    std::vector<TimerResolution> receipts; ///< the keystone lane's own order receipts
    std::vector<std::string> declines;     ///< preparation asks a Timer refused
    std::int64_t letters_to_me = 0;        ///< clock reads the ruler took

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
/// answers, and accepts everything these lanes watch move.
class Witness
    : public loom::WeaveBase<Witness, WitnessState,
                             loom::Accept<loom::Result, loom::Ack, loom::Refused, TimerFired,
                                          TimerReady,
                                          zengine::snake::SnakeVisual,
                                          zengine::probe::ProbeReport, loom::Bequest,
                                          TimerResolution, TimerCandidateDeclined>,
                             loom::Emit<>> {
public:
    explicit Witness(Seen& seen) : seen_(&seen) {}
    /// THE RULER. Asking a Timer to describe itself changes nothing in its schedule (TIMER-03's
    /// second MEANS), so the suite uses it as an exact clock read: send one, catch the letter,
    /// and know how much of a standing schedule is left at that instant. The letter must be
    /// ACCEPTED to be seen, because the tap carries a payload only for a delivery.
    void on(const loom::Bequest&, loom::Mail&) { ++seen_->letters_to_me; }
    /// Receipts for the orders the suite itself places, kept in arrival order so
    /// "applied exactly once" is a count and not an impression.
    void on(const TimerResolution& t, loom::Mail&) { seen_->receipts.push_back(t); }
    /// What a Timer says when the suite offers it a preparation it should not
    /// accept — a live one, or a second ask.
    void on(const TimerCandidateDeclined& d, loom::Mail&) { seen_->declines.push_back(d.reason); }
    void on(const zengine::probe::ProbeReport& p, loom::Mail&) { seen_->reports.push_back(p); }
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

/// One letter, caught on the wire. `zen.Bequest` crosses this bus twice in a graceful
/// succession -- predecessor to steward, steward to heir -- and a tap sees both, payload
/// included, so the continuity lane proves what the predecessor OFFERED without reaching inside
/// either weave: "two seconds remaining" is read off the actual message, through the real gate.
struct CaughtLetter {
    loom::WeaveId target{};
    loom::WeaveId sender{};
    loom::Value payload;
    std::int64_t at_ms = 0; ///< the host's clock when it landed
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
    std::vector<CaughtLetter> letters;

    /// The host waits on the Timer's role, as Workshop and the app do.
    Rig() {
        wait.trust(kTimerRole);
        watch();
    }

    loom::WeaveId mount_witness() {
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
        reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
        reach.allow_to_any(StartTimer::zen_name, StartTimer::zen_version);
        reach.allow_to_any(StartRoleTimer::zen_name, StartRoleTimer::zen_version);
        reach.allow_to_any(EnsureTimer::zen_name, EnsureTimer::zen_version);
        reach.allow_to_any(EnsureRoleTimer::zen_name, EnsureRoleTimer::zen_version);
        reach.allow_to_any(CancelTimer::zen_name, CancelTimer::zen_version);
        reach.allow_to_any(CancelAllMyTimers::zen_name, CancelAllMyTimers::zen_version);
        reach.allow_to_any(loom::PrepareShutdown::zen_name, loom::PrepareShutdown::zen_version);
        reach.allow_to_any(PrepareTimerHandover::zen_name, PrepareTimerHandover::zen_version);
        reach.allow_to_any(zengine::probe::AskProbe::zen_name,
                           zengine::probe::AskProbe::zen_version);
        reach.allow_to_any(zengine::probe::RestartProbe::zen_name,
                           zengine::probe::RestartProbe::zen_version);
        reach.allow_to_any(zengine::probe::CancelProbe::zen_name,
                           zengine::probe::CancelProbe::zen_version);
        return loom::mount_granted<Witness>(bus, std::move(reach), seen);
    }

    /// What the tap counts: every wake the host's wait handed the Timer's role, the letters that
    /// crossed, and how many of each shape were handled.
    std::int64_t wakes = 0;
    std::map<std::string, std::int64_t> handled;
    std::string stop_on;          ///< stop the bus the moment this shape is handled...
    std::int64_t stop_target = 0; ///< ...for this count's time

    void watch() {
        bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered) {
                return;
            }
            ++handled[ev.schema_name];
            if (!stop_on.empty() && ev.schema_name == stop_on &&
                handled[ev.schema_name] >= stop_target) {
                bus.stop();
            }
            if (ev.target == bus.role_holder(kTimerRole) && due_wake_of(ev, ev.target)) {
                ++wakes;
            }
            if (ev.schema_name == loom::Bequest::zen_name && ev.payload != nullptr) {
                letters.push_back(CaughtLetter{ev.target, ev.sender, *ev.payload, clock.now_ms()});
            }
        });
    }

    /// The bus turned until it is empty, and no time passing.
    void settle() {
        host_loop(bus, wait, clock, [] { return true; });
    }

    /// The host's loop until one more message of `schema` has been handled, stopped the moment it
    /// is, with whatever is queued behind it left queued: the lanes' ruler. The clock moves only
    /// when the wait rests to a time due, so stopping at a named message puts it at a known
    /// instant, and a case can place a send between two deliveries. `bound_ms` is a hang guard,
    /// not a schedule.
    void pump_until(const std::string& schema, std::int64_t bound_ms = 10000) {
        const std::int64_t target = handled[schema] + 1;
        const std::int64_t until = clock.now_ms() + bound_ms;
        stop_on = schema;
        stop_target = target;
        host_loop(bus, wait, clock, [&] { return handled[schema] >= target; }, until,
                  /*at_once=*/true);
        stop_on.clear();
        REQUIRE_MESSAGE(handled[schema] >= target, schema, " was not handled within ", bound_ms,
                        " ms");
    }

    /// The host's loop for `ms` on the clock.
    void run_for(std::int64_t ms) {
        host_loop(bus, wait, clock, [] { return false; }, clock.now_ms() + ms);
    }

    /// The host's loop until the clock reads `ms`.
    void run_until(std::int64_t ms) {
        host_loop(bus, wait, clock, [] { return false; }, ms);
    }

    /// Read one integer field, no time passing.
    std::int64_t poke_int(loom::WeaveId target, const char* field) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(loom::PokeRead{field}), loom::WeaveId{},
                                       witness, corr));
        settle();
        const Seen::Answer* a = seen.find(corr);
        REQUIRE(a != nullptr);
        REQUIRE_MESSAGE(a->kind == 0, "poke refused: ", a->text);
        return std::stoll(a->text);
    }

    /// Drive the steward, as an operator would: one command, its own
    /// correlation, answered on the witness's ledger.
    template <class Command>
    std::uint64_t command(const Command& c) {
        const std::uint64_t corr = next_corr++;
        bus.send_as(witness, manager,
                    loom::Message(loom::to_value(c), witness, witness, corr));
        return corr;
    }

    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const std::uint64_t corr = next_corr++;
        bus.send_as(witness, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{name, path, zengine::testing::role_set(role)}), witness,
                                  witness, corr));
        settle();
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

    /// The same, addressed to the TIMER ROLE — which is how every production
    /// consumer reaches the Timer, and therefore the only addressing whose
    /// behaviour across a replacement boundary the package promises anything
    /// about. Resolved at delivery, so where it lands is decided by the topology
    /// at that instant rather than at this one.
    template <class T>
    void ask_role(const T& msg) {
        bus.send_as_to_role(witness, kTimerRole,
                            loom::Message(loom::to_value(msg), witness, witness, 0));
    }

    /// Speak AS another weave — the host's root authority, used here only to put
    /// on the wire exactly the message that weave's own binding sends, at an
    /// instant the test chooses. Nothing unsayable is said.
    template <class T>
    void ask_as(loom::WeaveId who, loom::WeaveId target, const T& msg) {
        bus.send_as(who, target, loom::Message(loom::to_value(msg), who, who, 0));
    }

    /// "Where do you stand?" — one message out, one report back.
    zengine::probe::ProbeReport ask_probe(loom::WeaveId probe) {
        const std::size_t before = seen.reports.size();
        ask(probe, zengine::probe::AskProbe{});
        pump_until(zengine::probe::ProbeReport::zen_name);
        REQUIRE(seen.reports.size() == before + 1);
        return seen.reports.back();
    }

    /// The one entry a Timer letter carries, decoded through the real gate.
    static std::vector<TimerHandoffEntry> handoff_of(const CaughtLetter& l) {
        const loom::Bequest letter = loom::from_value<loom::Bequest>(l.payload);
        for (const loom::Bytes& item : letter.items) {
            if (const std::optional<TimerHandoff> h = loom::claim_item<TimerHandoff>(item)) {
                return h->entries;
            }
        }
        return {};
    }

    /// The host's loop until `done()` holds, at most `bound_ms` on the clock: a wait for
    /// something that arrives, not a measure of time passing. Fails naming `what`.
    template <class Done>
    void wait_until(Done done, const char* what, std::int64_t bound_ms = 5000) {
        host_loop(bus, wait, clock, done, clock.now_ms() + bound_ms);
        REQUIRE_MESSAGE(done(), what, " did not arrive within ", bound_ms, " ms");
    }

    /// The answer correlated `corr`, waited for under the host's loop (`wait_until`); a copy,
    /// since the ledger grows under every later pump.
    Seen::Answer wait_answer(std::uint64_t corr, const char* what, std::int64_t bound_ms = 5000) {
        wait_until([&] { return seen.find(corr) != nullptr; }, what, bound_ms);
        return *seen.find(corr);
    }

    /// Poke and wait for its answer (`wait_answer`).
    template <class Poke>
    Seen::Answer poke_timed(loom::WeaveId target, const Poke& p) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(p), loom::WeaveId{}, witness, corr));
        return wait_answer(corr, "timed poke");
    }
};

} // namespace

// ============================================================================
// Tier 1 — the locked contract, pinned by content-id
// ============================================================================

TEST_CASE("contract: ZEN_SHAPE spellings derive the locked schemas exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;

    // The V1 shapes, exactly as locked.
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
    // The address is contract too, and so is the time a Timer claims while it must learn the
    // time: a reading every clock a host's wait keeps has already passed.
    CHECK(std::string(kTimerRole) == "zengine.timer");
    CHECK(kTimePast == 0);
}

TEST_CASE("contract: the ordered continuity shapes are frozen the same way, and their Text fields "
          "are contract rather than taste") {
    using loom::Kind;
    using loom::SchemaBuilder;

    // The ORDERED forms. New shapes rather than fields on StartTimer: a frozen
    // (name, version) keeps meaning exactly what it meant, and the raw
    // fire-and-forget vocabulary keeps its own promise unchanged.
    CHECK(schema_of<EnsureTimer>()->content_id() == SchemaBuilder("EnsureTimer", 1)
                                                        .field("id", Kind::Text)
                                                        .field("delay_ms", Kind::Int)
                                                        .field("repeat", Kind::Bool)
                                                        .field("preferred", Kind::Text)
                                                        .field("fallback", Kind::Text)
                                                        .build()
                                                        ->content_id());
    CHECK(schema_of<EnsureRoleTimer>()->content_id() == SchemaBuilder("EnsureRoleTimer", 1)
                                                            .field("id", Kind::Text)
                                                            .field("delay_ms", Kind::Int)
                                                            .field("repeat", Kind::Bool)
                                                            .field("role", Kind::Text)
                                                            .field("preferred", Kind::Text)
                                                            .field("fallback", Kind::Text)
                                                            .build()
                                                            ->content_id());
    // The receipt. `resolved` and `reason` are Text because a receipt is read by
    // people and consoles as well as by code — a numeric code would need this
    // header to be readable.
    CHECK(schema_of<TimerResolution>()->content_id() == SchemaBuilder("TimerResolution", 1)
                                                            .field("id", Kind::Text)
                                                            .field("resolved", Kind::Text)
                                                            .field("reason", Kind::Text)
                                                            .build()
                                                            ->content_id());

    // The letter. `requester` is TEXT and that is contract, not taste: a WeaveId
    // is unsigned 64-bit and the wire's Int is signed, so an Int field would
    // silently narrow the top half of the range — and this field is what a
    // firing is addressed to. `remaining_ms` and NOT a due time is the other
    // contract: an absolute deadline from the predecessor's clock epoch would be
    // a number with no meaning in the successor's.
    CHECK(schema_of<TimerHandoffEntry>()->content_id() ==
          SchemaBuilder("TimerHandoffEntry", 1)
              .field("requester", Kind::Text)
              .field("id", Kind::Text)
              .field("role", Kind::Text)
              .field("delay_ms", Kind::Int)
              .field("repeat", Kind::Bool)
              .field("remaining_ms", Kind::Int)
              .build()
              ->content_id());
    CHECK(schema_of<TimerHandoff>()->version() == 1);

    // The service's state: seven counters, and the table as a reload carries it, in the letter's
    // own records -- one description of schedule progress, not two.
    CHECK(schema_of<TimerState>()->version() == 4);
    CHECK(schema_of<TimerState>()->content_id() ==
          SchemaBuilder("TimerState", 4)
              .field("wakes", Kind::Int)
              .field("fired", Kind::Int)
              .field("active", Kind::Int)
              .field("dropped", Kind::Int)
              .field("deferred_dropped", Kind::Int)
              .field("inherited", Kind::Int)
              .field("gone", Kind::Int)
              .list("standing", loom::type_message(schema_of<TimerHandoffEntry>()))
              .build()
              ->content_id());

    // The stable spellings. These travel on the wire and appear in receipts a
    // stranger reads, so a rename is a contract change and a red test.
    CHECK(std::string(kPreserveRemaining) == "preserve_remaining");
    CHECK(std::string(kRestartDelay) == "restart_delay");
    CHECK(std::string(kDrop) == "drop");
    CHECK(std::string(kResolutionPreserved) == "preserved_remaining");
    CHECK(std::string(kResolutionRestarted) == "restarted_delay");
    CHECK(std::string(kResolutionDropped) == "dropped");
    CHECK(std::string(kResolutionRefused) == "refused");

    // Round-tripping the menu: every choice has exactly one spelling and every
    // spelling exactly one choice, and nothing else parses.
    CHECK(continuity_from(kPreserveRemaining) == Continuity::PreserveRemaining);
    CHECK(continuity_from(kRestartDelay) == Continuity::RestartDelay);
    CHECK(continuity_from(kDrop) == Continuity::Drop);
    CHECK_FALSE(continuity_from("").has_value());
    CHECK_FALSE(continuity_from("preserved_remaining").has_value()); // a receipt is not an order

    // The DEFAULT ORDER is contract: prefer keeping the remaining time, accept
    // restarting. It is what makes a graceful replacement continuous and an
    // initial load honest, and a drift either way is a behaviour change.
    const ContinuityOrder fallback_default;
    CHECK(fallback_default.preferred == Continuity::PreserveRemaining);
    REQUIRE(fallback_default.fallback.has_value());
    CHECK(*fallback_default.fallback == Continuity::RestartDelay);
    CHECK(fallback_spelling(fallback_default) == kRestartDelay);

    // A REQUIRED preference travels as an empty fallback — the one spelling that
    // means "nothing else will do".
    ContinuityOrder required;
    required.fallback = std::nullopt;
    CHECK(fallback_spelling(required).empty());

    // The bounds are published, not discovered as leaks.
    CHECK(kMaxHandoffEntries == 32);
    CHECK(kMaxDeferredOps == 32);
    CHECK(kBootstrapWakes == 2);
}

TEST_CASE("contract: the handover's preparation conversation is frozen too, and it adds no second "
          "way to describe a schedule") {
    using loom::Kind;
    using loom::SchemaBuilder;

    CHECK(schema_of<PrepareTimerHandover>()->content_id() ==
          SchemaBuilder("PrepareTimerHandover", 1)
              .field("transaction", Kind::Int)
              .field("continuity", Kind::Text)
              .build()
              ->content_id());
    CHECK(schema_of<TimerCandidatePrepared>()->content_id() ==
          SchemaBuilder("TimerCandidatePrepared", 1).field("transaction", Kind::Int).build()
              ->content_id());
    // A decline carries its own self-contained reason, for the same reason a
    // TimerResolution does: an operator reading it must not need this header.
    CHECK(schema_of<TimerCandidateDeclined>()->content_id() ==
          SchemaBuilder("TimerCandidateDeclined", 1)
              .field("transaction", Kind::Int)
              .field("reason", Kind::Text)
              .build()
              ->content_id());

    // The two plans, spelled as they travel.
    CHECK(std::string(kInheritFromIncumbent) == "inherit");
    CHECK(std::string(kStartFresh) == "fresh");
    CHECK(kPreparedClaimWakes == 8);

    // THE LETTER IS THE GRACEFUL PATH'S: prepared replacement reuses `TimerHandoff` exactly, so
    // there is one interpretation of schedule progress in this package and not two.
    CHECK(schema_of<TimerHandoff>()->version() == 1);
    CHECK(schema_of<TimerHandoffEntry>()->version() == 1);
    // ...and the service's own state did not have to grow to carry a preparation.
    CHECK(schema_of<TimerState>()->version() == 4);
}

// ============================================================================
// Tier 2 — the service through a real bus, handed its time by the host's wait
// ============================================================================

// ---- the role's manual ----------------------------------------------------

TEST_CASE("the Timer answers a shape's section from its own page, and its page covers every shape "
          "it accepts") {
    // Unactivated, so it claims nothing and the bus drains: the words are the image's, said
    // whether or not this incarnation is live.
    FakeRig r;
    const std::vector<std::string> described = described_shapes_of(r.bus, r.service);
    REQUIRE_FALSE(described.empty());
    CHECK(check_role_manual(r.bus, kTimerRole) >= described.size());
    check_page_names_accepted(zengine::manual::pages::zengine_timer::kManual, described);

    // A request is answered from Commands, the host's wait's word from Hears.
    ShapeDocumentReader reader(r.bus, kTimerRole);
    CHECK(reader.ask(EnsureTimer::zen_name, EnsureTimer::zen_version).group == "Commands");
    CHECK(reader.ask(loom::host::Waited::zen_name, loom::host::Waited::zen_version).group ==
          "Hears");
    CHECK(reader.ask(TimerFired::zen_name, TimerFired::zen_version).refusal ==
          "zengine.timer does not accept `TimerFired` v1");
}

// ---- the activation law (TIMER-01) ------------------------------------------
// Time is the host's to hand over: a Timer claims the time it waits for as its role, and the
// host's wait wakes it then. `wait.sources()` is what the wait holds of that claim.

namespace {

/// The time the Timer's role claims, as the host's wait holds it: none when it claims no time.
std::optional<std::int64_t> claimed_until(const FakeRig& r) {
    for (const loom::host::Reactor::Source& s : r.wait.sources()) {
        if (s.role == kTimerRole) {
            return s.until_ms;
        }
    }
    return std::nullopt;
}

/// The host's wait's word, `due` at `now_ms`, as only the host may say it.
loom::host::Waited woke_at(std::int64_t now_ms) {
    loom::host::Waited w;
    w.held = true;
    w.due = true;
    w.now_ms = now_ms;
    return w;
}

} // namespace

TEST_CASE("nothing wakes an unactivated incarnation: the host's word before activation is inert") {
    FakeRig r;
    // The host's own word, no sender, arriving at a service that has not been told it is live.
    // THIS is what makes a wake meant for a predecessor harmless after a reload: the new instance
    // begins unactivated, so it has no time to be handed.
    r.bus.send(r.service, loom::Message(loom::to_value(woke_at(500))));
    r.ask_as(r.ear, StartTimer{"held", 5, false});
    r.run_wakes(4); // no activation: nothing claimed, so the host is quiet
    CHECK(r.count("wakes") == 0);
    CHECK(r.heard.ready == 0);
    CHECK(r.heard.fired.empty());
    CHECK(r.bus.pending() == 0);
    CHECK_FALSE(claimed_until(r).has_value());

    // Then activate for real: the held ask is anchored at the first time handed over and fires
    // its delay later, on the host's clock, not the forged word's.
    r.run_wakes(4, /*start=*/true);
    CHECK(r.heard.ready == 1);
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired_at[0] == 5);
}

TEST_CASE("an invalid, duplicate, or non-newer activation changes nothing") {
    FakeRig r;

    // Sequence 0 and negatives are not activations — the contract says the
    // sequence is positive, and a service that accepted one would be inventing
    // a lineage the operator never authored.
    r.activate(0);
    r.activate(-7);
    r.settle();
    CHECK(r.heard.ready == 0);
    CHECK_FALSE(claimed_until(r).has_value());

    // A real one makes it available.
    r.activate(); // sequence 1
    r.settle();
    CHECK(r.heard.ready == 1);
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.run_until(20);
    REQUIRE(r.heard.fired.size() == 2);

    // THE SAME activation again: a duplicate. It does not republish availability and does not
    // disturb the standing schedule. An OLDER sequence from the same sender is a replay: same
    // answer.
    r.activate(1);
    r.settle();
    r.activate(1);
    r.run_until(40);
    CHECK(r.heard.ready == 1);
    CHECK(r.heard.fired_at == std::vector<std::int64_t>{10, 20, 30, 40}); // one lattice, unmoved
}

TEST_CASE("a newer activation re-announces; a different sender begins a new lineage — either "
          "way, one schedule") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.activate(); // sequence 1
    r.run_until(20);
    REQUIRE(r.heard.ready == 1);
    REQUIRE(r.heard.fired.size() == 2);

    // A NEWER activation from the same lineage republishes availability (the notice is once per
    // ACCEPTED activation) and keeps the table: activation is not state migration.
    r.activate(); // sequence 2
    r.run_until(30);
    CHECK(r.heard.ready == 2);

    // A DIFFERENT SENDER — a second lifecycle initiator — is a new lineage. It is accepted (the
    // Loom attested it; activation/docs/activation.md says why an attested sender is a lineage),
    // and its sequence is 1: LOWER than the current one, which is exactly the point. Sequences
    // are only comparable WITHIN a lineage, so a new sender's 1 is not a replay of the old
    // sender's 2.
    r.activate_as(r.other_door, /*sequence=*/1);
    r.run_until(50);
    CHECK(r.heard.ready == 3);
    // Every rung once, on its lattice: no activation doubled the schedule or moved it.
    CHECK(r.heard.fired_at == std::vector<std::int64_t>{10, 20, 30, 40, 50});
}

TEST_CASE("only the host's word is time: a wake from a participant moves nothing") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"once", 50, false});
    r.run_wakes(1, /*start=*/true); // the time learned, 0, and the deadline claimed
    REQUIRE(r.heard.fired.empty());
    CHECK(claimed_until(r) == std::optional<std::int64_t>(50));
    const std::int64_t wakes = r.count("wakes");

    // An ordinary weave says the wait woke the Timer at 1000, through the honest API: a
    // perfectly shaped `Waited`, stamped with its sender. It is nothing -- no time, no firing.
    r.bus.send_as(r.ear, r.service,
                  loom::Message(loom::to_value(woke_at(1000)), r.ear, r.ear, 0));
    r.settle();
    CHECK(r.heard.fired.empty());
    CHECK(r.count("wakes") == wakes);
    CHECK(claimed_until(r) == std::optional<std::int64_t>(50));

    // The host's own wait still wakes it at its time.
    r.run_wakes(1);
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired_at[0] == 50);
}

TEST_CASE("a deadline saturates rather than wraps: the farthest delay claims the last reading") {
    // A wrapped deadline would read as overdue at once and fire forever; a saturated one is
    // claimed as the last reading a clock has, and the wait rests until then.
    FakeRig r;
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    r.ask_as(r.ear, StartTimer{"near", 5, false});
    r.ask_as(r.ear, StartTimer{"far", kMax, false});
    r.activate();
    r.run_until_fired("near");
    CHECK(r.clock.now_ms() == 5);
    CHECK(claimed_until(r) == std::optional<std::int64_t>(kMax));
    CHECK(r.heard.fired == std::vector<std::string>{"near"});
}

TEST_CASE("an activation announces availability once, and a Timer with nothing standing claims "
          "no time") {
    FakeRig r;
    r.run_wakes(3, /*start=*/true);
    CHECK(r.heard.ready == 1); // hello, once the decision is made
    // Nothing standing: the claim names no time, the bus is empty, and the host rests with
    // nothing to wait for -- nothing is queued to keep time.
    CHECK_FALSE(claimed_until(r).has_value());
    CHECK(r.bus.pending() == 0);
    const std::int64_t wakes = r.count("wakes");
    r.run_until(10000);
    CHECK(r.heard.ready == 1); // never again, however long it runs
    CHECK(r.count("wakes") == wakes); // and woken by nothing
}

TEST_CASE("a one-shot fires once, on time, and is gone") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"once", 25, false});
    r.run_wakes(6, /*start=*/true);
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired[0] == "once");
    CHECK(r.heard.fired_at[0] == 25); // woken at its deadline, not past it
    r.run_until(1000);
    CHECK(r.heard.fired.size() == 1); // spent means spent
    CHECK_FALSE(claimed_until(r).has_value());
}

TEST_CASE("an immediate one-shot fires at the time first handed; a 0ms repeat is clamped") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"asap", -5, false}); // negative: due at once
    r.ask_as(r.ear, StartTimer{"fast", 0, true});   // 0ms repeat: clamped to 1ms
    r.activate();
    r.run_until(2);
    REQUIRE(r.heard.fired.size() == 3);
    CHECK(r.heard.fired[0] == "asap");
    CHECK(r.heard.fired_at[0] == 0); // due already: fired at the time the wait handed over
    // The clamp is what makes a "0ms" repeat a schedule instead of a hot spin: the clock MOVES
    // 1ms per firing (a deadline that stayed at 0 forever would wake at t=0 and never satisfy
    // these stamps).
    CHECK(r.heard.fired[1] == "fast");
    CHECK(r.heard.fired_at[1] == 1);
    CHECK(r.heard.fired[2] == "fast");
    CHECK(r.heard.fired_at[2] == 2);
}

// ---- the delay a Timer INTERPRETS ------------------------------------------
// The semantic transform, pinned as a table. What a weaver writes and what the Timer schedules
// are two numbers, and a firing stamp proves a schedule behaved without stating the rule. An
// entry stores what the normalization answered as `TimerHandoffEntry.delay_ms`, so a letter is
// where a suite reads the transform's answer on the wire, through the real gate -- a test that
// called the arithmetic would move with it and notice nothing.

namespace {

/// Ask a fresh service to schedule exactly one timer, then ask it to describe
/// itself, and answer with the delay it recorded.
std::int64_t interpreted_delay_with(zengine::op::Catalog operators, std::int64_t delay_ms,
                                    bool repeat) {
    FakeRig r(std::move(operators));
    r.mount_steward(Letters::Answer::Refuse);
    r.run_wakes(3, /*start=*/true);
    // Queued so the letter is written in the SAME turn the ask lands in, before any wake can
    // fire and retire a one-shot that is already due. Same reason the due-or-overdue letter
    // case queues its ask this way.
    r.ask_as(r.ear, StartTimer{"probe", delay_ms, repeat});
    const std::vector<TimerHandoffEntry> offered = r.prepare_shutdown();
    REQUIRE(offered.size() == 1);
    return offered[0].delay_ms;
}

std::int64_t interpreted_delay(std::int64_t delay_ms, bool repeat) {
    return interpreted_delay_with(zengine::timer::fallback_vocabulary(), delay_ms, repeat);
}

} // namespace

TEST_CASE("the delay a Timer interprets, over the whole matrix") {
    // A repeating delay below 1ms is a hot spin wearing a timer's clothes; a
    // negative delay is due at once. Those two sentences are the rule,
    // and this is every edge they have.
    constexpr std::int64_t kBig = 86'400'000;               // a day
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();

    CHECK(interpreted_delay(-500, false) == 0);
    CHECK(interpreted_delay(-500, true) == 1);
    CHECK(interpreted_delay(-1, false) == 0);
    CHECK(interpreted_delay(-1, true) == 1);
    CHECK(interpreted_delay(0, false) == 0);
    CHECK(interpreted_delay(0, true) == 1);
    CHECK(interpreted_delay(1, false) == 1);
    CHECK(interpreted_delay(1, true) == 1);
    CHECK(interpreted_delay(2, false) == 2);
    CHECK(interpreted_delay(2, true) == 2);
    CHECK(interpreted_delay(kBig, false) == kBig);
    CHECK(interpreted_delay(kBig, true) == kBig);
    CHECK(interpreted_delay(kMax, false) == kMax);
    CHECK(interpreted_delay(kMax, true) == kMax);

    // The floor is the ONLY thing the rule moves. Everything at or above it is
    // returned untouched, in both modes, which is what makes "normalize" an
    // honest word for it rather than "adjust".
    CHECK(interpreted_delay(kMax - 1, true) == kMax - 1);
}

TEST_CASE("a running Timer's delay comes from the operator, and the composition is what runs") {
    // The same claim the matrix above makes, from the other side: not that the
    // answer is right, but that it came from `timer.normalize_delay`. A Timer
    // that had kept a private three-line `clamp_delay` would schedule exactly the
    // same 1ms and move this counter by ZERO.
    FakeRig r;
    const std::uint64_t before = zengine::op::invocations();
    r.ask_as(r.ear, StartTimer{"beat", -500, true});
    r.activate();
    r.run_until(2);

    // Two maxes and a select: the three nodes of the composition, and nothing
    // beside them. A fourth node, or a native shortcut, would move this.
    CHECK(zengine::op::invocations() - before == 3);

    // ...and the number the operator answered is the number that was scheduled.
    REQUIRE(r.heard.fired_at.size() >= 2);
    CHECK(r.heard.fired[0] == "beat");
    CHECK(r.heard.fired_at[0] == 1);
    CHECK(r.heard.fired_at[1] == 2);
}

TEST_CASE("the Ensure comparison spends the same delay rule the write does") {
    // `EnsureTimer` decides whether a draft IS the standing schedule, and it is right only if it
    // interprets the draft exactly as the write interpreted the ask. Here the standing beat is
    // 1ms repeating and the draft says -500 repeating: the SAME schedule, and only the shared
    // rule makes them so.
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 1, true});
    r.run_wakes(3, /*start=*/true);
    REQUIRE(r.count("active") == 1);

    const std::uint64_t before = zengine::op::invocations();
    r.ask_as(r.ear, EnsureTimer{"beat", -500, true, kPreserveRemaining, ""});
    r.run_wakes(3);

    // ONE evaluation, three primitive invocations: the comparison normalized the
    // draft through the composition and then preserved, so no second write
    // normalized anything again.
    CHECK(zengine::op::invocations() - before == 3);

    REQUIRE(r.heard.resolutions.size() == 1);
    CHECK(r.heard.resolutions[0].id == "beat");
    CHECK(r.heard.resolutions[0].resolved == kResolutionPreserved);
}

TEST_CASE("replace a primitive under the rule and the RUNNING Timer moves with it") {
    // THE DECISIVE WITNESS. Everything else here could come from two independent implementations
    // that happen to agree; this cannot: the leaf is swapped underneath, nothing structural
    // notices, and the Timer's own scheduling and an independent reader's answer change TOGETHER.
    const zengine::op::Catalog sabotaged = zengine::testing::sabotaged_operators();

    CHECK(interpreted_delay(-500, true) == 1);
    CHECK(interpreted_delay_with(sabotaged, -500, true) ==
          zengine::testing::kSabotagedRepeatingDelay);

    // The stranger shares no line of code with the weave and never heard of it,
    // and it says the same new thing about the same catalog.
    const stranger::Reading read = stranger::ask(
        sabotaged, "timer.normalize_delay", {{"delay_ms", "-500"}, {"repeat", "true"}});
    REQUIRE_MESSAGE(read.ok, "refused: ", read.reason);
    CHECK(read.answer == "-500");

    const stranger::Reading honest =
        stranger::ask(zengine::timer::fallback_vocabulary(), "timer.normalize_delay",
                      {{"delay_ms", "-500"}, {"repeat", "true"}});
    REQUIRE(honest.ok);
    CHECK(honest.answer == "1");
}

TEST_CASE("a repeating timer holds its lattice") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.activate();
    r.run_until(30);
    // Anchored at the time first handed over, 0, and woken at each rung.
    REQUIRE(r.heard.fired.size() == 3);
    CHECK(r.heard.fired_at[0] == 10);
    CHECK(r.heard.fired_at[1] == 20);
    CHECK(r.heard.fired_at[2] == 30);
}

TEST_CASE("a LATE repeating timer gets one firing, never a burst (the catch-up clamp)") {
    // THE NO-BURST PIN: a mutation removing the clamp once left this suite green, so this case
    // stalls the host the way one wedged past several periods does to the lattice.
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.activate();
    r.run_until_fired("beat"); // the first firing (t=10)
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired_at[0] == 10); // the lattice's second rung is due at 20

    // ...and now the host stalls until t=75: rungs 20 through 70 are all in the past when the
    // wait next hands the time over. SIX deadlines missed, by construction.
    const std::int64_t resumed = 75;
    r.clock.advance_to(resumed);
    r.run_until(resumed + 20);

    // What arrives is ONE firing at the resuming instant, then a lattice re-anchored to the
    // stall — not six stale rungs delivered back to back. Without the clamp every one of them
    // arrives at t=75 (each claim names a rung still in the past), so the burst is exactly what
    // these stamps refuse.
    REQUIRE(r.heard.fired.size() == 4);
    CHECK(r.heard.fired_at[1] == resumed);      // the one catch-up firing
    CHECK(r.heard.fired_at[2] == resumed + 10); // re-anchored to NOW...
    CHECK(r.heard.fired_at[3] == resumed + 20); // ...and cadence restored
    CHECK(r.heard.fired_at[2] != 80);           // not the pre-stall lattice
}

TEST_CASE("a Timer is woken at its deadlines and at nothing else: one more wake an arm") {
    // The wakes are the host's: a Timer claims the soonest time it waits for and is woken then.
    // An ask is anchored at the next time handed over, so it claims a time already past and the
    // wait hands over the time at once -- one wake an arm -- and then one wake a deadline.
    FakeRig r;
    r.run_wakes(4, /*start=*/true);
    const std::int64_t settled = r.count("wakes");
    const std::int64_t at = r.clock.now_ms();

    r.ask_as(r.ear, StartTimer{"soon", 4, false});
    r.ask_as(r.ear, StartTimer{"far", 25, false});
    r.run_until(at + 1000);
    REQUIRE(r.heard.fired.size() == 2);
    CHECK(r.heard.fired_at[0] == at + 4);  // the deadline, not a step past it
    CHECK(r.heard.fired_at[1] == at + 25); // the next, with nothing between
    CHECK(r.count("wakes") - settled == 3); // the time learned, then each deadline
    CHECK(r.wakes == r.count("wakes"));     // every one the host's wait's own
}

// ---- the host rests only when the bus is empty -----------------------------------------------

namespace {

/// A consumer's reply to a firing.
struct RestReply {
    std::int64_t fired_at = 0;
    ZEN_SHAPE(RestReply, 1, ZEN_FIELD(fired_at));
};

struct RestHeard {
    std::vector<std::int64_t> fired_at;   ///< the host's clock at each firing the consumer heard
    std::vector<std::int64_t> replied_at; ///< the host's clock at each reply the listener heard
    const loom::host::Clock* clock = nullptr;
    loom::WeaveId listener{};
};

/// Answers every firing at once with a message to another weave.
class Replier : public loom::WeaveBase<Replier, EarState, loom::Accept<TimerFired>,
                                       loom::Emit<StartTimer, RestReply>> {
public:
    explicit Replier(RestHeard& heard) : heard_(&heard) {}
    void on(const TimerFired&, loom::Mail& mail) {
        heard_->fired_at.push_back(heard_->clock->now_ms());
        (void)mail.send(heard_->listener, RestReply{heard_->clock->now_ms()});
    }

private:
    RestHeard* heard_;
};

class ReplyListener
    : public loom::WeaveBase<ReplyListener, EarState, loom::Accept<RestReply>, loom::Emit<>> {
public:
    explicit ReplyListener(RestHeard& heard) : heard_(&heard) {}
    void on(const RestReply&, loom::Mail&) {
        heard_->replied_at.push_back(heard_->clock->now_ms());
    }

private:
    RestHeard* heard_;
};

} // namespace

TEST_CASE("a reply to a firing is delivered in the next turn, not after a rest: the host rests "
          "only when the bus is empty") {
    FakeRig r;
    RestHeard heard;
    heard.clock = &r.clock;
    heard.listener = loom::mount<ReplyListener>(r.bus, heard);
    const loom::WeaveId replier = loom::mount<Replier>(r.bus, heard);
    r.ask_as(replier, StartTimer{"tick", 25, true});
    r.activate();

    // The host's loop: a turn, then the wait. It rests -- the clock moves -- only with the bus
    // empty, so a reply sent from a firing is delivered at the instant of that firing.
    r.run_until(75);
    REQUIRE(heard.replied_at.size() == 3);
    CHECK(heard.replied_at == heard.fired_at);
    CHECK(heard.fired_at == std::vector<std::int64_t>{25, 50, 75}); // each on its deadline
    CHECK(r.wakes == r.count("wakes")); // every wake the host's own
}

TEST_CASE("the wait rests for nothing a Timer has not claimed: no Timer, no time") {
    loom::Switchboard bus;
    loom::host::ManualClock clock;
    loom::host::Reactor wait(bus, clock);
    wait.trust(kTimerRole);
    RestHeard heard;
    heard.clock = &clock;
    heard.listener = loom::mount<ReplyListener>(bus, heard);
    bus.send(heard.listener, loom::Message(loom::to_value(RestReply{0})));
    CHECK(bus.pending() == 1);

    // Nothing holds the Timer's role, so nothing claims a time: a wait with nothing to wait on
    // returns at once, and the clock does not move.
    const loom::host::Reactor::Woke woke = wait.after_turn();
    CHECK(woke.due == 0);
    CHECK(clock.now_ms() == 0);
    host_loop(bus, wait, clock, [] { return true; });
    CHECK(heard.replied_at == std::vector<std::int64_t>{0});
    CHECK(clock.now_ms() == 0);
}

TEST_CASE("cancel before activation: the timer never fires at all") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"gone", 10, true});
    r.ask_as(r.ear, CancelTimer{"gone"});
    r.run_wakes(5, /*start=*/true);
    CHECK(r.heard.fired.empty());
    CHECK_FALSE(claimed_until(r).has_value());
}

TEST_CASE("cancel between wakes: no firing lands after it, and the Timer claims no time") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.activate();
    r.run_until_fired("beat");
    REQUIRE(r.heard.fired.size() == 1);

    // Nothing is in flight between wakes -- the next rung is a time the wait holds, not a
    // message on the bus -- so the cancel lands before it and nothing more arrives.
    r.ask_as(r.ear, CancelTimer{"beat"});
    r.run_until(100);
    CHECK(r.heard.fired.size() == 1);
    CHECK_FALSE(claimed_until(r).has_value());
    CHECK(r.bus.pending() == 0);
}

TEST_CASE("asking again with the same id REPLACES the schedule (upsert)") {
    FakeRig r;
    r.ask_as(r.ear, StartTimer{"beat", 10, true});
    r.activate();
    r.run_until_fired("beat");
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired_at[0] == 10);

    r.ask_as(r.ear, StartTimer{"beat", 30, true}); // re-ask at t=10: new cadence
    r.run_until(70);
    // The old lattice's rung at 20 never fires; the new one runs 30ms from the re-ask.
    REQUIRE(r.heard.fired.size() == 3);
    CHECK(r.heard.fired_at[1] == 40);
    CHECK(r.heard.fired_at[2] == 70);
}

TEST_CASE("CancelAllMyTimers kills mine and nobody else's") {
    FakeRig r;
    Heard other_heard;
    other_heard.clock = &r.clock;
    const loom::WeaveId other = loom::mount<Ear>(r.bus, other_heard);

    r.ask_as(r.ear, StartTimer{"mine.a", 20, false});
    r.ask_as(r.ear, StartTimer{"mine.b", 30, false});
    r.ask_as(other, StartTimer{"theirs", 40, false});
    r.ask_as(r.ear, CancelAllMyTimers{});
    r.run_wakes(8, /*start=*/true);

    CHECK(r.heard.fired.empty()); // both of mine died before firing
    REQUIRE(other_heard.fired.size() == 1);
    CHECK(other_heard.fired[0] == "theirs"); // theirs lived
    CHECK(other_heard.fired_at[0] == 40);
}

TEST_CASE("CancelAllMyTimers is TOTAL — role timers die with the rest, chosen not accidental") {
    // The shape's doc teaches WHEN to send this (a weave being replaced should leave its role
    // timers standing for its successor; one retiring with no heir should cancel) — but the
    // mechanism itself stays total and neutral, because succession here is AUTHORED, never
    // guessed by a shape. This pins the totality as a decision, so it cannot erode into an
    // accident.
    const char* kRole = "suite.pulse.holder";

    // The positive control first: without the cancel, that role timer fires. (An all-empty
    // assertion is only meaningful next to a rig where the same asks DO produce firings.)
    {
        FakeRig c;
        Heard pulse;
        pulse.clock = &c.clock;
        (void)mount_role<Ear>(c.bus, kRole, pulse);
        c.ask_as(c.ear, StartTimer{"mine", 20, false});
        c.ask_as(c.ear, StartRoleTimer{"pulse", 10, true, kRole});
        c.activate();
        c.run_until(30);
        CHECK(c.heard.fired.size() == 1); // the requester timer fired...
        CHECK(pulse.fired.size() >= 2);   // ...and the role timer was firing
    }

    FakeRig r;
    Heard pulse;
    pulse.clock = &r.clock;
    (void)mount_role<Ear>(r.bus, kRole, pulse);
    r.ask_as(r.ear, StartTimer{"mine", 20, false});
    r.ask_as(r.ear, StartRoleTimer{"pulse", 10, true, kRole});
    r.ask_as(r.ear, CancelAllMyTimers{});
    r.run_wakes(6, /*start=*/true);

    CHECK(r.heard.fired.empty()); // the requester-addressed timer: gone
    CHECK(pulse.fired.empty());   // and the ROLE timer with it — that is the point
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
    heard_a.clock = &r.clock;
    const loom::WeaveId holder_a = mount_role<Ear>(r.bus, "suite.pulse.holder", heard_a);
    r.ask_as(r.ear, StartRoleTimer{"pulse", 10, true, "suite.pulse.holder"});
    r.activate();
    r.run_until(20);
    CHECK(heard_a.fired.size() == 2); // t=10, t=20

    // The holder goes; the timer keeps its address and refuses cleanly into the vacancy (an
    // unheld role degrades like an unknown target), and the role keeps its timer...
    (void)r.bus.unregister_weave(holder_a);
    r.run_until(50);
    CHECK(fired_refused == 3);
    CHECK(r.count("active") == 1);
    CHECK(r.count("gone") == 0);

    // ...and a SUCCESSOR simply inherits it: no re-ask, no handshake.
    Heard heard_b;
    heard_b.clock = &r.clock;
    (void)mount_role<Ear>(r.bus, "suite.pulse.holder", heard_b);
    r.run_until(80);
    CHECK(heard_b.fired.size() == 3);
    CHECK(heard_a.fired.size() == 2); // the gone never heard another word
}

TEST_CASE("a successor's re-ask upserts the ROLE timer instead of doubling it") {
    FakeRig r;
    Heard heard_a;
    heard_a.clock = &r.clock;
    const loom::WeaveId holder = mount_role<Ear>(r.bus, "suite.pulse.holder", heard_a);

    // Two askers, same (role, id) — the second replaces the first, so the
    // holder hears ONE lattice, not an interleaving of two.
    r.ask_as(r.ear, StartRoleTimer{"pulse", 7, true, "suite.pulse.holder"});
    r.ask_as(holder, StartRoleTimer{"pulse", 10, true, "suite.pulse.holder"});
    r.activate();
    r.run_until(30);
    REQUIRE(heard_a.fired.size() == 3);
    CHECK(heard_a.fired_at[0] == 10);
    CHECK(heard_a.fired_at[1] == 20);
    CHECK(heard_a.fired_at[2] == 30);
}

TEST_CASE("a gone requester's timers are dropped at the bus's first refusal: one refused firing, "
          "then none") {
    // The service hears a weave is gone when the bus refuses a firing at it (`zen.DispatchRefused`,
    // NoSuchTarget), and drops every timer that weave started then: ids are never reused, so none
    // of them could ever be delivered. A role timer is not dropped -- its role may be held again.
    FakeRig r;
    std::int64_t fired_refused = 0;
    r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Refused && ev.schema_name == "TimerFired") {
            CHECK(ev.refusal.reason == loom::RefusalReason::NoSuchTarget);
            ++fired_refused;
        }
    });

    Heard orphan_heard;
    orphan_heard.clock = &r.clock;
    const loom::WeaveId orphan = loom::mount<Ear>(r.bus, orphan_heard);
    r.ask_as(orphan, StartTimer{"orphan.beat", 10, true});
    r.ask_as(orphan, StartTimer{"orphan.later", 500, false});
    r.activate();
    r.run_until(10);
    CHECK(orphan_heard.fired.size() == 1); // alive: it hears its timer
    REQUIRE(r.count("active") == 2);

    (void)r.bus.unregister_weave(orphan);
    r.run_until(1000);
    CHECK(fired_refused == 1);              // the one firing that found it gone...
    CHECK(r.count("gone") == 2);            // ...and both its timers dropped then
    CHECK(r.count("active") == 0);
    CHECK_FALSE(claimed_until(r).has_value()); // nothing left to wake it for
    CHECK(orphan_heard.fired.size() == 1);  // and nothing landed anywhere else
}

TEST_CASE("a root ask has no one to answer: dropped, not misdelivered") {
    FakeRig r;
    // Root sends carry no weave identity; a StartTimer from one has no requester to fire at.
    // The service drops it, counted, rather than inventing a target.
    r.bus.send(r.service, loom::Message(loom::to_value(StartTimer{"nobody", 5, true})));
    r.run_wakes(6, /*start=*/true);
    CHECK(r.heard.fired.empty());
    CHECK(r.count("dropped") == 1);
}

// ============================================================================
// Tier 3 — the real library through the real Kernel, under the host's wait
// ============================================================================

TEST_CASE("the timer .so is live from its own activation, and the host's wait is what wakes it") {
    Rig r;
    // NOTHING here winds anything. Loading the service is the whole gesture: the control door
    // activates the freshly committed incarnation, and it decides what it inherited and says so.
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    CHECK(r.seen.ready == 1);    // available, announced on the activation itself
    CHECK(r.bus.pending() == 0); // and the bus drained: nothing is queued to keep time

    // A root ask: dropped, and honestly counted.
    r.bus.send(timer_so, loom::Message(loom::to_value(StartTimer{"nobody", 5, true})));

    // The witness asks for a repeating timer; the host's loop runs 45 ms of its clock.
    const std::int64_t start = r.clock.now_ms();
    r.ask(timer_so, StartTimer{"suite.tick", 15, true});
    r.run_for(45);
    CHECK(std::count(r.seen.fired.begin(), r.seen.fired.end(), "suite.tick") == 3);
    CHECK(r.clock.now_ms() == start + 45);
    CHECK(r.bus.pending() == 0); // between wakes the bus holds nothing for the Timer

    // The honest counters.
    CHECK(r.poke_int(timer_so, "dropped") == 1);
    CHECK(r.poke_int(timer_so, "active") == 1);
    CHECK(r.poke_int(timer_so, "fired") == 3);
    CHECK(r.poke_int(timer_so, "wakes") == r.wakes);

    // Cancel, then prove silence by outliving several of its periods.
    r.ask(timer_so, CancelTimer{"suite.tick"});
    const std::size_t before = r.seen.fired.size();
    r.run_for(100);
    CHECK(r.seen.fired.size() == before);
}

TEST_CASE("a Timer with nothing standing is woken by nothing, and an arm costs two wakes") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    REQUIRE(r.seen.ready == 1);
    const std::int64_t settled = r.wakes;

    // A long while of the host's clock with nothing standing: the Timer claims no time, so the
    // wait rests with nothing to wake it for.
    r.run_for(10000);
    CHECK(r.wakes == settled);

    // One arm: the time learned at once, then the deadline.
    const std::int64_t at = r.clock.now_ms();
    r.ask(timer_so, StartTimer{"suite.once", 250, false});
    r.run_for(1000);
    REQUIRE(r.seen.fired.size() == 1);
    CHECK(r.wakes - settled == 2);
    CHECK(r.poke_int(timer_so, "wakes") == r.wakes);
    CHECK(r.clock.now_ms() == at + 1000);
}

// ============================================================================
// Tier 4 — what the Timer serves, and what the host's wait serves without it
// ============================================================================

TEST_CASE("world time comes from the timer path: ticks with nobody sending SnakeTick") {
    Rig r;
    (void)r.load("zengine-timer", TIMER_SO, kTimerRole);
    const loom::WeaveId world_so =
        r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);

    r.run_for(400); // three ticks of the world's own 120ms timer

    // The world seeded and MOVED — and no test code ever sent a SnakeTick.
    REQUIRE(r.seen.visuals.size() >= 2);
    CHECK(r.seen.visuals.front().snake.front().x == 12); // the seed pin
    CHECK(r.seen.visuals.front().snake.front().y == 8);
    CHECK(r.seen.visuals.back().snake.front().x > 12); // heading right, on time
    CHECK(r.poke_int(world_so, "ticks") == 3);
}

// ---- TIMER-02: the load-order matrix ----------------------------------------
// Load order decides nothing, and these two cases are the halves that prove it: a consumer AFTER
// the timer is served by its own activation; a consumer BEFORE it is served by the retry.

TEST_CASE("consumer BEFORE the timer: its declared timer waits, and TimerReady is what rescues "
          "it") {
    Rig r;
    // The world is loaded into a host with NO timer service: it declares its tick, and nothing
    // serves it. It could not retry alone, which is why TimerReady matters.
    const loom::WeaveId world_so =
        r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);
    r.run_for(1000);
    CHECK(r.bus.pending() == 0);
    CHECK(r.poke_int(world_so, "ticks") == 0); // measured, not assumed: ticked by nothing

    // Now the timer arrives. Its activation publishes TimerReady, the world's binding asks
    // AGAIN, and from then on it is served.
    (void)r.load("zengine-timer", TIMER_SO, kTimerRole);
    r.run_for(400);
    CHECK(r.seen.ready >= 1);
    CHECK(r.poke_int(world_so, "ticks") >= 2); // served, with nobody ticking it
}

TEST_CASE("a world reloaded in place asks from its own activation and does NOT double its tick") {
    Rig r;
    const loom::WeaveId timer_so = r.load("zengine-timer", TIMER_SO, kTimerRole);
    const loom::WeaveId first =
        r.load("snake-world-v1", WORLD_V1_SO, zengine::snake::kWorldRole);
    r.run_for(250);
    const std::int64_t standing_before = r.poke_int(timer_so, "active");
    CHECK(standing_before >= 1); // the world's tick is standing

    // Replace the world in place. The successor's own activation makes it ask.
    const std::uint64_t corr = r.next_corr++;
    r.bus.send_as(r.witness, r.manager,
                  loom::Message(loom::to_value(loom::ReloadWeave{"snake-world-v1", WORLD_V1_SO}),
                                r.witness, r.witness, corr));
    const Seen::Answer reloaded = r.wait_answer(corr, "world reload");
    REQUIRE_MESSAGE(reloaded.kind != 2, "world reload refused: ", reloaded.text);
    const loom::WeaveId successor = r.bus.role_holder(zengine::snake::kWorldRole);
    REQUIRE(successor.valid());
    CHECK(successor == first); // a reload in place keeps the weave's id; its incarnation is new
    const std::int64_t ticks_before = r.poke_int(successor, "ticks");
    r.run_for(250);

    // The successor is served...
    CHECK(r.poke_int(successor, "ticks") > ticks_before);
    // ...and the tick was REPLACED, not added: the successor's declaration takes the incumbent's
    // schedule instead of standing a second one beside it.
    CHECK(r.poke_int(timer_so, "active") == standing_before);
}

namespace {

/// THE HOST'S WAKES OF A ROLE, counted off the bus: each `Waited` the wait handed whichever weave
/// holds `role` then, its source's time come.
struct WakesOf {
    std::int64_t due = 0;
    loom::Switchboard& bus;
    std::string role;
    loom::ObserverId tap;
    WakesOf(loom::Switchboard& b, std::string r) : bus(b), role(std::move(r)) {
        tap = bus.add_observer([this](const loom::BusEvent& ev) {
            if (due_wake_of(ev, bus.role_holder(role))) {
                ++due;
            }
        });
    }
    ~WakesOf() { bus.remove_observer(tap); }
    WakesOf(const WakesOf&) = delete;
    WakesOf& operator=(const WakesOf&) = delete;
};

constexpr const char* kTuiWindowRole = "zengine.window.tui";

} // namespace

namespace {

/// The host's wait as it stands with `role`'s claim.
std::optional<loom::host::Reactor::Source> source_of(const Rig& r, const char* role) {
    for (const loom::host::Reactor::Source& s : r.wait.sources()) {
        if (s.role == role) {
            return s;
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("the input package is a source of the host's wait: headless, it claims nothing to read "
          "and no time, and is woken by nothing") {
    Rig r;
    r.wait.trust(zengine::input::kInputRole);
    WakesOf woke(r.bus, zengine::input::kInputRole);
    // No Timer at all: the input weave claims what its reader waits on as its role, from its own
    // activation. Headless, its reader has nothing to read, so the claim names no handle and no
    // time -- and nothing polls it.
    const loom::WeaveId input_so =
        r.load("zengine-input", INPUT_SO, zengine::input::kInputRole);
    r.settle();
    const std::optional<loom::host::Reactor::Source> claim =
        source_of(r, zengine::input::kInputRole);
    REQUIRE(claim.has_value());
    CHECK(claim->claimed);
    CHECK(claim->handles == 0);
    CHECK_FALSE(claim->until_ms.has_value());

    const std::int64_t pumped = r.poke_int(input_so, "pumped");
    r.run_for(1000);
    CHECK(woke.due == 0);                               // no time, so no wake
    CHECK(r.poke_int(input_so, "pumped") == pumped);    // and no read nobody asked for
    CHECK(r.poke_int(input_so, "emitted") == 0);
    CHECK(r.bus.pending() == 0);
}

TEST_CASE("the window is a source of the host's wait: headless, it claims its wait from its own "
          "activation and is polled by nothing") {
    Rig r;
    r.wait.trust(kTuiWindowRole);
    WakesOf woke(r.bus, kTuiWindowRole);
    (void)r.load("zengine-window-tui", WINDOW_SO, kTuiWindowRole);
    r.settle();
    const std::optional<loom::host::Reactor::Source> claim = source_of(r, kTuiWindowRole);
    REQUIRE(claim.has_value());
    CHECK(claim->claimed);

    // Not one PumpSurface in this lane, and no Timer: what wakes the window is the wait, on what
    // it claimed -- here, nothing ready and no time, so the host rests.
    r.run_for(1000);
    CHECK(woke.due == 0);
    CHECK(r.bus.pending() == 0);
}

// ============================================================================
// Tier 5 — the timer BINDING (TIMER-05)
// ============================================================================
// The binding declares desire and owns the Timer protocol. These prove, over a real bus: a
// declaration is not execution, activation and TimerReady both reconcile, dispatch is exact,
// cancellation is both halves, and the convenience widens no authority.

namespace {

/// A domain shape a callback emits, to prove a callback can speak ordinary
/// domain messages through the Mail it is handed.
struct Woke {
    std::int64_t n;
    ZEN_SHAPE(Woke, 1, ZEN_FIELD(n));
};
/// The bound weave's own door — and the only place a Mail exists, which is why
/// cancellation is expressed as a message rather than a free function.
struct CancelTick {
    ZEN_SHAPE(CancelTick, 1);
};
struct RestartTick {
    ZEN_SHAPE(RestartTick, 1);
};
/// "Restart yourself from inside your own firing" — the deliberate re-arm the
/// spent-before-callback ordering exists to make possible.
struct ArmFromCallback {
    ZEN_SHAPE(ArmFromCallback, 1);
};

struct BoundState {
    std::int64_t beats = 0;
    std::int64_t role_beats = 0;
    std::int64_t shots = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(BoundState, 1, ZEN_FIELD(beats), ZEN_FIELD(role_beats), ZEN_FIELD(shots));
};

/// A weave that uses the binding for everything, with handlers of its own so
/// the `using` requirement is exercised rather than assumed.
class Bound : public TimedWeave<Bound, BoundState, loom::Accept<CancelTick, RestartTick>,
                                loom::Emit<Woke>> {
public:
    Bound()
        : tick_(timers().repeat("bound.tick", std::chrono::milliseconds(5), &Bound::on_tick)),
          role_(timers().repeat_to_role("bound.role", std::chrono::milliseconds(5), "bound.slot",
                                        &Bound::on_role)) {}

    /// The one line of ceremony. Without it this class does not compile: a
    /// derived `on` hides every base `on`, and WeaveBase dispatches on Self.
    using TimedWeave::on;

    void on(const CancelTick&, loom::Mail& mail) { tick_.cancel(mail); }
    void on(const RestartTick&, loom::Mail& mail) { tick_.restart(mail); }

    void on_tick(const TimerFired&, loom::Mail& mail) {
        ++state_.beats;
        mail.publish(Woke{state_.beats}); // a callback speaking domain, through its Mail
    }
    void on_role(const TimerFired&, loom::Mail&) { ++state_.role_beats; }

    Handle tick_;
    Handle role_;
};

/// One declared one-shot, and a callback that can re-arm itself from inside its
/// own firing. Its own fixture so the multi-binding pins above keep their
/// counts — a boring diff is a feature.
class Shooter : public TimedWeave<Shooter, BoundState, loom::Accept<ArmFromCallback>,
                                  loom::Emit<>> {
public:
    Shooter() : shot_(timers().once("shooter.shot", std::chrono::milliseconds(5),
                                    &Shooter::on_shot)) {}

    using TimedWeave::on;

    void on(const ArmFromCallback&, loom::Mail&) { rearm_ = true; }

    /// This only works because the binding is marked Spent BEFORE the callback
    /// runs: marking afterwards would overwrite the callback's deliberate
    /// restart with a stale Spent.
    void on_shot(const TimerFired&, loom::Mail& mail) {
        ++state_.shots;
        if (rearm_) {
            shot_.restart(mail);
        }
    }

    Handle shot_;
    bool rearm_ = false;
};

/// A role beat with no role: the OTHER programmer error the binding must refuse,
/// because the alternative is a silent degradation into the addressing mode the
/// author deliberately did not choose.
class EmptyRoleRepeat : public TimedWeave<EmptyRoleRepeat, BoundState, loom::Accept<>,
                                          loom::Emit<>> {
public:
    EmptyRoleRepeat()
        : h_(timers().repeat_to_role("no.role", std::chrono::milliseconds(5), "",
                                     &EmptyRoleRepeat::beat)) {}
    void beat(const TimerFired&, loom::Mail&) { ++state_.beats; }
    Handle h_;
};
class EmptyRoleOnce : public TimedWeave<EmptyRoleOnce, BoundState, loom::Accept<>, loom::Emit<>> {
public:
    EmptyRoleOnce()
        : h_(timers().once_to_role("no.role", std::chrono::milliseconds(5), "",
                                   &EmptyRoleOnce::beat)) {}
    void beat(const TimerFired&, loom::Mail&) { ++state_.beats; }
    Handle h_;
};

/// Two declarations, one id: the programmer error the binding must refuse.
class Aliased : public TimedWeave<Aliased, BoundState, loom::Accept<>, loom::Emit<>> {
public:
    Aliased()
        : a_(timers().repeat("same.id", std::chrono::milliseconds(5), &Aliased::first)),
          b_(timers().repeat("same.id", std::chrono::milliseconds(5), &Aliased::second)) {}
    void first(const TimerFired&, loom::Mail&) { ++state_.beats; }
    void second(const TimerFired&, loom::Mail&) { ++state_.role_beats; }
    Handle a_;
    Handle b_;
};

/// Catches what a bound weave asks the service for, wire-exact — and answers
/// with whatever receipt the test wants, so the handle's own view of "what
/// actually happened" is exercised through the real message rather than poked in.
struct AskSeen {
    std::vector<EnsureTimer> asks;
    std::vector<EnsureRoleTimer> role_asks;
    std::vector<CancelTimer> cancels;
    std::string answer_with;       ///< resolution spelling to reply with ("" = stay silent)
    std::string answer_reason = "because the stand-in service said so";
};
struct CatcherState {
    std::int64_t n = 0;
    ZEN_SHAPE(CatcherState, 1, ZEN_FIELD(n));
};
class AskCatcher : public loom::WeaveBase<AskCatcher, CatcherState,
                                          loom::Accept<EnsureTimer, EnsureRoleTimer, CancelTimer>,
                                          loom::Emit<TimerFired, TimerReady, TimerResolution>> {
public:
    explicit AskCatcher(AskSeen& seen) : seen_(&seen) {}
    void on(const EnsureTimer& s, loom::Mail& mail) {
        seen_->asks.push_back(s);
        receipt(mail, s.id);
    }
    void on(const EnsureRoleTimer& s, loom::Mail& mail) {
        seen_->role_asks.push_back(s);
        receipt(mail, s.id);
    }
    void on(const CancelTimer& c, loom::Mail&) { seen_->cancels.push_back(c); }

private:
    void receipt(loom::Mail& mail, const std::string& id) {
        if (seen_->answer_with.empty()) {
            return;
        }
        mail.send(mail.sender(), TimerResolution{id, seen_->answer_with, seen_->answer_reason});
    }
    AskSeen* seen_;
};

/// What the activation hook says out loud, so its RUNNING is a wire fact and
/// not merely a counter the test trusts.
struct ActivationObserved {
    std::int64_t sequence = 0;
    ZEN_SHAPE(ActivationObserved, 1, ZEN_FIELD(sequence));
};

struct AwareState {
    std::int64_t beats = 0;
    std::int64_t hooks = 0;
    std::int64_t last_activation = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(AwareState, 1, ZEN_FIELD(beats), ZEN_FIELD(hooks), ZEN_FIELD(last_activation));
};

/// A bound weave that ALSO wants domain work on activation -- which a raw
/// `on(const loom::Activated&, loom::Mail&)` would do at the cost of every declared timer. It
/// carries all three things at once on purpose: inherited Timer handlers, a domain handler of
/// its own (so the `using` requirement is exercised, not assumed), and the activation extension.
class ActivationAware
    : public TimedWeave<ActivationAware, AwareState, loom::Accept<CancelTick>,
                        loom::Emit<ActivationObserved>> {
public:
    ActivationAware()
        : tick_(timers().repeat("aware.tick", std::chrono::milliseconds(5),
                                &ActivationAware::on_tick)) {}

    using TimedWeave::on;

    void on(const CancelTick&, loom::Mail& mail) { tick_.cancel(mail); }

    /// THE EXTENSION POINT — public, because the binding layer has to be able to
    /// reach it, and a private one would be indistinguishable from none.
    ///
    /// By the time this runs the activation has been accepted once and every
    /// binding has already been reconciled, so it may assume its timers are
    /// ordered. It speaks only a shape this weave declared in its own Emit list.
    void on_timed_activation(const loom::Activated& a, loom::Mail& mail) {
        ++state_.hooks;
        state_.last_activation = a.sequence;
        mail.publish(ActivationObserved{a.sequence});
    }

    void on_tick(const TimerFired&, loom::Mail&) { ++state_.beats; }

    Handle tick_;
};

/// Hears what the hook said, so ordering can be read off the bus.
struct HookEarState {
    std::int64_t n = 0;
    ZEN_SHAPE(HookEarState, 1, ZEN_FIELD(n));
};
class HookEar : public loom::WeaveBase<HookEar, HookEarState, loom::Accept<ActivationObserved>,
                                       loom::Emit<>> {
public:
    explicit HookEar(std::vector<std::int64_t>& heard) : heard_(&heard) {}
    void on(const ActivationObserved& o, loom::Mail&) { heard_->push_back(o.sequence); }

private:
    std::vector<std::int64_t>* heard_;
};

struct WokeState {
    std::int64_t n = 0;
    ZEN_SHAPE(WokeState, 1, ZEN_FIELD(n));
};
class WokeEar : public loom::WeaveBase<WokeEar, WokeState, loom::Accept<Woke>, loom::Emit<>> {
public:
    explicit WokeEar(std::vector<std::int64_t>& heard) : heard_(&heard) {}
    void on(const Woke& w, loom::Mail&) { heard_->push_back(w.n); }

private:
    std::vector<std::int64_t>* heard_;
};

/// A real bus with a bound weave and a stand-in service that only RECORDS
/// asks — so what the binding SAYS is inspected wire-exactly, with no real
/// scheduling in the way.
struct BindRig {
    loom::Switchboard bus;
    AskSeen seen;
    std::vector<std::int64_t> woke;
    loom::WeaveId weave{};
    loom::WeaveId service{};
    loom::WeaveId door{};
    loom::WeaveId ear{};
    std::int64_t next_sequence = 1;

    BindRig() {
        weave = loom::mount<Bound>(bus);
        service = mount_role<AskCatcher>(bus, kTimerRole, seen);
        door = zengine::testing::mount_door(bus);
        ear = loom::mount<WokeEar>(bus, woke);
    }

    void activate(std::int64_t sequence) {
        zengine::testing::order_activation(bus, door, weave, sequence);
        bus.drain_until_idle();
    }
    void activate() { activate(next_sequence++); }

    /// The service says it is available (root-published, as the real one does).
    void ready() {
        bus.publish(loom::Message(loom::to_value(TimerReady{})));
        bus.drain_until_idle();
    }

    /// A firing, stamped as the service — the honest wire path.
    void fire(const char* id) {
        bus.send_as(service, weave,
                    loom::Message(loom::to_value(TimerFired{id}), service, service, 0));
        bus.drain_until_idle();
    }

    template <class T>
    void tell(const T& msg) {
        bus.send(weave, loom::Message(loom::to_value(msg)));
        bus.drain_until_idle();
    }

    std::int64_t count(const char* field) {
        loom::Unverified u = loom::parse(bus.snapshot_bytes(weave));
        loom::Admission a = loom::admit(u, loom::schema_of<BoundState>());
        REQUIRE(a.ok());
        return a.value().get(field)->as_int();
    }
};

/// The same rig for the weave that carries an activation hook, plus a TAP that records the
/// ORDER shapes were delivered in -- read off the bus rather than inferred from final counters,
/// because "both happened" and "they happened in this order" are different claims, and the law
/// is the second.
struct HookRig {
    loom::Switchboard bus;
    AskSeen seen;
    std::vector<std::int64_t> observed;
    std::vector<std::string> delivered; ///< every shape, in delivery order
    loom::WeaveId weave{};
    loom::WeaveId service{};
    loom::WeaveId door{};
    loom::WeaveId ear{};
    std::int64_t next_sequence = 1;

    HookRig() {
        weave = loom::mount<ActivationAware>(bus);
        service = mount_role<AskCatcher>(bus, kTimerRole, seen);
        door = zengine::testing::mount_door(bus);
        ear = loom::mount<HookEar>(bus, observed);
        bus.add_observer([this](const loom::BusEvent& e) {
            if (e.kind == loom::EventKind::Delivered) {
                delivered.push_back(e.schema_name);
            }
        });
    }

    void activate(std::int64_t sequence) {
        zengine::testing::order_activation(bus, door, weave, sequence);
        bus.drain_until_idle();
    }
    void activate() { activate(next_sequence++); }

    /// An activation nobody attested — an ordinary root send of the right shape.
    void activate_unattested(std::int64_t sequence) {
        bus.send(weave, loom::Message(loom::to_value(loom::Activated{sequence})));
        bus.drain_until_idle();
    }

    void ready() {
        bus.publish(loom::Message(loom::to_value(TimerReady{})));
        bus.drain_until_idle();
    }

    void fire(const char* id) {
        bus.send_as(service, weave,
                    loom::Message(loom::to_value(TimerFired{id}), service, service, 0));
        bus.drain_until_idle();
    }

    /// Where a shape first appears in the delivery order, or -1.
    std::ptrdiff_t first(const char* shape) const {
        for (std::size_t i = 0; i < delivered.size(); ++i) {
            if (delivered[i] == shape) {
                return static_cast<std::ptrdiff_t>(i);
            }
        }
        return -1;
    }

    std::int64_t count(const char* field) {
        loom::Unverified u = loom::parse(bus.snapshot_bytes(weave));
        loom::Admission a = loom::admit(u, loom::schema_of<AwareState>());
        REQUIRE(a.ok());
        return a.value().get(field)->as_int();
    }
};

} // namespace

TEST_CASE("binding: declaration is not execution — constructing sends nothing, and happens "
          "quite happily with no Timer in the process at all") {
    loom::Switchboard bus;
    std::int64_t events = 0;
    bus.add_observer([&](const loom::BusEvent&) { ++events; });

    // No timer service, no role holder, nothing. Declaring two bindings is a
    // purely local act: `timers().repeat(...)` records DESIRE. There is no Mail
    // during construction, and a weave that reached out from its constructor
    // would be speaking outside the one place a weave is allowed to speak.
    const loom::WeaveId w = loom::mount<Bound>(bus);
    bus.drain_until_idle();
    CHECK(events == 0);
    CHECK(bus.pending() == 0);

    // And no callback ran: a declaration is not a firing either.
    loom::Unverified u = loom::parse(bus.snapshot_bytes(w));
    loom::Admission a = loom::admit(u, loom::schema_of<BoundState>());
    REQUIRE(a.ok());
    CHECK(a.value().get("beats")->as_int() == 0);
    CHECK(a.value().get("role_beats")->as_int() == 0);

    // THE POSITIVE CONTROL. Everything above is an assertion of ABSENCE, which
    // a blind observer would satisfy exactly as happily as real silence. So
    // prove the meter reads: one ordinary message, and the count moves.
    bus.send(w, loom::Message(loom::to_value(CancelTick{})));
    bus.drain_until_idle();
    CHECK(events > 0);
}

// ---- the activation extension hook ------------------------------------------
// A derived weave may EXTEND Timer activation, never REPLACE it: the raw `on(zen.Activated)` is
// the binding's, and a derived redefinition is refused at build time -- proven in the
// compile-negative lane, since an offending weave never becomes an artifact. Here: the
// supported hook, and exactly when it does and does not run.

TEST_CASE("binding: the activation hook runs AFTER the timers were ordered, once, and only for "
          "an activation this weave accepted") {
    HookRig r;
    CHECK(r.seen.asks.empty());
    CHECK(r.count("hooks") == 0);

    r.activate(7);

    // 1-3. The binding did its work: exactly one order, for the declared timer.
    REQUIRE(r.seen.asks.size() == 1);
    CHECK(r.seen.asks[0].id == "aware.tick");
    CHECK(r.seen.asks[0].delay_ms == 5);

    // 4. The hook ran exactly once, and saw the real activation.
    CHECK(r.count("hooks") == 1);
    CHECK(r.count("last_activation") == 7);
    REQUIRE(r.observed.size() == 1);
    CHECK(r.observed[0] == 7);

    // 5. THE ORDER, READ OFF THE BUS. `reconcile` enqueued the order first and
    //    the hook enqueued its own message second, so FIFO delivery puts
    //    EnsureTimer strictly before ActivationObserved. Two counters both
    //    reading 1 would be satisfied by either order; this is not.
    const std::ptrdiff_t order = r.first(EnsureTimer::zen_name);
    const std::ptrdiff_t hook = r.first(ActivationObserved::zen_name);
    REQUIRE(order >= 0);
    REQUIRE(hook >= 0);
    CHECK(order < hook);

    // 6-7. The receipt reaches the binding, and a firing reaches the callback —
    //      the hook did not displace any of the ordinary machinery.
    r.seen.answer_with = "restart_delay";
    r.ready();
    CHECK(r.count("beats") == 0);
    r.fire("aware.tick");
    CHECK(r.count("beats") == 1);
}

TEST_CASE("binding: a refused activation reaches neither the bindings nor the hook") {
    int route = 0;
    SUBCASE("a duplicate of the accepted sequence") { route = 0; }
    SUBCASE("a replay of an older sequence") { route = 1; }
    SUBCASE("an activation nobody attested") { route = 2; }

    HookRig r;
    r.activate(4);
    REQUIRE(r.seen.asks.size() == 1);
    REQUIRE(r.count("hooks") == 1);
    const std::size_t hooks_said = r.observed.size();

    if (route == 0) {
        r.activate(4);
    } else if (route == 1) {
        r.activate(1);
    } else {
        r.activate_unattested(99);
    }

    // Nothing moved: not the orders, not the hook, not the hook's speech. The
    // cursor decided ONCE, above the hook, which is the whole reason the hook is
    // not a raw handler — an author cannot get this wrong by forgetting a check.
    CHECK(r.seen.asks.size() == 1);
    CHECK(r.count("hooks") == 1);
    CHECK(r.count("last_activation") == 4);
    CHECK(r.observed.size() == hooks_said);
}

TEST_CASE("binding: TimerReady reconciles the bindings and does NOT invoke the activation hook") {
    HookRig r;
    r.activate(2);
    REQUIRE(r.seen.asks.size() == 1);
    REQUIRE(r.count("hooks") == 1);

    // The Timer service becoming available is not this weave's activation. It
    // must re-establish what the weave declared — that is the whole point of the
    // path — and it must not run domain activation work a second time.
    r.ready();
    CHECK(r.seen.asks.size() == 2); // reconciled again
    CHECK(r.count("hooks") == 1);   // and the hook did not run again
    CHECK(r.observed.size() == 1);

    // ...and a second TimerReady is still reconcile-only.
    r.ready();
    CHECK(r.seen.asks.size() == 3);
    CHECK(r.count("hooks") == 1);
}

TEST_CASE("binding: a weave with no activation hook is untouched — the extension is absent, not "
          "merely unused") {
    // `Bound` declares no hook. Its activation must do exactly what it always
    // did, and nothing about the hook may become a requirement: no extra shape
    // in its manifest, no extra message on the bus, no second reconciliation.
    BindRig r;
    std::vector<std::string> delivered;
    r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Delivered) {
            delivered.push_back(e.schema_name);
        }
    });

    r.activate();
    CHECK(r.seen.asks.size() == 1);
    CHECK(r.seen.role_asks.size() == 1);

    // The hook's shape never appears, because this weave never declared it.
    CHECK(std::find(delivered.begin(), delivered.end(),
                    std::string(ActivationObserved::zen_name)) == delivered.end());

    // And its composed contract is the Timer protocol plus its OWN shapes only —
    // the hook adds nothing to a manifest, for anybody.
    std::vector<std::string> accepted;
    for (const auto& s : r.bus.accepted_schemas(r.weave)) {
        accepted.push_back(s->name());
    }
    std::sort(accepted.begin(), accepted.end());
    std::string joined;
    for (const std::string& n : accepted) {
        joined += n + ";";
    }
    INFO("accepted: " << joined);
    // The Timer protocol, this weave's own two doors, and the FIVE universal substrate doors
    // every woven weave carries (`WeaveBase::accepted_schemas` appends the poke and describe
    // doors) -- nothing else, no trace of the hook's shape. The SET is asserted rather than a
    // handful of memberships because a door appearing in every weave in the process is exactly
    // the change a membership check cannot see; this list once went red on a Loom that added one.
    CHECK(accepted == std::vector<std::string>{
                          "CancelTick", "RestartTick", "TimerFired", "TimerReady",
                          "TimerResolution", "zen.Activated", "zen.DescribeAccepted",
                          "zen.PokeDescribe", "zen.PokeRead", "zen.PokeResetState",
                          "zen.PokeWrite"});
}

TEST_CASE("binding: an accepted activation reconciles every desired binding exactly once — "
          "and a duplicate reconciles nothing") {
    BindRig r;
    CHECK(r.seen.asks.empty());
    CHECK(r.seen.role_asks.empty());

    r.activate(); // sequence 1

    // Each binding asked exactly once, and the WIRE CONTENT is the ORDERED
    // protocol carrying the binding's declared order: the convenience composed
    // the message the author would otherwise hand-write, field for field —
    // including the default preference and its accepted fallback.
    REQUIRE(r.seen.asks.size() == 1);
    CHECK(r.seen.asks[0].id == "bound.tick");
    CHECK(r.seen.asks[0].delay_ms == 5);
    CHECK(r.seen.asks[0].repeat);
    CHECK(r.seen.asks[0].preferred == kPreserveRemaining);
    CHECK(r.seen.asks[0].fallback == kRestartDelay);
    REQUIRE(r.seen.role_asks.size() == 1);
    CHECK(r.seen.role_asks[0].id == "bound.role");
    CHECK(r.seen.role_asks[0].delay_ms == 5);
    CHECK(r.seen.role_asks[0].repeat);
    CHECK(r.seen.role_asks[0].role == "bound.slot"); // the role really travels
    CHECK(r.seen.role_asks[0].preferred == kPreserveRemaining);
    CHECK(r.seen.role_asks[0].fallback == kRestartDelay);

    // THE SAME activation again. The binding layer owns deduplication so that
    // consumers do not each carry a cursor — and nothing is re-asked.
    r.activate(1);
    CHECK(r.seen.asks.size() == 1);
    CHECK(r.seen.role_asks.size() == 1);

    // A NEWER one is a new lineage point, and does reconcile again.
    r.activate(); // sequence 2
    CHECK(r.seen.asks.size() == 2);
    CHECK(r.seen.role_asks.size() == 2);
}

TEST_CASE("binding: TimerReady reconciles too — the path that rescues a consumer whose "
          "declarations were made before any service existed") {
    BindRig r;
    // The consumer is live and its bindings are declared; the service turns up
    // afterwards and says so. That notice alone establishes everything.
    r.ready();
    CHECK(r.seen.asks.size() == 1);
    CHECK(r.seen.role_asks.size() == 1);

    // And it keeps working: a reloaded or swapped service publishes again with
    // an empty table, and the standing bindings refill it. (Cardinality-
    // idempotent, not timing-neutral — each re-ask replaces and re-anchors.)
    r.ready();
    CHECK(r.seen.asks.size() == 2);
    CHECK(r.seen.role_asks.size() == 2);
}

TEST_CASE("binding: dispatch is exact — the right callback, never a neighbour, never on an "
          "id nobody declared") {
    BindRig r;
    r.activate();

    r.fire("bound.tick");
    CHECK(r.count("beats") == 1);
    CHECK(r.count("role_beats") == 0); // no cross-dispatch

    r.fire("bound.role");
    CHECK(r.count("beats") == 1);
    CHECK(r.count("role_beats") == 1);

    // An id this weave never declared is data, not a drive — a role beat can be
    // aimed at by anyone, so this is the ordinary case, not the hostile one.
    r.fire("somebody.elses.timer");
    CHECK(r.count("beats") == 1);
    CHECK(r.count("role_beats") == 1);

    // A callback speaks ordinary domain messages through the Mail it is handed:
    // it is an ordinary handler on the ordinary Loom execution thread, not a
    // callback smuggled in from somewhere else.
    REQUIRE(r.woke.size() == 1);
    CHECK(r.woke[0] == 1);
}

TEST_CASE("binding: cancellation is BOTH halves — the service is told, and a later "
          "TimerReady does not resurrect it") {
    BindRig r;
    r.activate();
    REQUIRE(r.seen.asks.size() == 1);
    REQUIRE(r.seen.role_asks.size() == 1);

    // Cancelled from inside a handler, which is the only place a Mail exists.
    r.tell(CancelTick{});
    REQUIRE(r.seen.cancels.size() == 1);
    CHECK(r.seen.cancels[0].id == "bound.tick"); // the remote half: the real CancelTimer
    CHECK(static_cast<Bound*>(r.bus.weave(r.weave))->tick_.canceled());

    // THE LOCAL HALF, and it is the one a naive implementation forgets: the
    // binding is no longer wanted, so reconciliation must not bring it back.
    r.ready();
    CHECK(r.seen.asks.size() == 1);      // the cancelled one stayed cancelled...
    CHECK(r.seen.role_asks.size() == 2); // ...and its neighbour reconciled normally

    // An explicit restart wants it again, and asks once.
    r.tell(RestartTick{});
    CHECK(r.seen.asks.size() == 2);
    CHECK(static_cast<Bound*>(r.bus.weave(r.weave))->tick_.waiting());

    // ...and it reconciles like any other desired binding from then on.
    r.ready();
    CHECK(r.seen.asks.size() == 3);
}

TEST_CASE("binding: duplicate local ids are refused at declaration, never silently aliased") {
    // A timer id is the ONLY thing a firing carries, so two bindings sharing
    // one could not be told apart and dispatch would have to pick. Picking
    // silently is how a weave runs the wrong behaviour forever — so this is a
    // programmer error, and it takes the project's established path for one.
    loom::Switchboard bus;
    CHECK_THROWS_AS(loom::mount<Aliased>(bus), std::invalid_argument);

    // For a weave loaded through the KERNEL the same throw becomes a clean
    // "library create() refused: <what it threw>" load refusal — the ABI's create
    // thunk catches everything — which is the path every construction failure
    // already takes. True by construction; not separately pinned here because
    // it would need a fixture .so that exists only to be broken.
}

TEST_CASE("binding: the convenience hides ceremony from the author, never the conversation "
          "from Loom") {
    // THE CONTRACT-HONESTY PIN. A bound weave's manifest must carry the whole
    // Timer protocol it actually speaks — accepted and emitted — because the
    // point of the binding is to stop authors retyping ceremony, not to let a
    // weave converse off the books.
    loom::Switchboard bus;
    const loom::WeaveId w = loom::mount<Bound>(bus);

    const auto accepts = [&](const char* name, std::uint32_t version) {
        for (const auto& s : bus.accepted_schemas(w)) {
            if (s && s->name() == name && s->version() == version) {
                return true;
            }
        }
        return false;
    };
    // The four the binding accepts on the author's behalf — the receipt is one
    // of them, because an order that could not be answered would be a
    // conversation the manifest lied about.
    CHECK(accepts(loom::Activated::zen_name, loom::Activated::zen_version));
    CHECK(accepts("TimerReady", 1));
    CHECK(accepts("TimerFired", 1));
    CHECK(accepts("TimerResolution", 1));
    // ...and the author's own doors, unharmed.
    CHECK(accepts("CancelTick", 1));
    CHECK(accepts("RestartTick", 1));

    // The emitted set is the composed truth too -- the three the binding may send, plus the
    // author's own -- and it is ORDERED only: the binding speaks no raw start shape, so its
    // manifest claims none. (The raw vocabulary stays public; this weave does not use it.)
    Bound probe;
    const auto emits = [&](const char* name) {
        for (const auto& s : probe.emitted_schemas()) {
            if (s && s->name() == name) {
                return true;
            }
        }
        return false;
    };
    CHECK(emits("EnsureTimer"));
    CHECK(emits("EnsureRoleTimer"));
    CHECK(emits("CancelTimer"));
    CHECK(emits("Woke"));
    CHECK_FALSE(emits("StartTimer"));
    CHECK_FALSE(emits("StartRoleTimer"));

    // AND NOT BY WIDENING. The grant a mounted weave gets is derived from that
    // declared emit set — nothing here is `allow_any`, and acceptance is the
    // listed set, not AcceptMode::AnyRegistered. The proof that it is narrow is
    // that a shape the weave never declared is NOT permitted: Nudge-shaped
    // authority does not come free with the convenience.
    const loom::Grant g = loom::emit_default_grant(probe);
    CHECK(g.permits("EnsureTimer", 1, loom::WeaveId{1}));
    CHECK(g.permits("EnsureRoleTimer", 1, loom::WeaveId{1}));
    CHECK_FALSE(g.permits("StartTimer", 1, loom::WeaveId{1}));
    CHECK_FALSE(g.permits("SnakeTick", 1, loom::WeaveId{1}));
    CHECK_FALSE(g.permits("TimerFired", 1, loom::WeaveId{1})); // it receives these, never sends
}

// ============================================================================
// Tier 5 — CONTINUITY end to end, on the host's clock (TIMER-03)
// ============================================================================
// Real libraries, the real kernel and steward, a real graceful replacement and a real letter
// through the real gate, all through `zengine-timer-virtual` -- the shipped Timer, handed its
// time by the host's wait on a clock that moves only when told. A ceremony spends no time: the
// clock moves only when the wait rests to a time due, so every duration here is exact.

namespace {

/// Establish the probe's five-second one-shot at a KNOWN instant, and answer it. The message is
/// the very one the probe's own binding sends; `restart_delay` re-anchors it at the next time the
/// wait hands over, which is the time it was handled, since the Timer claims a time already past.
std::int64_t anchor_probe(Rig& r, loom::WeaveId probe, loom::WeaveId timer) {
    r.ask_as(probe, timer,
             EnsureTimer{zengine::probe::kProbeTimerId, 5000, false, kRestartDelay, ""});
    r.pump_until(EnsureTimer::zen_name);
    return r.clock.now_ms();
}

/// A one-shot of the witness's own, due `ms` after the next time handed over: what makes the
/// host's wait hand the Timer the time then, so a letter written after it measures from it (a
/// letter measures from the last time the Timer was handed).
void hand_time_at(Rig& r, loom::WeaveId timer, std::int64_t ms) {
    r.ask(timer, StartTimer{"suite.handed", ms, false});
}

} // namespace

TEST_CASE("continuity, graceful: a five-second one-shot with two seconds left crosses a real "
          "Timer replacement, and the consumer is told exactly what happened") {
    Rig r;
    const loom::WeaveId timer = r.load("zengine-timer-virtual", TIMER_VIRTUAL_SO, kTimerRole);
    const loom::WeaveId probe = r.load("zengine-probe-oneshot", PROBE_ONESHOT_SO, "");

    // 1 + 2. A five-second one-shot, declared with the binding's DEFAULT order
    // and established on this incarnation's own activation. On an initial load
    // there is nothing to preserve, and the receipt says exactly that instead of
    // reporting a bland success.
    zengine::probe::ProbeReport rep = r.ask_probe(probe);
    CHECK(rep.resolved == kResolutionRestarted);
    CHECK(rep.lifecycle == "waiting");
    CHECK(rep.fires == 0);
    CHECK(rep.reason.find("no matching schedule to preserve") != std::string::npos);

    const std::int64_t anchored = anchor_probe(r, probe, timer);

    // 3. Three seconds of the host's clock, the Timer handed the time at their end.
    hand_time_at(r, timer, 3000);
    r.run_until(anchored + 3000);

    // 4. The graceful replacement. Same library, a genuinely new incarnation.
    const std::uint64_t corr = r.command(loom::SwapWeave{
        kTimerRole, "zengine-timer-virtual", TIMER_VIRTUAL_SO, /*graceful=*/true});
    r.pump_until(TimerResolution::zen_name); // once the heir has answered the probe
    CHECK(r.clock.now_ms() == anchored + 3000); // the ceremony spent no time

    const Seen::Answer* swapped = r.seen.find(corr);
    REQUIRE(swapped != nullptr);
    REQUIRE_MESSAGE(swapped->kind == 0, "swap refused: ", swapped->text);
    const loom::WeaveId successor{static_cast<std::uint64_t>(std::stoll(swapped->text))};
    CHECK(successor.value != timer.value);

    // 5. THE PREDECESSOR OFFERED TWO SECONDS REMAINING — read off the letter
    // itself, on the wire, decoded through the real gate. Two letters cross: the
    // predecessor's to the steward, and the steward's to the heir.
    REQUIRE(r.letters.size() == 2);
    CHECK(r.letters[0].sender.value == timer.value);     // written by the incumbent
    CHECK(r.letters[0].target.value == r.manager.value); // to the steward
    CHECK(r.letters[1].target.value == successor.value); // and pulled by the heir

    const std::vector<TimerHandoffEntry> offered = Rig::handoff_of(r.letters[0]);
    REQUIRE(offered.size() == 1);
    CHECK(offered[0].id == zengine::probe::kProbeTimerId);
    CHECK(offered[0].remaining_ms == 2000); // five declared, three elapsed
    CHECK(offered[0].delay_ms == 5000);     // the intent travels beside the progress
    CHECK_FALSE(offered[0].repeat);
    CHECK(offered[0].role.empty());
    CHECK(offered[0].requester == std::to_string(probe.value)); // lossless, and the real id

    // ...and the heir was handed the same bytes.
    const std::vector<TimerHandoffEntry> inherited = Rig::handoff_of(r.letters[1]);
    REQUIRE(inherited.size() == 1);
    CHECK(inherited[0].remaining_ms == 2000);

    // 6. THE SUCCESSOR RESTORED BEFORE ANNOUNCING. `inherited` counts what was
    // adopted at this incarnation's bootstrap; `active` is the standing table.
    CHECK(r.poke_int(successor, "inherited") == 1);
    CHECK(r.poke_int(successor, "active") == 1);

    // 7. THE CONSUMER ASKED TO PRESERVE, AND WAS TOLD THE TIMER DID.
    rep = r.ask_probe(probe);
    CHECK(rep.resolved == kResolutionPreserved);
    CHECK(rep.reason.find("kept its remaining time") != std::string::npos);
    CHECK(rep.fires == 0);
    CHECK(rep.lifecycle == "waiting");

    // 8. A MILLISECOND SHORT OF THE REMAINING TIME: NOTHING FIRES. The heir anchored what it
    // inherited at the first time it was handed, the swap's, so the boundary is exact.
    r.run_until(anchored + 5000 - 1);
    CHECK(r.ask_probe(probe).fires == 0);

    // 9. THE BOUNDARY: exactly once. It did not wait a fresh five seconds, which is what a
    // re-anchored schedule would have done.
    r.run_until(anchored + 5000);
    rep = r.ask_probe(probe);
    CHECK(rep.fires == 1);

    // 10. AND IT IS SPENT — an honest lifecycle state, not a bool that could
    // only say "wanted".
    CHECK(rep.lifecycle == "spent");
    CHECK(r.poke_int(successor, "active") == 0);

    // 11. ANOTHER AVAILABILITY NOTICE RECREATES NOTHING. This is exactly the
    // resurrection a single `desired` flag could not prevent.
    r.bus.publish(loom::Message(loom::to_value(TimerReady{})));
    r.settle();
    rep = r.ask_probe(probe);
    CHECK(rep.fires == 1);
    CHECK(rep.lifecycle == "spent");
    CHECK(r.poke_int(successor, "active") == 0);

    // 12. AN EXPLICIT RESTART ARMS IT AGAIN — once, from the full declared
    // delay, and the receipt says restarted rather than preserved.
    r.ask(probe, zengine::probe::RestartProbe{});
    r.pump_until(TimerResolution::zen_name);
    const std::int64_t restarted = r.clock.now_ms();
    rep = r.ask_probe(probe);
    CHECK(rep.lifecycle == "waiting");
    CHECK(rep.resolved == kResolutionRestarted);
    CHECK(r.poke_int(successor, "active") == 1);

    r.run_until(restarted + 5000);
    rep = r.ask_probe(probe);
    CHECK(rep.fires == 2); // once more, and only once more
    CHECK(rep.lifecycle == "spent");
}

TEST_CASE("continuity, hard: no letter exists, so the declared fallback restarts the delay — "
          "and says so instead of pretending") {
    Rig r;
    const loom::WeaveId timer = r.load("zengine-timer-virtual", TIMER_VIRTUAL_SO, kTimerRole);
    const loom::WeaveId probe = r.load("zengine-probe-oneshot", PROBE_ONESHOT_SO, "");
    const std::int64_t anchored = anchor_probe(r, probe, timer);
    r.run_until(anchored + 3000);

    // A HARD replacement: the incumbent is never asked, so nothing is offered.
    const std::uint64_t corr = r.command(loom::SwapWeave{
        kTimerRole, "zengine-timer-virtual", TIMER_VIRTUAL_SO, /*graceful=*/false});
    r.pump_until(TimerResolution::zen_name);
    const Seen::Answer* swapped = r.seen.find(corr);
    REQUIRE(swapped != nullptr);
    REQUIRE_MESSAGE(swapped->kind == 0, "swap refused: ", swapped->text);
    const loom::WeaveId successor{static_cast<std::uint64_t>(std::stoll(swapped->text))};

    // No letter crossed at all — not an empty one, none.
    CHECK(r.letters.empty());
    CHECK(r.poke_int(successor, "inherited") == 0);

    // The default order resolves through its FALLBACK, and the reason names both
    // halves: what was unavailable, and what was done instead.
    zengine::probe::ProbeReport rep = r.ask_probe(probe);
    CHECK(rep.resolved == kResolutionRestarted);
    CHECK(rep.reason.find("was unavailable") != std::string::npos);
    CHECK(rep.reason.find("fell back to 'restart_delay'") != std::string::npos);
    CHECK(rep.fires == 0);

    // AND IT DOES NOT FIRE AT THE OLD BOUNDARY. Two seconds was the whole remaining schedule a
    // moment ago; from a full restart it is not even half of one.
    r.run_until(anchored + 5000 + 10);
    CHECK(r.ask_probe(probe).fires == 0);

    // It fires at the FULL declared delay from the restart instead, which is the honest meaning
    // of "restarted".
    r.run_until(anchored + 3000 + 5000);
    CHECK(r.ask_probe(probe).fires == 1);
}

TEST_CASE("continuity, reload: an in-place reload keeps every standing timer, one-shot and "
          "repeating, with the time it had left") {
    Rig r;
    const loom::WeaveId timer = r.load("zengine-timer-virtual", TIMER_VIRTUAL_SO, kTimerRole);
    const loom::WeaveId probe = r.load("zengine-probe-oneshot", PROBE_ONESHOT_SO, "");
    const std::int64_t anchored = anchor_probe(r, probe, timer);
    // ...and a raw repeating timer nobody re-asks for: only the service can carry it. Its
    // firings are the wakes that hand the Timer the time.
    r.ask(timer, StartTimer{"suite.repeat", 1000, true});
    r.run_until(anchored + 3000);
    const auto repeats = [&] {
        return std::count(r.seen.fired.begin(), r.seen.fired.end(), "suite.repeat");
    };
    const auto repeats_before = repeats();
    REQUIRE(repeats_before == 3);

    const std::uint64_t corr =
        r.command(loom::ReloadWeave{"zengine-timer-virtual", TIMER_VIRTUAL_SO});
    r.pump_until(TimerResolution::zen_name); // the probe re-asks on the reloaded Timer's notice
    const Seen::Answer* reloaded = r.seen.find(corr);
    REQUIRE(reloaded != nullptr);
    REQUIRE_MESSAGE(reloaded->kind == 1, "reload refused: ", reloaded->text);
    CHECK(r.kernel.weave_id("zengine-timer-virtual") == timer);
    CHECK(r.letters.empty()); // nothing crossed as a letter: the state carried it

    // THE CONSUMER ASKED TO PRESERVE, AND THE RELOADED TIMER HAD SOMETHING TO PRESERVE.
    zengine::probe::ProbeReport rep = r.ask_probe(probe);
    CHECK(rep.resolved == kResolutionPreserved);
    CHECK(rep.fires == 0);
    CHECK(r.poke_int(timer, "active") == 2);
    CHECK(r.poke_int(timer, "inherited") == 2);

    // THE RULER: the reloaded table, read off its own letter, holds both, the one-shot with the
    // two seconds it had left -- not a fresh five.
    r.ask(timer, loom::PrepareShutdown{});
    r.pump_until(loom::Bequest::zen_name);
    REQUIRE(r.letters.size() == 1);
    const std::vector<TimerHandoffEntry> held = Rig::handoff_of(r.letters[0]);
    REQUIRE(held.size() == 2);
    const auto oneshot = std::find_if(held.begin(), held.end(), [](const TimerHandoffEntry& e) {
        return e.id == zengine::probe::kProbeTimerId;
    });
    REQUIRE(oneshot != held.end());
    CHECK(oneshot->requester == std::to_string(probe.value));
    CHECK(oneshot->delay_ms == 5000);
    CHECK(oneshot->remaining_ms == 2000);

    // IT FIRES WHEN ITS OWN TIME RUNS OUT, ONCE; THE REPEATING ONE KEEPS FIRING.
    r.run_until(anchored + 5000 - 1);
    CHECK(r.ask_probe(probe).fires == 0);
    r.run_until(anchored + 5000);
    rep = r.ask_probe(probe);
    CHECK(rep.fires == 1);
    CHECK(rep.lifecycle == "spent");
    CHECK(repeats() > repeats_before);
}

TEST_CASE("continuity, required preservation: an order with no acceptable fallback is REFUSED, "
          "and creates no schedule at all") {
    Rig r;
    const loom::WeaveId timer = r.load("zengine-timer-virtual", TIMER_VIRTUAL_SO, kTimerRole);
    const loom::WeaveId probe = r.load("zengine-probe-required", PROBE_REQUIRED_SO, "");

    // Same source, same declaration, one difference: preservation is REQUIRED.
    // On an initial load there is nothing to preserve, so the order is refused —
    // and refusal here means nothing happened, not "something else did".
    const zengine::probe::ProbeReport rep = r.ask_probe(probe);
    CHECK(rep.resolved == kResolutionRefused);
    CHECK(rep.reason.find("no fallback was acceptable") != std::string::npos);
    CHECK(rep.reason.find("no schedule was created or changed") != std::string::npos);
    CHECK(rep.fires == 0);
    CHECK(r.poke_int(timer, "active") == 0); // no entry, not a silently-restarted one

    // And it stays that way: a refused order is not a deferred one.
    r.run_for(5200);
    CHECK(r.ask_probe(probe).fires == 0);
    CHECK(r.poke_int(timer, "active") == 0);
}

// ============================================================================
// Tier 2 continuity — the unit pins, on the host's wait (TIMER-03)
// ============================================================================
// The end-to-end lane proves the whole sentence; these prove the pieces, including those an
// honest predecessor never produces: a letter to another version, one over the bound, a
// steward that never answers.

TEST_CASE("letter: the predecessor describes every standing entry as a REMAINING duration, "
          "reads its clock once, and changes nothing by being asked") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Refuse);
    r.run_wakes(3, /*start=*/true);

    r.ask_as(r.ear, StartTimer{"mine.oneshot", 5000, false});
    r.ask_as(r.ear, StartRoleTimer{"slot.beat", 9000, true, "some.slot"});
    // ...and a short repeating timer, so the host's wait hands the Timer the time as it passes.
    r.ask_as(r.ear, StartTimer{"mine.tick", 400, true});
    r.run_until(1200);

    // The two long ones are nowhere near due, and the letter is written between wakes, so
    // writing it provably changes nothing — which is what makes the no-movement assertion at
    // the end mean something.
    const std::int64_t fired_before = r.count("fired");
    const std::int64_t active_before = r.count("active");
    REQUIRE(active_before == 3);
    REQUIRE(fired_before == 3);

    const std::vector<TimerHandoffEntry> offered = r.prepare_shutdown();
    REQUIRE(offered.size() == 3);

    // A REQUESTER entry round-trips whole: the id, the mode (no role), the
    // declared delay, the repeat flag, and the requester as lossless decimal.
    CHECK(offered[0].id == "mine.oneshot");
    CHECK(offered[0].role.empty());
    CHECK(offered[0].delay_ms == 5000);
    CHECK_FALSE(offered[0].repeat);
    CHECK(offered[0].requester == std::to_string(r.ear.value));
    // The whole arithmetic claim, exactly: what is left is what was declared minus what has
    // elapsed since the schedule was anchored -- the host's clock at the ask and at the letter,
    // the Timer last handed the time at the latter -- not a hoped-for constant.
    REQUIRE(r.ask_now >= 0);
    REQUIRE(r.letter_now == 1200);
    CHECK(offered[0].remaining_ms == 5000 - (r.letter_now - r.ask_now));

    // A ROLE entry round-trips too, carrying the role it belongs to — the two
    // addressing modes are different promises, and a letter that dropped the
    // role would silently convert one into the other.
    CHECK(offered[1].id == "slot.beat");
    CHECK(offered[1].role == "some.slot");
    CHECK(offered[1].repeat);
    CHECK(offered[1].delay_ms == 9000);

    // AND THE PREDECESSOR DID NOT MOVE. Being asked to describe a schedule is
    // not an event in that schedule's life: nothing fired, nothing was
    // cancelled, nothing advanced.
    CHECK(r.count("fired") == fired_before);
    CHECK(r.count("active") == active_before);
}

TEST_CASE("letter: what is left is measured from the last time the Timer was handed -- the time "
          "since its last wake is paused, as replacement downtime is") {
    // The Timer reads no clock: it knows the time the host's wait last handed it. Between wakes
    // the host's clock moves and the Timer is told nothing, so a letter written then measures
    // from that wake. Pinned as the behaviour it is, beside its bound: the gap is never longer
    // than the time to the soonest deadline, since the wait wakes the Timer then.
    FakeRig r;
    r.mount_steward(Letters::Answer::Refuse);
    r.run_wakes(3, /*start=*/true);
    r.ask_as(r.ear, StartTimer{"far", 5000, false});
    r.run_wakes(1); // the time learned at once: anchored at 0, due at 5000
    const std::int64_t wakes = r.count("wakes");
    r.run_until(3000); // the clock moves; nothing is due, so nothing wakes the Timer
    REQUIRE(r.count("wakes") == wakes);

    const std::vector<TimerHandoffEntry> offered = r.prepare_shutdown();
    REQUIRE(offered.size() == 1);
    CHECK(offered[0].remaining_ms == 5000); // from the time last handed, 0, not from 3000
}

TEST_CASE("letter: a due or overdue entry transfers with ZERO remaining, never a negative "
          "number") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Refuse);
    r.run_wakes(3, /*start=*/true);

    // Queued so the letter is written in the SAME pump, before any beat can
    // fire it: a schedule that is due right now is exactly the case an
    // unguarded subtraction gets wrong.
    r.ask_as(r.ear, StartTimer{"due.now", 0, false});
    const std::vector<TimerHandoffEntry> offered = r.prepare_shutdown();
    REQUIRE(offered.size() == 1);
    CHECK(offered[0].id == "due.now");
    CHECK(offered[0].remaining_ms == 0);
}

TEST_CASE("letter: the published bound is the same number on both sides — the writer carries "
          "at most it, and the reader refuses more than it, whole") {
    // The writing side. Every entry over the bound is simply not carried; the
    // bound is published (kMaxHandoffEntries) rather than discovered as a leak.
    FakeRig w;
    w.mount_steward(Letters::Answer::Refuse);
    w.run_wakes(3, /*start=*/true);
    for (std::size_t i = 0; i < kMaxHandoffEntries + 5; ++i) {
        w.ask_as(w.ear, StartTimer{"t" + std::to_string(i), 1000, true});
    }
    w.run_wakes(4);
    CHECK(w.count("active") == static_cast<std::int64_t>(kMaxHandoffEntries + 5));
    CHECK(w.prepare_shutdown().size() == kMaxHandoffEntries);

    // The reading side. A letter claiming MORE than the bound is untrusted
    // input — an honest predecessor cannot produce one — so nothing is adopted
    // rather than the parseable half of it.
    FakeRig r;
    TimerHandoff huge;
    for (std::size_t i = 0; i < kMaxHandoffEntries + 1; ++i) {
        huge.entries.push_back(
            TimerHandoffEntry{"1", "t" + std::to_string(i), "", 1000, true, 500});
    }
    r.mount_steward(Letters::Answer::Letter);
    r.letters.items.push_back(loom::bequeath_item(huge));
    r.run_wakes(4, /*start=*/true);
    CHECK(r.count("inherited") == 0);
    CHECK(r.count("active") == 0);
    CHECK(r.heard.ready == 1); // and it still became available: refusing a letter is not a hang
}

TEST_CASE("letter: bytes the gate refuses, a shape from another version, and a requester that "
          "is not a lossless id are each adopted as NOTHING") {
    // Garbage that is not a Zen value at all.
    {
        FakeRig r;
        r.mount_steward(Letters::Answer::Letter);
        r.letters.items.push_back(loom::Bytes{0x00, 0x01, 0x02, 0x03});
        r.run_wakes(4, /*start=*/true);
        CHECK(r.count("inherited") == 0);
        CHECK(r.heard.ready == 1);
    }
    // A perfectly well-formed message of a DIFFERENT shape. The version
    // detection is the gate's verdict, never a label this weave chose to trust:
    // claim_item re-admits the bytes against TimerHandoff's own schema, and a
    // v2 handoff simply is not that shape.
    {
        FakeRig r;
        r.mount_steward(Letters::Answer::Letter);
        r.letters.items.push_back(loom::bequeath_item(HandoffV2{{}}));
        r.run_wakes(4, /*start=*/true);
        CHECK(r.count("inherited") == 0);
        CHECK(r.count("active") == 0);
        CHECK(r.heard.ready == 1);
    }
    // The right shape, one entry whose requester is not canonical decimal. A
    // letter is adopted WHOLE or not at all, so the good entry beside it does
    // not come across either.
    {
        FakeRig r;
        TimerHandoff mixed;
        mixed.entries.push_back(TimerHandoffEntry{"7", "fine", "", 1000, true, 400});
        mixed.entries.push_back(TimerHandoffEntry{"-3", "bad", "", 1000, true, 400});
        r.mount_steward(Letters::Answer::Letter);
        r.letters.items.push_back(loom::bequeath_item(mixed));
        r.run_wakes(4, /*start=*/true);
        CHECK(r.count("inherited") == 0);
        CHECK(r.count("active") == 0);
    }
}

TEST_CASE("bootstrap: the claim is made once per activation, restoration happens BEFORE the "
          "availability notice, and a late or duplicate letter changes nothing") {
    FakeRig r;
    TimerHandoff handoff;
    handoff.entries.push_back(
        TimerHandoffEntry{std::to_string(r.ear.value), "inherited.tick", "", 1000, true, 250});
    r.mount_steward(Letters::Answer::Letter);
    r.letters.items.push_back(loom::bequeath_item(handoff));

    r.run_wakes(3, /*start=*/true);

    // Exactly one claim, and it inherited exactly one entry.
    CHECK(r.letters.claims == 1);
    CHECK(r.count("inherited") == 1);
    CHECK(r.count("active") == 1);

    // ORDER, off the bus's own tape: the letter was delivered here BEFORE the
    // service told anyone it was available. That is the whole ordering rule —
    // a consumer that heard TimerReady first would re-ask and re-anchor the
    // very schedule the letter carried.
    const std::int64_t letter_at = r.first(loom::Bequest::zen_name);
    const std::int64_t ready_at = r.first("TimerReady");
    REQUIRE(letter_at >= 0);
    REQUIRE(ready_at >= 0);
    CHECK(letter_at < ready_at);

    // ...and the claim went out before either.
    const std::int64_t claim_at = r.first(loom::ClaimBequest::zen_name);
    REQUIRE(claim_at >= 0);
    CHECK(claim_at < letter_at);

    // A SECOND letter, correctly correlated, arriving after the decision. It is
    // ignored: once resolved, always resolved. (Forged or merely late, the
    // answer is the same, which is the point.)
    loom::Bequest second;
    second.role = kTimerRole;
    TimerHandoff other;
    other.entries.push_back(
        TimerHandoffEntry{std::to_string(r.ear.value), "sneaky", "", 9000, true, 9000});
    second.items.push_back(loom::bequeath_item(other));
    r.bus.send_as(r.steward, r.service,
                  loom::Message(loom::to_value(second), r.steward, r.steward,
                                r.letters.last_claim_corr));
    r.run_wakes(4);
    CHECK(r.count("inherited") == 1); // unchanged
    CHECK(r.count("active") == 1);    // "sneaky" never joined the table
    CHECK(r.heard.ready == 1);        // and nothing re-announced
}

TEST_CASE("bootstrap: a steward that never answers does not hang the service — it resolves "
          "fresh after its own bounded wakes, with no wall clock and no spin") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Silence);
    r.ask_as(r.ear, StartTimer{"pre.activation", 100, true});

    r.run_wakes(3, /*start=*/true);
    CHECK(r.letters.claims == 1); // it did ask
    CHECK(r.heard.ready == 1);    // and it became available anyway
    CHECK(r.count("inherited") == 0);

    // It became LIVE, not merely resolved: the entry it was holding is standing and firing.
    CHECK(r.count("active") == 1);
    const std::int64_t fired = r.count("fired");
    r.run_wakes(3);
    CHECK(r.count("fired") > fired);

    // And it keeps time by its claim alone: the bus holds nothing between wakes.
    CHECK(r.bus.pending() == 0);
    CHECK(claimed_until(r).has_value());
}

TEST_CASE("bootstrap: operations that arrive while the decision is pending are applied AFTER "
          "the inheritance, so a fresh request beats inherited state for the same key") {
    FakeRig r;
    TimerHandoff handoff;
    handoff.entries.push_back(
        TimerHandoffEntry{std::to_string(r.ear.value), "contested", "", 5000, false, 4000});
    r.mount_steward(Letters::Answer::Letter);
    r.letters.items.push_back(loom::bequeath_item(handoff));

    // The activation and a competing order, enqueued back to back. The order
    // arrives while the claim is still out.
    r.activate();
    r.ask_as(r.ear, EnsureTimer{"contested", 5000, false, kRestartDelay, ""});
    r.settle(); // no time passes: the decision, the replay and the time learned, at 0

    // The inheritance happened AND the later request won: one entry, and its
    // schedule is the requested restart rather than the inherited 4000ms
    // remainder. (Applied under the inheritance instead, the entry would still
    // be due 4000ms after restoration.)
    CHECK(r.count("inherited") == 1);
    CHECK(r.count("active") == 1);
    REQUIRE(r.heard.resolutions.size() == 1);
    CHECK(r.heard.resolutions[0].id == "contested");
    CHECK(r.heard.resolutions[0].resolved == kResolutionRestarted);

    // And it is timed as a restart: nothing at the inherited boundary...
    r.run_until(4100); // well past the inherited 4000
    CHECK(r.heard.fired.empty());
    // ...and a firing at the declared delay from when the order landed.
    r.run_until(5000);
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired[0] == "contested");
    CHECK(r.heard.fired_at[0] == 5000);
}

TEST_CASE("bootstrap: the hold is bounded, and overflowing it is visible both ways it can be") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Silence);
    r.activate();
    // One more than the hold can take, all ordered so each has somewhere to
    // hear an answer.
    for (std::size_t i = 0; i < kMaxDeferredOps + 1; ++i) {
        r.ask_as(r.ear,
                 EnsureTimer{"held" + std::to_string(i), 1000, true, kRestartDelay, ""});
    }
    r.run_wakes(4);

    // The bound held: the last one did not join the table...
    CHECK(r.count("active") == static_cast<std::int64_t>(kMaxDeferredOps));
    // ...it was counted...
    CHECK(r.count("deferred_dropped") == 1);
    // ...and its requester was TOLD, which is the half a counter alone cannot do.
    bool refused_one = false;
    for (const TimerResolution& t : r.heard.resolutions) {
        if (t.resolved == kResolutionRefused &&
            t.reason.find("still restoring") != std::string::npos) {
            refused_one = true;
        }
    }
    CHECK(refused_one);
}

TEST_CASE("order: the menu resolves by MATCHING, and a changed schedule meaning can never "
          "masquerade as preserved") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Refuse);
    r.run_wakes(3, /*start=*/true);

    // Nothing standing: preservation is unavailable, the fallback restarts.
    r.ask_as(r.ear, EnsureTimer{"m", 1000, true, kPreserveRemaining, kRestartDelay});
    r.settle();
    REQUIRE(r.heard.resolutions.size() == 1);
    CHECK(r.heard.resolutions.back().resolved == kResolutionRestarted);

    // The SAME declaration now matches: same key, same repeat mode, same delay.
    r.ask_as(r.ear, EnsureTimer{"m", 1000, true, kPreserveRemaining, kRestartDelay});
    r.settle();
    CHECK(r.heard.resolutions.back().resolved == kResolutionPreserved);

    // A DIFFERENT DELAY under the same id is a different schedule. The entry is
    // found, but preserving it would be describing a schedule nobody asked for.
    r.ask_as(r.ear, EnsureTimer{"m", 2000, true, kPreserveRemaining, kRestartDelay});
    r.settle();
    CHECK(r.heard.resolutions.back().resolved == kResolutionRestarted);
    CHECK(r.heard.resolutions.back().reason.find("differs from the standing one") !=
          std::string::npos);

    // A DIFFERENT REPEAT MODE likewise: "every two seconds" and "once, in two
    // seconds" are not the same timer wearing different clothes.
    r.ask_as(r.ear, EnsureTimer{"m", 2000, false, kPreserveRemaining, kRestartDelay});
    r.settle();
    CHECK(r.heard.resolutions.back().resolved == kResolutionRestarted);

    // A DIFFERENT ADDRESSING MODE has a different key by construction, so it
    // finds nothing at all — and the requester-addressed entry it did not match
    // is left exactly where it was.
    const std::int64_t active_before = r.count("active");
    r.ask_as(r.ear,
             EnsureRoleTimer{"m", 2000, false, "some.slot", kPreserveRemaining, kRestartDelay});
    r.settle();
    CHECK(r.heard.resolutions.back().resolved == kResolutionRestarted);
    CHECK(r.count("active") == active_before + 1);
}

TEST_CASE("order: drop removes a standing schedule and declines to create one, and an unknown "
          "preference is refused rather than guessed at") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Refuse);
    r.run_wakes(3, /*start=*/true);

    r.ask_as(r.ear, StartTimer{"d", 1000, true});
    r.settle();
    REQUIRE(r.count("active") == 1);

    // Drop removes it.
    r.ask_as(r.ear, EnsureTimer{"d", 1000, true, kDrop, ""});
    r.settle();
    CHECK(r.heard.resolutions.back().resolved == kResolutionDropped);
    CHECK(r.count("active") == 0);

    // Drop with nothing to drop declines to create one — same word, and it is
    // the truthful one either way.
    r.ask_as(r.ear, EnsureTimer{"never.existed", 1000, true, kDrop, ""});
    r.settle();
    CHECK(r.heard.resolutions.back().resolved == kResolutionDropped);
    CHECK(r.count("active") == 0);

    // A spelling this package does not know is REFUSED. Nothing is guessed at,
    // and the reason quotes the word back so a stranger can see the typo.
    r.ask_as(r.ear, EnsureTimer{"x", 1000, true, "preserve-remaining", kRestartDelay});
    r.settle();
    CHECK(r.heard.resolutions.back().resolved == kResolutionRefused);
    CHECK(r.heard.resolutions.back().reason.find("preserve-remaining") != std::string::npos);
    CHECK(r.count("active") == 0);

    // An ordered ROLE request with no role is refused too — the raw shape drops
    // it silently, but an order has somewhere to hear why.
    r.ask_as(r.ear, EnsureRoleTimer{"y", 1000, true, "", kPreserveRemaining, kRestartDelay});
    r.settle();
    CHECK(r.heard.resolutions.back().resolved == kResolutionRefused);
    CHECK(r.heard.resolutions.back().reason.find("no role") != std::string::npos);
    CHECK(r.count("active") == 0);
}

TEST_CASE("binding lifecycle: a one-shot is SPENT before its callback runs, does not reconcile "
          "afterwards, and re-arms only when something deliberately says so") {
    loom::Switchboard bus;
    AskSeen seen;
    const loom::WeaveId weave = loom::mount<Shooter>(bus);
    const loom::WeaveId service = mount_role<AskCatcher>(bus, kTimerRole, seen);
    const loom::WeaveId door = zengine::testing::mount_door(bus);

    const auto activate = [&](std::int64_t sequence) {
        zengine::testing::order_activation(bus, door, weave, sequence);
        bus.drain_until_idle();
    };
    const auto fire = [&] {
        bus.send_as(service, weave,
                    loom::Message(loom::to_value(TimerFired{"shooter.shot"}), service, service, 0));
        bus.drain_until_idle();
    };
    const auto ready = [&] {
        bus.publish(loom::Message(loom::to_value(TimerReady{})));
        bus.drain_until_idle();
    };
    const auto shooter = [&] { return static_cast<Shooter*>(bus.weave(weave)); };

    activate(1);
    REQUIRE(seen.asks.size() == 1);
    CHECK(shooter()->shot_.waiting());

    // It fires. `once` means once per binding incarnation.
    fire();
    CHECK(shooter()->shot_.spent());

    // A SPENT binding does not reconcile -- not on an availability notice, not on a fresh
    // activation. Nothing resurrects it, which a single `desired` flag could not express.
    ready();
    CHECK(seen.asks.size() == 1);
    activate(2);
    CHECK(seen.asks.size() == 1);
    CHECK(shooter()->shot_.spent());

    // An explicit restart arms it again, ONCE, and it reconciles normally from
    // then on.
    loom::Bus& root = bus;
    (void)root;
    bus.send(weave, loom::Message(loom::to_value(ArmFromCallback{})));
    bus.drain_until_idle();

    // ...and now the ordering pin itself: the callback re-arms from INSIDE the
    // firing. That can only work if Spent was written before the callback ran —
    // otherwise the mark would land on top of the restart and the binding would
    // end up spent with an ask already sent, i.e. permanently out of step.
    const std::size_t asks_before = seen.asks.size();
    fire();
    CHECK(shooter()->shot_.waiting());        // the callback's word was the last one
    CHECK(seen.asks.size() == asks_before + 1); // and the restart really asked
}

TEST_CASE("binding: a receipt updates the binding it names, and only that one; an id nobody "
          "declared is ignored") {
    BindRig r;
    r.seen.answer_with = kResolutionPreserved;
    r.activate();

    Bound* w = static_cast<Bound*>(r.bus.weave(r.weave));
    REQUIRE(w->tick_.resolution() == kResolutionPreserved);
    REQUIRE(w->role_.resolution() == kResolutionPreserved);

    // A receipt naming exactly one binding moves exactly that one.
    r.bus.send_as(r.service, r.weave,
                  loom::Message(loom::to_value(TimerResolution{"bound.tick", kResolutionDropped,
                                                               "because the test said so"}),
                                r.service, r.service, 0));
    r.bus.drain_until_idle();
    CHECK(w->tick_.resolution() == kResolutionDropped);
    CHECK(w->tick_.resolution_reason() == "because the test said so");
    CHECK(w->role_.resolution() == kResolutionPreserved); // the neighbour is untouched

    // A receipt for an id this weave never declared is data, not news. (A role
    // beat can be aimed at by anyone, so this is the ordinary case rather than
    // the hostile one — the same consumer obligation a firing carries.)
    r.bus.send_as(r.service, r.weave,
                  loom::Message(loom::to_value(TimerResolution{"somebody.elses", kResolutionRefused,
                                                               "not yours"}),
                                r.service, r.service, 0));
    r.bus.drain_until_idle();
    CHECK(w->tick_.resolution() == kResolutionDropped);
    CHECK(w->role_.resolution() == kResolutionPreserved);
}

TEST_CASE("binding edges: an empty role is refused at declaration, and an invalid handle fails "
          "loudly rather than dereferencing nothing") {
    // AN EMPTY ROLE cannot mean "the beat belongs to a slot", and the service
    // treats a role-addressed ask with no role as no ask at all — so accepting
    // one here would leave the author with a binding that quietly behaves like
    // the requester-addressed mode they deliberately did not choose.
    loom::Switchboard bus;
    CHECK_THROWS_AS(loom::mount<EmptyRoleRepeat>(bus), std::invalid_argument);
    CHECK_THROWS_AS(loom::mount<EmptyRoleOnce>(bus), std::invalid_argument);

    // A DEFAULT HANDLE is part of the public surface, because valid() exists.
    // Asking one about the binding it does not name is a programmer error and
    // takes the project's path for one: loud, and never a null dereference.
    TimerHandle<Bound> nowhere;
    CHECK_FALSE(nowhere.valid());
    CHECK_THROWS_AS((void)nowhere.id(), std::logic_error);
    CHECK_THROWS_AS((void)nowhere.resolution(), std::logic_error);
    CHECK_THROWS_AS((void)nowhere.resolution_reason(), std::logic_error);

    // The safe questions stay safe and answerable: an invalid handle wants
    // nothing and cancels nothing.
    CHECK(nowhere.state() == BindingState::Canceled);
    CHECK_FALSE(nowhere.waiting());
}

TEST_CASE("direct load: a Timer loaded straight through the control door, with NO steward in "
          "the process at all, still becomes live — and keeps time by its claim alone") {
    // The whole point of this case is what is MISSING. There is no Weave Manager, so
    // `zen.manager` is unheld and the service's claim (`zen.ClaimBequest`) reaches nobody: the
    // bus refuses it, and that refusal is the service's answer -- a fresh start, at once. A
    // bootstrap that waited on an answer would wait forever, on a working bus, and the operator
    // would see a load that succeeded and a service that never spoke.
    loom::Switchboard bus;
    loom::host::ManualClock clock;
    loom::host::Reactor wait(bus, clock);
    wait.trust(kTimerRole);
    loom::Kernel kernel{bus, loom::trust_every_artifact(
        "Zengine's test harness: it loads only this build tree's output")};
    const loom::WeaveId control = loom::mount_control(kernel, bus);

    Seen seen;
    loom::Grant reach = loom::load_capability(control); // the dangerous half, target-scoped
    reach.allow_to_any(StartTimer::zen_name, StartTimer::zen_version); // and one ordinary ask
    const loom::WeaveId witness = loom::mount_granted<Witness>(bus, std::move(reach), seen);
    const auto settle = [&] { host_loop(bus, wait, clock, [] { return true; }); };

    const std::uint64_t corr = 1;
    bus.send_as(witness, control,
                loom::Message(loom::to_value(loom::LoadLibrary{"zengine-timer-virtual",
                                                               TIMER_VIRTUAL_SO, {kTimerRole}}),
                              witness, witness, corr));
    settle();

    const Seen::Answer* a = seen.find(corr);
    REQUIRE(a != nullptr);
    REQUIRE_MESSAGE(a->kind == 0, "load refused: ", a->text);

    // AVAILABLE: the claim reached nobody and the bootstrap resolved itself on the bus's
    // refusal -- no wall clock, no timeout, no steward.
    CHECK(seen.ready == 1);
    CHECK(bus.pending() == 0); // and nothing queued to keep time

    // And it does real work, which is the difference between "resolved" and "alive": a timer
    // asked for here fires, at its time on the host's clock.
    const loom::WeaveId service{static_cast<std::uint64_t>(std::stoll(a->text))};
    const std::int64_t asked_at = clock.now_ms();
    bus.send_as(witness, service,
                loom::Message(loom::to_value(StartTimer{"direct.tick", 20, false}), witness,
                              witness, 0));
    host_loop(bus, wait, clock, [] { return false; }, asked_at + 100);
    REQUIRE(seen.fired.size() == 1);
    CHECK(seen.fired[0] == "direct.tick");
    CHECK(bus.pending() == 0);
}

// ============================================================================
// The hostile half: an ordinary weave trying to be the steward
// ============================================================================
// It knows every public schema, the published `kClaimCorrelation`, the Timer's role and a
// handoff the gate passes, and may send `zen.Bequest` and `zen.Refused` to anyone; it lacks the
// one thing that is not a value -- Loom's word that the Timer's claim was delivered to it.

namespace {

/// The impersonator's order: what to send, and with which correlation.
struct ForgeLetter {
    bool refused = false;   ///< send zen.Refused instead of a letter
    std::int64_t remaining = 0; ///< the remaining time the forged handoff claims
    std::int64_t correlation = 0;
    ZEN_SHAPE(ForgeLetter, 1, ZEN_FIELD(refused), ZEN_FIELD(remaining),
              ZEN_FIELD(correlation));
};

struct ForgerState {
    std::int64_t sent = 0;
    ZEN_SHAPE(ForgerState, 1, ZEN_FIELD(sent));
};

/// A weave that forges a steward's answer. Everything it sends is legal, gated,
/// stamped, and shaped exactly right.
class LetterForger : public loom::WeaveBase<LetterForger, ForgerState, loom::Accept<ForgeLetter>,
                                            loom::Emit<loom::Bequest, loom::Refused>> {
public:
    LetterForger(loom::WeaveId victim, loom::WeaveId requester)
        : victim_(victim), requester_(requester) {}

    void on(const ForgeLetter& f, loom::Mail& mail) {
        ++state_.sent;
        const auto corr = static_cast<std::uint64_t>(f.correlation);
        if (f.refused) {
            mail.send(victim_, loom::Refused{"no bequest is held for you"}, corr);
            return;
        }
        // A handoff that would PASS THE GATE and, if adopted, would aim this
        // service's future firings at a weave of the forger's choosing. That is
        // the concrete harm, and why this letter's provenance is load-bearing
        // rather than tidy.
        TimerHandoff handoff;
        handoff.entries.push_back(TimerHandoffEntry{std::to_string(requester_.value), "forged.tick",
                                                    "", 1000, true, f.remaining});
        loom::Bequest letter;
        letter.role = kTimerRole;
        letter.items.push_back(loom::bequeath_item(handoff));
        mail.send(victim_, letter, corr);
    }

private:
    loom::WeaveId victim_;
    loom::WeaveId requester_;
};

} // namespace

TEST_CASE("hostile: a forged letter with the right shape, the right correlation and gate-valid "
          "handoff bytes inherits NOTHING — and does not close the claim either") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Silence); // no genuine answer will ever come
    const loom::WeaveId forger = loom::mount<LetterForger>(r.bus, r.service, r.ear);

    // The claim goes out on activation; the forgery is queued right behind it,
    // squarely inside the window the heir is waiting in.
    r.activate();
    r.bus.send(forger, loom::Message(loom::to_value(ForgeLetter{
                           false, 4000, static_cast<std::int64_t>(kClaimCorrelation)})));
    r.run_wakes(4);

    // It really was sent and really was delivered — this is a rejection by
    // PROVENANCE, not by routing, not by a grant, and not by the gate.
    CHECK(static_cast<LetterForger*>(r.bus.weave(forger))->snapshot().get("sent")->as_int() == 1);
    CHECK(r.first(loom::Bequest::zen_name) >= 0);

    // And it changed nothing: nothing inherited, nothing standing.
    CHECK(r.count("inherited") == 0);
    CHECK(r.count("active") == 0);

    // The claim was not closed by it either — the service still resolved through
    // its own bounded fallback, which is what keeps a forgery from being a
    // denial-of-service as well as an impersonation.
    CHECK(r.heard.ready == 1);
    CHECK(r.bus.pending() == 0); // and nothing queued to keep time
}

TEST_CASE("hostile: a forged REFUSAL cannot end the heir's wait early either") {
    FakeRig r;
    TimerHandoff handoff;
    handoff.entries.push_back(
        TimerHandoffEntry{std::to_string(r.ear.value), "real.tick", "", 1000, true, 250});
    r.mount_steward(Letters::Answer::Letter);
    r.letters.items.push_back(loom::bequeath_item(handoff));
    const loom::WeaveId forger = loom::mount<LetterForger>(r.bus, r.service, r.ear);

    // The forgery races ahead of the genuine answer, claiming there is nothing
    // to inherit. If it were believed, the real letter would arrive at a
    // service that had already resolved — and the schedule would be lost.
    r.activate();
    r.bus.send(forger, loom::Message(loom::to_value(ForgeLetter{
                           true, 0, static_cast<std::int64_t>(kClaimCorrelation)})));
    r.run_wakes(4);

    // The genuine letter won, because the forged refusal was never an answer.
    CHECK(r.count("inherited") == 1);
    CHECK(r.count("active") == 1);
    CHECK(r.heard.ready == 1);
}

TEST_CASE("hostile: a forgery AFTER the legitimate answer cannot overwrite what was inherited") {
    FakeRig r;
    TimerHandoff handoff;
    handoff.entries.push_back(
        TimerHandoffEntry{std::to_string(r.ear.value), "real.tick", "", 1000, true, 250});
    r.mount_steward(Letters::Answer::Letter);
    r.letters.items.push_back(loom::bequeath_item(handoff));
    const loom::WeaveId forger = loom::mount<LetterForger>(r.bus, r.service, r.ear);

    r.activate();
    r.run_wakes(4);
    REQUIRE(r.count("inherited") == 1);
    REQUIRE(r.count("active") == 1);

    // Late, correlated, gate-valid, and describing a completely different world.
    r.bus.send(forger, loom::Message(loom::to_value(ForgeLetter{
                           false, 9000, static_cast<std::int64_t>(kClaimCorrelation)})));
    r.run_wakes(4);

    CHECK(r.count("inherited") == 1); // the count is of the LAST bootstrap, unchanged
    CHECK(r.count("active") == 1);    // and "forged.tick" never joined the table
    CHECK(r.heard.ready == 1);        // nothing re-announced
}

TEST_CASE("hostile: even the legitimate steward's own bytes, replayed ordinarily, are not an "
          "answer — the word does not travel with the message") {
    FakeRig r;
    TimerHandoff handoff;
    handoff.entries.push_back(
        TimerHandoffEntry{std::to_string(r.ear.value), "real.tick", "", 1000, true, 250});
    // The steward answers with the RIGHT bytes the WRONG way: an ordinary send,
    // which is exactly what it would have to do if it ever answered outside the
    // delivery that asked — and exactly what an impersonator can produce.
    r.mount_steward(Letters::Answer::UnattestedLetter);
    r.letters.items.push_back(loom::bequeath_item(handoff));

    r.activate();
    r.run_wakes(4);

    // The steward really was asked, and really did reply.
    CHECK(r.letters.claims == 1);
    CHECK(r.first(loom::Bequest::zen_name) >= 0);
    // And the heir inherited nothing, falling back through its bounded path.
    CHECK(r.count("inherited") == 0);
    CHECK(r.count("active") == 0);
    CHECK(r.heard.ready == 1);
}

TEST_CASE("hostile: a forged ACTIVATION cannot give a Timer a first breath, and a genuine "
          "attestation for the wrong sequence cannot either") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Refuse);

    // An ordinary weave, holding only the grant for the public shape, announces
    // a perfectly plausible activation.
    r.forge_activation(1);
    r.run_wakes(4);
    CHECK(r.heard.ready == 0);      // never became available
    CHECK(r.letters.claims == 0);   // never asked for its past
    CHECK_FALSE(claimed_until(r).has_value()); // never claimed a time
    CHECK(r.bus.pending() == 0);    // and left nothing behind

    // A REAL authority, used dishonestly: Loom attests sequence 5 while the
    // payload claims 6. A proof for one activation is not a proof for another.
    r.activate_mismatched(5, 6);
    r.run_wakes(4);
    CHECK(r.heard.ready == 0);
    CHECK(r.letters.claims == 0);

    // THE POSITIVE CONTROL. The same door, the same service, the two halves
    // agreeing — so the silence above is refusal, not a broken rig.
    r.run_wakes(4, /*start=*/true);
    CHECK(r.heard.ready == 1);
    CHECK(r.letters.claims == 1);
}

// ============================================================================
// Tier 6 — THE KEYSTONE: a LIVE Timer crosses a prepared replacement
// ============================================================================
// While a candidate prepares the incumbent keeps serving, so a snapshot at transaction begin is
// stale at commit. The boundary is THE ADMISSION: the Timer's claim on the host's wait rides the
// role, so moving it ends the incumbent's wakes, and admission seals the incumbent at once.

namespace {

/// The keystone's correlation, so a reader can tell the ceremony's letter apart
/// from the suite's ruler reads on the wire.
constexpr std::uint64_t kFinalizeCorrelation = 0x3C0DE;

/// Everything the coordinator has done and been told, in one place the test can
/// read without reaching inside a weave.
struct Prep {
    loom::Switchboard* bus = nullptr;
    loom::WeaveId incumbent{};
    loom::TxnId txn{};
    std::int64_t sequence = 7; ///< the activation sequence the admission carries
    bool commit_on_ready = true;
    bool finalize = true; ///< ask the retired incumbent for its letter
    /// FIXTURE CONTROL, in the tradition of `storage::DoForge`: an honest
    /// incumbent cannot write a letter the gate refuses, so a suite that wants
    /// to prove the candidate refuses one has to manufacture it. The coordinator
    /// is trusted infrastructure and this is what a BUGGY or HOSTILE one would
    /// hand over — authentically, through the real answer door.
    enum class Corrupt { None, OverBound, MalformedRequester };
    Corrupt corrupt = Corrupt::None;

    std::int64_t prepared = 0; ///< preparation answers consumed as Ready
    std::int64_t declined = 0; ///< ...and as Refused
    std::int64_t claims = 0;   ///< prepared claims deferred
    std::int64_t relayed = 0;  ///< letters spent into a prepared claim
    std::string decline_reason;
    std::vector<loom::TxnResult> readiness;
    std::vector<loom::TxnResult> commits;
};

struct PreparerState {
    std::int64_t acted = 0;
    ZEN_SHAPE(PreparerState, 1, ZEN_FIELD(acted));
};

/// THE COORDINATOR -- trusted host-tier infrastructure like the Weave Manager, and the only party
/// in the keystone holding a Switchboard. CREDULOUS about payloads on purpose: what authorizes a
/// transition is read by the bus from the delivery, never a transaction id off a message. It
/// consumes the candidate's preparation answer, admits INSIDE THAT SAME DELIVERY, asks the
/// retired incumbent for its letter, and hands it to the candidate as the authenticated answer
/// to the candidate's OWN claim, which it therefore defers.
class Preparer
    : public loom::WeaveBase<Preparer, PreparerState,
                             loom::Accept<TimerCandidatePrepared, TimerCandidateDeclined,
                                          loom::ClaimBequest, loom::Bequest>,
                             loom::Emit<PrepareTimerHandover, loom::PrepareShutdown,
                                        loom::Bequest>> {
public:
    Preparer(Prep& p, loom::LifecycleAuthority authority) : p_(&p), authority_(authority) {}

    void on(const TimerCandidatePrepared&, loom::Mail& mail) {
        ++state_.acted;
        ++p_->prepared;
        p_->readiness.push_back(
            p_->bus->accept_preparation_answer(p_->txn, loom::PreparationAnswer::Ready));
        if (!p_->commit_on_ready) {
            return;
        }
        admit(mail);
    }

    void on(const TimerCandidateDeclined& d, loom::Mail&) {
        ++state_.acted;
        ++p_->declined;
        p_->decline_reason = d.reason;
        p_->readiness.push_back(
            p_->bus->accept_preparation_answer(p_->txn, loom::PreparationAnswer::Refused));
    }

    /// The prepared candidate's claim, asked of THIS weave by id because it read
    /// its preparer off the ask. The answer is not available yet — the incumbent
    /// has not written it — so the right is taken away and spent later.
    void on(const loom::ClaimBequest&, loom::Mail& mail) {
        ++state_.acted;
        ++p_->claims;
        claim_ = mail.defer_answer();
    }

    /// The retired incumbent's letter. It becomes the candidate's inheritance by
    /// being spent into the claim it already made — so what the candidate finally
    /// sees is an authenticated answer to its own ask, through exactly the code
    /// a graceful succession already uses.
    void on(const loom::Bequest& letter, loom::Mail& mail) {
        ++state_.acted;
        if (!(mail.sender() == p_->incumbent) || !claim_.valid()) {
            return;
        }
        if (loom::answer_deferred(claim_, mail, doctored(letter)).valid()) {
            ++p_->relayed;
        }
        claim_ = loom::DeferredAnswer{};
    }

    /// Commit, and then — and only then — ask the retired incumbent to describe
    /// itself. Both inside one delivery.
    void admit(loom::Mail& mail) {
        loom::Activated fact{p_->sequence};
        p_->commits.push_back(p_->bus->commit_prepared_replacement(
            p_->txn, authority_, loom::Message(loom::to_value(fact)), p_->sequence));
        if (!p_->commits.back().ok || !p_->finalize) {
            return;
        }
        // THE ONLY THING EVER SAID TO THE INCUMBENT, and it is said after the
        // role has already left it: describe yourself. It is the same ask a
        // graceful ceremony makes, answered by the same unchanged handler.
        mail.send(p_->incumbent, loom::PrepareShutdown{}, kFinalizeCorrelation);
    }

private:
    /// The letter as the coordinator chooses to hand it over. Untouched unless
    /// the fixture is playing the buggy preparer.
    loom::Bequest doctored(const loom::Bequest& letter) const {
        if (p_->corrupt == Prep::Corrupt::None) {
            return letter;
        }
        TimerHandoff bad;
        const std::size_t n = p_->corrupt == Prep::Corrupt::OverBound
                                  ? kMaxHandoffEntries + 1
                                  : std::size_t{2};
        for (std::size_t i = 0; i < n; ++i) {
            bad.entries.push_back(TimerHandoffEntry{"1", "doctored." + std::to_string(i), "", 1000,
                                                    true, 500});
        }
        if (p_->corrupt == Prep::Corrupt::MalformedRequester) {
            // ONE entry whose requester is not a lossless decimal weave id. An
            // honest predecessor cannot produce this, so the whole letter is
            // untrusted input rather than a large truth.
            bad.entries.back().requester = "0x2a";
        }
        loom::Bequest out;
        out.role = kTimerRole;
        out.items.push_back(loom::bequeath_item(bad));
        return out;
    }

    Prep* p_;
    loom::LifecycleAuthority authority_;
    loom::DeferredAnswer claim_{};
};

/// The keystone rig: the tier-5 continuity lane plus a coordinator, a Kernel-sealed
/// candidate, and the transaction that binds them.
struct Keystone {
    Rig r;
    Prep prep;
    loom::WeaveId coordinator{};
    loom::WeaveId incumbent{};
    loom::WeaveId probe{};
    std::int64_t anchored = -1; ///< the host's clock when the probe's one-shot was anchored

    /// `timer_path` is the incumbent artifact — the suites' build for the proofs, the SHIPPED
    /// one for the pilot. `probe` is loaded where a five-second one-shot is the subject.
    explicit Keystone(const char* timer_path = TIMER_VIRTUAL_SO, bool with_probe = true) {
        prep.bus = &r.bus;
        loom::Grant reach;
        reach.allow_to_any(PrepareTimerHandover::zen_name, PrepareTimerHandover::zen_version);
        reach.allow_to_any(loom::PrepareShutdown::zen_name, loom::PrepareShutdown::zen_version);
        reach.allow_to_any(loom::Bequest::zen_name, loom::Bequest::zen_version);
        // DELIBERATELY NOT `zen.Activated`: withholding it is the assertion. An admission that
        // sent the activation as an ordinary send stamped with the coordinator would commit --
        // role moved, candidate unsealed, `commit_prepared_replacement` ok -- and then have the
        // activation refused at delivery as `CapabilityDenied`: publicly the service, never told
        // it is alive. A committed activation is Loom's own act, under the lifecycle authority
        // this coordinator holds, so the keystone runs without the ordinary grant.
        coordinator = loom::mount_granted<Preparer>(r.bus, std::move(reach), prep,
                                                    loom::host_lifecycle_authority(r.bus));
        incumbent = r.load("zengine-timer-v1", timer_path, kTimerRole);
        prep.incumbent = incumbent;
        if (with_probe) {
            // The consumer: one requester-addressed five-second one-shot whose preservation is
            // REQUIRED -- no fallback will do.
            probe = r.load("zengine-probe-required", PROBE_REQUIRED_SO, "");
        }
    }

    /// Load a candidate through the Kernel and seal it to the coordinator. Every
    /// artifact-level refusal an ordinary load can produce happens here, before
    /// the live world has been touched at all.
    loom::WeaveId seal(const char* name, const char* path) {
        const loom::LoadResult lr = r.kernel.load_candidate(name, path, coordinator);
        REQUIRE_MESSAGE(lr.ok, "candidate load refused: ", lr.error);
        return lr.id;
    }

    loom::TxnResult begin(loom::WeaveId candidate, std::uint32_t budget = 16) {
        const loom::TxnResult t = r.bus.begin_prepared_replacement(
            r.witness, coordinator, incumbent, candidate, kTimerRole, budget);
        prep.txn = t.id;
        return t;
    }

    /// Enqueue the preparation ask. Nothing is pumped: the caller decides where
    /// the boundary falls by deciding when to resume.
    void ask(const char* continuity) {
        PrepareTimerHandover p;
        p.transaction = static_cast<std::int64_t>(prep.txn.value);
        p.continuity = continuity;
        REQUIRE(r.bus.ask_candidate_to_prepare(prep.txn, loom::Message(loom::to_value(p))).ok);
    }

    /// A weave's own state, read synchronously off the host call every weave
    /// answers — the only door that still works on a SEALED weave, which a
    /// retired incumbent is.
    std::int64_t field(loom::WeaveId who, const char* name) const {
        const loom::Weave* w = r.bus.weave(who);
        REQUIRE(w != nullptr);
        const loom::Value v = w->snapshot();
        const loom::Cell* c = v.get(name);
        REQUIRE(c != nullptr);
        return c->as_int();
    }

    /// THE RULER: how much of `id` is left on the live Timer, right now, read by
    /// asking it to describe itself. Changes nothing it describes.
    ///
    /// Addressed to the ROLE, deliberately, so the same call is the right one on
    /// both sides of a replacement: before the boundary it reaches the incumbent,
    /// after it the admitted candidate, and the caller never has to know which.
    std::int64_t remaining_now(const char* id) {
        const std::size_t before = r.letters.size();
        r.ask_role(loom::PrepareShutdown{});
        r.pump_until(loom::Bequest::zen_name);
        REQUIRE(r.letters.size() > before);
        for (const TimerHandoffEntry& e : Rig::handoff_of(r.letters.back())) {
            if (e.id == id) {
                return e.remaining_ms;
            }
        }
        return -1;
    }

    /// What is left of the probe's one-shot RIGHT NOW — the ruler's read. It costs no time.
    std::int64_t probe_remaining() { return remaining_now(zengine::probe::kProbeTimerId); }

    /// Run the host's loop until exactly `want` milliseconds of the probe's one-shot are left,
    /// the live Timer handed that time -- a one-shot of the witness's own, due then, wakes it --
    /// so a letter written now measures from it; then the ruler says so, measured, not assumed.
    void advance_until_remaining(std::int64_t want) {
        REQUIRE(anchored >= 0);
        const std::int64_t at = anchored + 5000 - want;
        REQUIRE(at >= r.clock.now_ms());
        if (at > r.clock.now_ms()) {
            r.ask_role(StartTimer{"suite.handed", at - r.clock.now_ms(), false});
        }
        r.run_until(at);
        REQUIRE(probe_remaining() == want);
    }

    /// The last letter that crossed the wire, decoded through the real gate. It REQUIREs one:
    /// what it returns is indexed and iterated, and a mutation that produces NO letter must stop
    /// the case, not detonate an unguarded `.back()` and take every later verdict in the run
    /// with it -- a mutation run once did exactly that.
    std::vector<TimerHandoffEntry> last_handoff() {
        REQUIRE_FALSE(r.letters.empty());
        return Rig::handoff_of(r.letters.back());
    }

    /// The one entry of the last letter that names `id`.
    const TimerHandoffEntry* crossed(const std::vector<TimerHandoffEntry>& entries,
                                     const char* id) const {
        for (const TimerHandoffEntry& e : entries) {
            if (e.id == id) {
                return &e;
            }
        }
        return nullptr;
    }
};

/// Establish the probe's five-second one-shot at a known instant, as the probe's
/// own binding would if preservation were available.
void anchor_required_probe(Keystone& k) {
    k.r.ask_as(k.probe, k.incumbent,
               EnsureTimer{zengine::probe::kProbeTimerId, 5000, false, kRestartDelay, ""});
    k.r.pump_until(EnsureTimer::zen_name);
    k.anchored = k.r.clock.now_ms();
}

} // namespace

TEST_CASE("keystone: the live Timer keeps serving while a sealed candidate prepares, and the "
          "candidate crosses the boundary with the progress that existed AT it") {
    Keystone k;

    // 1-4. The consumer's own binding requires preservation, so on an initial
    // load its order is REFUSED — there is nothing to preserve and no honest way
    // to pretend otherwise. The suite then anchors the five-second one-shot as
    // the binding would have, and THAT receipt is `restarted_delay`.
    zengine::probe::ProbeReport rep = k.r.ask_probe(k.probe);
    CHECK(rep.resolved == kResolutionRefused);
    CHECK(rep.activations == 1); // its own activation hook ran, once
    anchor_required_probe(k);
    rep = k.r.ask_probe(k.probe);
    CHECK(rep.resolved == kResolutionRestarted);
    CHECK(rep.lifecycle == "waiting");
    CHECK(rep.fires == 0);

    // 5-6. Two and a half seconds of the host's clock, and the one-shot has not fired.
    k.advance_until_remaining(2500);
    CHECK(k.r.ask_probe(k.probe).fires == 0);

    // 7-8. A sealed candidate, a transaction naming all four participants, and
    // all of the fallible preflight: the artifact loads, its contracts validate,
    // its clock exists, and it reserves the bounded capacity a full letter could
    // ever need.
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    CHECK(k.r.bus.sealed(candidate));
    CHECK(k.r.bus.candidate_owner(candidate).who == k.coordinator);
    REQUIRE(k.begin(candidate).ok);

    // 9-10. PRODUCTION TOPOLOGY IS UNTOUCHED, and the incumbent is still serving:
    // an order addressed to the ROLE is answered by it, right now, mid-preflight.
    CHECK(k.r.bus.role_holder(kTimerRole) == k.incumbent);
    const std::size_t receipts_before = k.r.seen.receipts.size();
    k.r.ask_role(EnsureTimer{"preflight.control", 60000, false, kRestartDelay, ""});
    k.r.pump_until(TimerResolution::zen_name);
    REQUIRE(k.r.seen.receipts.size() == receipts_before + 1); // guards the index below
    CHECK(k.r.seen.receipts.back().id == "preflight.control");
    CHECK(k.r.seen.receipts.back().resolved == kResolutionRestarted);
    CHECK(k.field(k.incumbent, "active") == 2); // the probe's, and the control
    CHECK(k.field(candidate, "wakes") == 0);    // the candidate has no time at all
    CHECK(k.field(candidate, "active") == 0);

    // 11-14. THE BOUNDARY. Run to exactly two seconds left, then let it run: the ask goes
    // through the coordinator-only door, the candidate answers for itself, and the coordinator
    // admits it inside that same delivery.
    k.advance_until_remaining(2000);
    const std::int64_t wakes_before = k.field(k.incumbent, "wakes");
    const std::int64_t boundary = k.r.clock.now_ms();
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerReady::zen_name);
    k.r.pump_until(TimerResolution::zen_name); // ...and the consumer's re-ask answered
    CHECK(k.prep.prepared == 1);
    REQUIRE(k.prep.readiness.size() == 1);
    CHECK(k.prep.readiness.back().ok);
    REQUIRE(k.prep.commits.size() == 1);
    CHECK(k.prep.commits.back().ok); // the commit call SCHEDULED the admission...
    // ...AND THE REAL COMMITTED OUTCOME IS THE ONE THAT COUNTS. The
    // coordinator's `ok` says the admission is on its way; what says it happened
    // is the transaction's own terminal result, written inside the dispatch that
    // moved the role and told the successor. The operator collects it once.
    loom::TxnOutcome outcome{};
    REQUIRE(k.r.bus.take_outcome(k.r.witness, outcome));
    CHECK(outcome.state == loom::TxnState::Committed);
    CHECK(outcome.reason == loom::TxnReason::None);
    CHECK_FALSE(k.r.bus.take_outcome(k.r.witness, outcome)); // consumed
    CHECK(k.r.bus.active_transactions() == 0);
    // The ceremony spent no time, and the incumbent was woken by nothing while it ran.
    CHECK(k.r.clock.now_ms() == boundary);
    CHECK(k.field(k.incumbent, "wakes") == wakes_before);

    // 15-17. The role moved; the incumbent is retirement-private, not merely
    // renamed; the candidate is the service, in the Kernel's books as well.
    CHECK(k.r.bus.role_holder(kTimerRole) == candidate);
    CHECK(k.r.bus.sealed(k.incumbent));
    CHECK_FALSE(k.r.bus.sealed(candidate));
    CHECK(k.r.kernel.roles_of("zengine-timer-v2") == std::vector<std::string>{kTimerRole});
    CHECK(k.r.kernel.roles_of("zengine-timer-v1").empty());

    // EXACTLY TWO SECONDS REMAINED AT THE BOUNDARY, read off the letter the retired incumbent
    // wrote, decoded through the real gate. It is also a positive control: the successor's claim
    // is ordinary domain speech sent from inside a COMMITTED ADMISSION ACTIVATION, so an
    // activation that grants no answer authority still leaves a weave able to speak -- no claim,
    // no letter, no continuity. (That it is not ANSWERABLE is Loom's law, pinned by Loom's suite.)
    CHECK(k.prep.claims == 1); // the candidate asked its PREPARER, not the steward
    REQUIRE(k.prep.relayed == 1);
    const std::vector<TimerHandoffEntry> crossed = k.last_handoff();
    REQUIRE(crossed.size() == 2);
    const TimerHandoffEntry* one_shot = k.crossed(crossed, zengine::probe::kProbeTimerId);
    REQUIRE(one_shot != nullptr);
    CHECK(one_shot->remaining_ms == 2000);
    CHECK(one_shot->delay_ms == 5000); // the intent travels beside the progress
    CHECK(one_shot->requester == std::to_string(k.probe.value));

    // 18-20. The candidate restored before it announced anything, and published
    // exactly one availability notice.
    CHECK(k.field(candidate, "inherited") == 2);
    CHECK(k.field(candidate, "active") == 2);
    CHECK(k.r.seen.ready == 2); // one for the incumbent's load, one for this
    CHECK(k.field(k.incumbent, "inherited") == 0);

    // 21-23. THE CONSUMER'S RECEIPT. It required preservation, and it got it —
    // and its own domain activation hook did NOT run a second time.
    rep = k.r.ask_probe(k.probe);
    CHECK(rep.resolved == kResolutionPreserved);
    CHECK(rep.reason.find("kept its remaining time") != std::string::npos);
    CHECK(rep.fires == 0);
    CHECK(rep.lifecycle == "waiting");
    CHECK(rep.activations == 1);

    // 25-28. AND THE CLOCK DID NOT RESTART: one millisecond short of the remaining time nothing
    // fires; the next millisecond fires it exactly once -- not five seconds from the commit, as a
    // re-anchored schedule or a stale snapshot would. The ruler now reads the CANDIDATE (it is
    // addressed to the role), itself the proof the inherited schedule is the one it holds.
    k.advance_until_remaining(1);
    CHECK(k.r.ask_probe(k.probe).fires == 0);
    k.r.run_for(1);
    rep = k.r.ask_probe(k.probe);
    CHECK(rep.fires == 1);
    CHECK(rep.lifecycle == "spent");

    // ...and only once. A full five seconds later there is still one firing.
    k.r.run_for(5000);
    CHECK(k.r.ask_probe(k.probe).fires == 1);
}

TEST_CASE("keystone: the incumbent moved DURING preparation, and what crossed was where it "
          "actually was — one second, not the stale two") {
    Keystone k;
    anchor_required_probe(k);

    // Three seconds gone: two remain, and a design that snapshotted at
    // transaction begin would carry exactly this number to the end.
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);

    // ONE MORE FULL SECOND, with the candidate sealed and the transaction live.
    // The transaction is stepped through part of its budget while it passes, so
    // this is a preparation genuinely in progress rather than a paused one — and
    // the incumbent's OWN wakes say the second really reached it.
    const std::int64_t wakes_at_begin = k.field(k.incumbent, "wakes");
    REQUIRE(k.r.bus.tick_preparation(k.prep.txn).ok);
    k.advance_until_remaining(1000);
    REQUIRE(k.r.bus.tick_preparation(k.prep.txn).ok);
    CHECK(k.r.bus.transaction_state(k.prep.txn) == loom::TxnState::Preparing);
    CHECK(k.field(k.incumbent, "wakes") > wakes_at_begin);

    // ...and THE INCUMBENT WAS THE ONE SERVING THROUGH IT. Not "the role still
    // pointed at it" — it answered.
    CHECK(k.r.bus.role_holder(kTimerRole) == k.incumbent);
    CHECK(k.field(candidate, "wakes") == 0);

    // Only now the boundary.
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerReady::zen_name);
    k.r.pump_until(TimerResolution::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);
    REQUIRE(k.prep.relayed == 1);

    // ONE SECOND. The letter carries where the clock actually was at the
    // replacement boundary, not where it was when the transaction began.
    const std::vector<TimerHandoffEntry> crossed = k.last_handoff();
    const TimerHandoffEntry* one_shot = k.crossed(crossed, zengine::probe::kProbeTimerId);
    REQUIRE(one_shot != nullptr);
    CHECK(one_shot->remaining_ms == 1000);
    CHECK(k.field(candidate, "inherited") == 1);
    CHECK(k.r.ask_probe(k.probe).resolved == kResolutionPreserved);

    // And it fires after THAT second — not after the two the earlier snapshot
    // would have carried, which is a full second of daylight between the honest
    // answer and the plausible one.
    k.advance_until_remaining(1);
    CHECK(k.r.ask_probe(k.probe).fires == 0);
    k.r.run_for(1);
    CHECK(k.r.ask_probe(k.probe).fires == 1);
}

TEST_CASE("keystone: the queue boundary — an operation ordered before the admission is in the "
          "letter, one ordered after it is held and replayed, and neither happens twice") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);

    // A AND B ARE THE SAME TIMER, ordered differently. Same id, different
    // declared delay, so the final table says which one landed LAST — a proof
    // that ordering survived, which two unrelated ids could never give.
    // Role-addressed, because that is how every production consumer reaches the
    // Timer and therefore the only addressing the package promises about.
    const std::size_t receipts_before = k.r.seen.receipts.size();
    k.r.ask_role(EnsureTimer{"boundary", 60000, false, kRestartDelay, ""});
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(PrepareTimerHandover::zen_name); // the candidate has answered; nobody has committed

    // B is queued HERE: after the candidate said it was ready, and before the
    // coordinator's next delivery — which is the one that admits it.
    k.r.ask_role(EnsureTimer{"boundary", 70000, false, kRestartDelay, ""});
    k.r.pump_until(TimerReady::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);
    REQUIRE(k.prep.relayed == 1);

    // A IS IN THE CAPTURED INCUMBENT STATE, and B is not: the letter describes
    // the boundary, and B was ordered after it.
    const std::vector<TimerHandoffEntry> crossed = k.last_handoff();
    const TimerHandoffEntry* a = k.crossed(crossed, "boundary");
    REQUIRE(a != nullptr);
    CHECK(a->delay_ms == 60000);
    CHECK(k.crossed(crossed, zengine::probe::kProbeTimerId) != nullptr);
    CHECK(crossed.size() == 2); // exactly the two that existed at the boundary

    // B WAS NOT LOST. It reached the admitted candidate, was HELD through the
    // restoration, and was replayed after it — so the standing schedule is B's,
    // over the top of the A the letter carried. Nothing was dropped to buy
    // atomicity, and nothing applied twice.
    k.r.pump_until(TimerResolution::zen_name);
    CHECK(k.remaining_now("boundary") > 0); // a ruler read against the CANDIDATE now
    const std::vector<TimerHandoffEntry> live = k.last_handoff();
    const TimerHandoffEntry* b = k.crossed(live, "boundary");
    REQUIRE(b != nullptr);
    CHECK(b->delay_ms == 70000); // B landed after A, and last writer won
    CHECK(k.field(candidate, "active") == 2);
    CHECK(k.field(candidate, "inherited") == 2);

    // EXACTLY ONE RECEIPT EACH, in order: A answered by the incumbent, B by the
    // candidate. Two operations, two receipts, no repeats and no silence.
    std::vector<std::string> boundary_receipts;
    for (std::size_t i = receipts_before; i < k.r.seen.receipts.size(); ++i) {
        if (k.r.seen.receipts[i].id == "boundary") {
            boundary_receipts.push_back(k.r.seen.receipts[i].resolved);
        }
    }
    // REQUIRE, not CHECK: the two lines below index it, and an assertion that
    // guards an index has to stop the case rather than merely note it — a bare
    // CHECK here would let a mutation that empties the list crash the run and
    // take every later verdict with it.
    REQUIRE(boundary_receipts.size() == 2);
    CHECK(boundary_receipts[0] == kResolutionRestarted);
    CHECK(boundary_receipts[1] == kResolutionRestarted);

    // ...and the probe's own schedule crossed untouched by any of it.
    CHECK(k.r.ask_probe(k.probe).resolved == kResolutionPreserved);
}

namespace {

/// NOTHING ABOUT THE INCUMBENT MOVED -- every failure proof in this lane, asked of the bus and
/// the service rather than of any object a failing case might have destroyed. The last check
/// matters most and is easiest to omit: the one-shot is not merely listed, it still fires WHERE
/// IT WAS GOING TO, once, at the boundary the incumbent has been counting toward.
void incumbent_never_stopped_being_the_timer(Keystone& k, loom::WeaveId candidate) {
    CHECK(k.r.bus.alive(k.incumbent));
    CHECK_FALSE(k.r.bus.sealed(k.incumbent));
    CHECK(k.r.bus.role_holder(kTimerRole) == k.incumbent);
    CHECK_FALSE(k.r.bus.role_holder(kTimerRole) == candidate);
    CHECK(k.field(k.incumbent, "active") == 1); // the probe's one-shot, still standing
    CHECK(k.field(k.incumbent, "inherited") == 0);
    CHECK(k.r.seen.ready == 1); // no candidate ever announced itself
    CHECK(k.r.ask_probe(k.probe).fires == 0);

    // ONE TIMER KEEPING TIME, BY ITS CLAIM: the bus holds nothing for it, and the wait holds the
    // incumbent's claim of the probe's deadline.
    k.r.settle();
    CHECK(k.r.bus.pending() == 0);
    bool claimed = false;
    for (const loom::host::Reactor::Source& s : k.r.wait.sources()) {
        if (s.role == kTimerRole) {
            claimed = s.until_ms == std::optional<std::int64_t>(k.anchored + 5000);
        }
    }
    CHECK(claimed);

    // ...and the schedule was not reset: it runs out where it always would have,
    // and fires exactly once.
    k.advance_until_remaining(1);
    CHECK(k.r.ask_probe(k.probe).fires == 0);
    k.r.run_for(1);
    CHECK(k.r.ask_probe(k.probe).fires == 1);
    k.r.run_for(2000);
    CHECK(k.r.ask_probe(k.probe).fires == 1); // and only once
}

} // namespace

TEST_CASE("keystone: every way a candidate can fail leaves the incumbent's clock exactly where "
          "it was — the negative proof of prepared replacement") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);

    enum class Route { Declines, UnknownPlan, Dies, BudgetExhausted, OperatorAborts };
    Route route = Route::Declines;
    const char* artifact = TIMER_VIRTUAL_V2_SO;
    SUBCASE("the candidate is a build that refuses to become the Timer") {
        route = Route::Declines;
        artifact = TIMER_DECLINES_SO;
    }
    SUBCASE("the candidate is asked for a continuity plan it does not understand") {
        route = Route::UnknownPlan;
    }
    SUBCASE("the candidate dies while it is preparing") { route = Route::Dies; }
    SUBCASE("the preparation budget runs out") { route = Route::BudgetExhausted; }
    SUBCASE("the operator changes its mind") { route = Route::OperatorAborts; }

    const loom::WeaveId candidate = k.seal("zengine-timer-v2", artifact);
    REQUIRE(k.begin(candidate, /*budget=*/2).ok);
    const loom::TxnId txn = k.prep.txn;

    switch (route) {
    case Route::Declines:
        k.ask(kInheritFromIncumbent);
        k.r.pump_until(TimerCandidateDeclined::zen_name);
        CHECK(k.prep.declined == 1);
        CHECK(k.prep.decline_reason.find("cannot become the Timer") != std::string::npos);
        break;
    case Route::UnknownPlan:
        k.ask("keep-the-absolute-deadlines");
        k.r.pump_until(TimerCandidateDeclined::zen_name);
        CHECK(k.prep.declined == 1);
        CHECK(k.prep.decline_reason.find("unknown continuity plan") != std::string::npos);
        break;
    case Route::Dies:
        k.ask(kInheritFromIncumbent);
        k.r.pump_until(PrepareTimerHandover::zen_name);
        k.r.bus.kill(candidate); // a lifecycle change invalidates the transaction
        break;
    case Route::BudgetExhausted:
        // A budget of two spends one step and ends on the next: the step that
        // takes it to zero IS the exhaustion, not the one after.
        REQUIRE(k.r.bus.tick_preparation(txn).ok);
        CHECK_FALSE(k.r.bus.tick_preparation(txn).ok);
        break;
    case Route::OperatorAborts:
        REQUIRE(k.r.bus.abort_prepared_replacement(txn, k.r.witness).ok);
        break;
    }

    // THE TRANSACTION ENDED ONCE, and its one result is collectable by the exact
    // operator exactly once.
    CHECK_FALSE(k.r.bus.transaction_active(txn));
    CHECK(k.r.bus.active_transactions() == 0);
    loom::TxnOutcome out;
    REQUIRE(k.r.bus.take_outcome(k.r.witness, out));
    CHECK(out.id == txn);
    CHECK(out.state == loom::TxnState::Aborted);
    CHECK_FALSE(k.r.bus.take_outcome(k.r.witness, out));

    // The candidate never became production, and no commit was even attempted.
    CHECK(k.prep.commits.empty());
    CHECK_FALSE(k.r.bus.role_holder(kTimerRole) == candidate);
    // ...and the Kernel's books followed the discard without anyone calling it.
    CHECK_FALSE(k.r.kernel.is_loaded("zengine-timer-v2"));

    incumbent_never_stopped_being_the_timer(k, candidate);
}

TEST_CASE("keystone: a candidate artifact that cannot load never becomes a candidate, and the "
          "live world is not touched to find that out") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);

    const loom::LoadResult bad =
        k.r.kernel.load_candidate("zengine-timer-v2", "/nonexistent/not-a-timer.so", k.coordinator);
    CHECK_FALSE(bad.ok);
    CHECK_FALSE(bad.error.empty());
    CHECK_FALSE(k.r.kernel.is_loaded("zengine-timer-v2"));
    CHECK(k.r.bus.active_transactions() == 0);

    incumbent_never_stopped_being_the_timer(k, loom::WeaveId{});
}

TEST_CASE("keystone: an admission that refuses AT DISPATCH leaves the incumbent Timer's chain "
          "and schedule exactly as they were") {
    // A FAILED ADMISSION AFTER IT WAS SCHEDULED: every other failure route here ends the
    // transaction BEFORE an admission is scheduled, so "a failed admission disturbs the
    // incumbent" had no road until a mutation showed it; it has one because scheduling and
    // dispatch are two moments, and the world may change between them.
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);
    k.ask(kInheritFromIncumbent);

    // Stop the instant the coordinator has consumed the readiness and SCHEDULED
    // the admission. Nothing has moved: the incumbent still holds the role.
    k.r.pump_until(TimerCandidatePrepared::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);
    CHECK(k.r.bus.transaction_state(k.prep.txn) == loom::TxnState::AdmissionPending);
    CHECK(k.r.bus.role_holder(kTimerRole) == k.incumbent);
    CHECK_FALSE(k.r.bus.sealed(k.incumbent));

    // Then the successor dies before it can be admitted.
    k.r.bus.kill(candidate);
    k.r.settle();

    // The admission refused; the transaction ended once, naming what happened;
    // no letter was ever written, because nothing ever froze the incumbent.
    loom::TxnOutcome out{};
    REQUIRE(k.r.bus.take_outcome(k.r.witness, out));
    CHECK(out.state == loom::TxnState::Aborted);
    CHECK(out.reason == loom::TxnReason::CandidateChanged);
    CHECK(k.prep.relayed == 0);
    CHECK(k.r.bus.active_transactions() == 0);

    // ...and the Timer that was serving is exactly the Timer that is serving:
    // same role, one claim, and a schedule that runs out where it always would.
    incumbent_never_stopped_being_the_timer(k, candidate);
}

TEST_CASE("keystone: the boundary is a POSITION IN THE QUEUE — a by-id operation ahead of it "
          "reaches the incumbent and crosses in the letter; one behind it is refused visibly") {
    // WHY THE MODEL IS COHERENT: an admission that jumped the queue would move topology inside
    // the commit call, judging a message enqueued while the incumbent was public against a later
    // world, and refusing a by-id operation queued BEFORE the boundary as `NoSuchTarget`. The
    // admission holds a place in the queue -- ahead of it the old world, behind it the new -- so
    // an operation the incumbent accepts before the boundary is in its table when the letter is
    // written, and CROSSES. Both sides of the boundary are truthful, and neither is silent.
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);

    std::vector<loom::RefusalReason> refusals;
    k.r.bus.add_observer([&refusals](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused &&
            e.schema_name == std::string(EnsureTimer::zen_name)) {
            refusals.push_back(e.refusal.reason);
        }
    });

    k.ask(kInheritFromIncumbent);
    k.r.pump_until(PrepareTimerHandover::zen_name);
    // AHEAD of the admission: queued while the incumbent is still the public
    // service, and delivered into that world.
    k.r.ask(k.incumbent, EnsureTimer{"by.id", 60000, false, kRestartDelay, ""});
    k.r.pump_until(TimerReady::zen_name);
    CHECK(refusals.empty()); // it was accepted, not dropped and not refused

    // BEHIND the admission: the incumbent is sealed for retirement now, and a
    // direct approach is refused exactly as the seal refuses — the world may not
    // learn that a retiring service is still there. The consumer sees a refusal
    // it can act on; what it does NOT see is a schedule it never got.
    k.r.ask(k.incumbent, EnsureTimer{"by.id.after", 60000, false, kRestartDelay, ""});
    k.r.pump_until(TimerResolution::zen_name);
    REQUIRE(refusals.size() == 1);
    CHECK(refusals[0] == loom::RefusalReason::NoSuchTarget);

    // ...and the one that got through crossed in the letter: the successor holds
    // the probe's schedule AND the by-id one, and the retired incumbent's table
    // is the frozen record of what it handed over.
    CHECK(k.field(candidate, "active") == 2);
    CHECK(k.field(k.incumbent, "active") == 2);
}

TEST_CASE("keystone: on commit the incumbent is woken no more — its claim rides the role — and "
          "the successor alone keeps the time") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerReady::zen_name);
    k.r.pump_until(TimerResolution::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);

    // NOTHING WAKES THE OLD SERVICE. Its wakes and its firings are frozen at the boundary and
    // stay frozen past the probe's deadline -- because the host's wait reads the claim of the
    // role, which is the successor's now, and the successor alone fires the probe.
    const std::int64_t wakes = k.field(k.incumbent, "wakes");
    const std::int64_t fired = k.field(k.incumbent, "fired");
    const std::int64_t candidate_wakes = k.field(candidate, "wakes");
    k.r.run_until(k.anchored + 5000 + 1000);
    CHECK(k.field(k.incumbent, "wakes") == wakes);
    CHECK(k.field(k.incumbent, "fired") == fired);
    CHECK(k.field(candidate, "wakes") > candidate_wakes);
    CHECK(k.field(candidate, "fired") == 1);

    // ...and the retired service's table is frozen with it, so the letter it wrote described
    // the boundary and not some later instant.
    CHECK(k.field(k.incumbent, "active") == 1);

    // And nothing is queued to keep anyone's time.
    CHECK(k.r.bus.pending() == 0);
}

TEST_CASE("keystone: a prepared candidate told to start FRESH waits for no letter, asks nobody, "
          "and refuses a required preservation instead of inventing one") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);

    // The same door, the other plan. Prepared restoration is DECLARED, so a
    // candidate that was not told to inherit does not — no matter what arrives.
    k.ask(kStartFresh);
    k.r.pump_until(TimerReady::zen_name);
    k.r.pump_until(TimerResolution::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);
    CHECK(k.r.bus.role_holder(kTimerRole) == candidate);

    // IT ASKED NOBODY — not its preparer, and not the steward.
    CHECK(k.prep.claims == 0);
    CHECK(k.prep.relayed == 0);
    CHECK(k.field(candidate, "inherited") == 0);

    // The letter the coordinator DID collect from the retired incumbent had
    // nowhere to go, and the candidate would not have taken it: a late letter
    // meets a bootstrap that resolved at activation.
    CHECK(k.field(candidate, "active") == 0);

    // AND THE CONSUMER IS TOLD THE TRUTH. It required preservation, there was
    // none, and the receipt says refused rather than quietly restarting a
    // five-second schedule that had two seconds left.
    const zengine::probe::ProbeReport rep = k.r.ask_probe(k.probe);
    CHECK(rep.resolved == kResolutionRefused);
    CHECK(rep.reason.find("no fallback was acceptable") != std::string::npos);
    CHECK(rep.fires == 0);
    CHECK(rep.activations == 1); // and its domain hook still did not re-run
}

TEST_CASE("keystone: a prepared candidate whose promised letter never arrives starts fresh at a "
          "published bound — alive and honest, never holding forever") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);

    // The coordinator admits it and then never collects the letter. This is the
    // failure the whole two-stage design has to survive: readiness was answered
    // before the state crossed, so a design that could wedge here would have had
    // no business calling itself ready.
    k.prep.finalize = false;
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerReady::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);
    CHECK(k.prep.claims == 1);  // it did ask
    CHECK(k.prep.relayed == 0); // and was never answered

    // IT IS THE TIMER, AND IT WORKS. The bound is a count of its own wakes, and
    // it spent exactly them before deciding the promise was not kept.
    CHECK(k.r.bus.role_holder(kTimerRole) == candidate);
    CHECK(k.field(candidate, "inherited") == 0);
    CHECK(k.field(candidate, "wakes") == kPreparedClaimWakes);
    CHECK(k.r.seen.ready == 2);

    // The consumer is refused rather than silently restarted — the difference
    // between "your two seconds are gone" and "here are five fresh ones".
    k.r.pump_until(TimerResolution::zen_name);
    const zengine::probe::ProbeReport rep = k.r.ask_probe(k.probe);
    CHECK(rep.resolved == kResolutionRefused);
    CHECK(rep.fires == 0);

    // ...and it goes on serving: a new order is taken and kept.
    k.r.ask_role(EnsureTimer{"after.the.gap", 1000, false, kRestartDelay, ""});
    k.r.pump_until(TimerResolution::zen_name);
    REQUIRE_FALSE(k.r.seen.receipts.empty());
    CHECK(k.r.seen.receipts.back().id == "after.the.gap");
    CHECK(k.r.seen.receipts.back().resolved == kResolutionRestarted);
}

TEST_CASE("keystone: a preparation ask reaches a LIVE Timer and is declined, and a second ask to "
          "the same candidate is declined too") {
    Keystone k;
    anchor_required_probe(k);

    // A live service is not a candidate. A stray ask must not be able to
    // re-point its bootstrap, reserve on its behalf, or make it wait for a
    // letter nobody is writing.
    k.r.ask(k.incumbent, PrepareTimerHandover{0, kInheritFromIncumbent});
    k.r.settle();
    REQUIRE(k.r.seen.declines.size() == 1);
    CHECK(k.r.seen.declines[0].find("already live") != std::string::npos);
    CHECK(k.field(k.incumbent, "active") == 1);
    CHECK(k.r.bus.role_holder(kTimerRole) == k.incumbent);

    // ONE ASK, ONE ANSWER. A sealed candidate that has already answered a
    // preparation refuses the next one rather than silently re-preparing.
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerCandidatePrepared::zen_name);
    CHECK(k.prep.prepared == 1);
    // The transaction's one conversation is spent, so the SECOND ask cannot use
    // that door either — the bus refuses it before the candidate is reached.
    CHECK_FALSE(k.r.bus
                    .ask_candidate_to_prepare(
                        k.prep.txn, loom::Message(loom::to_value(PrepareTimerHandover{
                                        0, kInheritFromIncumbent})))
                    .ok);
    // ...and the coordinator's own ordinary send, which the seal DOES deliver,
    // is answered with a decline rather than a second preparation.
    k.r.ask_as(k.coordinator, candidate, PrepareTimerHandover{0, kStartFresh});
    k.r.settle();
    CHECK(k.prep.prepared == 1);
}

TEST_CASE("keystone: a forged letter cannot be inherited by a prepared candidate, cannot end its "
          "wait early, and cannot overwrite what it did inherit") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);

    // The impersonator of the hostile-steward lane above, pointed at the prepared
    // candidate. It
    // knows the published claim correlation, it builds handoff bytes the gate
    // accepts, and it holds an ordinary grant to send `zen.Bequest` to anyone.
    // The one thing it cannot have is Loom's word that it received the claim.
    const loom::WeaveId forger = loom::mount<LetterForger>(k.r.bus, candidate, k.r.witness);

    bool refused = false;
    SUBCASE("a forged letter") { refused = false; }
    SUBCASE("a forged refusal, to end the wait early") { refused = true; }

    k.ask(kInheritFromIncumbent);
    // Stop the moment the candidate's own claim has been delivered: from here to
    // the real answer is precisely the window a forgery would have to win.
    k.r.pump_until(loom::ClaimBequest::zen_name);
    CHECK(k.prep.claims == 1);
    k.r.bus.send(forger, loom::Message(loom::to_value(ForgeLetter{
                             refused, 90000, static_cast<std::int64_t>(kClaimCorrelation)})));
    k.r.pump_until(TimerReady::zen_name);
    k.r.pump_until(TimerResolution::zen_name);

    // THE FORGERY CHANGED NOTHING, and — the half that is easy to miss — it did
    // not close the claim either. The genuine letter still landed.
    REQUIRE(k.prep.relayed == 1);
    CHECK(k.field(candidate, "inherited") == 1);
    CHECK(k.field(candidate, "active") == 1);
    CHECK(k.remaining_now("forged.tick") == -1); // no such schedule exists
    CHECK(k.r.ask_probe(k.probe).resolved == kResolutionPreserved);

    // AND A FORGERY AFTER THE DECISION cannot overwrite what was inherited.
    // Once resolved, always resolved.
    k.r.bus.send(forger, loom::Message(loom::to_value(ForgeLetter{
                             false, 90000, static_cast<std::int64_t>(kClaimCorrelation)})));
    k.r.settle();
    CHECK(k.field(candidate, "inherited") == 1);
    CHECK(k.field(candidate, "active") == 1);
}

TEST_CASE("keystone: an AUTHENTIC letter the gate refuses, or one over the published bound, is "
          "adopted whole or not at all — the candidate starts fresh and says so") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);

    SUBCASE("more entries than the published bound") { k.prep.corrupt = Prep::Corrupt::OverBound; }
    SUBCASE("one entry whose requester is not a lossless weave id") {
        k.prep.corrupt = Prep::Corrupt::MalformedRequester;
    }

    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerReady::zen_name);
    k.r.pump_until(TimerResolution::zen_name);

    // The answer WAS authentic — it went through the real answer door, spent the
    // candidate's own claim, and closed the bootstrap. What it carried was not
    // adoptable, and half of an untrusted letter is worse than none of it.
    REQUIRE(k.prep.relayed == 1);
    CHECK(k.field(candidate, "inherited") == 0);
    CHECK(k.field(candidate, "active") == 0);
    CHECK(k.r.ask_probe(k.probe).resolved == kResolutionRefused);
    CHECK(k.r.ask_probe(k.probe).fires == 0);

    // ...and it is a working Timer regardless.
    k.r.ask_role(EnsureTimer{"after.the.bad.letter", 1000, false, kRestartDelay, ""});
    k.r.pump_until(TimerResolution::zen_name);
    REQUIRE_FALSE(k.r.seen.receipts.empty());
    CHECK(k.r.seen.receipts.back().resolved == kResolutionRestarted);
}

TEST_CASE("keystone: retirement is private and finite — the old artifact unloads exactly once, "
          "and the new service never notices") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerReady::zen_name);
    k.r.pump_until(TimerResolution::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);

    // The Kernel's books agree with the Switchboard's reality without anyone
    // reconciling them: the retiring artifact is SEALED, the new one is LIVE.
    CHECK(k.r.kernel.status("zengine-timer-v1") == loom::ArtifactStatus::Sealed);
    CHECK(k.r.kernel.status("zengine-timer-v2") == loom::ArtifactStatus::Live);
    CHECK(k.r.kernel.roles_of("zengine-timer-v2") == std::vector<std::string>{kTimerRole});

    // RETIREMENT PUBLISHED NOTHING. The only thing the old service said after
    // the boundary was its letter, to its coordinator — no availability notice,
    // no firing, nothing a consumer could mistake for production.
    const std::int64_t ready_before = k.r.seen.ready;
    const std::size_t fired_before = k.r.seen.fired.size();

    // Unload it, and count: one instance destroyed, one library closed.
    const loom::KernelLifetimeCounts before = loom::kernel_lifetime_counts();
    CHECK(k.r.kernel.unload("zengine-timer-v1"));
    const loom::KernelLifetimeCounts after = loom::kernel_lifetime_counts();
    CHECK(after.instances_destroyed - before.instances_destroyed == 1);
    CHECK(after.libraries_closed - before.libraries_closed == 1);
    CHECK(k.r.kernel.status("zengine-timer-v1") == loom::ArtifactStatus::NotLoaded);
    CHECK_FALSE(k.r.kernel.unload("zengine-timer-v1")); // and not a second time

    // THE NEW SERVICE NEVER NOTICED: same role, same chain, same schedule, and
    // the one-shot still fires where the letter said it would.
    CHECK(k.r.bus.role_holder(kTimerRole) == candidate);
    CHECK(k.r.seen.ready == ready_before);
    CHECK(k.r.seen.fired.size() == fired_before);
    k.advance_until_remaining(1);
    CHECK(k.r.ask_probe(k.probe).fires == 0);
    k.r.run_for(1);
    CHECK(k.r.ask_probe(k.probe).fires == 1);
    CHECK(k.r.bus.pending() == 0); // nothing queued to keep time, still
}

// ---- the pilot: the same crossing, by the SHIPPED artifact -----------------------------------
// The proofs above run the suites' build; this one runs the artifact a host ships (`timer.cpp`),
// under the same host's wait, so what crossed is exact here too.

TEST_CASE("keystone pilot: the SHIPPED Timer artifact crosses a prepared replacement, and the "
          "schedule it hands over is exact") {
    Keystone k(TIMER_SO, /*with_probe=*/false);

    // A second real artifact from the same shipped library, sealed and its transaction begun
    // before the schedule exists.
    const loom::WeaveId candidate = k.seal("zengine-timer-real-v2", TIMER_SO);
    REQUIRE(k.begin(candidate).ok);

    // A 400ms one-shot, then half of it spent, the incumbent handed the time at its end.
    const std::size_t receipts_asked = k.r.seen.receipts.size();
    k.r.ask_role(EnsureTimer{"pilot.oneshot", 400, false, kRestartDelay, ""});
    k.r.wait_until([&k, receipts_asked] { return k.r.seen.receipts.size() > receipts_asked; },
                   "the one-shot's receipt");
    CHECK(k.r.seen.receipts.back().id == "pilot.oneshot");
    CHECK(k.r.seen.receipts.back().resolved == kResolutionRestarted);
    const std::int64_t anchored = k.r.clock.now_ms();
    k.r.ask_role(StartTimer{"suite.handed", 200, false});
    k.r.run_until(anchored + 200);

    // The crossing, through the same coordinator. The re-ask preferring preservation is ordered at
    // the boundary -- the candidate has answered, nobody has committed -- so it reaches the
    // admitted candidate, which holds it through the restoration and answers it from what the
    // letter carried before it fires anything.
    const std::size_t receipts_before = k.r.seen.receipts.size();
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(PrepareTimerHandover::zen_name);
    k.r.ask_role(EnsureTimer{"pilot.oneshot", 400, false, kPreserveRemaining, ""});
    k.r.pump_until(TimerReady::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);
    REQUIRE(k.prep.relayed == 1);
    CHECK(k.r.bus.role_holder(kTimerRole) == candidate);

    std::vector<std::string> answered;
    for (std::size_t i = receipts_before; i < k.r.seen.receipts.size(); ++i) {
        if (k.r.seen.receipts[i].id == "pilot.oneshot") {
            answered.push_back(k.r.seen.receipts[i].resolved);
        }
    }
    REQUIRE(answered.size() == 1);
    const auto fired = [&k] {
        return std::count(k.r.seen.fired.begin(), k.r.seen.fired.end(), "pilot.oneshot");
    };

    // WHAT CROSSED IS EXACT: half the 400ms is gone. A restarted schedule would read 400; a
    // snapshot taken at the transaction's begin would not hold the one-shot at all.
    const std::vector<TimerHandoffEntry> crossed = k.last_handoff();
    const TimerHandoffEntry* shot = k.crossed(crossed, "pilot.oneshot");
    REQUIRE(shot != nullptr);
    CHECK(fired() == 0);
    CHECK(shot->delay_ms == 400);
    CHECK(shot->remaining_ms == 200);

    // AND THE NEW SERVICE KEPT IT: the re-ask is answered `preserved_remaining` -- the receipt,
    // not an inference.
    CHECK(answered.front() == kResolutionPreserved);

    // ...and it fires ONCE, at the time that was left, which a schedule re-anchored at the commit
    // would still be waiting out.
    k.r.run_until(anchored + 400 - 1);
    CHECK(fired() == 0);
    k.r.run_until(anchored + 400);
    CHECK(fired() == 1);
    k.r.run_for(1000);
    CHECK(fired() == 1);
}
