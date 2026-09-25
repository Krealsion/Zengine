// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LOAD_PERSIST_HPP
#define ZENGINE_WORKSHOP_LOAD_PERSIST_HPP

// The load plan's own file: which artifacts this project runs on, the one execution-authority
// document beside the setup.

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

/// What a Workshop load plan says it is: its own word, so handing Workshop the wrong file is named
/// rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-load-plan";

/// The newest load-plan format version: what a plan authoring choices is written as.
inline constexpr std::int64_t kFormatVersion = 3;

/// The version a plan authoring no choices is written as, and the oldest this build reads.
inline constexpr std::int64_t kFormatVersionV1 = 1;
/// ...and the one between, which authors choices and no optional rows.
inline constexpr std::int64_t kFormatVersionV2 = 2;

/// The two plans Workshop ships, staged beside the executable, where the artifacts they name are.
/// Two files, not a flag: their diff is the whole difference between the terminal and graphical
/// arrangements, and a third is a copy passed with `--load-plan`.
inline constexpr const char* kDefaultLoadPlanName = "default-load-plan.json";

/// The plan a maker authors into, under the project: where `load it` writes the minimum row,
/// seeded from the plan in force at launch. The shipped default stays as it is.
// WL-AUTH-02 -- agents/workshop/authoring.md
inline constexpr const char* kProjectLoadPlanName = "workshop-plan.json";

/// WHICH PLAN IS IN FORCE AT LAUNCH -- one rule, and it is the host's. An explicit
/// `--load-plan` wins; otherwise a project plan at the captured project root, when there is
/// one; otherwise the shipped default beside the executable. `present` is the host's own
/// existence probe, handed in so the rule is a pure function a case can pin.
// WL-AUTH-02 -- agents/workshop/authoring.md
template <class Present>
inline std::string plan_in_force(const std::string& explicit_path, const std::string& project_dir,
                                 const std::string& host_dir, Present present) {
    if (!explicit_path.empty()) {
        return explicit_path;
    }
    if (!project_dir.empty()) {
        const std::string project = project_dir + "/" + kProjectLoadPlanName;
        if (present(project)) {
            return project;
        }
    }
    return host_dir + "/" + kDefaultLoadPlanName;
}
inline constexpr const char* kGraphicalLoadPlanName = "graphical-load-plan.json";

/// The ceiling: an order of magnitude above the largest legal plan, so a hostile file does not
/// choose the cost of refusing it.
inline constexpr std::uintmax_t kMaxPlanBytes = 1u << 14;

// ---- The mode words ----------------------------------------------------------
// Words, not `op::MountMode`'s numbers: a renumbered enumerator would silently change which
// provider covers which in every saved plan, and a maker can read `overlay`.

inline constexpr const char* kModeNormal = "normal";
inline constexpr const char* kModeOverlay = "overlay";

/// The words a provider mode may be said in. One list, in one place, so the reader
/// and the refusal cannot come to disagree about what would have worked.
inline constexpr const char* kModeWords = "normal or overlay";

// ---- The file's own shapes ---------------------------------------------------

/// Provider participation as written: its own shape, not `load::ProviderIntent`, since a saved
/// plan must not change because an implementation did.
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

namespace v1 {
/// ONE ARTIFACT ROW AS VERSIONS 1 AND 2 WROTE IT, RETAINED WHOLE. Every plan a maker already
/// has holds these bytes; it is read against its own shape so an old file is admitted by the
/// gate that describes it, rather than by a newer shape with a field it was never going to have.
struct WorkshopLoadArtifact {
    std::string artifact;
    std::vector<WorkshopLoadProvider> provider;
    std::vector<WorkshopLoadWeave> weave;

    ZEN_SHAPE(WorkshopLoadArtifact, 1, ZEN_FIELD(artifact), ZEN_FIELD(provider),
              ZEN_FIELD(weave));
};
} // namespace v1

