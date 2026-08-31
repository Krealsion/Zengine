// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A MIGRATION IS AN ORDINARY OPERATOR WHOSE SIGNATURE IS THE EDGE (MIG-0) -- whether that
// is true of the machinery, or a sentence a report wrote.
//
// `test_operator.cpp` asks what an operator is. `test_operator_host.cpp` asks whether a
// loaded stranger can spend a host's catalog. `test_operator_canonical.cpp` asks whether
// the Timer and that stranger spend ONE. `test_operator_provider.cpp` asks where a host's
// powers came from. `test_operator_source.cpp` asks which of them can be spent with nothing
// supplied. This one asks the next question in the same line: what it takes for one of them
// to convert YESTERDAY'S BYTES, and what a durable file may cause by claiming an old
// version.
//
// THE TIERS:
//
//   1  THE CONVENTION     an edge is read off two schemas and nothing else; the identity is
//                         DERIVED from the edge, so two providers of one edge collide at
//                         mount rather than becoming an ambiguity met at a spend.
//   2  THE LOOKUP         one direct edge, spent through the one gate; what each of the
//                         five refusals says, and whose sentence it is.
//   3  NO ROUTE           `v1 -> v2` and `v2 -> v3` mounted, `v1 -> v3` wanted, and the
//                         answer is a refusal -- then the SAME reader is satisfied the
//                         moment somebody authors the direct edge, with nothing else
//                         changed.
//   4  THE SIGNATURE      a contribution whose NAME says one edge while its schemas say
//                         another is not spent; nor is one that answers the right name at
//                         the wrong shape.
//   5  LIFETIME           mounted, unmounted, lawfully covered: every spend resolves the
//                         catalog as it is at that instant, and nothing survives a
//                         provider.
//   6  NO AUTHORITY       a version claim opens no image, mounts nothing and realizes
//                         nothing -- measured on the image ledger, not asserted.
//   7  THE FENCE          the seam names no loader, no plan and no filesystem.

#include "doctest.h"

#include "migration_family.hpp"

#include "operator/catalog.hpp"
#include "operator/migration.hpp"
#include "operator/operator.hpp"
#include "operator/primitives.hpp"
#include "operator/provider_host.hpp"

#include <zen/gate.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace op = zengine::op;

