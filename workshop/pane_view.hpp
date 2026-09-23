// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_VIEW_HPP
#define ZENGINE_WORKSHOP_PANE_VIEW_HPP
#include <zen/weave/shape.hpp>
#include <cstdint>
#include <string>
#include <vector>
namespace zengine::workshop {
// Visible text presentation, not a semantic command or an authority to interact.
struct PaneViewRequested {
    std::string provider, pane;
    ZEN_SHAPE(PaneViewRequested, 1, ZEN_FIELD(provider), ZEN_FIELD(pane));
};
struct PaneViewRow {
    std::int64_t row = 0, x = 0, y = 0, space = 0;
    std::string text;
    ZEN_SHAPE(PaneViewRow, 1, ZEN_FIELD(row), ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(space), ZEN_FIELD(text));
};
struct PaneView {
    std::string provider, pane;
    std::int64_t picture = 0;
    std::vector<PaneViewRow> rows;
    ZEN_SHAPE(PaneView, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture), ZEN_FIELD(rows));
};
} // namespace zengine::workshop
#endif
