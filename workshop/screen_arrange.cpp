// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The screen's pane management: what a weaver is arranging, and how, and where the keyboard's
// selection moves.
// Workshop law: agents/workshop/focus.md (+9 registers; agents/workshop.md routes)

#include "screen.hpp"

#include <algorithm>
#include <cstdlib>

namespace zengine::workshop {

// ---- PANE MANAGEMENT: what a weaver is ARRANGING, and how ------------------------------

// WL-KEY-03 -- agents/workshop/keyboard.md
KeyContext keyboard_context_beneath_menu(const Session& s) {
    if (s.setup.naming.open) {
        return KeyContext::kNaming;
    }
    if (is_runtime_kind(keyboard_pane(s.panes))) {
        return KeyContext::kPane;
    }
    return KeyContext::kCommand;
}

// WL-KEY-03 -- agents/workshop/keyboard.md
// WL-ARR-14 -- agents/workshop/arrangement.md
KeyContext keyboard_context(const Session& s) {
    if (s.arrange.open && !s.arrange.opened) {
        if (s.arrange.resetting) {
            return KeyContext::kArrangeReset;
        }
        return s.arrange.desk ? KeyContext::kArrangeDesk : KeyContext::kArrangePane;
    }
    // The contextual-action surface is a mode at the top of the modes' band, below the
    // arrangement scopes: it answers before a focused pane, or its navigation keys would leak
    // beneath it. A pane's menu presented by its presenter is the same kind of surface.
    if (s.context.open || s.presented.open) {
        return KeyContext::kContext;
    }
    return keyboard_context_beneath_menu(s);
}

std::int64_t typing_pane(const Session& s) {
    // THE KEY HANDLER'S OWN ORDER, READ AS A VALUE: the resolved context decides
    // (`on(KeyPressed)`).
    if (keyboard_context(s) != KeyContext::kPane) {
        return kNoPaneKind;
    }
    return keyboard_pane(s.panes);
}

/// Where the keys went to a list or to nothing, as against a place a weaver types into, which
/// keeps its own keys: the contexts in which the application's default rows are answered.
// WL-ARR-13, WL-ARR-14 -- agents/workshop/arrangement.md
// WL-DESK-02 -- agents/workshop/desktop.md
bool default_row_context(KeyContext c) {
    return c == KeyContext::kCommand;
}

// ---- MOVING THE KEYBOARD'S SELECTION: reading order and the neighbour in a direction ----------

namespace {

/// One pane the selection may stand on: where it is now, and how far front it is authored.
struct Selectable {
    std::int64_t kind = kNoPaneKind;
    PixelRect rect{};
    std::size_t depth = 0; ///< its place in `presentation_order`, back to front
};

std::vector<Selectable> selectable_rects(const Panes& panes, const Setup& setup,
                                         const Screen& sc) {
    std::vector<Selectable> out;
    const std::vector<std::int64_t> order = presentation_order(setup, panes);
    for (std::size_t depth = 0; depth < order.size(); ++depth) {
        const std::int64_t kind = order[depth];
        if (!kind_takes_keyboard(kind)) {
            continue;
        }
        const PaneBounds where = bounds_of(panes, setup, kind, sc);
        if (!where.open || where.rect.empty()) {
            continue;
        }
        out.push_back(Selectable{kind, where.rect, depth});
    }
    std::stable_sort(out.begin(), out.end(), [](const Selectable& a, const Selectable& b) {
        if (a.rect.y != b.rect.y) {
            return a.rect.y < b.rect.y;
        }
        if (a.rect.x != b.rect.x) {
            return a.rect.x < b.rect.x;
        }
        return a.depth > b.depth;
    });
    return out;
}

} // namespace

std::vector<std::int64_t> selectable_panes(const Panes& panes, const Setup& setup,
                                           const Screen& sc) {
    std::vector<std::int64_t> out;
    for (const Selectable& p : selectable_rects(panes, setup, sc)) {
        out.push_back(p.kind);
    }
    return out;
}

std::optional<std::int64_t> cycle_pane(const Panes& panes, const Setup& setup, const Screen& sc,
                                       std::int64_t from, std::int64_t step) {
    const std::vector<std::int64_t> order = selectable_panes(panes, setup, sc);
    if (order.empty()) {
        return std::nullopt;
    }
    const auto n = static_cast<std::int64_t>(order.size());
    const auto at = std::find(order.begin(), order.end(), from);
    if (at == order.end()) {
        return step >= 0 ? order.front() : order.back();
    }
    const std::int64_t i = static_cast<std::int64_t>(at - order.begin());
    return order[static_cast<std::size_t>(((i + step) % n + n) % n)];
}

std::optional<std::int64_t> spatial_neighbour(const Panes& panes, const Setup& setup,
                                              const Screen& sc, std::int64_t from, Act dir) {
    const bool horizontal = dir == Act::kFocusLeft || dir == Act::kFocusRight;
    if (!horizontal && dir != Act::kFocusUp && dir != Act::kFocusDown) {
        return std::nullopt;
    }
    const std::vector<Selectable> all = selectable_rects(panes, setup, sc);
    const auto here = std::find_if(all.begin(), all.end(),
                                   [from](const Selectable& p) { return p.kind == from; });
    if (here == all.end()) {
        if (all.empty()) {
            return std::nullopt;
        }
        return all.front().kind;
    }
    const PixelRect f = here->rect;
    const std::int64_t fcx = f.x + f.w / 2;
    const std::int64_t fcy = f.y + f.h / 2;
    // Past the edge, as the W3C draft filters; where panes overlap or share a border, past the
    // centre, so a pane half over another is still reachable.
    const auto beyond = [&](const PixelRect& r, bool edge) {
        const std::int64_t cx = r.x + r.w / 2;
        const std::int64_t cy = r.y + r.h / 2;
        switch (dir) {
        case Act::kFocusLeft: return edge ? cx <= f.x : cx < fcx;
        case Act::kFocusRight: return edge ? cx >= f.x + f.w : cx > fcx;
        case Act::kFocusUp: return edge ? cy <= f.y : cy < fcy;
        default: return edge ? cy >= f.y + f.h : cy > fcy;
        }
    };
    const auto score = [&](const PixelRect& r) {
        std::int64_t gap = 0;
        switch (dir) {
        case Act::kFocusLeft: gap = f.x - (r.x + r.w); break;
        case Act::kFocusRight: gap = r.x - (f.x + f.w); break;
        case Act::kFocusUp: gap = f.y - (r.y + r.h); break;
        default: gap = r.y - (f.y + f.h); break;
        }
        const std::int64_t across =
            horizontal ? std::abs(r.y + r.h / 2 - fcy) : std::abs(r.x + r.w / 2 - fcx);
        return (gap > 0 ? gap : 0) + 2 * across;
    };
    for (const bool edge : {true, false}) {
        const Selectable* best = nullptr;
        std::int64_t best_score = 0;
        for (const Selectable& p : all) {
            if (p.kind == from || !beyond(p.rect, edge)) {
                continue;
            }
            const std::int64_t points = score(p.rect);
            if (best == nullptr || points < best_score ||
                (points == best_score && p.depth > best->depth)) {
                best = &p;
                best_score = points;
            }
        }
        if (best != nullptr) {
            return best->kind;
        }
    }
    return std::nullopt;
}

} // namespace zengine::workshop
