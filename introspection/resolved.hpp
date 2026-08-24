// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_RESOLVED_HPP
#define ZENGINE_INTROSPECTION_RESOLVED_HPP

// THE TWO VIEWS INTR-1 ADDED, as pure functions over the host's own answers.
//
//     project_arrangement   `workshop::ResolvedArrangement` + a prose budget
//                           -> the rows a maker reads
//     project_powers        `workshop::ResolvedPowers` + a prose budget
//                           -> the rows a maker reads
//
// NEITHER TOUCHES A BUS, for `loaded.hpp`'s reason exactly: the weave beside them owns
// WHEN to ask and WHOM to believe, and this owns what an answer MEANS -- so both
// questions are askable of a value in a test instead of of a running system.
//
// ---- THEY MAKE NO TRUTH AND THEY KEEP NONE -------------------------------------
//
// Every fact below arrived in one message and is spent building rows. There is no
// inventory here, no arrangement mirror, no provider map, no diff against a previous
// reading, no timestamp and no clock. `loaded.hpp` says at length why the Loaded pane
// keeps a PROJECTION and never an INVENTORY; these two do not even keep the
// projection, because neither pane has a gesture to read a row back against.
//
// ---- WHY ONE FILE FOR TWO PANES -------------------------------------------------
//
// One thing: `lay_blocks`. Both views show a list whose entries are SEVERAL ROWS TALL,
// so both meet the arithmetic INTR-0 was measured getting wrong once -- reserving one
// row for "the list" bought a row the omission marker then took, and a four-row body
// spent two rows on notes and named nothing at all. AN ENTRY AND ITS OMISSION MARKER
// ARE ONE DEMAND ON THE BUDGET. That rule is spelled once, here, so the second pane
// cannot inherit a subtly different version of it.
//
// Everything else in the two is separate: separate headings, separate caveats,
// separate row vocabularies, separate blocks. They share a law, not a layout.
//
// ---- WHAT EACH COUNT MEANS, AND WHAT BOUNDS IT ----------------------------------
//
// A COUNT WITH AN UNSTATED POPULATION IS THE DEFECT BOTH VIEWS ARE SHAPED AROUND
// (INTR-0, in a third place). So each pane reserves one row for the sentence that
// bounds its own number, BEFORE the list is offered anything but its first entry:
//
//     arrangement   `kNotAuthored`     -- the in-process participants that were never
//                                         authored artifacts, and so are not rows here
//     powers        `kHostResolution`  -- these rows are what THIS host resolves, and
//                                         the sentence claims nothing past that
//
// Both are true of the shipped Workshop right now. The first is why the Builder, the
// runner, the Manager, the control door, the terminal participant, Workshop's own
// weave and the arrangement door itself are absent from a pane headed `artifacts`.
// The second is why `powers` is one host's resolution rather than a census of every
// operator in the process.

#include "loaded.hpp" // `fit`, `kElided` -- one spelling of a cut, for all three panes

#include "surface/vocabulary.hpp"
#include "workshop/arrangement_vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::introspection {

// ---- What each pane will not let its count be misread as -----------------------

/// WHAT THE ARRANGEMENT IS NOT. A project's artifacts are what an authored plan named;
/// every weave this host mounted in-process -- its own, the plan booter, the control
/// door, the Weave Manager, the terminal participant, the Builder, the build runner
/// and the door that answers this very question -- is a live participant that was
/// never an authored artifact and can never be a row here.
inline constexpr const char* kNotAuthored = "in-process participants are not authored artifacts";

/// WHAT THE POWERS LIST IS NOT. It is ONE host's resolution, and the sentence says
/// exactly that much -- because one catalog is the whole of what this pane read.
///
/// IT SAYS LESS THAN IT USED TO, DELIBERATELY. `a weave that took no offer holds its
/// own catalog` stood here and claimed too much: it is true of the Timer's supported
/// local fallback and it is NOT a law of every weave that accepts no operator host.
/// Whether some other participant owns a private catalog, whether an unbound one can
/// evaluate at all, and what any particular weave makes of an offer are facts this
/// pane never read -- so a sentence bounding this count must not appear to settle them.
inline constexpr const char* kHostResolution =
    "this pane describes this host's operator resolution only";

