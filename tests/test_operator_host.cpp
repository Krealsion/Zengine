// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE OPERATOR HOST SEAM (OPH-0) — whether a consumer in ANOTHER IMAGE can spend
// the host's operator truth.
//
// `test_operator.cpp` next door asks what an operator IS, and its stranger is an
// independent translation unit in the same binary handed a `const op::Catalog&`.
// Every case here is about the thing that reference cannot cross: a real `.so` /
// `.dll` opened by the real Kernel, driven through the real Weave Manager by an
// ordinary `zen.LoadWeave`, whose weave was constructed by `create(void)` with
// nothing and which has no `op::Catalog` in its address space to fall back on.
//
// The tiers:
//
//   1  OPTIONAL      an ordinary weave meets the offer path and is untouched by
//                    it; a version this host does not speak is REFUSED, not
//                    guessed at.
//   2  THE HANDOFF   the offer reaches the instance inside `create()`, the
//                    consumer describes the rule off the host's own definition,
//                    and spends it.
//   3  NOT A MESSAGE sixteen evaluations cost the same bus turns one does, and
//                    the primitives ran in the HOST.
//   4  LIFECYCLE     unload leaves no usable binding, reload gets a fresh one,
//                    and nothing outlives the host.
//   5  THE FENCE     change the host's operator truth and the loaded consumer
//                    moves — with no edit to the consumer, and no way for it to
//                    have answered from a private copy.
//   6  REFUSALS      five failures, five answers.

#include "doctest.h"

#include "operator/catalog.hpp"
#include "operator/host.hpp"
#include "operator/host_surface.hpp"
#include "operator/primitives.hpp"
#include "operator_ask.hpp"
#include "operator_fixture.hpp"
#include "timer/normalize.hpp"

#include <zen/kernel/control.hpp>
#include <zen/kernel/kernel.hpp>
#include <zen/kernel/manager.hpp>
#include <zen/switchboard/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace op = zengine::op;
namespace tmr = zengine::timer;

using zengine::testing::OperatorArgument;
using zengine::testing::OperatorDescribeAsk;
using zengine::testing::OperatorEvaluateAsk;
using zengine::testing::OperatorReadingSaid;
using zengine::testing::OperatorSignatureSaid;

