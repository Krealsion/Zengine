// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// POWERS COME FROM PROVIDERS (PROV-0) — whether the host still AUTHORS meaning, or
// only decides which supplied meaning is currently in force.
//
// `test_operator.cpp` asks what an operator is. `test_operator_host.cpp` asks
// whether a loaded stranger can spend a host's catalog. `test_operator_canonical.cpp`
// asks whether the Timer and that stranger spend ONE catalog instance. All three were
// answered yes, and every power in the process still came from a line in the host
// that called a package's authoring function. Every case here is about the moment
// the powers come from somewhere else and one of them is replaced.
//
// THE TIERS:
//
//   1  THE SEAM        a real provider artifact, opened and read across a native
//                      module boundary; the refusals for everything it could be
//                      instead.
//   2  CROSS-PROVIDER  a composition supplied by one provider whose nodes name
//                      powers supplied by another, structurally and then in
//                      arithmetic.
//   3  THE CHAIN       1 -> 2 -> 3, pinned; provider B covers ONLY 3; all three
//                      move; B unmounts; all three come back.
//   4  RESIDENT        A's contribution was never deleted, and B's removal REVEALED
//                      that object rather than rebuilding one that compares equal.
//   5  REFUSALS        an ordinary collision, an incompatible overlay, a missing
//                      dependency, a stale ABI, an artifact that is not a provider.
//   6  ONE STORE       describe and evaluate resolve the same contribution, before
//                      and after.
//   7  THE REAL TIMER  a running host-backed Timer's STORED delay and a loaded
//                      stranger's answer both move when a primitive provider is
//                      overlaid, and both come back when it is removed.
//   8  CUSTODY         the image is held while its contributions resolve and
//                      released after they stop; evaluation costs the bus nothing.
//   9  THE HOST        `workshop.cpp`, read as a source file: it no longer authors
//                      any of this.

#include "doctest.h"

#include "operator/catalog.hpp"
#include "operator/host.hpp"
#include "operator/host_surface.hpp"
#include "operator/image.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "operator/provider.hpp"
#include "operator/provider_host.hpp"
#include "operator_ask.hpp"
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

// ---- the chain, and what each provider makes of it --------------------------

constexpr const char* kF1 = "prov.function.1";
constexpr const char* kF2 = "prov.function.2";
constexpr const char* kF3 = "prov.function.3";
constexpr const char* kProviderA = "zengine.provider.a";
constexpr const char* kProviderB = "zengine.provider.b";
constexpr const char* kBasic = "zengine.operators.basic";
constexpr const char* kTimerProvider = "zengine.timer";
constexpr const char* kMinProvider = "zengine.operators.test.min";

/// THE PINNED BASELINE, worked through once so a reader need not.
///
///     A:  f3(x) = 2x        f2 = f3(f3(x)) = 4x       f1 = f2(f2(x)) = 16x
///     B:  f3(x) = x + 100   f2 = x + 200              f1 = x + 400
///
/// At x = 3 that is 6/12/48 and 103/203/403 — six values, all distinct, so no
/// arrangement of the chain can produce one provider's answer by accident and a
/// composite that failed to move would be visible immediately.
constexpr std::int64_t kArgument = 3;
constexpr std::int64_t kBaseline3 = 6;
constexpr std::int64_t kBaseline2 = 12;
constexpr std::int64_t kBaseline1 = 48;
constexpr std::int64_t kOverlaid3 = 103;
constexpr std::int64_t kOverlaid2 = 203;
constexpr std::int64_t kOverlaid1 = 403;

/// Spend one of the chain's operators. Every one of them takes a single Int named
/// `value`, whichever provider currently supplies it.
std::int64_t spend(const op::Catalog& catalog, const char* identity, std::int64_t value) {
    const op::OperatorDef* def = catalog.find(identity);
    REQUIRE_MESSAGE(def != nullptr, "nothing currently supplies ", identity);
    loom::Value pack(def->inputs());
    pack.set("value", loom::Cell::integer(value));
    const op::Evaluation answered = catalog.evaluate(identity, std::move(pack));
    REQUIRE_MESSAGE(answered.ok(), answered.reason());
    return answered.value().at(0)->as_int();
}

/// Which provider currently satisfies an identity, and which are underneath it.
/// One read of the ONE store: `contributions` walks the same stack `find` answers
/// the top of.
std::string active_provider(const op::Catalog& catalog, const char* identity) {
    const std::vector<op::Contribution> stack = catalog.contributions(identity);
    return stack.empty() ? std::string("<nobody>") : stack.back().provider;
}

std::vector<std::string> shadowed_providers(const op::Catalog& catalog, const char* identity) {
    std::vector<op::Contribution> stack = catalog.contributions(identity);
    std::vector<std::string> under;
    for (std::size_t i = 0; i + 1 < stack.size(); ++i) {
        under.push_back(stack[i].provider);
    }
    return under;
}

/// The exact object one provider contributed to one identity, or nullptr. What the
/// non-destructive witness needs and the only thing it needs: an ADDRESS, so
/// "revealed" and "rebuilt" cannot be confused.
const op::OperatorDef* contribution_of(const op::Catalog& catalog, const char* identity,
                                       const char* provider) {
    for (const op::Contribution& c : catalog.contributions(identity)) {
        if (c.provider == provider) {
            return c.definition.get();
        }
    }
    return nullptr;
}

} // namespace

// ---- 1. the seam ------------------------------------------------------------

TEST_CASE("a real provider artifact supplies powers across a native module boundary") {
    op::Catalog catalog;
    CHECK(catalog.size() == 0);
    CHECK(catalog.providers().empty());

    const op::MountResult mounted = op::mount_provider(catalog, PROVIDER_BASIC_SO);
    REQUIRE_MESSAGE(mounted.ok, mounted.reason);

    // The provider named ITSELF; the host chose nothing about that identity.
    CHECK(mounted.provider == kBasic);
    CHECK(mounted.contributed == 2);
    CHECK(catalog.mounted(kBasic));
    CHECK(catalog.identities() == std::vector<std::string>{op::kSelectInt, op::kMaxInt});

    // ...and the powers WORK, which is what makes the mount more than bookkeeping.
    const op::OperatorDef* max = catalog.find(op::kMaxInt);
    REQUIRE(max != nullptr);
    loom::Value pack(max->inputs());
    pack.set("lhs", loom::Cell::integer(-500));
    pack.set("rhs", loom::Cell::integer(0));
    const op::Evaluation answered = catalog.evaluate(op::kMaxInt, std::move(pack));
    REQUIRE_MESSAGE(answered.ok(), answered.reason());
    CHECK(answered.value().at(0)->as_int() == 0);
}

