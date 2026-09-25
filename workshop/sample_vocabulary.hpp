// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SAMPLE_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_SAMPLE_VOCABULARY_HPP

// The two sentences an explicit sample costs: `SampleRequested` to `zengine.sources`, and
// `SourceSampled` back (workshop/sample_door.hpp). An office apart from the arrangement door,
// because this one causes evaluation. The answer is rendered lines, never a value, and promises
// nothing about freshness: a retained sample is history.

#include <zen/weave/shape.hpp>

#include <string>
#include <vector>

namespace zengine::workshop {

/// The office that may cause a Source to be evaluated. A host that mounts no sample door holds
/// none, and an ask reaches nobody.
inline constexpr const char* kSampleRole = "zengine.sources";

/// Sample this Source now. A name, never a definition: the door resolves the catalog when it acts.
struct SampleRequested {
    std::string identity;
    ZEN_SHAPE(SampleRequested, 1, ZEN_FIELD(identity));
};

/// What that Source said when asked. `identity` is echoed so a pane can label a retained answer;
/// `ok` and `reason` are separate because a refusal is a sentence somebody owns.
struct SourceSampled {
    std::string identity;
    bool ok = false;
    /// The refusal, in the words of whichever layer owns it -- empty when `ok`.
    std::string reason;
    /// The rendered value, one logical line per row -- empty when it was refused.
    std::vector<std::string> lines;

    ZEN_SHAPE(SourceSampled, 1, ZEN_FIELD(identity), ZEN_FIELD(ok), ZEN_FIELD(reason),
              ZEN_FIELD(lines));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SAMPLE_VOCABULARY_HPP
