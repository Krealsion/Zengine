// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// A PROVIDER THAT CONTRIBUTES A SOURCE (SOURCE-0) -- zero maker inputs, across the
// real provider ABI, out of a real image the host opens for itself.
//
// It exists because "a zero-input contribution crosses the existing codec unchanged"
// is a claim about TWO images and cannot be made inside one. Encoding here, decoding
// in the host, and a body that lives on this side of the boundary for the rest of the
// mount: everything that could quietly be one process is separated by a `dlopen`.
//
// THE SOURCE COUNTS ITS OWN SPENDS, and that is what makes the interesting question
// answerable. `op::invocations()` is a vague-linkage static, so the HOST's counter
// cannot see a body running in this image -- the standard instrument is blind here by
// construction. So the body carries its own: it answers 1 the first time it is
// sampled, 2 the second, and never anything at all if mounting, describing, decoding
// or enumerating it were to run it. A constant would have made an accidental
// evaluation invisible, which is the exact shape SOURCE-0 forbids testing this with.
//
// IT ALSO CONTRIBUTES AN ORDINARY OPERATOR, and the mixture is deliberate. A PROVIDER
// may supply both -- Source is a shape a definition has, not a kind of provider, and
// there is no Source contribution format, no Source ABI and no Source registration
// door for this artifact to use. What is restricted is the HOST's own door into its
// own catalog (`workshop/host_sources.hpp`), which is a different claim about a
// different party.
//
// IT IS NOT A WEAVE. `zengine_provider()`, no `loom::switchboard`, no `zen_weave_abi`:
// no participant, no WeaveId, no role, no grant, no manifest and no bus.

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
