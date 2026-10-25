// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SETUP_PERSIST_HPP
#define ZENGINE_WORKSHOP_SETUP_PERSIST_HPP

// The setup's own file: a weaver's desk as a standalone artifact, with what the layout keeps for
// itself -- its baseline, its locks and its pins.
// Workshop law: agents/workshop/setup-file.md (+2 registers; agents/workshop.md routes)

#include "pane_migration.hpp"
#include "persist.hpp"
#include "setup.hpp"

#include "graphics/pixels.hpp" // a retired file's sub-units landed on pixels

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop::setup_persist {

/// What a Workshop setup file says it is: its own word, so handing Workshop the wrong file is
/// named rather than half-read.
inline constexpr const char* kFormat = "zengine-workshop-setup";

/// The setup format version this build WRITES, and the newest it reads.
inline constexpr std::int64_t kFormatVersion = 6;

/// The oldest version this build still reads. Version 2 (whole cells), version 3 (sub-units),
/// version 4 (no settings) and version 5 (a coordinate written where its word reads none) are each
/// translated on load -- never rewritten in place.
inline constexpr std::int64_t kOldestFormatVersion = 2;

/// THE MOST BYTES ONE STORED SETTING SPELLS TO in the file: its key, its text with every byte
/// escaped to two, and the field names and punctuation around them.
// WL-SESSION-06 -- agents/workshop/session-restore.md
inline constexpr std::uintmax_t kMaxPaneSettingSpelledBytes =
    kMaxPaneSettingKeyLen + 2u * kMaxPaneSettingTextLen + 32u;

/// ...AND ONE LOCK: both halves of its row's reference, each byte escaped to two, its key, and the
/// field names and punctuation around them. A pin list spends less a unit: its reference once,
/// and each pinned key a unit of its own.
// WL-SESSION-06 -- agents/workshop/session-restore.md
inline constexpr std::uintmax_t kMaxSetupLockSpelledBytes =
    2u * 2u * kMaxPaneKeyLen + kMaxPaneSettingKeyLen + 48u;

/// The most bytes any one unit a desk keeps spells to: a setting, a lock, a pin list or a key.
inline constexpr std::uintmax_t kMaxSetupUnitSpelledBytes =
    std::max(kMaxPaneSettingSpelledBytes, kMaxSetupLockSpelledBytes);

/// A setup is a smaller thing than a document, and its ceiling says so: room for a desk of
/// `kMaxSetupPanes` rows at their bounds, and for `kMaxPaneSettingsPerDesk` units at the most any
/// unit spells to.
// WL-SESSION-06 -- agents/workshop/session-restore.md
inline constexpr std::uintmax_t kMaxSetupBytes = 256u << 10;
static_assert(kMaxSetupBytes >= (1u << 16) + kMaxPaneSettingsPerDesk * kMaxSetupUnitSpelledBytes,
              "a setup's ceiling holds its rows' 64 KiB and every unit a desk keeps, each at the "
              "most bytes its largest kind -- a lock -- can spell to: a lower ceiling lets `s` "
              "write what `r` refuses");

// ---- The mode WORDS, and why they are words ----------------------------------
// WL-SETUP-04 -- agents/workshop/setup-file.md

inline constexpr const char* kUnitDefault = "default";
/// THE GEOMETRY WORD: amounts in canvas pixels, twelve to a canvas cell.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr const char* kUnitPixels = "pixels";
/// The place-only word: the right column, named.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr const char* kUnitRightColumn = "right-column";

/// The words a PLACE may be said in, and the words a SIZE may be said in -- two
/// lists, because they are two different closed sets.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline constexpr const char* kPlaceWords = "default, right-column or pixels";
inline constexpr const char* kSizeWords = "default or pixels";

/// Where the room began on the canvas when a place was measured from the canvas -- a version 2 or
/// 3 setup, and every session before version 7: two cells down.
inline constexpr std::int64_t kCanvasRoomTopPx = 2 * kSavedCellPx;

/// A whole-cell amount an older file wrote, in pixels: `kSavedCellPx` to the cell, saturating
/// at both ends.
inline constexpr std::int64_t px_of_saved_cells(std::int64_t cells) noexcept {
    constexpr std::int64_t bound = (std::numeric_limits<std::int64_t>::max)() / kSavedCellPx;
    if (cells >= bound) {
        return bound * kSavedCellPx;
    }
    if (cells <= -bound) {
        return -bound * kSavedCellPx;
    }
    return cells * kSavedCellPx;
}

/// A place such a file measured from the canvas, as a place in the room: two cells less, and one
/// above that room's top at the room's top, the nearest place the room has.
// WL-SETUP-02 -- agents/workshop/setup-file.md
inline constexpr std::int64_t room_place_of_canvas(std::int64_t px) noexcept {
    return px > kCanvasRoomTopPx ? px - kCanvasRoomTopPx : 0;
}

/// An extent an older file said in pixels, raised to one cell where it was less: a pane is
/// never narrower than a cell. A count that is not positive is left for the law to refuse.
inline constexpr std::int64_t at_least_a_cell(std::int64_t px) noexcept {
    return px > 0 && px < kPanePxMin ? kPanePxMin : px;
}

// ---- The file's own shapes ---------------------------------------------------

/// ONE AXIS OF AN AUTHORED SIZE AS WRITTEN: a word, and the amount where the word reads one.
// WL-SETUP-01, WL-SETUP-04 -- agents/workshop/setup-file.md
struct WorkshopPaneSize {
    std::string mode;
    std::optional<std::int64_t> amount = std::nullopt;

    /// Version 4: the amount is present exactly where the word reads it.
    ZEN_SHAPE(WorkshopPaneSize, 4, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// AN AUTHORED PLACE AS WRITTEN. One word for the pair, for `PanePlace`'s reason, and the
/// coordinates where the word reads them.
// WL-SETUP-01, WL-SETUP-04 -- agents/workshop/setup-file.md
struct WorkshopPanePlace {
    std::string mode;
    std::optional<std::int64_t> x = std::nullopt;
    std::optional<std::int64_t> y = std::nullopt;

    /// Version 4: the coordinates are present exactly where the word reads them.
    ZEN_SHAPE(WorkshopPanePlace, 4, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y));
};

/// ONE PANE ROW AS WRITTEN: the durable reference, the authored window, how far forward it
/// sits, and the settings this layout keeps for the pane -- `[]`, the one spelling of none.
// WL-SETUP-01 -- agents/workshop/setup-file.md
struct WorkshopSetupPane {
    std::string provider;
    std::string pane;
    WorkshopPanePlace place;
    WorkshopPaneSize width;
    WorkshopPaneSize height;
    std::int64_t front = 0;
    std::vector<PaneSetting> settings;

    ZEN_SHAPE(WorkshopSetupPane, 6, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(place),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(front), ZEN_FIELD(settings));
};

/// ONE LOCK THIS LAYOUT KEEPS: a key of one row frozen; it holds no value.
// WL-LOCK-01 -- agents/workshop/setting-locks.md
struct WorkshopSetupLock {
    std::string provider;
    std::string pane;
    std::string key;

    ZEN_SHAPE(WorkshopSetupLock, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(key));
};

/// ONE PANE'S PIN LIST IN THIS LAYOUT, in pin order.
// WL-SETTING-12 -- agents/workshop/settings.md
struct WorkshopSetupPinList {
    std::string provider;
    std::string pane;
    std::vector<std::string> keys;

    ZEN_SHAPE(WorkshopSetupPinList, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(keys));
};

/// A WHOLE SAVED SETUP: what it is, which version of that it is, what a weaver calls it, the panes
/// it means to have open IN AUTHORED ORDER, and what the layout keeps for itself.
struct WorkshopSetup {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::vector<WorkshopSetupPane> panes;
    LayoutTheme theme;
    std::vector<WorkshopSetupLock> locks;
    std::vector<WorkshopSetupPinList> pins;

    /// Version 6: a coordinate or an amount present exactly where its word reads it, and the
    /// layout's baseline, locks and pins beside its rows.
    ZEN_SHAPE(WorkshopSetup, 6, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(panes), ZEN_FIELD(theme), ZEN_FIELD(locks), ZEN_FIELD(pins));
};

/// THE ENVELOPE'S SHAPE VERSION AND THE SETUP FORMAT VERSION ARE ONE NUMBER.
static_assert(WorkshopSetup::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the setup file's format version and its envelope's shape version are one "
              "number: a version-1 file must be refused by ITS NUMBER, before its rows are "
              "judged against a later version's shape");

// ---- Writing -------------------------------------------------------------------

/// The word for an authored unit, total over the integer.
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

/// A place as written: its coordinates exactly where its word reads them, a pixel place's.
inline WorkshopPanePlace place_out(const PanePlace& p) {
    WorkshopPanePlace out{unit_word(p.mode), std::nullopt, std::nullopt};
    if (p.mode == pane_unit::kPixels) {
        out.x = p.x;
        out.y = p.y;
    }
    return out;
}

/// A size as written: its amount exactly where its word reads one, a pixel size's.
inline WorkshopPaneSize size_out(const PaneSize& s) {
    WorkshopPaneSize out{unit_word(s.mode), std::nullopt};
    if (s.mode == pane_unit::kPixels) {
        out.amount = s.amount;
    }
    return out;
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
        out.panes.push_back(WorkshopSetupPane{row.ref.provider, row.ref.pane,
                                              place_out(row.place), size_out(row.width),
                                              size_out(row.height), row.front, row.settings});
    }
    out.theme = s.theme;
    for (const SetupLock& lock : s.locks) {
        out.locks.push_back(WorkshopSetupLock{lock.ref.provider, lock.ref.pane, lock.key});
    }
    for (const SetupPins& list : s.pins) {
        out.pins.push_back(WorkshopSetupPinList{list.ref.provider, list.ref.pane, list.keys});
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

/// WHAT TO SAY ABOUT A SETUP VERSION THIS BUILD DOES NOT READ.
// WL-SETUP-05 -- agents/workshop/setup-file.md
inline std::string wrong_version(std::int64_t found) {
    std::string read;
    for (std::int64_t v = kOldestFormatVersion; v < kFormatVersion; ++v) {
        read += std::to_string(v) + (v + 1 < kFormatVersion ? ", " : " and ");
    }
    return "setup version " + std::to_string(found) + " -- this Workshop reads versions " + read +
           std::to_string(kFormatVersion);
}

/// What to say about a mode with no word: what was found and what would have worked.
inline std::string unknown_unit(const std::string& found, const char* which,
                                const char* allowed) {
    return "`" + found + "` is not a pane " + which + " mode (" + allowed + ")";
}

/// THE AUTHORED PLACE A WRITTEN ONE MEANS, or why not: a word with no meaning, a coordinate where
/// the word reads none, or one missing where it reads both.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline Written place_in(const WorkshopPanePlace& w, PanePlace& out) {
    const bool any = w.x.has_value() || w.y.has_value();
    if (w.mode == kUnitDefault || w.mode == kUnitRightColumn) {
        if (any) {
            return Written::no(std::string("a ") +
                               (w.mode == kUnitDefault ? "default" : "named") +
                               " pane place carries no coordinates");
        }
        out = PanePlace{w.mode == kUnitDefault ? pane_unit::kDefault : pane_unit::kRightColumn, 0,
                        0};
        return Written::ok();
    }
    if (w.mode == kUnitPixels) {
        if (!w.x.has_value() || !w.y.has_value()) {
            return Written::no("a pixels pane place names both x and y");
        }
        out = PanePlace{pane_unit::kPixels, *w.x, *w.y};
        return Written::ok();
    }
    return Written::no(unknown_unit(w.mode, "place", kPlaceWords));
}

/// The authored size a written one means, or why not, by the same rule.
// WL-SETUP-04 -- agents/workshop/setup-file.md
inline Written size_in(const WorkshopPaneSize& w, PaneSize& out, const char* which) {
    if (w.mode == kUnitDefault) {
        if (w.amount.has_value()) {
            return Written::no(std::string("a default pane ") + which + " carries no amount");
        }
        out = PaneSize{pane_unit::kDefault, 0};
        return Written::ok();
    }
    if (w.mode == kUnitPixels) {
        if (!w.amount.has_value()) {
            return Written::no(std::string("a pixels pane ") + which + " names its amount");
        }
        out = PaneSize{pane_unit::kPixels, *w.amount};
        return Written::ok();
    }
    return Written::no(unknown_unit(w.mode, which, kSizeWords));
}

// ---- VERSION 5, RETAINED FOR READING -----------------------------------------------------
// WL-SETUP-02 -- agents/workshop/setup-file.md
namespace v5 {

inline constexpr std::int64_t kRetainedVersion = 5;

/// Version 5's size: a word and an amount always written, zero where the word reads none.
struct WorkshopPaneSize {
    std::string mode;
    std::int64_t amount = 0;

    ZEN_SHAPE(WorkshopPaneSize, 3, ZEN_FIELD(mode), ZEN_FIELD(amount));
};

/// ...and its place, the same way.
struct WorkshopPanePlace {
    std::string mode;
    std::int64_t x = 0;
    std::int64_t y = 0;

    ZEN_SHAPE(WorkshopPanePlace, 3, ZEN_FIELD(mode), ZEN_FIELD(x), ZEN_FIELD(y));
};

struct WorkshopSetupPane {
    std::string provider;
    std::string pane;
    WorkshopPanePlace place;
    WorkshopPaneSize width;
    WorkshopPaneSize height;
    std::int64_t front = 0;
    std::vector<PaneSetting> settings;

    ZEN_SHAPE(WorkshopSetupPane, 5, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(place),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(front), ZEN_FIELD(settings));
};

struct WorkshopSetup {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::vector<WorkshopSetupPane> panes;

    ZEN_SHAPE(WorkshopSetup, 5, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(panes));
};

static_assert(WorkshopSetup::zen_version == static_cast<std::uint32_t>(kRetainedVersion),
              "a retained setup shape's envelope version and its format version are one "
              "number, as the current one's are");

/// A PLACE AT VERSION 5, BY ITS WORD: the coordinates where the word reads them; a zero where it reads
/// none is the absence version 5 wrote; any other number there is the two answers the law refuses.
inline Written place_to_current(const WorkshopPanePlace& w, setup_persist::WorkshopPanePlace& out) {
    if (w.mode == kUnitPixels) {
        out = setup_persist::WorkshopPanePlace{w.mode, w.x, w.y};
        return Written::ok();
    }
    if (w.mode == kUnitDefault || w.mode == kUnitRightColumn) {
        if (w.x != 0 || w.y != 0) {
            return Written::no(std::string("a ") +
                               (w.mode == kUnitDefault ? "default" : "named") +
                               " pane place carries no coordinates");
        }
        out = setup_persist::WorkshopPanePlace{w.mode, std::nullopt, std::nullopt};
        return Written::ok();
    }
    return Written::no(unknown_unit(w.mode, "place", kPlaceWords));
}

/// ...AND A SIZE, the same way.
inline Written size_to_current(const WorkshopPaneSize& w, setup_persist::WorkshopPaneSize& out,
                               const char* which) {
    if (w.mode == kUnitPixels) {
        out = setup_persist::WorkshopPaneSize{w.mode, w.amount};
        return Written::ok();
    }
    if (w.mode == kUnitDefault) {
        if (w.amount != 0) {
            return Written::no(std::string("a default pane ") + which + " carries no amount");
        }
        out = setup_persist::WorkshopPaneSize{w.mode, std::nullopt};
        return Written::ok();
    }
    return Written::no(unknown_unit(w.mode, which, kSizeWords));
}

/// ONE DESK OF VERSION 5 IN THE CURRENT SHAPE: every row by its words, and no baseline, locks or
/// pins. Refused, in version 5's own words, for a claim that version never made.
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
        row.settings = p.settings;
        if (const Written w = place_to_current(p.place, row.place); !w.accepted) {
            return w;
        }
        if (const Written w = size_to_current(p.width, row.width, "width"); !w.accepted) {
            return w;
        }
        if (const Written w = size_to_current(p.height, row.height, "height"); !w.accepted) {
            return w;
        }
        made.panes.push_back(std::move(row));
    }
    out = std::move(made);
    return Written::ok();
}

} // namespace v5

