// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_PERSIST_HPP
#define ZENGINE_WORKSHOP_SETUP_PERSIST_HPP

// The setup's own file: a weaver's desk as a standalone artifact.
// Workshop law: agents/workshop/setup-file.md (+2 registers; agents/workshop.md routes)

#include "pane_migration.hpp"
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

/// What a Workshop setup file says it is: its own word, so handing Workshop the wrong file is
/// named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-setup";

/// The setup format version this build WRITES, and the newest it reads.
inline constexpr std::int64_t kFormatVersion = 4;

/// The oldest version this build still reads. Version 2 (whole cells) and version 3 (sub-units)
/// are each translated on load — never rewritten in place.
inline constexpr std::int64_t kOldestFormatVersion = 2;

/// A setup is a smaller thing than a document, and its ceiling says so.
// WL-SESSION-06 -- agents/workshop/session-restore.md
inline constexpr std::uintmax_t kMaxSetupBytes = 1u << 16;

// ---- The mode WORDS, and why they are words ----------------------------------
// WL-SETUP-04 -- agents/workshop/setup-file.md

inline constexpr const char* kUnitDefault = "default";
/// THE GEOMETRY WORD: amounts in canvas pixels, twelve to a canvas cell.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr const char* kUnitPixels = "pixels";
/// The place-only word: the right column, named. Adding it is not a format version: the shape
/// is unchanged, and an older build refuses the word by name.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr const char* kUnitRightColumn = "right-column";

/// The words a PLACE may be said in, and the words a SIZE may be said in -- two
/// lists, because they are two different closed sets.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr const char* kPlaceWords = "default, right-column or pixels";
inline constexpr const char* kSizeWords = "default or pixels";

/// An extent an older file said in pixels, raised to one cell where it was less: a pane is
/// never narrower than a cell. A count that is not positive is left for the law to refuse.
inline constexpr std::int64_t at_least_a_cell(std::int64_t px) noexcept {
    return px > 0 && px < kPanePxMin ? kPanePxMin : px;
}

// ---- The file's own shapes ---------------------------------------------------

/// ONE AXIS OF AN AUTHORED SIZE AS WRITTEN: a mode a person can read, and an
/// amount.
// WL-SETUP-01 -- agents/workshop/setup-file.md
struct WorkshopPaneSize {
    std::string mode;
    std::int64_t amount = 0;

    /// Version 3: the amount is canvas pixels, word `pixels`. Same fields — the version IS the
    /// semantic gate.
    ZEN_SHAPE(WorkshopPaneSize, 3, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// AN AUTHORED PLACE AS WRITTEN. One mode for the pair, for `PanePlace`'s reason.
struct WorkshopPanePlace {
    std::string mode;
    std::int64_t x = 0;
    std::int64_t y = 0;

    /// Version 3: coordinates in canvas pixels, word `pixels`.
    ZEN_SHAPE(WorkshopPanePlace, 3, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y));
};

/// ONE PANE ROW AS WRITTEN: the durable reference, the authored window, and how
/// far forward it sits.
// WL-SETUP-01 -- agents/workshop/setup-file.md
struct WorkshopSetupPane {
    std::string provider;
    std::string pane;
    WorkshopPanePlace place;
    WorkshopPaneSize width;
    WorkshopPaneSize height;
    std::int64_t front = 0;

    ZEN_SHAPE(WorkshopSetupPane, 4, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(place),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(front));
};

/// A WHOLE SAVED SETUP: what it is, which version of that it is, what a weaver
/// calls it, and the panes it means to have open IN AUTHORED ORDER.
struct WorkshopSetup {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::vector<WorkshopSetupPane> panes;

