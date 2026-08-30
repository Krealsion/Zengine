// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A SOURCE IS A ZERO-MAKER-INPUT ENTRY IN THE ONE CATALOG (SOURCE-0) -- whether that
// is a real distinction the existing machinery already carries, or a new species.
//
// `test_operator.cpp` asks what an operator is. `test_operator_host.cpp` asks whether
// a loaded stranger can spend a host's catalog. `test_operator_canonical.cpp` asks
// whether the Timer and that stranger spend ONE. `test_operator_provider.cpp` asks
// where a host's powers came from. This one asks the next question in the same line:
// which of those entries can be spent with nothing supplied, what it costs to ask
// that, and what it costs to ask it WITHOUT spending one.
//
// THE TIERS:
//
//   1  THE PREDICATE      shape decides, never a name, never a technique: a native
//                         getter and a fully-bound composite are both Sources, a
//                         partially-bound composite is not, and `source.*` buys
//                         nothing.
//   2  THE SAMPLE         what the seam answers, and whose sentence each answer is;
//                         the pack is built from the Source's OWN input schema, which
//                         the gate is what enforces.
//   3  NOT EVALUATED      registration, mount, find, classification, enumeration,
//                         schema and provenance inspection and the contribution codec
//                         all run the body ZERO times; one explicit sample runs it
//                         exactly once. Measured on a COUNTING body, because a
//                         constant would have made an accidental spend invisible.
//   4  THE COMPOSITE      a composite with no exterior inputs samples through the
//                         ordinary walk and stays inspectable as a composite.
//   5  THE PROVIDER       a zero-input contribution crosses the real ABI out of a real
//                         image: encoded there, decoded here, mounted, sampled,
//                         unmounted -- with no ABI version change and no Loom change.
//   6  FRESH, NOT CACHED  a Source over changing owner state answers the owner at
//                         every sample, and nothing survives its provider; and every
//                         catalog law a Source was always subject to still applies to it.
//   7  THE FENCE          the seam names no bus, no Sense and no loader -- a Source runs
//                         an evaluator NOW, a Sense read returns a stored claim, and
//                         neither is being turned into the other.

#include "doctest.h"

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "operator/provider.hpp"
#include "operator/provider_host.hpp"
#include "operator/source.hpp"

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace op = zengine::op;

