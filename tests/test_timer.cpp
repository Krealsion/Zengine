// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

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

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "timer/binding.hpp"
#include "timer/timer_weave.hpp"
#include "timer/vocabulary.hpp"

#include "input/vocabulary.hpp"
#include "probe_vocabulary.hpp"
#include "lifecycle_door.hpp"
#include "snake/vocabulary.hpp"
#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
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
    std::vector<TimerResolution> resolutions; ///< receipts, in arrival order
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
class Ear
    : public loom::WeaveBase<Ear, EarState,
                             loom::Accept<TimerFired, TimerReady, TimerResolution>,
                             loom::Emit<StartTimer, StartRoleTimer, EnsureTimer, EnsureRoleTimer,
                                        CancelTimer, CancelAllMyTimers, Drive>> {
public:
    explicit Ear(Heard& heard) : heard_(&heard) {}
    void on(const TimerResolution& t, loom::Mail&) { heard_->resolutions.push_back(t); }
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

/// A stand-in steward: holds `zen.manager`, answers a claim however the test
/// tells it to, and keeps whatever letter it is handed.
///
/// It exists so the tier-2 pins can drive BOTH ends of the handoff without a
/// kernel — and, more usefully, so they can drive the ends a real steward never
/// would: a silence that never answers, a letter written to the wrong version, a
/// letter carrying more entries than the published bound. Those are the frames
/// an honest predecessor cannot produce, so a test that only used the honest
/// path would be pinning nothing.
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
            // ANSWERED, not merely sent (R2B-1). "There is nothing for you" is
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

/// The tier-2 world: a real bus, the service on a fake clock, one ear, and a
/// stand-in door to activate from.
struct FakeRig {
    loom::Switchboard bus;
    FakeHooks hooks;
    Heard heard;
    Letters letters;
    std::vector<std::string> tape; ///< delivered schema names, in order
    loom::WeaveId service{};
    loom::WeaveId ear{};
    loom::WeaveId door{};
    loom::WeaveId other_door{}; ///< a SECOND lifecycle operator: a different lineage
    loom::WeaveId impostor{};   ///< an ordinary weave that merely knows the shape
    loom::WeaveId steward{};    ///< mounted only where a test wants one
    std::int64_t next_sequence = 1;

    FakeRig() {
        hooks.bus = &bus;
        heard.hooks = &hooks;
        heard.bus = &bus;
        // Into its ROLE — the beat is role-addressed, exactly as the .so's is.
        service = mount_role<FakeService>(bus, kTimerRole, FakeClock{&hooks});
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
                ask_now = hooks.now; // the instant a schedule was anchored
            }
            if (ev.schema_name == loom::PrepareShutdown::zen_name) {
                // The instant the predecessor reads its clock. Nothing naps
                // inside that handler, so this IS the letter's `now`.
                letter_now = hooks.now;
            }
            if (ev.schema_name == "Drive") {
                ++drives;
                if (stop_after_drives >= 0 && drives >= stop_after_drives) {
                    bus.stop();
                }
            }
        });
    }

    std::int64_t drives = 0;
    std::int64_t stop_after_drives = -1;
    std::int64_t letter_now = -1; ///< virtual now when the predecessor was asked to write
    std::int64_t ask_now = -1;    ///< virtual now when a StartTimer was last applied

    /// Bounded by BEATS DELIVERED rather than by naps: a bootstrap beat spends a
    /// queue turn without napping, so a nap-counting bound would not see one.
    void pump_drives(std::int64_t n) {
        stop_after_drives = drives + n;
        bus.pump();
        stop_after_drives = -1;
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

    /// Ask the service to write its letter, as the steward would, and decode it.
    std::vector<TimerHandoffEntry> prepare_shutdown() {
        REQUIRE(steward.valid());
        const std::size_t before = letters.received.size();
        bus.send_as(steward, service,
                    loom::Message(loom::to_value(loom::PrepareShutdown{}), steward, steward, 77));
        pump_drives(4); // the ask, the letter, and the beats they queue behind
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
    /// This — not a wind — is what starts time.
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
        // A HANG GUARD, not a schedule. This bound counts BEATS DELIVERED while
        // the one above counts naps, and the two differ exactly where a chain
        // beats without napping — a bootstrap that never resolves would spin
        // forever under the nap bound alone. A test that fails should fail with
        // an assertion, never by being killed.
        stop_after_drives = drives + n + 8;
        if (start) {
            activate();
        }
        bus.pump();
        hooks.stop_after = -1;
        stop_after_drives = -1;
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
    std::vector<zengine::probe::ProbeReport> reports; ///< the continuity lane's answers
    std::vector<TimerResolution> receipts; ///< the keystone lane's own order receipts
    std::vector<std::string> declines;     ///< preparation asks a Timer refused
    std::int64_t letters_to_me = 0;        ///< clock reads the ruler took
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
                                          zengine::snake::SnakeVisual,
                                          zengine::probe::ProbeReport, loom::Bequest,
                                          TimerResolution, TimerCandidateDeclined>,
                             loom::Emit<>> {
public:
    explicit Witness(Seen& seen) : seen_(&seen) {}
    /// THE RULER (R2B-3c). Asking a Timer to describe itself changes nothing in
    /// its schedule — R2B-0 made that a law of `on(PrepareShutdown)` — so the
    /// suite uses it as an exact clock read: send one, catch the letter, and
    /// know precisely how much of a standing schedule is left at that instant.
    /// The letter has to be ACCEPTED to be seen, because the tap carries a
    /// payload only for a delivery.
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

/// One letter, caught on the wire.
///
/// `zen.Bequest` crosses this bus twice in a graceful succession — predecessor
/// to steward, steward to heir — and a tap sees both, payload included. That is
/// how the continuity lane proves what the predecessor OFFERED without reaching
/// inside either weave: the claim "two seconds remaining" is read off the actual
/// message, decoded through the actual gate.
struct CaughtLetter {
    loom::WeaveId target{};
    loom::WeaveId sender{};
    loom::Value payload;
    std::int64_t drives = 0; ///< beats delivered when it landed — the lane's clock
};

struct Rig {
    loom::Switchboard bus;
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
    Seen seen;
    loom::WeaveId witness = mount_witness();
    std::uint64_t next_corr = 1;
    std::vector<CaughtLetter> letters;

    Rig() { watch_drives(); }

    loom::WeaveId mount_witness() {
        seen.bus = &bus;
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow(loom::SwapWeave::zen_name, loom::SwapWeave::zen_version, manager);
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

    /// The beat watchdog: the one pump lever that works on BOTH sides of the
    /// clock's existence. Before any timer is loaded no Drive is ever
    /// delivered, so it never trips and the pump simply drains; once a chain is
    /// alive it bounds an otherwise endless pump. Since R2A-2 that dual nature
    /// is required rather than convenient — loading the timer service is what
    /// starts time, so even `load()` runs under a live chain from its own
    /// second half onward.
    std::int64_t drives = 0;
    std::int64_t stop_after_drives = -1;

    std::string stop_on_schema; ///< stop the moment a message of this shape is handled

    void watch_drives() {
        bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered) {
                return;
            }
            if (ev.schema_name == "Drive") {
                ++drives;
                if (stop_after_drives >= 0 && drives >= stop_after_drives) {
                    bus.stop();
                }
            }
            if (ev.schema_name == loom::Bequest::zen_name && ev.payload != nullptr) {
                letters.push_back(CaughtLetter{ev.target, ev.sender, *ev.payload, drives});
            }
            if (!stop_on_schema.empty() && ev.schema_name == stop_on_schema) {
                bus.stop();
            }
        });
    }

    void pump_beats(std::int64_t n) {
        stop_after_drives = drives + n;
        bus.pump();
        stop_after_drives = -1;
    }

    /// Pump until one message of `schema` has been handled, then stop.
    ///
    /// The continuity lane's ruler. Virtual time only moves inside a beat's nap,
    /// so stopping at a named message puts the clock at a known instant and
    /// makes every duration after it exactly `kBeatCapMs` per delivered beat.
    /// The beat budget is a hang guard, not a schedule: the chain never
    /// quiesces, so a message that never arrives would otherwise pump forever.
    void pump_until(std::string schema, std::int64_t max_beats = 400) {
        stop_on_schema = std::move(schema);
        stop_after_drives = drives + max_beats;
        bus.pump();
        stop_after_drives = -1;
        stop_on_schema.clear();
    }

    /// Read one integer field, WITHOUT the stopwatch. `poke_timed` bounds its
    /// pump by asking the service for a one-shot, which is exactly what the
    /// continuity lane must not do — the instrument would appear in the table
    /// it is measuring, and in the letter it is measuring it with.
    std::int64_t poke_int(loom::WeaveId target, const char* field) {
        const std::uint64_t corr = next_corr++;
        bus.send(target, loom::Message(loom::to_value(loom::PokeRead{field}), loom::WeaveId{},
                                       witness, corr));
        for (int i = 0; i < 8 && seen.find(corr) == nullptr; ++i) {
            pump_beats(1);
        }
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

TEST_CASE("contract: the R2B-0 continuity shapes are frozen the same way, and their Text "
          "fields are contract rather than taste") {
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

    // The service's counters grew, honestly: v1 meant four and still does.
    CHECK(schema_of<TimerState>()->version() == 2);
    CHECK(schema_of<TimerState>()->content_id() == SchemaBuilder("TimerState", 2)
                                                       .field("beats", Kind::Int)
                                                       .field("fired", Kind::Int)
                                                       .field("active", Kind::Int)
                                                       .field("dropped", Kind::Int)
                                                       .field("deferred_dropped", Kind::Int)
                                                       .field("inherited", Kind::Int)
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
    CHECK(kBootstrapBeats == 2);
}

TEST_CASE("contract: the R2B-3c preparation conversation is frozen too, and it adds no second "
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
    CHECK(kPreparedClaimBeats == 8);

    // THE LETTER DID NOT CHANGE, and that is the contract this phase most needed
    // to keep: prepared replacement reuses `TimerHandoff` exactly, so there is
    // one interpretation of schedule progress in this package and not two.
    CHECK(schema_of<TimerHandoff>()->version() == 1);
    CHECK(schema_of<TimerHandoffEntry>()->version() == 1);
    // ...and the service's own state did not have to grow to carry a preparation.
    CHECK(schema_of<TimerState>()->version() == 2);
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

// ============================================================================
// Tier 5 — the timer BINDING (R2A-3): the word that replaced the ceremony
// ============================================================================
//
// The binding declares desire and owns the Timer protocol. These cases prove
// what that makes true — declaration is not execution, activation and
// TimerReady both reconcile, dispatch is exact, cancellation is both halves,
// and the convenience buys nothing by widening authority — over a real bus.

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

/// A bound weave that ALSO wants domain work on activation — the case that used
/// to be written as a raw `on(const loom::Activated&, loom::Mail&)` and silently
/// cost the author every timer they had declared.
///
/// It carries all three things at once on purpose: Timer handlers it inherits, a
/// domain handler of its own (so the `using` requirement is exercised, not
/// assumed), and the activation extension. All three must stay reachable.
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
        bus.pump();
    }
    void activate() { activate(next_sequence++); }

    /// The service says it is available (root-published, as the real one does).
    void ready() {
        bus.publish(loom::Message(loom::to_value(TimerReady{})));
        bus.pump();
    }

    /// A firing, stamped as the service — the honest wire path.
    void fire(const char* id) {
        bus.send_as(service, weave,
                    loom::Message(loom::to_value(TimerFired{id}), service, service, 0));
        bus.pump();
    }

    template <class T>
    void tell(const T& msg) {
        bus.send(weave, loom::Message(loom::to_value(msg)));
        bus.pump();
    }

    std::int64_t count(const char* field) {
        loom::Unverified u = loom::parse(bus.snapshot_bytes(weave));
        loom::Admission a = loom::admit(u, loom::schema_of<BoundState>());
        REQUIRE(a.ok());
        return a.value().get(field)->as_int();
    }
};

/// The same rig for the weave that carries an activation hook, plus a TAP that
/// records the ORDER shapes were actually delivered in. Order is read off the
/// bus rather than inferred from final counters, because "both happened" and
/// "they happened in this order" are different claims and only one of them is
/// the phase law.
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
        bus.pump();
    }
    void activate() { activate(next_sequence++); }

    /// An activation nobody attested — an ordinary root send of the right shape.
    void activate_unattested(std::int64_t sequence) {
        bus.send(weave, loom::Message(loom::to_value(loom::Activated{sequence})));
        bus.pump();
    }

    void ready() {
        bus.publish(loom::Message(loom::to_value(TimerReady{})));
        bus.pump();
    }

    void fire(const char* id) {
        bus.send_as(service, weave,
                    loom::Message(loom::to_value(TimerFired{id}), service, service, 0));
        bus.pump();
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
    bus.pump();
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
    bus.pump();
    CHECK(events > 0);
}

// ---- the activation extension hook ------------------------------------------
//
//   A derived weave may EXTEND Timer activation. It may never REPLACE it.
//
// The raw `on(zen.Activated)` handler is the binding's, and a derived
// redefinition is refused at build time (see the compile-negative lane, which
// is where that half is proven — an offending weave never becomes an artifact,
// so no runtime suite can watch it). What runs here is the other half: the
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
    // The Timer protocol, this weave's own two doors, and the four substrate
    // poke doors its ZEN_EXPOSE'd state earns — and nothing else. In particular
    // no trace of the hook's shape, which belongs to a different weave entirely.
    CHECK(accepted == std::vector<std::string>{
                          "CancelTick", "RestartTick", "TimerFired", "TimerReady",
                          "TimerResolution", "zen.Activated", "zen.PokeDescribe", "zen.PokeRead",
                          "zen.PokeResetState", "zen.PokeWrite"});
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
    // "library create() returned null" load refusal — the ABI's create thunk
    // catches everything — which is the path every construction failure
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
    // The four the binding accepts on the author's behalf — the receipt joined
    // them in R2B-0, because an order that could not be answered would be a
    // conversation the manifest lied about.
    CHECK(accepts(loom::Activated::zen_name, loom::Activated::zen_version));
    CHECK(accepts("TimerReady", 1));
    CHECK(accepts("TimerFired", 1));
    CHECK(accepts("TimerResolution", 1));
    // ...and the author's own doors, unharmed.
    CHECK(accepts("CancelTick", 1));
    CHECK(accepts("RestartTick", 1));

    // The emitted set is the composed truth too — the three the binding may
    // send, plus the author's own. It says ORDERED now, and says ONLY ordered:
    // the binding stopped speaking the raw start shapes, so its manifest stopped
    // claiming it could. (The raw vocabulary is still public and unchanged; this
    // weave simply does not use it.)
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
// Tier 5 — CONTINUITY, end to end: real libraries, real kernel, real steward, a
// real graceful replacement, a real letter through the real gate, and virtual
// time so the semantics are exact instead of slept for (R2B-0)
// ============================================================================
//
// Everything here runs through `zengine-timer-virtual` — TimerServiceT exactly
// as shipped, over a clock whose nap books the requested duration and returns.
// One delivered beat is therefore exactly kBeatCapMs of virtual time, and every
// duration in the scenario is an integer nobody waited for.

namespace {

/// The one thing this lane knows about the Loom's ceremony: SwapWeave ->
/// QueryRole -> RoleInfo -> PrepareShutdown is four queue turns, and the beat
/// chain always has exactly one parked beat ahead of whatever is enqueued next,
/// so four napping beats pass between the command and the predecessor reading
/// its clock. Counted rather than absorbed — that is what lets "three seconds
/// elapsed" mean three seconds, and keeps the exact-2000 assertion a real pin.
constexpr std::int64_t kGracefulCeremonyBeats = 4;

/// Establish the probe's five-second one-shot at a KNOWN instant. The message is
/// the very one the probe's own binding sends; `restart_delay` re-anchors it
/// from now, and stopping the pump the moment it is handled puts the virtual
/// clock exactly at the anchor.
void anchor_probe(Rig& r, loom::WeaveId probe, loom::WeaveId timer) {
    r.ask_as(probe, timer,
             EnsureTimer{zengine::probe::kProbeTimerId, 5000, false, kRestartDelay, ""});
    r.pump_until(EnsureTimer::zen_name);
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

    anchor_probe(r, probe, timer);

    // 3. Three seconds of virtual time, the ceremony's own beats counted in, so
    // that when the predecessor reads its clock exactly 3000ms have passed.
    r.pump_beats(3000 / kBeatCapMs - kGracefulCeremonyBeats);

    // 4. The graceful replacement. Same library, a genuinely new incarnation.
    const std::uint64_t corr = r.command(loom::SwapWeave{
        kTimerRole, "zengine-timer-virtual", TIMER_VIRTUAL_SO, /*graceful=*/true});
    r.pump_until(TimerResolution::zen_name); // stops once the heir has answered the probe

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

    // 8. LESS THAN THE REMAINING TIME: NOTHING FIRES. The boundary is derived,
    // not guessed — the heir's clock began at zero and only napping beats have
    // moved it since the letter landed, so the beats left are exactly countable.
    const std::int64_t napped = r.drives - r.letters[1].drives;
    const std::int64_t to_boundary = 2000 / kBeatCapMs - napped;
    REQUIRE(to_boundary > 1);
    r.pump_beats(to_boundary - 1); // one beat short of two seconds
    CHECK(r.ask_probe(probe).fires == 0);

    // 9. THE BOUNDARY BEAT: exactly once. Note what did NOT happen — it did not
    // wait a fresh five seconds, which is what a re-anchored schedule would have
    // done and what every earlier version of this system did.
    r.pump_beats(1);
    rep = r.ask_probe(probe);
    CHECK(rep.fires == 1);

    // 10. AND IT IS SPENT — an honest lifecycle state, not a bool that could
    // only say "wanted".
    CHECK(rep.lifecycle == "spent");
    CHECK(r.poke_int(successor, "active") == 0);

    // 11. ANOTHER AVAILABILITY NOTICE RECREATES NOTHING. This is exactly the
    // resurrection a single `desired` flag could not prevent.
    r.bus.publish(loom::Message(loom::to_value(TimerReady{})));
    r.pump_beats(4);
    rep = r.ask_probe(probe);
    CHECK(rep.fires == 1);
    CHECK(rep.lifecycle == "spent");
    CHECK(r.poke_int(successor, "active") == 0);

    // 12. AN EXPLICIT RESTART ARMS IT AGAIN — once, from the full declared
    // delay, and the receipt says restarted rather than preserved.
    r.ask(probe, zengine::probe::RestartProbe{});
    r.pump_until(TimerResolution::zen_name);
    rep = r.ask_probe(probe);
    CHECK(rep.lifecycle == "waiting");
    CHECK(rep.resolved == kResolutionRestarted);
    CHECK(r.poke_int(successor, "active") == 1);

    r.pump_beats(5000 / kBeatCapMs + 4);
    rep = r.ask_probe(probe);
    CHECK(rep.fires == 2); // once more, and only once more
    CHECK(rep.lifecycle == "spent");
}

TEST_CASE("continuity, hard: no letter exists, so the declared fallback restarts the delay — "
          "and says so instead of pretending") {
    Rig r;
    const loom::WeaveId timer = r.load("zengine-timer-virtual", TIMER_VIRTUAL_SO, kTimerRole);
    const loom::WeaveId probe = r.load("zengine-probe-oneshot", PROBE_ONESHOT_SO, "");
    anchor_probe(r, probe, timer);
    r.pump_beats(3000 / kBeatCapMs);

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

    // AND IT DOES NOT FIRE AT THE OLD BOUNDARY. Two seconds of virtual time was
    // the whole remaining schedule a moment ago; from a full restart it is not
    // even half of one.
    r.pump_beats(2000 / kBeatCapMs + 10);
    CHECK(r.ask_probe(probe).fires == 0);

    // It fires at the FULL declared delay instead, which is the honest meaning
    // of "restarted".
    r.pump_beats(3000 / kBeatCapMs + 10);
    CHECK(r.ask_probe(probe).fires == 1);
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
    r.pump_beats(5000 / kBeatCapMs + 20);
    CHECK(r.ask_probe(probe).fires == 0);
    CHECK(r.poke_int(timer, "active") == 0);
}

// ============================================================================
// Tier 2 continuity — the unit pins, over the fake clock (R2B-0)
// ============================================================================
//
// The end-to-end lane (tier 5) proves the whole sentence through real
// libraries. These prove the pieces, including the ones an honest predecessor
// could never produce: a letter written to another version, a letter over the
// published bound, a steward that never answers at all.

TEST_CASE("letter: the predecessor describes every standing entry as a REMAINING duration, "
          "reads its clock once, and changes nothing by being asked") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Refuse);
    r.run_beats(3, /*start=*/true);

    r.ask_as(r.ear, StartTimer{"mine.oneshot", 5000, false});
    r.ask_as(r.ear, StartRoleTimer{"slot.beat", 9000, true, "some.slot"});
    r.run_beats(3);

    // Nothing here is anywhere near due, so the beats the letter's own pump
    // spends provably change nothing — which is what makes the no-movement
    // assertion at the end mean something.
    const std::int64_t fired_before = r.count("fired");
    const std::int64_t active_before = r.count("active");
    REQUIRE(active_before == 2);
    REQUIRE(fired_before == 0);

    const std::vector<TimerHandoffEntry> offered = r.prepare_shutdown();
    REQUIRE(offered.size() == 2);

    // A REQUESTER entry round-trips whole: the id, the mode (no role), the
    // declared delay, the repeat flag, and the requester as lossless decimal.
    CHECK(offered[0].id == "mine.oneshot");
    CHECK(offered[0].role.empty());
    CHECK(offered[0].delay_ms == 5000);
    CHECK_FALSE(offered[0].repeat);
    CHECK(offered[0].requester == std::to_string(r.ear.value));
    // The whole arithmetic claim, exactly: what is left is what was declared
    // minus what has elapsed since the schedule was anchored — measured from the
    // bus's own view of both instants, not from a hoped-for constant.
    REQUIRE(r.ask_now >= 0);
    REQUIRE(r.letter_now >= r.ask_now);
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

TEST_CASE("letter: a due or overdue entry transfers with ZERO remaining, never a negative "
          "number") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Refuse);
    r.run_beats(3, /*start=*/true);

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
    w.run_beats(3, /*start=*/true);
    for (std::size_t i = 0; i < kMaxHandoffEntries + 5; ++i) {
        w.ask_as(w.ear, StartTimer{"t" + std::to_string(i), 1000, true});
    }
    w.run_beats(4);
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
    r.run_beats(4, /*start=*/true);
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
        r.run_beats(4, /*start=*/true);
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
        r.run_beats(4, /*start=*/true);
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
        r.run_beats(4, /*start=*/true);
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

    r.run_beats(3, /*start=*/true);

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
    r.run_beats(4);
    CHECK(r.count("inherited") == 1); // unchanged
    CHECK(r.count("active") == 1);    // "sneaky" never joined the table
    CHECK(r.heard.ready == 1);        // and nothing re-announced
}