namespace {

std::shared_ptr<const loom::Schema> rung_v1() { return loom::schema_of<mig_fixture::v1::Rung>(); }
std::shared_ptr<const loom::Schema> rung_v2() { return loom::schema_of<mig_fixture::v2::Rung>(); }
std::shared_ptr<const loom::Schema> rung_v3() { return loom::schema_of<mig_fixture::v3::Rung>(); }

/// A version-1 file's bytes, as a durable artifact's owner would hold them: parsed, not
/// admitted, revealing only what they CLAIM to be.
loom::Unverified old_bytes(std::int64_t carried) {
    mig_fixture::v1::Rung was;
    was.carried = carried;
    return loom::parse(loom::serialize(loom::to_value(was)));
}

/// The same, in the compat codec every Workshop durable file is written in -- so a case can
/// say that the seam is about a CLAIM rather than about an encoding.
loom::Unverified old_text(std::int64_t carried) {
    mig_fixture::v1::Rung was;
    was.carried = carried;
    return loom::compat::parse(loom::compat::serialize(loom::to_value(was)));
}

mig_fixture::v3::Rung answered(const op::Evaluation& e) {
    return loom::from_value<mig_fixture::v3::Rung>(op::migrated(e));
}

std::string slurp(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

} // namespace

// ---- 1. The convention -------------------------------------------------------------

TEST_CASE("MIG-0: an edge is two schemas, and the identity is derived from them") {
    // THE SPELLING IS ONE FUNCTION OF THE EDGE, so a provider and a consumer cannot
    // disagree about it by each writing a string.
    CHECK(op::migration_identity(*rung_v1(), *rung_v3()) == "zengine.migrate.Rung.v1-to-v3");
    CHECK(op::migration_identity("WorkshopSession", 1, 3) ==
          "zengine.migrate.WorkshopSession.v1-to-v3");
    // ...and the two overloads are the same function, said two ways.
    CHECK(op::migration_identity(*rung_v2(), *rung_v3()) ==
          op::migration_identity("Rung", 2, 3));

    const op::OperatorDef edge = op::make_migration(
        rung_v1(), rung_v3(), [](const loom::Value&) { return loom::Cell::integer(0); });

    // THE INPUT IS THE HISTORICAL SHAPE ITSELF -- not a port list wrapping one. That is what
    // lets a durable file's own bytes be admitted at it, because the gate asks the claim to
    // name the door.
    CHECK(loom::same_identity(*edge.inputs(), *rung_v1()));
    // THE ANSWER IS ONE PORT CARRYING THE TARGET, because an output schema is a port list
    // everywhere in this package and a conversion is not an exception to that.
    REQUIRE(edge.outputs()->fields().size() == 1);
    CHECK(edge.outputs()->fields()[0].name == std::string(op::kMigrationPort));
    REQUIRE(op::migration_target(edge) != nullptr);
    CHECK(loom::same_identity(*op::migration_target(edge), *rung_v3()));
    CHECK(edge.identity() == "zengine.migrate.Rung.v1-to-v3");
    CHECK(op::declares_migration(edge));
}

TEST_CASE("MIG-0: an ordinary operator is not a conversion, and is not judged as a bad one") {
    // `math.max`'s ports are `math.max.in` / `math.max.out` -- two different names, so the
    // predicate cannot match it. Nothing derives this from a naming convention.
    for (const op::OperatorDef& def : op::primitive_definitions()) {
        CAPTURE(def.identity());
        CHECK_FALSE(op::declares_migration(def));
        CHECK(op::migration_target(def) == nullptr);
    }
    // ...nor does an operator whose answer IS a message of its own input: that is the same
    // identity on both sides, which is a copy and not an edge.
    const op::OperatorDef copy("fixture.copy", rung_v3(),
                               op::migration_answer_schema("fixture.copy", rung_v3()),
                               [](const loom::Value& v) { return loom::Cell::message(v); });
    CHECK(op::migration_target(copy) != nullptr);
    CHECK_FALSE(op::declares_migration(copy));
}

TEST_CASE("MIG-0: two providers of ONE edge collide at mount, not at a spend") {
    // ⭐ WHAT THE DERIVED IDENTITY BUYS. Ambiguity is a maker-visible refusal at the moment
    // an arrangement is composed, in the catalog's own words -- rather than a question
    // somebody's session file has to answer months later.
    op::Catalog catalog;
    const auto body = [](const loom::Value&) { return loom::Cell::integer(0); };
    std::vector<op::OperatorDef> first;
    first.push_back(op::make_migration(rung_v1(), rung_v3(), body));
    REQUIRE(catalog.mount("one", std::move(first)));

    std::vector<op::OperatorDef> second;
    second.push_back(op::make_migration(rung_v1(), rung_v3(), body));
    const op::MountReport clash = catalog.mount("two", std::move(second));
    CHECK_FALSE(clash.ok);
    CHECK(clash.reason.find("zengine.migrate.Rung.v1-to-v3") != std::string::npos);
    CHECK(clash.reason.find("one") != std::string::npos);
    CHECK_FALSE(catalog.mounted("two"));
}

// ---- 2. The lookup, and its five refusals -------------------------------------------

TEST_CASE("MIG-0: a mounted direct edge is found by one lookup and spent through the gate") {
    op::Catalog catalog;
    const op::MountResult mounted = op::mount_provider(catalog, PROVIDER_MIG_DIRECT_SO);
    REQUIRE_MESSAGE(mounted.ok, mounted.reason);

    const std::uint64_t gates_before = loom::gate_invocations();
    const op::Evaluation converted = op::migrate(catalog, old_bytes(41), rung_v3());
    REQUIRE_MESSAGE(converted.ok(), converted.reason());

    // THE ANSWER IS THE TARGET SHAPE, admitted, and it says which road ran.
    CHECK(loom::same_identity(op::migrated(converted).schema(), *rung_v3()));
    CHECK(answered(converted).carried == 41);
    CHECK(answered(converted).note == mig_fixture::v1_note());
    CHECK(answered(converted).rungs == mig_fixture::kDirectRungs);
    // BOTH GATES RAN. The bytes were admitted at the OLD shape and the answer at the new
    // one; a seam that decoded the file itself would have skipped the first.
    CHECK(loom::gate_invocations() > gates_before);
}

TEST_CASE("MIG-0: the compat codec a durable file is written in changes nothing") {
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_MIG_DIRECT_SO).ok);
    const op::Evaluation converted = op::migrate(catalog, old_text(7), rung_v3());
    REQUIRE_MESSAGE(converted.ok(), converted.reason());
    CHECK(answered(converted).carried == 7);
    // The seam is about a CLAIM, and both codecs carry one.
}