namespace {

// ---- the counting body, and the owner it reads ------------------------------
//
// TWO NUMBERS, AND THEY ANSWER TWO DIFFERENT QUESTIONS. `g_spends` is how many times
// the BODY ran, which is what "registration does not evaluate" is a claim about;
// `g_owner` is the state the body READS, which is what "a route, not a cached answer"
// is a claim about. A body that returned a constant would have made the first
// unfalsifiable and the second unaskable.

std::int64_t g_spends = 0;
std::int64_t g_owner = 0;

constexpr const char* kOwned = "test.source.owned";
constexpr const char* kBound = "test.source.bound";
constexpr const char* kPartial = "test.operator.partial";
/// A name that LOOKS like a Source and is not one. If classification ever consults a
/// spelling, this is the case that reddens.
constexpr const char* kNamedLikeOne = "source.looks.like.one";

std::shared_ptr<const loom::Schema> no_inputs(const std::string& identity) {
    return loom::make_schema(identity + ".in", 1, std::vector<loom::Field>());
}

std::shared_ptr<const loom::Schema> one_int_out(const std::string& identity,
                                                const std::string& port) {
    return loom::make_schema(
        identity + ".out", 1,
        std::vector<loom::Field>{loom::Field{port, loom::type_of(loom::Kind::Int), true}});
}

/// A native zero-input definition over live owner state, counting its own spends.
op::OperatorDef owned_source(const char* identity = kOwned) {
    return op::OperatorDef(identity, no_inputs(identity), one_int_out(identity, "value"),
                           [](const loom::Value&) {
                               ++g_spends;
                               return loom::Cell::integer(g_owner);
                           });
}

/// The same body behind a Source-flavoured NAME but with a maker input, so the only
/// thing that can distinguish it from `owned_source` is its shape.
op::OperatorDef named_like_a_source() {
    auto in = loom::make_schema(
        std::string(kNamedLikeOne) + ".in", 1,
        std::vector<loom::Field>{loom::Field{"n", loom::type_of(loom::Kind::Int), true}});
    return op::OperatorDef(kNamedLikeOne, std::move(in), one_int_out(kNamedLikeOne, "value"),
                           [](const loom::Value& args) {
                               ++g_spends;
                               return loom::Cell::integer(args.get("n")->as_int());
                           });
}

struct Counters {
    std::uint64_t bodies;
    std::uint64_t gates;
};

Counters counters() { return Counters{op::invocations(), loom::gate_invocations()}; }

/// A catalog with the two primitives and one owner-reading Source in it.
op::Catalog with_a_source() {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    catalog.publish(owned_source());
    return catalog;
}

/// `max(3, 7)` as a composite with NO exterior inputs -- every argument bound to a
/// constant, which is what turns an Operator into a Source without a second authoring
/// surface.
op::OperatorDef fully_bound(const op::Catalog& catalog) {
    op::Builder b(catalog, kBound, {});
    const op::Builder::Ref answer =
        b.call(op::kMaxInt, {b.constant(std::int64_t{3}), b.constant(std::int64_t{7})});
    return std::move(b).result("value", answer);
}

/// The SAME composite one binding short: `max(x, 0)` still wants an `x`.
op::OperatorDef partially_bound(const op::Catalog& catalog) {
    op::Builder b(catalog, kPartial,
                  {loom::Field{"x", loom::type_of(loom::Kind::Int), true}});
    const op::Builder::Ref answer =
        b.call(op::kMaxInt, {b.input("x"), b.constant(std::int64_t{0})});
    return std::move(b).result("value", answer);
}

std::int64_t sampled_int(const op::Catalog& catalog, const char* identity) {
    const op::Evaluation answered = op::sample(catalog, identity);
    REQUIRE_MESSAGE(answered.ok(), answered.reason());
    return answered.value().at(0)->as_int();
}

} // namespace

// ---- 1. the predicate -------------------------------------------------------

TEST_CASE("a Source is a SHAPE and not a species: zero unbound maker inputs, and nothing else") {
    op::Catalog catalog = with_a_source();
    catalog.publish(named_like_a_source());
    catalog.publish(fully_bound(catalog));
    catalog.publish(partially_bound(catalog));

    SUBCASE("a native zero-input definition is a Source") {
        CHECK(op::is_source(*catalog.find(kOwned)));
    }
    SUBCASE("an ordinary parameterized operator is not") {
        CHECK_FALSE(op::is_source(*catalog.find(op::kMaxInt)));
        CHECK_FALSE(op::is_source(*catalog.find(op::kSelectInt)));
    }
    SUBCASE("a fully-bound composite is a Source, and is still a composite") {
        const op::OperatorDef* def = catalog.find(kBound);
        REQUIRE(def != nullptr);
        CHECK(op::is_source(*def));
        // THE TWO QUESTIONS ARE INDEPENDENT and both public. If Source had been made a
        // species, this definition would have had to be one or the other.
        CHECK(def->is_composite());
        CHECK(def->composition() != nullptr);
    }
    SUBCASE("a partially-bound composite is an Operator until the last input goes") {
        const op::OperatorDef* def = catalog.find(kPartial);
        REQUIRE(def != nullptr);
        CHECK_FALSE(op::is_source(*def));
        CHECK(def->is_composite());
    }
    SUBCASE("the NAME decides nothing") {
        // Identically-named to a Source convention, and it takes an argument, so it is
        // an Operator. The reverse case is every Source above: none of them is spelled
        // `source.*` and all of them classify.
        CHECK_FALSE(op::is_source(*catalog.find(kNamedLikeOne)));
    }
    SUBCASE("there is exactly ONE store, and it holds both kinds") {
        const std::vector<std::string> identities = catalog.identities();
        CHECK(identities.size() == 6); // two primitives, two Sources, two Operators
        std::size_t sources = 0;
        for (const std::string& id : identities) {
            sources += op::is_source(*catalog.find(id)) ? 1U : 0U;
        }
        CHECK(sources == 2); // the owner-reading native and the fully-bound composite
    }
}

