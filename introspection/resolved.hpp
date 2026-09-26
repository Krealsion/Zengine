// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_RESOLVED_HPP
#define ZENGINE_INTROSPECTION_RESOLVED_HPP

// The arrangement view, and the vocabulary the Powers view shares with it: `project_arrangement`
// turns a `workshop::ResolvedArrangement` and a budget into rows and keeps nothing. `lay_blocks`
// is the shared budget rule (an entry and its omission marker are one demand), `elision` its
// spelling, `counted` the grammar of a count, and the sentences bound what rows may claim.
// Pane law: agents/panes.md

// Each pane reserves a row for the sentence bounding its count: `kNotAuthored` (in-process
// participants were never authored artifacts) and `kHostResolution` (one host's resolution).

#include "loaded.hpp" // `fit`, `kElided` -- one spelling of a cut, for all three panes

#include "surface/vocabulary.hpp"
#include "workshop/arrangement_vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::introspection {

// ---- What each pane will not let its count be misread as -----------------------

/// What the arrangement is not: every weave this host mounted in-process -- its own, the plan
/// booter, the Builder, the arrangement door itself -- was never an authored artifact.
inline constexpr const char* kNotAuthored = "in-process participants are not authored artifacts";

/// What the powers list is not: one host's resolution, and nothing more -- whether another
/// participant owns a private catalog is a fact this pane never read.
inline constexpr const char* kHostResolution =
    "this pane describes this host's operator resolution only";

/// Where the powers came from and how old they are: `snapshot` first, re-read only on a room
/// grant, since nothing polls and no provider-mount event exists.
inline constexpr const char* kPowersSource = "snapshot from zengine.arrangement, on room grant";

/// What a row says instead of a provider identity when the host itself published a
/// contribution -- `op::Contribution`'s empty provider, given a maker's word.
inline constexpr const char* kHostItself = "(this host)";

/// A row realization has not reached yet -- a live state, since realization proceeds through
/// ordinary deliveries and a pane opened mid-startup shows such rows below `(loading)`.
inline constexpr const char* kNotReached = "(not reached)";

/// What a row says for the artifact whose load is in flight at this instant. Only
/// ever one: authored order is strict and serial, and the owner holds one
/// conversation.
inline constexpr const char* kLoadingNow = "(loading)";

/// What a row says for the artifact that refused. Only ever one too -- the plan stops
/// at the first refusal -- and its own mount, if it made one, has been rolled back.
inline constexpr const char* kRefusedRow = "(refused)";

/// A ROW WHOSE OFFICE A SWITCH MOVED to another authored choice: resolved, and not running. The
/// choice that holds the office is its own row of the same view.
inline constexpr const char* kSwitchedRow = "(switched: its office is held by another choice)";

/// An optional row that refused and was stepped over: settled and not running. Its `why` and
/// `next` rows follow it, from the owner's answer.
inline constexpr const char* kUnavailableRow = "(unavailable: optional, and this run stepped over it)";

/// What an artifact row says where a surface was not authored at all.
inline constexpr const char* kNoIntent = "none";

// ---- The one budget rule both lists obey ---------------------------------------

/// How many whole blocks fit, and whether something must say so. A block is all or nothing
/// (half an artifact is a wrong answer, not a shorter one), and the omission marker is claimed
/// from the same budget, taking back the last block if that is its cost. Total: at a budget of
/// one with a three-row block, nothing is shown and `... N more` is.
struct Laid {
    std::size_t shown = 0; ///< how many leading blocks reached the rows
    bool marker = false;   ///< whether one row must be spent saying how many did not
};

inline Laid lay_blocks(const std::vector<std::int64_t>& heights, std::int64_t budget) {
    Laid out;
    if (budget <= 0) {
        return out;
    }
    std::int64_t used = 0;
    while (out.shown < heights.size() && used + heights[out.shown] <= budget) {
        used += heights[out.shown];
        ++out.shown;
    }
    if (out.shown == heights.size()) {
        return out; // everything fit; there is nothing to mark
    }
    out.marker = true;
    while (out.shown > 0 && used + 1 > budget) {
        --out.shown;
        used -= heights[out.shown];
    }
    return out;
}

/// `n more`, in the one spelling all three panes share.
inline std::string elision(std::size_t hidden, std::int64_t columns) {
    return fit("  " + std::string(kElided) + " " + std::to_string(hidden) + " more", columns);
}

