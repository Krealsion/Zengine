// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_ARRANGEMENT_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_ARRANGEMENT_VOCABULARY_HPP

// Asking a host what it resolved: `ArrangementRequested` -> `ResolvedArrangement` and
// `PowersRequested` -> `ResolvedPowers`, answered by one office (docs/reference/introspection.md).
// Values only: a picture of the owners' facts, which confers no authority over them.

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// The office the host's observation door holds. A host that mounts no door holds none, and an
/// ask then reaches nobody.
inline constexpr const char* kArrangementRole = "zengine.arrangement";

/// Ask what this host's authored project asked for, and what came of it. It carries nothing: a
/// filter would be a policy nobody asked for.
struct ArrangementRequested {
    ZEN_SHAPE(ArrangementRequested, 1);
};

/// Ask which operator powers this host resolves, and whose code satisfies each.
struct PowersRequested {
    ZEN_SHAPE(PowersRequested, 1);
};

// ---- The arrangement ----------------------------------------------------------

/// `ArtifactParticipation::offer`'s tokens: `op::OfferOutcome` in words a renumbering cannot
/// move. Empty for a row with no weave intent, where no offer was ever made.
inline constexpr const char* kOfferNone = "";
inline constexpr const char* kOfferedToken = "offered";
inline constexpr const char* kNotAConsumerToken = "not-a-consumer";
inline constexpr const char* kVersionMismatchToken = "version-mismatch";
inline constexpr const char* kNotOpenedToken = "not-opened";

/// `ArtifactParticipation::state`'s tokens: `load::RowState`, in words. A string, so the states
/// can grow without a new version. `pending` is reached and waiting on the maker, nothing mounted:
/// a barrier, so at most one row is pending and every row after it is `authored`.
inline constexpr const char* kAuthoredToken = "authored";
inline constexpr const char* kPendingToken = "pending";
inline constexpr const char* kLoadingToken = "loading";
inline constexpr const char* kResolvedToken = "resolved";
inline constexpr const char* kRefusedToken = "refused";
/// A resolved row whose reload-in-place conversation is open. It keeps its resolved fields: the
/// weave is live while the kernel decides.
inline constexpr const char* kReloadingToken = "reloading";
/// The row resolved, and a switch moved its office to another choice, which is a row of its own.
inline constexpr const char* kSwitchedToken = "switched";
/// An optional row that refused and that realization stepped over. Only `v2::ResolvedArrangement`
/// carries it; version 1 says `refused`.
inline constexpr const char* kUnavailableToken = "unavailable";

/// One authored project participant, and what this run made of it: one row per artifact whatever
/// it participates as. `artifact`, `authored_provider` and `authored_role` are authored; the rest
/// are resolved, carried only by a `resolved` row. No resolved role: the office the Kernel bound is
/// `zen.ListLoaded`'s answer.
struct ArtifactParticipation {
    std::string artifact;
    std::string authored_provider;
    std::string authored_role;

    std::string state = kAuthoredToken;
    std::string provider;
    std::int64_t powers = 0;
    std::int64_t weave = 0;
    std::string offer;

    ZEN_SHAPE(ArtifactParticipation, 2, ZEN_FIELD(artifact), ZEN_FIELD(authored_provider),
              ZEN_FIELD(authored_role), ZEN_FIELD(state), ZEN_FIELD(provider),
              ZEN_FIELD(powers), ZEN_FIELD(weave), ZEN_FIELD(offer));
};

/// What this project asked to participate, and what resolved from it: one entry per authored row
/// in the plan's order, never sorted, then each authored choice a switch loaded that is not an
/// artifact row. `plan` is the file read -- a provenance line, not an identity.
struct ResolvedArrangement {
    std::string plan;
    std::vector<ArtifactParticipation> artifacts;

    ZEN_SHAPE(ResolvedArrangement, 1, ZEN_FIELD(plan), ZEN_FIELD(artifacts));
};

namespace v3 {
/// One participant, with why it is not running and what a maker can do: version 2's fields plus
/// `optional`, `reason` (the refusing layer's sentence, never an identity) and `next`.
struct ArtifactParticipation {
    std::string artifact;
    std::string authored_provider;
    std::string authored_role;
    bool optional = false;
    std::string state = kAuthoredToken;
    std::string reason;
    std::string next;
    std::string provider;
    std::int64_t powers = 0;
    std::int64_t weave = 0;
    std::string offer;
    ZEN_SHAPE(ArtifactParticipation, 3, ZEN_FIELD(artifact), ZEN_FIELD(authored_provider),
              ZEN_FIELD(authored_role), ZEN_FIELD(optional), ZEN_FIELD(state),
              ZEN_FIELD(reason), ZEN_FIELD(next), ZEN_FIELD(provider), ZEN_FIELD(powers),
              ZEN_FIELD(weave), ZEN_FIELD(offer));
};
} // namespace v3

namespace v2 {
/// The same answer with version 3 rows, answered to an asker whose office accepts it; any other
/// asker is answered version 1.
struct ResolvedArrangement {
    std::string plan;
    std::vector<v3::ArtifactParticipation> artifacts;
    ZEN_SHAPE(ResolvedArrangement, 2, ZEN_FIELD(plan), ZEN_FIELD(artifacts));
};
} // namespace v2

// ---- The powers ---------------------------------------------------------------

/// Which shape: name, version and content id together, because the gate compares them as one
/// (`loom::same_identity`). An identity, not a structure.
struct SchemaIdentity {
    std::string name;
    std::int64_t version = 0;
    /// `loom::ContentId`'s 64 bits reinterpreted as Loom's signed Int: compared, never ordered.
    std::int64_t content_id = 0;

    ZEN_SHAPE(SchemaIdentity, 1, ZEN_FIELD(name), ZEN_FIELD(version), ZEN_FIELD(content_id));
};

/// One contribution eligible to satisfy a power; an empty `provider` means the host published it.
/// `composite` matters because a composite resolves its leaves at every spend. `source` and
/// `output` say what sampling would yield without sampling: both are read off the definition.
struct PowerContribution {
    std::string provider;
    bool composite = false;
    bool source = false;
    SchemaIdentity output;

    ZEN_SHAPE(PowerContribution, 2, ZEN_FIELD(provider), ZEN_FIELD(composite), ZEN_FIELD(source),
              ZEN_FIELD(output));
};

/// One power and every contribution eligible to satisfy it, active last -- the catalog's stack
/// order carries the answer, so there is no `active` field. A stack is never empty.
struct PowerStack {
    std::string power;
    std::vector<PowerContribution> contributions;

    ZEN_SHAPE(PowerStack, 1, ZEN_FIELD(power), ZEN_FIELD(contributions));
};

/// Which powers this host resolves (name-ordered, as the catalog stores them) and who is mounted:
/// `providers` is `Catalog::providers()` verbatim, not derived from `powers`.
struct ResolvedPowers {
    std::vector<PowerStack> powers;
    std::vector<std::string> providers;

    ZEN_SHAPE(ResolvedPowers, 1, ZEN_FIELD(powers), ZEN_FIELD(providers));
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_ARRANGEMENT_VOCABULARY_HPP
