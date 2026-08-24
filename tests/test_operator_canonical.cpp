// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// ONE LIVE OPERATOR TRUTH (CAT-0) — whether the Timer and a loaded stranger
// spend the SAME catalog INSTANCE, and not merely the same authoring.
//
// `test_operator.cpp` asks what an operator is. `test_operator_host.cpp` asks
// whether a loaded stranger can spend a host's catalog. Both were answered yes,
// and a process running the real Timer beside that stranger still had TWO live
// catalogs built from ONE authoring — the host's, and the one the shipped Timer
// carried inside its own image. Nothing disagreed, because nothing had been
// replaced yet. Every case here is about the moment something is.
//
// THE INSTRUMENT IS SEM-0's AND IT IS THE ONLY ONE THAT CAN ANSWER THIS.
// Agreement is indistinguishable from sharing until the shared thing CHANGES,
// so the witnesses replace `math.max` in the HOST — same identity, same ports,
// same types, therefore the same content ids, so nothing structural notices —
// and require the running Timer and the loaded stranger to move together, with
// neither artifact rebuilt or edited.
//
// AND THE TIMER IS OBSERVED THROUGH WHAT IT SCHEDULED, never through an
// accessor that says which mode it is in. `TimerHandoffEntry.delay_ms` is the
// STORED delay of a live entry, read off a real `zen.Bequest` through the real
// gate — the same read SEM-0 found and used, now across a module boundary.
//
// The tiers:
//
//   1  TOPOLOGY     one catalog, owned by the host arrangement, outliving the
//                   Kernel and everything the Kernel holds.
//   2  THE CHOICE   a Timer offered a host is host-backed for its whole life; a
//                   Timer offered nothing is a fallback Timer and is not warned
//                   at.
//   3  NO SILENT    a host that cannot serve the rule, and a host that serves it
//      FALLBACK     at another signature, are REFUSED at the deepest layer that
//                   knows — and neither reaches local semantics.
//   4  CANONICALITY replace a primitive in the host and BOTH the Timer and the
//                   stranger move; a fallback Timer does not.
//   5  NOT A MESSAGE a host-backed schedule costs the bus exactly what a local
//                   one does.
//   6  LIFECYCLE    the offer is withdrawn, unload and fresh load rebind, a
//                   bracketed reload keeps the binding, and two instances of one
//                   image each get their own.

#include "doctest.h"

#include "operator/catalog.hpp"
#include "operator/host.hpp"
#include "operator/host_surface.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "operator_ask.hpp"
#include "operator_fixture.hpp"
#include "timer/normalize.hpp"
#include "timer/vocabulary.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace op = zengine::op;
namespace tmr = zengine::timer;

using zengine::testing::OperatorArgument;
using zengine::testing::OperatorEvaluateAsk;
using zengine::testing::OperatorReadingSaid;