// ---- 2. the sample seam -----------------------------------------------------

TEST_CASE("sampling a Source spends the one evaluator and answers with an admitted value") {
    op::Catalog catalog = with_a_source();
    g_owner = 41;

    const Counters before = counters();
    const op::Evaluation answered = op::sample(catalog, kOwned);
    const Counters after = counters();

    REQUIRE_MESSAGE(answered.ok(), answered.reason());
    CHECK(answered.value().at(0)->as_int() == 41);
    // THE ANSWER IS ADMITTED AT THE SOURCE'S OWN OUTPUT SCHEMA, not merely produced.
    CHECK(loom::same_identity(answered.value().schema(), *catalog.find(kOwned)->outputs()));

    // ONE BODY, TWO GATE ADMISSIONS -- the pack in and the answer out, which is
    // `Catalog::run`'s shape and not a second policy written here. A `sample` that had
    // built its own evaluation would show a different number in one of these.
    CHECK(after.bodies - before.bodies == 1);
    CHECK(after.gates - before.gates == 2);
}

TEST_CASE("sampling refuses in the RIGHT VOCABULARY, and never by guessing an argument") {
    op::Catalog catalog = with_a_source();

    SUBCASE("an identity nobody supplies is the CATALOG's sentence, quoted") {
        const op::Evaluation answered = op::sample(catalog, "test.source.absent");
        CHECK_FALSE(answered.ok());
        // Character for character what an in-process `evaluate` says, because `sample`
        // resolves through it rather than re-wording the refusal.
        const op::Evaluation direct =
            catalog.evaluate("test.source.absent", loom::Value(no_inputs("whatever")));
        CHECK(answered.reason() == direct.reason());
        CHECK(answered.reason() == "unresolved operator reference 'test.source.absent'");
    }

    SUBCASE("a parameterized Operator is refused, and the refusal NAMES what it wanted") {
        const Counters before = counters();
        const op::Evaluation answered = op::sample(catalog, op::kMaxInt);
        const Counters after = counters();
        CHECK_FALSE(answered.ok());
        CHECK(answered.reason().find("is an operator and not a source") != std::string::npos);
        CHECK(answered.reason().find("lhs, rhs") != std::string::npos);
        // NOTHING WAS MADE UP AND NOTHING WAS SPENT: no default argument, no zero, no
        // trip through the gate to find out.
        CHECK(after.bodies == before.bodies);
        CHECK(after.gates == before.gates);
    }
}

TEST_CASE("the empty pack is built from the SOURCE'S OWN input schema, which the gate enforces") {
    op::Catalog catalog = with_a_source();
    catalog.publish(owned_source("test.source.second"));

    // TWO SOURCES, TWO EMPTY SCHEMAS, TWO CONTENT IDS. An empty schema is not a
    // universal empty value: the name is hashed into the identity the gate compares.
    const loom::Schema& mine = *catalog.find(kOwned)->inputs();
    const loom::Schema& theirs = *catalog.find("test.source.second")->inputs();
    REQUIRE(mine.fields().empty());
    REQUIRE(theirs.fields().empty());
    CHECK(mine.content_id() != theirs.content_id());

    // ...SO A GENERIC EMPTY PACK IS REFUSED BY THE DOOR IT WAS AIMED AT. This is the
    // falsifier for a `sample` that hard-coded one instead of reading the definition.
    const op::Evaluation wrong = catalog.evaluate(kOwned, loom::Value(no_inputs("test.other")));
    CHECK_FALSE(wrong.ok());
    CHECK(wrong.reason().find("refused its arguments") != std::string::npos);

    // ...and the real one is admitted.
    CHECK(op::sample(catalog, kOwned).ok());
    CHECK(op::sample(catalog, "test.source.second").ok());
}

