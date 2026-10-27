// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop screen suite: composition and geometry -- what is painted where, and what a hand
// can reach, asserted as one canvas value. The geometry claims are integration claims: these prove
// that a pane's painted rectangle and the cells a press meets it on both derive from one placement,
// `bounds_of`. A window carrying the desk to a human's eyes is the graphics suite's;
// `doctest_main.cpp` refuses a run selecting no case.

#include "workshop_support.hpp"
#include "pane_window.hpp"

// The historical session shapes and their conversions, an artifact's material, named here
// because this suite owns the whole-cell-to-fine-lattice claim.
#include "workshop/layouts/session_history.hpp"
#include "workshop/screen_canvas.hpp"

// ============================================================================
// Tier 3 — the whole screen, as a value
// ============================================================================

// The Info pane's composition, windows, draft and presses are the pane's, in
// `tests/test_workshop_panes_info.cpp`; this suite keeps the screen -- composition in cells,
// the placement path, occupancy, the press chain, and every pane's rectangle.

TEST_CASE("the object-list window is total, and never spends more rows than it has") {
    // The calculation the paint loop trusts, asked directly about every shape it
    // can be handed -- including budgets no screen has. It may not underflow,
    // may not plan past the end of the document, and may not plan more lines
    // than it was given.
    for (std::size_t rows = 0; rows <= 8; ++rows) {
        for (std::size_t total = 0; total <= 12; ++total) {
            for (std::size_t at = 0; at <= total; ++at) { // `at == total` is "nothing selected"
                CAPTURE(rows);
                CAPTURE(total);
                CAPTURE(at);
                const ListWindow w = list_window(total, at, rows);

                // It accounts for the WHOLE document, always: every object is
                // either shown or counted, and none is both.
                CHECK(w.first + w.count <= total);
                CHECK(w.before == w.first);
                CHECK(w.before + w.count + w.after == total);

                // The plan fits its budget. (With no rows there is no pane, so
                // there is nowhere to say anything either.)
                if (rows >= 1) {
                    const std::size_t used = w.count + (w.before > 0 ? 1u : 0u) +
                                             (w.after > 0 ? 1u : 0u);
                    CHECK(used <= rows);
                }

                if (total <= rows) {
                    CHECK(w.count == total); // it fits: shown whole, and no chrome
                    CHECK(w.before == 0);
                    CHECK(w.after == 0);
                } else {
                    CHECK(w.before + w.after > 0); // it does not fit: never silently
                }

                // And wherever a budget can seat an object between two markers,
                // the selected one is in the window.
                if (rows >= 3 && at < total) {
                    CHECK(at >= w.first);
                    CHECK(at < w.first + w.count);
                }
            }
        }
    }
}

TEST_CASE("fitting text to a line: unchanged when it fits, marked when it does not") {
    using detail::fit;
    CHECK(fit("short", 10) == "short");
    CHECK(fit("exactly-10", 10) == "exactly-10"); // exactly the width is not truncation
    CHECK(fit("exactly-11!", 10) == "exactly..."); // one over IS
    CHECK(fit("exactly-11!", 10).size() == 10);    // and the mark is INSIDE the width

    // The mark itself has to fit. These are the widths that would underflow a
    // `width - 3`, and no screen has them -- which is exactly why a helper
    // reachable from anywhere must answer for them anyway.
    CHECK(fit("anything", 3) == "...");
    CHECK(fit("anything", 2) == "..");
    CHECK(fit("anything", 1) == ".");
    CHECK(fit("anything", 0).empty());
    CHECK(fit("anything", -7).empty());
    CHECK(fit("", 78).empty());
}

TEST_CASE("a refusal longer than a row is kept whole in the session, and the screen draws none of it") {
    // THE STRONG WITNESS. A refusal is a producer's sentence at whatever length its facts take; the
    // sentence is a real door's: the file door, refusing a path a weaver could type. What Workshop
    // said is published and shown by the Notices pane, so the screen it paints draws none of it.
    const std::string path = "/no/such/place/" + std::string(90, 'x') + ".json";
    const persist::FileText read = persist::read_file(path, 1024, "a test file");
    REQUIRE_FALSE(read.outcome.accepted);
    const std::string refusal = read.outcome.refusal;
    REQUIRE(refusal.size() > static_cast<std::size_t>(cells_of(kMinScreen).w));

    Session s;
    const ScreenCanvas quiet = paint(s);
    s.notice = refusal;
    s.notice_is_bad = true;
    const ScreenCanvas c = paint(s);
    // Truth is not presentation capacity: the session holds the whole sentence...
    CHECK(s.notice == refusal);
    // ...and the screen is the same picture with it as without it: no row cuts it, and no row
    // wears the alert's role for it.
    CHECK(c == quiet);
    for (const ScreenLabel& l : all_labels(c)) {
        CHECK(l.text.find("/no/such/place/") == std::string::npos);
        if (gfx::cell_of_pixel(l.y) == cells_of(kMinScreen).band_y) {
            CHECK(l.role != gfx::role::kAlert);
        }
    }
}

namespace {

/// Is this canvas cell inside any OPEN pane's bounds, worked out from the
/// painting path alone -- `bounds_of` per open kind, exactly as `paint_panes`
/// asks it. The point of computing it a second way is that `occupied_at` has to
/// agree with it cell for cell.
bool inside_a_painted_pane(const Panes& panes, const Screen& sc, std::int64_t x,
                            std::int64_t y) {
    for (const OpenPane& p : panes.open) {
        if (cells_covered(bounds_of(panes, setup_for(panes), p.kind, sc).rect)
                .contains(x, y)) {
            return true;
        }
    }
    return false;
}

/// EVERY CELL OF THE CANVAS, ASKED BOTH WAYS -- what the painting path says is inside a
/// pane, and what the pointer path says is occupied. A SWEEP reporting a tally, not a case
/// asserting per cell: seventeen hundred identical assertions would drown the suite's totals,
/// and a count plus the first cell that disagreed diagnoses as precisely. `first_bad_x` and
/// `first_bad_y` are -1 when nothing disagreed.
struct Sweep {
    std::size_t cells = 0;    ///< how many were asked
    std::size_t occupied = 0; ///< how many the pointer path called occupied
    std::size_t painted = 0;  ///< how many the painting path called covered
    std::size_t disagreed = 0;
    std::int64_t first_bad_x = -1;
    std::int64_t first_bad_y = -1;
};

Sweep sweep_canvas(const Panes& panes, const Screen& sc) {
    Sweep out;
    for (std::int64_t y = 0; y < cells_of(sc).h; ++y) {
        for (std::int64_t x = 0; x < cells_of(sc).w; ++x) {
            const bool painted = inside_a_painted_pane(panes, sc, x, y);
            const bool occupied = occupied_at(panes, setup_for(panes), sc, x, y).occupied;
            ++out.cells;
            out.painted += painted ? 1u : 0u;
            out.occupied += occupied ? 1u : 0u;
            if (painted != occupied) {
                if (out.disagreed == 0) {
                    out.first_bad_x = x;
                    out.first_bad_y = y;
                }
                ++out.disagreed;
            }
        }
    }
    return out;
}

} // namespace

// ---- tier 7: the medium's own extent -----------------------------------------------------
// A canvas publisher learns how much room its medium has from the window that shows it; below is
// what a Workshop makes of the answer: where the extra columns and rows go, what a weaver's
// authored work does while they arrive, and what a pane does with the room.

TEST_CASE("a bigger surface is a bigger workspace, not a bigger picture of a small one") {
    // THE CENTRAL CLAIM, as arithmetic. Every number here is derived in one place
    // (`screen_of`).
    CHECK(cells_of(kMinScreen).w == 78);
    CHECK(cells_of(kMinScreen).h == 22);

    const Screen big = screen_of(cells_px(100), cells_px(33));
    CHECK(cells_of(big).w == 100);
    CHECK(cells_of(big).h == 33);

    // THE WORKSPACE TAKES THE EXTRA ROOM. Twenty-two more columns of surface are twenty-two
    // more columns a weaver can build in; eleven more rows are eleven more rows.
    CHECK(cells_of(big).room_w == cells_of(kMinScreen).room_w + 22);
    CHECK(cells_of(big).room_h == cells_of(kMinScreen).room_h + 11);

    // The bottom band keeps its shape against the bottom edge, and the top band keeps its
    // rows against the top one -- both are fixed reservations, not shares.
    CHECK(cells_of(big).h - cells_of(big).band_y == cells_of(kMinScreen).h - cells_of(kMinScreen).band_y);
    CHECK(cells_of(big).h - help_row(big) == cells_of(kMinScreen).h - help_row(kMinScreen));
    CHECK(cells_of(big).band_y == cells_of(big).h - kBottomRows);   // the band's own first row
    CHECK(help_row(big) + 2 == cells_of(big).h - 1);           // ...and two legend rows under it

    // AND THE ROOM IS THE SURFACE: nothing comes off its width.
    CHECK(cells_of(big).room_w == cells_of(big).w);
}

TEST_CASE("the screen's extent is TOTAL over whatever a medium published") {
    // It arrives as a ZEN_SHAPE off the bus, so its fields are whatever the sender put in
    // them. Nothing below may produce a negative extent, an inverted layout, or a multiply that
    // leaves the number line.
    const std::int64_t hostile[] = {(std::numeric_limits<std::int64_t>::min)(),
                                    -1,
                                    0,
                                    1,
                                    kScreenMinW - 1,
                                    kScreenMaxW,
                                    kScreenMaxW + 1,
                                    (std::numeric_limits<std::int64_t>::max)()};
    // The metric is on the same wire and gets the same treatment, so it is tried against the
    // same hostile set -- as its own axis rather than as a cross product with the extent's.
    // Sixty-four by sixty-four is four thousand screens and seventy thousand assertions for a
    // property neither axis needs the other to state; an assertion total inflated two orders
    // of magnitude for one property stops meaning anything.
    const auto judge = [](const Screen& sc) {
        CHECK(sc.w >= kScreenMinW);
        CHECK(sc.w <= kScreenMaxW);
        CHECK(sc.h >= kScreenMinH);
        CHECK(sc.h <= kScreenMaxH);
        // and the furniture stays a layout rather than becoming a shape: a room of at least a
        // cell, between bands that tile the screen with it.
        CHECK(sc.room_w >= kScreenMinW);
        CHECK(sc.room_h >= cells_px(1));
        CHECK(sc.room_y > 0);
        CHECK(sc.room_y + sc.room_h == sc.band_y);
        // The furniture that is every pane's, and the room's own rule: the room IS the
        // surface, at every extent this screen can be asked for.
        CHECK(sc.room_w == sc.w);
        CHECK(sc.band_y < sc.h);
    };

    for (const std::int64_t w : hostile) {
        for (const std::int64_t h : hostile) {
            judge(screen_of(w, h));         // no metric: the pre-HD-1 domain
            judge(screen_of(w, h, 8, 18));  // and the shipped face's
        }
    }
    // The metric's own axis, against the minimum screen and a large one -- an advance of
    // INT64_MIN and a line height of zero are as ordinary here as a width of -1.
    for (const std::int64_t advance : hostile) {
        for (const std::int64_t line : hostile) {
            judge(screen_of(kScreenMinW, kScreenMinH, advance, line));
            judge(screen_of(cells_px(200), cells_px(90), advance, line));
        }
    }
}

// ============================================================================
// Tier 11 -- a pane occupies POINTER space, not only pixels
// ============================================================================
// A press a visible pane covers is the pane's, not whatever is underneath -- a pane is a
// thing, not a picture of one. A press on a pane begins nothing, and a gesture begun on the
// workspace owns the pointer until its release: so no capture state exists.

TEST_CASE("a visible pane occupies the pointer space it covers") {
    // THE PRESS IS THE PANE'S where the pane is painted: it selects the pane and reaches
    // nothing behind it; with the pane gone, the same cell is the bare room's.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    const Screen sc = screen_of(t.session());
    const CellRect pane_rect =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect);
    const std::int64_t wx = 10;
    const std::int64_t wy = 5;
    REQUIRE(pane_rect.contains(wx + kRoomCellX, wy + kRoomCellY)); // the pane covers it
    t.press(wx, wy);
    CHECK(t.session().panes.selected == stock::kKind);
    CHECK(t.notice() != "nothing there");
    CHECK_FALSE(t.session().pane_drag.active);
    t.release(wx, wy);

    // AND THE SAME CELL IS THE ROOM'S THE MOMENT THE PANE IS REMOVED -- the occlusion is the
    // pane's presence and nothing else. The press above pointed the keys at the stand-in,
    // which takes them as every runtime pane does, so they are handed back first.
    release_keys(t);
    pick(t, stock::kKind);
    REQUIRE_FALSE(t.session().panes.has(stock::kKind));
    t.press(wx, wy);
    CHECK(t.notice() == "nothing there");
    CHECK(t.session().panes.selected == kNoPaneKind);
    t.release(wx, wy);
}

TEST_CASE("a press that lands on a pane begins nothing, so a hand that leaves it drags nothing") {
    // PRESS BEGINS ON THE PANE, POINTER LATER LEAVES IT. There is no capture state to get this
    // right: a press on a pane begins no gesture outside the arrangement, so there is nothing
    // for the motion to continue, and the absence of a gesture is the whole of the memory.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    const PixelRect before =
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect;
    t.press(10, 5); // on the stand-in
    CHECK_FALSE(t.session().pane_drag.active);
    t.motion(8, 11); // off it
    t.motion(9, 12);
    t.release(9, 12);
    CHECK_FALSE(t.session().pane_drag.active);
    CHECK(bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                    screen_of(t.session()))
              .rect == before);
    CHECK(t.notice().find("holding") == std::string::npos);
}

TEST_CASE("the columns the pane took are its own, and the band is the weaver's") {
    // BOTH SIDES OF THE TRADE, AT ONE EXTENT, THROUGH THE LIVE DOORS. At 200x60 a stack slot is
    // 124 cells wide, and that is two sentences: every added cell is opaque paint AND pointer
    // ownership, and the columns beyond it are still the weaver's to reach. Neither is proved
    // by `Rect::contains`: the paint is read off the published canvas and the presses go
    // through the pointer path a weaver's hand does.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    t.extent_px(cells_px(200), cells_px(60));
    open_stock_pane(t);

    const Screen sc = screen_of(t.session());
    REQUIRE(cells_of(sc).room_w == 200);
    const CellRect pane_rect =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect);
    REQUIRE(pane_rect == CellRect{0, 2, 124, 9});

    // ---- INSIDE THE WIDE SLOT. Workspace column 60 is past a 48-column slot and inside this
    // one: the pane's.
    REQUIRE(pane_rect.contains(60 + kRoomCellX, 3 + kRoomCellY));
    // ...AND OUTSIDE A 48-COLUMN SLOT (`kStackW`), said against that constant rather than
    // `kMinScreen`'s slot, which is 63 columns wide because the room is the surface.
    REQUIRE_FALSE(CellRect{kRoomCellX, kRoomCellY, kStackW, kStackRows}
                      .contains(60 + kRoomCellX, 3 + kRoomCellY));

    // IT IS PAINTED, and the paint is the whole rectangle: one opaque backdrop at the pane's
    // bounds, which is also its visible boundary in the ordinary pane chrome, and every row of
    // the INTERIOR padded to the interior's width, so a character medium's spaces erase what is
    // under them.
    const ScreenCanvas c = paint(t.session());
    CHECK(has_rect(c, pane_rect.x, pane_rect.y, pane_rect.w, pane_rect.h, kPaneChrome));
    CHECK_FALSE(has_rect(c, pane_rect.x, pane_rect.y, kStackW, pane_rect.h, kPaneChrome));
    const CellRect body = pane_body_cells(pane_rect);
    std::size_t padded = 0;
    for (const ScreenLabel& l : cell_text_of(c)) {
        const std::int64_t lx = gfx::cell_of_pixel(l.x), ly = gfx::cell_of_pixel(l.y);
        if (lx == body.x && ly >= body.y && ly < body.y + body.h) {
            CHECK(l.text.size() == static_cast<std::size_t>(body.w));
            ++padded;
        }
    }
    CHECK(padded == static_cast<std::size_t>(body.h));

    // AND IT IS OCCUPIED: the press is the pane's, not the room's.
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, 60 + kRoomCellX, 3 + kRoomCellY).occupied);
    t.press(60, 3);
    CHECK(t.session().panes.selected == stock::kKind);
    CHECK(t.notice() != "nothing there");
    t.release(60, 3);

    // ---- INSIDE THE RETAINED FREE BAND: workspace column 140, past the pane's right edge and
    // inside the room, on one of the pane's OWN rows -- the only place the claim means
    // anything. A press there is the room's, and says so.
    REQUIRE(140 >= pane_rect.x + pane_rect.w);
    REQUIRE(140 < cells_of(sc).room_w);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc, 140 + kRoomCellX,
                            3 + kRoomCellY)
                    .occupied);
    release_keys(t);
    t.press(140, 3);
    CHECK(t.notice() == "nothing there");
    t.release(140, 3);
}

TEST_CASE("what a pane is painted at and what it occupies are one resolved truth") {
    // THE STRUCTURAL CLAIM, swept over every cell of the canvas rather than
    // sampled: the occupancy answer IS the union of the open panes' bounds, and
    // those are the same rectangles `paint_panes` hands the painters. There is no
    // second geometry to drift.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    const Screen sc = screen_of(t.session());
    const Panes& panes = t.session().panes;
    REQUIRE(panes.open.size() == 2);

    const Sweep swept = sweep_canvas(panes, sc);
    CHECK(swept.cells == static_cast<std::size_t>(cells_of(sc).w * cells_of(sc).h));
    CHECK(swept.first_bad_x == -1); // named, so a failure says WHICH cell disagreed
    CHECK(swept.first_bad_y == -1);
    CHECK(swept.disagreed == 0);
    // BOTH PLACES, EXACTLY: 63x9 of stack and the full-width two rows the Layouts pane defaults
    // to. Nothing is placed in the side region by this run -- a desk row can still name it,
    // which `"occupancy is resolved against the screen"` measures -- so the union is two
    // disjoint rectangles here, and `swept.occupied` is still SWEPT rather than added up
    // because an overlap is a fact about a desk.
    CHECK(swept.occupied == static_cast<std::size_t>(63 * 9 + cells_of(sc).w * kTopRows));
    CHECK(swept.painted == swept.occupied);

    // AND WHAT THE PAINTERS ACTUALLY WROTE IS INSIDE THE SPACE THAT IS OCCUPIED --
    // painted through the same one call the screen makes, into a canvas holding
    // nothing else.
    ScreenCanvas only_panes;
    paint_panes(only_panes, t.session(), sc);
    REQUIRE_FALSE(cell_text_of(only_panes).empty());
    for (const ScreenLabel& l : cell_text_of(only_panes)) {
        CHECK(occupied_at(panes, setup_for(panes), sc, gfx::cell_of_pixel(l.x),
                          gfx::cell_of_pixel(l.y))
                  .occupied);
    }
    // ONE BACKDROP PER OPEN PANE, AND EACH IS ITS OWN BOUNDS. Asserted as an EQUALITY against
    // `bounds_of` rather than containment: the failure to guard is a pane painting less than
    // it occupies, and "inside the occupied space" is satisfied by a backdrop over half of it.
    REQUIRE(all_rects(only_panes).size() == panes.open.size());
    for (const OpenPane& p : panes.open) {
        const CellRect pb =
cells_covered(bounds_of(panes, setup_for(panes), p.kind, sc).rect);
        CHECK(has_rect(only_panes, pb.x, pb.y, pb.w, pb.h, kPaneChrome));
    }
    for (const ScreenRect& r : all_rects(only_panes)) {
        const CellRect cells = covered_cells(r);
        CHECK(occupied_at(panes, setup_for(panes), sc, cells.x, cells.y).occupied);
        CHECK(occupied_at(panes, setup_for(panes), sc, cells.x + cells.w - 1,
                          cells.y + cells.h - 1)
                  .occupied);
    }
}

TEST_CASE("an anchored pane keeps its corner where its word puts it, at every size of the window") {
    // ⚔ MUTATION: `pane_part` reading every place word as `pixels` -- the pane stands at its
    // offsets from the room's top-left, and its corner leaves the room's bottom-right.
    Live t;
    pick(t, second::kKind);
    stage_desk(t, [](Setup& desk) {
        for (SetupPane& row : desk.panes) {
            if (row.ref == ref_of(second::kKind)) {
                row.place = PanePlace{ui::kPlaceBottomRight, 24, 36};
                row.width = PaneSize{pane_unit::kPixels, 240};
                row.height = PaneSize{ui::kSizeShare, 2500};
            }
        }
    });
    // Two terminal rooms and a window's, whose face sets type.
    const std::int64_t sizes[][4] = {{cells_px(80), cells_px(30), 0, 0},
                                     {cells_px(120), cells_px(44), 0, 0},
                                     {1913, 1031, 8, 18}};
    for (const auto& size : sizes) {
        CAPTURE(size[0]);
        CAPTURE(size[1]);
        t.extent_px(size[0], size[1], size[2], size[3]);
        const Screen sc = screen_of(t.session());
        const std::int64_t step = chrome_grain(sc);
        const PaneBounds b =
            bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc);
        REQUIRE(b.open);
        // ITS OWN BOTTOM-RIGHT STANDS 24 IN FROM THE ROOM'S RIGHT AND 36 UP FROM ITS FLOOR.
        CHECK(b.resolved.x + b.resolved.w == sc.room_w - 24);
        CHECK(b.resolved.y + b.resolved.h == sc.room_y + sc.room_h - 36);
        CHECK(b.resolved.w == 240);
        // ...and a quarter of the room tall, its far edge floored to the window's step.
        CHECK(b.resolved.h >= sc.room_h / 4);
        CHECK(b.resolved.h <= sc.room_h / 4 + step);
    }
}

TEST_CASE("a centred pane stands in the middle of the room, and a fill runs to the room's far edge") {
    // ⚔ MUTATION: a `center` place read as `default` -- the pane stands down the stack.
    Live t;
    pick(t, second::kKind);
    stage_desk(t, [](Setup& desk) {
        for (SetupPane& row : desk.panes) {
            if (row.ref == ref_of(second::kKind)) {
                row.place = PanePlace{ui::kPlaceCenter, 0, 0};
                row.width = PaneSize{ui::kSizeShare, 5000};
                row.height = PaneSize{pane_unit::kPixels, cells_px(4)};
            }
        }
    });
    const Screen sc = screen_of(t.session());
    const PaneBounds b = bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc);
    REQUIRE(b.open);
    const std::int64_t step = chrome_grain(sc);
    CHECK(b.resolved.x == sc.room_w / 4 / step * step);
    CHECK(b.resolved.x + b.resolved.w == sc.room_w * 3 / 4 / step * step);
    CHECK(b.resolved.y - sc.room_y == (sc.room_h - cells_px(4)) / 2 / step * step);
    // A FILL FROM A LEFT OFFSET runs to the room's right edge, whatever the window.
    stage_desk(t, [](Setup& desk) {
        for (SetupPane& row : desk.panes) {
            if (row.ref == ref_of(second::kKind)) {
                row.place = PanePlace{ui::kPlaceLeft, 60, 0};
                row.width = PaneSize{ui::kSizeFill, 0};
            }
        }
    });
    const PaneBounds filled =
        bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc);
    CHECK(filled.resolved.x == 60);
    CHECK(filled.resolved.x + filled.resolved.w == sc.room_w);
}

TEST_CASE("a bar a third of the way down stays a third down at every size, in both windows, its ends at the room's sides") {
    // ⚔ MUTATION: `ui_place_of` dropping a share place's numbers -- the bar stands at the
    // room's top.
    Live t;
    pick(t, second::kKind);
    stage_desk(t, [](Setup& desk) {
        for (SetupPane& row : desk.panes) {
            if (row.ref == ref_of(second::kKind)) {
                row.place = PanePlace{ui::kPlaceShare, 0, 3333};
                row.width = PaneSize{ui::kSizeFill, 0};
                row.height = PaneSize{pane_unit::kPixels, 48};
            }
        }
    });
    // Three terminal rooms and three of a window whose face sets type.
    const std::int64_t sizes[][4] = {{cells_px(80), cells_px(30), 0, 0},
                                     {cells_px(120), cells_px(44), 0, 0},
                                     {cells_px(160), cells_px(50), 0, 0},
                                     {1000, 640, 8, 18},
                                     {1500, 860, 8, 18},
                                     {1913, 1031, 8, 18}};
    for (const auto& size : sizes) {
        CAPTURE(size[0]);
        CAPTURE(size[1]);
        t.extent_px(size[0], size[1], size[2], size[3]);
        const Screen sc = screen_of(t.session());
        const std::int64_t step = chrome_grain(sc);
        const PaneBounds b =
            bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc);
        REQUIRE(b.open);
        // ITS TOP A THIRD OF THE ROOM DOWN, floored to the window's step...
        const std::int64_t third = sc.room_h * 3333 / 10000;
        CHECK(b.resolved.y - sc.room_y == third / step * step);
        // ...its ends at the room's sides, 48 pixels tall...
        CHECK(b.resolved.x == 0);
        CHECK(b.resolved.w == sc.room_w);
        CHECK(b.resolved.h == 48);
        // ...touching neither the room's top nor its floor.
        CHECK(b.resolved.y > sc.room_y);
        CHECK(b.resolved.y + b.resolved.h < sc.room_y + sc.room_h);
    }
}

TEST_CASE("occupancy is resolved against the screen, not remembered from one") {
    // A BIGGER SURFACE MOVES A PANE AT ITS TOP-RIGHT, and the cells it occupies move with
    // it -- because the occupancy answer is `bounds_of` on the CURRENT screen and
    // is cached nowhere. The stack is anchored to the other corner and does not
    // move, which is the same asymmetry the painting has.
    Live t;
    // THE PANE AT THE TOP-RIGHT IS AUTHORED, NOT INHERITED: a desk row's word is what puts a
    // pane there.
    open_at_top_right(t, second::kKind);
    const Screen small = screen_of(t.session());
    const CellRect side_small =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, second::kKind, small).rect);
    REQUIRE(occupied_at(t.session().panes, t.session().setup.active, small, side_small.x, 4).occupied);

    t.extent_px(cells_px(100), cells_px(33));
    const Screen big = screen_of(t.session());
    REQUIRE(cells_of(big).w == 100);
    const CellRect side_big =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, second::kKind, big).rect);
    CHECK(side_big.x == cells_of(big).strip_x);
    CHECK(side_big.x > side_small.x);

    // The column a weaver presses is the column they can see, on either screen.
    CHECK(occupied_at(t.session().panes, t.session().setup.active, big, side_big.x, 4).occupied);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, big, side_small.x, 4).occupied);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, big, side_big.x - 1, 4).occupied);

    // AND THE PRESS FOLLOWS IT, through the whole live path: the small screen's column is
    // ordinary workspace on the big one, and a press there meets the bare room.
    t.press(side_small.x, 20);
    CHECK(t.notice() == "nothing there");
    // AND THE PANE THERE ANSWERS: the press selects it -- an ordinary pane's press is its
    // provider's, and what the host does with it is select the pane. What is asserted is that
    // the press was ANSWERED THERE: the workspace's own "nothing there" would be the
    // click-through this case exists to refuse.
    REQUIRE(t.session().panes.selected != second::kKind);
    t.press(side_big.x, 3);
    CHECK(t.session().panes.selected == second::kKind);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, big, side_big.x, 3).kind ==
          second::kKind);
}

TEST_CASE("a closed pane occupies nothing, and neither does a screen with none open") {
    // THE OTHER HALF OF THE INVARIANT, and the one a weaver feels every second: a
    // pane that is not open takes nothing away. `bounds_of` answers a closed
    // pane with an empty rectangle, `contains` says an empty rectangle holds
    // nothing, and the whole canvas is therefore the workspace's again.
    Live t;
    // Remove the one pane a fresh session opens with, the Layouts pane: it is not furniture,
    // so it goes through the close door. Info is a weave with no office here, so the desk names
    // it and opens nothing.
    pick(t, pane_kind::kLayouts);
    REQUIRE(t.session().panes.open.empty());
    const Screen sc = screen_of(t.session());
    const Sweep swept = sweep_canvas(t.session().panes, sc);
    CHECK(swept.cells == static_cast<std::size_t>(cells_of(sc).w * cells_of(sc).h));
    CHECK(swept.occupied == 0);
    CHECK(swept.disagreed == 0);
    // Including the cells the two places WOULD have had, the stack's asked at its slot's own
    // first cell.
    const CellRect stack = cells_covered(placement_bounds(placement::kOverlayStack, 0, sc));
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc, stack.x, stack.y).occupied);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc, cells_of(sc).strip_x, 0).occupied);

    // And a press in the vacated column reaches the bare room, which is what "the
    // pane was the only thing in the way" means.
    t.press(10, 5);
    CHECK(t.notice() == "nothing there");
    t.release(10, 5);
}

TEST_CASE("the window's pixels meet the same pane the terminal's cells do") {
    // THE OTHER MEDIUM, through the OS window's own projection. Occupancy is
    // asked in CANVAS cells, so both media arrive at the same question -- and the
    // case is worth having because the two report different numbers for one place,
    // which is exactly where a second geometry would hide.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    t.press_px(10, 5);
    CHECK(t.session().panes.selected == stock::kKind);
    t.release_px(10, 5);

    release_keys(t);
    t.press_px(7, 11); // below the pane: the room's
    CHECK(t.notice() == "nothing there");
    t.release_px(7, 11);
}

TEST_CASE("the cells just outside a pane are ordinary workspace, on every edge") {
    // WHERE THE OCCLUSION STOPS, asked at the boundary rather than in the middle.
    // A press one row below the stack's last row is the cell that separates "the
    // pane occupies what it covers" from "the pane occupies a bit more", and it
    // is also the cell that separates CANVAS cells from WORKSPACE cells: the two
    // spaces are one row apart, so a question asked in the wrong one is invisible
    // everywhere except here.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    // TWO PLACES WITH A PANE IN EACH: the stack, and the right column a desk row names -- an
    // authored row, not a catalog entry, fills it.
    open_at_top_right(t, second::kKind);
    open_stock_pane(t);
    const Screen sc = screen_of(t.session());
    const CellRect stack =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect);
    const CellRect side =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc).rect);

    // The four edges of each place, in canvas cells: inside, then one cell out.
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, stack.x, stack.y + stack.h - 1).occupied);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc, stack.x, stack.y + stack.h).occupied);
    // ⚠ THE ROW ABOVE THE STACK IS NOT WORKSPACE. The stack begins at `kRoomCellY`, so the
    // cell above it is the last reserved row at the top, where the Layouts pane stands and
    // occupies like any other pane -- so the boundary claim is that the cell belongs to the
    // pane ABOVE rather than to no one, which refuses a see-here/press-there divergence.
    {
        const Occupancy above =
            occupied_at(t.session().panes, t.session().setup.active, sc, stack.x, stack.y - 1);
        CHECK(above.occupied);
        CHECK(above.kind == pane_kind::kLayouts);
    }
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, stack.x + stack.w - 1, stack.y).occupied);
    // ⚠ AND THE CELL JUST RIGHT OF THE STACK IS NOT WORKSPACE: the slot's half-share is measured
    // against the whole surface, so at this extent it ends at 62 and the column begins at 50.
    // The two cells below ask the row-above question again: whose is it, not whether it is
    // anybody's.
    {
        const Occupancy right =
            occupied_at(t.session().panes, t.session().setup.active, sc, stack.x + stack.w,
                        stack.y);
        CHECK(right.occupied);
        CHECK(right.kind == second::kKind);
    }
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, side.x, side.y + side.h - 1).occupied);
    {
        const Occupancy left =
            occupied_at(t.session().panes, t.session().setup.active, sc, side.x - 1, side.y);
        CHECK(left.occupied);
        CHECK(left.kind == stock::kKind);
    }
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc, side.x, side.y + side.h).occupied);

    // AND THROUGH THE LIVE PRESS, which is the half that would catch a question
    // asked in the wrong space: the row under the pane is empty workspace, and
    // it says the empty-workspace sentence rather than the pane's.
    const std::int64_t below = stack.y + stack.h - kRoomCellY; // workspace row under it
    REQUIRE_FALSE(stack.contains(10 + kRoomCellX, below + kRoomCellY));
    t.press(10, below);
    CHECK(t.notice() == "nothing there");
    t.press(10, below - 1); // the pane's own last row
}

// The extents this composition lays out. A pane measures its own rows: the Terminal's prompt
// row is pinned in `tests/test_workshop_panes_terminal.cpp`, a component's window in
// `test_component.cpp`.
namespace {

/// The extents the pane-overlap case measures over: the minimum, the widths where the room is
/// narrower than the pane's want, the width where the two first agree, and up to the largest
/// surface this composition lays out.
const std::vector<std::pair<std::int64_t, std::int64_t>> kOverlapExtents = {
    {78, 22},  {79, 22},  {80, 23},  {81, 24},  {94, 30},  {98, 60},
    {100, 33}, {120, 40}, {160, 60}, {240, 80}, {640, 400}};

} // namespace

TEST_CASE("a pane may lie over a pane, and the boundary is what makes it legible") {
    // THREE FACTS ABOUT ANY TWO PANES: they may overlap, whichever is in front is painted where
    // it is hit, and `screen_of` reserves nothing for either. The Terminal is a pane like any
    // other, so no rule here is only the Terminal's.
    Live t;
    const PaneRef front{"zengine.workshop", "layouts"};
    open_pane(t, ref_of(stock::kKind));
    REQUIRE(t.session().panes.has(stock::kKind));

    // 1. THEY OVERLAP AT ALL, because a weaver may put a pane anywhere. Two panes in the
    //    overlay stack's slots is the arrangement a fresh session already produces.
    const Screen sc = screen_of(t.session());
    const CellRect a =
        cells_covered(bounds_of(t.session().panes, t.session().setup.active,
                                stock::kKind, sc).rect);
    const CellRect b =
        cells_covered(bounds_of(t.session().panes, t.session().setup.active,
                                pane_kind::kLayouts, sc).rect);
    CHECK(a.w > 0);
    CHECK(b.w > 0);

    // 2. WHICHEVER IS IN FRONT IS PAINTED WHERE IT IS HIT -- the front-order law, which is
    //    what a weaver uses to read one pane over another.
    (void)front;
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, a.x + 1, a.y + 1)
              .occupied);

    // 3. AND `screen_of` RESERVES NOTHING FOR ANY OF THEM: the room is the surface at every
    //    extent this composition lays out.
    for (const auto& wh : kOverlapExtents) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        const Screen at = screen_of(cells_px(wh.first), cells_px(wh.second));
        CHECK(at.room_w == at.w);
    }
}

TEST_CASE("the screen's furniture cannot see a pane, open or closed") {
    // THE TEST FOR THE WRONG OWNER, AS A CASE. Were a pane's placement "must not cover Info",
    // closing Info would move it -- a placement that moves because a weaver hid a list of names,
    // which `agents/decisions/the-room-is-the-screen.md` refuses. Nothing in `screen_of` can
    // see a pane; that is what lets every point of the room be an ordinary place.
    Session open_info;
    Session no_info;
    // ONE PANE TO CLOSE: a default `Session` opens what `kDefaultPanes` names, which is the
    // Layouts pane alone.
    REQUIRE(close_kind(no_info.panes, pane_kind::kLayouts));
    REQUIRE(no_info.panes.open.empty());
    REQUIRE_FALSE(open_info.panes.open.empty()); // the control really is the other case
    for (const auto& wh : kOverlapExtents) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        adopt_screen(open_info, cells_px(wh.first), cells_px(wh.second), 0, 0);
        adopt_screen(no_info, cells_px(wh.first), cells_px(wh.second), 0, 0);
        const Screen a = screen_of(open_info);
        const Screen b = screen_of(no_info);
        // ⚠ THE CLAIM IS OVER THE WHOLE OF THE FURNITURE: NOTHING this function answers moves
        // because a pane is open or closed.
        CHECK(a.room_w == b.room_w);
        CHECK(a.room_h == b.room_h);
        CHECK(a.room_y == b.room_y);
        CHECK(a.band_y == b.band_y);
        CHECK(b.room_w == b.w);
    }
}

namespace {

/// The character a weaver reads at one canvas cell, whatever painted it -- the medium's own
/// answer, read back off the raster rather than off the layer that claims the cell.
char cell_seen_at(const ScreenCanvas& c, std::int64_t x, std::int64_t y) {
    const std::vector<std::string> rows = rasterized(c);
    if (y < 0 || static_cast<std::size_t>(y) >= rows.size()) {
        return '\0';
    }
    const std::string& row = rows[static_cast<std::size_t>(y)];
    if (x < 0 || static_cast<std::size_t>(x) >= row.size()) {
        return '\0';
    }
    return row[static_cast<std::size_t>(x)];
}

} // namespace

TEST_CASE("nothing is painted behind the panes: where no pane stands, the room is the medium's own "
          "ground") {
    // EVERY MARK THE CANVAS CARRIES lies in a pane it belongs to or in the bottom band, so a cell
    // of the room no pane covers shows whatever the medium shows for nothing at all.
    Session s;
    admit_stock(s.panes);
    s.screen_w = cells_px(120);
    s.screen_h = cells_px(40);
    s.setup.active = two_overlays();
    s.panes.open = {OpenPane{stock::kKind}};
    const ScreenCanvas c = paint(s);
    const Screen sc = screen_of(s);
    std::vector<PixelRect> owned{band_bounds(sc)};
    for (const std::int64_t kind : effective_pane_order(s.setup.active, s.panes)) {
        const PaneBounds b = bounds_of(s.panes, s.setup.active, kind, sc);
        if (b.open) {
            owned.push_back(b.rect);
        }
    }
    REQUIRE(owned.size() >= 2); // the band and the stand-in
    const auto inside = [&owned](std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) {
        for (const PixelRect& r : owned) {
            if (x >= r.x && y >= r.y && x + w <= r.x + r.w && y + h <= r.y + r.h) {
                return true;
            }
        }
        return false;
    };
    std::size_t marks = 0;
    for (const ScreenPlane& layer : c.layers) {
        for (const ScreenRect& r : layer.rects) {
            CHECK_MESSAGE(inside(r.x, r.y, r.w, r.h), r.x << "," << r.y << " " << r.w << "x" << r.h);
            ++marks;
        }
        for (const ScreenLabel& l : layer.labels) {
            CHECK_MESSAGE(inside(l.x, l.y, 1, 1), l.x << "," << l.y << " " << l.text);
            ++marks;
        }
        for (const ScreenRegion& t : layer.texts) {
            CHECK_MESSAGE(inside(t.x, t.y, t.w, t.h), t.x << "," << t.y << " " << t.w << "x" << t.h);
            ++marks;
        }
    }
    CHECK(marks > 0);
    // ...AND THE ROOM HAS GROUND TO SHOW: a cell of it under no pane.
    CHECK_FALSE(inside(gfx::px_of_cells(60), sc.room_y + gfx::px_of_cells(20), 1, 1));
}

TEST_CASE("an overlapping pane is painted where it is hit, in both front orders") {
    // Two stand-ins share the stack, so their overlap is authored, and the claim is that what
    // the hand meets is what the eye reads, in either front order.
    Session s;
    admit_stock(s.panes); // the stand-in, first (stock)
    admit_second(s.panes); // ...and the second
    s.screen_w = cells_px(120);
    s.screen_h = cells_px(40);
    s.setup.active = two_overlays();
    s.panes.open = {OpenPane{stock::kKind}, OpenPane{second::kKind}};

    // THE ONE CELL TWO PRESENTATIONS CAN BOTH CLAIM. The Editor is taken out of the slot queue
    // FIRST -- a pane that names its own place does not queue for one it will not use -- and
    // only then is the second pane's rectangle read, because that is the rectangle it keeps
    // for the rest of this case.
    const Screen sc = screen_of(s);
    REQUIRE(author_pane_place(s.setup.active, ref_of(stock::kKind), 0, 0).accepted);
    const CellRect manager =
        pane_body_cells(bounds_of(s.panes, s.setup.active, second::kKind, sc).rect);
    REQUIRE(manager.w > 0);
    const std::int64_t x = manager.x;
    const std::int64_t y = manager.y;
    REQUIRE(place_at_canvas(s, ref_of(stock::kKind),
                              gfx::px_of_cells(x - kChromeCells),
                              gfx::px_of_cells(y - kChromeCells))
                .accepted);
    // THE PROBE IS THE SECOND LETTER OF EACH PANE'S OWN NAME IN ITS HEADER -- `Stack` against
    // `Second` -- because the two stand-ins paint the same waiting line under it, and a probe
    // on a cell they agree about would measure nothing.
    const std::int64_t cx = x + static_cast<std::int64_t>(std::string(kTypingElsewhere).size()) + 1;
    const std::int64_t cy = y;
    REQUIRE(cells_covered(bounds_of(s.panes, s.setup.active, stock::kKind, sc).rect)
                .contains(cx, cy));
    REQUIRE(cells_covered(bounds_of(s.panes, s.setup.active, second::kKind, sc).rect)
                .contains(cx, cy));

    // THE TWO CONTROLS: what each pane draws there WITH THE OTHER ABSENT. Neither rectangle
    // depends on the other, so these are the same two pictures the overlap is made of.
    const auto alone = [&](std::int64_t kind) {
        Session one = s;
        one.setup.active = Setup{};
        one.setup.active.name = "one";
        REQUIRE(add_pane(one.setup.active, ref_of(kind)));
        if (kind == stock::kKind) {
            REQUIRE(place_at_canvas(one, ref_of(kind),
                                      gfx::px_of_cells(x - kChromeCells),
                                      gfx::px_of_cells(y - kChromeCells))
                        .accepted);
        }
        one.panes.open = {OpenPane{kind}};
        return cell_seen_at(paint(one), cx, cy);
    };
    const char editor_alone = alone(stock::kKind);
    const char manager_alone = alone(second::kKind);
    // AND THE CONTROL ON THE CONTROLS: the two draw DIFFERENT characters there, so neither
    // assertion below can pass by the media agreeing about nothing.
    REQUIRE(editor_alone != manager_alone);

    for (const std::int64_t front : {stock::kKind, second::kKind}) {
        CAPTURE(front);
        REQUIRE(send_to_front(s.setup.active, ref_of(front)));
        // WHAT THE HAND MEETS...
        CHECK(occupied_at(s.panes, s.setup.active, sc, cx, cy).what == kind_name(s.panes, front));
        // ...IS WHAT THE MEDIUM PAINTS.
        CHECK(cell_seen_at(paint(s), cx, cy) ==
              (front == stock::kKind ? editor_alone : manager_alone));
    }
}

TEST_CASE("the close door can reach and remove an unresolved row") {
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(add_pane(desk, stranger()));
    });
    // THE ROW IS IN THE ONE INVENTORY, SO WHATEVER LISTS IT CAN NAME IT TO THE DOOR. A
    // population listed from one source and removed through another is a row a weaver can see
    // and cannot touch.
    bool listed = false;
    for (const CatalogRow& row : inventory_rows(t.session().setup.active, t.session().panes)) {
        listed = listed || row.ref == stranger();
    }
    REQUIRE(listed);
    const PaneCloseAnswered said = hand_close(t, stranger());
    CHECK(said.closed);
    CHECK_MESSAGE(said.refusal.empty(), said.refusal);
    CHECK_FALSE(has_pane(t.session().setup.active, stranger()));
    // AND THE ROWS THAT WERE THERE ARE STILL THERE: the gesture removed what it was on. Both
    // of a fresh desk's rows, including the one that is ALSO unresolved -- the sharper half:
    // removing one unresolved row leaves the other standing.
    CHECK(has_pane(t.session().setup.active, ref_of(pane_kind::kLayouts)));
    CHECK(has_pane(t.session().setup.active, info_ref()));
}

TEST_CASE("a clipped default resize begins from the full resolved size") {
    // ---- THE RIGHT EDGE. A default width of a hundred and four, four cells of it on the canvas.
    {
        Live t;
        t.extent_px(cells_px(160), cells_px(44), 0, 0);
        open_pane(t, ref_of(stock::kKind));
        const PaneRef builder = ref_of(stock::kKind);
        REQUIRE(place_at_canvas(t, builder, cells_px(156), cells_px(2))
                    .accepted);

        const Screen sc = screen_of(t.session());
        const PaneBounds where =
            bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc);
        // THE DEVELOPER'S HALF-SHARE AT THIS EXTENT: 104 of a 160-cell room, four cells of it
        // on the canvas.
        REQUIRE(where.resolved.w == cells_px(104));
        REQUIRE(where.rect.w == cells_px(4));

        // THE KEY. One cell wider than what the pane RESOLVES to, not one cell wider than
        // the sliver of it a weaver can currently see.
        enter_arrange_desk(t);
        select_pane(t, builder);
        t.key(input::scan::kRight, input::mod::kShift);
        const SetupPane* row = pane_of(t.session().setup.active, builder);
        REQUIRE(row != nullptr);
        CHECK(row->width.mode == pane_unit::kPixels);
        CHECK(row->width.amount == where.resolved.w + cells_px(1));
        // AND THE AXIS THE EDGE DID NOT NAME KEEPS EXACTLY WHAT IT HAD, mode included: a
        // width edit leaves a default height still reacting to the room.
        CHECK(row->height.mode == pane_unit::kDefault);
        CHECK(row->height.amount == 0);

        // AND THE HAND, FROM THE SAME BASE. The affordance stays on the VISIBLE boundary --
        // that is where a weaver's eye and hand are -- and its delta applies to the resolved
        // size. THE PRESS RECORDS THAT BASE AND THE MOTION SPENDS IT, and both halves are
        // asserted: a case that stopped at `base_w` would witness what the gesture
        // remembered rather than what it authored, which is the half a weaver actually sees.
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(reset_pane_width(desk, builder));
        });
        const CellRect vis =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                      screen_of(t.session())).rect);
        t.press_at(vis.x + vis.w - 1, vis.y + vis.h - 1,
                   input::space::kCells);
        REQUIRE(t.session().pane_drag.active);
        REQUIRE(t.session().pane_drag.sizing);
        CHECK(t.session().pane_drag.base_w == where.resolved.w);
        t.hand_moment(input::PointerMoved{
            vis.x + vis.w, vis.y + vis.h - 1, 0, 0,
            input::space::kCells, input::mod::kNone});
        t.release(0, 0);
        const SetupPane* pulled = pane_of(t.session().setup.active, builder);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->width.mode == pane_unit::kPixels);
        CHECK(pulled->width.amount == where.resolved.w + cells_px(1));
        CHECK(pulled->height.mode == pane_unit::kDefault);
        CHECK(pulled->height.amount == 0);
    }

    // ---- THE BOTTOM EDGE. The same sentence about the other axis, on a fresh session so
    // that nothing the width half authored can answer a default-height question for it: a
    // default height of nine, two rows of it on the canvas.
    {
        Live t;
        t.extent_px(cells_px(160), cells_px(44), 0, 0);
        open_pane(t, ref_of(stock::kKind));
        const PaneRef builder = ref_of(stock::kKind);
        REQUIRE(place_at_canvas(t, builder, 0, cells_px(42)).accepted);

        const Screen sc = screen_of(t.session());
        const PaneBounds where =
            bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc);
        // THE DEVELOPER'S STACK HEIGHT, of which two rows are on the canvas.
        REQUIRE(where.resolved.h == cells_px(9));
        REQUIRE(where.rect.h == cells_px(2));

        enter_arrange_desk(t);
        select_pane(t, builder);
        t.key(input::scan::kDown, input::mod::kShift);
        const SetupPane* row = pane_of(t.session().setup.active, builder);
        REQUIRE(row != nullptr);
        CHECK(row->height.mode == pane_unit::kPixels);
        CHECK(row->height.amount == where.resolved.h + cells_px(1));
        CHECK(row->width.mode == pane_unit::kDefault);
        CHECK(row->width.amount == 0);

        stage_desk(t, [&](Setup& desk) {
            REQUIRE(reset_pane_height(desk, builder));
        });
        const CellRect vis =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                      screen_of(t.session())).rect);
        // THE MIDDLE OF THE BOTTOM RUN, which is the cell on that edge and on neither of the
        // corners it shares the run with -- a two-row sliver still has one.
        const std::int64_t cx = vis.x + vis.w / 2;
        const std::int64_t cy = vis.y + vis.h - 1;
        t.press_at(cx, cy, input::space::kCells);
        REQUIRE(t.session().pane_drag.active);
        REQUIRE(t.session().pane_drag.sizing);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kBottom);
        CHECK(t.session().pane_drag.base_h == where.resolved.h);
        t.hand_moment(input::PointerMoved{cx, cy + 1, 0,
                                                     0, input::space::kCells,
                                                     input::mod::kNone});
        t.release(0, 0);
        const SetupPane* pulled = pane_of(t.session().setup.active, builder);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->height.mode == pane_unit::kPixels);
        CHECK(pulled->height.amount == where.resolved.h + cells_px(1));
        CHECK(pulled->width.mode == pane_unit::kDefault);
        CHECK(pulled->width.amount == 0);
    }

    // ---- ONE CORNER, ONE ILLEGAL AXIS, AND ONLY ITS OWN AXIS HELD. A corner whose height is
    // illegal still widens: the real management route, at the terminal's cell grain, refuses
    // the vertical axis atomically while the legal horizontal transaction lands, and the write
    // is a status rather than an alert.
    {
        Live t;
        t.extent_px(cells_px(160), cells_px(44), 0, 0);
        open_pane(t, ref_of(stock::kKind));
        const PaneRef builder = ref_of(stock::kKind);
        enter_arrange_desk(t);
        select_pane(t, builder);
        const PaneBounds where =
            bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                      screen_of(t.session()));
        REQUIRE(where.rect.w > 2);
        REQUIRE(where.rect.h > 2);

        // BOTH AXES PARTICIPATE, because a corner names two of them.
        const CellRect vis_cells = cells_covered(where.rect);
        t.press_at(vis_cells.x + vis_cells.w - 1,
                   vis_cells.y + vis_cells.h - 1,
                   input::space::kCells);
        REQUIRE(t.session().pane_drag.active);
        REQUIRE(t.session().pane_drag.sizing);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kBottomRight);
        REQUIRE(t.session().pane_drag.base_w == where.resolved.w);
        REQUIRE(t.session().pane_drag.base_h == where.resolved.h);

        // ONE CELL OUT AND A WHOLE HEIGHT UP: a width of ninety, which is legal and changed,
        // beside a height of zero, which is not a size at all.
        t.hand_moment(input::PointerMoved{
            vis_cells.x + vis_cells.w,
            vis_cells.y + vis_cells.h - 1 - (where.resolved.h / gfx::kCellPx),
            0, 0, input::space::kCells, input::mod::kNone});

        // THE LEGAL AXIS LANDED AND THE ILLEGAL ONE HELD: the width is authored one cell
        // out, the height keeps its default mode untouched, and the place — a trailing
        // corner proposes no position — stays reactive on both axes.
        INFO(t.session().notice);
        CHECK_FALSE(t.session().notice_is_bad);
        const SetupPane* held = pane_of(t.session().setup.active, builder);
        REQUIRE(held != nullptr);
        CHECK(held->width.mode == pane_unit::kPixels);
        CHECK(held->width.amount == where.resolved.w + cells_px(1));
        CHECK(held->height.mode == pane_unit::kDefault);
        CHECK(held->height.amount == 0);
        CHECK(held->place.mode == pane_unit::kDefault);

        // AND THE GESTURE ENDS THE ORDINARY WAY, because an answer — either answer — is
        // not a broken hand.
        t.release(0, 0);
        CHECK_FALSE(t.session().pane_drag.active);
    }
}

TEST_CASE("a release ends a pane gesture whatever mode sees it") {
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    open_pane(t, ref_of(stock::kKind));
    const PaneRef builder = ref_of(stock::kKind);
    enter_arrange_desk(t);
    select_pane(t, builder);
    const Screen sc = screen_of(t.session());
    const CellRect rect =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect);
    REQUIRE(rect.w > 4);

    // A MOVE BEGUN, THEN A MODE ARRIVES OVER THE TOP OF IT: an arrangement scope entered
    // mid-drag must not swallow the release, or `pane_drag.active` stays true with the button
    // up and the next bare motion moves a pane nobody is holding.
    t.press_at(rect.x + 1, rect.y + 1, input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    enter_arrange_desk(t);
    REQUIRE(t.session().arrange.open);
    t.release(0, 0);
    // THE GESTURE IS OVER. Occluding a release is the one thing this file already knew not
    // to do for a document drag; a pane gesture is the same sentence about a different hand.
    CHECK_FALSE(t.session().pane_drag.active);

    // AND A LATER BARE MOTION MOVES NOTHING, which is what a stranded gesture would do.
    const SetupPane* before = pane_of(t.session().setup.active, builder);
    REQUIRE(before != nullptr);
    const std::int64_t place_mode = before->place.mode;
    const std::int64_t place_x = before->place.x;
    t.key(input::scan::kEscape); // leave the mode
    REQUIRE_FALSE(t.session().arrange.open);
    t.motion(20, 20);
    const SetupPane* after = pane_of(t.session().setup.active, builder);
    REQUIRE(after != nullptr);
    CHECK(after->place.mode == place_mode);
    CHECK(after->place.x == place_x);

    // THE SAME FOR A SIZE GESTURE, which is a different record with the same custody. The
    // arrangement is re-entered (a size gesture is a scope's, not command mode's) and the
    // rectangle is re-resolved, because the release above PLACED the pane and the corner the
    // hand reaches for is wherever it is now.
    enter_arrange_desk(t);
    select_pane(t, builder);
    const CellRect now = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.press_at(now.x + now.w - 1, now.y + now.h - 1,
               input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE(t.session().pane_drag.sizing);
    enter_arrange_desk(t);
    t.release(0, 0);
    CHECK_FALSE(t.session().pane_drag.active);
}

TEST_CASE("a removed target leaves no stale selection, submode or heading") {
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    open_pane(t, ref_of(stock::kKind));
    const PaneRef builder = ref_of(stock::kKind);
    enter_arrange_desk(t);
    select_pane(t, builder);
    const Screen sc = screen_of(t.session());
    const CellRect rect =
cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect);
    t.press_at(rect.x + 1, rect.y + 1, input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);

    // THE TARGET LEAVES THE SETUP UNDER THE HAND.
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(remove_pane(desk, builder));
    });
    t.hand_moment(input::PointerMoved{rect.x + 4,
                                                 rect.y + 4, 0, 0,
                                                 input::space::kCells, input::mod::kNone});
    CHECK_FALSE(t.session().pane_drag.active);
    // MEMBERSHIP IS THE LAW: the pane is not in the setup, so nothing addresses it -- and the
    // DESK stays open, because its subject is the desk.
    CHECK_FALSE(t.session().arrange.addressed());
    CHECK(t.session().arrange.open);
    CHECK(t.session().arrange.desk);

    // AND AN UNRESOLVED ADDRESS IS KEPT, because unresolved is recoverable and still
    // authored -- membership, not presentation, is what clears one.
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(add_pane(desk, stranger()));
    });
    for (std::size_t guard = 0; guard < 8; ++guard) {
        if (t.session().arrange.pane == stranger()) {
            break;
        }
        t.key(input::scan::kTab);
    }
    REQUIRE(t.session().arrange.pane == stranger());
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    CHECK(t.session().arrange.pane == stranger());
}

TEST_CASE("removing the pane being arranged ends the arrangement about it") {
    // THE ONE-PANE SCOPE CLOSES WITH ITS PANE: an interaction bound to exactly one pane
    // is a state about nothing once that pane is gone -- and it closes SILENTLY, so the
    // removal's own sentence stays on the notice line. The desk's half of the same law
    // is pinned above; this is the scope the law was written for.
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    open_pane(t, ref_of(stock::kKind));
    const PaneRef builder = ref_of(stock::kKind);
    enter_arrange_desk(t);
    select_pane(t, builder);
    t.key(input::scan::kReturn); // narrow to arranging exactly this pane
    REQUIRE(t.session().arrange.open);
    REQUIRE_FALSE(t.session().arrange.desk);
    t.key(input::scan::kD); // remove the bound pane, by the scope's own key
    REQUIRE_FALSE(has_pane(t.session().setup.active, builder));
    CHECK_FALSE(t.session().arrange.open);
    CHECK_FALSE(t.session().arrange.addressed());
    CHECK(t.notice().find("hid ") == 0);
}

TEST_CASE("the opening gestures are claimed by the band's own truth") {
    // A MODE WITH NO WAY TO DISCOVER IT IS NOT A FEATURE. The claim surfaces are the keymap
    // projections -- the band's legend packs `help_pairs`, the full hotkey view lists
    // everything -- so the pin is on the TRUTH the band packs from: the three gestures are
    // pairs of the command context, spelled from the effective keymap, and the top row carries
    // only the pane's own heading.
    Session s; // the minimum screen, which is where every hint is tightest
    REQUIRE(screen_of(s).w == kScreenMinW);
    REQUIRE(screen_of(s).h == kScreenMinH);
    const std::vector<std::string> pairs =
        help_pairs(s.keymap, KeySpelling::kShort, KeyContext::kCommand);
    const auto has_pair = [&pairs](const std::string& want) {
        for (const std::string& pair : pairs) {
            if (pair == want) {
                return true;
            }
        }
        return false;
    };
    CHECK(has_pair("w arrange desk"));
    // THE KEY LIST AND THE PANE MANAGER ARE APPLICATION ROWS: taught from the desktop's
    // declaration, as it is in force, not by this host's own catalog -- and `p + panel` is no
    // pair of this host's.
    CHECK_FALSE(has_pair("p + panel"));
    CHECK_FALSE(has_pair("^k hotkeys"));
    CHECK_FALSE(has_pair("^p panes"));
    Keymap with_desktop = s.keymap;
    REQUIRE(join_app_rows(with_desktop,
                          std::vector<AppRow>{AppRow{"desktop.hotkeys", "hotkeys",
                                                     Gesture{input::scan::kK, input::mod::kCtrl},
                                                     app_precedence::kAboveModes},
                                              AppRow{"desktop.panes", "panes",
                                                     Gesture{input::scan::kP, input::mod::kCtrl},
                                                     app_precedence::kAboveModes}})
                .accepted);
    bool taught = false;
    bool panes = false;
    for (const std::string& pair :
         help_pairs(with_desktop, KeySpelling::kShort, KeyContext::kCommand)) {
        taught = taught || pair == "^k hotkeys";
        panes = panes || pair == "^p panes";
    }
    CHECK(taught);
    CHECK(panes);

    const std::vector<std::string> raster = rasterized(paint(s));
    REQUIRE_FALSE(raster.empty());
    const std::string& top = raster[0];
    INFO(top);
    CHECK(top.find("[window]") == std::string::npos);
    CHECK(top.find("[+ panel]") == std::string::npos);
    CHECK(top.find("terminal") == std::string::npos);
    CHECK(top.find("WORKSPACE") == std::string::npos);
    // AND INFO'S HEADING IS NOT IN THIS PICTURE: it is the Info pane's, and this session opens
    // no office. The case is about what the BAND claims, and the four assertions above are all
    // of it.
}

// ---- the Introspection tool, and what its rows are allowed to mean ---------------------
// Two tiers: what a reading MEANS, as pure functions over a value -- "every displayed fact is
// true" is a statement about a projection -- and the REAL `zengine-introspection` library
// through the real Kernel and Manager, rows read off a published canvas -- "the fact has an
// authoritative owner" is a statement about a path, which a mock owner would not prove.

// ---- Tier one: what a reading means -----------------------------------------------

TEST_CASE("the Manager's blob is read as name and role, and an absent role is a fact") {
    const std::vector<intro::LoadedWeave> got = intro::parse_loaded(
        "zengine-skin-tui-classic@zengine.skin,snake-controls,zengine-timer@zengine.timer");
    REQUIRE(got.size() == 3);
    CHECK(got[0].name == "zengine-skin-tui-classic");
    CHECK(got[0].role == "zengine.skin");
    // AN EMPTY ROLE IS THE KERNEL ANSWERING "this one holds none", not a reading that
    // failed -- `LoadLibrary` carries a role field that may legitimately be empty.
    CHECK(got[1].name == "snake-controls");
    CHECK(got[1].role.empty());
    CHECK(got[2].name == "zengine-timer");
    CHECK(got[2].role == "zengine.timer");
}

TEST_CASE("an empty map is an observed zero, and stray commas invent no weave") {
    CHECK(intro::parse_loaded("").empty());
    CHECK(intro::parse_loaded(",,,").empty());
    // A BLANK ROW IN AN INVENTORY IS INDISTINGUISHABLE FROM A WEAVE WHOSE NAME DID NOT
    // SURVIVE THE TRIP, so an empty entry yields no row at all.
    const std::vector<intro::LoadedWeave> got = intro::parse_loaded(",a@b,,c,");
    REQUIRE(got.size() == 2);
    CHECK(got[0].name == "a");
    CHECK(got[1].name == "c");
    // ZERO IS SAID, and it is said as a count rather than as a silence: a map that has
    // not been answered yet produces no content at all, so Workshop's own
    // `(waiting for the provider)` is what a weaver reads in that state.
    const std::vector<TextRow> none = intro::project_loaded({}, 8, 46).rows;
    REQUIRE_FALSE(none.empty());
    CHECK(none[0].text == "loaded weaves -- 0");
    CHECK(any_row(none, intro::kNotInProcess));
}

TEST_CASE("the split is on the LAST at-sign, and the ambiguity is bounded not solved") {
    // THE WIRE FORM HAS NO ESCAPING. The producer joins on a comma and an at-sign and
    // emits no delimiter of its own, so a name carrying either is unrecoverable in
    // principle. The reading chosen is the ambiguity's better half: a name with an
    // at-sign still resolves against a role that has none.
    const std::vector<intro::LoadedWeave> got = intro::parse_loaded("odd@name@zengine.role");
    REQUIRE(got.size() == 1);
    CHECK(got[0].name == "odd@name");
    CHECK(got[0].role == "zengine.role");
}

TEST_CASE("a weave with no role says so, rather than leaving the column blank") {
    const std::vector<TextRow> rows =
        intro::project_loaded({intro::LoadedWeave{"snake-controls", ""}}, 8, 46).rows;
    CHECK(any_row(rows, std::string("snake-controls @") + intro::kNoRole));
}

TEST_CASE("the heading counts the whole population, not the shown part") {
    // A COUNT THAT SHRANK WITH THE WINDOW WOULD BE THE ONE NUMBER ON THIS PANE A WEAVER
    // COULD NOT TRUST. It is taken from the population and never from the rows spent.
    for (const std::int64_t rows : {std::int64_t{4}, std::int64_t{8}, std::int64_t{40}}) {
        CAPTURE(rows);
        const std::vector<TextRow> out =
            intro::project_loaded(loaded_population(9), rows, 46).rows;
        REQUIRE_FALSE(out.empty());
        CHECK(out[0].text == "loaded weaves -- 9");
    }
}

TEST_CASE("what the list is NOT survives every budget that shows a list at all") {
    // THE RESERVATION, WHICH IS THE PROJECTION'S ONE POLICY. A count with an unstated
    // population is an honest number that leaves a false picture; a weaver reading
    // `loaded weaves -- 4` beside a running Builder would be right to conclude the
    // Builder is not running. So the sentence bounding the count is subtracted BEFORE
    // the list is offered anything but its first row, and the rows lost to it are
    // counted out loud.
    for (std::int64_t rows = 3; rows <= 40; ++rows) {
        CAPTURE(rows);
        const std::vector<TextRow> out =
            intro::project_loaded(loaded_population(12), rows, 46).rows;
        CHECK(any_row(out, intro::kNotInProcess));
        // FROM FOUR ROWS UP THERE IS A NAMED WEAVE, and the reason it is four rather
        // than three is the finding this case exists to hold: showing PART of a list
        // obliges saying how much was hidden, so an entry and its marker are ONE demand
        // on the budget. Three rows buys the count, the omission and the boundary; the
        // fourth is where a name fits.
        CHECK(any_row(out, "weave-0") == (rows >= 4));
        // ...AND EVERY WEAVE IS EITHER NAMED OR COUNTED, at every budget in the sweep.
        // Shown plus hidden is the population, which is the accounting the marker
        // exists to keep and the one thing a windowed list can get silently wrong.
        std::size_t named = 0;
        for (const TextRow& r : out) {
            if (r.text.rfind("  weave-", 0) == 0) {
                ++named;
            }
        }
        CHECK((named == 12 || any_row(out, "... " + std::to_string(12 - named) + " more")));
    }
}

TEST_CASE("an omission is counted on its own row and the count adds up") {
    const std::vector<TextRow> out =
        intro::project_loaded(loaded_population(20), 8, 46).rows;
    // Heading, some entries, the marker, the two notes -- every weave either shown or
    // counted, and nothing quietly dropped.
    std::size_t named = 0;
    for (const TextRow& r : out) {
        if (r.text.rfind("  weave-", 0) == 0) {
            ++named;
        }
    }
    REQUIRE(named > 0);
    CHECK(any_row(out, "... " + std::to_string(20 - named) + " more"));
}

TEST_CASE("at a budget too small to show and to say, it says") {
    // THE SHORTEST ANSWER THIS VIEW HAS: it cannot show a weaver a weave AND tell them
    // what it is hiding, so it tells them.
    const std::vector<TextRow> out =
        intro::project_loaded(loaded_population(20), 3, 46).rows;
    REQUIRE(out.size() == 3);
    CHECK(out[0].text == "loaded weaves -- 20");
    CHECK(out[1].text == "  ... 20 more");
    CHECK(out[2].text == std::string(intro::kNotInProcess));
}

TEST_CASE("every projection fits the room it was given, over the whole domain") {
    // THE OBLIGATION THAT IS NOT A COURTESY: a row past the room is not drawn, so a provider
    // that miscounts by one row loses what it said there. The sweep is the population crossed
    // with the budget, including the degenerate budgets no pane has.
    for (const std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{4}, std::size_t{31}}) {
        for (std::int64_t rows = 0; rows <= 12; ++rows) {
            for (const std::int64_t cols : {std::int64_t{0}, std::int64_t{1}, std::int64_t{3},
                                            std::int64_t{12}, std::int64_t{46}, std::int64_t{200}}) {
                CAPTURE(n);
                CAPTURE(rows);
                CAPTURE(cols);
                const std::vector<TextRow> out =
                    intro::project_loaded(loaded_population(n), rows, cols).rows;
                REQUIRE(static_cast<std::int64_t>(out.size()) <= rows);
                for (const TextRow& r : out) {
                    REQUIRE(static_cast<std::int64_t>(r.text.size()) <= cols);
                    for (const char c : r.text) {
                        const unsigned char byte = static_cast<unsigned char>(c);
                        // A row's plain-ASCII contract: a character is an advance of the
                        // window's text.
                        REQUIRE(byte >= 0x20u);
                        REQUIRE(byte < 0x7Fu);
                    }
                    // A blank separator is `role::kFill`, never `role::kNone` -- that
                    // value is the absence of a BACKGROUND and is not a window's ink.
                    REQUIRE(r.role != gfx::role::kNone);
                }
            }
        }
    }
}

TEST_CASE("a long name is cut with a mark rather than silently") {
    const std::vector<TextRow> out = intro::project_loaded(
        {intro::LoadedWeave{"a-library-with-a-very-long-name-indeed", "zengine.role"}}, 8, 20).rows;
    REQUIRE(out.size() >= 2);
    CHECK(out[1].text.size() == 20);
    CHECK(out[1].text.find(intro::kElided) != std::string::npos);
}

// ---- Tier two: the real library, through the real load path -----------------------

TEST_CASE("loading the real tool puts its pane in the catalog, offered by its office") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    REQUIRE(r.session().panes.runtime.entries.empty());

    const loom::WeaveId id =
        r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(r.load_refusals.empty());
    REQUIRE(id.valid());
    // WORKSHOP LEARNED THESE PANES FROM LIVE OFFERS and from nowhere else: no
    // `pane_kind::k*` was minted for any of them, no arm was compiled for any of them,
    // and every handle they carry is a runtime one.
    REQUIRE(r.session().panes.runtime.entries.size() == kIntroPaneCount);
    const RuntimePane* found = intro_row(r, kIntroPane);
    REQUIRE(found != nullptr);
    const RuntimePane& row = *found;
    CHECK(row.provider == std::string(kIntroOffice)); // Loom's stamp, not a payload field
    CHECK(row.pane == std::string(kIntroPane));
    CHECK(row.name == std::string(intro::kLoadedPaneName));
    CHECK(row.summary == std::string(intro::kLoadedPaneSummary));
    CHECK(is_runtime_kind(row.kind));

    // ...AND SO DID THE OTHER TWO. Three panes, one office, three distinct runtime handles --
    // `PaneOffered`'s own claim that a provider is not a pane, measured on the first office in
    // this repository with more than one.
    for (const char* pane : {intro::kArrangementPane, intro::kPowersPane}) {
        const RuntimePane* more = intro_row(r, pane);
        REQUIRE(more != nullptr);
        CHECK(more->provider == std::string(kIntroOffice));
        CHECK(is_runtime_kind(more->kind));
        CHECK(more->kind != row.kind);
        // A NAME AND A SUMMARY A WEAVER CAN READ WHOLE: admission allows thirty-two cells, so a
        // name within that bound reaches a weaver's eye unmarked.
        CHECK_FALSE(more->name.empty());
        CHECK(more->name.size() <= 10);
        CHECK_FALSE(more->summary.empty());
    }
    CHECK(intro_row(r, kIntroPane)->kind != intro_row(r, intro::kPowersPane)->kind);
}

TEST_CASE("the opened pane names what this Loom actually loaded, itself included") {
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(r.session().panes.runtime.entries.size() == kIntroPaneCount);
    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::int64_t kind = intro_row(r, kIntroPane)->kind;
    const std::vector<std::string> shown =
        loaded_rows(r, kind);

    // TWO LOADED WEAVES IN THIS RIG: the window's directory, and this one. Self-introspection
    // through the same observation path used for everyone else -- there is no registration
    // mirror for the tool to see itself in.
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0] == "loaded weaves -- 2");
    CHECK(any_row(shown, std::string("zengine-graphics @") + gfx::kGraphicsRole));
    CHECK(any_row(shown, std::string(intro::kIntrospectionStem) + " @" + kIntroOffice));
    // AND THE FACT IS BOUNDED WHERE A WEAVER READS IT.
    CHECK(any_row(shown, intro::kNotInProcess));
    CHECK(any_row(shown, intro::kSnapshotSource));
}

TEST_CASE("the count is the kernel's and moves when the kernel's map does") {
    // THE WITNESS IS NOT A HARDCODED NAME. A second library is loaded through the same
    // door, and the same pane -- re-granted its room by an extent change -- says two.
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::int64_t kind = intro_row(r, kIntroPane)->kind;
    REQUIRE(loaded_rows(r, kind)[0] ==
            "loaded weaves -- 2");

    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    // The Hello provider's own offer arrives too; that is the catalog's business and
    // not this pane's, and the pane's rows are unmoved until it is re-granted room.
    REQUIRE(r.session().panes.runtime.entries.size() == kIntroPaneCount + 1);
    CHECK(loaded_rows(r, kind)[0] ==
          "loaded weaves -- 2");

    // A WIDER SURFACE MOVES THE AREA'S ROOM, WHICH IS THIS TOOL'S ONE BEAT. Nothing polled and
    // nothing timed out.
    author_test_pane_room(r, kind, held_canvas_rows(*r.session().panes.external_pane(kind)).rows + 1,
                          held_canvas_rows(*r.session().panes.external_pane(kind)).columns + 1);
    r.extent(140, 40);
    const std::vector<std::string> after =
        loaded_rows(r, kind);
    REQUIRE_FALSE(after.empty());
    CHECK(after[0] == "loaded weaves -- 3");
    CHECK(any_row(after, "zengine-workshop-hello @" + std::string(kHelloOffice)));
}

TEST_CASE("the graphical medium grants a different budget and the view spends it") {
    // BOTH PROJECTIONS OF ONE PANE, and the provider composes for neither medium: it is handed
    // a room and the advance and line its text stands at, never a font, so what differs between
    // the readings is a few integers the window resolved. The metric
    // arrives as a NUMBER, which is how every medium-dependent claim here is proved on a lane
    // with no font engine.
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;

    const ExternalPane* cells = r.session().panes.external_pane(kind);
    REQUIRE(cells != nullptr);
    const std::int64_t cell_rows = held_canvas_rows(*cells).rows;
    const std::int64_t cell_cols = held_canvas_rows(*cells).columns;
    REQUIRE(cell_rows > 0);
    const std::vector<std::string> in_cells =
        loaded_rows(r, kind);

    // A REAL FACE'S METRIC over the same surface: an 18-pixel line in a 12-pixel cell
    // is fewer prose rows in the same rectangle, and a 10-pixel advance is more columns.
    author_test_pane_room(r, kind, cell_rows, cell_cols);
    r.extent(1200, 500, 10, 18, gfx::kCellPx); // a window reports its device scale
    const ExternalPane* graphical = r.session().panes.external_pane(kind);
    REQUIRE(graphical != nullptr);
    CHECK(held_canvas_rows(*graphical).rows != cell_rows);
    CHECK(held_canvas_rows(*graphical).columns != cell_cols);

    // AND THE VIEW ANSWERED THE NEW ROOM rather than the old one.
    const std::vector<std::string> in_pixels =
        loaded_rows(r, kind);
    REQUIRE_FALSE(in_pixels.empty());
    CHECK(in_pixels[0] == "loaded weaves -- 2");
    CHECK(any_row(in_pixels, intro::kIntrospectionStem));
    CHECK(static_cast<std::int64_t>(in_pixels.size()) <= held_canvas_rows(*graphical).rows);
    // THE SAME TRUTH IN BOTH, which is the honesty claim: two projections of one fact.
    CHECK(in_cells[0] == in_pixels[0]);
}

TEST_CASE("an in-process weave is absent from the list and the pane says why") {
    // THE ABSENCE THAT MATTERS. Workshop itself is a live participant holding a live
    // office in this very rig, and it is not in the kernel's map -- so a pane that
    // printed the count without the boundary would leave a weaver with a false picture
    // of their own system.
    PaneRig r;
    REQUIRE(r.mount_workshop() != nullptr);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;
    const std::vector<std::string> shown =
        loaded_rows(r, kind);

    CHECK_FALSE(any_row(shown, kWorkshopProvider)); // and it is not silently implied either
    CHECK(any_row(shown, intro::kNotInProcess));
}

TEST_CASE("the pane header says whose facts these are") {
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    // WORKSHOP'S OWN ROW, over rows written by a party this build never compiled into
    // itself: the office is the only thing about that party Workshop actually knows.
    CHECK(stack_text(r.shown())
              .find(std::string(intro::kLoadedPaneName) + " @" + kIntroOffice) !=
          std::string::npos);
}

TEST_CASE("the tool answers Workshop and refuses everybody else") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(r.session().panes.runtime.entries.size() == kIntroPaneCount);

    // AN UNAUTHENTICATED CATALOG REQUEST -- a root publication carrying no authored
    // role at all. Answering it would hand this tool's catalog to whoever asked.
    const std::size_t before = r.session().panes.runtime.entries.size();
    r.publish(loom::to_value(PaneCatalogRequested{}));
    CHECK(r.session().panes.runtime.entries.size() == before);
    // ...and the authored one is still answered, so the refusal above is about
    // AUTHORSHIP and not about the tool having stopped talking.
    r.ready();
    CHECK(r.session().panes.runtime.entries.size() == before);
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    CHECK(intro_row(r, kIntroPane)->name == std::string(intro::kLoadedPaneName));
}

TEST_CASE("a forged room produces no content at all") {
    // MEASURED FROM THE OTHER SIDE: a room no window's office authored draws nothing, and the
    // room a window gives the pane's area is drawn there.
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    const loom::WeaveId id = r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(id.valid());
    REQUIRE(watch->offers.size() == kIntroPaneCount);
    zengine::testing::pane_window::Window window(r.bus);
    const std::string area = std::string(kIntroOffice) + "." + kIntroPane;
    const std::size_t before = window.weave().heard.size();

    r.bus.send(id, loom::Message(loom::to_value(gfx::AreaRoom{area, window.name(), 1, 40 * gfx::kCellPx,
                                                              6 * gfx::kCellPx, {}, true}),
                                 loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
    CHECK(window.weave().heard.size() == before); // holding no window's office is giving no room

    window.room(area, kIntroOffice, 40, 6);
    r.bus.drain_until_idle();
    const std::vector<std::string> rows = window.rows(area);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows[0] == "loaded weaves -- 1");
}

TEST_CASE("adding this tool widens nothing -- it says three shapes and no more") {
    // THE AUTHORITY AUDIT, TAKEN FROM THE BUS RATHER THAN FROM THE DECLARATION.
    // `Emit<...>` is informational in this Loom and the loader binds `allow_any()` to
    // every library it opens, so what this weave DECLARES proves nothing on its own.
    // What the tap sees is every sentence it actually spoke across a whole life:
    // discovery, a room, a reading, a resize and a second reading.
    PaneRig r;
    std::vector<std::string> said;
    loom::WeaveId who{};
    const loom::ObserverId tap = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (who.valid() && e.sender == who && !e.schema_name.empty()) {
            said.push_back(e.schema_name);
        }
    });
    r.mount_workshop();
    who = r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(who.valid());
    r.ready();
    r.pick(intro_ref());
    r.extent(140, 40);
    r.bus.remove_observer(tap);

    REQUIRE_FALSE(said.empty());
    std::vector<std::string> distinct = said;
    std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
    // FIVE: the Powers pane's declared actions and the settings it takes ride beside its offer
    // (WL-KEY-15, WL-SETTING-03), declarations of what a pane DOES, not a reach into anything;
    // its rows are drawn in its own area of the window.
    const std::vector<std::string> allowed{"Drawn", "PaneActions", "PaneOffered",
                                           "PaneSettingsDeclared", "zen.ListLoaded"};
    CHECK(distinct == allowed);
    // NAMED NEGATIVELY TOO, because the interesting half of an authority audit is the
    // shapes that are ABSENT. Asking what is loaded is not being able to load anything:
    // a grant is per (shape, version, target), and none of these was ever spoken.
    for (const char* forbidden : {"zen.LoadWeave", "zen.SwapWeave", "zen.ReloadWeave",
                                  "zen.UnloadLibrary", "zen.UnloadRole", "zen.LoadLibrary",
                                  "BuildRequested"}) {
        CHECK(std::find(said.begin(), said.end(), std::string(forbidden)) == said.end());
    }
}

TEST_CASE("unload and reload -- waiting is said, and a reload recovers the view") {
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::int64_t kind = intro_row(r, kIntroPane)->kind;
    REQUIRE(any_row(loaded_rows(r, kind),
                    intro::kIntrospectionStem));

    // THE PROVIDER LEAVES. Workshop is told NOTHING -- Loom gives a participant no
    // unload notification -- so the catalog row stays, the pane stays open, and the
    // rows a weaver is looking at are the last valid ones. That is a stated limit and
    // not liveness.
    REQUIRE(r.unload(intro::kIntrospectionStem));
    REQUIRE(r.session().panes.runtime.entries.size() == kIntroPaneCount);
    CHECK(any_row(loaded_rows(r, kind),
                  intro::kIntrospectionStem));

    // ...and Workshop's next look is the moment the silence becomes visible: no participant holds
    // the office, so the pane's area goes and its frame says, in the window, that it WAITS --
    // never `unavailable`, which is a fate nothing here has observed.
    r.key(input::scan::kUnknown);
    const std::vector<std::string> gone =
        loaded_rows(r, kind);
    REQUIRE(gone.size() == 1);
    CHECK(gone[0] == std::string(kExternalWaiting) + intro::kLoadedPaneName);

    // THE PROVIDER COMES BACK. Its attested activation offers the same `PaneRef`, which
    // refreshes the descriptor in place -- so the area is placed again, the window gives it a
    // room, and the view returns with no gesture from the weaver.
    REQUIRE(r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice).valid());
    // identity de-duplicated all three
    CHECK(r.session().panes.runtime.entries.size() == kIntroPaneCount);
    const std::vector<std::string> back =
        loaded_rows(r, kind);
    REQUIRE_FALSE(back.empty());
    CHECK(back[0] == "loaded weaves -- 2");
    CHECK(any_row(back, intro::kIntrospectionStem));
}

TEST_CASE("the pane composes as an ordinary saved setup row") {
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    REQUIRE(has_pane(r.session().setup.active, intro_ref()));

    // THE FILE NAMES THE TWO STRINGS AND NOTHING THIS RUN INVENTED: no runtime handle,
    // no room, no rows, no rectangle.
    Setup authored = r.session().setup.active;
    authored.name = "Inspect";
    const std::string text = setup_persist::to_text(authored);
    CHECK(text.find(std::string("\"provider\":\"") + kIntroOffice + "\"") != std::string::npos);
    CHECK(text.find(std::string("\"pane\":\"") + kIntroPane + "\"") != std::string::npos);
    CHECK(text.find("loaded weaves") == std::string::npos);
    const setup_persist::LoadedSetup back = setup_persist::from_text(text);
    REQUIRE(back.outcome.accepted);
    CHECK(back.setup == authored);

    // AND IN A PROCESS WHERE THE TOOL IS ABSENT IT IS UNRESOLVED AND PRESERVED -- the
    // word is `unresolved`, never `unavailable`, and the row is kept rather than tidied
    // away out of somebody else's file.
    PaneRig fresh;
    fresh.mount_workshop();
    stage_desk(fresh, [&](Setup& desk) {
        desk = authored;
    });
    CHECK_FALSE(resolve_pane(intro_ref(), fresh.session().panes).has_value());
    CHECK(has_pane(fresh.session().setup.active, intro_ref()));
}

// ---- WHICH TEXT IS SEMANTIC, AND WHICH TEXT IS CELLS ------------------------------------
// Does a text name MEANING inside room it owns (a `ScreenRegion`, whose interior a medium
// may set in its own face) or glyphs at cells something else owns (a `ScreenLabel`, one cell
// per byte in every medium, right for a mark ON something)? A one-cell row stays a label: this
// face's 18-pixel line fits ZERO rows in a region one `kCellPx` cell tall, so
// `fit_region` hands it back to the cell projection.

TEST_CASE("the arrangement's visible statement is the ring on the pane itself") {
    // Entering a scope publishes no region in the stack's first slot: what says "you are
    // arranging" is the affordance ring on the panes -- muted over the arrangeable SET, accent
    // on the pane the keyboard addresses -- eight glyphs at the same edge cells the pointer
    // grabs, one geometry.
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    open_pane(t, ref_of(stock::kKind));
    enter_arrange_desk(t);
    const PixelRect at = bounds_of(t.session().panes, t.session().setup.active,
                                  stock::kKind, screen_of(t.session()))
                            .rect;
    const auto ring_roles = [&](std::int64_t edge) {
        // THE WIRE CURRENCY: a label's x/y are canvas PIXELS -- asserting the pixel is what
        // catches a construction that stays green while every mark paints off the canvas.
        const PixelRect cell = pane_edge_cell(at, edge);
        std::vector<std::int64_t> roles;
        for (const ScreenPlane& l : paint(t.session()).layers) {
            for (const ScreenLabel& g : l.labels) {
                if (g.x == cell.x && g.y == cell.y &&
                    g.text == std::string(pane_edge_glyph(edge))) {
                    roles.push_back(g.role);
                }
            }
        }
        return roles;
    };
    // UNADDRESSED: the desk shows the arrangeable set, muted.
    for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
        const std::vector<std::int64_t> roles = ring_roles(edge);
        REQUIRE(roles.size() == 1);
        CHECK(roles.front() == gfx::role::kMuted);
    }
    // ADDRESSED: the keyboard's target wears the ring in accent.
    select_pane(t, ref_of(stock::kKind));
    for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
        const std::vector<std::int64_t> roles = ring_roles(edge);
        REQUIRE(roles.size() == 1);
        CHECK(roles.front() == gfx::role::kAccent);
    }
    // AND NO REGION OPENED OVER THE STACK'S FIRST SLOT: the statement is on the pane, not in a
    // pane covering another one.
    const CellRect box = cells_covered(placement_bounds(placement::kOverlayStack, 0, screen_of(t.session())));
    for (const ScreenPlane& l : paint(t.session()).layers) {
        for (const ScreenRegion& r : l.texts) {
            const bool roster_region = covered_cells(r).x == box.x &&
                                       covered_cells(r).y == box.y && !r.rows.empty() &&
                                      r.rows[0].text.rfind("+ WINDOW", 0) == 0;
            CHECK_FALSE(roster_region);
        }
    }
    // LEAVING TAKES THE RING WITH IT -- the state's existence and its statement are one.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().arrange.open);
    for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
        CHECK(ring_roles(edge).empty());
    }
}

TEST_CASE("the band is the keys' legend alone, and the room takes the row the notice left") {
    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    const Screen sc = screen_of(s);

    // THE BOTTOM BAND IS ONE REGION -- the whole of its reserved cells, as tall as the legend's
    // rows in this face, from the room's floor to the screen's foot.
    const std::vector<ScreenRegion> quiet = regions_at(paint(s), 0, cells_of(sc).band_y);
    REQUIRE(quiet.size() == 1);
    const ScreenRegion& band = quiet.front();
    CHECK(band.w == sc.w);
    CHECK(band.y == sc.band_y);
    CHECK(band.y + band.h == sc.h);
    CHECK(sc.room_y + sc.room_h == sc.band_y);
    const RegionFit fit = band_fit(sc);
    CHECK(fit.graphical());
    CHECK(fit.rows == kBottomRows);
    CHECK(layouts_body(s, sc).rows == kTopRows); // ...and the identity's own rows, up there
    REQUIRE_FALSE(band.rows.empty());
    for (const TextRow& row : band.rows) {
        CHECK(row.role == gfx::role::kMuted); // every row is the legend's
    }
    // THE ROOM TOOK THE ROW: the minimum composition's workspace is a row taller than the four
    // rows of band it once stood over left it.
    CHECK(kMinScreen.room_h == 17 * gfx::kCellPx);

    // WHAT WORKSHOP SAID IS NOWHERE ON THE SCREEN, a statement or a refusal: the band is the
    // same rows either way, and no region of the screen carries the words.
    s.notice = "created #1 -- a new identity, not a new name";
    for (const bool bad : {false, true}) {
        s.notice_is_bad = bad;
        const ScreenCanvas said = paint(s);
        const std::vector<ScreenRegion> at_band = regions_at(said, 0, cells_of(sc).band_y);
        REQUIRE(at_band.size() == 1);
        CHECK(at_band.front().rows == band.rows);
        for (const ScreenPlane& plane : said.layers) {
            for (const ScreenRegion& region : plane.texts) {
                for (const TextRow& row : region.rows) {
                    CHECK(row.text.find(s.notice) == std::string::npos);
                }
            }
        }
    }

    // AND IN A CHARACTER MEDIUM, the same: the legend, whole cells, and no notice.
    Session cell = s;
    cell.text_advance_px = 0;
    cell.text_line_px = 0;
    const Screen cell_sc = screen_of(cell);
    const std::vector<ScreenRegion> at_cells = regions_at(paint(cell), 0, cells_of(cell_sc).band_y);
    REQUIRE(at_cells.size() == 1);
    CHECK(static_cast<std::int64_t>(at_cells.front().rows.size()) <= kBottomRows);
    for (const TextRow& row : at_cells.front().rows) {
        CHECK(row.text.find(s.notice) == std::string::npos);
    }
    CHECK(cell_sc.h - cell_sc.band_y == kBottomRows * gfx::kCellPx);
}

TEST_CASE("a pane with room for the header and nothing else still says whose it is") {
    // THE EDGE A PROSE HEADER CREATES, pinned rather than argued: the header is the region's
    // first PROSE row, and reserving it can leave the provider nothing -- so "is there a body"
    // must be asked AFTER the header is written, or a weaver gets a rectangle that says nothing.
    Panes panes;
    panes.runtime.entries.push_back(RuntimePane{kFirstRuntimeKind, "zengine.probe", "p",
                                                 "Probe", "a summary", {}});
    ExternalPane live;
    live.kind = kFirstRuntimeKind;
    live.held = true;
    live.in_window = true;
    panes.external.push_back(live);

    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    const Screen sc = screen_of(s);
    // FOUR CELLS: two of chrome around two of interior, which is one prose row of an 18-pixel
    // face -- and the header takes it whole.
    const CellRect tiny{0, 1, 48, 2 + 2 * kChromeCells};
    const ExternalBodyPlace body = external_body_place(
        pixels_of_cells(tiny), sc,
        external_title_rows(panes, kFirstRuntimeKind, /*titles_shown=*/true));
    CHECK(body.fit.rows == 1);
    CHECK_FALSE(body.present); // no room was granted, and none is invented
    CHECK(body.rows == 0);

    ScreenCanvas c;
    paint_external(plane(c), panes, kFirstRuntimeKind, pixels_of_cells(tiny), sc,
                   /*titles=*/true);
    const std::vector<ScreenRegion> at =
        regions_at(c, tiny.x + kChromeCells, tiny.y + kChromeCells);
    REQUIRE(at.size() == 1);
    REQUIRE(at.front().rows.size() == 1);
    // ...AND IT CARRIES THE TYPING MARK, unset: a header is the same width whether or not the
    // pane has the keyboard, because the unmarked form spends the same two columns.
    CHECK(at.front().rows[0].text == std::string(kTypingElsewhere) + "Probe @zengine.probe");
    CHECK(at.front().rows[0].role == gfx::role::kAccent);

    // AND WITH NO ROW OF TYPE AT ALL the painter says nothing rather than drawing a header
    // into a rectangle that cannot hold one.
    const CellRect none{0, 1, 48, 1};
    CHECK(fit_region(cells_px(none.x), cells_px(none.y), cells_px(none.w), cells_px(none.h), 8, 18).graphical() == false);
    ScreenCanvas flat;
    paint_external(plane(flat), panes, kFirstRuntimeKind, PixelRect{}, sc,
                   /*titles=*/true);
    CHECK(all_texts(flat).empty());
}

// ========================================================================================
// Fine-grained arrangement: the pane lattice is sub-cell, gestures are pixel-responsive,
// every resize edge preserves its opposite anchor, the TUI stays an honest cell projection,
// and the persisted formats migrate deliberately.
// ========================================================================================

namespace {

/// A Live Workshop on a graphical medium: a real text metric, so pointer events arrive
/// in window pixels and the fine lattice is reachable at its pixel grain.
struct FineRig : Live {
    /// `prefs`, when named: a settings file this Workshop reads as its desk starts.
    explicit FineRig(const std::string& prefs = std::string()) {
        if (!prefs.empty()) {
            host.prefs_path = prefs;
        }
        extent_on_window(160, 44);
        open_pane(*this, ref_of(stock::kKind));
        enter_arrange_desk(*this);
        select_pane(*this, ref_of(stock::kKind));
    }

    void motion_at(std::int64_t x, std::int64_t y, std::int64_t space) {
        hand_moment(input::PointerMoved{x, y, 0, 0, space, input::mod::kNone});
    }

    const SetupPane* builder_row() {
        return pane_of(session().setup.active, ref_of(stock::kKind));
    }

    PixelRect builder_rect() {
        return bounds_of(session().panes, session().setup.active, stock::kKind,
                         screen_of(session()))
            .rect;
    }

    /// Where the room begins on the canvas: an authored place is measured from it.
    std::int64_t room_y() { return screen_of(session()).room_y; }
};

/// A MOTION WITH MODIFIERS HELD, as a backend reports Alt during a drag.
void motion_with(Live& t, std::int64_t x, std::int64_t y, std::int64_t space, std::int64_t mods) {
    t.hand_moment(input::PointerMoved{x, y, 0, 0, space, mods});
}

/// The primary button let go at a raw position.
void release_at(Live& t, std::int64_t x, std::int64_t y, std::int64_t space) {
    t.hand_moment(input::PointerButton{1, false, x, y, space, input::mod::kNone});
}

/// THE LINES A HELD SNAP MARKED ON THE LATEST PICTURE, in the room: accent rectangles one device
/// unit across, running the room's whole height (a vertical edge met) or width (a horizontal
/// one).
struct SnapMarks {
    std::vector<std::int64_t> xs;
    std::vector<std::int64_t> ys;
};
SnapMarks snap_marks(Live& t) {
    const Screen sc = screen_of(t.session());
    const std::int64_t grain = chrome_grain(sc);
    SnapMarks out;
    REQUIRE_FALSE((t.pictures == 0));
    for (const ScreenPlane& layer : paint(t.session()).layers) {
        for (const ScreenRect& r : layer.rects) {
            if (r.role != gfx::role::kAccent) {
                continue;
            }
            if (r.w == grain && r.y == sc.room_y && r.h == sc.room_h) {
                out.xs.push_back(r.x);
            }
            if (r.h == grain && r.x == 0 && r.w == sc.room_w) {
                out.ys.push_back(r.y - sc.room_y);
            }
        }
    }
    return out;
}

/// A FINE RIG WITH TWO PANES OF KNOWN SIZE, IN THE OPEN: the stand-in at 100,100, 300 by 120, and
/// the second at 600,200, 200 wide -- so every edge but the room's own stands in open room.
struct TwoPaneRig : FineRig {
    explicit TwoPaneRig(const std::string& prefs = std::string()) : FineRig(prefs) {
        open_pane(*this, ref_of(second::kKind));
        stage_desk(*this, [&](Setup& s) {
            REQUIRE(author_pane_place(s, ref_of(stock::kKind), 100, 100).accepted);
            REQUIRE(author_pane_size(s, ref_of(stock::kKind), PaneSize{pane_unit::kPixels, 300},
                                     PaneSize{pane_unit::kPixels, 120})
                        .accepted);
            REQUIRE(author_pane_place(s, ref_of(second::kKind), 600, 200).accepted);
            REQUIRE(author_pane_size(s, ref_of(second::kKind), PaneSize{pane_unit::kPixels, 200},
                                     PaneSize{pane_unit::kDefault, 0})
                        .accepted);
        });
        key(input::scan::kUnknown); // a delivery, so the picture is the desk as authored
        select_pane(*this, ref_of(stock::kKind));
    }
};

} // namespace

TEST_CASE("a hand pulling a share's edge writes the share whose edge is the hand's pixel, at every pixel of the widest room") {
    // ⚔ MUTATION: `author_pane_window` writing the hand's extent as `pixels` whatever the word --
    // the width comes back a count of pixels, and the share is gone.
    FineRig t;
    stage_desk(t, [](Setup& s) {
        REQUIRE(author_pane_place(s, ref_of(stock::kKind), 100, 100).accepted);
        REQUIRE(author_pane_size(s, ref_of(stock::kKind), PaneSize{ui::kSizeShare, 2500},
                                 PaneSize{pane_unit::kPixels, 120})
                    .accepted);
    });
    t.key(input::scan::kUnknown);
    const Screen sc = screen_of(t.session());
    const PixelRect at = t.builder_rect();
    const PixelRect right = pane_edge_cell(at, pane_edge::kRight);
    t.press_at(right.x + 2, right.y + 2, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    // ALT HELD, so no edge snaps: the edge goes where the hand leaves it.
    motion_with(t, right.x + 2 + 37, right.y + 2, input::space::kPixels, input::mod::kAlt);
    REQUIRE(t.builder_row()->width.mode == ui::kSizeShare);
    const PixelRect pulled = t.builder_rect();
    CHECK(pulled.x == at.x);
    CHECK(pulled.x + pulled.w == at.x + at.w + 37);
    CHECK(t.builder_row()->width.amount ==
          share_reaching(at.w + 37, sc.room_w, ui::Anchor::Start));
    release_at(t, right.x + 2 + 37, right.y + 2, input::space::kPixels);
    CHECK(t.builder_row()->width.mode == ui::kSizeShare);

    // EVERY PIXEL OF THE WIDEST ROOM a window lays out, from the room's left and from its right:
    // the door's share solves back to the hand's pixel, in the solver the desk is drawn with.
    const std::int64_t span = kScreenMaxW;
    std::int64_t missed = 0;
    for (const ui::Anchor anchor : {ui::Anchor::Start, ui::Anchor::End}) {
        Setup desk = setup_of("Reach", {stock::kKind});
        desk.panes[0].place = anchor == ui::Anchor::Start ? PanePlace{pane_unit::kPixels, 0, 0}
                                                          : PanePlace{ui::kPlaceTopRight, 0, 0};
        desk.panes[0].width = PaneSize{ui::kSizeShare, 5000};
        for (std::int64_t length = kPanePxMin; length <= span; ++length) {
            PaneAxisProposal across;
            across.base = anchor == ui::Anchor::Start ? 0 : span - span / 2;
            across.length = span / 2;
            across.span = span;
            across.extent = PaneSize{pane_unit::kPixels, length};
            if (anchor == ui::Anchor::End) {
                across.position = span - length; // a left pull: the right edge holds
            }
            REQUIRE(author_pane_window(desk, ref_of(stock::kKind), across, PaneAxisProposal{})
                        .written.accepted);
            ui::Element e;
            e.place = ui_place_of(desk.panes[0].place);
            e.width = ui_size_of(desk.panes[0].width);
            e.height = ui::Size{pane_unit::kPixels, 100};
            const ui::Rect r = ui::arrange({e}, ui::Room{span, 1000, 1}).items.at(0).rect;
            if (r.w != length || (anchor == ui::Anchor::End && r.x + r.w != span)) {
                ++missed;
            }
            desk.panes[0].width = PaneSize{ui::kSizeShare, 5000};
        }
    }
    CHECK(missed == 0);
}

TEST_CASE("a hand moving a share place writes the share whose edge is the hand's pixel, at every pixel of the widest room") {
    // ⚔ MUTATION: `author_pane_window` reading a share place as an anchor -- the place is
    // written as a count of pixels in a word that reads ten-thousandths.
    FineRig t;
    stage_desk(t, [](Setup& s) {
        SetupPane* row = pane_of(s, ref_of(stock::kKind));
        row->place = PanePlace{ui::kPlaceShare, 1000, 2000};
        row->width = PaneSize{pane_unit::kPixels, 300};
        row->height = PaneSize{pane_unit::kPixels, 120};
    });
    t.key(input::scan::kUnknown);
    const Screen sc = screen_of(t.session());
    const PixelRect at = t.builder_rect();
    t.press_at(at.x + 30, at.y + 20, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);
    // ALT HELD, so no edge snaps: the pane goes where the hand leaves it, and keeps its word.
    motion_with(t, at.x + 30 + 37, at.y + 20 + 23, input::space::kPixels, input::mod::kAlt);
    const SetupPane* row = t.builder_row();
    REQUIRE(row->place.mode == ui::kPlaceShare);
    CHECK(row->place.x == share_placing(at.x + 37, sc.room_w));
    CHECK(row->place.y == share_placing(at.y - sc.room_y + 23, sc.room_h));
    CHECK(t.builder_rect().x == at.x + 37);
    CHECK(t.builder_rect().y == at.y + 23);
    CHECK(t.notice().find("place now") == std::string::npos); // its word kept, nothing to say
    release_at(t, at.x + 30 + 37, at.y + 20 + 23, input::space::kPixels);

    // EVERY PIXEL OF THE WIDEST ROOM a window lays out, across and down: the door's share solves
    // back to the hand's pixel, in the solver the desk is drawn with.
    const std::int64_t span = kScreenMaxW;
    std::int64_t missed = 0;
    for (const bool across : {true, false}) {
        Setup desk = setup_of("Reach", {stock::kKind});
        desk.panes[0].place = PanePlace{ui::kPlaceShare, 5000, 5000};
        desk.panes[0].width = PaneSize{pane_unit::kPixels, 100};
        desk.panes[0].height = PaneSize{pane_unit::kPixels, 100};
        for (std::int64_t pixel = 0; pixel < span; ++pixel) {
            PaneAxisProposal moved;
            moved.base = span / 2;
            moved.length = 100;
            moved.span = span;
            moved.position = pixel;
            const PaneAxisProposal still{std::nullopt, std::nullopt, span / 2, 100, span, 1};
            REQUIRE(author_pane_window(desk, ref_of(stock::kKind), across ? moved : still,
                                       across ? still : moved)
                        .written.accepted);
            ui::Element e;
            e.place = ui_place_of(desk.panes[0].place);
            e.width = ui_size_of(desk.panes[0].width);
            e.height = ui_size_of(desk.panes[0].height);
            const ui::Rect r = ui::arrange({e}, ui::Room{span, span, 1}).items.at(0).rect;
            if ((across ? r.x : r.y) != pixel || desk.panes[0].place.mode != ui::kPlaceShare) {
                ++missed;
            }
            desk.panes[0].place = PanePlace{ui::kPlaceShare, 5000, 5000};
        }
    }
    CHECK(missed == 0);
}

TEST_CASE("a fill pulled at its far edge, with nothing sharing its frame, writes the share whose edge is the hand's pixel, and says so") {
    // ⚔ MUTATION: a fill pulled at its far edge written as pixels -- the width stops following the
    // room, and nothing is said.
    FineRig t;
    stage_desk(t, [](Setup& s) {
        REQUIRE(author_pane_place(s, ref_of(stock::kKind), 100, 100).accepted);
        REQUIRE(author_pane_size(s, ref_of(stock::kKind), PaneSize{ui::kSizeFill, 0},
                                 PaneSize{pane_unit::kPixels, 120})
                    .accepted);
    });
    t.key(input::scan::kUnknown);
    const Screen sc = screen_of(t.session());
    const PixelRect at = t.builder_rect();
    REQUIRE(at.x + at.w == sc.room_w); // a fill runs to the room's far edge
    const PixelRect right = pane_edge_cell(at, pane_edge::kRight);
    t.press_at(right.x + 2, right.y + 2, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    motion_with(t, right.x + 2 - 300, right.y + 2, input::space::kPixels, input::mod::kAlt);
    const SetupPane* row = t.builder_row();
    REQUIRE(row->width.mode == ui::kSizeShare);
    const PixelRect pulled = t.builder_rect();
    CHECK(pulled.x == 100);
    CHECK(pulled.x + pulled.w == sc.room_w - 300);
    CHECK(t.notice().find("width now " + share_text(row->width.amount) + " -- was fill") !=
          std::string::npos);
    release_at(t, right.x + 2 - 300, right.y + 2, input::space::kPixels);

    // ITS NEAR EDGE PULLED LEAVES A FILL A FILL: the far edge is still the room's.
    stage_desk(t, [](Setup& s) {
        REQUIRE(author_pane_size(s, ref_of(stock::kKind), PaneSize{ui::kSizeFill, 0},
                                 PaneSize{pane_unit::kPixels, 120})
                    .accepted);
    });
    t.key(input::scan::kUnknown);
    const PixelRect again = t.builder_rect();
    const PixelRect left = pane_edge_cell(again, pane_edge::kLeft);
    t.press_at(left.x + 2, left.y + 2, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    motion_with(t, left.x + 2 + 40, left.y + 2, input::space::kPixels, input::mod::kAlt);
    CHECK(t.builder_row()->width.mode == ui::kSizeFill);
    CHECK(t.builder_row()->place.x == 140);
    CHECK(t.builder_rect().x + t.builder_rect().w == sc.room_w);
    release_at(t, left.x + 2 + 40, left.y + 2, input::space::kPixels);
}

TEST_CASE("an anchored pane pulled past its own edge is refused across and still follows the hand down") {
    // ⚔ MUTATION: the size of an axis whose offset was refused written anyway -- the width grows
    // though its edge could not go where the hand took it.
    FineRig t;
    stage_desk(t, [](Setup& s) {
        SetupPane* row = pane_of(s, ref_of(stock::kKind));
        row->place = PanePlace{ui::kPlaceBottomRight, 20, 60};
        row->width = PaneSize{ui::kSizeShare, 2500};
        row->height = PaneSize{pane_unit::kPixels, 120};
    });
    t.key(input::scan::kUnknown);
    const PixelRect at = t.builder_rect();
    const PixelRect corner = pane_edge_cell(at, pane_edge::kBottomRight);
    t.press_at(corner.x + 1, corner.y + 1, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    // THE CORNER FORTY PIXELS PAST THE RIGHT WALL, AND TEN DOWN: the offset from the right would
    // be -20, so the width is refused whole; the height takes the ten.
    motion_with(t, corner.x + 1 + 40, corner.y + 1 + 10, input::space::kPixels, input::mod::kAlt);
    CHECK(t.builder_row()->place == PanePlace{ui::kPlaceBottomRight, 20, 50});
    CHECK(t.builder_row()->width == PaneSize{ui::kSizeShare, 2500});
    CHECK(t.builder_row()->height == PaneSize{pane_unit::kPixels, 130});
    release_at(t, corner.x + 1 + 40, corner.y + 1 + 10, input::space::kPixels);
}

TEST_CASE("a hand moves an anchored pane in its word, a centred one into pixels, and pulls a fitted size into pixels, each said") {
    // ⚔ MUTATION: an anchored place written as pixels from the room's top-left -- the word is
    // gone, and the pane no longer keeps its corner when the window changes.
    FineRig t;
    stage_desk(t, [](Setup& s) {
        SetupPane* row = pane_of(s, ref_of(stock::kKind));
        row->place = PanePlace{ui::kPlaceBottomRight, 50, 40};
        row->width = PaneSize{pane_unit::kPixels, 300};
        row->height = PaneSize{pane_unit::kPixels, 120};
    });
    t.key(input::scan::kUnknown);
    const PixelRect at = t.builder_rect();
    t.press_at(at.x + 30, at.y + 20, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);
    motion_with(t, at.x + 30 - 70, at.y + 20 - 25, input::space::kPixels, input::mod::kAlt);
    CHECK(t.builder_row()->place == PanePlace{ui::kPlaceBottomRight, 120, 65});
    release_at(t, at.x + 30 - 70, at.y + 20 - 25, input::space::kPixels);

    // A CENTRED PLACE CANNOT SAY WHERE THE HAND LEFT IT: it becomes pixels, and is said.
    stage_desk(t, [](Setup& s) {
        pane_of(s, ref_of(stock::kKind))->place = PanePlace{ui::kPlaceCenter, 0, 0};
    });
    t.key(input::scan::kUnknown);
    const PixelRect centred = t.builder_rect();
    t.press_at(centred.x + 30, centred.y + 20, input::space::kPixels);
    motion_with(t, centred.x + 30 + 11, centred.y + 20, input::space::kPixels, input::mod::kAlt);
    const Screen sc = screen_of(t.session());
    CHECK(t.builder_row()->place ==
          PanePlace{pane_unit::kPixels, centred.x + 11, centred.y - sc.room_y});
    CHECK(t.notice().find("place now pixels -- was center") != std::string::npos);
    release_at(t, centred.x + 30 + 11, centred.y + 20, input::space::kPixels);

    // A FITTED HEIGHT PULLED becomes pixels at the hand's extent, said.
    stage_desk(t, [](Setup& s) {
        pane_of(s, ref_of(stock::kKind))->height = PaneSize{ui::kSizeFit, 0};
    });
    t.key(input::scan::kUnknown);
    const PixelRect fitted = t.builder_rect();
    const PixelRect bottom = pane_edge_cell(fitted, pane_edge::kBottom);
    t.press_at(bottom.x + 2, bottom.y + 2, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    motion_with(t, bottom.x + 2, bottom.y + 2 + 30, input::space::kPixels, input::mod::kAlt);
    CHECK(t.builder_row()->height == PaneSize{pane_unit::kPixels, fitted.h + 30});
    CHECK(t.notice().find("height now " + std::to_string(fitted.h + 30) + " px -- was fit") !=
          std::string::npos);
    release_at(t, bottom.x + 2, bottom.y + 2 + 30, input::space::kPixels);
}

TEST_CASE("a pane dragged near another pane's edge or the room's comes to it, and the edge met is marked across the room while held") {
    // ⚔ MUTATION: no snap -- the place stays where the hand left it, a few pixels short of the
    // edge, and nothing is marked.
    TwoPaneRig t;
    const PixelRect at = t.builder_rect();
    REQUIRE(at.x == 100);
    const std::int64_t press_x = at.x + 30;
    const std::int64_t press_y = at.y + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);

    // FIVE PIXELS SHORT OF THE ROOM'S LEFT EDGE, and it comes to it.
    t.motion_at(press_x - 95, press_y, input::space::kPixels);
    CHECK(t.builder_row()->place.x == 0);
    CHECK(t.builder_row()->place.y == 100);
    CHECK(snap_marks(t).xs == std::vector<std::int64_t>{0});
    CHECK(snap_marks(t).ys.empty());
    // ITS RIGHT EDGE FOUR PIXELS SHORT OF THE SECOND PANE'S LEFT, and it comes to that.
    t.motion_at(press_x + 196, press_y, input::space::kPixels); // the right edge at 596
    CHECK(t.builder_row()->place.x == 300);
    CHECK(snap_marks(t).xs == std::vector<std::int64_t>{600});
    // ITS BOTTOM EDGE SIX PIXELS PAST THE SECOND PANE'S TOP, on the other axis at once.
    t.motion_at(press_x + 196, press_y - 14, input::space::kPixels); // the bottom at 206
    CHECK(t.builder_row()->place.x == 300);
    CHECK(t.builder_row()->place.y == 80);
    CHECK(snap_marks(t).ys == std::vector<std::int64_t>{200});
    // IN THE OPEN, with no line within reach: where the hand left it, and nothing marked.
    t.motion_at(press_x + 50, press_y, input::space::kPixels);
    CHECK(t.builder_row()->place.x == 150);
    CHECK(t.builder_row()->place.y == 100);
    CHECK(snap_marks(t).xs.empty());
    CHECK(snap_marks(t).ys.empty());
    // LET GO SNAPPED: the mark goes with the hand, and the layout holds a place in whole pixels,
    // the size untouched.
    t.motion_at(press_x - 95, press_y, input::space::kPixels);
    REQUIRE(snap_marks(t).xs == std::vector<std::int64_t>{0});
    release_at(t, press_x - 95, press_y, input::space::kPixels);
    CHECK_FALSE(t.session().pane_drag.active);
    CHECK(snap_marks(t).xs.empty());
    CHECK(t.builder_row()->place == (PanePlace{pane_unit::kPixels, 0, 100}));
    CHECK(t.builder_row()->width == (PaneSize{pane_unit::kPixels, 300}));
}

TEST_CASE("Alt held while dragging sets every snap aside, and the hand places to the pixel") {
    // ⚔ MUTATION: the modifier ignored -- the pane comes to the room's edge with Alt held.
    TwoPaneRig t;
    const PixelRect at = t.builder_rect();
    const std::int64_t press_x = at.x + 30;
    const std::int64_t press_y = at.y + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    motion_with(t, press_x - 95, press_y, input::space::kPixels, input::mod::kAlt);
    CHECK(t.builder_row()->place.x == 5);
    CHECK(snap_marks(t).xs.empty());
    // ...AND THE SAME POSITION WITHOUT IT SNAPS: each motion asks afresh.
    t.motion_at(press_x - 95, press_y, input::space::kPixels);
    CHECK(t.builder_row()->place.x == 0);
    motion_with(t, press_x - 94, press_y, input::space::kPixels, input::mod::kAlt);
    CHECK(t.builder_row()->place.x == 6);
    CHECK(snap_marks(t).xs.empty());
    // ...AND A RIGHT EDGE PULLED NEAR THE SECOND PANE WITH ALT HELD STOPS WHERE THE HAND DOES.
    release_at(t, press_x - 94, press_y, input::space::kPixels);
    const PixelRect now = t.builder_rect();
    const PixelRect mark = pane_edge_cell(now, pane_edge::kRight);
    t.press_at(mark.x + 2, mark.y + 2, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    motion_with(t, mark.x + 2 + (597 - (now.x + now.w)), mark.y + 2, input::space::kPixels,
                input::mod::kAlt);
    CHECK(t.builder_row()->width.amount == 597 - now.x);
    CHECK(snap_marks(t).xs.empty());
    release_at(t, 0, 0, input::space::kPixels);
}

namespace {

/// A SETTINGS FILE KEEPING WORKSHOP'S OWN NUMBER SETTINGS, written where a case's Workshop reads it.
std::string measures_file(TempDir& dir, const std::string& name,
                          const std::vector<std::pair<std::string, std::int64_t>>& numbers) {
    prefs_persist::Prefs kept;
    for (const auto& [key, n] : numbers) {
        REQUIRE(prefs_persist::put_workshop_value(
            kept, key, PaneSetting{key, std::nullopt, n, std::nullopt}));
    }
    const std::string path = dir.file(name.c_str());
    REQUIRE(prefs_persist::save_file(path, kept).accepted);
    return path;
}

/// WHERE THE STAND-IN'S LEFT EDGE COMES TO when a hand drags it to `left` pixels from the room's
/// left edge and lets go.
std::int64_t left_edge_dragged_to(TwoPaneRig& t, std::int64_t left) {
    const PixelRect at = t.builder_rect();
    const std::int64_t press_x = at.x + 30;
    const std::int64_t press_y = at.y + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    t.motion_at(press_x - (at.x - left), press_y, input::space::kPixels);
    const std::int64_t came_to = t.builder_row()->place.x;
    release_at(t, press_x - (at.x - left), press_y, input::space::kPixels);
    return came_to;
}

} // namespace

TEST_CASE("the snap reaches as far as `workshop.snap-reach` says: eleven pixels meets an edge ten away, six misses one seven away") {
    // ⚔ MUTATION: the reach left at `kPaneSnapReachPx` -- the far hand stops ten pixels short and
    // the near one is still pulled seven.
    TempDir dir("measures-snap");
    TwoPaneRig standard;
    REQUIRE(standard.session().snap_reach_px == kPaneSnapReachPx);
    CHECK(left_edge_dragged_to(standard, 10) == 10);
    CHECK(left_edge_dragged_to(standard, 7) == 0);

    TwoPaneRig wide_reach(measures_file(dir, "far.json", {{"workshop.snap-reach", 11}}));
    REQUIRE(wide_reach.session().snap_reach_px == 11);
    CHECK(left_edge_dragged_to(wide_reach, 12) == 12);
    CHECK(left_edge_dragged_to(wide_reach, 10) == 0);

    TwoPaneRig short_reach(measures_file(dir, "near.json", {{"workshop.snap-reach", 6}}));
    REQUIRE(short_reach.session().snap_reach_px == 6);
    CHECK(left_edge_dragged_to(short_reach, 7) == 7);
    CHECK(left_edge_dragged_to(short_reach, 6) == 0);
}

TEST_CASE("the coarse step is `workshop.arrange-step` cells, read from Workshop's settings file") {
    // ⚔ MUTATION: the grow spends `kCoarseStepCells` -- four cells where the file says eight.
    TempDir dir("measures-step");
    FineRig t(measures_file(dir, "step.json", {{"workshop.arrange-step", 8}}));
    REQUIRE(t.session().arrange_step_cells == 8);
    const PixelRect before = bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                                       screen_of(t.session()))
                                 .resolved;
    t.key(input::scan::kEquals);
    CHECK(t.builder_row()->width.amount == before.w + cells_px(8));
    CHECK(t.builder_row()->height.amount == before.h + cells_px(8));
    t.key(input::scan::kMinus);
    CHECK(t.builder_row()->width.amount == before.w);
    CHECK(t.builder_row()->height.amount == before.h);
}

TEST_CASE("a stacked pane is `workshop.stack-rows` tall, read from Workshop's settings file, and a file still keeping `workshop.side-columns` reads") {
    // ⚔ MUTATION: `placement_bounds` spends `kStackRows` -- nine rows, whatever the file says.
    // The retired key is still in the file: it is carried and read by nothing, so a pane at the
    // room's top-right is the width its row says.
    TempDir dir("measures-room");
    Live t;
    t.host.prefs_path = measures_file(dir, "room.json",
                                      {{"workshop.side-columns", 40}, {"workshop.stack-rows", 20}});
    t.ready();
    t.extent_px(cells_px(160), cells_px(44), 0, 0, 0);
    REQUIRE(t.session().stack_rows == 20);
    const Screen sc = screen_of(t.session());
    CHECK(sc.stack_rows == 20);
    open_stock_pane(t);
    const PixelRect stacked =
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect;
    CHECK(stacked.h == cells_px(20));
    open_at_top_right(t, second::kKind);
    const PixelRect strip =
        bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc).rect;
    CHECK(strip.x == sc.w - cells_px(kStripCols));
    CHECK(strip.w == cells_px(kStripCols));

    // THE SCREEN HOLDS THE SETTING TO WHAT IT TAKES, and its own furniture is the default's.
    const Screen held = screen_of(kScreenMinW, kScreenMinH, 0, 0, 0, 99);
    CHECK(held.stack_rows == kMaxStackRows);
    CHECK(kMinScreen.stack_rows == kStackRows);
}

TEST_CASE("an edge pulled near another pane's edge or the room's comes to it, its opposite edge held") {
    // ⚔ MUTATION: no snap -- the width stops where the hand did, three pixels short.
    TwoPaneRig t;
    const PixelRect at = t.builder_rect();
    // THE RIGHT EDGE, pulled to three pixels short of the second pane's left edge.
    const PixelRect right = pane_edge_cell(at, pane_edge::kRight);
    t.press_at(right.x + 2, right.y + 2, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    t.motion_at(right.x + 2 + 197, right.y + 2, input::space::kPixels); // the right edge at 597
    CHECK(t.builder_row()->place.x == 100);
    CHECK(t.builder_row()->width.amount == 500);
    CHECK(snap_marks(t).xs == std::vector<std::int64_t>{600});
    release_at(t, 0, 0, input::space::kPixels);
    CHECK(snap_marks(t).xs.empty());
    // THE TOP EDGE, pulled to six pixels below the room's top: it comes to it, the bottom held.
    const PixelRect now = t.builder_rect();
    const PixelRect top = pane_edge_cell(now, pane_edge::kTop);
    t.press_at(top.x + 2, top.y + 2, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    t.motion_at(top.x + 2, top.y + 2 - 94, input::space::kPixels); // the top at 6
    CHECK(t.builder_row()->place.y == 0);
    CHECK(t.builder_row()->height.amount == 220);
    CHECK(snap_marks(t).ys == std::vector<std::int64_t>{0});
    release_at(t, 0, 0, input::space::kPixels);
}

TEST_CASE("the arrow keys and a typed value place exactly, near an edge or not") {
    // ⚔ MUTATION: the keys or a typed value snapped -- eight would come to the room's edge at
    // zero, five likewise.
    Live t;
    t.extent_on_window(160, 44);
    open_pane(t, ref_of(stock::kKind));
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), 20, 100).accepted);
    });
    enter_arrange_desk(t);
    select_pane(t, ref_of(stock::kKind));
    t.key(input::scan::kLeft); // one cell: 20 - 12
    CHECK(pane_of(t.session().setup.active, ref_of(stock::kKind))->place.x == 8);
    t.key(input::scan::kEscape);
    REQUIRE(hand_inspect(t, ref_of(stock::kKind)).accepted);
    REQUIRE(hand_commit(t, "X", "5").accepted);
    CHECK(pane_of(t.session().setup.active, ref_of(stock::kKind))->place.x == 5);
}

TEST_CASE("in a terminal a hand moving by cells meets an edge between them, and the mark is a cell across the room") {
    // ⚔ MUTATION: no snap -- a pane 500 pixels wide moved by whole cells never meets the room's
    // right edge, its own right edge four pixels short of it.
    Live t;
    t.extent_px(cells_px(120), cells_px(40), 0, 0);
    open_pane(t, ref_of(stock::kKind));
    stage_desk(t, [&](Setup& s) {
        REQUIRE(author_pane_place(s, ref_of(stock::kKind), cells_px(10), cells_px(10)).accepted);
        REQUIRE(author_pane_size(s, ref_of(stock::kKind), PaneSize{pane_unit::kPixels, 500},
                                 PaneSize{pane_unit::kDefault, 0})
                    .accepted);
    });
    enter_arrange_desk(t);
    select_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    REQUIRE(sc.room_w == cells_px(120));
    // TWO CELLS INTO THE PANE ON EACH AXIS, as the terminal reports that cell.
    const std::int64_t cell_x = t.term_x(12);
    const std::int64_t cell_y = t.term_y(12);
    t.press_at(cell_x, cell_y, input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);
    // SIXTY-EIGHT CELLS RIGHT: by the lattice the place is 936 and the right edge 1436, four
    // pixels short of the room's right edge at 1440, so it comes to it.
    t.motion_at(cell_x + 68, cell_y, input::space::kCells);
    CHECK(pane_of(t.session().setup.active, ref_of(stock::kKind))->place.x == 940);
    // THE MARK IS ONE CELL ACROSS AND STANDS INSIDE THE ROOM, on the edge's last cell.
    CHECK(snap_marks(t).xs == std::vector<std::int64_t>{sc.room_w - gfx::kCellPx});
    // ...AND WITH ALT HELD, WHICH A TERMINAL REPORTS TOO, THE LATTICE STANDS.
    motion_with(t, cell_x + 68, cell_y, input::space::kCells, input::mod::kAlt);
    CHECK(pane_of(t.session().setup.active, ref_of(stock::kKind))->place.x == 936);
    CHECK(snap_marks(t).xs.empty());
    release_at(t, cell_x + 68, cell_y, input::space::kCells);
}

TEST_CASE("a pane dragged out of the stack meets the desk as its move leaves it, the pane below risen") {
    // ⚔ MUTATION: the lines measured with the held pane where it stands. The move takes it out of
    // the stack and the pane below rises into its slot, so a snap to that pane's edge as it stood
    // meets a line nothing stands on once the write lands -- and one motion keeps that place.
    const auto stacked = [](FineRig& t, PixelRect& first, PixelRect& below) {
        open_pane(t, ref_of(second::kKind));
        select_pane(t, ref_of(stock::kKind));
        const Screen sc = screen_of(t.session());
        first = t.builder_rect();
        below = bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc).rect;
        REQUIRE(below.x == first.x);
        REQUIRE(below.y > first.y + first.h); // the second stacked under the first
    };
    SUBCASE("an edge the pane below leaves is no line") {
        FineRig t;
        PixelRect first, below;
        stacked(t, first, below);
        const std::int64_t press_x = first.x + 30;
        const std::int64_t press_y = first.y + 20;
        t.press_at(press_x, press_y, input::space::kPixels);
        // ONE MOTION, the top coming four pixels past where the second's top stood.
        const std::int64_t dy = below.y + 4 - first.y;
        t.motion_at(press_x, press_y + dy, input::space::kPixels);
        // The second rose to the column's top, and the hand's place stands: nothing is there.
        CHECK(bounds_of(t.session().panes, t.session().setup.active, second::kKind,
                        screen_of(t.session()))
                  .rect.y == first.y);
        CHECK(t.builder_row()->place.y == below.y + 4 - t.room_y());
        CHECK(snap_marks(t).ys.empty());
        release_at(t, press_x, press_y + dy, input::space::kPixels);
    }
    SUBCASE("an edge the pane below rises to is a line") {
        FineRig t;
        PixelRect first, below;
        stacked(t, first, below);
        const std::int64_t press_x = first.x + 30;
        const std::int64_t press_y = first.y + 20;
        t.press_at(press_x, press_y, input::space::kPixels);
        // ONE MOTION, the top coming four pixels under where the risen second's bottom will be.
        const std::int64_t risen_bottom = first.y + below.h;
        const std::int64_t dy = risen_bottom + 4 - first.y;
        t.motion_at(press_x, press_y + dy, input::space::kPixels);
        CHECK(t.builder_row()->place.y == risen_bottom - t.room_y());
        CHECK(snap_marks(t).ys == std::vector<std::int64_t>{risen_bottom - t.room_y()});
        release_at(t, press_x, press_y + dy, input::space::kPixels);
    }
}

TEST_CASE("a one-pixel drag moves a pane by exactly one pixel of lattice") {
    FineRig t;
    // IN THE OPEN, away from every line a snap could meet, so the hand alone moves it.
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), cells_px(6),
                                  cells_px(6))
                    .accepted);
    });
    const PixelRect at = t.builder_rect();
    // PRESS THE BODY, midway in, in window pixels: a place is said in those same pixels, so
    // the grab offset is exact too.
    const std::int64_t press_x = at.x + 30;
    const std::int64_t press_y = at.y + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);

    // ONE PIXEL RIGHT: the place moves by exactly one pixel -- no whole-cell threshold anywhere
    // on the path, which would take twelve pixels of hand before anything moved.
    t.motion_at(press_x + 1, press_y, input::space::kPixels);
    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    REQUIRE(row->place.mode == pane_unit::kPixels);
    const std::int64_t base_x = at.x;
    CHECK(row->place.x == base_x + gfx::kPixelGrainPx);
    CHECK(row->place.y == at.y - t.room_y()); // the same row, said in the room

    // TWELVE ONE-PIXEL STEPS LAND EXACTLY ONE CELL OVER — stability under repeated small
    // deltas: every motion proposes from the press's base, so the sum is the distance the
    // hand travelled and nothing accumulates or drifts through a cell round-trip.
    for (std::int64_t i = 2; i <= 12; ++i) {
        t.motion_at(press_x + i, press_y, input::space::kPixels);
    }
    CHECK(t.builder_row()->place.x == base_x + cells_px(1));
    CHECK(t.builder_row()->place.y == at.y - t.room_y());
    // ...and the SAME hand position always means the same place: jitter back and forth
    // and the pane is wherever the pointer last was, not somewhere error piled up.
    t.motion_at(press_x + 5, press_y, input::space::kPixels);
    t.motion_at(press_x + 12, press_y, input::space::kPixels);
    CHECK(t.builder_row()->place.x == base_x + cells_px(1));
    t.release(0, 0);
}

TEST_CASE("every edge resizes pixel-fine and preserves its opposite anchor") {
    // THE INVARIANT FOR ALL EIGHT HANDLES, driven through the REAL pointer path in window
    // pixels: the pulled edge follows the hand by one pixel of lattice, the opposite
    // edge's position is untouched, and the place is written exactly when a left or top
    // edge moved it.
    struct Pull {
        std::int64_t edge;
        std::int64_t dx; // one pixel outward on the axes the edge names
        std::int64_t dy;
    };
    const std::vector<Pull> pulls = {
        {pane_edge::kLeft, -1, 0},        {pane_edge::kRight, +1, 0},
        {pane_edge::kTop, 0, -1},         {pane_edge::kBottom, 0, +1},
        {pane_edge::kTopLeft, -1, -1},    {pane_edge::kTopRight, +1, -1},
        {pane_edge::kBottomLeft, -1, +1}, {pane_edge::kBottomRight, +1, +1},
    };
    for (const Pull& pull : pulls) {
        CAPTURE(std::string(pane_edge_name(pull.edge)));
        FineRig t;
        // AWAY FROM EVERY WALL, so no pull below meets a refusal.
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(author_pane_place(desk, ref_of(stock::kKind), cells_px(6),
                                      cells_px(6))
                        .accepted);
        });
        const PixelRect base = t.builder_rect();
        const std::int64_t left = base.x;
        const std::int64_t top = base.y;
        const std::int64_t right = gfx::add_cells(base.x, base.w);
        const std::int64_t bottom = gfx::add_cells(base.y, base.h);

        // PRESS THE EDGE'S OWN MARK, at the pixel its painted band begins on.
        const PixelRect mark = pane_edge_cell(base, pull.edge);
        const std::int64_t press_x = mark.x + 2;
        const std::int64_t press_y = mark.y + 2;
        t.press_at(press_x, press_y, input::space::kPixels);
        REQUIRE(t.session().pane_drag.active);
        REQUIRE(t.session().pane_drag.sizing);
        REQUIRE(t.session().pane_drag.edge == pull.edge);

        t.motion_at(press_x + pull.dx, press_y + pull.dy, input::space::kPixels);
        const SetupPane* row = t.builder_row();
        REQUIRE(row != nullptr);
        const bool wide = pull.dx != 0;
        const bool tall = pull.dy != 0;
        const bool leftwards = pull.edge == pane_edge::kLeft ||
                               pull.edge == pane_edge::kTopLeft ||
                               pull.edge == pane_edge::kBottomLeft;
        const bool upwards = pull.edge == pane_edge::kTop ||
                             pull.edge == pane_edge::kTopLeft ||
                             pull.edge == pane_edge::kTopRight;
        const std::int64_t g = gfx::kPixelGrainPx;
        const std::int64_t now_x =
            row->place.mode == pane_unit::kPixels ? row->place.x : base.x;
        const std::int64_t now_y =
            row->place.mode == pane_unit::kPixels ? row->place.y + t.room_y() : base.y;
        const std::int64_t now_w =
            row->width.mode == pane_unit::kPixels ? row->width.amount : base.w;
        const std::int64_t now_h =
            row->height.mode == pane_unit::kPixels ? row->height.amount : base.h;
        if (wide) {
            CHECK(now_w == base.w + g);
        }
        if (tall) {
            CHECK(now_h == base.h + g);
        }
        // THE ANCHOR LAW: whichever edge was pulled, the opposite one did not move.
        if (leftwards) {
            CHECK(now_x == left - g);
            CHECK(now_x + now_w == right); // the RIGHT edge is the anchor
        } else if (wide) {
            CHECK(now_x == left); // the place IS the anchor
            CHECK(now_x + now_w == right + g);
        }
        if (upwards) {
            CHECK(now_y == top - g);
            CHECK(now_y + now_h == bottom); // the BOTTOM edge is the anchor
        } else if (tall) {
            CHECK(now_y == top);
            CHECK(now_y + now_h == bottom + g);
        }
        t.release(0, 0);
    }
}

TEST_CASE("the reported top-edge defect is dead -- the bottom edge holds still") {
    // THE MEASURED DEFECT THIS GUARDS: drag-resizing from the top edge changed the height by
    // the correct amount and failed to move `y`, so the BOTTOM edge moved instead of staying
    // anchored (height 9 -> 10 with y fixed at 20, bottom 29 -> 30). This is that scenario,
    // asserting the law that makes it unsayable.
    FineRig t;
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), 0, cells_px(20))
                    .accepted);
    });
    const PixelRect base = t.builder_rect();
    REQUIRE(base.y == cells_px(20) + t.room_y());
    REQUIRE(base.h == cells_px(9));
    const std::int64_t bottom_before = gfx::add_cells(base.y, base.h);

    // TAKE THE TOP EDGE'S MARK AND PULL UP ONE WHOLE CELL, in pixels.
    const PixelRect mark = pane_edge_cell(base, pane_edge::kTop);
    const std::int64_t press_x = mark.x + 2;
    const std::int64_t press_y = mark.y + 2;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kTop);
    t.motion_at(press_x, press_y - gfx::kCellPx, input::space::kPixels);

    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    // THE HEIGHT GREW BY THE PULL, `y` MOVED WITH IT, AND THE BOTTOM EDGE DID NOT MOVE.
    CHECK(row->height.mode == pane_unit::kPixels);
    CHECK(row->height.amount == base.h + cells_px(1));
    CHECK(row->place.y == cells_px(20) - cells_px(1));
    CHECK(t.room_y() + row->place.y + row->height.amount == bottom_before);
    t.release(0, 0);
}

TEST_CASE("a refused anchored resize writes neither the place nor the size") {
    // PULL THE TOP EDGE DOWN PAST THE MINIMUM: the proposed height is illegal, and the
    // proposal carries a moved `y` beside it in the SAME vertical axis. The axis is atomic:
    // `y` and the height settle together or not at all, so a moved top edge beside a refused
    // height is exactly what `author_pane_window` makes unsayable -- and with no other axis
    // proposed, the whole gesture is a refusal.
    FineRig t;
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), cells_px(4),
                                  cells_px(20))
                    .accepted);
    });
    const SetupPane before = *t.builder_row();
    const PixelRect base = t.builder_rect();
    const PixelRect mark = pane_edge_cell(base, pane_edge::kTop);
    const std::int64_t press_x = mark.x + 2;
    const std::int64_t press_y = mark.y + 2;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kTop);
    // Down by the whole height: h' would be zero, which is below the one-cell floor.
    t.motion_at(press_x, press_y + base.h, input::space::kPixels);
    INFO(t.session().notice);
    CHECK(t.session().notice_is_bad);
    CHECK(*t.builder_row() == before);

    // AND THE PLACE WALL HOLDS THE WHOLE AXIS TOO: pulling the LEFT edge past the
    // canvas's own origin proposes a negative x beside a legal width — one horizontal
    // transaction — and neither member lands.
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), cells_px(1),
                                  cells_px(20))
                    .accepted);
    });
    t.release(0, 0);
    const SetupPane at_wall = *t.builder_row();
    const PixelRect wall_base = t.builder_rect();
    const PixelRect left_mark = pane_edge_cell(wall_base, pane_edge::kLeft);
    const std::int64_t lx = left_mark.x + 2;
    const std::int64_t ly = left_mark.y + 2;
    t.press_at(lx, ly, input::space::kPixels);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kLeft);
    t.motion_at(lx - 2 * gfx::kCellPx, ly, input::space::kPixels);
    CHECK(t.session().notice_is_bad);
    CHECK(*t.builder_row() == at_wall);
    t.release(0, 0);
}

TEST_CASE("a right or bottom resize leaves a default place reactive") {
    // THOSE EDGES ANCHOR THE PLACE BY NOT WRITING IT: a weaver who widened a reactive pane
    // has said nothing about where it belongs, so it keeps following the developer's
    // tiling — and a top-edge pull on the same pane IS a placement decision (the bottom
    // edge's position becomes authored fact), so that one writes the place and takes the
    // pane out of the stack.
    FineRig t;
    const PixelRect base = t.builder_rect();
    const PixelRect right_mark = pane_edge_cell(base, pane_edge::kRight);
    const std::int64_t rx = right_mark.x + 2;
    const std::int64_t ry = right_mark.y + 2;
    t.press_at(rx, ry, input::space::kPixels);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kRight);
    t.motion_at(rx + 3, ry, input::space::kPixels);
    CHECK(t.builder_row()->width.mode == pane_unit::kPixels);
    CHECK(t.builder_row()->place.mode == pane_unit::kDefault);
    t.release(0, 0);

    const PixelRect grown = t.builder_rect();
    const PixelRect top_mark = pane_edge_cell(grown, pane_edge::kTop);
    const std::int64_t tx = top_mark.x + 2;
    const std::int64_t ty = top_mark.y + 2;
    t.press_at(tx, ty, input::space::kPixels);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kTop);
    // TEN PIXELS DOWN, beyond the reach of the room's top edge, which would take it back.
    t.motion_at(tx, ty + 10, input::space::kPixels);
    CHECK(t.builder_row()->place.mode == pane_unit::kPixels);
    CHECK(t.builder_row()->place.y == grown.y - t.room_y() + 10 * gfx::kPixelGrainPx);
    t.release(0, 0);
}

TEST_CASE("a move blocked at the left wall still follows the hand down") {
    // THE LIVE DEFECT THIS GUARDS: a drag whose proposal leaves the canvas on ONE axis refused
    // the WHOLE proposal, so a pane slid along the left wall froze on both axes. Independent
    // axes settle independently -- and the blocked coordinate KEEPS ITS OWN VALUE rather than
    // clamping to the wall, which staging the pane five pixels off the wall distinguishes
    // (a clamp would write 0 here).
    FineRig t;
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), 5, cells_px(20))
                    .accepted);
    });
    const PixelRect at = t.builder_rect();
    REQUIRE(at.x == 5);
    const std::int64_t press_x = at.x + 30;
    const std::int64_t press_y = at.y + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);

    // TWENTY PIXELS LEFT, past the wall and beyond the reach of its snap, AND SEVEN DOWN:
    // x' = 5 - 20 is refused, y' lands.
    t.motion_at(press_x - 20, press_y + 7, input::space::kPixels);
    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    REQUIRE(row->place.mode == pane_unit::kPixels);
    CHECK(row->place.x == 5);
    CHECK(row->place.y == cells_px(20) + 7 * gfx::kPixelGrainPx);
    // SOMETHING LANDED, so this is a status and not an alert: the visible stop at the wall
    // is the refusal's consequence, and the pane tracking the hand is the statement.
    CHECK_FALSE(t.session().notice_is_bad);
    t.release(0, 0);
}

TEST_CASE("a move blocked at the top wall still follows the hand sideways") {
    // THE MIRROR ORIENTATION: y refused, x lands.
    FineRig t;
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), cells_px(20), 5)
                    .accepted);
    });
    const PixelRect at = t.builder_rect();
    REQUIRE(at.y == t.room_y() + 5); // five pixels below the room's top, its wall
    const std::int64_t press_x = at.x + 30;
    const std::int64_t press_y = at.y + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);

    // TWENTY UP, past the wall and beyond the reach of its snap: y' is refused, x' lands.
    t.motion_at(press_x + 7, press_y - 20, input::space::kPixels);
    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    REQUIRE(row->place.mode == pane_unit::kPixels);
    CHECK(row->place.x == at.x + 7 * gfx::kPixelGrainPx);
    CHECK(row->place.y == 5);
    CHECK_FALSE(t.session().notice_is_bad);
    t.release(0, 0);
}

TEST_CASE("a move past two walls at once writes nothing") {
    // BOTH AXES REFUSED is the one case a move gesture is still refused WHOLE: authored
    // geometry is untouched, byte for byte, and the refusal is said as an alert.
    FineRig t;
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), 5, 5)
                    .accepted);
    });
    const SetupPane before = *t.builder_row();
    const PixelRect at = t.builder_rect();
    const std::int64_t press_x = at.x + 30;
    const std::int64_t press_y = at.y + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    t.motion_at(press_x - 20, press_y - 20, input::space::kPixels); // beyond both snaps' reach
    CHECK(*t.builder_row() == before);
    CHECK(t.session().notice_is_bad);
    t.release(0, 0);
}

TEST_CASE("a refused nudge does not author a reactive place") {
    // A KEY INTO THE WALL proposes a change on exactly one axis; the other axis, unchanged,
    // is NOT a proposal -- so nothing lands, the refusal is said, and in particular a
    // DEFAULT place is not converted to an authored one as a side effect of a step that
    // visibly did nothing. The reactive pane stays reactive.
    FineRig t;
    REQUIRE(t.builder_row()->place.mode == pane_unit::kDefault);
    const PixelRect at = t.builder_rect();
    REQUIRE(at.x == 0); // the first stack tile sits on the wall (`kRoomCellX`)
    t.key(input::scan::kLeft);
    CHECK(t.builder_row()->place.mode == pane_unit::kDefault);
    CHECK(t.builder_rect() == at);
    CHECK(t.session().notice_is_bad);

    // AND THE PANE IS NOT FROZEN: the very next step away from the wall lands, authoring
    // the place -- the blocked axis contributes the value it already stood at.
    t.key(input::scan::kDown);
    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == pane_unit::kPixels);
    CHECK(row->place.x == at.x);
    CHECK(row->place.y == at.y - t.room_y() + gfx::kCellPx);
    CHECK_FALSE(t.session().notice_is_bad);
}

TEST_CASE("a corner resize blocked on one axis still resizes the other") {
    // FOR ALL FOUR CORNERS: one axis meets a wall (the canvas origin, or the one-cell
    // minimum) and refuses ATOMICALLY -- its anchored position+extent pair holds together
    // -- while the other axis settles its own transaction, anchor law intact. Two corners
    // are blocked horizontally, two vertically, across both kinds of wall.
    const PaneRef builder = ref_of(stock::kKind);

    {
        CAPTURE("top-left: horizontal blocked at the canvas origin");
        FineRig t;
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(author_pane_place(desk, builder, 5, cells_px(20)).accepted);
        });
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(author_pane_size(desk, builder,
                                     PaneSize{pane_unit::kPixels, cells_px(9)},
                                     PaneSize{pane_unit::kPixels, cells_px(9)})
                        .accepted);
        });
        const PixelRect base = t.builder_rect();
        const PixelRect mark = pane_edge_cell(base, pane_edge::kTopLeft);
        const std::int64_t px = mark.x + 2;
        const std::int64_t py = mark.y + 2;
        t.press_at(px, py, input::space::kPixels);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kTopLeft);
        // TWENTY LEFT, beyond the wall's snap: x' = 5 - 20 is illegal, so x + width HOLD
        // TOGETHER. SEVEN DOWN: the top edge comes down legally, y + height settle together,
        // bottom edge anchored.
        t.motion_at(px - 20, py + 7, input::space::kPixels);
        const SetupPane* row = t.builder_row();
        REQUIRE(row != nullptr);
        CHECK(row->place.x == 5);
        CHECK(row->width.amount == cells_px(9));
        CHECK(row->place.y == cells_px(20) + 7 * gfx::kPixelGrainPx);
        CHECK(row->height.amount == cells_px(9) - 7 * gfx::kPixelGrainPx);
        CHECK(row->place.y + row->height.amount == cells_px(20) + cells_px(9));
        t.release(0, 0);
    }
    {
        CAPTURE("bottom-left: horizontal blocked at the canvas origin");
        FineRig t;
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(author_pane_place(desk, builder, 5, cells_px(20)).accepted);
        });
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(author_pane_size(desk, builder,
                                     PaneSize{pane_unit::kPixels, cells_px(9)},
                                     PaneSize{pane_unit::kPixels, cells_px(9)})
                        .accepted);
        });
        const PixelRect base = t.builder_rect();
        const PixelRect mark = pane_edge_cell(base, pane_edge::kBottomLeft);
        const std::int64_t px = mark.x + 2;
        const std::int64_t py = mark.y + 2;
        t.press_at(px, py, input::space::kPixels);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kBottomLeft);
        // TWENTY LEFT refused, beyond the wall's snap; SEVEN DOWN grows the height from the
        // bottom, top edge anchored by not writing the place at all.
        t.motion_at(px - 20, py + 7, input::space::kPixels);
        const SetupPane* row = t.builder_row();
        REQUIRE(row != nullptr);
        CHECK(row->place.x == 5);
        CHECK(row->place.y == cells_px(20));
        CHECK(row->width.amount == cells_px(9));
        CHECK(row->height.amount == cells_px(9) + 7 * gfx::kPixelGrainPx);
        t.release(0, 0);
    }
    {
        CAPTURE("top-right: vertical blocked at the one-cell minimum");
        FineRig t;
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(author_pane_place(desk, builder, cells_px(6), cells_px(20))
                        .accepted);
        });
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(author_pane_size(desk, builder,
                                     PaneSize{pane_unit::kPixels, cells_px(9)},
                                     PaneSize{pane_unit::kPixels, cells_px(9)})
                        .accepted);
        });
        const PixelRect base = t.builder_rect();
        const PixelRect mark = pane_edge_cell(base, pane_edge::kTopRight);
        const std::int64_t px = mark.x + 2;
        const std::int64_t py = mark.y + 2;
        t.press_at(px, py, input::space::kPixels);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kTopRight);
        // 102 DOWN: h' = 9 cells - 408 subs is below the floor, so y + height HOLD.
        // EIGHT RIGHT: the width grows legally on its own axis.
        t.motion_at(px + 8, py + 102, input::space::kPixels);
        const SetupPane* row = t.builder_row();
        REQUIRE(row != nullptr);
        CHECK(row->place.x == cells_px(6));
        CHECK(row->place.y == cells_px(20));
        CHECK(row->height.amount == cells_px(9));
        CHECK(row->width.amount == cells_px(9) + 8 * gfx::kPixelGrainPx);
        t.release(0, 0);
    }
    {
        CAPTURE("bottom-right: vertical blocked at the one-cell minimum");
        FineRig t;
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(author_pane_place(desk, builder, cells_px(6), cells_px(20))
                        .accepted);
        });
        stage_desk(t, [&](Setup& desk) {
            REQUIRE(author_pane_size(desk, builder,
                                     PaneSize{pane_unit::kPixels, cells_px(9)},
                                     PaneSize{pane_unit::kPixels, cells_px(9)})
                        .accepted);
        });
        const PixelRect base = t.builder_rect();
        const PixelRect mark = pane_edge_cell(base, pane_edge::kBottomRight);
        const std::int64_t px = mark.x + 2;
        const std::int64_t py = mark.y + 2;
        t.press_at(px, py, input::space::kPixels);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kBottomRight);
        // 102 UP: h' is below the floor, height holds. EIGHT LEFT: the width shrinks
        // legally, place untouched on both axes (trailing edges write no place).
        t.motion_at(px - 8, py - 102, input::space::kPixels);
        const SetupPane* row = t.builder_row();
        REQUIRE(row != nullptr);
        CHECK(row->place.x == cells_px(6));
        CHECK(row->place.y == cells_px(20));
        CHECK(row->height.amount == cells_px(9));
        CHECK(row->width.amount == cells_px(9) - 8 * gfx::kPixelGrainPx);
        t.release(0, 0);
    }
}

TEST_CASE("the hand meets exactly the pixels and cells a pane paints") {
    // THE ALIGNED-SPAN LAW, at an edge inside a cell: a pane whose left edge is pixel 75 is
    // painted from that pixel in a window and from the cell it floors to in a terminal, and in
    // each the FIRST painted unit answers the hand while the unit before it does not --
    // measured through the real routing path.
    FineRig t;
    // x = 6 cells + 3 pixels: the window paints from pixel 75, a terminal from cell 6.
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), cells_px(6) + 3,
                                  cells_px(6))
                    .accepted);
    });
    const PixelRect at = t.builder_rect();
    REQUIRE(at.x == cells_px(6) + 3);
    const std::int64_t left_px = at.x;
    CHECK(left_px == 75);
    const std::int64_t mid_y = at.y + 30;

    const Screen sc = screen_of(t.session());
    const Occupancy on = occupied_at(
        t.session().panes, t.session().setup.active, sc,
        canvas_point_of(input::space::kPixels, left_px, mid_y));
    CHECK(on.occupied);
    CHECK(on.kind == stock::kKind);
    const Occupancy off = occupied_at(
        t.session().panes, t.session().setup.active, sc,
        canvas_point_of(input::space::kPixels, left_px - 1, mid_y));
    CHECK_FALSE(off.occupied);

    // AND THE RIGHT EDGE, half-open exactly as painted: the last painted pixel answers,
    // the pixel past it does not.
    const std::int64_t right_px = (gfx::add_cells(at.x, at.w));
    const Occupancy inside = occupied_at(
        t.session().panes, t.session().setup.active, sc,
        canvas_point_of(input::space::kPixels, right_px - 1, mid_y));
    CHECK(inside.occupied);
    const Occupancy past = occupied_at(
        t.session().panes, t.session().setup.active, sc,
        canvas_point_of(input::space::kPixels, right_px, mid_y));
    CHECK_FALSE(past.occupied);

    // A TERMINAL'S HAND: cell 6 holds the pane's first painted cell and answers; cell 5 is
    // before it and does not.
    const std::int64_t mid_row = gfx::cell_of_pixel(mid_y);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc,
                      canvas_point_of(input::space::kCells, 6, mid_row))
              .kind == stock::kKind);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc,
                            canvas_point_of(input::space::kCells, 5, mid_row))
                    .occupied);
}

TEST_CASE("the TUI projects a pane onto its covered cells and rewrites nothing") {
    // THE CELL MEDIUM'S HONEST PICTURE of a pane placed inside cells: the cells its floored
    // edges span, deterministically — and projecting it changes not one authored byte,
    // however many frames are drawn.
    Live t; // a character medium: no text metric
    open_pane(t, ref_of(stock::kKind));
    REQUIRE(place_at_canvas(t, ref_of(stock::kKind), cells_px(6) + 6,
                              cells_px(6) + 10)
                .accepted);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_size(desk, ref_of(stock::kKind),
                                 PaneSize{pane_unit::kPixels, cells_px(20) + 8},
                                 PaneSize{pane_unit::kPixels, cells_px(5) + 5})
                    .accepted);
    });
    const std::string authored_before = setup_persist::to_text(t.session().setup.active);

    const Screen sc = screen_of(t.session());
    const PixelRect fine = bounds_of(t.session().panes, t.session().setup.active,
                                    stock::kKind, sc)
                              .rect;
    const CellRect covered = cells_covered(fine);
    // floor(78/12) = 6; floor((78 + 248)/12) = 27 -> 21 covered columns. floor(82/12) = 6;
    // floor((82 + 65)/12) = 12 -> 6 covered rows.
    CHECK(covered == CellRect{6, 6, 21, 6});

    // THE PAINTED BYTES SIT ON EXACTLY THOSE CELLS: the pane's backdrop begins at the
    // covered corner in the cell rasterization, and the column before it is not the
    // pane's.
    const ScreenCanvas c = paint(t.session());
    // ...and the pane's own prose begins one cell inside that corner, where its visible
    // boundary is. The corner cell itself is the boundary's.
    CHECK_FALSE(
        label_at(c, covered.x + kChromeCells, covered.y + kChromeCells).empty());
    const Occupancy at_corner = occupied_at(t.session().panes, t.session().setup.active,
                                            sc, covered.x, covered.y);
    CHECK(at_corner.occupied);
    CHECK(at_corner.kind == stock::kKind);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc,
                            covered.x - 1, covered.y)
                    .occupied);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc,
                            covered.x + covered.w, covered.y)
                    .occupied);

    // PROJECTION IS PURE: paint it again — twice more, through the same one function —
    // and the authored bytes and the picture are identical. The TUI quantizes at ITS
    // boundary and writes nothing back.
    const ScreenCanvas again = paint(t.session());
    CHECK(rasterized(c) == rasterized(again));
    CHECK(setup_persist::to_text(t.session().setup.active) == authored_before);

    // AND EXACT-CELL VALUES STAY EXACT: reauthor on the boundary and the covered cells are the
    // authored cells, byte for byte the whole-cell picture of the same desk.
    REQUIRE(place_at_canvas(t, ref_of(stock::kKind), cells_px(6),
                              cells_px(6))
                .accepted);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_size(desk, ref_of(stock::kKind),
                                 PaneSize{pane_unit::kPixels, cells_px(20)},
                                 PaneSize{pane_unit::kPixels, cells_px(5)})
                    .accepted);
    });
    const PixelRect exact = bounds_of(t.session().panes, t.session().setup.active,
                                     stock::kKind, screen_of(t.session()))
                               .rect;
    CHECK(cells_covered(exact) == CellRect{6, 6, 20, 5});
}

TEST_CASE("a version-2 whole-cell setup loads at exactly its old picture") {
    // THE LEGACY ROAD: the retained `v2` shapes write a version-2 file -- that namespace IS
    // those shapes, so this text is byte-honest -- and this build reads it, scales it exactly
    // onto pixels, and resolves it to the rectangle a version-2 build resolved.
    setup_persist::v2::WorkshopSetup old;
    old.format = setup_persist::kFormat;
    old.format_version = 2;
    old.name = "Old desk";
    setup_persist::v2::WorkshopSetupPane pane;
    pane.provider = "zengine.workshop";
    pane.pane = "builder";
    pane.place = setup_persist::v2::WorkshopPanePlace{"cells", 6, 5};
    pane.width = setup_persist::v2::WorkshopPaneSize{"cells", 40};
    pane.height = setup_persist::v2::WorkshopPaneSize{"pixels", 220};
    pane.front = 0;
    old.panes.push_back(pane);
    const std::string bytes = loom::compat::serialize(loom::to_value(old));

    const setup_persist::LoadedSetup read = setup_persist::from_text(bytes);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.setup.name == "Old desk");
    REQUIRE(read.setup.panes.size() == 1);
    const SetupPane& got = read.setup.panes[0];
    // CELLS BECAME PIXELS, EXACTLY — the same place, said finer and in the room, whose top was
    // the canvas's row 2.
    CHECK(got.place.mode == pane_unit::kPixels);
    CHECK(got.place.x == cells_px(6));
    CHECK(got.place.y == cells_px(3));
    CHECK(got.width.mode == pane_unit::kPixels);
    CHECK(got.width.amount == cells_px(40));
    // A PIXEL AXIS IS PIXELS IN EVERY VERSION AND CROSSES UNSCALED.
    CHECK(got.height.mode == pane_unit::kPixels);
    CHECK(got.height.amount == 220);

    // AND THE NEXT SAVE WRITES THE CURRENT VERSION, WHICH ROUND-TRIPS BYTE-IDENTICALLY.
    const std::string saved = setup_persist::to_text(read.setup);
    CHECK(saved.find("\"format_version\":\"6\"") != std::string::npos);
    CHECK(saved.find("\"mode\":\"pixels\"") != std::string::npos);
    const setup_persist::LoadedSetup back = setup_persist::from_text(saved);
    REQUIRE(back.outcome.accepted);
    CHECK(back.setup == read.setup);
    CHECK(setup_persist::to_text(back.setup) == saved);

    // A WORD THE VERSION 2 FORMAT NEVER HAD IS STILL REFUSED IN THAT VOCABULARY'S OWN TERMS
    // -- the legacy gate is version 2's, full strength.
    std::string forged = bytes;
    const std::size_t at = forged.find("\"mode\":\"cells\"");
    REQUIRE(at != std::string::npos);
    forged.replace(at, std::string("\"mode\":\"cells\"").size(), "\"mode\":\"barns\"");
    const setup_persist::LoadedSetup refused = setup_persist::from_text(forged);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("barns") != std::string::npos);
    CHECK(refused.outcome.refusal.find("default or cells") != std::string::npos);
}

TEST_CASE("a version-1 session restores a whole-cell desk through a conversion") {
    // THE SAME CLAIM THROUGH THE SEAM THAT CARRIES IT: the old shape and the multiply are
    // `session_history`'s -- an artifact's material -- and the reader compiles neither; it
    // asks the catalog. So the case mounts the conversions and reads the same file.
    session_history::v1::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = 1;
    old.viewport = session_history::v6::WorkshopViewport{120, 44};
    old.desk.format = setup_persist::kFormat;
    old.desk.format_version = 2;
    old.desk.name = "Yesterday";
    setup_persist::v2::WorkshopSetupPane pane;
    pane.provider = "zengine.workshop";
    pane.pane = "info";
    pane.place = setup_persist::v2::WorkshopPanePlace{"default", 0, 0};
    pane.width = setup_persist::v2::WorkshopPaneSize{"cells", 28};
    pane.height = setup_persist::v2::WorkshopPaneSize{"default", 0};
    pane.front = 0;
    old.desk.panes.push_back(pane);
    const std::string bytes = loom::compat::serialize(loom::to_value(old));

    // ...and with NO conversion live it is refused, by number, changing nothing.
    const session_persist::LoadedSession without = session_persist::from_text(bytes);
    CHECK_FALSE(without.outcome.accepted);
    CHECK(without.outcome.refusal.find("session version 1") != std::string::npos);

    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));
    const session_persist::LoadedSession read =
        session_persist::from_text(bytes, &conversions);
    const window_persist::LoadedWindow room_read = window_persist::import_from_session_text(bytes);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.present);
    CHECK(room_read.honoured);
    CHECK(room_read.viewport_w == cells_px(120));
    CHECK(room_read.viewport_h == cells_px(44));
    CHECK(live_layout(read).name == "Yesterday");
    // TWO ROWS: the one the version-1 file authored, whose whole-cell geometry became
    // sub-cells, and the Layouts pane its vintage had implicitly.
    REQUIRE(live_layout(read).panes.size() == 2);
    CHECK(live_layout(read).panes[0].width.mode == pane_unit::kPixels);
    CHECK(live_layout(read).panes[0].width.amount == cells_px(28));
    CHECK(live_layout(read).panes[1].ref == PaneRef{"zengine.workshop", "layouts"});

    // A LEGACY ROAD CARRIES NO PLACEMENT: nothing in a v1 file could have said one.
    CHECK_FALSE(room_read.placement.known);
    // ...AND NO LAYOUT PLURALITY EITHER: one layout, live at position zero.
    CHECK(read.layouts.size() == 1);
    CHECK(read.active == 0);

    // AND THE NEXT CLOSE WRITES THE CURRENT VERSION, byte-stable thereafter. The ENVELOPE's
    // number is what is asserted: the nested desk carries a `format_version` of its OWN at a
    // different number, and a search for the bare field would find that one.
    const std::string saved = session_text(read.layouts, read.active);
    CHECK(saved.find("\"version\":12") != std::string::npos);
    CHECK(saved.find("\"format\":\"zengine-workshop-session\",\"format_version\":\"12\"") !=
          std::string::npos);
    const session_persist::LoadedSession back = session_persist::from_text(saved);
    REQUIRE(back.outcome.accepted);
    CHECK(live_layout(back) == live_layout(read));
    CHECK(session_text(back.layouts, back.active) == saved);
}

TEST_CASE("pixel geometry survives the setup file without losing a pixel") {
    // EVERY PIXEL INSIDE A CELL IS A DIFFERENT AUTHORED FACT, and the file must keep each: a
    // sweep across the pixels of a cell, through save -> load -> save.
    Setup s = two_overlays();
    const PaneRef builder = ref_of(stock::kKind);
    for (const std::int64_t rem : {1, 5, 6, 11}) {
        CAPTURE(rem);
        REQUIRE(author_pane_place(s, builder, cells_px(3) + rem, cells_px(9) + (11 - rem)).accepted);
        REQUIRE(author_pane_size(s, builder, PaneSize{pane_unit::kPixels, cells_px(30) + rem},
                                 PaneSize{pane_unit::kPixels, cells_px(7) + rem})
                    .accepted);
        const std::string a = setup_persist::to_text(s);
        const setup_persist::LoadedSetup read = setup_persist::from_text(a);
        REQUIRE(read.outcome.accepted);
        CHECK(read.setup == s);
        CHECK(setup_persist::to_text(read.setup) == a);
        const SetupPane* row = pane_of(read.setup, builder);
        REQUIRE(row != nullptr);
        CHECK(row->place.x == cells_px(3) + rem);
        CHECK(row->width.amount == cells_px(30) + rem);
    }
}

TEST_CASE("a fine setup opens where the window painted it, its place in the room") {
    // A FILE WRITTEN ON THE 1/48 LATTICE, four sub-units to a window pixel: each edge lands on
    // the pixel the window painted it at -- floored, never rounded -- so the width is the painted
    // span, not the authored count divided; and the place, measured from the canvas then, is said
    // from the room's top, two cells down.
    setup_persist::v3::WorkshopSetup old;
    old.format = setup_persist::kFormat;
    old.format_version = 3;
    old.name = "Fine desk";
    setup_persist::v3::WorkshopSetupPane pane;
    pane.provider = "zengine.workshop";
    pane.pane = "builder";
    pane.place = setup_persist::v3::WorkshopPanePlace{"subcells", 301, 242}; // pixels 75.25, 60.5
    pane.width = setup_persist::v3::WorkshopPaneSize{"subcells", 1927};      // right edge 557.0
    pane.height = setup_persist::v3::WorkshopPaneSize{"pixels", 5};          // less than a cell
    pane.front = 0;
    old.panes.push_back(pane);
    const setup_persist::LoadedSetup read =
        setup_persist::from_text(loom::compat::serialize(loom::to_value(old)));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    REQUIRE(read.setup.panes.size() == 1);
    const SetupPane& got = read.setup.panes[0];
    CHECK(got.place.x == 75);
    CHECK(got.place.y == 60 - 24);
    CHECK(got.width.amount == 557 - 75); // floor((301 + 1927) / 4) less the floored place
    CHECK(got.height.amount == kPanePxMin); // a pane is never narrower than a cell

    // AND THE NEXT SAVE KEEPS EVERY PIXEL OF IT.
    const setup_persist::LoadedSetup back =
        setup_persist::from_text(setup_persist::to_text(read.setup));
    REQUIRE(back.outcome.accepted);
    CHECK(back.setup == read.setup);
}

TEST_CASE("a place in an older file keeps its place in the room") {
    // VERSIONS 2 AND 3, AND EVERY SESSION BEFORE 7, MEASURED A PLACE FROM THE CANVAS, while the
    // room began two cells down. So each place reads two cells less and stands where it stood in
    // its room; one above that room's top lands at the room's top, the nearest place there is.
    // Extents read as they always did.
    struct Row { const char* pane; std::int64_t x, y; std::int64_t room_y; };
    const Row rows[] = {{"top", 48, 96, 0},     // the old room's top: pixel 24
                        {"inside", 300, 400, 76}, // pixel 100
                        {"above", 40, 40, 0}};    // pixel 10, above the old room's top
    setup_persist::v3::WorkshopSetup three;
    three.format = setup_persist::kFormat;
    three.format_version = 3;
    three.name = "Room";
    for (const Row& r : rows) {
        setup_persist::v3::WorkshopSetupPane p;
        p.provider = "test.room";
        p.pane = r.pane;
        p.place = setup_persist::v3::WorkshopPanePlace{"subcells", r.x, r.y};
        p.width = setup_persist::v3::WorkshopPaneSize{"subcells", 480};
        p.height = setup_persist::v3::WorkshopPaneSize{"subcells", 480};
        p.front = static_cast<std::int64_t>(three.panes.size());
        three.panes.push_back(p);
    }
    const setup_persist::LoadedSetup read =
        setup_persist::from_text(loom::compat::serialize(loom::to_value(three)));
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    for (std::size_t i = 0; i < 3; ++i) {
        CAPTURE(rows[i].pane);
        CHECK(read.setup.panes[i].place.x == rows[i].x / 4);
        CHECK(read.setup.panes[i].place.y == rows[i].room_y);
        CHECK(read.setup.panes[i].height.amount == cells_px(10));
    }

    // VERSION 2, IN CELLS: row 5 of the canvas is row 3 of the room.
    setup_persist::v2::WorkshopSetup two;
    two.format = setup_persist::kFormat;
    two.format_version = 2;
    two.name = "Cells";
    setup_persist::v2::WorkshopSetupPane cell;
    cell.provider = "test.room";
    cell.pane = "cells";
    cell.place = setup_persist::v2::WorkshopPanePlace{"cells", 6, 5};
    cell.width = setup_persist::v2::WorkshopPaneSize{"cells", 20};
    cell.height = setup_persist::v2::WorkshopPaneSize{"cells", 4};
    two.panes.push_back(cell);
    const setup_persist::LoadedSetup old =
        setup_persist::from_text(loom::compat::serialize(loom::to_value(two)));
    REQUIRE_MESSAGE(old.outcome.accepted, old.outcome.refusal);
    CHECK(old.setup.panes[0].place == PanePlace{pane_unit::kPixels, cells_px(6), cells_px(3)});
    CHECK(old.setup.panes[0].height.amount == cells_px(4));

    // A SESSION OF VERSION 6, through its conversion: the same places, the same rule.
    session_history::v6::WorkshopSession six;
    six.format = session_persist::kFormat;
    six.format_version = 6;
    six.viewport = session_history::v6::WorkshopViewport{120, 44};
    six.layouts.push_back(session_history::v6::WorkshopLayout{three, session_history::absent_link()});
    six.active = 0;
    six.placement = session_history::absent_placement();
    op::Catalog conversions;
    REQUIRE(conversions.mount("suite", session_history::conversions()));
    const session_persist::LoadedSession restored = session_persist::from_text(
        loom::compat::serialize(loom::to_value(six)), &conversions);
    REQUIRE_MESSAGE(restored.outcome.accepted, restored.outcome.refusal);
    for (std::size_t i = 0; i < 3; ++i) {
        CAPTURE(rows[i].pane);
        CHECK(live_layout(restored).panes[i].place.y == rows[i].room_y);
    }
}

TEST_CASE("one authored value, spelled in whatever unit the active face reported") {
    // A ROW MUST NOT PRESENT A ROUNDED VALUE AS THE STORED ONE. A weaver reads the unit the face
    // in front of them can distinguish, and a value that face cannot say exactly is MARKED as
    // the projection it is: `10+1/2` would be exact and unreadable on a window; `126` is exact
    // there, and `~10` is honest in a terminal.
    const std::int64_t px = gfx::kCellPx; // what the shipped face REPORTS, not a
    const std::int64_t cells = 0;                   // constant Workshop is allowed to hold

    // THE UNIT WORD IS THE MEDIUM'S OWN ANSWER, and zero is a character medium's.
    CHECK(std::string(geometry_unit(px)) == "px");
    CHECK(std::string(geometry_unit(cells)) == "cells");
    CHECK(std::string(geometry_unit(0)) == "cells");

    // A WHOLE-CELL VALUE IS EXACT ON EVERY MEDIUM — it is the authored number in both
    // units, so neither spelling is a projection.
    CHECK(geometry_spelling(cells_px(40), cells).amount == "40");
    CHECK(geometry_spelling(cells_px(40), cells).exact);
    CHECK(geometry_spelling(cells_px(40), px).amount == "480");
    CHECK(geometry_spelling(cells_px(40), px).exact);
    CHECK(geometry_spelling(0, px).amount == "0");
    CHECK(geometry_spelling(0, px).exact);

    // A VALUE A HAND AUTHORED AT THE WINDOW'S PIXEL GRAIN IS EXACT IN PIXELS AND IS NOT
    // EXACT IN CELLS: ten and a half cells is 126 window pixels exactly, and no number of cells.
    CHECK(geometry_spelling(cells_px(10) + 6, px).amount == "126");
    CHECK(geometry_spelling(cells_px(10) + 6, px).exact);
    CHECK(geometry_spelling(cells_px(10) + 6, cells).amount == "10");
    CHECK_FALSE(geometry_spelling(cells_px(10) + 6, cells).exact);

    // EVERY PIXEL IS EXACT ON THE WINDOW: nothing finer than a pixel can be authored.
    CHECK(geometry_spelling(cells_px(10) + 1, px).amount == "121");
    CHECK(geometry_spelling(cells_px(10) + 1, px).exact);

    // THE MARK IS ASCII, because the shipped face's letterform is.
    CHECK(std::string(kProjectedMark) == "~");
    CHECK(std::string(kProjectedMark).size() == 1);

    SetupPane row;
    row.place = PanePlace{pane_unit::kPixels, cells_px(6) + 6, cells_px(5)};
    row.width = PaneSize{pane_unit::kPixels, cells_px(40) + 3};
    row.height = PaneSize{pane_unit::kPixels, 220};
    row.front = 2;
    // ON THE SHIPPED FACE every authored number is a whole window pixel, so nothing is marked
    // and no clause is owed.
    CHECK(pane_window_text(&row, px) == "@78,60 483x220 px f2");
    // IN A TERMINAL the same authored value is a projection on three axes, and says so once.
    CHECK(pane_window_text(&row, cells) == "@~6,5 ~40x~18 cells f2 (~ projected)");

    // AND THE WHOLE-CELL CASE READS AS THE PLAIN COUNT IT ALWAYS DID, in either face.
    SetupPane plain;
    plain.place = PanePlace{pane_unit::kPixels, cells_px(6), cells_px(5)};
    plain.width = PaneSize{pane_unit::kPixels, cells_px(40)};
    plain.height = PaneSize{pane_unit::kDefault, 0};
    plain.front = 0;
    CHECK(pane_window_text(&plain, cells) == "@6,5 40x- cells f0");
    CHECK(pane_window_text(&plain, px) == "@72,60 480x- px f0");

    // A ROW THAT AUTHORED NOTHING MEASURABLE NAMES NO UNIT — there is no number for a unit
    // word to be about, and `-x- cells` would be one.
    SetupPane reactive;
    reactive.front = 0;
    CHECK(pane_window_text(&reactive, px) == "-x- f0");
    CHECK(pane_window_text(&reactive, cells) == "-x- f0");
    CHECK(pane_window_text(nullptr, px) == "--");

    // A WHOLE RECTANGLE, THE SAME WAY — and the clause appears exactly once however many
    // numbers on the line are projections.
    CHECK(pixel_rect_text(PixelRect{cells_px(3), cells_px(4), cells_px(20), cells_px(6)}, cells) ==
          "@3,4 20x6 cells");
    CHECK(pixel_rect_text(PixelRect{cells_px(3), cells_px(4), cells_px(20), cells_px(6)}, px) ==
          "@36,48 240x72 px");
    CHECK(pixel_rect_text(PixelRect{cells_px(3) + 6, cells_px(4) + 6, cells_px(20), cells_px(6)}, cells) ==
          "@~3,~4 20x6 cells (~ projected)");
}

TEST_CASE("the canvas's device unit is the medium's answer, never Workshop's") {
    // An application HOLDS no medium's layout number: one is correct only for as long as there
    // is one medium. So this number is only ever the
    // one a medium reported, it arrives on the same message the room and the face metric
    // do, and a run nobody has spoken to reads as the character medium it has always been.
    Session s;
    CHECK(s.cell_px == 0); // a fresh Workshop claims no device pixel

    REQUIRE(adopt_screen(s, cells_px(100), cells_px(33)));
    CHECK(s.cell_px == 0); // ...and a medium that named no unit still claims none

    // A MEDIUM THAT NAMES ONE IS BELIEVED. It is the only party that can know.
    REQUIRE(adopt_screen(s, cells_px(100), cells_px(33), 8, 18, gfx::kCellPx));
    CHECK(s.cell_px == gfx::kCellPx);
    CHECK(std::string(geometry_unit(s.cell_px)) == "px");

    // A CHANGE OF UNIT ALONE IS A CHANGE -- a window that opens its canvas after its
    // first frame must not leave a weaver reading the wrong unit until something else
    // happens to move. (`report_extent` guards the same fact on the medium's side.)
    CHECK_FALSE(adopt_screen(s, cells_px(100), cells_px(33), 8, 18, gfx::kCellPx));
    CHECK(adopt_screen(s, cells_px(100), cells_px(33), 8, 18, 0));
    CHECK(s.cell_px == 0);

    // NON-POSITIVE IS THE VOCABULARY'S "MY DEVICE UNIT IS THE CELL", and a number nobody
    // could mean resolves to the reading that changes nothing rather than to a guess.
    REQUIRE(adopt_screen(s, cells_px(100), cells_px(33), 8, 18, gfx::kCellPx));
    REQUIRE(s.cell_px == gfx::kCellPx);
    CHECK(adopt_screen(s, cells_px(100), cells_px(33), 8, 18, -12));
    CHECK(s.cell_px == 0);

    // THE FACE METRIC AND THE CANVAS UNIT ARE DIFFERENT FACTS. A window whose font failed
    // to open sets no type and still lays its canvas out in pixels; reading the metric as
    // "am I graphical" is a near miss: the device unit is a fact of its own.
    REQUIRE(adopt_screen(s, cells_px(100), cells_px(33), 0, 0, gfx::kCellPx));
    CHECK(s.text_advance_px == 0);
    CHECK(s.cell_px == gfx::kCellPx);
    CHECK(std::string(geometry_unit(s.cell_px)) == "px");
}

TEST_CASE("the medium's unit reaches the READOUT and no geometry at all") {
    // THE FALSIFIER FOR "a device unit leaked into the lattice". Two sessions, identical
    // in every way except which medium spoke to them, holding the same authored desk:
    // every rectangle, every hit test and the whole composition must be indistinguishable.
    // Only the SENTENCE differs.
    Session cells;
    admit_stock(cells.panes); // the stand-in, first (stock)
    Session window;
    admit_stock(window.panes); // the stand-in, first (stock)
    cells.setup.active = two_overlays();
    window.setup.active = two_overlays();

    // THE DESK IS AUTHORED FIRST, and then each medium speaks -- deliberately that order.
    // A medium that rewrote a weaver's geometry on its way in would do it to a desk that
    // already existed, which is exactly the shape a restore or a second face has.
    const PaneRef builder = ref_of(stock::kKind);
    REQUIRE(author_pane_place(cells.setup.active, builder, cells_px(3) + 12, cells_px(4) + 12)
                .accepted);
    REQUIRE(author_pane_place(window.setup.active, builder, cells_px(3) + 12, cells_px(4) + 12)
                .accepted);
    REQUIRE(author_pane_size(cells.setup.active, builder,
                             PaneSize{pane_unit::kPixels, cells_px(30) + 12},
                             PaneSize{pane_unit::kPixels, cells_px(7) + 12})
                .accepted);
    REQUIRE(author_pane_size(window.setup.active, builder,
                             PaneSize{pane_unit::kPixels, cells_px(30) + 12},
                             PaneSize{pane_unit::kPixels, cells_px(7) + 12})
                .accepted);

    REQUIRE(adopt_screen(cells, cells_px(140), cells_px(40), 0, 0, 0));
    REQUIRE(adopt_screen(window, cells_px(140), cells_px(40), 0, 0, gfx::kCellPx));

    // NOT ONE AUTHORED NUMBER MOVED BECAUSE A MEDIUM SPOKE.
    CHECK(*pane_of(cells.setup.active, builder) == *pane_of(window.setup.active, builder));
    CHECK(pane_of(window.setup.active, builder)->width.amount == cells_px(30) + 12);

    const Screen sc_cells = screen_of(cells);
    const Screen sc_window = screen_of(window);
    CHECK(sc_cells.w == sc_window.w);
    CHECK(sc_cells.room_w == sc_window.room_w);

    const PaneBounds a = bounds_of(cells.panes, cells.setup.active, stock::kKind, sc_cells);
    const PaneBounds b =
        bounds_of(window.panes, window.setup.active, stock::kKind, sc_window);
    CHECK(a.rect == b.rect);
    CHECK(a.resolved == b.resolved);

    // AND THE HAND MEETS THE SAME CELLS. `occupied_at`'s answer is the pane's own
    // geometry; a device unit that had entered it would move the boundary here.
    for (std::int64_t y = 0; y < 12; ++y) {
        for (std::int64_t x = 0; x < 40; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            REQUIRE(occupied_at(cells.panes, cells.setup.active, sc_cells, x, y).occupied ==
                    occupied_at(window.panes, window.setup.active, sc_window, x, y).occupied);
        }
    }

    // WHAT DIFFERS IS THE SENTENCE, AND ONLY THE SENTENCE.
    const SetupPane* row = pane_of(cells.setup.active, builder);
    REQUIRE(row != nullptr);
    CHECK(pane_window_text(row, cells.cell_px) != pane_window_text(row, window.cell_px));
    CHECK(pane_window_text(row, window.cell_px).find(" px ") != std::string::npos);
    CHECK(pane_window_text(row, cells.cell_px).find(" cells ") != std::string::npos);
}

TEST_CASE("which parts of a pane's window its numbers alone do not place") {
    // The question the arrangement readout asks before it adds where the pane actually is: any
    // axis that is not exact pixels -- the code's answer, or a word.
    // ⚔ MUTATION: asking only for `default` -- a share-wide pane is read without where it is.
    SetupPane none;
    CHECK(pane_window_reads_words(&none));

    SetupPane placed;
    placed.place = PanePlace{pane_unit::kPixels, cells_px(2), cells_px(2)};
    CHECK(pane_window_reads_words(&placed)); // both extents still the code's

    SetupPane whole;
    whole.place = PanePlace{pane_unit::kPixels, cells_px(2), cells_px(2)};
    whole.width = PaneSize{pane_unit::kPixels, cells_px(20)};
    whole.height = PaneSize{pane_unit::kPixels, cells_px(6)};
    CHECK_FALSE(pane_window_reads_words(&whole));

    SetupPane shared = whole;
    shared.width = PaneSize{ui::kSizeShare, 5000};
    CHECK(pane_window_reads_words(&shared));
    SetupPane anchored = whole;
    anchored.place = PanePlace{ui::kPlaceBottomRight, cells_px(1), cells_px(1)};
    CHECK(pane_window_reads_words(&anchored));

    CHECK_FALSE(pane_window_reads_words(nullptr));
}

TEST_CASE("the readout says each word, and where the words put the pane") {
    // ⚔ MUTATION: `pane_window_text` saying `-` for every axis that is not pixels -- a share, a
    // fit and a point of the room all read as the code's answer.
    SetupPane row;
    row.front = 2;
    row.place = PanePlace{ui::kPlaceBottomRight, cells_px(1), cells_px(2)};
    row.width = PaneSize{ui::kSizeShare, 5000};
    row.height = PaneSize{ui::kSizeFit, 0};
    CHECK(pane_window_text(&row, 0) == "@bottom-right 1,2 50.00%xfit cells f2");
    row.place = PanePlace{ui::kPlaceTop, 0, cells_px(3)};
    row.width = PaneSize{ui::kSizeFill, 0};
    row.height = PaneSize{pane_unit::kPixels, cells_px(7)};
    CHECK(pane_window_text(&row, 0) == "@top 3 fillx7 cells f2");
    row.place = PanePlace{ui::kPlaceCenter, 0, 0};
    row.width = PaneSize{ui::kSizeShare, 3333};
    row.height = PaneSize{};
    CHECK(pane_window_text(&row, 0) == "@center 33.33%x- f2"); // no number, so no unit
    // A SHARE PLACE says both its numbers as shares. ⚔ MUTATION: `pane_window_text` saying a
    // share place's numbers as counts of the face's unit -- `@share 0,277 cells`.
    row.place = PanePlace{ui::kPlaceShare, 0, 3333};
    row.width = PaneSize{ui::kSizeFill, 0};
    row.height = PaneSize{pane_unit::kPixels, cells_px(4)};
    CHECK(pane_window_text(&row, 0) == "@share 0.00%,33.33% fillx4 cells f2");

    // ...AND THE ARRANGING NOTICE ADDS WHERE THE WORDS PUT IT, from the unclipped window.
    Live t;
    pick(t, second::kKind);
    stage_desk(t, [](Setup& desk) {
        for (SetupPane& r : desk.panes) {
            if (r.ref == ref_of(second::kKind)) {
                r.place = PanePlace{ui::kPlaceBottomRight, cells_px(1), cells_px(2)};
                r.width = PaneSize{ui::kSizeShare, 5000};
                r.height = PaneSize{pane_unit::kPixels, cells_px(6)};
            }
        }
    });
    enter_arrange_desk(t);
    select_pane(t, ref_of(second::kKind));
    const Screen sc = screen_of(t.session());
    const PixelRect at = room_of_canvas(
        bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc).resolved, sc);
    CHECK(t.notice().find("@bottom-right 1,2 50.00%x6 cells") != std::string::npos);
    CHECK(t.notice().find(" -- now " + pixel_rect_text(at, t.session().cell_px)) !=
          std::string::npos);
}

// ============================================================================
// The contextual surface's picture is the surface a press meets
// ============================================================================

TEST_CASE("the contextual surface is painted where it is hit") {
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const CellRect slot = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);

    // The published region, at `context_bounds` exactly: the FIRST ROW IS AN ACTION -- no
    // heading naming the subject, no row restating the two gestures the band's legend already
    // says -- and the rows are the declared population with the cursor's own mark, labels from
    // the one action truth, groups saying they descend.
    const std::vector<std::string> rows = context_rows_on(t.shown(), t.session());
    REQUIRE(rows.size() == 5);
    for (const std::string& row : rows) {
        CHECK(row.find("ACTIONS") == std::string::npos);
        CHECK(row.find("chooses") == std::string::npos);
    }
    CHECK(rows[0] == "> arrange");
    CHECK(rows[1] == "  Order >");
    CHECK(rows[2] == "  Reset >");
    CHECK(rows[3] == "  edit code");
    CHECK(rows[4] == "  hide pane");

    // THE INVERSE PAIR, SPENT: a press at the row the painter drew chooses that row.
    // Row 4 is `remove` -- and the pane is gone, through the one door.
    t.press_canvas(context_cell_x(t.session()), context_entry_cell_y(t.session(), 4));
    CHECK_FALSE(t.menu().open);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(stock::kKind)));

    // ...and inside a group the heading's hint says the smaller way out.
    t.right_press(40, 0); // any subject re-opens; the room serves
    REQUIRE(t.menu().open);
}

TEST_CASE("an open group paints its own rows and its own way out") {
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const CellRect slot = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    t.key(input::scan::kDown);
    t.key(input::scan::kReturn); // Order
    REQUIRE(t.menu().group == "Order");
    // A GROUP LEVEL IS ITS ROWS AND NOTHING ELSE: no breadcrumb, because every one of these
    // labels says what it does on its own, which is what `kActionCatalog` declares.
    const std::vector<std::string> rows = context_rows_on(t.shown(), t.session());
    REQUIRE(rows.size() == 4);
    CHECK(rows[0] == "> front");
    CHECK(rows[1] == "  back");
    CHECK(rows[2] == "  raise");
    CHECK(rows[3] == "  lower");
}

TEST_CASE("manage.remove speaks through the keymap's own claim surfaces") {
    // The band's legend for an arrangement scope carries the verb the moment the row is
    // declared -- generated, never hand-kept -- and its spelling is the effective
    // binding's.
    const Keymap defaults;
    const std::vector<std::string> pairs =
        help_pairs(defaults, KeySpelling::kShort, KeyContext::kArrangePane);
    bool said = false;
    for (const std::string& pair : pairs) {
        if (pair == "d hide pane") {
            said = true;
        }
    }
    CHECK(said);
    // ...and the contextual surface's own vocabulary is a declared context like any
    // other, so the hotkey view can describe it and a keymap file can rebind it.
    CHECK(defaults.action_for(KeyContext::kContext, input::scan::kReturn,
                              input::mod::kNone) == Act::kContextChoose);
    CHECK(defaults.action_for(KeyContext::kContext, input::scan::kEscape,
                              input::mod::kNone) == Act::kContextBack);
}

// ============================================================================
// The contextual surface opens beside the hand, sized by what it says
// ============================================================================

TEST_CASE("the popup opens at the press's own cell, and its extent is its content") {
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);

    // TWO WIDELY SEPARATED PRESSES, TWO LOCAL RECTANGLES -- the falsifier for one fixed
    // column, wherever the weaver clicked.
    t.right_press_canvas(10, 5);
    REQUIRE(t.menu().open);
    const PixelRect near_a = context_bounds(t.session(), screen_of(t.session()));
    CHECK(gfx::cell_of_pixel(near_a.x) == 10);
    CHECK(gfx::cell_of_pixel(near_a.y) == 5);
    t.right_press_canvas(100, 10);
    const PixelRect near_b = context_bounds(t.session(), screen_of(t.session()));
    CHECK(gfx::cell_of_pixel(near_b.x) == 100);
    CHECK(gfx::cell_of_pixel(near_b.y) == 10);

    // THE EXTENT IS THE LEVEL'S OWN COMPOSITION: on a cell medium exactly the population,
    // inside the surface's own chrome, with no heading rows and no floor-to-ceiling column.
    const std::vector<ContextEntry> rows =
        context_population(t.menu().subject, t.menu().group);
    const ProsePlace place =
        prose_place(near_b, screen_of(t.session()));
    CHECK(place.rows == static_cast<std::int64_t>(rows.size()));
    const PixelRect column = overlay_column(screen_of(t.session()));
    CHECK(near_b.h < column.h);
    CHECK(near_b.w <= gfx::px_of_cells(kContextMaxCols));
    CHECK(near_b.w < column.w);
}

TEST_CASE("the popup shifts to stay usable inside the room, at every boundary") {
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    // A PANE IN THE FAR CORNER, so the press on its CHROME captures a PANE subject and the
    // menu's top row is `arrange`. The body is empty by default (WL-CTX-08), so the host menu
    // is reached on the title row (the chrome), which for a right-column pane sits at the
    // top-right corner.
    open_at_top_right(t, second::kKind);
    const Screen sc = screen_of(t.session());
    const std::int64_t floor_y = kRoomCellY + cells_of(sc).room_h;
    const CellRect corner_pane = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc).rect);

    // NEAR THE FAR (RIGHT) EDGE: the menu may not fit rightward of the press, so it shifts
    // left exactly as far as wholeness requires -- and stays a popup within the room.
    t.right_press_canvas(corner_pane.x + corner_pane.w - 2, corner_pane.y);
    REQUIRE(t.menu().open);
    const PixelRect clamped = context_bounds(t.session(), screen_of(t.session()));
    CHECK(clamped.x + clamped.w <= sc.w);
    CHECK(clamped.y + clamped.h <= gfx::px_of_cells(floor_y));
    CHECK(clamped.x >= 0);
    CHECK(clamped.y >= gfx::px_of_cells(kRoomCellY));

    // ...AND THE PAINTED SURFACE IS AT THE SHIFTED PLACE, whole: the press resolver and
    // the painter read one geometry, so a row chosen at the clamped rectangle is the row
    // the weaver sees there (the inverse pair, spent at the wall).
    const std::vector<std::string> shown = context_rows_on(t.shown(), t.session());
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0] == "> arrange");

    // NEAR THE ORIGIN, nothing shifts: the anchor is already legal.
    t.right_press_canvas(0, 1);
    const PixelRect origin = context_bounds(t.session(), screen_of(t.session()));
    CHECK(gfx::cell_of_pixel(origin.x) == 0);
    CHECK(gfx::cell_of_pixel(origin.y) == kRoomCellY);
}

TEST_CASE("the keyboard entrance has no pointer and invents none") {
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    t.key(input::scan::kA);
    t.text("a");
    REQUIRE(t.menu().open);
    CHECK_FALSE(t.menu().anchored);
    // The deterministic non-pointer placement: the overlay stack's own corner, the home
    // of every keyboard surface on this screen -- and still content-sized.
    const Screen sc = screen_of(t.session());
    const CellRect slot = cells_covered(placement_bounds(placement::kOverlayStack, 0, sc));
    const PixelRect at = context_bounds(t.session(), sc);
    CHECK(gfx::cell_of_pixel(at.x) == slot.x);
    CHECK(gfx::cell_of_pixel(at.y) == slot.y);
}

TEST_CASE("entering a group stays at the anchor, and the popup resizes to it") {
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    open_pane(t, ref_of(stock::kKind));
    // Author the pane somewhere unmistakably far from any fixed column, and ask beside it.
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), cells_px(90),
                                  cells_px(20))
                    .accepted);
    });
    const CellRect slot = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);
    const PixelRect top = context_bounds(t.session(), screen_of(t.session()));
    CHECK(gfx::cell_of_pixel(top.x) == slot.x + 1);

    // DESCEND: the level's rows change, the anchor does not -- context stays local
    // instead of jumping back to any global column.
    t.key(input::scan::kDown);
    t.key(input::scan::kReturn); // Order
    REQUIRE(t.menu().group == "Order");
    const PixelRect inside = context_bounds(t.session(), screen_of(t.session()));
    CHECK(inside.x == top.x);
    CHECK(inside.y == top.y);
    const ProsePlace place = prose_place(inside, screen_of(t.session()));
    CHECK(place.rows == 4); // exactly the Order group's rows, and no chrome row

    // AND THE INVERSE PAIR HOLDS AT THE ANCHORED PLACE: pressing the `back` row where
    // the painter drew it performs back on the captured pane.
    t.press_canvas(context_cell_x(t.session()), context_entry_cell_y(t.session(), 1));
    CHECK_FALSE(t.menu().open);
    CHECK(t.notice().find("back-most") != std::string::npos);
}

TEST_CASE("shortcut annotations teach only truthful surrounding bindings") {
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);

    const auto entry_of = [&](const char* id) {
        const std::vector<ContextEntry> rows =
            context_population(t.menu().subject, t.menu().group);
        for (const ContextEntry& e : rows) {
            if (!e.is_group && e.row != nullptr && std::string(e.row->id) == id) {
                return e;
            }
        }
        FAIL("no entry for ", id);
        return ContextEntry{};
    };

    // OVER COMMAND MODE, the room's doors teach their command keys -- and
    // `manage.reset-order`, whose only row lives in a mode the weaver is NOT returning to,
    // teaches nothing (the distant-mode refusal).
    t.right_press(40, 0);
    REQUIRE(t.menu().subject == context_subject::kRoot);
    CHECK(context_annotation(t.session(), entry_of("workshop.manage")) == "w");
    CHECK(context_annotation(t.session(), entry_of("setup.name")) == "s");
    CHECK(context_annotation(t.session(), entry_of("manage.reset-order")).empty());

    // ...AND THE PAINTED ROW CARRIES THE GESTURE AT THE LEVEL'S ANNOTATION COLUMN, visually
    // subordinate by position, the label leading.
    {
        const std::vector<std::string> shown =
            context_rows_on(t.shown(), t.session());
        bool painted = false;
        for (const std::string& row : shown) {
            if (row.find("arrange desk") != std::string::npos) {
                painted = row.size() > 2 && row.back() == 'w' &&
                          row.find("arrange desk") < row.rfind('w');
            }
        }
        CHECK(painted);
    }

    t.key(input::scan::kEscape); // close the room menu

    // A PANE ROW NEVER ANNOTATES: its actions live in the arrangement scopes, which are
    // not the interaction the weaver returns to when this surface closes.
    open_pane(t, ref_of(stock::kKind));
    const CellRect slot = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);
    CHECK(context_annotation(t.session(), entry_of("manage.arrange")).empty());
    CHECK(context_annotation(t.session(), entry_of("manage.remove")).empty());

    // AND A MODE BENEATH THAT SWALLOWS BARE KEYS SUPPRESSES THE COMMAND ANNOTATIONS: with a
    // pane holding the keys, `s` is the pane's and would not save -- so the menu does not teach
    // it. The rule is `row_active`'s, over whatever rows there are.
    t.key(input::scan::kEscape); // close the pane menu
    t.press_canvas(slot.x + 1, slot.y + 1); // into the pane: it holds the keys
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPane);
    t.right_press(40, 15);
    REQUIRE(t.menu().subject == context_subject::kRoot);
    CHECK(context_annotation(t.session(), entry_of("setup.name")).empty());
    CHECK(context_annotation(t.session(), entry_of("setup.restore")).empty());
    t.key(input::scan::kEscape); // the menu
    release_keys(t);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kCommand);

    // A REMAP MOVES THE ANNOTATION WITH NOTHING HERE TO UPDATE: the spelling is the
    // effective keymap's, so one truth moves every claim.
    REQUIRE(apply_overrides({{"workshop.manage", "y"}}, legend_mode::kDefault,
                            live(t).keymap)
                .accepted);
    t.right_press(40, 15);
    REQUIRE(t.menu().subject == context_subject::kRoot);
    CHECK(context_annotation(t.session(), entry_of("workshop.manage")) == "y");
}

// ============================================================================
// The desk as a set of tools: boundaries, selection, and the help beside it
// ============================================================================
// Each case is a FALSIFIER naming the mutation it catches, together because they make ONE
// claim: what a weaver sees on the desk and what their hand reaches are the same thing, and
// every pane's edge is where the pane says it is.

TEST_CASE("a pane's interior is its outer rectangle less one cell of chrome") {
    // ⚔ MUTATION: delete the border -- `pane_interior` answering its own argument. Every
    // number below moves, in both directions, so the picture and the room move together.
    const PixelRect outer = pixels_of_cells(CellRect{4, 3, 20, 9});
    CHECK(cells_covered(pane_interior(outer, kChromePx)) == CellRect{5, 4, 18, 7});

    // ...AND THE INVERSE IS EXACT, which is what a content-sized surface asks for.
    const CellRect grown = cells_covered(chrome_outer_of(0, 0, cells_px(18), cells_px(7)));
    CHECK(grown.w == 20);
    CHECK(grown.h == 9);
    CHECK(cells_covered(pane_interior(pixels_of_cells(grown), kChromePx)) == CellRect{1, 1, 18, 7});

    // A PANE TOO SMALL FOR ITS OWN CHROME HAS NO INTERIOR, and says so with an empty
    // rectangle rather than a negative one -- ⚔ MUTATION: dropping the guard, which would
    // hand every `w <= 0` consumer a wrapped-around width instead of "nowhere".
    for (const CellRect tiny : {CellRect{0, 0, 2, 9}, CellRect{0, 0, 20, 2},
                                CellRect{0, 0, 1, 1}, CellRect{0, 0, 0, 0}}) {
        CAPTURE(tiny.w);
        CAPTURE(tiny.h);
        const PixelRect none = pane_interior(pixels_of_cells(tiny), kChromePx);
        CHECK(none.empty());
        CHECK(none.w <= 0);
        CHECK(none.h <= 0);
    }
}

TEST_CASE("the border a weaver sees and the room a pane spends are one subtraction") {
    // ⚔ MUTATION: insetting the PAINTER and not the press inverse, or not the room grant.
    // The three resolutions below are the only three, and they answer one rectangle.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const PixelRect outer =
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect;
    REQUIRE_FALSE(outer.empty());

    // WHAT IS PAINTED: one rect at the OUTER bounds, and the region inside it.
    const ScreenCanvas c = paint(t.session());
    const CellRect cells = cells_covered(outer);
    CHECK(has_rect(c, cells.x, cells.y, cells.w, cells.h, kPaneChrome));
    const CellRect inside = pane_body_cells(outer);
    const std::vector<ScreenRegion> at = regions_at(c, inside.x, inside.y);
    REQUIRE(at.size() == 1);
    CHECK(covered_cells(at.front()) == inside);

    // WHAT THE ROOM IS: the same interior, resolved by the same call.
    const ExternalBodyPlace body = external_body_place(outer, sc, kExternalHeaderRows);
    CHECK(cells_covered(PixelRect{body.region_x, body.region_y, body.region_w, body.region_h}) ==
          inside);

    // WHAT THE HAND MEETS: the OUTER rectangle, unchanged -- the boundary is INSIDE the pane,
    // so the corner cell is still the pane's and the cell past it is not. Said as "not this
    // pane's" rather than "not anybody's": the stack's slot ends inside the right column's
    // place, so one cell past this pane is the pane standing there.
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, cells.x, cells.y)
              .kind == stock::kKind);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, cells.x + cells.w - 1,
                      cells.y + cells.h - 1)
              .kind == stock::kKind);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, cells.x + cells.w,
                      cells.y)
              .kind != stock::kKind);
}

TEST_CASE("selecting a pane lifts it, in the picture and under the hand at once") {
    // THE OVERLAP THIS CASE IS ABOUT, authored directly so the case is about the LIFT rather
    // than about the gestures that made the desk.
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    // THE PANE IN THE RIGHT COLUMN IS AUTHORED: its occupant is a desk row.
    open_at_top_right(t, second::kKind);
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const CellRect side = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc).rect);
    REQUIRE(place_at_canvas(t, ref_of(stock::kKind),
                              gfx::px_of_cells(side.x - 4),
                              gfx::px_of_cells(side.y + 2))
                .accepted);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(send_to_back(desk, ref_of(stock::kKind)));
    });
    const std::int64_t at_x = side.x + 1;
    const std::int64_t at_y = side.y + 3;

    // AUTHORED: the right-column pane is in front, so it answers -- and with nothing selected
    // the two orders are the same list.
    const std::vector<std::int64_t> authored = authored_order(t.session());
    REQUIRE(authored.size() >= 2);
    // THE RIGHT-COLUMN PANE IS AHEAD OF THE BUILDER, the relation this case is about -- said
    // as a relation rather than as "last", because a fresh desk's front-most row is the
    // Layouts pane, nowhere near the cells this case overlaps.
    CHECK(index_of(authored, second::kKind) > index_of(authored, stock::kKind));
    CHECK(painted_order(t.session()) == authored);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, at_x, at_y).kind ==
          second::kKind);

    // SELECTED: press the Builder where only the Builder is, and it comes forward.
    // ⚔ MUTATION: `paint_panes` or `occupied_at` walking `presentation_order` again.
    t.press_canvas(side.x - 3, side.y + 3);
    REQUIRE(t.session().panes.selected == stock::kKind);
    const std::vector<std::int64_t> lifted = painted_order(t.session());
    CHECK(lifted.back() == stock::kKind);
    CHECK(lifted != authored);
    CHECK(authored_order(t.session()) == authored); // ...and the AUTHORED order did not move
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, at_x, at_y).kind ==
          stock::kKind);

    // AND THE PAINT AGREES WITH IT: the plane carrying the Builder's backdrop is after the one
    // carrying the right-column pane's. ⚔ MUTATION: lifting the hit order and not the paint
    // order, the exact defect "what I see in front is what my pointer reaches" forbids.
    const CellRect builder_cells =
        cells_covered(bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                                screen_of(t.session()))
                          .rect);
    const ScreenCanvas painted = paint(t.session());
    std::int64_t last_info = -1;
    std::int64_t last_builder = -1;
    for (std::size_t li = 0; li < painted.layers.size(); ++li) {
        for (const ScreenRect& r : painted.layers[li].rects) {
            const CellRect at = covered_cells(r);
            if (at.x == side.x && at.y == side.y && at.w == side.w) {
                last_info = static_cast<std::int64_t>(li);
            }
            if (at.x == builder_cells.x && at.y == builder_cells.y && at.w == builder_cells.w) {
                last_builder = static_cast<std::int64_t>(li);
            }
        }
    }
    REQUIRE(last_info >= 0);
    REQUIRE(last_builder >= 0);
    CHECK(last_builder > last_info);

    // THE LIFT TRANSFERS, and the pane that had it falls back into its authored place with
    // nothing restored -- because nothing was moved.
    t.press_canvas(side.x + side.w - 1, side.y);
    REQUIRE(t.session().panes.selected == second::kKind);
    CHECK(painted_order(t.session()).back() == second::kKind);
    CHECK(authored_order(t.session()) == authored);
}

TEST_CASE("the selected pane wears its own chrome, and only it") {
    // The second pane is authored into the right column. ⚔ MUTATION: `paint_panes` handing
    // every pane `kPaneChrome`, which collapses the selected style into the ordinary one -- the
    // picture would still be bordered and a weaver could not tell which pane they work with.
    Live t;
    open_at_top_right(t, second::kKind);
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const auto chrome_of = [&](std::int64_t kind) {
        const CellRect at =
            cells_covered(bounds_of(t.session().panes, t.session().setup.active, kind,
                                    screen_of(t.session()))
                              .rect);
        for (const ScreenRect& r : all_rects(paint(t.session()))) {
            if (covered_cells(r) == at) {
                return r.role;
            }
        }
        return gfx::role::kNone;
    };
    const CellRect builder = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect);

    CHECK(chrome_of(stock::kKind) == kPaneChrome);
    CHECK(chrome_of(second::kKind) == kPaneChrome);

    t.press_canvas(builder.x, builder.y);
    REQUIRE(t.session().panes.selected == stock::kKind);
    CHECK(chrome_of(stock::kKind) == kPaneChromeSelected);
    CHECK(chrome_of(second::kKind) == kPaneChrome);
    CHECK(kPaneChromeSelected != kPaneChrome); // the two roles are two roles

    // A PRESS ON NOBODY'S PANE CLEARS IT, by the same one line that set it.
    t.press_canvas(kRoomCellX + 1,
                   kRoomCellY + gfx::cell_of_pixel(screen_of(t.session()).room_h) - 1);
    CHECK(t.session().panes.selected == kNoPaneKind);
    CHECK(chrome_of(stock::kKind) == kPaneChrome);
}

TEST_CASE("the selection lift never reaches the file, and no session starts with one") {
    // ⚔ MUTATION: writing `front` on selection -- which is what "raise on click" would be, and
    // what a selection that never writes the order forbids. The authored BYTES are compared and
    // not only the ranks, so a rewrite that happened to produce the same permutation is still
    // caught if it moved anything else.
    Live t;
    open_at_top_right(t, second::kKind);
    open_pane(t, ref_of(stock::kKind));
    const std::string before = setup_persist::to_text(t.session().setup.active);
    const std::vector<std::int64_t> ranks = ranks_of(t.session().setup.active);

    const Screen sc = screen_of(t.session());
    // ⚠ THE PRESS IS ON EACH PANE'S TOP-RIGHT CELL, NOT ITS TOP-LEFT. The stack's slot and the
    // right column overlap at this extent, and the right-column pane's top-left is under the
    // Editor: a press there selects the pane in front, the wrong pane. The right-hand cell of
    // the same row is each pane's own at every extent.
    for (const std::int64_t kind : {stock::kKind, second::kKind, stock::kKind}) {
        CAPTURE(kind);
        const CellRect at = cells_covered(
            bounds_of(t.session().panes, t.session().setup.active, kind, sc).rect);
        // Each pane's own far corner: the stack's slot at its left edge, the right column's
        // pane at its right, so neither press can land in the columns the two share -- where
        // the answer would be whichever of them the selection lift had put in front.
        const std::int64_t x = kind == second::kKind ? at.x + at.w - 1 : at.x;
        t.press_canvas(x, at.y);
        REQUIRE(t.session().panes.selected == kind);
        CHECK(setup_persist::to_text(t.session().setup.active) == before);
        CHECK(ranks_of(t.session().setup.active) == ranks);
    }

    // A FRESH SESSION SELECTS NOTHING: it is a field of no durable shape.
    Session fresh;
    admit_stock(fresh.panes); // the stand-in, first (stock)
    CHECK(fresh.panes.selected == kNoPaneKind);
    CHECK(selected_pane(fresh.panes) == kNoPaneKind);

    // ...AND A SELECTION THAT STOPPED BEING SEATED LEAVES NO GHOST IN FRONT.
    Panes gone = t.session().panes;
    gone.selected = kFirstRuntimeKind + 99; // a kind nobody ever opened
    CHECK(selected_pane(gone) == kNoPaneKind);
    CHECK(effective_pane_order(t.session().setup.active, gone) ==
          presentation_order(t.session().setup.active, gone));
}

TEST_CASE("a transient surface stays over the pane it covers, selected or not") {
    // ⚔ MUTATION: giving the selection lift priority over the overlay planes -- a pane
    // drawn in front of the contextual surface a weaver just opened on it.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const CellRect builder = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect);
    t.press_canvas(builder.x, builder.y);
    REQUIRE(t.session().panes.selected == stock::kKind);

    // THE CHROME (title row) opens the host's pane menu; the body is empty by default (WL-CTX-08).
    t.right_press_canvas(builder.x + 2, builder.y);
    REQUIRE(t.menu().open);
    const CellRect popup = cells_covered(context_bounds(t.session(), screen_of(t.session())));
    const ScreenCanvas c = paint(t.session());
    std::int64_t last_pane = -1;
    std::int64_t last_popup = -1;
    for (std::size_t li = 0; li < c.layers.size(); ++li) {
        for (const ScreenRect& r : c.layers[li].rects) {
            const CellRect at = covered_cells(r);
            if (at.x == builder.x && at.y == builder.y && at.w == builder.w) {
                last_pane = static_cast<std::int64_t>(li);
            }
            if (at == popup) {
                last_popup = static_cast<std::int64_t>(li);
            }
        }
    }
    REQUIRE(last_pane >= 0);
    REQUIRE(last_popup >= 0);
    CHECK(last_popup > last_pane);
    // ...and the transient chrome is its own voice, neither pane role.
    CHECK(kTransientChrome != kPaneChrome);
    CHECK(kTransientChrome != kPaneChromeSelected);
}

TEST_CASE("the contextual surface is its actions, and its width is theirs") {
    // ⚔ MUTATION: putting the heading or the hint row back. The row COUNT is the
    // population's exactly, and the width is bounded by the widest row that remains --
    // which the removed heading, being the longest string on this level, would break.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const CellRect builder = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect);
    t.right_press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);

    const std::vector<ContextEntry> rows =
        context_population(t.menu().subject, t.menu().group);
    const PixelRect bounds = context_bounds(t.session(), screen_of(t.session()));
    const ProsePlace place = prose_place(bounds, screen_of(t.session()));
    CHECK(place.rows == static_cast<std::int64_t>(rows.size()));

    // NO CHROME ROW SURVIVES ANYWHERE IN THE PAINTED SURFACE.
    const std::vector<std::string> shown = context_rows_on(t.shown(), t.session());
    REQUIRE(shown.size() == rows.size());
    for (const std::string& row : shown) {
        CHECK(row.find("ACTIONS") == std::string::npos);
        CHECK(row.find("chooses") == std::string::npos);
        CHECK(row.find("closes") == std::string::npos);
        CHECK(row.find(ref_text(ref_of(stock::kKind))) == std::string::npos);
    }

    // THE WIDTH IS THE WIDEST ROW PLUS THE CHROME, AND NOTHING WIDER: a heading such as
    // `ACTIONS -- ` and the pane's reference would be the longest string on this level, and
    // the popup is measured to the shorter action rows.
    std::size_t widest = 0;
    for (const std::string& row : shown) {
        widest = row.size() > widest ? row.size() : widest;
    }
    const CellRect popup = cells_covered(bounds);
    CHECK(popup.w == static_cast<std::int64_t>(widest) + 2 * kChromeCells);
    const std::string retired = "ACTIONS -- " + ref_text(ref_of(stock::kKind));
    CHECK(popup.w < static_cast<std::int64_t>(retired.size()));

    // AND THE INVERSE PAIR HAS NO OFFSET LEFT TO DISAGREE ABOUT: pressing the LAST row
    // where the painter drew it performs the last action, which on this level is `remove`.
    t.press_canvas(context_cell_x(t.session()),
                   context_entry_cell_y(t.session(), rows.size() - 1));
    CHECK_FALSE(t.menu().open);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(stock::kKind)));
}

TEST_CASE("no ordinary pane spends a row teaching a key the keymap already owns") {
    // ⚔ MUTATION: putting a gesture claim back into a pane's header or body. The audit is over
    // the PROJECTION -- what a weaver reads at each pane's rectangle -- so a hint anywhere in a
    // built-in pane's composition is caught, keymap-spelled or hard-coded. Not audited: an
    // EXTERNAL pane's rows (the provider's words about bindings Workshop is never told), the
    // band and the full hotkey view (the help surfaces), and the attention view and the
    // contextual surface (modes, not panes).
    Live t;
    t.extent_px(cells_px(160), cells_px(60), 0, 0);
    for (const PaneRef& ref : {ref_of(stock::kKind), ref_of(second::kKind)}) {
        open_pane(t, ref);
    }
    for (const std::int64_t kind : {stock::kKind, second::kKind}) {
        CAPTURE(kind);
        const CellRect body =
            pane_body_cells(bounds_of(t.session().panes, t.session().setup.active, kind,
                                      screen_of(t.session()))
                                .rect);
        if (body.w <= 0 || body.h <= 0) {
            continue;
        }
        const std::string shown = rect_text(t.shown(), body);
        for (const char* claim : {" build,", " pick,", " frontier,", " removes", "press ",
                                  "up/down", " opens the ", "ctrl+", "keys:"}) {
            CHECK_MESSAGE(shown.find(claim) == std::string::npos, "pane ", kind,
                          " teaches '", claim, "'");
        }
        // A CONTROL IS NOT A KEYBOARD HINT, and the audit must find rows at all: a pane whose
        // body were empty would pass every claim above for the wrong reason.
        CHECK_FALSE(shown.empty());
    }
    // ...AND THE GESTURES ARE STILL DISCOVERABLE, in the one surface that owns them. The
    // Builder's `build`, `recipe`, `frontier` and `edit source` are the BUILDER PANE'S declared
    // rows, in the legend and hotkey view when that pane holds the keyboard, spelled from ITS
    // declaration under the weaver's keymap (WL-KEY-15) -- a host listing them unconditionally
    // would teach a key that does nothing where the weaver stands. The panes suite proves that.
    const std::string view = keymap_text(t.session());
    for (const char* label : {"titles", "arrange desk"}) {
        CHECK_MESSAGE(view.find(label) != std::string::npos, "the help lost '", label, "'");
    }
    for (const char* gone : {"build", "recipe", "frontier", "edit source"}) {
        CHECK_MESSAGE(view.find(gone) == std::string::npos,
                      "the host still teaches the Builder pane's '", gone, "'");
    }
}

// ============================================================================
// The graphical desk's chrome: a terminal frames a pane with a cell, the shipped window frames
// the same authored pane with a device PIXEL, and the interior saved goes back to the pane.
// ============================================================================

/// The two faces compared, built from what a medium REPORTS: a terminal says nothing about
/// pixels; the shipped window says a cell is `kCellPx` of them and hands over its face.
inline Screen tui_screen() {
    return screen_of(screen_session(kScreenMinW, kScreenMinH, 0, 0, 0));
}
inline Screen sdl_screen() {
    return screen_of(screen_session(kScreenMinW, kScreenMinH, 8, 18, gfx::kCellPx));
}

TEST_CASE("the chrome a pane wears is one unit of the face in front of the weaver") {
    // ⚔ MUTATION: `chrome_grain` answering `kChromePx` whatever the medium said -- what "SDL
    // still subtracts one full text cell" looks like from inside. ⚔ MUTATION (the other
    // direction): a terminal adopting the pixel answer, which floors to nothing there and
    // leaves a pane with no edge at all.
    const Screen tui = tui_screen();
    const Screen sdl = sdl_screen();

    CHECK(tui.cell_px == 0); // "my device unit IS the cell" -- every terminal's answer
    CHECK(sdl.cell_px == gfx::kCellPx);
    CHECK(chrome_grain(tui) == kChromePx);
    CHECK(chrome_grain(sdl) == gfx::kCellPx / gfx::kCellPx);
    CHECK(chrome_grain(sdl) < chrome_grain(tui));

    // AND IT IS EXACTLY ONE DEVICE UNIT ON EACH, said in the medium's own arithmetic
    // rather than by comparing constants: one grain reads back as one unit, and one less
    // than a grain reads back as none.
    CHECK(gfx::device_of_px(chrome_grain(sdl), sdl.cell_px) == 1);
    CHECK(gfx::device_of_px(chrome_grain(sdl) - 1, sdl.cell_px) == 0);
    CHECK(gfx::device_of_px(chrome_grain(tui), tui.cell_px) == 1); // ...one CELL

    // THE PANE ITSELF: one authored rectangle, two boundaries, and the outer rectangle is
    // the same object on both faces.
    const PixelRect outer = pixels_of_cells(CellRect{4, 3, 20, 9});
    const PaneInside on_tui = pane_inside(outer, tui);
    const PaneInside on_sdl = pane_inside(outer, sdl);
    CHECK(on_tui.chrome_px == kChromePx);
    CHECK(on_sdl.chrome_px == gfx::kCellPx / gfx::kCellPx);
    CHECK(cells_covered(on_tui.rect) == CellRect{5, 4, 18, 7}); // the cell-chrome picture, unmoved
    CHECK(on_sdl.rect.w > on_tui.rect.w);
    CHECK(on_sdl.rect.h > on_tui.rect.h);
}

TEST_CASE("the graphical boundary is one device pixel, drawn INSIDE the pane") {
    // ⚔ MUTATION: growing the pane outward to pay for its boundary, or drawing the ring on
    // the pixel outside it. The authored rectangle is the outer rectangle; the ring is
    // subtracted from it, and every number below is a pixel of the shipped face.
    const Screen sdl = sdl_screen();
    const PixelRect outer = pixels_of_cells(CellRect{4, 3, 20, 9});
    const PixelRect inside = pane_inside(outer, sdl).rect;

    const std::int64_t left = outer.x;
    const std::int64_t top = outer.y;
    const std::int64_t right = (gfx::add_cells(outer.x, outer.w));
    const std::int64_t bottom = (gfx::add_cells(outer.y, outer.h));

    CHECK(inside.x == left + 1);
    CHECK(inside.y == top + 1);
    CHECK((gfx::add_cells(inside.x, inside.w)) == right - 1);
    CHECK((gfx::add_cells(inside.y, inside.h)) == bottom - 1);

    // ...WHICH IS FOUR VISIBLE SIDES AND NOT AN OUTLINE AROUND NOTHING: the interior is
    // strictly inside the pane on both axes and still holds almost all of it.
    CHECK(inside.x > outer.x);
    CHECK(gfx::add_cells(inside.x, inside.w) < gfx::add_cells(outer.x, outer.w));
    CHECK(right - left == 20 * gfx::kCellPx);
    CHECK((gfx::add_cells(inside.x, inside.w)) -
              inside.x ==
          20 * gfx::kCellPx - 2);
}

TEST_CASE("the interior a window no longer reserves goes back to the pane") {
    // ⚔ MUTATION: painting a one-pixel line while the body, the room and the press inverse
    // keep spending a whole cell -- the "looks thin, acts thick" pane. The capacity below is
    // derived from the ACTUAL post-chrome pixels, so a body that kept the cell inset shows up
    // as fewer rows than the arithmetic says.
    const Screen sdl = sdl_screen();
    const PixelRect outer = pixels_of_cells(CellRect{4, 3, 20, 9});

    const ProsePlace thin = prose_place(outer, sdl);
    REQUIRE(thin.present);
    const std::int64_t px_h = 9 * gfx::kCellPx - 2; // the pane, less one px a side
    const std::int64_t px_w = 20 * gfx::kCellPx - 2;
    CHECK(thin.rows == (px_h - 2 * kTextInsetPx) / 18);
    CHECK(thin.columns == (px_w - 2 * kTextInsetPx) / 8);

    // ...AND IT IS STRICTLY MORE THAN THE SAME FACE WOULD HAVE PAYING A CELL: same authored
    // pane, same face, one honest boundary instead of a borrowed one.
    const PixelRect cell_inset = pane_interior(outer, kChromePx);
    const RegionFit was =
        fit_region(cell_inset.x, cell_inset.y, cell_inset.w, cell_inset.h,
                            sdl.text_advance_px, sdl.text_line_px);
    CHECK(thin.rows > was.rows);
    CHECK(thin.columns > was.columns);

    // AND NOTHING WAS PADDED BACK. ⚔ MUTATION: an invisible cell-sized reservation kept "so
    // the row counts do not move" -- the body would then be a whole cell narrower than the
    // pixels it was granted.
    CHECK(thin.inside.w == outer.w - 2 * chrome_grain(sdl));
    CHECK(thin.inside.h == outer.h - 2 * chrome_grain(sdl));
}

TEST_CASE("a face that describes an interior in CELLS pays the cell") {
    // THE HALF THAT MAKES THE THIN BOUNDARY HONEST: where a face projects a pane's interior
    // onto covered CELLS -- a terminal always, a window whose font never opened, a window whose
    // face is too tall for this pane -- an inset finer than a cell is projected away, and the
    // interior would spill over its own left and top edge. ⚔ MUTATION: `pane_inside` returning
    // the thin candidate unconditionally fails every symmetric-ring check below, on exactly
    // the faces that cannot draw the pixel.
    const PixelRect outer = pixels_of_cells(CellRect{4, 3, 20, 9});
    const CellRect box = cells_covered(outer);

    struct Face {
        const char* what;
        Screen sc;
    };
    const std::vector<Face> cell_faces{
        {"a terminal", tui_screen()},
        {"a window whose font never opened",
         screen_of(screen_session(kScreenMinW, kScreenMinH, 0, 0, gfx::kCellPx))},
    };
    for (const Face& face : cell_faces) {
        INFO(face.what);
        const PaneInside in = pane_inside(outer, face.sc);
        CHECK(in.chrome_px == kChromePx);
        const CellRect body = cells_covered(in.rect);
        CHECK(body.x == box.x + 1);
        CHECK(body.y == box.y + 1);
        CHECK(body.w == box.w - 2); // ...a ring on all FOUR sides, in the unit that face
        CHECK(body.h == box.h - 2); //    actually draws
    }

    // A WINDOW WITH A FACE TOO TALL FOR THIS PANE IS THE SAME SENTENCE, one pane at a time.
    // A three-cell pane is 36 device pixels; a 40-pixel line fits no row of type in it, so
    // this face describes THIS interior in cells and pays the cell for it -- while the same
    // window pays a pixel for a pane it can set in type.
    const Screen tall =
        screen_of(screen_session(kScreenMinW, kScreenMinH, 8, 40, gfx::kCellPx));
    const PixelRect small = pixels_of_cells(CellRect{4, 3, 20, 3});
    const PaneInside cramped = pane_inside(small, tall);
    CHECK_FALSE(cramped.fit.graphical());
    CHECK(cramped.chrome_px == kChromePx);
    const PaneInside roomy = pane_inside(pixels_of_cells(CellRect{4, 3, 20, 12}), tall);
    REQUIRE(roomy.fit.graphical());
    CHECK(roomy.chrome_px == chrome_grain(tall));
    CHECK(roomy.chrome_px < kChromePx);
}

TEST_CASE("the ring IS the backdrop the interior did not cover, on both faces") {
    // ⚔ MUTATION: a painter that strokes a border of its own. There is no thickness on
    // `paint_pane_frame` to get wrong -- it pushes the OUTER rect and the body's own
    // ground clears what it occupies -- so what a weaver sees is a subtraction rather than
    // a drawing, and it is the same subtraction the room and the press inverse spend.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const Screen tui = screen_of(t.session());
    const PixelRect outer =
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind, tui).rect;
    REQUIRE_FALSE(outer.empty());

    const ScreenCanvas c = paint(t.session());
    const CellRect box = cells_covered(outer);
    CHECK(has_rect(c, box.x, box.y, box.w, box.h, kPaneChrome)); // the whole pane, once

    // ON A TERMINAL the interior it publishes leaves a one-CELL ring...
    const CellRect body = pane_body_cells(outer);
    CHECK(body == CellRect{box.x + 1, box.y + 1, box.w - 2, box.h - 2});
    const std::vector<ScreenRegion> at = regions_at(c, body.x, body.y);
    REQUIRE(at.size() == 1);
    CHECK(covered_cells(at.front()) == body);

    // ...AND ON THE SHIPPED WINDOW the same authored pane leaves a one-PIXEL one, from the
    // same rect and the same subtraction. The region's fine origin is what a graphical
    // medium clips and fills to (`fit_region`), so this is the ring a weaver sees.
    const Screen sdl = sdl_screen();
    const PaneInside on_sdl = pane_inside(outer, sdl);
    const ScreenRect wire = wire_rect_of(on_sdl.rect, gfx::role::kFill);
    CHECK((wire.x) ==
          outer.x + 1);
    CHECK(on_sdl.fit.view.x == outer.x + 1);
    CHECK(on_sdl.fit.view.y == outer.y + 1);
    CHECK(on_sdl.fit.view.w == (gfx::add_cells(outer.x, outer.w)) -
                                   outer.x - 2);
    CHECK(on_sdl.fit.view.h == (gfx::add_cells(outer.y, outer.h)) -
                                   outer.y - 2);
}

TEST_CASE("selected and ordinary differ in INK, and in nothing else") {
    // ⚔ MUTATION: a painter that insets a SELECTED pane further, so its boundary reads
    // heavier. The pane's contents would then jump the moment a weaver pointed at it, which
    // is the one thing a selection must never do. The picture is compared field by field:
    // same backdrop rectangle, same published region, different ROLE.
    Live t;
    t.extent_on_window(160, 60);
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    REQUIRE(sc.cell_px == gfx::kCellPx);
    const PixelRect outer =
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect;
    REQUIRE_FALSE(outer.empty());
    const CellRect box = cells_covered(outer);
    const CellRect body = pane_body_cells(outer, sc);

    const auto picture = [&](const ScreenCanvas& c) {
        // `regions_at` keys on the region's own published corner, which on this face is the
        // pane's cell with a sub-cell remainder -- the cell the thin boundary shares with the
        // body it does not displace.
        const std::vector<ScreenRegion> at = regions_at(c, body.x, body.y);
        REQUIRE_FALSE(at.empty());
        return at.front();
    };

    REQUIRE(t.session().panes.selected == kNoPaneKind);
    const ScreenRegion ordinary = picture(paint(t.session()));
    CHECK(has_rect(paint(t.session()), box.x, box.y, box.w, box.h, kPaneChrome));

    t.press_canvas(box.x, box.y); // the pane's own border cell: choosing it, nothing else
    REQUIRE(t.session().panes.selected == stock::kKind);
    const ScreenRegion chosen = picture(paint(t.session()));

    // THE INK CHANGED...
    CHECK(has_rect(paint(t.session()), box.x, box.y, box.w, box.h, kPaneChromeSelected));
    CHECK_FALSE(has_rect(paint(t.session()), box.x, box.y, box.w, box.h, kPaneChrome));

    // ...AND NOT ONE NUMBER OF THE GEOMETRY DID, sub-cell remainders included -- which is
    // where a one-pixel change would hide.
    CHECK(chosen.x == ordinary.x);
    CHECK(chosen.y == ordinary.y);
    CHECK(chosen.w == ordinary.w);
    CHECK(chosen.h == ordinary.h);
    CHECK(chosen.rows.size() == ordinary.rows.size());

    // AND THE BOUNDARY IS STILL ONE PIXEL: the region begins one device pixel inside the
    // pane on both axes, selected or not.
    CHECK(chosen.x == outer.x + 1);
    CHECK(chosen.y == outer.y + 1);
}

TEST_CASE("a content-sized surface reserves the coarsest boundary, once") {
    // THE CONTEXTUAL POPUP asks for a rectangle to hold its rows INSIDE its chrome, and that
    // is ONE whole-cell `CellRect` shown on whichever face draws it -- so it reserves the
    // boundary the COARSEST face spends, and a graphical face hands the difference to the
    // popup's interior; a popup sized for pixels would cut a row off itself on a terminal.
    // ⚔ MUTATION: a medium-specific `chrome_outer_of`, making the popup's placement depend on
    // the face.
    const CellRect grown = cells_covered(chrome_outer_of(0, 0, cells_px(18), cells_px(7)));
    CHECK(grown.w == 18 + 2 * kChromeCells);
    CHECK(grown.h == 7 + 2 * kChromeCells);
    CHECK(cells_covered(pane_interior(pixels_of_cells(grown), kChromePx)) ==
          CellRect{1, 1, 18, 7}); // exact on the face that reserved it

    // AND THE POPUP'S RECTANGLE IS THE SAME RECTANGLE ON BOTH FACES. ⚔ MUTATION: anchoring
    // the contextual surface to a pane's BODY rather than to the pane.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const CellRect pane = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(pane.x + 1, pane.y + 1);
    REQUIRE(t.session().context.open);
    const PixelRect on_cells = context_bounds(t.session(), screen_of(t.session()));
    Session graphical = t.session();
    graphical.cell_px = gfx::kCellPx;
    graphical.text_advance_px = 8;
    graphical.text_line_px = 18;
    const PixelRect on_window = context_bounds(graphical, screen_of(graphical));
    // Beside the same press on both, each kept inside its own room: a face whose top band is
    // taller starts its room lower.
    CHECK(on_window.x == on_cells.x);
    CHECK(on_window.y == std::max(on_cells.y, screen_of(graphical).room_y));
}

// ---- two presses, one gesture ------------------------------------------------------------

TEST_CASE("what makes two presses on a tab one double-click, and what does not") {
    // THE QUALIFICATION IS PURE AND TOTAL, so every one of its conditions is stated here as an
    // ordinary comparison rather than raced against a stopwatch. ⚔ MUTATION: dropping any one
    // of them reddens exactly the subcase that names it. A tab's double-click is the one this
    // host reads.
    const TabClickMemory armed{true, 2, 1000};
    REQUIRE(armed.armed);

    SUBCASE("the ordinary case: same tab, soon enough") {
        CHECK(doubles_a_tab_click(armed, 2, 1000));
        CHECK(doubles_a_tab_click(armed, 2, 1399));
        CHECK(doubles_a_tab_click(armed, 2, 1000 + kDoubleClickMs)); // the boundary doubles
    }
    SUBCASE("too long apart is two presses") {
        CHECK_FALSE(doubles_a_tab_click(armed, 2, 1001 + kDoubleClickMs));
        CHECK_FALSE(doubles_a_tab_click(armed, 2, 99999));
    }
    SUBCASE("a different tab is an ordinary click") {
        CHECK_FALSE(doubles_a_tab_click(armed, 1, 1000));
        CHECK_FALSE(doubles_a_tab_click(armed, 3, 1000));
    }
    SUBCASE("nothing armed is nothing to double") {
        CHECK_FALSE(doubles_a_tab_click(TabClickMemory{}, 0, 0));
        TabClickMemory disarmed = armed;
        disarmed.armed = false;
        CHECK_FALSE(doubles_a_tab_click(disarmed, 2, 1000));
    }
    SUBCASE("a reading that went backwards is not a fast hand") {
        CHECK_FALSE(doubles_a_tab_click(armed, 2, 999));
    }
    SUBCASE("a fresh session has armed nothing") {
        Session fresh;
        CHECK_FALSE(fresh.tab_click.armed);
    }
}

// ---- the pane a gesture addressed -------------------------------------------------------

TEST_CASE("contextual Arrange lifts the pane it addressed, not the one in front") {
    // THE OVERLAP FALSIFIER, and a non-overlapping desk would not be evidence for it: the
    // Builder is authored UNDER the right-column pane and the two share cells, so "which pane
    // did this select" and "which pane was on top" are two answers before the gesture and must
    // be one after it.
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    // THE PANE IN THE RIGHT COLUMN IS AUTHORED: its occupant is a desk row.
    open_at_top_right(t, second::kKind);
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const CellRect side = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc).rect);
    REQUIRE(place_at_canvas(t, ref_of(stock::kKind),
                              gfx::px_of_cells(side.x - 4),
                              gfx::px_of_cells(side.y + 2))
                .accepted);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(send_to_back(desk, ref_of(stock::kKind)));
    });

    // BEFORE: the right-column pane owns the overlap, and a press there would reach it.
    const std::int64_t over_x = side.x + 1;
    const std::int64_t over_y = side.y + 3;
    const std::vector<std::int64_t> authored = authored_order(t.session());
    const std::string desk = setup_persist::to_text(t.session().setup.active);
    const std::vector<std::int64_t> ranks = ranks_of(t.session().setup.active);
    t.press_canvas(over_x, over_y);
    REQUIRE(t.session().panes.selected == second::kKind);
    REQUIRE(occupied_at(t.session().panes, t.session().setup.active, sc, over_x, over_y).kind ==
            second::kKind);
    const std::int64_t keyboard_before = t.session().panes.keyboard;

    // THE GESTURE: right-press where only the Builder is, and choose the first row.
    // ⚔ MUTATION: `spend_context_choice` re-hit-testing after the action instead of
    // spending the CAPTURED subject -- the topmost pane would be arranged.
    t.right_press_canvas(side.x - 3, side.y + 3);
    REQUIRE(t.menu().open);
    REQUIRE(t.menu().subject == context_subject::kPane);
    REQUIRE(t.menu().pane == ref_of(stock::kKind));
    REQUIRE(context_population(context_subject::kPane, "")[0].row->act == Act::kArrange);
    t.key(input::scan::kReturn);

    // AFTER: the addressed pane is the selected one, it is in front, and the hand reaches
    // what the eye sees. ⚔ MUTATION: `enter_arrange_pane` binding the scope without
    // writing `Panes::selected`.
    CHECK(t.session().arrange.open);
    CHECK_FALSE(t.session().arrange.desk);
    CHECK(t.session().arrange.pane == ref_of(stock::kKind));
    CHECK(t.session().panes.selected == stock::kKind);
    CHECK(selected_pane(t.session().panes) == stock::kKind);
    CHECK(painted_order(t.session()).back() == stock::kKind);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, screen_of(t.session()),
                      over_x, over_y)
              .kind == stock::kKind);

    // ...AND THE LIFT IS STILL A ROTATION. No rank moved, no authored byte changed, and a
    // save right now writes the desk it would have written before the gesture.
    CHECK(authored_order(t.session()) == authored);
    CHECK(ranks_of(t.session().setup.active) == ranks);
    CHECK(setup_persist::to_text(t.session().setup.active) == desk);

    // ...AND THE KEYBOARD CANDIDATE IS A SEPARATE FACT, untouched by an entrance that
    // happens to change the selection. ⚔ MUTATION: writing `panes.keyboard` here too.
    CHECK(t.session().panes.keyboard == keyboard_before);

    // THE ARRANGEMENT OPERATION ADDRESSES THE SAME PANE the selection names.
    const CellRect before = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    const CellRect info_before = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, second::kKind,
                  screen_of(t.session()))
            .rect);
    t.key(input::scan::kLeft);
    const CellRect after = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    CHECK(after.x < before.x);
    CHECK(cells_covered(bounds_of(t.session().panes, t.session().setup.active, second::kKind,
                                  screen_of(t.session()))
                            .rect)
              .x == info_before.x);
    CHECK(t.session().panes.selected == stock::kKind); // moving it did not lose it

    // LEAVING KEEPS THE SELECTION, because a selection is not a mode: it lives and dies by
    // the same rules an ordinary press's does.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().arrange.open);
    CHECK(t.session().panes.selected == stock::kKind);
    CHECK(painted_order(t.session()).back() == stock::kKind);
    CHECK(ranks_of(t.session().setup.active) == ranks);
}

TEST_CASE("every pane a weaver can point at can be arranged, and the refusals are blind") {
    // ADMISSION PRECEDES BINDING, and that includes the selection: a refusal that had quietly
    // re-selected something would have moved the desk while saying it changed nothing.
    // ⚔ MUTATION: selecting before `arrange_geometry_ready`. Two facts: every pane a weaver can
    // point at is arrangeable, and every refusal left belongs to a pane with no rectangle to
    // point at -- so the admission and the pointer cannot disagree.
    Live t;
    // THE PANE IN THE RIGHT COLUMN IS AUTHORED: its occupant is a desk row.
    open_at_top_right(t, second::kKind);
    open_pane(t, ref_of(stock::kKind));
    const CellRect slot = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(slot.x, slot.y);
    REQUIRE(t.session().panes.selected == stock::kKind);

    // ONE: Arrange on the right column's pane, through the weaver's own gesture, is accepted.
    const CellRect side = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, second::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(side.x + side.w - 1, side.y + 1);
    REQUIRE(t.menu().pane == ref_of(second::kKind));
    t.key(input::scan::kReturn);
    CHECK(t.session().arrange.open);
    CHECK(t.notice().find("reserved side column") == std::string::npos);

    // TWO: A PANE SIZED IN PIXELS IS A PANE LIKE ANY OTHER. Its pixels are presented, so its
    // cells answer the hand that reaches them.
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_size(desk, ref_of(second::kKind),
                                 PaneSize{pane_unit::kPixels, 300}, PaneSize{pane_unit::kDefault, 0})
                    .accepted);
    });
    const Screen sc = screen_of(t.session());
    const PaneBounds sized =
        bounds_of(t.session().panes, t.session().setup.active, second::kKind, sc);
    CHECK(sized.open);
    CHECK(sized.rect.w == 300);
    const CellRect sized_cells = cells_covered(sized.rect);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, sized_cells.x,
                      sized_cells.y)
              .kind == second::kKind);
}

TEST_CASE("the arrangement desk's pointer takes what is visibly in front") {
    // ⚔ THE MASK THIS CASE CLOSES: a desk pointer walk spending the AUTHORED order instead of
    // the effective one left every other arrangement case green, since each reached its pane
    // with no selection or with no pane over it. Only a LIFTED pane under the arrangement
    // pointer, in an overlap it does not authoredly own, tells the two orders apart.
    Live t;
    t.extent_px(cells_px(160), cells_px(44), 0, 0);
    open_pane(t, ref_of(second::kKind));
    open_pane(t, ref_of(stock::kKind));
    // THE SECOND PANE LEAVES THE STACK FIRST, and only then is the other pane's rectangle read:
    // a pane taken out of the composition lets the ones under it move up, so a rectangle
    // read before that is a rectangle about a desk that no longer exists.
    REQUIRE(place_at_canvas(t, ref_of(second::kKind),
                              gfx::px_of_cells(1), gfx::px_of_cells(30))
                .accepted);
    const PaneBounds second_at = bounds_of(t.session().panes, t.session().setup.active,
                                            stock::kKind, screen_of(t.session()));
    REQUIRE(second_at.open);
    const CellRect files = cells_covered(second_at.rect);
    REQUIRE(files.w > 4);
    REQUIRE(place_at_canvas(t, ref_of(second::kKind),
                              gfx::px_of_cells(files.x + 2),
                              gfx::px_of_cells(files.y + 2))
                .accepted);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(send_to_back(desk, ref_of(second::kKind)));
    });

    // AUTHORED: the second pane owns the overlap.
    const std::int64_t ox = files.x + 3;
    const std::int64_t oy = files.y + 3;
    REQUIRE(occupied_at(t.session().panes, t.session().setup.active, screen_of(t.session()),
                        ox, oy)
                .kind == stock::kKind);

    // SELECTED: a press on a strip only the Builder covers lifts it, and the lift reaches
    // the overlap -- which is the state every other arrangement case is missing.
    const CellRect builder = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, second::kKind,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + builder.w - 1, builder.y + builder.h - 1);
    REQUIRE(t.session().panes.selected == second::kKind);
    REQUIRE(occupied_at(t.session().panes, t.session().setup.active, screen_of(t.session()),
                        ox, oy)
                .kind == second::kKind);

    // ...AND THE ARRANGEMENT DESK'S OWN WALK AGREES WITH THE PICTURE. ⚠ The keyboard is put
    // back by hand, the one non-gesture here: every overlay-stack pane TAKES THE KEYBOARD, so
    // the lifting press points the keys at it, and `w` is a command-mode row; the weaver's way
    // back would clear `panes.selected`, the lift this case is about. Only the keyboard
    // address is reset -- the fact `w` reads and the walk below does not.
    live(t).panes.keyboard = kNoPaneKind;
    enter_arrange_desk(t);
    t.press_canvas(ox, oy);
    CHECK(t.session().arrange.pane == ref_of(second::kKind));
    CHECK(t.session().pane_drag.active);
    CHECK(t.session().pane_drag.pane == ref_of(second::kKind));
}

// ============================================================================
// ---- the layout tabs -------------------------------------------------------
// The run of desk arrangements this Workshop is holding, on the left of the band's
// existing status row. Everything here is composition: what the run says, what it does
// when it does not fit, and that the press inverse spends the painter's own spans.

namespace {

/// A `SetupState` holding `names.size()` layouts in that order, with `live` active.
SetupState shelf_of(const std::vector<std::string>& names, std::size_t live) {
    REQUIRE(!names.empty());
    REQUIRE(live < names.size());
    SetupState s;
    s.active = setup_of(names[live], {second::kKind});
    s.active_at = live;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != live) {
            s.shelved.push_back(Layout{setup_of(names[i], {second::kKind}), SetupLink{}});
        }
    }
    return s;
}

/// ...AS THE COPY A SESSION PRESENTS OF IT.
LiveDesk live_copy_of_shelf(const std::vector<std::string>& names, std::size_t live) {
    return live_copy_of(shelf_of(names, live));
}

/// The weaver's order, read back out of the run -- the one thing switching may never move.
std::vector<std::string> order_of(const SetupState& s) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < layout_count(s); ++i) {
        out.push_back(layout_at(s, i).name);
    }
    return out;
}

} // namespace

TEST_CASE("a layout is a Setup and the run is the shelf plus the live value") {
    SetupState s = shelf_of({"Code", "Build", "Inspect"}, 0);
    CHECK(layout_count(s) == 3);
    CHECK(order_of(s) == std::vector<std::string>{"Code", "Build", "Inspect"});
    // THE LIVE ONE IS `active` AND IS NOT ALSO ON THE SHELF -- one value, one owner.
    CHECK(s.shelved.size() == 2);
    CHECK(&layout_at(s, 0) == &s.active);
    for (const Layout& shelved : s.shelved) {
        CHECK(shelved.desk.name != s.active.name);
    }
}

TEST_CASE("switching never reorders the run, and the live value never doubles") {
    SetupState s = shelf_of({"Code", "Build", "Inspect"}, 0);
    const std::vector<std::string> authored = order_of(s);

    // EVERY DESTINATION, IN EVERY ORDER, AND THE ORDER IS THE SAME AFTERWARDS. A swap of
    // `active` with the shelf entry would pass the first hop and reorder on the second,
    // which is exactly the mutation this sweep exists to kill.
    for (const std::size_t to : std::vector<std::size_t>{2, 1, 0, 1, 2, 0}) {
        CAPTURE(to);
        REQUIRE(activate_layout(s, to));
        CHECK(s.active_at == to);
        CHECK(s.active.name == authored[to]);
        CHECK(order_of(s) == authored);
        CHECK(s.shelved.size() == 2);
    }
    // AND A DESTINATION THAT IS ALREADY LIVE, OR IS NOT A LAYOUT, MOVES NOTHING.
    CHECK_FALSE(activate_layout(s, s.active_at));
    CHECK_FALSE(activate_layout(s, 3));
    CHECK(order_of(s) == authored);
}

TEST_CASE("new is BLANK and appended, and it is a value of its own") {
    // NEW MEANS NEW: copying is what `duplicate_layout` is for, and a weaver asking for a new
    // desk is asking for an empty one.
    SetupState s = shelf_of({"Code"}, 0);
    REQUIRE(add_pane(s.active, ref_of(stock::kKind)));
    const Setup original = s.active;

    REQUIRE(add_layout(s));
    // A FRESH DEFAULT DESK, APPENDED, AND LIVE.
    CHECK(layout_count(s) == 2);
    CHECK(s.active_at == 1);
    CHECK(s.active == default_setup());
    CHECK_FALSE(has_pane(s.active, ref_of(stock::kKind)));
    CHECK(order_of(s) == std::vector<std::string>{"Code", default_setup().name});
    // ...and the layout it was made from is untouched.
    CHECK(layout_at(s, 0) == original);
    // ITS ASSOCIATION IS NONE: a desk that has just been made has never been written to or
    // read from any artifact.
    CHECK(link_status(s.active, s.active_link) == setup_link::kNone);

    // THE CEILING IS A BOUND ON WORK AND IT REFUSES RATHER THAN DROPPING ANYTHING.
    while (layout_count(s) < kMaxLayouts) {
        REQUIRE(add_layout(s));
    }
    CHECK(layout_count(s) == kMaxLayouts);
    CHECK_FALSE(add_layout(s));
    CHECK(layout_count(s) == kMaxLayouts);
}

TEST_CASE("a new layout appends however far into the run you stand") {
    // FOUND BY A MUTATION: `add_layout` puts the ORIGINAL back at `active_at` and takes the
    // appended position; a `push_back` of the original is identical while the live layout is
    // LAST -- where every other case here stands, because that is where `new` leaves you --
    // and swaps two layouts the moment a weaver adds one from the middle of their own run.
    const std::string blank = default_setup().name;
    SetupState s = shelf_of({"A", "B", "C"}, 1);
    REQUIRE(s.active.name == "B");
    REQUIRE(add_layout(s));
    CHECK(order_of(s) == std::vector<std::string>{"A", "B", "C", blank});
    CHECK(s.active_at == 3);
    CHECK(s.active.name == blank);

    // ...from the FIRST position too, which is the other end of the same mistake.
    SetupState first = shelf_of({"A", "B", "C"}, 0);
    REQUIRE(add_layout(first));
    CHECK(order_of(first) == std::vector<std::string>{"A", "B", "C", blank});
    CHECK(first.active_at == 3);

    // ...and the layout a weaver was STANDING on goes back on its own position with every
    // byte of it intact -- which is the half a `push_back` of the live value gets wrong.
    SetupState middle = shelf_of({"A", "B", "C"}, 1);
    REQUIRE(add_pane(middle.active, ref_of(stock::kKind)));
    const Setup standing = middle.active;
    REQUIRE(add_layout(middle));
    CHECK(layout_at(middle, 1) == standing);
    CHECK_FALSE(has_pane(layout_at(middle, 0), ref_of(stock::kKind)));
    CHECK_FALSE(has_pane(middle.active, ref_of(stock::kKind)));
}

TEST_CASE("removing takes the next neighbour, the previous only at the end") {
    SetupState s = shelf_of({"A", "B", "C"}, 0);
    REQUIRE(remove_layout(s, s.active_at)); // the first: the NEXT one becomes live
    CHECK(s.active.name == "B");
    CHECK(s.active_at == 0);
    CHECK(order_of(s) == std::vector<std::string>{"B", "C"});

    SetupState last = shelf_of({"A", "B", "C"}, 2);
    REQUIRE(remove_layout(last, last.active_at)); // the last: there is no next, so the PREVIOUS one
    CHECK(last.active.name == "B");
    CHECK(last.active_at == 1);
    CHECK(order_of(last) == std::vector<std::string>{"A", "B"});

    // THE FLOOR: the only layout cannot be removed, and nothing about it moves.
    SetupState one = shelf_of({"A"}, 0);
    CHECK_FALSE(remove_layout(one, one.active_at));
    CHECK(layout_count(one) == 1);
    CHECK(one.active.name == "A");
}

// ---- the value operations the tab gestures spend ------------------------------------------

namespace {

/// A shelf whose live layout is associated with `path`, its known value being whatever the
/// live desk is right now -- the state a successful `s` leaves behind.
SetupState linked_shelf(const std::vector<std::string>& names, std::size_t live,
                        const std::string& path) {
    SetupState s = shelf_of(names, live);
    s.active_link = SetupLink{path, s.active};
    return s;
}

} // namespace

TEST_CASE("duplicate copies the desk exactly and always clears the association") {
    // THE ONE MANDATORY LAW ABOUT COPYING. An inherited association would have the copy
    // claim an artifact it has never been written to, and the first `s` would overwrite the
    // very file the weaver duplicated in order not to touch.
    SetupState s = linked_shelf({"Home", "Code", "Art"}, 1, "/w/code.json");
    REQUIRE(add_pane(s.active, ref_of(stock::kKind)));
    s.active_link.known = s.active; // saved again after the edit: `current`
    REQUIRE(link_status(s.active, s.active_link) == setup_link::kCurrent);
    const Setup source = s.active;

    REQUIRE(duplicate_layout(s, 1));

    // THE DESK IS COPIED WHOLE -- the name included, because duplicate names are legal and
    // position is a layout's identity. Inventing `Code (copy)` would be this file authoring
    // a weaver's word for them.
    CHECK(layout_count(s) == 4);
    CHECK(s.active_at == 2); // directly after the source, and live
    CHECK(s.active == source);
    CHECK(s.active.name == "Code");
    CHECK(has_pane(s.active, ref_of(stock::kKind)));
    CHECK(order_of(s) == std::vector<std::string>{"Home", "Code", "Code", "Art"});
    // ...AND THE ASSOCIATION IS GONE.
    CHECK(link_status(s.active, s.active_link) == setup_link::kNone);
    CHECK(s.active_link.path.empty());
    // THE SOURCE KEPT ITS OWN, untouched.
    CHECK(link_at(s, 1).path == "/w/code.json");
    CHECK(link_status(layout_at(s, 1), link_at(s, 1)) == setup_link::kCurrent);
    // AND EVERY OTHER LAYOUT IS WHERE IT WAS.
    CHECK(layout_at(s, 0).name == "Home");
    CHECK(layout_at(s, 3).name == "Art");

    // DUPLICATING AN INACTIVE TAB copies THAT tab, not the live one.
    SetupState other = linked_shelf({"Home", "Code", "Art"}, 1, "/w/code.json");
    REQUIRE(duplicate_layout(other, 0));
    CHECK(other.active_at == 1);
    CHECK(other.active.name == "Home");
    CHECK(order_of(other) == std::vector<std::string>{"Home", "Home", "Code", "Art"});
    CHECK(link_status(other.active, other.active_link) == setup_link::kNone);

    // THE CEILING REFUSES A DUPLICATE WITHOUT DROPPING ANYTHING.
    SetupState full = shelf_of({"A", "B", "C", "D", "E", "F", "G", "H"}, 0);
    REQUIRE(layout_count(full) == kMaxLayouts);
    const std::vector<std::string> before = order_of(full);
    CHECK_FALSE(duplicate_layout(full, 3));
    CHECK_FALSE(add_layout(full));
    CHECK(order_of(full) == before);
    CHECK(layout_count(full) == kMaxLayouts);
    // ...and a position that is not a layout is refused too, whatever the count.
    SetupState room = shelf_of({"A"}, 0);
    CHECK_FALSE(duplicate_layout(room, 1));
    CHECK(layout_count(room) == 1);
}

TEST_CASE("rename writes one layout's name and touches nothing else") {
    SetupState s = linked_shelf({"Home", "Code", "Art"}, 1, "/w/code.json");
    const Setup was = s.active;
    const std::vector<std::string> shelf_before = order_of(s);

    // THE LIVE ONE, BY POSITION.
    REQUIRE(rename_layout(s, 1, "Build"));
    CHECK(s.active.name == "Build");
    CHECK(s.active.panes == was.panes); // only the name moved
    CHECK(order_of(s) == std::vector<std::string>{"Home", "Build", "Art"});
    // AND THE ASSOCIATION IS NOT REPLACED OR DETACHED -- it is the same artifact, and the
    // desk has simply diverged from the value that artifact holds. `link_status` DERIVES
    // that; nothing was remembered.
    CHECK(s.active_link.path == "/w/code.json");
    CHECK(link_status(s.active, s.active_link) == setup_link::kModified);
    // ...and renaming BACK reads `current` again, which is what a comparison buys and a
    // flag could not.
    REQUIRE(rename_layout(s, 1, "Code"));
    CHECK(link_status(s.active, s.active_link) == setup_link::kCurrent);

    // AN INACTIVE ONE, without switching to it: the live desk never moves.
    REQUIRE(rename_layout(s, 2, "Gallery"));
    CHECK(s.active_at == 1);
    CHECK(s.active == was);
    CHECK(order_of(s) == std::vector<std::string>{"Home", "Code", "Gallery"});
    CHECK(shelf_before.size() == 3);

    // AND A POSITION THAT IS NOT A LAYOUT IS REFUSED, with nothing moved.
    const std::vector<std::string> now = order_of(s);
    CHECK_FALSE(rename_layout(s, 3, "Nowhere"));
    CHECK(order_of(s) == now);
}

TEST_CASE("moving a layout changes order and nothing else") {
    // EVERY POSITION TO EVERY OTHER, SWEPT. `move_layout` goes through the inverse pair,
    // so what this measures is that the run a weaver sees is exactly the run with one
    // element moved -- and that the layout that WAS live is still live afterwards, at
    // wherever it now sits.
    const std::vector<std::string> names{"A", "B", "C", "D"};
    for (std::size_t live = 0; live < names.size(); ++live) {
        for (std::size_t from = 0; from < names.size(); ++from) {
            for (std::size_t to = 0; to < names.size(); ++to) {
                if (from == to) {
                    continue;
                }
                CAPTURE(live);
                CAPTURE(from);
                CAPTURE(to);
                SetupState s = shelf_of(names, live);
                // Give every layout a distinguishable association, so a move that dropped
                // or swapped one is visible rather than silently equal.
                s.active_link = SetupLink{"/w/" + names[live] + ".json", s.active};
                for (std::size_t i = 0; i < s.shelved.size(); ++i) {
                    s.shelved[i].link =
                        SetupLink{"/w/" + s.shelved[i].desk.name + ".json", s.shelved[i].desk};
                }
                const std::string standing = s.active.name;

                REQUIRE(move_layout(s, from, to));

                // THE ORDER IS THE VECTOR MOVE, spelled independently of the operation.
                std::vector<std::string> want = names;
                const std::string moved = want[from];
                want.erase(want.begin() + static_cast<std::ptrdiff_t>(from));
                want.insert(want.begin() + static_cast<std::ptrdiff_t>(to), moved);
                CHECK(order_of(s) == want);
                CHECK(layout_count(s) == names.size());
                // THE SAME DESK IS STILL LIVE, wherever it went.
                CHECK(s.active.name == standing);
                CHECK(layout_at(s, s.active_at).name == standing);
                // AND EVERY ASSOCIATION TRAVELLED WITH ITS OWN DESK.
                for (std::size_t at = 0; at < layout_count(s); ++at) {
                    CAPTURE(at);
                    CHECK(link_at(s, at).path == "/w/" + layout_at(s, at).name + ".json");
                    CHECK(link_status(layout_at(s, at), link_at(s, at)) ==
                          setup_link::kCurrent);
                }
            }
        }
    }

    // NOTHING MOVED: a position that is not a layout, or a move to where it already is.
    SetupState s = shelf_of({"A", "B"}, 0);
    CHECK_FALSE(move_layout(s, 0, 0));
    CHECK_FALSE(move_layout(s, 2, 0));
    CHECK_FALSE(move_layout(s, 0, 2));
    CHECK(order_of(s) == std::vector<std::string>{"A", "B"});
    CHECK(s.active_at == 0);
}

TEST_CASE("closing an inactive tab leaves the live desk exactly where it was") {
    SetupState s = linked_shelf({"Home", "Code", "Art", "Notes"}, 2, "/w/art.json");
    const Setup live = s.active;

    // A TAB BEFORE THE LIVE ONE: the live desk is untouched and its position slides down.
    REQUIRE(remove_layout(s, 0));
    CHECK(s.active == live);
    CHECK(s.active_at == 1);
    CHECK(s.active_link.path == "/w/art.json");
    CHECK(order_of(s) == std::vector<std::string>{"Code", "Art", "Notes"});

    // A TAB AFTER IT: the position does not move either.
    REQUIRE(remove_layout(s, 2));
    CHECK(s.active == live);
    CHECK(s.active_at == 1);
    CHECK(order_of(s) == std::vector<std::string>{"Code", "Art"});

    // THE ASSOCIATION DIES WITH THE LAYOUT AND ONLY WITH IT.
    SetupState two = shelf_of({"One", "Two"}, 0);
    two.active_link = SetupLink{"/w/one.json", two.active};
    two.shelved[0].link = SetupLink{"/w/two.json", two.shelved[0].desk};
    REQUIRE(remove_layout(two, 1));
    CHECK(layout_count(two) == 1);
    CHECK(two.active.name == "One");
    CHECK(two.active_link.path == "/w/one.json"); // the survivor kept its own
}

TEST_CASE("switching carries the association, sharing keeps it honest") {
    // TWO LAYOUTS, ONE ARTIFACT -- which is legal and must stay honest.
    SetupState s = shelf_of({"Wide", "Narrow"}, 0);
    s.active_link = SetupLink{"/w/desk.json", s.active};
    s.shelved[0].link = SetupLink{"/w/desk.json", s.active}; // both know the same value
    REQUIRE(link_status(s.active, s.active_link) == setup_link::kCurrent);
    REQUIRE(link_status(layout_at(s, 1), link_at(s, 1)) == setup_link::kModified);

    // SWITCHING SWITCHES WHICH ASSOCIATION IS PRESENTED, and nothing else about either.
    REQUIRE(activate_layout(s, 1));
    CHECK(s.active.name == "Narrow");
    CHECK(s.active_link.path == "/w/desk.json");
    CHECK(link_status(s.active, s.active_link) == setup_link::kModified);
    CHECK(link_status(layout_at(s, 0), link_at(s, 0)) == setup_link::kCurrent);

    // WRITING THE ARTIFACT TEACHES EVERY ASSOCIATION TO IT. Without the sweep the first
    // layout would go on claiming `current` against bytes this write just replaced, which is a
    // status wrong about the only thing it is for.
    adopt_known_setup(s, "/w/desk.json", s.active);
    CHECK(link_status(s.active, s.active_link) == setup_link::kCurrent);
    CHECK(link_status(layout_at(s, 0), link_at(s, 0)) == setup_link::kModified);

    // ...AND IT ESTABLISHES NOTHING. A layout with no association, or one associated with
    // another artifact, is not touched.
    SetupState mixed = shelf_of({"Bare", "Other"}, 0);
    mixed.shelved[0].link = SetupLink{"/w/other.json", mixed.shelved[0].desk};
    adopt_known_setup(mixed, "/w/desk.json", mixed.active);
    CHECK(mixed.active_link.path.empty());
    CHECK(link_at(mixed, 1).path == "/w/other.json");
    CHECK(link_status(layout_at(mixed, 1), link_at(mixed, 1)) == setup_link::kCurrent);
    // An empty path is not an artifact and sweeps nothing.
    adopt_known_setup(mixed, "", setup_of("Ghost", {}));
    CHECK(mixed.active_link.known == Setup{});
}

TEST_CASE("the three verdicts, and what makes a fresh desk `none`") {
    // `none` DOES NOT MEAN UNSAVED. A layout with no association is safely persisted by
    // the session; what it has not got is an explicit standalone Setup artifact.
    const Setup desk = setup_of("Code", {second::kKind});
    CHECK(link_status(desk, SetupLink{}) == setup_link::kNone);
    CHECK(link_status(desk, SetupLink{"/w/code.json", desk}) == setup_link::kCurrent);
    CHECK(link_status(desk, SetupLink{"/w/code.json", setup_of("Other", {second::kKind})}) ==
          setup_link::kModified);
    // AND A DEFAULT-CONSTRUCTED KNOWN VALUE IS STRUCTURALLY UNREACHABLE AS A DESK: no
    // legal setup equals it, because `check_setup_name` refuses an empty name -- which is
    // what makes "there is no known value" structural rather than a rule somebody keeps.
    CHECK_FALSE(check_setup_name(Setup{}.name).accepted);
    CHECK(link_status(desk, SetupLink{"/w/code.json", Setup{}}) == setup_link::kModified);
}

TEST_CASE("stepping wraps over the whole population, painted or not") {
    SetupState s = shelf_of({"A", "B", "C"}, 0);
    CHECK(layout_step(s, +1) == 1);
    CHECK(layout_step(s, -1) == 2); // wraps
    REQUIRE(activate_layout(s, 2));
    CHECK(layout_step(s, +1) == 0); // wraps
    CHECK(layout_step(s, -1) == 1);
    // One layout steps to itself rather than off the end.
    SetupState one = shelf_of({"A"}, 0);
    CHECK(layout_step(one, +1) == 0);
    CHECK(layout_step(one, -1) == 0);
}

TEST_CASE("the run marks the live layout and its width does not move") {
    const SetupState first = shelf_of({"Code", "Build"}, 0);
    const SetupState second = shelf_of({"Code", "Build"}, 1);
    const LayoutTabRun a = layout_tab_run(first, 80);
    const LayoutTabRun b = layout_tab_run(second, 80);

    // THE MARKER BRACKETS THE LIVE NAME AND IS TIGHT TO IT, and every other tab wears the same
    // two cells as blanks -- so the run reads as a run of names -- followed by the create
    // affordance, out of what the budget had left.
    CHECK(a.text == ">Code< Build  +");
    CHECK(b.text == " Code >Build< +");
    // NO QUOTATION MARK ANYWHERE IN IT: the authored bytes, and nothing a weaver did not
    // type.
    CHECK(a.text.find('"') == std::string::npos);
    CHECK(b.text.find('"') == std::string::npos);
    // ...AND NO SPACE BETWEEN A MARKER AND THE NAME IT IS ABOUT. The cell before the live
    // name is `>` and the cell after it is `<`, with nothing in between.
    CHECK(a.text.find("> ") == std::string::npos);
    CHECK(a.text.find(" <") == std::string::npos);
    CHECK(b.text.find("> ") == std::string::npos);
    CHECK(b.text.find(" <") == std::string::npos);
    // THE SAME WIDTH EITHER WAY, which is why the status to the right of it does not slide
    // sideways every time a weaver switches. It is one cell, the name, one cell, on both -- so
    // the equality is the TYPE's rather than two literals' agreement (`kLayoutLiveOpen` and its
    // neighbours are `char`).
    CHECK(a.text.size() == b.text.size());
    CHECK(a.text.size() == std::string("Code").size() + std::string("Build").size() + 4 +
                               static_cast<std::size_t>(kLayoutCreateCols));
    REQUIRE(a.tabs.size() == 2);
    REQUIRE(b.tabs.size() == 2);
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(a.tabs[i].at == i);
        CHECK(a.tabs[i].column == b.tabs[i].column);
        CHECK(a.tabs[i].columns == b.tabs[i].columns);
    }
    CHECK(a.tabs[0].active);
    CHECK_FALSE(a.tabs[1].active);
    CHECK_FALSE(b.tabs[0].active);
    CHECK(b.tabs[1].active);
    // A RUN THAT FITS IS PAINTED WHOLE, with no marker at all.
    CHECK(a.before == 0);
    CHECK(a.after == 0);
    // AND THE CREATE AFFORDANCE IS ONE CELL WITH A SPAN OF ITS OWN -- an action, never a
    // layout: it is not in `tabs`, not counted, and not steppable.
    CHECK(a.create_columns == 1);
    CHECK(a.create_column == b.create_column);
    CHECK(a.text.substr(static_cast<std::size_t>(a.create_column), 1) ==
          std::string(1, kLayoutCreate));
}

TEST_CASE("a multi-word name is delimited by its own cells, not by quotes") {
    // A layout name may hold spaces, so the delimiter is paid in GEOMETRY: every tab reserves
    // one cell on each side of its name, so the gap BETWEEN two tabs is two cells and a space
    // INSIDE a name is one.
    const SetupState s = shelf_of({"my desk", "other"}, 0);
    const LayoutTabRun run = layout_tab_run(s, 80);
    CHECK(run.text == ">my desk< other  +");
    CHECK(run.text.find('"') == std::string::npos);
    REQUIRE(run.tabs.size() == 2);
    // THE SPAN IS THE DELIMITER, and it holds the whole tab: both marker cells and the
    // authored bytes between them, for the live tab and the shelved one alike.
    CHECK(run.text.substr(static_cast<std::size_t>(run.tabs[0].column),
                          static_cast<std::size_t>(run.tabs[0].columns)) == ">my desk<");
    CHECK(run.text.substr(static_cast<std::size_t>(run.tabs[1].column),
                          static_cast<std::size_t>(run.tabs[1].columns)) == " other ");
    // ...AND THE AUTHORED NAME SITS ONE CELL INSIDE ITS OWN SPAN, which is what keeps a
    // bare run recoverable at all: the row's bytes alone cannot say where a name with a
    // space in it ends, and the composition's own arithmetic can.
    CHECK(run.text.find("my desk") == static_cast<std::size_t>(run.tabs[0].column) + 1);
    // THE WEAVER'S OWN EXAMPLE, whole: an active multi-word name between two ordinary ones.
    const SetupState three = shelf_of({"Home", "My Layout", "Art"}, 1);
    CHECK(layout_tab_run(three, 80).text == " Home >My Layout< Art  +");
    // ...AND DUPLICATE NAMES ARE LEGAL AND DISAMBIGUATED BY POSITION, never by the text.
    const SetupState twins = shelf_of({"same", "same"}, 1);
    const LayoutTabRun two = layout_tab_run(twins, 80);
    REQUIRE(two.tabs.size() == 2);
    CHECK(two.tabs[0].at == 0);
    CHECK(two.tabs[1].at == 1);
    CHECK(two.tabs[0].column != two.tabs[1].column);
}

TEST_CASE("the visible window is derived, keeps the live tab, and marks its ends") {
    SetupState s = shelf_of({"A", "B", "C", "D", "E", "F", "G", "H"}, 0);
    const std::vector<std::string> authored = order_of(s);

    // TOO MANY FOR THE ROOM: the omissions are counted on the side they were left out on,
    // the live tab is painted, and the order inside the window is the authored order.
    const LayoutTabRun narrow = layout_tab_run(s, 20);
    REQUIRE(!narrow.tabs.empty());
    CHECK(narrow.tabs.front().active);
    CHECK(narrow.before == 0);
    CHECK(narrow.after == 8 - narrow.tabs.size());
    CHECK(narrow.after > 0);
    // THE HONEST RIGHT-HAND COUNT, asked for by its own wording rather than by the glyph: the
    // live tab carries a `>` of its own, so a bare search for one would pass with no omission
    // marker on the row at all.
    CHECK(narrow.text.find(layouts_omitted_text(narrow.after, true)) != std::string::npos);
    CHECK(static_cast<std::int64_t>(narrow.text.size()) <= 20);
    for (std::size_t i = 0; i < narrow.tabs.size(); ++i) {
        CHECK(narrow.tabs[i].at == i);
    }

    // THE WINDOW FOLLOWS THE LIVE LAYOUT, and nothing is stored: the same state, asked
    // again after a switch, answers a different window over the SAME order.
    REQUIRE(activate_layout(s, 7));
    const LayoutTabRun moved = layout_tab_run(s, 20);
    CHECK(order_of(s) == authored);
    CHECK(moved.tabs.back().active);
    CHECK(moved.tabs.back().at == 7);
    CHECK(moved.before > 0);
    CHECK(moved.after == 0);
    CHECK(moved.text.find("<") == 0); // the honest left-hand count leads the run

    // ...AND IN THE MIDDLE BOTH ENDS ARE MARKED. ⚠ The width is derived for this tab spelling:
    // a one-letter layout costs three cells (` A `), so eighteen columns hold the whole run
    // and mark neither end; twelve is where both markers are owed.
    REQUIRE(activate_layout(s, 4));
    CHECK(layout_tab_run(s, 18).after == 0); // eighteen columns hold the whole run
    const LayoutTabRun middle = layout_tab_run(s, 12);
    CHECK(middle.before > 0);
    CHECK(middle.after > 0);
    bool live_painted = false;
    for (const LayoutTab& tab : middle.tabs) {
        live_painted = live_painted || tab.active;
    }
    CHECK(live_painted);
    // EVERY PAINTED SPAN IS INSIDE THE TEXT AND THEY DO NOT OVERLAP.
    std::int64_t reach = 0;
    for (const LayoutTab& tab : middle.tabs) {
        CHECK(tab.column >= reach);
        reach = tab.column + tab.columns;
        CHECK(reach <= static_cast<std::int64_t>(middle.text.size()));
    }
}

TEST_CASE("the live tab is cut rather than dropped when even it will not fit") {
    const SetupState s = shelf_of({"short", std::string(kMaxSetupNameLen, 'z')}, 1);
    const LayoutTabRun run = layout_tab_run(s, 12);
    REQUIRE(run.tabs.size() == 1);
    CHECK(run.tabs[0].active);
    CHECK(run.tabs[0].at == 1);
    CHECK(run.before == 1);
    CHECK(static_cast<std::int64_t>(run.text.size()) <= 12);
    CHECK(run.text.find("...") != std::string::npos); // the cut is marked, never silent
}

TEST_CASE("the status row is tabs on the left and the existing status right") {
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.setup = live_copy_of_shelf({"Code", "Build"}, 0);
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);

    // THE TABS, THE CREATE AFFORDANCE, THEN THE ACTIVE LAYOUT'S OWN ASSOCIATION -- not one
    // comparison for a whole Workshop against one file, on a screen showing several desks.
    CHECK(row.text.rfind(">Code< Build  +", 0) == 0);
    CHECK(row.text.find("setup: none") != std::string::npos);
    // ...AND THE HOST'S CONFIGURED SETUP PATH IS NOT ON THE ROW. This layout is related to
    // no artifact, and `none` is the whole truth about it.
    CHECK(row.text.find("workshop-setup.json") == std::string::npos);
    CHECK(row.text.find("UNSAVED") == std::string::npos);
    CHECK(static_cast<std::int64_t>(row.text.size()) <= cells_of(sc).w);
    // THE NAME IS SAID ONCE. The tabs carry it; the status half does not repeat it.
    CHECK(row.text.find("setup ") == std::string::npos);
    REQUIRE(row.tabs.size() == 2);
    CHECK(row.text.find("Code", static_cast<std::size_t>(row.tabs[0].column +
                                                         row.tabs[0].columns)) ==
          std::string::npos);
    // AND THE GEOMETRY DID NOT MOVE TO SEAT THEM: the band is the rows it always was.
    CHECK(band_bounds(sc).h == cells_px(kBottomRows));
    CHECK(cells_of(sc).room_w == cells_of(kMinScreen).room_w);
    CHECK(cells_of(sc).room_h == cells_of(kMinScreen).room_h);

    // THE ASSOCIATION IS STILL ABOUT THE LIVE LAYOUT, and it survives a run of names long
    // enough to want the whole row.
    Session crowded = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    crowded.setup = live_copy_of_shelf({std::string(20, 'a'), std::string(20, 'b'),
                              std::string(20, 'c'), std::string(20, 'd')},
                             0);
    const BandStatus tight = band_status(crowded, screen_of(crowded));
    CHECK(tight.text.find("setup: none") != std::string::npos);
    CHECK(static_cast<std::int64_t>(tight.text.size()) <= cells_of(kMinScreen).w);
}

TEST_CASE("the association's verdict survives the row's cut, at every width") {
    // FOUND BY THE LIVE TUI WITNESS, NOT BY A CASE. The reservation is against the tabs, and
    // its price includes the mark `detail::fit` spends on saying it cut the row: stopping at
    // the last real character leaves the verdict three cells short, and a weaver at the minimum
    // extent reads `| modifi...`. What must survive is the meaning (`setup:` and the verdict);
    // WHICH artifact a narrow row may elide. Swept over every name length, because the defect
    // lives at exactly one of them.
    const std::string artifact = "/home/weaver/projects/zen/layouts/workshop-setup.json";
    for (std::size_t count = 1; count <= kMaxLayouts; ++count) {
        for (std::size_t len = 1; len <= 24; ++len) {
            std::vector<std::string> names;
            for (std::size_t i = 0; i < count; ++i) {
                names.push_back(std::string(len, static_cast<char>('a' + i)));
            }
            for (std::size_t live = 0; live < count; ++live) {
                CAPTURE(count);
                CAPTURE(len);
                CAPTURE(live);
                Session none = screen_session(kScreenMinW, kScreenMinH, 0, 0);
                none.setup = live_copy_of_shelf(names, live);
                const BandStatus bare = band_status(none, screen_of(none));
                REQUIRE(bare.text.find("setup: none") != std::string::npos);
                REQUIRE(static_cast<std::int64_t>(bare.text.size()) <= cells_of(kMinScreen).w);

                // ...and the two associated verdicts, whose sentence is the longer one.
                Session live_current = none;
                live_current.setup.link_path = artifact;
                live_current.setup.link = setup_link::kCurrent;
                const BandStatus fresh = band_status(live_current, screen_of(live_current));
                REQUIRE(fresh.text.find("setup: ") != std::string::npos);
                REQUIRE(fresh.text.find("| current") != std::string::npos);
                REQUIRE(static_cast<std::int64_t>(fresh.text.size()) <= cells_of(kMinScreen).w);

                Session diverged = none;
                diverged.setup.link_path = artifact;
                diverged.setup.link = setup_link::kModified;
                const BandStatus moved = band_status(diverged, screen_of(diverged));
                REQUIRE(moved.text.find("setup: ") != std::string::npos);
                REQUIRE(moved.text.find("| modified") != std::string::npos);
                REQUIRE(static_cast<std::int64_t>(moved.text.size()) <= cells_of(kMinScreen).w);
            }
        }
    }
}

TEST_CASE("a press answers a painted tab and nothing else on the band") {
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.setup = live_copy_of_shelf({"Code", "Build", "Inspect"}, 0);
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);
    // ⚠ THE TOP BAND, because that is where the run is painted. A press resolved against the
    // bottom band's origin would be the stale-geometry lie this case exists to refuse.
    const CellRect b = cells_covered(top_band_bounds(sc));
    REQUIRE(b.y == 0);
    REQUIRE(band_tab_row(s, sc) == 0);
    REQUIRE(row.tabs.size() == 3);

    const auto press = [&](std::int64_t column, std::int64_t band_row) {
        return band_tab_at(s, sc, input::space::kCells,
                           b.x + column, b.y + band_row);
    };

    // EVERY CELL OF EVERY PAINTED TAB ANSWERS THAT TAB -- the span the composition wrote,
    // never a second measure of the same text.
    for (const LayoutTab& tab : row.tabs) {
        for (std::int64_t c = tab.column; c < tab.column + tab.columns; ++c) {
            CAPTURE(c);
            const LayoutTabPress hit = press(c, 0);
            REQUIRE(hit.hit);
            CHECK(hit.at == tab.at);
        }
    }
    // THE STATUS TO THE RIGHT OF THE RUN SELECTS NOTHING, and neither does the blank beyond
    // the end of the row -- nor past the create affordance: `+` is the run's one other span,
    // and it answers `create` rather than a layout, asserted here so a case landing on it by
    // accident would not read as a hole.
    REQUIRE(row.create_columns == 1);
    const LayoutTabPress plus = press(row.create_column, 0);
    CHECK(plus.hit);
    CHECK(plus.create);
    const std::int64_t past = row.create_column + row.create_columns;
    CHECK_FALSE(press(past, 0).hit);
    CHECK_FALSE(press(past + 1, 0).hit);
    CHECK_FALSE(press(kScreenMinW - 1, 0).hit);
    // ...and the gap between the last tab and the `+` is not either of them.
    CHECK_FALSE(press(row.tabs.back().column + row.tabs.back().columns, 0).hit);
    // ...NOR DOES ANY OTHER ROW OF THE SCREEN, above or below the one the tabs are on --
    // including the whole of the bottom band, the stale vertical hit map this sweep exists to
    // kill.
    for (std::int64_t r = 1; r < cells_of(sc).h; ++r) {
        CAPTURE(r);
        CHECK_FALSE(press(row.tabs[1].column, r).hit);
    }
    CHECK_FALSE(press(row.tabs[1].column, -1).hit);

    // AND WHILE A WEAVER IS NAMING THE SETUP THERE ARE NO TABS ON SCREEN TO PRESS.
    Session naming = s;
    naming.setup.naming.open = true;
    CHECK(band_tab_row(naming, sc) == kNoBandRow);
    CHECK_FALSE(band_tab_at(naming, sc, input::space::kCells,
                            b.x + row.tabs[1].column, b.y)
                    .hit);
}

TEST_CASE("an omitted tab has no span and cannot be pressed") {
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    SetupState eight = shelf_of({"A", "B", "C", "D", "E", "F", "G", "H"}, 0);
    // Names long enough that the reserved row cannot hold all eight.
    for (std::size_t i = 0; i < eight.shelved.size(); ++i) {
        eight.shelved[i].desk.name = std::string(12, static_cast<char>('a' + i));
    }
    s.setup = live_copy_of(eight);
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);
    REQUIRE(row.after > 0);
    CHECK(row.tabs.size() + row.before + row.after == layout_count(s.setup));
    // MEASURED AT THE MINIMUM, and pinned so the number a report quotes is the suite's. The
    // association's words and its elision marks reserve 27 columns of 78, leaving 51 for the
    // run: a 3-column live tab (`>A<`), three 14-column neighbours and a 4-column ` 4>` come
    // to 49, where a fifth tab would want 63.
    CHECK(row.tabs.size() == 4);
    CHECK(row.before == 0);
    CHECK(row.after == 4);
    CHECK(row.text.find("setup: none") != std::string::npos);
    // Nothing painted claims a layout the window left out, and no press anywhere on the
    // row can reach one: the spans are exactly the painted population.
    for (const LayoutTab& tab : row.tabs) {
        CHECK(tab.at < layout_count(s.setup) - row.after);
    }
    const CellRect b = cells_covered(top_band_bounds(sc));
    for (std::int64_t c = 0; c < cells_of(sc).w; ++c) {
        const LayoutTabPress hit =
            band_tab_at(s, sc, input::space::kCells, b.x + c,
                        b.y);
        if (hit.hit) {
            CAPTURE(c);
            CHECK(hit.at < layout_count(s.setup) - row.after);
        }
    }
}

TEST_CASE("the tab run is one composition on both media") {
    // The band is a REGION, so a face's row holds however many characters the face fits;
    // the run is composed against that number rather than against canvas cells, and the
    // press inverse resolves through the same `prose_at` every other region press does.
    Session cells = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    cells.setup = live_copy_of_shelf({"Code", "Build"}, 1);
    Session face = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    face.setup = live_copy_of_shelf({"Code", "Build"}, 1);

    const BandStatus cell_row = band_status(cells, screen_of(cells));
    const BandStatus face_row = band_status(face, screen_of(face));
    // ONE ANSWER ABOUT WHICH LAYOUT IS LIVE, whatever the medium can fit beside it.
    REQUIRE(cell_row.tabs.size() == 2);
    REQUIRE(face_row.tabs.size() == 2);
    CHECK(cell_row.tabs[1].active);
    CHECK(face_row.tabs[1].active);
    CHECK(cell_row.tabs[1].column == face_row.tabs[1].column);
    CHECK(band_tab_row(cells, screen_of(cells)) == 0);
    CHECK(band_tab_row(face, screen_of(face)) == 0);
    // A window pixel inside the second tab answers the second layout: where the Layouts pane's
    // first row sets that tab's characters.
    const RegionFit fit = layouts_body(face, screen_of(face)).fit;
    REQUIRE(fit.graphical());
    const LayoutTabPress hit =
        band_tab_at(face, screen_of(face), input::space::kPixels,
                    fit.view.x + fit.origin_x + face_row.tabs[1].column * fit.advance_px + 4,
                    fit.view.y + fit.origin_y + 4);
    CHECK(hit.hit);
    CHECK(hit.at == 1);
}

TEST_CASE("the run never spends more columns than it was given") {
    // THE BOUND, AS A PROPERTY over every population this run can hold and every width a band
    // row can offer -- including the degenerate ones, where a name too wide for the room sits
    // between two omitted populations and both markers must still be paid for out of the SAME
    // budget. A marker appended after the budget was spent is a bound that grows when exceeded.
    for (std::size_t count = 1; count <= kMaxLayouts; ++count) {
        std::vector<std::string> names;
        for (std::size_t i = 0; i < count; ++i) {
            // Long enough that the run cannot fit, at every width this sweep asks about.
            names.push_back(std::string(kMaxSetupNameLen, static_cast<char>('a' + i)));
        }
        for (std::size_t live = 0; live < count; ++live) {
            const SetupState s = shelf_of(names, live);
            for (std::int64_t columns = 0; columns <= 120; ++columns) {
                CAPTURE(count);
                CAPTURE(live);
                CAPTURE(columns);
                const LayoutTabRun run = layout_tab_run(s, columns);
                REQUIRE(static_cast<std::int64_t>(run.text.size()) <= columns);
                // ...and whatever it did paint is inside what it wrote, in the weaver's
                // order, with the omitted counted on the side they were left out on.
                std::int64_t reach = 0;
                for (const LayoutTab& tab : run.tabs) {
                    REQUIRE(tab.column >= reach);
                    reach = tab.column + tab.columns;
                    REQUIRE(reach <= static_cast<std::int64_t>(run.text.size()));
                }
                REQUIRE(run.tabs.size() + run.before + run.after == count);
                for (std::size_t i = 0; i + 1 < run.tabs.size(); ++i) {
                    REQUIRE(run.tabs[i].at + 1 == run.tabs[i + 1].at);
                }
                if (!run.tabs.empty()) {
                    REQUIRE(run.tabs.front().at == run.before);
                }
            }
        }
    }
}

// ============================================================================
// ---- the layout selector is the first row of Workshop ----------------------
// What the move to the top must not disturb: the reserved total, the body's own extent, and
// the agreement between every owner of where the body begins.

TEST_CASE("the layout selector is the first Workshop row, on both media") {
    for (const std::int64_t line : {std::int64_t{0}, std::int64_t{18}}) {
        CAPTURE(line);
        Session s = screen_session(kScreenMinW, kScreenMinH, line == 0 ? 0 : 8, line);
        s.setup = live_copy_of_shelf({"Code", "Build"}, 0);
        const Screen sc = screen_of(s);

        // THE BAND IS AT THE CANVAS'S OWN ORIGIN, and the run is inside it.
        CHECK(top_band_bounds(sc).y == 0);
        CHECK(top_band_bounds(sc).x == 0);
        CHECK(top_band_bounds(sc).w == sc.w);
        CHECK(band_tab_row(s, sc) == 0);
        CHECK(band_status(s, sc).text.rfind(">Code<", 0) == 0);

        // ...AND NOT IN THE FOOTER. The bottom band still exists and still speaks; it does not
        // carry the identity, and no tab is painted anywhere in it.
        CHECK(band_bounds(sc).y == sc.band_y);
        CHECK(band_bounds(sc).y + band_bounds(sc).h == sc.h);
        CHECK(band_bounds(sc).y > top_band_bounds(sc).y + top_band_bounds(sc).h);
        const ScreenCanvas c = paint(s);
        for (const ScreenRegion& r : all_texts(c)) {
            if (r.y == band_bounds(sc).y) {
                for (const TextRow& row : r.rows) {
                    CAPTURE(row.text);
                    // ⚠ ASKED BY THE TAB'S OWN SPELLING, because a bare name is an ordinary
                    // word: the foot's legend may honestly say `build`, and only `>Code<` /
                    // ` Code ` is a layout TAB.
                    CHECK(row.text.find(layout_tab_text(s.setup, 0)) == std::string::npos);
                    CHECK(row.text.find(layout_tab_text(s.setup, 1)) == std::string::npos);
                }
            }
        }
    }
}

TEST_CASE("the move re-homed reserved rows and did not add one") {
    // THE PROPERTY THAT MATTERS MOST, because the workspace's extent is what a share
    // resolves against: a chrome change that resized the body would resize every `%` object
    // a weaver authored. Two reserved bands and no blank row between them, at every extent.
    CHECK(kTopRows + kBottomRows == 5);
    for (std::int64_t h = kScreenMinRows; h <= 120; ++h) {
        CAPTURE(h);
        const Screen sc = screen_of(kScreenMinW, cells_px(h));
        // The three regions tile the screen exactly: no cell reserved twice, none left over.
        CHECK(top_band_bounds(sc).y + top_band_bounds(sc).h == sc.room_y);
        CHECK(sc.room_y + sc.room_h == band_bounds(sc).y);
        CHECK(band_bounds(sc).y + band_bounds(sc).h == sc.h);
        // ...and the body is what the two bands leave.
        CHECK(cells_of(sc).room_h == h - 5);
    }
    // The minimum composition's own numbers are the ones every golden is written against; the
    // width is 78 because the room is the screen
    // (agents/decisions/the-room-is-the-screen.md), and this case's subject is the two BANDS.
    CHECK(cells_of(kMinScreen).room_w == 78);
    CHECK(cells_of(kMinScreen).room_h == 17);
}

TEST_CASE("every owner of the body agrees about where it begins") {
    // THE ONE-ROW DISAGREEMENT THIS CASE REFUSES. Paint, occupancy, the placement path, the
    // overlay column, the contextual popup and the hotkey view all resolve the body from the
    // same constants -- so the question is asked of each of them rather than of the constant
    // they share.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    const Screen sc = screen_of(t.session());

    const CellRect slot = cells_covered(placement_bounds(placement::kOverlayStack, 0, sc));
    CHECK(slot.y == kRoomCellY); // the stack begins at the body's own top
    // ...and the room's own top, where every place word counts from.
    CHECK(gfx::cell_of_pixel(sc.room_y) == kRoomCellY);
    CHECK(gfx::cell_of_pixel(overlay_column(sc).y) == kRoomCellY);

    // THE PANE IS PAINTED WHERE IT IS HIT. Its top-left cell answers the pointer with the
    // Builder, and the cell directly above it -- the reserved top row's last -- answers with
    // the pane standing THERE: a boundary between two panes, checkable by one rule, and not
    // between a pane and a hole.
    const CellRect builder = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect);
    CHECK(builder.y == kRoomCellY);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, builder.x, builder.y)
              .kind == stock::kKind);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, builder.x,
                      builder.y - 1)
              .kind == pane_kind::kLayouts);

    // AND A REAL PRESS AGREES WITH BOTH. A press on the pane's own first row selects it;
    // one row higher selects the pane that owns that row.
    t.press_canvas(builder.x + 2, builder.y);
    CHECK(t.session().panes.selected == stock::kKind);
    t.press_canvas(builder.x + 2, builder.y - 1);
    CHECK(t.session().panes.selected == pane_kind::kLayouts);
}

// ============================================================================
// ---- the selection hugs the name -------------------------------------------
// The two marker cells are spent symmetrically and bare, `Home >Code< Art`, and must not
// move: the derived window, the omission counts, the reservation, the band's own geometry,
// and the agreement between what is painted and what a press answers.

TEST_CASE("every tab is one cell, the name, one cell") {
    // THE WHOLE PRESENTATION LAW AS A PROPERTY, over every population this run can hold, every
    // live position in it, and names of every length -- because each falsifier is a different
    // way for ONE of these spans to come out wrong: a quote retained, only the left marker
    // kept, a space between a marker and its name, or an active tab costing more than another.
    for (std::size_t count = 1; count <= kMaxLayouts; ++count) {
        for (std::size_t len = 1; len <= 12; ++len) {
            std::vector<std::string> names;
            for (std::size_t i = 0; i < count; ++i) {
                names.push_back(std::string(len, static_cast<char>('a' + i)));
            }
            for (std::size_t live = 0; live < count; ++live) {
                CAPTURE(count);
                CAPTURE(len);
                CAPTURE(live);
                const SetupState s = shelf_of(names, live);
                // WIDE ENOUGH THAT NOTHING IS WINDOWED OR CUT, so what is asserted is the
                // spelling and not the degradation (that is the windowing sweep's, above).
                const LayoutTabRun run =
                    layout_tab_run(s, static_cast<std::int64_t>(count * (len + 2)));
                REQUIRE(run.before == 0);
                REQUIRE(run.after == 0);
                REQUIRE(run.tabs.size() == count);
                for (const LayoutTab& tab : run.tabs) {
                    const std::string span =
                        run.text.substr(static_cast<std::size_t>(tab.column),
                                        static_cast<std::size_t>(tab.columns));
                    // ONE ASSERTION, FOUR FALSIFIERS. `>a<` is not `> "a"`, not `> a`, not
                    // `>a `, and not two cells longer than ` a `.
                    REQUIRE(span == (tab.active ? ">" + names[tab.at] + "<"
                                                : " " + names[tab.at] + " "));
                    REQUIRE(tab.columns == static_cast<std::int64_t>(len) + 2);
                }
                // ...AND NOT ONE QUOTATION MARK IN THE WHOLE RUN.
                REQUIRE(run.text.find('"') == std::string::npos);
            }
        }
    }
}

TEST_CASE("switching the live layout moves nothing to the right of it") {
    // THE DEFECT EQUAL WIDTH REFUSES, asked of the composed ROW rather than the run: a weaver
    // stepping through their layouts must not watch the `setup:` slot, its verdict or the
    // gestures slide sideways under the marker. The right-hand block is adjusted to the row's
    // edge as well, so it is doubly still -- but what is asserted is EQUAL TAB WIDTH.
    const std::vector<std::string> names{"Home", "Code", "Art"};
    std::vector<std::int64_t> status_at;
    std::vector<std::string> spans;
    for (std::size_t live = 0; live < names.size(); ++live) {
        CAPTURE(live);
        Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
        s.setup = live_copy_of_shelf(names, live);
        const BandStatus row = band_status(s, screen_of(s));
        REQUIRE(row.tabs.size() == names.size());
        REQUIRE(row.before == 0);
        REQUIRE(row.after == 0);
        status_at.push_back(static_cast<std::int64_t>(row.text.find("setup: none")));
        // EVERY TAB'S SPAN IS THE SAME SPAN whichever one is live -- the geometry does not
        // know which layout the marker is on, which is the point.
        std::string shape;
        for (const LayoutTab& tab : row.tabs) {
            shape += std::to_string(tab.column) + ":" + std::to_string(tab.columns) + " ";
        }
        spans.push_back(shape);
        // AND THE RUN READS AS THE WEAVER'S OWN SENTENCE.
        CHECK(row.text.rfind(live == 0   ? ">Home< Code  Art "
                             : live == 1 ? " Home >Code< Art "
                                         : " Home  Code >Art<",
                             0) == 0);
    }
    CHECK(status_at[0] == status_at[1]);
    CHECK(status_at[1] == status_at[2]);
    CHECK(status_at[0] > 0);
    CHECK(spans[0] == spans[1]);
    CHECK(spans[1] == spans[2]);
}

TEST_CASE("the closing marker belongs to the layout it closes") {
    // THE POINTER FALSIFIER. `<` is the last cell of the ACTIVE tab and the cell after it is
    // the first of its neighbour; an inverse that handed either one to the wrong layout would
    // be a press that disagrees with the paint.
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.setup = live_copy_of_shelf({"Home", "Code", "Art"}, 1);
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);
    REQUIRE(row.tabs.size() == 3);
    REQUIRE(row.tabs[1].active);
    CHECK(row.text.substr(static_cast<std::size_t>(row.tabs[1].column),
                          static_cast<std::size_t>(row.tabs[1].columns)) == ">Code<");

    const CellRect b = cells_covered(top_band_bounds(sc));
    const auto press = [&](std::int64_t column) {
        return band_tab_at(s, sc, input::space::kCells, b.x + column,
                           b.y);
    };
    const std::int64_t opens = row.tabs[1].column;
    const std::int64_t closes = row.tabs[1].column + row.tabs[1].columns - 1;
    CHECK(row.text[static_cast<std::size_t>(opens)] == '>');
    CHECK(row.text[static_cast<std::size_t>(closes)] == '<');
    // BOTH MARKERS ARE THE ACTIVE LAYOUT'S...
    REQUIRE(press(opens).hit);
    CHECK(press(opens).at == 1);
    REQUIRE(press(closes).hit);
    CHECK(press(closes).at == 1);
    // ...AND NEITHER NEIGHBOUR OVERLAPS THEM. The cell before the `>` is the previous
    // tab's own reserved cell and the cell after the `<` is the next tab's.
    REQUIRE(press(opens - 1).hit);
    CHECK(press(opens - 1).at == 0);
    REQUIRE(press(closes + 1).hit);
    CHECK(press(closes + 1).at == 2);
    // AND THERE ARE NO QUOTE POSITIONS: no quotation mark anywhere on the row.
    CHECK(row.text.find('"') == std::string::npos);
}

TEST_CASE("the weaver reads `Home >Code< Art` on Workshop's first row") {
    // THE COMPLETION SENTENCE, through the real rasterizer rather than the composition -- the
    // bytes a weaver's terminal actually receives, on the selector's row.
    for (const std::size_t live : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
        CAPTURE(live);
        Session s;
        s.setup = live_copy_of_shelf({"Home", "Code", "Art"}, live);
        const std::vector<std::string> rows = rasterized(paint(s));
        REQUIRE(rows.size() == static_cast<std::size_t>(cells_of(kMinScreen).h));
        CHECK(rows[0].rfind(live == 0   ? ">Home< Code  Art "
                            : live == 1 ? " Home >Code< Art "
                                        : " Home  Code >Art<",
                            0) == 0);
        CHECK(rows[0].find('"') == std::string::npos);
    }
    // A MULTI-WORD NAME IS STILL ONE TAB, and reads as one without a quotation mark.
    Session wordy;
    wordy.setup = live_copy_of_shelf({"Home", "My Layout", "Art"}, 1);
    const std::vector<std::string> rows = rasterized(paint(wordy));
    CHECK(rows[0].rfind(" Home >My Layout< Art ", 0) == 0);
}

TEST_CASE("no press outside the painted run reaches a layout") {
    // THE STALE HIT MAP, SWEPT. Every cell of the screen is offered to the tab inverse, and
    // the only ones that answer are cells of a tab the composition actually painted -- on row
    // 0 and nowhere else.
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.setup = live_copy_of_shelf({"Code", "Build", "Inspect"}, 1);
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);
    REQUIRE(row.tabs.size() == 3);

    std::size_t answered = 0;
    for (std::int64_t y = 0; y < cells_of(sc).h; ++y) {
        for (std::int64_t x = 0; x < cells_of(sc).w; ++x) {
            const LayoutTabPress hit =
                band_tab_at(s, sc, input::space::kCells, x,
                            y);
            if (!hit.hit) {
                continue;
            }
            CAPTURE(x);
            CAPTURE(y);
            CHECK(y == 0); // the first Workshop row, and no other
            // ...OR THE CREATE AFFORDANCE, which is the run's one other span and answers an
            // ACTION rather than a layout.
            bool inside = hit.create && x >= row.create_column &&
                          x < row.create_column + row.create_columns;
            for (const LayoutTab& tab : row.tabs) {
                inside = inside || (!hit.create && x >= tab.column &&
                                    x < tab.column + tab.columns && hit.at == tab.at);
            }
            CHECK(inside);
            ++answered;
        }
    }
    // ...and every painted cell DID answer, so the sweep is a bijection rather than an
    // absence: what is painted is pressable, and what is pressable is painted.
    std::size_t painted = static_cast<std::size_t>(row.create_columns);
    for (const LayoutTab& tab : row.tabs) {
        painted += static_cast<std::size_t>(tab.columns);
    }
    CHECK(answered == painted);
}

// ============================================================================
// ---- the layout surface is an ordinary pane ---------------------------------
// The tab run, the active layout's Setup association and the workspace fact are one built-in
// pane, `pane_kind::kLayouts`, with a catalog row, a setup row, a place in `occupied_at`,
// coverage and persistence. These cases are about those seams; the composition is above.
// ============================================================================

TEST_CASE("the Layouts pane's developer default IS the historical rectangle") {
    // THE EQUIVALENCE CLAIM, ON BOTH SHIPPED FACES. A weaver who never authors anything sees
    // the layout run in the reserved top band -- so the pane's default rectangle is asked of
    // the one place resolver and compared against the reservation itself, not a
    // transcription of two numbers.
    for (const auto& metric : {std::pair<std::int64_t, std::int64_t>{0, 0},
                               std::pair<std::int64_t, std::int64_t>{8, 18}}) {
        CAPTURE(metric.first);
        Session s = screen_session(kScreenMinW, kScreenMinH, metric.first, metric.second);
        const Screen sc = screen_of(s);
        const PaneBounds where =
            bounds_of(s.panes, s.setup.active, pane_kind::kLayouts, sc);
        REQUIRE(where.open);
        CHECK(where.placed_in == placement::kTopBand);
        // THE OUTER RECTANGLE IS THE RESERVATION'S, to the pixel, and it holds the band's rows.
        CHECK(where.rect == top_band_bounds(sc));
        CHECK(layouts_body(s, sc).rows == kTopRows);
        if (metric.first == 0) {
            // ...where text is a cell, the two cell rows it has always been.
            CHECK(cells_covered(where.rect) == CellRect{0, 0, cells_of(sc).w, kTopRows});
        }
    }
}

TEST_CASE("a two-cell pane keeps its content and drops its boundary") {
    // THE ONE COST OF BEING A PANE, AND WHERE IT IS PAID: chrome is one unit of the active
    // face a side, and on a character medium the two-row rectangle would keep ZERO rows of
    // interior. `pane_inside`'s last candidate is no boundary at all: the terminal pays nothing
    // and keeps both rows; the shipped face's band is fitted to its two rows of type.
    // ⚔ MUTATION: dropping that candidate -- `rows` goes to 0 and `band_status(...).text` is
    // empty.
    Session cells = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    const PaneInside on_cells =
        pane_inside(top_band_bounds(screen_of(cells)), screen_of(cells));
    CHECK(on_cells.chrome_px == 0);
    CHECK(on_cells.fit.rows == kTopRows);
    CHECK(layouts_body(cells, screen_of(cells)).rows == kTopRows);
    CHECK_FALSE(band_status(cells, screen_of(cells)).text.empty());

    // ⚠ THE SHIPPED FACE REPORTS ITS CELL SIZE AS WELL AS ITS TYPE. `cell_px` is what
    // `chrome_grain` spends: a medium that does not publish one can show nothing thinner than
    // a cell, which is the terminal's answer above.
    Session face =
        screen_session(kScreenMinW, kScreenMinH, 8, 18, gfx::kCellPx);
    const PaneInside on_face =
        pane_inside(top_band_bounds(screen_of(face)), screen_of(face));
    CHECK(on_face.chrome_px == gfx::px_of_one_device(screen_of(face).cell_px));
    CHECK(on_face.chrome_px > 0);
    CHECK(layouts_body(face, screen_of(face)).rows == kTopRows);
    CHECK_FALSE(band_status(face, screen_of(face)).text.empty());

    // ...AND A PANE WITH ROOM FOR A BOUNDARY STILL PAYS ONE, on the same face. The rung is
    // a floor for a rectangle that cannot hold an edge, not a repeal of the edge.
    const PaneInside roomy =
        pane_inside(pixels_of_cells(CellRect{0, 0, 40, 9}), screen_of(cells));
    CHECK(roomy.chrome_px == kChromePx);
}

TEST_CASE("authored geometry moves the Layouts pane, and the tabs with it") {
    // THE POINT OF BEING A PANE, in one case: place and size are the weaver's, through
    // the SAME doors every other pane's geometry goes through -- and paint, the press
    // inverse and occupancy all follow, because there is one resolution.
    Live t;
    t.extent_px(cells_px(120), cells_px(40), 0, 0);
    const PaneRef layouts = ref_of(pane_kind::kLayouts);
    const Screen sc = screen_of(t.session());
    REQUIRE(bounds_of(t.session().panes, t.session().setup.active, pane_kind::kLayouts, sc)
                .rect == top_band_bounds(sc));

    REQUIRE(place_at_canvas(t, layouts, cells_px(10), cells_px(20)).accepted);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_size(desk, layouts,
                                 PaneSize{pane_unit::kPixels, cells_px(40)},
                                 PaneSize{pane_unit::kPixels, cells_px(4)})
                    .accepted);
    });
    const CellRect moved = cells_covered(
        bounds_of(t.session().panes, t.session().setup.active, pane_kind::kLayouts, sc).rect);
    CHECK(moved == CellRect{10, 20, 40, 4});

    // THE PRESS INVERSE FOLLOWED IT. A tab press at the pane's new first interior row lands
    // on a tab; the reserved top row answers nothing at all.
    const ExternalBodyPlace body = layouts_body(t.session(), sc);
    REQUIRE(body.present);
    // ...AND THIS ONE IS TALL ENOUGH TO WEAR A BOUNDARY, which is the other half of the
    // chrome rung: four rows less one cell a side leaves two, so the edge is drawn and the
    // interior begins one row inside the rectangle the weaver authored.
    CHECK(body.region_y == cells_px(21));
    const BandStatus row = band_status(t.session(), sc);
    REQUIRE_FALSE(row.tabs.empty());
    CHECK(band_tab_at(t.session(), sc, input::space::kCells,
                      gfx::cell_of_pixel(body.region_x) + row.tabs[0].column,
                      gfx::cell_of_pixel(body.region_y))
              .hit);
    CHECK_FALSE(band_tab_at(t.session(), sc, input::space::kCells,
                            row.tabs[0].column,
                            0)
                    .hit);

    // AND OCCUPANCY FOLLOWED IT TOO: the pane is where it was authored and nowhere else.
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, 12, 21).kind ==
          pane_kind::kLayouts);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc, 12, 0).occupied);
}

TEST_CASE("a pane in front of the Layouts pane takes the press") {
    // THE FALSIFIER: a band painted in FRONT of every pane that answered presses on the tabs
    // alone would draw over a pane authored over the tab run and still take its press --
    // see-here, press-there. ⚔ MUTATION: asking `band_tab_at` before the occupancy walk; the
    // Builder stops being selected, `switch_layout` runs, and both checks below move.
    Live t;
    t.extent_px(cells_px(120), cells_px(40), 0, 0);
    const LayoutKeys k = layout_keys(t);
    press_gesture(t, k.make); // two layouts, so switching is observable
    REQUIRE(layout_count(t.session().setup) == 2);
    const std::size_t live_at = t.session().setup.active_at;

    open_pane(t, ref_of(stock::kKind));
    // THE LAYOUTS PANE MOVED INTO THE ROOM, and the Builder authored OVER its tab run, covering
    // it whole: a place is measured from the room, so no pane can stand over the band itself.
    // The Builder is already front-most -- the launch door appends the front-most rank --
    // which is why `send_to_front` would answer "already".
    const Screen sc = screen_of(t.session());
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(pane_kind::kLayouts), 0,
                                  cells_px(5))
                    .accepted);
    });
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, ref_of(stock::kKind), 0, cells_px(5))
                    .accepted);
    });
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_size(desk, ref_of(stock::kKind),
                                 PaneSize{pane_unit::kPixels, sc.w},
                                 PaneSize{pane_unit::kPixels, gfx::px_of_cells(kTopRows)})
                    .accepted);
    });
    REQUIRE(index_of(authored_order(t.session()), stock::kKind) >
            index_of(authored_order(t.session()), pane_kind::kLayouts));

    // THE POINT IS THE BUILDER'S, on the effective order the paint walk spends.
    const BandStatus row = band_status(t.session(), sc);
    REQUIRE_FALSE(row.tabs.empty());
    const std::int64_t at_x = row.tabs.front().column;
    const std::int64_t at_y = gfx::cell_of_pixel(layouts_body(t.session(), sc).region_y);
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, at_x, at_y).kind ==
          stock::kKind);
    // ...AND SO IS THE PRESS. The layout does not switch and the Builder is what the weaver
    // is pointing at.
    t.press_canvas(at_x, at_y);
    CHECK(t.session().panes.selected == stock::kKind);
    CHECK(t.session().setup.active_at == live_at);

    // AND THE LAYOUTS PANE KNOWS IT IS COVERED: coverage counts panes, and it is one.
    CHECK(pane_state_of(t.session().panes, t.session().setup.active, sc,
                        CatalogRow{pane_kind::kLayouts, ref_of(pane_kind::kLayouts), "Layouts", ""}) ==
          pane_state::kCovered);

    // THE OTHER DIRECTION, from the same desk: send the Layouts pane to the front and the same
    // point is its again, and the press switches the layout. ⚠ The standing selection is
    // cleared first: `effective_pane_order` lifts the pane a weaver points at, so a desk whose
    // front rank says Layouts still answers Builder while the Builder is selected. ⚠ Just left
    // of the right column, not at the room's edge, which is under the pane standing there.
    t.press_canvas(cells_of(sc).strip_x - 1, cells_of(sc).h - kBottomRows - 1); // bare workspace: selects nothing
    REQUIRE(t.session().panes.selected == kNoPaneKind);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(send_to_front(desk, ref_of(pane_kind::kLayouts)));
    });
    CHECK(occupied_at(t.session().panes, t.session().setup.active, sc, at_x, at_y).kind ==
          pane_kind::kLayouts);
    t.press_canvas(at_x, at_y);
    CHECK(t.session().panes.selected == pane_kind::kLayouts);
    CHECK(t.session().setup.active_at == row.tabs.front().at);
}

TEST_CASE("the reservation does not follow the Layouts pane") {
    // THE FIXED-RESERVATION PROOF, a claim about the DOCUMENT rather than chrome: `room_w` and
    // `room_h` are what a `%`-sized object resolves against, so moving, resizing or removing
    // the layout surface must not change a weaver's material's size. ⚔ MUTATION: `screen_of`
    // subtracting `kTopRows` only when the pane participates; every comparison below moves.
    Live t;
    t.extent_px(cells_px(132), cells_px(46), 0, 0);
    const Screen before = screen_of(t.session());

    const PaneRef layouts = ref_of(pane_kind::kLayouts);
    // MOVED, RESIZED, AND THEN REMOVED ALTOGETHER -- three presentation changes, one
    // unchanged basis.
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_place(desk, layouts, cells_px(20), cells_px(30)).accepted);
    });
    CHECK(screen_of(t.session()).room_w == before.room_w);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    stage_desk(t, [&](Setup& desk) {
        REQUIRE(author_pane_size(desk, layouts,
                                 PaneSize{pane_unit::kPixels, cells_px(9)},
                                 PaneSize{pane_unit::kPixels, cells_px(9)})
                    .accepted);
    });
    CHECK(screen_of(t.session()).room_w == before.room_w);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    pick(t, pane_kind::kLayouts);
    REQUIRE_FALSE(t.session().panes.has(pane_kind::kLayouts));
    CHECK(screen_of(t.session()).room_w == before.room_w);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    CHECK(screen_of(t.session()).band_y == before.band_y);
    CHECK(gfx::cell_of_pixel(overlay_column(screen_of(t.session())).y) == kRoomCellY);
}

TEST_CASE("removing the Layouts pane strands nobody") {
    // THE RECOVERY CLAIM. A pane a weaver can remove is a pane a weaver can lose, and the answer
    // is one that exists: the desktop's Pane Manager lists it (the one inventory is its
    // population), the keyboard's layout gestures never went through it, and the desk reset
    // brings the default back. No new recovery framework.
    Live t;
    t.host.setup_path = "workshop-setup.json";
    const LayoutKeys k = layout_keys(t);
    pick(t, pane_kind::kLayouts);
    REQUIRE_FALSE(t.session().panes.has(pane_kind::kLayouts));
    const Screen sc = screen_of(t.session());
    // NOTHING IS PAINTED AND NOTHING IS PRESSABLE, honestly.
    CHECK(band_status(t.session(), sc).text.empty());
    CHECK(band_tab_row(t.session(), sc) == kNoBandRow);
    CHECK_FALSE(band_tab_at(t.session(), sc, input::space::kCells,
                            0, 0)
                    .hit);
    CHECK_FALSE(occupied_at(t.session().panes, t.session().setup.active, sc, 0, 0).occupied);

    // ...AND THE KEYS STILL REACH EVERY LAYOUT, because they never went through the
    // presentation. A weaver with no tab run still makes, names and steps between desks.
    press_gesture(t, k.make);
    CHECK(layout_count(t.session().setup) == 2);
    press_gesture(t, k.next);
    CHECK(t.session().setup.active_at == 0);

    // AND OPENING IT BRINGS IT BACK, at the developer default it was born with.
    open_pane(t, ref_of(pane_kind::kLayouts));
    CHECK(t.session().panes.has(pane_kind::kLayouts));
    CHECK(bounds_of(t.session().panes, t.session().setup.active, pane_kind::kLayouts,
                    screen_of(t.session()))
              .rect == top_band_bounds(screen_of(t.session())));
    CHECK_FALSE(band_status(t.session(), screen_of(t.session())).text.empty());
}

// ========================================================================================
// The keyboard's selection: reading order, the neighbour in a direction, and its ring
// ========================================================================================

namespace {

/// A DESK OF STAND-IN PANES AT AUTHORED RECTANGLES, as a value: each offered under the stand-ins'
/// office, opened, and placed at its rectangle in room pixels, on a graphical medium with room for
/// every one. The selection walks what `bounds_of` answers, so no bus is needed.
struct PlacedDesk {
    Session s = screen_session(cells_px(160), cells_px(44), 8, 18);
    std::vector<std::int64_t> kinds;

    explicit PlacedDesk(const std::vector<PixelRect>& rects) {
        for (std::size_t i = 0; i < rects.size(); ++i) {
            const std::string pane = "placed" + std::to_string(i);
            const Admission took = admit_pane_offer(
                s.panes.runtime, stock::kOffice,
                PaneOffered{pane, "Placed " + std::to_string(i), "a pane at a known place"});
            REQUIRE_MESSAGE(took.written.accepted, took.written.refusal);
            REQUIRE(open_kind(s.panes, took.kind));
            const PaneRef ref{stock::kOffice, pane};
            REQUIRE(add_pane(s.setup.active, ref));
            REQUIRE(author_pane_place(s.setup.active, ref, rects[i].x, rects[i].y).accepted);
            REQUIRE(author_pane_size(s.setup.active, ref, PaneSize{pane_unit::kPixels, rects[i].w},
                                     PaneSize{pane_unit::kPixels, rects[i].h})
                        .accepted);
            kinds.push_back(took.kind);
        }
        // EVERY PANE STANDS AS IT WAS AUTHORED, or the case would measure a clip.
        for (std::size_t i = 0; i < rects.size(); ++i) {
            const PixelRect at = rect(i);
            CHECK(at.w == rects[i].w);
            CHECK(at.h == rects[i].h);
        }
    }

    PixelRect rect(std::size_t i) const {
        return bounds_of(s.panes, s.setup.active, kinds[i], screen_of(s)).rect;
    }

    std::int64_t kind_at(std::int64_t i) const {
        return i < 0 ? kNoPaneKind : kinds[static_cast<std::size_t>(i)];
    }

    /// The pane `dir` reaches from pane `from`, as its index, or -1 for none.
    std::int64_t toward(std::int64_t from, Act dir) const {
        return index(spatial_neighbour(s.panes, s.setup.active, screen_of(s), kind_at(from), dir));
    }

    std::int64_t step(std::int64_t from, std::int64_t by) const {
        return index(cycle_pane(s.panes, s.setup.active, screen_of(s), kind_at(from), by));
    }

    std::int64_t index(const std::optional<std::int64_t>& kind) const {
        if (!kind.has_value()) {
            return -1;
        }
        const auto at = std::find(kinds.begin(), kinds.end(), *kind);
        return at == kinds.end() ? -2 : static_cast<std::int64_t>(at - kinds.begin());
    }
};

constexpr std::int64_t kCell = gfx::kCellPx;

/// The rectangles a window's drawing outlines in the selection's group.
std::vector<PixelRect> window_selection_rings(const Session& s) {
    namespace gfx = zengine::graphics;
    const WorkshopWindowDrawing drawn = workshop_window_drawing(s);
    std::vector<PixelRect> out;
    for (const auto& node : drawn.sketch.nodes()) {
        if (node.path.find("/selection/") == std::string::npos) {
            continue;
        }
        REQUIRE(node.op == gfx::op::kRect);
        const auto r = gfx::from_body<gfx::RectArgs>(node.args);
        REQUIRE(r.has_value());
        out.push_back(PixelRect{r->x, r->y, r->w, r->h});
    }
    return out;
}

} // namespace

TEST_CASE("the keyboard's selection walks the open panes in reading order, whatever order they were "
          "authored in, and wraps both ways") {
    // ⚔ MUTATION: the walk in authored order -- `focus.next` from the left pane goes to the
    // right one, the order this desk was written in.
    // Authored middle, right, left on one row, and a fourth pane below them all.
    const PlacedDesk d({PixelRect{30 * kCell, 0, 20 * kCell, 8 * kCell},
                        PixelRect{60 * kCell, 0, 20 * kCell, 8 * kCell},
                        PixelRect{0, 0, 20 * kCell, 8 * kCell},
                        PixelRect{10 * kCell, 12 * kCell, 20 * kCell, 8 * kCell}});
    const std::vector<std::int64_t> order =
        selectable_panes(d.s.panes, d.s.setup.active, screen_of(d.s));
    REQUIRE(order.size() == 4);
    CHECK(order[0] == d.kinds[2]);
    CHECK(order[1] == d.kinds[0]);
    CHECK(order[2] == d.kinds[1]);
    CHECK(order[3] == d.kinds[3]);
    // ...FORWARD, round and back to the start...
    CHECK(d.step(2, 1) == 0);
    CHECK(d.step(0, 1) == 1);
    CHECK(d.step(1, 1) == 3);
    CHECK(d.step(3, 1) == 2);
    // ...AND BACKWARD, the same ring the other way.
    CHECK(d.step(2, -1) == 3);
    CHECK(d.step(3, -1) == 1);
    CHECK(d.step(0, -1) == 2);
    // FROM NOTHING SELECTED, the first in reading order, or the last going back.
    CHECK(d.step(-1, 1) == 2);
    CHECK(d.step(-1, -1) == 3);
    // THE LAYOUT RUN IS ON THE DESK AND TAKES NO KEYS, so it is no stop.
    REQUIRE(d.s.panes.has(pane_kind::kLayouts));
    CHECK(std::find(order.begin(), order.end(), pane_kind::kLayouts) == order.end());
    // ...AND A DESK WITH NOTHING TO SELECT ANSWERS NONE.
    const PlacedDesk empty({});
    CHECK(empty.step(-1, 1) == -1);
    CHECK(empty.toward(-1, Act::kFocusRight) == -1);
}

TEST_CASE("a direction selects the neighbour past the selected pane's edge, and nothing past the "
          "desk's") {
    // ⚔ MUTATION: right and left swapped in `spatial_neighbour`'s filter -- `right` from the top
    // left reaches nothing, and `left` from it reaches the top right.
    // A two-by-two grid with a gap between the columns and the rows.
    const PlacedDesk d({PixelRect{0, 0, 30 * kCell, 10 * kCell},
                        PixelRect{40 * kCell, 0, 30 * kCell, 10 * kCell},
                        PixelRect{0, 14 * kCell, 30 * kCell, 10 * kCell},
                        PixelRect{40 * kCell, 14 * kCell, 30 * kCell, 10 * kCell}});
    CHECK(d.toward(0, Act::kFocusRight) == 1);
    CHECK(d.toward(0, Act::kFocusDown) == 2);
    CHECK(d.toward(3, Act::kFocusLeft) == 2);
    CHECK(d.toward(3, Act::kFocusUp) == 1);
    CHECK(d.toward(1, Act::kFocusDown) == 3);
    CHECK(d.toward(2, Act::kFocusUp) == 0);
    // THE DESK'S EDGE: nothing beyond it, said as none.
    CHECK(d.toward(0, Act::kFocusLeft) == -1);
    CHECK(d.toward(0, Act::kFocusUp) == -1);
    CHECK(d.toward(3, Act::kFocusRight) == -1);
    CHECK(d.toward(3, Act::kFocusDown) == -1);
    // FROM NOTHING SELECTED, a direction starts where the walk does.
    CHECK(d.toward(-1, Act::kFocusDown) == 0);
    // An action that is no direction reaches nothing.
    CHECK(d.toward(0, Act::kFocusNext) == -1);
}

TEST_CASE("misalignment across the direction counts twice the gap along it") {
    // ⚔ MUTATION: misalignment weighted once -- the near pane fourteen cells lower wins, 5 + 14
    // against the level pane's 30.
    // From the first pane, `right` meets a near pane fourteen cells lower (gap 5, across 14: 33)
    // and a far pane level with it (gap 30, across 0: 30). The level one wins.
    const PlacedDesk d({PixelRect{0, 0, 10 * kCell, 10 * kCell},
                        PixelRect{15 * kCell, 14 * kCell, 10 * kCell, 10 * kCell},
                        PixelRect{40 * kCell, 0, 10 * kCell, 10 * kCell}});
    CHECK(d.toward(0, Act::kFocusRight) == 2);
    // ...and with the level pane five cells further, the near pane wins (33 against 35).
    const PlacedDesk farther({PixelRect{0, 0, 10 * kCell, 10 * kCell},
                              PixelRect{15 * kCell, 14 * kCell, 10 * kCell, 10 * kCell},
                              PixelRect{45 * kCell, 0, 10 * kCell, 10 * kCell}});
    CHECK(farther.toward(0, Act::kFocusRight) == 1);
}

TEST_CASE("two neighbours scored alike go to the one in front") {
    // ⚔ MUTATION: a tie kept by the first candidate read -- the pane behind is chosen whichever
    // is in front.
    // From the left pane, `right` meets two panes the same distance off, one above its centre and
    // one below.
    PlacedDesk d({PixelRect{0, 6 * kCell, 10 * kCell, 10 * kCell},
                  PixelRect{20 * kCell, 0, 10 * kCell, 10 * kCell},
                  PixelRect{20 * kCell, 12 * kCell, 10 * kCell, 10 * kCell}});
    // Authored in order, the last is in front.
    CHECK(d.toward(0, Act::kFocusRight) == 2);
    // ...and with the ranks turned over, the other.
    const auto n = static_cast<std::int64_t>(d.s.setup.active.panes.size());
    for (SetupPane& row : d.s.setup.active.panes) {
        row.front = n - 1 - row.front;
    }
    CHECK(d.toward(0, Act::kFocusRight) == 1);
}

TEST_CASE("a pane overlapping the selected one is reached past its centre") {
    // ⚔ MUTATION: the edge filter alone -- a pane half over the selected one is never past its
    // edge, so `right` reaches nothing.
    // The second's centre is five cells short of the first's right edge, and ten past its centre.
    const PlacedDesk d({PixelRect{0, 0, 30 * kCell, 10 * kCell},
                        PixelRect{10 * kCell, 2 * kCell, 30 * kCell, 10 * kCell}});
    CHECK(d.toward(0, Act::kFocusRight) == 1);
    CHECK(d.toward(1, Act::kFocusLeft) == 0);
    CHECK(d.toward(0, Act::kFocusLeft) == -1);
}

TEST_CASE("command mode marks the selected pane, with brackets where text is a cell and rings where "
          "an edge is finer, and the pane the keys are in wears none") {
    // ⚔ MUTATION: no ring -- the selected pane looks as it does with the keys in it, but for the
    // two-character mark.
    PlacedDesk d({PixelRect{0, 0, 30 * kCell, 10 * kCell},
                  PixelRect{40 * kCell, 0, 30 * kCell, 10 * kCell}});
    const Screen sc = screen_of(d.s);
    ScreenPlane none;
    paint_selection_ring(none, d.s, sc);
    CHECK(none.labels.empty()); // nothing selected, nothing marked

    d.s.panes.selected = d.kinds[1];
    REQUIRE(keyboard_context(d.s) == KeyContext::kCommand);
    ScreenPlane ring;
    paint_selection_ring(ring, d.s, sc);
    REQUIRE(ring.labels.size() == 4);
    const PixelRect b = d.rect(1);
    const PixelRect tl = pane_edge_cell(b, pane_edge::kTopLeft);
    const PixelRect br = pane_edge_cell(b, pane_edge::kBottomRight);
    CHECK(ring.labels[0].x == tl.x);
    CHECK(ring.labels[0].y == tl.y);
    CHECK(ring.labels[0].text == "[");
    CHECK(ring.labels[3].x == br.x);
    CHECK(ring.labels[3].y == br.y);
    CHECK(ring.labels[3].text == "]");
    for (const ScreenLabel& l : ring.labels) {
        CHECK(l.role == gfx::role::kAccent);
        CHECK(l.text != std::string(kHandleGlyph)); // not an arrangement handle
    }
    // ...ON THE CANVAS AND IN A WINDOW ALIKE.
    const ScreenCanvas c = paint(d.s);
    std::size_t brackets = 0;
    for (const ScreenPlane& layer : c.layers) {
        for (const ScreenLabel& l : layer.labels) {
            if (l.text == "[" || l.text == "]") {
                ++brackets;
            }
        }
    }
    CHECK(brackets == 4);
    // IN A WINDOW, TWO RINGS JUST OUTSIDE THE EDGE: a corner cell there would stand on the header's
    // words, so nothing is drawn inside the pane.
    const std::vector<PixelRect> rings = window_selection_rings(d.s);
    REQUIRE(rings.size() == 2);
    for (const PixelRect& r : rings) {
        CHECK(r.x < b.x);
        CHECK(r.y < b.y);
        CHECK(r.x + r.w > b.x + b.w);
        CHECK(r.y + r.h > b.y + b.h);
    }

    // ...AND A CANVAS WHOSE EDGES ARE A DEVICE UNIT WIDE wears the rings too, as filled sides, and
    // no glyph that could stand on the header's words.
    Session fine = d.s;
    fine.cell_px = gfx::kCellPx;
    const Screen fine_sc = screen_of(fine);
    REQUIRE(chrome_grain(fine_sc) < gfx::kCellPx);
    ScreenPlane fine_ring;
    paint_selection_ring(fine_ring, fine, fine_sc);
    CHECK(fine_ring.labels.empty());
    REQUIRE(fine_ring.rects.size() == 8);
    const PixelRect fb = bounds_of(fine.panes, fine.setup.active, d.kinds[1], fine_sc).rect;
    for (const ScreenRect& side : fine_ring.rects) {
        CHECK(side.role == gfx::role::kAccent);
        // Every side lies outside the pane: wholly left, right, above or below its rectangle.
        CHECK((side.x + side.w <= fb.x || side.x >= fb.x + fb.w || side.y + side.h <= fb.y ||
               side.y >= fb.y + fb.h));
    }

    // THE KEYS IN THE PANE: its `> ` mark says so, and no ring is drawn.
    ExternalPane* pane = d.s.panes.external_pane(d.kinds[1]);
    REQUIRE(pane != nullptr);
    d.s.panes.keyboard = d.kinds[1];
    REQUIRE(keyboard_context(d.s) == KeyContext::kPane);
    ScreenPlane held;
    paint_selection_ring(held, d.s, sc);
    CHECK(held.labels.empty());
    CHECK(window_selection_rings(d.s).empty());
}

TEST_CASE("`workshop.focus.ring` plain marks the selection in the chrome's ink, its corners where text is a cell and one ring where an edge is finer") {
    PlacedDesk d({PixelRect{0, 0, 30 * kCell, 10 * kCell},
                  PixelRect{40 * kCell, 0, 30 * kCell, 10 * kCell}});
    d.s.panes.selected = d.kinds[1];
    d.s.selection_ring_plain = true;
    const Screen sc = screen_of(d.s);
    ScreenPlane corners;
    paint_selection_ring(corners, d.s, sc);
    REQUIRE(corners.labels.size() == 4);
    for (const ScreenLabel& l : corners.labels) {
        CHECK(l.role == kPaneChrome);
    }
    // ...ONE RING IN A WINDOW, and one ring of sides where the canvas's edges are a device unit.
    CHECK(window_selection_rings(d.s).size() == 1);
    Session fine = d.s;
    fine.cell_px = gfx::kCellPx;
    const Screen fine_sc = screen_of(fine);
    ScreenPlane fine_ring;
    paint_selection_ring(fine_ring, fine, fine_sc);
    REQUIRE(fine_ring.rects.size() == 4);
    for (const ScreenRect& side : fine_ring.rects) {
        CHECK(side.role == kPaneChrome);
    }
}

// ---- A REGION'S FIT (workshop/pane_text_fit.hpp) -----------------------------------------------
// How many rows and columns of a measure a rectangle holds, read both ways, where a press lands in
// its prose, and the span a selection covers on a row: the one fitting Workshop's screen and every
// pane set their rows by.
namespace region_fitting {

using zengine::graphics::kCellPx;

TEST_CASE("region: with no text metric a region is exactly its own cells") {
    // THE HINGE OF A REGION'S HONESTY. A window that measures no face has said "text is a
    // cell", and this is what that resolves to: the region's own bounds, no inset, no pixel
    // arithmetic anybody has to trust. The terminal window lives here, and so does the OS window
    // before its face opens and after a face has failed to open.
    const RegionFit f = fit_region(cells_px(3), cells_px(4), cells_px(20), cells_px(6), 0, 0);
    CHECK(f.columns == 20);
    CHECK(f.rows == 6);
    CHECK(f.origin_x == 0);
    CHECK(f.origin_y == 0);
    CHECK_FALSE(f.graphical());
    CHECK(f.view == RegionViewport{3 * kCellPx, 4 * kCellPx, 20 * kCellPx,
                                   6 * kCellPx});

    // A metric is two numbers or it is none: half an answer describes half a line
    // of text, so either missing half means cells.
    CHECK_FALSE(fit_region(0, 0, cells_px(20), cells_px(6), 8, 0).graphical());
    CHECK_FALSE(fit_region(0, 0, cells_px(20), cells_px(6), 0, 18).graphical());
    CHECK(fit_region(0, 0, cells_px(20), cells_px(6), 8, 0).columns == 20);
    CHECK(fit_region(0, 0, cells_px(20), cells_px(6), 0, 18).rows == 6);

    // A negative advance is not a size. It is a number off the wire, and it means
    // the same thing zero does rather than meaning an error.
    CHECK_FALSE(fit_region(0, 0, cells_px(20), cells_px(6), -8, -18).graphical());
    CHECK(fit_region(0, 0, cells_px(20), cells_px(6), -8, -18).columns == 20);
}

TEST_CASE("region: a real metric divides the region's pixels, inset and all") {
    // The minimum Terminal pane, at the shipped face: 56x13 cells is 672x156 device pixels, and
    // an 8px advance with an 18px line divides what is left after the inset comes off BOTH sides.
    const RegionFit f = fit_region(cells_px(22), cells_px(9), cells_px(56), cells_px(13), 8, 18);
    REQUIRE(f.graphical());
    CHECK(f.view == RegionViewport{22 * 12, 9 * 12, 672, 156});
    CHECK(f.origin_x == kTextInsetPx);
    CHECK(f.origin_y == kTextInsetPx);
    CHECK(f.columns == (672 - 2 * kTextInsetPx) / 8);
    CHECK(f.rows == (156 - 2 * kTextInsetPx) / 18);
    CHECK(f.advance_px == 8);
    CHECK(f.line_px == 18);

    // THE INSET IS IN THE CAPACITY, WHICH IS THE WHOLE POINT OF IT LIVING HERE.
    // Padding a renderer consumed privately would leave a publisher wrapping
    // against 84 columns and a medium drawing 83 of them, and the difference
    // would appear as the last character of every long row falling off the edge.
    CHECK(f.columns == 83);
    CHECK(f.columns < 672 / 8);
}

TEST_CASE("region: a metric off the wire cannot make the arithmetic misbehave") {
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();

    // A LINE TALLER THAN THE REGION, OR A CHARACTER WIDER THAN IT, IS A CELL REGION. Zero,
    // "nothing fits", was reachable in a running application: a region ONE CELL TALL holds
    // (12 - 2*inset) / 18 = no rows of this repository's own face (the Inspector's editable
    // row), and a fit with no rows draws nothing. So the fit falls back to what a zero metric
    // already means: text is a cell here.
    for (const RegionFit& f : {fit_region(0, 0, cells_px(4), cells_px(2), 8, 4000),
                              fit_region(0, 0, cells_px(4), cells_px(2), 4000, 18),
                              fit_region(0, 0, cells_px(4), cells_px(2), kMax, kMax)}) {
        CHECK_FALSE(f.graphical());   // this medium cannot set THIS region in its own type
        CHECK(f.advance_px == 0);     // ...and says so in the same words a faceless one does
        CHECK(f.line_px == 0);
        CHECK(f.columns == 4);        // the region's own cell bounds, exactly
        CHECK(f.rows == 2);
        CHECK(f.origin_x == 0);       // no inset: a cell projection has none
        CHECK(f.origin_y == 0);
    }

    // IT IS THE SAME ANSWER A ZERO METRIC GIVES, byte for byte, which is what makes the
    // fallback a sentence this vocabulary already knows how to say rather than a fourth state.
    CHECK(fit_region(0, 0, cells_px(4), cells_px(2), 8, 4000) == fit_region(0, 0, cells_px(4), cells_px(2), 0, 0));
    CHECK(fit_region(0, 0, cells_px(1), cells_px(1), 8, 18) == fit_region(0, 0, cells_px(1), cells_px(1), 0, 0));

    // A region with no bounds, or bounds off the number line: still an answer.
    CHECK(fit_region(0, 0, cells_px(-5), cells_px(-5), 8, 18).columns == 0);
    CHECK(fit_region(0, 0, cells_px(-5), cells_px(-5), 0, 0).columns == 0);
    CHECK(fit_region(gfx::px_of_cells(kMax), gfx::px_of_cells(kMax), gfx::px_of_cells(kMax),
                     gfx::px_of_cells(kMax), 8, 18)
              .view.w > 0);
    CHECK(fit_region(gfx::px_of_cells(kMin), gfx::px_of_cells(kMin), gfx::px_of_cells(kMin),
                     gfx::px_of_cells(kMin), 8, 18)
              .view.w == 0);

    // A REGION BIG ENOUGH FOR ONE ROW OF TYPE STILL GETS IT, so the fallback is a floor and
    // not a ceiling: two cells is 24 pixels, which holds this face's 18-pixel line.
    const RegionFit two = fit_region(0, 0, cells_px(4), cells_px(2), 8, 18);
    CHECK(two.graphical());
    CHECK(two.rows == (2 * 12 - 2 * kTextInsetPx) / 18);
    CHECK(two.rows == 1);
    CHECK(two.columns == (4 * 12 - 2 * kTextInsetPx) / 8);

    // The inset can be larger than the region itself, and a region so small that the inset
    // eats it answers zero -- never a negative width.
    CHECK(fit_region(0, 0, cells_px(1), cells_px(1), 8, 18).columns == 1); // one cell, as cells
    CHECK(fit_region(0, 0, cells_px(1), cells_px(1), 8, 18).rows == 1);
    CHECK(fit_region(0, 0, 0, 0, 8, 18).columns == 0);
    CHECK(fit_region(0, 0, 0, 0, 8, 18).rows == 0);
}

TEST_CASE("pointing: a pixel inside a region lands on a prose column and row") {
    // The same floored division `cell_of_pixel` performs, one step finer, and
    // resolved with the fit the rows were DRAWN with rather than with a metric
    // read separately.
    const RegionFit fit = fit_region(cells_px(22), cells_px(9), cells_px(56), cells_px(13), 8, 18);
    const std::int64_t x0 = 22 * kCellPx + kTextInsetPx;
    const std::int64_t y0 = 9 * kCellPx + kTextInsetPx;

    CHECK(prose_column_of_pixel(x0, fit) == 0);
    CHECK(prose_column_of_pixel(x0 + 7, fit) == 0);
    CHECK(prose_column_of_pixel(x0 + 8, fit) == 1);
    CHECK(prose_row_of_pixel(y0, fit) == 0);
    CHECK(prose_row_of_pixel(y0 + 17, fit) == 0);
    CHECK(prose_row_of_pixel(y0 + 18, fit) == 1);

    // FLOORED, so a pixel to the LEFT of the region's prose is column -1 and not
    // column 0. Truncating division would put a press just outside the pane onto
    // its first character.
    CHECK(prose_column_of_pixel(x0 - 1, fit) == -1);
    CHECK(prose_row_of_pixel(y0 - 1, fit) == -1);

    // A PROJECTION, NOT A HIT TEST: past the region is an answer, not an error.
    CHECK(prose_column_of_pixel(x0 + 8 * 500, fit) == 500);
    CHECK(prose_column_of_pixel(x0 + 8 * 500, fit) > fit.columns);

    // Under a cell-projection fit it degrades to the cell answer, which is the
    // truthful one for a medium whose character IS a cell.
    const RegionFit cells = fit_region(cells_px(22), cells_px(9), cells_px(56), cells_px(13), 0, 0);
    CHECK(prose_column_of_pixel(22 * kCellPx + 5, cells) == 0);
    CHECK(prose_column_of_pixel(23 * kCellPx, cells) == 1);
    CHECK(prose_row_of_pixel(10 * kCellPx, cells) == 1);

    // Total over the number line, both fits: these are wire values on both sides.
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    CHECK(prose_column_of_pixel(kMin, fit) < 0);
    CHECK(prose_column_of_pixel(kMax, fit) > 0);
    CHECK(prose_row_of_pixel(kMin, cells) < 0);
    CHECK(prose_row_of_pixel(kMax, cells) > 0);
}

TEST_CASE("selection_span_of_row is one rule, total over garbage") {
    ScreenRegion r;
    r.sel_begin_row = 0;
    r.sel_begin_col = 2;
    r.sel_end_row = 2;
    r.sel_end_col = 3;

    // The three row kinds: begin row to its own end, middle rows whole, end row to the col.
    CHECK(selection_span_of_row(r, 0, 6).begin == 2);
    CHECK(selection_span_of_row(r, 0, 6).end == 6);
    CHECK(selection_span_of_row(r, 1, 4).begin == 0);
    CHECK(selection_span_of_row(r, 1, 4).end == 4);
    CHECK(selection_span_of_row(r, 2, 6).begin == 0);
    CHECK(selection_span_of_row(r, 2, 6).end == 3);
    // Rows outside the range, and rows with no text, cover nothing.
    CHECK_FALSE(selection_span_of_row(r, 3, 6).present());
    CHECK_FALSE(selection_span_of_row(r, 1, 0).present());
    // Clamped into the text that exists: a position past a short row covers what is there.
    CHECK(selection_span_of_row(r, 2, 2).end == 2);
    CHECK(selection_span_of_row(ScreenRegion{}, 0, 6).present() == false);

    // GARBAGE IS THE ABSENCE, NEVER A GUESS: absent, reversed rows, reversed columns on one
    // row, and an empty range all answer nothing for every row.
    ScreenRegion bad;
    bad.sel_begin_row = 2;
    bad.sel_end_row = 0;
    CHECK_FALSE(selection_span_of_row(bad, 1, 6).present());
    bad.sel_begin_row = 1;
    bad.sel_end_row = 1;
    bad.sel_begin_col = 4;
    bad.sel_end_col = 2;
    CHECK_FALSE(selection_span_of_row(bad, 1, 6).present());
    bad.sel_end_col = 4; // empty
    CHECK_FALSE(selection_span_of_row(bad, 1, 6).present());
    // ...and a begin column off the row's left is clamped, not refused.
    ScreenRegion wide;
    wide.sel_begin_row = 0;
    wide.sel_begin_col = -3;
    wide.sel_end_row = 0;
    wide.sel_end_col = 2;
    CHECK(selection_span_of_row(wide, 0, 6).begin == 0);
    CHECK(selection_span_of_row(wide, 0, 6).end == 2);
}

TEST_CASE("region_px_for is fit_region read backwards, and minimal") {
    // THE ONE MEASURER, BOTH DIRECTIONS: a publisher that wants a region sized to its
    // content asks THIS function, so the inset, the division and the cell fallback stay
    // one arithmetic. The property, swept over metrics a real medium reports and asks a
    // real level makes: the answer satisfies the forward read, and one unit less on either
    // axis -- a pixel where the medium sets type, a cell where text is a cell -- no longer does.
    const std::int64_t metrics[][2] = {{8, 18}, {7, 15}, {9, 21}, {0, 0}};
    for (const auto& m : metrics) {
        const std::int64_t less = m[0] > 0 ? 1 : kCellPx;
        for (std::int64_t columns = 1; columns <= 48; columns += 7) {
            for (std::int64_t rows = 1; rows <= 20; rows += 3) {
                CAPTURE(m[0]);
                CAPTURE(columns);
                CAPTURE(rows);
                const RegionPixels px = region_px_for(columns, rows, m[0], m[1]);
                const RegionFit forward = fit_region(0, 0, px.w, px.h, m[0], m[1]);
                CHECK(forward.columns >= columns);
                CHECK(forward.rows >= rows);
                // MINIMAL WITHIN ITS OWN PROJECTION: one unit less no longer holds the ask. (A
                // region that small may instead FALL BACK to the cell projection, a different
                // honest presentation, not a smaller version of this one.)
                const RegionFit less_w = fit_region(0, 0, px.w - less, px.h, m[0], m[1]);
                if (less_w.graphical() == forward.graphical()) {
                    CHECK(less_w.columns < columns);
                }
                const RegionFit less_h = fit_region(0, 0, px.w, px.h - less, m[0], m[1]);
                if (less_h.graphical() == forward.graphical()) {
                    CHECK(less_h.rows < rows);
                }
            }
        }
    }
    // A NON-POSITIVE ASK IS AN EMPTY EXTENT, never a negative one.
    CHECK(region_px_for(0, 5, 8, 18) == RegionPixels{});
    CHECK(region_px_for(5, -1, 8, 18) == RegionPixels{});
}

} // namespace region_fitting

// ---- One writer: every change to the live desk is asked of the layouts' owner ----------------

namespace {

/// EVERY EDIT WORKSHOP ASKS THE LAYOUTS' OWNER, op by op, read off the bus as it is delivered.
loom::ObserverId tap_layout_edits(loom::Switchboard& bus, std::vector<std::string>& ops) {
    return bus.add_observer([&ops](const loom::BusEvent& ev) {
        if (ev.kind != loom::EventKind::Delivered || ev.payload == nullptr ||
            ev.schema_name != LayoutEditRequested::zen_name) {
            return;
        }
        for (const LayoutEdit& e : loom::from_value<LayoutEditRequested>(*ev.payload).edits) {
            ops.push_back(e.op);
        }
    });
}

} // namespace

TEST_CASE("every writer of the live desk asks the layouts' owner, and the picture is the owner's next copy") {
    // ⚔ MUTATION: a gesture writing Workshop's copy of the desk itself -- its op never reaches
    // the owner, and the copy Workshop draws parts from the owner's desk.
    TempDir dir("one-writer");
    FineRig t;
    Live k;
    k.host.setup_path = dir.file("setup.json");
    k.ready();
    k.extent_px(cells_px(160), cells_px(44));
    std::vector<std::string> ops;
    (void)tap_layout_edits(t.bus, ops);
    (void)tap_layout_edits(k.bus, ops);
    const auto asked = [&ops](Live& r, const std::string& op, const auto& gesture) {
        CAPTURE(op);
        ops.clear();
        gesture();
        CHECK(std::find(ops.begin(), ops.end(), op) != ops.end());
        // THE PICTURE IS THE OWNER'S: Workshop's copy is the live desk the owner holds.
        CHECK(r.session().setup.active == live_desk(r.owner->layouts().run()));
        CHECK(r.session().setup.revision == r.owner->layouts().revision());
    };
    // A DRAG'S RELEASE.
    asked(t, "window", [&t] {
        const PixelRect at = t.builder_rect();
        t.press_at(at.x + 30, at.y + 20, input::space::kPixels);
        t.motion_at(at.x + 30 + cells_px(2), at.y + 20, input::space::kPixels);
        t.hand_moment(input::PointerButton{1, false, at.x + 30 + cells_px(2), at.y + 20,
                                           input::space::kPixels, input::mod::kNone});
    });
    // A LAUNCH SEATS A PANE.
    asked(k, "seat", [&k] { open_pane(k, ref_of(stock::kKind)); });
    // `s`, THEN `r`: the file written, then put live again.
    asked(k, "save", [&k] { k.key(input::scan::kS); });
    asked(k, "restore", [&k] { k.key(input::scan::kR); });
    // EACH LAYOUT GESTURE.
    asked(k, "new", [&k] { layout_key(k, Act::kLayoutNew); });
    asked(k, "duplicate", [&k] { duplicate_live_layout(k); });
    asked(k, "move", [&k] { tab_menu_act(k, "layout.move-left", "Order"); });
    asked(k, "step", [&k] { layout_key(k, Act::kLayoutNext); });
    asked(k, "remove", [&k] { layout_key(k, Act::kLayoutRemove); });
}

TEST_CASE("two edits judged from one picture: the first is written, the second refused whole, and the picture is the owner's") {
    // ⚔ MUTATION: an edit judged from an older picture applied anyway -- the second edit lands on
    // a desk its gesture never saw.
    Live t;
    t.ready();
    t.extent_px(cells_px(160), cells_px(44));
    open_pane(t, ref_of(stock::kKind));
    t.owner->read_host_now();
    LayoutStore& store = t.owner->store();
    const std::int64_t seen = store.revision();
    const Setup before = live_desk(store.run());
    LayoutEdit made;
    made.op = layout_op::kNew;
    LayoutEdit moved;
    moved.op = layout_op::kWindow;
    moved.provider = ref_of(stock::kKind).provider;
    moved.pane = ref_of(stock::kKind).pane;
    moved.place = setup_persist::place_out(PanePlace{pane_unit::kPixels, cells_px(6), cells_px(6)});
    moved.width = setup_persist::size_out(PaneSize{});
    moved.height = setup_persist::size_out(PaneSize{});
    REQUIRE(store.edit(LayoutEditRequested{seen, {made}}).accepted);
    const LayoutEditAnswered second = store.edit(LayoutEditRequested{seen, {moved}});
    CHECK_FALSE(second.accepted);
    CHECK(second.refusal == "the layout changed -- the next picture is the current one");
    // ...NOTHING OF IT WAS WRITTEN: the new layout stands, and the first desk is as it was.
    REQUIRE(layout_run(store.run()).size() == 2);
    CHECK(layout_run(store.run())[0].desk == before);
    // AND THE PICTURE WORKSHOP PRESENTS IS THE OWNER'S, once it is sent.
    t.nudge_owner();
    t.bus.drain_until_idle();
    CHECK(t.session().setup.active == live_desk(store.run()));
    CHECK(layout_count(t.session().setup) == 2);
}

TEST_CASE("a batch's setup file is written once every edit in it is judged, and not at all when one is refused") {
    // ⚔ MUTATION: the save written as its own edit is judged, before the rest of the batch -- a
    // refused batch leaves the file written.
    TempDir dir("batch-save");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    t.ready();
    t.extent_px(cells_px(160), cells_px(44));
    t.owner->read_host_now();
    LayoutStore& store = t.owner->store();
    LayoutEdit save;
    save.op = layout_op::kSave;
    LayoutEdit gone;
    gone.op = layout_op::kSwitch;
    gone.layout = 9999;
    const std::int64_t seen = store.revision();
    const LayoutEditAnswered refused = store.edit(LayoutEditRequested{seen, {save, gone}});
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal == "that layout is gone -- nothing was switched");
    CHECK_FALSE(std::filesystem::exists(t.host.setup_path));
    CHECK(store.revision() == seen);
    // ONE BATCH SAVES ONE FILE: a second save would make a failed write leave the first written.
    const LayoutEditAnswered twice = store.edit(LayoutEditRequested{seen, {save, save}});
    CHECK_FALSE(twice.accepted);
    CHECK(twice.refusal == "one edit saves one setup file");
    CHECK_FALSE(std::filesystem::exists(t.host.setup_path));
    // ...AND THE SAME SAVE, JUDGED WHOLE, IS WRITTEN.
    REQUIRE(store.edit(LayoutEditRequested{seen, {save}}).accepted);
    CHECK(std::filesystem::exists(t.host.setup_path));
}

TEST_CASE("a batch whose setup file cannot be written is refused whole, the run as it was") {
    TempDir dir("batch-save-fails");
    std::filesystem::create_directories(dir.file("taken"));
    Live t;
    t.host.setup_path = dir.file("taken"); // a folder: no file can be written there
    t.ready();
    t.extent_px(cells_px(160), cells_px(44));
    t.owner->read_host_now();
    LayoutStore& store = t.owner->store();
    LayoutEdit made;
    made.op = layout_op::kNew;
    LayoutEdit save;
    save.op = layout_op::kSave;
    const std::int64_t seen = store.revision();
    const LayoutEditAnswered answered = store.edit(LayoutEditRequested{seen, {made, save}});
    CHECK_FALSE(answered.accepted);
    CHECK_FALSE(answered.refusal.empty());
    CHECK(layout_run(store.run()).size() == 1);
    CHECK(store.revision() == seen);
}

TEST_CASE("WL-DESK-19: a pane the layout never had opens at the room's centre, placed by its title and edges while its keys and body stay its own; shown again after a hide it comes back where it was left") {
    // MUTATION: a seat that forgets where a pane was left -- the second open stands at the centre
    // and is placed again; the checks after the hide go red.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    REQUIRE(hand_launch(t, stock_ref()).opened);
    const SetupPane* row = pane_of(t.session().setup.active, stock_ref());
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == ui::kPlaceCenter);
    CHECK(t.session().arrange.open);
    CHECK(t.session().arrange.opened);
    CHECK(t.session().arrange.pane == stock_ref());
    CHECK(keyboard_context(t.session()) != KeyContext::kArrangePane);
    // ITS TITLE TAKES THE HAND, and the move says where it stood and where it stands alone.
    const Screen sc = screen_of(t.session());
    const PixelRect rect = bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect;
    const PixelRect body = canvas_body_place(
        rect, sc, external_title_rows(t.session().panes, stock::kKind, t.session().pane_titles));
    const std::int64_t title_px = body.y - gfx::kCellPx;
    REQUIRE(title_px > rect.y);
    const std::int64_t wx = (rect.x + rect.w / 2) / gfx::kCellPx - kRoomCellX;
    const std::int64_t wy = title_px / gfx::kCellPx - kRoomCellY;
    t.press(wx, wy);
    REQUIRE(t.session().pane_drag.active);
    // ...WORKSHOP'S AREA IN FRONT FOR THE DRAG ALONE, so a window keeps it to its release.
    CHECK(desk_in_front(t.session()));
    t.motion(wx - 6, wy + 3);
    t.release(wx - 6, wy + 3);
    CHECK_FALSE(desk_in_front(t.session()));
    CHECK(t.notice().rfind(std::string("moved ") + stock::kName + ": @", 0) == 0);
    CHECK(t.notice().find(" -> @") != std::string::npos);
    CHECK(t.notice().find("(open)") == std::string::npos);
    CHECK(t.session().arrange.opened);
    const SetupPane left = *pane_of(t.session().setup.active, stock_ref());
    CHECK(left.place.mode == pane_unit::kPixels);
    // ...A MODIFIER PRESSED ALONE ENDS NOTHING -- a window says Alt when it takes the foreground...
    t.key(kFirstModifierScan + 2); // left Alt
    CHECK(t.session().arrange.opened);
    // ...AND A KEY IS THE PANE'S: it ends the placing, said, and moves nothing.
    t.key(input::scan::kD);
    CHECK_FALSE(t.session().arrange.open);
    CHECK(t.notice().find("ended placing it") != std::string::npos);
    REQUIRE(has_pane(t.session().setup.active, stock_ref()));
    CHECK(*pane_of(t.session().setup.active, stock_ref()) == left);
    REQUIRE(hand_close(t, stock_ref()).closed);
    REQUIRE_FALSE(has_pane(t.session().setup.active, stock_ref()));
    REQUIRE(hand_launch(t, stock_ref()).opened);
    const SetupPane* back = pane_of(t.session().setup.active, stock_ref());
    REQUIRE(back != nullptr);
    CHECK(back->place == left.place);
    CHECK(back->width == left.width);
    CHECK(back->height == left.height);
    CHECK_FALSE(t.session().arrange.open);
}

TEST_CASE("WL-DESK-19: a press in the body of a pane being placed is the pane's, and ends the placing") {
    Live t;
    (void)mount_tool(t, "zengine-snake");
    REQUIRE(hand_launch(t, stock_ref()).opened);
    REQUIRE(t.session().arrange.opened);
    const Screen sc = screen_of(t.session());
    const PixelRect rect = bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect;
    const PixelRect body = canvas_body_place(
        rect, sc, external_title_rows(t.session().panes, stock::kKind, t.session().pane_titles));
    const SetupPane before = *pane_of(t.session().setup.active, stock_ref());
    // ITS TOP EDGE MOVES IT, as its title does: in a window the title stands on that edge.
    const std::int64_t mid = (rect.x + rect.w / 2) / gfx::kCellPx - kRoomCellX;
    const std::int64_t top = rect.y / gfx::kCellPx - kRoomCellY;
    t.press(mid, top);
    REQUIRE(t.session().pane_drag.active);
    CHECK_FALSE(t.session().pane_drag.sizing);
    t.release(mid, top);
    REQUIRE(t.session().arrange.opened);
    const std::int64_t wx = (body.x + body.w / 2) / gfx::kCellPx - kRoomCellX;
    const std::int64_t wy = (body.y + body.h / 2) / gfx::kCellPx - kRoomCellY;
    t.press(wx, wy);
    CHECK_FALSE(t.session().pane_drag.active);
    CHECK_FALSE(t.session().arrange.open);
    CHECK(t.session().panes.selected == stock::kKind);
    t.motion(wx + 4, wy + 2);
    t.release(wx + 4, wy + 2);
    CHECK(*pane_of(t.session().setup.active, stock_ref()) == before);
}

TEST_CASE("WL-DESK-19: where workshop.open-at says mouse, a pane the layout never had opens at the hand") {
    TempDir dir("open-at-mouse");
    prefs_persist::Prefs kept;
    REQUIRE(prefs_persist::put_workshop_value(
        kept, prefs_persist::kOpenAtKey,
        PaneSetting{prefs_persist::kOpenAtKey, std::nullopt, std::nullopt,
                    std::string(prefs_persist::kOpenAtMouse)}));
    const std::string path = dir.file("open-at.json");
    REQUIRE(prefs_persist::save_file(path, kept).accepted);
    Live t;
    t.host.prefs_path = path;
    (void)mount_tool(t, "zengine-snake");
    t.press(30, 12); // on the bare room: the hand is seen there
    t.release(30, 12);
    REQUIRE(t.session().hand_px.has_value());
    REQUIRE(t.session().open_at_mouse);
    REQUIRE(hand_launch(t, stock_ref()).opened);
    const SetupPane* row = pane_of(t.session().setup.active, stock_ref());
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == pane_unit::kPixels);
    const PixelRect at = room_of_canvas(
        PixelRect{t.session().hand_px->x, t.session().hand_px->y, 0, 0}, screen_of(t.session()));
    CHECK(row->place.x == at.x);
    CHECK(row->place.y == at.y);
}

TEST_CASE("WL-ARR-18: arranging says so at the room's top, naming what it arranges, and in a window every other pane fades") {
    // MUTATION: no veil -- the window drawing has no fade over the pane not arranged.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    (void)seat_pane(t, second_ref());
    REQUIRE_FALSE(t.session().arrange.open);
    REQUIRE(hand_launch(t, stock_ref()).opened);
    REQUIRE(t.session().arrange.open);
    const Screen sc = screen_of(t.session());
    const auto said = [&](const std::string& words) {
        ScreenPlane layer;
        paint_pane_affordances(layer, t.session(), sc);
        return std::any_of(layer.labels.begin(), layer.labels.end(),
                           [&](const ScreenLabel& l) { return l.text == words; });
    };
    CHECK(said(kArrangingCaption));
    CHECK(said(stock::kName));
    // ...ON A GROUND THAT CLEARS THE PANE IT ARRANGES, whose title the hand takes it by.
    const PixelRect caption = arranging_caption_rect(t.session(), sc);
    const PixelRect held = bounds_of(t.session().panes, t.session().setup.active, stock::kKind, sc).rect;
    CHECK_FALSE((caption.x < held.x + held.w && held.x < caption.x + caption.w &&
                 caption.y < held.y + held.h && held.y < caption.y + caption.h));
    // ...THE OVERLAY'S, which hears nothing, while a pane opened is placed; Workshop's own area
    // while it arranges in front of the panes.
    const auto fades = [&]() {
        std::size_t n = 0;
        const gfx::Sketch desk = workshop_window_drawing(t.session()).sketch;
        const gfx::Sketch overlay = workshop_overlay_drawing(t.session(), {});
        for (const gfx::Sketch* drawn : {&desk, &overlay}) {
            for (const gfx::Node& node : drawn->nodes()) {
                if (node.path.find("/veil/fade") != std::string::npos) ++n;
            }
        }
        return n;
    };
    CHECK(fades() > 0);
    // ...AND THE DESK'S ARRANGEMENT SAYS `all panes`, fading none.
    t.key(input::scan::kEscape);
    release_keys(t); // the stand-in kept the keys its open gave it
    enter_arrange_desk(t);
    CHECK(said(kArrangingCaption));
    CHECK(said("all panes"));
    CHECK(fades() == 0);
    t.key(input::scan::kEscape);
    CHECK_FALSE(said(kArrangingCaption));
}