/// One artifact row as written (format version 3): which artifact, which surfaces it is asked
/// for, and whether this project may stand without it. `provider` and `weave` are lists because
/// the wire has no optional; the plan law bounds each at one. `optional` is a plain `bool`: every
/// row either may be stepped over or may not, so a version-3 file says so on every row.
struct WorkshopLoadArtifact {
    std::string artifact;
    std::vector<WorkshopLoadProvider> provider;
    std::vector<WorkshopLoadWeave> weave;
    /// TRUE = this project stands without the row (`load_plan.hpp`'s `optional`).
    bool optional = false;

    ZEN_SHAPE(WorkshopLoadArtifact, 2, ZEN_FIELD(artifact), ZEN_FIELD(provider),
              ZEN_FIELD(weave), ZEN_FIELD(optional));
};

/// ONE AUTHORED CHOICE AS WRITTEN: the office, the maker's name for the choice, and the artifact.
struct WorkshopLoadChoice {
    std::string role;
    std::string name;
    std::string artifact;

    ZEN_SHAPE(WorkshopLoadChoice, 1, ZEN_FIELD(role), ZEN_FIELD(name), ZEN_FIELD(artifact));
};

/// A WHOLE SAVED LOAD PLAN: what it is, which version of that it is, the artifacts it means to
/// run IN AUTHORED ORDER, and the choices an office may be switched between.
struct WorkshopLoadFile {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<WorkshopLoadArtifact> artifacts;
    std::vector<WorkshopLoadChoice> choices;

    ZEN_SHAPE(WorkshopLoadFile, 3, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(artifacts), ZEN_FIELD(choices));
};

/// VERSION 1, RETAINED WHOLE: the envelope every plan written before choices carries, read
/// against its own shape so a version-1 file is admitted by the gate that describes its bytes.
namespace v1 {
struct WorkshopLoadFile {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<v1::WorkshopLoadArtifact> artifacts;

    ZEN_SHAPE(WorkshopLoadFile, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(artifacts));
};
} // namespace v1

/// VERSION 2, RETAINED WHOLE for version 1's reason, one version on: the envelope a plan
/// written with choices and before optional rows carries.
namespace v2 {
struct WorkshopLoadFile {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<v1::WorkshopLoadArtifact> artifacts;
    std::vector<WorkshopLoadChoice> choices;

    ZEN_SHAPE(WorkshopLoadFile, 2, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(artifacts), ZEN_FIELD(choices));
};
} // namespace v2

/// The envelope's shape version and the plan format version are one number, so a file from
/// another version is refused by its number before a row is judged.
static_assert(WorkshopLoadFile::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the load plan's format version and its envelope's shape version are one "
              "number: a file from another version must be refused by ITS NUMBER, before "
              "its rows are judged against this version's shape");
static_assert(v1::WorkshopLoadFile::zen_version == static_cast<std::uint32_t>(kFormatVersionV1),
              "version 1's retained envelope carries version 1's number");
static_assert(v2::WorkshopLoadFile::zen_version == static_cast<std::uint32_t>(kFormatVersionV2),
              "version 2's retained envelope carries version 2's number");

// ---- Writing -------------------------------------------------------------------

/// The word for an authored mount mode, total, falling through to `normal`: a mode this build
/// cannot name must not cover somebody else's power.
inline const char* mode_word(op::MountMode mode) {
    return mode == op::MountMode::Overlay ? kModeOverlay : kModeNormal;
}

inline std::vector<WorkshopLoadArtifact> artifact_rows(const load::LoadPlan& plan) {
    std::vector<WorkshopLoadArtifact> rows;
    rows.reserve(plan.artifacts.size());
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
        row.optional = a.optional;
        rows.push_back(std::move(row));
    }
    return rows;
}