// ---- 3. routing is not evaluation -------------------------------------------

TEST_CASE("registration, mounting, lookup, classification and description evaluate NOTHING") {
    g_spends = 0;
    g_owner = 5;
    const std::int64_t start = g_spends;

    op::Catalog catalog;
    op::publish_primitives(catalog);

    // PUBLISH -- the host's own door.
    catalog.publish(owned_source());
    // MOUNT -- a provider batch, judged and installed.
    std::vector<op::OperatorDef> batch;
    batch.push_back(owned_source("test.source.mounted"));
    REQUIRE(catalog.mount("test.provider.sources", std::move(batch)));

    // LOOKUP, CLASSIFICATION, ENUMERATION, PROVENANCE.
    const op::OperatorDef* def = catalog.find(kOwned);
    REQUIRE(def != nullptr);
    CHECK(op::is_source(*def));
    CHECK(catalog.identities().size() == 4);
    CHECK(catalog.contributions("test.source.mounted").size() == 1);
    CHECK(catalog.providers().size() == 1);

    // SCHEMA INSPECTION -- both ports, by identity, including the content ids an
    // enumeration would carry.
    CHECK(def->inputs()->fields().empty());
    CHECK(def->outputs()->name() == std::string(kOwned) + ".out");
    CHECK(def->outputs()->content_id() != 0);

    // THE CONTRIBUTION CODEC, both ways.
    const op::DecodedContribution decoded =
        op::decode_contribution(op::encode_contribution(*def));
    CHECK(decoded.identity == kOwned);

    // NOT ONE SPEND, and it is a counting body so an accidental one could not hide in
    // a plausible constant.
    CHECK(g_spends == start);

    // ...AND THEN EXACTLY ONE, when somebody actually asks.
    CHECK(sampled_int(catalog, kOwned) == 5);
    CHECK(g_spends == start + 1);
    CHECK(sampled_int(catalog, "test.source.mounted") == 5);
    CHECK(g_spends == start + 2);
}

// ---- 4. the fully-bound composite -------------------------------------------

TEST_CASE("a composite with no exterior inputs samples through the ORDINARY walk") {
    op::Catalog catalog;
    op::publish_primitives(catalog);
    catalog.publish(fully_bound(catalog));

    const op::OperatorDef* def = catalog.find(kBound);
    REQUIRE(def != nullptr);
    CHECK(op::is_source(*def));
    CHECK(def->is_composite());

    const Counters before = counters();
    CHECK(sampled_int(catalog, kBound) == 7); // max(3, 7)
    const Counters after = counters();

    // ONE NATIVE BODY RAN -- `math.max`'s. The composite is not an arithmetic step and
    // is not counted as one, so a Source-specific composite runtime would show up here
    // as a different number.
    CHECK(after.bodies - before.bodies == 1);

    // AND IT IS STILL A COMPOSITE, inspectable as data: one node, naming a leaf by
    // IDENTITY, resolved at the spend like every other.
    REQUIRE(def->composition() != nullptr);
    CHECK(def->composition()->nodes.size() == 1);
    CHECK(def->composition()->nodes[0].identity == op::kMaxInt);

    SUBCASE("its leaf still resolves at the spend -- unmounting one refuses the Source") {
        op::Catalog fresh;
        std::vector<op::OperatorDef> primitives = op::primitive_definitions();
        REQUIRE(fresh.mount("test.primitives", std::move(primitives)));
        fresh.publish(fully_bound(fresh));
        CHECK(sampled_int(fresh, kBound) == 7);

        REQUIRE(fresh.unmount("test.primitives"));
        const op::Evaluation orphaned = op::sample(fresh, kBound);
        CHECK_FALSE(orphaned.ok());
        CHECK(orphaned.reason().find("unresolved operator reference 'math.max'") !=
              std::string::npos);
        // ITS OWN REGISTRATION SURVIVED: a Source is data in the store, and what went
        // away is what its graph names.
        CHECK(op::is_source(*fresh.find(kBound)));
    }
}

