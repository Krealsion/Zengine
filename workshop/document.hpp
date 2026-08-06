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
// AUTHORED VERSUS RESOLVED USED TO LIVE HERE, as a private resolve(), with a
// private pick() beside it testing geometry it worked out for itself. W-1 moved
// both to the UI package (ui/layout.hpp), and the split of responsibility that
// came out of the move is the useful part:
//
//   ui::resolve / ui::hit   HOW authored intent becomes geometry, and what is
//                           under a cell. The same answer for every application.
//   doc::check_extent       WHAT THIS DOCUMENT considers a legal extent. Its
//                           bounds are policy (at most 4096 cells; a share is
//                           1..100), and policy is exactly what does not
//                           generalise -- a different application may hold a
//                           different opinion and still resolve identically.
//
// So Workshop still owns every refusal, and owns no geometry at all.

#include "property.hpp"
#include "vocabulary.hpp"

#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace zengine::workshop::doc {

/// The limits the operations enforce. Small, named, and in one place so a
/// refusal message and the check that produces it cannot drift apart.
///
/// The FLOOR is not one of them: `ui::kMinCells` is the UI package's, because it
/// is the same number resolution already refuses to round below, and two
/// spellings of one minimum is how a check and the thing it checks come to
/// disagree.
inline constexpr std::int64_t kMaxCells = 4096; ///< an authored size, not a workspace size
inline constexpr std::size_t kMaxNameLen = 32;  ///< a label, not a document

/// Find an authored object by identity. Null when nothing carries that id --
/// which is a normal answer, not an error: a selection can outlive its object.
inline const ui::Element* find(const WorkshopDoc& d, std::int64_t id) {
    for (const ui::Element& e : d.elements) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

inline ui::Element* find_mut(WorkshopDoc& d, std::int64_t id) {
    for (ui::Element& e : d.elements) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

/// Add an authored object, minting its identity. Returns the new id.
///
/// Deliberately not refusing: this is the one operation with no maker-supplied
/// value to judge (W-0 has no create-with-arguments gesture), so a Written
/// return would be a promise of a refusal that cannot happen. The invariants
/// still hold, because the values come from the caller's own defaults and every
/// LATER change to them goes through the operations below.
inline std::int64_t add(WorkshopDoc& d, std::string label, std::int64_t x, std::int64_t y,
                        ui::Extent width, ui::Extent height) {
    ui::Element e;
    e.id = d.next_id++;
    e.label = std::move(label);
    e.x = x;
    e.y = y;
    e.width = width;
    e.height = height;
    d.elements.push_back(std::move(e));
    return d.elements.back().id;
}

/// Rename: a label may be anything a maker can type, except nothing, and except
/// so long it stops being a label. Note what is NOT refused -- a duplicate. Two
/// objects may share a label, because the label is not the identity (see
/// ui::Element), and refusing a duplicate here would quietly make it one.
inline Written rename(WorkshopDoc& d, std::int64_t id, std::string label) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    if (label.empty()) {
        return Written::no("a name cannot be empty");
    }
    if (label.size() > kMaxNameLen) {
        return Written::no("a name is at most " + std::to_string(kMaxNameLen) + " characters");
    }
    e->label = std::move(label);
    return Written::ok();
}

/// The extent check, shared by width and height so the two cannot disagree
/// about what a legal extent is -- the same argument TextForm<ui::Extent>
/// makes one layer up about conversion.
///
/// This is Workshop's POLICY over a shared vocabulary, and the division is worth
/// naming: `ui::Extent` says what an extent can BE, this says what this document
/// will ACCEPT, and `ui::resolve_extent` says what a viewport makes of one --
/// which it must do for values this function would have refused, because a poke
/// writes the state directly (see vocabulary.hpp).
inline Written check_extent(const ui::Extent& e) {
    if (e.mode == ui::kExtentPercent) {
        if (e.amount < 1 || e.amount > 100) {
            return Written::no("a share is 1% to 100%");
        }
        return Written::ok();
    }
    if (e.mode != ui::kExtentCells) {
        return Written::no("an extent is either cells or a share");
    }
    if (e.amount < ui::kMinCells) {
        return Written::no("at least " + std::to_string(ui::kMinCells) + " cell");
    }
    if (e.amount > kMaxCells) {
        return Written::no("at most " + std::to_string(kMaxCells) + " cells");
    }
    return Written::ok();
}

inline Written set_width(WorkshopDoc& d, std::int64_t id, ui::Extent value) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    const Written check = check_extent(value);
    if (!check.accepted) {
        return check;
    }
    e->width = value;
    return Written::ok();
}

inline Written set_height(WorkshopDoc& d, std::int64_t id, ui::Extent value) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    const Written check = check_extent(value);
    if (!check.accepted) {
        return check;
    }
    e->height = value;
    return Written::ok();
}

/// Position: a rectangle may sit anywhere in the workspace, including partly off
/// its right or bottom edge (the canvas clips, and a maker dragging something
/// half out of view has not made a mistake). Negative is refused, because the
/// workspace has no cells there at all -- the object would be authored somewhere
/// that does not exist.
inline Written set_x(WorkshopDoc& d, std::int64_t id, std::int64_t x) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    if (x < 0) {
        return Written::no("the workspace starts at 0");
    }
    e->x = x;
    return Written::ok();
}

inline Written set_y(WorkshopDoc& d, std::int64_t id, std::int64_t y) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    if (y < 0) {
        return Written::no("the workspace starts at 0");
    }
    e->y = y;
    return Written::ok();
}

// ---- The properties of one authored object -------------------------------------------
//
// The bindings. Each is one call, and each closes over the document and the
// identity -- never over a ui::Element*, which would dangle the moment the
// vector reallocated, and never over a member address, which would skip the
// refusal. `id` is what survives, which is the second thing identity is for --
// and it is the same reason ui::Placed carries an id rather than a pointer.

inline Property<std::string> name_of(WorkshopDoc& d, std::int64_t id) {
    return Property<std::string>(
        [&d, id] {
            const ui::Element* e = find(d, id);
            return e == nullptr ? std::string() : e->label;
        },
        [&d, id](std::string v) { return rename(d, id, std::move(v)); });
}

inline Property<std::int64_t> x_of(WorkshopDoc& d, std::int64_t id) {
    return Property<std::int64_t>(
        [&d, id] {
            const ui::Element* e = find(d, id);
            return e == nullptr ? 0 : e->x;
        },
        [&d, id](std::int64_t v) { return set_x(d, id, v); });
}

inline Property<std::int64_t> y_of(WorkshopDoc& d, std::int64_t id) {
    return Property<std::int64_t>(
        [&d, id] {
            const ui::Element* e = find(d, id);
            return e == nullptr ? 0 : e->y;
        },
        [&d, id](std::int64_t v) { return set_y(d, id, v); });
}

inline Property<ui::Extent> width_of(WorkshopDoc& d, std::int64_t id) {
    return Property<ui::Extent>(
        [&d, id] {
            const ui::Element* e = find(d, id);
            return e == nullptr ? ui::Extent{} : e->width;
        },
        [&d, id](ui::Extent v) { return set_width(d, id, v); });
}

inline Property<ui::Extent> height_of(WorkshopDoc& d, std::int64_t id) {
    return Property<ui::Extent>(
        [&d, id] {
            const ui::Element* e = find(d, id);
            return e == nullptr ? ui::Extent{} : e->height;
        },
        [&d, id](ui::Extent v) { return set_height(d, id, v); });
}

} // namespace zengine::workshop::doc

#endif // ZENGINE_WORKSHOP_DOCUMENT_HPP
