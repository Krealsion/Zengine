// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_PERSIST_HPP
#define ZENGINE_WORKSHOP_SETUP_PERSIST_HPP

// THE SETUP'S OWN FILE -- a second artifact, separate from the document's, and
// separate on purpose.
//
// ---- Why it is not in the document's file ---------------------------------
//
// A document is what a maker MADE; a setup is what they were LOOKING AT while
// they made it. Two facts, two lifetimes, two audiences:
//
//   the same document is worth opening in two different arrangements
//   the same arrangement is worth using over two different documents
//
// A single project/workspace container would make each of those impossible to
// say, and it would make every save of one an edit to the other's bytes. So the
// separation is enforced by the strongest thing available -- they are different
// files, with different format identities, written and read by different
// functions -- and the two commands stay apart: `^s`/`^o` are the DOCUMENT's,
// and they save and load nothing else.
//
// ---- What it shares with the document's file, and what it does not ---------
//
// It rides the same LOOM COMPAT CODEC (<zen/serialize.hpp>) for every reason
// persist.hpp gives for the document: an already-linked dependency, the same
// gate the live bus uses, unknown-field rejection, kind validation, UTF-8
// validation, a materialisation budget, and deterministic output that makes
// save -> load -> save byte-identical with no canonicalisation framework.
//
// It shares the SAFE-WRITE mechanism too (`persist::write_file`), because that
// is one genuinely identical invariant -- write a complete candidate to a
// sibling and rename over the destination, so a detected failure cannot destroy
// the last good file -- and reimplementing it here would be a second copy of a
// promise. It shares `persist::read_file` for the same reason, with its own
// ceiling and its own word for what it is reading.
//
// What it does NOT share is a shape, a version, a format word, a path, a
// command, or a validity law. Nothing in this file can make the document
// refuse, and nothing about the document can make a setup refuse.
//
// ---- What version 3 promises (WUX-2) ---------------------------------------
//
//   PROMISED   Workshop reads and writes setup format version 3 — authored
//              geometry in SUB-CELL UNITS (1/surface::kCellSubs of a canvas
//              cell, unit word `subcells`) — and a second save of a loaded
//              setup is byte-identical to the first. It also READS version 2,
//              the whole-cell format WIND-2 shipped: a v2 file's cells map to
//              exactly equivalent sub-unit values (x kCellSubs, exact), so a
//              desk saved before the lattice got finer resolves to the pixel it
//              always did. Loading is not rewriting: the file on disk changes
//              only when a maker saves, and the save writes version 3.
//   REFUSED    any OTHER `format_version`, with the number named -- version 1
//              still by its NUMBER rather than by the shape of its rows; a
//              `format` that is not this one; a field the shape does not
//              declare; a field of the wrong kind; an unrecognised mode WORD, with
//              the word found and the words that would have worked both named --
//              `cells` is version 2's word and is not in version 3's vocabulary;
//              a row, a name or a rank the setup law refuses; a file larger than
//              a setup can be.
//   ACCEPTED   a well-formed reference this build cannot resolve, with all of its
//              authored window intent. That is not an error and must never become
//              one -- it is the case the two-string reference exists for.
//              And a `pixels` size, on every medium: the unit is a legal authored
//              value everywhere and is refused at PROJECTION, never here (its
//              amounts are device pixels in both versions and are not scaled).
//   NOT DONE   a version graph, an upgrade path past v2, a dual writer, or a
//              general migration framework. WIND-2's clean-break stance stands
//              for every OTHER transition; the v2 reader exists because WUX-2's
//              contract says a maker's whole-cell desk must not silently break,
//              and it is one namespace of retained shapes plus one exact
//              multiply -- not a policy.
//
// ---- Why the version is read BEFORE the rows (WIND-2) ---------------------
//
// A version-1 file's pane rows carry `provider` and `pane` and nothing else, so
// admitting it against version 2's shape fails on a MISSING FIELD -- and
// `a Workshop setup pane is missing `place`` is a true sentence that tells a maker
// nothing about what is actually wrong with their file. The version is the real
// cause and it has to be the reported one.
//
// So `from_text` reads the ENVELOPE first, through a shape carrying only `format`
// and `format_version`, and answers the version question before the rows are
// admitted at all. That is a preflight and not a loosening: the whole candidate
// still meets the full shape afterwards, unknown fields are still refused, and
// nothing about ordinary admission was made more permissive to buy the ordering.
//
// NO INTEGER PANEL KIND APPEARS ANYWHERE IN THE PERSISTED REPRESENTATION, which
// is the invariant a reader should check this file against first: search it for
// `panel::` and find nothing. Neither does a RESOLVED RECTANGLE, a slot, a screen
// extent, a text metric, a medium identity, a mounted state, a selection, a
// management mode or a pointer gesture -- what version 2 added is authored INTENT
// and the list of what a file may not hold did not move.