TEST_CASE("MIG-0: what is NOT a migration question, and never becomes one") {
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_MIG_DIRECT_SO).ok);

    SUBCASE("bytes that are not a Zen value at all") {
        const op::Evaluation no = op::migrate(catalog, loom::parse("not bytes"), rung_v3());
        CHECK_FALSE(no.ok());
        CHECK(no.reason().find("not a Zen value") != std::string::npos);
    }
    SUBCASE("another shape entirely -- the wrong file, not an old one") {
        mig_fixture::v3::Rung right_family;
        const op::OperatorDef unrelated = op::make_migration(
            rung_v1(), rung_v2(), [](const loom::Value&) { return loom::Cell::integer(0); });
        (void)unrelated;
        (void)right_family;
        const loom::Unverified elsewhere =
            loom::parse(loom::serialize(loom::to_value(mig_fixture::v2::Rung{})));
        // ...asked against a target of a DIFFERENT NAME.
        const auto other_family = loom::make_schema(
            "Ladder", 3, std::vector<loom::Field>{loom::Field{
                            "carried", loom::type_of(loom::Kind::Int), true}});
        const op::Evaluation no = op::migrate(catalog, elsewhere, other_family);
        CHECK_FALSE(no.ok());
        CHECK(no.reason().find("not a version of `Ladder`") != std::string::npos);
    }
    SUBCASE("a CURRENT value needs no conversion and asks for none") {
        const loom::Unverified current =
            loom::parse(loom::serialize(loom::to_value(mig_fixture::v3::Rung{})));
        const op::Evaluation no = op::migrate(catalog, current, rung_v3());
        CHECK_FALSE(no.ok());
        CHECK(no.reason().find("needs no conversion") != std::string::npos);
    }
}

TEST_CASE("MIG-0: with no conversion live, an old claim gets an honest refusal and no more") {
    const op::Catalog empty;
    const op::Evaluation no = op::migrate(empty, old_bytes(3), rung_v3());
    CHECK_FALSE(no.ok());
    // IT NAMES THE MISSING POWER and claims nothing else -- not which artifact would supply
    // it, not that one exists on disk, not that anybody should install anything.
    CHECK(no.reason() == "no live conversion from `Rung` v1 to v3 "
                         "(`zengine.migrate.Rung.v1-to-v3`)");
    CHECK(no.reason().find("install") == std::string::npos);
    CHECK(no.reason().find("disk") == std::string::npos);

    // AND A HOST WITH NO CATALOG AT ALL SAYS THE SAME SENTENCE, because to the file being
    // read the two are one fact.
    const op::Catalog* none = nullptr;
    CHECK(op::migrate(none, old_bytes(3), rung_v3()).reason() == no.reason());
    // ...while a question that was never a migration question keeps its own answer.
    CHECK(op::migrate(none, loom::parse("junk"), rung_v3()).reason().find("not a Zen value") !=
          std::string::npos);
}

