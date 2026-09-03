// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_DOCUMENT_HPP
#define ZENGINE_WORKSHOP_DOCUMENT_HPP

// The document's semantic surface: every operation a maker's edit can go
// through, and every one of them able to refuse.
// Workshop law: agents/workshop/document.md (+2 registers; agents/workshop.md routes)



#include "property.hpp"
#include "vocabulary.hpp"

#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop::doc {

/// The limits the operations enforce. Small, named, and in one place so a
/// refusal message and the check that produces it cannot drift apart.
// WL-DOC-07 -- agents/workshop/document.md
inline constexpr std::int64_t kMaxCells = 4096; ///< an authored size, not a workspace size

/// HOW LONG A MAKER'S NAME FOR AN OBJECT MAY BE, in bytes.
// WL-DOC-03 -- agents/workshop/document.md
inline constexpr std::size_t kMaxNameLen = 64;

/// The last identity this document could ever mint.
// WL-DOC-01 -- agents/workshop/document.md
inline constexpr std::int64_t kMaxIdentity = (std::numeric_limits<std::int64_t>::max)();

/// The first identity a document may carry. Zero is reserved: the session
/// spells "nothing is selected" as 0, so an object numbered 0 could never be
/// told apart from no object at all.
inline constexpr std::int64_t kFirstIdentity = 1;

/// What a new object is, before a maker has said anything about it.
// WL-DOC-10 -- agents/workshop/document.md
inline constexpr const char* kNewLabel = "panel";
inline constexpr std::int64_t kNewX = 1; ///< inside the workspace, with room for the ring
inline constexpr std::int64_t kNewY = 1;
inline constexpr std::int64_t kNewWidthCells = 12;
inline constexpr std::int64_t kNewHeightCells = 4;

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

/// Whether this document can still mint.
// WL-DOC-01 -- agents/workshop/document.md
inline bool can_mint(const WorkshopDoc& d) {
    return d.next_id >= kFirstIdentity && d.next_id < kMaxIdentity;
}

/// Add an authored object, minting its identity. Returns the new id, or 0 when
/// this document has no identity left to give.
// WL-DOC-01 -- agents/workshop/document.md
inline std::int64_t add(WorkshopDoc& d, std::string label, std::int64_t x, std::int64_t y,
                        ui::Extent width, ui::Extent height) {
    if (!can_mint(d)) {
        return 0;
    }
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

/// Mint one new authored object with this document's defaults. Returns its
/// identity.
// WL-DOC-01, WL-DOC-10 -- agents/workshop/document.md
inline std::int64_t add_default(WorkshopDoc& d) {
    return add(d, kNewLabel, kNewX, kNewY, ui::Extent{ui::kExtentCells, kNewWidthCells},
               ui::Extent{ui::kExtentCells, kNewHeightCells});
}

/// Everything that measures against `id`, in document order.
// WL-DOC-11 -- agents/workshop/document.md
inline std::vector<std::int64_t> dependents_of(const WorkshopDoc& d, std::int64_t id) {
    std::vector<std::int64_t> who;
    if (id == ui::kRootContext) {
        return who; // the root is not an object and nothing can be deleted out from under it
    }
    for (const ui::Element& e : d.elements) {
        if (e.context == id) {
            who.push_back(e.id);
        }
    }
    return who;
}

/// A list of identities as a maker reads them: `#2`, or `#2 and #5`, or
/// `#2, #5 and 3 others` — because a refusal that names two dozen objects is a
/// refusal nobody reads.
inline std::string identities_text(const std::vector<std::int64_t>& ids) {
    constexpr std::size_t kShow = 2;
    std::string out;
    const std::size_t shown = ids.size() < kShow ? ids.size() : kShow;
    for (std::size_t i = 0; i < shown; ++i) {
        if (i > 0) {
            out += (shown == ids.size() && i + 1 == shown) ? " and " : ", ";
        }
        out += "#" + std::to_string(ids[i]);
    }
    if (ids.size() > shown) {
        out += " and " + std::to_string(ids.size() - shown) + " others";
    }
    return out;
}

/// Remove one authored object BY IDENTITY.
// WL-DOC-10 -- agents/workshop/document.md; WL-CTRL-03 -- agents/workshop/info-controls.md
inline Written remove(WorkshopDoc& d, std::int64_t id) {
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        if (d.elements[i].id == id) {
            const std::vector<std::int64_t> who = dependents_of(d, id);
            if (!who.empty()) {
                return Written::no(identities_text(who) +
                                   (who.size() == 1 ? " takes context from #" : " take context from #") +
                                   std::to_string(id) + " -- change or delete " +
                                   (who.size() == 1 ? "it" : "them") + " first");
            }
            d.elements.erase(d.elements.begin() + static_cast<std::ptrdiff_t>(i));
            return Written::ok();
        }
    }
    return Written::no("no such object");
}