namespace {

// ---- the two dishonest hosts ------------------------------------------------

/// A vocabulary with the primitives and NO rule composed over them. A perfectly
/// well-formed catalog that simply does not publish what a Timer needs.
op::Catalog primitives_only() {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    return catalog;
}

/// Namespace scope, because a block-scope lambda cannot be a
/// `make_operator<&F>` argument at all.
std::int64_t halve(std::int64_t delay_ms) { return delay_ms / 2; }

/// A host that publishes `timer.normalize_delay` at ANOTHER SIGNATURE — one Int
/// in, one Int out, under the identity and the port names a Timer would look
/// for. This is the dangerous one: the identity resolves, `describe` succeeds,
/// and only the SHAPE is wrong. A Timer that checked for presence alone would
/// take it and then fail on its first schedule.
op::Catalog wrong_signature() {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    catalog.publish(op::make_operator<&halve>(tmr::kNormalizeDelay, {tmr::kAuthoredDelayPort},
                                              tmr::kEffectiveDelayPort));
    return catalog;
}

// ---- the one weave every case drives ----------------------------------------

struct WitnessState {
    std::int64_t noted = 0;
    ZEN_SHAPE(WitnessState, 1, ZEN_FIELD(noted));
};

/// What this host has heard. The Timer's answers and the stranger's, side by
/// side, because the phase's claim is about the two of them together.
struct Heard {
    std::vector<std::string> results;   ///< zen.Result payloads (a load's WeaveId)
    std::vector<std::string> refusals;  ///< zen.Refused reasons, verbatim
    std::vector<tmr::TimerHandoffEntry> entries; ///< the last letter's table
    std::int64_t letters = 0;
    std::int64_t ready = 0;        ///< TimerReady: the service has finished bootstrapping
    std::vector<OperatorReadingSaid> readings;
};

/// The host's hand and ears in one weave: it commands the Manager and the
/// control door, asks the Timer for a schedule and for a description of itself,
/// asks the stranger to spend an operator, and hears every answer.
class Witness
    : public loom::WeaveBase<
          Witness, WitnessState,
          loom::Accept<loom::Result, loom::Ack, loom::Refused, loom::Bequest,
                       tmr::TimerFired, tmr::TimerReady, tmr::TimerResolution,
                       zengine::testing::OperatorSignatureSaid, OperatorReadingSaid>,
          loom::Emit<loom::LoadWeave, loom::ReloadWeave, loom::UnloadLibrary, tmr::StartTimer,
                     tmr::EnsureTimer, loom::PrepareShutdown,
                     zengine::testing::OperatorDescribeAsk, OperatorEvaluateAsk>> {
public:
    explicit Witness(Heard& heard) : heard_(&heard) {}

    void on(const loom::Result& r, loom::Mail&) { heard_->results.push_back(r.value); }
    void on(const loom::Ack&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { heard_->refusals.push_back(r.reason); }
    void on(const tmr::TimerFired&, loom::Mail&) {}
    void on(const tmr::TimerReady&, loom::Mail&) { ++heard_->ready; }
    void on(const tmr::TimerResolution&, loom::Mail&) {}
    void on(const zengine::testing::OperatorSignatureSaid&, loom::Mail&) {}
    void on(const OperatorReadingSaid& r, loom::Mail&) { heard_->readings.push_back(r); }

    /// THE READ THIS WHOLE SUITE TURNS ON. Asking a Timer to describe itself
    /// changes nothing it describes (TIMER-03), and the entry it describes
    /// carries the delay it STORED — which is what the normalization answered.
    /// So this is a direct reading of the transform's result, on the wire,
    /// through the real gate, out of another image.
    void on(const loom::Bequest& letter, loom::Mail&) {
        ++heard_->letters;
        for (const loom::Bytes& item : letter.items) {
            if (const std::optional<tmr::TimerHandoff> h =
                    loom::claim_item<tmr::TimerHandoff>(item)) {
                heard_->entries = h->entries;
            }
        }
    }

private:
    Heard* heard_;
};

// ---- the rig ----------------------------------------------------------------

/// A PRODUCTION-SHAPED HOST: one operator catalog, one surface over it, a real
/// Kernel, the real Weave Manager, and nothing test-only in the load path.
///
/// THE MEMBER ORDER IS THE LIFETIME CLAIM, and it is the same order
/// `workshop.cpp` writes. `catalog` and `operators` are declared before
/// `kernel`, so destruction — which runs in reverse — takes the Kernel down
/// first and the Kernel destroys every artifact it holds before the surface
/// those artifacts point at goes anywhere.
struct CanonRig {
    loom::Switchboard bus;
    op::Catalog catalog;
    op::OperatorHostSurface operators{catalog};
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);

    Heard heard;
    loom::WeaveId witness{};
    std::int64_t deliveries = 0;
    /// Deliveries that are not the Timer's own heartbeat. The beat runs on a
    /// real clock and nothing here should have to be right about how many of it
    /// happened; what a bus-turn witness needs is the traffic the ASKS caused.
    std::int64_t traffic = 0;

    /// How the last offer ended, so a case can read the offer's verdict beside
    /// the load's.
    op::OfferOutcome offer = op::OfferOutcome::NotAConsumer;

    explicit CanonRig(op::Catalog operators_in = tmr::fallback_vocabulary())
        : catalog(std::move(operators_in)) {
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow(loom::ReloadWeave::zen_name, loom::ReloadWeave::zen_version, manager);
        reach.allow(loom::UnloadLibrary::zen_name, loom::UnloadLibrary::zen_version, control);
        reach.allow_to_any(tmr::StartTimer::zen_name, tmr::StartTimer::zen_version);
        reach.allow_to_any(tmr::EnsureTimer::zen_name, tmr::EnsureTimer::zen_version);
        reach.allow_to_any(loom::PrepareShutdown::zen_name, loom::PrepareShutdown::zen_version);
        reach.allow_to_any(zengine::testing::OperatorDescribeAsk::zen_name,
                           zengine::testing::OperatorDescribeAsk::zen_version);
        reach.allow_to_any(OperatorEvaluateAsk::zen_name, OperatorEvaluateAsk::zen_version);
        witness = loom::mount_granted<Witness>(bus, std::move(reach), heard);
        bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered) {
                ++deliveries;
                if (ev.schema_name != tmr::Drive::zen_name &&
                    ev.schema_name != tmr::TimerFired::zen_name) {
                    ++traffic;
                }
            }
        });
    }

    /// DISPATCH IN BOUNDED TURNS, never to empty. A live Timer re-arms its own
    /// beat inside its own handler, so `pump()` would never return; this is the
    /// same `pump_pending()` loop `workshop.cpp`'s boot uses, for the same
    /// reason. The turn budget is a hang guard, not a schedule.
    template <class Pred>
    void drain_until(Pred done, int turns = 40) {
        for (int i = 0; i < turns && !done(); ++i) {
            if (bus.pump_pending() == 0) {
                return;
            }
        }
    }

    void drain(int turns = 8) {
        drain_until([] { return false; }, turns);
    }

    /// LOAD THE WAY A REAL ZENGINE HOST DOES, with the offer bracketing the whole
    /// thing — the command is sent before anything is pumped, because `create()`
    /// runs several deliveries deep and no host can get between the Kernel and a
    /// constructor.
    loom::WeaveId load(const char* name, const char* path, const char* role, bool with_offer) {
        const std::size_t ok_before = heard.results.size();
        const std::size_t no_before = heard.refusals.size();
        // ANSWERED, NOT MERELY LOADED. `is_loaded` turns true the instant
        // `Kernel::load` returns, while the `zen.Result` naming the new WeaveId
        // is still queued -- so the drain waits for the ANSWER, which is the
        // fact this function returns. A refused load answers too, and stopping
        // on either is what keeps the negative cases from pumping out their
        // whole budget.
        const auto answered = [&] {
            return heard.results.size() > ok_before || heard.refusals.size() > no_before;
        };
        if (with_offer) {
            op::OperatorOffer offering(operators, path);
            offer = offering.outcome();
            send_load(name, path, role);
            drain_until(answered);
        } else {
            offer = op::OfferOutcome::NotAConsumer;
            send_load(name, path, role);
            drain_until(answered);
        }
        if (heard.results.size() <= ok_before) {
            return loom::WeaveId{};
        }
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(heard.results.back()))};
    }

    /// THE SUPPORTED HOT RELOAD, bracketed exactly as the load is. `reload_from`
    /// builds a NEW instance with `create()`, so an operator-aware host owes the
    /// replacement the same offer it owed the original — and OPH-0's scoped
    /// offer is the whole mechanism, unchanged.
    bool reload(const char* name, const char* path, bool with_offer) {
        const std::size_t refused_before = heard.refusals.size();
        if (with_offer) {
            op::OperatorOffer offering(operators, path);
            offer = offering.outcome();
            send_reload(name, path);
            drain(12);
        } else {
            offer = op::OfferOutcome::NotAConsumer;
            send_reload(name, path);
            drain(12);
        }
        return heard.refusals.size() == refused_before;
    }

    bool unload(const char* name) {
        const std::size_t before = heard.refusals.size();
        bus.send_as(witness, control,
                    loom::Message(loom::to_value(loom::UnloadLibrary{name}), witness, witness, 0));
        drain(6);
        return heard.refusals.size() == before;
    }

    /// Schedule one timer on a live service and read back the delay it STORED.
    ///
    /// The letter is the ruler: `zen.PrepareShutdown` asks the Timer to describe
    /// itself and changes nothing it describes, and every entry in the answer
    /// carries the delay the normalization produced.
    std::int64_t scheduled_delay(loom::WeaveId service, const char* id, std::int64_t delay_ms,
                                 bool repeat) {
        bus.send_as(witness, service,
                    loom::Message(loom::to_value(tmr::StartTimer{id, delay_ms, repeat}), witness,
                                  witness, 0));
        drain(6);
        const std::int64_t letters_before = heard.letters;
        heard.entries.clear();
        bus.send_as(witness, service,
                    loom::Message(loom::to_value(loom::PrepareShutdown{}), witness, witness, 0));
        drain_until([&] { return heard.letters > letters_before; });
        REQUIRE(heard.letters > letters_before);
        for (const tmr::TimerHandoffEntry& e : heard.entries) {
            if (e.id == id) {
                return e.delay_ms;
            }
        }
        return -424242; // a value the rule cannot produce, so a miss cannot pass
    }

    /// Ask the loaded stranger what the host says `timer.normalize_delay` makes
    /// of the same two arguments. It shares no line of code with the Timer.
    std::string stranger_says(loom::WeaveId stranger, const char* delay, const char* repeat) {
        const std::size_t before = heard.readings.size();
        bus.send_as(witness, stranger,
                    loom::Message(loom::to_value(OperatorEvaluateAsk{
                                      std::string(tmr::kNormalizeDelay),
                                      {OperatorArgument{std::string(tmr::kAuthoredDelayPort),
                                                        delay},
                                       OperatorArgument{std::string(tmr::kRepeatPort), repeat}},
                                      1}),
                                  witness, witness, 0));
        drain_until([&] { return heard.readings.size() > before; });
        REQUIRE(heard.readings.size() > before);
        return heard.readings.back().answer;
    }