// ---- 3. No route, and what authorship buys ------------------------------------------

TEST_CASE("MIG-0/SC-10: two edges that meet in the middle are not a third edge") {
    // ⭐ THE CENTRAL REFUSAL. `v1 -> v2` and `v2 -> v3` are both live, both mounted from a
    // real artifact, and a reader that wants v3 out of a v1 value is REFUSED -- because the
    // road it needs is one nobody wrote down, and a searched multi-hop is a result no
    // participant authored.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_MIG_CHAIN_SO).ok);
    REQUIRE(catalog.find("zengine.migrate.Rung.v1-to-v2") != nullptr);
    REQUIRE(catalog.find("zengine.migrate.Rung.v2-to-v3") != nullptr);
    REQUIRE(catalog.find("zengine.migrate.Rung.v1-to-v3") == nullptr);

    const op::Evaluation no = op::migrate(catalog, old_bytes(5), rung_v3());
    CHECK_FALSE(no.ok());
    CHECK(no.reason() == "no live conversion from `Rung` v1 to v3 "
                         "(`zengine.migrate.Rung.v1-to-v3`)");

    // AND THE HALF-STEP IS STILL THERE, so the refusal is about the ROUTE and not about the
    // edges: asking for v2 out of the same value succeeds.
    const op::Evaluation half = op::migrate(catalog, old_bytes(5), rung_v2());
    REQUIRE_MESSAGE(half.ok(), half.reason());
    CHECK(loom::from_value<mig_fixture::v2::Rung>(op::migrated(half)).carried == 5);
}

TEST_CASE("MIG-0/SC-10: authoring the direct edge satisfies the same reader, unchanged") {
    // ⭐ THE OTHER HALF: nothing about the consumer moves. The same call, against a catalog
    // that now holds an edge somebody WROTE, answers -- and says it came in one rung.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_MIG_CHAIN_SO).ok);
    REQUIRE_FALSE(op::migrate(catalog, old_bytes(9), rung_v3()).ok());

    REQUIRE(op::mount_provider(catalog, PROVIDER_MIG_DIRECT_SO).ok);
    const op::Evaluation now = op::migrate(catalog, old_bytes(9), rung_v3());
    REQUIRE_MESSAGE(now.ok(), now.reason());
    CHECK(answered(now).carried == 9);
    CHECK(answered(now).rungs == mig_fixture::kDirectRungs);
}

TEST_CASE("MIG-0: an authored edge may be a COMPOSITION, and the seam does not care") {
    // A migration is an ordinary operator, so it may be a graph over other identities --
    // which crosses the provider seam as STRUCTURE and is walked by this host's own
    // evaluator. The seam resolves the EDGE; what the edge is made of is the author's.
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_MIG_COMPOSED_SO).ok);
    const op::OperatorDef* edge = catalog.find("zengine.migrate.Rung.v1-to-v3");
    REQUIRE(edge != nullptr);
    CHECK(edge->is_composite());
    CHECK(op::declares_migration(*edge));

    const op::Evaluation composed = op::migrate(catalog, old_bytes(4), rung_v3());
    REQUIRE_MESSAGE(composed.ok(), composed.reason());
    CHECK(answered(composed).carried == 4);
    CHECK(answered(composed).rungs == mig_fixture::kComposedRungs);
}

// ---- 4. The signature is the proof, the name is diagnostic ---------------------------

