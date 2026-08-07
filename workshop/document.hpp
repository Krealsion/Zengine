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
//
// W-2 ADDED THE OPERATIONS A MAKER'S HANDS NEED -- create, move, delete -- and
// the arrangement that came out of it is the useful part. There is now exactly
// ONE operation in this package that writes a position (`move`), and `set_x` /
// `set_y` are that operation holding one coordinate still. A typed edit in the
// inspector and a drag on the canvas are therefore not two write paths that
// happen to validate alike; they are one write path reached two ways, which is
// the only version of "shared policy" a mutation cannot quietly separate.
//
// W-3 PUT SIZE UNDER THE SAME HAND, and the arrangement repeated exactly:
// `resize` is now the one operation that writes an extent, and `set_width` /
// `set_height` are it holding one extent still. Two properties, one shape of
// answer -- which is the second piece of evidence that the granularity of an
// operation is decided by the GESTURE, not by how many fields it happens to
// touch.
//
// WHAT DOES NOT LIVE HERE, and the distinction W-3 turns on: the CLAMP. These
// operations judge an authored proposal and refuse the ones this document will
// not accept. They never quietly correct one. A maker's HAND, on the other hand,
// can ask for a place or a size that does not exist, and stopping it at the wall
// is interaction policy -- so it lives with the gestures in screen.hpp, which
// then hand a real proposal down to these functions. Refusal and clamping are
// two different truths about two different acts, and keeping them in two files
// is the cheapest reminder of which one is being told.

// W-5 ADDED THE ONE OPERATION A FILE NEEDS -- `restore` -- and with it the first
// law in this package that is about the DOCUMENT rather than about one edit.
// Every rule above judges a proposal against a document that is already legal.
// A document read from a file has not been past any of them, and it can be
// illegal in ways a single edit cannot: two objects carrying one identity, a
// mint that has already handed out the number it says it will hand out next.
// Those are stated in `check_document` and nowhere else, and `restore` is the
// only door that admits a whole document. See the note there for why the maker
// path does not need them (it satisfies them by construction) and why that is
// not a second set of laws.

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
///
/// The FLOOR is not one of them: `ui::kMinCells` is the UI package's, because it
/// is the same number resolution already refuses to round below, and two
/// spellings of one minimum is how a check and the thing it checks come to
/// disagree.
inline constexpr std::int64_t kMaxCells = 4096; ///< an authored size, not a workspace size
inline constexpr std::size_t kMaxNameLen = 32;  ///< a label, not a document

/// The last identity this document could ever mint.
///
/// It exists because W-5 let a document arrive from a FILE. `next_id++` on the
/// largest representable integer is signed overflow -- undefined behaviour
/// produced by data -- and before persistence the only way to reach it was a
/// poke. A file is ordinary maker input, so the mint now has an end and says so
/// (`add` below) instead of wrapping into an identity it has already used.
inline constexpr std::int64_t kMaxIdentity = (std::numeric_limits<std::int64_t>::max)();

/// The first identity a document may carry. Zero is reserved: the session
/// spells "nothing is selected" as 0, so an object numbered 0 could never be
/// told apart from no object at all.
inline constexpr std::int64_t kFirstIdentity = 1;

/// What a new object is, before a maker has said anything about it.
///
/// These live here, beside the limits, because creation must not be able to
/// author a state this document's own setters would refuse -- and the suite
/// checks exactly that rather than trusting that whoever picked the numbers
/// looked. One default and not a palette: the create gesture is "make me one of
/// these", and a component palette is a product decision with no evidence
/// behind it yet.
///
/// The label is deliberately the SAME word the opening document already uses
/// twice. Two objects called `panel` was W-0's fixture for "a name is not an
/// identity"; making it the default means a maker meets that fact by creating,
/// which is the moment it is cheapest to learn.
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

/// Whether this document can still mint. False only for a document whose mint
/// has reached the end of the number line -- unreachable by creating (a maker
/// would have to press `n` more times than there are seconds in the age of the
/// universe) and reachable in one line of a FILE, which is why it is asked.
inline bool can_mint(const WorkshopDoc& d) {
    return d.next_id >= kFirstIdentity && d.next_id < kMaxIdentity;
}