TEST_CASE("a provider is not a weave: the basic provider exports no weave ABI") {
    // `provider != weave`, measured on the artifact this repository SHIPS rather
    // than asserted in prose. The image opens, answers the provider symbol, and has
    // no `zen_weave_abi` in it at all -- so `Kernel::load` would refuse it and it
    // has no WeaveId, no role, no grant and no line in `zen.ListLoaded`. Its whole
    // relationship with a host is the one symbol below.
    op::ImageShare image{PROVIDER_BASIC_SO};
    REQUIRE(image.open());
    CHECK(image.symbol(ZENGINE_OPERATOR_PROVIDER_SYMBOL) != nullptr);
    CHECK(image.symbol("zen_weave_abi") == nullptr);
    CHECK(image.symbol(ZENGINE_OPERATOR_CONSUMER_SYMBOL) == nullptr);
}

TEST_CASE("the Timer artifact is a provider, a consumer AND a weave, from one image") {
    // Three independent relationships, and CAT-0 could only have two of them. What
    // makes the third worth its own case is that nothing requires the others: the
    // basic provider next door has only the middle symbol, and every weave that
    // predates this seam has only the first.
    op::ImageShare image{TIMER_SO};
    REQUIRE(image.open());
    CHECK(image.symbol("zen_weave_abi") != nullptr);
    CHECK(image.symbol(ZENGINE_OPERATOR_PROVIDER_SYMBOL) != nullptr);
    CHECK(image.symbol(ZENGINE_OPERATOR_CONSUMER_SYMBOL) != nullptr);
}

TEST_CASE("the Timer provider contributes ONE power, and it is not a primitive") {
    op::Catalog catalog;
    const op::MountResult mounted = op::mount_provider(catalog, TIMER_SO);
    REQUIRE_MESSAGE(mounted.ok, mounted.reason);
    CHECK(mounted.provider == kTimerProvider);

    // The whole of what the Timer supplies: its own domain composition. `math.max`
    // and `logic.select_int` are named by that composition and supplied by nobody
    // here, which is exactly the state the next tier resolves.
    CHECK(mounted.contributed == 1);
    CHECK(catalog.identities() == std::vector<std::string>{tmr::kNormalizeDelay});
    CHECK(catalog.find(op::kMaxInt) == nullptr);
    CHECK(catalog.find(op::kSelectInt) == nullptr);
}

// ---- 2. cross-provider composition -------------------------------------------

TEST_CASE("a composition from one provider names powers supplied by another") {
    // THE STRUCTURAL HALF, and it is the one that makes the arithmetic mean
    // something. The rule that arrives from the Timer artifact is a GRAPH, its
    // nodes say `math.max` and `logic.select_int`, and those identities are
    // satisfied by a different artifact entirely.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_BASIC_SO).ok);
    REQUIRE(op::mount_provider(catalog, TIMER_SO).ok);

    const op::OperatorDef* rule = catalog.find(tmr::kNormalizeDelay);
    REQUIRE(rule != nullptr);
    REQUIRE(rule->is_composite());
    const op::Composite* graph = rule->composition();
    REQUIRE(graph != nullptr);
    REQUIRE(graph->nodes.size() == 3);
    CHECK(graph->nodes[0].identity == op::kMaxInt);
    CHECK(graph->nodes[1].identity == op::kMaxInt);
    CHECK(graph->nodes[2].identity == op::kSelectInt);

    // Every node's step is supplied by the OTHER provider, and every node's
    // authored signature is the one that other provider actually publishes -- which
    // is what the walk compares at every spend.
    for (const op::Node& node : graph->nodes) {
        CHECK(active_provider(catalog, node.identity.c_str()) == kBasic);
        const op::OperatorDef* step = catalog.find(node.identity);
        REQUIRE(step != nullptr);
        CHECK(step->inputs()->content_id() == node.authored_in);
        CHECK(step->outputs()->content_id() == node.authored_out);
    }
    CHECK(active_provider(catalog, tmr::kNormalizeDelay) == kTimerProvider);
}

TEST_CASE("...and it answers, which is the arithmetic half") {
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_BASIC_SO).ok);
    REQUIRE(op::mount_provider(catalog, TIMER_SO).ok);

    // max(max(-500,0),1) chosen on `repeat`. Neither artifact was compiled against
    // the other and the host wrote none of it.
    CHECK(tmr::effective_delay(catalog, -500, true) == 1);
    CHECK(tmr::effective_delay(catalog, -500, false) == 0);
    CHECK(tmr::effective_delay(catalog, 250, true) == 250);
}

TEST_CASE("a composition whose primitives nobody supplies is UNRESOLVED, by name") {
    // The Timer provider alone. The rule is present, structurally complete and
    // perfectly well formed; what is missing is somebody to supply what it names,
    // and the deepest layer that knows says which one.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, TIMER_SO).ok);

    const op::OperatorDef* rule = catalog.find(tmr::kNormalizeDelay);
    REQUIRE(rule != nullptr);
    const op::Evaluation answered =
        catalog.evaluate(tmr::kNormalizeDelay, tmr::normalize_ask(rule->inputs(), -500, true));
    CHECK_FALSE(answered.ok());
    CHECK(answered.reason().find("unresolved operator reference") != std::string::npos);
    CHECK(answered.reason().find(op::kMaxInt) != std::string::npos);
}

// ---- 3. the three-level replacement witness ---------------------------------

