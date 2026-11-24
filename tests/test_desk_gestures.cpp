// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#include <doctest.h>

// The desk's gestures as transitions, proved as a library's functions: no host is built, no bus is
// made, and nothing of Workshop is included. That a press, a motion and a release in Workshop call
// these is Workshop's suites' claim.

#include "desk-gestures/gestures.hpp"
#include "desk-gestures/keys.hpp"
#include "desk-gestures/menus.hpp"

#include "input/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
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

/// A menu row as a host's population gives one: a group to enter, or an action.
struct MenuRow {
    bool is_group = false;
    const char* group = "";
};

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

TEST_CASE("a press on the tabs: a miss is not the tabs', the + makes a layout and arms nothing, "
          "a double on one tab renames it and spends the arming, any other press switches and "
          "holds") {
    const TabClickMemory armed{true, 1};
    const TabPressAsk none = tab_press(armed, LayoutTabPress{}, 2);
    CHECK(none.kind == TabPressAsk::Kind::kNone);
    CHECK(none.next.armed); // a press elsewhere leaves the arming as it was
    CHECK(none.next.at == 1);

    const TabPressAsk plus = tab_press(armed, LayoutTabPress{true, 0, true}, 2);
    CHECK(plus.kind == TabPressAsk::Kind::kNew);
    CHECK_FALSE(plus.next.armed);

    const TabPressAsk first = tab_press(TabClickMemory{}, LayoutTabPress{true, 1, false}, 1);
    CHECK(first.kind == TabPressAsk::Kind::kSwitch);
    CHECK(first.at == 1);
    CHECK(first.next.armed);
    CHECK(first.next.at == 1);

    const TabPressAsk second = tab_press(first.next, LayoutTabPress{true, 1, false}, 2);
    CHECK(second.kind == TabPressAsk::Kind::kRename);
    CHECK(second.at == 1);
    CHECK_FALSE(second.next.armed);
    // The third press of a triple is a first again: a switch, not a second rename.
    CHECK(tab_press(second.next, LayoutTabPress{true, 1, false}, 3).kind ==
          TabPressAsk::Kind::kSwitch);
    // A second press on another tab switches to it.
    const TabPressAsk other = tab_press(first.next, LayoutTabPress{true, 2, false}, 2);
    CHECK(other.kind == TabPressAsk::Kind::kSwitch);
    CHECK(other.at == 2);
}

TEST_CASE("a held tab is carried to the tab the hand is over, and over its own place, the +, no "
          "tab or past the run nothing moves") {
    CHECK(tab_drag_to(LayoutTabPress{true, 3, false}, 1, 5) == std::optional<std::size_t>{3});
    CHECK(tab_drag_to(LayoutTabPress{true, 0, false}, 1, 5) == std::optional<std::size_t>{0});
    CHECK_FALSE(tab_drag_to(LayoutTabPress{true, 1, false}, 1, 5).has_value()); // its own place
    CHECK_FALSE(tab_drag_to(LayoutTabPress{true, 3, true}, 1, 5).has_value());  // the +
    CHECK_FALSE(tab_drag_to(LayoutTabPress{}, 1, 5).has_value());               // no tab
    CHECK_FALSE(tab_drag_to(LayoutTabPress{true, 5, false}, 1, 5).has_value()); // past the run
}

TEST_CASE("a committed name asks the rename of the layout its line opened on, a name the setup's "
          "judge refuses is refused first, and a layout that has gone asks nothing") {
    CHECK(naming_commit(true, 2, 3) == NamingCommit::kRename);
    CHECK(naming_commit(false, 2, 3) == NamingCommit::kRefused);
    CHECK(naming_commit(false, 3, 3) == NamingCommit::kRefused); // the line stays open to retry
    CHECK(naming_commit(true, 3, 3) == NamingCommit::kGone);
}

TEST_CASE("a nudge moves the place by whole cells from the window it measures from, and saturates "
          "at the number line's ends") {
    const PixelRect base{100, 60, 240, 120};
    CHECK(nudged(base, 1, 0) == CanvasPoint{100 + kCell, 60});
    CHECK(nudged(base, 0, -1) == CanvasPoint{100, 60 - kCell});
    CHECK(nudged(base, -4, 4) == CanvasPoint{100 - 4 * kCell, 60 + 4 * kCell});
    CHECK(nudged(PixelRect{kMax, kMin, 1, 1}, 1, -1) == CanvasPoint{kMax, kMin});
}

TEST_CASE("a key's grow pulls the bottom-right corner by whole cells, the place anchored and no "
          "pane moved") {
    const PixelRect base{100, 60, 240, 120};
    const PaneWindowProposal one = grown(base, 1, 0);
    CHECK(one.x == 100);
    CHECK(one.y == 60);
    CHECK(one.w == 240 + kCell);
    CHECK(one.h == 120);
    CHECK_FALSE(one.place_moved_x);
    CHECK_FALSE(one.place_moved_y);
    const PaneWindowProposal coarse = grown(base, -4, -4);
    CHECK(coarse.w == 240 - 4 * kCell);
    CHECK(coarse.h == 120 - 4 * kCell);
    CHECK(coarse.x == 100);
    CHECK(coarse.y == 60);
}