TEST_CASE("bootstrap: a steward that never answers does not hang the service — it resolves "
          "fresh after its own bounded beats, with no wall clock and no spin") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Silence);
    r.ask_as(r.ear, StartTimer{"pre.activation", 100, true});

    r.run_beats(3, /*start=*/true);
    CHECK(r.letters.claims == 1); // it did ask
    CHECK(r.heard.ready == 1);    // and it became available anyway
    CHECK(r.count("inherited") == 0);

    // It became LIVE, not merely resolved: the entry it was holding is standing
    // and the chain is beating.
    CHECK(r.count("active") == 1);
    const std::int64_t beats = r.count("beats");
    r.run_beats(3);
    CHECK(r.count("beats") > beats);

    // Exactly one chain: the bus holds one parked beat, not two.
    CHECK(r.bus.pending() == 1);
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
    r.run_beats(4);

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
    r.run_beats(410); // 4100ms of virtual time, well past the inherited 4000
    CHECK(r.heard.fired.empty());
    // ...and a firing at the declared delay from when the order landed.
    r.run_beats(100);
    REQUIRE(r.heard.fired.size() == 1);
    CHECK(r.heard.fired[0] == "contested");
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
    r.run_beats(4);

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
    r.run_beats(3, /*start=*/true);

    // Nothing standing: preservation is unavailable, the fallback restarts.
    r.ask_as(r.ear, EnsureTimer{"m", 1000, true, kPreserveRemaining, kRestartDelay});
    r.run_beats(3);
    REQUIRE(r.heard.resolutions.size() == 1);
    CHECK(r.heard.resolutions.back().resolved == kResolutionRestarted);

    // The SAME declaration now matches: same key, same repeat mode, same delay.
    r.ask_as(r.ear, EnsureTimer{"m", 1000, true, kPreserveRemaining, kRestartDelay});
    r.run_beats(3);
    CHECK(r.heard.resolutions.back().resolved == kResolutionPreserved);

    // A DIFFERENT DELAY under the same id is a different schedule. The entry is
    // found, but preserving it would be describing a schedule nobody asked for.
    r.ask_as(r.ear, EnsureTimer{"m", 2000, true, kPreserveRemaining, kRestartDelay});
    r.run_beats(3);
    CHECK(r.heard.resolutions.back().resolved == kResolutionRestarted);
    CHECK(r.heard.resolutions.back().reason.find("differs from the standing one") !=
          std::string::npos);

    // A DIFFERENT REPEAT MODE likewise: "every two seconds" and "once, in two
    // seconds" are not the same timer wearing different clothes.
    r.ask_as(r.ear, EnsureTimer{"m", 2000, false, kPreserveRemaining, kRestartDelay});
    r.run_beats(3);
    CHECK(r.heard.resolutions.back().resolved == kResolutionRestarted);

    // A DIFFERENT ADDRESSING MODE has a different key by construction, so it
    // finds nothing at all — and the requester-addressed entry it did not match
    // is left exactly where it was.
    const std::int64_t active_before = r.count("active");
    r.ask_as(r.ear,
             EnsureRoleTimer{"m", 2000, false, "some.slot", kPreserveRemaining, kRestartDelay});
    r.run_beats(3);
    CHECK(r.heard.resolutions.back().resolved == kResolutionRestarted);
    CHECK(r.count("active") == active_before + 1);
}