TEST_CASE("Provider B covers ONLY function.3, and all three answers move with it") {
    op::Catalog catalog;
    REQUIRE_MESSAGE(op::mount_provider(catalog, PROVIDER_A_SO).ok, "provider A must mount");

    // ---- the baseline, pinned before anything is replaced ------------------
    CHECK(spend(catalog, kF3, kArgument) == kBaseline3);
    CHECK(spend(catalog, kF2, kArgument) == kBaseline2);
    CHECK(spend(catalog, kF1, kArgument) == kBaseline1);

    // FUNCTION.1'S GRAPH DOES NOT NAME FUNCTION.3, and this is where that matters.
    // What follows changes function.1's answer through two layers it never
    // mentions.
    const op::OperatorDef* f1 = catalog.find(kF1);
    REQUIRE(f1 != nullptr);
    REQUIRE(f1->composition() != nullptr);
    for (const op::Node& node : f1->composition()->nodes) {
        CHECK(node.identity == kF2);
    }

    // ---- an EXPLICIT overlay, supplying one power ---------------------------
    const op::MountResult overlaid =
        op::mount_provider(catalog, PROVIDER_B_SO, op::MountMode::Overlay);
    REQUIRE_MESSAGE(overlaid.ok, overlaid.reason);
    CHECK(overlaid.provider == kProviderB);
    CHECK(overlaid.contributed == 1);

    // Provider A was neither rebuilt nor edited nor notified, and neither composite
    // was rewritten, rebound or walked over. The next evaluation simply resolves
    // what is there now.
    CHECK(spend(catalog, kF3, kArgument) == kOverlaid3);
    CHECK(spend(catalog, kF2, kArgument) == kOverlaid2);
    CHECK(spend(catalog, kF1, kArgument) == kOverlaid1);

    // ---- and taking it away restores the world ------------------------------
    REQUIRE(catalog.unmount(kProviderB));
    CHECK_FALSE(catalog.mounted(kProviderB));
    CHECK(spend(catalog, kF3, kArgument) == kBaseline3);
    CHECK(spend(catalog, kF2, kArgument) == kBaseline2);
    CHECK(spend(catalog, kF1, kArgument) == kBaseline1);
}

TEST_CASE("the composites were never touched: same graph objects, before and after") {
    // WHAT WOULD MAKE THE WITNESS ABOVE A LIE: a rebinding pass that rewrote the
    // graphs when a provider arrived. There is none, and this is how a reader can
    // tell -- the two composites are the same objects, holding the same graphs,
    // with the same authored signatures, on both sides of the replacement.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);
    const op::OperatorDef* f1_before = catalog.find(kF1);
    const op::OperatorDef* f2_before = catalog.find(kF2);
    REQUIRE(f1_before != nullptr);
    REQUIRE(f2_before != nullptr);
    const op::Composite* g1 = f1_before->composition();
    const op::Composite* g2 = f2_before->composition();
    REQUIRE(g1 != nullptr);
    REQUIRE(g2 != nullptr);
    REQUIRE_FALSE(g2->nodes.empty());
    const loom::ContentId authored = g2->nodes[0].authored_in;

    REQUIRE(op::mount_provider(catalog, PROVIDER_B_SO, op::MountMode::Overlay).ok);
    const op::OperatorDef* f1_after = catalog.find(kF1);
    const op::OperatorDef* f2_after = catalog.find(kF2);
    const op::OperatorDef* f3_after = catalog.find(kF3);
    REQUIRE(f1_after != nullptr);
    REQUIRE(f2_after != nullptr);
    REQUIRE(f3_after != nullptr);
    CHECK(f1_after == f1_before);
    CHECK(f2_after == f2_before);
    CHECK(f1_after->composition() == g1);
    CHECK(f2_after->composition() == g2);
    CHECK(g2->nodes[0].authored_in == authored);
    // ...and the overlay satisfies that same authored signature, which is why the
    // walk accepts it rather than refusing a reshaped power.
    CHECK(f3_after->inputs()->content_id() == authored);
}

// ---- 4. the shadowed contribution is still there ----------------------------

TEST_CASE("shadowing COVERS: A's contribution is resident underneath B") {
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);
    const op::OperatorDef* a3 = catalog.find(kF3);
    REQUIRE(a3 != nullptr);
    CHECK(active_provider(catalog, kF3) == kProviderA);
    CHECK(shadowed_providers(catalog, kF3).empty());

    REQUIRE(op::mount_provider(catalog, PROVIDER_B_SO, op::MountMode::Overlay).ok);

    // ACTIVE IS B; A IS STILL HERE. Read structurally, off the one store, without
    // going through the ordinary resolution path at all -- because the whole
    // question is what is underneath the answer that path gives.
    CHECK(active_provider(catalog, kF3) == kProviderB);
    CHECK(shadowed_providers(catalog, kF3) == std::vector<std::string>{kProviderA});
    CHECK(catalog.find(kF3) != a3);
    CHECK(contribution_of(catalog, kF3, kProviderA) == a3);

    // ...and both providers are mounted. Being covered is not being gone.
    CHECK(catalog.mounted(kProviderA));
    CHECK(catalog.mounted(kProviderB));
}

TEST_CASE("unmount REVEALS the object that was there, rather than rebuilding one") {
    // THE DIFFERENCE BETWEEN REVEALED AND RECONSTRUCTED IS AN ADDRESS. A catalog
    // that re-ran provider A's authoring on unmount would produce a definition that
    // compares equal in every way a value comparison could reach -- same identity,
    // same schemas, same content ids, same answers -- and would be a different
    // object. So the witness is the pointer.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);
    const op::OperatorDef* a3 = catalog.find(kF3);
    REQUIRE(a3 != nullptr);
    const loom::Schema* a3_ports = a3->inputs().get();

    REQUIRE(op::mount_provider(catalog, PROVIDER_B_SO, op::MountMode::Overlay).ok);
    REQUIRE(catalog.unmount(kProviderB));

    // REQUIRE, not CHECK, and the difference is what a canary reads: a catalog that
    // DELETED what it covered answers nullptr here, and a case that went on to
    // dereference it would crash instead of failing -- and a crash is not a red.
    const op::OperatorDef* revealed = catalog.find(kF3);
    REQUIRE(revealed != nullptr);
    CHECK(revealed == a3);
    CHECK(revealed->inputs().get() == a3_ports);
    CHECK(active_provider(catalog, kF3) == kProviderA);
    CHECK(shadowed_providers(catalog, kF3).empty());
}

TEST_CASE("unmounting the provider UNDERNEATH leaves the overlay standing alone") {
    // The other order, so "unmount removes exactly that provider's contributions"
    // is a rule rather than a description of the easy case. A's three go; B's one
    // stays; the two composites are gone with their author and are honestly
    // unresolved rather than half-present.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);
    REQUIRE(op::mount_provider(catalog, PROVIDER_B_SO, op::MountMode::Overlay).ok);

    REQUIRE(catalog.unmount(kProviderA));
    CHECK(catalog.find(kF1) == nullptr);
    CHECK(catalog.find(kF2) == nullptr);
    CHECK(active_provider(catalog, kF3) == kProviderB);
    CHECK(spend(catalog, kF3, kArgument) == kOverlaid3);
    CHECK(catalog.identities() == std::vector<std::string>{kF3});
}