/// WHERE THE POWERS CAME FROM AND HOW OLD THEY ARE, in one line. `snapshot` is the
/// honest word and it is first: this view re-reads when Workshop grants it room and at
/// no other moment, because nothing polls and there is no provider-mount event to
/// subscribe to. Between two grants these rows are a reading and not a feed.
inline constexpr const char* kPowersSource = "snapshot from zengine.arrangement, on room grant";

/// What a row says instead of a provider identity when the host itself published a
/// contribution -- `op::Contribution`'s empty provider, given a maker's word.
inline constexpr const char* kHostItself = "(this host)";

/// What a row says for an authored artifact the executor never reached. Nothing in a
/// running production Workshop produces one -- a refused plan exits the host before a
/// pane exists -- so this is what a PARTIAL arrangement looks like rather than a state
/// this build can show a maker.
inline constexpr const char* kNotReached = "(not reached)";

/// What an artifact row says where a surface was not authored at all.
inline constexpr const char* kNoIntent = "none";

// ---- The one budget rule both lists obey ---------------------------------------

/// HOW MANY WHOLE BLOCKS FIT, AND WHETHER SOMETHING MUST SAY SO.
///
/// A BLOCK IS ALL OR NOTHING. Half an artifact -- a stem with no participation under
/// it, or an `authored` line with the `resolved` line cut off -- is not a shorter
/// answer, it is a different and wrong one. So a block that does not fit whole is not
/// shown at all, and is counted instead.
///
/// AND THE MARKER IS CLAIMED WITH THE BLOCKS, NEVER AFTER THEM. Showing PART of a list
/// obliges saying how much was hidden, so if anything is left over the marker's row
/// comes out of this same budget -- taking back the last block that fits if that is
/// what it costs. That is the exact arithmetic `project_loaded` was measured getting
/// wrong, generalised from one-row entries to many-row ones.
///
/// TOTAL over every budget, including ones no pane has. At a budget of one with a
/// three-row block, nothing is shown and `... N more` is: even there nothing is hidden
/// without being counted.
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

/// `1 power` / `2 powers`, `1 provider` / `2 providers`. A count a maker reads is
/// written the way a maker writes it: a number whose noun disagrees with it spends the
/// reader's attention on the grammar instead of on the fact the row exists to carry.
/// This is a count word beside a number and nothing more -- there is no locale here, no
/// message catalogue and no rule for a noun neither call site names.
inline std::string counted(std::int64_t n, const char* one, const char* many) {
    return std::to_string(n) + " " + (n == 1 ? one : many);
}

inline std::string powers_said(std::int64_t n) { return counted(n, "power", "powers"); }
inline std::string providers_said(std::int64_t n) { return counted(n, "provider", "providers"); }

// ---- The arrangement view -------------------------------------------------------

/// THE ROWS ONE ARTIFACT OCCUPIES -- three of them, or four, or three for a row that
/// never ran.
///
///     the stem          what a person wrote in the plan
///     `authored`        what they asked it to participate as, both surfaces on one row
///     `resolved` x1..2  what this run made of each surface that resolved
///     `(not reached)`   INSTEAD of the resolved rows, for a row the executor never
///                       performed
///
/// AUTHORED AND RESOLVED ARE SEPARATE LABELLED ROWS, and that is the whole reason the
/// block is vertical rather than a line. A column layout would have had to put a
/// resolved provider identity beside an authored role under one heading, and a maker
/// reading `zengine.timer` twice in one row has no way to tell which of the two is a
/// promise their file makes and which is a number this process minted.
///
/// A PROVIDER AND A WEAVE GET SEPARATE RESOLVED ROWS, so `zengine-timer` is ONE
/// artifact showing TWO participations -- which is LOAD-0's central result made
/// visible. It is emphatically not two artifacts, and the stem above them says so by
/// appearing once.
inline std::vector<surface::SurfaceTextRow>
artifact_rows(const workshop::ArtifactParticipation& a, std::int64_t columns) {
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
        // Unreachable through `check_artifact`, which refuses a row asking for
        // neither -- written because a view that dereferenced an invariant it does
        // not own is a view whose correctness lives in another file.
        authored = kNoIntent;
    }
    rows.push_back(
        surface::SurfaceTextRow{fit("    authored  " + authored, columns), surface::role::kMuted});

    if (!a.performed) {
        // `kAlert`: an authored artifact this host never reached is exactly "something
        // the maker must see", and it is the one row in this pane that is about a
        // failure rather than about an arrangement.
        rows.push_back(
            surface::SurfaceTextRow{fit("    " + std::string(kNotReached), columns),
                                    surface::role::kAlert});
        return rows;
    }
    if (!a.provider.empty()) {
        rows.push_back(surface::SurfaceTextRow{
            fit("    resolved  provider " + a.provider + ", " + powers_said(a.powers), columns),
            surface::role::kMuted});
    }
    if (a.weave != 0) {
        // THE OFFER IS SHOWN ONLY WHERE ONE WAS MADE. The wire carries the empty token
        // for a record with no weave load, and this row exists only when there was one
        // -- so a maker never reads a handoff outcome about an artifact no Kernel ever
        // constructed anything from.
        std::string said = "    resolved  weave #" + std::to_string(a.weave);
        if (!a.offer.empty()) {
            said += ", operator host " + a.offer;
        }
        rows.push_back(surface::SurfaceTextRow{fit(said, columns), surface::role::kMuted});
    }
    return rows;
}

