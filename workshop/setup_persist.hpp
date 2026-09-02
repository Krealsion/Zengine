// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_PERSIST_HPP
#define ZENGINE_WORKSHOP_SETUP_PERSIST_HPP

// THE SETUP'S OWN FILE -- a second artifact, separate from the document's, and
// separate on purpose.
// Workshop law: agents/workshop/setup-file.md (+2 registers; agents/workshop.md routes)

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

/// The one older version this build still reads: the
/// whole-cell format, translated on load — never rewritten in place.
inline constexpr std::int64_t kLegacyFormatVersion = 2;

/// A setup is a smaller thing than a document, and its ceiling says so.
// WL-SESSION-06 -- agents/workshop/session.md
inline constexpr std::uintmax_t kMaxSetupBytes = 1u << 16;

// ---- The mode WORDS, and why they are words ----------------------------------
// WL-SETUP-04 -- agents/workshop/setup-file.md

inline constexpr const char* kUnitDefault = "default";
/// VERSION 3'S GEOMETRY WORD: amounts in 1/surface::kCellSubs of a canvas cell.
// WL-SETUP-02, WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr const char* kUnitSubcells = "subcells";
inline constexpr const char* kUnitPixels = "pixels";

/// The words a PLACE may be said in, and the words a SIZE may be said in -- two
/// lists, because they are two different closed sets.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr const char* kPlaceWords = "default or subcells";
inline constexpr const char* kSizeWords = "default, subcells or pixels";

// ---- The file's own shapes ---------------------------------------------------

/// ONE AXIS OF AN AUTHORED SIZE AS WRITTEN: a mode a person can read, and an
/// amount.
// WL-SETUP-01 -- agents/workshop/setup-file.md
struct WorkshopPaneSize {
    std::string mode;
    std::int64_t amount = 0;

    /// Version 2: the amount's geometry unit became sub-cells and the mode
    /// word moved with it. Same fields — the version IS the semantic gate.
    ZEN_SHAPE(WorkshopPaneSize, 2, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// AN AUTHORED PLACE AS WRITTEN. One mode for the pair, for `PanePlace`'s reason.
struct WorkshopPanePlace {
    std::string mode;
    std::int64_t x = 0;
    std::int64_t y = 0;

    /// Version 2: coordinates in sub-cells, word `subcells`.
    ZEN_SHAPE(WorkshopPanePlace, 2, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y));
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

    /// Version 2, because the rows it holds grew four fields; version 3,
    /// because their geometry became sub-cell units.
    ZEN_SHAPE(WorkshopSetup, 3, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(panes));
};

/// THE ENVELOPE'S SHAPE VERSION AND THE SETUP FORMAT VERSION ARE ONE NUMBER.
// WL-SETUP-05 -- agents/workshop/setup-file.md
static_assert(WorkshopSetup::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the setup file's format version and its envelope's shape version are one "
              "number: a version-1 file must be refused by ITS NUMBER, before its rows are "
              "judged against version 2's shape");

// ---- Writing -------------------------------------------------------------------

/// The word for an authored unit. TOTAL over the integer.
// WL-SETUP-01, WL-SETUP-04 -- agents/workshop/setup-file.md
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
// WL-SETUP-05 -- agents/workshop/setup-file.md
struct LoadedSetup {
    Written outcome;
    Setup setup;

    static LoadedSetup no(std::string why) {
        return LoadedSetup{Written::no(std::move(why)), {}};
    }
};

/// to disagree about what is legal).
/// WHAT TO SAY ABOUT A SETUP VERSION THIS BUILD DOES NOT READ. One sentence, one
/// place, so the two doors that can meet a wrong version -- the envelope's claim
// WL-SETUP-05 -- agents/workshop/setup-file.md
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

// ---- VERSION 2, RETAINED FOR READING -----------------------------------------------------
// WL-SESSION-04 -- agents/workshop/session.md; WL-SETUP-02 -- agents/workshop/setup-file.md
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
/// version 2's own format claim and word vocabulary, landing on the fine lattice.
// WL-SETUP-02 -- agents/workshop/setup-file.md
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
    // THE VERSION PREFLIGHT, and it is an ORDERING rather than a
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
    // A VERSION-2 CLAIM TAKES THE LEGACY ROAD: admitted against version 2's
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
// WL-DOC-15 -- agents/workshop/document.md
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
