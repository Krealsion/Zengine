// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_DESK_GESTURES_GESTURES_HPP
#define ZENGINE_DESK_GESTURES_GESTURES_HPP

// Each gesture of the desk as a transition: a function of what the hand holds and what it just
// did, answering what it holds next and what it asks for. None holds state or authority. A host
// keeps the wiring: it reads the event, calls the transition, stores what comes back, and says and
// sends what it asks. So a gesture's arithmetic is proved here, by this package's own suite, and a
// host's suites prove only that the wiring calls it.

#include "desk-gestures/geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zengine::desk_gestures {

// ---- a pane taken by an edge or by its body, moved or sized ---------------------------------

/// A PANE WINDOW PROPOSAL, IN CANVAS PIXELS: what one resize gesture asks the whole
/// window to become.
// WL-ARR-06 -- agents/workshop/arrangement.md
struct PaneWindowProposal {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    /// TRUE EXACTLY WHEN THE EDGE NAMES THE LEFT -- the horizontal axis must author the
    /// place's `x` for the right-edge anchor to hold. False keeps `x` unproposed.
    bool place_moved_x = false;
    /// TRUE EXACTLY WHEN THE EDGE NAMES THE TOP -- the vertical twin, for `y`.
    bool place_moved_y = false;
};

/// The window a pull of `edge` by `dx`, `dy` proposes, from the window at `base_*`: the pulled
/// edges move and the opposite ones hold.
PaneWindowProposal pane_window_proposal(std::int64_t edge, std::int64_t base_x,
                                        std::int64_t base_y, std::int64_t base_w,
                                        std::int64_t base_h, std::int64_t dx,
                                        std::int64_t dy) noexcept;

/// WHAT A HAND HOLDS of a pane it took: by an edge, to size it from the window it had when the
/// press landed, or by its body, to move it with the grab kept. What the hand holds and never
/// the pane it holds, which the host keeps beside it.
struct PaneHold {
    bool sizing = false;
    std::int64_t edge = kNoPaneEdge;
    std::int64_t grab_dx = 0; ///< move: where inside the pane's rectangle the hand took hold
    std::int64_t grab_dy = 0;
    std::int64_t from_x = 0;  ///< size: the pixel the press landed on
    std::int64_t from_y = 0;
    std::int64_t base_x = 0;  ///< size: the pane's window at that moment -- place...
    std::int64_t base_y = 0;
    std::int64_t base_w = 0;  ///< ...and extent
    std::int64_t base_h = 0;
};

/// THE WINDOW A PANE'S ROW AUTHORS IN PIXELS, axis by axis: an axis authored in another unit, or
/// not authored, is none, and the size takes the resolved window's there.
struct AuthoredPixels {
    std::optional<std::int64_t> x;
    std::optional<std::int64_t> y;
    std::optional<std::int64_t> w;
    std::optional<std::int64_t> h;
};

/// A PRESS ON A PANE, TAKEN OR NOT. `drawn` is the pane's rectangle on the canvas, `resolved` its
/// window in the room, `authored` what its row writes in pixels. A press on an edge sizes it,
/// unless it is the top edge and `top_moves`: a pane being placed is taken by its title, which
/// stands on that edge. A press on the body moves it. A press elsewhere takes nothing.
std::optional<PaneHold> hold_at(const PixelRect& drawn, const PixelRect& resolved,
                                const AuthoredPixels& authored, const PointedAt& at,
                                bool top_moves) noexcept;

/// THE WINDOW A SIZING HOLD PROPOSES with the hand at `px_x`, `px_y`, before any snap.
PaneWindowProposal sized_to(const PaneHold& hold, std::int64_t px_x, std::int64_t px_y) noexcept;

/// WHERE A MOVING HOLD PUTS THE PANE'S CORNER on the canvas with the hand at `px_x`, `px_y`: the
/// grab kept.
CanvasPoint moved_to(const PaneHold& hold, std::int64_t px_x, std::int64_t px_y) noexcept;

/// THE LINES AN EDGE THE HAND MOVES MAY MEET, in the room: the room's own edges and every edge
/// of every other pane on the screen.
struct PaneSnapLines {
    std::vector<std::int64_t> xs;
    std::vector<std::int64_t> ys;
};

/// A HAND'S PROPOSAL, SNAPPED, and the line each axis met.
struct SnappedWindow {
    PaneWindowProposal want;
    std::optional<std::int64_t> met_x;
    std::optional<std::int64_t> met_y;
};

/// The places and extents a window may take: a snap never proposes one outside them.
struct WindowLimits {
    std::int64_t min_extent = 0;
    std::int64_t max_px = 0;
};

/// Each edge the hand moves -- both edges of both axes for a move (`kNoPaneEdge`), the pulled
/// edges for a resize -- comes to the nearest line within `reach_px`, the opposite edge held; a
/// snap that would leave the place or the extent outside `limits` is not taken.
SnappedWindow snap_pane_window(const PaneWindowProposal& want, std::int64_t edge,
                               const PaneSnapLines& lines, std::int64_t reach_px,
                               const WindowLimits& limits) noexcept;

// ---- a layout tab: pressed, pressed twice ---------------------------------------------------

/// WHAT THE LAST PRESS ON A LAYOUT TAB NAMED, so the next one can be a double.
// WL-PTR-01 -- agents/workshop/pointer.md
struct TabClickMemory {
    bool armed = false;
    std::size_t at = 0;      ///< the position the press landed on
};

/// IS THIS PRESS THE SECOND HALF OF A DOUBLE-CLICK ON THE SAME TAB? Pure, total, and the ONE
/// place the question is decided: the window counted it 2 or more (`clicks`),
/// and the last press armed on the same tab.
bool doubles_a_tab_click(const TabClickMemory& prior, std::size_t at,
                         std::int64_t clicks) noexcept;

// ---- a bounded list: what it shows, and what it must say it cannot --------------------------

/// Which members of an ordered collection a bounded place is showing, and how
/// many it is leaving out on each side of them.
struct ListWindow {
    std::size_t first = 0;  ///< the first member shown, as a position in the collection's order
    std::size_t count = 0;  ///< how many are shown, contiguously, in that order
    std::size_t before = 0; ///< how many the place left out ahead of them
    std::size_t after = 0;  ///< how many it left out behind them
};

/// What `rows` lines can honestly show of `total` members while the
/// `selected_at`'th is selected.
ListWindow list_window(std::size_t total, std::size_t selected_at, std::size_t rows);

/// What one omission marker says. `... 2 earlier` / `... 4 more`: a count and a direction.
std::string omitted_text(std::size_t how_many, const char* which);

} // namespace zengine::desk_gestures

#endif // ZENGINE_DESK_GESTURES_GESTURES_HPP