private:
    void send_load(const char* name, const char* path, const char* role) {
        bus.send_as(witness, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{name, path, role}), witness,
                                  witness, 0));
    }
    void send_reload(const char* name, const char* path) {
        bus.send_as(witness, manager,
                    loom::Message(loom::to_value(loom::ReloadWeave{name, path}), witness, witness,
                                  0));
    }
};

/// The one delay every canonicality case asks about, and what the two vocabularies
/// make of it. `max(max(-500,0),1)` is 1; with `math.max` replaced by a min,
/// `min(min(-500,0),1)` is -500.
constexpr std::int64_t kAuthoredDelay = -500;
constexpr std::int64_t kHonestAnswer = 1;
constexpr std::int64_t kSubstitutedAnswer = zengine::testing::kSabotagedRepeatingDelay;

} // namespace

// ---- 1. topology ------------------------------------------------------------

TEST_CASE("the host arrangement owns ONE catalog, and it is the package's own authoring") {
    CanonRig r;

    // The vocabulary a host publishes is `timer::fallback_vocabulary()` and not a
    // second definition written in a host: three identities, the two primitives
    // and the composition over them.
    const std::vector<std::string> published = r.catalog.identities();
    CHECK(published.size() == 3);
    CHECK(r.catalog.find(op::kMaxInt) != nullptr);
    CHECK(r.catalog.find(op::kSelectInt) != nullptr);
    CHECK(r.catalog.find(tmr::kNormalizeDelay) != nullptr);

    // ...and the surface reads THAT object rather than a copy of it. There is no
    // snapshot on this seam: `catalog()` is the host's own.
    CHECK(&r.operators.catalog() == &r.catalog);
}