/// Add an authored object, minting its identity. Returns the new id, or 0 when
/// this document has no identity left to give.
///
/// It still judges no maker-supplied value -- there is none, W-0 has no
/// create-with-arguments gesture -- and W-0 through W-4 recorded that as "this
/// operation cannot refuse". W-5 found the one thing it can refuse about: the
/// MINT ITSELF can be exhausted. `next_id++` at the top of the number line is
/// signed overflow, and the result would be an identity this document has
/// already handed out, which is the one law the whole arc rests on. So the
/// exhausted mint is an answer (0) rather than an overflow, and 0 is the same
/// number the session already spells as "no object".
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
///
/// The mint is still `WorkshopDoc::next_id` -- W-1 left it with the document and
/// W-2 deliberately did not move it, because interactive creation is exactly the
/// pressure that would have shown it to be in the wrong place. What that pressure
/// actually surfaced is one property worth naming: the counter NEVER REWINDS. A
/// deleted object's id is not handed out again, so a maker who deletes #3 and
/// creates another gets #4, and a selection, a notice or a half-finished thought
/// that still says "#3" can never quietly come to mean a different object.
inline std::int64_t add_default(WorkshopDoc& d) {
    return add(d, kNewLabel, kNewX, kNewY, ui::Extent{ui::kExtentCells, kNewWidthCells},
               ui::Extent{ui::kExtentCells, kNewHeightCells});
}

/// Remove one authored object BY IDENTITY.
///
/// By identity and never by position in the vector: an index is a fact about the
/// storage that changes under every other operation, while the id is the one
/// thing the selection, the list marker, the ring and the hit test already agree
/// about. A delete addressed by index would be correct exactly until something
/// was inserted before it.
///
/// It refuses rather than quietly doing nothing, because "there is no such
/// object" is a thing a maker did (pressed delete with nothing selected) and not
/// a no-op worth hiding.
inline Written remove(WorkshopDoc& d, std::int64_t id) {
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        if (d.elements[i].id == id) {
            d.elements.erase(d.elements.begin() + static_cast<std::ptrdiff_t>(i));
            return Written::ok();
        }
    }
    return Written::no("no such object");
}

/// What this document considers a legal label: anything a maker can type,
/// except nothing, and except so long it stops being a label. Note what is NOT
/// refused -- a duplicate. Two objects may share a label, because the label is
/// not the identity (see ui::Element), and refusing a duplicate here would
/// quietly make it one.
///
/// Its own function since W-5, for the reason `check_extent` and `check_coord`
/// are their own functions: a document read from a file carries labels nobody
/// typed, and it must meet the SAME rule a maker's rename meets. Two spellings
/// of one rule is how a typed name and a loaded one come to disagree.
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

/// Author a size. THE ONE PLACE an extent is written in this package.
///
/// One operation and not two, for exactly the reason `move` below is one. W-2
/// found that a gesture and a property can disagree about the granularity of an
/// operation, and a corner handle says it again at the other property: a maker
/// pulling a corner proposes a SIZE, not a width and then a height. Written as
/// two independent setters, a diagonal resize whose height is illegal would
/// narrow the object AND report a refusal -- the refusal-beside-a-successful-write
/// W-2 removed from placement, reappearing here. So both extents are checked
/// before either is written, and a refused resize leaves the object exactly the
/// size it was.
///
/// `set_width`/`set_height` below are this operation holding one extent still.
/// That is what makes "the inspector and the maker's hand author through the same
/// rules" structural for size as well as for position: there is no second place
/// for a resize gesture to acquire its own opinion about what a legal extent is.
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

/// The first cell the workspace has. Named rather than spelled `0` twice,
/// because the check below REFUSES anything before it and a maker's hand STOPS
/// at it (screen.hpp), and those two behaviours are only coherent while they are
/// the same number.
inline constexpr std::int64_t kFirstCell = 0;

/// Position: a rectangle may sit anywhere in the workspace, including partly off
/// its right or bottom edge (the canvas clips, and a maker dragging something
/// half out of view has not made a mistake). Negative is refused, because the
/// workspace has no cells there at all -- the object would be authored somewhere
/// that does not exist.
///
/// One check, shared by every operation that writes a coordinate, for the same
/// reason `check_extent` is shared by width and height: two spellings of one rule
/// is how a typed edit and a dragged one come to disagree about what is legal.
inline Written check_coord(std::int64_t v) {
    if (v < kFirstCell) {
        return Written::no("the workspace starts at 0");
    }
    return Written::ok();
}

