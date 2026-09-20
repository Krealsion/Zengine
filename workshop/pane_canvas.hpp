// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CANVAS_HPP
#define ZENGINE_WORKSHOP_PANE_CANVAS_HPP
#include "pane_canvas_vocabulary.hpp"
#include <string_view>

namespace zengine::workshop {
inline std::string_view canvas_content_problem(const PaneCanvasContent& c) {
    if (c.grant <= 0 || c.picture <= 0) return "canvas grant and picture must be positive";
    if (c.rects.size() > kPaneCanvasMaxRects || c.labels.size() > kPaneCanvasMaxLabels ||
        c.texts.size() > kPaneCanvasMaxTexts)
        return "canvas primitive budget exceeded";
    const auto role_ok = [](std::int64_t r) { return r >= surface::role::kFill && r <= surface::role::kGround; };
    for (const auto& r : c.rects) {
        if (r.w <= 0 || r.h <= 0) return "canvas rectangles must have positive extents";
        if (!role_ok(r.role)) return "canvas rectangle has an unknown role";
    }
    std::size_t bytes = 0;
    for (const auto& l : c.labels) {
        if (l.text.size() > kPaneCanvasMaxLabelBytes) return "canvas label byte budget exceeded";
        bytes += l.text.size();
        if (bytes > kPaneCanvasMaxTextBytes) return "canvas text byte budget exceeded";
        if (!role_ok(l.role)) return "canvas label has an unknown role";
        for (char byte : l.text) {
            const auto ch = static_cast<unsigned char>(byte);
            if (ch < 32 || ch > 126) return "canvas labels require printable ASCII";
        }
    }
    for (const auto& t : c.texts) {
        if (t.text.size() > kPaneCanvasMaxTextRunBytes) return "canvas text run byte budget exceeded";
        if (t.text.size() > kPaneCanvasMaxTextBytes - bytes) return "canvas text byte budget exceeded";
        bytes += t.text.size();
        if (!role_ok(t.role)) return "canvas text has an unknown role";
        for (char byte : t.text) {
            const auto ch = static_cast<unsigned char>(byte);
            if (ch < 32 || ch > 126) return "canvas text requires printable ASCII";
        }
        const auto length = static_cast<std::int64_t>(t.text.size());
        if (t.caret_col > length) return "canvas text caret exceeds its text";
        const bool begin = t.sel_begin_col >= 0, end = t.sel_end_col >= 0;
        if (begin != end || (begin && (t.sel_end_col < t.sel_begin_col || t.sel_end_col > length)))
            return "canvas text selection is not a range in its text";
    }
    return {};
}
} // namespace zengine::workshop
#endif
