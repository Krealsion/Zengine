// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#include <doctest.h>

// The desk's gestures as transitions, proved as a library's functions: no host is built, no bus is
// made, and nothing of Workshop is included. That a press, a motion and a release in Workshop call
// these is Workshop's suites' claim.

#include "desk-gestures/gestures.hpp"

#include "input/vocabulary.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

using namespace zengine::desk_gestures;

namespace {

constexpr std::int64_t kCell = zengine::graphics::kCellPx;
constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();

/// A pointer at a canvas pixel, reported by a window (one pixel's grain).
PointedAt pixel(std::int64_t x, std::int64_t y) {
    return canvas_point_of(zengine::input::space::kPixels, x, y);
}

/// A pane drawn at (120, 60), 240 by 120: wide and tall enough for every band to stand apart.
constexpr PixelRect kDrawn{120, 60, 240, 120};
/// ...resolved in the room one cell up and to the left of where it is drawn.
constexpr PixelRect kResolved{108, 48, 240, 120};

const WindowLimits kLimits{kCell, 65536};

} // namespace

TEST_SUITE("desk_gestures") {

TEST_CASE("a pointer lands on a canvas pixel at its medium's grain, and a space this "
          "application cannot place is not understood") {
    const PointedAt px = canvas_point_of(zengine::input::space::kPixels, 25, 13);
    CHECK(px.understood);
    CHECK(px.px == CanvasPoint{25, 13});
    CHECK(px.cell == CanvasPoint{25 / kCell, 13 / kCell});
    CHECK(px.grain == zengine::graphics::kPixelGrainPx);
    const PointedAt cell = canvas_point_of(zengine::input::space::kCells, 3, 2);
    CHECK(cell.px == CanvasPoint{3 * kCell, 2 * kCell});
    CHECK(cell.grain == zengine::graphics::kCellGrainPx);
    CHECK_FALSE(canvas_point_of(99, 1, 1).understood);
}

TEST_CASE("each of a pane's eight edge bands is its own affordance, its body none, and outside "
          "it nothing") {
    const std::int64_t l = kDrawn.x + 1, r = kDrawn.x + kDrawn.w - 2, xm = kDrawn.x + kDrawn.w / 2;
    const std::int64_t t = kDrawn.y + 1, b = kDrawn.y + kDrawn.h - 2, ym = kDrawn.y + kDrawn.h / 2;
    CHECK(pane_edge_at(kDrawn, l, ym, 1) == pane_edge::kLeft);
    CHECK(pane_edge_at(kDrawn, r, ym, 1) == pane_edge::kRight);
    CHECK(pane_edge_at(kDrawn, xm, t, 1) == pane_edge::kTop);
    CHECK(pane_edge_at(kDrawn, xm, b, 1) == pane_edge::kBottom);
    CHECK(pane_edge_at(kDrawn, l, t, 1) == pane_edge::kTopLeft);
    CHECK(pane_edge_at(kDrawn, r, t, 1) == pane_edge::kTopRight);
    CHECK(pane_edge_at(kDrawn, l, b, 1) == pane_edge::kBottomLeft);
    CHECK(pane_edge_at(kDrawn, r, b, 1) == pane_edge::kBottomRight);
    CHECK(pane_edge_at(kDrawn, xm, ym, 1) == kNoPaneEdge);
    CHECK(pane_edge_at(kDrawn, kDrawn.x - 1, ym, 1) == kNoPaneEdge);
    // Each affordance's mark is one cell, on the pane's own boundary.
    for (std::int64_t e = 0; e < pane_edge::kCount; ++e) {
        const PixelRect m = pane_edge_cell(kDrawn, e);
        CHECK(m.w == kCell);
        CHECK(m.h == kCell);
        CHECK(pane_edge_at(kDrawn, m.x + kCell / 2, m.y + kCell / 2, 1) == e);
    }
}

TEST_CASE("a press on an edge takes that edge to size from the window the row authors in "
          "pixels, and from the resolved window on an axis it does not") {
    const std::int64_t r = kDrawn.x + kDrawn.w - 2, ym = kDrawn.y + kDrawn.h / 2;
    AuthoredPixels authored;
    authored.x = 200;
    authored.w = 300;
    const std::optional<PaneHold> hold = hold_at(kDrawn, kResolved, authored, pixel(r, ym), true);
    REQUIRE(hold.has_value());
    CHECK(hold->sizing);
    CHECK(hold->edge == pane_edge::kRight);
    CHECK(hold->from_x == r);
    CHECK(hold->from_y == ym);
    CHECK(hold->base_x == 200);          // authored in pixels
    CHECK(hold->base_w == 300);
    CHECK(hold->base_y == kResolved.y);  // not authored: the resolved window's
    CHECK(hold->base_h == kResolved.h);
}

TEST_CASE("a press on the body takes a move with the grab kept, the top edge moves where asked, "
          "and a press outside takes nothing") {
    const PointedAt body = pixel(kDrawn.x + 50, kDrawn.y + 40);
    const std::optional<PaneHold> move = hold_at(kDrawn, kResolved, {}, body, false);
    REQUIRE(move.has_value());
    CHECK_FALSE(move->sizing);
    CHECK(move->edge == kNoPaneEdge);
    CHECK(move->grab_dx == 50);
    CHECK(move->grab_dy == 40);
    CHECK(moved_to(*move, 400, 300) == CanvasPoint{350, 260});

    const PointedAt top = pixel(kDrawn.x + kDrawn.w / 2, kDrawn.y + 1);
    CHECK(hold_at(kDrawn, kResolved, {}, top, false)->sizing);
    const std::optional<PaneHold> by_title = hold_at(kDrawn, kResolved, {}, top, true);
    REQUIRE(by_title.has_value());
    CHECK_FALSE(by_title->sizing);

    CHECK_FALSE(hold_at(kDrawn, kResolved, {}, pixel(5, 5), false).has_value());
    CHECK_FALSE(hold_at(PixelRect{0, 0, 0, 10}, kResolved, {}, pixel(0, 0), false).has_value());
}

TEST_CASE("a motion moves only the pulled edges and holds the opposite ones") {
    PaneHold left;
    left.sizing = true;
    left.edge = pane_edge::kLeft;
    left.from_x = 100;
    left.from_y = 100;
    left.base_x = 100;
    left.base_y = 50;
    left.base_w = 200;
    left.base_h = 80;
    const PaneWindowProposal want = sized_to(left, 70, 130);
    CHECK(want.x == 70);
    CHECK(want.w == 230);              // the right edge held: 100 + 200 == 70 + 230
    CHECK(want.place_moved_x);
    CHECK(want.y == 50);               // a left pull proposes nothing vertical
    CHECK(want.h == 80);
    CHECK_FALSE(want.place_moved_y);

    PaneHold bottom_right = left;
    bottom_right.edge = pane_edge::kBottomRight;
    const PaneWindowProposal grown = sized_to(bottom_right, 130, 120);
    CHECK(grown.x == 100);
    CHECK(grown.w == 230);
    CHECK(grown.y == 50);
    CHECK(grown.h == 100);
    CHECK_FALSE(grown.place_moved_x);
    CHECK_FALSE(grown.place_moved_y);
}

TEST_CASE("a snap within reach meets the nearest line, none past reach, and none a pane's "
          "limits refuse") {
    PaneSnapLines lines;
    lines.xs = {0, 400, 410};
    lines.ys = {0, 300};
    // The right edge pulled to 405: 400 and 410 are both in reach and equally near, so the first
    // line met stands.
    const PaneWindowProposal want{100, 50, 305, 80, false, false};
    const SnappedWindow met = snap_pane_window(want, pane_edge::kRight, lines, 8, kLimits);
    REQUIRE(met.met_x.has_value());
    CHECK(*met.met_x == 400);
    CHECK(met.want.w == 300);
    CHECK(met.want.x == 100);             // the opposite edge held
    CHECK_FALSE(met.met_y.has_value());   // a right pull moves no vertical edge

    const PaneWindowProposal far{100, 50, 280, 80, false, false};
    const SnappedWindow none = snap_pane_window(far, pane_edge::kRight, lines, 8, kLimits);
    CHECK_FALSE(none.met_x.has_value());
    CHECK(none.want.w == 280);

    // A move snaps both edges of both axes; the nearest line wins.
    const PaneWindowProposal moving{3, 296, 100, 0, true, true};
    const SnappedWindow moved = snap_pane_window(moving, kNoPaneEdge, lines, 8, kLimits);
    CHECK(moved.want.x == 0);
    CHECK(moved.want.y == 300);

    // A snap that would leave the extent under the least a pane takes is not taken.
    PaneSnapLines tight;
    tight.xs = {104};
    const PaneWindowProposal thin{100, 50, kCell + 2, 80, false, false};
    const SnappedWindow refused = snap_pane_window(thin, pane_edge::kRight, tight, 11, kLimits);
    CHECK_FALSE(refused.met_x.has_value());
    CHECK(refused.want.w == kCell + 2);
}

TEST_CASE("every value a gesture computes saturates at the number line's ends") {
    CHECK(saturating_step(kMax, 1) == kMax);
    CHECK(saturating_step(kMin, -1) == kMin);
    CHECK(saturating_minus(kMax, -1) == kMax);
    CHECK(saturating_minus(kMin, 1) == kMin);
    PaneHold edge;
    edge.sizing = true;
    edge.edge = pane_edge::kRight;
    edge.base_w = kMax - 1;
    CHECK(sized_to(edge, kMax, 0).w == kMax - 1); // a pull past the end proposes no wrap
    PaneHold grab;
    grab.grab_dx = kMax;
    CHECK(moved_to(grab, kMin, 0).x == kMin);
}

TEST_CASE("a second press on the same tab within the window's count is a double, and on another "
          "tab or unarmed it is not") {
    const TabClickMemory armed{true, 2};
    CHECK(doubles_a_tab_click(armed, 2, 2));
    CHECK_FALSE(doubles_a_tab_click(armed, 3, 2));
    CHECK_FALSE(doubles_a_tab_click(armed, 2, 1));
    CHECK_FALSE(doubles_a_tab_click(TabClickMemory{}, 0, 2));
}

TEST_CASE("a bounded list shows what fits around its selection and says what it left out") {
    const ListWindow all = list_window(4, 0, 6);
    CHECK(all.count == 4);
    CHECK(all.before + all.after == 0);
    const ListWindow head = list_window(20, 1, 6);
    CHECK(head.first == 0);
    CHECK(head.count == 5);
    CHECK(head.after == 15);
    const ListWindow mid = list_window(20, 10, 6);
    CHECK(mid.count == 4);
    CHECK(mid.first <= 10);
    CHECK(mid.first + mid.count > 10);
    CHECK(mid.before + mid.count + mid.after == 20);
    const ListWindow tail = list_window(20, 19, 6);
    CHECK(tail.first + tail.count == 20);
    CHECK(list_window(9, 0, 2).after == 9); // too few lines to seat an entry: all said omitted
    CHECK(omitted_text(4, "more") == "... 4 more");
}

} // TEST_SUITE