    /// Version 4, because the geometry its rows hold became canvas pixels.
    ZEN_SHAPE(WorkshopSetup, 4, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(panes));
};

/// THE ENVELOPE'S SHAPE VERSION AND THE SETUP FORMAT VERSION ARE ONE NUMBER.
static_assert(WorkshopSetup::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the setup file's format version and its envelope's shape version are one "
              "number: a version-1 file must be refused by ITS NUMBER, before its rows are "
              "judged against a later version's shape");

// ---- Writing -------------------------------------------------------------------

/// The word for an authored unit, total over the integer. One function for both lists: a size
/// never holds `kRightColumn` and a place never holds `kPixels`, and the closed sets are enforced
/// where a value is judged.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline const char* unit_word(std::int64_t mode) {
    if (mode == pane_unit::kPixels) {
        return kUnitPixels;
    }
    if (mode == pane_unit::kRightColumn) {
        return kUnitRightColumn;
    }
    return kUnitDefault;
}

inline WorkshopPanePlace place_out(const PanePlace& p) {
    return WorkshopPanePlace{unit_word(p.mode), p.x, p.y};
}

inline WorkshopPaneSize size_out(const PaneSize& s) {
    return WorkshopPaneSize{unit_word(s.mode), s.amount};
}

/// The setup, as the value that gets written. NOTHING IS SORTED, NORMALISED, RESOLVED OR
/// DROPPED ON THE WAY OUT: the order is the setup's, the keys are byte-for-byte what came
/// in, and a reference this build cannot resolve is written exactly as it was read.
inline WorkshopSetup to_setup(const Setup& s) {
    WorkshopSetup out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.name = s.name;
    out.panes.reserve(s.panes.size());
    for (const SetupPane& row : s.panes) {
        // THE AUTHORED VALUES, AS AUTHORED. Not resolved, not clamped against the
        // current screen, not sorted by rank, not dropped for being off this screen,
        // and not renumbered.
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

/// What reading produced: whether it worked, the setup if it did, and how many references named
/// a pane that has since changed hands (`pane_migration.hpp`), counted so the note is said once.
struct LoadedSetup {
    Written outcome;
    Setup setup;
    pane_migration::Converted converted;

    static LoadedSetup no(std::string why) {
        return LoadedSetup{Written::no(std::move(why)), {}, {}};
    }
};

/// WHAT TO SAY ABOUT A SETUP VERSION THIS BUILD DOES NOT READ. One sentence, one place, so
/// the two doors that can meet a wrong version -- the envelope's claim and the file's own
/// `format_version` field -- cannot come to word it differently.
// WL-SETUP-05 -- agents/workshop/setup-file.md
inline std::string wrong_version(std::int64_t found) {
    std::string read;
    for (std::int64_t v = kOldestFormatVersion; v < kFormatVersion; ++v) {
        read += std::to_string(v) + (v + 1 < kFormatVersion ? ", " : " and ");
    }
    return "setup version " + std::to_string(found) + " -- this Workshop reads versions " + read +
           std::to_string(kFormatVersion);
}

/// The authored place a written one means. False for a mode this format has no word for.
inline bool place_in(const WorkshopPanePlace& w, PanePlace& out) {
    if (w.mode == kUnitDefault) {
        out = PanePlace{pane_unit::kDefault, w.x, w.y};
        return true;
    }
    if (w.mode == kUnitPixels) {
        out = PanePlace{pane_unit::kPixels, w.x, w.y};
        return true;
    }
    // THE COORDINATES COME THROUGH UNTOUCHED, and `check_pane_place` refuses them if they are
    // not zero. A codec that silently zeroed them would turn a weaver's contradictory row into
    // a valid one behind their back; the admission's job is to tell them they wrote two
    // things.
    if (w.mode == kUnitRightColumn) {
        out = PanePlace{pane_unit::kRightColumn, w.x, w.y};
        return true;
    }
    return false;
}

/// The authored size a written one means.
inline bool size_in(const WorkshopPaneSize& w, PaneSize& out) {
    if (w.mode == kUnitDefault) {
        out = PaneSize{pane_unit::kDefault, w.amount};
        return true;
    }
    if (w.mode == kUnitPixels) {
        out = PaneSize{pane_unit::kPixels, w.amount};
        return true;
    }
    return false;
}

/// What to say about a mode with no word: what was found and what would have worked, since a
/// weaver looking at their own file can fix that.
inline std::string unknown_unit(const std::string& found, const char* which,
                                const char* allowed) {
    return "`" + found + "` is not a pane " + which + " mode (" + allowed + ")";
}

// ---- VERSION 2, RETAINED FOR READING -----------------------------------------------------
// WL-SETUP-02 -- agents/workshop/setup-file.md
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

inline constexpr std::int64_t kRetainedVersion = 2;
static_assert(kRetainedVersion == kOldestFormatVersion, "version 2 is the oldest this build reads");

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
        out = PanePlace{pane_unit::kPixels, surface::px_of_cells(w.x),
                        surface::px_of_cells(w.y)};
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
        out = PaneSize{pane_unit::kPixels, surface::px_of_cells(w.amount)};
        return true;
    }
    if (w.mode == kUnitPixels) {
        out = PaneSize{pane_unit::kPixels, at_least_a_cell(w.amount)};
        return true;
    }
    return false;
}

} // namespace v2