TEST_CASE("order: drop removes a standing schedule and declines to create one, and an unknown "
          "preference is refused rather than guessed at") {
    FakeRig r;
    r.mount_steward(Letters::Answer::Refuse);
    r.run_beats(3, /*start=*/true);

    r.ask_as(r.ear, StartTimer{"d", 1000, true});
    r.run_beats(3);
    REQUIRE(r.count("active") == 1);

    // Drop removes it.
    r.ask_as(r.ear, EnsureTimer{"d", 1000, true, kDrop, ""});
    r.run_beats(3);
    CHECK(r.heard.resolutions.back().resolved == kResolutionDropped);
    CHECK(r.count("active") == 0);

    // Drop with nothing to drop declines to create one — same word, and it is
    // the truthful one either way.
    r.ask_as(r.ear, EnsureTimer{"never.existed", 1000, true, kDrop, ""});
    r.run_beats(3);
    CHECK(r.heard.resolutions.back().resolved == kResolutionDropped);
    CHECK(r.count("active") == 0);

    // A spelling this package does not know is REFUSED. Nothing is guessed at,
    // and the reason quotes the word back so a stranger can see the typo.
    r.ask_as(r.ear, EnsureTimer{"x", 1000, true, "preserve-remaining", kRestartDelay});
    r.run_beats(3);
    CHECK(r.heard.resolutions.back().resolved == kResolutionRefused);
    CHECK(r.heard.resolutions.back().reason.find("preserve-remaining") != std::string::npos);
    CHECK(r.count("active") == 0);

    // An ordered ROLE request with no role is refused too — the raw shape drops
    // it silently, but an order has somewhere to hear why.
    r.ask_as(r.ear, EnsureRoleTimer{"y", 1000, true, "", kPreserveRemaining, kRestartDelay});
    r.run_beats(3);
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
        bus.pump();
    };
    const auto fire = [&] {
        bus.send_as(service, weave,
                    loom::Message(loom::to_value(TimerFired{"shooter.shot"}), service, service, 0));
        bus.pump();
    };
    const auto ready = [&] {
        bus.publish(loom::Message(loom::to_value(TimerReady{})));
        bus.pump();
    };
    const auto shooter = [&] { return static_cast<Shooter*>(bus.weave(weave)); };

    activate(1);
    REQUIRE(seen.asks.size() == 1);
    CHECK(shooter()->shot_.waiting());

    // It fires. `once` means once per binding incarnation.
    fire();
    CHECK(shooter()->shot_.spent());

    // A SPENT binding does not reconcile — not on an availability notice, not
    // on a fresh activation. Nothing resurrects it, which is precisely what the
    // old single `desired` flag could not express.
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
    bus.pump();

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
    r.bus.pump();
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
    r.bus.pump();
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
          "the process at all, still becomes live — and authors exactly one chain") {
    // The whole point of this case is what is MISSING. There is no Weave
    // Manager, so `zen.manager` is unheld and `zen.ClaimBequest` is a shape
    // nobody in this process accepts — the service's claim is rejected at the
    // library seam and no answer can ever come. A bootstrap that waited on one
    // would wait forever, on a working bus, and the operator would see a load
    // that succeeded and a service that never spoke.
    loom::Switchboard bus;
    loom::Kernel kernel{bus};
    const loom::WeaveId control = loom::mount_control(kernel, bus);

    Seen seen;
    seen.bus = &bus;
    loom::Grant reach = loom::load_capability(control); // the dangerous half, target-scoped
    reach.allow_to_any(StartTimer::zen_name, StartTimer::zen_version); // and one ordinary ask
    const loom::WeaveId witness = loom::mount_granted<Witness>(bus, std::move(reach), seen);

    std::int64_t drives = 0;
    std::int64_t stop_at = -1;
    bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered && ev.schema_name == "Drive") {
            ++drives;
            if (stop_at >= 0 && drives >= stop_at) {
                bus.stop();
            }
        }
    });
    const auto pump_beats = [&](std::int64_t n) {
        stop_at = drives + n;
        bus.pump();
        stop_at = -1;
    };

    const std::uint64_t corr = 1;
    bus.send_as(witness, control,
                loom::Message(loom::to_value(loom::LoadLibrary{"zengine-timer-virtual",
                                                               TIMER_VIRTUAL_SO, kTimerRole}),
                              witness, witness, corr));
    pump_beats(6);

    const Seen::Answer* a = seen.find(corr);
    REQUIRE(a != nullptr);
    REQUIRE_MESSAGE(a->kind == 0, "load refused: ", a->text);

    // AVAILABLE: the claim went unanswered and the bootstrap resolved itself
    // after its own bounded beats — no wall clock, no timeout, no steward.
    CHECK(seen.ready == 1);

    // LIVE: the chain is beating, with nobody winding anything.
    CHECK(drives >= 3);

    // And it does real work, which is the difference between "resolved" and
    // "alive": a timer asked for here fires.
    const loom::WeaveId service{static_cast<std::uint64_t>(std::stoll(a->text))};
    bus.send_as(witness, service,
                loom::Message(loom::to_value(StartTimer{"direct.tick", 20, false}), witness,
                              witness, 0));
    pump_beats(6);
    REQUIRE(seen.fired.size() >= 1);
    CHECK(seen.fired[0] == "direct.tick");

    // EXACTLY ONE CHAIN. With the queue quiet the only thing pending is the one
    // parked beat: a second chain would leave a second.
    pump_beats(1);
    CHECK(bus.pending() == 1);
}

