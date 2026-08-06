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

/// A drag in progress. Session, emphatically not content.
///
/// It holds the IDENTITY being moved and where inside that object the maker took
/// hold -- and deliberately not a position. The object's authored place is on the
/// object; a second copy of it here would be the shadow model the whole Workshop
/// arc is arranged to avoid, and it would be the copy that goes stale.
///
/// `grab_dx/dy` are in AUTHORED cells, which costs nothing to convert because
/// authored placement and resolved placement are the same number (`ui::resolve`
/// copies x/y through untouched). See `begin_drag` -- that identity is the reason
/// a drag is a subtraction here rather than an inverse of the resolver.
struct Drag {
    bool active = false;
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
/// The whole list is six calls, and `Width` and `Height` are the two that matter
/// most -- they are the same semantic type, so they share every line of
/// conversion, parsing and refusal wording. A seventh property of an existing
/// type would be one more line here and nothing else anywhere. That is the old
/// builder's per-row plumbing, replaced.
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

} // namespace detail

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
inline std::int64_t create(WorkshopDoc& d, Session& s) {
    const std::int64_t id = doc::add_default(d);
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

/// Step the selected object one cell — the keyboard's move gesture, and the only
/// one the canonical POSIX lane can perform at all (that lane produces no pointer
/// events; see workshop.cpp).
///
/// It proposes a position and lets `doc::move` decide, exactly as a drag does.
/// Two gestures, one write path.
inline Written nudge(WorkshopDoc& d, Session& s, std::int64_t ddx, std::int64_t ddy) {
    const ui::Element* e = doc::find(d, s.selected);
    if (e == nullptr) {
        return Written::no("no such object");
    }
    return doc::move(d, s.selected, detail::step(e->x, ddx), detail::step(e->y, ddy));
}

/// Take hold of whatever authored object is under a workspace cell. Returns the
/// identity taken hold of, or 0 for empty space.
///
/// The hit test is the SAME one the canvas is painted from -- `ui::hit` over
/// `workspace_scene` -- so what a maker can see is what they can grab. There is
/// no second geometry test for dragging, which is how a drag and a click cannot
/// come to disagree about which object they are talking about.
///
/// The grabbed point is recorded as an offset from the object's AUTHORED
/// placement, and it is a plain subtraction because `rect.x` IS `x`: resolution
/// interprets extents and copies placement through untouched. An authored
/// position expressed as a share, or relative to something else, could not be
/// dragged this way -- the gesture would have to invert the resolver, and the
/// resolver is not invertible (it clamps and it floors). That is the sharpest
/// thing W-2 learned about the current placement model.
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
    s.drag = Drag{true, under->id, cx - under->rect.x, cy - under->rect.y};
    return under->id;
}

/// Where the drag now proposes the object should be, committed through the
/// document's one position-writing operation.
///
/// It writes AUTHORED placement. Nothing here touches a Rect, a Placed or a
/// Scene: those are the observation, they are rebuilt from the authored state on
/// the next paint, and a drag that moved them would have moved the picture
/// without moving the thing.
inline Written drag_to(WorkshopDoc& d, const Session& s, std::int64_t cx, std::int64_t cy) {
    if (!s.drag.active) {
        return Written::no("nothing is being dragged");
    }
    return doc::move(d, s.drag.id, cx - s.drag.grab_dx, cy - s.drag.grab_dy);
}

inline void end_drag(Session& s) { s.drag = Drag{}; }

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
    label(0, kHelpY, "n new | d delete | hjkl move | tab object | enter edit | esc cancel",
          surface::role::kMuted);
    label(0, kHelpY + 1, "up/down row | [ ] workspace | q quit", surface::role::kMuted);

    return c;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
