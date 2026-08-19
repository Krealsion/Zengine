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
// WHAT THIS PACKAGE OWNS, AND WHAT IT DELIBERATELY DOES NOT. Authored-versus-
// resolved is the UI package's (ui/layout.hpp); the split is the useful part:
//
//   ui::resolve / ui::hit   HOW authored intent becomes geometry, and what is
//                           under a cell. The same answer for every application.
//   doc::check_extent       WHAT THIS DOCUMENT considers a legal extent. Its
//                           bounds are policy (at most 4096 cells; a share is
//                           1..100), and policy is exactly what does not
//                           generalise -- a different application may hold a
//                           different opinion and still resolve identically.
//
// So Workshop owns every refusal, and owns no geometry at all.
//
// ONE ACT, ONE OPERATION. `move` is the only function here that writes a
// position, `resize` the only one that writes an extent, and `set_context` the
// only one that writes a relationship; `set_x`/`set_y`, `set_width`/
// `set_height` are those operations holding one half still. A typed edit in the
// inspector and a drag on the canvas are therefore not two write paths that
// happen to validate alike; they are one write path reached two ways, which is
// the only version of "shared policy" a mutation cannot quietly separate. Each
// judges the maker's act WHOLE before writing any of it -- `set_context` judges
// three facts, because changing a context can make an already-written
// coordinate illegal -- so a two-part gesture never half-succeeds. The
// granularity of an operation is decided by the GESTURE, not by how many fields
// it happens to touch.
//
// WHAT DOES NOT LIVE HERE, and it is the distinction the whole boundary turns
// on: the CLAMP. These operations judge an authored proposal and refuse the ones
// this document will not accept. They never quietly correct one. A maker's HAND,
// on the other hand, can ask for a place or a size that does not exist, and
// stopping it at the wall is interaction policy -- so it lives with the gestures
// in screen.hpp, which then hand a real proposal down to these functions.
// Refusal and clamping are two different truths about two different acts, and
// keeping them in two files is the cheapest reminder of which one is being told.

// TWO KINDS OF LAW, and only one of them is about a single edit. Every rule
// above judges a proposal against a document that is already legal. A document
// read from a FILE has not been past any of them, and can be illegal in ways a
// single edit cannot: two objects carrying one identity, a mint that has already
// handed out the number it says it will hand out next. Those are stated in
// `check_document` and nowhere else, and `restore` is the only door that admits
// a whole document -- so an interactive rewire and a loaded file are refused in
// the same words. See the note there for why the maker path does not need them
// (it satisfies them by construction) and why that is not a second set of laws.

// `check_coord` IS THE ROOT'S GUARD, not every coordinate's, and the reason is
// its own stated one: the workspace has no cells before 0, which is a fact about
// the WORKSPACE. In another element's frame a coordinate is an offset, -1 is an
// ordinary thing to author, and the cell it lands on may well exist. So the
// check takes the context it is judging in, and the root's guard is exactly as
// strong as it would be without it.
//
// What all of this looks like to a maker -- and why a source with dependents is
// not deletable, which is this document's policy and not the vocabulary's -- is
// README.md#workshop--the-maker-facing-surface.

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

/// HOW LONG A MAKER'S NAME FOR AN OBJECT MAY BE, in bytes.
///
/// SIXTY-FOUR SINCE QR-3, AND IT WAS THIRTY-TWO FROM W-0 UNTIL THEN. The old number is
/// worth recording because it was traced rather than inherited: W-0 introduced it with the
/// six words `a label, not a document` and no other rationale in the commit, in any
/// document, or in any report-back. The only rationale ever written down for a
/// thirty-two in this application is `setup::kMaxSetupNameLen`'s and
/// `setup::kMaxPaneNameLen`'s -- a SCREEN measurement, one line of the narrowest
/// composition -- and WS-0 attributed the same reason to this constant in passing.
///
/// THAT REASON WAS MEASURABLY FALSE HERE, which is what settled it. The narrowest place an
/// object's name is read is the OBJECTS list, whose body is 28 columns at the 78x22
/// minimum in a character medium; a name AT the old bound already came back
/// `> #1 xxxxxxxxxxxxxxxxxxxx...`. Thirty-two never bought a whole read anywhere, and
/// every reader of a name has marked its own cut since INTR-0/TYPE-0/HD-7 -- the workspace
/// object being the last of them, bounded to its own material by QR-3.
///
/// SO WHAT SURVIVES IS THE KIND OF BOUND AND NOT THE NUMBER. It survives for the reason
/// every other bound in this application has: a document arrives from a FILE, `check_name`
/// is what `check_document` spends on each one, and a field with no bound at all would be
/// the only authored string in Workshop without one. Sixty-four is this application's
/// existing measure for a maker's prose that is not a routing name
/// (`setup::kMaxPaneSummaryLen`, one sentence), it keeps `a label, not a document` true,
/// and it is above what the tool itself has already needed: a suite case wanting a
/// realistically long name reaches past this check with a 43-byte one written straight onto
/// the element, because a maker could not author it.
///
/// NOT A CAPACITY, and no saved byte depends on it: the file's own bounds are
/// `persist::kMaxDocumentBytes` and the Loom decoder's materialisation budget, and the
/// format holds a name as an ordinary string.
inline constexpr std::size_t kMaxNameLen = 64;