TEST_CASE("MIG-0: a name that says one edge over schemas that say another is not spent") {
    // ⭐ THE HOSTILE CONTRIBUTION. `make_migration` derives the name from the schemas, so an
    // honest provider cannot build this; a hand-built definition can, and it is refused by
    // the only thing that could catch it -- what its ports actually declare.
    op::Catalog catalog;
    const std::string identity = op::migration_identity("Rung", 1, 3);
    std::vector<op::OperatorDef> liar;
    liar.push_back(op::OperatorDef(identity, rung_v1(),
                                   op::migration_answer_schema(identity, rung_v2()),
                                   [](const loom::Value&) {
                                       return loom::Cell::message(
                                           loom::to_value(mig_fixture::v2::Rung{}));
                                   }));
    REQUIRE(catalog.mount("liar", std::move(liar)));
    REQUIRE(catalog.find(identity) != nullptr); // it IS there, under that exact name

    const op::Evaluation no = op::migrate(catalog, old_bytes(1), rung_v3());
    CHECK_FALSE(no.ok());
    CHECK(no.reason().find("answers with `Rung` v2") != std::string::npos);
    CHECK(no.reason().find("not the shape this reader admits") != std::string::npos);
}

TEST_CASE("MIG-0: the right name and version at the WRONG SHAPE is not spent either") {
    // A provider built against another era's `Rung v3`. The name matches, the version
    // matches, and the content id does not -- which `same_identity` is exactly what catches,
    // so a maker is told about the SHAPE rather than about a missing field three layers down.
    op::Catalog catalog;
    const auto other_v3 = loom::make_schema(
        "Rung", 3,
        std::vector<loom::Field>{loom::Field{"carried", loom::type_of(loom::Kind::Int), true}});
    REQUIRE_FALSE(loom::same_identity(*other_v3, *rung_v3()));
    const std::string identity = op::migration_identity("Rung", 1, 3);
    std::vector<op::OperatorDef> wrong;
    wrong.push_back(op::OperatorDef(identity, rung_v1(),
                                    op::migration_answer_schema(identity, other_v3),
                                    [other_v3](const loom::Value&) {
                                        loom::Value v(other_v3);
                                        v.set("carried", loom::Cell::integer(0));
                                        return loom::Cell::message(std::move(v));
                                    }));
    REQUIRE(catalog.mount("wrong-shape", std::move(wrong)));

    const op::Evaluation no = op::migrate(catalog, old_bytes(1), rung_v3());
    CHECK_FALSE(no.ok());
    CHECK(no.reason().find("not the shape this reader admits") != std::string::npos);
}


TEST_CASE("MIG-0: a conversion at the right name converting the WRONG VINTAGE is not spent") {
    // ⚠ FOUND BY A MUTATION, not by design (MIG-0's own matrix). The two hostile
    // contributions above lie about their ANSWER; this one lies about what it reads. Removing
    // the source check left the suite green, because the gate refuses the pack anyway -- so
    // what was untested was not whether the file is safe (it is) but whether the maker is
    // told the useful thing: `evaluate` would say the bytes claim a different schema than
    // this door, which is true and says nothing about the conversion that was wrong.
    op::Catalog catalog;
    const std::string identity = op::migration_identity("Rung", 1, 3);
    std::vector<op::OperatorDef> wrong_source;
    wrong_source.push_back(op::OperatorDef(
        identity, rung_v2(), op::migration_answer_schema(identity, rung_v3()),
        [](const loom::Value&) {
            return loom::Cell::message(loom::to_value(mig_fixture::v3::Rung{}));
        }));
    REQUIRE(catalog.mount("wrong-source", std::move(wrong_source)));

    const op::Evaluation no = op::migrate(catalog, old_bytes(1), rung_v3());
    CHECK_FALSE(no.ok());
    CHECK(no.reason() == "`" + identity + "` converts `Rung` v2, not `Rung` v1");
    // ...and the seam said it, rather than leaving the gate to complain about a door.
    CHECK(no.reason().find("different schema than this door") == std::string::npos);
}
TEST_CASE("MIG-0: an operator answering that name which is no conversion at all is refused") {
    op::Catalog catalog;
    const std::string identity = op::migration_identity("Rung", 1, 3);
    std::vector<op::OperatorDef> plain;
    plain.push_back(op::OperatorDef(
        identity, rung_v1(),
        loom::make_schema(identity + ".out", 1,
                          std::vector<loom::Field>{
                              loom::Field{"result", loom::type_of(loom::Kind::Int), true}}),
        [](const loom::Value&) { return loom::Cell::integer(0); }));
    REQUIRE(catalog.mount("not-a-conversion", std::move(plain)));

    const op::Evaluation no = op::migrate(catalog, old_bytes(1), rung_v3());
    CHECK_FALSE(no.ok());
    CHECK(no.reason().find("is not a conversion") != std::string::npos);
}