TEST_CASE("a host-backed Timer and a loaded stranger resolve through the SAME instance") {
    // The topology claim, stated as an address rather than as an answer: both
    // consumers were offered the same `OperatorHostSurface`, whose `ctx` is one
    // object, and that object borrows one catalog.
    CanonRig r;
    const loom::WeaveId timer = r.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    REQUIRE(timer.value != 0);
    CHECK(r.offer == op::OfferOutcome::Offered);

    const loom::WeaveId stranger = r.load("stranger", OPH_STRANGER_SO, "", true);
    REQUIRE(stranger.value != 0);
    CHECK(r.offer == op::OfferOutcome::Offered);

    // Two images, two offers, one table, one catalog — and the same answer,
    // which is necessary but nowhere near sufficient. Tier 4 is what makes it
    // mean something.
    CHECK(r.scheduled_delay(timer, "beat", kAuthoredDelay, true) == kHonestAnswer);
    CHECK(r.stranger_says(stranger, "-500", "true") == "1");
}

// ---- 2. the choice ----------------------------------------------------------

TEST_CASE("a Timer offered nothing is a fallback Timer, and that is a supported arrangement") {
    // Every host that predates this seam, `snake` included. The Timer loads,
    // schedules, and answers exactly what it answered before CAT-0 existed —
    // with no offer, no diagnostic, and no warning about a host it never met.
    CanonRig r;
    const loom::WeaveId timer =
        r.load("zengine-timer", TIMER_SO, tmr::kTimerRole, /*with_offer=*/false);
    REQUIRE(timer.value != 0);
    CHECK(r.heard.refusals.empty());
    CHECK(r.scheduled_delay(timer, "beat", kAuthoredDelay, true) == kHonestAnswer);
}

TEST_CASE("the authority is chosen at construction and is fixed for the instance's life") {
    // The in-process half of the same claim, where both states can be built
    // directly. A `DelayAuthority` has no setter, no rebind and no second door:
    // what it is, it was made.
    op::Catalog catalog = tmr::fallback_vocabulary();
    op::OperatorHostSurface surface(catalog);

    const tmr::DelayAuthority backed{op::OperatorHost::over(surface.api())};
    CHECK(backed.host_backed());
    CHECK(backed.effective_delay(kAuthoredDelay, true) == kHonestAnswer);
    // ...and it carries no catalog of its own to fall back to.
    CHECK_THROWS_AS(backed.operators(), std::invalid_argument);

    const tmr::DelayAuthority local{op::OperatorHost()};
    CHECK_FALSE(local.host_backed());
    CHECK(local.effective_delay(kAuthoredDelay, true) == kHonestAnswer);
    CHECK(local.operators().size() == 3);

    const tmr::DelayAuthority defaulted;
    CHECK_FALSE(defaulted.host_backed());
}

// ---- 3. no silent fallback --------------------------------------------------

