// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_DOCUMENT_HPP
#define ZENGINE_WORKSHOP_DOCUMENT_HPP

// The document's semantic surface: every operation a maker's edit can go
// through, and every one of them able to refuse.
//
// These are free functions over the plain WorkshopDoc rather than methods on a
// wrapper, and the reason is the substrate's, not taste: ZEN_SHAPE needs the
// state's members public (see vocabulary.hpp), so a wrapper could not actually
// prevent a raw write -- it could only make the raw write look further away. So
// the honest arrangement is the visible one: the data is plain and open, and
// these functions are what the application uses, every time, with no second
// path anywhere in the package. The refusals below are therefore real
// properties of the OPERATIONS. They are not claims about the data.
//
// AUTHORED VERSUS RESOLVED lives here too, as one function -- resolve(). An
// authored extent and the cell count the current workspace makes of it are two
// different facts, and the only place they meet is a call with the workspace
// width in it. Nothing caches the result: a resolved value that outlived the
// workspace it was resolved against would be exactly the stale-number lie the
// distinction exists to prevent.

#include "property.hpp"
#include "vocabulary.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace zengine::workshop::doc {

/// The limits the operations enforce. Small, named, and in one place so a
/// refusal message and the check that produces it cannot drift apart.
inline constexpr std::int64_t kMinCells = 1;    ///< a rectangle with no area is not one
inline constexpr std::int64_t kMaxCells = 4096; ///< an authored size, not a workspace size
inline constexpr std::size_t kMaxNameLen = 32;  ///< a label, not a document

