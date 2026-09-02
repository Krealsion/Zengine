// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SESSION_HISTORY_HPP
#define ZENGINE_WORKSHOP_SESSION_HISTORY_HPP

// WHAT A WORKSHOP SESSION USED TO LOOK LIKE, AND HOW IT BECOMES WHAT ONE LOOKS LIKE NOW
// — the whole of yesterday, in one place that is not the reader.
// Workshop law: agents/workshop/migration.md

#include "operator/migration.hpp"
#include "operator/operator.hpp"
#include "panel.hpp"
#include "session_persist.hpp"
#include "setup_persist.hpp"
#include "surface/region.hpp"

#include <zen/schema.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop::session_history {

// ---- The shapes older Workshops wrote -----------------------------------------------
// WL-MIG-02 -- agents/workshop/migration.md

namespace v1 {

/// Version 1: the room, and a whole-cell desk. No placement existed to write.
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    session_persist::WorkshopViewport viewport;
    setup_persist::v2::WorkshopSetup desk;

    ZEN_SHAPE(WorkshopSession, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(desk));
};

} // namespace v1

namespace v2 {

/// Version 2: the desk moved to the fine lattice and this format's version moved
/// with it. Still no placement.
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    session_persist::WorkshopViewport viewport;
    setup_persist::WorkshopSetup desk;

    ZEN_SHAPE(WorkshopSession, 2, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(desk));
};

} // namespace v2

namespace v3 {

/// Version 3: the room, the current desk shape, and the desktop
/// placement. ONE desk — the plural arrived at version 4, and this is the shape that had
/// singular in it.
// WL-MIG-02 -- agents/workshop/migration.md
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    session_persist::WorkshopViewport viewport;
    setup_persist::WorkshopSetup desk;
    session_persist::WorkshopPlacement placement;

    ZEN_SHAPE(WorkshopSession, 3, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(desk), ZEN_FIELD(placement));
};

} // namespace v3

namespace v4 {

/// Version 4: the room, the whole ordered layout RUN, which
/// position was live, and the desktop placement. Every layout is a bare desk — the optional
/// Setup ASSOCIATION arrived at version 5, and this is the shape that had no room for one.
// WL-MIG-02 -- agents/workshop/migration.md
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    session_persist::WorkshopViewport viewport;
    std::vector<setup_persist::WorkshopSetup> layouts;
    std::int64_t active = 0;
    session_persist::WorkshopPlacement placement;

    ZEN_SHAPE(WorkshopSession, 4, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(layouts), ZEN_FIELD(active),
              ZEN_FIELD(placement));
};

} // namespace v4

namespace v5 {

/// Version 5: the room, the whole ordered layout run with each
/// layout's optional Setup association, which position was live, and the desktop placement.
// WL-MIG-02, WL-MIG-03 -- agents/workshop/migration.md
struct WorkshopSession {
    std::string format;
    std::int64_t format_version = 0;
    session_persist::WorkshopViewport viewport;
    std::vector<session_persist::WorkshopLayout> layouts;
    std::int64_t active = 0;
    session_persist::WorkshopPlacement placement;

    ZEN_SHAPE(WorkshopSession, 5, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(viewport), ZEN_FIELD(layouts), ZEN_FIELD(active),
              ZEN_FIELD(placement));
};

} // namespace v5

/// THE VERSION NUMBERS THIS FILE CONVERTS FROM. They are the edges' own, said once so the
/// shapes above and the definitions below cannot come to disagree about which vintage each
/// is.
inline constexpr std::int64_t kV1FormatVersion = 1;
inline constexpr std::int64_t kV2FormatVersion = 2;
inline constexpr std::int64_t kV3FormatVersion = 3;
inline constexpr std::int64_t kV4FormatVersion = 4;
inline constexpr std::int64_t kV5FormatVersion = 5;

static_assert(v1::WorkshopSession::zen_version == static_cast<std::uint32_t>(kV1FormatVersion),
              "a historical session shape's envelope version and the format version it "
              "converts are one number, exactly as they are for the current one");
static_assert(v2::WorkshopSession::zen_version == static_cast<std::uint32_t>(kV2FormatVersion),
              "a historical session shape's envelope version and the format version it "
              "converts are one number, exactly as they are for the current one");
static_assert(v3::WorkshopSession::zen_version == static_cast<std::uint32_t>(kV3FormatVersion),
              "a historical session shape's envelope version and the format version it "
              "converts are one number, exactly as they are for the current one");
static_assert(v4::WorkshopSession::zen_version == static_cast<std::uint32_t>(kV4FormatVersion),
              "a historical session shape's envelope version and the format version it "
              "converts are one number, exactly as they are for the current one");