TEST_CASE("MIG-0: a conversion that refuses says so in ITS OWN words, through the gate") {
    op::Catalog catalog;
    const std::string identity = op::migration_identity("Rung", 1, 3);
    std::vector<op::OperatorDef> cross;
    cross.push_back(op::make_migration(rung_v1(), rung_v3(), [](const loom::Value&) -> loom::Cell {
        throw std::invalid_argument("`barns` is not a unit anybody wrote down");
    }));
    REQUIRE(catalog.mount("refuses", std::move(cross)));

    const op::Evaluation no = op::migrate(catalog, old_bytes(1), rung_v3());
    CHECK_FALSE(no.ok());
    // The evaluator contains the throw and names the identity; yesterday's vocabulary
    // survives the trip.
    CHECK(no.reason().find(identity) != std::string::npos);
    CHECK(no.reason().find("`barns`") != std::string::npos);
}

// ---- 5. Resolve at spend: mounted, unmounted, covered ---------------------------------

TEST_CASE("MIG-0/SC-11: unmounting a provider removes its edge, with nothing left behind") {
    op::Catalog catalog;
    const op::MountResult mounted = op::mount_provider(catalog, PROVIDER_MIG_DIRECT_SO);
    REQUIRE(mounted.ok);
    REQUIRE(op::migrate(catalog, old_bytes(2), rung_v3()).ok());

    REQUIRE(catalog.unmount(mounted.provider));
    const op::Evaluation gone = op::migrate(catalog, old_bytes(2), rung_v3());
    CHECK_FALSE(gone.ok());
    CHECK(gone.reason().find("no live conversion") != std::string::npos);
    // NOTHING WAS HELD. The seam caches no callable, no contribution and no index, so there
    // is nowhere for a removed edge to still be reachable from.
    CHECK(catalog.find("zengine.migrate.Rung.v1-to-v3") == nullptr);
    CHECK(catalog.contributions("zengine.migrate.Rung.v1-to-v3").empty());
}

TEST_CASE("MIG-0/SC-11: a lawful overlay changes what the next spend means") {
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_MIG_DIRECT_SO).ok);
    CHECK(answered(op::migrate(catalog, old_bytes(6), rung_v3())).rungs ==
          mig_fixture::kDirectRungs);

    // The same identity at the same signature, deliberately covering it -- ordinary catalog
    // law, with no migration-specific rule anywhere in it.
    const op::MountResult over =
        op::mount_provider(catalog, PROVIDER_MIG_ALT_SO, op::MountMode::Overlay);
    REQUIRE_MESSAGE(over.ok, over.reason);
    CHECK(answered(op::migrate(catalog, old_bytes(6), rung_v3())).rungs ==
          mig_fixture::kReplacedRungs);

    // ...AND WITHDRAWING IT REVEALS WHAT IT COVERED, unchanged and unrebuilt.
    REQUIRE(catalog.unmount(over.provider));
    CHECK(answered(op::migrate(catalog, old_bytes(6), rung_v3())).rungs ==
          mig_fixture::kDirectRungs);
}

