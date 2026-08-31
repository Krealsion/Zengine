// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// FOUR PROVIDERS OF ONE INVENTED HISTORY (MIG-0) — the arrangements the migration seam has
// to be asked about, as real artifacts a host opens for itself.
//
// The shape they convert is `tests/weavelib/migration_family.hpp`'s `Rung`, at versions 1,
// 2 and 3. Every road writes how many rungs the value climbed, so a case reads which road
// ran off the ANSWER rather than trusting the identity it asked for.
//
// ONE SOURCE, FOUR LIBRARIES (the weavelib pattern), and the difference under test is one
// preprocessor branch:
//
//   (default)          zengine-provider-mig-chain
//                        `v1 -> v2` and `v2 -> v3`, and NO `v1 -> v3`. Two edges that meet
//                        in the middle: a reader wanting v3 out of a v1 file must be
//                        REFUSED here, because composing them is a route nobody authored.
//   MIG_DIRECT         zengine-provider-mig-direct
//                        the authored `v1 -> v3`, in one step, recording ONE rung. It is
//                        what makes the refusal above a statement about authorship rather
//                        than about capability: the same reader, unchanged, is satisfied
//                        the moment somebody writes the edge down.
//   MIG_DIRECT_ALT     zengine-provider-mig-direct-alt
//                        the SAME edge at the same signature, answering differently. Same
//                        identity, same port schemas, same content ids -- so an overlay
//                        mount may lawfully cover the one above, and what a reader gets
//                        next is whatever the catalog currently resolves.
//   MIG_COMPOSED       zengine-provider-mig-composed
//                        the same edge as a COMPOSITION over another identity. A migration
//                        is an ordinary operator, so it may be a graph; the graph crosses
//                        the provider seam as structure, and the power underneath it stays
//                        replaceable.
//
// NONE OF THEM IS A WEAVE. No `zen_weave_abi`, no participant, no state, no bus.

#include "migration_family.hpp"

#include "operator/catalog.hpp"
#include "operator/migration.hpp"
#include "operator/operator.hpp"
#include "operator/provider.hpp"

#include <zen/schema.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace op = zengine::op;

/// One rung's climb, written where every arm can spend it: carry the number, keep or invent
/// the note, and record how far this road came.
loom::Cell answered(std::int64_t carried, const std::string& note, std::int64_t rungs) {
    mig_fixture::v3::Rung out;
    out.carried = carried;
    out.note = note;
    out.rungs = rungs;
    return loom::Cell::message(loom::to_value(out));
}

#if defined(MIG_DIRECT) || defined(MIG_DIRECT_ALT) || defined(MIG_COMPOSED)

/// The authored `v1 -> v3` body, with the rung count its own arm decides.
op::OperatorDef::Native straight_to_v3(std::int64_t rungs) {
    return [rungs](const loom::Value& old) {
        const auto was = loom::from_value<mig_fixture::v1::Rung>(old);
        return answered(was.carried, mig_fixture::v1_note(), rungs);
    };
}

#endif

#if defined(MIG_COMPOSED)

/// The identity the composed edge is a composition OVER. An ordinary operator name -- it is
/// not a conversion identity, because what it is is an implementation detail of one, and
/// the seam must resolve the EDGE rather than whatever the edge happens to be built from.
constexpr const char* kUnderneath = "mig.fixture.rung.step";

#endif

/// This artifact's contributions.
std::vector<op::OperatorDef> migrations() {
    auto v1 = loom::schema_of<mig_fixture::v1::Rung>();
    auto v2 = loom::schema_of<mig_fixture::v2::Rung>();
    auto v3 = loom::schema_of<mig_fixture::v3::Rung>();
    std::vector<op::OperatorDef> edges;
#if defined(MIG_DIRECT)
    edges.push_back(
        op::make_migration(v1, v3, straight_to_v3(mig_fixture::kDirectRungs)));
#elif defined(MIG_DIRECT_ALT)
    edges.push_back(
        op::make_migration(v1, v3, straight_to_v3(mig_fixture::kReplacedRungs)));
#elif defined(MIG_COMPOSED)
    // THE POWER, AND THE EDGE THAT IS A GRAPH OVER IT. The composition is hand-built rather
    // than built with `op::Builder`, and the reason is worth recording: `Builder` mints its
    // own `<identity>.in` port list, and a conversion's input schema must BE the historical
    // shape -- so the authoring surface for ordinary rules cannot spell this exterior. The
    // GRAPH itself is ordinary in every other way, and crosses the provider seam as
    // structure exactly as `prov.function.1`'s does.
    op::OperatorDef under(kUnderneath, v1,
                          op::migration_answer_schema(kUnderneath, v3),
                          straight_to_v3(mig_fixture::kComposedRungs));
    op::Node step;
    step.identity = kUnderneath;
    step.authored_in = under.inputs()->content_id();
    step.authored_out = under.outputs()->content_id();
    for (const loom::Field& port : v1->fields()) {
        step.arguments.push_back(op::Binding::input(port.name));
    }
    op::Composite graph;
    graph.nodes.push_back(std::move(step));
    graph.result_node = 0;
    const std::string identity = op::migration_identity(*v1, *v3);
    edges.push_back(std::move(under));
    edges.push_back(op::OperatorDef(identity, v1, op::migration_answer_schema(identity, v3),
                                    std::move(graph)));
#else
    // The chain arm: two edges, and deliberately no third.
    edges.push_back(op::make_migration(v1, v2, [](const loom::Value& old) {
        const auto was = loom::from_value<mig_fixture::v1::Rung>(old);
        mig_fixture::v2::Rung out;
        out.carried = was.carried;
        out.note = mig_fixture::v1_note();
        return loom::Cell::message(loom::to_value(out));
    }));
    edges.push_back(op::make_migration(v2, v3, [](const loom::Value& old) {
        const auto was = loom::from_value<mig_fixture::v2::Rung>(old);
        return answered(was.carried, was.note, mig_fixture::kViaV2Rungs);
    }));
#endif
    return edges;
}

} // namespace

#if defined(MIG_DIRECT)
ZENGINE_OPERATOR_PROVIDER("zengine.migration.fixture.direct", migrations)
#elif defined(MIG_DIRECT_ALT)
ZENGINE_OPERATOR_PROVIDER("zengine.migration.fixture.direct.alt", migrations)
#elif defined(MIG_COMPOSED)
ZENGINE_OPERATOR_PROVIDER("zengine.migration.fixture.composed", migrations)
#else
ZENGINE_OPERATOR_PROVIDER("zengine.migration.fixture.chain", migrations)
#endif