// ---- VERSION 4, RETAINED FOR READING -----------------------------------------------------
// WL-SETUP-02 -- agents/workshop/setup-file.md
namespace v4 {

inline constexpr std::int64_t kRetainedVersion = 4;

/// Version 4's row: version 5's fields but the settings, over the same place and size.
struct WorkshopSetupPane {
    std::string provider;
    std::string pane;
    v5::WorkshopPanePlace place;
    v5::WorkshopPaneSize width;
    v5::WorkshopPaneSize height;
    std::int64_t front = 0;

    ZEN_SHAPE(WorkshopSetupPane, 4, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(place),
              ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(front));
};

struct WorkshopSetup {
    std::string format;
    std::int64_t format_version = 0;
    std::string name;
    std::vector<WorkshopSetupPane> panes;

    ZEN_SHAPE(WorkshopSetup, 4, ZEN_FIELD(format), ZEN_FIELD(format_version), ZEN_FIELD(name),
              ZEN_FIELD(panes));
};

static_assert(WorkshopSetup::zen_version == static_cast<std::uint32_t>(kRetainedVersion),
              "a retained setup shape's envelope version and its format version are one "
              "number, as the current one's are");

/// ONE DESK OF VERSION 4 AS VERSION 5: every field as it was, and no settings. Refused, in
/// version 4's own words, for a claim that version never made.
// WL-SETUP-02 -- agents/workshop/setup-file.md
inline Written to_v5(const WorkshopSetup& old, v5::WorkshopSetup& out) {
    if (old.format != kFormat) {
        return Written::no("not a Workshop setup: it says it is `" + old.format + "`");
    }
    if (old.format_version != kRetainedVersion) {
        return Written::no(wrong_version(old.format_version));
    }
    v5::WorkshopSetup made;
    made.format = old.format;
    made.format_version = v5::kRetainedVersion;
    made.name = old.name;
    made.panes.reserve(old.panes.size());
    for (const WorkshopSetupPane& p : old.panes) {
        made.panes.push_back(
            v5::WorkshopSetupPane{p.provider, p.pane, p.place, p.width, p.height, p.front, {}});
    }
    out = std::move(made);
    return Written::ok();
}

/// ...AND IN THE CURRENT SHAPE, through version 5.
inline Written to_current(const WorkshopSetup& old, setup_persist::WorkshopSetup& out) {
    v5::WorkshopSetup five;
    if (const Written w = to_v5(old, five); !w.accepted) {
        return w;
    }
    return v5::to_current(five, out);
}

} // namespace v4

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
        out = PanePlace{pane_unit::kPixels, px_of_saved_cells(w.x),
                        room_place_of_canvas(px_of_saved_cells(w.y))};
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
        out = PaneSize{pane_unit::kPixels, px_of_saved_cells(w.amount)};
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
    return graphics::floor_div_px(subs, kSubsPerPixel);
}

