// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SCREEN_HPP
#define ZENGINE_WORKSHOP_SCREEN_HPP

// The Workshop screen: the session facts, the maker's gestures over them, the
// inspector's rows, and the one function that turns all of it into a published
// canvas.
//
// W-2 added the gestures — create, delete, nudge, and the three halves of a
// pointer drag — and put them HERE rather than in the weave on purpose. A
// gesture whose only witness is a keystroke is a gesture no suite can pin, and
// W-0/W-1's evidence all lives in this header's purity. `workshop.cpp` now binds
// keys and pointer events to these functions and does nothing else with the
// document, so what the suite drives is what a maker's hand drives.
//
// PURE, and that is the point of it being its own header: paint() takes a
// document and a session and returns a SurfaceCanvas. No terminal, no window, no
// weave, no bus. So the suite pins an entire Workshop screen -- the rectangle,
// the selection ring, the object list, the inspector, a refusal -- as a value it
// can assert on, and the only thing left for the live run to prove is that the
// bus and the Skin carry it, which is a different claim from "the screen is
// right".
//
// THE AUTHORED MATERIAL IS RESOLVED IN EXACTLY ONE PLACE, and after W-1 that
// place is not this file. `workspace_scene()` below builds the viewport and calls
// `ui::resolve` once; the canvas, the inspector's resolved reading and the hit
// test all read the Scene it returns. W-0 had three separate call sites doing
// their own extent arithmetic and agreeing only because one person wrote all
// three -- which is a coincidence, not a guarantee, and it is the coincidence
// this phase removed.
//
// The SCREEN'S OWN FURNITURE is still constants, and that is a different thing
// from the authored material and stays a Workshop decision: where the object
// list sits beside the workspace is this application's composition, not
// something a maker authors and not something a package should decide for it.
// SurfaceCanvas paints a picture; whoever publishes one has already decided what
// the picture is. A relational layout engine (stacks, weights, "beside") is the
// Loom's loom::Widget + px_layout, a different model that stays where it is --
// see W-1-RB for why the assumed relocation did not happen.

#include "document.hpp"
#include "property.hpp"
#include "vocabulary.hpp"

#include "surface/vocabulary.hpp"
#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace zengine::workshop {

// ---- The one screen's layout, in canvas cells ------------------------------------------

inline constexpr std::int64_t kScreenW = 78;
inline constexpr std::int64_t kScreenH = 22;

inline constexpr std::int64_t kWorkspaceX = 0; ///< the workspace's origin ON THE CANVAS...
inline constexpr std::int64_t kWorkspaceY = 1; ///< ...which authored coordinates are relative to
inline constexpr std::int64_t kWorkspaceW = 48; ///< the default workspace extent, in cells
inline constexpr std::int64_t kWorkspaceH = 16;
inline constexpr std::int64_t kWorkspaceMinW = 12; ///< narrow enough to make a share visibly shrink

inline constexpr std::int64_t kPanelX = 50; ///< the object list and the inspector
inline constexpr std::int64_t kListY = 1;
inline constexpr std::int64_t kListRows = 5;
inline constexpr std::int64_t kRowsY = 8;
inline constexpr std::int64_t kNoticeY = 18;
inline constexpr std::int64_t kHelpY = 20;

/// What the size handle looks like. One character, because it occupies one cell,
/// and one that none of the medium's role glyphs already use (`.` workspace,
/// `#` body, `*` ring, `!` alert) -- an affordance a maker cannot tell from the
/// furniture is not an affordance.
inline constexpr const char* kHandleGlyph = "+";

/// A drag in progress. Session, emphatically not content.
///
/// It holds the IDENTITY being manipulated and where inside that object the
/// maker took hold -- and deliberately not a position, and deliberately not a
/// size. The object's authored place and extents are on the object; a second
/// copy of either here would be the shadow model the whole Workshop arc is
/// arranged to avoid, and it would be the copy that goes stale.
///
/// `grab_dx/dy` are in RESOLVED cells — where inside the object's rectangle on
/// the workspace the maker took hold. W-2 wrote "authored cells" and it was the
/// same number, because `ui::resolve` copied x/y through untouched; W-6 made the
/// two differ (a resolved position is now its context's origin plus the authored
/// offset) and the honest name for what a hand grabs is the resolved one. The
/// grab is still a plain SUBTRACTION and still not an inverse of the resolver;
/// what changed is that turning the hand's answer back into authored truth is
/// now a second subtraction -- the context's origin -- performed in `place`.
///
/// `resizing` is the whole of W-3's session cost: ONE bool, because there is one
/// gesture in flight and it is either moving the object or sizing it. A resize
/// stores no grab offset (the handle is one cell and the pointer names the new
/// corner outright), no starting extent and no starting viewport -- the
/// projection reads the CURRENT extent for its mode and the CURRENT workspace for
/// its span, and a proposal computed from the pointer rather than from the
/// previous proposal cannot drift. Nothing here is a Tool, a Manipulator or a
/// TransformSession; one gesture does not earn an interaction framework.
struct Drag {
    bool active = false;
    bool resizing = false; ///< the maker took hold of the size handle, not the body
    std::int64_t id = 0;
    std::int64_t grab_dx = 0;
    std::int64_t grab_dy = 0;
};

