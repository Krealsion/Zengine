// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LOAD_PERSIST_HPP
#define ZENGINE_WORKSHOP_LOAD_PERSIST_HPP

// THE LOAD PLAN'S OWN FILE -- the third durable artifact beside the document's and
// the setup's, and separate from both for the reason those two are separate from
// each other.
//
//   a DOCUMENT is what a maker made
//   a SETUP is the arrangement of panes they were looking at while they made it
//   a LOAD PLAN is which artifacts this project runs on at all
//
// The first two are a maker's work; this one is a deployment's composition, and it
// is the only one of the three that is an EXECUTION-AUTHORITY document. A row here
// says *allow this native artifact to contribute executable semantic power to the
// host* or *allow this native artifact to participate as a Loom weave under this
// identity*. It is not harmless configuration and this file does not describe it as
// any. It is explicit precisely so that choice is visible and diffable.
//
// ---- What it shares with the other two, and what it does not ------------------
//
// It rides the same LOOM COMPAT CODEC (<zen/serialize.hpp>) for every reason
// persist.hpp gives: an already-linked dependency, the same gate the live bus uses,
// unknown-field rejection, kind validation, UTF-8 validation, a materialisation
// budget, and deterministic output that makes save -> load -> save byte-identical
// with no canonicalisation framework. No hand-written JSON parser exists here and
// none is wanted; ONE CODEC, and every door goes through it.
//
// It shares `persist::read_file` and `persist::write_file` for the same reason
// setup_persist.hpp does -- one safe-write promise rather than a second copy of one
// -- with its own ceiling and its own word for what it is reading.
//
// What it does NOT share is a shape, a version, a format word, a path, a command, or
// a validity law. Nothing in this file can make a document or a setup refuse, and
// nothing about either can make a plan refuse.
//
// ---- Why an optional surface is a LIST of at most one -------------------------
//
// Zen's wire grammar is seven kinds and none of them is `optional`: a `Message`
// field is present and non-null or the value is not serializable at all. The honest
// spelling of "may expose ZERO OR MORE runtime surfaces" is therefore the kind that
// already means exactly that, and a plan reads:
//
//     { "artifact": "zengine-operators-basic",
//       "provider": [ { "mode": "normal" } ],
//       "weave":    [] }
//
// The alternative -- one always-present record with a `"none"` sentinel in its mode
// -- would put a value nobody authored inside a surface nobody requested, and would
// make `mode` carry two unrelated questions (does this artifact provide? and how?).
// The list carries the presence question and the record carries only its own fields,
// which is also what lets the gate refuse *a weave declaration missing `role`* as a
// missing FIELD rather than as an empty string somebody has to remember to check.
//
// AT MOST ONE is the PLAN's law and not the wire's (`check_load_file`, below), for
// `check_setup`'s reason: the file says what a file may hold, and Workshop says what
// Workshop accepts.
//
// ---- What version 1 promises -------------------------------------------------
//
//   PROMISED   Workshop reads and writes load-plan format version 1, and a second
//              save of a loaded plan is byte-identical to the first.
//   REFUSED    any other `format_version`, with the number named; a `format` that is
//              not this one; a field the shape does not declare; a field of the wrong
//              kind; an unrecognised provider mode WORD, with the word found and the
//              words that would have worked both named; more than one provider or
//              weave surface on one artifact; anything the plan law refuses (an empty
//              or traversing stem, a weave with no role, an artifact requesting
//              nothing, a stem declared twice); a file larger than a plan can be.
//   ACCEPTED   a stem naming an artifact that is not on this disk. That is authored
//              intent and stays authored intent -- the RUNTIME refuses it, by name,
//              and nothing rewrites the entry (the same law an unresolved `PaneRef`
//              lives under).
//   NOT DONE   migration, a legacy reader, a version graph, an upgrade path, a dual
//              writer. There is no version 0 to migrate from.

#include "load_plan.hpp"
#include "persist.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop::load_persist {

/// What a Workshop load plan says it is. Its own word, beside and not equal to the
/// document's `zengine-workshop` or the setup's `zengine-workshop-setup`, so that
/// handing Workshop the wrong one of its three files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-load-plan";

/// The only load-plan format version this build reads or writes.
inline constexpr std::int64_t kFormatVersion = 1;

