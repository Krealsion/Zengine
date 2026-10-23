// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_SHOWN_PICTURE_HPP
#define ZENGINE_TESTS_SHOWN_PICTURE_HPP

// A pane's canvas picture read back from what a graphics window holds of its area, as
// `workshop::canvas_sketch` draws one: the rectangles and runs of text under its `picture` group in
// the order they paint, each run where its words stand and with the caret and selection its marks
// show, and each part under its `parts` group, by the name its record asks and the place its group
// stands. A case reads what the window shows, never what the pane meant to send.

#include "graphics/ops.hpp"
#include "graphics/window/store.hpp"
#include "workshop/pane_canvas_text.hpp"
#include "workshop/pane_picture.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace zengine::testing {

inline workshop::CanvasPicture shown_picture(const graphics::window::WinArea& area,
                                             const workshop::CanvasRoom& room) {
    namespace gfx = zengine::graphics;
    namespace gw = zengine::graphics::window;
    workshop::CanvasPicture out;
    const auto metrics = workshop::canvas_text_metrics(room);
    const std::int64_t inset = metrics.inset;
    std::map<std::string, std::size_t> runs; ///< each run's node, by the text it reads back as
    std::map<std::size_t, std::int64_t> glyphs_x; ///< ...and where its words begin
    const auto at = gw::index_of(area);
    std::vector<const gw::KeptNode*> order;
    for (const gw::KeptNode& k : area.nodes) order.push_back(&k);
    std::stable_sort(order.begin(), order.end(), [&](const auto* x, const auto* y) {
        return gw::key_of(at, x->node.path) < gw::key_of(at, y->node.path);
    });
    const auto record_of = [&](const std::string& path) -> const gfx::ElementRecord* {
        for (const gfx::ElementRecord& r : area.records) {
            if (r.path == path) return &r;
        }
        return nullptr;
    };
    for (const gw::KeptNode* k : order) {
        const std::string& path = k->node.path;
        if (path.rfind("picture/", 0) == 0) {
            if (k->node.op == gfx::op::kRect) {
                if (const auto r = gfx::from_body<gfx::RectArgs>(k->node.args)) {
                    out.rects.push_back(workshop::CanvasRect{r->x, r->y, r->w, r->h, r->fill.role});
                }
            } else if (k->node.op == gfx::op::kText) {
                if (const auto t = gfx::from_body<gfx::TextArgs>(k->node.args)) {
                    runs[path.substr(8)] = out.texts.size();
                    glyphs_x[out.texts.size()] = t->x;
                    out.texts.push_back(
                        workshop::CanvasText{t->x - inset, t->y - inset, t->text, t->ink.role});
                }
            }
        } else if (path.rfind("parts/", 0) == 0 && path.find('/', 6) == std::string::npos &&
                   k->node.op == gfx::op::kGroup) {
            const auto g = gfx::from_body<gfx::GroupArgs>(k->node.args);
            const gfx::ElementRecord* r = record_of(path);
            if (!g || r == nullptr || r->action.empty()) continue;
            out.parts.push_back(workshop::CanvasPart{r->action, g->x, g->y, g->clip_w, g->clip_h,
                                                     r->type.empty()});
        } else if (path.rfind("marks/", 0) == 0) {
            const std::string mark = path.substr(6);
            const bool caret = mark.rfind("caret", 0) == 0;
            const auto run = runs.find(mark.substr(caret ? 5 : 3));
            if (run == runs.end() || metrics.advance <= 0) continue;
            workshop::CanvasText& t = out.texts[run->second];
            std::int64_t x = 0;
            std::size_t width = 0;
            if (const auto bar = gfx::from_body<gfx::LineArgs>(k->node.args)) {
                x = bar->x1;
            } else if (const auto words = gfx::from_body<gfx::TextArgs>(k->node.args)) {
                x = words->x;
                width = words->text.size();
            } else {
                continue;
            }
            const std::int64_t column = (x - glyphs_x[run->second]) / metrics.advance;
            if (caret) {
                t.caret_col = column;
            } else {
                t.sel_begin_col = column;
                t.sel_end_col = column + static_cast<std::int64_t>(width);
            }
        }
    }
    return out;
}

} // namespace zengine::testing

#endif // ZENGINE_TESTS_SHOWN_PICTURE_HPP
