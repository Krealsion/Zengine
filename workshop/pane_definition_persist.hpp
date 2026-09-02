// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_DEFINITION_PERSIST_HPP
#define ZENGINE_WORKSHOP_PANE_DEFINITION_PERSIST_HPP

// A MAKER-MADE PANE'S OWN FILE -- the ninth durable artifact, and a PROJECT one.
// Workshop law: agents/workshop/maker-pane.md

#include "pane_definition.hpp"
#include "persist.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop::pane_definition_persist {

/// What a Workshop pane-definition file says it is -- its own word, beside the family's
/// others, so handing Workshop the wrong one of its own files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-pane";

/// The only pane-definition format version this build reads or writes.
inline constexpr std::int64_t kFormatVersion = 1;

/// The file's suggested name, beside the document's and the setup's under the project.
inline constexpr const char* kDefaultPaneFileName = "workshop-pane.json";

/// THE REGION KIND, AS A WORD. The in-memory integer is arbitrary and a renumber would
/// silently change every saved pane; a word cannot be renumbered, and an unknown word is
/// refused rather than defaulted -- `persist.hpp`'s own argument about an extent's mode.
inline constexpr const char* kKindText = "text";
inline constexpr const char* kKindWords = "text";

/// THE READ CEILING, DERIVED FROM THE BOUNDS THAT BOUND THE LIST.
// WL-MAKER-10 -- agents/workshop/maker-pane.md
inline constexpr std::uintmax_t kMaxRegionFileBytes =
    7u * 32u + 2u * static_cast<std::uintmax_t>(kMaxRegionTextLen) + 64u;
inline constexpr std::uintmax_t kMaxPaneDefinitionBytes = 1u << 16;
static_assert(static_cast<std::uintmax_t>(kMaxRegions) * kMaxRegionFileBytes +
                      kMaxMakerPaneNameLen + 256u <=
                  kMaxPaneDefinitionBytes,
              "the pane-definition read ceiling must hold a maximal legal definition: a file "
              "this build writes must never be one it refuses to read");

// ---- The file's own shapes ------------------------------------------------------------------

/// ONE REGION AS WRITTEN: its identity, its kind as a word, its authored geometry in
/// sub-units relative to the pane's interior, and its text.
// WL-MAKER-10 -- agents/workshop/maker-pane.md
struct WorkshopPaneRegion {
    std::int64_t id = 0;
    std::string kind;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::string text;

    ZEN_SHAPE(WorkshopPaneRegion, 1, ZEN_FIELD(id), ZEN_FIELD(kind), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(text));
};

/// A WHOLE SAVED PANE: what it is, which version of that it is, its durable name, the next
/// region id to mint, and the regions in AUTHORED ORDER.
// WL-MAKER-10 -- agents/workshop/maker-pane.md
struct WorkshopPaneDefinition {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::int64_t next_id = 0;
    std::vector<WorkshopPaneRegion> regions;

    ZEN_SHAPE(WorkshopPaneDefinition, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(name), ZEN_FIELD(next_id), ZEN_FIELD(regions));
};

/// The envelope's shape version and the format version are ONE NUMBER -- the family's
/// coupling, for the family's reason: a file of another version is refused by ITS number,
/// before its rows are judged against this shape.
static_assert(WorkshopPaneDefinition::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the pane-definition file's format version and its envelope's shape version are "
              "one number: a file of another version must be refused by ITS number, before "
              "its regions are judged against this shape");

// ---- Writing --------------------------------------------------------------------------------

/// The word for a region's kind. Total over the integer, for `unit_word`'s reason: nothing
/// reachable spends the fall-through, because every door judges the kind, and a total
/// function is cheaper than an invariant somebody maintains.
inline const char* kind_word(std::int64_t kind) {
    (void)kind;
    return kKindText;
}

/// The definition, as the value that gets written.
// WL-MAKER-06 -- agents/workshop/maker-pane.md
inline WorkshopPaneDefinition to_file(const PaneDefinition& d) {
    WorkshopPaneDefinition out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.name = d.name;
    out.next_id = d.next_id;
    out.regions.reserve(d.regions.size());
    for (const TextRegion& r : d.regions) {
        out.regions.push_back(
            WorkshopPaneRegion{r.id, kind_word(r.kind), r.x, r.y, r.w, r.h, r.text});
    }
    return out;
}

inline std::string to_text(const PaneDefinition& d) {
    return loom::compat::serialize(loom::to_value(to_file(d)));
}

// ---- Reading --------------------------------------------------------------------------------