/// `1 power` / `2 powers`: a noun that disagrees with its number spends the reader on grammar.
/// A count word beside a number, and nothing more (no locale, no catalogue).
inline std::string counted(std::int64_t n, const char* one, const char* many) {
    return std::to_string(n) + " " + (n == 1 ? one : many);
}

inline std::string powers_said(std::int64_t n) { return counted(n, "power", "powers"); }
inline std::string providers_said(std::int64_t n) { return counted(n, "provider", "providers"); }

// ---- The arrangement view -------------------------------------------------------

/// The rows one artifact occupies: the stem, `authored` (both surfaces on one row), then a
/// `resolved` row per surface that resolved -- or, for a row not running, one state row and the
/// owner's `why` and `next`. Authored and resolved are separate labelled rows, so a promise the
/// plan made is never read as a number this process minted; one artifact's provider and weave
/// are two participations under one stem.
inline std::vector<surface::SurfaceTextRow>
artifact_rows(const workshop::v3::ArtifactParticipation& a, std::int64_t columns) {
    std::vector<surface::SurfaceTextRow> rows;
    rows.push_back(surface::SurfaceTextRow{fit("  " + a.artifact, columns), surface::role::kFill});

    std::string authored;
    if (!a.authored_provider.empty()) {
        authored = "provider " + a.authored_provider;
    }
    if (!a.authored_role.empty()) {
        authored += (authored.empty() ? "" : ", ");
        authored += "weave " + a.authored_role;
    }
    if (authored.empty()) {
        // Unreachable through `check_artifact`; handled because this view does not own that.
        authored = kNoIntent;
    }
    rows.push_back(
        surface::SurfaceTextRow{fit("    authored  " + authored, columns), surface::role::kMuted});

    if (a.state != workshop::kResolvedToken) {
        // Loading, refused, switched, unavailable or not reached. Only refused and unavailable
        // are alerts: loading and not yet reached are a healthy project coming up.
        const bool loading = a.state == workshop::kLoadingToken;
        const bool refused = a.state == workshop::kRefusedToken;
        const bool switched = a.state == workshop::kSwitchedToken;
        const bool unavailable = a.state == workshop::kUnavailableToken;
        const char* said = loading       ? kLoadingNow
                           : refused     ? kRefusedRow
                           : switched    ? kSwitchedRow
                           : unavailable ? kUnavailableRow
                                         : kNotReached;
        rows.push_back(surface::SurfaceTextRow{
            fit("    " + std::string(said), columns),
            refused || unavailable ? surface::role::kAlert : surface::role::kMuted});
        // WHY, AND WHAT A MAKER CAN DO -- the owner's own two fields, shown as they came. A
        // version 1 answer has neither, and says only the state.
        if (!a.reason.empty()) {
            rows.push_back(
                surface::SurfaceTextRow{fit("    why   " + a.reason, columns), surface::role::kMuted});
        }
        if (!a.next.empty()) {
            rows.push_back(
                surface::SurfaceTextRow{fit("    next  " + a.next, columns), surface::role::kMuted});
        }
        return rows;
    }
    if (!a.provider.empty()) {
        rows.push_back(surface::SurfaceTextRow{
            fit("    resolved  provider " + a.provider + ", " + powers_said(a.powers), columns),
            surface::role::kMuted});
    }
    if (a.weave != 0) {
        // The offer is shown only where one was made: no handoff outcome for a row no Kernel
        // loaded anything from.
        std::string said = "    resolved  weave #" + std::to_string(a.weave);
        if (!a.offer.empty()) {
            said += ", operator host " + a.offer;
        }
        rows.push_back(surface::SurfaceTextRow{fit(said, columns), surface::role::kMuted});
    }
    return rows;
}