/// The session: what a maker is currently doing, as opposed to what they have
/// authored. Kept out of WorkshopDoc deliberately (see vocabulary.hpp) so the
/// two kinds of fact cannot be mistaken for each other -- selection is not
/// content, and neither is the size of the window it is being looked at through,
/// and neither is a half-finished drag.
struct Session {
    std::int64_t selected = 0;              ///< the selected object's IDENTITY (0 = none)
    std::int64_t workspace_w = kWorkspaceW; ///< what a share of the workspace currently means
    std::int64_t workspace_h = kWorkspaceH;
    std::size_t cursor = 0;   ///< which inspector row the maker is on
    std::vector<Row> rows;    ///< the inspector, rebuilt when the selection changes
    Drag drag;                ///< a pointer drag in flight, if any
    std::string notice;       ///< the last thing Workshop had to say
    bool notice_is_bad = false; ///< whether that thing was a refusal
};

/// The workspace as a viewport, and the document resolved against it — the ONE
/// call that turns authored intent into geometry in this application.
///
/// It is a function rather than a cached member of Session on purpose. A
/// resolved scene that outlived the workspace it was resolved against is exactly
/// the stale-number lie the authored/resolved split exists to prevent, so the
/// answer is recomputed wherever it is wanted and stored nowhere. It is three
/// integers and a loop over a handful of elements; the cost of being right is
/// nothing.
inline ui::Scene workspace_scene(const WorkshopDoc& d, const Session& s) {
    return ui::resolve(d.elements, ui::Viewport{s.workspace_w, s.workspace_h});
}

/// The inspector for one authored object: the properties, plus the facts that
/// are not properties.
///
/// The whole list is seven calls, and `Width` and `Height` are the two that
/// matter most -- they are the same semantic type, so they share every line of
/// conversion, parsing and refusal wording. A further property of an existing
/// type would be one more line here and nothing else anywhere. That is the old
/// builder's per-row plumbing, replaced -- and W-6 spent the prediction: adding
/// `Context`, a property of a type that did not exist before, cost one line here
/// plus one `TextForm` specialisation, and nothing else in this file changed.
///
/// The last row is the RESOLVED size, and it is a `show` rather than an `edit`
/// because it is not the maker's to author: it is what the current workspace
/// makes of `Width` and `Height`. A maker looking at `70%` and `33 x 8 cells` is
/// looking at two true things, and the inspector says which is which by what it
/// will let them touch.
///
/// After W-1 that row reads THE SCENE -- the same resolved scene the canvas is
/// painted from and the same one a click is tested against. Before, it did its
/// own extent arithmetic, and "the inspector agrees with the picture" was a
/// property of two functions happening to say the same thing.
inline std::vector<Row> inspector_rows(WorkshopDoc& d, const Session& s) {
    std::vector<Row> rows;
    const std::int64_t id = s.selected;
    if (doc::find(d, id) == nullptr) {
        return rows;
    }
    const std::int64_t ww = s.workspace_w;
    const std::int64_t wh = s.workspace_h;

    rows.push_back(Row::show("Identity", [id] { return "#" + std::to_string(id); }));
    rows.push_back(Row::edit("Name", doc::name_of(d, id)));
    // Context comes BEFORE the four numbers it gives meaning to, because that is
    // the order the reading has to happen in: `X 2` is not an answer until you
    // know 2 of what. It is one more `Row::edit` over one more property, and
    // that is the measurement W-6 wanted from the inspector -- a relationship is
    // not a different kind of thing needing a different kind of editor. It is
    // NOT labelled `Parent`: nothing here is a parent, and a familiar word that
    // implies ownership, clipping and cascade-delete would be the tool telling a
    // maker something the document does not do.
    rows.push_back(Row::edit("Context", doc::context_of(d, id)));
    rows.push_back(Row::edit("X", doc::x_of(d, id)));
    rows.push_back(Row::edit("Y", doc::y_of(d, id)));
    rows.push_back(Row::edit("Width", doc::width_of(d, id)));
    rows.push_back(Row::edit("Height", doc::height_of(d, id)));
    rows.push_back(Row::show("Resolved", [&d, id, ww, wh] {
        const ui::Scene scene = ui::resolve(d.elements, ui::Viewport{ww, wh});
        const ui::Placed* placed = ui::placed_for(scene, id);
        if (placed == nullptr) {
            return std::string("-");
        }
        return std::to_string(placed->rect.w) + " x " + std::to_string(placed->rect.h) + " cells";
    }));
    return rows;
}

/// Where the cursor belongs on a freshly built inspector: the first row a maker
/// can actually author. Landing it on `Identity` instead would open onto a row
/// whose only possible answer to "edit this" is a refusal.
inline std::size_t first_editable(const std::vector<Row>& rows) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].editable()) {
            return i;
        }
    }
    return 0;
}

/// Rebuild the inspector for the current selection and put the cursor somewhere
/// useful. One gesture, so the running weave and the suite cannot come to
/// disagree about what a fresh inspector is -- and rebuilding rather than
/// patching is why nothing in this package has a "refresh the inspector" call.
inline void refocus(WorkshopDoc& d, Session& s) {
    s.rows = inspector_rows(d, s);
    s.cursor = first_editable(s.rows);
}

