// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The screen's arrangement rings.
// Workshop law: agents/workshop/arrangement.md (+2 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// WL-ARR-09 -- agents/workshop/arrangement.md
// WL-ARR-17 -- agents/workshop/arrangement-snap.md
// WL-PANE-01 -- agents/workshop/panes-and-windows.md
// WL-FRONT-01 -- agents/workshop/planes.md
void paint_pane_affordances(ScreenPlane& layer, const Session& s,
                            const Screen& sc) {
    if (!s.arrange.open) {
        return;
    }
    // THE LINES A HELD SNAP MET, across the room, one device unit wide and inside it: over every
    // pane, beneath the rings, gone with the gesture.
    if (s.pane_drag.active) {
        const std::int64_t grain = chrome_grain(sc);
        const auto inside = [grain](std::int64_t line, std::int64_t extent) {
            return line < 0 ? std::int64_t{0} : (line > extent - grain ? extent - grain : line);
        };
        if (s.pane_drag.met_x.has_value()) {
            layer.rects.push_back(wire_rect_of(
                PixelRect{inside(*s.pane_drag.met_x, sc.room_w), sc.room_y, grain, sc.room_h},
                graphics::role::kAccent));
        }
        if (s.pane_drag.met_y.has_value()) {
            layer.rects.push_back(wire_rect_of(
                PixelRect{0, graphics::add_cells(sc.room_y, inside(*s.pane_drag.met_y, sc.room_h)),
                          sc.room_w, grain},
                graphics::role::kAccent));
        }
    }
    const auto ring = [&](const PaneRef& ref, bool emphasized) {
        const std::optional<std::int64_t> kind = resolve_pane(ref, s.panes);
        // Every pane this build can resolve wears handles; only a reference that resolves to no
        // kind stops a ring.
        if (!kind.has_value()) {
            return;
        }
        const PaneBounds where = bounds_of(s.panes, s.setup.active, *kind, sc);
        if (!where.open || where.rect.w <= 0 || where.rect.h <= 0) {
            return;
        }
        const bool held = s.pane_drag.active && s.pane_drag.sizing && s.pane_drag.pane == ref;
        for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
            const PixelRect at = pane_edge_cell(where.rect, edge);
            const bool chosen = held ? s.pane_drag.edge == edge : emphasized;
            layer.labels.push_back(ScreenLabel{
                at.x, at.y, std::string(pane_edge_glyph(edge)),
                chosen ? graphics::role::kAccent : graphics::role::kMuted});
        }
    };
    paint_arranging_caption(layer, s, sc);
    if (!s.arrange.desk) {
        if (s.arrange.addressed()) {
            ring(s.arrange.pane, true);
        }
        return;
    }
    for (const SetupPane& row : s.setup.active.panes) {
        ring(row.ref, s.arrange.addressed() && row.ref == s.arrange.pane);
    }
}

// WL-ARR-18 -- agents/workshop/arrangement.md
std::string arranging_caption_subject(const Session& s) {
    if (s.arrange.desk) {
        return "all panes";
    }
    const std::optional<std::int64_t> kind = resolve_pane(s.arrange.pane, s.panes);
    return kind.has_value() ? kind_name(s.panes, *kind) : ref_text(s.arrange.pane);
}

// WL-ARR-18 -- agents/workshop/arrangement.md
PixelRect arranging_caption_rect(const Session& s, const Screen& sc) {
    if (!s.arrange.open) {
        return PixelRect{};
    }
    const std::int64_t advance = sc.text_advance_px > 0 ? sc.text_advance_px : graphics::kCellPx;
    const std::int64_t line = sc.text_line_px > 0 ? sc.text_line_px : graphics::kCellPx;
    const std::size_t longest = (std::max)(std::char_traits<char>::length(kArrangingCaption),
                                           arranging_caption_subject(s).size());
    const std::int64_t w = (std::min)((static_cast<std::int64_t>(longest) + 4) * advance, sc.w);
    return PixelRect{(sc.w - w) / 2 / advance * advance, sc.room_y + line, w, 4 * line};
}

// WL-ARR-18 -- agents/workshop/arrangement.md
void paint_arranging_caption(ScreenPlane& layer, const Session& s, const Screen& sc) {
    if (!s.arrange.open) {
        return;
    }
    // THE MODE NAMED WHERE IT CANNOT BE MISSED: its words, and beneath them what it arranges, on a
    // ground of their own at the top of the room, set on the lattice of the medium's text.
    const std::string what = arranging_caption_subject(s);
    const std::string head = kArrangingCaption;
    const std::int64_t advance = sc.text_advance_px > 0 ? sc.text_advance_px : graphics::kCellPx;
    const std::int64_t line = sc.text_line_px > 0 ? sc.text_line_px : graphics::kCellPx;
    const PixelRect box = arranging_caption_rect(s, sc);
    const std::int64_t x = box.x, y = box.y, w = box.w;
    layer.rects.push_back(ScreenRect{box.x, box.y, box.w, box.h, graphics::role::kGround});
    const auto centred = [&](const std::string& words) {
        const std::int64_t used = static_cast<std::int64_t>(words.size()) * advance;
        return x + (used < w ? (w - used) / 2 / advance * advance : 0);
    };
    layer.labels.push_back(ScreenLabel{centred(head), y + line, head, graphics::role::kAccent});
    layer.labels.push_back(ScreenLabel{centred(what), y + 2 * line, what, graphics::role::kMuted});
}

} // namespace zengine::workshop