TEST_CASE("the arrangement's step addresses the next row or the one before, wrapping, and from "
          "nothing addressed the first forward and the last back") {
    CHECK(arrange_step_to(3, std::nullopt, +1) == 0);
    CHECK(arrange_step_to(3, std::nullopt, -1) == 2);
    CHECK(arrange_step_to(3, 0, +1) == 1);
    CHECK(arrange_step_to(3, 2, +1) == 0);
    CHECK(arrange_step_to(3, 0, -1) == 2);
    CHECK(arrange_step_to(3, 1, -1) == 0);
    CHECK(arrange_step_to(1, 0, +1) == 0);
}

TEST_CASE("the keyboard's selection walks the panes in reading order and wraps both ways") {
    // Laid out of order: a pane low on the desk, two on its top row, and a small one in front of
    // the first at the same corner.
    const std::vector<Selectable> laid = reading_order({
        Selectable{3, PixelRect{0, 200, 100, 100}, 0},
        Selectable{1, PixelRect{0, 0, 100, 100}, 1},
        Selectable{2, PixelRect{200, 0, 100, 100}, 2},
        Selectable{4, PixelRect{0, 0, 50, 50}, 3},
    });
    REQUIRE(laid.size() == 4);
    CHECK(laid[0].kind == 4); // the same corner as 1, and in front of it
    CHECK(laid[1].kind == 1);
    CHECK(laid[2].kind == 2);
    CHECK(laid[3].kind == 3);
    const std::vector<std::int64_t> order{4, 1, 2, 3};
    CHECK(cycle_from(order, 1, +1) == std::optional<std::int64_t>{2});
    CHECK(cycle_from(order, 3, +1) == std::optional<std::int64_t>{4});
    CHECK(cycle_from(order, 4, -1) == std::optional<std::int64_t>{3});
    CHECK(cycle_from(order, 99, +1) == std::optional<std::int64_t>{4});
    CHECK(cycle_from(order, 99, -1) == std::optional<std::int64_t>{3});
    CHECK_FALSE(cycle_from({}, 1, +1).has_value());
}

TEST_CASE("a direction selects the nearest pane past the selected one's edge by its gap and twice "
          "its misalignment, past its centre where panes overlap, a tie the front-most") {
    const PixelRect here{200, 200, 100, 100};
    // To the right: one level with it and far, one near and well off its row.
    const std::vector<Selectable> right{
        Selectable{1, here, 0},
        Selectable{2, PixelRect{500, 200, 100, 100}, 1}, // gap 200, misaligned 0: 200
        Selectable{3, PixelRect{320, 330, 100, 100}, 2}, // gap 20, misaligned 130: 280
    };
    CHECK(neighbour(right, 1, Toward::kRight) == std::optional<std::int64_t>{2});
    CHECK_FALSE(neighbour(right, 1, Toward::kLeft).has_value()); // nothing that way
    CHECK(neighbour(right, 2, Toward::kLeft) == std::optional<std::int64_t>{1});
    CHECK(neighbour(right, 99, Toward::kUp) == std::optional<std::int64_t>{1}); // from no pane
    CHECK_FALSE(neighbour({}, 1, Toward::kUp).has_value());
    // A pane over the selected one's right half: past its centre, not past its edge.
    const std::vector<Selectable> over{
        Selectable{1, here, 0},
        Selectable{2, PixelRect{220, 200, 100, 100}, 1}, // its centre short of the edge
    };
    CHECK(neighbour(over, 1, Toward::kRight) == std::optional<std::int64_t>{2});
    CHECK(neighbour(over, 2, Toward::kLeft) == std::optional<std::int64_t>{1});
    // Two scored alike: the one in front.
    const std::vector<Selectable> tie{
        Selectable{1, here, 0},
        Selectable{2, PixelRect{200, 400, 100, 100}, 1},
        Selectable{3, PixelRect{200, 400, 100, 100}, 2},
    };
    CHECK(neighbour(tie, 1, Toward::kDown) == std::optional<std::int64_t>{3});
}