#include "persist.hpp"
#include "setup.hpp"

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

namespace zengine::workshop::setup_persist {

/// What a Workshop setup file says it is. Its own word, beside and not equal to
/// the document's `zengine-workshop`, so that handing Workshop the wrong one of
/// its own two files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-setup";

/// The setup format version this build WRITES, and the newest it reads.
inline constexpr std::int64_t kFormatVersion = 3;

/// The one older version this build still reads (WUX-2's contract): WIND-2's
/// whole-cell format, translated on load — never rewritten in place.
inline constexpr std::int64_t kLegacyFormatVersion = 2;

/// A setup is a smaller thing than a document, and its ceiling says so. Sixty-four
/// kibibytes is more than an order of magnitude above the largest legal setup --
/// `kMaxSetupPanes` references of `kMaxPaneKeyLen` each, plus each row's authored
/// place, two sizes and rank, plus a name, is a few kilobytes with the envelope
/// (version 2 roughly doubled a row and did not approach this) -- and it is the
/// read side of the same law the
/// Loom's decoder applies to materialisation: a hostile file does not get to
/// choose the cost of refusing it.
inline constexpr std::uintmax_t kMaxSetupBytes = 1u << 16;

// ---- The mode WORDS, and why they are words ----------------------------------
//
// THE SAME DECISION persist.hpp MADE ABOUT AN EXTENT'S MODE, and its own words
// state the reason: "It is a CLOSED set, deliberately. The Loom's gate proves the
// field is text; only Workshop knows which text is a mode, so an unrecognised word
// is refused here rather than defaulted to cells. Defaulting would silently turn a
// document Workshop does not understand into one it does."
//
// The in-memory values are 0/1/2 and they are ARBITRARY: renumber `pane_unit` and
// every saved setup silently changes its geometry. A word cannot be renumbered.

inline constexpr const char* kUnitDefault = "default";
/// VERSION 3'S GEOMETRY WORD (WUX-2): amounts in 1/surface::kCellSubs of a
/// canvas cell. The word changed WITH the unit on purpose — a v3 file spelling
/// `cells` over sub-unit amounts would be a number wearing the wrong unit's
/// name, the exact lie a mode word exists to make impossible. Version 2's
/// `cells` lives only in the legacy namespace below.
inline constexpr const char* kUnitSubcells = "subcells";
inline constexpr const char* kUnitPixels = "pixels";

/// The words a PLACE may be said in, and the words a SIZE may be said in -- two
/// lists, because they are two different closed sets. A place has no pixel mode:
/// `pane_unit::kPixels` is a SIZE unit, and a file offering it for a place is
/// offering a word that is not in this field's vocabulary.
inline constexpr const char* kPlaceWords = "default or subcells";
inline constexpr const char* kSizeWords = "default, subcells or pixels";

// ---- The file's own shapes ---------------------------------------------------

/// ONE AXIS OF AN AUTHORED SIZE AS WRITTEN: a mode a person can read, and an
/// amount. `WorkshopExtent`'s shape (persist.hpp), asked about a pane -- and
/// deliberately its OWN shape rather than that one, because the two answer to
/// different laws and a shared shape would make a change to either an edit to
/// both.
struct WorkshopPaneSize {
    std::string mode;
    std::int64_t amount = 0;