/// What reading produced: whether it worked, and the definition if it did.
// WL-MAKER-08 -- agents/workshop/maker-pane.md
struct LoadedDefinition {
    Written outcome;
    PaneDefinition definition;

    static LoadedDefinition no(std::string why) {
        return LoadedDefinition{Written::no(std::move(why)), {}};
    }
};

/// What to say about a version this build does not read: one sentence, one place.
inline std::string wrong_version(std::int64_t found) {
    return "pane definition version " + std::to_string(found) + " -- this Workshop reads version " +
           std::to_string(kFormatVersion);
}

/// The region kind a written word means. False for a word this format has none for.
inline bool kind_in(const std::string& word, std::int64_t& out) {
    if (word == kKindText) {
        out = region_kind::kText;
        return true;
    }
    return false;
}

/// A WRITTEN DEFINITION AS A LIVE ONE -- its format word, its version, its kind words and
/// its law, in that order, into a LOCAL candidate that is assigned out only once every
/// layer has passed.
inline Written definition_in(const WorkshopPaneDefinition& file, PaneDefinition& out) {
    if (file.format != kFormat) {
        return Written::no("not a Workshop pane definition: it says it is `" + file.format + "`");
    }
    // THE FIELD IS CHECKED EVEN THOUGH THE ENVELOPE ALREADY WAS: the preflight in
    // `from_text` answers for a file whose envelope claims another version; this answers
    // for one whose envelope is this version and whose own stated version is not -- which
    // only a forgery produces, and which is exactly the forgery a reader of this format
    // would try.
    if (file.format_version != kFormatVersion) {
        return Written::no(wrong_version(file.format_version));
    }
    PaneDefinition candidate;
    candidate.name = file.name;
    candidate.next_id = file.next_id;
    candidate.regions.reserve(file.regions.size());
    for (const WorkshopPaneRegion& w : file.regions) {
        TextRegion r;
        r.id = w.id;
        if (!kind_in(w.kind, r.kind)) {
            return Written::no("region #" + std::to_string(w.id) + ": `" + w.kind +
                               "` is not a region kind (" + kKindWords + ")");
        }
        // COPIED, NEVER JUDGED HERE. Whether the numbers are on the lattice and the text is
        // sayable is the definition's own law, asked once on the whole candidate below.
        r.x = w.x;
        r.y = w.y;
        r.w = w.width;
        r.h = w.height;
        r.text = w.text;
        candidate.regions.push_back(std::move(r));
    }
    const Written legal = check_definition(candidate);
    if (!legal.accepted) {
        return legal;
    }
    out = std::move(candidate);
    return Written::ok();
}

/// Text to a definition. Total: every input is either a definition or a refusal with a
/// reason, and nothing here throws.
// WL-MAKER-10 -- agents/workshop/maker-pane.md
inline LoadedDefinition from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused = loom::admit(
            claim, loom::schema_of<WorkshopPaneDefinition>(), loom::Report::FirstError);
        return LoadedDefinition::no("not a Workshop pane definition: " +
                                    refused.first_error().message());
    }
    if (claim.claimed_name() == std::string(WorkshopPaneDefinition::zen_name) &&
        claim.claimed_version() != WorkshopPaneDefinition::zen_version) {
        return LoadedDefinition::no(
            wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    const loom::Admission admitted = loom::admit(
        claim, loom::schema_of<WorkshopPaneDefinition>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedDefinition::no(admitted.first_error().message());
    }
    PaneDefinition candidate;
    const Written understood =
        definition_in(loom::from_value<WorkshopPaneDefinition>(admitted.value()), candidate);
    if (!understood.accepted) {
        return LoadedDefinition::no(understood.refusal);
    }
    LoadedDefinition loaded;
    loaded.outcome = Written::ok();
    loaded.definition = std::move(candidate);
    return loaded;
}

// ---- The file itself -------------------------------------------------------------------------

/// Save a definition to a file, through the family's one safe write: a complete candidate to
/// a sibling, then a rename over the destination. The promise is `persist::write_file`'s and
/// is not restated as a second mechanism.
inline Written save_file(const std::string& path, const PaneDefinition& d) {
    return persist::write_file(path, to_text(d));
}

/// Read a definition from a file: the ceiling, the format, and the definition's law.
inline LoadedDefinition load_file(const std::string& path) {
    const persist::FileText read =
        persist::read_file(path, kMaxPaneDefinitionBytes, "a Workshop pane definition");
    if (!read.outcome.accepted) {
        return LoadedDefinition{read.outcome, {}};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::pane_definition_persist

#endif // ZENGINE_WORKSHOP_PANE_DEFINITION_PERSIST_HPP