namespace detail {

/// Left-align in a fixed width; longer text is cut. Workshop's own layout job --
/// the canvas has no notion of a column.
inline std::string pad(std::string text, std::size_t width) {
    if (text.size() > width) {
        text.resize(width);
        return text;
    }
    text.append(width - text.size(), ' ');
    return text;
}

/// One cell along, without leaving the number line.
///
/// A nudge's proposal is COMPUTED rather than typed, and that widens its input
/// domain the same way W-1's move into a package widened `resolve_extent`'s:
/// `x + 1` is well defined for every value a setter produced and undefined for
/// the largest one a poke can write (`WorkshopDoc` is ZEN_EXPOSE()d). So the step
/// saturates -- the neighbour of the last representable cell is itself -- and the
/// result then goes through the ordinary refusal like any other proposal. The
/// plain lane cannot see the difference; a sanitizer can, and the report records
/// the run that does.
inline std::int64_t step(std::int64_t v, std::int64_t by) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (by > 0) {
        return v > kMax - by ? v : v + by;
    }
    if (by < 0) {
        return v < kMin - by ? v : v + by;
    }
    return v;
}

/// `a - b`, without leaving the number line — `step`'s partner, and needed for
/// the same reason. A resize's proposal is a DIFFERENCE (`pointer - the object's
/// own edge`), and both terms are values this weave does not own: the pointer
/// comes off the wire and the edge comes off a poke-writable document. The
/// saturated end is far outside any workspace, which already means "nothing
/// reachable there".
inline std::int64_t minus(std::int64_t a, std::int64_t b) noexcept {
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    if (b < 0) {
        return a > kMax + b ? kMax : a - b;
    }
    return a < kMin + b ? kMin : a - b;
}

} // namespace detail

// ---- Direct manipulation, and the boundary policy it needs -----------------------------
//
// W-2 left P15 open: a drag into the workspace's edge stopped dead and reported a
// refusal, which is truthful and brusque, and W-2 declined to invent a clamping
// policy from one sighting. W-3 has the second sighting -- a resize meets a wall
// at BOTH ends of every extent -- so the policy is decided here, once, for both
// gestures:
//
//     A HAND that reaches past what exists           stops at the boundary,
//     (a drag, a nudge, a corner pulled)             authors the boundary value,
//                                                    AND SAYS SO.
//
//     A VALUE a maker WROTE that is not allowed      is refused, the authored
//     (`-1` in X, `500%` in Width)                   state is untouched, and the
//                                                    draft survives so it can be
//                                                    fixed.
//
// The two are different acts and deserve different answers. A hand did not
// propose "-1"; it proposed "further left than there is", and the honest reading
// of that is "as far left as there is". A typed `-1` is a specific claim, and
// silently storing `0` instead would be the tool putting words in a maker's
// mouth -- the exact silent correction the authored/resolved discipline exists to
// prevent, and the reason `doc::` still refuses and never clamps.
//
// The clamp therefore lives HERE and the refusal lives THERE, and the boundary a
// hand stops at is expressed in the document's OWN limits (`doc::kFirstCell`,
// `doc::kMaxCells`, `ui::kMinCells`, and — for a share — whatever the resolver
// says 1% and 100% of this workspace are). So the gesture is not permitted a
// second opinion about what is legal; it is only permitted to stop.

/// One boundary a hand can stop at, in words a maker can read. Separate
/// sentences from the refusals in document.hpp on purpose: "stopped at the
/// workspace edge" and "the workspace starts at 0" are different events, and a
/// maker who cannot tell them apart cannot tell whether anything was written.
inline constexpr const char* kAtWorkspaceStart = "stopped at the workspace edge";
inline constexpr const char* kAtSmallest = "stopped at the smallest size";
inline constexpr const char* kAtLargest = "stopped at the largest size";
/// Reworded by W-6, because the sentence stopped being true. It used to read
/// "a share stops at the whole workspace", which was exact while the workspace
/// was the only thing a share could be a share of. A share of another object
/// stops at the whole of THAT object, and the wall is the same wall -- the
/// vocabulary's, not the workspace's (100% of anything is all of it).
inline constexpr const char* kAtWholeContext = "a share stops at the whole of its context";

/// What one act of DIRECT MANIPULATION did — a hand's outcome, which is not the
/// same shape as a value's outcome.
///
/// `written` is what the document said about the proposal the gesture finally
/// made. `boundary` is empty unless the gesture had to REDUCE that proposal to
/// reach something authorable at all. Both facts are needed because they are
/// independent: a clamped gesture normally succeeds (something WAS written --
/// the boundary value), and a refusal writes nothing, so "was anything written"
/// and "did the hand hit a wall" cannot be read off one another.
struct Handled {
    Written written;
    std::string boundary;

    bool accepted() const { return written.accepted; }
    bool clamped() const { return !boundary.empty(); }

    static Handled of(Written w) { return Handled{std::move(w), {}}; }
};

// ---- The maker's gestures over one session ---------------------------------------------
//
// Session-level operations: each composes a document operation (which can refuse)
// with the selection bookkeeping that keeps the canvas, the object list and the
// inspector talking about the same object. They live here rather than in the
// weave because a gesture whose only witness is a keystroke is a gesture no suite
// can pin -- workshop.cpp binds keys and pointers to these, and nothing else.