    /// Version 2 (WUX-2): the amount's geometry unit became sub-cells and the mode
    /// word moved with it. Same fields — the version IS the semantic gate.
    ZEN_SHAPE(WorkshopPaneSize, 2, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// AN AUTHORED PLACE AS WRITTEN. One mode for the pair, for `PanePlace`'s reason.
struct WorkshopPanePlace {
    std::string mode;
    std::int64_t x = 0;
    std::int64_t y = 0;

    /// Version 2 (WUX-2): coordinates in sub-cells, word `subcells`.
    ZEN_SHAPE(WorkshopPanePlace, 2, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y));
};

/// ONE PANE ROW AS WRITTEN: the durable reference, the authored window, and how
/// far forward it sits.
///
/// Deliberately its own shape rather than the value's: `SetupPane` is how this
/// build HOLDS a row and is free to change when the program does; this is what a
/// saved setup IS, and it must not change because an implementation did. The same
/// argument persist.hpp makes about `WorkshopObject`.
///
/// VERSION 2, because it grew four members and a published shape is immutable;
/// VERSION 3 (WUX-2), because the geometry those members carry moved to the fine
/// lattice underneath it.
struct WorkshopSetupPane {
    std::string provider;
    std::string pane;
    WorkshopPanePlace place;
    WorkshopPaneSize width;
    WorkshopPaneSize height;
    std::int64_t front = 0;

    ZEN_SHAPE(WorkshopSetupPane, 3, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(place),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(front));
};

/// A WHOLE SAVED SETUP: what it is, which version of that it is, what a maker
/// calls it, and the panes it means to have open IN AUTHORED ORDER.
struct WorkshopSetup {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::vector<WorkshopSetupPane> panes;

    /// Version 2, because the rows it holds grew four fields; version 3 (WUX-2),
    /// because their geometry became sub-cell units.
    ZEN_SHAPE(WorkshopSetup, 3, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(panes));
};

/// THE ENVELOPE'S SHAPE VERSION AND THE SETUP FORMAT VERSION ARE ONE NUMBER
/// (WIND-2), and this is where that is a compile error to break rather than a
/// coincidence somebody has to keep noticing.
///
/// WHY THEY ARE COUPLED HERE AND NOT IN persist.hpp. The document's file keeps the
/// two claims apart deliberately -- a shape may grow a member for a reason that is
/// not a change to what a document MEANS. A setup has no such history: both of its
/// versions moved for the same reason at the same moment, and there is no
/// migration path in which they could sensibly disagree.
///
/// WHAT IT BUYS is the ORDERING §3.2 requires. A version-1 file's bytes claim
/// `WorkshopSetup v1`; the door is `WorkshopSetup v2`; so the gate refuses it on
/// the CLAIM, before it has looked at a single pane row -- which means a
/// version-1 file can never be reported as "a pane row is missing `place`". The
/// preflight in `from_text` reads that claimed version and says it in Workshop's
/// own words, by number.
static_assert(WorkshopSetup::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the setup file's format version and its envelope's shape version are one "
              "number: a version-1 file must be refused by ITS NUMBER, before its rows are "
              "judged against version 2's shape");

// ---- Writing -------------------------------------------------------------------

/// The setup, as the value that gets written.
///
/// NOTHING IS SORTED, NORMALISED, RESOLVED OR DROPPED ON THE WAY OUT. The
/// argument is const and this function reads it once: the order is the setup's,
/// the keys are byte-for-byte what came in, and a reference this build cannot
/// resolve is written exactly as it was read. A save that tidied would be a save
/// that edited the work it was asked to preserve -- and here it would do it to a
/// reference belonging to somebody who is not in the room.
/// The word for an authored unit. TOTAL over the integer, because a value can
/// reach this function from a place no law judged -- and the fall-through is
/// `default`, which is the one answer that cannot invent geometry: it says "no
/// override", which is what a mode this build has no word for has certainly not
/// earned the right to be treated as more than.
///
/// Nothing reachable spends the fall-through: `check_setup` refuses an unknown
/// mode and every door goes through it. It is written for the reason `panel_kind`
/// is total -- a total function is cheaper than an invariant somebody maintains.
inline const char* unit_word(std::int64_t mode) {
    if (mode == pane_unit::kSubcells) {
        return kUnitSubcells;
    }
    if (mode == pane_unit::kPixels) {
        return kUnitPixels;
    }
    return kUnitDefault;
}

inline WorkshopPanePlace place_out(const PanePlace& p) {
    return WorkshopPanePlace{unit_word(p.mode), p.x, p.y};
}

inline WorkshopPaneSize size_out(const PaneSize& s) {
    return WorkshopPaneSize{unit_word(s.mode), s.amount};
}

inline WorkshopSetup to_setup(const Setup& s) {
    WorkshopSetup out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.name = s.name;
    out.panes.reserve(s.panes.size());
    for (const SetupPane& row : s.panes) {
        // THE AUTHORED VALUES, AS AUTHORED. Not resolved, not clamped against the
        // current screen, not sorted by rank, not dropped for being unpresentable
        // on this medium, and not renumbered. A `pixels` width no medium in this
        // build can project is written exactly as the maker said it.
        out.panes.push_back(WorkshopSetupPane{row.ref.provider, row.ref.pane,
                                              place_out(row.place), size_out(row.width),
                                              size_out(row.height), row.front});
    }
    return out;
}

inline std::string to_text(const Setup& s) {
    return loom::compat::serialize(loom::to_value(to_setup(s)));
}

// ---- Reading -------------------------------------------------------------------

/// What reading produced: whether it worked, and the setup if it did.
///
/// THE SETUP IS RETURNED RATHER THAN WRITTEN THROUGH A REFERENCE, and that is
/// how "a malformed file never leaves Workshop halfway restored" is structural
/// rather than careful: there is no live value in scope here for a half-built
/// candidate to be written into. The caller reconciles only what it was handed,
/// and it is only handed a setup that passed every layer.
struct LoadedSetup {
    Written outcome;
    Setup setup;