// ============================================================================
// R2B-1 — the hostile half: an ordinary weave, with everything the threat model
// grants it, trying to be the steward
// ============================================================================
//
// The impersonator below is an ORDINARY REGISTERED WEAVE. It knows every public
// schema, it knows `kClaimCorrelation` (published, precisely so this test can
// use it), it knows the Timer's role and its handoff format well enough to build
// bytes that pass the gate, and it holds an ordinary grant to send `zen.Bequest`
// and `zen.Refused` to anyone. It reaches around nothing.
//
// What it does not have is the one thing that is not a value: Loom's word that
// it is the weave the Timer's claim was actually delivered to.

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
    r.run_beats(4);

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
    CHECK(r.bus.pending() == 1); // one chain, beating
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
    r.run_beats(4);

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
    r.run_beats(4);
    REQUIRE(r.count("inherited") == 1);
    REQUIRE(r.count("active") == 1);

    // Late, correlated, gate-valid, and describing a completely different world.
    r.bus.send(forger, loom::Message(loom::to_value(ForgeLetter{
                           false, 9000, static_cast<std::int64_t>(kClaimCorrelation)})));
    r.run_beats(4);

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
    r.run_beats(4);

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
    r.run_beats(4);
    CHECK(r.heard.ready == 0);      // never became available
    CHECK(r.count("beats") == 0);   // never authored a chain
    CHECK(r.bus.pending() == 0);    // and left nothing behind

    // A REAL authority, used dishonestly: Loom attests sequence 5 while the
    // payload claims 6. A proof for one activation is not a proof for another.
    r.activate_mismatched(5, 6);
    r.run_beats(4);
    CHECK(r.heard.ready == 0);
    CHECK(r.count("beats") == 0);

    // THE POSITIVE CONTROL. The same door, the same service, the two halves
    // agreeing — so the silence above is refusal, not a broken rig.
    r.run_beats(4, /*start=*/true);
    CHECK(r.heard.ready == 1);
    CHECK(r.count("beats") > 0);
}