/// What this document considers a legal label: anything a maker can type,
/// except nothing, and except so long it stops being a label.
// WL-DOC-03 -- agents/workshop/document.md
inline Written check_name(const std::string& label) {
    if (label.empty()) {
        return Written::no("a name cannot be empty");
    }
    if (label.size() > kMaxNameLen) {
        return Written::no("a name is at most " + std::to_string(kMaxNameLen) + " characters");
    }
    return Written::ok();
}

inline Written rename(WorkshopDoc& d, std::int64_t id, std::string label) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    const Written legal = check_name(label);
    if (!legal.accepted) {
        return legal;
    }
    e->label = std::move(label);
    return Written::ok();
}

/// The extent check, shared by width and height so the two cannot disagree
/// about what a legal extent is.
// WL-DOC-07, WL-DOC-08 -- agents/workshop/document.md
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

/// Author a size. THE ONE PLACE an extent is written in this package.
// WL-DOC-07 -- agents/workshop/document.md
inline Written resize(WorkshopDoc& d, std::int64_t id, ui::Extent width, ui::Extent height) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    const Written cw = check_extent(width);
    if (!cw.accepted) {
        return cw;
    }
    const Written ch = check_extent(height);
    if (!ch.accepted) {
        return ch;
    }
    e->width = width;
    e->height = height;
    return Written::ok();
}