static_assert(v5::WorkshopSession::zen_version == static_cast<std::uint32_t>(kV5FormatVersion),
              "a historical session shape's envelope version and the format version it "
              "converts are one number, exactly as they are for the current one");

/// ...AND THE VERSION THEY ALL CONVERT TO IS THE READER'S, NEVER A NUMBER TYPED HERE.
static_assert(session_persist::kFormatVersion > kV5FormatVersion,
              "every shape in this file is RETIRED: the current reader's version must be "
              "ahead of all of them, or one of these is not history");

// ---- Translating ---------------------------------------------------------------------

/// What to say about an old file whose own `format_version` field is not the version its
/// envelope claimed. One sentence, one place, because both edges can meet it.
// WL-MIG-07 -- agents/workshop/migration.md
inline std::string mismatched_version(const char* what, std::int64_t claimed,
                                      std::int64_t found) {
    return std::string("this ") + what + " claims version " + std::to_string(claimed) +
           " and its own format_version field says " + std::to_string(found);
}

/// THE PLACEMENT EVERY HISTORICAL SESSION HAS: none, in the one spelling that means it.
// WL-MIG-05 -- agents/workshop/migration.md
inline session_persist::WorkshopPlacement absent_placement() {
    session_persist::WorkshopPlacement none;
    none.mode = session_persist::kPlacementNone;
    none.x = 0;
    none.y = 0;
    none.window = session_persist::kWindowNormal;
    return none;
}

/// ONE AUTHORED PLACE, SAID IN THE FINE LATTICE'S WORDS.
// WL-SETUP-02 -- agents/workshop/setup-file.md
inline setup_persist::WorkshopPanePlace
place_v2_to_v3(const setup_persist::v2::WorkshopPanePlace& w) {
    setup_persist::WorkshopPanePlace out;
    if (w.mode == setup_persist::kUnitDefault) {
        out.mode = setup_persist::kUnitDefault;
        out.x = w.x;
        out.y = w.y;
        return out;
    }
    if (w.mode == setup_persist::v2::kUnitCells) {
        out.mode = setup_persist::kUnitSubcells;
        out.x = surface::subs_of_cells(w.x);
        out.y = surface::subs_of_cells(w.y);
        return out;
    }
    throw std::invalid_argument(
        setup_persist::unknown_unit(w.mode, "place", setup_persist::v2::kPlaceWords));
}

/// ONE AUTHORED SIZE, the same way — with `pixels`, which both versions spell identically
/// and which crosses unscaled because a device pixel did not change size when the lattice
/// got finer.
inline setup_persist::WorkshopPaneSize
size_v2_to_v3(const setup_persist::v2::WorkshopPaneSize& w, const char* which) {
    setup_persist::WorkshopPaneSize out;
    if (w.mode == setup_persist::kUnitDefault) {
        out.mode = setup_persist::kUnitDefault;
        out.amount = w.amount;
        return out;
    }
    if (w.mode == setup_persist::v2::kUnitCells) {
        out.mode = setup_persist::kUnitSubcells;
        out.amount = surface::subs_of_cells(w.amount);
        return out;
    }
    if (w.mode == setup_persist::kUnitPixels) {
        out.mode = setup_persist::kUnitPixels;
        out.amount = w.amount;
        return out;
    }
    throw std::invalid_argument(
        setup_persist::unknown_unit(w.mode, which, setup_persist::v2::kSizeWords));
}

/// A WHOLE-CELL DESK AS A FINE-LATTICE ONE.
// WL-MIG-02 -- agents/workshop/migration.md; WL-SETUP-02 -- agents/workshop/setup-file.md
inline setup_persist::WorkshopSetup desk_v2_to_v3(const setup_persist::v2::WorkshopSetup& old) {
    if (old.format_version != setup_persist::kLegacyFormatVersion) {
        throw std::invalid_argument(
            mismatched_version("desk", setup_persist::kLegacyFormatVersion,
                               old.format_version));
    }
    setup_persist::WorkshopSetup out;
    out.format = old.format;
    out.format_version = setup_persist::kFormatVersion;
    out.name = old.name;
    out.panes.reserve(old.panes.size());
    for (const setup_persist::v2::WorkshopSetupPane& p : old.panes) {
        setup_persist::WorkshopSetupPane row;
        row.provider = p.provider;
        row.pane = p.pane;
        row.front = p.front;
        row.place = place_v2_to_v3(p.place);
        row.width = size_v2_to_v3(p.width, "width");
        row.height = size_v2_to_v3(p.height, "height");
        out.panes.push_back(std::move(row));
    }
    return out;
}