// ============================================================================
// Tier 6 — THE KEYSTONE: a LIVE Timer crosses a prepared replacement (R2B-3c)
// ============================================================================
//
// Everything below runs through real dynamic artifacts, the real Kernel, the
// real prepared-replacement transaction, and virtual time — so a claim about
// where the clock was is an exact integer nobody slept for.
//
// THE MOVING-STATE PROBLEM, and where this package put the boundary. While a
// candidate prepares, the incumbent keeps serving: its clock advances, timers
// fire, repeats re-arm, consumers start and cancel schedules. So a snapshot
// taken at transaction begin is stale at commit, and one taken after commit
// would leave semantic restoration until after the world had already changed.
//
// The boundary is THE ADMISSION ITSELF, and two substrate facts (not Timer
// mechanisms) are what make it exact:
//
//   * the beat chain rides the ROLE, so moving the role ends the incumbent's
//     chain — it never beats again, never fires again, and its clock stops;
//   * admission seals the incumbent in the same breath, so nothing but the
//     coordinator can reach it and its table is frozen.
//
// The letter is therefore written AFTER the admission by a service that has
// already been made incapable of changing, through the UNCHANGED R2B-0 exchange
// (`zen.PrepareShutdown` -> `TimerHandoff`). One interpretation of schedule
// progress, and no state is parked anywhere that an abort would have to release.
//
// THE RULER. `on(PrepareShutdown)` fires nothing, cancels nothing and advances
// nothing — R2B-0's law — so the suite uses it as an exact clock read: ask the
// live incumbent to describe itself, catch the letter on the tap, and know how
// much of the probe's schedule is left at that instant. That is what lets every
// number below be derived from a measurement rather than predicted.

