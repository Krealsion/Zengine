// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A provider contributing a source, zero maker inputs, across the real provider ABI: that such a
// contribution crosses the codec unchanged is a claim about two images. The source counts its own
// spends, since the host's `op::invocations()`, a vague-linkage static, is blind across the
// boundary: 1 the first time it is sampled, 2 the second, and never by mounting, describing,
// decoding or enumerating. It also contributes an ordinary operator: Source is a shape a
// definition has, not a kind of provider. Not a weave.

#include "operator/operator.hpp"
#include "operator/provider.hpp"

#include <cstdint>
#include <vector>

namespace {

/// HOW MANY TIMES THIS IMAGE'S BODY HAS ACTUALLY RUN. Namespace scope because a
/// block-scope lambda cannot be a `make_operator<&F>` argument at all, and a plain
/// function because a Source's body is an ordinary C++ function with no arguments --
/// which is the whole of what "zero maker inputs" costs an author.
std::int64_t g_spends = 0;
std::int64_t spends() { return ++g_spends; }

/// An ordinary parameterized operator from the same provider, so the host can ask a
/// mixed batch what each of its definitions is.
std::int64_t doubled(std::int64_t value) { return value * 2; }

std::vector<zengine::op::OperatorDef> supply() {
    std::vector<zengine::op::OperatorDef> defs;
    defs.push_back(zengine::op::make_operator<&spends>("prov.source.spends", {}, "count"));
    defs.push_back(zengine::op::make_operator<&doubled>("prov.source.doubled", {"value"},
                                                        "result"));
    return defs;
}

} // namespace

ZENGINE_OPERATOR_PROVIDER("zengine.provider.source", supply)