inline Written set_width(WorkshopDoc& d, std::int64_t id, ui::Extent value) {
    const ui::Element* e = find(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    return resize(d, id, value, e->height);
}

inline Written set_height(WorkshopDoc& d, std::int64_t id, ui::Extent value) {
    const ui::Element* e = find(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    return resize(d, id, e->width, value);
}

/// The first cell the workspace has.
// WL-DOC-06 -- agents/workshop/document.md
inline constexpr std::int64_t kFirstCell = 0;

// WL-DOC-06 -- agents/workshop/document.md
inline Written check_coord(std::int64_t v, std::int64_t context) {
    if (context == ui::kRootContext && v < kFirstCell) {
        return Written::no("the workspace starts at 0");
    }
    return Written::ok();
}

/// Author a position. THE ONE PLACE a position is written in this package.
// WL-DOC-06 -- agents/workshop/document.md
inline Written move(WorkshopDoc& d, std::int64_t id, std::int64_t x, std::int64_t y) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    // The object's OWN context decides which coordinate domain this proposal is
    // in. It is read from the element rather than passed in, so a caller cannot
    // supply a domain the object is not actually in.
    const Written cx = check_coord(x, e->context);
    if (!cx.accepted) {
        return cx;
    }
    const Written cy = check_coord(y, e->context);
    if (!cy.accepted) {
        return cy;
    }
    e->x = x;
    e->y = y;
    return Written::ok();
}

inline Written set_x(WorkshopDoc& d, std::int64_t id, std::int64_t x) {
    const ui::Element* e = find(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    return move(d, id, x, e->y);
}

inline Written set_y(WorkshopDoc& d, std::int64_t id, std::int64_t y) {
    const ui::Element* e = find(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    return move(d, id, e->x, y);
}

// ---- The one relationship, and the law it lives under ----------------------------------
// WL-DOC-11 -- agents/workshop/document.md

/// How a broken chain reads: `#7 -> #9 -> #7`, cut off before it becomes wall.
// WL-DOC-11 -- agents/workshop/document.md
inline constexpr std::size_t kMaxChainChars = 20;

inline std::string chain_text(const std::vector<std::int64_t>& ids) {
    std::string out;
    std::size_t shown = 0;
    for (const std::int64_t id : ids) {
        const std::string next = (shown == 0 ? "" : " -> ") + ("#" + std::to_string(id));
        if (out.size() + next.size() > kMaxChainChars) {
            break;
        }
        out += next;
        ++shown;
    }
    if (shown < ids.size()) {
        out += (shown == 0 ? "..." : " -> ...");
    }
    return out;
}

/// Whether `candidate` may supply `id`'s frame — the ONE STATEMENT of the
/// relationship law, asked about one proposal.
// WL-DOC-11 -- agents/workshop/document.md
inline Written check_context(const std::vector<ui::Element>& elements, std::int64_t id,
                             std::int64_t candidate) {
    if (candidate == ui::kRootContext) {
        return Written::ok(); // the root is always there; that is what makes it the root
    }
    if (candidate == id) {
        return Written::no("#" + std::to_string(id) +
                           " cannot take its context from itself");
    }
    const ui::ById index(elements);
    if (index.find(candidate) == ui::ById::npos) {
        return Written::no("no object #" + std::to_string(candidate) +
                           " to take context from");
    }
    // No memo: the COMPLETE chain is the answer here, and a memo shortens it.
    const ui::ContextWalk walk = ui::walk_context(elements, index, candidate);
    if (walk.end == ui::ContextEnd::Missing) {
        return Written::no("#" + std::to_string(candidate) + " takes context from #" +
                           std::to_string(walk.at) + ", which is not an object");
    }
    if (walk.end == ui::ContextEnd::Cycle || walk.passes_through(id)) {
        std::vector<std::int64_t> loop;
        loop.push_back(id);
        for (const std::int64_t step : walk.chain) {
            loop.push_back(step);
            if (step == id) {
                break;
            }
        }
        if (loop.back() != id) {
            loop.push_back(walk.at);
        }
        return Written::no("#" + std::to_string(id) + " cannot use #" +
                           std::to_string(candidate) + " as context: a cycle (" +
                           chain_text(loop) + ")");
    }
    return Written::ok();
}

/// Author what an object's values are measured against. THE ONE PLACE a context
/// is written in this package.
// WL-DOC-11 -- agents/workshop/document.md
inline Written set_context(WorkshopDoc& d, std::int64_t id, std::int64_t candidate) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    const Written legal = check_context(d.elements, id, candidate);
    if (!legal.accepted) {
        return legal;
    }
    const Written cx = check_coord(e->x, candidate);
    if (!cx.accepted) {
        return Written::no("#" + std::to_string(id) + " is at " + std::to_string(e->x) + "," +
                           std::to_string(e->y) + " -- " + cx.refusal);
    }
    const Written cy = check_coord(e->y, candidate);
    if (!cy.accepted) {
        return Written::no("#" + std::to_string(id) + " is at " + std::to_string(e->x) + "," +
                           std::to_string(e->y) + " -- " + cy.refusal);
    }
    e->context = candidate;
    return Written::ok();
}

// ---- A whole document, and the one door that admits one -------------------------------
// WL-DOC-11 -- agents/workshop/document.md; WL-DOC-14 -- agents/workshop/document-file.md

/// Is this a document? Refuses with the FIRST reason, naming the object it is
/// about -- a maker reading "#3: the workspace starts at 0" knows where to look,
/// which "invalid document" does not tell them.
inline Written check_document(const WorkshopDoc& d) {
    if (d.next_id < kFirstIdentity) {
        return Written::no("the next identity to mint is at least " +
                           std::to_string(kFirstIdentity));
    }
    for (const ui::Element& e : d.elements) {
        const std::string who = "#" + std::to_string(e.id) + ": ";
        if (e.id < kFirstIdentity) {
            return Written::no("an identity is at least " + std::to_string(kFirstIdentity));
        }
        if (e.id >= d.next_id) {
            // The mint never rewinds, so it can never be BEHIND. A document
            // claiming otherwise would hand its next created object an identity
            // one of its existing objects already carries.
            return Written::no(who + "an identity is below the next one to mint (" +
                               std::to_string(d.next_id) + ")");
        }
        const Written name = check_name(e.label);
        if (!name.accepted) {
            return Written::no(who + name.refusal);
        }
        const Written cx = check_coord(e.x, e.context);
        if (!cx.accepted) {
            return Written::no(who + cx.refusal);
        }
        const Written cy = check_coord(e.y, e.context);
        if (!cy.accepted) {
            return Written::no(who + cy.refusal);
        }
        const Written cw = check_extent(e.width);
        if (!cw.accepted) {
            return Written::no(who + "width: " + cw.refusal);
        }
        const Written ch = check_extent(e.height);
        if (!ch.accepted) {
            return Written::no(who + "height: " + ch.refusal);
        }
    }
    // Distinctness before the relationships, and by sorting a copy rather than
    // by a nested loop: the object count is bounded by the decoder's
    // materialization budget and not by anything a screen can show, so the
    // quadratic form is a cost a hostile file gets to choose.
    //
    // It goes FIRST of the two because a relationship names an identity, and
    // "the object called #4" is not a question with one answer while two objects
    // are both called #4.
    std::vector<std::int64_t> ids;
    ids.reserve(d.elements.size());
    for (const ui::Element& e : d.elements) {
        ids.push_back(e.id);
    }
    std::sort(ids.begin(), ids.end());
    const auto twice = std::adjacent_find(ids.begin(), ids.end());
    if (twice != ids.end()) {
        return Written::no("#" + std::to_string(*twice) +
                           ": two objects cannot share one identity");
    }
    // The relationships. One pass over the whole document rather than one walk
    // per object: `settled` remembers which objects have already been proven to
    // reach the workspace, so an element is visited once and a thousand-deep
    // chain costs a thousand steps rather than a thousand squared. A file gets
    // to choose the object count; it does not also get to choose the exponent.
    const ui::ById index(d.elements);
    std::vector<char> settled(d.elements.size(), 0);
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        const ui::Element& e = d.elements[i];
        const std::string who = "#" + std::to_string(e.id) + ": ";
        const ui::ContextWalk walk = ui::walk_context(d.elements, index, e.context, &settled);
        if (walk.end == ui::ContextEnd::Missing) {
            return Written::no(who + "no object #" + std::to_string(walk.at) +
                               " to take context from");
        }
        if (walk.end == ui::ContextEnd::Cycle) {
            return Written::no(who + "its context never reaches the workspace (" +
                               chain_text(walk.chain) + ")");
        }
        settled[i] = 1; // its own chain reaches the workspace, so it is settled too
    }
    return Written::ok();
}

/// Replace this document with another one, KEEPING that one's identities.
// WL-DOC-14 -- agents/workshop/document-file.md
inline Written restore(WorkshopDoc& live, WorkshopDoc candidate) {
    const Written legal = check_document(candidate);
    if (!legal.accepted) {
        return legal;
    }
    live = std::move(candidate);
    return Written::ok();
}

// ---- The properties of one authored object -------------------------------------------
// WL-DOC-02 -- agents/workshop/document.md

inline Property<std::string> name_of(WorkshopDoc& d, std::int64_t id) {
    return Property<std::string>(
        [&d, id] {
            const ui::Element* e = find(d, id);
            return e == nullptr ? std::string() : e->label;
        },
        [&d, id](std::string v) { return rename(d, id, std::move(v)); });
}

/// What this object's values are measured against. The relationship, as an
/// ordinary editable property — which is the whole of the authoring surface for
/// it.
// WL-DOC-02 -- agents/workshop/document.md
inline Property<ContextRef> context_of(WorkshopDoc& d, std::int64_t id) {
    return Property<ContextRef>(
        [&d, id] {
            const ui::Element* e = find(d, id);
            return e == nullptr ? ContextRef{} : ContextRef{e->context};
        },
        [&d, id](ContextRef v) { return set_context(d, id, v.id); });
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