namespace {

/// The keystone's correlation, so a reader can tell the ceremony's letter apart
/// from the suite's ruler reads on the wire.
constexpr std::uint64_t kFinalizeCorrelation = 0x3C0DE;

/// How many NAPPING incumbent beats pass between resuming the pump with the
/// preparation ask enqueued and the admission that closes the boundary.
///
/// Counted rather than absorbed, exactly as the graceful lane counts its
/// ceremony: the queue reads [parked Drive, ask], the ask is answered on the
/// turn after the beat it sits behind, and the coordinator admits on the turn
/// after that — one beat later again. Two, and the exact-2000 assertion below is
/// what keeps it honest.
constexpr std::int64_t kPreparedCeremonyBeats = 2;

/// THE RULER'S OWN COST. Reading the clock costs queue turns, and the beat that
/// was already parked when the read was asked for runs inside one of them — so
/// the number a read hands back is exactly one beat old by the time the pump
/// stops. Named rather than absorbed into the arithmetic, because an instrument
/// that quietly perturbs what it measures is how a suite starts proving the
/// wrong thing.
constexpr std::int64_t kRulerTrailingBeats = 1;

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

/// THE COORDINATOR — trusted host-tier infrastructure, exactly the tier the Weave
/// Manager occupies, and the only party in the keystone that holds a Switchboard.
///
/// It is deliberately CREDULOUS about payloads: it never reads a transaction id
/// off a message to decide anything, because the one it began is the one it
/// remembers. Everything that authorizes a transition is read by the bus from the
/// delivery itself, so a green here is never "the coordinator was careful".
///
/// It does four things, and the ORDER of the first two is the whole phase:
///   1. consume the candidate's authentic preparation answer;
///   2. admit INSIDE THAT SAME DELIVERY — no ordinary delivery runs between
///      readiness and admission, so there is no unaccounted window;
///   3. ask the now-retired incumbent for its letter (the ordinary R2B-0 ask);
///   4. hand that letter to the candidate as the authenticated answer to the
///      candidate's OWN claim — which is why it defers the claim rather than
///      inventing a second way to deliver a bequest.
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
    /// R2B-0 wrote for a graceful succession.
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

/// The keystone rig: the R2B-0 lane plus a coordinator, a Kernel-sealed
/// candidate, and the transaction that binds them.
struct Keystone {
    Rig r;
    Prep prep;
    loom::WeaveId coordinator{};
    loom::WeaveId incumbent{};
    loom::WeaveId probe{};

