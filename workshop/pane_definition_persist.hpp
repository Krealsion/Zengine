// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_DEFINITION_PERSIST_HPP
#define ZENGINE_WORKSHOP_PANE_DEFINITION_PERSIST_HPP

// A MAKER-MADE PANE'S OWN FILE -- the ninth durable artifact, and a PROJECT one.
//
// ---- Why it is its own file ----------------------------------------------------------------
//
// A pane definition is what a maker MADE: the inside of a pane they created. It is not the
// document (a different subject, on a different lattice, with its own promise), not a
// setup (one desk's participation and geometry -- an interior welded to one arrangement
// would make the same pane unsayable on a second desk), not the session (machine-local
// "the desk I was using"; a definition must survive a session being thrown away), not a
// provider's (a maker-made pane has no provider, and a provider cannot be told to own a
// maker's file), and not generated code (that would make C++ the only intelligible
// representation again, which is the premise this artifact exists to end).
//
// So it follows the project -- the launch directory, or the path the maker typed -- beside
// the document and the named setup, under the same durable-file discipline every other
// Workshop file keeps: its own format word, an explicit version, a pinned envelope, a
// derived byte ceiling, whole-value admission, its own semantic law, a safe write through a
// sibling, refusal by number and by shape, and never a rewrite of bytes this build could
// not read.
//
// ---- What it shares with the family, and what it does not ----------------------------------
//
// It rides the same LOOM COMPAT CODEC (<zen/serialize.hpp>) for every reason `persist.hpp`
// gives: an already-linked dependency, the same gate the live bus uses, unknown-field
// rejection, kind validation, UTF-8 validation, a materialisation budget, and deterministic
// output so that save -> load -> save is byte-identical with no canonicalisation framework.
// It shares `persist::write_file` (a complete candidate to a sibling, then a rename) and
// `persist::read_file` (a ceiling before a byte is read).
//
// What it does NOT share is a shape, a version, a format word, a path, a gesture or a law.
// Nothing here can make the document, a setup or a session refuse, and none of them can
// make a definition refuse.
//
// ---- What the file deliberately cannot say ---------------------------------------------------
//
// Search this file for a pixel, a cell count, a row or column capacity, a font metric, a
// canvas coordinate, a medium identity, a callback, a role, a key binding, a path, a
// callable, a grant, a provider, an operator, a Source or a message destination, and find
// none: the two shapes below are the only things serialised, and they hold a name, a mint,
// and per region an id, a kind WORD, four fine-lattice numbers and a line of text. A file
// that admits against them can describe a picture and can do nothing else -- loading one
// reads, parses, admits, judges, holds and presents, and there is no field through which
// it could mount, send, sample, invoke, bind, open or grant.
//
// ---- What version 1 promises ----------------------------------------------------------------
//
//   PROMISED   this build reads and writes format version 1, and a second save of a loaded
//              definition is byte-identical to the first.
//   REFUSED    any other `format_version`, by its NUMBER, before its rows are judged; a
//              `format` that is not this one; a field the shape does not declare; a field
//              of the wrong kind; a kind WORD this build has no region for, with the word
//              found and the word that would have worked both named; a name, a region or a
//              mint the definition's own law refuses; a file larger than a definition can
//              be. A refusal is WHOLE: no field of a refused candidate reaches a live value.
//   NOT DONE   a version graph, a legacy reader, an upgrade path, a migration. There is one
//              version and nothing yet to migrate from; the day this format moves, the
//              retired shape goes to a history namespace and a conversion operator, the
//              session file's own road.

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

/// THE READ CEILING, DERIVED FROM THE BOUNDS THAT BOUND THE LIST (the session file's own
/// lesson: a ceiling is part of a shape, and a bound the singular earned must be re-derived
/// when the value becomes a list). A maximal region is its seven fields at their longest --
/// an int64 quoted is at most 22 bytes, the text at most `kMaxRegionTextLen` bytes each of
/// which JSON may escape to two -- and the definition is `kMaxRegions` of those plus a name
/// and an envelope. The sum is a few tens of kibibytes at the very worst; sixty-four
/// kibibytes holds it with room, and the `static_assert` below is what keeps that true
/// when a bound above moves.
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
/// sub-units relative to the pane's interior, and its text. Deliberately its own shape
/// rather than `TextRegion`'s: that struct is how this build HOLDS a region and is free to
/// change when the program does; this is what a saved pane IS, a promise to a file a maker
/// owns.
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
///
/// `next_id` is here for the document's reason: without it a loader has to guess the
/// mint, and the only guess recycles the identity of a region deleted before the save.
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

/// The definition, as the value that gets written. SERIALIZATION IS OBSERVATION: the
/// argument is const, order is the definition's, and nothing is normalised, rounded,
/// resolved, quantized or tidied on the way out -- a fine value nobody's face can show
/// exactly is written exactly as the maker authored it.
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
///
/// THE DEFINITION IS RETURNED RATHER THAN WRITTEN THROUGH A REFERENCE, which is how "a
/// malformed file never leaves the live definition half replaced" is structural rather
/// than careful: there is no live value in scope here for a half-built candidate to be
/// written into. The caller installs only what it was handed, and it is handed only a
/// definition that passed every layer.
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
///
/// FOUR LAYERS, IN ORDER, AND THE LAST IS THE DEFINITION'S OWN LAW: the envelope must
/// parse; a claim of another version is refused by its NUMBER before a row is looked at;
/// the bytes must admit against this shape (unknown field, wrong kind, bad integer, invalid
/// UTF-8 -- the bus's own gate); and the value must be a legal definition
/// (`check_definition`, the SAME function the Pane Creator's own doors spend).
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
