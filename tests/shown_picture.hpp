// SPDX-License-Identifier: AGPL-3.0-only
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
#include "pane/canvas_text.hpp"
#include "pane/picture.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace zengine::testing {

inline pane::CanvasPicture shown_picture(const graphics::window::WinArea& area,
                                             const pane::CanvasRoom& room) {
    namespace gfx = zengine::graphics;
    namespace gw = zengine::graphics::window;
    pane::CanvasPicture out;
    const auto metrics = pane::canvas_text_metrics(room);
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
            const gfx::Look* look = gw::look_of(area, k->node);
            if (const auto* r = gfx::operands<gfx::RectArgs>(k->node)) {
                const gfx::Paint* paint = gfx::look_as<gfx::Paint>(look);
                out.rects.push_back(pane::CanvasRect{r->x, r->y, r->w, r->h,
                                                     zengine::graphics::pool_color_of(paint != nullptr ? paint->fill : gfx::Color{})});
            } else if (const auto* t = gfx::operands<gfx::TextArgs>(k->node)) {
                const gfx::TextLook* words = gfx::look_as<gfx::TextLook>(look);
                runs[path.substr(8)] = out.texts.size();
                glyphs_x[out.texts.size()] = t->x;
                out.texts.push_back(pane::CanvasText{
                    t->x - inset, t->y - inset, t->text,
                    zengine::graphics::pool_color_of(words != nullptr ? words->ink : gfx::Color{})});
            }
        } else if (path.rfind("parts/", 0) == 0 && path.find('/', 6) == std::string::npos &&
                   gfx::operands<gfx::GroupArgs>(k->node) != nullptr) {
            const auto g = gfx::operands<gfx::GroupArgs>(k->node);
            const gfx::ElementRecord* r = record_of(path);
            if (!g || r == nullptr || r->action.empty()) continue;
            out.parts.push_back(pane::CanvasPart{r->action, g->x, g->y, g->clip_w, g->clip_h,
                                                     r->type.empty()});
        } else if (path.rfind("marks/", 0) == 0) {
            const std::string mark = path.substr(6);
            const bool caret = mark.rfind("caret", 0) == 0;
            const auto run = runs.find(mark.substr(caret ? 5 : 3));
            if (run == runs.end() || metrics.advance <= 0) continue;
            pane::CanvasText& t = out.texts[run->second];
            std::int64_t x = 0;
            std::size_t width = 0;
            if (const auto bar = gfx::operands<gfx::LineArgs>(k->node)) {
                x = bar->x1;
            } else if (const auto words = gfx::operands<gfx::TextArgs>(k->node)) {
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