    static LoadedSetup no(std::string why) {
        return LoadedSetup{Written::no(std::move(why)), {}};
    }
};

/// Text to a setup. Total: every input is either a setup or a refusal with a
/// reason, and nothing here throws.
///
/// FOUR LAYERS, IN ORDER, AND THE LAST ONE IS THE SETUP'S OWN LAW: the envelope
/// must parse; it must admit against this shape (which is where an unknown
/// field, a wrong kind, a bad integer or invalid UTF-8 is refused, by the same
/// gate the bus uses); it must say it is this format at this version; and the
/// value it describes must be a legal setup (`check_setup` -- the SAME function
/// the one-line name editor calls, so a typed name and a loaded one cannot come
/// to disagree about what is legal).
/// WHAT TO SAY ABOUT A SETUP VERSION THIS BUILD DOES NOT READ. One sentence, one
/// place, so the two doors that can meet a wrong version -- the envelope's claim
/// and the file's own `format_version` field -- cannot come to word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "setup version " + std::to_string(found) + " -- this Workshop reads versions " +
           std::to_string(kLegacyFormatVersion) + " and " + std::to_string(kFormatVersion);
}

/// The authored place a written one means. False for a mode this format has no
/// word for -- and `pixels` is deliberately one of those FOR A PLACE, because a
/// place has no pixel unit at all.
inline bool place_in(const WorkshopPanePlace& w, PanePlace& out) {
    if (w.mode == kUnitDefault) {
        out = PanePlace{pane_unit::kDefault, w.x, w.y};
        return true;
    }
    if (w.mode == kUnitSubcells) {
        out = PanePlace{pane_unit::kSubcells, w.x, w.y};
        return true;
    }
    return false;
}

/// The authored size a written one means. `pixels` IS a word here and is admitted
/// on every medium -- whether this build can PROJECT one is screen.hpp's question,
/// asked against a screen this function has never seen.
inline bool size_in(const WorkshopPaneSize& w, PaneSize& out) {
    if (w.mode == kUnitDefault) {
        out = PaneSize{pane_unit::kDefault, w.amount};
        return true;
    }
    if (w.mode == kUnitSubcells) {
        out = PaneSize{pane_unit::kSubcells, w.amount};
        return true;
    }
    if (w.mode == kUnitPixels) {
        out = PaneSize{pane_unit::kPixels, w.amount};
        return true;
    }
    return false;
}

/// What to say about a mode with no word. It names both what was found and what
/// would have worked, because a maker looking at their own file can fix that --
/// persist.hpp's `unknown_mode`, said about the other artifact.
inline std::string unknown_unit(const std::string& found, const char* which,
                                const char* allowed) {
    return "`" + found + "` is not a pane " + which + " mode (" + allowed + ")";
}

/// A WRITTEN SETUP, AS A LIVE ONE -- its format word, its version, its mode words and its
/// law, in that order.
///
/// SEPARATE FROM `from_text` SINCE WUX-0, and the separation is what makes "one durable
/// representation of a desk" true rather than aspirational: a setup arrives either as a
/// whole file (below) or as one field of a LARGER file that carries a desk beside something
/// that is not one (`session_persist.hpp`), and both must meet the same layers. A second
/// copy of these checks would be a second answer to "what is a legal saved setup".
///
/// IT WRITES THROUGH A REFERENCE AND STILL CANNOT HALF-RESTORE ANYTHING, because the
/// reference every caller passes is a LOCAL CANDIDATE of theirs and never a live setup --
/// the candidate is built here in full and assigned out only once every layer has passed.
// ---- VERSION 2, RETAINED FOR READING (WUX-2) -----------------------------------------
//
// The exact shapes WIND-2 wrote, byte for byte and version for version, kept so a
// whole-cell setup admits against the schema that actually described it — a legacy file
// meets its OWN gate, full strength, and only then is translated. The C++ names live in a
// nested namespace; the WIRE names are the same tokens they always were, which is the
// whole trick: `ZEN_SHAPE` stamps the bare struct name, so `v2::WorkshopSetup` claims
// `WorkshopSetup` v2 exactly as the old build did.
//
// TRANSLATION IS ONE EXACT MULTIPLY. `cells` amounts become `subcells` amounts times
// `surface::kCellSubs` — every whole-cell value lands on the sub-unit lattice's exact cell
// boundary, so a migrated desk resolves to the identical pixels and the identical
// characters it did before the lattice got finer (saturating, so a hostile huge value
// arrives at the setup law's walls rather than leaving the number line). `pixels` amounts
// are device pixels in both versions and cross unscaled; `default` carries nothing.
namespace v2 {

struct WorkshopPaneSize {
    std::string mode;
    std::int64_t amount = 0;