/// Author a position. THE ONE PLACE a position is written in this package.
///
/// It is ONE operation and not two, and that is a semantic finding rather than a
/// convenience. A maker performs one gesture: a drag proposes a PLACE, not an x
/// and then a y. Written as two independent setters, a diagonal drag into the
/// top-left corner would slide the object down the edge while reporting a
/// refusal -- a refusal message beside a successful write, which is the one thing
/// a refusal must never be. So both coordinates are checked before either is
/// written, and a refused move leaves the object exactly where it was.
///
/// `set_x`/`set_y` below are this operation holding one coordinate still. That is
/// what makes "the inspector and the maker's hand author through the same rules"
/// STRUCTURAL rather than a promise: there is no second place to change, so a
/// gesture cannot acquire its own opinion about what a legal position is.
inline Written move(WorkshopDoc& d, std::int64_t id, std::int64_t x, std::int64_t y) {
    ui::Element* e = find_mut(d, id);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    const Written cx = check_coord(x);
    if (!cx.accepted) {
        return cx;
    }
    const Written cy = check_coord(y);
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

// ---- A whole document, and the one door that admits one -------------------------------
//
// Everything above judges ONE PROPOSAL against a document that is already legal.
// W-5 needs the other question -- is this whole thing a document at all -- and
// the two are not the same question, because a document can be wrong in ways no
// single edit can make it wrong.
//
// WHY THIS IS NOT A SECOND SET OF LAWS. Every per-value rule below is the SAME
// function the maker's own edits go through: check_name, check_coord,
// check_extent. Nothing is re-spelled. What is added are exactly the two facts
// the maker path holds BY CONSTRUCTION and therefore never had to state:
//
//     ids are distinct          `add` takes each from the mint, and the mint
//                               advances -- so no maker gesture can produce
//                               two objects with one identity.
//     the mint is ahead         `add` mints `next_id` and then increments, so
//                               every id already handed out is below it.
//
// A document from a file has been past neither, so it says both out loud. That
// is the honest shape of the difference: the loader is not distrusted more than
// a maker, it simply arrives without the history that made the invariants free.

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
        const Written cx = check_coord(e.x);
        if (!cx.accepted) {
            return Written::no(who + cx.refusal);
        }
        const Written cy = check_coord(e.y);
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
    // Distinctness last, and by sorting a copy rather than by a nested loop:
    // the object count is bounded by the decoder's materialization budget and
    // not by anything a screen can show, so the quadratic form is a cost a
    // hostile file gets to choose.
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
    return Written::ok();
}

/// Replace this document with another one, KEEPING that one's identities.
///
/// THE ONE DOOR a whole document comes through, and the reason it exists rather
/// than persistence calling `add` in a loop: `add` MINTS. A loader built on it
/// would hand every loaded object a fresh number and then display the old one,
/// which is not "the same object came back" -- it is a lookalike wearing the
/// label. The identities in the candidate are the identities that survive.
///
/// IT IS A TRANSACTION. The candidate is judged whole, before anything is
/// written; a refusal leaves the live document byte-for-byte what it was. That
/// is the same rule the property editor has kept since W-0 -- a refusal leaves
/// committed truth unchanged -- said about a whole document instead of one
/// field, and it is what stops a malformed file from leaving Workshop half
/// loaded.
///
/// IT INVALIDATES EVERY VIEW INTO THE OLD DOCUMENT, and a caller must assume so.
/// A successful restore moves a new element vector into place and frees the old
/// one, so a `ui::Element*` from `find`, a `ui::Placed*` from a scene, or
/// anything else holding a position in the previous storage is dangling the
/// instant this returns true. That is a hazard the phase INTRODUCED -- before
/// persistence, the elements a Workshop session was looking at were the ones it
/// had been looking at all along -- and it is why the weave cancels the drag and
/// rebuilds the inspector rather than repairing them. (The suite's first draft
/// held such a pointer across a load; the ordinary lane passed and the sanitizer
/// lane called it a heap-use-after-free.)
///
/// What it is NOT: an undo system, an event log, a snapshot framework, or a
/// general "set the state" door. It reconstructs one validated document with
/// existing identities, and that is the whole of it.
inline Written restore(WorkshopDoc& live, WorkshopDoc candidate) {
    const Written legal = check_document(candidate);
    if (!legal.accepted) {
        return legal;
    }
    live = std::move(candidate);
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