TEST_CASE("unmounting something that is not mounted changes nothing and says so") {
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);
    CHECK_FALSE(catalog.unmount(kProviderB));
    CHECK_FALSE(catalog.unmount(""));
    CHECK(catalog.identities().size() == 3);
    CHECK(catalog.providers() == std::vector<std::string>{kProviderA});
}

// ---- 5. the refusals ---------------------------------------------------------

TEST_CASE("an ORDINARY second contribution to a taken identity refuses") {
    // NO AUTOMATIC PRIORITY. Load order, filesystem order, lexical order and map
    // iteration are not policy: two providers of one power with no stated intent is
    // an ambiguity nobody authored, and answering it silently is how "last loaded
    // wins" becomes undocumented behaviour.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);
    const op::OperatorDef* a3 = catalog.find(kF3);

    const op::MountResult collided = op::mount_provider(catalog, PROVIDER_B_SO);
    CHECK_FALSE(collided.ok);
    CHECK(collided.reason.find(kF3) != std::string::npos);
    CHECK(collided.reason.find(kProviderA) != std::string::npos);
    CHECK(collided.reason.find("overlay") != std::string::npos);

    // NOTHING MOVED. A refused mount leaves the catalog exactly as it was.
    CHECK_FALSE(catalog.mounted(kProviderB));
    CHECK(catalog.find(kF3) == a3);
    CHECK(catalog.contributions(kF3).size() == 1);

    // ...and asking for it deliberately is then accepted, which is what makes the
    // refusal a question rather than a wall.
    CHECK(op::mount_provider(catalog, PROVIDER_B_SO, op::MountMode::Overlay).ok);
    CHECK(active_provider(catalog, kF3) == kProviderB);
}

TEST_CASE("a signature-INCOMPATIBLE overlay refuses, however explicit it is") {
    // A different power wearing the same name. `prov.function.3` is Int -> Int here
    // and Text -> Bool there; existing compositions were authored against the
    // former, so covering it with the latter would answer a question nobody asked.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);

    const op::MountResult wrong =
        op::mount_provider(catalog, PROVIDER_WRONG_SO, op::MountMode::Overlay);
    CHECK_FALSE(wrong.ok);
    CHECK(wrong.reason.find("different signature") != std::string::npos);
    CHECK(wrong.reason.find(kF3) != std::string::npos);

    CHECK(active_provider(catalog, kF3) == kProviderA);
    CHECK(spend(catalog, kF1, kArgument) == kBaseline1);
}

TEST_CASE("a provider from another era is refused on its NUMBER, not guessed at") {
    // The version rides in a FIELD rather than in the symbol's name, because an
    // absent symbol already means something else entirely -- an artifact that
    // provides no operators -- and two different facts must not arrive as the same
    // silence. Its table is otherwise complete, so the ONLY thing wrong with it is
    // the number, and the refusal must name the number.
    op::Catalog catalog;
    const op::MountResult stale = op::mount_provider(catalog, PROVIDER_ABI_SO);
    CHECK_FALSE(stale.ok);
    CHECK(stale.reason.find("operator provider surface v") != std::string::npos);
    CHECK(stale.reason.find(std::to_string(ZENGINE_OPERATOR_PROVIDER_ABI_VERSION + 1u)) !=
          std::string::npos);
    CHECK(catalog.size() == 0);
    CHECK(catalog.providers().empty());
}

TEST_CASE("an ordinary weave is not a provider, and mounting one says exactly that") {
    // A REAL PRE-EXISTING ARTIFACT, built by another package's rules and untouched
    // by this phase. Most artifacts provide no operators; that is the floor, and it
    // has to be distinguishable from every other way a mount can fail.
    op::Catalog catalog;
    const op::MountResult ordinary = op::mount_provider(catalog, PROV_UNTOUCHED_WEAVE_SO);
    CHECK_FALSE(ordinary.ok);
    CHECK(ordinary.reason.find("exports no operator provider surface") != std::string::npos);
    CHECK(ordinary.provider.empty());
}

TEST_CASE("a path that is not there is refused by the loader's question, not the seam's") {
    op::Catalog catalog;
    const op::MountResult missing = op::mount_provider(catalog, "no-such-artifact.so");
    CHECK_FALSE(missing.ok);
    CHECK(missing.reason.find("could not open") != std::string::npos);
}

TEST_CASE("one provider cannot be mounted twice") {
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);
    const op::MountResult again = op::mount_provider(catalog, PROVIDER_A_SO);
    CHECK_FALSE(again.ok);
    CHECK(again.reason.find("already mounted") != std::string::npos);
    CHECK(catalog.contributions(kF3).size() == 1);
}

namespace {

/// A power nobody else supplies, for the atomicity witness. Namespace scope,
/// because a block-scope lambda cannot be a `make_operator<&F>` argument at all.
std::int64_t negate(std::int64_t value) { return -value; }

} // namespace

TEST_CASE("a mount is ALL OR NOTHING: a refused batch installs none of itself") {
    // WHAT A PARTIAL MOUNT WOULD LOOK LIKE, and why no artifact can produce it here:
    // a provider whose FIRST contribution is fine and whose SECOND collides. Every
    // provider in this repository supplies powers that either all collide or none
    // do, so the batch is driven directly -- which is also the smallest way to ask
    // the question, since what is under test is `Catalog::mount`'s own ordering and
    // not anything about loading.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_BASIC_SO).ok);

    std::vector<op::OperatorDef> batch;
    batch.push_back(op::make_operator<&negate>("test.negate", {"value"}, "result"));
    batch.push_back(op::make_operator<&op::max_int>(op::kMaxInt, {"lhs", "rhs"}, "result"));
    const op::MountReport refused = catalog.mount("test.batch", std::move(batch));
    CHECK_FALSE(refused.ok);
    CHECK(refused.reason.find(op::kMaxInt) != std::string::npos);

    // THE FIRST ONE IS NOT THERE. A catalog that installed as it went would carry
    // `test.negate` from a provider it does not consider mounted -- a contribution
    // with no owner, which no unmount could ever remove.
    CHECK(catalog.find("test.negate") == nullptr);
    CHECK_FALSE(catalog.mounted("test.batch"));
    CHECK(catalog.identities() == std::vector<std::string>{op::kSelectInt, op::kMaxInt});
    CHECK(active_provider(catalog, op::kMaxInt) == kBasic);
}

