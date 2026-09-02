// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_DEFINITION_HPP
#define ZENGINE_WORKSHOP_PANE_DEFINITION_HPP

// WHAT EXISTS INSIDE A PANE A MAKER MADE -- the first pane implementation whose interior is
// authored DATA rather than a painter compiled into this application.
//
// THE QUESTION THIS FILE ANSWERS, and the whole of it: what is the smallest value that can
// truthfully say what is inside a pane a maker created from inside Workshop? Until this
// file, no type could. `PaneRef` says WHICH pane a maker meant, `SetupPane` says WHERE it
// participates on one desk, and the inside of every pane was one of two things: C++
// composition (a built-in's painter) or rows a provider chose to say (an external pane's
// cache). Neither is a thing a maker can author, inspect or keep.
//
// So this is that value:
//
//   PaneDefinition     a durable NAME, and a list of REGIONS in authored order, plus the
//                      identity mint that keeps a deleted region's id from being reused.
//   TextRegion         one region: a stable definition-local id, its KIND (one is admitted,
//                      `text`), an authored place and extent on the fine lattice RELATIVE
//                      TO THE PANE'S INTERIOR, and its content.
//
// ---- What this value IS NOT, and the sentence that keeps it honest ----------------------
//
// THIS IS ONE WAY A PANE CAN BE IMPLEMENTED. IT IS NOT THE ONTOLOGY OF PANE.
//
// A built-in pane's interior is still code. A provider pane's interior is still its own,
// behind its own seam. Neither has to be converted into this representation before
// Workshop can describe it, and nothing here implies it should be: a later tool that works
// on an arbitrary pane asks that pane what regions it honestly exposes, and THIS kind of
// pane answers with the list below because the list is what it is made of. The limitation
// to one region kind, and to static text, is a limitation of this first implementation and
// not a law about panes.
//
// ---- The lattice and the frame ------------------------------------------------------------
//
// A REGION IS PLACED RELATIVE TO THE PANE'S INTERIOR -- the rectangle inside the pane's
// chrome that the ordinary pane path offers at presentation (`pane_inside`, screen.hpp) --
// and never relative to the canvas, the screen, or any workspace. That is the whole reason
// a definition needs no exterior size: move the pane, shrink it, open it on the other face,
// and the region resolves freshly against the interior it is offered, exactly as a pane
// resolves freshly against the screen it is offered.
//
// ITS GEOMETRY IS SUB-UNITS OF A CANVAS CELL (`surface::kCellSubs`), the same fine lattice
// pane arrangement has stood on since WUX-2, so a maker at a window authors a pixel of
// place and a maker at a terminal authors a cell, and both are ONE number here. What a face
// makes of that number -- how many rows of type it sets, whether it falls back to the cell
// projection, which pixel a fractional edge lands on -- is presentation, resolved at every
// paint and stored nowhere.
//
// ---- What this value cannot carry, deliberately -------------------------------------------
//
// No pixel, no cell count, no font metric, no row or column capacity, no viewport, no
// canvas coordinate: every one of those is a medium's answer and would be a stale claim
// the moment it was written down. And no callback, no role, no key binding, no path, no
// callable, no grant, no provider reference, no operator, no Source, no message
// destination: loading a definition presents; it may not act. There is no field on which
// any of those could be spelled, which is the enforcement -- a representation that cannot
// say "send" cannot be made to send by a file.
//
// THE LIST IS THE SHAPE EVEN THOUGH THE FIRST FLOOR EXPOSES ONE REGION, so a second text
// region is one more row and not a representation rewrite. What is NOT here -- anchors,
// docking, fill, constraints, nesting, clipping policy, a second kind, a widget set -- is
// absent because nothing has earned it, and every one of them would be a framework arriving
// ahead of its consumer.

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

/// How long a maker-made pane's name may be. THIRTY-TWO BYTES, the same number a runtime
/// pane's display name is admitted at (`kMaxPaneNameLen`, setup.hpp) and for the same
/// measured reason: this name is also what the picker lists, in a column of twelve cells
/// that marks its own cut. It is comfortably under the pane-key bound the durable
/// reference imposes (`kMaxPaneKeyLen`), which setup.hpp asserts at compile time.
inline constexpr std::size_t kMaxMakerPaneNameLen = 32;