/// THE TWO PLANS WORKSHOP SHIPS, by the names they are staged under BESIDE THE
/// EXECUTABLE -- which is where they belong and where `--document`'s default
/// deliberately does not go. A document is a maker's file and lives wherever the
/// maker started Workshop; a plan names artifacts staged beside the binary, so a
/// plan resolved against a launch directory would be a plan whose artifacts are
/// somewhere else.
///
/// TWO, AND NOT A FLAG. The terminal and graphical arrangements differ by two rows
/// out of six, and the diff between these files is the whole of that difference --
/// which is what `--skin`/`--input` were for and could not show. A maker wanting a
/// third copies one of these and passes `--load-plan`; there is no plan registry, no
/// picker, no recent list and no search path.
inline constexpr const char* kDefaultLoadPlanName = "default-load-plan.json";
inline constexpr const char* kGraphicalLoadPlanName = "graphical-load-plan.json";

/// A load plan is the smallest of the three durable artifacts and its ceiling says
/// so. Sixteen kibibytes is an order of magnitude above the largest legal plan --
/// `kMaxPlanArtifacts` rows of a stem, a mode word and a role is under six kilobytes
/// with the envelope -- and it is the read side of the same law the Loom's decoder
/// applies to materialisation: a hostile file does not get to choose the cost of
/// refusing it.
inline constexpr std::uintmax_t kMaxPlanBytes = 1u << 14;

// ---- The mode WORDS, and why they are words ----------------------------------
//
// setup_persist.hpp's decision, and its reason is unchanged here: the in-memory
// value is `op::MountMode`'s enumerator and the enumerator's NUMBER is arbitrary.
// Renumber `MountMode` and every saved plan would silently change which provider
// covers which. A word cannot be renumbered -- and a maker looking at their own file
// can see what `overlay` means, which is the entire reason this artifact is text.

inline constexpr const char* kModeNormal = "normal";
inline constexpr const char* kModeOverlay = "overlay";

/// The words a provider mode may be said in. One list, in one place, so the reader
/// and the refusal cannot come to disagree about what would have worked.
inline constexpr const char* kModeWords = "normal or overlay";

// ---- The file's own shapes ---------------------------------------------------

/// PROVIDER PARTICIPATION AS WRITTEN.
///
/// Deliberately its own shape rather than `load::ProviderIntent`: that is how this
/// build HOLDS the intent and is free to change when the program does; this is what
/// a saved plan IS, and it must not change because an implementation did. The same
/// argument persist.hpp makes about `WorkshopObject`.
struct WorkshopLoadProvider {
    std::string mode;

    ZEN_SHAPE(WorkshopLoadProvider, 1, ZEN_FIELD(mode));
};

/// WEAVE PARTICIPATION AS WRITTEN. One field, because the artifact record already
/// carries the name (`load_plan.hpp` says why a role is not accompanied by one).
struct WorkshopLoadWeave {
    std::string role;

    ZEN_SHAPE(WorkshopLoadWeave, 1, ZEN_FIELD(role));
};

/// ONE ARTIFACT ROW AS WRITTEN: which artifact, and which surfaces it is asked for.
///
/// `provider` and `weave` are LISTS because the wire has no optional and "zero or
/// more surfaces" is what a list means. The plan law bounds each at one.
struct WorkshopLoadArtifact {
    std::string artifact;
    std::vector<WorkshopLoadProvider> provider;
    std::vector<WorkshopLoadWeave> weave;

    ZEN_SHAPE(WorkshopLoadArtifact, 1, ZEN_FIELD(artifact), ZEN_FIELD(provider),
              ZEN_FIELD(weave));
};

/// A WHOLE SAVED LOAD PLAN: what it is, which version of that it is, and the
/// artifacts it means to run IN AUTHORED ORDER.
struct WorkshopLoadFile {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<WorkshopLoadArtifact> artifacts;

    ZEN_SHAPE(WorkshopLoadFile, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(artifacts));
};

/// THE ENVELOPE'S SHAPE VERSION AND THE PLAN FORMAT VERSION ARE ONE NUMBER, and this
/// is where that is a compile error to break rather than a coincidence somebody has
/// to keep noticing. setup_persist.hpp's decision, taken here for the same reason it
/// was taken there: there is no history in which the two could sensibly disagree, and
/// coupling them is what lets a file from another version be refused by ITS NUMBER
/// before a single row is judged against this version's shape.
static_assert(WorkshopLoadFile::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the load plan's format version and its envelope's shape version are one "
              "number: a file from another version must be refused by ITS NUMBER, before "
              "its rows are judged against this version's shape");