/// The whole arrangement view against the granted room, in `project_loaded`'s priority order:
/// the heading and counts, `kNotAuthored`, the list (whole blocks, omissions counted), the plan
/// line from genuine slack only, one blank. The counts are derived from the answer's rows, so
/// the summary and the rows cannot disagree.
inline std::vector<surface::SurfaceTextRow>
project_arrangement(const workshop::v2::ResolvedArrangement& said, std::int64_t rows,
                    std::int64_t columns) {
    std::vector<surface::SurfaceTextRow> out;
    if (rows <= 0 || columns <= 0) {
        return out;
    }
    std::size_t performed = 0;
    std::size_t unavailable = 0;
    std::size_t providers = 0;
    std::size_t weaves = 0;
    for (const workshop::v3::ArtifactParticipation& a : said.artifacts) {
        performed += a.state == workshop::kResolvedToken ? 1u : 0u;
        unavailable += a.state == workshop::kUnavailableToken ? 1u : 0u;
        providers += !a.provider.empty() ? 1u : 0u;
        weaves += a.weave != 0 ? 1u : 0u;
    }
    // A PLAN THAT COMPLETED IS NOT A PLAN WHOSE EVERY ROW RAN: the unavailable count is said
    // beside the resolved one, never folded into it.
    out.push_back(surface::SurfaceTextRow{
        fit(std::to_string(performed) + " of " + std::to_string(said.artifacts.size()) +
                " artifacts resolved" +
                (unavailable > 0 ? ", " + std::to_string(unavailable) + " unavailable"
                                 : std::string()) +
                " -- " + std::to_string(providers) + " providers, " +
                std::to_string(weaves) + " weaves",
            columns),
        surface::role::kAccent});

    std::vector<std::vector<surface::SurfaceTextRow>> blocks;
    std::vector<std::int64_t> heights;
    blocks.reserve(said.artifacts.size());
    heights.reserve(said.artifacts.size());
    std::int64_t total = 0;
    for (const workshop::v3::ArtifactParticipation& a : said.artifacts) {
        blocks.push_back(artifact_rows(a, columns));
        heights.push_back(static_cast<std::int64_t>(blocks.back().size()));
        total += heights.back();
    }

    std::int64_t left = rows - 1;
    const std::int64_t caveat = left >= 1 && (blocks.empty() || left >= 2) ? 1 : 0;
    left -= caveat;
    // ...and the plan line only out of GENUINE slack: the whole list must fit and
    // still leave a row over. A pane that had to window its own project spends that
    // row on the project instead, and keeps the caveat it already reserved.
    const std::int64_t source =
        caveat == 1 && !said.plan.empty() && total < left ? 1 : 0;
    std::int64_t budget = left - source;

    const Laid laid = lay_blocks(heights, budget);
    for (std::size_t i = 0; i < laid.shown; ++i) {
        for (surface::SurfaceTextRow& row : blocks[i]) {
            out.push_back(std::move(row));
        }
        budget -= heights[i];
    }
    if (laid.marker) {
        out.push_back(surface::SurfaceTextRow{elision(said.artifacts.size() - laid.shown, columns),
                                              surface::role::kMuted});
        --budget;
    }
    // A spare row nothing else wanted separates the list from the small print. It is
    // the LAST claim on the budget, so it never costs a block or a note.
    if (budget > 0 && caveat > 0) {
        out.push_back(surface::SurfaceTextRow{std::string(), surface::role::kFill});
    }
    if (caveat > 0) {
        out.push_back(surface::SurfaceTextRow{fit(kNotAuthored, columns), surface::role::kMuted});
    }
    if (source > 0) {
        out.push_back(
            surface::SurfaceTextRow{fit("plan: " + said.plan, columns), surface::role::kMuted});
    }
    return out;
}

/// A VERSION 1 ANSWER, read into the one projection above: the fields it has, and none it lacks.
/// A host built before version 2 answers this way, and its rows say their state without a reason.
inline workshop::v2::ResolvedArrangement in_version_two(const workshop::ResolvedArrangement& said) {
    workshop::v2::ResolvedArrangement out;
    out.plan = said.plan;
    out.artifacts.reserve(said.artifacts.size());
    for (const workshop::ArtifactParticipation& a : said.artifacts) {
        workshop::v3::ArtifactParticipation row;
        row.artifact = a.artifact;
        row.authored_provider = a.authored_provider;
        row.authored_role = a.authored_role;
        row.state = a.state;
        row.provider = a.provider;
        row.powers = a.powers;
        row.weave = a.weave;
        row.offer = a.offer;
        out.artifacts.push_back(std::move(row));
    }
    return out;
}

inline std::vector<surface::SurfaceTextRow>
project_arrangement(const workshop::ResolvedArrangement& said, std::int64_t rows,
                    std::int64_t columns) {
    return project_arrangement(in_version_two(said), rows, columns);
}

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_RESOLVED_HPP