// ---- 5. across the provider ABI ---------------------------------------------

TEST_CASE("a zero-input contribution round-trips the contribution codec with a stable identity") {
    const op::OperatorDef def = owned_source();
    const op::DecodedContribution decoded =
        op::decode_contribution(op::encode_contribution(def));

    CHECK(decoded.identity == kOwned);
    CHECK_FALSE(decoded.composition.has_value()); // native: the body stays in its image
    // ⚠ `static_cast<bool>` RATHER THAN `!= nullptr`, and it is not style. A doctest
    // expression captures both sides and stringifies them; MSVC's `<memory>` supplies an
    // `operator<<` for `shared_ptr` that forwards to the raw pointer, and doctest's own
    // `char*` overload then hides the built-in `const void*` one, so the comparison does
    // not compile there at all. A bool has nothing to stringify.
    REQUIRE(static_cast<bool>(decoded.inputs));
    REQUIRE(static_cast<bool>(decoded.outputs));
    // THE EMPTY INPUT SCHEMA IS AN IDENTITY AND SURVIVES AS ONE. `same_identity` is
    // name, version and content id -- an empty schema rebuilt with a different name on
    // the far side would compare equal on field count and fail here.
    CHECK(decoded.inputs->fields().empty());
    CHECK(loom::same_identity(*decoded.inputs, *def.inputs()));
    CHECK(loom::same_identity(*decoded.outputs, *def.outputs()));
}

TEST_CASE("a real provider ARTIFACT contributes a Source, and mounting it does not spend it") {
    op::Catalog catalog;
    const op::MountResult mounted = op::mount_provider(catalog, PROVIDER_SOURCE_SO);
    REQUIRE_MESSAGE(mounted.ok, mounted.reason);
    CHECK(mounted.provider == "zengine.provider.source");
    CHECK(mounted.contributed == 2);

    const op::OperatorDef* source = catalog.find("prov.source.spends");
    const op::OperatorDef* operator_ = catalog.find("prov.source.doubled");
    REQUIRE(source != nullptr);
    REQUIRE(operator_ != nullptr);

    SUBCASE("the shape crossed, so the classification crossed with it") {
        CHECK(op::is_source(*source));
        CHECK_FALSE(op::is_source(*operator_));
        // A provider may contribute both; there is no Source contribution format and
        // no Source ABI for it to have used.
        CHECK(source->inputs()->fields().empty());
        CHECK(source->outputs()->fields().size() == 1);
    }

    SUBCASE("mount, describe and enumerate ran the far-side body ZERO times") {
        // The body in that image counts its own spends and answers the count, so the
        // FIRST sample answering 1 is the proof that nothing before it ran the body.
        // `op::invocations()` cannot see across an image and is not asked to.
        (void)catalog.identities();
        (void)catalog.contributions("prov.source.spends");
        (void)op::encode_contribution(*source);
        CHECK(sampled_int(catalog, "prov.source.spends") == 1);
        CHECK(sampled_int(catalog, "prov.source.spends") == 2);
    }

    SUBCASE("its parameterized sibling refuses sampling from the same seam") {
        const op::Evaluation answered = op::sample(catalog, "prov.source.doubled");
        CHECK_FALSE(answered.ok());
        CHECK(answered.reason().find("is an operator and not a source") != std::string::npos);
        CHECK(answered.reason().find("value") != std::string::npos);
    }

    SUBCASE("unmounting takes the Source with it, and nothing was retained") {
        REQUIRE(catalog.unmount("zengine.provider.source"));
        const op::Evaluation answered = op::sample(catalog, "prov.source.spends");
        CHECK_FALSE(answered.ok());
        CHECK(answered.reason() == "unresolved operator reference 'prov.source.spends'");
    }
}

// ---- 6. a route, never a cached answer --------------------------------------