TEST_CASE("MIG-0: an ordinary second contribution to a live edge is refused, not layered") {
    op::Catalog catalog;
    REQUIRE(op::mount_provider(catalog, PROVIDER_MIG_DIRECT_SO).ok);
    const op::MountResult clash = op::mount_provider(catalog, PROVIDER_MIG_ALT_SO);
    CHECK_FALSE(clash.ok);
    CHECK(clash.reason.find("zengine.migrate.Rung.v1-to-v3") != std::string::npos);
    // The refused mount left the catalog exactly as it was.
    CHECK(answered(op::migrate(catalog, old_bytes(6), rung_v3())).rungs ==
          mig_fixture::kDirectRungs);
}

TEST_CASE("MIG-0: a converted value outlives the provider that produced it") {
    // The answer owns its own schema (Loom's value lifetime law), so final admission by the
    // owner never has to call back into an image that may already be gone.
    op::Catalog catalog;
    const op::MountResult mounted = op::mount_provider(catalog, PROVIDER_MIG_DIRECT_SO);
    REQUIRE(mounted.ok);
    const op::Evaluation converted = op::migrate(catalog, old_bytes(77), rung_v3());
    REQUIRE(converted.ok());
    const std::uint64_t closes_before = op::image_counts().closes;
    REQUIRE(catalog.unmount(mounted.provider));
    CHECK(op::image_counts().closes > closes_before);

    // The value is still whole, still says what it said, and still knows its own shape.
    CHECK(answered(converted).carried == 77);
    CHECK(loom::same_identity(op::migrated(converted).schema(), *rung_v3()));
    CHECK(loom::admit(op::migrated(converted), *rung_v3()).ok());
}

// ---- 6. Demand is not authority -------------------------------------------------------

TEST_CASE("MIG-0/SC-5: a version claim opens no image and mounts nothing") {
    // ⭐ THE AUTHORITY MEASUREMENT, taken on the ledger rather than argued. An old file's
    // claim is a LOOKUP KEY: it selects among conversions a host already has, and asking
    // for one that is not there costs exactly a sentence.
    op::Catalog catalog;
    const op::ImageCounts before = op::image_counts();
    const std::size_t providers_before = catalog.providers().size();

    for (int i = 0; i < 5; ++i) {
        CHECK_FALSE(op::migrate(catalog, old_bytes(i), rung_v3()).ok());
    }
    // ...and the artifact that WOULD supply it is sitting right there on disk, unopened.
    CHECK_FALSE(slurp(PROVIDER_MIG_DIRECT_SO).empty());

    const op::ImageCounts after = op::image_counts();
    CHECK(after.opens == before.opens);
    CHECK(after.closes == before.closes);
    CHECK(catalog.providers().size() == providers_before);
    CHECK(catalog.identities().empty());
}

TEST_CASE("MIG-0/SC-5: a claim cannot name a provider, only an edge") {
    // Two artifacts can supply one edge; which of them answers is the CATALOG's current
    // resolution, decided by mount order and mount mode -- both of which are the host's.
    // Nothing in the bytes participates in that decision, and there is no field where it
    // could: an `Unverified` reveals a name and a version and nothing else.
    const loom::Unverified claim = old_bytes(1);
    CHECK(claim.claimed_name() == "Rung");
    CHECK(claim.claimed_version() == 1u);
}

// ---- 7. The fence ---------------------------------------------------------------------

TEST_CASE("MIG-0: the migration seam names no loader, no plan and no filesystem") {
    // Defence in depth, the shape `test_operator_source.cpp`'s own fence uses: this suite
    // links the kernel and the loader, so no link line can carry this claim -- only reading
    // the file can. The seam is a projection over the catalog, and it must stay one.
    const std::string source = slurp(OPERATOR_MIGRATION_HPP);
    REQUIRE_FALSE(source.empty());
    for (const char* forbidden : {"provider_host.hpp", "operator/image.hpp", "mount_provider",
                                  "load_plan", "load_execute", "std::filesystem", "dlopen",
                                  "LoadLibrary", "ifstream"}) {
        CAPTURE(forbidden);
        CHECK(source.find(forbidden) == std::string::npos);
    }
}