/// The last identity this document could ever mint.
///
/// It exists because a document can arrive from a FILE. `next_id++` on the
/// largest representable integer is signed overflow -- undefined behaviour
/// produced by data -- and a file is ordinary maker input, so the mint has an
/// end and says so (`add` below) instead of wrapping into an identity it has
/// already used.
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
/// twice. Two objects called `panel` is the fixture for "a name is not an
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
/// It judges no maker-supplied value -- there is none, because there is no
/// create-with-arguments gesture -- but it is not unable to refuse: the MINT
/// ITSELF can be exhausted. `next_id++` at the top of the number line is
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
/// The mint is `WorkshopDoc::next_id`, and it rides with the document rather
/// than with the session because a saved document must come back able to mint
/// (persist.hpp). The property that matters: the counter NEVER REWINDS. A
/// deleted object's id is not handed out again, so a maker who deletes #3 and
/// creates another gets #4, and a selection, a notice or a half-finished thought
/// that still says "#3" can never quietly come to mean a different object.
inline std::int64_t add_default(WorkshopDoc& d) {
    return add(d, kNewLabel, kNewX, kNewY, ui::Extent{ui::kExtentCells, kNewWidthCells},
               ui::Extent{ui::kExtentCells, kNewHeightCells});
}

/// Everything that measures against `id`, in document order.
///
/// One-directional on purpose. An element records what IT measures against and
/// nothing records what measures against it, so this is a scan rather than a
/// lookup -- and that asymmetry is the honest one: the relationship is a fact
/// the dependent authored, and a back-pointer would be a second copy of it that
/// every edit would have to remember to maintain.
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
///
/// AND IT REFUSES A SECOND WAY: an object something else measures against
/// cannot be deleted. That is a POLICY, chosen here, and the two
/// alternatives were both rejected for changing a maker's work without being
/// asked to:
///
///   delete the dependents too   would be treating a dependency as OWNERSHIP.
///                               Nothing about "B's numbers are measured from
///                               A's" says B is part of A, and deleting one
///                               rectangle should never destroy another one a
///                               maker cannot even see is related.
///   move them to the root       would be rewriting their authored x/y's
///                               MEANING while leaving the numbers alone. B at
///                               2,1 in A's frame is somewhere; B at 2,1 in the
///                               root's frame is somewhere else, and the tool
///                               would have silently authored the move.
///
/// So it refuses and NAMES the dependents, and rewiring or deleting them is a
/// separate act the maker performs deliberately. This is Workshop's document
/// policy and not a property of `ui::Element::context`: another application may
/// decide a source does own its dependents, and nothing in the vocabulary
/// stops it.
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
/// except nothing, and except so long it stops being a label. Note what is NOT
/// refused -- a duplicate. Two objects may share a label, because the label is
/// not the identity (see ui::Element), and refusing a duplicate here would
/// quietly make it one.
///
/// Its own function, for the reason `check_extent` and `check_coord` are their
/// own functions: a document read from a file carries labels nobody
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
/// One operation and not two, for exactly the reason `move` below is one: a
/// gesture and a property can disagree about the granularity of an operation,
/// and a corner handle says so at this property too -- a maker pulling a corner
/// proposes a SIZE, not a width and then a height. Written as two independent
/// setters, a diagonal resize whose height is illegal would narrow the object
/// AND report a refusal, which is the refusal-beside-a-successful-write that
/// placement already refuses to produce. So both extents are checked
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
/// half out of view has not made a mistake). Negative is refused AT THE ROOT,
/// because the workspace has no cells there at all -- the object would be
/// authored somewhere that does not exist.
///
/// THAT IS A LAW ABOUT THE ROOT AND NOT ABOUT COORDINATES, and the reason is in
/// the sentence above: "the workspace has no cells there".
/// That is a fact about the WORKSPACE. A coordinate authored in another
/// element's frame is an OFFSET FROM ITS ORIGIN, and -1 there means "one cell
/// before my source starts", which is an ordinary thing to want (a marker on a
/// panel's left edge, a badge over its corner) and which resolves to a cell that
/// exists whenever the source is not itself at 0. So the rule now says what it
/// always meant:
///
///     context == root     the coordinate IS a workspace cell. Unchanged: the
///                         workspace starts at 0 and nothing may be authored
///                         before it.
///     context == an id    the coordinate is an offset in that element's frame.
///                         Any offset is authorable; where it lands is
///                         resolution's answer, and landing outside the
///                         workspace is the same already-legal situation as an
///                         object authored past the right edge.
///
/// The root's guard did not weaken by one cell, and it is now stated as being
/// the root's rather than being every coordinate's by accident. What a HAND is
/// allowed to reach is a separate question with a separate answer -- screen.hpp
/// stops a drag at the workspace edge in RESOLVED cells, for everybody, so a
/// dependent dragged left stops where a maker can see it stop and the negative
/// offset is what that stop is authored AS.
///
/// One check, shared by every operation that writes a coordinate, for the same
/// reason `check_extent` is shared by width and height: two spellings of one rule
/// is how a typed edit and a dragged one come to disagree about what is legal.
inline Written check_coord(std::int64_t v, std::int64_t context) {
    if (context == ui::kRootContext && v < kFirstCell) {
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
//
// The authored relationship is one identity per element, and everything in this
// section exists because an identity that names another object can name one
// that is not there, or one whose own chain comes back around. Both are
// REFUSALS, and both are refused in the same words whether they arrive by a
// maker's keystroke or out of a file -- there is one law, stated here, and
// `check_document` and `set_context` are the two doors that ask it.
//
// WHAT IS NOT HERE, and could not be: how deep a chain may go. There is no limit
// and there is deliberately no constant to find. The only bound is that a chain
// visiting more objects than the document holds has visited one twice, which is
// what a cycle IS -- so depth is bounded by the document's own size and by
// nothing this file decided.

/// How a broken chain reads: `#7 -> #9 -> #7`, cut off before it becomes wall.
///
/// CUT BY CHARACTERS AND NOT BY LINKS, which is the difference between a cap
/// that works and one that looks like it does. Workshop's notice is ONE LINE and
/// the canvas clips at its own width, so a refusal that does not fit is a
/// refusal whose tail a maker never sees -- and the tail of a cycle message is
/// the part that names the loop. Measured live: a two-object cycle printed one
/// character too long and lost its closing bracket, and a fixed link count
/// then failed the same way one identity later, because an identity is an
/// int64 and `#9223372036854775807` is twenty characters on its own.
///
/// WHAT IT DOES NOT CLAIM: that every refusal fits. The prose around the chain
/// carries two identities of its own, so a document whose objects are numbered
/// in the quadrillions can still overrun the line. That is the notice line's
/// standing limit rather than this function's, it is recorded as pressure, and
/// nothing here pretends to have closed it.
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
///
/// Three refusals, each naming the number a maker can act on:
///
///   itself         an object measured against itself has no frame at all, and
///                  it is the cycle a maker is most likely to type by accident.
///   nothing        `#999` when nothing carries 999. It is refused rather than
///                  quietly becoming the root, because a missing dependency is
///                  not the same fact as no dependency, and a tool that turned
///                  one into the other would silently move the object.
///   a cycle        `candidate`'s own chain already runs through `id`, so
///                  writing this would close a loop that never reaches the
///                  workspace. The test is exactly that -- ask the source where
///                  it measures FROM, and refuse if the answer comes back here.
///
/// The chain is walked by `ui::walk_context`, which is also what checks a whole
/// loaded document, so an interactive rewire and a file are judged by one
/// implementation rather than by two that agree today.
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
///
/// It is one operation over three facts, for exactly the reason `move` is one
/// operation over two coordinates: changing a context can make an existing,
/// previously legal coordinate illegal, and half-authoring that is the
/// refusal-beside-a-successful-write this package has removed twice already. A
/// dependent sitting at x = -3 is legal (it is an offset in its source's frame);
/// the same object moved to the ROOT would be authored three cells before the
/// workspace begins. So the coordinates are re-judged IN THE PROPOSED DOMAIN
/// before anything is written, and a refused rewire leaves the object exactly as
/// it was -- context and position both.
///
/// WHAT IT DELIBERATELY DOES NOT DO: compensate. The authored x/y and the
/// authored extents are left exactly as the maker wrote them, so an object
/// rewired from the root into a source at 10,4 visibly MOVES. The alternative --
/// rewriting x/y to preserve the picture -- would author two facts a maker
/// changed one of, which is the silent correction the whole authored/resolved
/// discipline exists to prevent. (A future "drag B into A" gesture is a
/// different act and may reasonably project the hand instead; that is a gesture
/// question, and gestures live in screen.hpp.)
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
//
// Everything above judges ONE PROPOSAL against a document that is already legal.
// A loaded file poses the other question -- is this whole thing a document at
// all -- and the two are not the same, because a document can be wrong in ways
// no single edit can make it wrong.
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
///
/// THE ONE DOOR a whole document comes through, and the reason it exists rather
/// than persistence calling `add` in a loop: `add` MINTS. A loader built on it
/// would hand every loaded object a fresh number and then display the old one,
/// which is not "the same object came back" -- it is a lookalike wearing the
/// label. The identities in the candidate are the identities that survive.
///
/// IT IS A TRANSACTION. The candidate is judged whole, before anything is
/// written; a refusal leaves the live document byte-for-byte what it was. That
/// is the same rule the property editor keeps -- a refusal leaves committed
/// truth unchanged -- said about a whole document instead of one
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

/// What this object's values are measured against. The relationship, as an
/// ordinary editable property — which is the whole of the authoring surface for
/// it.
///
/// There is no tree editor, no node graph and no hierarchy panel, because the
/// relationship is one field and the inspector already knows how to edit one
/// field. That it costs a single line here is the measurement: the property
/// machinery written for `Name` carries a relationship without being told
/// relationships exist.
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