TEST_CASE("a Source over changing owner state answers the OWNER at every sample") {
    op::Catalog catalog = with_a_source();

    g_owner = 11;
    CHECK(sampled_int(catalog, kOwned) == 11);

    g_owner = 22; // the owner changed; nobody told the catalog and nobody needed to
    CHECK(sampled_int(catalog, kOwned) == 22);

    g_owner = -7;
    CHECK(sampled_int(catalog, kOwned) == -7);

    // THE SOURCE WAS REGISTERED ONCE. Nothing re-published, re-mounted or refreshed it
    // between those three answers -- what the catalog holds is a route, and the answer
    // is made at the moment somebody asks for one.
    CHECK(catalog.contributions(kOwned).size() == 1);
}

TEST_CASE("a sampled answer is INERT: it is a copy, and it is nobody's live binding") {
    op::Catalog catalog = with_a_source();
    g_owner = 100;
    const op::Evaluation first = op::sample(catalog, kOwned);
    REQUIRE(first.ok());

    g_owner = 200;
    // THE HELD ANSWER DID NOT MOVE. It is an ordinary `loom::Value` and carries no
    // subscription, no watcher and no promise about the world past the moment it was
    // produced -- which is the whole of what a sample claims.
    CHECK(first.value().at(0)->as_int() == 100);
    CHECK(sampled_int(catalog, kOwned) == 200);
}

TEST_CASE("an overlay replaces what a Source answers, and unmounting reveals the original") {
    op::Catalog catalog = with_a_source();
    g_owner = 3;
    CHECK(sampled_int(catalog, kOwned) == 3);

    // A SECOND CONTRIBUTION AT THE SAME SIGNATURE, deliberately covering the first --
    // ordinary catalog law, unchanged, and Sources get no exemption from it.
    std::vector<op::OperatorDef> cover;
    cover.push_back(op::OperatorDef(kOwned, no_inputs(kOwned), one_int_out(kOwned, "value"),
                                    [](const loom::Value&) {
                                        ++g_spends;
                                        return loom::Cell::integer(99);
                                    }));
    REQUIRE(catalog.mount("test.provider.cover", std::move(cover), op::MountMode::Overlay));
    CHECK(sampled_int(catalog, kOwned) == 99);

    REQUIRE(catalog.unmount("test.provider.cover"));
    CHECK(sampled_int(catalog, kOwned) == 3);

    SUBCASE("an ordinary second contribution to a taken identity is still refused") {
        std::vector<op::OperatorDef> collide;
        collide.push_back(owned_source());
        const op::MountReport refused = catalog.mount("test.provider.collide", std::move(collide));
        CHECK_FALSE(refused.ok);
        CHECK(refused.reason.find("needs an explicit overlay") != std::string::npos);
    }
}

