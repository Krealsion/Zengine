// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_DEFINITION_HPP
#define ZENGINE_WORKSHOP_PANE_DEFINITION_HPP

// WHAT EXISTS INSIDE A PANE A MAKER MADE -- the first pane implementation whose interior is
// authored DATA rather than a painter compiled into this application.
// Workshop law: agents/workshop/maker-pane.md

#include "document.hpp" // `doc::kMaxCells` -- the one lattice bound every authored extent already has
#include "property.hpp" // `Written` -- the one refusal-with-reason shape

#include "surface/vocabulary.hpp" // `kCellSubs` -- the fine lattice a region is authored on

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// THE KINDS OF REGION A DEFINITION MAY HOLD. One, and the set is closed: a kind is earned
/// by a consumer with an invariant, not declared ahead of one. The in-memory integer is
/// arbitrary; the FILE writes the word (`pane_definition_persist.hpp`).
namespace region_kind {
inline constexpr std::int64_t kText = 0;
} // namespace region_kind

// ---- The bounds, each an INPUT BOUNDARY and not a capacity --------------------------------

/// How long a maker-made pane's name may be.
// WL-MAKER-01 -- agents/workshop/maker-pane.md
inline constexpr std::size_t kMaxMakerPaneNameLen = 32;

/// How many regions one definition may carry. Sixteen bounds a file at a few kilobytes
/// and is far above what one text-only pane has any use for; it is a bound on what a
/// stranger's file can push into a live definition, not a statement about layout.
inline constexpr std::size_t kMaxRegions = 16;

/// How many bytes a text region's content may hold. One line of prose, cut and marked by
/// the presentation wherever the region is narrower.
inline constexpr std::size_t kMaxRegionTextLen = 256;

/// THE AUTHORED LATTICE'S WALLS, in sub-units.
// WL-MAKER-01 -- agents/workshop/maker-pane.md
inline constexpr std::int64_t kRegionSubMax = doc::kMaxCells * surface::kCellSubs;

/// The first identity the mint hands out. Never 0, so an absent identity has a number no
/// region can carry.
inline constexpr std::int64_t kFirstRegionId = 1;

// ---- The value ------------------------------------------------------------------------------

/// ONE REGION OF A MAKER-MADE PANE'S INTERIOR.
// WL-MAKER-01 -- agents/workshop/maker-pane.md
struct TextRegion {
    std::int64_t id = 0;
    std::int64_t kind = region_kind::kText;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::string text;

    friend bool operator==(const TextRegion&, const TextRegion&) = default;
};

/// A MAKER-MADE PANE'S INTERIOR: its durable name, its regions in authored order, and the
/// mint.
// WL-MAKER-01, WL-MAKER-03 -- agents/workshop/maker-pane.md
struct PaneDefinition {
    std::string name;
    std::vector<TextRegion> regions;
    std::int64_t next_id = kFirstRegionId;

    bool open() const noexcept { return !name.empty(); }

    friend bool operator==(const PaneDefinition&, const PaneDefinition&) = default;
};

// ---- The law: what this application accepts as a definition ----------------------------------
// WL-MAKER-07 -- agents/workshop/maker-pane.md

/// What this application accepts as a maker-made pane's name.
///
/// IT IS A DURABLE KEY AND A DISPLAY NAME AT ONCE, so it meets both laws: present, short
/// enough to read, and free of whitespace and control bytes so that `provider/name` stays
/// one legible token in a notice and in a file. `/` is refused too, because that is the
/// character the reference's own prose spelling uses between its halves.
inline Written check_maker_pane_name(const std::string& name) {
    if (name.empty()) {
        return Written::no("a pane name cannot be empty");
    }
    if (name.size() > kMaxMakerPaneNameLen) {
        return Written::no("a pane name is at most " + std::to_string(kMaxMakerPaneNameLen) +
                           " bytes");
    }
    for (const char c : name) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte >= 0x7Fu) {
            return Written::no("a pane name is plain ASCII with no spaces or control characters");
        }
        if (c == '/') {
            return Written::no("a pane name cannot contain `/`");
        }
    }
    return Written::ok();
}