/// A VERSION-1 SESSION AS A VERSION-3 ONE — yesterday's meaning, said in the last shape
/// that had a single desk in it.
// WL-MIG-02, WL-MIG-05 -- agents/workshop/migration.md
inline v3::WorkshopSession session_v1_to_v3(const v1::WorkshopSession& old) {
    if (old.format_version != kV1FormatVersion) {
        throw std::invalid_argument(
            mismatched_version("session", kV1FormatVersion, old.format_version));
    }
    v3::WorkshopSession out;
    out.format = old.format;
    out.format_version = kV3FormatVersion;
    out.viewport = old.viewport;
    out.desk = desk_v2_to_v3(old.desk);
    out.placement = absent_placement();
    return out;
}

/// A VERSION-2 SESSION AS A VERSION-3 ONE — that format's version, plus the placement it
/// never had.
// WL-MIG-02 -- agents/workshop/migration.md
inline v3::WorkshopSession session_v2_to_v3(const v2::WorkshopSession& old) {
    if (old.format_version != kV2FormatVersion) {
        throw std::invalid_argument(
            mismatched_version("session", kV2FormatVersion, old.format_version));
    }
    v3::WorkshopSession out;
    out.format = old.format;
    out.format_version = kV3FormatVersion;
    out.viewport = old.viewport;
    out.desk = old.desk;
    out.placement = absent_placement();
    return out;
}

/// A VERSION-3 SESSION AS A VERSION-4 ONE — the one desk it had, as a layout run
/// holding exactly that desk, live.
// WL-MIG-02, WL-MIG-05 -- agents/workshop/migration.md
inline v4::WorkshopSession session_v3_to_v4(const v3::WorkshopSession& old) {
    if (old.format_version != kV3FormatVersion) {
        throw std::invalid_argument(
            mismatched_version("session", kV3FormatVersion, old.format_version));
    }
    v4::WorkshopSession out;
    out.format = old.format;
    out.format_version = kV4FormatVersion;
    out.viewport = old.viewport;
    out.layouts.push_back(old.desk);
    out.active = 0;
    out.placement = old.placement;
    return out;
}

/// THE SETUP ASSOCIATION EVERY HISTORICAL LAYOUT HAS: none, in the one spelling that means
/// it.
// WL-MIG-05 -- agents/workshop/migration.md
inline session_persist::WorkshopSetupLink absent_link() {
    return session_persist::WorkshopSetupLink{std::string(), setup_persist::to_setup(Setup{})};
}

/// A VERSION-4 SESSION AS A VERSION-5 ONE — every layout it had, in its own order,
/// each with no Setup association.
// WL-MIG-02, WL-MIG-05 -- agents/workshop/migration.md
inline v5::WorkshopSession session_v4_to_v5(const v4::WorkshopSession& old) {
    if (old.format_version != kV4FormatVersion) {
        throw std::invalid_argument(
            mismatched_version("session", kV4FormatVersion, old.format_version));
    }
    v5::WorkshopSession out;
    out.format = old.format;
    out.format_version = kV5FormatVersion;
    out.viewport = old.viewport;
    out.layouts.reserve(old.layouts.size());
    for (const setup_persist::WorkshopSetup& desk : old.layouts) {
        out.layouts.push_back(session_persist::WorkshopLayout{desk, absent_link()});
    }
    out.active = old.active;
    out.placement = old.placement;
    return out;
}

/// WAS THE LAYOUTS PANE ALREADY WRITTEN DOWN HERE?
// WL-MIG-03 -- agents/workshop/migration.md
inline bool names_layouts(const setup_persist::WorkshopSetup& desk) {
    for (const setup_persist::WorkshopSetupPane& row : desk.panes) {
        if (row.provider == kWorkshopProvider && row.pane == pane_key::kLayouts) {
            return true;
        }
    }
    return false;
}

/// WHAT A DESK WRITTEN BEFORE VERSION 6 MEANT, SAID IN THE SHAPE THAT CAN SAY IT.
// WL-MIG-03 -- agents/workshop/migration.md
inline setup_persist::WorkshopSetup
desk_v5_to_v6(const setup_persist::WorkshopSetup& old, const char* what) {
    if (names_layouts(old)) {
        return old;
    }
    if (old.panes.size() >= kMaxSetupPanes) {
        throw std::invalid_argument(
            std::string("this ") + what + " already names " + std::to_string(old.panes.size()) +
            " panes, which is all a setup may hold, so the layout surface every Workshop of "
            "its vintage had cannot be written into it as the pane it has become");
    }
    setup_persist::WorkshopSetup out = old;
    setup_persist::WorkshopSetupPane row;
    row.provider = kWorkshopProvider;
    row.pane = pane_key::kLayouts;
    row.front = static_cast<std::int64_t>(out.panes.size());
    row.place.mode = setup_persist::kUnitDefault;
    row.width.mode = setup_persist::kUnitDefault;
    row.height.mode = setup_persist::kUnitDefault;
    out.panes.push_back(std::move(row));
    return out;
}