TEST_CASE("where the keys go: each mode above the next, and a mode closed stops being the answer") {
    KeyModes m;
    CHECK(keyboard_context(m) == KeyContext::kCommand);
    m.pane_keys = true;
    CHECK(keyboard_context(m) == KeyContext::kPane);
    m.naming = true;
    CHECK(keyboard_context(m) == KeyContext::kNaming);
    m.menu = true;
    CHECK(keyboard_context(m) == KeyContext::kContext);
    CHECK(keyboard_context_beneath_menu(m) == KeyContext::kNaming);
    m.arranging = true;
    CHECK(keyboard_context(m) == KeyContext::kArrangePane);
    m.desk = true;
    CHECK(keyboard_context(m) == KeyContext::kArrangeDesk);
    m.resetting = true;
    CHECK(keyboard_context(m) == KeyContext::kArrangeReset);
    m.arranging = false;
    CHECK(keyboard_context(m) == KeyContext::kContext);
    m.menu = false;
    m.naming = false;
    CHECK(keyboard_context(m) == KeyContext::kPane);
    CHECK(default_row_context(KeyContext::kCommand));
    CHECK_FALSE(default_row_context(KeyContext::kPane));
    CHECK_FALSE(default_row_context(KeyContext::kContext));
}

TEST_CASE("a menu's cursor moves a row at a time and stops at either end, from where it stands "
          "bounded") {
    CHECK(menu_cursor_moved(0, 3, +1) == 1);
    CHECK(menu_cursor_moved(2, 3, +1) == 2);
    CHECK(menu_cursor_moved(1, 3, -1) == 0);
    CHECK(menu_cursor_moved(0, 3, -1) == 0);
    CHECK(menu_cursor_moved(7, 3, -1) == 1); // a cursor the rows outgrew is bounded first
    CHECK(menu_cursor_moved(0, 0, +1) == 0);
    CHECK(context_cursor_bound(5, 2) == 1);
    CHECK(context_cursor_bound(1, 2) == 1);
}

TEST_CASE("a menu's back key leaves an open group onto that group's row, else closes the menu, "
          "and a choice names a group to enter or an action by its row") {
    CHECK(menu_back("Order") == MenuBack::kLeaveGroup);
    CHECK(menu_back("") == MenuBack::kClose);
    const std::vector<MenuRow> top{MenuRow{false, ""}, MenuRow{true, "Order"},
                                   MenuRow{true, "Reset"}, MenuRow{}};
    CHECK(cursor_on_group(top, "Reset") == 2);
    CHECK(cursor_on_group(top, "Order") == 1);
    CHECK(cursor_on_group(top, "Dock") == 0);
    CHECK(menu_choice(top, 1).kind == MenuChoice::Kind::kGroup);
    CHECK(menu_choice(top, 1).at == 1);
    CHECK(menu_choice(top, 3).kind == MenuChoice::Kind::kAction);
    CHECK(menu_choice(top, 3).at == 3);
    CHECK(menu_choice(top, 4).kind == MenuChoice::Kind::kNothing);
}

TEST_CASE("a refusal naming an attempt at or after a menu's grant is that menu's, one before it an "
          "older menu's, and a grant not yet queued owns none") {
    CHECK(menu_refusal_is_ours(10, 10));
    CHECK(menu_refusal_is_ours(10, 11));
    CHECK_FALSE(menu_refusal_is_ours(10, 9));
    CHECK_FALSE(menu_refusal_is_ours(0, 5));
}

TEST_CASE("a presented menu keeps its newest presses, newest last, and past its bound the oldest "
          "goes") {
    std::vector<MenuPress> kept;
    for (std::uint64_t i = 1; i <= kMaxMenuPresses; ++i) {
        kept = kept_presses(std::move(kept), MenuPress{i, 100 + i});
    }
    REQUIRE(kept.size() == kMaxMenuPresses);
    CHECK(kept.front().press == 1);
    kept = kept_presses(std::move(kept), MenuPress{99, 7});
    REQUIRE(kept.size() == kMaxMenuPresses);
    CHECK(kept.front().press == 2);
    CHECK(kept.back().press == 99);
    CHECK(kept.back().act == 7);
}

TEST_CASE("a refusal is about the withdrawn menu whose sentences include its attempt, and once "
          "that menu is answered and forgotten no second answer is found") {
    WithdrawnMenu a;
    a.menu = 1;
    a.first_attempt = 10;
    a.last_attempt = 14;
    WithdrawnMenu b;
    b.menu = 2;
    b.first_attempt = 20;
    b.last_attempt = 21;
    std::vector<WithdrawnMenu> kept{a, b};
    CHECK(withdrawn_refused(kept, 10) == std::optional<std::size_t>{0});
    CHECK(withdrawn_refused(kept, 14) == std::optional<std::size_t>{0});
    CHECK(withdrawn_refused(kept, 21) == std::optional<std::size_t>{1});
    CHECK_FALSE(withdrawn_refused(kept, 15).has_value());
    CHECK_FALSE(withdrawn_refused(kept, 9).has_value());
    const std::optional<std::size_t> answered = withdrawn_refused(kept, 12);
    REQUIRE(answered.has_value());
    kept.erase(kept.begin() + static_cast<std::ptrdiff_t>(*answered));
    CHECK_FALSE(withdrawn_refused(kept, 12).has_value());
}

} // TEST_SUITE