/// How many regions one definition may carry. Sixteen bounds a file at a few kilobytes
/// and is far above what one text-only pane has any use for; it is a bound on what a
/// stranger's file can push into a live definition, not a statement about layout.
inline constexpr std::size_t kMaxRegions = 16;

/// How many bytes a text region's content may hold. One line of prose, cut and marked by
/// the presentation wherever the region is narrower.
inline constexpr std::size_t kMaxRegionTextLen = 256;

/// THE AUTHORED LATTICE'S WALLS, in sub-units -- the document's cell ceiling expressed at
/// the resolution a region carries. A coordinate is `0..kRegionSubMax`, an extent is
/// `1..kRegionSubMax`: a region may be authored past its pane's interior (the interior
/// clips it at presentation, exactly as the canvas clips an off-room pane) and may be
/// authored finer than a cell (a fine rectangle is honest INTENT; what a face can show of
/// it is that face's answer).
inline constexpr std::int64_t kRegionSubMax = doc::kMaxCells * surface::kCellSubs;

/// The first identity the mint hands out. Never 0, so an absent identity has a number no
/// region can carry.
inline constexpr std::int64_t kFirstRegionId = 1;

// ---- The value ------------------------------------------------------------------------------

/// ONE REGION OF A MAKER-MADE PANE'S INTERIOR.
///
/// `id` is definition-local and stable: minted once, never reused after a deletion, so a
/// row that names region 3 names the same region until that region is gone. `kind` is the
/// closed set above. `x`/`y`/`w`/`h` are sub-units relative to the pane's INTERIOR origin.
/// `text` is the content of a `text` region -- printable ASCII, the shipped media's honest
/// reach, and empty is legal (a fresh region has nothing to say yet).
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
/// mint. `regions` is paint order and, one day, hit order -- the document's own rule about
/// its objects, and the reason the list is never sorted.
///
/// AN EMPTY NAME IS "NO DEFINITION". A definition always has a name -- the name IS the
/// durable identity a `PaneRef` carries -- so a value with none is the absence of one, and
/// `open()` is that test written once.
struct PaneDefinition {
    std::string name;
    std::vector<TextRegion> regions;
    std::int64_t next_id = kFirstRegionId;

    bool open() const noexcept { return !name.empty(); }

    friend bool operator==(const PaneDefinition&, const PaneDefinition&) = default;
};

// ---- The law: what this application accepts as a definition ----------------------------------
//
// ONE LAW, REACHED TWO WAYS -- the setup's discipline, applied to the ninth durable artifact.
// A name typed at the Pane Creator's prompt and a name read out of a file meet the SAME
// function; a region authored through a typed row and a region read out of a file meet the
// SAME function.

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
//
// `New Pane` seeds ONE empty text region, because the first floor's completion is a pane
// with one region and a pane with none would be a thing a maker cannot type into. The
// numbers are the developer's default for a region nobody has placed yet -- and unlike a
// pane's default place they are AUTHORED the moment the region exists: a region has no
// `default` mode, so these are ordinary values the maker reads and retypes.
//
// TWENTY-FOUR BY TWO CELLS, at the interior's origin. Two cells tall so the shipped face sets
// one row of type in it (one cell holds no row of an 18-pixel line, HD-5's table) and a
// terminal shows two; twenty-four wide so a short sentence fits on either face at the
// narrowest stack pane this composition lays out.
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
///
/// ONE OPEN DEFINITION, SESSION-OWNED, PRESENTATION OWNS NONE OF IT -- the source editor's
/// lifecycle law, inherited whole. The pane that presents this on the desk is a
/// presentation; removing it, covering it, moving it or switching layouts touches the
/// presentation and leaves every byte here standing.
///
/// DIRTY DERIVES BY COMPARISON, never by a flag: `saved` is the definition as it is on
/// disk (or the absence of one, an empty value, for a pane never yet written), so "does
/// the file hold what the screen shows" is one comparison that cannot drift from the thing
/// it describes. A fresh pane that was never saved is dirty by exactly that arithmetic --
/// the file holds nothing and the screen holds a pane.
///
/// `path` IS WHERE A SAVE GOES AND WHERE THE OPEN CAME FROM. Empty means no file was
/// chosen for this run: the pane is still made, still edited and still presented, and the
/// save door refuses in words.
struct MakerPane {
    std::string path;
    PaneDefinition definition;
    PaneDefinition saved;

    bool open() const noexcept { return definition.open(); }
    bool dirty() const noexcept { return definition != saved; }
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DEFINITION_HPP