/// What a `text` region may say: printable ASCII, bounded. Empty is legal.
inline Written check_region_text(const std::string& text) {
    if (text.size() > kMaxRegionTextLen) {
        return Written::no("a text region holds at most " + std::to_string(kMaxRegionTextLen) +
                           " bytes");
    }
    for (const char c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte >= 0x7Fu) {
            return Written::no("a text region holds plain ASCII with no control characters");
        }
    }
    return Written::ok();
}

/// ONE COORDINATE of a region's place: not negative (the interior has no sub-units there),
/// and inside the lattice's ceiling.
inline Written check_region_coord(std::int64_t v) {
    if (v < 0) {
        return Written::no("a region place cannot be negative");
    }
    if (v > kRegionSubMax) {
        return Written::no("a region place is at most " + std::to_string(doc::kMaxCells) +
                           " cells");
    }
    return Written::ok();
}

/// ONE AXIS of a region's extent: positive, and inside the lattice's ceiling. The floor is
/// one sub-unit and not one cell, deliberately -- a region finer than a cell is honest
/// intent, and which faces can show anything of it is those faces' answer.
inline Written check_region_extent(std::int64_t v, const char* which) {
    if (v <= 0) {
        return Written::no(std::string("a region ") + which + " must be positive");
    }
    if (v > kRegionSubMax) {
        return Written::no(std::string("a region ") + which + " is at most " +
                           std::to_string(doc::kMaxCells) + " cells");
    }
    return Written::ok();
}

/// Every law ONE region meets, minus the two that are about the whole definition (its
/// identity is minted and distinct).
inline Written check_region(const TextRegion& r) {
    if (r.kind != region_kind::kText) {
        return Written::no("a region's kind is `text` -- no other kind exists yet");
    }
    const Written x = check_region_coord(r.x);
    if (!x.accepted) {
        return x;
    }
    const Written y = check_region_coord(r.y);
    if (!y.accepted) {
        return y;
    }
    const Written w = check_region_extent(r.w, "width");
    if (!w.accepted) {
        return w;
    }
    const Written h = check_region_extent(r.h, "height");
    if (!h.accepted) {
        return h;
    }
    return check_region_text(r.text);
}

/// THE WHOLE-DEFINITION LAW, asked once on a complete candidate: the name, how many regions,
/// that every identity is minted (below the mint, never 0), that no two share one, and every
/// region's own law. It judges and never repairs.
inline Written check_definition(const PaneDefinition& d) {
    const Written named = check_maker_pane_name(d.name);
    if (!named.accepted) {
        return named;
    }
    if (d.next_id < kFirstRegionId) {
        return Written::no("a pane definition's next region id is at least " +
                           std::to_string(kFirstRegionId));
    }
    if (d.regions.size() > kMaxRegions) {
        return Written::no("a pane definition holds at most " + std::to_string(kMaxRegions) +
                           " regions -- this one names " + std::to_string(d.regions.size()));
    }
    for (std::size_t i = 0; i < d.regions.size(); ++i) {
        const TextRegion& r = d.regions[i];
        const std::string who = "region #" + std::to_string(r.id) + ": ";
        if (r.id < kFirstRegionId || r.id >= d.next_id) {
            return Written::no(who + "its id was never minted (the next id is " +
                               std::to_string(d.next_id) + ")");
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (d.regions[j].id == r.id) {
                return Written::no(who + "two regions share this id");
            }
        }
        const Written legal = check_region(r);
        if (!legal.accepted) {
            return Written::no(who + legal.refusal);
        }
    }
    return Written::ok();
}

// ---- The doors: every write to a definition goes through one of these ------------------------