/// Create one new authored object and select it.
///
/// The identity comes from the DOCUMENT's mint, and the new object is immediately
/// the selected one -- which is what makes creation a complete gesture rather
/// than a thing that happens somewhere off screen. The canvas, the object list
/// and the inspector all read the selection, so all three follow from this one
/// assignment; there is no "add it to the list too" step to forget.
/// Returns 0 when the document has no identity left to mint (W-5: a document
/// can arrive from a file, and a file can say its mint is spent). Nothing is
/// created and nothing in the session moves -- a gesture that could not happen
/// must not leave the selection somewhere new.
inline std::int64_t create(WorkshopDoc& d, Session& s) {
    const std::int64_t id = doc::add_default(d);
    if (id == 0) {
        return 0;
    }
    s.selected = id;
    refocus(d, s);
    return id;
}

/// Delete the selected object.
///
/// THE POST-DELETE SELECTION RULE, stated once here and tested: the selection
/// moves to whichever object took the deleted one's place in authored order; if
/// the deleted one was last, to the new last; if the document is now empty, to
/// NONE. It is the smallest rule that never leaves a dangling identity and never
/// makes a maker hunt for where they are.
///
/// A refusal (nothing selected, or a selection that has already outlived its
/// object) changes neither the document nor the session.
inline Written delete_selected(WorkshopDoc& d, Session& s) {
    const std::int64_t id = s.selected;
    std::size_t at = d.elements.size();
    for (std::size_t i = 0; i < d.elements.size(); ++i) {
        if (d.elements[i].id == id) {
            at = i;
            break;
        }
    }
    const Written removed = doc::remove(d, id);
    if (!removed.accepted) {
        return removed;
    }
    if (d.elements.empty()) {
        s.selected = 0;
    } else {
        s.selected = (at < d.elements.size() ? d.elements[at] : d.elements.back()).id;
    }
    refocus(d, s);
    return Written::ok();
}

/// Put an object where a HAND asked for it, IN WORKSPACE CELLS — the one place a
/// proposed position meets the boundary policy, and the only door `nudge` and
/// `drag_to` use.
///
/// THE PROPOSAL IS GLOBAL AND THE WRITE IS LOCAL, and that is W-6's whole change
/// to direct manipulation. A hand points at a cell of the workspace; it does not
/// point at "two cells into #1". So the gesture layer takes the hand's answer in
/// the only coordinates a hand has, and projects it into whatever the object's
/// authored coordinates MEAN by subtracting the origin of the frame the resolver
/// says the object is read in. For a root-context object that origin is 0,0 and
/// the projection is the identity, which is why W-0 through W-5 never had to
/// name it.
///
/// IT ASKS THE RESOLVER FOR THAT ORIGIN (`ui::frame_in`) rather than working it
/// out. A gesture that reasoned "the source is at 3,2, so subtract 3 and 2"
/// would be a second copy of the geometry, and W-1's lesson is that the second
/// copy is the one that goes stale. It also inherits, free, the answer for the
/// case a source is missing: an empty frame, for an object the resolver did not
/// place, so `doc::move` still judges an ordinary proposal.
///
/// THE CLAMP IS IN GLOBAL CELLS, FOR EVERYBODY. The proposal is reduced to the
/// first cell the workspace has BEFORE the projection, so a hand stops where a
/// maker can see it stop -- at the workspace edge -- whether the object it is
/// dragging measures against the root or against something else. What that stop
/// is authored AS then differs, correctly: at the root it is x = 0, and inside a
/// frame whose origin is at 3 it is x = -3, which since W-6 is an ordinary
/// authorable offset (see doc::check_coord). The document still judges the
/// result and there is no path here that writes past its refusal.
inline Handled place(WorkshopDoc& d, const ui::Scene& scene, std::int64_t id, std::int64_t gx,
                     std::int64_t gy) {
    const ui::Element* e = doc::find(d, id);
    if (e == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    Handled done;
    if (gx < doc::kFirstCell) {
        gx = doc::kFirstCell;
        done.boundary = kAtWorkspaceStart;
    }
    if (gy < doc::kFirstCell) {
        gy = doc::kFirstCell;
        done.boundary = kAtWorkspaceStart;
    }
    const ui::Rect frame = ui::frame_in(scene, *e);
    done.written = doc::move(d, id, detail::minus(gx, frame.x), detail::minus(gy, frame.y));
    return done;
}

/// Step the selected object one cell — the keyboard's move gesture, and the only
/// one the canonical POSIX lane can perform at all (that lane produces no pointer
/// events; see workshop.cpp).
///
/// It proposes a position and lets `place` + `doc::move` decide, exactly as a
/// drag does. Two gestures, one write path.
///
/// It steps the RESOLVED position, not the authored one, and that is not a
/// detail: `place` now speaks workspace cells, and a maker pressing `l` means
/// "one cell to the right on the screen" whatever frame the object is authored
/// in. Stepping the authored offset and stepping the resolved position happen to
/// agree (both add one) -- but only because a frame's origin does not move when
/// its dependent does, and going through the resolved position is what makes the
/// keyboard and the pointer literally one path rather than two that agree.
inline Handled nudge(WorkshopDoc& d, Session& s, std::int64_t ddx, std::int64_t ddy) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, s.selected);
    if (placed == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    return place(d, scene, s.selected, detail::step(placed->rect.x, ddx),
                 detail::step(placed->rect.y, ddy));
}

// ---- The size a hand asked for, as an authored extent ----------------------------------