    ZEN_SHAPE(WorkshopPaneSize, 1, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

struct WorkshopPanePlace {
    std::string mode;
    std::int64_t x = 0;
    std::int64_t y = 0;

    ZEN_SHAPE(WorkshopPanePlace, 1, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y));
};

struct WorkshopSetupPane {
    std::string provider;
    std::string pane;
    WorkshopPanePlace place;
    WorkshopPaneSize width;
    WorkshopPaneSize height;
    std::int64_t front = 0;

    ZEN_SHAPE(WorkshopSetupPane, 2, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(place),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(front));
};

struct WorkshopSetup {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::vector<WorkshopSetupPane> panes;

    ZEN_SHAPE(WorkshopSetup, 2, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(panes));
};

/// Version 2's word for a whole-cell amount — alive only behind this reader.
inline constexpr const char* kUnitCells = "cells";
inline constexpr const char* kPlaceWords = "default or cells";
inline constexpr const char* kSizeWords = "default, cells or pixels";

inline bool place_in(const WorkshopPanePlace& w, PanePlace& out) {
    if (w.mode == kUnitDefault) {
        out = PanePlace{pane_unit::kDefault, w.x, w.y};
        return true;
    }
    if (w.mode == kUnitCells) {
        out = PanePlace{pane_unit::kSubcells, surface::subs_of_cells(w.x),
                        surface::subs_of_cells(w.y)};
        return true;
    }
    return false;
}

inline bool size_in(const WorkshopPaneSize& w, PaneSize& out) {
    if (w.mode == kUnitDefault) {
        out = PaneSize{pane_unit::kDefault, w.amount};
        return true;
    }
    if (w.mode == kUnitCells) {
        out = PaneSize{pane_unit::kSubcells, surface::subs_of_cells(w.amount)};
        return true;
    }
    if (w.mode == kUnitPixels) {
        out = PaneSize{pane_unit::kPixels, w.amount};
        return true;
    }
    return false;
}

} // namespace v2

/// A VERSION-2 SETUP AS A LIVE ONE — the same four layers `setup_in` below walks, against
/// version 2's own format claim and word vocabulary, landing on the fine lattice. The one
/// law both readers share unduplicated is the last and largest: `check_setup`, so a
/// migrated desk is legal by exactly the same sentence a native one is.
inline Written setup_in_v2(const v2::WorkshopSetup& file, Setup& out) {
    if (file.format != kFormat) {
        return Written::no("not a Workshop setup: it says it is `" + file.format + "`");
    }
    if (file.format_version != kLegacyFormatVersion) {
        return Written::no(wrong_version(file.format_version));
    }
    Setup candidate;
    candidate.name = file.name;
    candidate.panes.reserve(file.panes.size());
    for (const v2::WorkshopSetupPane& p : file.panes) {
        SetupPane row;
        row.ref = PaneRef{p.provider, p.pane};
        row.front = p.front;
        if (!v2::place_in(p.place, row.place)) {
            return Written::no(unknown_unit(p.place.mode, "place", v2::kPlaceWords));
        }
        if (!v2::size_in(p.width, row.width)) {
            return Written::no(unknown_unit(p.width.mode, "width", v2::kSizeWords));
        }
        if (!v2::size_in(p.height, row.height)) {
            return Written::no(unknown_unit(p.height.mode, "height", v2::kSizeWords));
        }
        candidate.panes.push_back(std::move(row));
    }
    const Written legal = check_setup(candidate);
    if (!legal.accepted) {
        return legal;
    }
    out = std::move(candidate);
    return Written::ok();
}

inline Written setup_in(const WorkshopSetup& file, Setup& out) {
    if (file.format != kFormat) {
        return Written::no("not a Workshop setup: it says it is `" + file.format + "`");
    }
    // AND THE FIELD IS STILL CHECKED. The preflight in `from_text` answers for a file whose
    // ENVELOPE is another version; this answers for one whose envelope is this version and
    // whose own stated version is not -- which only a forgery produces, and which is exactly
    // the forgery a reader of this format would try.
    if (file.format_version != kFormatVersion) {
        return Written::no(wrong_version(file.format_version));
    }
    Setup candidate;
    candidate.name = file.name;
    candidate.panes.reserve(file.panes.size());
    for (const WorkshopSetupPane& p : file.panes) {
        // COPIED, NEVER RESOLVED. Whether this build can present the pane is a
        // question for `resolve_pane`, asked later and by somebody who has
        // somewhere to put the answer; a reference that resolves to nothing is
        // still a reference this setup holds.
        SetupPane row;
        row.ref = PaneRef{p.provider, p.pane};
        row.front = p.front;
        // AN UNKNOWN WORD REFUSES THE WHOLE CANDIDATE and leaves whatever the caller is
        // showing exactly as it was -- see the note above about whose value `out` is.
        if (!place_in(p.place, row.place)) {
            return Written::no(unknown_unit(p.place.mode, "place", kPlaceWords));
        }
        if (!size_in(p.width, row.width)) {
            return Written::no(unknown_unit(p.width.mode, "width", kSizeWords));
        }
        if (!size_in(p.height, row.height)) {
            return Written::no(unknown_unit(p.height.mode, "height", kSizeWords));
        }
        candidate.panes.push_back(std::move(row));
    }
    const Written legal = check_setup(candidate);
    if (!legal.accepted) {
        return legal;
    }
    out = std::move(candidate);
    return Written::ok();
}

inline LoadedSetup from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopSetup>(), loom::Report::FirstError);
        return LoadedSetup::no("not a Workshop setup: " + refused.first_error().message());
    }
    // THE VERSION PREFLIGHT (WIND-2), and it is an ORDERING rather than a
    // loosening: the whole candidate still meets the full shape three lines down,
    // unknown fields are still refused, and nothing about ordinary admission was
    // made more permissive to buy this. What it does is answer the version question
    // FIRST, so a version-1 file -- whose rows carry no `place`, `width`, `height`
    // or `front` -- is refused by its number rather than by the first field version
    // 2 added, which is a true sentence about a false cause.
    //
    // IT READS THE CLAIM AND NOT A FIELD, because a claim is what exists before
    // admission. The static_assert over `WorkshopSetup::zen_version` is what makes
    // that number the setup format's version and not merely an envelope's.
    //
    // A VERSION-2 CLAIM TAKES THE LEGACY ROAD (WUX-2): admitted against version 2's
    // own retained shape — full strength, unknown fields refused by the gate that
    // actually described those bytes — and translated onto the fine lattice by one
    // exact multiply. Every other claimed version is refused by its number.
    if (claim.claimed_name() == std::string(WorkshopSetup::zen_name) &&
        claim.claimed_version() == v2::WorkshopSetup::zen_version) {
        const loom::Admission old =
            loom::admit(claim, loom::schema_of<v2::WorkshopSetup>(), loom::Report::FirstError);
        if (!old.ok()) {
            return LoadedSetup::no(old.first_error().message());
        }
        Setup candidate;
        const Written understood =
            setup_in_v2(loom::from_value<v2::WorkshopSetup>(old.value()), candidate);
        if (!understood.accepted) {
            return LoadedSetup::no(understood.refusal);
        }
        LoadedSetup loaded;
        loaded.outcome = Written::ok();
        loaded.setup = std::move(candidate);
        return loaded;
    }
    if (claim.claimed_name() == std::string(WorkshopSetup::zen_name) &&
        claim.claimed_version() != WorkshopSetup::zen_version) {
        return LoadedSetup::no(wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopSetup>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedSetup::no(admitted.first_error().message());
    }

    // THE CANDIDATE IS A LOCAL OF THIS FUNCTION, which is how "a malformed file never
    // leaves Workshop halfway restored" stays structural: `setup_in` fills this and nothing
    // else, and only a setup that passed every layer is ever returned.
    Setup candidate;
    const Written understood =
        setup_in(loom::from_value<WorkshopSetup>(admitted.value()), candidate);
    if (!understood.accepted) {
        return LoadedSetup::no(understood.refusal);
    }

    LoadedSetup loaded;
    loaded.outcome = Written::ok();
    loaded.setup = std::move(candidate);
    return loaded;
}

// ---- The file itself -------------------------------------------------------------

/// Save a setup to a file, through the document's own safe write: a complete
/// candidate to a sibling, then a rename over the destination.
///
/// THE PROMISE IS THE ONE `persist::write_file` MAKES and it is not restated
/// here as though it were a second mechanism: an ordinary detected write failure
/// -- no space, no permission, an unwritable path -- does not destroy the
/// previously valid setup file, because nothing touches the destination until a
/// complete file exists beside it. Crash durability is not claimed here either.
inline Written save_file(const std::string& path, const Setup& s) {
    return persist::write_file(path, to_text(s));
}

/// Read a setup from a file. The composition of every layer: the file, the
/// format, and the setup law.
inline LoadedSetup load_file(const std::string& path) {
    const persist::FileText read = persist::read_file(path, kMaxSetupBytes, "a Workshop setup");
    if (!read.outcome.accepted) {
        return LoadedSetup{read.outcome, {}};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::setup_persist

#endif // ZENGINE_WORKSHOP_SETUP_PERSIST_HPP