// ---- Writing -------------------------------------------------------------------

/// The word for an authored mount mode. TOTAL over the enumeration, and the
/// fall-through is `normal` -- the one answer that cannot invent authority. A mode
/// this build has no word for has certainly not earned the right to COVER somebody
/// else's power, and `Ordinary` is the mode that refuses a collision.
///
/// Nothing reachable spends the fall-through: `op::MountMode` has two enumerators and
/// both are named above. It is written total for the reason `unit_word` is -- a total
/// function is cheaper than an invariant somebody maintains.
inline const char* mode_word(op::MountMode mode) {
    return mode == op::MountMode::Overlay ? kModeOverlay : kModeNormal;
}

inline WorkshopLoadFile to_file(const load::LoadPlan& plan) {
    WorkshopLoadFile out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.artifacts.reserve(plan.artifacts.size());
    for (const load::ArtifactIntent& a : plan.artifacts) {
        // AS AUTHORED. Not sorted, not reordered, not resolved against a disk, not
        // dropped for naming a file this machine does not have. The order IS the
        // plan's meaning, so a save that tidied would be a save that edited the
        // arrangement it was asked to preserve.
        WorkshopLoadArtifact row;
        row.artifact = a.stem;
        if (a.provider.has_value()) {
            row.provider.push_back(WorkshopLoadProvider{mode_word(a.provider->mode)});
        }
        if (a.weave.has_value()) {
            row.weave.push_back(WorkshopLoadWeave{a.weave->role});
        }
        out.artifacts.push_back(std::move(row));
    }
    return out;
}

inline std::string to_text(const load::LoadPlan& plan) {
    return loom::compat::serialize(loom::to_value(to_file(plan)));
}

// ---- Reading -------------------------------------------------------------------

/// What reading produced: whether it worked, and the plan if it did.
///
/// THE PLAN IS RETURNED RATHER THAN WRITTEN THROUGH A REFERENCE, which is how "a
/// malformed file never leaves a host halfway composed" is structural rather than
/// careful: there is no live value in scope here for a half-built candidate to be
/// written into. Nothing is mounted and nothing is loaded until a whole plan has
/// passed every layer.
struct LoadedPlan {
    Written outcome;
    load::LoadPlan plan;

    static LoadedPlan no(std::string why) { return LoadedPlan{Written::no(std::move(why)), {}}; }
};

/// WHAT TO SAY ABOUT A PLAN VERSION THIS BUILD DOES NOT READ. One sentence, one
/// place, so the two doors that can meet a wrong version -- the envelope's claim and
/// the file's own `format_version` field -- cannot come to word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "load plan version " + std::to_string(found) + " -- this Workshop reads version " +
           std::to_string(kFormatVersion);
}

/// The authored mode a written one means. False for a word this format does not have.
inline bool mode_in(const std::string& word, op::MountMode& out) {
    if (word == kModeNormal) {
        out = op::MountMode::Ordinary;
        return true;
    }
    if (word == kModeOverlay) {
        out = op::MountMode::Overlay;
        return true;
    }
    return false;
}

/// EVERY LAW THE FILE'S OWN GRAMMAR ADDS on top of the plan law -- which is exactly
/// one question the typed plan cannot ask, because the typed plan has already
/// answered it by construction: how MANY of each surface a row carries.
///
/// `std::optional` holds zero or one; a list holds any number. So the count is
/// checked exactly where the two representations meet and nowhere else.
inline Written check_load_file(const WorkshopLoadArtifact& row) {
    if (row.provider.size() > 1) {
        return Written::no("artifact `" + row.artifact +
                           "` declares provider participation more than once");
    }
    if (row.weave.size() > 1) {
        return Written::no("artifact `" + row.artifact +
                           "` declares weave participation more than once");
    }
    return Written::ok();
}