TEST_CASE("a provider that contributes one identity twice is refused, by name") {
    // Which of its two would be active? The question has no authored answer, so it
    // is refused rather than settled by the order the provider happened to describe
    // them in.
    op::Catalog catalog;
    std::vector<op::OperatorDef> batch;
    batch.push_back(op::make_operator<&negate>("test.negate", {"value"}, "result"));
    batch.push_back(op::make_operator<&negate>("test.negate", {"value"}, "result"));
    const op::MountReport refused = catalog.mount("test.batch", std::move(batch));
    CHECK_FALSE(refused.ok);
    CHECK(refused.reason.find("twice") != std::string::npos);
    CHECK(catalog.size() == 0);
}

TEST_CASE("a provider that contributes nothing is refused rather than mounted empty") {
    // The way this happens in practice is a provider whose own authoring threw: the
    // C seam has no other way to say so, and a host that accepted it would list a
    // provider that supplies nothing and could never be told why.
    op::Catalog catalog;
    const op::MountReport nothing = catalog.mount("test.empty", {});
    CHECK_FALSE(nothing.ok);
    CHECK(nothing.reason.find("contributes nothing") != std::string::npos);
    CHECK_FALSE(catalog.mounted("test.empty"));
}

TEST_CASE("a provider may not claim the host's own authoring, and publish still refuses") {
    // The empty provider identity means "the host authored this itself", so a
    // provider that could claim it would be unmountable -- `unmount("")` would take
    // the host's own vocabulary with it. And a host that publishes into an identity
    // a provider already supplies meets the same law from the other side.
    op::Catalog catalog;
    CHECK_FALSE(catalog.mount(std::string(), op::primitive_definitions()).ok);
    REQUIRE(op::mount_provider(catalog, PROVIDER_BASIC_SO).ok);
    CHECK_THROWS_AS(catalog.publish(op::make_operator<&op::max_int>(op::kMaxInt, {"lhs", "rhs"},
                                                                    "result")),
                    std::invalid_argument);
}

// ---- 6. one store, read twice ------------------------------------------------

namespace {

/// A host arrangement shaped like the shipped one: an empty catalog filled by
/// mounting artifacts, a surface over it, and a real Kernel underneath.
///
/// THE MEMBER ORDER IS THE LIFETIME CLAIM and it is the order `workshop.cpp`
/// writes: `catalog` and `operators` before `kernel`, so reverse-order destruction
/// takes the Kernel down first and every artifact it holds with it, while the
/// surface those artifacts point at is still alive.
struct ProviderRig {
    loom::Switchboard bus;
    op::Catalog catalog;
    op::OperatorHostSurface operators{catalog};
    loom::Kernel kernel{bus};
    loom::WeaveId control = loom::mount_control(kernel, bus);
    loom::WeaveId manager = loom::mount_manager(control, bus);
};

} // namespace

TEST_CASE("describe and evaluate resolve the SAME contribution, before and after a shadow") {
    // ONE STORE, READ TWICE. A separate descriptor cache is how a system comes to
    // describe A while executing B, and the way to not have one is to not have one:
    // both verbs below go through `Catalog::find`, at the moment they are asked.
    ProviderRig r;
    REQUIRE(op::mount_provider(r.catalog, PROVIDER_BASIC_SO).ok);
    const op::OperatorHost host = op::OperatorHost::over(r.operators.api());
    REQUIRE(host.bound());

    const op::HostSignature before = host.describe(op::kMaxInt);
    REQUIRE(before.ok());
    loom::Value pack(before.inputs);
    pack.set("lhs", loom::Cell::integer(-500));
    pack.set("rhs", loom::Cell::integer(0));
    op::HostAnswer answered = host.evaluate(before, pack);
    REQUIRE(answered.ok());
    CHECK(answered.value->at(0)->as_int() == 0);
    CHECK(active_provider(r.catalog, op::kMaxInt) == kBasic);

    REQUIRE(op::mount_provider(r.catalog, PROVIDER_MIN_SO, op::MountMode::Overlay).ok);

    // The SIGNATURE is unchanged, which is the whole reason the overlay was allowed
    // -- and the ANSWER moved, which is the whole reason it matters. A consumer
    // holding the old contract is holding an identity and two schemas, never a
    // resolution, so it spends the new contribution without being told.
    const op::HostSignature after = host.describe(op::kMaxInt);
    REQUIRE(after.ok());
    CHECK(loom::same_identity(*after.inputs, *before.inputs));
    CHECK(loom::same_identity(*after.outputs, *before.outputs));
    answered = host.evaluate(before, pack);
    REQUIRE(answered.ok());
    CHECK(answered.value->at(0)->as_int() == -500);
    CHECK(active_provider(r.catalog, op::kMaxInt) == kMinProvider);

    REQUIRE(r.catalog.unmount(kMinProvider));
    answered = host.evaluate(host.describe(op::kMaxInt), pack);
    REQUIRE(answered.ok());
    CHECK(answered.value->at(0)->as_int() == 0);
}

TEST_CASE("a power nobody supplies is NOT FOUND across the seam, in both verbs") {
    ProviderRig r;
    REQUIRE(op::mount_provider(r.catalog, PROVIDER_BASIC_SO).ok);
    const op::OperatorHost host = op::OperatorHost::over(r.operators.api());
    REQUIRE(r.catalog.unmount(kBasic));

    const op::HostSignature gone = host.describe(op::kMaxInt);
    CHECK(gone.status == ZENGINE_OP_ERR_NOT_FOUND);
    CHECK(r.catalog.find(op::kMaxInt) == nullptr);
    CHECK(r.catalog.identities().empty());
}

// ---- 7. the real Timer, and a stranger, and one replaced primitive ----------