/// The region with this id, or nothing. By handle; nothing may hold one across an edit
/// that could grow `regions`.
inline TextRegion* region_of(PaneDefinition& d, std::int64_t id) {
    for (TextRegion& r : d.regions) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

inline const TextRegion* region_of(const PaneDefinition& d, std::int64_t id) {
    for (const TextRegion& r : d.regions) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

/// MINT ONE TEXT REGION and append it. The geometry is judged first; the mint moves only
/// when the region lands, so a refused add leaves the definition byte-identical.
inline Written add_text_region(PaneDefinition& d, std::int64_t x, std::int64_t y, std::int64_t w,
                               std::int64_t h) {
    if (d.regions.size() >= kMaxRegions) {
        return Written::no("a pane definition holds at most " + std::to_string(kMaxRegions) +
                           " regions");
    }
    if (d.next_id < kFirstRegionId || d.next_id == (std::numeric_limits<std::int64_t>::max)()) {
        return Written::no("this pane definition can mint no more region ids");
    }
    TextRegion r;
    r.id = d.next_id;
    r.kind = region_kind::kText;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    const Written legal = check_region(r);
    if (!legal.accepted) {
        return legal;
    }
    ++d.next_id;
    d.regions.push_back(std::move(r));
    return Written::ok();
}

/// SET WHAT A TEXT REGION SAYS. Judged whole, written whole.
inline Written set_region_text(PaneDefinition& d, std::int64_t id, std::string text) {
    TextRegion* r = region_of(d, id);
    if (r == nullptr) {
        return Written::no("no region #" + std::to_string(id) + " in this pane");
    }
    const Written legal = check_region_text(text);
    if (!legal.accepted) {
        return legal;
    }
    r->text = std::move(text);
    return Written::ok();
}

/// AUTHOR ONE AXIS OF A REGION'S GEOMETRY: 0 = X, 1 = Y, 2 = width, 3 = height, in
/// sub-units. Each axis is its own fact and its own refusal -- refuse-never-clamp, per axis
/// -- and an accepted write moves exactly the one number.
inline Written author_region_axis(PaneDefinition& d, std::int64_t id, std::size_t axis,
                                  std::int64_t subs) {
    TextRegion* r = region_of(d, id);
    if (r == nullptr) {
        return Written::no("no region #" + std::to_string(id) + " in this pane");
    }
    switch (axis) {
    case 0: {
        const Written ok = check_region_coord(subs);
        if (!ok.accepted) {
            return ok;
        }
        r->x = subs;
        return ok;
    }
    case 1: {
        const Written ok = check_region_coord(subs);
        if (!ok.accepted) {
            return ok;
        }
        r->y = subs;
        return ok;
    }
    case 2: {
        const Written ok = check_region_extent(subs, "width");
        if (!ok.accepted) {
            return ok;
        }
        r->w = subs;
        return ok;
    }
    default: {
        const Written ok = check_region_extent(subs, "height");
        if (!ok.accepted) {
            return ok;
        }
        r->h = subs;
        return ok;
    }
    }
}

// ---- A new pane's first region: the developer's default, authored -------------------------------
// WL-MAKER-11 -- agents/workshop/maker-pane.md
inline constexpr std::int64_t kNewRegionX = 0;
inline constexpr std::int64_t kNewRegionY = 0;
inline constexpr std::int64_t kNewRegionW = 24 * surface::kCellSubs;
inline constexpr std::int64_t kNewRegionH = 2 * surface::kCellSubs;

/// A FRESH DEFINITION FOR A NAME: one empty text region, minted as #1. The name is the
/// caller's to have judged (`check_maker_pane_name`) -- this composes a legal value and does
/// not re-judge it, for the same reason `default_setup` composes rather than checks.
inline PaneDefinition new_definition(std::string name) {
    PaneDefinition d;
    d.name = std::move(name);
    (void)add_text_region(d, kNewRegionX, kNewRegionY, kNewRegionW, kNewRegionH);
    return d;
}

// ---- The session's one open definition --------------------------------------------------------

/// THE ONE MAKER-MADE PANE THIS RUN HAS OPEN, and the facts about it that are the run's
/// rather than the definition's: which file it stands for, and the last value that file
/// was known to hold.
// WL-MAKER-01, WL-MAKER-08 -- agents/workshop/maker-pane.md
struct MakerPane {
    std::string path;
    PaneDefinition definition;
    PaneDefinition saved;

    bool open() const noexcept { return definition.open(); }
    bool dirty() const noexcept { return definition != saved; }
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DEFINITION_HPP