    /// `timer_path` is the incumbent artifact — the virtual-clock build for the
    /// deterministic proofs, the SHIPPED real-clock one for the pilot. `probe`
    /// is loaded only where a five-second one-shot is a sensible thing to wait
    /// for, which on a real clock it is not.
    explicit Keystone(const char* timer_path = TIMER_VIRTUAL_SO, bool with_probe = true) {
        prep.bus = &r.bus;
        loom::Grant reach;
        reach.allow_to_any(PrepareTimerHandover::zen_name, PrepareTimerHandover::zen_version);
        reach.allow_to_any(loom::PrepareShutdown::zen_name, loom::PrepareShutdown::zen_version);
        reach.allow_to_any(loom::Bequest::zen_name, loom::Bequest::zen_version);
        // AND DELIBERATELY *NOT* `zen.Activated` (R2B-3d).
        //
        // R2B-3c had to grant it, and said so at length: `admit_candidate` used to
        // enqueue the activation as an ordinary gated send stamped with the
        // coordinator's identity, so a coordinator that could not emit the shape
        // committed perfectly successfully — the role moved, the candidate was
        // unsealed, `commit_prepared_replacement` returned ok — and the activation
        // was then refused at delivery as `CapabilityDenied`. A candidate that was
        // publicly the service and had never been told it was alive.
        //
        // A committed activation is Loom's own act now, authorized by the
        // lifecycle authority this coordinator holds and performed as part of the
        // admission. So the ordinary grant is gone from this line, and the whole
        // keystone below — a real dynamic Timer crossing a real replacement —
        // runs without it. That absence is the end-to-end proof, across the
        // library seam, that the ordinary grant was never what made it true.
        coordinator = loom::mount_granted<Preparer>(r.bus, std::move(reach), prep,
                                                    loom::host_lifecycle_authority(r.bus));
        incumbent = r.load("zengine-timer-v1", timer_path, kTimerRole);
        prep.incumbent = incumbent;
        if (with_probe) {
            // The consumer of the phase: one requester-addressed five-second
            // one-shot whose preservation is REQUIRED — no fallback will do.
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

    /// What is left of the probe's one-shot RIGHT NOW — the ruler's read, with
    /// the ruler's own cost taken back off it.
    std::int64_t probe_remaining() {
        return remaining_now(zengine::probe::kProbeTimerId) -
               kRulerTrailingBeats * kBeatCapMs;
    }

    /// Advance the live Timer until exactly `want` milliseconds of the probe's
    /// one-shot are left — measured, never assumed.
    void advance_until_remaining(std::int64_t want) {
        const std::int64_t now = probe_remaining();
        REQUIRE(now >= want);
        REQUIRE((now - want) % kBeatCapMs == 0);
        r.pump_beats((now - want) / kBeatCapMs);
    }

    /// The last letter that crossed the wire, decoded through the real gate.
    ///
    /// REQUIREs one to exist rather than trusting that it does: what it returns
    /// is indexed and iterated, and a mutation that produces NO letter must stop
    /// the case rather than detonate an unguarded `.back()` and take every later
    /// verdict in the run with it. (M18b did exactly that, once.)
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

    // 5-6. Three seconds of virtual time, and the one-shot has not fired. (The
    // exact ruler read that makes "two seconds" exact comes just before the
    // boundary; everything between here and there costs beats of its own, and
    // measuring last is what keeps the number honest rather than predicted.)
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
    CHECK(k.field(candidate, "beats") == 0);    // the candidate has no time at all
    CHECK(k.field(candidate, "active") == 0);

    // 11-14. THE BOUNDARY. Wind to exactly two seconds plus the ceremony's own
    // beats, then let it run: the ask goes through the coordinator-only door,
    // the candidate answers for itself, and the coordinator admits it inside
    // that same delivery.
    k.advance_until_remaining(2000 + kPreparedCeremonyBeats * kBeatCapMs);
    const std::int64_t beats_before = k.field(k.incumbent, "beats");
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerReady::zen_name);
    k.r.pump_until(TimerResolution::zen_name); // ...and the consumer's re-ask answered
    CHECK(k.prep.prepared == 1);
    REQUIRE(k.prep.readiness.size() == 1);
    CHECK(k.prep.readiness.back().ok);
    REQUIRE(k.prep.commits.size() == 1);
    CHECK(k.prep.commits.back().ok); // the commit call SCHEDULED the admission...
    // ...AND THE REAL COMMITTED OUTCOME IS THE ONE THAT COUNTS (R2B-3d). The
    // coordinator's `ok` says the admission is on its way; what says it happened
    // is the transaction's own terminal result, written inside the dispatch that
    // moved the role and told the successor. The operator collects it once.
    loom::TxnOutcome outcome{};
    REQUIRE(k.r.bus.take_outcome(k.r.witness, outcome));
    CHECK(outcome.state == loom::TxnState::Committed);
    CHECK(outcome.reason == loom::TxnReason::None);
    CHECK_FALSE(k.r.bus.take_outcome(k.r.witness, outcome)); // consumed
    CHECK(k.r.bus.active_transactions() == 0);
    // The ceremony cost exactly the beats the constant claims.
    CHECK(k.field(k.incumbent, "beats") - beats_before == kPreparedCeremonyBeats);

    // 15-17. The role moved; the incumbent is retirement-private, not merely
    // renamed; the candidate is the service, in the Kernel's books as well.
    CHECK(k.r.bus.role_holder(kTimerRole) == candidate);
    CHECK(k.r.bus.sealed(k.incumbent));
    CHECK_FALSE(k.r.bus.sealed(candidate));
    CHECK(k.r.kernel.role_of("zengine-timer-v2") == kTimerRole);
    CHECK(k.r.kernel.role_of("zengine-timer-v1").empty());

    // THE NUMBER THAT IS THE PHASE. Exactly two seconds remained at the boundary
    // — read off the letter the retired incumbent actually wrote, decoded through
    // the real gate.
    // ⭐ AND THIS IS ALSO THE R2B-3d-1 POSITIVE CONTROL AT THE PACKAGE TIER.
    // That claim is ORDINARY DOMAIN SPEECH sent from inside a COMMITTED
    // ADMISSION ACTIVATION — `mail.send(preparer_, ClaimBequest{...})`, the
    // first thing the successor does on its first breath. An activation that
    // grants no answer authority must still leave a weave able to speak, and
    // the entire handoff below is the proof that it does: no claim, no letter,
    // no continuity. (That the activation is not ANSWERABLE is a Loom-tier law
    // — the shipped Timer never tries, so only the Loom suite can pin it.)
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

    // 25-28. AND THE CLOCK DID NOT RESTART. One millisecond short of the
    // remaining time: nothing. The next beat: exactly one firing — not five
    // seconds from the commit, which is what every re-anchoring design would
    // have done and what a stale snapshot would have produced.
    //
    // The ruler now reads the CANDIDATE (it is addressed to the role), which is
    // itself the proof that the schedule it inherited is the schedule it holds.
    k.advance_until_remaining(kBeatCapMs);
    CHECK(k.r.ask_probe(k.probe).fires == 0);
    k.r.pump_beats(1);
    rep = k.r.ask_probe(k.probe);
    CHECK(rep.fires == 1);
    CHECK(rep.lifecycle == "spent");

    // ...and only once. A full five seconds later there is still one firing.
    k.r.pump_beats(5000 / kBeatCapMs);
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
    // the incumbent's OWN beat counter says the second really elapsed on it.
    const std::int64_t beats_at_begin = k.field(k.incumbent, "beats");
    REQUIRE(k.r.bus.tick_preparation(k.prep.txn).ok);
    k.advance_until_remaining(1000 + kPreparedCeremonyBeats * kBeatCapMs);
    REQUIRE(k.r.bus.tick_preparation(k.prep.txn).ok);
    CHECK(k.r.bus.transaction_state(k.prep.txn) == loom::TxnState::Preparing);
    CHECK(k.field(k.incumbent, "beats") - beats_at_begin ==
          1000 / kBeatCapMs - kPreparedCeremonyBeats);

    // ...and THE INCUMBENT WAS THE ONE SERVING THROUGH IT. Not "the role still
    // pointed at it" — it answered.
    CHECK(k.r.bus.role_holder(kTimerRole) == k.incumbent);
    CHECK(k.field(candidate, "beats") == 0);

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
    k.advance_until_remaining(kBeatCapMs);
    CHECK(k.r.ask_probe(k.probe).fires == 0);
    k.r.pump_beats(1);
    CHECK(k.r.ask_probe(k.probe).fires == 1);
}

TEST_CASE("keystone: the queue boundary — an operation ordered before the admission is in the "
          "letter, one ordered after it is held and replayed, and neither happens twice") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000 + kPreparedCeremonyBeats * kBeatCapMs);
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

/// NOTHING ABOUT THE INCUMBENT MOVED. The shape of every failure proof in this
/// lane, asked of the bus and of the service rather than of any object a failing
/// case might already have destroyed.
///
/// The last check is the one that matters most and is easiest to omit: the
/// one-shot is not merely still listed, it still fires WHERE IT WAS GOING TO —
/// once, at the boundary the incumbent has been counting toward all along.
void incumbent_never_stopped_being_the_timer(Keystone& k, loom::WeaveId candidate) {
    CHECK(k.r.bus.alive(k.incumbent));
    CHECK_FALSE(k.r.bus.sealed(k.incumbent));
    CHECK(k.r.bus.role_holder(kTimerRole) == k.incumbent);
    CHECK_FALSE(k.r.bus.role_holder(kTimerRole) == candidate);
    CHECK(k.field(k.incumbent, "active") == 1); // the probe's one-shot, still standing
    CHECK(k.field(k.incumbent, "inherited") == 0);
    CHECK(k.r.seen.ready == 1); // no candidate ever announced itself
    CHECK(k.r.ask_probe(k.probe).fires == 0);

    // EXACTLY ONE CHAIN. With the queue otherwise quiet the only pending
    // envelope is the incumbent's one parked beat; a forked or duplicated chain
    // would leave two.
    k.r.pump_beats(1);
    CHECK(k.r.bus.pending() == 1);

    // ...and the schedule was not reset: it runs out where it always would have,
    // and fires exactly once.
    k.advance_until_remaining(kBeatCapMs);
    CHECK(k.r.ask_probe(k.probe).fires == 0);
    k.r.pump_beats(1);
    CHECK(k.r.ask_probe(k.probe).fires == 1);
    k.r.pump_beats(200);
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
    // THE INPUT THE PACKAGE COULD NOT PRODUCE BEFORE R2B-3d, and a mutation is
    // what found that out: every other failure route in this lane ends the
    // transaction BEFORE an admission is ever scheduled, so "a failed admission
    // disturbs the incumbent" had no Timer-tier road at all. It has one now,
    // because scheduling and dispatch are two moments and the world may change
    // between them.
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000 + kPreparedCeremonyBeats * kBeatCapMs);
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
    k.r.pump_beats(4);

    // The admission refused; the transaction ended once, naming what happened;
    // no letter was ever written, because nothing ever froze the incumbent.
    loom::TxnOutcome out{};
    REQUIRE(k.r.bus.take_outcome(k.r.witness, out));
    CHECK(out.state == loom::TxnState::Aborted);
    CHECK(out.reason == loom::TxnReason::CandidateChanged);
    CHECK(k.prep.relayed == 0);
    CHECK(k.r.bus.active_transactions() == 0);

    // ...and the Timer that was serving is exactly the Timer that is serving:
    // same role, one chain, and a schedule that runs out where it always would.
    incumbent_never_stopped_being_the_timer(k, candidate);
}

TEST_CASE("keystone: the boundary is a POSITION IN THE QUEUE — a by-id operation ahead of it "
          "reaches the incumbent and crosses in the letter; one behind it is refused visibly") {
    // CHANGED BY R2B-3d, and the change is the whole reason the new model is
    // coherent. Admission used to jump ahead of the queue: it moved topology
    // inside the commit call, so a message enqueued while the incumbent was
    // public was judged against a world that only came into being afterwards, and
    // a by-id operation queued BEFORE the boundary was refused as `NoSuchTarget`.
    //
    // Now the admission occupies a position in the queue like everything else.
    // Ahead of it is the old world; behind it is the new one. That is the same
    // rule that makes role-addressed traffic land correctly, applied once instead
    // of twice — and it loses nothing: an operation the incumbent accepts before
    // the boundary is in the incumbent's table when the letter is written, so it
    // CROSSES rather than dying.
    //
    // The package's promise is unchanged and is still about the ROLE. What this
    // pins is that both sides of the boundary are truthful, and neither is silent.
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000 + kPreparedCeremonyBeats * kBeatCapMs);
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