// ---- VERSION 3, RETAINED FOR READING -----------------------------------------------------
// WL-SETUP-02 -- agents/workshop/setup-file.md
namespace v3 {

inline constexpr std::int64_t kRetainedVersion = 3;

struct WorkshopPaneSize {
    std::string mode;
    std::int64_t amount = 0;

    ZEN_SHAPE(WorkshopPaneSize, 2, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

struct WorkshopPanePlace {
    std::string mode;
    std::int64_t x = 0;
    std::int64_t y = 0;

    ZEN_SHAPE(WorkshopPanePlace, 2, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y));
};

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

struct WorkshopSetup {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::vector<WorkshopSetupPane> panes;

    ZEN_SHAPE(WorkshopSetup, 3, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(panes));
};

static_assert(WorkshopSetup::zen_version == static_cast<std::uint32_t>(kRetainedVersion),
              "a retained setup shape's envelope version and its format version are one "
              "number, as the current one's are");

/// Version 3's word for a geometry amount — sub-units, 48 to a canvas cell and
/// `kSubsPerPixel` to a canvas pixel; alive only behind this reader.
inline constexpr const char* kUnitSubcells = "subcells";
inline constexpr const char* kPlaceWords = "default, right-column or subcells";
inline constexpr const char* kSizeWords = "default, subcells or pixels";
inline constexpr std::int64_t kSubsPerPixel = 4;

/// A `subcells` coordinate as the pixel the window painted it on: floored.
inline constexpr std::int64_t px_of_subcells(std::int64_t subs) noexcept {
    return surface::floor_div_px(subs, kSubsPerPixel);
}

/// A `subcells` extent from `at` as the pixels the window painted: its far edge floored, less
/// its near edge floored.
inline constexpr std::int64_t px_extent_of_subcells(std::int64_t at, std::int64_t extent) noexcept {
    return px_of_subcells(surface::add_cells(at, extent)) - px_of_subcells(at);
}

/// ONE DESK OF VERSION 3 IN THE CURRENT SHAPE, LANDED ON THE PIXELS THE WINDOW PAINTED IT AT. A
/// place floors; an extent is its painted span from the authored place on its axis, or from a
/// whole cell when the place is the code's (every default place is one); a `pixels` extent is a
/// pixel count already, raised to one cell if it was less. Refused, in version 3's own words,
/// for a claim or a word that version never had.
// WL-SETUP-02 -- agents/workshop/setup-file.md
inline Written to_current(const WorkshopSetup& old, setup_persist::WorkshopSetup& out) {
    if (old.format != kFormat) {
        return Written::no("not a Workshop setup: it says it is `" + old.format + "`");
    }
    if (old.format_version != kRetainedVersion) {
        return Written::no(wrong_version(old.format_version));
    }
    setup_persist::WorkshopSetup made;
    made.format = old.format;
    made.format_version = setup_persist::kFormatVersion;
    made.name = old.name;
    made.panes.reserve(old.panes.size());
    for (const WorkshopSetupPane& p : old.panes) {
        setup_persist::WorkshopSetupPane row;
        row.provider = p.provider;
        row.pane = p.pane;
        row.front = p.front;
        const bool placed = p.place.mode == kUnitSubcells;
        if (p.place.mode == kUnitDefault || p.place.mode == kUnitRightColumn) {
            row.place = setup_persist::WorkshopPanePlace{p.place.mode, p.place.x, p.place.y};
        } else if (placed) {
            row.place = setup_persist::WorkshopPanePlace{
                kUnitPixels, px_of_subcells(p.place.x), px_of_subcells(p.place.y)};
        } else {
            return Written::no(unknown_unit(p.place.mode, "place", kPlaceWords));
        }
        const auto axis = [](const WorkshopPaneSize& w, std::int64_t at,
                             setup_persist::WorkshopPaneSize& to) {
            if (w.mode == kUnitDefault) {
                to = setup_persist::WorkshopPaneSize{kUnitDefault, w.amount};
            } else if (w.mode == kUnitSubcells) {
                to = setup_persist::WorkshopPaneSize{kUnitPixels,
                                                     px_extent_of_subcells(at, w.amount)};
            } else if (w.mode == kUnitPixels) {
                to = setup_persist::WorkshopPaneSize{kUnitPixels, at_least_a_cell(w.amount)};
            } else {
                return false;
            }
            return true;
        };
        if (!axis(p.width, placed ? p.place.x : 0, row.width)) {
            return Written::no(unknown_unit(p.width.mode, "width", kSizeWords));
        }
        if (!axis(p.height, placed ? p.place.y : 0, row.height)) {
            return Written::no(unknown_unit(p.height.mode, "height", kSizeWords));
        }
        made.panes.push_back(std::move(row));
    }
    out = std::move(made);
    return Written::ok();
}

} // namespace v3

/// A version-2 SETUP AS A LIVE ONE — the same four layers `setup_in` below walks, against
/// version 2's own format claim and word vocabulary, landing on pixels.
// WL-SETUP-02 -- agents/workshop/setup-file.md
inline Written setup_in_v2(const v2::WorkshopSetup& file, Setup& out,
                           pane_migration::Converted* converted = nullptr) {
    if (file.format != kFormat) {
        return Written::no("not a Workshop setup: it says it is `" + file.format + "`");
    }
    if (file.format_version != v2::kRetainedVersion) {
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
    // A REFERENCE THAT CHANGED HANDS, REWRITTEN -- the same step the current road takes, on
    // the same candidate, before the same law. A version-2 setup can name the built-in
    // browser as easily as a version-3 one, and it is older, so if either road were to skip
    // this it would be the wrong one.
    const pane_migration::Converted moved = pane_migration::convert_retired_panes(candidate);
    if (converted != nullptr) {
        for (std::size_t i = 0; i < pane_migration::kRetiredCount; ++i) {
            converted->rows[i] += moved.rows[i];
        }
    }
    const Written legal = check_setup(candidate);
    if (!legal.accepted) {
        return legal;
    }
    out = std::move(candidate);
    return Written::ok();
}

/// A WRITTEN SETUP, AS A LIVE ONE -- its format word, its version, its mode words and its
/// law, in that order. Separate from `from_text` so a setup arriving as one field of a
/// larger file meets the same layers; it writes through a reference and cannot half-restore.
// WL-SETUP-02 -- agents/workshop/setup-file.md
// WL-SESSION-04 -- agents/workshop/session.md; WL-SESSION-06 -- agents/workshop/session-restore.md
inline Written setup_in(const WorkshopSetup& file, Setup& out,
                        pane_migration::Converted* converted = nullptr) {
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
    // A pane that changed hands is rewritten here, in the one function that turns written rows
    // into a live `Setup` (the setup file, every desk in a session, every remembered link). The
    // converted candidate then meets the whole law, so a file naming both spellings is refused.
    const pane_migration::Converted moved = pane_migration::convert_retired_panes(candidate);
    if (converted != nullptr) {
        for (std::size_t i = 0; i < pane_migration::kRetiredCount; ++i) {
            converted->rows[i] += moved.rows[i];
        }
    }
    const Written legal = check_setup(candidate);
    if (!legal.accepted) {
        return legal;
    }
    out = std::move(candidate);
    return Written::ok();
}

/// Text to a setup. Total: every input is either a setup or a refusal with a reason, and
/// nothing here throws. Four layers, in order -- the envelope, the shape, the format and
/// its version, then the setup's own law -- and the last is the one the name editor calls.
// WL-SETUP-02, WL-SETUP-04, WL-SETUP-05 -- agents/workshop/setup-file.md
inline LoadedSetup from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopSetup>(), loom::Report::FirstError);
        return LoadedSetup::no("not a Workshop setup: " + refused.first_error().message());
    }
    // The version preflight orders, never loosens: it reads the claim, which exists before
    // admission, so a version-1 file is refused by its number rather than by the first field
    // version 2 added. A version-2 or version-3 claim takes its own road: admitted at full
    // strength against that version's retained shape, then landed on pixels.
    if (claim.claimed_name() == std::string(WorkshopSetup::zen_name) &&
        claim.claimed_version() == v2::WorkshopSetup::zen_version) {
        const loom::Admission old =
            loom::admit(claim, loom::schema_of<v2::WorkshopSetup>(), loom::Report::FirstError);
        if (!old.ok()) {
            return LoadedSetup::no(old.first_error().message());
        }
        Setup candidate;
        pane_migration::Converted converted;
        const Written understood = setup_in_v2(
            loom::from_value<v2::WorkshopSetup>(old.value()), candidate, &converted);
        if (!understood.accepted) {
            return LoadedSetup::no(understood.refusal);
        }
        LoadedSetup loaded;
        loaded.outcome = Written::ok();
        loaded.setup = std::move(candidate);
        loaded.converted = converted;
        return loaded;
    }
    if (claim.claimed_name() == std::string(WorkshopSetup::zen_name) &&
        claim.claimed_version() == v3::WorkshopSetup::zen_version) {
        const loom::Admission old =
            loom::admit(claim, loom::schema_of<v3::WorkshopSetup>(), loom::Report::FirstError);
        if (!old.ok()) {
            return LoadedSetup::no(old.first_error().message());
        }
        WorkshopSetup current;
        const Written landed =
            v3::to_current(loom::from_value<v3::WorkshopSetup>(old.value()), current);
        if (!landed.accepted) {
            return LoadedSetup::no(landed.refusal);
        }
        Setup candidate;
        pane_migration::Converted converted;
        const Written understood = setup_in(current, candidate, &converted);
        if (!understood.accepted) {
            return LoadedSetup::no(understood.refusal);
        }
        LoadedSetup loaded;
        loaded.outcome = Written::ok();
        loaded.setup = std::move(candidate);
        loaded.converted = converted;
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
    pane_migration::Converted converted;
    const Written understood =
        setup_in(loom::from_value<WorkshopSetup>(admitted.value()), candidate, &converted);
    if (!understood.accepted) {
        return LoadedSetup::no(understood.refusal);
    }

    LoadedSetup loaded;
    loaded.outcome = Written::ok();
    loaded.setup = std::move(candidate);
    loaded.converted = converted;
    return loaded;
}

// ---- The file itself -------------------------------------------------------------

/// Save a setup through `persist`'s safe write: a complete candidate to a sibling, then a rename
/// over the destination.
// WL-SETUP-11 -- agents/workshop/setup-file.md
inline Written save_file(const std::string& path, const Setup& s) {
    return persist::write_file(path, to_text(s));
}

/// Read a setup from a file. The composition of every layer: the file, the
/// format, and the setup law.
// WL-SETUP-11 -- agents/workshop/setup-file.md
inline LoadedSetup load_file(const std::string& path) {
    const persist::FileText read = persist::read_file(path, kMaxSetupBytes, "a Workshop setup");
    if (!read.outcome.accepted) {
        return LoadedSetup{read.outcome, {}, {}};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::setup_persist

#endif // ZENGINE_WORKSHOP_SETUP_PERSIST_HPP