/// THE SAME ROWS AS VERSIONS 1 AND 2 WROTE THEM -- reachable only for a plan that marks no row
/// optional, which is what `to_text` checks before it chooses one of those versions.
inline std::vector<v1::WorkshopLoadArtifact> artifact_rows_v1(const load::LoadPlan& plan) {
    std::vector<v1::WorkshopLoadArtifact> rows;
    rows.reserve(plan.artifacts.size());
    for (const load::ArtifactIntent& a : plan.artifacts) {
        v1::WorkshopLoadArtifact row;
        row.artifact = a.stem;
        if (a.provider.has_value()) {
            row.provider.push_back(WorkshopLoadProvider{mode_word(a.provider->mode)});
        }
        if (a.weave.has_value()) {
            row.weave.push_back(WorkshopLoadWeave{a.weave->role});
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

/// Does this plan need the version that can say so?
inline bool authors_optional(const load::LoadPlan& plan) {
    for (const load::ArtifactIntent& a : plan.artifacts) {
        if (a.optional) {
            return true;
        }
    }
    return false;
}

inline WorkshopLoadFile to_file(const load::LoadPlan& plan) {
    WorkshopLoadFile out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.artifacts = artifact_rows(plan);
    for (const load::ChoiceIntent& c : plan.choices) {
        out.choices.push_back(WorkshopLoadChoice{c.role, c.name, c.stem});
    }
    return out;
}

/// THE PLAN AS VERSION 1 -- meaningful only for a plan that authors no choices and no
/// optional rows.
inline v1::WorkshopLoadFile to_file_v1(const load::LoadPlan& plan) {
    v1::WorkshopLoadFile out;
    out.format = kFormat;
    out.format_version = kFormatVersionV1;
    out.artifacts = artifact_rows_v1(plan);
    return out;
}

/// THE PLAN AS VERSION 2 -- choices, and no optional rows.
inline v2::WorkshopLoadFile to_file_v2(const load::LoadPlan& plan) {
    v2::WorkshopLoadFile out;
    out.format = kFormat;
    out.format_version = kFormatVersionV2;
    out.artifacts = artifact_rows_v1(plan);
    for (const load::ChoiceIntent& c : plan.choices) {
        out.choices.push_back(WorkshopLoadChoice{c.role, c.name, c.stem});
    }
    return out;
}

/// THE SMALLEST VERSION THAT SAYS THE PLAN: version 1 while it authors neither choices nor
/// optional rows, so every plan written before either existed is written again exactly as it
/// was; version 2 while it authors choices and no optional row; version 3 otherwise.
inline std::string to_text(const load::LoadPlan& plan) {
    if (!authors_optional(plan)) {
        if (plan.choices.empty()) {
            return loom::compat::serialize(loom::to_value(to_file_v1(plan)));
        }
        return loom::compat::serialize(loom::to_value(to_file_v2(plan)));
    }
    return loom::compat::serialize(loom::to_value(to_file(plan)));
}

// ---- Reading -------------------------------------------------------------------

/// What reading produced. The plan is returned, not written through a reference, so a malformed
/// file never leaves a host half composed.
struct LoadedPlan {
    Written outcome;
    load::LoadPlan plan;

    static LoadedPlan no(std::string why) { return LoadedPlan{Written::no(std::move(why)), {}}; }
};

/// WHAT TO SAY ABOUT A PLAN VERSION THIS BUILD DOES NOT READ. One sentence, one
/// place, so the two doors that can meet a wrong version -- the envelope's claim and
/// the file's own `format_version` field -- cannot come to word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "load plan version " + std::to_string(found) + " -- this Workshop reads versions " +
           std::to_string(kFormatVersionV1) + ", " + std::to_string(kFormatVersionV2) + " and " +
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

/// The one law the file's grammar adds to the plan law: how many of each surface a row carries,
/// which a list can exceed and `std::optional` cannot.
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

/// Text to a plan; total. Five layers in order: the envelope parses; its claim is a version this
/// build reads (refused by its number, not by a field this version gained); it admits against
/// that version's shape at full strength; it says this format at this version; and the plan is
/// legal (`check_plan`, the function a plan authored in memory meets too).
inline LoadedPlan from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopLoadFile>(), loom::Report::FirstError);
        return LoadedPlan::no("not a Workshop load plan: " + refused.first_error().message());
    }
    // The version preflight orders, never loosens: each version is admitted against its own
    // retained shape, so an older file is never refused by a field it was never going to have.
    const bool claims_v1 = claim.claimed_name() == std::string(WorkshopLoadFile::zen_name) &&
                           claim.claimed_version() == v1::WorkshopLoadFile::zen_version;
    const bool claims_v2 = claim.claimed_name() == std::string(WorkshopLoadFile::zen_name) &&
                           claim.claimed_version() == v2::WorkshopLoadFile::zen_version;
    if (claim.claimed_name() == std::string(WorkshopLoadFile::zen_name) && !claims_v1 &&
        !claims_v2 && claim.claimed_version() != WorkshopLoadFile::zen_version) {
        return LoadedPlan::no(wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    WorkshopLoadFile file;
    if (claims_v1) {
        const loom::Admission old =
            loom::admit(claim, loom::schema_of<v1::WorkshopLoadFile>(), loom::Report::FirstError);
        if (!old.ok()) {
            return LoadedPlan::no(old.first_error().message());
        }
        v1::WorkshopLoadFile read = loom::from_value<v1::WorkshopLoadFile>(old.value());
        file.format = std::move(read.format);
        file.format_version = read.format_version;
        for (v1::WorkshopLoadArtifact& row : read.artifacts) {
            file.artifacts.push_back(WorkshopLoadArtifact{std::move(row.artifact),
                                                          std::move(row.provider),
                                                          std::move(row.weave), false});
        }
    } else if (claims_v2) {
        const loom::Admission old =
            loom::admit(claim, loom::schema_of<v2::WorkshopLoadFile>(), loom::Report::FirstError);
        if (!old.ok()) {
            return LoadedPlan::no(old.first_error().message());
        }
        v2::WorkshopLoadFile read = loom::from_value<v2::WorkshopLoadFile>(old.value());
        file.format = std::move(read.format);
        file.format_version = read.format_version;
        file.choices = std::move(read.choices);
        for (v1::WorkshopLoadArtifact& row : read.artifacts) {
            file.artifacts.push_back(WorkshopLoadArtifact{std::move(row.artifact),
                                                          std::move(row.provider),
                                                          std::move(row.weave), false});
        }
    } else {
        const loom::Admission admitted =
            loom::admit(claim, loom::schema_of<WorkshopLoadFile>(), loom::Report::FirstError);
        if (!admitted.ok()) {
            return LoadedPlan::no(admitted.first_error().message());
        }
        file = loom::from_value<WorkshopLoadFile>(admitted.value());
    }
    if (file.format != kFormat) {
        return LoadedPlan::no("not a Workshop load plan: it says it is `" + file.format + "`");
    }
    // AND THE FIELD IS STILL CHECKED. The preflight above answers for a file whose
    // ENVELOPE is another version; this answers for one whose envelope is one version
    // and whose own stated version is another -- which only a forgery produces,
    // and which is exactly the forgery a reader of this format would try.
    const std::int64_t envelope =
        claims_v1 ? kFormatVersionV1 : (claims_v2 ? kFormatVersionV2 : kFormatVersion);
    if (file.format_version != envelope) {
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
        a.optional = row.optional;
        candidate.artifacts.push_back(std::move(a));
    }
    for (const WorkshopLoadChoice& c : file.choices) {
        candidate.choices.push_back(load::ChoiceIntent{c.role, c.name, c.artifact});
    }
    // One plan, one spelling: a file in a version larger than its plan needs is refused, or a
    // save would rewrite a file nobody edited.
    if (!claims_v1 && !claims_v2 && !authors_optional(candidate)) {
        return LoadedPlan::no("a version-3 load plan marks a row optional; a plan with none is "
                              "written as version " +
                              std::to_string(candidate.choices.empty() ? kFormatVersionV1
                                                                       : kFormatVersionV2));
    }
    if (claims_v2 && candidate.choices.empty()) {
        return LoadedPlan::no("a version-2 load plan authors choices; a plan with none is "
                              "written as version 1");
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

/// Save a plan through `persist::write_file`'s safe write. The host calls it for one act, the
/// maker's `load it` on the project plan; Workshop never rewrites authored intent on its own.
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