TEST_CASE("a host that publishes no delay rule does NOT get a quietly local Timer") {
    // THE NEGATIVE WITNESS THIS PHASE EXISTS FOR. A host supplied an operator
    // surface and that surface cannot serve `timer.normalize_delay`. The one
    // thing that must not happen is the Timer shrugging and scheduling by its
    // own copy while the host believes it owns the rule.
    //
    // (The Timer's own sentence goes to stderr on the way past — see
    // timer/timer.cpp. It is expected output for this case, not a fault.)
    CanonRig r{primitives_only()};
    const loom::WeaveId timer = r.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);

    CHECK(r.offer == op::OfferOutcome::Offered); // the handoff itself was fine
    CHECK(timer.value == 0);                     // ...and the Timer refused to exist
    CHECK_FALSE(r.kernel.is_loaded("zengine-timer"));
    REQUIRE_FALSE(r.heard.refusals.empty());

    // THE REFUSAL'S OWNER IS THE KERNEL'S LOAD, in the Kernel's own words: a
    // constructor that throws is `create()` returning null, and a weave that
    // could not be constructed is not a participant. Nothing partial was
    // registered and no role was bound.
    CHECK(r.heard.refusals.back().find("create() returned null") != std::string::npos);
    CHECK(r.kernel.role_of("zengine-timer").empty());
}

TEST_CASE("a host that publishes the rule at another SIGNATURE is refused the same way") {
    // The sharper one. The identity resolves and `describe` succeeds — only the
    // shape is wrong, which is exactly the failure a presence check would sail
    // through and then hit on the first schedule.
    CanonRig r{wrong_signature()};
    REQUIRE(r.catalog.find(tmr::kNormalizeDelay) != nullptr);

    const loom::WeaveId timer = r.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    CHECK(r.offer == op::OfferOutcome::Offered);
    CHECK(timer.value == 0);
    CHECK_FALSE(r.kernel.is_loaded("zengine-timer"));
    REQUIRE_FALSE(r.heard.refusals.empty());
    CHECK(r.heard.refusals.back().find("create() returned null") != std::string::npos);
}

TEST_CASE("neither dishonest host produced a Timer that answers by local arithmetic") {
    // Said as its own case because it is the claim, and the two above only
    // establish half of it: not merely that the load failed, but that NOTHING
    // in this process is scheduling delays out of a private copy of the rule.
    // There is no Timer at all, which is the only honest outcome.
    for (op::Catalog dishonest : {primitives_only(), wrong_signature()}) {
        CanonRig r{std::move(dishonest)};
        r.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
        CHECK(r.kernel.loaded().empty());
        CHECK(r.bus.role_holder(tmr::kTimerRole).value == 0);
    }
}

// ---- 4. the canonicality canary --------------------------------------------

TEST_CASE("replace a primitive in the HOST and the host-backed TIMER moves with it") {
    // THE PHASE'S DECISIVE WITNESS, and the half OPH-0 could only show for a
    // stranger. `math.max` becomes a min underneath the rule; the Timer's own
    // artifact is byte-for-byte the one the honest rig loaded; and what the
    // running service SCHEDULED changes.
    //
    // A Timer holding a private catalog — which is exactly what shipped before
    // this phase — would still schedule 1.
    CanonRig honest;
    CanonRig substituted{zengine::testing::sabotaged_operators()};

    const loom::WeaveId a = honest.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    const loom::WeaveId b = substituted.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    REQUIRE(a.value != 0);
    REQUIRE(b.value != 0);

    CHECK(honest.scheduled_delay(a, "beat", kAuthoredDelay, true) == kHonestAnswer);
    CHECK(substituted.scheduled_delay(b, "beat", kAuthoredDelay, true) == kSubstitutedAnswer);
}

TEST_CASE("...and the loaded stranger moves with it, in the same process, together") {
    // The two halves in ONE arrangement, which is the thing neither SEM-0 nor
    // OPH-0 could state: one catalog, two consumer images that share no code
    // and never heard of each other, and a substitution in the host that moves
    // both. Neither artifact was rebuilt between the two rigs.
    CanonRig honest;
    CanonRig substituted{zengine::testing::sabotaged_operators()};

    const loom::WeaveId timer_a = honest.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    const loom::WeaveId strange_a = honest.load("stranger", OPH_STRANGER_SO, "", true);
    const loom::WeaveId timer_b =
        substituted.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    const loom::WeaveId strange_b = substituted.load("stranger", OPH_STRANGER_SO, "", true);
    REQUIRE(timer_a.value != 0);
    REQUIRE(strange_a.value != 0);
    REQUIRE(timer_b.value != 0);
    REQUIRE(strange_b.value != 0);

    CHECK(honest.scheduled_delay(timer_a, "beat", kAuthoredDelay, true) == kHonestAnswer);
    CHECK(honest.stranger_says(strange_a, "-500", "true") == "1");

    CHECK(substituted.scheduled_delay(timer_b, "beat", kAuthoredDelay, true) ==
          kSubstitutedAnswer);
    CHECK(substituted.stranger_says(strange_b, "-500", "true") == "-500");
}