/// A VERSION-5 SESSION AS A CURRENT ONE -- every layout it had, in its own order,
/// with the layout surface it always had now written down as the pane it has become.
// WL-MIG-02, WL-MIG-03 -- agents/workshop/migration.md
inline session_persist::WorkshopSession session_v5_to_v6(const v5::WorkshopSession& old) {
    if (old.format_version != kV5FormatVersion) {
        throw std::invalid_argument(
            mismatched_version("session", kV5FormatVersion, old.format_version));
    }
    session_persist::WorkshopSession out;
    out.format = old.format;
    out.format_version = session_persist::kFormatVersion;
    out.viewport = old.viewport;
    out.layouts.reserve(old.layouts.size());
    for (const session_persist::WorkshopLayout& layout : old.layouts) {
        session_persist::WorkshopLayout made;
        made.desk = desk_v5_to_v6(layout.desk, "layout");
        made.link.path = layout.link.path;
        made.link.known = layout.link.path.empty()
                              ? layout.link.known
                              : desk_v5_to_v6(layout.link.known, "remembered Setup value");
        out.layouts.push_back(std::move(made));
    }
    out.active = old.active;
    out.placement = old.placement;
    return out;
}

/// A VERSION-1 SESSION AS A CURRENT ONE — one authored edge, whose body composes the
/// translations above.
// WL-MIG-02 -- agents/workshop/migration.md
inline session_persist::WorkshopSession session_v1_to_v6(const v1::WorkshopSession& old) {
    return session_v5_to_v6(session_v4_to_v5(session_v3_to_v4(session_v1_to_v3(old))));
}

/// A VERSION-2 SESSION AS A CURRENT ONE, the same way.
inline session_persist::WorkshopSession session_v2_to_v6(const v2::WorkshopSession& old) {
    return session_v5_to_v6(session_v4_to_v5(session_v3_to_v4(session_v2_to_v3(old))));
}

/// A VERSION-3 SESSION AS A CURRENT ONE, the same way.
inline session_persist::WorkshopSession session_v3_to_v6(const v3::WorkshopSession& old) {
    return session_v5_to_v6(session_v4_to_v5(session_v3_to_v4(old)));
}

/// A VERSION-4 SESSION AS A CURRENT ONE, the same way.
inline session_persist::WorkshopSession session_v4_to_v6(const v4::WorkshopSession& old) {
    return session_v5_to_v6(session_v4_to_v5(old));
}

// ---- ...and the two of them as ordinary contributions ---------------------------------

/// EVERY EDGE THIS HISTORY SUPPLIES, as ordinary operator definitions.
// WL-MIG-01, WL-MIG-02 -- agents/workshop/migration.md
inline std::vector<op::OperatorDef> conversions() {
    auto current = loom::schema_of<session_persist::WorkshopSession>();
    std::vector<op::OperatorDef> edges;
    edges.push_back(op::make_migration(
        loom::schema_of<v1::WorkshopSession>(), current, [](const loom::Value& old) {
            return loom::Cell::message(
                loom::to_value(session_v1_to_v6(loom::from_value<v1::WorkshopSession>(old))));
        }));
    edges.push_back(op::make_migration(
        loom::schema_of<v2::WorkshopSession>(), current, [](const loom::Value& old) {
            return loom::Cell::message(
                loom::to_value(session_v2_to_v6(loom::from_value<v2::WorkshopSession>(old))));
        }));
    edges.push_back(op::make_migration(
        loom::schema_of<v3::WorkshopSession>(), current, [](const loom::Value& old) {
            return loom::Cell::message(
                loom::to_value(session_v3_to_v6(loom::from_value<v3::WorkshopSession>(old))));
        }));
    edges.push_back(op::make_migration(
        loom::schema_of<v4::WorkshopSession>(), current, [](const loom::Value& old) {
            return loom::Cell::message(
                loom::to_value(session_v4_to_v6(loom::from_value<v4::WorkshopSession>(old))));
        }));
    edges.push_back(op::make_migration(
        loom::schema_of<v5::WorkshopSession>(), current, [](const loom::Value& old) {
            return loom::Cell::message(
                loom::to_value(session_v5_to_v6(loom::from_value<v5::WorkshopSession>(old))));
        }));
    return edges;
}

} // namespace zengine::workshop::session_history

#endif // ZENGINE_WORKSHOP_SESSION_HISTORY_HPP