TEST_CASE("keystone: on commit the incumbent's chain simply ends — because the chain rides the "
          "role — and exactly one new chain begins") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000 + kPreparedCeremonyBeats * kBeatCapMs);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerReady::zen_name);
    k.r.pump_until(TimerResolution::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);

    // NOTHING THE OLD SERVICE HAD QUEUED ADVANCED ANYTHING. Its beats and its
    // firings are frozen at the boundary and stay frozen through hundreds more
    // of the successor's — because a beat addressed to the role now reaches the
    // successor, which refuses it as a different activation's.
    const std::int64_t beats = k.field(k.incumbent, "beats");
    const std::int64_t fired = k.field(k.incumbent, "fired");
    const std::int64_t candidate_beats = k.field(candidate, "beats");
    k.r.pump_beats(100);
    CHECK(k.field(k.incumbent, "beats") == beats);
    CHECK(k.field(k.incumbent, "fired") == fired);
    CHECK(k.field(candidate, "beats") == candidate_beats + 100);

    // ...and the retired service's clock stopped with it, so the letter it wrote
    // described the boundary and not some later instant.
    CHECK(k.field(k.incumbent, "active") == 1); // its table is frozen too

    // EXACTLY ONE CHAIN, the successor's. Two would leave two parked beats.
    k.r.pump_beats(1);
    CHECK(k.r.bus.pending() == 1);
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

    // IT IS THE TIMER, AND IT WORKS. The bound is a count of its own beats, and
    // it spent exactly them before deciding the promise was not kept.
    CHECK(k.r.bus.role_holder(kTimerRole) == candidate);
    CHECK(k.field(candidate, "inherited") == 0);
    CHECK(k.field(candidate, "beats") == kPreparedClaimBeats);
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
    k.r.pump_beats(3);
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
    k.r.pump_beats(3);
    CHECK(k.prep.prepared == 1);
}

TEST_CASE("keystone: a forged letter cannot be inherited by a prepared candidate, cannot end its "
          "wait early, and cannot overwrite what it did inherit") {
    Keystone k;
    anchor_required_probe(k);
    k.advance_until_remaining(2000 + kPreparedCeremonyBeats * kBeatCapMs);
    const loom::WeaveId candidate = k.seal("zengine-timer-v2", TIMER_VIRTUAL_V2_SO);
    REQUIRE(k.begin(candidate).ok);

    // The impersonator of the R2B-1 lane, pointed at the prepared candidate. It
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
    k.r.pump_beats(3);
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
    k.advance_until_remaining(2000 + kPreparedCeremonyBeats * kBeatCapMs);
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
    CHECK(k.r.kernel.role_of("zengine-timer-v2") == kTimerRole);

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
    k.advance_until_remaining(kBeatCapMs);
    CHECK(k.r.ask_probe(k.probe).fires == 0);
    k.r.pump_beats(1);
    CHECK(k.r.ask_probe(k.probe).fires == 1);
    k.r.pump_beats(1);
    CHECK(k.r.bus.pending() == 1); // exactly one chain, still
}

// ---- the pilot: the same crossing, on the REAL monotonic clock --------------
//
// Everything above runs on virtual time so a claim about milliseconds is an
// exact integer. That is the right instrument for a semantic proof and the wrong
// one for the question this case asks, which is whether the whole thing works
// when the clock is the OS's and the nap is a real sleep: the shipped artifact,
// the real `timer.cpp` clock, real deliveries, and durations nobody controls.
//
// It is deliberately loose about numbers and strict about MEANING. A real clock
// cannot promise "exactly 200ms remained", so it does not pretend to — what it
// pins is that the schedule was PRESERVED rather than restarted, which the two
// answers put a whole declared delay apart.

TEST_CASE("keystone pilot: the SHIPPED real-clock Timer crosses a prepared replacement, and the "
          "schedule it hands over is measured in real milliseconds") {
    Keystone k(TIMER_SO, /*with_probe=*/false);

    // A 400ms one-shot, then roughly half of it spent on the real clock.
    k.r.ask_role(EnsureTimer{"pilot.oneshot", 400, false, kRestartDelay, ""});
    k.r.pump_for(k.incumbent, 20);
    REQUIRE_FALSE(k.r.seen.receipts.empty());
    CHECK(k.r.seen.receipts.back().id == "pilot.oneshot");
    CHECK(k.r.seen.receipts.back().resolved == kResolutionRestarted);
    k.r.pump_for(k.incumbent, 200);
    CHECK(k.r.seen.fired.size() ==
          std::count(k.r.seen.fired.begin(), k.r.seen.fired.end(), "suite.stopwatch"));

    // The same crossing, through the same coordinator: a second real artifact
    // from the same shipped library, sealed, prepared, and admitted.
    const loom::WeaveId candidate = k.seal("zengine-timer-real-v2", TIMER_SO);
    REQUIRE(k.begin(candidate).ok);
    k.ask(kInheritFromIncumbent);
    k.r.pump_until(TimerReady::zen_name);
    REQUIRE(k.prep.commits.size() == 1);
    REQUIRE(k.prep.commits.back().ok);
    REQUIRE(k.prep.relayed == 1);
    CHECK(k.r.bus.role_holder(kTimerRole) == candidate);

    // WHAT CROSSED IS A REAL MEASUREMENT: some of the 400ms is gone, and what is
    // left is neither zero nor the whole delay. A restarted schedule would read
    // 400; a stale snapshot taken at transaction begin would read more than what
    // the clock actually had left.
    const std::vector<TimerHandoffEntry> crossed = k.last_handoff();
    const TimerHandoffEntry* shot = k.crossed(crossed, "pilot.oneshot");
    REQUIRE(shot != nullptr);
    CHECK(shot->delay_ms == 400);
    CHECK(shot->remaining_ms > 0);
    CHECK(shot->remaining_ms < 400);

    // AND THE NEW SERVICE KEPT IT. An ordered re-ask preferring preservation is
    // answered `preserved_remaining` — the receipt, not an inference.
    k.r.ask_role(EnsureTimer{"pilot.oneshot", 400, false, kPreserveRemaining, ""});
    k.r.pump_for(candidate, 20);
    REQUIRE_FALSE(k.r.seen.receipts.empty());
    CHECK(k.r.seen.receipts.back().id == "pilot.oneshot");
    CHECK(k.r.seen.receipts.back().resolved == kResolutionPreserved);

    // ...and it fires ONCE, inside the time that was actually left, which a
    // schedule re-anchored at the commit would still be waiting out.
    k.r.pump_for(candidate, shot->remaining_ms + 100);
    CHECK(std::count(k.r.seen.fired.begin(), k.r.seen.fired.end(), "pilot.oneshot") == 1);
    k.r.pump_for(candidate, 100);
    CHECK(std::count(k.r.seen.fired.begin(), k.r.seen.fired.end(), "pilot.oneshot") == 1);
}