TEST_CASE("a FALLBACK Timer does not move when only the host's catalog is substituted") {
    // The third fact, and the one that makes the other two mean what they say.
    // If a fallback Timer moved too, the witness above would be measuring
    // something else entirely — a global, an interposed symbol, a shared static.
    // It does not move, because nothing was offered to it and its truth is its
    // own image's.
    CanonRig substituted{zengine::testing::sabotaged_operators()};
    const loom::WeaveId timer =
        substituted.load("zengine-timer", TIMER_SO, tmr::kTimerRole, /*with_offer=*/false);
    REQUIRE(timer.value != 0);
    CHECK(substituted.scheduled_delay(timer, "beat", kAuthoredDelay, true) == kHonestAnswer);

    // ...while a stranger in the SAME process, offered the same substituted
    // host, does move. One rig, two artifacts, two authorities, no ambiguity
    // about which catalog either of them read.
    const loom::WeaveId stranger = substituted.load("stranger", OPH_STRANGER_SO, "", true);
    REQUIRE(stranger.value != 0);
    CHECK(substituted.stranger_says(stranger, "-500", "true") == "-500");
}

TEST_CASE("the substitution is invisible to every structural check the seam makes") {
    // Which is what makes it a test of SHARING rather than a test of the
    // signature check: same identity, same ports, same types, same content ids.
    // A Timer built against the honest vocabulary accepts the substituted host
    // without noticing anything, and then answers differently.
    const op::Catalog honest = tmr::fallback_vocabulary();
    const op::Catalog substituted = zengine::testing::sabotaged_operators();
    const op::OperatorDef* h = honest.find(tmr::kNormalizeDelay);
    const op::OperatorDef* s = substituted.find(tmr::kNormalizeDelay);
    REQUIRE(h != nullptr);
    REQUIRE(s != nullptr);
    CHECK(loom::same_identity(*h->inputs(), *s->inputs()));
    CHECK(loom::same_identity(*h->outputs(), *s->outputs()));
}

// ---- 5. not a message -------------------------------------------------------

namespace {

/// EIGHT SCHEDULES, READ BACK, AND WHAT THE BUS CARRIED FOR THEM.
///
/// Counted with the heartbeat excluded, because the beat runs on a real clock
/// and is not what is under test. What IS under test is the traffic the asks
/// caused: eight `StartTimer`s, one `zen.PrepareShutdown`, one `zen.Bequest`.
/// Ten, whichever authority did the normalizing -- and a Timer that reached its
/// host by MESSAGE, the repair OPH-0 rejected, could not produce ten.
std::int64_t traffic_of_eight(CanonRig& r, loom::WeaveId service) {
    // Let the BOOTSTRAP finish first. A freshly loaded service asks the steward
    // for a letter, is refused, and announces itself -- three deliveries that
    // belong to becoming a Timer rather than to scheduling anything, and
    // counting them would be measuring the load.
    r.drain_until([&] { return r.heard.ready > 0; });
    REQUIRE(r.heard.ready > 0);
    const std::int64_t before = r.traffic;
    for (int i = 0; i < 8; ++i) {
        r.bus.send_as(r.witness, service,
                      loom::Message(loom::to_value(tmr::StartTimer{"beat" + std::to_string(i),
                                                                   kAuthoredDelay, true}),
                                    r.witness, r.witness, 0));
    }
    // The letter is what proves the eight really were normalized rather than
    // dropped: it carries their STORED delays, so this count is the cost of work
    // that demonstrably happened.
    const std::int64_t letters_before = r.heard.letters;
    r.heard.entries.clear();
    r.bus.send_as(r.witness, service,
                  loom::Message(loom::to_value(loom::PrepareShutdown{}), r.witness, r.witness, 0));
    r.drain_until([&] { return r.heard.letters > letters_before; });
    REQUIRE(r.heard.entries.size() == 8);
    for (const tmr::TimerHandoffEntry& e : r.heard.entries) {
        REQUIRE(e.delay_ms == kHonestAnswer);
    }
    return r.traffic - before;
}

} // namespace