TEST_CASE("a Source is subject to every catalog law it always was, drift included") {
    op::Catalog catalog = with_a_source();
    g_owner = 3;
    CHECK(sampled_int(catalog, kOwned) == 3);

    SUBCASE("an overlay at a DIFFERENT signature is refused, in the catalog's own words") {
        // SCHEMA DRIFT STAYS DETECTABLE BY THE LAW THAT ALREADY DETECTS IT. A Source's
        // ports are two `loom::Schema`s like any operator's, so covering one at another
        // shape is a different power wearing a taken name -- and being a Source buys no
        // exemption from that.
        std::vector<op::OperatorDef> reshaped;
        reshaped.push_back(op::OperatorDef(
            kOwned, no_inputs(kOwned),
            loom::make_schema(std::string(kOwned) + ".out", 1,
                              std::vector<loom::Field>{
                                  loom::Field{"value", loom::type_of(loom::Kind::Text), true}}),
            [](const loom::Value&) { return loom::Cell::text("not an integer"); }));
        const op::MountReport refused =
            catalog.mount("test.provider.reshaped", std::move(reshaped), op::MountMode::Overlay);
        CHECK_FALSE(refused.ok);
        CHECK(refused.reason.find("at a different signature") != std::string::npos);
        // ...AND THE ORIGINAL IS UNTOUCHED, because a mount is all or nothing.
        CHECK(sampled_int(catalog, kOwned) == 3);
    }

    SUBCASE("a composition that named it detects a re-shaped replacement rather than binding") {
        // The authored-signature law is the composite's, not the Source's, and it applies to
        // a node naming a Source exactly as it applies to one naming an operator.
        op::Builder b(catalog, "test.source.over_a_source", {});
        const op::Builder::Ref answer =
            b.call(op::kMaxInt, {b.call(kOwned, {}), b.constant(std::int64_t{0})});
        catalog.publish(std::move(b).result("value", answer));
        CHECK(op::is_source(*catalog.find("test.source.over_a_source")));
        CHECK(sampled_int(catalog, "test.source.over_a_source") == 3);

        g_owner = -5;
        CHECK(sampled_int(catalog, "test.source.over_a_source") == 0); // max(-5, 0)

        // Take the leaf away and put back something with the same identity and another
        // shape -- an unmount/remount era, which is the only way a live identity's ports
        // can change at all.
        std::vector<op::OperatorDef> replacement;
        replacement.push_back(op::OperatorDef(
            op::kMaxInt,
            loom::make_schema("math.max.in", 2,
                              std::vector<loom::Field>{
                                  loom::Field{"lhs", loom::type_of(loom::Kind::Int), true},
                                  loom::Field{"rhs", loom::type_of(loom::Kind::Int), true}}),
            loom::make_schema("math.max.out", 1,
                              std::vector<loom::Field>{
                                  loom::Field{"result", loom::type_of(loom::Kind::Int), true}}),
            [](const loom::Value&) { return loom::Cell::integer(0); }));
        op::Catalog era;
        era.publish(owned_source());
        REQUIRE(era.mount("test.provider.v2", std::move(replacement)));
        era.publish(*catalog.find("test.source.over_a_source"));

        const op::Evaluation drifted = op::sample(era, "test.source.over_a_source");
        CHECK_FALSE(drifted.ok());
        CHECK(drifted.reason().find("not the signature this composition was authored against") !=
              std::string::npos);
    }
}

// ---- 7. the fence: a Source is not a Sense and not a message ------------------

TEST_CASE("the Source seam names no bus, no Sense and no loader") {
    // DEFENCE IN DEPTH, AND SAID TO BE. This suite links `loom::switchboard` for the OPH-0
    // tier, so no link line can carry this claim -- only reading the file can, which is
    // exactly the shape the stranger fence next door has.
    //
    // WHAT IT PROTECTS. A Sense read returns an owner's already-stored claim and runs no
    // owner code; a Source sample runs the evaluator NOW. Both are honest and neither
    // substitutes for the other, so a bridge between them would be one surface with two
    // freshness laws inside it -- and it would hand the catalog a bus dependency and an
    // authorization question it has never had. A `Switchboard` reaching this header is the
    // first line of that, and it is the line this case exists to notice.
    std::ifstream in(OPERATOR_SOURCE_HPP);
    REQUIRE_MESSAGE(in.good(), "cannot read the Source seam at ", OPERATOR_SOURCE_HPP);
    std::ostringstream all;
    all << in.rdbuf();
    const std::string seam = all.str();

    for (const char* forbidden : {"Switchboard", "switchboard.hpp", "observe_as", "claim_as",
                                  "SenseAuthorship", "Claims<", "loom::Mail", "loom::Kernel",
                                  "zen/weave.hpp", "dlopen", "LoadLibrary"}) {
        CHECK_MESSAGE(seam.find(forbidden) == std::string::npos, "operator/source.hpp names '",
                      forbidden,
                      "', which is a participant, a claim or a loader reaching a projection "
                      "over the catalog");
    }

    // ...AND SAMPLING COSTS THE BUS NOTHING, measured rather than read: a whole round trip
    // through the Source seam runs in a process with no Switchboard in it at all. Every case
    // in this file has been doing that; this is the sentence that says so.
    op::Catalog catalog = with_a_source();
    g_owner = 77;
    CHECK(sampled_int(catalog, kOwned) == 77);
}
