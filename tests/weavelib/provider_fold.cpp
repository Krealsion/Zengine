// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A fold carried across a real module boundary: `prov.thousand(acc, count)` = acc + (0 + 1 + ... +
// 999), one step, the evaluator's fold over `math.add`. It crosses as structure -- its body is a
// reference resolved in the host at every spend -- and as a fold's body itself it nests a fold
// whose spends the host's evaluation budget counts. Not a weave: no Kernel loads it.

#include "operator/catalog.hpp"
#include "operator/primitives.hpp"
#include "operator/provider.hpp"

#include <zen/kind.hpp>
#include <zen/schema.hpp>

#include <cstdint>
#include <vector>

namespace {

namespace op = zengine::op;

std::vector<op::OperatorDef> folds() {
    // The primitives are scaffolding for authoring and die at the closing brace; the host
    // resolves `math.add` against whatever supplies it there.
    op::Catalog against;
    op::publish_primitives(against);
    const loom::TypeRef integer = loom::type_of(loom::Kind::Int);
    op::Builder b(against, "prov.thousand",
                  {loom::Field{"acc", integer, true}, loom::Field{"count", integer, true}});
    const op::Builder::Ref summed =
        b.fold(op::kAddInt, "rhs", "lhs",
               {b.constant(std::int64_t{0}), b.constant(std::int64_t{1000}),
                b.constant(std::int64_t{1}), b.input("acc")});
    std::vector<op::OperatorDef> defs;
    defs.push_back(std::move(b).result("result", summed,
                                       "adds 0 + 1 + ... + 999 to the accumulator: a fold's body "
                                       "that folds"));
    return defs;
}

} // namespace

ZENGINE_OPERATOR_PROVIDER("zengine.provider.fold", folds)
