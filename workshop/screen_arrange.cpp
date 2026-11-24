// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The screen's pane management: what a weaver is arranging, and how, and where the keyboard's
// selection moves.
// Workshop law: agents/workshop/focus.md (+9 registers; agents/workshop.md routes)

#include "screen.hpp"

#include <utility>

namespace zengine::workshop {

// ---- PANE MANAGEMENT: what a weaver is ARRANGING, and how ------------------------------

namespace {

/// The facts the routing chain reads, as this session holds them; the chain itself is the desk's
/// gestures' (`desk_gestures::keyboard_context`).
desk_gestures::KeyModes key_modes(const Session& s) {
    desk_gestures::KeyModes m;
    // A pane being placed as it opened is not an arrangement the keys are in: a key goes to it.
    m.arranging = s.arrange.open && !s.arrange.opened;
    m.resetting = s.arrange.resetting;
    m.desk = s.arrange.desk;
    // A pane's menu presented by its presenter is the same kind of surface as the host's own.
    m.menu = s.context.open || s.presented.open;
    m.naming = s.setup.naming.open;
    m.pane_keys = is_runtime_kind(keyboard_pane(s.panes));
    return m;
}

} // namespace

KeyContext keyboard_context_beneath_menu(const Session& s) {
    return desk_gestures::keyboard_context_beneath_menu(key_modes(s));
}

KeyContext keyboard_context(const Session& s) {
    return desk_gestures::keyboard_context(key_modes(s));
}

std::int64_t typing_pane(const Session& s) {
    // THE KEY HANDLER'S OWN ORDER, READ AS A VALUE: the resolved context decides
    // (`on(KeyPressed)`).
    if (keyboard_context(s) != KeyContext::kPane) {
        return kNoPaneKind;
    }
    return keyboard_pane(s.panes);
}

// ---- MOVING THE KEYBOARD'S SELECTION: reading order and the neighbour in a direction ----------

namespace {

using desk_gestures::Selectable;

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
    return desk_gestures::reading_order(std::move(out));
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
    return desk_gestures::cycle_from(selectable_panes(panes, setup, sc), from, step);
}

std::optional<std::int64_t> spatial_neighbour(const Panes& panes, const Setup& setup,
                                              const Screen& sc, std::int64_t from, Act dir) {
    using desk_gestures::Toward;
    Toward toward = Toward::kLeft;
    switch (dir) {
    case Act::kFocusLeft: toward = Toward::kLeft; break;
    case Act::kFocusRight: toward = Toward::kRight; break;
    case Act::kFocusUp: toward = Toward::kUp; break;
    case Act::kFocusDown: toward = Toward::kDown; break;
    default: return std::nullopt;
    }
    return desk_gestures::neighbour(selectable_rects(panes, setup, sc), from, toward);
}

} // namespace zengine::workshop