/// A `subcells` extent from `at` as the pixels the window painted: its far edge floored, less
/// its near edge floored.
inline constexpr std::int64_t px_extent_of_subcells(std::int64_t at, std::int64_t extent) noexcept {
    return px_of_subcells(graphics::add_cells(at, extent)) - px_of_subcells(at);
}

/// ONE DESK OF VERSION 3 AS VERSION 4, LANDED ON THE PIXELS THE WINDOW PAINTED IT AT. A place
/// floors and is said in the room (`room_place_of_canvas`); an extent is its painted span from the
/// authored place on its axis, or from a whole cell when the place is the code's (every default
/// place is one); a `pixels` extent is a pixel count already, raised to one cell if it was less.
/// Refused, in version 3's own words, for a claim or a word that version never had.
// WL-SETUP-02 -- agents/workshop/setup-file.md
inline Written to_v4(const WorkshopSetup& old, v4::WorkshopSetup& out) {
    if (old.format != kFormat) {
        return Written::no("not a Workshop setup: it says it is `" + old.format + "`");
    }
    if (old.format_version != kRetainedVersion) {
        return Written::no(wrong_version(old.format_version));
    }
    v4::WorkshopSetup made;
    made.format = old.format;
    made.format_version = v4::kRetainedVersion;
    made.name = old.name;
    made.panes.reserve(old.panes.size());
    for (const WorkshopSetupPane& p : old.panes) {
        v4::WorkshopSetupPane row;
        row.provider = p.provider;
        row.pane = p.pane;
        row.front = p.front;
        const bool placed = p.place.mode == kUnitSubcells;
        if (p.place.mode == kUnitDefault || p.place.mode == kUnitRightColumn) {
            row.place = v5::WorkshopPanePlace{p.place.mode, p.place.x, p.place.y};
        } else if (placed) {
            row.place = v5::WorkshopPanePlace{kUnitPixels, px_of_subcells(p.place.x),
                                              room_place_of_canvas(px_of_subcells(p.place.y))};
        } else {
            return Written::no(unknown_unit(p.place.mode, "place", kPlaceWords));
        }
        const auto axis = [](const WorkshopPaneSize& w, std::int64_t at,
                             v5::WorkshopPaneSize& to) {
            if (w.mode == kUnitDefault) {
                to = v5::WorkshopPaneSize{kUnitDefault, w.amount};
            } else if (w.mode == kUnitSubcells) {
                to = v5::WorkshopPaneSize{kUnitPixels, px_extent_of_subcells(at, w.amount)};
            } else if (w.mode == kUnitPixels) {
                to = v5::WorkshopPaneSize{kUnitPixels, at_least_a_cell(w.amount)};
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

/// The pane-migration count of `moved`, added into `converted` when one is kept.
inline void add_converted(pane_migration::Converted* converted,
                          const pane_migration::Converted& moved) {
    if (converted != nullptr) {
        for (std::size_t i = 0; i < pane_migration::kRetiredCount; ++i) {
            converted->rows[i] += moved.rows[i];
        }
    }
}

/// A version-2 SETUP AS A LIVE ONE -- the same four layers `setup_in` below walks, against
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
    // A REFERENCE THAT CHANGED HANDS, REWRITTEN -- the same step the current road takes, on the
    // same candidate, before the same law.
    add_converted(converted, pane_migration::convert_retired_panes(candidate));
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
    // AND THE FIELD IS STILL CHECKED, for a file whose envelope is this version and whose own
    // stated version is not -- which only a forgery produces.
    if (file.format_version != kFormatVersion) {
        return Written::no(wrong_version(file.format_version));
    }
    Setup candidate;
    candidate.name = file.name;
    candidate.panes.reserve(file.panes.size());
    for (const WorkshopSetupPane& p : file.panes) {
        // COPIED, NEVER RESOLVED: whether this build can present the pane is asked later.
        SetupPane row;
        row.ref = PaneRef{p.provider, p.pane};
        row.front = p.front;
        // AS WRITTEN: their form is the law's to judge below, and their meaning the pane's.
        row.settings = p.settings;
        // A WORD WITH NO MEANING, OR A FIELD WHERE ITS WORD READS NONE, REFUSES THE WHOLE
        // CANDIDATE and leaves whatever the caller is showing exactly as it was.
        if (const Written w = place_in(p.place, row.place); !w.accepted) {
            return w;
        }
        if (const Written w = size_in(p.width, row.width, "width"); !w.accepted) {
            return w;
        }
        if (const Written w = size_in(p.height, row.height, "height"); !w.accepted) {
            return w;
        }
        candidate.panes.push_back(std::move(row));
    }
    candidate.theme = file.theme;
    for (const WorkshopSetupLock& lock : file.locks) {
        candidate.locks.push_back(SetupLock{PaneRef{lock.provider, lock.pane}, lock.key});
    }
    for (const WorkshopSetupPinList& list : file.pins) {
        candidate.pins.push_back(SetupPins{PaneRef{list.provider, list.pane}, list.keys});
    }
    // A pane that changed hands is rewritten here, in the one function that turns written rows
    // into a live `Setup` (the setup file, every desk in a session, every remembered link). The
    // converted candidate then meets the whole law, so a file naming both spellings is refused.
    add_converted(converted, pane_migration::convert_retired_panes(candidate));
    const Written legal = check_setup(candidate);
    if (!legal.accepted) {
        return legal;
    }
    out = std::move(candidate);
    return Written::ok();
}

/// A SETUP OF AN OLDER VERSION AS THE CURRENT VALUE OF ITS FILE, or why not: version 5 by its
/// words, version 4 through version 5, version 3 through version 4. Version 2 is read as a live
/// setup directly (`setup_in_v2`).
// WL-SETUP-02 -- agents/workshop/setup-file.md
inline Written lifted(const loom::Unverified& claim, WorkshopSetup& out) {
    const std::uint32_t version = claim.claimed_version();
    if (version == v5::WorkshopSetup::zen_version) {
        const loom::Admission old =
            loom::admit(claim, loom::schema_of<v5::WorkshopSetup>(), loom::Report::FirstError);
        if (!old.ok()) {
            return Written::no(old.first_error().message());
        }
        return v5::to_current(loom::from_value<v5::WorkshopSetup>(old.value()), out);
    }
    v4::WorkshopSetup four;
    if (version == v3::WorkshopSetup::zen_version) {
        const loom::Admission old =
            loom::admit(claim, loom::schema_of<v3::WorkshopSetup>(), loom::Report::FirstError);
        if (!old.ok()) {
            return Written::no(old.first_error().message());
        }
        if (const Written landed =
                v3::to_v4(loom::from_value<v3::WorkshopSetup>(old.value()), four);
            !landed.accepted) {
            return landed;
        }
    } else {
        const loom::Admission old =
            loom::admit(claim, loom::schema_of<v4::WorkshopSetup>(), loom::Report::FirstError);
        if (!old.ok()) {
            return Written::no(old.first_error().message());
        }
        four = loom::from_value<v4::WorkshopSetup>(old.value());
    }
    return v4::to_current(four, out);
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
    const bool ours = claim.claimed_name() == std::string(WorkshopSetup::zen_name);
    // The version preflight orders, never loosens: it reads the claim, which exists before
    // admission, so a version-1 file is refused by its number rather than by the first field
    // version 2 added. An older claim takes its own road: admitted at full strength against that
    // version's retained shape, then landed on the current one.
    if (ours && claim.claimed_version() == v2::WorkshopSetup::zen_version) {
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
        return LoadedSetup{Written::ok(), std::move(candidate), converted};
    }
    WorkshopSetup current;
    if (ours && (claim.claimed_version() == v3::WorkshopSetup::zen_version ||
                 claim.claimed_version() == v4::WorkshopSetup::zen_version ||
                 claim.claimed_version() == v5::WorkshopSetup::zen_version)) {
        if (const Written landed = lifted(claim, current); !landed.accepted) {
            return LoadedSetup::no(landed.refusal);
        }
    } else {
        if (ours && claim.claimed_version() != WorkshopSetup::zen_version) {
            return LoadedSetup::no(
                wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
        }
        const loom::Admission admitted =
            loom::admit(claim, loom::schema_of<WorkshopSetup>(), loom::Report::FirstError);
        if (!admitted.ok()) {
            return LoadedSetup::no(admitted.first_error().message());
        }
        current = loom::from_value<WorkshopSetup>(admitted.value());
    }
    // THE CANDIDATE IS A LOCAL OF THIS FUNCTION, which is how "a malformed file never leaves
    // Workshop halfway restored" stays structural.
    Setup candidate;
    pane_migration::Converted converted;
    const Written understood = setup_in(current, candidate, &converted);
    if (!understood.accepted) {
        return LoadedSetup::no(understood.refusal);
    }
    return LoadedSetup{Written::ok(), std::move(candidate), converted};
}

// ---- The file itself -------------------------------------------------------------

/// Save a setup through `persist`'s safe write: a complete candidate to a sibling, then a rename
/// over the destination. Text past `kMaxSetupBytes`, which `load_file` would refuse, is refused
/// here with nothing written.
// WL-SETUP-11 -- agents/workshop/setup-file.md
inline Written save_file(const std::string& path, const Setup& s) {
    const std::string text = to_text(s);
    if (text.size() > kMaxSetupBytes) {
        return Written::no("this setup is " + std::to_string(text.size()) +
                           " bytes, larger than a Workshop setup can be (" +
                           std::to_string(kMaxSetupBytes) + ") -- nothing was written");
    }
    return persist::write_file(path, text);
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