/// The authored extent a maker's HAND asks for, when it asks for a resolved size.
///
/// NOT AN INVERSE, and the name is the first line of the argument.
/// `ui::resolve_extent` is many-to-one -- it floors a share and it clamps an
/// out-of-range one -- so it has no inverse at all, which is precisely what W-2
/// measured as the reason authored PLACEMENT stayed raw cells. This function does
/// something different and weaker: it AUTHORS A NEW VALUE that this viewport
/// resolves to what the maker asked for. It does not reconstruct the value that
/// was there before and it cannot, because several percentages name the same cell
/// count (at a 48-cell workspace, 59% and 60% are both 28 cells -- 58% is 27, so
/// the range this comment used to name was one wider than the truth). A maker who
/// drags a 60%-wide object out and back gets the same PICTURE and a different
/// NUMBER, and that is a true fact about shares rather than a defect.
///
/// MODE IS PRESERVED. A resize is a direct manipulation of a property the maker
/// already authored, so it writes that property in the spelling they authored it
/// in: a share stays a share, cells stay cells. Converting 60% into 34 cells
/// because cells are easier to compute would silently destroy the only part of
/// the value that survives the next workspace change -- and §24's live evidence
/// is exactly that: after resize the cells object holds its size and the share
/// object still moves with the workspace.
///
/// THE ROUNDING RULE IS NOT A TASTE, IT IS FORCED. The resolver FLOORS, so the
/// projection must take the SMALLEST share this viewport resolves to at least the
/// asked-for size. Choosing the two rules independently is what makes a value
/// walk: `nearest` sends 60% (28 cells at 48) to 58%, which resolves to 27, so
/// merely grabbing an edge and letting go would shrink the object by a cell.
/// `ceil` round-trips exactly wherever a percent is finer than a cell (any
/// workspace of 100 cells or fewer, which is every workspace this tool has), and
/// where it is not, it is still the smallest share that COVERS what was asked.
///
/// AND IT ASKS THE RESOLVER RATHER THAN RE-DERIVING ITS ARITHMETIC. A hundred
/// candidates is nothing, and the payment is large: the projection cannot drift
/// from the resolver, it inherits the resolver's totality over hostile values for
/// free (no `100 * want` to overflow), and "the authored value resolves to what
/// the maker asked for" is true by construction rather than by two functions
/// agreeing. That is W-1's one-place-resolves lesson, spent.
///
/// The clamp is the gesture layer's, per this header's boundary policy: `want` is
/// first reduced to the reachable band, and `boundary` says which wall it met.
/// The band is asked of the document's own limits and of the resolver -- never
/// invented here -- so the extent this returns is always one `doc::check_extent`
/// accepts, and `doc::resize` still gets to judge it.
inline ui::Extent extent_from_drag(const ui::Extent& current, std::int64_t want,
                                   std::int64_t span, std::string& boundary) {
    if (current.mode == ui::kExtentPercent) {
        const std::int64_t least = ui::resolve_extent(ui::Extent{ui::kExtentPercent, 1}, span);
        const std::int64_t most = ui::resolve_extent(ui::Extent{ui::kExtentPercent, 100}, span);
        if (want < least) {
            want = least;
            boundary = kAtSmallest;
        } else if (want > most) {
            want = most;
            // The wall a share meets at the far end is not the workspace being a
            // wall -- placement has no such limit and a cells extent has none
            // either. It is the vocabulary: a share OF something cannot be more
            // than the whole of it, so 100% is where this mode stops.
            boundary = kAtWholeContext;
        }
        if (ui::resolve_extent(current, span) == want) {
            return current; // this share already says exactly that: do not re-author it
        }
        for (std::int64_t pct = 1; pct <= 100; ++pct) {
            const ui::Extent candidate{ui::kExtentPercent, pct};
            if (ui::resolve_extent(candidate, span) >= want) {
                return candidate;
            }
        }
        return ui::Extent{ui::kExtentPercent, 100};
    }
    // Cells, and anything a poke wrote that is neither: an absolute size, whose
    // limits are the document's and have nothing to do with the workspace. An
    // object may be authored WIDER than the workspace for the same reason W-2 let
    // one be positioned past its right edge -- the canvas clips, and a maker who
    // did that has not made a mistake.
    if (want < ui::kMinCells) {
        want = ui::kMinCells;
        boundary = kAtSmallest;
    } else if (want > doc::kMaxCells) {
        want = doc::kMaxCells;
        boundary = kAtLargest;
    }
    return ui::Extent{ui::kExtentCells, want};
}

