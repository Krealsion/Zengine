// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_SCREEN_HPP
#define ZENGINE_WORKSHOP_SCREEN_HPP

// The Workshop screen: the session facts, the inspector's rows, and the one
// function that turns both into a published canvas.
//
// PURE, and that is the point of it being its own header: paint() takes a
// document and a session and returns a SurfaceCanvas. No terminal, no window, no
// weave, no bus. So the suite pins an entire Workshop screen -- the rectangle,
// the selection ring, the object list, the inspector, a refusal -- as a value it
// can assert on, and the only thing left for the live run to prove is that the
// bus and the Skin carry it, which is a different claim from "the screen is
// right".
//
// It is also where Workshop DECIDES WHERE THINGS GO, and that is not a gap in
// the canvas vocabulary being papered over -- it is the boundary. SurfaceCanvas
// paints a picture; whoever publishes one has already done the layout. W-0 does
// its layout in constants, on purpose: a layout ENGINE is what the Loom's
// geometry-free semantic tree is for (loom::Widget + px_layout), and that tree is
// not reachable from a Zengine consumer today (the report's Loom pressure). One
// screen's worth of constants is the honest amount of layout to invent while
// that is true.

#include "document.hpp"
#include "property.hpp"
#include "vocabulary.hpp"

#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
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

/// The session: what a maker is currently doing, as opposed to what they have
/// authored. Kept out of WorkshopDoc deliberately (see vocabulary.hpp) so the
/// two kinds of fact cannot be mistaken for each other -- selection is not
/// content, and neither is the size of the window it is being looked at through.
struct Session {
    std::int64_t selected = 0;              ///< the selected object's IDENTITY (0 = none)
    std::int64_t workspace_w = kWorkspaceW; ///< what a share of the workspace currently means
    std::int64_t workspace_h = kWorkspaceH;
    std::size_t cursor = 0;   ///< which inspector row the maker is on
    std::vector<Row> rows;    ///< the inspector, rebuilt when the selection changes
    std::string notice;       ///< the last thing Workshop had to say
    bool notice_is_bad = false; ///< whether that thing was a refusal
};

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
        const WorkshopRect* r = doc::find(d, id);
        if (r == nullptr) {
            return std::string("-");
        }
        return std::to_string(doc::resolve(r->width, ww)) + " x " +
               std::to_string(doc::resolve(r->height, wh)) + " cells";
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

} // namespace detail

/// The whole screen as one published canvas.
///
/// Painter's order, which is list order: the workspace backdrop, then each
/// authored rectangle (with the selected one's ring UNDER it, so the ring reads
/// as a ring rather than a border the object grew), then every label over
/// everything.
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

    for (const WorkshopRect& r : d.rects) {
        const std::int64_t w = doc::resolve(r.width, s.workspace_w);
        const std::int64_t h = doc::resolve(r.height, s.workspace_h);
        const std::int64_t x = kWorkspaceX + r.x;
        const std::int64_t y = kWorkspaceY + r.y;
        if (r.id == s.selected) {
            rect(x - 1, y - 1, w + 2, h + 2, surface::role::kAccent);
        }
        rect(x, y, w, h, surface::role::kFill);
        // The name, written on the object, clipped to the workspace rather than
        // allowed to run into the panel beside it.
        const std::int64_t room = s.workspace_w - r.x;
        if (room > 0) {
            std::string shown = r.name;
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
    for (const WorkshopRect& r : d.rects) {
        if (line >= kListRows) {
            break;
        }
        const bool chosen = r.id == s.selected;
        label(kPanelX, kListY + line,
              std::string(chosen ? "> " : "  ") + "#" + std::to_string(r.id) + " " + r.name,
              chosen ? surface::role::kAccent : surface::role::kFill);
        ++line;
    }

    // The inspector.
    label(kPanelX, kRowsY - 1, "PROPERTIES", surface::role::kAccent);
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
    label(0, kHelpY,
          "tab object | up/down row | enter edit | esc cancel | [ ] workspace | q quit",
          surface::role::kMuted);

    return c;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_SCREEN_HPP