namespace {

struct WitnessState {
    std::int64_t noted = 0;
    ZEN_SHAPE(WitnessState, 1, ZEN_FIELD(noted));
};

struct Heard {
    std::vector<std::string> results;
    std::vector<std::string> refusals;
    std::vector<tmr::TimerHandoffEntry> entries;
    std::int64_t letters = 0;
    std::vector<OperatorReadingSaid> readings;
};

/// The host's hand and ears: it commands the Manager, schedules on the Timer, asks
/// the Timer to describe itself, asks the loaded stranger to spend an operator, and
/// hears every answer.
class Witness
    : public loom::WeaveBase<
          Witness, WitnessState,
          loom::Accept<loom::Result, loom::Ack, loom::Refused, loom::Bequest, tmr::TimerFired,
                       tmr::TimerReady, tmr::TimerResolution, OperatorReadingSaid>,
          loom::Emit<loom::LoadWeave, tmr::StartTimer, loom::PrepareShutdown,
                     OperatorEvaluateAsk>> {
public:
    explicit Witness(Heard& heard) : heard_(&heard) {}

    void on(const loom::Result& r, loom::Mail&) { heard_->results.push_back(r.value); }
    void on(const loom::Ack&, loom::Mail&) {}
    void on(const loom::Refused& r, loom::Mail&) { heard_->refusals.push_back(r.reason); }
    void on(const tmr::TimerFired&, loom::Mail&) {}
    void on(const tmr::TimerReady&, loom::Mail&) {}
    void on(const tmr::TimerResolution&, loom::Mail&) {}
    void on(const OperatorReadingSaid& r, loom::Mail&) { heard_->readings.push_back(r); }

    /// THE READ THIS TIER TURNS ON. Asking a Timer to describe itself changes
    /// nothing it describes (TIMER-03), and every entry it describes carries the
    /// delay it STORED -- which is what the normalization answered, on the wire,
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

/// A production-shaped host: providers mounted first, then a surface, then a real
/// Kernel and the real Weave Manager, then the loads.
struct LiveRig : ProviderRig {
    Heard heard;
    loom::WeaveId witness{};
    std::int64_t deliveries = 0;
    std::int64_t traffic = 0;

    LiveRig() {
        loom::Grant reach;
        reach.allow(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version, manager);
        reach.allow_to_any(tmr::StartTimer::zen_name, tmr::StartTimer::zen_version);
        reach.allow_to_any(loom::PrepareShutdown::zen_name, loom::PrepareShutdown::zen_version);
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

    /// DISPATCH IN BOUNDED TURNS, never to idle: a live Timer re-arms its own beat
    /// inside its own handler, so `drain_until_idle()` would never return.
    ///
    /// THE PREDICATE IS THE ONLY STOP (QR-9). An empty turn used to end the wait too,
    /// which is an inference the substrate does not support -- `pending()` describes
    /// this instant's queue, and a deferred answer is held off it. The count is a
    /// fuse, and every caller asserts afterwards, so expiring it is a red.
    template <class Pred>
    void drain_until(Pred done, int turns = 40) {
        for (int i = 0; i < turns && !done(); ++i) {
            bus.pump_pending();
        }
    }

    /// Load the way a real Zengine host does, with the offer bracketing the whole
    /// thing -- `create()` runs several deliveries deep and no host can get between
    /// the Kernel and a constructor.
    loom::WeaveId load(const char* name, const char* path, const char* role) {
        const std::size_t ok_before = heard.results.size();
        const std::size_t no_before = heard.refusals.size();
        {
            op::OperatorOffer offering(operators, path);
            bus.send_as(witness, manager,
                        loom::Message(loom::to_value(loom::LoadWeave{name, path, role}), witness,
                                      witness, 0));
            drain_until([&] {
                return heard.results.size() > ok_before || heard.refusals.size() > no_before;
            });
        }
        if (heard.results.size() <= ok_before) {
            return loom::WeaveId{};
        }
        return loom::WeaveId{static_cast<std::uint64_t>(std::stoll(heard.results.back()))};
    }

    /// Schedule one timer on a live service and read back the delay it STORED.
    std::int64_t scheduled_delay(loom::WeaveId service, const char* id, std::int64_t delay_ms,
                                 bool repeat) {
        bus.send_as(witness, service,
                    loom::Message(loom::to_value(tmr::StartTimer{id, delay_ms, repeat}), witness,
                                  witness, 0));
        drain_until([] { return false; }, 6);
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

    /// Ask the loaded stranger what the host says the rule makes of the same two
    /// arguments. It shares no line of code with the Timer.
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
};

} // namespace

TEST_CASE("the real Timer and a loaded stranger both move when a PRIMITIVE provider is overlaid") {
    // THE PHASE'S PRODUCT CLAIM, on the real semantic path. Nothing in this case
    // rebuilds, edits or notifies the Timer artifact or the Timer provider: a third
    // artifact supplies `math.max` differently, and a rule two providers away
    // changes what a running service SCHEDULES.
    LiveRig r;
    REQUIRE(op::mount_provider(r.catalog, PROVIDER_BASIC_SO).ok);
    REQUIRE(op::mount_provider(r.catalog, TIMER_SO).ok);

    const loom::WeaveId timer = r.load("zengine-timer", TIMER_SO, tmr::kTimerRole);
    REQUIRE(timer.value != 0);
    const loom::WeaveId stranger = r.load("stranger", PROV_STRANGER_SO, "");
    REQUIRE(stranger.value != 0);

    // ---- baseline ----------------------------------------------------------
    CHECK(r.scheduled_delay(timer, "beat", -500, true) == 1);
    CHECK(r.stranger_says(stranger, "-500", "true") == "1");

    // ---- one primitive, explicitly overlaid, from a third artifact ----------
    REQUIRE(op::mount_provider(r.catalog, PROVIDER_MIN_SO, op::MountMode::Overlay).ok);
    CHECK(active_provider(r.catalog, op::kMaxInt) == kMinProvider);
    CHECK(active_provider(r.catalog, tmr::kNormalizeDelay) == kTimerProvider);

    // min(min(-500,0),1) is -500. The Timer STORED it, which is a real product
    // consequence and not a diagnostic; the stranger, in a third image, agrees.
    CHECK(r.scheduled_delay(timer, "beat2", -500, true) == -500);
    CHECK(r.stranger_says(stranger, "-500", "true") == "-500");

    // ---- and removing it restores the world --------------------------------
    REQUIRE(r.catalog.unmount(kMinProvider));
    CHECK(active_provider(r.catalog, op::kMaxInt) == kBasic);
    CHECK(r.scheduled_delay(timer, "beat3", -500, true) == 1);
    CHECK(r.stranger_says(stranger, "-500", "true") == "1");
}

TEST_CASE("a fallback Timer does NOT move, which is what makes the two rows above mean something") {
    // The control. A Timer nobody offered anything to assembles its own vocabulary
    // and spends that; overlaying the HOST's `math.max` cannot reach it, and if it
    // did, the case above would be measuring a global rather than a resolution.
    LiveRig r;
    REQUIRE(op::mount_provider(r.catalog, PROVIDER_BASIC_SO).ok);
    REQUIRE(op::mount_provider(r.catalog, TIMER_SO).ok);
    REQUIRE(op::mount_provider(r.catalog, PROVIDER_MIN_SO, op::MountMode::Overlay).ok);

    const std::size_t ok_before = r.heard.results.size();
    r.bus.send_as(r.witness, r.manager,
                  loom::Message(loom::to_value(loom::LoadWeave{"zengine-timer", TIMER_SO,
                                                               tmr::kTimerRole}),
                                r.witness, r.witness, 0));
    r.drain_until([&] { return r.heard.results.size() > ok_before; });
    REQUIRE(r.heard.results.size() > ok_before);
    const loom::WeaveId timer{static_cast<std::uint64_t>(std::stoll(r.heard.results.back()))};

    CHECK(r.scheduled_delay(timer, "beat", -500, true) == 1);
    CHECK(r.heard.refusals.empty());
}

TEST_CASE("a host-backed Timer refuses to exist where the host cannot serve its rule") {
    // NO SILENT FALLBACK, unchanged by this phase and re-proved through the provider
    // path: a host with the primitives but no domain composition is offered, the
    // Timer validates the rule it was promised, does not find it, and the load is
    // REFUSED rather than quietly becoming a Timer with its own arithmetic.
    LiveRig r;
    REQUIRE(op::mount_provider(r.catalog, PROVIDER_BASIC_SO).ok);

    const loom::WeaveId timer = r.load("zengine-timer", TIMER_SO, tmr::kTimerRole);
    CHECK(timer.value == 0);
    REQUIRE_FALSE(r.heard.refusals.empty());
    CHECK(r.heard.refusals.back().find("create() returned null") != std::string::npos);
}

// ---- 8. custody and cost -----------------------------------------------------

TEST_CASE("a provider's image is held while its contributions resolve, and released after") {
    // THE ORDER §23 ASKS FOR, measured rather than sequenced: the contributions go,
    // which is what makes the callables unreachable, and only then does the record
    // -- and the image inside it -- go. Nothing here orders that; a refcount does.
    const op::ImageCounts start = op::image_counts();
    {
        op::Catalog catalog;
        REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);
        const op::ImageCounts mountedc = op::image_counts();
        CHECK(mountedc.opens - start.opens == 1);
        CHECK(mountedc.closes - start.closes == 0);

        // Still callable, because still held.
        CHECK(spend(catalog, kF3, kArgument) == kBaseline3);

        REQUIRE(catalog.unmount(kProviderA));
        const op::ImageCounts after = op::image_counts();
        CHECK(after.closes - start.closes == 1);
        CHECK(catalog.find(kF3) == nullptr);
    }
    const op::ImageCounts end = op::image_counts();
    CHECK(end.opens - start.opens == 1);
    CHECK(end.closes - start.closes == 1);
}

TEST_CASE("...and a catalog that simply goes away releases every provider it held") {
    const op::ImageCounts start = op::image_counts();
    {
        op::Catalog catalog;
        REQUIRE(op::mount_provider(catalog, PROVIDER_BASIC_SO).ok);
        REQUIRE(op::mount_provider(catalog, TIMER_SO).ok);
        CHECK(op::image_counts().opens - start.opens == 2);
    }
    CHECK(op::image_counts().closes - start.closes == 2);
}

TEST_CASE("a refused mount closes the image it opened") {
    // The failure paths matter more than the happy one here: a record that only
    // reached the catalog on success would leak a mapping on every refusal, and
    // there are six of them.
    const op::ImageCounts start = op::image_counts();
    op::Catalog catalog;
    CHECK_FALSE(op::mount_provider(catalog, PROVIDER_ABI_SO).ok);
    CHECK_FALSE(op::mount_provider(catalog, PROV_UNTOUCHED_WEAVE_SO).ok);
    REQUIRE(op::mount_provider(catalog, PROVIDER_A_SO).ok);
    CHECK_FALSE(op::mount_provider(catalog, PROVIDER_B_SO).ok);
    const op::ImageCounts now = op::image_counts();
    CHECK(now.opens - start.opens == 4);
    CHECK(now.closes - start.closes == 3); // every one but the mount that took
}

TEST_CASE("transitive evaluation across providers costs the bus nothing") {
    // A provider node is SYNCHRONOUS COMPUTATION, not conversation. Sixteen
    // evaluations of a three-deep chain that crosses a module boundary at every leaf
    // move the bus by exactly zero, which a design that reached its provider by
    // message could not do at any price.
    LiveRig r;
    REQUIRE(op::mount_provider(r.catalog, PROVIDER_A_SO).ok);
    REQUIRE(op::mount_provider(r.catalog, PROVIDER_B_SO, op::MountMode::Overlay).ok);
    const std::int64_t before = r.deliveries;
    for (int i = 0; i < 16; ++i) {
        CHECK(spend(r.catalog, kF1, kArgument) == kOverlaid1);
    }
    CHECK(r.deliveries - before == 0);
}

TEST_CASE("a provider that answers badly is a REFUSAL, not an escape") {
    // The deepest layer that can say it. A native body may now live in another
    // image, so "the provider could not answer" has to become an evaluation's own
    // reason rather than an exception travelling out of a call whose whole contract
    // is a value or a reason. Driven directly, because no shipped provider fails.
    op::Catalog catalog;
    op::OperatorDef::Native explodes = [](const loom::Value&) -> loom::Cell {
        throw std::runtime_error("this provider's image is not answering");
    };
    catalog.publish(op::OperatorDef("test.explodes",
                                    loom::make_schema("test.explodes.in", 1,
                                                      {loom::Field{"value",
                                                                   loom::type_of(loom::Kind::Int),
                                                                   true}}),
                                    loom::make_schema("test.explodes.out", 1,
                                                      {loom::Field{"result",
                                                                   loom::type_of(loom::Kind::Int),
                                                                   true}}),
                                    std::move(explodes)));
    const op::OperatorDef* def = catalog.find("test.explodes");
    REQUIRE(def != nullptr);
    loom::Value pack(def->inputs());
    pack.set("value", loom::Cell::integer(1));
    const op::Evaluation answered = catalog.evaluate("test.explodes", std::move(pack));
    CHECK_FALSE(answered.ok());
    CHECK(answered.reason().find("could not be spent") != std::string::npos);
    CHECK(answered.reason().find("not answering") != std::string::npos);
}

// ---- 9. the production host, read as a source file ---------------------------
//
// DEFENCE IN DEPTH, AND SAID TO BE. Every case above drives a rig shaped like
// `workshop.cpp` rather than `workshop.cpp` itself, because Workshop's `main()`
// claims a terminal and this suite cannot run one. So the arrangement the product
// actually ships is read off the source, exactly as the no-privileged-wind and
// clock-binding tripwires already are: this is not a proof that the host authors
// nothing, it is a guard against the claim quietly becoming false while every rig
// here stays green.

namespace {

std::string host_source() {
    std::ifstream in(WORKSHOP_HOST_CPP);
    REQUIRE_MESSAGE(in.good(), "cannot read the host source at ", WORKSHOP_HOST_CPP);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

} // namespace

TEST_CASE("the production host AUTHORS no operator, and cannot: it names none of them") {
    const std::string host = host_source();

    // WORKSHOP KNOWS HOW TO HOST OPERATORS. IT DOES NOT KNOW WHAT ANY OF THEM MEANS.
    // Every string below is something the host would have to name in order to
    // manufacture semantics for itself, and the header that defines the delay rule
    // is not included at all.
    for (const char* forbidden : {"standard_operators", "publish_primitives",
                                  "primitive_definitions", "normalize_delay", "math.max",
                                  "logic.select_int", "make_operator", "op::Builder",
                                  "timer/normalize.hpp"}) {
        CHECK_MESSAGE(host.find(forbidden) == std::string::npos,
                      "workshop.cpp names '", forbidden, "', which is semantic authorship");
    }
}

TEST_CASE("the production host mounts providers, and does it before it offers or loads") {
    const std::string host = host_source();

    const std::size_t catalog = host.find("op::Catalog operators;");
    const std::size_t surface = host.find("op::OperatorHostSurface operator_host(operators)");
    const std::size_t kernel = host.find("loom::Kernel kernel(bus)");
    const std::size_t executor = host.find("load::PlanExecutor executor(");
    const std::size_t run = host.find("executor.run(read_plan.plan)");
    REQUIRE(catalog != std::string::npos);
    REQUIRE(surface != std::string::npos);
    REQUIRE(kernel != std::string::npos);
    REQUIRE(executor != std::string::npos);
    REQUIRE(run != std::string::npos);

    // THE CATALOG STARTS EMPTY, is dressed in the C table, and only then does a
    // Kernel exist -- which is both the lifetime claim (destruction runs in reverse)
    // and the reason the executor is declared AFTER the Kernel: it retains the
    // provider identities it mounted and must not outlive what holds them.
    CHECK(catalog < surface);
    CHECK(surface < kernel);
    CHECK(kernel < executor);
    CHECK(executor <= run);

    // ---- LOAD-0 INVERTED THE LAST CHECK OF THIS CASE ---------------------------
    //
    // It used to read: `CHECK(host.find("\"zengine-operators-basic\", \"zengine-timer\"")
    // != npos)` -- "the artifacts it mounts are named as artifacts, which is all a
    // host is allowed to know UNTIL A LOAD LIST EXISTS". A load list exists now, so
    // the host is allowed to know none of them, and the tripwire is the opposite
    // claim: NO ARTIFACT STEM APPEARS IN THIS FILE AT ALL, and neither does either
    // verb that could turn one into a running thing.
    //
    // A ROLE IS DELIBERATELY NOT ON THIS LIST. `surface::kSkinRole` and
    // `timer::kTimerRole` are still in `workshop.cpp`, inside GRANTS -- "this
    // participant may say SurfaceText to whoever holds `zengine.skin`" -- which is a
    // statement about who may be spoken to and is the host's to make. A role cannot
    // become a load; only a stem can, and forbidding roles here would be forbidding
    // this host from writing its own authority.
    for (const char* forbidden : {"zengine-operators-basic", "zengine-timer", "zengine-input",
                                  "zengine-composer", "zengine-introspection", "zengine-skin",
                                  "kComposerStem", "kIntrospectionStem",
                                  "mount_provider", "OperatorOffer", "MountMode"}) {
        CHECK_MESSAGE(host.find(forbidden) == std::string::npos,
                      "workshop.cpp names '", forbidden,
                      "', which is a load list the authored plan owns");
    }

    // ...AND THE TWO LISTS ARE ONE FILE. What the host does instead is read a plan
    // and execute it, in that order, with nothing between them it could have
    // manufactured for itself.
    const std::size_t read = host.find("load_persist::load_file(plan_path)");
    REQUIRE(read != std::string::npos);
    CHECK(read < run);
}

// ---- 10. the plan executor, read as a source file ----------------------------
//
// THE TRIPWIRE FOLLOWED THE CODE. The offer that used to bracket the Timer's boot in
// `workshop.cpp` is not there any more -- there is no Timer in `workshop.cpp` -- and
// the law it enforced did not go away with it: within one artifact record, the
// provider contribution is mounted BEFORE the weave is created, and the weave load is
// bracketed by an offer that is withdrawn afterwards. That is now `load_execute.hpp`'s
// to keep, so this is where it is read.
//
// It is a tripwire and not a proof, exactly as the one above is:
// `tests/test_workshop_load.cpp` drives the real executor over real artifacts and
// proves the BEHAVIOUR. What a source read adds is that the arrangement cannot quietly
// stop being written this way while every rig stays green.

TEST_CASE("the plan executor mounts before it offers, and offers before it loads") {
    std::ifstream in(WORKSHOP_LOAD_EXECUTE_HPP);
    REQUIRE_MESSAGE(in.good(), "cannot read the executor at ", WORKSHOP_LOAD_EXECUTE_HPP);
    std::ostringstream all;
    all << in.rdbuf();
    const std::string exec = all.str();

    const std::size_t mount = exec.find("op::mount_provider(*catalog_, path,");
    const std::size_t offer = exec.find("op::OperatorOffer offering(*operators_, path)");
    const std::size_t load = exec.find("load_weave(artifact.stem, path,");
    REQUIRE(mount != std::string::npos);
    REQUIRE(offer != std::string::npos);
    REQUIRE(load != std::string::npos);

    // THE INTRA-RECORD ORDER IS A SEMANTIC LAW and this is the one place it is
    // written: a host-backed Timer validates the rule it is about to spend inside its
    // own constructor, so the contribution has to be in the catalog before `create()`.
    CHECK(mount < offer);
    CHECK(offer < load);

    // ...and the executor authors no semantics of its own, exactly as the host does
    // not: it knows how to host what an artifact supplies and nothing about what any
    // of it means.
    for (const char* forbidden : {"normalize_delay", "math.max", "logic.select_int",
                                  "make_operator", "timer/normalize.hpp"}) {
        CHECK_MESSAGE(exec.find(forbidden) == std::string::npos,
                      "load_execute.hpp names '", forbidden, "', which is semantic authorship");
    }
}