TEST_CASE("a host-backed schedule creates no bus traffic of its own") {
    // Evaluation across the operator seam is synchronous computation, not
    // conversation: it happens inside one delivery and enqueues nothing. If a
    // host-backed Timer had reached its host by message, these two numbers would
    // differ by at least one round trip per schedule.
    CanonRig backed;
    CanonRig local;
    const loom::WeaveId a = backed.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    const loom::WeaveId b = local.load("zengine-timer", TIMER_SO, tmr::kTimerRole, false);
    REQUIRE(a.value != 0);
    REQUIRE(b.value != 0);

    const std::int64_t backed_cost = traffic_of_eight(backed, a);
    const std::int64_t local_cost = traffic_of_eight(local, b);

    CHECK(backed_cost == local_cost);
    CHECK(backed_cost == 10); // eight asks, one PrepareShutdown, one Bequest
}

// ---- 6. lifecycle -----------------------------------------------------------

TEST_CASE("the offer is withdrawn: a Timer loaded after one is UNBOUND") {
    // OPH-0's law, re-proved on the artifact CAT-0 added to the seam. If the
    // module slot in the Timer's image still held the previous table, this
    // second instance would pick it up silently and be host-backed by accident.
    CanonRig substituted{zengine::testing::sabotaged_operators()};
    const loom::WeaveId first =
        substituted.load("timer-offered", TIMER_SO, tmr::kTimerRole, /*with_offer=*/true);
    REQUIRE(first.value != 0);
    CHECK(substituted.scheduled_delay(first, "beat", kAuthoredDelay, true) == kSubstitutedAnswer);

    const loom::WeaveId second =
        substituted.load("timer-unoffered", TIMER_SO, "", /*with_offer=*/false);
    REQUIRE(second.value != 0);
    CHECK(substituted.scheduled_delay(second, "beat", kAuthoredDelay, true) == kHonestAnswer);
}

TEST_CASE("two host-backed instances of one image spend the SAME host catalog") {
    // Two loads of one artifact under two names: one image, two instances, two
    // separate offers. Both move with the host, which is what "the offer is per
    // instance" has to mean in the presence of more than one.
    CanonRig substituted{zengine::testing::sabotaged_operators()};
    const loom::WeaveId a = substituted.load("timer-a", TIMER_SO, tmr::kTimerRole, true);
    const loom::WeaveId b = substituted.load("timer-b", TIMER_SO, "", true);
    REQUIRE(a.value != 0);
    REQUIRE(b.value != 0);
    CHECK(a.value != b.value);

    CHECK(substituted.scheduled_delay(a, "beat", kAuthoredDelay, true) == kSubstitutedAnswer);
    CHECK(substituted.scheduled_delay(b, "beat", kAuthoredDelay, true) == kSubstitutedAnswer);
}

TEST_CASE("unload and a fresh load rebind: the replacement is host-backed again") {
    CanonRig substituted{zengine::testing::sabotaged_operators()};
    const loom::WeaveId first = substituted.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    REQUIRE(first.value != 0);
    CHECK(substituted.scheduled_delay(first, "beat", kAuthoredDelay, true) == kSubstitutedAnswer);

    REQUIRE(substituted.unload("zengine-timer"));
    CHECK_FALSE(substituted.kernel.is_loaded("zengine-timer"));

    const loom::WeaveId second = substituted.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    REQUIRE(second.value != 0);
    CHECK(second.value != first.value);
    CHECK(substituted.scheduled_delay(second, "beat", kAuthoredDelay, true) == kSubstitutedAnswer);
}

TEST_CASE("a hot reload BRACKETED by an offer keeps the host binding") {
    // OPH-0 covered unload + fresh load and explicitly did not cover
    // `Kernel::reload_from`, which is the OTHER `create()` site in the Kernel.
    // It needs no new mechanism: an operator-aware host owes a replacement
    // instance the same offer it owed the original, and `OperatorOffer` is
    // already exactly that object.
    CanonRig substituted{zengine::testing::sabotaged_operators()};
    const loom::WeaveId before = substituted.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    REQUIRE(before.value != 0);
    CHECK(substituted.scheduled_delay(before, "beat", kAuthoredDelay, true) == kSubstitutedAnswer);

    REQUIRE(substituted.reload("zengine-timer", TIMER_SO, /*with_offer=*/true));
    CHECK(substituted.offer == op::OfferOutcome::Offered);
    // A reload PRESERVES the logical WeaveId: same participant, new code.
    CHECK(substituted.kernel.weave_id("zengine-timer").value == before.value);
    CHECK(substituted.scheduled_delay(before, "beat2", kAuthoredDelay, true) ==
          kSubstitutedAnswer);
}