namespace {

// ---- the two native weaves a case drives ----------------------------------

struct TallyState {
    std::int64_t n = 0;
    ZEN_SHAPE(TallyState, 1, ZEN_FIELD(n));
};

/// Commands the Weave Manager and HEARS its answers, so a refused load cannot
/// look like one that worked.
class Booter : public loom::WeaveBase<Booter, TallyState,
                                      loom::Accept<loom::Result, loom::Ack, loom::Refused>,
                                      loom::Emit<loom::LoadWeave, loom::UnloadLibrary>> {
public:
    Booter(std::vector<std::string>& ok, std::vector<std::string>& no) : ok_(&ok), no_(&no) {}
    void on(const loom::Result& r, loom::Mail&) { ok_->push_back(r.value); }
    void on(const loom::Ack&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { no_->push_back(r.reason); }

private:
    std::vector<std::string>* ok_;
    std::vector<std::string>* no_;
};

/// What the stranger said back. The asks themselves go out through
/// `bus.send_as`, so this weave only ever listens.
struct Heard {
    std::vector<OperatorSignatureSaid> signatures;
    std::vector<OperatorReadingSaid> readings;
};

class Asker : public loom::WeaveBase<Asker, TallyState,
                                     loom::Accept<OperatorSignatureSaid, OperatorReadingSaid>,
                                     loom::Emit<OperatorDescribeAsk, OperatorEvaluateAsk>> {
public:
    explicit Asker(Heard& heard) : heard_(&heard) {}
    void on(const OperatorSignatureSaid& s, loom::Mail&) { heard_->signatures.push_back(s); }
    void on(const OperatorReadingSaid& r, loom::Mail&) { heard_->readings.push_back(r); }

private:
    Heard* heard_;
};

// ---- the rig ---------------------------------------------------------------

/// A HOST WITH ONE OPERATOR CATALOG AND A REAL KERNEL.
///
/// THE MEMBER ORDER IS THE TEARDOWN CLAIM (§14). `catalog` and `operators` are
/// declared before `kernel`, so destruction — which runs in reverse — destroys
/// the Kernel first, and the Kernel destroys every artifact it holds before the
/// surface those artifacts were pointing at goes anywhere. A host that declared
/// them the other way round would be handing loaded code a context that dies
/// while the code can still run, and no refcount across the ABI would be
/// papering over it: the ordering is the host's to get right, and this is what
/// getting it right looks like.
struct HostRig {
    loom::Switchboard bus;
    op::Catalog catalog;
    op::OperatorHostSurface operators{catalog};
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);

    Heard heard;
    loom::WeaveId asker{};
    std::vector<std::string> loaded;
    std::vector<std::string> refusals;

    /// How the last `load()` ended, so a case can read the offer's own verdict
    /// beside the load's.
    op::OfferOutcome offer = op::OfferOutcome::NotAConsumer;
    std::string offer_reason;

    /// Deliveries counted off the bus tap — the number that says whether an
    /// operator call is a message.
    std::int64_t deliveries = 0;

    explicit HostRig(op::Catalog operators_in = tmr::fallback_vocabulary())
        : catalog(std::move(operators_in)) {
        loom::Grant speak;
        speak.allow_to_any(OperatorDescribeAsk::zen_name, OperatorDescribeAsk::zen_version);
        speak.allow_to_any(OperatorEvaluateAsk::zen_name, OperatorEvaluateAsk::zen_version);
        asker = loom::mount_granted<Asker>(bus, std::move(speak), heard);
        bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered) {
                ++deliveries;
            }
        });
    }

    /// LOAD AN ARTIFACT THE WAY A REAL ZENGINE HOST DOES — an ordinary
    /// `zen.LoadWeave` sent as a weave that can hear the answer, through the
    /// Weave Manager, through the control door, into `Kernel::load`. There is no
    /// direct `Kernel::load` here and no test-only door.
    ///
    /// THE OFFER BRACKETS THE WHOLE THING, which is the point: the load happens
    /// deep inside a delivery that this host cannot get between, so the offer is
    /// placed before the command is even sent and withdrawn after the pump has
    /// drained. `create()` runs inside that window whichever path took it there.
    loom::WeaveId load(const char* name, const char* path, const char* role = "") {
        op::OperatorOffer offering(operators, path);
        offer = offering.outcome();
        offer_reason = offering.reason();

        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        const std::size_t before = loaded.size();
        const loom::WeaveId booter =
            loom::mount_granted<Booter>(bus, std::move(reach), loaded, refusals);
        bus.send_as(booter, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{name, path, role}), booter,
                                  booter, 0));
        bus.pump();
        if (loaded.size() <= before) {
            return loom::WeaveId{};
        }
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(loaded.back()))};
    }

    /// THE SAME LOAD WITH NO OFFER IN FORCE — a host that does not intend this
    /// artifact to receive its operators, which is every host in this repository
    /// today and every weave that predates the seam.
    ///
    /// It exists so a case can ask the one question the ordinary path cannot:
    /// whether the offer's WITHDRAWAL really happened. An instance created here
    /// after a successful offer elsewhere must be UNBOUND -- if the module's slot
    /// still held the previous table it would pick it up, silently, and be right
    /// by accident.
    loom::WeaveId load_unoffered(const char* name, const char* path) {
        offer = op::OfferOutcome::NotAConsumer;
        offer_reason.clear();
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        const std::size_t before = loaded.size();
        const loom::WeaveId booter =
            loom::mount_granted<Booter>(bus, std::move(reach), loaded, refusals);
        bus.send_as(booter, manager,
                    loom::Message(loom::to_value(loom::LoadWeave{name, path, ""}), booter,
                                  booter, 0));
        bus.pump();
        if (loaded.size() <= before) {
            return loom::WeaveId{};
        }
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(loaded.back()))};
    }

    /// UNLOAD THROUGH THE REAL CONTROL DOOR, with Loom's own capability grant
    /// rather than a hand-written subset.
    bool unload(const char* name) {
        const loom::WeaveId booter = loom::mount_granted<Booter>(
            bus, loom::load_capability(control), loaded, refusals);
        const std::size_t before = refusals.size();
        bus.send_as(booter, control,
                    loom::Message(loom::to_value(loom::UnloadLibrary{name}), booter, booter, 0));
        bus.pump();
        return refusals.size() == before;
    }

    void ask_describe(loom::WeaveId target, const std::string& identity) {
        bus.send_as(asker, target,
                    loom::Message(loom::to_value(OperatorDescribeAsk{identity}), asker, asker, 0));
        bus.pump();
    }

    void ask_evaluate(loom::WeaveId target, const std::string& identity,
                      std::vector<OperatorArgument> arguments, std::int64_t repetitions = 1) {
        bus.send_as(asker, target,
                    loom::Message(loom::to_value(OperatorEvaluateAsk{identity,
                                                                     std::move(arguments),
                                                                     repetitions}),
                                  asker, asker, 0));
        bus.pump();
    }

    const OperatorSignatureSaid& last_signature() const { return heard.signatures.back(); }
    const OperatorReadingSaid& last_reading() const { return heard.readings.back(); }
};