/// Author a new size from a proposal in RESOLVED cells — the shape both the
/// pointer and the keyboard arrive in, and the one place either of them becomes
/// an authored extent.
///
/// Both extents are projected, then written by ONE `doc::resize`, so the
/// atomicity the document promises is not undone by the gesture proposing twice.
///
/// THE SPAN IS THE CONTEXT'S, and W-6's whole change to resizing is those three
/// words. `extent_from_drag` asks "which share of this span resolves to what the
/// hand wants"; before, the span was always the workspace's, because that was
/// the only thing a share could be a share of. It is now whatever frame the
/// resolver says this object is read in, asked for with `ui::frame_in`. Nothing
/// else moved: there is no second projection, no "child resize" path, and no
/// branch on whether an object has a context. The existing operation was simply
/// being handed the wrong context all along, and now it is handed the right one.
inline Handled size_to(WorkshopDoc& d, const Session& s, std::int64_t id, std::int64_t want_w,
                       std::int64_t want_h) {
    const ui::Element* e = doc::find(d, id);
    if (e == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    const ui::Rect frame = ui::frame_in(workspace_scene(d, s), *e);
    Handled done;
    const ui::Extent w = extent_from_drag(e->width, want_w, frame.w, done.boundary);
    const ui::Extent h = extent_from_drag(e->height, want_h, frame.h, done.boundary);
    done.written = doc::resize(d, id, w, h);
    return done;
}

/// Grow or shrink the selected object by whole RESOLVED cells — the keyboard's
/// resize gesture, and the canonical lane's only one.
///
/// It asks for "one more cell than I can see", which is the same question the
/// pointer asks by landing one cell further out, and it goes through the same
/// projection and the same document operation. A maker who grows and shrinks
/// returns to the same size; on a share they may not return to the same NUMBER,
/// because the projection authors the one canonical share for a given cell count.
inline Handled grow(WorkshopDoc& d, Session& s, std::int64_t dw, std::int64_t dh) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, s.selected);
    if (placed == nullptr) {
        return Handled::of(Written::no("no such object"));
    }
    return size_to(d, s, s.selected, detail::step(placed->rect.w, dw),
                   detail::step(placed->rect.h, dh));
}

// ---- The one resize affordance ---------------------------------------------------------

/// Where the selected object's size handle is, in WORKSPACE cells.
///
/// WORKSHOP PRESENTATION, not authored content. It is not a ui::Element, has no
/// identity of its own, is not a child of anything, and nothing persists it -- it
/// is DERIVED from the selected object's resolved `Placed::rect` every time it is
/// wanted, exactly as the canvas and the hit test are. `zengine::ui` never learns
/// that handles exist; the generic vocabulary already carried this without
/// changing, which is the third phase in a row it has.
///
/// It sits on the selection ring's bottom-right corner cell -- one past the
/// object's own last cell -- so it steals no content cell, it lands where a hand
/// expects a corner grip, and the arithmetic is exact: a pointer AT the handle
/// proposes the size the object already has.
///
/// `shown` is false when it would fall outside the workspace, and that is one
/// fact rather than two: what a maker cannot see, a maker cannot grab. An object
/// authored wider than the workspace is therefore resizable through the inspector
/// and the keyboard but not by hand, which is honest -- the grip is off-screen.
struct Handle {
    bool shown = false;
    std::int64_t id = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
};

inline Handle size_handle(const WorkshopDoc& d, const Session& s) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, s.selected);
    if (placed == nullptr) {
        return Handle{};
    }
    // Asked without performing the addition: `rect.x + rect.w` is not
    // representable for every rect a poked extent can produce, and an overflow
    // here would put the grip somewhere the object is not.
    const std::int64_t back = detail::minus(0, placed->rect.x);
    const std::int64_t room = detail::minus(s.workspace_w, placed->rect.x);
    const std::int64_t up = detail::minus(0, placed->rect.y);
    const std::int64_t down = detail::minus(s.workspace_h, placed->rect.y);
    if (placed->rect.w < back || placed->rect.w >= room || placed->rect.h < up ||
        placed->rect.h >= down) {
        return Handle{};
    }
    return Handle{true, s.selected, placed->rect.x + placed->rect.w,
                  placed->rect.y + placed->rect.h};
}

/// Take hold of whatever authored object is under a workspace cell. Returns the
/// identity taken hold of, or 0 for empty space.
///
/// The hit test is the SAME one the canvas is painted from -- `ui::hit` over
/// `workspace_scene` -- so what a maker can see is what they can grab. There is
/// no second geometry test for dragging, which is how a drag and a click cannot
/// come to disagree about which object they are talking about.
///
/// The grabbed point is recorded as an offset from the object's RESOLVED
/// placement — where inside the rectangle on screen the maker took hold — and it
/// is a plain subtraction in the coordinates the pointer already speaks.
///
/// W-2 wrote this as an offset from AUTHORED placement and observed that the two
/// were the same number, then drew the sharp conclusion that an authored
/// position "relative to something else" could not be dragged, because the
/// gesture would have to invert the resolver and the resolver is not invertible.
/// W-6 built exactly that position, and the conclusion turned out to be true of
/// EXTENTS and false of PLACEMENT. What is not invertible in the resolver is the
/// share arithmetic -- it floors and it clamps, which is why `extent_from_drag`
/// authors a new value rather than recovering the old one. Placement composes by
/// ADDITION, and a sum has an inverse: the hand's global answer minus the
/// context's origin is the authored offset, exactly. So a relative position is
/// draggable without a projection, and it is `place` that performs the one
/// subtraction.
///
/// It does NOT change the selection. Composing that is the caller's gesture: a
/// press on empty space should not silently mean "deselect".
inline std::int64_t begin_drag(const WorkshopDoc& d, Session& s, std::int64_t cx,
                               std::int64_t cy) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* under = ui::hit(scene, cx, cy);
    if (under == nullptr) {
        s.drag = Drag{};
        return 0;
    }
    s.drag = Drag{true, false, under->id, detail::minus(cx, under->rect.x),
                  detail::minus(cy, under->rect.y)};
    return under->id;
}