TEST_CASE("...and an UNBRACKETED reload is the host's own error, stated rather than smoothed") {
    // The negative, pinned so nobody has to discover it. A host that reloads an
    // operator-consuming artifact without an offer gets a replacement instance
    // that was offered nothing — which is a fallback Timer, by exactly the same
    // rule that makes an unoffered LOAD one.
    //
    // This is not a path any host in this repository can take: Workshop is the
    // only host that supplies operator truth, and the only lifecycle command any
    // weave in it may send is `zen.LoadWeave`. What closes the gap in general is
    // a Kernel that can be told an artifact must always be offered something,
    // which is LOAD-0-shaped and deliberately not built here.
    CanonRig substituted{zengine::testing::sabotaged_operators()};
    const loom::WeaveId before = substituted.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
    REQUIRE(before.value != 0);
    CHECK(substituted.scheduled_delay(before, "beat", kAuthoredDelay, true) == kSubstitutedAnswer);

    REQUIRE(substituted.reload("zengine-timer", TIMER_SO, /*with_offer=*/false));
    CHECK(substituted.scheduled_delay(before, "beat2", kAuthoredDelay, true) == kHonestAnswer);
}

// ---- 7. the production host, read as a source file --------------------------
//
// DEFENCE IN DEPTH, AND SAID TO BE. Every case above drives a rig that is shaped
// like `workshop.cpp` rather than `workshop.cpp` itself, because Workshop's
// `main()` claims a terminal and this suite cannot run one. So the arrangement
// the product actually ships is read off the source, exactly as the
// no-privileged-wind and clock-binding tripwires already are (R2A-2, R2A-3):
// this is not a proof that the host is canonical, it is a guard against the
// claim quietly becoming false while every rig here stays green.

namespace {

std::string host_source() {
    std::ifstream in(WORKSHOP_HOST_CPP);
    REQUIRE_MESSAGE(in.good(), "cannot read the host source at ", WORKSHOP_HOST_CPP);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

} // namespace

TEST_CASE("the production host owns ONE catalog, and owns it for longer than the Kernel") {
    const std::string host = host_source();

    // ONE CATALOG, and only one: CAT-0's claim, unchanged. WHERE ITS CONTENTS COME
    // FROM stopped being this case's business at PROV-0 -- the host authors nothing
    // now and `test_operator_provider.cpp` owns that tripwire -- but that there is a
    // single object, declared here, outliving the Kernel, is still this phase's.
    const std::size_t catalog = host.find("op::Catalog operators;");
    const std::size_t surface = host.find("op::OperatorHostSurface operator_host(operators)");
    const std::size_t kernel = host.find("loom::Kernel kernel(bus)");
    REQUIRE(catalog != std::string::npos);
    REQUIRE(surface != std::string::npos);
    REQUIRE(kernel != std::string::npos);
    CHECK(host.find("op::Catalog", catalog + 1) == std::string::npos);

    // THE LIFETIME CLAIM IS AN ORDER OF DECLARATION and nothing else, so this is
    // the one place it can be read. Destruction runs in reverse: the Kernel goes
    // first, taking every artifact it holds with it, while the surface those
    // artifacts point at is still alive.
    CHECK(catalog < surface);
    CHECK(surface < kernel);

    // ...and the ONE surface over it is handed to the thing that performs the plan,
    // which is what makes the shipped Timer host-backed in the shipped host.
    //
    // ⚠ LOAD-0 MOVED THE OTHER HALF OF THIS CASE. It used to read the OFFER and the
    // Timer's boot off this file and check their order; `workshop.cpp` now contains
    // neither, because it names no artifact at all. The law is unchanged and its
    // tripwire followed the code -- `test_operator_provider.cpp`'s tier 10 reads
    // `load_execute.hpp`, where the mount/offer/load order now lives.
    const std::size_t executor = host.find("load::PlanExecutor executor(bus, operators, "
                                           "operator_host,");
    REQUIRE(executor != std::string::npos);
    CHECK(kernel < executor);
    CHECK(host.find("op::OperatorOffer") == std::string::npos);
}

TEST_CASE("the host and its catalog outlive every Timer that was offered them") {
    // The lifetime the host promises, exercised rather than asserted: the rig's
    // members are declared catalog-first, so this scope's destruction runs the
    // Kernel's destructor — and every loaded artifact's — while the surface
    // those artifacts hold a table into is still alive. Under ASan a reversed
    // declaration order is a use-after-free here.
    {
        CanonRig substituted{zengine::testing::sabotaged_operators()};
        const loom::WeaveId timer =
            substituted.load("zengine-timer", TIMER_SO, tmr::kTimerRole, true);
        REQUIRE(timer.value != 0);
        const loom::WeaveId stranger = substituted.load("stranger", OPH_STRANGER_SO, "", true);
        REQUIRE(stranger.value != 0);
        CHECK(substituted.scheduled_delay(timer, "beat", kAuthoredDelay, true) ==
              kSubstitutedAnswer);
    }
    CHECK(true); // reached, with nothing dangling on the way out
}