/// Find an authored object by identity. Null when nothing carries that id --
/// which is a normal answer, not an error: a selection can outlive its object.
inline const WorkshopRect* find(const WorkshopDoc& d, std::int64_t id) {
    for (const WorkshopRect& r : d.rects) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

inline WorkshopRect* find_mut(WorkshopDoc& d, std::int64_t id) {
    for (WorkshopRect& r : d.rects) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

/// Resolve an authored extent against a workspace extent, in cells.
///
/// The ONE place authored intent becomes a number, and it takes the workspace as
/// an argument precisely so it cannot be called without one. A percent extent
/// resolves to at least kMinCells: a rectangle a maker authored is a rectangle
/// they meant to see, and rounding it out of existence in a narrow workspace
/// would be the tool losing their work to arithmetic.
inline std::int64_t resolve(const WorkshopExtent& e, std::int64_t workspace) {
    if (e.mode != kExtentPercent) {
        return e.amount;
    }
    const std::int64_t cells = workspace * e.amount / 100;
    return cells < kMinCells ? kMinCells : cells;
}

/// Add an authored object, minting its identity. Returns the new id.
///
/// Deliberately not refusing: this is the one operation with no maker-supplied
/// value to judge (W-0 has no create-with-arguments gesture), so a Written
/// return would be a promise of a refusal that cannot happen. The invariants
/// still hold, because the values come from the caller's own defaults and every
/// LATER change to them goes through the operations below.
inline std::int64_t add(WorkshopDoc& d, std::string name, std::int64_t x, std::int64_t y,
                       WorkshopExtent width, WorkshopExtent height) {
    WorkshopRect r;
    r.id = d.next_id++;
    r.name = std::move(name);
    r.x = x;
    r.y = y;
    r.width = width;
    r.height = height;
    d.rects.push_back(std::move(r));
    return d.rects.back().id;
}

/// Rename: a label may be anything a maker can type, except nothing, and except
/// so long it stops being a label. Note what is NOT refused -- a duplicate. Two
/// objects may share a name, because the name is not the identity (see
/// WorkshopRect), and refusing a duplicate here would quietly make it one.
inline Written rename(WorkshopDoc& d, std::int64_t id, std::string name) {
    WorkshopRect* r = find_mut(d, id);
    if (r == nullptr) {
        return Written::no("no such object");
    }
    if (name.empty()) {
        return Written::no("a name cannot be empty");
    }
    if (name.size() > kMaxNameLen) {
        return Written::no("a name is at most " + std::to_string(kMaxNameLen) + " characters");
    }
    r->name = std::move(name);
    return Written::ok();
}

/// The extent check, shared by width and height so the two cannot disagree
/// about what a legal extent is -- the same argument TextForm<WorkshopExtent>
/// makes one layer up about conversion.
inline Written check_extent(const WorkshopExtent& e) {
    if (e.mode == kExtentPercent) {
        if (e.amount < 1 || e.amount > 100) {
            return Written::no("a share is 1% to 100%");
        }
        return Written::ok();
    }
    if (e.mode != kExtentCells) {
        return Written::no("an extent is either cells or a share");
    }
    if (e.amount < kMinCells) {
        return Written::no("at least " + std::to_string(kMinCells) + " cell");
    }
    if (e.amount > kMaxCells) {
        return Written::no("at most " + std::to_string(kMaxCells) + " cells");
    }
    return Written::ok();
}

inline Written set_width(WorkshopDoc& d, std::int64_t id, WorkshopExtent e) {
    WorkshopRect* r = find_mut(d, id);
    if (r == nullptr) {
        return Written::no("no such object");
    }
    const Written check = check_extent(e);
    if (!check.accepted) {
        return check;
    }
    r->width = e;
    return Written::ok();
}

inline Written set_height(WorkshopDoc& d, std::int64_t id, WorkshopExtent e) {
    WorkshopRect* r = find_mut(d, id);
    if (r == nullptr) {
        return Written::no("no such object");
    }
    const Written check = check_extent(e);
    if (!check.accepted) {
        return check;
    }
    r->height = e;
    return Written::ok();
}

/// Position: a rectangle may sit anywhere in the workspace, including partly off
/// its right or bottom edge (the canvas clips, and a maker dragging something
/// half out of view has not made a mistake). Negative is refused, because the
/// workspace has no cells there at all -- the object would be authored somewhere
/// that does not exist.
inline Written set_x(WorkshopDoc& d, std::int64_t id, std::int64_t x) {
    WorkshopRect* r = find_mut(d, id);
    if (r == nullptr) {
        return Written::no("no such object");
    }
    if (x < 0) {
        return Written::no("the workspace starts at 0");
    }
    r->x = x;
    return Written::ok();
}

inline Written set_y(WorkshopDoc& d, std::int64_t id, std::int64_t y) {
    WorkshopRect* r = find_mut(d, id);
    if (r == nullptr) {
        return Written::no("no such object");
    }
    if (y < 0) {
        return Written::no("the workspace starts at 0");
    }
    r->y = y;
    return Written::ok();
}

/// Which authored object occupies a workspace cell -- 0 for none. The TOPMOST
/// wins, meaning the last one painted, so the answer agrees with what the maker
/// can actually see (paint order is list order; see SurfaceCanvas).
///
/// This is hit testing against the REAL authored objects: the geometry it tests
/// is resolve()'d from the same authored extents the inspector shows, not a
/// separate copy kept for picking. So there is no second truth to fall out of
/// step -- but note honestly what that costs: it works because W-0's objects
/// carry their own position, and it is not an answer to the general question
/// (see the pointer seam in the report -- nothing in Zengine can currently be
/// asked "what visual object is under this pointer", and this function is
/// Workshop answering it about its own material only).
inline std::int64_t pick(const WorkshopDoc& d, std::int64_t cx, std::int64_t cy,
                         std::int64_t workspace_w, std::int64_t workspace_h) {
    std::int64_t hit = 0;
    for (const WorkshopRect& r : d.rects) {
        const std::int64_t w = resolve(r.width, workspace_w);
        const std::int64_t h = resolve(r.height, workspace_h);
        if (cx >= r.x && cx < r.x + w && cy >= r.y && cy < r.y + h) {
            hit = r.id;
        }
    }
    return hit;
}

// ---- The properties of one authored object -------------------------------------------
//
// The bindings. Each is one call, and each closes over the document and the
// identity -- never over a WorkshopRect*, which would dangle the moment the
// vector reallocated, and never over a member address, which would skip the
// refusal. `id` is what survives, which is the second thing identity is for.

inline Property<std::string> name_of(WorkshopDoc& d, std::int64_t id) {
    return Property<std::string>(
        [&d, id] {
            const WorkshopRect* r = find(d, id);
            return r == nullptr ? std::string() : r->name;
        },
        [&d, id](std::string v) { return rename(d, id, std::move(v)); });
}

inline Property<std::int64_t> x_of(WorkshopDoc& d, std::int64_t id) {
    return Property<std::int64_t>(
        [&d, id] {
            const WorkshopRect* r = find(d, id);
            return r == nullptr ? 0 : r->x;
        },
        [&d, id](std::int64_t v) { return set_x(d, id, v); });
}

inline Property<std::int64_t> y_of(WorkshopDoc& d, std::int64_t id) {
    return Property<std::int64_t>(
        [&d, id] {
            const WorkshopRect* r = find(d, id);
            return r == nullptr ? 0 : r->y;
        },
        [&d, id](std::int64_t v) { return set_y(d, id, v); });
}

inline Property<WorkshopExtent> width_of(WorkshopDoc& d, std::int64_t id) {
    return Property<WorkshopExtent>(
        [&d, id] {
            const WorkshopRect* r = find(d, id);
            return r == nullptr ? WorkshopExtent{} : r->width;
        },
        [&d, id](WorkshopExtent v) { return set_width(d, id, v); });
}

inline Property<WorkshopExtent> height_of(WorkshopDoc& d, std::int64_t id) {
    return Property<WorkshopExtent>(
        [&d, id] {
            const WorkshopRect* r = find(d, id);
            return r == nullptr ? WorkshopExtent{} : r->height;
        },
        [&d, id](WorkshopExtent v) { return set_height(d, id, v); });
}

} // namespace zengine::workshop::doc

#endif // ZENGINE_WORKSHOP_DOCUMENT_HPP