/// What a press takes hold of: the selected object's SIZE HANDLE if the press
/// landed on it, otherwise whatever object's body is under the cell. Returns the
/// identity taken hold of, or 0.
///
/// THE PRIORITY LIVES HERE, and that is the point of the function existing. The
/// handle sits one cell outside its own object, so it can sit on top of a
/// neighbour, and something has to say which one a press means. `ui::hit` must
/// not: it answers "which authored element is under this cell", and a handle is
/// not an authored element -- teaching the package about affordances would be
/// teaching a shared vocabulary one application's furniture. So the package still
/// gives the whole answer about the DOCUMENT and Workshop simply consults its own
/// affordance first, in Workshop's own gesture, where a suite can drive it.
inline std::int64_t take_hold(WorkshopDoc& d, Session& s, std::int64_t cx, std::int64_t cy) {
    const Handle handle = size_handle(d, s);
    if (handle.shown && handle.x == cx && handle.y == cy) {
        s.drag = Drag{true, true, handle.id, 0, 0};
        return handle.id;
    }
    return begin_drag(d, s, cx, cy);
}

/// Where the gesture in flight now proposes the object should BE, or how big it
/// should be — committed through the document's one position operation or its one
/// size operation.
///
/// It writes AUTHORED state, and only ever that. Nothing here touches a Rect, a
/// Placed or a Scene: those are the observation, they are rebuilt from the
/// authored state on the next paint, and a gesture that moved or stretched them
/// would have changed the picture without changing the thing.
///
/// The resize arm reads the object's own left/top edge and asks for
/// `pointer - edge` cells, which is exact because the handle sits one cell past
/// the object: a pointer resting ON the handle proposes the size it already has.
/// The subtraction saturates for the same reason `begin_drag`'s does -- both
/// terms are values this weave does not own.
inline Handled drag_to(WorkshopDoc& d, const Session& s, std::int64_t cx, std::int64_t cy) {
    if (!s.drag.active) {
        return Handled::of(Written::no("nothing is being dragged"));
    }
    const ui::Scene scene = workspace_scene(d, s);
    if (s.drag.resizing) {
        // The object's RESOLVED left/top edge, because the pointer and the
        // handle are both in workspace cells. Before W-6 this read the authored
        // `e->x`, which was the same number; it is not any more, and reading the
        // authored one would have asked for a size measured from the wrong
        // corner the moment the object had a context.
        const ui::Placed* placed = ui::placed_for(scene, s.drag.id);
        if (placed == nullptr) {
            return Handled::of(Written::no("no such object"));
        }
        return size_to(d, s, s.drag.id, detail::minus(cx, placed->rect.x),
                       detail::minus(cy, placed->rect.y));
    }
    return place(d, scene, s.drag.id, detail::minus(cx, s.drag.grab_dx),
                 detail::minus(cy, s.drag.grab_dy));
}

inline void end_drag(Session& s) { s.drag = Drag{}; }

// ---- Where a pointer is, in workspace cells --------------------------------------------
//
// This lives here and not in the weave because it is the only NEW arithmetic on
// the pointer path, it is the arithmetic a hostile value could break, and code in
// workshop.cpp's anonymous namespace is code no suite can reach. (The weave's
// message handlers themselves are still unreachable that way -- see the report;
// what this relocation buys is that the part with an undefined-behaviour edge is
// not among them.)

/// The canvas's first row on the terminal, 0-based: the Skin puts a canvas at
/// terminal row 3, and a pointer reports where it is on the terminal.
inline constexpr std::int64_t kCanvasTopRow = 2;

/// The workspace cell a reported pointer position lands on.
///
/// W-4 deleted the conversion that used to stand in front of these. `PointerMoved`
/// and `PointerButton` carry int64 cells and say so (`space`), so there is no
/// double to narrow and no NaN to defend against -- the backend reports the unit
/// it means, and this is a translation of origin and nothing else.
///
/// The saturation stays, and for the unchanged reason: the numbers come off the
/// wire from whichever weave holds the input role, a backend is a weave like any
/// other, and `INT64_MIN - 3` is undefined behaviour produced by data. The
/// saturated end is far outside any canvas, which already means "nothing there".
inline std::int64_t workspace_cell_x(std::int64_t pointer_x) noexcept {
    return detail::minus(pointer_x, kWorkspaceX);
}
inline std::int64_t workspace_cell_y(std::int64_t pointer_y) noexcept {
    return detail::minus(detail::minus(pointer_y, kCanvasTopRow), kWorkspaceY);
}