/// The witness arguments, spelled once.
std::vector<OperatorArgument> delay_args(const char* delay_ms, const char* repeat) {
    return {OperatorArgument{tmr::kAuthoredDelayPort, delay_ms},
            OperatorArgument{tmr::kRepeatPort, repeat}};
}

std::string read_file(const char* path) {
    std::ifstream in(path);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

} // namespace

// ---- 1. optional means optional --------------------------------------------

TEST_CASE("an artifact written before this seam existed is simply not a consumer") {
    // A REAL, UNTOUCHED WEAVE from another package's rules -- not a fixture this
    // phase shaped for the occasion. It meets the offer path, exports no operator
    // surface, and the offer says so with no diagnostic at all, because being an
    // ordinary weave is not a fault.
    HostRig r;
    const loom::WeaveId id = r.load("untouched", OPH_UNTOUCHED_WEAVE_SO);

    CHECK(r.offer == op::OfferOutcome::NotAConsumer);
    CHECK(r.offer_reason.empty());
    REQUIRE(id.value != 0);
    CHECK(r.kernel.status("untouched") == loom::ArtifactStatus::Live);
    CHECK(r.refusals.empty());
}

TEST_CASE("the difference between an operator-aware artifact and an ordinary one is ONE line") {
    // The same source, one declaration apart. The legacy build exports nothing, so
    // the host has nothing to offer to -- and the weave, which contains every line
    // of the consumer code, still answers "no host" rather than reaching anywhere.
    HostRig r;
    const loom::WeaveId id = r.load("legacy", OPH_STRANGER_LEGACY_SO);
    REQUIRE(id.value != 0);
    CHECK(r.offer == op::OfferOutcome::NotAConsumer);

    r.ask_evaluate(id, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(r.heard.readings.size() == 1);
    CHECK_FALSE(r.last_reading().ok);
    CHECK(r.last_reading().status == ZENGINE_OP_ERR_NO_HOST);
    CHECK(r.last_reading().reason == "no operator host was offered to this weave");
}

TEST_CASE("a surface at a version this host does not speak is REFUSED, not guessed at") {
    HostRig r;
    const loom::WeaveId id = r.load("stale", OPH_STRANGER_ABI_SO);

    // The artifact still LOADS -- an optional surface it could not agree on is no
    // reason to refuse a weave -- and the refusal names both numbers, which is the
    // only thing that tells an operator where to look.
    REQUIRE(id.value != 0);
    CHECK(r.kernel.status("stale") == loom::ArtifactStatus::Live);
    CHECK(r.offer == op::OfferOutcome::VersionMismatch);
    CHECK(r.offer_reason.find("operator surface v2") != std::string::npos);
    CHECK(r.offer_reason.find("this host speaks v1") != std::string::npos);

    // NOTHING WAS HANDED OVER. The fixture's `offer` is a real function pointer,
    // so "the host refused on the number" and "the host called through a table it
    // could not vouch for" are distinguishable -- and this is the distinction.
    r.ask_evaluate(id, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(r.heard.readings.size() == 1);
    CHECK(r.last_reading().status == ZENGINE_OP_ERR_NO_HOST);
}

TEST_CASE("a consumer refuses a host table it cannot vouch for, before storing anything") {
    // The other direction of the same check, and the only way to reach it: a table
    // built by hand at a version this consumer does not know. Both sides check,
    // because each is the only one that knows what IT was compiled against.
    ZengineOperatorHostApiV1 slot{};
    ZengineOperatorHostApiV1 stale{};
    stale.abi_version = ZENGINE_OPERATOR_ABI_VERSION + 1u;
    stale.ctx = &stale;

    CHECK(op::detail::accept_offer_into(slot, &stale) == ZENGINE_OP_ERR_ABI);
    CHECK(slot.abi_version == 0u);
    CHECK(slot.ctx == nullptr);
    CHECK_FALSE(op::OperatorHost::over(&stale).bound());

    // ...and a withdrawal always succeeds, whatever happened during the load.
    ZengineOperatorHostApiV1 good{};
    good.abi_version = ZENGINE_OPERATOR_ABI_VERSION;
    REQUIRE(op::detail::accept_offer_into(slot, &good) == ZENGINE_OP_OK);
    CHECK(slot.abi_version == ZENGINE_OPERATOR_ABI_VERSION);
    CHECK(op::detail::accept_offer_into(slot, nullptr) == ZENGINE_OP_OK);
    CHECK(slot.abi_version == 0u);
}

// ---- 2. the handoff --------------------------------------------------------

TEST_CASE("a loaded stranger reads the rule's ports off the HOST's own definition") {
    HostRig r;
    const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(id.value != 0);
    CHECK(r.offer == op::OfferOutcome::Offered);

    r.ask_describe(id, tmr::kNormalizeDelay);
    REQUIRE(r.heard.signatures.size() == 1);
    const OperatorSignatureSaid& said = r.last_signature();

    CHECK(said.status == ZENGINE_OP_OK);
    CHECK(said.identity == "timer.normalize_delay");
    // The actual contract, port for port and kind for kind, decoded in the other
    // image from the schemas `evaluate` resolves. Nothing here was authored twice.
    CHECK(said.inputs == "delay_ms:Int,repeat:Bool");
    CHECK(said.outputs == "effective_delay:Int");

    // ...and it IS the host's own schema, not a plausible retelling of it. The
    // names and versions match the definitions in this process's catalog.
    const op::OperatorDef* def = r.catalog.find(tmr::kNormalizeDelay);
    REQUIRE(def != nullptr);
    CHECK(def->inputs()->name() == "timer.normalize_delay.in");
    CHECK(def->outputs()->fields()[0].name == said.outputs.substr(0, said.outputs.find(':')));
}

TEST_CASE("the offer reaches the instance inside create(), before any delivery") {
    // The stranger takes the offer in its CONSTRUCTOR, which the Kernel calls
    // inside the load -- so if the binding were not already in place there, the
    // instance would have started unbound and stayed that way, because nothing
    // offers it anything afterwards. That the very first ask succeeds is the
    // witness: the state it reports could only have been reached at construction.
    HostRig r;
    const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(id.value != 0);

    const std::int64_t deliveries_before = r.deliveries;
    r.ask_evaluate(id, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(r.heard.readings.size() == 1);
    CHECK(r.last_reading().ok);
    // Two deliveries and no more: the ask in, the reading back. The binding was
    // not negotiated over the bus, because there was no room in which to do it.
    CHECK(r.deliveries - deliveries_before == 2);
}

TEST_CASE("a loaded stranger evaluates the real composition: -500 repeating is 1ms") {
    HostRig r;
    const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(id.value != 0);

    r.ask_evaluate(id, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(r.heard.readings.size() == 1);
    CHECK(r.last_reading().ok);
    CHECK(r.last_reading().status == ZENGINE_OP_OK);
    CHECK(r.last_reading().port == "effective_delay");
    CHECK(r.last_reading().answer == "1");
}

TEST_CASE("the loaded stranger's matrix is SEM-0's matrix, across a module boundary") {
    HostRig r;
    const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(id.value != 0);

    struct Row {
        const char* delay;
        const char* repeat;
        const char* effective;
    };
    // The floor is the only thing the rule moves, and a repeating beat's floor is
    // 1 while a one-shot's is 0. Everything at or above it comes back untouched.
    const Row rows[] = {
        {"-500", "true", "1"},   {"-500", "false", "0"}, {"-1", "true", "1"},
        {"-1", "false", "0"},    {"0", "true", "1"},     {"0", "false", "0"},
        {"1", "true", "1"},      {"1", "false", "1"},    {"2", "true", "2"},
        {"2", "false", "2"},     {"5000", "true", "5000"}, {"5000", "false", "5000"},
    };
    for (const Row& row : rows) {
        r.ask_evaluate(id, tmr::kNormalizeDelay, delay_args(row.delay, row.repeat));
        REQUIRE_FALSE(r.heard.readings.empty());
        const OperatorReadingSaid& said = r.last_reading();
        INFO("delay_ms=", row.delay, " repeat=", row.repeat);
        REQUIRE(said.ok);
        CHECK(said.answer == row.effective);
    }

    // ...and the same answers, from the same catalog, asked in-process. If these
    // two ever disagreed, one of them would be reading a copy.
    for (const Row& row : rows) {
        const std::int64_t expected = std::stoll(row.effective);
        const bool repeat = std::string(row.repeat) == "true";
        CHECK(tmr::effective_delay(r.catalog, std::stoll(row.delay), repeat) == expected);
    }
}

// ---- 3. an operator call is not a message ----------------------------------

TEST_CASE("sixteen evaluations cost the bus what one does, and run in the HOST") {
    HostRig r;
    const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(id.value != 0);

    const std::int64_t deliveries_before = r.deliveries;
    const std::uint64_t invocations_before = op::invocations();
    r.ask_evaluate(id, tmr::kNormalizeDelay, delay_args("-500", "true"), /*repetitions=*/16);

    REQUIRE(r.heard.readings.size() == 1);
    CHECK(r.last_reading().answer == "1");

    // THE CLAIM, IN TWO NUMBERS. The bus moved by exactly the ask and the reading
    // -- the same two it moves for a single evaluation -- while the composition's
    // three primitive bodies ran sixteen times. A rule spelled as conversation
    // would have cost three pump generations per evaluation, so this number would
    // be 2 + 48 rather than 2.
    CHECK(r.deliveries - deliveries_before == 2);

    // ...and it is the HOST'S counter that moved: the native bodies live in this
    // image, so the loaded consumer spent code it does not contain. Three nodes,
    // sixteen times.
    CHECK(op::invocations() - invocations_before == 3 * 16);
}

TEST_CASE("describing an operator is not a message either") {
    HostRig r;
    const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(id.value != 0);

    const std::int64_t before = r.deliveries;
    r.ask_describe(id, tmr::kNormalizeDelay);
    REQUIRE(r.heard.signatures.size() == 1);
    CHECK(r.last_signature().status == ZENGINE_OP_OK);
    CHECK(r.deliveries - before == 2);
}

// ---- 4. lifecycle ----------------------------------------------------------

TEST_CASE("unload leaves no usable binding, and a reload gets a fresh one") {
    HostRig r;
    const loom::KernelLifetimeCounts start = loom::kernel_lifetime_counts();

    const loom::WeaveId first = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(first.value != 0);
    CHECK(r.offer == op::OfferOutcome::Offered);
    r.ask_evaluate(first, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(r.heard.readings.size() == 1);
    CHECK(r.last_reading().answer == "1");

    REQUIRE(r.unload("stranger"));
    CHECK(r.kernel.status("stranger") == loom::ArtifactStatus::NotLoaded);
    CHECK_FALSE(r.kernel.is_loaded("stranger"));

    // THE SECOND LOAD IS A SECOND OFFER, and it has to be: the withdrawal at the
    // end of the first left the module's slot empty, so an instance created now
    // with no offer in force would be unbound. It is not.
    const loom::WeaveId second = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(second.value != 0);
    CHECK(second.value != first.value);
    CHECK(r.offer == op::OfferOutcome::Offered);
    r.ask_evaluate(second, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(r.heard.readings.size() == 2);
    CHECK(r.last_reading().ok);
    CHECK(r.last_reading().answer == "1");

    // Two instances created, one destroyed by the unload -- and nothing was left
    // half-open on the way through.
    const loom::KernelLifetimeCounts now = loom::kernel_lifetime_counts();
    CHECK(now.instances_created - start.instances_created == 2);
    CHECK(now.instances_destroyed - start.instances_destroyed == 1);
}

TEST_CASE("two instances of ONE image each get their own offer, not a shared binding") {
    // The module-scope slot is the honest hazard of any exported setter, and this
    // is what stops it being one: it holds a value only between an offer and its
    // withdrawal, so two loads of the same file are two separate handoffs rather
    // than one durable module-wide binding two instances race over.
    HostRig r;
    const loom::WeaveId a = r.load("stranger-a", OPH_STRANGER_SO);
    const loom::WeaveId b = r.load("stranger-b", OPH_STRANGER_SO);
    REQUIRE(a.value != 0);
    REQUIRE(b.value != 0);
    REQUIRE(a.value != b.value);

    r.ask_evaluate(a, tmr::kNormalizeDelay, delay_args("-500", "true"));
    r.ask_evaluate(b, tmr::kNormalizeDelay, delay_args("-500", "false"));
    REQUIRE(r.heard.readings.size() == 2);
    CHECK(r.heard.readings[0].answer == "1");
    CHECK(r.heard.readings[1].answer == "0");
}

TEST_CASE("the withdrawal is real: an instance loaded with no offer is UNBOUND") {
    // THE HAZARD OF ANY EXPORTED SETTER, asked directly. The slot behind
    // `zengine_operator_consumer` is module-scope because a C export has no other
    // scope; what stops it being a durable module-wide binding is that the host
    // takes its offer back when the load is done. So: offer once, and then load
    // the SAME IMAGE again with no offer at all.
    //
    // A stale slot would make this instance bound -- and right, and right for the
    // wrong reason, which is the failure that survives every other case in this
    // file.
    HostRig r;
    const loom::WeaveId offered = r.load("offered", OPH_STRANGER_SO);
    REQUIRE(offered.value != 0);
    REQUIRE(r.offer == op::OfferOutcome::Offered);
    r.ask_evaluate(offered, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(r.heard.readings.size() == 1);
    CHECK(r.last_reading().ok);

    const loom::WeaveId unoffered = r.load_unoffered("unoffered", OPH_STRANGER_SO);
    REQUIRE(unoffered.value != 0);
    r.ask_evaluate(unoffered, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(r.heard.readings.size() == 2);
    CHECK_FALSE(r.last_reading().ok);
    CHECK(r.last_reading().status == ZENGINE_OP_ERR_NO_HOST);

    // ...and the one that WAS offered still works, because it holds a copy rather
    // than a pointer into the slot the withdrawal emptied.
    r.ask_evaluate(offered, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(r.heard.readings.size() == 3);
    CHECK(r.last_reading().ok);
    CHECK(r.last_reading().answer == "1");
}

TEST_CASE("the host and its catalog outlive every consumer that was offered them") {
    // WHAT THIS CAN HONESTLY PROVE, and it is the useful half: that the whole
    // teardown ran, that the consumer's instance was destroyed inside the host's
    // lifetime rather than after it, and that the image closed. The ORDER is
    // structural -- `HostRig` declares its catalog and its surface before its
    // Kernel, so reverse-order destruction takes the Kernel and its artifacts
    // first -- and a rig that got that wrong would be handing loaded code a
    // context that dies while the code can still run.
    const loom::KernelLifetimeCounts start = loom::kernel_lifetime_counts();
    {
        HostRig r;
        const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
        REQUIRE(id.value != 0);
        r.ask_evaluate(id, tmr::kNormalizeDelay, delay_args("-500", "true"));
        REQUIRE(r.heard.readings.size() == 1);
        CHECK(r.last_reading().ok);
        // No explicit unload: the rig simply goes out of scope.
    }
    const loom::KernelLifetimeCounts now = loom::kernel_lifetime_counts();
    CHECK(now.instances_created - start.instances_created == 1);
    CHECK(now.instances_destroyed - start.instances_destroyed == 1);
    // ONE OPEN AND ONE CLOSE, AND THE LEDGER IS LOOM'S. It counts the KERNEL's
    // opens and closes and knows nothing about the offer's own share, so what
    // this says is precisely that the operator seam did not change the Kernel's
    // bookkeeping: an operator-aware artifact opens and closes exactly what a
    // weave that never heard of operators does. The offer's share is invisible
    // here because it is released inside `load()`, before this is read at all.
    CHECK(now.libraries_opened - start.libraries_opened == 1);
    CHECK(now.libraries_closed - start.libraries_closed == 1);
}

// ---- 5. the fence ----------------------------------------------------------

TEST_CASE("replace a primitive in the HOST and the LOADED consumer moves with it") {
    // THE PHASE'S DECISIVE WITNESS, and the thing SEM-0 could only assert about an
    // in-process reader. `math.max` becomes a min underneath the rule -- same
    // identity, same port names, same types, therefore the same two content ids,
    // so nothing structural notices -- and the artifact, whose source and binary
    // are byte-for-byte the ones the honest case loaded, answers differently.
    //
    // A consumer holding a private catalog could not do this. It would still say 1.
    HostRig honest;
    HostRig sabotaged{zengine::testing::sabotaged_operators()};

    const loom::WeaveId a = honest.load("stranger", OPH_STRANGER_SO);
    const loom::WeaveId b = sabotaged.load("stranger", OPH_STRANGER_SO);
    REQUIRE(a.value != 0);
    REQUIRE(b.value != 0);

    honest.ask_evaluate(a, tmr::kNormalizeDelay, delay_args("-500", "true"));
    sabotaged.ask_evaluate(b, tmr::kNormalizeDelay, delay_args("-500", "true"));
    REQUIRE(honest.heard.readings.size() == 1);
    REQUIRE(sabotaged.heard.readings.size() == 1);

    CHECK(honest.last_reading().answer == "1");
    CHECK(sabotaged.last_reading().answer == "-500");

    // The substitution is invisible to every check the seam makes: the described
    // contract is identical, so the change could only have arrived through the
    // evaluation itself.
    honest.ask_describe(a, tmr::kNormalizeDelay);
    sabotaged.ask_describe(b, tmr::kNormalizeDelay);
    REQUIRE(honest.heard.signatures.size() == 1);
    REQUIRE(sabotaged.heard.signatures.size() == 1);
    CHECK(honest.last_signature().inputs == sabotaged.last_signature().inputs);
    CHECK(honest.last_signature().outputs == sabotaged.last_signature().outputs);
}

TEST_CASE("the stranger's translation unit cannot name the host's side of the seam") {
    // The link line says `zengine-operator-consumer` and not `zengine-operator`,
    // which is real but not self-checking: these are header-only packages, so
    // nothing at link time stops a later edit from including sideways. What CAN be
    // checked, and is checked here, is that the source still names none of it.
    //
    // The tripwire earns its place because the canary above is the load-bearing
    // proof and this is the thing that keeps it meaningful: a stranger that
    // quietly acquired a catalog would still pass every other case in this file
    // until somebody thought to swap a primitive.
    const std::string source = read_file(OPH_STRANGER_CPP);
    REQUIRE_FALSE(source.empty());

    for (const char* forbidden : {"op::Catalog", "OperatorDef", "make_operator",
                                  "operator/catalog.hpp", "operator/primitives.hpp",
                                  "operator/host_surface.hpp", "math.max", "logic.select_int",
                                  "fallback_vocabulary", "timer/"}) {
        INFO("the dynamic stranger must not name: ", forbidden);
        CHECK(source.find(forbidden) == std::string::npos);
    }

    // ...and it does name the one thing it is allowed to know (§21): the operator
    // it was authored against arrives in a message, so not even the identity is
    // compiled in.
    CHECK(source.find("timer.normalize_delay") == std::string::npos);
    CHECK(source.find("operator/host.hpp") != std::string::npos);
}

// ---- 6. five failures, five answers ----------------------------------------

TEST_CASE("an operator the host does not publish is NOT_FOUND, not a refusal") {
    HostRig r;
    const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(id.value != 0);

    r.ask_describe(id, "nobody.publishes.this");
    REQUIRE(r.heard.signatures.size() == 1);
    CHECK(r.last_signature().status == ZENGINE_OP_ERR_NOT_FOUND);
    CHECK(r.last_signature().inputs.empty());

    r.ask_evaluate(id, "nobody.publishes.this", {});
    REQUIRE(r.heard.readings.size() == 1);
    CHECK(r.last_reading().status == ZENGINE_OP_ERR_NOT_FOUND);
    CHECK(r.last_reading().reason == "this host publishes no 'nobody.publishes.this'");
}

TEST_CASE("a missing argument is the HOST GATE's refusal, in the gate's own words") {
    HostRig r;
    const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(id.value != 0);

    // `repeat` never supplied. The pack is serialized in the consumer's image,
    // parsed in the host's, and refused by `loom::admit` against the operator's
    // real input schema -- and the sentence that comes back is the CATALOG's, word
    // for word, because the seam hands the bytes to the catalog rather than
    // admitting them itself and inventing a second wording.
    r.ask_evaluate(id, tmr::kNormalizeDelay, {OperatorArgument{tmr::kAuthoredDelayPort, "-500"}});
    REQUIRE(r.heard.readings.size() == 1);
    const OperatorReadingSaid& said = r.last_reading();
    CHECK_FALSE(said.ok);
    CHECK(said.status == ZENGINE_OP_ERR_REFUSED);
    CHECK(said.reason.find("timer.normalize_delay") != std::string::npos);
    CHECK(said.reason.find("refused its arguments") != std::string::npos);
    CHECK(said.reason.find("repeat") != std::string::npos);

    // ...and it is the SAME sentence an in-process caller gets. Two spellings of
    // one refusal is how a caller and a callee stop meaning the same thing.
    loom::Value short_pack(r.catalog.find(tmr::kNormalizeDelay)->inputs());
    short_pack.set(tmr::kAuthoredDelayPort, loom::Cell::integer(-500));
    const op::Evaluation in_process = r.catalog.evaluate(tmr::kNormalizeDelay,
                                                         std::move(short_pack));
    REQUIRE_FALSE(in_process.ok());
    CHECK(in_process.reason() == said.reason);
}

TEST_CASE("a port that is not a port is refused where it is spelled") {
    HostRig r;
    const loom::WeaveId id = r.load("stranger", OPH_STRANGER_SO);
    REQUIRE(id.value != 0);

    r.ask_evaluate(id, tmr::kNormalizeDelay,
                   {OperatorArgument{"delay", "-500"}, OperatorArgument{"repeat", "true"}});
    REQUIRE(r.heard.readings.size() == 1);
    CHECK(r.last_reading().status == ZENGINE_OP_ERR_REFUSED);
    CHECK(r.last_reading().reason.find("has no input port named 'delay'") != std::string::npos);

    // ...and a value that is not of the port's kind is refused by the reader, which
    // learned that kind from the host rather than from a compiled-in expectation.
    r.ask_evaluate(id, tmr::kNormalizeDelay, delay_args("-500", "yes"));
    REQUIRE(r.heard.readings.size() == 2);
    CHECK(r.last_reading().status == ZENGINE_OP_ERR_REFUSED);
    CHECK(r.last_reading().reason == "'yes' is not a Bool");
}

TEST_CASE("an unbound host answers NO_HOST for every verb, and never crashes") {
    const op::OperatorHost none;
    CHECK_FALSE(none.bound());
    CHECK(none.describe(tmr::kNormalizeDelay).status == ZENGINE_OP_ERR_NO_HOST);

    // ...including with a contract it could not possibly have obtained.
    const op::Catalog catalog = tmr::fallback_vocabulary();
    op::OperatorHostSurface surface(catalog);
    const op::OperatorHost real = op::OperatorHost::over(surface.api());
    const op::HostSignature contract = real.describe(tmr::kNormalizeDelay);
    REQUIRE(contract.ok());
    loom::Value pack(contract.inputs);
    pack.set(tmr::kAuthoredDelayPort, loom::Cell::integer(-500));
    pack.set(tmr::kRepeatPort, loom::Cell::boolean(true));
    CHECK(none.evaluate(contract, pack).status == ZENGINE_OP_ERR_NO_HOST);

    // ...and a bound host handed a contract that was never obtained refuses too,
    // rather than admitting an answer against a shape nobody described.
    CHECK(real.evaluate(op::HostSignature{}, pack).status == ZENGINE_OP_ERR_NOT_FOUND);
}

TEST_CASE("the descriptor carries the closure of every schema a port nests") {
    // `timer.normalize_delay` is flat -- Int, Bool, Int -- so it emits no closure
    // at all, and that is the case the seam is exercised on. This pins the other
    // half against the day a port is a Message: the encoder walks the same
    // post-order `collect_referenced` a manifest does, and the decoder resolves it
    // the same way, so a nested port arrives whole rather than unresolved.
    const auto nested = loom::make_schema(
        "oph.test.Point", 1,
        std::vector<loom::Field>{loom::Field{"x", loom::type_of(loom::Kind::Int), true},
                                 loom::Field{"y", loom::type_of(loom::Kind::Int), true}});
    const auto ports = loom::make_schema(
        "oph.test.Nested.in", 1,
        std::vector<loom::Field>{loom::Field{"where", loom::type_message(nested), true}});
    const auto answer = loom::make_schema(
        "oph.test.Nested.out", 1,
        std::vector<loom::Field>{loom::Field{"result", loom::type_of(loom::Kind::Int), true}});

    const loom::Value desc = op::encode_operator_desc("oph.test.nested", *ports, *answer);
    REQUIRE(desc.get("referenced") != nullptr);
    REQUIRE(desc.get("referenced")->as_list().size() == 1);

    // The round trip a consumer performs, against a registry that starts empty --
    // so the only way the nested shape can resolve is the closure the encoder sent.
    const loom::Unverified u = loom::parse(loom::serialize(desc));
    const loom::Admission admitted = loom::admit(u, op::operator_desc_schema());
    REQUIRE(admitted.ok());
    loom::Registry vocabulary;
    loom::decode_referenced(admitted.value(), vocabulary);
    const auto rebuilt =
        loom::decode_schema(*admitted.value().get("inputs")->as_message(), vocabulary);
    REQUIRE(rebuilt->fields().size() == 1);
    CHECK(rebuilt->fields()[0].type.kind == loom::Kind::Message);
    CHECK(rebuilt->fields()[0].type.message->content_id() == nested->content_id());
    // Byte-for-byte the schema the host holds: the contract crossed, not a
    // retelling of it.
    CHECK(rebuilt->content_id() == ports->content_id());
}