/// THE WHOLE ARRANGEMENT VIEW, spent against the room Workshop granted.
///
/// THE PRIORITY ORDER IS `project_loaded`'S, and it is deliberately the same one:
///
///     the heading      how many artifacts, how many resolved, and what they became
///     `kNotAuthored`   what these rows are NOT -- half of what the count means
///     the list         whole blocks, with every omission counted
///     the plan line    where the authored rows came from, out of GENUINE slack only
///     one blank row    only out of room nothing else wanted
///
/// THE HEADING IS THE FLOOR OF THE ACCOUNTING. At a one-row body there is no list and
/// no note, and the population is still stated -- so even there nothing is hidden
/// without being counted.
///
/// THE COUNTS ARE DERIVED FROM THE ANSWER AND NOT ASSERTED BESIDE IT. `artifacts` is
/// one entry per AUTHORED row, so its length is what the plan declared; `performed`,
/// `provider` and `weave` are counted over that same list. Nothing here is a number
/// the door computed and this view repeated, which is what makes the summary and the
/// rows incapable of disagreeing.
inline std::vector<surface::SurfaceTextRow>
project_arrangement(const workshop::ResolvedArrangement& said, std::int64_t rows,
                    std::int64_t columns) {
    std::vector<surface::SurfaceTextRow> out;
    if (rows <= 0 || columns <= 0) {
        return out;
    }
    std::size_t performed = 0;
    std::size_t providers = 0;
    std::size_t weaves = 0;
    for (const workshop::ArtifactParticipation& a : said.artifacts) {
        performed += a.performed ? 1u : 0u;
        providers += !a.provider.empty() ? 1u : 0u;
        weaves += a.weave != 0 ? 1u : 0u;
    }
    out.push_back(surface::SurfaceTextRow{
        fit(std::to_string(performed) + " of " + std::to_string(said.artifacts.size()) +
                " artifacts resolved -- " + std::to_string(providers) + " providers, " +
                std::to_string(weaves) + " weaves",
            columns),
        surface::role::kAccent});

    std::vector<std::vector<surface::SurfaceTextRow>> blocks;
    std::vector<std::int64_t> heights;
    blocks.reserve(said.artifacts.size());
    heights.reserve(said.artifacts.size());
    std::int64_t total = 0;
    for (const workshop::ArtifactParticipation& a : said.artifacts) {
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

// ---- The powers view ------------------------------------------------------------

/// THE ROWS ONE LOGICAL POWER OCCUPIES: its identity, then every contribution
/// eligible to satisfy it, ACTIVE FIRST.
///
/// THE WIRE'S ORDER IS THE CATALOG'S AND THIS REVERSES IT DELIBERATELY.
/// `op::Catalog` holds a stack whose BACK is active, because that is how contributions
/// were pushed; a maker reads top-down and the first thing they should read is the one
/// whose code actually runs. The reversal is total and lossless -- nothing is
/// windowed inside a block and every contribution keeps its own label -- so this is
/// putting the answer first rather than sorting a population by a second key.
///
/// THE LABEL IS THE STATEMENT AND THE INK IS THE SECOND SIGNAL, `kSelectedMark`'s rule:
/// `active` and `shadowed` are words, so a monochrome terminal reads the same fact a
/// coloured one does, and `kMuted` on a shadowed row is the picture agreeing with the
/// word rather than carrying it.
inline std::vector<surface::SurfaceTextRow> power_rows(const workshop::PowerStack& p,
                                                       std::int64_t columns) {
    std::vector<surface::SurfaceTextRow> rows;
    rows.push_back(surface::SurfaceTextRow{fit("  " + p.power, columns), surface::role::kFill});
    for (std::size_t i = p.contributions.size(); i > 0; --i) {
        const workshop::PowerContribution& c = p.contributions[i - 1];
        const bool active = i == p.contributions.size();
        std::string said = active ? "      active    " : "      shadowed  ";
        said += c.provider.empty() ? kHostItself : c.provider;
        if (c.composite) {
            said += " (composite)";
        }
        rows.push_back(surface::SurfaceTextRow{
            fit(said, columns), active ? surface::role::kFill : surface::role::kMuted});
    }
    return rows;
}

/// THE WHOLE POWERS VIEW, spent against the room Workshop granted.
///
/// SAME PRIORITY ORDER AS THE ARRANGEMENT, and the same reasons:
///
///     the heading        how many powers resolve here, and from how many providers
///     `kHostResolution`  whose resolution this is -- half of what the count means
///     the list           whole blocks, with every omission counted
///     `kPowersSource`    where it came from and how old it is, out of slack only
///     one blank row      only out of room nothing else wanted
///
/// IT NAMES NO POWER AND NO PROVIDER. There is no `math.max` in this file, no
/// `zengine.operators.basic`, and no branch that treats one identity differently from
/// another: every row is built from whatever the host's resolution currently contains,
/// so a provider mounted tomorrow appears here with nothing edited. That genericity is
/// the feature, and it is why the suite may name identities that this cannot.
inline std::vector<surface::SurfaceTextRow>
project_powers(const workshop::ResolvedPowers& said, std::int64_t rows, std::int64_t columns) {
    std::vector<surface::SurfaceTextRow> out;
    if (rows <= 0 || columns <= 0) {
        return out;
    }
    // THE VERB AGREES WITH THE COUNT TOO, because singularising the noun and leaving
    // `resolve` behind would have traded one visible grammar defect for another.
    const std::int64_t identities = static_cast<std::int64_t>(said.powers.size());
    out.push_back(surface::SurfaceTextRow{
        fit(powers_said(identities) + (identities == 1 ? " resolves" : " resolve") +
                " here -- from " +
                providers_said(static_cast<std::int64_t>(said.providers.size())),
            columns),
        surface::role::kAccent});

    std::vector<std::vector<surface::SurfaceTextRow>> blocks;
    std::vector<std::int64_t> heights;
    blocks.reserve(said.powers.size());
    heights.reserve(said.powers.size());
    std::int64_t total = 0;
    for (const workshop::PowerStack& p : said.powers) {
        blocks.push_back(power_rows(p, columns));
        heights.push_back(static_cast<std::int64_t>(blocks.back().size()));
        total += heights.back();
    }

    std::int64_t left = rows - 1;
    const std::int64_t caveat = left >= 1 && (blocks.empty() || left >= 2) ? 1 : 0;
    left -= caveat;
    const std::int64_t source = caveat == 1 && total < left ? 1 : 0;
    std::int64_t budget = left - source;

    const Laid laid = lay_blocks(heights, budget);
    for (std::size_t i = 0; i < laid.shown; ++i) {
        for (surface::SurfaceTextRow& row : blocks[i]) {
            out.push_back(std::move(row));
        }
        budget -= heights[i];
    }
    if (laid.marker) {
        out.push_back(surface::SurfaceTextRow{elision(said.powers.size() - laid.shown, columns),
                                              surface::role::kMuted});
        --budget;
    }
    if (budget > 0 && caveat > 0) {
        out.push_back(surface::SurfaceTextRow{std::string(), surface::role::kFill});
    }
    if (caveat > 0) {
        out.push_back(
            surface::SurfaceTextRow{fit(kHostResolution, columns), surface::role::kMuted});
    }
    if (source > 0) {
        out.push_back(surface::SurfaceTextRow{fit(kPowersSource, columns), surface::role::kMuted});
    }
    return out;
}

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_RESOLVED_HPP