/// The whole screen as one published canvas.
///
/// Painter's order, which is list order: the workspace backdrop, then each
/// authored element as the scene placed it (with the selected one's ring UNDER
/// it, so the ring reads as a ring rather than a border the object grew), then
/// every label over everything.
///
/// The picture is derived from `workspace_scene()` and from nothing else, which
/// is what makes "what you see is what the hit test answers about" structural
/// rather than a claim: the two read one value.
inline surface::SurfaceCanvas paint(const WorkshopDoc& d, const Session& s) {
    surface::SurfaceCanvas c;
    c.width = kScreenW;
    c.height = kScreenH;

    const auto rect = [&c](std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h,
                           std::int64_t role) {
        c.rects.push_back(surface::SurfaceRect{x, y, w, h, role});
    };
    const auto label = [&c](std::int64_t x, std::int64_t y, std::string text, std::int64_t role) {
        c.labels.push_back(surface::SurfaceLabel{x, y, std::move(text), role});
    };

    // The workspace, as a thing with edges a maker can see. Its extent is a
    // session fact, so resizing it visibly changes what a share resolves to
    // while changing no authored value at all.
    rect(kWorkspaceX, kWorkspaceY, s.workspace_w, s.workspace_h, surface::role::kMuted);

    // The scene: the authored elements, as this workspace places them. Painting
    // walks the SCENE, not the document -- so a rectangle on screen is by
    // construction a rectangle the hit test can find.
    const ui::Scene scene = workspace_scene(d, s);
    for (const ui::Placed& p : scene.items) {
        const std::int64_t x = kWorkspaceX + p.rect.x;
        const std::int64_t y = kWorkspaceY + p.rect.y;
        if (p.id == s.selected) {
            rect(x - 1, y - 1, p.rect.w + 2, p.rect.h + 2, surface::role::kAccent);
        }
        rect(x, y, p.rect.w, p.rect.h, surface::role::kFill);
        // The label, written on the object, clipped to the workspace rather than
        // allowed to run into the panel beside it. The label is authored, so it
        // is read from the element and not from the observation of it.
        const ui::Element* authored = doc::find(d, p.id);
        const std::int64_t room = s.workspace_w - p.rect.x;
        if (authored != nullptr && room > 0) {
            std::string shown = authored->label;
            if (static_cast<std::int64_t>(shown.size()) > room) {
                shown.resize(static_cast<std::size_t>(room));
            }
            label(x, y, shown, surface::role::kMuted);
        }
    }

    // The size handle, over everything in the workspace, as a GLYPH rather than
    // as another rectangle. That is not decoration: the ring already paints this
    // exact cell in the accent role, so a rect here would be invisible, and the
    // affordance has to be distinguishable from the ring, from the object's body
    // and from the workspace at a glance. `SurfaceLabel` carries arbitrary text
    // over the rects, so the generic canvas vocabulary already had what this
    // needed -- no role was added, and nothing in surface/ or ui/ changed.
    // (Honest cost: a Skin with no text stack draws no handle. The SDL skin is
    // that Skin today -- P8, unchanged by this phase.)
    const Handle handle = size_handle(d, s);
    if (handle.shown) {
        label(kWorkspaceX + handle.x, kWorkspaceY + handle.y, kHandleGlyph,
              surface::role::kAccent);
    }

    label(0, 0,
          "WORKSPACE " + std::to_string(s.workspace_w) + "x" + std::to_string(s.workspace_h) +
              " cells",
          surface::role::kAccent);

    // The object list: the same objects, named by identity, pointing at the same
    // selection the ring in the workspace does.
    label(kPanelX, kListY - 1, "OBJECTS", surface::role::kAccent);
    std::int64_t line = 0;
    for (const ui::Element& e : d.elements) {
        if (line >= kListRows) {
            break;
        }
        const bool chosen = e.id == s.selected;
        label(kPanelX, kListY + line,
              std::string(chosen ? "> " : "  ") + "#" + std::to_string(e.id) + " " + e.label,
              chosen ? surface::role::kAccent : surface::role::kFill);
        ++line;
    }
    // An empty document SAYS it is empty. W-2 is the phase that made emptiness
    // reachable by a maker's own hand rather than only by a suite constructing
    // one, and a panel that merely goes blank is indistinguishable from a tool
    // that has broken. It also says what to do next, because the answer is one
    // key and the alternative is a maker who thinks they have destroyed it.
    if (d.elements.empty()) {
        label(kPanelX, kListY, "(none) -- n makes one", surface::role::kMuted);
    }

    // The inspector.
    label(kPanelX, kRowsY - 1, "PROPERTIES", surface::role::kAccent);
    if (s.rows.empty()) {
        label(kPanelX, kRowsY, "(nothing selected)", surface::role::kMuted);
    }
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        const Row& row = s.rows[i];
        const bool here = i == s.cursor;
        std::int64_t role = surface::role::kFill;
        if (row.editing()) {
            role = surface::role::kAlert; // a live draft is never quiet
        } else if (!row.editable()) {
            role = surface::role::kMuted; // not the maker's to author
        }
        label(kPanelX, kRowsY + static_cast<std::int64_t>(i),
              std::string(here ? ">" : " ") + detail::pad(row.label(), 9) + row.display(), role);
    }

    if (!s.notice.empty()) {
        label(0, kNoticeY, s.notice,
              s.notice_is_bad ? surface::role::kAlert : surface::role::kFill);
    }
    // Two lines, because the canvas clips at its own width and a help line that
    // silently loses its last hint is worse than no hint. The maker's gestures
    // come first; the tool's own furniture second.
    //
    // It advertises `shift+hjkl` and not `,. width | -= height`, which is a
    // REPAIR and not a W-5 addition: W-4 deleted those four bindings and left
    // the help line naming them, so until now the tool's own instructions told
    // a maker to press keys that do nothing.
    label(0, kHelpY, "n new | d delete | hjkl move | shift+hjkl size | tab object | q quit",
          surface::role::kMuted);
    label(0, kHelpY + 1,
          "enter edit | esc cancel | up/down row | [ ] workspace | ^s save | ^o open",
          surface::role::kMuted);

    return c;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