/// Text to a plan. Total: every input is either a plan or a refusal with a reason,
/// and nothing here throws.
///
/// FIVE LAYERS, IN ORDER, AND THE LAST ONE IS THE PLAN'S OWN LAW: the envelope must
/// parse; its CLAIM must be this version (the preflight, so a file from another
/// version is refused by its number rather than by whichever field this version
/// happens to have gained); it must admit against this shape (which is where an
/// unknown field, a wrong kind, a bad integer or invalid UTF-8 is refused, by the
/// same gate the bus uses); it must say it is this format at this version; and the
/// plan it describes must be a legal plan (`check_plan` -- the SAME function anything
/// authoring a plan in memory goes through, so a written plan and a typed one cannot
/// come to disagree about what is legal).
inline LoadedPlan from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopLoadFile>(), loom::Report::FirstError);
        return LoadedPlan::no("not a Workshop load plan: " + refused.first_error().message());
    }
    // THE VERSION PREFLIGHT, and it is an ORDERING rather than a loosening: the whole
    // candidate still meets the full shape three lines down and unknown fields are
    // still refused. What it does is answer the version question FIRST, so a file
    // from another version is refused by its number rather than by the first field
    // this version added -- which would be a true sentence about a false cause.
    if (claim.claimed_name() == std::string(WorkshopLoadFile::zen_name) &&
        claim.claimed_version() != WorkshopLoadFile::zen_version) {
        return LoadedPlan::no(wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopLoadFile>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedPlan::no(admitted.first_error().message());
    }

    const WorkshopLoadFile file = loom::from_value<WorkshopLoadFile>(admitted.value());
    if (file.format != kFormat) {
        return LoadedPlan::no("not a Workshop load plan: it says it is `" + file.format + "`");
    }
    // AND THE FIELD IS STILL CHECKED. The preflight above answers for a file whose
    // ENVELOPE is another version; this answers for one whose envelope is this
    // version and whose own stated version is not -- which only a forgery produces,
    // and which is exactly the forgery a reader of this format would try.
    if (file.format_version != kFormatVersion) {
        return LoadedPlan::no(wrong_version(file.format_version));
    }

    load::LoadPlan candidate;
    candidate.artifacts.reserve(file.artifacts.size());
    for (const WorkshopLoadArtifact& row : file.artifacts) {
        const Written counted = check_load_file(row);
        if (!counted.accepted) {
            return LoadedPlan::no(counted.refusal);
        }
        load::ArtifactIntent a;
        a.stem = row.artifact;
        if (!row.provider.empty()) {
            op::MountMode mode = op::MountMode::Ordinary;
            if (!mode_in(row.provider.front().mode, mode)) {
                // NAMES BOTH WHAT WAS FOUND AND WHAT WOULD HAVE WORKED, because a
                // maker looking at their own file can fix that. setup_persist.hpp's
                // `unknown_unit`, said about the other artifact.
                return LoadedPlan::no("`" + row.provider.front().mode +
                                      "` is not a provider mount mode (" + kModeWords + ")");
            }
            a.provider = load::ProviderIntent{mode};
        }
        if (!row.weave.empty()) {
            a.weave = load::WeaveIntent{row.weave.front().role};
        }
        candidate.artifacts.push_back(std::move(a));
    }
    const Written legal = load::check_plan(candidate);
    if (!legal.accepted) {
        return LoadedPlan::no(legal.refusal);
    }

    LoadedPlan loaded;
    loaded.outcome = Written::ok();
    loaded.plan = std::move(candidate);
    return loaded;
}

// ---- The file itself -------------------------------------------------------------

/// Save a plan to a file, through the document's own safe write: a complete candidate
/// to a sibling, then a rename over the destination.
///
/// THE PROMISE IS THE ONE `persist::write_file` MAKES and it is not restated here as
/// though it were a second mechanism. Nothing in the production host calls this --
/// Workshop READS its plan and never writes one, because a host that rewrote its own
/// authored intent is the one thing §13 forbids. It exists because a plan is a
/// durable authored artifact and a durable authored artifact whose codec cannot be
/// round-tripped is a codec nobody has checked.
inline Written save_file(const std::string& path, const load::LoadPlan& plan) {
    return persist::write_file(path, to_text(plan));
}

/// Read a plan from a file. The composition of every layer: the file, the format, and
/// the plan law.
inline LoadedPlan load_file(const std::string& path) {
    const persist::FileText read =
        persist::read_file(path, kMaxPlanBytes, "a Workshop load plan");
    if (!read.outcome.accepted) {
        return LoadedPlan{read.outcome, {}};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::load_persist

#endif // ZENGINE_WORKSHOP_LOAD_PERSIST_HPP
