// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop screen suite — composition and geometry: what is painted where, and what
// a hand can reach.
//
// One canvas, asserted as a value: the rectangle, the ring, the object list, the
// inspector, the authored-versus-resolved split, and a refusal visible on it.
//
// THE GEOMETRY CLAIMS HERE ARE INTEGRATION CLAIMS. Resolution and hit testing belong to
// the UI package and are proven in the `ui` suite; what these cases prove is that
// Workshop's answers COME from there — that the painted rectangle, the inspector's
// resolved reading and the reply to a click are all derived from one ui::Scene. Three
// separate call sites would have that property only by one person having written all
// three.
//
// What it holds: the whole screen as a value; a document larger than its panel and a
// notice longer than its line; one authored object read in another's frame; a panel
// occupying POINTER space and not only pixels; a region a hand cannot reach through and
// an eye cannot either; the reserved column that is nobody's to spend; the front the host
// hits being the front the medium paints; and the fine lattice, where the pane
// arrangement is sub-cell and gestures are pixel-responsive.
//
// What headless cannot prove is that a Skin carries the canvas to a human's eyes. That is
// the Surface suite's job (a canvas is a frame: same hello, same counter) plus the live
// run in the report.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// The historical session shapes and their conversions -- an artifact's material since MIG-0,
// named here because this suite owns the whole-cell-to-fine-lattice claim.
#include "workshop/session_history.hpp"

// ============================================================================
// Tier 3 — the whole screen, as a value
// ============================================================================

// ⭐ THE INFO PANEL'S PICTURE LEFT THIS SUITE WITH THE PANEL, AND THE CASES ARE NAMED HERE.
// Twenty cases stood in this file about what the Info panel LOOKED like and where a pointer met
// it: the ringed-listed-inspected screen and its rasterized twin, the live draft and its
// refusal, the empty document's sentence, the four object-list windowing cases and their
// omission markers, Info's column in pointer space, its backdrop, its bounds, its removal and
// return, HD-10's byte-identical panel and its typed-medium twin, QR-3's long name through the
// real property editor, and the two pane-in-front-of-a-control cases. Every one of them read
// `paint_info` or `info_body_place`.
//
// WHERE EACH CLAIM LIVES NOW. The pane's composition, its windows, its omission markers, its
// draft and its presses are the pane's, in `tests/test_workshop_panes_info.cpp`, driven through
// the real loaded image. What a WINDOW over a list does is `list_window`'s and is still pinned
// by the Pane Manager's, the Attention projection's and the pane-state cases that spend it.
// What this suite keeps is the screen: the composition in cells, the placement path, occupancy,
// the press chain, the terminal pane, and every pane's rectangle including the Info pane's --
// asked of a PANE now rather than of a panel this host draws.

TEST_CASE("a name longer than the workspace is clipped by Workshop, not spilled") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "a-name-far-too-long-for-here", 44, 0,
                                     ui::Extent{ui::kExtentCells, 2}, ui::Extent{ui::kExtentCells, 1});
    Session s;
    s.selected = id;
    refocus(d, s);

    const surface::SurfaceCanvas c = paint(d, s);
    // TWO BOUNDS MEET HERE AND THE TIGHTER ONE WINS. The object is 2 cells wide and sits 4
    // cells from the workspace's right edge (48 - 44), so the name's room is 2: the
    // MATERIAL's, since QR-3, and never more than the distance to the edge. Workshop does its
    // own layout, so the clip is Workshop's job either way -- the canvas would happily have
    // run the name into the panel.
    //
    // AND THE CUT IS MARKED (TYPE-0). It used to be a silent `resize` to the room, which
    // handed a maker a shorter name that looked finished -- INTR-0's defect, in the one
    // place a maker's OWN word is drawn. `detail::fit` spends the room on the mark, and a
    // room smaller than the mark itself spends all of it: two cells say `..`, which is a
    // maker's signal that there is a name here and no room to read it, rather than `a-`.
    CHECK(label_at(c, 44, kWorkspaceY) == "..");
    // The document is untouched: what was fitted is the picture, never the name.
    CHECK(doc::find(d, id)->label == "a-name-far-too-long-for-here");
}

// (...and the two windowing cases above are part of the same set: they read the panel's object
// lines and its omission markers out of the canvas.)

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

                // The plan fits its budget. (With no rows there is no panel, so
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

TEST_CASE("a refusal longer than the notice line says so, and the session keeps all of it") {
    // THE STRONG WITNESS. Met live, and answered by shortening one producer's
    // wording: the rendered cycle is budgeted in characters
    // (kMaxChainChars) so that ORDINARY identities fit. Identities are int64 and
    // a document arrives from a FILE, so "ordinary" is not a bound -- and the
    // general presentation rule has to hold exactly where a particular
    // producer's wording stops happening to.
    WorkshopDoc d;
    WorkshopDoc candidate;
    candidate.next_id = 9000000000000003;
    candidate.elements.push_back(ui::Element{9000000000000001, "panel", ui::kRootContext, 0, 0,
                                             ui::Extent{ui::kExtentCells, 2},
                                             ui::Extent{ui::kExtentCells, 1}});
    candidate.elements.push_back(ui::Element{9000000000000002, "panel", 9000000000000001, 0, 0,
                                             ui::Extent{ui::kExtentCells, 2},
                                             ui::Extent{ui::kExtentCells, 1}});
    REQUIRE(doc::restore(d, candidate).accepted);

    // An ordinary rewire that would close a loop, refused in the ordinary way.
    // Nothing here is forged and nothing is distorted to make the sentence long.
    const Written refused = doc::set_context(d, 9000000000000001, 9000000000000002);
    REQUIRE_FALSE(refused.accepted);
    REQUIRE(refused.refusal.size() > static_cast<std::size_t>(kMinScreen.w));

    Session s;
    s.selected = 9000000000000001;
    refocus(d, s);
    s.notice = refused.refusal;
    s.notice_is_bad = true;

    const surface::SurfaceCanvas c = paint(d, s);
    const std::string shown = label_at(c, 0, kMinScreen.notice_y);
    CHECK(shown.size() == static_cast<std::size_t>(kMinScreen.w)); // it fits the line it has
    CHECK(shown.compare(shown.size() - 3, 3, "...") == 0);     // and says it did not fit
    // What IS shown is the message's own head, unaltered -- the presentation
    // shortened it and did not reword it.
    CHECK(refused.refusal.compare(0, shown.size() - 3, shown, 0, shown.size() - 3) == 0);
    // truth != presentation capacity. The screen is bounded; the message is not,
    // and a wider screen would need nothing from anybody but room.
    CHECK(s.notice == refused.refusal);
    CHECK(s.notice.size() > shown.size());
    // The role is untouched: fitting a refusal does not make it less of one.
    for (const surface::SurfaceLabel& l : all_labels(c)) {
        if (l.y == kMinScreen.notice_y) {
            CHECK(l.role == surface::role::kAlert);
        }
    }
}

// (...and `retype`, the helper that opened an inspector row and replaced its draft by keys,
// went with the three command-mode rows it spent. The two cases that used it -- the truncated
// notice through the real rasterizer and the multibyte press on a scrolled line -- were about
// the NOTICE and the TERMINAL, and both are reached through the terminal's own line now, which
// is where those two claims already had a home.)

namespace {

// ⭐ RESTORED HERE AFTER THE INFO PANEL'S CASES LEFT. These helpers sat between two of the
// deleted cases and are about the DOCUMENT and the CANVAS rather than about the panel.

/// A composed document: #1 in the workspace as a share, #2 read in #1 as a
/// share OF IT, #3 read in #1 in cells. The fixture the composition cases share,
/// built through the maker's OWN operations -- `add` then `set_context` -- so
/// nothing here is authored in a way a maker could not reach.
WorkshopDoc composed() {
    WorkshopDoc d;
    doc::add(d, "A", 4, 3, ui::Extent{ui::kExtentPercent, 50}, ui::Extent{ui::kExtentCells, 10});
    doc::add(d, "B", 2, 1, ui::Extent{ui::kExtentPercent, 50}, ui::Extent{ui::kExtentCells, 4});
    doc::add(d, "C", 1, 6, ui::Extent{ui::kExtentCells, 6}, ui::Extent{ui::kExtentCells, 2});
    REQUIRE(doc::set_context(d, 2, 1).accepted);
    REQUIRE(doc::set_context(d, 3, 1).accepted);
    return d;
}

/// Where an identity landed, as the canvas and the hit test read it.
///
/// The Scene is a NAMED LOCAL and not a temporary, and the first draft of this
/// helper got that wrong -- `placed_for(workspace_scene(d, s), id)` hands back a
/// pointer into a Scene that dies at the end of that statement, so every rect it
/// returned was read out of freed memory. It is the same defect the sanitizer
/// lane has found in committed test code before; here the ordinary lane
/// caught it, because the garbage happened to be visible in an assertion.
ui::Rect rect_of(const WorkshopDoc& d, const Session& s, std::int64_t id) {
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* p = ui::placed_for(scene, id);
    REQUIRE(p != nullptr);
    return p->rect;
}


/// Is this canvas cell inside any OPEN panel's bounds, worked out from the
/// painting path alone -- `bounds_of` per open kind, exactly as `paint_panels`
/// asks it. The point of computing it a second way is that `occupied_at` has to
/// agree with it cell for cell.
bool inside_a_painted_panel(const Panels& panels, const Screen& sc, std::int64_t x,
                            std::int64_t y) {
    for (const Panel& p : panels.open) {
        if (cells_covered(bounds_of(panels, setup_for(panels), p.kind, sc).rect)
                .contains(x, y)) {
            return true;
        }
    }
    return false;
}

/// EVERY CELL OF THE CANVAS, ASKED BOTH WAYS -- what the painting path says is
/// inside a panel, and what the pointer path says is occupied.
///
/// It is a SWEEP that reports a tally rather than a case that asserts per cell:
/// seventeen hundred assertions saying the same thing would drown the suite's
/// own totals, and a count plus the first cell that disagreed diagnoses a failure
/// just as precisely. `first_bad` is `{-1,-1}` when nothing disagreed.
struct Sweep {
    std::size_t cells = 0;    ///< how many were asked
    std::size_t occupied = 0; ///< how many the pointer path called occupied
    std::size_t painted = 0;  ///< how many the painting path called covered
    std::size_t disagreed = 0;
    std::int64_t first_bad_x = -1;
    std::int64_t first_bad_y = -1;
};

Sweep sweep_canvas(const Panels& panels, const Screen& sc) {
    Sweep out;
    for (std::int64_t y = 0; y < sc.h; ++y) {
        for (std::int64_t x = 0; x < sc.w; ++x) {
            const bool painted = inside_a_painted_panel(panels, sc, x, y);
            const bool occupied = occupied_at(panels, setup_for(panels), sc, x, y).occupied;
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

TEST_CASE("the root is the default, and it costs a maker nothing to mean it") {
    // The constraint the phase was given: composition must not make the flat
    // case ceremonious. A created object measures against the workspace because
    // nobody said otherwise -- there is no node to make, no graph to join, and
    // no field to fill in.
    WorkshopDoc d;
    Session s;
    const std::int64_t made = create(d, s);
    const ui::Element* e = doc::find(d, made);
    REQUIRE(e != nullptr);
    CHECK(e->context == ui::kRootContext);
    CHECK(rect_of(d, s, made).x == e->x); // the root's origin is 0,0, so it reads through

    // ...and the whole opening document is still flat, which is what a maker's
    // first screen shows.
    const WorkshopDoc opening = two_panels();
    for (const ui::Element& one : opening.elements) {
        CHECK(one.context == ui::kRootContext);
    }
}

TEST_CASE("a context is authored BY IDENTITY, and an identity is not a position") {
    WorkshopDoc d = composed();
    CHECK(doc::find(d, 2)->context == 1);

    // The relationship names #1. Moving #1's POSITION in the vector changes
    // nothing about it -- which a reference stored as an index could not have
    // survived.
    REQUIRE(doc::set_context(d, 3, ui::kRootContext).accepted);
    doc::add(d, "extra", 0, 0, ui::Extent{ui::kExtentCells, 2}, ui::Extent{ui::kExtentCells, 2});
    std::rotate(d.elements.begin(), d.elements.begin() + 3, d.elements.end());
    CHECK(d.elements[0].id == 4); // #1 is no longer first
    CHECK(doc::find(d, 2)->context == 1);
    Session s;
    CHECK(rect_of(d, s, 2).x == rect_of(d, s, 1).x + 2);
}

TEST_CASE("a relationship that cannot mean anything is refused, and says which") {
    WorkshopDoc d = composed();

    SUBCASE("itself") {
        const Written no = doc::set_context(d, 2, 2);
        CHECK_FALSE(no.accepted);
        CHECK(no.refusal == "#2 cannot take its context from itself");
        CHECK(doc::find(d, 2)->context == 1); // untouched
    }
    SUBCASE("nothing") {
        const Written no = doc::set_context(d, 2, 999);
        CHECK_FALSE(no.accepted);
        CHECK(no.refusal == "no object #999 to take context from");
        CHECK(doc::find(d, 2)->context == 1);
    }
    SUBCASE("a two-object loop") {
        const Written no = doc::set_context(d, 1, 2); // #2 already measures against #1
        CHECK_FALSE(no.accepted);
        CHECK(no.refusal == "#1 cannot use #2 as context: a cycle (#1 -> #2 -> #1)");
        CHECK(doc::find(d, 1)->context == ui::kRootContext);
    }
    SUBCASE("a loop three objects long") {
        REQUIRE(doc::set_context(d, 3, 2).accepted); // #3 -> #2 -> #1 -> root
        const Written no = doc::set_context(d, 1, 3);
        CHECK_FALSE(no.accepted);
        // The diagnostic names the chain, which is the difference between a
        // maker who can fix it and one who cannot -- AND IT FITS ON THE NOTICE
        // LINE, which the first live run proved is not automatic: Workshop's
        // notice is one line and the canvas clips it, so a message that does
        // not fit loses exactly the part that names the loop.
        CHECK(no.refusal == "#1 cannot use #3 as context: a cycle (#1 -> #3 -> #2 -> #1)");
        CHECK(std::string("Context: " + no.refusal).size() <= static_cast<std::size_t>(kMinScreen.w));
        CHECK(doc::find(d, 1)->context == ui::kRootContext);
    }
    SUBCASE("a chain a long way further along") {
        // Depth changes nothing about the law: #1 measured against the far end
        // of a chain that already runs through #1 is still a loop.
        std::int64_t previous = 3;
        for (int i = 0; i < 40; ++i) {
            const std::int64_t made = doc::add(d, "link", 0, 0,
                                               ui::Extent{ui::kExtentCells, 2},
                                               ui::Extent{ui::kExtentCells, 2});
            REQUIRE(doc::set_context(d, made, previous).accepted);
            previous = made;
        }
        const Written no = doc::set_context(d, 1, previous);
        CHECK_FALSE(no.accepted);
        // A 43-link loop still fits on the notice line: the chain is cut with an
        // ellipsis rather than allowed to run off the end of what a maker sees.
        CHECK(no.refusal.find("...") != std::string::npos);
        CHECK(std::string("Context: " + no.refusal).size() <= static_cast<std::size_t>(kMinScreen.w));
        CHECK(doc::check_document(d).accepted);
    }
    SUBCASE("no such object at all") {
        CHECK(doc::set_context(d, 404, 1).refusal == "no such object");
    }
}

TEST_CASE("a rewire is ONE authored act: a refused one writes neither half") {
    // The lesson `move` and `resize` already taught, at the property that made
    // it hardest: changing a context can make an ALREADY WRITTEN coordinate
    // illegal, so the coordinates are re-judged in the proposed frame before
    // anything at all is written.
    WorkshopDoc d = composed();
    REQUIRE(doc::set_x(d, 2, -3).accepted); // legal: an offset in #1's frame

    const Written no = doc::set_context(d, 2, ui::kRootContext);
    CHECK_FALSE(no.accepted);
    CHECK(no.refusal == "#2 is at -3,1 -- the workspace starts at 0");
    // Neither the context nor the position moved.
    CHECK(doc::find(d, 2)->context == 1);
    CHECK(doc::find(d, 2)->x == -3);

    // The maker's repair is the one the message names, and then it goes through.
    REQUIRE(doc::set_x(d, 2, 5).accepted);
    CHECK(doc::set_context(d, 2, ui::kRootContext).accepted);
    CHECK(doc::find(d, 2)->context == ui::kRootContext);
}

TEST_CASE("changing a context does not rewrite the values whose meaning it changed") {
    // The decision, pinned. Editing the Context property changes exactly the
    // relationship; it does not silently rewrite x/y or an extent to keep the
    // picture still. The object VISIBLY MOVES, and that is honest -- a maker who
    // changed what a number is measured from changed what the number means, and
    // compensating would author facts they did not touch.
    WorkshopDoc d = composed();
    Session s;
    REQUIRE(doc::set_context(d, 3, ui::kRootContext).accepted);
    const ui::Element before = *doc::find(d, 3);
    const ui::Rect was = rect_of(d, s, 3);

    REQUIRE(doc::set_context(d, 3, 1).accepted);
    const ui::Element after = *doc::find(d, 3);

    CHECK(after.x == before.x);
    CHECK(after.y == before.y);
    CHECK(after.width == before.width);
    CHECK(after.height == before.height);
    CHECK(after.context == 1);
    // ...and the resolved rectangle moved by exactly #1's origin.
    const ui::Rect now = rect_of(d, s, 3);
    CHECK(now.x == was.x + rect_of(d, s, 1).x);
    CHECK(now.y == was.y + rect_of(d, s, 1).y);
}

TEST_CASE("a coordinate is a workspace cell at the root and an OFFSET in a frame") {
    // "The workspace starts at 0" is a law about the WORKSPACE and not about
    // coordinates, and its own stated reason is what says so.
    WorkshopDoc d = composed();

    // At the root: unchanged, to the cell.
    CHECK_FALSE(doc::move(d, 1, -1, 0).accepted);
    CHECK(doc::move(d, 1, -1, 0).refusal == "the workspace starts at 0");
    CHECK(doc::find(d, 1)->x == 4);
    CHECK_FALSE(doc::check_coord(-1, ui::kRootContext).accepted);

    // In a frame: an offset, and -1 means one cell before the source starts.
    CHECK(doc::check_coord(-1, 1).accepted);
    REQUIRE(doc::move(d, 2, -1, -1).accepted);
    Session s;
    CHECK(rect_of(d, s, 2).x == rect_of(d, s, 1).x - 1);
    CHECK(rect_of(d, s, 2).y == rect_of(d, s, 1).y - 1);

    // ...and a document carrying that is legal, which is the load half of the
    // same law.
    CHECK(doc::check_document(d).accepted);
    WorkshopDoc bad = d;
    bad.elements[0].x = -1; // #1 measures against the root
    CHECK(doc::check_document(bad).refusal == "#1: the workspace starts at 0");
}

TEST_CASE("two objects called the same thing are still two references") {
    // A relationship names an identity, so the oldest fixture in this suite --
    // two objects sharing a label -- has nothing to say about which one is
    // meant, and that is the point.
    WorkshopDoc d = two_panels(); // both called `panel`, ids 1 and 2
    REQUIRE(doc::set_context(d, 2, 1).accepted);
    REQUIRE(doc::rename(d, 1, "panel").accepted);
    REQUIRE(doc::rename(d, 2, "panel").accepted);
    CHECK(doc::find(d, 2)->context == 1);
    CHECK(TextForm<ContextRef>::format(ContextRef{doc::find(d, 2)->context}) == "#1");
    CHECK(TextForm<ContextRef>::format(ContextRef{doc::find(d, 1)->context}) == "root");
    CHECK(TextForm<ContextRef>::parse("#1")->id == 1);
    CHECK(TextForm<ContextRef>::parse("root")->id == ui::kRootContext);
    // Not an ordinal, and not the reserved non-identity.
    CHECK_FALSE(TextForm<ContextRef>::parse("1").has_value());
    CHECK_FALSE(TextForm<ContextRef>::parse("#0").has_value());
    CHECK_FALSE(TextForm<ContextRef>::parse("#").has_value());
    CHECK_FALSE(TextForm<ContextRef>::parse("panel").has_value());
}

// ---- The composition proofs -------------------------------------------------

TEST_CASE("moving a source moves what measures against it, and rewrites none of it") {
    WorkshopDoc d = composed();
    Session s;
    const ui::Element b_before = *doc::find(d, 2);
    const ui::Rect a_was = rect_of(d, s, 1);
    const ui::Rect b_was = rect_of(d, s, 2);

    REQUIRE(doc::move(d, 1, 10, 7).accepted);

    // A's authored position changed; B's did not; B's RESOLVED position did.
    CHECK(doc::find(d, 1)->x == 10);
    CHECK(*doc::find(d, 2) == b_before); // nothing about B was touched at all

    const ui::Rect b_now = rect_of(d, s, 2);
    CHECK(b_now.x == b_was.x + (10 - a_was.x));
    CHECK(b_now.y == b_was.y + (7 - a_was.y));
    // ...and the gap between them is exactly what B authored, still.
    CHECK(b_now.x - rect_of(d, s, 1).x == b_before.x);
}

TEST_CASE("resizing a source re-resolves a share and leaves an authored cell count alone") {
    // The Percent proof and the Cells proof are one case, because they are the
    // same claim told about the two extent modes: B is 50% OF A, C is 6 cells
    // wherever it is.
    WorkshopDoc d = composed();
    Session s;
    CHECK(rect_of(d, s, 1).w == 39); // 50% of a 78-cell workspace
    CHECK(rect_of(d, s, 2).w == 19); // 50% of that, floored
    CHECK(rect_of(d, s, 3).w == 6);  // cells

    REQUIRE(doc::set_width(d, 1, ui::Extent{ui::kExtentCells, 30}).accepted);

    CHECK(doc::find(d, 2)->width == ui::Extent{ui::kExtentPercent, 50}); // still 50%
    CHECK(doc::find(d, 3)->width == ui::Extent{ui::kExtentCells, 6});    // still 6 cells
    CHECK(rect_of(d, s, 2).w == 15); // 50% of 30
    CHECK(rect_of(d, s, 3).w == 6);  // unmoved: cells are cells in every frame
}

TEST_CASE("the workspace re-resolves a whole composed chain, and authors nothing") {
    // The Percent proof with the workspace as the thing that moves. Every
    // authored value is identical before and after, all the way down.
    WorkshopDoc d = composed();
    Session wide;
    Session narrow;
    narrow.workspace_w = 24;

    const WorkshopDoc authored_before = d;
    CHECK(rect_of(d, wide, 1).w == 39);
    CHECK(rect_of(d, wide, 2).w == 19);
    CHECK(rect_of(d, narrow, 1).w == 12); // 50% of 24
    CHECK(rect_of(d, narrow, 2).w == 6);  // 50% of that 12 -- transitively
    CHECK(rect_of(d, narrow, 3).w == 6);  // and cells do not move
    CHECK(d == authored_before);
}

TEST_CASE("document order is not dependency order, and stays paint, hit and list order") {
    // A document deliberately not in topological order: C, A, B with
    // C -> B -> A -> root. Nothing sorts it.
    WorkshopDoc d;
    const std::int64_t c = doc::add(d, "C", 1, 1, ui::Extent{ui::kExtentCells, 20},
                                    ui::Extent{ui::kExtentCells, 8});
    const std::int64_t a = doc::add(d, "A", 5, 2, ui::Extent{ui::kExtentCells, 20},
                                    ui::Extent{ui::kExtentCells, 8});
    const std::int64_t b = doc::add(d, "B", 1, 1, ui::Extent{ui::kExtentCells, 20},
                                    ui::Extent{ui::kExtentCells, 8});
    REQUIRE(doc::set_context(d, b, a).accepted);
    REQUIRE(doc::set_context(d, c, b).accepted);

    Session s;
    s.selected = a;
    refocus(d, s);
    const ui::Scene scene = workspace_scene(d, s);

    // Resolved correctly, though the work had to run A, then B, then C.
    REQUIRE(scene.items.size() == 3);
    CHECK(ui::placed_for(scene, a)->rect.x == 5);
    CHECK(ui::placed_for(scene, b)->rect.x == 6);
    CHECK(ui::placed_for(scene, c)->rect.x == 7);

    // ORDER: the scene, the object list and the document all still read C, A, B.
    CHECK(scene.items[0].id == c);
    CHECK(scene.items[1].id == a);
    CHECK(scene.items[2].id == b);
    CHECK(d.elements[0].id == c);
    // ...AND THE PICTURE THE HOST PUBLISHES IS IN THAT SAME ORDER. It used to be read off the
    // Info panel's own rows; the panel is a weave, so what this asks is the host's reading --
    // which is the thing document order is a fact about.
    CHECK(shown_object(d, s, 0) == "  #" + std::to_string(c) + " C");
    CHECK(shown_object(d, s, 1) == "> #" + std::to_string(a) + " A");
    CHECK(shown_object(d, s, 2) == "  #" + std::to_string(b) + " B");

    // ...and the topmost thing under an overlapping cell is the LAST authored,
    // B -- the one the others depend on. Dependency order is not z-order.
    const ui::Placed* under = ui::hit(scene, 10, 5);
    REQUIRE(under != nullptr);
    CHECK(under->id == b);

    // The maker still says which is in front by authoring order, and doing so
    // leaves the dependency exactly where it was.
    std::rotate(d.elements.begin(), d.elements.begin() + 1, d.elements.end()); // A, B, C
    const ui::Scene after = workspace_scene(d, s);
    CHECK(after.items[2].id == c);
    CHECK(ui::hit(after, 10, 5)->id == c);
    CHECK(doc::find(d, c)->context == b);
}

TEST_CASE("a dependent may spill past its source, and nothing clips, owns or reorders it") {
    // A contextual relationship is not containment. Recorded as behaviour rather
    // than asserted as an intention.
    WorkshopDoc d;
    doc::add(d, "small", 5, 5, ui::Extent{ui::kExtentCells, 4}, ui::Extent{ui::kExtentCells, 2});
    doc::add(d, "spills", 0, 0, ui::Extent{ui::kExtentCells, 20},
             ui::Extent{ui::kExtentCells, 6});
    REQUIRE(doc::set_context(d, 2, 1).accepted);
    Session s;
    s.selected = 2;
    refocus(d, s);

    // PAINT: the whole rectangle reaches the canvas, not the part inside #1.
    CHECK(rect_of(d, s, 2) == ui::Rect{5, 5, 20, 6});
    CHECK(has_rect(paint(d, s), kWorkspaceX + 5, kWorkspaceY + 5, 20, 6, surface::role::kFill));

    // HIT: everywhere it is, including well outside its source.
    const ui::Scene scene = workspace_scene(d, s);
    REQUIRE(ui::hit(scene, 20, 9) != nullptr);
    CHECK(ui::hit(scene, 20, 9)->id == 2);

    // DRAG: it can be taken hold of out there, and moved further out.
    CHECK(take_hold(d, s, 20, 9) == 2);
    CHECK(drag_to(d, s, 30, 12).accepted());
    CHECK(rect_of(d, s, 2).x == 15);
    end_drag(s);

    // RESIZE: its handle is at its own far corner, not its source's.
    const Handle handle = size_handle(d, s);
    CHECK(handle.shown);
    CHECK(handle.x == rect_of(d, s, 2).x + rect_of(d, s, 2).w);
}

// ---- Direct manipulation through a context ----------------------------------

TEST_CASE("dragging a dependent authors its LOCAL position, and never touches its source") {
    WorkshopDoc d = composed();
    Session s;
    s.selected = 2;
    refocus(d, s);
    const ui::Element a_before = *doc::find(d, 1);
    const ui::Rect frame = rect_of(d, s, 1);

    // Take hold one cell into B, and drag it somewhere on the WORKSPACE.
    const ui::Rect b_was = rect_of(d, s, 2);
    REQUIRE(take_hold(d, s, b_was.x + 1, b_was.y + 1) == 2);
    REQUIRE(drag_to(d, s, 20, 9).accepted());

    // What was written is a LOCAL offset -- the global answer minus the frame's
    // origin -- and the object is where the hand put it.
    CHECK(doc::find(d, 2)->x == (20 - 1) - frame.x);
    CHECK(doc::find(d, 2)->y == (9 - 1) - frame.y);
    CHECK(rect_of(d, s, 2) == ui::Rect{19, 8, b_was.w, b_was.h});
    CHECK(doc::find(d, 2)->context == 1); // still measured against #1
    CHECK(*doc::find(d, 1) == a_before);  // and #1 was not rewritten
    end_drag(s);

    // NOW MOVE THE SOURCE. B follows, because what the drag authored was the
    // relationship's offset and not a global position baked in.
    const std::int64_t local_x = doc::find(d, 2)->x;
    REQUIRE(doc::move(d, 1, a_before.x + 3, a_before.y + 2).accepted);
    CHECK(doc::find(d, 2)->x == local_x);
    CHECK(rect_of(d, s, 2).x == 19 + 3);
    CHECK(rect_of(d, s, 2).y == 8 + 2);

    // And a second drag, after the source moved, still projects through the
    // frame the source is in NOW.
    const ui::Rect b_now = rect_of(d, s, 2);
    REQUIRE(take_hold(d, s, b_now.x, b_now.y) == 2);
    REQUIRE(drag_to(d, s, 6, 4).accepted());
    CHECK(rect_of(d, s, 2) == ui::Rect{6, 4, b_now.w, b_now.h});
    CHECK(doc::find(d, 2)->x == 6 - rect_of(d, s, 1).x);
}

TEST_CASE("a hand stops at the workspace edge for a dependent too, and the offset goes negative") {
    // The boundary policy, unchanged in kind: a hand that reaches past what
    // exists stops at the wall, authors the wall's value, and says so. What
    // changed is what "the wall's value" is authored AS -- an offset in the
    // frame, negative whenever the frame does not start at 0.
    WorkshopDoc d = composed();
    Session s;
    s.selected = 2;
    refocus(d, s);
    const ui::Rect frame = rect_of(d, s, 1);
    REQUIRE(frame.x > 0);

    const ui::Rect b_was = rect_of(d, s, 2);
    REQUIRE(take_hold(d, s, b_was.x, b_was.y) == 2);
    const Handled far_end = drag_to(d, s, -20, -20);
    CHECK(far_end.accepted());
    CHECK(far_end.clamped());
    CHECK(far_end.boundary == kAtWorkspaceStart);

    // It stopped where a maker can SEE it stop: the workspace's first cell.
    CHECK(rect_of(d, s, 2).x == doc::kFirstCell);
    CHECK(rect_of(d, s, 2).y == doc::kFirstCell);
    // ...and what that stop is authored as is the negative offset it is.
    CHECK(doc::find(d, 2)->x == -frame.x);
    CHECK(doc::find(d, 2)->y == -frame.y);
    CHECK(doc::check_document(d).accepted);
    end_drag(s);

    // A ROOT object meets the same wall and authors 0.
    // On a fresh document, because #2 is now sitting on top of #1's corner --
    // which is itself the phase working: paint order decided that, not the
    // dependency.
    WorkshopDoc fresh = composed();
    Session root;
    root.selected = 1;
    refocus(fresh, root);
    const ui::Rect a_was = rect_of(fresh, root, 1);
    REQUIRE(take_hold(fresh, root, a_was.x, a_was.y) == 1);
    const Handled at_edge = drag_to(fresh, root, -5, -5);
    CHECK(at_edge.boundary == kAtWorkspaceStart);
    CHECK(doc::find(fresh, 1)->x == doc::kFirstCell);
    CHECK(doc::find(fresh, 1)->y == doc::kFirstCell);
}

TEST_CASE("resizing a dependent's share asks for a share of its SOURCE, not the workspace") {
    // The one projection, handed the right span. There is no second one and
    // no branch on whether an object has a context: `extent_from_drag` always
    // asked "which share of this span reaches the hand", and the span was the
    // only thing that had been wrong.
    WorkshopDoc d = composed();
    Session s;
    // THE ROOM IS PINNED, AND THAT IS THE POINT OF THE CASE. Its subject is which SPAN a
    // share is taken of -- #1's, not the room's -- so the room is held at the 48 cells this
    // case's prose is written in rather than following the composition's default, which the
    // retirement of the reserved column moved to 78. What the room's own width is, and that a
    // share follows it, is witnessed by `"the workspace re-resolves a whole composed chain,
    // and authors nothing"`.
    s.workspace_w = 48;
    s.selected = 2;
    refocus(d, s);
    CHECK(rect_of(d, s, 1).w == 24); // #1 is 24 cells wide
    CHECK(rect_of(d, s, 2).w == 12);

    // Ask, by hand, for 18 resolved cells. As a share of #1's 24 that is 75%;
    // as a share of the 48-cell workspace it would have been 38%, which resolves
    // to 18 against the WORKSPACE and to 9 against #1 -- so the object the maker
    // just grew would have come back half the size.
    REQUIRE(size_to(d, s, 2, 18, 4).accepted());
    CHECK(doc::find(d, 2)->width == ui::Extent{ui::kExtentPercent, 75});
    CHECK(rect_of(d, s, 2).w == 18);

    // MODE IS PRESERVED: it is still a share, so it still follows its source.
    REQUIRE(doc::set_width(d, 1, ui::Extent{ui::kExtentCells, 40}).accepted);
    CHECK(doc::find(d, 2)->width == ui::Extent{ui::kExtentPercent, 75});
    CHECK(rect_of(d, s, 2).w == 30);

    // The far wall is 100% OF THE SOURCE, and it says so in words that are true
    // in any context.
    const Handled far_end = size_to(d, s, 2, 400, 4);
    CHECK(far_end.clamped());
    CHECK(far_end.boundary == kAtWholeContext);
    CHECK(doc::find(d, 2)->width == ui::Extent{ui::kExtentPercent, 100});
    CHECK(rect_of(d, s, 2).w == 40); // the whole of #1, not the whole workspace
}

TEST_CASE("resizing a dependent in cells stays cells, and a no-op preserves the spelling") {
    WorkshopDoc d = composed();
    Session s;
    s.selected = 3; // authored 6 cells, in #1's frame
    refocus(d, s);

    REQUIRE(size_to(d, s, 3, 9, 2).accepted());
    CHECK(doc::find(d, 3)->width == ui::Extent{ui::kExtentCells, 9});
    CHECK(rect_of(d, s, 3).w == 9);

    // The source's size changes; an absolute size does not.
    REQUIRE(doc::set_width(d, 1, ui::Extent{ui::kExtentCells, 12}).accepted);
    CHECK(rect_of(d, s, 3).w == 9);

    // ...and asking for exactly what it already resolves to re-authors nothing,
    // for a dependent's share as well as for a root object's.
    s.selected = 2;
    refocus(d, s);
    const ui::Extent share = doc::find(d, 2)->width;
    REQUIRE(size_to(d, s, 2, rect_of(d, s, 2).w, rect_of(d, s, 2).h).accepted());
    CHECK(doc::find(d, 2)->width == share);
}

TEST_CASE("the keyboard and the pointer compose identically, because they are one path") {
    WorkshopDoc d = composed();
    Session s;
    s.selected = 2;
    refocus(d, s);
    const ui::Rect was = rect_of(d, s, 2);

    // A nudge speaks the screen: one cell right is one cell right, whatever
    // frame the object is authored in.
    REQUIRE(nudge(d, s, +1, 0).accepted());
    CHECK(rect_of(d, s, 2).x == was.x + 1);
    CHECK(doc::find(d, 2)->x == 3); // the authored offset, one further along

    // ...and `grow` reaches the same projection the handle does.
    const ui::Extent share = doc::find(d, 2)->width;
    REQUIRE(grow(d, s, +1, 0).accepted());
    CHECK(doc::find(d, 2)->width.mode == share.mode);
    CHECK(rect_of(d, s, 2).w == was.w + 1);
}

// ---- Deletion ---------------------------------------------------------------

TEST_CASE("a source something measures against is not deletable, and the refusal names who") {
    WorkshopDoc d = composed(); // #2 and #3 both measure against #1
    Session s;
    s.selected = 1;
    refocus(d, s);

    const Written no = delete_selected(d, s);
    CHECK_FALSE(no.accepted);
    CHECK(no.refusal == "#2 and #3 take context from #1 -- change or delete them first");
    // Nothing moved: not the document, not the selection.
    CHECK(d.elements.size() == 3);
    CHECK(s.selected == 1);
    CHECK(doc::find(d, 2)->context == 1);

    // A DEPENDENT deletes normally, and the ordinary selection rule applies.
    s.selected = 3;
    refocus(d, s);
    REQUIRE(delete_selected(d, s).accepted);
    CHECK(d.elements.size() == 2);

    // With one dependent left the refusal is singular, and correct.
    s.selected = 1;
    refocus(d, s);
    CHECK(delete_selected(d, s).refusal ==
          "#2 takes context from #1 -- change or delete it first");

    // REWIRE, THEN DELETE. Two authored acts, both the maker's.
    REQUIRE(doc::set_context(d, 2, ui::kRootContext).accepted);
    CHECK(delete_selected(d, s).accepted);
    CHECK(d.elements.size() == 1);
    CHECK(doc::find(d, 2)->context == ui::kRootContext);
    // No dangling reference survived an accepted delete.
    CHECK(doc::check_document(d).accepted);
}

// ---- The document law, over relationships ------------------------------------

TEST_CASE("the relationship law is the document law, and a poke cannot smuggle one past it") {
    WorkshopDoc d = composed();
    CHECK(doc::check_document(d).accepted);

    SUBCASE("a source that is not there") {
        WorkshopDoc bad = d;
        bad.elements[1].context = 42;
        CHECK(doc::check_document(bad).refusal == "#2: no object #42 to take context from");
    }
    SUBCASE("an object measured against itself") {
        WorkshopDoc bad = d;
        bad.elements[0].context = 1;
        CHECK(doc::check_document(bad).refusal ==
              "#1: its context never reaches the workspace (#1 -> #1)");
    }
    SUBCASE("a loop") {
        WorkshopDoc bad = d;
        bad.elements[0].context = 2; // #1 -> #2 -> #1
        const Written no = doc::check_document(bad);
        CHECK_FALSE(no.accepted);
        CHECK(no.refusal.find("never reaches the workspace") != std::string::npos);
    }
    SUBCASE("the root itself is always available") {
        CHECK(doc::check_document(two_panels()).accepted);
    }
    SUBCASE("a deep legal chain is legal, however deep") {
        WorkshopDoc deep;
        std::int64_t previous = ui::kRootContext;
        for (int i = 0; i < 500; ++i) {
            const std::int64_t made = doc::add(deep, "link", 1, 0,
                                               ui::Extent{ui::kExtentCells, 2},
                                               ui::Extent{ui::kExtentCells, 2});
            REQUIRE(doc::set_context(deep, made, previous).accepted);
            previous = made;
        }
        CHECK(doc::check_document(deep).accepted);
        Session s;
        CHECK(rect_of(deep, s, previous).x == 500);
    }
}

// ---- Persistence -------------------------------------------------------------

TEST_CASE("the relationship round-trips by identity, and its RESULT is not in the file") {
    WorkshopDoc original = composed();
    REQUIRE(doc::move(original, 2, -1, 2).accepted); // a negative local offset, deliberately
    const std::string text = persist::to_text(original);

    // What is written: the identity. What is not: any resolved consequence of
    // it -- a frame, a global position, a cell count, a traversal order.
    CHECK(text.find("\"context\":\"1\"") != std::string::npos);
    CHECK(text.find("\"context\":\"0\"") != std::string::npos);
    for (const char* derived : {"\"frame\"", "\"global\"", "\"depth\"", "\"order\"",
                                "\"resolved\"", "\"rect\"", "\"parent\""}) {
        CHECK(text.find(derived) == std::string::npos);
    }

    WorkshopDoc live = two_panels();
    REQUIRE(persist::load_into(live, text).accepted);
    CHECK(live == original);
    CHECK(doc::find(live, 2)->context == 1);
    CHECK(doc::find(live, 2)->x == -1);

    // ...and it is the SAME relationship, not a lookalike: changing #1 still
    // changes #2, and #2's authored value stays put.
    Session s;
    const ui::Rect was = rect_of(live, s, 2);
    REQUIRE(doc::move(live, 1, 12, 8).accepted);
    CHECK(rect_of(live, s, 2).x != was.x);
    CHECK(doc::find(live, 2)->x == -1);

    // save -> load -> save is still byte-identical with relationships in it.
    WorkshopDoc again;
    REQUIRE(persist::load_into(again, persist::to_text(live)).accepted);
    CHECK(persist::to_text(again) == persist::to_text(live));
}

TEST_CASE("a composed document loaded under a different workspace rebuilds every rectangle") {
    // The strongest evidence the phase can produce inside one process; the
    // report's live witness is the same claim across two.
    const WorkshopDoc saved = composed();
    const std::string text = persist::to_text(saved);

    Session wide;
    Session narrow;
    narrow.workspace_w = 24;

    WorkshopDoc loaded;
    REQUIRE(persist::load_into(loaded, text).accepted);

    // AUTHORED: identical, to the byte.
    CHECK(loaded == saved);
    CHECK(persist::to_text(loaded) == text);
    CHECK(doc::find(loaded, 2)->context == 1);
    CHECK(doc::find(loaded, 2)->width == ui::Extent{ui::kExtentPercent, 50});

    // RESOLVED: rebuilt, and different, all the way down the chain.
    CHECK(rect_of(loaded, wide, 1).w == 39);
    CHECK(rect_of(loaded, wide, 2).w == 19);
    CHECK(rect_of(loaded, narrow, 1).w == 12);
    CHECK(rect_of(loaded, narrow, 2).w == 6);
    CHECK(rect_of(loaded, narrow, 3).w == 6); // the cells one, unmoved
}

TEST_CASE("a forged relationship never leaves Workshop halfway loaded") {
    const WorkshopDoc subject = composed();
    WorkshopDoc live = subject;
    const std::string untouched = persist::to_text(live);

    struct Forgery {
        const char* what;
        std::string from;
        std::string to;
    };
    // Each is a file the honest writer could not produce, and each has to be
    // refused by the DOCUMENT's law rather than by a second copy of it in the
    // reader.
    const Forgery forgeries[] = {
        {"a source that does not exist", "\"context\":\"1\"", "\"context\":\"77\""},
        {"an object measured against itself", "\"id\":\"2\",\"name\":\"B\",\"context\":\"1\"",
         "\"id\":\"2\",\"name\":\"B\",\"context\":\"2\""},
        {"the root turned into a loop", "\"id\":\"1\",\"name\":\"A\",\"context\":\"0\"",
         "\"id\":\"1\",\"name\":\"A\",\"context\":\"2\""},
        {"a negative identity", "\"context\":\"1\"", "\"context\":\"-4\""},
    };
    for (const Forgery& f : forgeries) {
        CAPTURE(f.what);
        const Written no = persist::load_into(live, forged(subject, f.from, f.to));
        CHECK_FALSE(no.accepted);
        CHECK_FALSE(no.refusal.empty());
        CHECK(live == subject); // byte for byte
        CHECK(persist::to_text(live) == untouched);
    }

    // An indirect loop, three long, forged the same way.
    WorkshopDoc chain = composed();
    REQUIRE(doc::set_context(chain, 3, 2).accepted); // #3 -> #2 -> #1 -> root
    const std::string looped = forged(chain, "\"id\":\"1\",\"name\":\"A\",\"context\":\"0\"",
                                      "\"id\":\"1\",\"name\":\"A\",\"context\":\"3\"");
    CHECK_FALSE(persist::load_into(live, looped).accepted);
    CHECK(live == subject);

    // ...and a document that merely LOOKS unusual is not refused: document order
    // is not dependency order in a file either.
    WorkshopDoc backwards = composed();
    std::rotate(backwards.elements.begin(), backwards.elements.begin() + 1,
                backwards.elements.end()); // B, C, A
    REQUIRE(persist::load_into(live, persist::to_text(backwards)).accepted);
    CHECK(live.elements[0].id == 2);
    CHECK(live.elements[2].id == 1);
    Session s;
    CHECK(rect_of(live, s, 2).x == rect_of(live, s, 1).x + 2);
}

TEST_CASE("a deep composed document survives a whole file round trip") {
    // The deep chain, through persistence, because a depth assumption is exactly
    // as likely to live in a loader as in a resolver.
    WorkshopDoc deep;
    std::int64_t previous = ui::kRootContext;
    for (int i = 0; i < 300; ++i) {
        const std::int64_t made = doc::add(deep, "link", 1, 0, ui::Extent{ui::kExtentCells, 2},
                                           ui::Extent{ui::kExtentCells, 2});
        REQUIRE(doc::set_context(deep, made, previous).accepted);
        previous = made;
    }

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(deep)).accepted);
    CHECK(live == deep);
    Session s;
    CHECK(rect_of(live, s, previous).x == 300);
    CHECK(persist::to_text(live) == persist::to_text(deep));
}

TEST_CASE("a document written before relationships existed is refused, and says what is missing") {
    // The written shape changed, so an older document no longer admits. That is
    // stated here rather than papered over with a migration: Workshop is
    // pre-release and its own only consumer, and no artifact in the world
    // deserves a compatibility layer yet. What matters is that the refusal is
    // CLOSED and legible -- never a silent default to the root.
    const std::string w5_era =
        "{\"zen\":1,\"schema\":\"WorkshopDocument\",\"version\":1,\"value\":{"
        "\"format\":\"zengine-workshop\",\"format_version\":\"1\",\"next_id\":\"2\","
        "\"objects\":[{\"id\":\"1\",\"name\":\"panel\",\"x\":\"3\",\"y\":\"2\","
        "\"width\":{\"mode\":\"percent\",\"amount\":\"60\"},"
        "\"height\":{\"mode\":\"cells\",\"amount\":\"6\"}}]}}";

    WorkshopDoc live = composed();
    const WorkshopDoc before = live;
    const Written no = persist::load_into(live, w5_era);
    CHECK_FALSE(no.accepted);
    CHECK_FALSE(no.refusal.empty());
    CHECK(live == before);
}

// ---- Through the message path ------------------------------------------------

// (...and two more cases went with `retype`: `"a maker authors a context in the inspector, and
// the picture follows"` and `"a delete refusal reaches the maker with the dependents named"`.
// Both drove the document THROUGH a property draft opened by keys this host no longer has. What
// they proved -- that authoring a context re-parents the picture, and that a refused delete
// names its dependents -- is the DOCUMENT's and is pinned by the document suite's own cases on
// the same operations, without a draft in the way.)

// ---- tier 7: the surface's own extent (G-2) ----------------------------------------------
//
// Until G-2 the screen was 78x22 because a canvas publisher had no way to learn how much room
// its medium had. The Surface package now says so, and everything below is what a Workshop
// makes of the answer: where the extra columns and rows go, what a maker's authored work does
// while they arrive, and what a pane does with room it did not have before.

TEST_CASE("a bigger surface is a bigger workspace, not a bigger picture of a small one") {
    // THE PHASE'S CENTRAL CLAIM, as arithmetic. Every number here is derived in one place
    // (`screen_of`) and the minimum reproduces the composition that existed before.
    CHECK(kMinScreen.w == 78);
    CHECK(kMinScreen.h == 22);

    const Screen big = screen_of(100, 33);
    CHECK(big.w == 100);
    CHECK(big.h == 33);

    // THE WORKSPACE TAKES THE EXTRA ROOM. Twenty-two more columns of surface are twenty-two
    // more columns a maker can build in; eleven more rows are eleven more rows.
    CHECK(big.room_w == kMinScreen.room_w + 22);
    CHECK(big.room_h == kMinScreen.room_h + 11);

    // ...AND THE PANEL DOES NOT. Its width is a fact about how much of a name is worth
    // showing, so it is the same column count anchored to the new right edge.
    CHECK(big.w - big.panel_x == kMinScreen.w - kMinScreen.panel_x);
    CHECK(big.panel_x == 72);

    // The bottom band keeps its shape against the bottom edge, and the top band keeps its
    // rows against the top one -- both are fixed reservations, not shares (QR-14).
    CHECK(big.h - big.notice_y == kMinScreen.h - kMinScreen.notice_y);
    CHECK(big.h - big.help_y == kMinScreen.h - kMinScreen.help_y);
    CHECK(big.notice_y == big.h - kBottomRows);   // the band's own first row
    CHECK(big.help_y + 2 == big.h - 1);           // ...and two legend rows under it

    // AND THE ROOM RUNS UNDER THE PANEL RATHER THAN STOPPING BESIDE IT. This read
    // `room_w + kPanelGap == panel_x` while the column was subtracted; the column is a place
    // now, so the room is the surface and the place stands on it.
    CHECK(big.room_w == big.w);
    CHECK(big.panel_x < big.room_w);
}

TEST_CASE("the screen's extent is TOTAL over whatever a medium published") {
    // It arrives as a ZEN_SHAPE off the bus, so its fields are whatever the sender put in
    // them -- W-1's lesson at the other end of the same telescope. Nothing below may produce
    // a negative extent, an inverted layout, or a multiply that leaves the number line.
    const std::int64_t hostile[] = {(std::numeric_limits<std::int64_t>::min)(),
                                    -1,
                                    0,
                                    1,
                                    kScreenMinW - 1,
                                    kScreenMaxW,
                                    kScreenMaxW + 1,
                                    (std::numeric_limits<std::int64_t>::max)()};
    // The metric is on the same wire and gets the same treatment (HD-1), so it is tried
    // against the same hostile set -- as its own axis rather than as a cross product with
    // the extent's. Sixty-four extents against sixty-four metrics is four thousand screens
    // and seventy thousand assertions to say a thing that neither axis needs the other to
    // say; a suite's assertion total is evidence about itself, and inflating it by two
    // orders of magnitude for one property is how that evidence stops meaning anything.
    const auto judge = [](const Screen& sc) {
        CHECK(sc.w >= kScreenMinW);
        CHECK(sc.w <= kScreenMaxW);
        CHECK(sc.h >= kScreenMinH);
        CHECK(sc.h <= kScreenMaxH);
        // and the furniture stays a layout rather than becoming a shape
        CHECK(sc.room_w >= kWorkspaceMinW);
        CHECK(sc.room_h >= 1);
        CHECK(sc.panel_x > 0);
        // ⚠ THE TERMINAL'S RECTANGLE AND INTERIOR WERE JUDGED HERE (VD-24) -- six `Screen`
        // fields and two floors, all of them one particular tool's furniture. What is left
        // is the furniture that is every pane's, and the room's own rule: the room IS the
        // surface, at every extent this screen can be asked for.
        CHECK(sc.room_w == sc.w);
        CHECK(sc.panel_x + kPanelCols == sc.w);
        CHECK(sc.notice_y < sc.h);
        CHECK(sc.help_y + 1 < sc.h);
    };

    for (const std::int64_t w : hostile) {
        for (const std::int64_t h : hostile) {
            judge(screen_of(w, h));                 // no metric: the pre-HD-1 domain
            judge(screen_of(w, h, 8, 18));          // and the shipped face's
        }
    }
    // The metric's own axis, against the minimum screen and a large one -- an advance of
    // INT64_MIN and a line height of zero are as ordinary here as a width of -1.
    for (const std::int64_t advance : hostile) {
        for (const std::int64_t line : hostile) {
            judge(screen_of(kScreenMinW, kScreenMinH, advance, line));
            judge(screen_of(200, 90, advance, line));
        }
    }
}

// ============================================================================
// Tier 11 -- a panel occupies POINTER space, not only pixels (PNL-2)
// ============================================================================
//
// WHAT PNL-1 MEASURED AND DID NOT REPAIR: with the Builder open, a press at a
// canvas cell that panel was visibly covering took hold of the object underneath
// it, selected it, and began a drag a maker could not see. The panel was a
// picture of a thing rather than a thing.
//
// THE RULE THESE CASES PIN, in the order the press is asked:
//
//     the terminal overlay, while it is open   -- it has the pointer entirely
//     a visible panel, by its resolved bounds  -- it occupies what it covers
//     the workspace and the document underneath
//
// AND THE TWO ASYMMETRIES THAT MEAN NO CAPTURE STATE EXISTS: a press on a panel
// begins nothing, so nothing later needs cancelling; a gesture that began on the
// workspace owns the pointer until its release, wherever that release lands.
//
// The boot document is #1 at workspace 3,2 (28x6) and #2 at 6,10 (14x4); the
// overlay stack's first slot is canvas {0,1,48,9} and the side region is canvas
// {50,0,28,17} on the minimum screen. So workspace (10,5) is canvas (10,6):
// inside the Builder's bounds AND on top of #1, which is the whole overlap this
// tier is about.

TEST_CASE("a visible panel occupies the pointer space it covers") {
    Live t;
    (void)mount_tool(t, "zengine-snake");
    // A SELECTION THAT IS NOT THE COVERED OBJECT, so "cannot select" is a claim
    // this case can actually make: the press below is on #1, and #2 has to still
    // be the selection afterwards.
    t.key(input::scan::kTab);
    const std::int64_t other = t.session().selected;
    REQUIRE(other == 2);

    open_stock_pane(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect panel =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
    // The press cell, named from the panel's OWN bounds and from the object's own
    // placement, so neither the panel nor the object is where this case guessed.
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* covered = ui::placed_for(scene, 1);
    REQUIRE(covered != nullptr);
    const std::int64_t wx = covered->rect.x + 7;
    const std::int64_t wy = covered->rect.y + 3;
    REQUIRE(panel.contains(wx + kWorkspaceX, wy + kWorkspaceY)); // the panel covers it
    REQUIRE(ui::hit(scene, wx, wy) != nullptr);                  // and so does the object

    const ui::Element before = *doc::find(t.doc(), 1);
    t.press(wx, wy);
    // NOTHING UNDER IT WAS REACHED: not selected, not held, not moved -- and all
    // three at once, because `take_hold` is the one door to all of them.
    //
    // ⚠ AND THE SENTENCE IS THE PANE'S NOW, WHICH IS A CONSEQUENCE OF THE ARC AND IS
    // WRITTEN DOWN HERE. The host says `<name> is here -- nothing under it can be taken hold
    // of` for a pane that owns the point and has no press vocabulary of its own; every pane
    // that can cover a workspace object HAS one now (the Editor places a caret, the Pane
    // Manager chooses a subject, an external pane's press is its provider's), so what a
    // maker reads is that pane's own answer. The OCCUPANCY -- the thing this case is named
    // for -- is unchanged and is what the lines below read. The host's sentence still
    // reaches Info, Layouts and a maker pane, and its own case is one tier down.
    CHECK(t.session().panels.selected == stock::kKind);
    CHECK(t.session().selected == other);
    CHECK_FALSE(t.session().drag.active);
    CHECK_FALSE(t.session().drag.resizing);
    CHECK(doc::find(t.doc(), 1)->x == before.x);
    CHECK(doc::find(t.doc(), 1)->y == before.y);
    t.release(wx, wy);
    CHECK_FALSE(t.session().drag.active);

    // AND THE SAME OBJECT IS REACHABLE AGAIN THE MOMENT THE PANEL IS REMOVED --
    // the same cell, the same document, the same gesture. The occlusion is the
    // panel's presence and nothing else. (The press above pointed the keys at the stand-in,
    // which takes them as every runtime pane does; the picker's `p` needs them back.)
    release_keys(t);
    pick(t, stock::kKind);
    REQUIRE_FALSE(t.session().panels.has(stock::kKind));
    t.press(wx, wy);
    CHECK(t.notice() == "holding #1 -- drag to move it");
    CHECK(t.session().selected == 1);
    CHECK(t.session().drag.active);
    t.release(wx, wy);
}

TEST_CASE("a press that lands on a panel begins nothing, so a hand that leaves it drags nothing") {
    // PRESS BEGINS ON THE PANEL, POINTER LATER LEAVES IT. There is no capture
    // state to get this right: a press on a panel never calls `take_hold`, so
    // there is no drag for the motion to continue, and the absence of the drag is
    // the whole of the memory.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    const ui::Element one_before = *doc::find(t.doc(), 1);
    const ui::Element two_before = *doc::find(t.doc(), 2);

    t.press(10, 5); // on the Builder
    CHECK_FALSE(t.session().drag.active);
    t.motion(8, 11); // off it, over #2
    t.motion(9, 12);
    t.release(9, 12);

    CHECK_FALSE(t.session().drag.active);
    CHECK(doc::find(t.doc(), 1)->x == one_before.x);
    CHECK(doc::find(t.doc(), 1)->y == one_before.y);
    CHECK(doc::find(t.doc(), 2)->x == two_before.x);
    CHECK(doc::find(t.doc(), 2)->y == two_before.y);
    // Not one frame of this said "holding": the gesture never began.
    CHECK(t.notice().find("holding") == std::string::npos);
}

TEST_CASE("a gesture that began on the workspace is not interrupted by a panel") {
    // PRESS BEGINS ON THE WORKSPACE, POINTER LATER MOVES BENEATH A PANEL. The
    // drag owns the pointer until it ends, so it keeps authoring -- and the
    // object goes where the maker's hand put it, UNDER the panel, because a panel
    // that stopped a drag at its own edge would be clamping the document. That is
    // a panel's presence becoming visible in what a maker can author, which is
    // the rule that also keeps a removed Info's column empty.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect panel =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);

    // #2 sits at workspace 6,10 -- canvas row 11, below the panel's last row.
    REQUIRE_FALSE(panel.contains(7 + kWorkspaceX, 11 + kWorkspaceY));
    t.press(7, 11);
    REQUIRE(t.session().drag.active);
    REQUIRE(t.session().drag.id == 2);

    t.motion(7, 6);
    t.motion(7, 3); // under the panel now
    CHECK(t.session().drag.active);
    const ui::Scene mid = workspace_scene(t.doc(), t.session());
    const ui::Placed* moved = ui::placed_for(mid, 2);
    REQUIRE(moved != nullptr);
    CHECK(panel.contains(moved->rect.x + kWorkspaceX, moved->rect.y + kWorkspaceY));
    const std::int64_t rested_x = moved->rect.x;
    const std::int64_t rested_y = moved->rect.y;

    // AND THE RELEASE ENDS IT, wherever the hand is. Occluding the release would
    // strand the drag with the button up, and the next motion would carry an
    // object nobody was holding.
    t.release(7, 3);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice() == "released #2");
    t.motion(20, 14); // no button down: nothing follows the pointer
    const ui::Scene after = workspace_scene(t.doc(), t.session());
    const ui::Placed* still = ui::placed_for(after, 2);
    REQUIRE(still != nullptr);
    CHECK(still->rect.x == rested_x);
    CHECK(still->rect.y == rested_y);

    // The object is genuinely under the panel now -- and genuinely out of reach
    // there, which closes the loop: the document may hold what the picture does
    // not show, and the pointer answers about the picture.
    t.press(rested_x + 1, rested_y + 1);
    CHECK_FALSE(t.session().drag.active);
}

TEST_CASE("WIND-1: the columns the panel took are its own, and the band is the maker's") {
    // BOTH SIDES OF THE TRADE, AT ONE EXTENT, THROUGH THE LIVE DOORS. WIND-1 widens a stack
    // slot from 48 to 124 cells at 200x60 -- 109 until the right column stopped coming off
    // the room, and the same half-share of a room thirty columns wider since -- and the
    // honest account of that is two sentences
    // rather than one: every added cell is opaque paint AND pointer ownership, and the
    // columns beyond it are still the maker's to reach. Neither is proved by `Rect::contains`
    // -- the paint is read off the published canvas and the presses go through the same
    // pointer path a maker's hand does.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60}));
    // A SELECTION THAT IS NOT THE COVERED OBJECT, so "cannot select" is a claim this case
    // can actually make.
    t.key(input::scan::kTab);
    const std::int64_t other = t.session().selected;
    REQUIRE(other == 2);
    open_stock_pane(t);

    const Screen sc = screen_of(t.session());
    REQUIRE(sc.room_w == 200);
    const ui::Rect panel =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
    REQUIRE(panel == ui::Rect{0, 2, 124, 9});

    // ---- INSIDE THE NEWLY OWNED AREA. Workspace column 60 was free before this phase (the
    // slot was 48 wide) and is the panel's now. #1 is underneath it, so there is genuinely
    // something for the press to have reached.
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    REQUIRE(ui::hit(scene, 60, 3) != nullptr);
    REQUIRE(ui::hit(scene, 60, 3)->id == 1);
    REQUIRE(panel.contains(60 + kWorkspaceX, 3 + kWorkspaceY));
    // ...AND IT WAS FREE BEFORE, said against the 48-column slot WIND-1 started from rather
    // than against `kMinScreen`'s slot: the minimum screen's slot is 63 columns wide now that
    // the room is the surface, so `kMinScreen` no longer names the width this phase widened.
    REQUIRE_FALSE(ui::Rect{kStackX, kStackY, kStackW, kStackRows}
                      .contains(60 + kWorkspaceX, 3 + kWorkspaceY));

    // IT IS PAINTED, and the paint is the whole rectangle rather than the old 48 columns:
    // one opaque backdrop at the panel's bounds -- which since WUX-5 is also its visible
    // boundary, in the ordinary pane chrome -- and every row of the INTERIOR padded to the
    // interior's width, so a character medium's spaces erase what is under them.
    const surface::SurfaceCanvas& c = t.canvases.back();
    CHECK(has_rect(c, panel.x, panel.y, panel.w, panel.h, kPaneChrome));
    CHECK_FALSE(has_rect(c, panel.x, panel.y, kStackW, panel.h, kPaneChrome));
    const ui::Rect body = pane_body_cells(panel);
    std::size_t padded = 0;
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == body.x && l.y >= body.y && l.y < body.y + body.h) {
            CHECK(l.text.size() == static_cast<std::size_t>(body.w));
            ++padded;
        }
    }
    CHECK(padded == static_cast<std::size_t>(body.h));

    // AND IT IS OCCUPIED. The press does not reach `take_hold`, so it cannot select, cannot
    // move and cannot resize -- all three at once, because they are that one call.
    const ui::Element before = *doc::find(t.doc(), 1);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, 60 + kWorkspaceX, 3 + kWorkspaceY).occupied);
    t.press(60, 3);
    CHECK(t.session().selected == other);
    CHECK_FALSE(t.session().drag.active);
    CHECK(doc::find(t.doc(), 1)->x == before.x);
    CHECK(doc::find(t.doc(), 1)->y == before.y);
    t.release(60, 3);

    // ---- INSIDE THE RETAINED FREE BAND. Put #2 at workspace 140 -- past the panel's right
    // edge and inside the room -- on one of the panel's OWN rows, which is the only place
    // the claim means anything. It was 120 while the slot was 109 wide; the slot is 124 now,
    // so the column that proves the claim moved with it, and 140 is still clear of the right
    // column's own place at 172. It is carried there by the pointer rather than authored, so
    // the case does not need a door the maker does not have.
    t.press(6, 10); // canvas row 11: below the panel, so the grab is legal
    REQUIRE(t.session().drag.active);
    REQUIRE(t.session().drag.id == 2);
    t.motion(140, 3);
    t.release(140, 3);

    const ui::Scene moved = workspace_scene(t.doc(), t.session());
    const ui::Placed* two = ui::placed_for(moved, 2);
    REQUIRE(two != nullptr);
    REQUIRE(two->rect.x == 140);
    REQUIRE(two->rect.y == 3);
    CHECK(two->rect.x >= panel.x + panel.w); // outside width 124...
    CHECK(two->rect.x < sc.room_w);          // ...and inside room width 200
    CHECK_FALSE(panel.contains(two->rect.x + kWorkspaceX, two->rect.y + kWorkspaceY));
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, two->rect.x + kWorkspaceX,
                            two->rect.y + kWorkspaceY)
                    .occupied);

    // ITS TOP-LEFT PRESS REACHES THE WORKSPACE OBJECT. Same row as the refused press above,
    // eighty columns further right, and the answer is the opposite one.
    t.key(input::scan::kTab); // move the selection off #2 first, again
    REQUIRE(t.session().selected == 1);
    t.press(two->rect.x, two->rect.y);
    CHECK(t.notice() == "holding #2 -- drag to move it");
    CHECK(t.session().selected == 2);
    CHECK(t.session().drag.active);

    // AND THE DRAG IT BEGAN THERE WALKS UNDER THE WIDENED PANE AND COMPLETES. PNL-2's law,
    // asked in the band this phase created: a gesture that began on the workspace owns the
    // pointer until its release, so the panel neither stops it nor clamps the document.
    t.motion(90, 3);
    CHECK(t.session().drag.active);
    t.motion(60, 3); // squarely under the widened panel now
    CHECK(t.session().drag.active);
    // THE SCENE IS NAMED, NOT A TEMPORARY. A `Placed*` outlives the `Scene` it points into,
    // which is W-2's own committed hazard and is the shape the sanitizer lane exists to name.
    const ui::Scene mid = workspace_scene(t.doc(), t.session());
    const ui::Placed* under = ui::placed_for(mid, 2);
    REQUIRE(under != nullptr);
    CHECK(panel.contains(under->rect.x + kWorkspaceX, under->rect.y + kWorkspaceY));
    t.release(60, 3);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice() == "released #2");
    const ui::Scene after = workspace_scene(t.doc(), t.session());
    const ui::Placed* rested = ui::placed_for(after, 2);
    REQUIRE(rested != nullptr);
    CHECK(rested->rect.x == 60);
    CHECK(rested->rect.y == 3);
}

TEST_CASE("a resize a panel covers cannot be started either") {
    // THE OTHER GESTURE, and the reason one is not enough: a press has two
    // possible meanings (take the size handle, or take the body) and the panel
    // has to refuse both. It does, structurally -- `take_hold` is what decides
    // between them and the press never reaches it.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    const Handle grip = size_handle(t.doc(), t.session());
    REQUIRE(grip.shown);
    REQUIRE(grip.id == 1);

    open_stock_pane(t);
    const Screen sc = screen_of(t.session());
    REQUIRE(cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                    stock::kKind, sc)
                              .rect)
                .contains(grip.x + kWorkspaceX, grip.y + kWorkspaceY));
    const ui::Element before = *doc::find(t.doc(), 1);

    t.press(grip.x, grip.y);
    CHECK_FALSE(t.session().drag.resizing);
    CHECK_FALSE(t.session().drag.active);
    t.motion(grip.x + 6, grip.y + 2); // a resize that never began authors nothing
    CHECK(doc::find(t.doc(), 1)->width.amount == before.width.amount);
    CHECK(doc::find(t.doc(), 1)->height.amount == before.height.amount);
    t.release(grip.x + 6, grip.y + 2);

    // Remove the panel and the very same press is the resize it always was.
    release_keys(t); // the press above pointed the keys at the stand-in
    pick(t, stock::kKind);
    t.press(grip.x, grip.y);
    CHECK(t.notice() == "holding #1 -- drag to resize it");
    CHECK(t.session().drag.resizing);
    t.motion(grip.x + 6, grip.y + 2);
    CHECK(doc::find(t.doc(), 1)->height.amount != before.height.amount);
    t.release(grip.x + 6, grip.y + 2);
}

TEST_CASE("the picker occupies the slot it opens over, and answers for it while it is there") {
    // THE PICKER IS A MODE AND NOT A PANEL -- no catalog row, no instance -- but
    // it is a box a maker can read, and PNL-0 went to the trouble of padding it to
    // a whole slot precisely so it could not be read through. A box that cannot be
    // read through and CAN be pressed through is the same defect wearing the other
    // half of its costume.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    REQUIRE_FALSE(t.session().panels.has(stock::kKind)); // nothing else is in that slot

    t.press(10, 5);
    CHECK(t.notice() == "+ panel is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.active);

    // Dismissed, the slot is workspace again.
    t.key(input::scan::kEscape);
    t.press(10, 5);
    CHECK(t.notice() == "holding #1 -- drag to move it");
    t.release(10, 5);

    // AND WHEN A PANEL IS UNDER IT, THE ANSWER IS THE PICKER -- what a maker would
    // say is there, which is the topmost and not the first. The two rectangles are
    // the same one; the names are not.
    open_stock_pane(t);
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    const Screen sc = screen_of(t.session());
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, 10, 6).occupied);
    CHECK(std::string(occupied_at(t.session().panels, t.session().setup.active, sc, 10, 6).what) == kPickerName);
    CHECK(picker_bounds(sc) == bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
}

TEST_CASE("what a panel is painted at and what it occupies are one resolved truth") {
    // THE STRUCTURAL CLAIM, swept over every cell of the canvas rather than
    // sampled: the occupancy answer IS the union of the open panels' bounds, and
    // those are the same rectangles `paint_panels` hands the painters. There is no
    // second geometry to drift.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    const Screen sc = screen_of(t.session());
    const Panels& panels = t.session().panels;
    REQUIRE(panels.open.size() == 2);

    const Sweep swept = sweep_canvas(panels, sc);
    CHECK(swept.cells == static_cast<std::size_t>(sc.w * sc.h));
    CHECK(swept.first_bad_x == -1); // named, so a failure says WHICH cell disagreed
    CHECK(swept.first_bad_y == -1);
    CHECK(swept.disagreed == 0);
    // BOTH PLACES, EXACTLY: 63x9 of stack, and -- since WUX-12 -- the full-width two rows
    // the Layouts pane defaults to. That second term is the conversion's own measurement:
    // those cells used to be painted by `paint` and occupied by nothing at all, so a pane
    // dragged under them was erased and still met the hand.
    //
    // ⭐ AND THE THIRD TERM IS GONE WITH THE THIRD PANEL. It was 28x17 of side region, less
    // thirteen columns of nine rows the stack and the column shared once the room became the
    // whole surface. Nothing is placed in the side region by this run -- a desk row can still
    // name it, which `"occupancy is resolved against the screen"` measures -- so the union is
    // two disjoint rectangles here, and `swept.occupied` is still SWEPT rather than added up
    // because the overlap the arithmetic would have to know about is a fact about a desk.
    CHECK(swept.occupied == static_cast<std::size_t>(63 * 9 + sc.w * kTopRows));
    CHECK(swept.painted == swept.occupied);

    // AND WHAT THE PAINTERS ACTUALLY WROTE IS INSIDE THE SPACE THAT IS OCCUPIED --
    // painted through the same one call the screen makes, into a canvas holding
    // nothing else.
    surface::SurfaceCanvas only_panels;
    paint_panels(only_panels, t.session(), sc);
    REQUIRE_FALSE(cell_text_of(only_panels).empty());
    for (const surface::SurfaceLabel& l : cell_text_of(only_panels)) {
        CHECK(occupied_at(panels, setup_for(panels), sc, l.x, l.y).occupied);
    }
    // ONE BACKDROP PER OPEN PANEL, AND EACH IS ITS OWN BOUNDS (PNL-2a). Asserted as
    // an EQUALITY against `bounds_of` rather than as containment, because the thing
    // that went wrong for two phases was a panel painting less than it occupied --
    // and "inside the occupied space" is satisfied by a backdrop over half of it.
    REQUIRE(all_rects(only_panels).size() == panels.open.size());
    for (const Panel& p : panels.open) {
        const ui::Rect pb =
cells_covered(bounds_of(panels, setup_for(panels), p.kind, sc).rect);
        CHECK(has_rect(only_panels, pb.x, pb.y, pb.w, pb.h, kPaneChrome));
    }
    for (const surface::SurfaceRect& r : all_rects(only_panels)) {
        CHECK(occupied_at(panels, setup_for(panels), sc, r.x, r.y).occupied);
        CHECK(occupied_at(panels, setup_for(panels), sc, r.x + r.w - 1, r.y + r.h - 1).occupied);
    }
}

TEST_CASE("occupancy is resolved against the screen, not remembered from one") {
    // A BIGGER SURFACE MOVES THE SIDE REGION, and the cells it occupies move with
    // it -- because the occupancy answer is `bounds_of` on the CURRENT screen and
    // is cached nowhere. The stack is anchored to the other corner and does not
    // move, which is the same asymmetry the painting has.
    Live t;
    // THE PANE AT THE RIGHT COLUMN IS AUTHORED, NOT INHERITED. It was Info, a built-in
    // placed there by the catalog; the place outlived the kind, and a desk row spelled
    // `right-column` is what puts a pane in it now.
    open_at_right_column(t, panel::kPaneEditor);
    const Screen small = screen_of(t.session());
    const ui::Rect side_small =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, small).rect);
    REQUIRE(occupied_at(t.session().panels, t.session().setup.active, small, side_small.x, 4).occupied);

    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    const Screen big = screen_of(t.session());
    REQUIRE(big.w == 100);
    const ui::Rect side_big =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, big).rect);
    CHECK(side_big.x == big.panel_x);
    CHECK(side_big.x > side_small.x);

    // The column a maker presses is the column they can see, on either screen.
    CHECK(occupied_at(t.session().panels, t.session().setup.active, big, side_big.x, 4).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, big, side_small.x, 4).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, big, side_big.x - 1, 4).occupied);

    // AND THE PRESS FOLLOWS IT, through the whole live path: what was the Info
    // column on the small screen is ordinary workspace on the big one.
    //
    // ⚠ ON A ROW WITH NOTHING ON IT, WHICH IS NEWLY WORTH SAYING. The room is the surface
    // now, so a 60%-wide object on a 100-column screen is 60 cells rather than 42 and reaches
    // this column: canvas row 3 is that object's ring. Row 20 is below every object in this
    // document and is the empty workspace the sentence is about.
    t.press(side_small.x, 20);
    CHECK(t.notice() == "nothing there");
    // ⭐ AND THE PANE THAT ANSWERS HAS ITS OWN PRESS ARM, so the sentence is the Pane
    // Manager's own doing rather than the generic occupancy line Info used to draw. What is
    // asserted is that the press was ANSWERED THERE: the workspace's own "nothing there"
    // would be the click-through this case exists to refuse.
    t.press(side_big.x, 3);
    CHECK(t.notice() != "nothing there");
    CHECK(occupied_at(t.session().panels, t.session().setup.active, big, side_big.x, 3).kind ==
          panel::kPaneEditor);
}

TEST_CASE("a closed panel occupies nothing, and neither does a screen with none open") {
    // THE OTHER HALF OF THE INVARIANT, and the one a maker feels every second: a
    // panel that is not open takes nothing away. `bounds_of` answers a closed
    // panel with an empty rectangle, `contains` says an empty rectangle holds
    // nothing, and the whole canvas is therefore the workspace's again.
    Live t;
    // Remove the one pane a fresh session opens with -- the Layouts pane the layout run
    // became (WUX-12). It is not furniture; it goes through the picker. Info used to be the
    // second and is a weave with no office here, so the desk names it and opens nothing.
    pick(t, panel::kLayouts);
    REQUIRE(t.session().panels.open.empty());
    const Screen sc = screen_of(t.session());
    const Sweep swept = sweep_canvas(t.session().panels, sc);
    CHECK(swept.cells == static_cast<std::size_t>(sc.w * sc.h));
    CHECK(swept.occupied == 0);
    CHECK(swept.disagreed == 0);
    // Including the cells the two places WOULD have had.
    CHECK_FALSE(
        occupied_at(t.session().panels, t.session().setup.active, sc, picker_bounds(sc).x, picker_bounds(sc).y).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, sc.panel_x, 0).occupied);

    // And a press in the vacated column reaches the workspace, which is what "the
    // panel was the only thing in the way" means.
    t.press(10, 5);
    CHECK(t.notice() == "holding #1 -- drag to move it");
    t.release(10, 5);
}

TEST_CASE("the window's pixels meet the same panel the terminal's cells do") {
    // THE OTHER MEDIUM, through the graphical Skin's own projection. Occupancy is
    // asked in CANVAS cells, so both media arrive at the same question -- and the
    // case is worth having because the two report different numbers for one place,
    // which is exactly where a second geometry would hide.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    t.press_px(10, 5);
    CHECK_FALSE(t.session().drag.active);
    t.release_px(10, 5);

    t.press_px(7, 11); // below the panel, on #2
    CHECK(t.notice() == "holding #2 -- drag to move it");
    t.release_px(7, 11);
}

TEST_CASE("the cells just outside a panel are ordinary workspace, on every edge") {
    // WHERE THE OCCLUSION STOPS, asked at the boundary rather than in the middle.
    // A press one row below the stack's last row is the cell that separates "the
    // panel occupies what it covers" from "the panel occupies a bit more", and it
    // is also the cell that separates CANVAS cells from WORKSPACE cells: the two
    // spaces are one row apart, so a question asked in the wrong one is invisible
    // everywhere except here.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    // TWO PLACES WITH A PANE IN EACH: the stack, and the right column a desk row names.
    // The column's occupant was Info; the place is what this case is about, and it takes
    // an authored row rather than a catalog entry to fill it now.
    open_at_right_column(t, panel::kPaneEditor);
    open_stock_pane(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect stack =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
    const ui::Rect side =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc).rect);

    // The four edges of each place, in canvas cells: inside, then one cell out.
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, stack.x, stack.y + stack.h - 1).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, stack.x, stack.y + stack.h).occupied);
    // ⚠ THE ROW ABOVE THE STACK IS NOT WORKSPACE AND HAS NOT BEEN SINCE WUX-12. The stack
    // begins at `kWorkspaceY`, so the cell above it is the last reserved row at the top --
    // and what stands on those rows is the Layouts pane, which occupies them like any other
    // pane. Before the conversion the same cell was painted by the band and owned by
    // nobody, which is the see-here/press-there divergence the conversion retired; the
    // claim this case makes about the boundary is therefore that the cell belongs to the
    // pane ABOVE rather than to no one.
    {
        const Occupancy above =
            occupied_at(t.session().panels, t.session().setup.active, sc, stack.x, stack.y - 1);
        CHECK(above.occupied);
        CHECK(above.kind == panel::kLayouts);
    }
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, stack.x + stack.w - 1, stack.y).occupied);
    // ⚠ AND THE CELL JUST RIGHT OF THE STACK IS NOT WORKSPACE, AND HAS NOT BEEN SINCE THE
    // RIGHT COLUMN STOPPED BEING SUBTRACTED FROM THE ROOM. The slot's half-share is measured
    // against the whole surface, so at this extent it ends at 62 and the column begins at 50.
    // The two cells below are the same boundary question the row above the stack already
    // asks: whose is it, rather than whether it is anybody's.
    {
        const Occupancy right =
            occupied_at(t.session().panels, t.session().setup.active, sc, stack.x + stack.w,
                        stack.y);
        CHECK(right.occupied);
        CHECK(right.kind == panel::kPaneEditor);
    }
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, side.x, side.y + side.h - 1).occupied);
    {
        const Occupancy left =
            occupied_at(t.session().panels, t.session().setup.active, sc, side.x - 1, side.y);
        CHECK(left.occupied);
        CHECK(left.kind == stock::kKind);
    }
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, side.x, side.y + side.h).occupied);

    // AND THROUGH THE LIVE PRESS, which is the half that would catch a question
    // asked in the wrong space: the row under the panel is empty workspace, and
    // it says the empty-workspace sentence rather than the panel's.
    const std::int64_t below = stack.y + stack.h - kWorkspaceY; // workspace row under it
    REQUIRE_FALSE(stack.contains(10 + kWorkspaceX, below + kWorkspaceY));
    t.press(10, below);
    CHECK(t.notice() == "nothing there");
    t.press(10, below - 1); // the panel's own last row
}

// ⭐ HD-3's AND HD-4's CASES LEFT WITH THE OVERLAY (VD-24). They drove the caret, the press
// and the horizontal window of the terminal's input row through `terminal_input_place` --
// the ONE MEASURER the painter, the hit test and the caret all went through. A pane measures
// its own row now: it is told its room and it decides which of its rows is the prompt, and
// the claims are pinned where that arithmetic lives (`tests/test_workshop_panes_terminal.cpp`
// for the pane's own, `test_component.cpp` for the window the component owns). The two pixel
// helpers that sat here were the inverse of a resolution this host no longer performs.

// ⭐ RESTORED AFTER THE INFO PANEL'S HD-10 CASES LEFT, and reduced again when the Terminal's
// did (VD-24). What is left is the extents this composition lays out, which is what the
// surviving HD-10 cases are still about.
namespace {

// ⚠ `Places` AND `shared_cells` WERE HERE (VD-24). They existed to measure how many cells
// the terminal overlay's rectangle shared with the reserved side column -- HD-10's own
// arithmetic. There is no overlay rectangle to measure, and what two panes share is a fact
// about the arrangement a maker authored rather than about this screen.

/// The extents HD-10 measures over: the minimum, the widths where the room is narrower than
/// the pane's want, the width where the two first agree, and up to the largest surface this
/// composition lays out.
const std::vector<std::pair<std::int64_t, std::int64_t>> kHd10Extents = {
    {78, 22},  {79, 22},  {80, 23},  {81, 24},  {94, 30},  {98, 60},
    {100, 33}, {120, 40}, {160, 60}, {240, 80}, {640, 400}};

} // namespace

TEST_CASE("HD-10 is over: a pane over a pane, and the boundary is what makes it legible") {
    // ⭐ THE CASE THIS REPLACES WAS THE MEASUREMENT THAT ENDED THE ARC. HD-10 pinned, and
    // deliberately did not repair, the one overlap in this composition that no boundary made
    // legible: the terminal OVERLAY had a rectangle `screen_of` reserved for it, wore no
    // chrome, and was painted on a plane after every pane -- so where it met the pane at the
    // right column, the maker read a panel that had STOPPED rather than one that was covered,
    // and could not move either. The founder was offered chrome, paint order or a ceiling and
    // chose none: the Terminal is a pane (VD-24), and a pane's boundary is by construction.
    //
    // SO WHAT IS PINNED HERE IS THE END STATE, and it is three facts about ANY two panes --
    // which is exactly the point, because there is no longer a rule that is only the
    // Terminal's.
    Live t;
    const PaneRef front{"zengine.workshop", "layouts"};
    open_pane(t, ref_of(stock::kKind));
    REQUIRE(t.session().panels.has(stock::kKind));

    // 1. THEY OVERLAP AT ALL, because a maker may put a pane anywhere. Two panes in the
    //    overlay stack's slots is the arrangement a fresh session already produces.
    const Screen sc = screen_of(t.session());
    const ui::Rect a =
        cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                stock::kKind, sc).rect);
    const ui::Rect b =
        cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                panel::kLayouts, sc).rect);
    CHECK(a.w > 0);
    CHECK(b.w > 0);

    // 2. WHICHEVER IS IN FRONT IS PAINTED WHERE IT IS HIT -- the front-order law, which is
    //    what a maker uses to read one pane over another. It is the claim the overlay could
    //    not make, because nothing could be in front of it.
    (void)front;
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, a.x + 1, a.y + 1)
              .occupied);

    // 3. AND `screen_of` RESERVES NOTHING FOR ANY OF THEM. The four constants that sized the
    //    overlay's rectangle are gone with it, so the room is the surface at every extent
    //    this composition lays out -- which is the sentence the reservation's retirement
    //    (PR #21) could only half say while an overlay still had a corner of its own.
    for (const auto& wh : kHd10Extents) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        const Screen at = screen_of(wh.first, wh.second);
        CHECK(at.room_w == at.w);
        CHECK(at.panel_x + kPanelCols == at.w);
    }
}

TEST_CASE("HD-10: the screen's furniture cannot see a panel, open or closed") {
    // THE TEST FOR THE WRONG OWNER, AS A CASE, AND IT OUTLIVES THE RESERVATION. If the pane's
    // placement had ever been "must not cover Info", closing Info would move it -- and a
    // placement that moved because a maker hid a list of names is the thing PNL-0 refused and
    // `the-room-is-the-screen` refuses again from the other side. Nothing in `screen_of` can
    // see a panel; that is what lets the right column become an ordinary place at all.
    Session open_info;
    Session no_info;
    // ⭐ ONE PANEL TO CLOSE, NOT TWO: a default `Session` opens what `kDefaultPanels` names,
    // and Info left that list with the panel.
    REQUIRE(close_panel(no_info.panels, panel::kLayouts));
    REQUIRE(no_info.panels.open.empty());
    REQUIRE_FALSE(open_info.panels.open.empty()); // the control really is the other case
    for (const auto& wh : kHd10Extents) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        adopt_screen(open_info, wh.first, wh.second, 0, 0);
        adopt_screen(no_info, wh.first, wh.second, 0, 0);
        const Screen a = screen_of(open_info);
        const Screen b = screen_of(no_info);
        // ⚠ THE WITNESS USED TO BE THE TERMINAL'S OWN RECTANGLE (VD-24), which was the last
        // thing `screen_of` sized for one particular tool. With it gone the claim is made
        // over the whole of the furniture, which is a wider statement of the same thing:
        // NOTHING this function answers moves because a panel is open or closed.
        CHECK(a.room_w == b.room_w);
        CHECK(a.room_h == b.room_h);
        CHECK(a.panel_x == b.panel_x);
        CHECK(a.notice_y == b.notice_y);
        CHECK(b.room_w == b.w);
    }
}

namespace {

/// The character a maker reads at one canvas cell, whatever painted it -- the medium's own
/// answer, read back off the raster rather than off the layer that claims the cell.
char cell_seen_at(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
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

TEST_CASE("WIND-2a: an overlapping pane is painted where it is hit, in both front orders") {
    // ⭐ REWRITTEN OVER TWO PANES THAT STILL EXIST. It was the Builder over Info, and the one
    // cell they could both claim had to be AUTHORED because the side region was reserved and
    // the stack's slots were disjoint. Both built-ins share the stack now, so the overlap is
    // authored the same way and the claim is unchanged: what the hand meets is what the eye
    // reads, in either front order.
    WorkshopDoc d;
    Session s;
    admit_stock(s.panels); // the stand-in, first (stock)
    s.screen_w = 120;
    s.screen_h = 40;
    s.setup.active = two_overlays();
    s.panels.open = {Panel{stock::kKind}, Panel{panel::kPaneEditor}};

    // THE ONE CELL TWO PRESENTATIONS CAN BOTH CLAIM. The Editor is taken out of the slot queue
    // FIRST -- a pane that names its own place does not queue for one it will not use -- and
    // only then is the Pane Manager's rectangle read, because that is the rectangle it keeps
    // for the rest of this case.
    const Screen sc = screen_of(s);
    REQUIRE(author_pane_place(s.setup.active, ref_of(stock::kKind), 0, 0).accepted);
    const ui::Rect manager =
        pane_body_cells(bounds_of(s.panels, s.setup.active, panel::kPaneEditor, sc).rect);
    REQUIRE(manager.w > 0);
    const std::int64_t x = manager.x;
    const std::int64_t y = manager.y;
    REQUIRE(author_pane_place(s.setup.active, ref_of(stock::kKind),
                              surface::subs_of_cells(x - kChromeCells),
                              surface::subs_of_cells(y - kChromeCells))
                .accepted);
    REQUIRE(cells_covered(bounds_of(s.panels, s.setup.active, stock::kKind, sc).rect)
                .contains(x, y));
    REQUIRE(cells_covered(bounds_of(s.panels, s.setup.active, panel::kPaneEditor, sc).rect)
                .contains(x, y));

    // THE TWO CONTROLS: what each pane draws there WITH THE OTHER ABSENT. Neither rectangle
    // depends on the other, so these are the same two pictures the overlap is made of.
    const auto alone = [&](std::int64_t kind) {
        Session one = s;
        one.setup.active = Setup{};
        one.setup.active.name = "one";
        REQUIRE(add_pane(one.setup.active, ref_of(kind)));
        if (kind == stock::kKind) {
            REQUIRE(author_pane_place(one.setup.active, ref_of(kind),
                                      surface::subs_of_cells(x - kChromeCells),
                                      surface::subs_of_cells(y - kChromeCells))
                        .accepted);
        }
        one.panels.open = {Panel{kind}};
        return cell_seen_at(paint(d, one), x, y);
    };
    const char editor_alone = alone(stock::kKind);
    const char manager_alone = alone(panel::kPaneEditor);
    // AND THE CONTROL ON THE CONTROLS: the two draw DIFFERENT characters there, so neither
    // assertion below can pass by the media agreeing about nothing.
    REQUIRE(editor_alone != manager_alone);

    for (const std::int64_t front : {stock::kKind, panel::kPaneEditor}) {
        CAPTURE(front);
        REQUIRE(send_to_front(s.setup.active, ref_of(front)));
        // WHAT THE HAND MEETS...
        CHECK(occupied_at(s.panels, s.setup.active, sc, x, y).what == kind_name(s.panels, front));
        // ...IS WHAT THE MEDIUM PAINTS.
        CHECK(cell_seen_at(paint(d, s), x, y) ==
              (front == stock::kKind ? editor_alone : manager_alone));
    }
}

TEST_CASE("WIND-2a: the picker can reach and remove an unresolved row") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    REQUIRE(add_pane(live(t).setup.active, stranger()));
    const std::vector<CatalogRow> rows =
        inventory_rows(t.session().setup.active, t.session().panels);
    std::size_t want = rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].ref == stranger()) {
            want = i;
        }
    }
    REQUIRE(want < rows.size());

    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    for (std::size_t guard = 0; guard <= rows.size(); ++guard) {
        if (t.session().panels.picker.cursor == want) {
            break;
        }
        t.key(input::scan::kDown);
    }
    // THE ROW IS PAINTED, SO THE CURSOR MUST BE ABLE TO REACH IT. A population painted from
    // one list and navigated from another is a row a maker can see and cannot touch.
    CHECK(t.session().panels.picker.cursor == want);
    t.key(input::scan::kReturn);
    CHECK_FALSE(has_pane(t.session().setup.active, stranger()));
    // AND THE ROWS THAT WERE THERE ARE STILL THERE: the gesture removed what it was on.
    // Both of a fresh desk's rows, including the one that is ALSO unresolved -- which is
    // the sharper half now: removing one unresolved row leaves the other standing.
    CHECK(has_pane(t.session().setup.active, ref_of(panel::kLayouts)));
    CHECK(has_pane(t.session().setup.active, info_ref()));
}

TEST_CASE("WIND-2a: a clipped default resize begins from the full resolved size") {
    // ---- THE RIGHT EDGE. A default width of eighty-nine, four cells of it on the canvas.
    {
        Live t;
        t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
        open_pane(t, ref_of(stock::kKind));
        const PaneRef builder = ref_of(stock::kKind);
        REQUIRE(author_pane_place(live(t).setup.active, builder, subs(156), subs(2))
                    .accepted);

        const Screen sc = screen_of(t.session());
        const PanelBounds where =
            bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc);
        // THE DEVELOPER'S HALF-SHARE AT THIS EXTENT, of which four cells are on the canvas.
        // 89 while the room stopped short of the right column; 104 of a 160-cell room now.
        REQUIRE(where.resolved.w == subs(104));
        REQUIRE(where.rect.w == subs(4));

        // THE KEY. One cell wider than what the pane RESOLVES to, not one cell wider than
        // the sliver of it a maker can currently see.
        enter_arrange_desk(t);
        select_pane(t, builder);
        t.key(input::scan::kRight, input::mod::kShift);
        const SetupPane* row = pane_of(t.session().setup.active, builder);
        REQUIRE(row != nullptr);
        CHECK(row->width.mode == pane_unit::kSubcells);
        CHECK(row->width.amount == where.resolved.w + subs(1));
        // AND THE AXIS THE EDGE DID NOT NAME KEEPS EXACTLY WHAT IT HAD, mode included: a
        // width edit leaves a default height still reacting to the room.
        CHECK(row->height.mode == pane_unit::kDefault);
        CHECK(row->height.amount == 0);

        // AND THE HAND, FROM THE SAME BASE. The affordance stays on the VISIBLE boundary --
        // that is where a maker's eye and hand are -- and its delta applies to the resolved
        // size. THE PRESS RECORDS THAT BASE AND THE MOTION SPENDS IT, and both halves are
        // asserted: a case that stopped at `base_w` would witness what the gesture
        // remembered rather than what it authored, which is the half a maker actually sees.
        REQUIRE(reset_pane_width(live(t).setup.active, builder));
        const ui::Rect vis =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                      screen_of(t.session())).rect);
        t.press_at(vis.x + vis.w - 1, vis.y + vis.h - 1 + surface::kTuiCanvasTopRow,
                   input::space::kCells);
        REQUIRE(t.session().pane_drag.active);
        REQUIRE(t.session().pane_drag.sizing);
        CHECK(t.session().pane_drag.base_w == where.resolved.w);
        t.publish(loom::to_value(input::PointerMoved{
            vis.x + vis.w, vis.y + vis.h - 1 + surface::kTuiCanvasTopRow, 0, 0,
            input::space::kCells, input::mod::kNone}));
        t.release(0, 0);
        const SetupPane* pulled = pane_of(t.session().setup.active, builder);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->width.mode == pane_unit::kSubcells);
        CHECK(pulled->width.amount == where.resolved.w + subs(1));
        CHECK(pulled->height.mode == pane_unit::kDefault);
        CHECK(pulled->height.amount == 0);
    }

    // ---- THE BOTTOM EDGE. The same sentence about the other axis, on a fresh session so
    // that nothing the width half authored can answer a default-height question for it: a
    // default height of nine, two rows of it on the canvas.
    {
        Live t;
        t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
        open_pane(t, ref_of(stock::kKind));
        const PaneRef builder = ref_of(stock::kKind);
        REQUIRE(author_pane_place(live(t).setup.active, builder, 0, subs(42)).accepted);

        const Screen sc = screen_of(t.session());
        const PanelBounds where =
            bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc);
        // THE DEVELOPER'S STACK HEIGHT, of which two rows are on the canvas.
        REQUIRE(where.resolved.h == subs(9));
        REQUIRE(where.rect.h == subs(2));

        enter_arrange_desk(t);
        select_pane(t, builder);
        t.key(input::scan::kDown, input::mod::kShift);
        const SetupPane* row = pane_of(t.session().setup.active, builder);
        REQUIRE(row != nullptr);
        CHECK(row->height.mode == pane_unit::kSubcells);
        CHECK(row->height.amount == where.resolved.h + subs(1));
        CHECK(row->width.mode == pane_unit::kDefault);
        CHECK(row->width.amount == 0);

        REQUIRE(reset_pane_height(live(t).setup.active, builder));
        const ui::Rect vis =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                      screen_of(t.session())).rect);
        // THE MIDDLE OF THE BOTTOM RUN, which is the cell on that edge and on neither of the
        // corners it shares the run with -- a two-row sliver still has one.
        const std::int64_t cx = vis.x + vis.w / 2;
        const std::int64_t cy = vis.y + vis.h - 1;
        t.press_at(cx, cy + surface::kTuiCanvasTopRow, input::space::kCells);
        REQUIRE(t.session().pane_drag.active);
        REQUIRE(t.session().pane_drag.sizing);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kBottom);
        CHECK(t.session().pane_drag.base_h == where.resolved.h);
        t.publish(loom::to_value(input::PointerMoved{cx, cy + 1 + surface::kTuiCanvasTopRow, 0,
                                                     0, input::space::kCells,
                                                     input::mod::kNone}));
        t.release(0, 0);
        const SetupPane* pulled = pane_of(t.session().setup.active, builder);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->height.mode == pane_unit::kSubcells);
        CHECK(pulled->height.amount == where.resolved.h + subs(1));
        CHECK(pulled->width.mode == pane_unit::kDefault);
        CHECK(pulled->width.amount == 0);
    }

    // ---- ONE CORNER, ONE ILLEGAL AXIS, AND ONLY ITS OWN AXIS HELD (WUX-2a). This block
    // used to pin the WHOLE window as one indivisible transaction — a corner whose height
    // was illegal could not widen — and that coupling was the measured live defect WUX-2a
    // removed. What the REAL management route now proves, at the terminal's cell grain:
    // the vertical axis refuses atomically while the legal horizontal transaction still
    // lands, and the write is a status rather than an alert.
    {
        Live t;
        t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
        open_pane(t, ref_of(stock::kKind));
        const PaneRef builder = ref_of(stock::kKind);
        enter_arrange_desk(t);
        select_pane(t, builder);
        const PanelBounds where =
            bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                      screen_of(t.session()));
        REQUIRE(where.rect.w > 2);
        REQUIRE(where.rect.h > 2);

        // BOTH AXES PARTICIPATE, because a corner names two of them.
        const ui::Rect vis_cells = cells_covered(where.rect);
        t.press_at(vis_cells.x + vis_cells.w - 1,
                   vis_cells.y + vis_cells.h - 1 + surface::kTuiCanvasTopRow,
                   input::space::kCells);
        REQUIRE(t.session().pane_drag.active);
        REQUIRE(t.session().pane_drag.sizing);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kBottomRight);
        REQUIRE(t.session().pane_drag.base_w == where.resolved.w);
        REQUIRE(t.session().pane_drag.base_h == where.resolved.h);

        // ONE CELL OUT AND A WHOLE HEIGHT UP: a width of ninety, which is legal and changed,
        // beside a height of zero, which is not a size at all.
        t.publish(loom::to_value(input::PointerMoved{
            vis_cells.x + vis_cells.w,
            vis_cells.y + vis_cells.h - 1 - (where.resolved.h / surface::kCellSubs) +
                surface::kTuiCanvasTopRow,
            0, 0, input::space::kCells, input::mod::kNone}));

        // THE LEGAL AXIS LANDED AND THE ILLEGAL ONE HELD: the width is authored one cell
        // out, the height keeps its default mode untouched, and the place — a trailing
        // corner proposes no position — stays reactive on both axes.
        INFO(t.session().notice);
        CHECK_FALSE(t.session().notice_is_bad);
        const SetupPane* held = pane_of(t.session().setup.active, builder);
        REQUIRE(held != nullptr);
        CHECK(held->width.mode == pane_unit::kSubcells);
        CHECK(held->width.amount == where.resolved.w + subs(1));
        CHECK(held->height.mode == pane_unit::kDefault);
        CHECK(held->height.amount == 0);
        CHECK(held->place.mode == pane_unit::kDefault);

        // AND THE GESTURE ENDS THE ORDINARY WAY, because an answer — either answer — is
        // not a broken hand.
        t.release(0, 0);
        CHECK_FALSE(t.session().pane_drag.active);
    }
}

TEST_CASE("WIND-2a: a release ends a pane gesture whatever mode sees it") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(t, ref_of(stock::kKind));
    const PaneRef builder = ref_of(stock::kKind);
    enter_arrange_desk(t);
    select_pane(t, builder);
    const Screen sc = screen_of(t.session());
    const ui::Rect rect =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
    REQUIRE(rect.w > 4);

    // A MOVE BEGUN, THEN A MODE ARRIVES OVER THE TOP OF IT.
    //
    // ⚠ THE MODE USED TO BE THE TERMINAL OVERLAY (VD-24), which is a pane now and cannot
    // arrive over a gesture in progress at all. The mode that still can is an arrangement
    // scope, and it makes the same claim: entering one mid-drag must not swallow the
    // release, or `pane_drag.active` stays true with the button up and the next bare motion
    // moves a pane nobody is holding.
    t.press_at(rect.x + 1, rect.y + 1 + surface::kTuiCanvasTopRow, input::space::kCells);
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
    const ui::Rect now = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.press_at(now.x + now.w - 1, now.y + now.h - 1 + surface::kTuiCanvasTopRow,
               input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE(t.session().pane_drag.sizing);
    enter_arrange_desk(t);
    t.release(0, 0);
    CHECK_FALSE(t.session().pane_drag.active);
}

TEST_CASE("WIND-2a: a removed target leaves no stale selection, submode or heading") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(t, ref_of(stock::kKind));
    const PaneRef builder = ref_of(stock::kKind);
    enter_arrange_desk(t);
    select_pane(t, builder);
    const Screen sc = screen_of(t.session());
    const ui::Rect rect =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
    t.press_at(rect.x + 1, rect.y + 1 + surface::kTuiCanvasTopRow, input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);

    // THE TARGET LEAVES THE SETUP UNDER THE HAND.
    REQUIRE(remove_pane(live(t).setup.active, builder));
    t.publish(loom::to_value(input::PointerMoved{rect.x + 4,
                                                 rect.y + 4 + surface::kTuiCanvasTopRow, 0, 0,
                                                 input::space::kCells, input::mod::kNone}));
    CHECK_FALSE(t.session().pane_drag.active);
    // MEMBERSHIP IS THE LAW: the pane is not in the setup, so nothing addresses it -- and
    // the DESK stays open, because its subject is the desk (ARR-0).
    CHECK_FALSE(t.session().arrange.addressed());
    CHECK(t.session().arrange.open);
    CHECK(t.session().arrange.desk);

    // AND AN UNRESOLVED ADDRESS IS KEPT, because unresolved is recoverable and still
    // authored -- membership, not presentation, is what clears one.
    REQUIRE(add_pane(live(t).setup.active, stranger()));
    for (std::size_t guard = 0; guard < 8; ++guard) {
        if (t.session().arrange.pane == stranger()) {
            break;
        }
        t.key(input::scan::kTab);
    }
    REQUIRE(t.session().arrange.pane == stranger());
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    CHECK(t.session().arrange.pane == stranger());
}

TEST_CASE("ARR-0: removing the pane being arranged ends the arrangement about it") {
    // THE ONE-PANE SCOPE CLOSES WITH ITS PANE: an interaction bound to exactly one pane
    // is a state about nothing once that pane is gone -- and it closes SILENTLY, so the
    // removal's own sentence stays on the notice line. The desk's half of the same law
    // is pinned above; this is the scope the law was written for.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
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
    CHECK(t.notice().find("removed") != std::string::npos);
}

TEST_CASE("WIND-2a: a pixel axis refuses every current pane projection, Info included") {
    Session s;
    s.setup.active = Setup{};
    s.setup.active.name = "Pixels";
    REQUIRE(add_pane(s.setup.active, ref_of(panel::kPaneEditor)));
    const PaneRef info = ref_of(panel::kPaneEditor);
    REQUIRE(author_pane_size(s.setup.active, info, PaneSize{pane_unit::kPixels, 240},
                             PaneSize{pane_unit::kDefault, 0})
                .accepted);
    REQUIRE(check_setup(s.setup.active).accepted);
    s.panels.open = {Panel{panel::kPaneEditor}};

    for (const std::int64_t adv : {std::int64_t{0}, std::int64_t{8}}) {
        CAPTURE(adv);
        s.text_advance_px = adv;
        s.text_line_px = adv > 0 ? 18 : 0;
        const Screen sc = screen_of(s);
        const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kPaneEditor, sc);
        // FIXED PLACEMENT IS NOT PERMISSION TO PRESENT AN UNSUPPORTED UNIT AS UNDERSTOOD.
        CHECK_FALSE(where.projected);
        CHECK(where.rect.w == 0);
        CHECK(pane_state_of(s.panels, s.setup.active, sc,
                            CatalogRow{panel::kPaneEditor, info, "Info", ""}) == pane_state::kRefused);
    }
    // AND THE AUTHORED BYTES ARE EXACT THROUGH THE REFUSAL.
    CHECK(pane_of(s.setup.active, info)->width.mode == pane_unit::kPixels);
    CHECK(pane_of(s.setup.active, info)->width.amount == 240);
    CHECK(setup_persist::from_text(setup_persist::to_text(s.setup.active)).setup ==
          s.setup.active);
}

TEST_CASE("WIND-2a/WUX-1: the opening gestures are claimed by the band's own truth") {
    // A MODE WITH NO WAY TO DISCOVER IT IS NOT A FEATURE -- the argument that once put the
    // three opening hints on the shared top row. That row is retired (WUX-1); the claim
    // surfaces are the keymap projections now: the band's legend packs `help_pairs`, and
    // the full hotkey view lists everything. So the discoverability pin moves to the
    // TRUTH the band packs from -- the three gestures are pairs of the command context,
    // spelled from the effective keymap -- and the top row carries nothing but the panel's
    // own heading.
    WorkshopDoc d;
    Session s; // the minimum screen, which is where every hint is tightest
    REQUIRE(screen_of(s).w == kScreenMinW);
    REQUIRE(screen_of(s).h == kScreenMinH);
    const std::vector<std::string> pairs = help_pairs(s.keymap, KeyContext::kCommand);
    const auto has_pair = [&pairs](const std::string& want) {
        for (const std::string& pair : pairs) {
            if (pair == want) {
                return true;
            }
        }
        return false;
    };
    CHECK(has_pair("w arrange desk"));
    CHECK(has_pair("p + panel"));
    CHECK(has_pair("^k hotkeys")); // the recovery key: the full list is one keystroke away

    const std::vector<std::string> raster = rasterized(paint(d, s));
    REQUIRE_FALSE(raster.empty());
    const std::string& top = raster[0];
    INFO(top);
    CHECK(top.find("[window]") == std::string::npos);
    CHECK(top.find("[+ panel]") == std::string::npos);
    CHECK(top.find("terminal") == std::string::npos);
    CHECK(top.find("WORKSPACE") == std::string::npos);
    // ⭐ AND INFO'S HEADING IS NOT IN THIS PICTURE. The line here read `OBJECTS` one row
    // inside the side region's boundary (WUX-5); that heading is `Zengine/info-pane/`'s and
    // this session opens no office. What the case is about is what the BAND claims, and the
    // four assertions above are all of it.
}

// ---- INTR-0: the Introspection tool, and what its rows are allowed to mean ---------
//
// TWO TIERS, AND THE SPLIT IS THE PHASE'S OWN CLAIM. The first asks what a reading
// MEANS -- pure functions over a value, no bus, no library, no Workshop -- because
// "every displayed fact is true" is a statement about a projection and is provable
// as one. The second loads the REAL `zengine-introspection` library, the same
// artifact `zengine-workshop` stages beside itself, through the real Kernel and
// Manager, and reads its rows off a published canvas -- because "the fact has an
// authoritative owner" is a statement about a path, and a mock owner would prove
// nothing about the one a maker runs.

// ---- Tier one: what a reading means -----------------------------------------------

TEST_CASE("INTR-0: the Manager's blob is read as name and role, and an absent role is a fact") {
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

TEST_CASE("INTR-0: an empty map is an observed zero, and stray commas invent no weave") {
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
    // `(waiting for the provider)` is what a maker reads in that state.
    const std::vector<surface::SurfaceTextRow> none = intro::project_loaded({}, 8, 46).rows;
    REQUIRE_FALSE(none.empty());
    CHECK(none[0].text == "loaded weaves -- 0");
    CHECK(any_row(none, intro::kNotInProcess));
}

TEST_CASE("INTR-0: the split is on the LAST at-sign, and the ambiguity is bounded not solved") {
    // THE WIRE FORM HAS NO ESCAPING. The producer joins on a comma and an at-sign and
    // emits no delimiter of its own, so a name carrying either is unrecoverable in
    // principle. The reading chosen is the ambiguity's better half: a name with an
    // at-sign still resolves against a role that has none.
    const std::vector<intro::LoadedWeave> got = intro::parse_loaded("odd@name@zengine.role");
    REQUIRE(got.size() == 1);
    CHECK(got[0].name == "odd@name");
    CHECK(got[0].role == "zengine.role");
}

TEST_CASE("INTR-0: a weave with no role says so, rather than leaving the column blank") {
    const std::vector<surface::SurfaceTextRow> rows =
        intro::project_loaded({intro::LoadedWeave{"snake-controls", ""}}, 8, 46).rows;
    CHECK(any_row(rows, std::string("snake-controls @") + intro::kNoRole));
}

TEST_CASE("INTR-0: the heading counts the whole population, not the shown part") {
    // A COUNT THAT SHRANK WITH THE WINDOW WOULD BE THE ONE NUMBER ON THIS PANE A MAKER
    // COULD NOT TRUST. It is taken from the population and never from the rows spent.
    for (const std::int64_t rows : {std::int64_t{4}, std::int64_t{8}, std::int64_t{40}}) {
        CAPTURE(rows);
        const std::vector<surface::SurfaceTextRow> out =
            intro::project_loaded(loaded_population(9), rows, 46).rows;
        REQUIRE_FALSE(out.empty());
        CHECK(out[0].text == "loaded weaves -- 9");
    }
}

TEST_CASE("INTR-0: what the list is NOT survives every budget that shows a list at all") {
    // THE RESERVATION, WHICH IS THE PROJECTION'S ONE POLICY. A count with an unstated
    // population is an honest number that leaves a false picture; a maker reading
    // `loaded weaves -- 4` beside a running Builder would be right to conclude the
    // Builder is not running. So the sentence bounding the count is subtracted BEFORE
    // the list is offered anything but its first row, and the rows lost to it are
    // counted out loud.
    for (std::int64_t rows = 3; rows <= 40; ++rows) {
        CAPTURE(rows);
        const std::vector<surface::SurfaceTextRow> out =
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
        for (const surface::SurfaceTextRow& r : out) {
            if (r.text.rfind("  weave-", 0) == 0) {
                ++named;
            }
        }
        CHECK((named == 12 || any_row(out, "... " + std::to_string(12 - named) + " more")));
    }
}

TEST_CASE("INTR-0: an omission is counted on its own row and the count adds up") {
    const std::vector<surface::SurfaceTextRow> out =
        intro::project_loaded(loaded_population(20), 8, 46).rows;
    // Heading, some entries, the marker, the two notes -- every weave either shown or
    // counted, and nothing quietly dropped.
    std::size_t named = 0;
    for (const surface::SurfaceTextRow& r : out) {
        if (r.text.rfind("  weave-", 0) == 0) {
            ++named;
        }
    }
    REQUIRE(named > 0);
    CHECK(any_row(out, "... " + std::to_string(20 - named) + " more"));
}

TEST_CASE("INTR-0: at a budget too small to show and to say, it says") {
    // THE SHORTEST ANSWER THIS VIEW HAS: it cannot show a maker a weave AND tell them
    // what it is hiding, so it tells them.
    const std::vector<surface::SurfaceTextRow> out =
        intro::project_loaded(loaded_population(20), 3, 46).rows;
    REQUIRE(out.size() == 3);
    CHECK(out[0].text == "loaded weaves -- 20");
    CHECK(out[1].text == "  ... 20 more");
    CHECK(out[2].text == std::string(intro::kNotInProcess));
}

TEST_CASE("INTR-0: every projection fits the room it was given, over the whole domain") {
    // THE OBLIGATION THAT IS NOT A COURTESY. Workshop refuses an over-budget update
    // WHOLE rather than truncating it, so a provider that miscounts by one row loses
    // everything it said and the pane goes back to waiting. The sweep is the population
    // crossed with the budget, including the degenerate budgets no pane has.
    for (const std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{4}, std::size_t{31}}) {
        for (std::int64_t rows = 0; rows <= 12; ++rows) {
            for (const std::int64_t cols : {std::int64_t{0}, std::int64_t{1}, std::int64_t{3},
                                            std::int64_t{12}, std::int64_t{46}, std::int64_t{200}}) {
                CAPTURE(n);
                CAPTURE(rows);
                CAPTURE(cols);
                const std::vector<surface::SurfaceTextRow> out =
                    intro::project_loaded(loaded_population(n), rows, cols).rows;
                REQUIRE(static_cast<std::int64_t>(out.size()) <= rows);
                for (const surface::SurfaceTextRow& r : out) {
                    REQUIRE(static_cast<std::int64_t>(r.text.size()) <= cols);
                    for (const char c : r.text) {
                        const unsigned char byte = static_cast<unsigned char>(c);
                        // `SurfaceTextRow`'s plain-ASCII contract, which is the third
                        // rule Workshop judges an update by.
                        REQUIRE(byte >= 0x20u);
                        REQUIRE(byte < 0x7Fu);
                    }
                    // A blank separator is `role::kFill`, never `role::kNone` -- that
                    // value is the absence of a BACKGROUND and is not a Skin ink.
                    REQUIRE(r.role != surface::role::kNone);
                }
            }
        }
    }
}

TEST_CASE("INTR-0: a long name is cut with a mark rather than silently") {
    const std::vector<surface::SurfaceTextRow> out = intro::project_loaded(
        {intro::LoadedWeave{"a-library-with-a-very-long-name-indeed", "zengine.role"}}, 8, 20).rows;
    REQUIRE(out.size() >= 2);
    CHECK(out[1].text.size() == 20);
    CHECK(out[1].text.find(intro::kElided) != std::string::npos);
}

// ---- Tier two: the real library, through the real load path -----------------------

TEST_CASE("INTR-0: loading the real tool puts its pane in the catalog, offered by its office") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    REQUIRE(r.session().panels.runtime.entries.empty());

    const loom::WeaveId id =
        r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(r.load_refusals.empty());
    REQUIRE(id.valid());
    // WORKSHOP LEARNED THESE PANES FROM LIVE OFFERS and from nowhere else: no
    // `panel::k*` was minted for any of them, no arm was compiled for any of them,
    // and every handle they carry is a runtime one.
    REQUIRE(r.session().panels.runtime.entries.size() == kIntroPaneCount);
    const RuntimePane* found = intro_row(r, kIntroPane);
    REQUIRE(found != nullptr);
    const RuntimePane& row = *found;
    CHECK(row.provider == std::string(kIntroOffice)); // Loom's stamp, not a payload field
    CHECK(row.pane == std::string(kIntroPane));
    CHECK(row.name == std::string(intro::kLoadedPaneName));
    CHECK(row.summary == std::string(intro::kLoadedPaneSummary));
    CHECK(is_runtime_kind(row.kind));

    // ...AND SO DID THE OTHER TWO (INTR-1). Three panes, one office, three distinct
    // runtime handles -- which is `PaneOffered`'s own claim that a provider is not a
    // pane, measured on the first office in this repository that has more than one.
    for (const char* pane : {intro::kArrangementPane, intro::kPowersPane}) {
        const RuntimePane* more = intro_row(r, pane);
        REQUIRE(more != nullptr);
        CHECK(more->provider == std::string(kIntroOffice));
        CHECK(is_runtime_kind(more->kind));
        CHECK(more->kind != row.kind);
        // A NAME AND A SUMMARY A MAKER CAN READ WHOLE. `kPickerNameCols` is ten cells
        // and admission allows thirty-two, so a name inside the bound is a name that
        // reaches a maker's eye unmarked -- INTR-0's own lesson, paid once.
        CHECK_FALSE(more->name.empty());
        CHECK(more->name.size() <= 10);
        CHECK_FALSE(more->summary.empty());
    }
    CHECK(intro_row(r, kIntroPane)->kind != intro_row(r, intro::kPowersPane)->kind);
}

TEST_CASE("INTR-0: the opened pane names what this Loom actually loaded, itself included") {
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(r.session().panels.runtime.entries.size() == kIntroPaneCount);
    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::int64_t kind = intro_row(r, kIntroPane)->kind;
    const std::vector<std::string> shown =
        external_rows(r.last_canvas(), external_body_rect(r.session(), kind));

    // ONE LOADED WEAVE IN THIS RIG, AND IT IS THIS ONE. Self-introspection through the
    // same observation path used for everyone else -- there is no registration mirror
    // for the tool to see itself in.
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0] == "loaded weaves -- 1");
    CHECK(any_row(shown, std::string(intro::kIntrospectionStem) + " @" + kIntroOffice));
    // AND THE FACT IS BOUNDED WHERE A MAKER READS IT.
    CHECK(any_row(shown, intro::kNotInProcess));
    CHECK(any_row(shown, intro::kSnapshotSource));
}

TEST_CASE("INTR-0: the count is the kernel's and moves when the kernel's map does") {
    // THE WITNESS IS NOT A HARDCODED NAME. A second library is loaded through the same
    // door, and the same pane -- re-granted its room by an extent change -- says two.
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::int64_t kind = intro_row(r, kIntroPane)->kind;
    REQUIRE(external_rows(r.last_canvas(), external_body_rect(r.session(), kind))[0] ==
            "loaded weaves -- 1");

    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    // The Hello provider's own offer arrives too; that is the catalog's business and
    // not this pane's, and the pane's rows are unmoved until it is re-granted room.
    REQUIRE(r.session().panels.runtime.entries.size() == kIntroPaneCount + 1);
    CHECK(external_rows(r.last_canvas(), external_body_rect(r.session(), kind))[0] ==
          "loaded weaves -- 1");

    // A WIDER SURFACE MOVES THE PROSE BUDGET, WHICH IS A ROOM GRANT, WHICH IS THIS
    // TOOL'S ONE BEAT. Nothing polled and nothing timed out.
    r.extent(140, 40);
    const std::vector<std::string> after =
        external_rows(r.last_canvas(), external_body_rect(r.session(), kind));
    REQUIRE_FALSE(after.empty());
    CHECK(after[0] == "loaded weaves -- 2");
    CHECK(any_row(after, "zengine-workshop-hello @" + std::string(kHelloOffice)));
}

TEST_CASE("INTR-0: the graphical medium grants a different budget and the view spends it") {
    // BOTH PROJECTIONS OF ONE PANE, and the provider cannot tell them apart. It is
    // handed `rows` and `columns` and never a cell, a pixel, a font or the identity of
    // the medium that answered -- so what differs between these two readings is a pair
    // of integers `fit_region` resolved on Workshop's side, and nothing else.
    //
    // The metric arrives as a NUMBER, which is how every medium-dependent claim in this
    // suite since HD-6 has been proved on a lane with no font engine.
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;

    const ExternalPane* cells = r.session().panels.external_pane(kind);
    REQUIRE(cells != nullptr);
    const std::int64_t cell_rows = cells->rows;
    const std::int64_t cell_cols = cells->columns;
    REQUIRE(cell_rows > 0);
    const std::vector<std::string> in_cells =
        external_rows(r.last_canvas(), external_body_rect(r.session(), kind));

    // A REAL FACE'S METRIC over the same surface: an 18-pixel line in a 12-pixel cell
    // is fewer prose rows in the same rectangle, and a 10-pixel advance is more columns.
    r.extent(1200, 500, 10, 18);
    const ExternalPane* graphical = r.session().panels.external_pane(kind);
    REQUIRE(graphical != nullptr);
    CHECK(graphical->rows != cell_rows);
    CHECK(graphical->columns != cell_cols);

    // AND THE VIEW ANSWERED THE NEW ROOM rather than the old one -- Workshop clears its
    // cache before every grant, so a projection that had not moved would be showing as
    // `waiting` here instead.
    const std::vector<std::string> in_pixels =
        external_rows(r.last_canvas(), external_body_rect(r.session(), kind));
    REQUIRE_FALSE(in_pixels.empty());
    CHECK(in_pixels[0] == "loaded weaves -- 1");
    CHECK(any_row(in_pixels, intro::kIntrospectionStem));
    CHECK(static_cast<std::int64_t>(in_pixels.size()) <= graphical->rows);
    // THE SAME TRUTH IN BOTH, which is the honesty claim: two projections of one fact.
    CHECK(in_cells[0] == in_pixels[0]);
}

TEST_CASE("INTR-0: an in-process weave is absent from the list and the pane says why") {
    // THE ABSENCE THAT MATTERS. Workshop itself is a live participant holding a live
    // office in this very rig, and it is not in the kernel's map -- so a pane that
    // printed the count without the boundary would leave a maker with a false picture
    // of their own system.
    PaneRig r;
    REQUIRE(r.mount_workshop() != nullptr);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const std::vector<std::string> shown =
        external_rows(r.last_canvas(), external_body_rect(r.session(), kind));

    CHECK_FALSE(any_row(shown, kWorkshopProvider)); // and it is not silently implied either
    CHECK(any_row(shown, intro::kNotInProcess));
}

TEST_CASE("INTR-0 bounded extension: a provider's long name is MARKED in the picker, not cut") {
    // THE DEFECT THE FIRST REAL EXTERNAL TOOL FOUND, on its first live run. Workshop
    // admits a pane name of up to thirty-two bytes and the picker's name column is ten,
    // and `detail::pad` truncates in silence -- so `Loaded Weaves` arrived at a maker's
    // eye as `Loaded Wea`, which reads as a finished name that means something else.
    //
    // WRITTEN AGAINST THE CONSTANT rather than against ten (EDIT-1): what this case pins
    // is that the cut is MARKED and that the state column does not move, and neither of
    // those is a fact about the number.
    //
    // The repair is `detail::fit` before `detail::pad`: the mark for the truth, the pad
    // for the alignment. Both halves are asserted, because a fix that marked the cut and
    // moved the state column would have traded one defect for another.
    const std::string wide = picker_entry_text("a-very-long-provider-name", "closed", "tail");
    CHECK(wide.rfind(detail::fit("a-very-long-provider-name", kPickerNameCols), 0) == 0);
    CHECK(wide.find("closed") == kPickerNameCols);
    // A name that FITS is untouched -- not padded differently, not marked, not moved.
    const std::string narrow = picker_entry_text("Loaded", "closed", "tail");
    CHECK(narrow.rfind(detail::pad("Loaded", kPickerNameCols) + "closed", 0) == 0);
    CHECK(narrow.find(detail::kElided) == std::string::npos);
    // AND THE NAME THIS TOOL ACTUALLY SHIPS NEEDS NO MARK, which is the other half of
    // the answer: a name that only reads correctly because a truncation is marked is a
    // name too long for the room it lives in.
    CHECK(std::string(intro::kLoadedPaneName).size() <= kPickerNameCols);
}

TEST_CASE("INTR-0: the pane header says whose facts these are") {
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    // WORKSHOP'S OWN ROW, over rows written by a party this build never compiled into
    // itself: the office is the only thing about that party Workshop actually knows.
    CHECK(stack_text(r.last_canvas())
              .find(std::string(intro::kLoadedPaneName) + " @" + kIntroOffice) !=
          std::string::npos);
}

TEST_CASE("INTR-0: the tool answers Workshop and refuses everybody else") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(r.session().panels.runtime.entries.size() == kIntroPaneCount);

    // AN UNAUTHENTICATED CATALOG REQUEST -- a root publication carrying no authored
    // role at all. Answering it would hand this tool's catalog to whoever asked.
    const std::size_t before = r.session().panels.runtime.entries.size();
    r.publish(loom::to_value(PaneCatalogRequested{}));
    CHECK(r.session().panels.runtime.entries.size() == before);
    // ...and the authored one is still answered, so the refusal above is about
    // AUTHORSHIP and not about the tool having stopped talking.
    r.ready();
    CHECK(r.session().panels.runtime.entries.size() == before);
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    CHECK(intro_row(r, kIntroPane)->name == std::string(intro::kLoadedPaneName));
}

TEST_CASE("INTR-0: a forged room produces no content at all") {
    // MEASURED FROM THE OTHER SIDE, through a watcher that holds `zengine.workshop` and
    // can therefore author a room deliberately OR send one personally.
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(watch->offers.size() == kIntroPaneCount);

    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant_personally(m, kIntroOffice, PaneRoom{kIntroPane, 6, 40});
    });
    CHECK(watch->content.empty()); // holding an office is not speaking as one

    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, kIntroOffice, PaneRoom{kIntroPane, 6, 40});
    });
    REQUIRE(watch->content.size() == 1);
    CHECK(watch->content[0].pane == std::string(kIntroPane));
    REQUIRE_FALSE(watch->content[0].rows.empty());
    CHECK(watch->content[0].rows[0].text == "loaded weaves -- 1");
}

TEST_CASE("INTR-0: adding this tool widens nothing -- it says three shapes and no more") {
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
    // FOUR SINCE WL-KEY-15: the Powers pane's declared actions ride beside its offer, and
    // they are a declaration of what a pane DOES, not a reach into anything.
    const std::vector<std::string> allowed{"PaneActions", "PaneContent", "PaneOffered",
                                           "zen.ListLoaded"};
    CHECK(distinct == allowed);
    // NAMED NEGATIVELY TOO, because the interesting half of an authority audit is the
    // shapes that are ABSENT. Asking what is loaded is not being able to load anything:
    // a grant is per (shape, version, target), and none of these was ever spoken.
    for (const char* forbidden : {"zen.LoadWeave", "zen.SwapWeave", "zen.ReloadWeave",
                                  "zen.UnloadLibrary", "zen.UnloadRole", "zen.LoadLibrary",
                                  "SurfaceCanvas", "BuildRequested"}) {
        CHECK(std::find(said.begin(), said.end(), std::string(forbidden)) == said.end());
    }
}

TEST_CASE("INTR-0: unload and reload -- waiting is said, and a reload recovers the view") {
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::int64_t kind = intro_row(r, kIntroPane)->kind;
    REQUIRE(any_row(external_rows(r.last_canvas(), external_body_rect(r.session(), kind)),
                    intro::kIntrospectionStem));

    // THE PROVIDER LEAVES. Workshop is told NOTHING -- Loom gives a participant no
    // unload notification -- so the catalog row stays, the pane stays open, and the
    // rows a maker is looking at are the last valid ones. That is a stated limit and
    // not liveness.
    REQUIRE(r.unload(intro::kIntrospectionStem));
    REQUIRE(r.session().panels.runtime.entries.size() == kIntroPaneCount);
    CHECK(any_row(external_rows(r.last_canvas(), external_body_rect(r.session(), kind)),
                  intro::kIntrospectionStem));

    // ...and the next room grant is the moment the silence becomes visible. Workshop
    // clears its cache before every grant, so what a maker reads is WAITING -- never
    // `unavailable`, which is a fate nothing here has observed.
    r.extent(140, 40);
    const std::vector<std::string> gone =
        external_rows(r.last_canvas(), external_body_rect(r.session(), kind));
    REQUIRE(gone.size() == 1);
    CHECK(gone[0] == std::string(kExternalWaiting));

    // THE PROVIDER COMES BACK. Its attested activation offers the same `PaneRef`, which
    // refreshes the descriptor in place and clears the grant -- so the next repaint
    // grants room again and the view returns with no gesture from the maker.
    REQUIRE(r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice).valid());
    // identity de-duplicated all three
    CHECK(r.session().panels.runtime.entries.size() == kIntroPaneCount);
    const std::vector<std::string> back =
        external_rows(r.last_canvas(), external_body_rect(r.session(), kind));
    REQUIRE_FALSE(back.empty());
    CHECK(back[0] == "loaded weaves -- 1");
    CHECK(any_row(back, intro::kIntrospectionStem));
}

TEST_CASE("INTR-0: the pane composes as an ordinary saved setup row") {
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
    fresh.session().setup.active = authored;
    CHECK_FALSE(resolve_pane(intro_ref(), fresh.session().panels).has_value());
    CHECK(has_pane(fresh.session().setup.active, intro_ref()));
}

// ---- TYPE-0: WHICH TEXT IS SEMANTIC, AND WHICH TEXT IS CELLS ---------------------------
//
// THE ONE QUESTION THIS SECTION ASKS OF EVERY CASE: does this text name MEANING inside room
// it owns, or glyphs at cells something else owns? The first is a `SurfaceTextRegion`, whose
// interior a medium may set in its own face; the second is a `SurfaceLabel`, which is one
// cell per byte in every medium and is exactly right for a mark ON something.
//
// AND WHY A ONE-CELL ROW CANNOT BE THE FIRST. A canvas cell is `kCanvasCellPx` and this
// repository's face has an 18-pixel line, so `fit_region` answers ZERO rows for a region one
// cell tall and hands it back to the cell projection (HD-5). Publishing a one-cell label as a
// one-cell region therefore changes nothing a maker can see -- which is why the migrations
// below are all of runs of rows, and why the retentions below are all of single rows.

TEST_CASE("TYPE-0: the picker is ONE bounded region, and its cells are what it used to write") {
    Panels panels;
    panels.picker.open = true;
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    const Screen sc = screen_of(s);
    const ui::Rect box = pane_body_cells(picker_bounds(sc));

    surface::SurfaceCanvas c;
    paint_picker(plane(c), panels, setup_for(panels), sc, Keymap{});

    // ONE REGION AT THE SLOT, AND NOT ONE LABEL. The picker used to be a column of padded
    // labels; a label is one cell per byte in every medium, so a maker on a surface that owns
    // a real face read this list in the bitmap letterform beside an Inspector set in type.
    const std::vector<surface::SurfaceTextRegion> at_slot = regions_at(c, box.x, box.y);
    REQUIRE(at_slot.size() == 1);
    const surface::SurfaceTextRegion& list = at_slot.front();
    CHECK(list.w == box.w);
    CHECK(list.h == box.h);
    CHECK(list.caret_row == surface::kNoCaret); // nothing here is editable
    for (const surface::SurfaceLayer& l : c.layers) {
        CHECK(l.labels.empty()); // the backdrop rect is the only other thing this painter says
    }

    // AND IN A CHARACTER MEDIUM IT IS THE SAME PICTURE, CELL FOR CELL. The projection pads
    // every row of the region to its width and writes every cell row of it -- which is
    // byte-for-byte what `paint_panel_row` did for itself, so a terminal cannot tell which
    // spelling the picker chose.
    const std::vector<surface::SurfaceLabel> cells = cell_text_of(c);
    REQUIRE(cells.size() == static_cast<std::size_t>(box.h));
    for (std::size_t i = 0; i < cells.size(); ++i) {
        INFO("cell row ", i);
        CHECK(cells[i].x == box.x);
        CHECK(cells[i].y == box.y + static_cast<std::int64_t>(i));
        CHECK(static_cast<std::int64_t>(cells[i].text.size()) == box.w);
    }
    CHECK(cells[0].text.rfind("+ PANEL -- up/down, enter opens or removes", 0) == 0);
    CHECK(cells[0].role == surface::role::kAccent);
}

TEST_CASE("TYPE-0: the picker spends the ACTIVE medium's rows, and says what it omitted") {
    // THE SAME SLOT, TWO MEDIA, TWO HONEST BUDGETS -- HD-6's sentence pointed at the picker.
    // Nine cells of slot is nine rows of a character medium and five of an 18-pixel face, and
    // the list is windowed against whichever it was told rather than against the cells.
    Panels panels;
    panels.picker.open = true;
    const Screen cells = screen_of(screen_session(kScreenMinW, kScreenMinH, 0, 0));
    const Screen typed = screen_of(screen_session(kScreenMinW, kScreenMinH, 8, 18));
    const FineRect box = picker_bounds(cells);
    CHECK(picker_bounds(typed) == box); // the SLOT did not move: this is not layout work

    // THE BUDGET IS THE SLOT'S INTERIOR SINCE WUX-5: the picker wears the transient chrome
    // like every other framed surface, so the rows it spends are the rows inside it.
    const ui::Rect inner = pane_body_cells(box);
    const PanelProsePlace cell_place = panel_prose_place(box, cells);
    const PanelProsePlace typed_place = panel_prose_place(box, typed);
    CHECK(cell_place.rows == inner.h);
    CHECK(cell_place.columns == inner.w);
    CHECK(typed_place.rows ==
          (inner.h * surface::kCanvasCellPx - 2 * surface::kTextInsetPx) / 18);
    CHECK(typed_place.rows < cell_place.rows);
    CHECK(typed_place.columns > cell_place.columns); // ...and more characters across each one

    // A CATALOG TALLER THAN THE GRAPHICAL BUDGET IS WINDOWED THERE AND WHOLE IN CELLS, which
    // is the assertion that actually spends the difference: four offers plus four built-ins
    // fit the nine rows a terminal has (heading included) and not the five an 18-pixel face
    // has, so the two media show different lists of one population and each says what it
    // left out. The marker is paid for OUT of the budget rather than added beneath it --
    // `list_window`'s rule, which this migration spends rather than reimplements.
    //
    // THE OFFER COUNT IS THE CHARACTER MEDIUM'S OWN BUDGET MINUS THE HEADING AND THE
    // BUILT-INS, and it has moved twice: when a fourth built-in arrived (EDIT-1), and when
    // WUX-5 gave the picker the same visible boundary every other framed surface wears.
    // What this case needs is a population that exactly fills the character medium, so the
    // number of probes is DERIVED from that medium's answer and never a constant.
    const std::int64_t crowd_want = cell_place.rows - 1;
    Panels crowded = panels;
    for (std::int64_t i = 0; i < crowd_want - static_cast<std::int64_t>(kPanelKinds); ++i) {
        crowded.runtime.entries.push_back(
            RuntimePane{kFirstRuntimeKind + i, "zengine.probe",
                        "p" + std::to_string(i), "Probe" + std::to_string(i), "a summary", {}});
    }
    const std::vector<CatalogRow> crowd = inventory_rows(setup_for(crowded), crowded);
    REQUIRE(crowd.size() == static_cast<std::size_t>(crowd_want));
    const auto published = [&](const Screen& medium) {
        surface::SurfaceCanvas to;
        paint_picker(plane(to), crowded, setup_for(crowded), medium, Keymap{});
        const ui::Rect at = pane_body_cells(picker_bounds(medium));
        const std::vector<surface::SurfaceTextRegion> found = regions_at(to, at.x, at.y);
        REQUIRE(found.size() == 1);
        return found.front().rows;
    };
    const std::vector<surface::SurfaceTextRow> crowd_typed = published(typed);
    const std::vector<surface::SurfaceTextRow> crowd_cells = published(cells);
    // NEVER MORE ROWS THAN THE MEDIUM SAID IT FITS. A publisher that spent the CELLS here
    // would hand this medium nine rows for a room that holds five, and the four it could not
    // draw would vanish with nothing said about them.
    CHECK(static_cast<std::int64_t>(crowd_typed.size()) <= typed_place.rows);
    CHECK(static_cast<std::int64_t>(crowd_cells.size()) <= cell_place.rows);
    CHECK(crowd_typed.size() < crowd_cells.size());
    // The graphical list is windowed and SAYS SO; the character one holds the whole catalog.
    const auto marked = [](const std::vector<surface::SurfaceTextRow>& rows) {
        for (const surface::SurfaceTextRow& row : rows) {
            if (row.text.find(" more") != std::string::npos ||
                row.text.find(" earlier") != std::string::npos) {
                return true;
            }
        }
        return false;
    };
    CHECK(marked(crowd_typed));
    CHECK_FALSE(marked(crowd_cells));
    CHECK(crowd_cells.size() == crowd.size() + 1); // heading + every catalog row

    const std::vector<CatalogRow> catalog = inventory_rows(setup_for(panels), panels);
    // ⚠ SINCE WUX-5 EVEN THE PLAIN CATALOG OUTGROWS THE GRAPHICAL BUDGET, and the stale
    // precondition that it fitted is gone rather than weakened: the picker wears the
    // transient chrome now, so an 18-pixel face fits four rows in the slot's interior where
    // it fitted five in its whole extent -- a heading and three offers. The windowing that
    // follows from it is the same windowing `marked(crowd_typed)` above already proves is
    // honest, reached one population sooner. What is asserted below is the FIT, which is
    // this case's actual property and is unchanged.
    REQUIRE_FALSE(catalog.empty());

    surface::SurfaceCanvas c;
    paint_picker(plane(c), panels, setup_for(panels), typed, Keymap{});
    const std::vector<surface::SurfaceTextRegion> at_slot = regions_at(c, inner.x, inner.y);
    REQUIRE(at_slot.size() == 1);
    const surface::SurfaceTextRegion& list = at_slot.front();
    // Every row the publisher said fits the room it was told about -- the medium truncating
    // for it would be the second measurer this whole seam exists not to have.
    CHECK(static_cast<std::int64_t>(list.rows.size()) <= typed_place.rows);
    for (const surface::SurfaceTextRow& row : list.rows) {
        CHECK(static_cast<std::int64_t>(row.text.size()) <= typed_place.columns);
    }
    // AND A SUMMARY CUT IN CELLS SHOWS MORE OF ITSELF IN TYPE: the same room, read by a
    // medium that fits more characters into it. Measured on a real offered pane rather than
    // on the built-ins, whose summaries are short enough to fit either -- the live case is
    // exactly INTR-0's `Loaded`, whose sentence a maker read as `what the kernel has lo...`.
    //
    // ⚠ THE COLUMN COUNTS MOVED WITH THE CHROME (WUX-5) and the case stopped holding them:
    // the picker's interior is two cells narrower than its slot, so the numbers this used to
    // pin (48 cells, 71 columns) are the slot's and not the list's. What is asserted is the
    // PROPERTY -- one medium fits strictly more of the same sentence than the other, and
    // whatever either cannot fit it marks.
    CHECK(typed_place.columns > cell_place.columns);
    Panels offered = panels;
    offered.runtime.entries.push_back(
        RuntimePane{kFirstRuntimeKind, "zengine.introspection", "loaded", "Loaded",
                    "what the kernel has loaded, and each one's role", {}});
    // THE MEASURED ROW IS THE SELECTED ONE, because the graphical budget no longer holds
    // the whole catalog: `list_window` keeps the selection in the window (its rule 2), so
    // selecting the offered pane is what makes "the same row, read by two media" a
    // question both media can answer.
    offered.picker.cursor = kPanelKinds;
    const auto loaded_row = [&](const Screen& medium) {
        surface::SurfaceCanvas paint_to;
        paint_picker(plane(paint_to), offered, setup_for(offered), medium, Keymap{});
        const ui::Rect at = pane_body_cells(picker_bounds(medium));
        const std::vector<surface::SurfaceTextRegion> found = regions_at(paint_to, at.x, at.y);
        REQUIRE(found.size() == 1);
        std::string out;
        for (const surface::SurfaceTextRow& row : found.front().rows) {
            if (row.text.find("Loaded") != std::string::npos) {
                out = row.text;
            }
        }
        return out;
    };
    const std::string in_cells = loaded_row(cells);
    const std::string in_type = loaded_row(typed);
    REQUIRE_FALSE(in_cells.empty());
    REQUIRE_FALSE(in_type.empty());
    CHECK(in_cells.find(detail::kElided) != std::string::npos); // cut, and MARKED, in cells
    CHECK(in_type.size() > in_cells.size());                    // strictly more, in type
    CHECK(in_type.find("what the kernel has loaded") != std::string::npos);
}

TEST_CASE("ARR-0: the arrangement's visible statement is the ring on the pane itself") {
    // THE ROSTER PANEL IS RETIRED: entering a scope publishes no region in the picker's
    // slot; what says "you are arranging" is the affordance ring on the panes -- muted
    // over the arrangeable SET on the desk, accent on the pane the keyboard addresses --
    // eight glyphs at the same edge cells the pointer grabs (one geometry, HD-3).
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(t, ref_of(stock::kKind));
    enter_arrange_desk(t);
    const FineRect at = bounds_of(t.session().panels, t.session().setup.active,
                                  stock::kKind, screen_of(t.session()))
                            .rect;
    const auto ring_roles = [&](std::int64_t edge) {
        // THE WIRE CURRENCY: a label's x/y are canvas CELLS with the fine anchor in the
        // remainders -- asserting the sub-unit values here is how the misplaced WUX-2
        // construction stayed green while every mark painted off the canvas.
        const FineRect cell = pane_edge_cell(at, edge);
        std::vector<std::int64_t> roles;
        for (const surface::SurfaceLayer& l : t.canvases.back().layers) {
            for (const surface::SurfaceLabel& g : l.labels) {
                if (g.x == surface::cell_of_subs(cell.x) &&
                    g.y == surface::cell_of_subs(cell.y) &&
                    surface::subs_of_wire(g.x, g.sub_x) == cell.x &&
                    surface::subs_of_wire(g.y, g.sub_y) == cell.y &&
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
        CHECK(roles.front() == surface::role::kMuted);
    }
    // ADDRESSED: the keyboard's target wears the ring in accent.
    select_pane(t, ref_of(stock::kKind));
    for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
        const std::vector<std::int64_t> roles = ring_roles(edge);
        REQUIRE(roles.size() == 1);
        CHECK(roles.front() == surface::role::kAccent);
    }
    // AND NO REGION OPENED OVER THE PICKER'S SLOT: the statement is on the pane, not in
    // a panel covering another one.
    const ui::Rect box = cells_covered(picker_bounds(screen_of(t.session())));
    for (const surface::SurfaceLayer& l : t.canvases.back().layers) {
        for (const surface::SurfaceTextRegion& r : l.texts) {
            const bool roster_panel = r.x == box.x && r.y == box.y && !r.rows.empty() &&
                                      r.rows[0].text.rfind("+ WINDOW", 0) == 0;
            CHECK_FALSE(roster_panel);
        }
    }
    // LEAVING TAKES THE RING WITH IT -- the state's existence and its statement are one.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().arrange.open);
    for (std::int64_t edge = 0; edge < pane_edge::kCount; ++edge) {
        CHECK(ring_roles(edge).empty());
    }
}

TEST_CASE("TYPE-0/WUX-1: the notice is a band row, and the SENTENCE is never shortened") {
    WorkshopDoc d;
    doc::add_default(d);
    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    s.selected = d.elements.front().id;
    refocus(d, s);
    const Screen sc = screen_of(s);

    // THE BOTTOM BAND IS ONE REGION SINCE WUX-1 -- the whole of its reserved cells,
    // published whether or not there is a notice; an empty notice is a blank ROW of it, not a
    // missing region, because the band owns its rectangle in every state. Since QR-14 the
    // notice is that band's FIRST row: the identity that used to lead it has a band of its
    // own at the top of the screen.
    const std::vector<surface::SurfaceTextRegion> quiet =
        regions_at(paint(d, s), 0, sc.notice_y);
    REQUIRE(quiet.size() == 1);
    CHECK(quiet.front().rows.size() >= 1);
    CHECK(quiet.front().rows[0].text.empty());

    // A SENTENCE THAT FITS: one prose row of the band, whole.
    s.notice = "created #1 -- a new identity, not a new name";
    const surface::SurfaceCanvas said = paint(d, s);
    const std::vector<surface::SurfaceTextRegion> at_band =
        regions_at(said, 0, sc.notice_y);
    REQUIRE(at_band.size() == 1);
    const surface::SurfaceTextRegion& band = at_band.front();
    CHECK(band.w == sc.w);
    CHECK(band.h == kBottomRows);
    CHECK(band.y + band.h == sc.h); // the band ends at the screen's own foot
    REQUIRE(band.rows.size() >= 1);
    CHECK(band.rows[0].text == s.notice);
    CHECK(band.rows[0].role == surface::role::kFill);
    // THE BAND'S FOUR CELLS HOLD TWO ROWS OF THIS FACE -- the budget the composition
    // spends: the notice, then one legend row. The third face row this band used to hold is
    // the identity's, and it is at the top of the screen now (QR-14).
    const surface::RegionFit fit = band_fit(sc);
    CHECK(fit.graphical());
    CHECK(fit.rows == 2);
    CHECK(layouts_body(s, sc).rows == 1); // ...and the identity's own row, up there

    // A BAD ONE WEARS THE ALERT ROLE, which is the second signal and not a second sentence.
    s.notice_is_bad = true;
    const surface::SurfaceCanvas bad = paint(d, s);
    const std::vector<surface::SurfaceTextRegion> at_bad =
        regions_at(bad, 0, sc.notice_y);
    REQUIRE(at_bad.size() == 1);
    CHECK(at_bad.front().rows[0].role == surface::role::kAlert);

    // A SENTENCE TOO LONG FOR THE ROOM IS MARKED, AND `Session::notice` STILL HOLDS ALL OF
    // IT. What a maker sees is bounded; what Workshop knows is not.
    s.notice = std::string(400, 'x') + "-END";
    s.notice_is_bad = false;
    const surface::SurfaceCanvas cut = paint(d, s);
    const std::vector<surface::SurfaceTextRegion> at_cut =
        regions_at(cut, 0, sc.notice_y);
    REQUIRE(at_cut.size() == 1);
    const surface::SurfaceTextRow& row = at_cut.front().rows[0];
    CHECK(static_cast<std::int64_t>(row.text.size()) == fit.columns);
    CHECK(row.text.find(detail::kElided) != std::string::npos);
    CHECK(row.text.find("-END") == std::string::npos);
    CHECK(s.notice.size() == 404); // the sentence itself was never touched

    // AND THE SAME PUBLICATION IN A CHARACTER MEDIUM: the same cells, cut at the cells the
    // medium has rather than at the columns the face has. One publisher, two projections.
    Session cell = s;
    cell.text_advance_px = 0;
    cell.text_line_px = 0;
    const Screen cell_sc = screen_of(cell);
    const surface::SurfaceCanvas in_cells = paint(d, cell);
    const std::vector<surface::SurfaceTextRegion> at_cells =
        regions_at(in_cells, 0, cell_sc.notice_y);
    REQUIRE(at_cells.size() == 1);
    const surface::SurfaceTextRow& cell_row = at_cells.front().rows[0];
    CHECK(static_cast<std::int64_t>(cell_row.text.size()) == cell_sc.w);
    CHECK(cell_row.text.find(detail::kElided) != std::string::npos);
    CHECK(cell_row.text.size() < row.text.size()); // fewer cells than the face has columns
}

TEST_CASE("TYPE-0/TYPE-1: cell text is RETAINED where the CELL is the meaning") {
    // THE POSITIVE HALF OF TYPE-0's LESSON, and it survives TYPE-1 whole: these publications
    // stay `SurfaceLabel` because their glyphs sit at ONE cell that something else already
    // fills, and the cell is the meaning rather than the room. TYPE-1 moved the object NAME
    // out of this list -- a name is a sentence and not a cell -- and moved nothing else.
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "panel", 1, 1, ui::Extent{ui::kExtentCells, 12},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    s.selected = id;
    refocus(d, s);
    const surface::SurfaceCanvas c = paint(d, s);

    bool handle = false;
    for (const surface::SurfaceLabel& l : all_labels(c)) {
        handle = handle || l.text == std::string(kHandleGlyph);
        // AND THE NAME IS NOT A LABEL ANY MORE -- the other half of the same partition. A
        // label here would be back to a bitmap letterform in a medium that owns a real face.
        CHECK(l.text != "panel");
    }
    CHECK(handle); // the size handle: one glyph, at one cell, over the ring that fills it
}

TEST_CASE("TYPE-0: a pane with room for the header and nothing else still says whose it is") {
    // THE EDGE THE HEADER'S MOVE CREATED, and it is pinned rather than argued: the header used
    // to be a CELL row written before the body was resolved, so a panel too short for a body
    // still carried it. Now it is the region's first PROSE row, and reserving it can leave the
    // provider nothing -- at which point the question "is there a body" must be asked AFTER
    // the header is written, or a maker gets a rectangle that says nothing at all.
    Panels panels;
    panels.runtime.entries.push_back(RuntimePane{kFirstRuntimeKind, "zengine.probe", "p",
                                                 "Probe", "a summary", {}});
    ExternalPane live;
    live.kind = kFirstRuntimeKind;
    live.heard = true;
    live.shown.push_back(surface::SurfaceTextRow{"a provider row", surface::role::kFill});
    panels.external.push_back(live);

    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    const Screen sc = screen_of(s);
    // FOUR CELLS: two of chrome (WUX-5) around two of interior, which is one prose row of
    // an 18-pixel face -- and the header takes it whole.
    const ui::Rect tiny{0, 1, 48, 2 + 2 * kChromeCells};
    const ExternalBodyPlace body = external_body_place(
        fine_of_cells(tiny), sc,
        external_title_rows(panels, kFirstRuntimeKind, /*titles_shown=*/true));
    CHECK(body.fit.rows == 1);
    CHECK_FALSE(body.present); // no room was granted, and none is invented
    CHECK(body.rows == 0);

    surface::SurfaceCanvas c;
    paint_external(plane(c), panels, kFirstRuntimeKind, fine_of_cells(tiny), sc,
                   /*titles=*/true);
    const std::vector<surface::SurfaceTextRegion> at =
        regions_at(c, tiny.x + kChromeCells, tiny.y + kChromeCells);
    REQUIRE(at.size() == 1);
    REQUIRE(at.front().rows.size() == 1);
    // ...AND IT CARRIES MSG-0'S MARK, unset: a header is the same width whether or not
    // the pane has the keyboard, because the unmarked form spends the same two columns.
    CHECK(at.front().rows[0].text == std::string(kTypingElsewhere) + "Probe @zengine.probe");
    CHECK(at.front().rows[0].role == surface::role::kAccent);

    // AND WITH NO ROW OF TYPE AT ALL the painter says nothing rather than drawing a header
    // into a rectangle that cannot hold one.
    const ui::Rect none{0, 1, 48, 1};
    CHECK(surface::fit_region(none.x, none.y, none.w, none.h, 8, 18).graphical() == false);
    surface::SurfaceCanvas flat;
    paint_external(plane(flat), panels, kFirstRuntimeKind, FineRect{}, sc,
                   /*titles=*/true);
    CHECK(all_texts(flat).empty());
}

// ---- TYPE-1: SEMANTIC TYPE ON MATERIAL SOMEBODY ELSE OWNS -------------------------------
//
// THE ONE QUESTION THIS SECTION ASKS: when a maker's own word has to be written ACROSS an
// authored object, what does each medium show where the word is not?
//
// TYPE-0 answered "cells", because the two things a region could be told were "clear this
// rectangle to the canvas" and "clear this rectangle to the canvas, and paint these row
// strips" -- and both erase an object drawn one line earlier. `surface::kGroundBeneath` is
// the third answer and the whole of TYPE-1's vocabulary: the region keeps its BOUNDS, so its
// rows are fitted and cut against them, and gives up its GROUND, so it paints nothing it was
// not given. A character medium reaches that by not padding; a graphical one by not filling.
//
// THE TWO PROJECTIONS ARE PINNED SEPARATELY AND SAY THE SAME THING, which is the property
// this whole vocabulary rests on: the terminal keeps `glyph_for_role`'s `#` in every cell the
// name does not occupy, and the window keeps the object's own quad under every pixel the type
// does not ink. Neither depends on colour.

TEST_CASE("TYPE-1: the object's name is set in the medium's own type, ON its material") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "panel", 1, 1, ui::Extent{ui::kExtentCells, 12},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    s.selected = id;
    refocus(d, s);
    const surface::SurfaceCanvas c = paint(d, s);

    // ONE REGION PER PLACED OBJECT, ON THE WORKSPACE'S OWN PLANE, at the object's resolved
    // origin -- and its ground is the one thing that makes it legal there.
    const std::vector<surface::SurfaceTextRegion> names = object_names(c);
    REQUIRE(names.size() == 1);
    CHECK(names.front().x == kWorkspaceX + 1);
    CHECK(names.front().y == kWorkspaceY + 1);
    CHECK(names.front().h == 4); // the object's own height
    CHECK(names.front().ground == surface::kGroundBeneath);
    REQUIRE(names.front().rows.size() == 1);
    CHECK(names.front().rows.front().text == "panel");
    CHECK(names.front().rows.front().role == surface::role::kMuted);
    CHECK(names.front().rows.front().background == surface::role::kNone);

    // IT IS IN THE TYPE LIST, which is the product ask: the real face rather than the 5x5
    // bitmap letterform every label goes through.
    const surface::SurfaceExtent metric{kScreenMinW * surface::kCanvasCellPx,
                                        kScreenMinH * surface::kCanvasCellPx, 8, 18};
    const surface::PlanLayer planned =
        surface::plan_canvas(c, metric, surface::PlanSize{4000, 4000}).front();
    REQUIRE(planned.regions.size() == 1);
    CHECK(planned.regions.front().line_px == 18);
    CHECK(planned.regions.front().ground == surface::kGroundBeneath);
    REQUIRE(planned.regions.front().rows.size() == 1);
    CHECK(planned.regions.front().rows.front().text == "panel");

    // AND THE MATERIAL UNDER IT IS UNTOUCHED, at the pixel. The object's quad is planned and
    // there is no quad of canvas ground anywhere inside it -- which is exactly what the six
    // label cells used to be, one per character of the name.
    const std::int64_t ox = (kWorkspaceX + 1) * surface::kCanvasCellPx;
    const std::int64_t oy = (kWorkspaceY + 1) * surface::kCanvasCellPx;
    bool material = false;
    bool punched = false;
    for (const surface::PlanRect& q : planned.quads) {
        material = material ||
                   (q == surface::PlanRect{ox, oy, 12 * surface::kCanvasCellPx,
                                           4 * surface::kCanvasCellPx, 176, 176, 188});
        const bool inside = q.x >= ox && q.y >= oy && q.x < ox + 12 * surface::kCanvasCellPx &&
                            q.y < oy + 4 * surface::kCanvasCellPx;
        const bool cleared = q.r == surface::kCanvasBackground.r &&
                             q.g == surface::kCanvasBackground.g &&
                             q.b == surface::kCanvasBackground.b;
        punched = punched || (inside && cleared);
    }
    CHECK(material);
    CHECK_FALSE(punched);
}

TEST_CASE("TYPE-1: the character medium's picture did not move, and its `#` is why") {
    // THE MONOCHROME WITNESS. A terminal that cannot distinguish four colours still knows
    // there is authored material here, because the material is a GLYPH -- and the name's
    // migration left every cell of it that the name does not occupy exactly as it was.
    WorkshopDoc d;
    (void)doc::add(d, "widget", 1, 1, ui::Extent{ui::kExtentCells, 12},
                   ui::Extent{ui::kExtentCells, 4});
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    const std::string cells = plain_cells(paint(d, s));

    // The name's six cells, then six cells of `#` completing the object's first row...
    CHECK(cells.find("widget######") != std::string::npos);
    // ...and its three whole rows below.
    CHECK(cells.find("############") != std::string::npos);
    // NOT ONE BACKGROUND BYTE was emitted for any of it: a ground would be colour, and colour
    // is the thing `glyph_for_role` exists to refuse to depend on.
    CHECK(surface::canvas_body(paint(d, s)).find("\x1b[47m") == std::string::npos);

    // AND THE SAME OBJECT UNDER A REAL METRIC PROJECTS THE SAME CELLS. The rest of the screen
    // does not -- the Inspector's body is a region and spends the FACE's rows there -- but the
    // workspace plane is a cell picture in both, because the region resolves through the same
    // `fit_region` both media call.
    Session typed = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    const std::string typed_cells = plain_cells(paint(d, typed));
    CHECK(typed_cells.find("widget######") != std::string::npos);
    CHECK(typed_cells.find("############") != std::string::npos);
}

TEST_CASE("QR-3: the name's bound is the OBJECT'S resolved width, clipped by the workspace") {
    // THE ONE BEHAVIOUR QR-3 CHANGED, and it is worth stating what it replaced: TYPE-1 gave
    // the name `workspace_w - x` cells, so a name longer than the object it names ran out of
    // it and across the backdrop -- where, in a medium that paints roles as ink, it was the
    // backdrop's exact colour (both are `kMuted`) and could not be read at all. The room is
    // the MATERIAL's now, because this is type ON material, and material the object does not
    // have is not the name's to spend.
    WorkshopDoc d;
    (void)doc::add(d, "a name much longer than its object", 1, 1,
                   ui::Extent{ui::kExtentCells, 4}, ui::Extent{ui::kExtentCells, 4});
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    const std::vector<surface::SurfaceTextRegion> names = object_names(paint(d, s));
    REQUIRE(names.size() == 1);
    CHECK(names.front().w == 4); // the object's own 4 cells, NOT the 47 to the edge
    CHECK(names.front().h == 4); // and its own height, which TYPE-1 already gave it
    CHECK(names.front().rows.front().text == "a...");
    CHECK(doc::find(d, d.elements[0].id)->label == "a name much longer than its object");

    // AND THE WORKSPACE'S EDGE IS STILL A BOUND -- the tighter of the two wins. An object
    // authored wider than the room to the edge has material the workspace does not show, and
    // its name is not the panel's to write into either.
    // ⚠ AT COLUMN 70, NOT 44. The room is the surface now, so the minimum screen's workspace
    // is 78 cells rather than 48 and an object at 44 no longer overhangs anything -- which
    // would have made this a case about clipping that never clipped.
    WorkshopDoc over;
    (void)doc::add(over, "a name much longer than its object", 70, 0,
                   ui::Extent{ui::kExtentCells, 12}, ui::Extent{ui::kExtentCells, 1});
    Session os = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    const std::vector<surface::SurfaceTextRegion> clipped = object_names(paint(over, os));
    REQUIRE(clipped.size() == 1);
    CHECK(clipped.front().w == os.workspace_w - 70); // 8, not the object's 12

    // WHAT A MEDIUM GETS TO SAY IS STILL HOW MANY CHARACTERS THOSE CELLS HOLD (TYPE-1,
    // unchanged): the bound is cells either way, but a face whose advance is narrower than a
    // cell fits more of them in it. Both are `fit_region`, and QR-3 changed only its `room`.
    CHECK(surface::fit_region(1, 2, 12, 4, 0, 0).columns == 12);
    CHECK(surface::fit_region(1, 2, 12, 4, 8, 18).columns == (12 * 12 - 4) / 8); // 17

    // SO A WIDER OBJECT SHOWS MORE OF THE SAME AUTHORED BYTES, which is the whole product
    // claim: resizing changes how much is visible and never what was written.
    struct Case {
        std::int64_t width;
        const char* shown;
    };
    for (const Case& one : {Case{3, "..."}, Case{4, "a..."}, Case{12, "a name mu..."},
                            Case{34, "a name much longer than its object"},
                            Case{40, "a name much longer than its object"}}) {
        CAPTURE(one.width);
        WorkshopDoc w;
        const std::int64_t id = doc::add(w, "a name much longer than its object", 1, 1,
                                         ui::Extent{ui::kExtentCells, one.width},
                                         ui::Extent{ui::kExtentCells, 4});
        Session ws = screen_session(kScreenMinW, kScreenMinH, 0, 0);
        const std::vector<surface::SurfaceTextRegion> shown = object_names(paint(w, ws));
        REQUIRE(shown.size() == 1);
        CHECK(shown.front().rows.front().text == one.shown);
        CHECK(doc::find(w, id)->label == "a name much longer than its object"); // never cut
    }

    // AND A NAME THAT GENUINELY DOES NOT FIT IS MARKED, never silently cut, in either medium.
    WorkshopDoc edge;
    const std::int64_t id = doc::add(edge, "a-name-far-too-long-for-here", 44, 0,
                                     ui::Extent{ui::kExtentCells, 2},
                                     ui::Extent{ui::kExtentCells, 1});
    Session es = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    const std::vector<surface::SurfaceTextRegion> cut = object_names(paint(edge, es));
    REQUIRE(cut.size() == 1);
    CHECK(cut.front().rows.front().text == ".."); // two cells of room, all of it the mark
    CHECK(doc::find(edge, id)->label == "a-name-far-too-long-for-here"); // the document keeps all
}

TEST_CASE("QR-3: no part of a name is drawn outside the material it names") {
    // THE PRODUCT CLAIM, MEASURED IN PIXELS RATHER THAN ARGUED. This is the case that would
    // have gone red on the pristine tree: a 6-cell object with a 32-byte name planned a
    // 564 px region against 72 px of material, and 23 of the 32 characters landed on a
    // backdrop wearing the name's own ink.
    WorkshopDoc d;
    (void)doc::add(d, "a long name across the workspace", 1, 1,
                   ui::Extent{ui::kExtentCells, 6}, ui::Extent{ui::kExtentCells, 4});
    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    const surface::SurfaceCanvas c = paint(d, s);

    // The region's own bounds first: it is the object's rectangle and nothing more.
    const std::vector<surface::SurfaceTextRegion> names = object_names(c);
    REQUIRE(names.size() == 1);
    CHECK(names.front().w == 6);

    // ...and the PLANNED region, whose viewport is what the renderer sets type inside.
    const surface::SurfaceExtent metric{kScreenMinW * surface::kCanvasCellPx,
                                        kScreenMinH * surface::kCanvasCellPx, 8, 18};
    const surface::PlanLayer planned =
        surface::plan_canvas(c, metric, surface::PlanSize{4000, 4000}).front();
    REQUIRE(planned.regions.size() == 1);
    const surface::PlanTextRegion& region = planned.regions.front();
    const std::int64_t ox = (kWorkspaceX + 1) * surface::kCanvasCellPx;
    const std::int64_t oy = (kWorkspaceY + 1) * surface::kCanvasCellPx;
    const std::int64_t ow = 6 * surface::kCanvasCellPx;
    const std::int64_t oh = 4 * surface::kCanvasCellPx;
    CHECK(region.view.x == ox);
    CHECK(region.view.y == oy);
    CHECK(region.view.w == ow); // 72 px, the material's own width -- 564 before QR-3
    CHECK(region.view.h == oh);
    // Every character the medium was given fits inside that material at the fit's own advance.
    REQUIRE(region.rows.size() == 1);
    CHECK(static_cast<std::int64_t>(region.rows.front().text.size()) * 8 <= ow);
    CHECK(region.rows.front().text == "a lon..."); // 8 columns of the face fit in 6 cells
    // AND THE MATERIAL IS STILL WHOLE UNDER IT (TYPE-1 preserved): the object's quad is
    // planned and nothing inside it was cleared to the canvas ground.
    bool material = false;
    bool punched = false;
    for (const surface::PlanRect& q : planned.quads) {
        material = material || (q == surface::PlanRect{ox, oy, ow, oh, 176, 176, 188});
        const bool inside = q.x >= ox && q.y >= oy && q.x < ox + ow && q.y < oy + oh;
        const bool cleared = q.r == surface::kCanvasBackground.r &&
                             q.g == surface::kCanvasBackground.g &&
                             q.b == surface::kCanvasBackground.b;
        punched = punched || (inside && cleared);
    }
    CHECK(material);
    CHECK_FALSE(punched);
}

TEST_CASE("TYPE-1: a tiny object shows its name in CELLS, and no rule was written to say so") {
    // §7. `fit_region` sends a region with no room for one row of the medium's face back to
    // the cell projection (HD-5), so an object one cell tall is drawn by the same glyph loop
    // it always was rather than by 18 pixels of type hanging out of a 12-pixel object. There
    // is no `if (h < N)` anywhere in `paint`: this is the rule both media already resolve
    // with, applied to a height a maker chose.
    const surface::SurfaceExtent metric{kScreenMinW * surface::kCanvasCellPx,
                                        kScreenMinH * surface::kCanvasCellPx, 8, 18};
    struct Case {
        std::int64_t height;
        bool typed;
    };
    for (const Case& one : {Case{1, false}, Case{2, true}, Case{3, true}, Case{4, true}}) {
        CAPTURE(one.height);
        WorkshopDoc d;
        (void)doc::add(d, "tiny", 1, 1, ui::Extent{ui::kExtentCells, 8},
                       ui::Extent{ui::kExtentCells, one.height});
        Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
        const surface::SurfaceCanvas c = paint(d, s);
        REQUIRE(object_names(c).size() == 1);
        CHECK(object_names(c).front().h == one.height);
        const surface::PlanLayer planned =
            surface::plan_canvas(c, metric, surface::PlanSize{4000, 4000}).front();
        CHECK(planned.regions.size() == (one.typed ? 1U : 0U));
        // AND WHICHEVER LIST IT LANDED IN, THE MATERIAL IS STILL THERE. A one-cell object is
        // drawn by the bitmap face as cells -- and those cells are NOT cleared first, because
        // the row carries its region's ground through the projection.
        const std::int64_t ox = (kWorkspaceX + 1) * surface::kCanvasCellPx;
        const std::int64_t oy = (kWorkspaceY + 1) * surface::kCanvasCellPx;
        bool punched = false;
        for (const surface::PlanRect& q : planned.quads) {
            const bool inside = q.x >= ox && q.y >= oy &&
                                q.x < ox + 8 * surface::kCanvasCellPx &&
                                q.y < oy + one.height * surface::kCanvasCellPx;
            const bool cleared = q.r == surface::kCanvasBackground.r &&
                                 q.g == surface::kCanvasBackground.g &&
                                 q.b == surface::kCanvasBackground.b;
            punched = punched || (inside && cleared);
        }
        CHECK_FALSE(punched);
    }
}

TEST_CASE("TYPE-1: an object with no resolved height still shows its name") {
    // THE FLOOR, AND WHY IT IS NOT A FUDGE. `check_extent` refuses an authored height below
    // one cell, so this is reachable only from a poke or a hand-built document -- but it was
    // reachable BEFORE TYPE-1, and such an object's name was the only trace of it on the
    // workspace. A region with no bounds shows nothing and says nothing about it, so the
    // name's room is the object's height or one row, whichever is more.
    //
    // QR-3 GIVES THE OTHER AXIS THE SAME FLOOR, for the same reason and by the same
    // arithmetic: the room is the object's WIDTH now, and a zero-width object would otherwise
    // publish no region at all. One column is what `detail::fit` needs to leave a mark, so a
    // bodyless object is still a thing on the workspace that says "a name is here".
    WorkshopDoc d;
    (void)doc::add(d, "bodyless", 1, 1, ui::Extent{ui::kExtentCells, 0},
                   ui::Extent{ui::kExtentCells, 0});
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    const surface::SurfaceCanvas c = paint(d, s);
    REQUIRE(object_names(c).size() == 1);
    CHECK(object_names(c).front().h == 1);
    CHECK(object_names(c).front().w == 1);
    CHECK(object_names(c).front().rows.front().text == "."); // the mark, in the room there is
    // ⭐ AND THE OBJECTS LIST THAT NAMED IT WHOLE IS A WEAVE'S NOW. This case asserted that
    // a maker could still read the full name in Info's column when the object itself had no
    // room for it; that half is `test_workshop_panes_info.cpp`'s, over the same document
    // crossing the same seam. What stays here is that the DOCUMENT is untouched by the
    // projection -- the name is whole in the document whatever the room can draw.
    CHECK(doc::find(d, d.elements[0].id)->label == "bodyless");
}

TEST_CASE("TYPE-1: the name is over every object's material and under nothing it should be") {
    // §10. Painter's order inside the workspace plane is rects, then labels, then regions --
    // so a name is drawn over every object's body, exactly as a label was drawn over every
    // rect. What changed is that the size handle is now drawn BEFORE the names rather than
    // after; it never shares a cell with one, because the handle sits at `rect.y + rect.h`,
    // one row past the region's last.
    WorkshopDoc d;
    // NOT `near`: that is a macro in <windows.h>, which the Windows lanes drag in through the
    // terminal Skin -- and the diagnostic it produces names a line four statements away.
    const std::int64_t left_id = doc::add(d, "left", 1, 1, ui::Extent{ui::kExtentCells, 6},
                                          ui::Extent{ui::kExtentCells, 3});
    (void)doc::add(d, "right", 10, 1, ui::Extent{ui::kExtentCells, 6},
                   ui::Extent{ui::kExtentCells, 3});
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.selected = left_id;
    refocus(d, s);
    const surface::SurfaceCanvas c = paint(d, s);

    // BOTH NAMES ARE ON ONE PLANE, in scene order, and the workspace publishes no other.
    const std::vector<surface::SurfaceTextRegion> names = object_names(c);
    REQUIRE(names.size() == 2);
    CHECK(names[0].rows.front().text == "left");
    CHECK(names[1].rows.front().text == "right");

    // THE HANDLE IS A ROW BELOW THE REGION IT BELONGS TO, so the two never contend.
    const Handle handle = size_handle(d, s);
    REQUIRE(handle.shown);
    CHECK(kWorkspaceY + handle.y == names[0].y + names[0].h);

    // AND A PANE IN FRONT STILL COVERS A NAME WHOLE, because a pane is a later PLANE and a
    // ground given up changes nothing about which plane is in front (WIND-2a).
    CHECK(c.layers.size() > 1);
}

// ========================================================================================
// WUX-2 — fine-grained arrangement: the pane lattice is sub-cell, gestures are
// pixel-responsive, every resize edge preserves its opposite anchor, the TUI stays an
// honest cell projection, and the persisted formats migrate deliberately.
// ========================================================================================

namespace {

/// A Live Workshop on a graphical medium: a real text metric, so pointer events arrive
/// in window pixels and the fine lattice is reachable at its pixel grain.
struct FineRig : Live {
    FineRig() {
        publish(loom::to_value(surface::SurfaceExtent{160, 44, 8, 18}));
        open_pane(*this, ref_of(stock::kKind));
        enter_arrange_desk(*this);
        select_pane(*this, ref_of(stock::kKind));
    }

    void motion_at(std::int64_t x, std::int64_t y, std::int64_t space) {
        publish(loom::to_value(input::PointerMoved{x, y, 0, 0, space, input::mod::kNone}));
    }

    const SetupPane* builder_row() {
        return pane_of(session().setup.active, ref_of(stock::kKind));
    }

    FineRect builder_rect() {
        return bounds_of(session().panels, session().setup.active, stock::kKind,
                         screen_of(session()))
            .rect;
    }
};

} // namespace

TEST_CASE("WUX-2: a one-pixel drag moves a pane by exactly one pixel of lattice") {
    FineRig t;
    const FineRect at = t.builder_rect();
    // PRESS THE BODY, midway in, in window pixels: the fine projection of that pixel is
    // exact (one pixel is four sub-units on this skin), so the grab offset is exact too.
    const std::int64_t press_x = surface::px_of_subs(at.x) + 30;
    const std::int64_t press_y = surface::px_of_subs(at.y) + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);

    // ONE PIXEL RIGHT: the place moves by exactly the pixel's worth of sub-units — no
    // whole-cell threshold anywhere on the path (the START tree needed twelve pixels of
    // hand before anything moved at all).
    t.motion_at(press_x + 1, press_y, input::space::kPixels);
    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    REQUIRE(row->place.mode == pane_unit::kSubcells);
    const std::int64_t base_x = at.x;
    CHECK(row->place.x == base_x + surface::kPixelGrainSubs);
    CHECK(row->place.y == at.y);

    // TWELVE ONE-PIXEL STEPS LAND EXACTLY ONE CELL OVER — stability under repeated small
    // deltas: every motion proposes from the press's base, so the sum is the distance the
    // hand travelled and nothing accumulates or drifts through a cell round-trip.
    for (std::int64_t i = 2; i <= 12; ++i) {
        t.motion_at(press_x + i, press_y, input::space::kPixels);
    }
    CHECK(t.builder_row()->place.x == base_x + subs(1));
    CHECK(t.builder_row()->place.y == at.y);
    // ...and the SAME hand position always means the same place: jitter back and forth
    // and the pane is wherever the pointer last was, not somewhere error piled up.
    t.motion_at(press_x + 5, press_y, input::space::kPixels);
    t.motion_at(press_x + 12, press_y, input::space::kPixels);
    CHECK(t.builder_row()->place.x == base_x + subs(1));
    t.release(0, 0);
}

TEST_CASE("WUX-2: every edge resizes pixel-fine and preserves its opposite anchor") {
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
        REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), subs(6),
                                  subs(6))
                    .accepted);
        const FineRect base = t.builder_rect();
        const std::int64_t left = base.x;
        const std::int64_t top = base.y;
        const std::int64_t right = surface::add_cells(base.x, base.w);
        const std::int64_t bottom = surface::add_cells(base.y, base.h);

        // PRESS THE EDGE'S OWN MARK, at the pixel its painted band begins on.
        const FineRect mark = pane_edge_cell(base, pull.edge);
        const std::int64_t press_x = surface::px_of_subs(mark.x) + 2;
        const std::int64_t press_y = surface::px_of_subs(mark.y) + 2;
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
        const std::int64_t g = surface::kPixelGrainSubs;
        const std::int64_t now_x =
            row->place.mode == pane_unit::kSubcells ? row->place.x : base.x;
        const std::int64_t now_y =
            row->place.mode == pane_unit::kSubcells ? row->place.y : base.y;
        const std::int64_t now_w =
            row->width.mode == pane_unit::kSubcells ? row->width.amount : base.w;
        const std::int64_t now_h =
            row->height.mode == pane_unit::kSubcells ? row->height.amount : base.h;
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

TEST_CASE("WUX-2: the reported top-edge defect is dead -- the bottom edge holds still") {
    // THE START TREE'S MEASURED DEFECT, exactly: drag-resizing from the top edge changed
    // the height by the correct amount and failed to move `y`, so the BOTTOM edge moved
    // instead of staying anchored (reproduced at this phase's START: height 9 -> 10 with
    // y fixed at 20, bottom 29 -> 30). This case is that scenario, asserting the law that
    // makes it unsayable.
    FineRig t;
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), 0, subs(20))
                .accepted);
    const FineRect base = t.builder_rect();
    REQUIRE(base.y == subs(20));
    REQUIRE(base.h == subs(9));
    const std::int64_t bottom_before = surface::add_cells(base.y, base.h);

    // TAKE THE TOP EDGE'S MARK AND PULL UP ONE WHOLE CELL, in pixels.
    const FineRect mark = pane_edge_cell(base, pane_edge::kTop);
    const std::int64_t press_x = surface::px_of_subs(mark.x) + 2;
    const std::int64_t press_y = surface::px_of_subs(mark.y) + 2;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.sizing);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kTop);
    t.motion_at(press_x, press_y - surface::kCanvasCellPx, input::space::kPixels);

    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    // THE HEIGHT GREW BY THE PULL, `y` MOVED WITH IT, AND THE BOTTOM EDGE DID NOT MOVE.
    CHECK(row->height.mode == pane_unit::kSubcells);
    CHECK(row->height.amount == base.h + subs(1));
    CHECK(row->place.y == base.y - subs(1));
    CHECK(row->place.y + row->height.amount == bottom_before);
    t.release(0, 0);
}

TEST_CASE("WUX-2: a refused anchored resize writes neither the place nor the size") {
    // PULL THE TOP EDGE DOWN PAST THE MINIMUM: the proposed height is illegal, and the
    // proposal carries a moved `y` beside it in the SAME vertical axis. The axis is
    // atomic (WUX-2a): `y` and the height settle together or not at all, so a moved top
    // edge beside a refused height is exactly what `author_pane_window` makes unsayable
    // — and with no other axis proposed, the whole gesture is a refusal.
    FineRig t;
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), subs(4),
                              subs(20))
                .accepted);
    const SetupPane before = *t.builder_row();
    const FineRect base = t.builder_rect();
    const FineRect mark = pane_edge_cell(base, pane_edge::kTop);
    const std::int64_t press_x = surface::px_of_subs(mark.x) + 2;
    const std::int64_t press_y = surface::px_of_subs(mark.y) + 2;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kTop);
    // Down by the whole height: h' would be zero, which is below the one-cell floor.
    t.motion_at(press_x, press_y + surface::px_of_subs(base.h), input::space::kPixels);
    INFO(t.session().notice);
    CHECK(t.session().notice_is_bad);
    CHECK(*t.builder_row() == before);

    // AND THE PLACE WALL HOLDS THE WHOLE AXIS TOO: pulling the LEFT edge past the
    // canvas's own origin proposes a negative x beside a legal width — one horizontal
    // transaction — and neither member lands.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), subs(1),
                              subs(20))
                .accepted);
    t.release(0, 0);
    const SetupPane at_wall = *t.builder_row();
    const FineRect wall_base = t.builder_rect();
    const FineRect left_mark = pane_edge_cell(wall_base, pane_edge::kLeft);
    const std::int64_t lx = surface::px_of_subs(left_mark.x) + 2;
    const std::int64_t ly = surface::px_of_subs(left_mark.y) + 2;
    t.press_at(lx, ly, input::space::kPixels);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kLeft);
    t.motion_at(lx - 2 * surface::kCanvasCellPx, ly, input::space::kPixels);
    CHECK(t.session().notice_is_bad);
    CHECK(*t.builder_row() == at_wall);
    t.release(0, 0);
}

TEST_CASE("WUX-2: a right or bottom resize leaves a default place reactive") {
    // THOSE EDGES ANCHOR THE PLACE BY NOT WRITING IT: a maker who widened a reactive pane
    // has said nothing about where it belongs, so it keeps following the developer's
    // tiling — and a top-edge pull on the same pane IS a placement decision (the bottom
    // edge's position becomes authored fact), so that one writes the place and takes the
    // pane out of the stack.
    FineRig t;
    const FineRect base = t.builder_rect();
    const FineRect right_mark = pane_edge_cell(base, pane_edge::kRight);
    const std::int64_t rx = surface::px_of_subs(right_mark.x) + 2;
    const std::int64_t ry = surface::px_of_subs(right_mark.y) + 2;
    t.press_at(rx, ry, input::space::kPixels);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kRight);
    t.motion_at(rx + 3, ry, input::space::kPixels);
    CHECK(t.builder_row()->width.mode == pane_unit::kSubcells);
    CHECK(t.builder_row()->place.mode == pane_unit::kDefault);
    t.release(0, 0);

    const FineRect grown = t.builder_rect();
    const FineRect top_mark = pane_edge_cell(grown, pane_edge::kTop);
    const std::int64_t tx = surface::px_of_subs(top_mark.x) + 2;
    const std::int64_t ty = surface::px_of_subs(top_mark.y) + 2;
    t.press_at(tx, ty, input::space::kPixels);
    REQUIRE(t.session().pane_drag.edge == pane_edge::kTop);
    t.motion_at(tx, ty - 1, input::space::kPixels);
    CHECK(t.builder_row()->place.mode == pane_unit::kSubcells);
    CHECK(t.builder_row()->place.y == grown.y - surface::kPixelGrainSubs);
    t.release(0, 0);
}

TEST_CASE("WUX-2a: a move blocked at the left wall still follows the hand down") {
    // THE LIVE DEFECT: a drag whose proposal leaves the canvas on ONE axis used to refuse
    // the WHOLE proposal, so a pane slid along the left wall froze on both axes.
    // Independent axes settle independently -- and the blocked coordinate KEEPS ITS OWN
    // VALUE rather than clamping to the wall, which is what staging the pane five
    // sub-units off the wall distinguishes (a clamp would write 0 here).
    FineRig t;
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), 5, subs(20))
                .accepted);
    const FineRect at = t.builder_rect();
    REQUIRE(at.x == 5);
    const std::int64_t press_x = surface::px_of_subs(at.x) + 30;
    const std::int64_t press_y = surface::px_of_subs(at.y) + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);

    // TEN PIXELS LEFT (past the wall) AND SEVEN DOWN: x' = 5 - 40 is refused, y' lands.
    t.motion_at(press_x - 10, press_y + 7, input::space::kPixels);
    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    REQUIRE(row->place.mode == pane_unit::kSubcells);
    CHECK(row->place.x == 5);
    CHECK(row->place.y == at.y + 7 * surface::kPixelGrainSubs);
    // SOMETHING LANDED, so this is a status and not an alert: the visible stop at the wall
    // is the refusal's consequence, and the pane tracking the hand is the statement.
    CHECK_FALSE(t.session().notice_is_bad);
    t.release(0, 0);
}

TEST_CASE("WUX-2a: a move blocked at the top wall still follows the hand sideways") {
    // THE MIRROR ORIENTATION: y refused, x lands.
    FineRig t;
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), subs(20), 5)
                .accepted);
    const FineRect at = t.builder_rect();
    REQUIRE(at.y == 5);
    const std::int64_t press_x = surface::px_of_subs(at.x) + 30;
    const std::int64_t press_y = surface::px_of_subs(at.y) + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE_FALSE(t.session().pane_drag.sizing);

    t.motion_at(press_x + 7, press_y - 10, input::space::kPixels);
    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    REQUIRE(row->place.mode == pane_unit::kSubcells);
    CHECK(row->place.x == at.x + 7 * surface::kPixelGrainSubs);
    CHECK(row->place.y == 5);
    CHECK_FALSE(t.session().notice_is_bad);
    t.release(0, 0);
}

TEST_CASE("WUX-2a: a move past two walls at once writes nothing") {
    // BOTH AXES REFUSED is the one case a move gesture is still refused WHOLE: authored
    // geometry is untouched, byte for byte, and the refusal is said as an alert.
    FineRig t;
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), 5, 5)
                .accepted);
    const SetupPane before = *t.builder_row();
    const FineRect at = t.builder_rect();
    const std::int64_t press_x = surface::px_of_subs(at.x) + 30;
    const std::int64_t press_y = surface::px_of_subs(at.y) + 20;
    t.press_at(press_x, press_y, input::space::kPixels);
    REQUIRE(t.session().pane_drag.active);
    t.motion_at(press_x - 10, press_y - 10, input::space::kPixels);
    CHECK(*t.builder_row() == before);
    CHECK(t.session().notice_is_bad);
    t.release(0, 0);
}

TEST_CASE("WUX-2a: a refused nudge does not author a reactive place") {
    // A KEY INTO THE WALL proposes a change on exactly one axis; the other axis, unchanged,
    // is NOT a proposal -- so nothing lands, the refusal is said, and in particular a
    // DEFAULT place is not converted to an authored one as a side effect of a step that
    // visibly did nothing. The reactive pane stays reactive.
    FineRig t;
    REQUIRE(t.builder_row()->place.mode == pane_unit::kDefault);
    const FineRect at = t.builder_rect();
    REQUIRE(at.x == 0); // the first stack tile sits on the wall (`kStackX`)
    t.key(input::scan::kLeft);
    CHECK(t.builder_row()->place.mode == pane_unit::kDefault);
    CHECK(t.builder_rect() == at);
    CHECK(t.session().notice_is_bad);

    // AND THE PANE IS NOT FROZEN: the very next step away from the wall lands, authoring
    // the place -- the blocked axis contributes the value it already stood at.
    t.key(input::scan::kDown);
    const SetupPane* row = t.builder_row();
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == pane_unit::kSubcells);
    CHECK(row->place.x == at.x);
    CHECK(row->place.y == at.y + surface::kCellSubs);
    CHECK_FALSE(t.session().notice_is_bad);
}

TEST_CASE("WUX-2a: a corner resize blocked on one axis still resizes the other") {
    // FOR ALL FOUR CORNERS: one axis meets a wall (the canvas origin, or the one-cell
    // minimum) and refuses ATOMICALLY -- its anchored position+extent pair holds together
    // -- while the other axis settles its own transaction, anchor law intact. Two corners
    // are blocked horizontally, two vertically, across both kinds of wall.
    const PaneRef builder = ref_of(stock::kKind);

    {
        CAPTURE("top-left: horizontal blocked at the canvas origin");
        FineRig t;
        REQUIRE(author_pane_place(live(t).setup.active, builder, 5, subs(20)).accepted);
        REQUIRE(author_pane_size(live(t).setup.active, builder,
                                 PaneSize{pane_unit::kSubcells, subs(9)},
                                 PaneSize{pane_unit::kSubcells, subs(9)})
                    .accepted);
        const FineRect base = t.builder_rect();
        const FineRect mark = pane_edge_cell(base, pane_edge::kTopLeft);
        const std::int64_t px = surface::px_of_subs(mark.x) + 2;
        const std::int64_t py = surface::px_of_subs(mark.y) + 2;
        t.press_at(px, py, input::space::kPixels);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kTopLeft);
        // TEN LEFT: x' = 5 - 40 is illegal, so x + width HOLD TOGETHER. SEVEN DOWN: the
        // top edge comes down legally, y + height settle together, bottom edge anchored.
        t.motion_at(px - 10, py + 7, input::space::kPixels);
        const SetupPane* row = t.builder_row();
        REQUIRE(row != nullptr);
        CHECK(row->place.x == 5);
        CHECK(row->width.amount == subs(9));
        CHECK(row->place.y == subs(20) + 7 * surface::kPixelGrainSubs);
        CHECK(row->height.amount == subs(9) - 7 * surface::kPixelGrainSubs);
        CHECK(row->place.y + row->height.amount == subs(20) + subs(9));
        t.release(0, 0);
    }
    {
        CAPTURE("bottom-left: horizontal blocked at the canvas origin");
        FineRig t;
        REQUIRE(author_pane_place(live(t).setup.active, builder, 5, subs(20)).accepted);
        REQUIRE(author_pane_size(live(t).setup.active, builder,
                                 PaneSize{pane_unit::kSubcells, subs(9)},
                                 PaneSize{pane_unit::kSubcells, subs(9)})
                    .accepted);
        const FineRect base = t.builder_rect();
        const FineRect mark = pane_edge_cell(base, pane_edge::kBottomLeft);
        const std::int64_t px = surface::px_of_subs(mark.x) + 2;
        const std::int64_t py = surface::px_of_subs(mark.y) + 2;
        t.press_at(px, py, input::space::kPixels);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kBottomLeft);
        // TEN LEFT refused; SEVEN DOWN grows the height from the bottom, top edge anchored
        // by not writing the place at all.
        t.motion_at(px - 10, py + 7, input::space::kPixels);
        const SetupPane* row = t.builder_row();
        REQUIRE(row != nullptr);
        CHECK(row->place.x == 5);
        CHECK(row->place.y == subs(20));
        CHECK(row->width.amount == subs(9));
        CHECK(row->height.amount == subs(9) + 7 * surface::kPixelGrainSubs);
        t.release(0, 0);
    }
    {
        CAPTURE("top-right: vertical blocked at the one-cell minimum");
        FineRig t;
        REQUIRE(author_pane_place(live(t).setup.active, builder, subs(6), subs(20))
                    .accepted);
        REQUIRE(author_pane_size(live(t).setup.active, builder,
                                 PaneSize{pane_unit::kSubcells, subs(9)},
                                 PaneSize{pane_unit::kSubcells, subs(9)})
                    .accepted);
        const FineRect base = t.builder_rect();
        const FineRect mark = pane_edge_cell(base, pane_edge::kTopRight);
        const std::int64_t px = surface::px_of_subs(mark.x) + 2;
        const std::int64_t py = surface::px_of_subs(mark.y) + 2;
        t.press_at(px, py, input::space::kPixels);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kTopRight);
        // 102 DOWN: h' = 9 cells - 408 subs is below the floor, so y + height HOLD.
        // EIGHT RIGHT: the width grows legally on its own axis.
        t.motion_at(px + 8, py + 102, input::space::kPixels);
        const SetupPane* row = t.builder_row();
        REQUIRE(row != nullptr);
        CHECK(row->place.x == subs(6));
        CHECK(row->place.y == subs(20));
        CHECK(row->height.amount == subs(9));
        CHECK(row->width.amount == subs(9) + 8 * surface::kPixelGrainSubs);
        t.release(0, 0);
    }
    {
        CAPTURE("bottom-right: vertical blocked at the one-cell minimum");
        FineRig t;
        REQUIRE(author_pane_place(live(t).setup.active, builder, subs(6), subs(20))
                    .accepted);
        REQUIRE(author_pane_size(live(t).setup.active, builder,
                                 PaneSize{pane_unit::kSubcells, subs(9)},
                                 PaneSize{pane_unit::kSubcells, subs(9)})
                    .accepted);
        const FineRect base = t.builder_rect();
        const FineRect mark = pane_edge_cell(base, pane_edge::kBottomRight);
        const std::int64_t px = surface::px_of_subs(mark.x) + 2;
        const std::int64_t py = surface::px_of_subs(mark.y) + 2;
        t.press_at(px, py, input::space::kPixels);
        REQUIRE(t.session().pane_drag.edge == pane_edge::kBottomRight);
        // 102 UP: h' is below the floor, height holds. EIGHT LEFT: the width shrinks
        // legally, place untouched on both axes (trailing edges write no place).
        t.motion_at(px - 8, py - 102, input::space::kPixels);
        const SetupPane* row = t.builder_row();
        REQUIRE(row != nullptr);
        CHECK(row->place.x == subs(6));
        CHECK(row->place.y == subs(20));
        CHECK(row->height.amount == subs(9));
        CHECK(row->width.amount == subs(9) - 8 * surface::kPixelGrainSubs);
        t.release(0, 0);
    }
}

TEST_CASE("WUX-2: the hand meets exactly the pixels a fine pane paints") {
    // SC-6'S IDENTITY, at a fractional edge: a pane whose left edge falls mid-pixel is
    // painted from the pixel that edge floors to, and the FIRST painted pixel answers the
    // hand while the pixel before it does not — the aligned-span law, measured through the
    // real routing path.
    FineRig t;
    // x = 6 cells + 13 subs = pixel 75.25: the painted left edge is pixel 75.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), subs(6) + 13,
                              subs(6))
                .accepted);
    const FineRect at = t.builder_rect();
    REQUIRE(at.x == subs(6) + 13);
    const std::int64_t left_px = surface::px_of_subs(at.x);
    CHECK(left_px == 75);
    const std::int64_t mid_y = surface::px_of_subs(at.y) + 30;

    const Screen sc = screen_of(t.session());
    const Occupancy on = occupied_at(
        t.session().panels, t.session().setup.active, sc,
        canvas_point_of(input::space::kPixels, left_px, mid_y));
    CHECK(on.occupied);
    CHECK(on.kind == stock::kKind);
    const Occupancy off = occupied_at(
        t.session().panels, t.session().setup.active, sc,
        canvas_point_of(input::space::kPixels, left_px - 1, mid_y));
    CHECK_FALSE(off.occupied);

    // AND THE RIGHT EDGE, half-open exactly as painted: the last painted pixel answers,
    // the pixel past it does not.
    const std::int64_t right_px = surface::px_of_subs(surface::add_cells(at.x, at.w));
    const Occupancy inside = occupied_at(
        t.session().panels, t.session().setup.active, sc,
        canvas_point_of(input::space::kPixels, right_px - 1, mid_y));
    CHECK(inside.occupied);
    const Occupancy past = occupied_at(
        t.session().panels, t.session().setup.active, sc,
        canvas_point_of(input::space::kPixels, right_px, mid_y));
    CHECK_FALSE(past.occupied);
}

TEST_CASE("WUX-2: the TUI projects a fine pane onto its covered cells and rewrites nothing") {
    // THE CELL MEDIUM'S HONEST PICTURE of a finely-placed pane: the cells its floored
    // edges span, deterministically — and projecting it changes not one authored byte,
    // however many frames are drawn.
    Live t; // a character medium: no text metric
    open_pane(t, ref_of(stock::kKind));
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), subs(6) + 24,
                              subs(6) + 40)
                .accepted);
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(stock::kKind),
                             PaneSize{pane_unit::kSubcells, subs(20) + 30},
                             PaneSize{pane_unit::kSubcells, subs(5) + 20})
                .accepted);
    const std::string authored_before = setup_persist::to_text(t.session().setup.active);

    const Screen sc = screen_of(t.session());
    const FineRect fine = bounds_of(t.session().panels, t.session().setup.active,
                                    stock::kKind, sc)
                              .rect;
    const ui::Rect covered = cells_covered(fine);
    // floor(6.5) = 6; floor(6.5 + 20.625) = 27 -> 21 covered columns. floor(6+40/48) = 6;
    // floor(y + h) = floor(6.833 + 5.416 = 12.25) = 12 -> 6 covered rows.
    CHECK(covered == ui::Rect{6, 6, 21, 6});

    // THE PAINTED BYTES SIT ON EXACTLY THOSE CELLS: the pane's backdrop begins at the
    // covered corner in the cell rasterization, and the column before it is not the
    // pane's.
    const surface::SurfaceCanvas c = paint(t.doc(), t.session());
    // ...and the pane's own prose begins one cell inside that corner, which is where its
    // visible boundary now is (WUX-5). The corner cell itself is the boundary's.
    CHECK_FALSE(
        label_at(c, covered.x + kChromeCells, covered.y + kChromeCells).empty());
    const Occupancy at_corner = occupied_at(t.session().panels, t.session().setup.active,
                                            sc, covered.x, covered.y);
    CHECK(at_corner.occupied);
    CHECK(at_corner.kind == stock::kKind);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc,
                            covered.x - 1, covered.y)
                    .occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc,
                            covered.x + covered.w, covered.y)
                    .occupied);

    // PROJECTION IS PURE: paint it again — twice more, through the same one function —
    // and the authored bytes and the picture are identical. The TUI quantizes at ITS
    // boundary and writes nothing back.
    const surface::SurfaceCanvas again = paint(t.doc(), t.session());
    CHECK(surface::canvas_body(c) == surface::canvas_body(again));
    CHECK(setup_persist::to_text(t.session().setup.active) == authored_before);

    // AND EXACT-CELL VALUES STAY EXACT: reauthor on the boundary and the covered cells
    // are the authored cells, byte for byte the pre-WUX-2 picture of the same desk.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), subs(6),
                              subs(6))
                .accepted);
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(stock::kKind),
                             PaneSize{pane_unit::kSubcells, subs(20)},
                             PaneSize{pane_unit::kSubcells, subs(5)})
                .accepted);
    const FineRect exact = bounds_of(t.session().panels, t.session().setup.active,
                                     stock::kKind, screen_of(t.session()))
                               .rect;
    CHECK(cells_covered(exact) == ui::Rect{6, 6, 20, 5});
}

TEST_CASE("WUX-2: a version-2 whole-cell setup loads at exactly its old picture") {
    // THE LEGACY ROAD (SC-8): WIND-2's own shapes write a version-2 file — the retained
    // `v2` namespace IS those shapes, so this text is byte-honest — and this build reads
    // it, scales it exactly onto the fine lattice, and resolves it to the rectangle the
    // old build resolved.
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
    // CELLS BECAME SUB-UNITS, EXACTLY — the same place on the lattice, said finer.
    CHECK(got.place.mode == pane_unit::kSubcells);
    CHECK(got.place.x == subs(6));
    CHECK(got.place.y == subs(5));
    CHECK(got.width.mode == pane_unit::kSubcells);
    CHECK(got.width.amount == subs(40));
    // A PIXEL AXIS IS DEVICE PIXELS IN BOTH VERSIONS AND CROSSES UNSCALED.
    CHECK(got.height.mode == pane_unit::kPixels);
    CHECK(got.height.amount == 220);

    // AND THE NEXT SAVE WRITES VERSION 3, WHICH ROUND-TRIPS BYTE-IDENTICALLY.
    const std::string saved = setup_persist::to_text(read.setup);
    CHECK(saved.find("\"format_version\":\"3\"") != std::string::npos);
    CHECK(saved.find("\"mode\":\"subcells\"") != std::string::npos);
    const setup_persist::LoadedSetup back = setup_persist::from_text(saved);
    REQUIRE(back.outcome.accepted);
    CHECK(back.setup == read.setup);
    CHECK(setup_persist::to_text(back.setup) == saved);

    // A VERSION-2 WORD THE OLD FORMAT NEVER HAD IS STILL REFUSED IN THE OLD VOCABULARY'S
    // OWN TERMS — the legacy gate is version 2's, full strength.
    std::string forged = bytes;
    const std::size_t at = forged.find("\"mode\":\"cells\"");
    REQUIRE(at != std::string::npos);
    forged.replace(at, std::string("\"mode\":\"cells\"").size(), "\"mode\":\"barns\"");
    const setup_persist::LoadedSetup refused = setup_persist::from_text(forged);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("barns") != std::string::npos);
    CHECK(refused.outcome.refusal.find("default or cells") != std::string::npos);
}

TEST_CASE("WUX-2/MIG-0: a version-1 session restores a whole-cell desk through a conversion") {
    // THE SAME CLAIM WUX-2 SHIPPED, THROUGH THE SEAM THAT NOW CARRIES IT (MIG-0). The old
    // shape and the multiply are `session_history`'s -- an artifact's material -- and the
    // reader has stopped compiling either; what it does is ask the catalog. So the case
    // mounts the conversions and reads the same file.
    session_history::v1::WorkshopSession old;
    old.format = session_persist::kFormat;
    old.format_version = 1;
    old.viewport = session_persist::WorkshopViewport{120, 44};
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
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.present);
    CHECK(read.honoured);
    CHECK(read.viewport_w == 120);
    CHECK(read.viewport_h == 44);
    CHECK(live_layout(read).name == "Yesterday");
    // TWO ROWS: the one the version-1 file authored, whose whole-cell geometry became
    // sub-cells, and the Layouts pane its vintage had implicitly (WUX-12).
    REQUIRE(live_layout(read).panes.size() == 2);
    CHECK(live_layout(read).panes[0].width.mode == pane_unit::kSubcells);
    CHECK(live_layout(read).panes[0].width.amount == subs(28));
    CHECK(live_layout(read).panes[1].ref == PaneRef{"zengine.workshop", "layouts"});

    // A LEGACY ROAD CARRIES NO PLACEMENT: nothing in a v1 file could have said one.
    CHECK_FALSE(read.placement.known);
    // ...AND NO LAYOUT PLURALITY EITHER (WUX-10): one layout, live at position zero.
    CHECK(read.layouts.size() == 1);
    CHECK(read.active == 0);

    // AND THE NEXT CLOSE WRITES THE CURRENT VERSION, byte-stable thereafter. The ENVELOPE's
    // number is what is asserted: since WUX-10 the nested desk carries a `format_version` of
    // its OWN at a different number, and a search for the bare field would find that one.
    const std::string saved = session_persist::to_text(read.layouts, read.active, read.viewport_w,
                                                       read.viewport_h, read.placement);
    CHECK(saved.find("\"version\":6") != std::string::npos);
    CHECK(saved.find("\"format\":\"zengine-workshop-session\",\"format_version\":\"6\"") !=
          std::string::npos);
    const session_persist::LoadedSession back = session_persist::from_text(saved);
    REQUIRE(back.outcome.accepted);
    CHECK(live_layout(back) == live_layout(read));
    CHECK(session_persist::to_text(back.layouts, back.active, back.viewport_w, back.viewport_h,
                                   back.placement) == saved);
}

TEST_CASE("WUX-2: fine geometry survives the setup file without losing a sub-unit") {
    // EVERY REMAINDER VALUE IS A DIFFERENT AUTHORED FACT, and the file must keep each: a
    // sweep across the lattice's finest steps, through save -> load -> save.
    Setup s = two_overlays();
    const PaneRef builder = ref_of(stock::kKind);
    for (const std::int64_t rem : {1, 7, 24, 47}) {
        CAPTURE(rem);
        REQUIRE(author_pane_place(s, builder, subs(3) + rem, subs(9) + (47 - rem)).accepted);
        REQUIRE(author_pane_size(s, builder, PaneSize{pane_unit::kSubcells, subs(30) + rem},
                                 PaneSize{pane_unit::kSubcells, subs(7) + rem})
                    .accepted);
        const std::string a = setup_persist::to_text(s);
        const setup_persist::LoadedSetup read = setup_persist::from_text(a);
        REQUIRE(read.outcome.accepted);
        CHECK(read.setup == s);
        CHECK(setup_persist::to_text(read.setup) == a);
        const SetupPane* row = pane_of(read.setup, builder);
        REQUIRE(row != nullptr);
        CHECK(row->place.x == subs(3) + rem);
        CHECK(row->width.amount == subs(30) + rem);
    }
}

TEST_CASE("WUX-6: one authored value, spelled in whatever unit the active face reported") {
    // WUX-2's SC-9 said the row must not present a ROUNDED value as the stored one, and
    // spent an exact mixed number (`10+1/2`) to keep that true. WUX-6 keeps the law and
    // changes the spelling: a maker reads the unit the face in front of them can actually
    // distinguish, and a value that face cannot say exactly is MARKED as the projection it
    // is. `10+1/2` was exact and unreadable on a window; `126` is exact there, and `~10` is
    // honest in a terminal.
    const std::int64_t px = surface::kCanvasCellPx; // what the shipped face REPORTS, not a
    const std::int64_t cells = 0;                   // constant Workshop is allowed to hold

    // THE UNIT WORD IS THE MEDIUM'S OWN ANSWER, and zero is a character medium's.
    CHECK(std::string(geometry_unit(px)) == "px");
    CHECK(std::string(geometry_unit(cells)) == "cells");
    CHECK(std::string(geometry_unit(0)) == "cells");

    // A WHOLE-CELL VALUE IS EXACT ON EVERY MEDIUM — it is the authored number in both
    // units, so neither spelling is a projection.
    CHECK(geometry_spelling(subs(40), cells).amount == "40");
    CHECK(geometry_spelling(subs(40), cells).exact);
    CHECK(geometry_spelling(subs(40), px).amount == "480");
    CHECK(geometry_spelling(subs(40), px).exact);
    CHECK(geometry_spelling(0, px).amount == "0");
    CHECK(geometry_spelling(0, px).exact);

    // A VALUE A HAND AUTHORED AT THE WINDOW'S PIXEL GRAIN IS EXACT IN PIXELS AND IS NOT
    // EXACT IN CELLS. This is the whole of the phase's distinction, in four lines: half a
    // cell is 126 window pixels exactly, and no number of cells at all.
    CHECK(geometry_spelling(subs(10) + 24, px).amount == "126");
    CHECK(geometry_spelling(subs(10) + 24, px).exact);
    CHECK(geometry_spelling(subs(10) + 24, cells).amount == "10");
    CHECK_FALSE(geometry_spelling(subs(10) + 24, cells).exact);

    // AND A VALUE FINER THAN THE SHIPPED FACE'S OWN PIXEL IS A PROJECTION THERE TOO. One
    // sub-unit is a quarter of a window pixel (48 = 4 x 12), so the lattice can hold three
    // values per pixel that no medium in this build can say.
    CHECK(geometry_spelling(subs(10) + 1, px).amount == "120");
    CHECK_FALSE(geometry_spelling(subs(10) + 1, px).exact);
    CHECK(geometry_spelling(subs(10) + 4, px).exact); // a whole pixel: exactly 121
    CHECK(geometry_spelling(subs(10) + 4, px).amount == "121");

    // THE MARK IS ASCII, because the shipped face's letterform is.
    CHECK(std::string(kProjectedMark) == "~");
    CHECK(std::string(kProjectedMark).size() == 1);

    SetupPane row;
    row.place = PanePlace{pane_unit::kSubcells, subs(6) + 24, subs(5)};
    row.width = PaneSize{pane_unit::kSubcells, subs(40) + 12};
    row.height = PaneSize{pane_unit::kPixels, 220};
    row.front = 2;
    // ON THE SHIPPED FACE every lattice number lands on a whole window pixel, so nothing
    // is marked and no clause is owed. The `220px` is the maker's own authored
    // device-pixel claim, said in the unit they wrote it in whatever the medium is.
    CHECK(pane_window_text(&row, px) == "@78,60 483x220px px f2");
    // IN A TERMINAL the same authored value is a projection on two axes, and says so.
    CHECK(pane_window_text(&row, cells) == "@~6,5 ~40x220px cells f2 (~ projected)");

    // AND THE WHOLE-CELL CASE READS AS THE PLAIN COUNT IT ALWAYS DID, in either face.
    SetupPane plain;
    plain.place = PanePlace{pane_unit::kSubcells, subs(6), subs(5)};
    plain.width = PaneSize{pane_unit::kSubcells, subs(40)};
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
    CHECK(fine_rect_text(FineRect{subs(3), subs(4), subs(20), subs(6)}, cells) ==
          "@3,4 20x6 cells");
    CHECK(fine_rect_text(FineRect{subs(3), subs(4), subs(20), subs(6)}, px) ==
          "@36,48 240x72 px");
    CHECK(fine_rect_text(FineRect{subs(3) + 24, subs(4) + 24, subs(20), subs(6)}, cells) ==
          "@~3,~4 20x6 cells (~ projected)");
}

TEST_CASE("WUX-6: the canvas's device unit is the medium's answer, never Workshop's") {
    // `surface/pointing.hpp` forbids an application to HOLD one Skin's layout number: it
    // is correct only for as long as there is one medium. So this number is only ever the
    // one a medium reported, it arrives on the same message the room and the face metric
    // do, and a run nobody has spoken to reads as the character medium it has always been.
    Session s;
    CHECK(s.cell_px == 0); // a fresh Workshop claims no device pixel

    REQUIRE(adopt_screen(s, 100, 33));
    CHECK(s.cell_px == 0); // ...and a medium that named no unit still claims none

    // A MEDIUM THAT NAMES ONE IS BELIEVED. It is the only party that can know.
    REQUIRE(adopt_screen(s, 100, 33, 8, 18, surface::kCanvasCellPx));
    CHECK(s.cell_px == surface::kCanvasCellPx);
    CHECK(std::string(geometry_unit(s.cell_px)) == "px");

    // A CHANGE OF UNIT ALONE IS A CHANGE -- a window that opens its canvas after its
    // first frame must not leave a maker reading the wrong unit until something else
    // happens to move. (`report_extent` guards the same fact on the medium's side.)
    CHECK_FALSE(adopt_screen(s, 100, 33, 8, 18, surface::kCanvasCellPx));
    CHECK(adopt_screen(s, 100, 33, 8, 18, 0));
    CHECK(s.cell_px == 0);

    // NON-POSITIVE IS THE VOCABULARY'S "MY DEVICE UNIT IS THE CELL", and a number nobody
    // could mean resolves to the reading that changes nothing rather than to a guess.
    REQUIRE(adopt_screen(s, 100, 33, 8, 18, surface::kCanvasCellPx));
    REQUIRE(s.cell_px == surface::kCanvasCellPx);
    CHECK(adopt_screen(s, 100, 33, 8, 18, -12));
    CHECK(s.cell_px == 0);

    // THE FACE METRIC AND THE CANVAS UNIT ARE DIFFERENT FACTS. A window whose font failed
    // to open sets no type and still lays its canvas out in pixels; reading the metric as
    // "am I graphical" is the near-miss agents/surface.md names.
    REQUIRE(adopt_screen(s, 100, 33, 0, 0, surface::kCanvasCellPx));
    CHECK(s.text_advance_px == 0);
    CHECK(s.cell_px == surface::kCanvasCellPx);
    CHECK(std::string(geometry_unit(s.cell_px)) == "px");
}

TEST_CASE("WUX-6: the medium's unit reaches the READOUT and no geometry at all") {
    // THE FALSIFIER FOR "a device unit leaked into the lattice". Two sessions, identical
    // in every way except which medium spoke to them, holding the same authored desk:
    // every rectangle, every hit test and the whole composition must be indistinguishable.
    // Only the SENTENCE differs.
    Session cells;
    admit_stock(cells.panels); // the stand-in, first (stock)
    Session window;
    admit_stock(window.panels); // the stand-in, first (stock)
    cells.setup.active = two_overlays();
    window.setup.active = two_overlays();

    // THE DESK IS AUTHORED FIRST, and then each medium speaks -- deliberately that order.
    // A medium that rewrote a maker's geometry on its way in would do it to a desk that
    // already existed, which is exactly the shape a restore or a second face has.
    const PaneRef builder = ref_of(stock::kKind);
    REQUIRE(author_pane_place(cells.setup.active, builder, subs(3) + 12, subs(4) + 12)
                .accepted);
    REQUIRE(author_pane_place(window.setup.active, builder, subs(3) + 12, subs(4) + 12)
                .accepted);
    REQUIRE(author_pane_size(cells.setup.active, builder,
                             PaneSize{pane_unit::kSubcells, subs(30) + 12},
                             PaneSize{pane_unit::kSubcells, subs(7) + 12})
                .accepted);
    REQUIRE(author_pane_size(window.setup.active, builder,
                             PaneSize{pane_unit::kSubcells, subs(30) + 12},
                             PaneSize{pane_unit::kSubcells, subs(7) + 12})
                .accepted);

    REQUIRE(adopt_screen(cells, 140, 40, 0, 0, 0));
    REQUIRE(adopt_screen(window, 140, 40, 0, 0, surface::kCanvasCellPx));

    // NOT ONE AUTHORED NUMBER MOVED BECAUSE A MEDIUM SPOKE.
    CHECK(*pane_of(cells.setup.active, builder) == *pane_of(window.setup.active, builder));
    CHECK(pane_of(window.setup.active, builder)->width.amount == subs(30) + 12);

    const Screen sc_cells = screen_of(cells);
    const Screen sc_window = screen_of(window);
    CHECK(sc_cells.w == sc_window.w);
    CHECK(sc_cells.room_w == sc_window.room_w);

    const PanelBounds a = bounds_of(cells.panels, cells.setup.active, stock::kKind, sc_cells);
    const PanelBounds b =
        bounds_of(window.panels, window.setup.active, stock::kKind, sc_window);
    CHECK(a.rect == b.rect);
    CHECK(a.resolved == b.resolved);

    // AND THE HAND MEETS THE SAME CELLS. `occupied_at`'s answer is the pane's own
    // geometry; a device unit that had entered it would move the boundary here.
    for (std::int64_t y = 0; y < 12; ++y) {
        for (std::int64_t x = 0; x < 40; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            REQUIRE(occupied_at(cells.panels, cells.setup.active, sc_cells, x, y).occupied ==
                    occupied_at(window.panels, window.setup.active, sc_window, x, y).occupied);
        }
    }

    // WHAT DIFFERS IS THE SENTENCE, AND ONLY THE SENTENCE.
    const SetupPane* row = pane_of(cells.setup.active, builder);
    REQUIRE(row != nullptr);
    CHECK(pane_window_text(row, cells.cell_px) != pane_window_text(row, window.cell_px));
    CHECK(pane_window_text(row, window.cell_px).find(" px ") != std::string::npos);
    CHECK(pane_window_text(row, cells.cell_px).find(" cells ") != std::string::npos);
}

TEST_CASE("WUX-6: which parts of a pane's window the maker has not authored") {
    // The question the arrangement readout asks before it adds where the pane actually is.
    SetupPane none;
    CHECK(pane_window_partly_default(&none));

    SetupPane placed;
    placed.place = PanePlace{pane_unit::kSubcells, subs(2), subs(2)};
    CHECK(pane_window_partly_default(&placed)); // both extents still the code's

    SetupPane whole;
    whole.place = PanePlace{pane_unit::kSubcells, subs(2), subs(2)};
    whole.width = PaneSize{pane_unit::kSubcells, subs(20)};
    whole.height = PaneSize{pane_unit::kSubcells, subs(6)};
    CHECK_FALSE(pane_window_partly_default(&whole));

    CHECK_FALSE(pane_window_partly_default(nullptr));
}

// ============================================================================
// CTX-0 — the contextual surface's picture is the surface a press meets
// ============================================================================

TEST_CASE("CTX-0: the contextual surface is painted where it is hit") {
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);

    // The published region, at `context_bounds` exactly: the FIRST ROW IS AN ACTION since
    // WUX-5 -- no heading naming the subject, no row restating the two gestures the band's
    // legend is already saying -- and the rows are the declared population with the
    // cursor's own mark, labels from the one action truth, groups saying they descend.
    const std::vector<std::string> rows = context_rows_on(t.canvases.back(), t.session());
    REQUIRE(rows.size() == 4);
    for (const std::string& row : rows) {
        CHECK(row.find("ACTIONS") == std::string::npos);
        CHECK(row.find("chooses") == std::string::npos);
    }
    CHECK(rows[0] == "> arrange");
    CHECK(rows[1] == "  Order >");
    CHECK(rows[2] == "  Reset >");
    CHECK(rows[3] == "  remove");

    // THE INVERSE PAIR, SPENT: a press at the row the painter drew chooses that row.
    // Row 3 is `remove` -- and the pane is gone, through the one door.
    t.press_canvas(context_cell_x(t.session()), context_entry_cell_y(t.session(), 3));
    CHECK_FALSE(t.menu().open);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(stock::kKind)));

    // ...and inside a group the heading's hint says the smaller way out.
    t.right_press(40, 0); // any subject re-opens; the room serves
    REQUIRE(t.menu().open);
}

TEST_CASE("CTX-0: an open group paints its own rows and its own way out") {
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    t.key(input::scan::kDown);
    t.key(input::scan::kReturn); // Order
    REQUIRE(t.menu().group == "Order");
    // A GROUP LEVEL IS ITS ROWS AND NOTHING ELSE (WUX-5). The breadcrumb went with the
    // hint row it lived on, and nothing was lost with it: every one of these labels says
    // what it does on its own, which is what `kActionCatalog` already declared.
    const std::vector<std::string> rows = context_rows_on(t.canvases.back(), t.session());
    REQUIRE(rows.size() == 4);
    CHECK(rows[0] == "> front");
    CHECK(rows[1] == "  back");
    CHECK(rows[2] == "  raise");
    CHECK(rows[3] == "  lower");
}

TEST_CASE("CTX-0: manage.remove speaks through the keymap's own claim surfaces") {
    // The band's legend for an arrangement scope carries the verb the moment the row is
    // declared -- generated, never hand-kept -- and its spelling is the effective
    // binding's.
    const Keymap defaults;
    const std::vector<std::string> pairs = help_pairs(defaults, KeyContext::kArrangePane);
    bool said = false;
    for (const std::string& pair : pairs) {
        if (pair == "d remove") {
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
// ARR-0 — the contextual surface opens beside the hand, sized by what it says
// ============================================================================

TEST_CASE("ARR-0: the popup opens at the press's own cell, and its extent is its content") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));

    // TWO WIDELY SEPARATED PRESSES, TWO LOCAL RECTANGLES -- the falsifier the old
    // spelling failed: one fixed column, wherever the maker clicked.
    t.right_press_canvas(10, 5);
    REQUIRE(t.menu().open);
    const FineRect near_a = context_bounds(t.session(), screen_of(t.session()));
    CHECK(surface::cell_of_subs(near_a.x) == 10);
    CHECK(surface::cell_of_subs(near_a.y) == 5);
    t.right_press_canvas(100, 10);
    const FineRect near_b = context_bounds(t.session(), screen_of(t.session()));
    CHECK(surface::cell_of_subs(near_b.x) == 100);
    CHECK(surface::cell_of_subs(near_b.y) == 10);

    // THE EXTENT IS THE LEVEL'S OWN COMPOSITION: on a cell medium, exactly the population
    // -- never the old floor-to-ceiling column, and since WUX-5 with no heading rows at
    // all, the population inside the surface's own chrome.
    const std::vector<ContextEntry> rows =
        context_population(t.menu().subject, t.menu().group);
    const PanelProsePlace place =
        panel_prose_place(near_b, screen_of(t.session()));
    CHECK(place.rows == static_cast<std::int64_t>(rows.size()));
    const FineRect column = overlay_column(screen_of(t.session()));
    CHECK(near_b.h < column.h);
    CHECK(near_b.w <= surface::subs_of_cells(kContextMaxCols));
    CHECK(near_b.w < column.w);
}

TEST_CASE("ARR-0: the popup shifts to stay usable inside the room, at every boundary") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    // A PANE IN THE FAR CORNER, so the press there captures a PANE subject and the menu's
    // top row is `arrange`. It was Info, which a fresh desk had in exactly this place.
    open_at_right_column(t, panel::kPaneEditor);
    const Screen sc = screen_of(t.session());
    const std::int64_t floor_y = kWorkspaceY + sc.room_h;

    // NEAR THE FAR CORNER: the menu may not fit rightward or downward of the press, so
    // it shifts left and up exactly as far as wholeness requires -- and stays a popup.
    t.right_press_canvas(sc.w - 2, floor_y - 2);
    REQUIRE(t.menu().open);
    const FineRect clamped = context_bounds(t.session(), screen_of(t.session()));
    CHECK(clamped.x + clamped.w <= surface::subs_of_cells(sc.w));
    CHECK(clamped.y + clamped.h <= surface::subs_of_cells(floor_y));
    CHECK(clamped.x >= 0);
    CHECK(clamped.y >= surface::subs_of_cells(kStackY));

    // ...AND THE PAINTED SURFACE IS AT THE SHIFTED PLACE, whole: the press resolver and
    // the painter read one geometry, so a row chosen at the clamped rectangle is the row
    // the maker sees there (the inverse pair, spent at the wall).
    const std::vector<std::string> shown = context_rows_on(t.canvases.back(), t.session());
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0] == "> arrange");

    // NEAR THE ORIGIN, nothing shifts: the anchor is already legal.
    t.right_press_canvas(0, 1);
    const FineRect origin = context_bounds(t.session(), screen_of(t.session()));
    CHECK(surface::cell_of_subs(origin.x) == 0);
    CHECK(surface::cell_of_subs(origin.y) == kStackY);
}

TEST_CASE("ARR-0: the keyboard entrance has no pointer and invents none") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    t.key(input::scan::kA);
    t.text("a");
    REQUIRE(t.menu().open);
    CHECK_FALSE(t.menu().anchored);
    // The deterministic non-pointer placement: the overlay stack's own corner, the home
    // of every keyboard surface on this screen -- and still content-sized.
    const Screen sc = screen_of(t.session());
    const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, sc);
    const FineRect at = context_bounds(t.session(), sc);
    CHECK(surface::cell_of_subs(at.x) == slot.x);
    CHECK(surface::cell_of_subs(at.y) == slot.y);
}

TEST_CASE("ARR-0: entering a group stays at the anchor, and the popup resizes to it") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(t, ref_of(stock::kKind));
    // Author the pane somewhere unmistakably far from the old column, and ask beside it.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), subs(90),
                              subs(20))
                .accepted);
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);
    const FineRect top = context_bounds(t.session(), screen_of(t.session()));
    CHECK(surface::cell_of_subs(top.x) == slot.x + 1);

    // DESCEND: the level's rows change, the anchor does not -- context stays local
    // instead of jumping back to any global column.
    t.key(input::scan::kDown);
    t.key(input::scan::kReturn); // Order
    REQUIRE(t.menu().group == "Order");
    const FineRect inside = context_bounds(t.session(), screen_of(t.session()));
    CHECK(inside.x == top.x);
    CHECK(inside.y == top.y);
    const PanelProsePlace place = panel_prose_place(inside, screen_of(t.session()));
    CHECK(place.rows == 4); // exactly the Order group's rows, and no chrome row

    // AND THE INVERSE PAIR HOLDS AT THE ANCHORED PLACE: pressing the `back` row where
    // the painter drew it performs back on the captured pane.
    t.press_canvas(context_cell_x(t.session()), context_entry_cell_y(t.session(), 1));
    CHECK_FALSE(t.menu().open);
    CHECK(t.notice().find("back-most") != std::string::npos);
}

TEST_CASE("ARR-0: shortcut annotations teach only truthful surrounding bindings") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));

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

    // OVER COMMAND MODE, the room's doors teach their command keys and the globals their
    // chords -- and `manage.reset-order`, whose only row lives in a mode the maker is NOT
    // returning to, teaches nothing (the distant-mode refusal).
    t.right_press(40, 0);
    REQUIRE(t.menu().subject == context_subject::kRoot);
    CHECK(context_annotation(t.session(), entry_of("workshop.picker")) == "p");
    CHECK(context_annotation(t.session(), entry_of("workshop.manage")) == "w");
    CHECK(context_annotation(t.session(), entry_of("workshop.hotkeys")) == "^k");
    CHECK(context_annotation(t.session(), entry_of("manage.reset-order")).empty());

    // ...AND THE PAINTED ROW CARRIES THE GESTURE AT THE LEVEL'S ANNOTATION COLUMN,
    // visually subordinate by position, the label leading.
    {
        const std::vector<std::string> shown =
            context_rows_on(t.canvases.back(), t.session());
        bool painted = false;
        for (const std::string& row : shown) {
            if (row.find("+ panel") != std::string::npos) {
                painted = row.size() > 2 && row.back() == 'p' &&
                          row.find("+ panel") < row.rfind('p');
            }
        }
        CHECK(painted);
    }

    t.key(input::scan::kEscape); // close the room menu before walking the picker

    // A PANE ROW NEVER ANNOTATES: its actions live in the arrangement scopes, which are
    // not the interaction the maker returns to when this surface closes.
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);
    CHECK(context_annotation(t.session(), entry_of("manage.arrange")).empty());
    CHECK(context_annotation(t.session(), entry_of("manage.remove")).empty());

    // AND A MODE BENEATH THAT SWALLOWS BARE KEYS SUPPRESSES THE COMMAND ANNOTATIONS
    // WHILE THE GLOBALS SURVIVE: over the open picker, `n` would not create -- so the
    // menu does not teach it -- while `^t` still works everywhere.
    t.key(input::scan::kEscape); // close the pane menu
    t.key(input::scan::kP);
    t.text("p");
    REQUIRE(t.session().panels.picker.open);
    t.right_press(40, 15);
    REQUIRE(t.menu().open);
    CHECK(context_annotation(t.session(), entry_of("object.new")).empty());
    CHECK(context_annotation(t.session(), entry_of("workshop.hotkeys")) == "^k");
    t.key(input::scan::kEscape); // the menu
    t.key(input::scan::kEscape); // the picker
    REQUIRE_FALSE(t.session().panels.picker.open);

    // A REMAP MOVES THE ANNOTATION WITH NOTHING HERE TO UPDATE: the spelling is the
    // effective keymap's, so one truth moves every claim.
    REQUIRE(apply_overrides({{"workshop.picker", "y"}}, legend_mode::kDefault,
                            live(t).keymap)
                .accepted);
    t.right_press(40, 15);
    REQUIRE(t.menu().subject == context_subject::kRoot);
    CHECK(context_annotation(t.session(), entry_of("workshop.picker")) == "y");
}

TEST_CASE("ARR-0: object.delete teaches its key exactly when the subject IS the selection") {
    Live t;
    REQUIRE(t.session().selected == 1);

    const auto delete_entry = [&]() {
        const std::vector<ContextEntry> rows =
            context_population(t.menu().subject, t.menu().group);
        REQUIRE(rows.size() == 1);
        return rows[0];
    };

    // The selected object: the key and the row perform the same act, so it is taught.
    // (The keyboard entrance names exactly the selection, so the equivalence holds by
    // construction on this route.)
    t.key(input::scan::kA);
    t.text("a");
    REQUIRE(t.menu().subject == context_subject::kObject);
    REQUIRE(t.menu().object == 1);
    CHECK(context_annotation(t.session(), delete_entry()) == "d");
    t.key(input::scan::kEscape);

    // A pointed-but-unselected object: the key would delete a DIFFERENT object than the
    // row it sits beside, so nothing is advertised.
    t.right_press(7, 11); // #2's body
    REQUIRE(t.menu().object == 2);
    REQUIRE(t.session().selected == 1);
    CHECK(context_annotation(t.session(), delete_entry()).empty());
}

// ============================================================================
// WUX-5 — the desk as a set of tools: boundaries, selection, and the help beside it
// ============================================================================
//
// Every case below is a FALSIFIER for one sentence of the phase, and each names the
// mutation it would catch. They are together rather than spread across the suites because
// they are all about ONE claim: what a maker sees on the desk and what their hand reaches
// are the same thing, and every pane's edge is where the pane says it is.

TEST_CASE("WUX-5: a pane's interior is its outer rectangle less one cell of chrome") {
    // ⚔ MUTATION: delete the border -- `pane_interior` answering its own argument. Every
    // number below moves, in both directions, so the picture and the room move together.
    const FineRect outer = fine_of_cells(ui::Rect{4, 3, 20, 9});
    CHECK(cells_covered(pane_interior(outer, kChromeSubs)) == ui::Rect{5, 4, 18, 7});

    // ...AND THE INVERSE IS EXACT, which is what a content-sized surface asks for.
    const ui::Rect grown = chrome_outer_of(0, 0, 18, 7);
    CHECK(grown.w == 20);
    CHECK(grown.h == 9);
    CHECK(cells_covered(pane_interior(fine_of_cells(grown), kChromeSubs)) == ui::Rect{1, 1, 18, 7});

    // A PANE TOO SMALL FOR ITS OWN CHROME HAS NO INTERIOR, and says so with an empty
    // rectangle rather than a negative one -- ⚔ MUTATION: dropping the guard, which would
    // hand every `w <= 0` consumer a wrapped-around width instead of "nowhere".
    for (const ui::Rect tiny : {ui::Rect{0, 0, 2, 9}, ui::Rect{0, 0, 20, 2},
                                ui::Rect{0, 0, 1, 1}, ui::Rect{0, 0, 0, 0}}) {
        CAPTURE(tiny.w);
        CAPTURE(tiny.h);
        const FineRect none = pane_interior(fine_of_cells(tiny), kChromeSubs);
        CHECK(none.empty());
        CHECK(none.w <= 0);
        CHECK(none.h <= 0);
    }
}

TEST_CASE("WUX-5: the border a maker sees and the room a pane spends are one subtraction") {
    // ⚔ MUTATION: insetting the PAINTER and not the press inverse, or not the room grant.
    // The three resolutions below are the only three, and they answer one rectangle.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const FineRect outer =
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect;
    REQUIRE_FALSE(outer.empty());

    // WHAT IS PAINTED: one rect at the OUTER bounds, and the region inside it.
    const surface::SurfaceCanvas& c = t.canvases.back();
    const ui::Rect cells = cells_covered(outer);
    CHECK(has_rect(c, cells.x, cells.y, cells.w, cells.h, kPaneChrome));
    const ui::Rect inside = pane_body_cells(outer);
    const std::vector<surface::SurfaceTextRegion> at = regions_at(c, inside.x, inside.y);
    REQUIRE(at.size() == 1);
    CHECK(at.front().w == inside.w);
    CHECK(at.front().h == inside.h);

    // WHAT THE ROOM IS: the same interior, resolved by the same call.
    const ExternalBodyPlace body = external_body_place(outer, sc, kExternalHeaderRows);
    CHECK(body.region_x == inside.x);
    CHECK(body.region_y == inside.y);
    CHECK(body.region_w == inside.w);
    CHECK(body.region_h == inside.h);

    // WHAT THE HAND MEETS: the OUTER rectangle, unchanged -- the boundary is INSIDE the
    // pane, so the corner cell is still the pane's and the cell past it is not. Said as "not
    // this pane's" rather than "not anybody's": the stack's slot ends inside the right
    // column's place now, so what is one cell past this pane is the pane standing there.
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, cells.x, cells.y)
              .kind == stock::kKind);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, cells.x + cells.w - 1,
                      cells.y + cells.h - 1)
              .kind == stock::kKind);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, cells.x + cells.w,
                      cells.y)
              .kind != stock::kKind);
}

TEST_CASE("WUX-5: selecting a pane lifts it, in the picture and under the hand at once") {
    // THE OVERLAP THE PHASE IS ABOUT, authored directly so the case is about the LIFT
    // rather than about the gestures that made the desk.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    // THE PANE IN THE RIGHT COLUMN IS AUTHORED. It was Info, a built-in the catalog put
    // there; the place is unchanged and its occupant is a desk row now.
    open_at_right_column(t, panel::kPaneEditor);
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const ui::Rect side = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc).rect);
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind),
                              surface::subs_of_cells(side.x - 4),
                              surface::subs_of_cells(side.y + 2))
                .accepted);
    REQUIRE(send_to_back(live(t).setup.active, ref_of(stock::kKind)));
    const std::int64_t at_x = side.x + 1;
    const std::int64_t at_y = side.y + 3;

    // AUTHORED: Info is in front, so it answers -- and with nothing selected the two
    // orders are the same list.
    const std::vector<std::int64_t> authored = authored_order(t.session());
    REQUIRE(authored.size() >= 2);
    // INFO IS AHEAD OF THE BUILDER, which is the relation this case is about -- said as a
    // relation rather than as "Info is last", because since WUX-12 a fresh desk's front-most
    // row is the Layouts pane and it is nowhere near the cells this case overlaps.
    CHECK(index_of(authored, panel::kPaneEditor) > index_of(authored, stock::kKind));
    CHECK(painted_order(t.session()) == authored);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, at_x, at_y).kind ==
          panel::kPaneEditor);

    // SELECTED: press the Builder where only the Builder is, and it comes forward.
    // ⚔ MUTATION: `paint_panels` or `occupied_at` walking `presentation_order` again.
    t.press_canvas(side.x - 3, side.y + 3);
    REQUIRE(t.session().panels.selected == stock::kKind);
    const std::vector<std::int64_t> lifted = painted_order(t.session());
    CHECK(lifted.back() == stock::kKind);
    CHECK(lifted != authored);
    CHECK(authored_order(t.session()) == authored); // ...and the AUTHORED order did not move
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, at_x, at_y).kind ==
          stock::kKind);

    // AND THE PAINT AGREES WITH IT: the plane carrying the Builder's backdrop is after the
    // one carrying Info's. ⚔ MUTATION: lifting the hit order and not the paint order,
    // which is the exact defect "what I see in front is what my pointer reaches" forbids.
    const ui::Rect builder_cells =
        cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                                screen_of(t.session()))
                          .rect);
    const surface::SurfaceCanvas& painted = t.canvases.back();
    std::int64_t last_info = -1;
    std::int64_t last_builder = -1;
    for (std::size_t li = 0; li < painted.layers.size(); ++li) {
        for (const surface::SurfaceRect& r : painted.layers[li].rects) {
            if (r.x == side.x && r.y == side.y && r.w == side.w) {
                last_info = static_cast<std::int64_t>(li);
            }
            if (r.x == builder_cells.x && r.y == builder_cells.y && r.w == builder_cells.w) {
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
    REQUIRE(t.session().panels.selected == panel::kPaneEditor);
    CHECK(painted_order(t.session()).back() == panel::kPaneEditor);
    CHECK(authored_order(t.session()) == authored);
}

TEST_CASE("WUX-5: the selected pane wears its own chrome, and only it") {
    // (The second pane is authored into the right column, where Info stood.)
    // ⚔ MUTATION: `paint_panels` handing every pane `kPaneChrome`, which collapses the
    // selected style into the ordinary one -- the picture would still be bordered and a
    // maker would have no way to tell which pane they are working with.
    Live t;
    open_at_right_column(t, panel::kPaneEditor);
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const auto chrome_of = [&](std::int64_t kind) {
        const ui::Rect at =
            cells_covered(bounds_of(t.session().panels, t.session().setup.active, kind,
                                    screen_of(t.session()))
                              .rect);
        for (const surface::SurfaceRect& r : all_rects(t.canvases.back())) {
            if (r.x == at.x && r.y == at.y && r.w == at.w && r.h == at.h) {
                return r.role;
            }
        }
        return surface::role::kNone;
    };
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);

    CHECK(chrome_of(stock::kKind) == kPaneChrome);
    CHECK(chrome_of(panel::kPaneEditor) == kPaneChrome);

    t.press_canvas(builder.x, builder.y);
    REQUIRE(t.session().panels.selected == stock::kKind);
    CHECK(chrome_of(stock::kKind) == kPaneChromeSelected);
    CHECK(chrome_of(panel::kPaneEditor) == kPaneChrome);
    CHECK(kPaneChromeSelected != kPaneChrome); // the two roles are two roles

    // A PRESS ON NOBODY'S PANE CLEARS IT, by the same one line that set it.
    t.press_canvas(kWorkspaceX + 1, kWorkspaceY + t.session().workspace_h - 1);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(chrome_of(stock::kKind) == kPaneChrome);
}

TEST_CASE("WUX-5: the selection lift never reaches the file, and no session starts with one") {
    // ⚔ MUTATION: writing `front` on selection -- which is what "raise on click" would be,
    // and what this phase's own invariant forbids. The authored BYTES are compared and not
    // only the ranks, so a rewrite that happened to produce the same permutation is still
    // caught if it moved anything else.
    Live t;
    open_at_right_column(t, panel::kPaneEditor);
    open_pane(t, ref_of(stock::kKind));
    const std::string before = setup_persist::to_text(t.session().setup.active);
    const std::vector<std::int64_t> ranks = ranks_of(t.session().setup.active);

    const Screen sc = screen_of(t.session());
    // ⚠ THE PRESS IS ON EACH PANE'S TOP-RIGHT CELL, NOT ITS TOP-LEFT. The stack's slot and
    // the right column overlap at this extent, and Info's top-left is under the Editor: a
    // press there selects the pane in front, which is the wrong pane and not the phase's
    // point. The right-hand cell of the same row is each pane's own at every extent.
    for (const std::int64_t kind : {stock::kKind, panel::kPaneEditor, stock::kKind}) {
        CAPTURE(kind);
        const ui::Rect at = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, kind, sc).rect);
        // Each pane's own far corner: the stack's slot at its left edge, the right column's
        // pane at its right, so neither press can land in the columns the two share -- where
        // the answer would be whichever of them the selection lift had put in front.
        const std::int64_t x = kind == panel::kPaneEditor ? at.x + at.w - 1 : at.x;
        t.press_canvas(x, at.y);
        REQUIRE(t.session().panels.selected == kind);
        CHECK(setup_persist::to_text(t.session().setup.active) == before);
        CHECK(ranks_of(t.session().setup.active) == ranks);
    }

    // A FRESH SESSION SELECTS NOTHING: it is a field of no durable shape.
    Session fresh;
    admit_stock(fresh.panels); // the stand-in, first (stock)
    CHECK(fresh.panels.selected == kNoPaneKind);
    CHECK(selected_pane(fresh.panels) == kNoPaneKind);

    // ...AND A SELECTION THAT STOPPED BEING SEATED LEAVES NO GHOST IN FRONT.
    Panels gone = t.session().panels;
    gone.selected = kFirstRuntimeKind + 99; // a kind nobody ever opened
    CHECK(selected_pane(gone) == kNoPaneKind);
    CHECK(effective_pane_order(t.session().setup.active, gone) ==
          presentation_order(t.session().setup.active, gone));
}

TEST_CASE("WUX-5: contextual help opens at the selected pane, and follows it") {
    // ⚔ MUTATION: `hotkeys_bounds` ignoring the selection and answering the overlay column's
    // corner, which is exactly where this view used to open unconditionally.
    //
    // A SCREEN WHOSE BAND HOLDS THE WHOLE LIST (QR-17). The view is its content's size now,
    // and where the band is shorter than the list the view keeps the band and shifts to its
    // top (the SC-4 case) -- so an anchor is only observable on a screen that has room for
    // the view beneath it.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 70, 0, 0}));
    // THE PANE IN THE RIGHT COLUMN IS AUTHORED. It was Info, a built-in the catalog put
    // there; the place is unchanged and its occupant is a desk row now.
    open_at_right_column(t, panel::kPaneEditor);
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const ui::Rect column = cells_covered(overlay_column(sc));

    // NO SELECTION: the global place -- the overlay column's own CORNER, where this view
    // always opened. Its extent is the content's since QR-17 (the QR-17 cases below), so
    // the corner is the whole of what "byte-for-byte" means here.
    REQUIRE(t.session().panels.selected == kNoPaneKind);
    const ui::Rect global = cells_covered(hotkeys_bounds(t.session(), sc));
    CHECK(global.x == column.x);
    CHECK(global.y == column.y);

    // SELECTED: the pane's own top-left corner, VERBATIM where the room allows it. The
    // Builder is put well inside the surface first, so this half of the case is about the
    // anchor and the clamp gets its own half below.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind),
                              surface::subs_of_cells(6), surface::subs_of_cells(4))
                .accepted);
    const ui::Rect first =
        cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                                screen_of(t.session()))
                          .rect);
    t.press_canvas(first.x, first.y);
    REQUIRE(t.session().panels.selected == stock::kKind);
    const ui::Rect at_first = cells_covered(hotkeys_bounds(t.session(), screen_of(t.session())));
    CHECK(at_first.x == first.x);
    CHECK(at_first.y == first.y);
    CHECK((at_first.x != global.x || at_first.y != global.y));

    // ...AND IT FOLLOWS THE PANE THAT MOVES. Nothing is stored: the anchor is re-derived,
    // so authoring a place moves the help on the next question.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind),
                              surface::subs_of_cells(11), surface::subs_of_cells(7))
                .accepted);
    const ui::Rect moved =
        cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                                screen_of(t.session()))
                          .rect);
    t.press_canvas(moved.x, moved.y);
    REQUIRE(t.session().panels.selected == stock::kKind);
    const ui::Rect at_builder =
        cells_covered(hotkeys_bounds(t.session(), screen_of(t.session())));
    CHECK(at_builder.x == moved.x);
    CHECK(at_builder.y == moved.y);
    CHECK((at_builder.x != at_first.x || at_builder.y != at_first.y));

    // A SELECTED PANE THE ROOM CANNOT HOLD THE VIEW BESIDE IS CLAMPED, not followed off
    // the surface: the side region begins at the canvas's own top row and its far edge is
    // the screen's, and the command list is wider than Info's column, so a view anchored
    // there shifts on BOTH axes.
    const ui::Rect side = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc).rect);
    t.press_canvas(side.x + 1, side.y + 1);
    REQUIRE(t.session().panels.selected == panel::kPaneEditor);
    const ui::Rect at_side = cells_covered(hotkeys_bounds(t.session(), screen_of(t.session())));
    REQUIRE(at_side.w > side.w); // the arrangement this half is about
    CHECK(at_side.x < side.x);
    CHECK(at_side.y == kStackY);
    CHECK(at_side.x + at_side.w <= screen_of(t.session()).w);

    // AND IT IS STILL WHOLE INSIDE THE ROOM, wherever the pane is: a selected pane near
    // the floor shifts the view UP rather than letting it be drawn off the surface -- to
    // exactly where its bottom meets the floor, since the view is its content's size.
    const std::int64_t floor_y = kWorkspaceY + screen_of(t.session()).room_h;
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind),
                              surface::subs_of_cells(2), surface::subs_of_cells(floor_y - 2))
                .accepted);
    const ui::Rect low =
        cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                                screen_of(t.session()))
                          .rect);
    t.press_canvas(low.x, low.y);
    REQUIRE(t.session().panels.selected == stock::kKind);
    // The press pointed the keys at the stand-in, whose context the view would describe in
    // rows of its own; what this half measures is the ANCHOR, so the keys go back to command
    // mode and the view's content is the global one it was compared against above.
    release_keys(t);
    const ui::Rect clamped =
        cells_covered(hotkeys_bounds(t.session(), screen_of(t.session())));
    CHECK(clamped.x == low.x);
    CHECK(clamped.y > kStackY);
    CHECK(clamped.y < low.y);
    CHECK(clamped.x + clamped.w <= screen_of(t.session()).w);
    CHECK(clamped.y + clamped.h == floor_y);
    // ...AND IT IS STILL THE SAME VIEW: the extent is the content's wherever the anchor
    // is -- not the room under a low anchor, and not a floor minted to keep a sliver a
    // view (QR-17 retired `kPickerRows` here; the content keeps a view a view).
    CHECK(clamped.w == global.w);
    CHECK(clamped.h == global.h);

    // (THE ATTENTION VIEW USED TO BE MEASURED HERE TOO, because it was an overlay in this
    // same column and the question "did it follow the anchor" was a real one. It is a pane
    // now and is placed by the setup like any other, so there is no second box left in this
    // family to compare against.)
}

// ============================================================================
// QR-17 — the hotkey view fits what it says
// ============================================================================
//
// WUX-5 put the full hotkey view beside the selected pane and left its extent where KEY-0
// had it: the overlay column's width, and the room under the anchor. The rows the view
// paints are derived from the keymap at every paint (`hotkeys_rows`), so the honest
// rectangle is the one those rows need -- read into cells by the contextual surface's own
// arithmetic (`popup_bounds_at`) rather than by the box a slot's reservation happened to
// leave. These cases are that law's falsifiers: content owns the extent, in both
// directions; the longest row and the last row fit; the room still bounds it; and a
// smaller rectangle is still nobody's pointer space and moves no reservation.

namespace {

/// The rows a canvas paints inside a rectangle, one string per row with the region's
/// padding trimmed -- so a composed row and its painted twin compare as what a maker reads.
std::vector<std::string> painted_rows_in(const surface::SurfaceCanvas& c, const ui::Rect& body) {
    std::vector<std::string> out;
    std::string line;
    for (const char ch : panel_text(c, body)) {
        if (ch != '\n') {
            line += ch;
            continue;
        }
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        out.push_back(line);
        line.clear();
    }
    return out;
}

std::size_t longest_hotkey_row(const std::vector<HotkeyRow>& rows) {
    std::size_t longest = 0;
    for (const HotkeyRow& row : rows) {
        longest = row.text.size() > longest ? row.text.size() : longest;
    }
    return longest;
}

} // namespace

TEST_CASE("QR-17/SC-1..3: the hotkey view is as tall as its rows and as wide as its longest") {
    // ⚔ MUTATIONS: the predecessor's fixed rectangle restored (the column, or the room under
    // the anchor); the longest row under-counted by one column; the rows under-counted by
    // one. Every painted row is compared with its composed twin BY INDEX, so a shortfall
    // names the line it lost -- the cut row, or the missing last one.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 70, 0, 0}));
    const auto measured = [&t](const char* beneath) {
        INFO("beneath the view: ", std::string(beneath));
        REQUIRE(t.session().hotkeys.open);
        const Screen sc = screen_of(t.session());
        const std::vector<HotkeyRow> rows = hotkeys_rows(t.session());
        REQUIRE(rows.size() > 1);
        const std::size_t longest = longest_hotkey_row(rows);
        // The fixture's premise: this room holds the whole list, chrome included.
        REQUIRE(static_cast<std::int64_t>(rows.size()) + 2 * kChromeCells <=
                kWorkspaceY + sc.room_h - kStackY);
        const ui::Rect outer = cells_covered(hotkeys_bounds(t.session(), sc));
        const ui::Rect body = pane_body_cells(fine_of_cells(outer));
        // ON A CELL MEDIUM THE INTERIOR IS EXACTLY THE CONTENT: one row per composed row
        // and one column per character of the longest -- no filler row, no unused field.
        CHECK(body.h == static_cast<std::int64_t>(rows.size()));
        CHECK(body.w == static_cast<std::int64_t>(longest));
        // ...AND EVERY COMPOSED ROW IS PAINTED WHOLE, ON ITS OWN ROW. Painted row i IS
        // composed row i; nothing is cut and nothing is omitted. (Compared as far as both
        // reach, so a view one row short names the row it lost rather than only a count.)
        const std::vector<std::string> painted = painted_rows_in(t.canvases.back(), body);
        CHECK(painted.size() == rows.size());
        const std::size_t both = painted.size() < rows.size() ? painted.size() : rows.size();
        for (std::size_t i = 0; i < both; ++i) {
            CAPTURE(i);
            CAPTURE(rows[i].text);
            CHECK(painted[i] == rows[i].text);
            CHECK(painted[i].rfind("  ... ", 0) != 0);
        }
        return outer;
    };
    t.key(input::scan::kK, input::mod::kCtrl);
    const ui::Rect command = measured("command mode");
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().hotkeys.open);

    // A SMALLER POPULATION IS A SMALLER VIEW, in both dimensions: the picker's own keys
    // are a fraction of command mode's, and the view over it is measured the same way.
    //
    // (⚠ THIS USED TO MEASURE THE ATTENTION VIEW'S CONTEXT, which had four rows and was the
    // smallest mode in the application. That context retired with the overlay -- the view
    // is a pane, and a pane's rows are the PANE's context -- so the claim is asked of the
    // picker, which is the smallest host context left and is measured the same way.)
    t.key(input::scan::kP);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPicker);
    t.key(input::scan::kK, input::mod::kCtrl);
    const ui::Rect picker = measured("the + panel picker");
    CHECK(picker.h < command.h);
    CHECK(picker.w < command.w);
    // ...AND NEITHER IS THE PREDECESSOR'S BOX: narrower than the column and shorter than
    // the room, both of them.
    const ui::Rect column = cells_covered(overlay_column(screen_of(t.session())));
    CHECK(command.w < column.w);
    CHECK(command.h < column.h);
    CHECK(picker.w < column.w);
    CHECK(picker.h < column.h);
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().hotkeys.open);
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().panels.picker.open);

    // ON THE SHIPPED FACE THE SAME ROWS RESOLVE THROUGH THE SAME MEASURER, in type. The
    // rectangle is whole cells on every face (`chrome_outer_of` reserves the coarsest
    // boundary, WUX-8), so the interior carries the face's slack and no more: every row
    // fits and is published whole, nothing is cut, and it is still nothing like the column.
    // (A taller screen: a row of type is a line and a half of cells, and the band has to
    // hold the whole list for the extent to be the content's rather than the band's.)
    t.publish(loom::to_value(surface::SurfaceExtent{160, 90, 8, 18, 12}));
    t.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(t.session().hotkeys.open);
    {
        const Screen sc = screen_of(t.session());
        const std::vector<HotkeyRow> rows = hotkeys_rows(t.session());
        const std::size_t longest = longest_hotkey_row(rows);
        const FineRect b = hotkeys_bounds(t.session(), sc);
        const PanelProsePlace place = panel_prose_place(b, sc);
        REQUIRE(place.present);
        CHECK(place.fit.graphical());
        CHECK(place.rows >= static_cast<std::int64_t>(rows.size()));
        CHECK(place.columns >= static_cast<std::int64_t>(longest));
        // The last plane is the view's (KEY-0's paint order): one region, every row whole.
        const surface::SurfaceLayer& plane = t.canvases.back().layers.back();
        REQUIRE(plane.texts.size() == 1);
        const surface::SurfaceTextRegion& region = plane.texts.back();
        REQUIRE(region.rows.size() == rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            CAPTURE(i);
            CHECK(region.rows[i].text == rows[i].text);
        }
        const ui::Rect outer = cells_covered(b);
        const ui::Rect face_column = cells_covered(overlay_column(sc));
        CHECK(outer.w < face_column.w);
        CHECK(outer.h < face_column.h);
        // ONE SIZING TRUTH (SC-8): the cells are the one measurer's own answer plus the
        // chrome, and not a cell more -- a second inversion beside `region_cells_for`
        // would show up here as a different rectangle.
        const surface::RegionCells cells = surface::region_cells_for(
            static_cast<std::int64_t>(longest), static_cast<std::int64_t>(rows.size()), 8, 18);
        CHECK(outer.w == cells.w + 2 * kChromeCells);
        CHECK(outer.h == cells.h + 2 * kChromeCells);
    }
    t.key(input::scan::kEscape);
}

TEST_CASE("QR-17/SC-4: a list the room cannot hold keeps the room and counts the cut") {
    // ⚔ MUTATION: the popup growing past the band's floor, or past the canvas's edge, to
    // hold its content. The minimum screen's band is sixteen rows and command mode's list
    // is three times that, so this is the impossibility the existing law has to answer.
    Live t;
    t.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(t.session().hotkeys.open);
    const Screen sc = screen_of(t.session());
    const std::vector<HotkeyRow> rows = hotkeys_rows(t.session());
    const std::int64_t floor_y = kWorkspaceY + sc.room_h;
    REQUIRE(static_cast<std::int64_t>(rows.size()) + 2 * kChromeCells > floor_y - kStackY);
    const ui::Rect b = cells_covered(hotkeys_bounds(t.session(), sc));
    CHECK(b.y == kStackY);
    CHECK(b.y + b.h == floor_y); // the band's height, not the content's
    CHECK(b.x >= 0);
    CHECK(b.x + b.w <= sc.w);
    // ...AND THE PAINTER SAYS WHAT IT CUT: the rows that fit, then a count of the rest --
    // and the last composed row is among the rest.
    const ui::Rect body = pane_body_cells(fine_of_cells(b));
    const std::vector<std::string> painted = painted_rows_in(t.canvases.back(), body);
    REQUIRE(painted.size() == static_cast<std::size_t>(body.h));
    const std::size_t shown = painted.size() - 1;
    for (std::size_t i = 0; i < shown; ++i) {
        CAPTURE(i);
        CHECK(painted[i] == rows[i].text);
    }
    CHECK(painted.back() == "  " + omitted_text(rows.size() - shown, "more"));
    CHECK(panel_text(t.canvases.back(), body).find(rows.back().text) == std::string::npos);
    t.key(input::scan::kEscape);

    // THE WIDTH IS BOUNDED BY THE SAME FUNCTION: asked for more columns than the canvas
    // has, `popup_bounds_at` answers the canvas, from its left edge, and never a rectangle
    // that hangs off it.
    const ui::Rect wide = cells_covered(popup_bounds_at(sc.w + 40, 3, 5, kStackY, sc));
    CHECK(wide.x == 0);
    CHECK(wide.w == sc.w);
    CHECK(wide.h == 3 + 2 * kChromeCells);
}

TEST_CASE("QR-17/SC-6,7: the compact view owns no pointer space and moves no reservation") {
    // ⚔ MUTATIONS: `occupied_at` answering the hotkey view's rectangle; the press chain
    // consuming a press while the view is open; `screen_of` reading `hotkeys.open`.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 70, 0, 0}));
    open_pane(t, ref_of(stock::kKind)); // slot 0: exactly where the unselected view opens
    REQUIRE(t.session().panels.selected == kNoPaneKind);
    const Screen before = screen_of(t.session());
    const std::int64_t doc_w = t.session().workspace_w;
    const std::int64_t doc_h = t.session().workspace_h;

    t.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(t.session().hotkeys.open);
    const Screen sc = screen_of(t.session());
    CHECK(sc.room_w == before.room_w);
    CHECK(sc.room_h == before.room_h);
    CHECK(sc.notice_y == before.notice_y);
    const ui::Rect view = cells_covered(hotkeys_bounds(t.session(), sc));
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
    // The view's own corner is the pane's corner beneath it -- a cell inside BOTH.
    REQUIRE(builder.contains(view.x, view.y));
    const Occupancy under = occupied_at(t.session().panels, t.session().setup.active, sc,
                                        view.x, view.y);
    CHECK(under.occupied);
    CHECK(under.kind == stock::kKind); // the view is not in the walk
    t.press_canvas(view.x, view.y);
    CHECK(t.session().panels.selected == stock::kKind); // the press reached the pane
    CHECK(t.session().hotkeys.open);                        // ...and the view did not take it

    // SELECTED NOW, so the view is anchored at the pane -- still nobody's pointer space,
    // and still invisible to the reservation.
    const ui::Rect anchored = cells_covered(hotkeys_bounds(t.session(), screen_of(t.session())));
    CHECK(anchored.x == builder.x);
    CHECK(anchored.y == builder.y);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, screen_of(t.session()),
                      anchored.x + 1, anchored.y + 1)
              .kind == stock::kKind);
    CHECK(screen_of(t.session()).room_w == before.room_w);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    CHECK(t.session().workspace_w == doc_w);
    CHECK(t.session().workspace_h == doc_h);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().hotkeys.open);
    CHECK(screen_of(t.session()).room_h == before.room_h);
}

TEST_CASE("WUX-5: a transient surface stays over the pane it covers, selected or not") {
    // ⚔ MUTATION: giving the selection lift priority over the overlay planes -- a pane
    // drawn in front of the contextual surface a maker just opened on it.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
    t.press_canvas(builder.x, builder.y);
    REQUIRE(t.session().panels.selected == stock::kKind);

    t.right_press_canvas(builder.x + 2, builder.y + 2);
    REQUIRE(t.menu().open);
    const ui::Rect popup = cells_covered(context_bounds(t.session(), screen_of(t.session())));
    const surface::SurfaceCanvas& c = t.canvases.back();
    std::int64_t last_pane = -1;
    std::int64_t last_popup = -1;
    for (std::size_t li = 0; li < c.layers.size(); ++li) {
        for (const surface::SurfaceRect& r : c.layers[li].rects) {
            if (r.x == builder.x && r.y == builder.y && r.w == builder.w) {
                last_pane = static_cast<std::int64_t>(li);
            }
            if (r.x == popup.x && r.y == popup.y && r.w == popup.w && r.h == popup.h) {
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

TEST_CASE("WUX-5: the contextual surface is its actions, and its width is theirs") {
    // ⚔ MUTATION: putting the heading or the hint row back. The row COUNT is the
    // population's exactly, and the width is bounded by the widest row that remains --
    // which the removed heading, being the longest string on this level, would break.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
    t.right_press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);

    const std::vector<ContextEntry> rows =
        context_population(t.menu().subject, t.menu().group);
    const FineRect bounds = context_bounds(t.session(), screen_of(t.session()));
    const PanelProsePlace place = panel_prose_place(bounds, screen_of(t.session()));
    CHECK(place.rows == static_cast<std::int64_t>(rows.size()));

    // NO CHROME ROW SURVIVES ANYWHERE IN THE PAINTED SURFACE.
    const std::vector<std::string> shown = context_rows_on(t.canvases.back(), t.session());
    REQUIRE(shown.size() == rows.size());
    for (const std::string& row : shown) {
        CHECK(row.find("ACTIONS") == std::string::npos);
        CHECK(row.find("chooses") == std::string::npos);
        CHECK(row.find("closes") == std::string::npos);
        CHECK(row.find(ref_text(ref_of(stock::kKind))) == std::string::npos);
    }

    // THE WIDTH IS THE WIDEST ROW PLUS THE CHROME, AND NOTHING WIDER. The heading this
    // surface used to open with is `ACTIONS -- ` and the pane's reference; the widest
    // action row here is far shorter, and the popup is measured to the shorter one.
    std::size_t widest = 0;
    for (const std::string& row : shown) {
        widest = row.size() > widest ? row.size() : widest;
    }
    const ui::Rect popup = cells_covered(bounds);
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

TEST_CASE("WUX-5: no ordinary pane spends a row teaching a key the keymap already owns") {
    // ⚔ MUTATION: putting a gesture claim back into a pane's header or body. The audit is
    // over the PROJECTION -- what a maker actually reads at each pane's rectangle -- so a
    // hint reintroduced anywhere in a built-in pane's composition is caught whether it is
    // spelled from the keymap or hard-coded.
    //
    // WHAT IS NOT AUDITED, and why: an EXTERNAL pane's rows are the provider's own words
    // about bindings Workshop is never told (the seam's own doctrine); the band and
    // the full hotkey view ARE the help surfaces; and the picker, the attention view and
    // the contextual surface are modes rather than panes.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 60, 0, 0}));
    for (const PaneRef& ref : {ref_of(stock::kKind), ref_of(panel::kPaneEditor)}) {
        open_pane(t, ref);
    }
    for (const std::int64_t kind : {stock::kKind, panel::kPaneEditor}) {
        CAPTURE(kind);
        const ui::Rect body =
            pane_body_cells(bounds_of(t.session().panels, t.session().setup.active, kind,
                                      screen_of(t.session()))
                                .rect);
        if (body.w <= 0 || body.h <= 0) {
            continue;
        }
        const std::string shown = panel_text(t.canvases.back(), body);
        for (const char* claim : {" build,", " pick,", " frontier,", " removes", "press ",
                                  "up/down", " opens the ", "ctrl+", "keys:"}) {
            CHECK_MESSAGE(shown.find(claim) == std::string::npos, "pane ", kind,
                          " teaches '", claim, "'");
        }
        // ⭐ A CONTROL IS NOT A KEYBOARD HINT, AND THE COUNTER-EXAMPLE MOVED. It was the
        // Info panel's footer -- `[ Create ]` and `[ Delete ]`, drawn by `action_row_text` --
        // which proved this audit had not simply deleted everything a maker can act on.
        // Those controls are `Zengine/info-pane/`'s rows now and are read across the seam.
        // What still holds this side honest is that the audit finds rows at all: a pane
        // whose body were empty would pass every claim above for the wrong reason.
        CHECK_FALSE(shown.empty());
    }
    // ...AND THE GESTURES ARE STILL DISCOVERABLE, in the one surface that owns them.
    //
    // ⭐ THE FOUR THE BUILDER TAUGHT ARE NOT IN THIS LIST ANY MORE, and the reason is what
    // VD-22 changed rather than a loss: `build`, `recipe`, `frontier` and `edit source` are
    // the BUILDER PANE'S declared rows now, so they appear in the legend and the hotkey view
    // when that pane holds the keyboard, spelled from ITS declaration under the maker's own
    // keymap file (WL-KEY-15). A host that listed them unconditionally would be teaching a
    // key that does nothing where the maker is standing -- which is the thing this case is
    // about. The pane's own half is proved through the real seam in the panes suite.
    t.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(t.session().hotkeys.open);
    const std::string view = panel_text(
        t.canvases.back(), pane_body_cells(hotkeys_bounds(t.session(), screen_of(t.session()))));
    for (const char* label : {"+ panel", "titles", "arrange desk"}) {
        CHECK_MESSAGE(view.find(label) != std::string::npos, "the help lost '", label, "'");
    }
    for (const char* gone : {"build", "recipe", "frontier", "edit source"}) {
        CHECK_MESSAGE(view.find(gone) == std::string::npos,
                      "the host still teaches the Builder pane's '", gone, "'");
    }
}

// ============================================================================
// WUX-8 — the graphical desk's chrome is graphical
// ============================================================================
//
// WUX-5 gave every pane a visible boundary and paid the smallest thing a TERMINAL can
// draw for it -- one whole canvas cell -- on both faces, because one number was the only
// way to put the boundary in the same place in each. WUX-6 then established that a face
// may spend the unit it can actually distinguish while ONE authored rectangle stays
// authoritative. These cases are the falsifiers for spending that on the boundary: the
// terminal still frames a pane with a cell, the shipped window frames the same authored
// pane with a device PIXEL, and the interior the window no longer has to reserve goes
// back to the pane rather than staying behind as an invisible tax.

/// The two faces these cases compare, built from what a medium REPORTS: a terminal says
/// nothing about pixels, and the shipped window says a cell is `kCanvasCellPx` of them and
/// hands over the face it opened.
inline Screen tui_screen() {
    return screen_of(screen_session(kScreenMinW, kScreenMinH, 0, 0, 0));
}
inline Screen sdl_screen() {
    return screen_of(screen_session(kScreenMinW, kScreenMinH, 8, 18, surface::kCanvasCellPx));
}

TEST_CASE("WUX-8: the chrome a pane wears is one unit of the face in front of the maker") {
    // ⚔ MUTATION: `chrome_grain` answering `kChromeSubs` whatever the medium said -- which
    // is WUX-5 exactly, and is what "SDL still subtracts one full text cell" looks like
    // from inside. ⚔ MUTATION (the other direction): a terminal adopting the pixel answer,
    // which floors to nothing there and leaves a pane with no edge at all.
    const Screen tui = tui_screen();
    const Screen sdl = sdl_screen();

    CHECK(tui.cell_px == 0); // "my device unit IS the cell" -- every terminal's answer
    CHECK(sdl.cell_px == surface::kCanvasCellPx);
    CHECK(chrome_grain(tui) == kChromeSubs);
    CHECK(chrome_grain(sdl) == surface::kCellSubs / surface::kCanvasCellPx);
    CHECK(chrome_grain(sdl) < chrome_grain(tui));

    // AND IT IS EXACTLY ONE DEVICE UNIT ON EACH, said in the medium's own arithmetic
    // rather than by comparing constants: one grain reads back as one unit, and one less
    // than a grain reads back as none.
    CHECK(surface::device_of_subs(chrome_grain(sdl), sdl.cell_px) == 1);
    CHECK(surface::device_of_subs(chrome_grain(sdl) - 1, sdl.cell_px) == 0);
    CHECK(surface::device_of_subs(chrome_grain(tui), tui.cell_px) == 1); // ...one CELL

    // THE PANE ITSELF: one authored rectangle, two boundaries, and the outer rectangle is
    // the same object on both faces.
    const FineRect outer = fine_of_cells(ui::Rect{4, 3, 20, 9});
    const PaneInside on_tui = pane_inside(outer, tui);
    const PaneInside on_sdl = pane_inside(outer, sdl);
    CHECK(on_tui.chrome_subs == kChromeSubs);
    CHECK(on_sdl.chrome_subs == surface::kCellSubs / surface::kCanvasCellPx);
    CHECK(cells_covered(on_tui.rect) == ui::Rect{5, 4, 18, 7}); // WUX-5's picture, unmoved
    CHECK(on_sdl.rect.w > on_tui.rect.w);
    CHECK(on_sdl.rect.h > on_tui.rect.h);
}

TEST_CASE("WUX-8: the graphical boundary is one device pixel, drawn INSIDE the pane") {
    // ⚔ MUTATION: growing the pane outward to pay for its boundary, or drawing the ring on
    // the pixel outside it. The authored rectangle is the outer rectangle; the ring is
    // subtracted from it, and every number below is a pixel of the shipped face.
    const Screen sdl = sdl_screen();
    const FineRect outer = fine_of_cells(ui::Rect{4, 3, 20, 9});
    const FineRect inside = pane_inside(outer, sdl).rect;

    const std::int64_t left = surface::px_of_subs(outer.x);
    const std::int64_t top = surface::px_of_subs(outer.y);
    const std::int64_t right = surface::px_of_subs(surface::add_cells(outer.x, outer.w));
    const std::int64_t bottom = surface::px_of_subs(surface::add_cells(outer.y, outer.h));

    CHECK(surface::px_of_subs(inside.x) == left + 1);
    CHECK(surface::px_of_subs(inside.y) == top + 1);
    CHECK(surface::px_of_subs(surface::add_cells(inside.x, inside.w)) == right - 1);
    CHECK(surface::px_of_subs(surface::add_cells(inside.y, inside.h)) == bottom - 1);

    // ...WHICH IS FOUR VISIBLE SIDES AND NOT AN OUTLINE AROUND NOTHING: the interior is
    // strictly inside the pane on both axes and still holds almost all of it.
    CHECK(inside.x > outer.x);
    CHECK(surface::add_cells(inside.x, inside.w) < surface::add_cells(outer.x, outer.w));
    CHECK(right - left == 20 * surface::kCanvasCellPx);
    CHECK(surface::px_of_subs(surface::add_cells(inside.x, inside.w)) -
              surface::px_of_subs(inside.x) ==
          20 * surface::kCanvasCellPx - 2);
}

TEST_CASE("WUX-8: the interior a window no longer reserves goes back to the pane") {
    // ⚔ MUTATION: painting a one-pixel line while the body, the room and the press inverse
    // keep spending the old cell -- the "looks thin, acts thick" pane. The capacity below is
    // derived from the ACTUAL post-chrome pixels, so a body that kept the cell inset shows
    // up as fewer rows than the arithmetic says.
    const Screen sdl = sdl_screen();
    const FineRect outer = fine_of_cells(ui::Rect{4, 3, 20, 9});

    const PanelProsePlace thin = panel_prose_place(outer, sdl);
    REQUIRE(thin.present);
    const std::int64_t px_h = 9 * surface::kCanvasCellPx - 2; // the pane, less one px a side
    const std::int64_t px_w = 20 * surface::kCanvasCellPx - 2;
    CHECK(thin.rows == (px_h - 2 * surface::kTextInsetPx) / 18);
    CHECK(thin.columns == (px_w - 2 * surface::kTextInsetPx) / 8);

    // ...AND IT IS STRICTLY MORE THAN THE SAME FACE HAD WHEN IT PAID A CELL. This is the
    // comparison the phase is FOR: same authored pane, same face, one honest boundary
    // instead of a borrowed one.
    const FineRect cell_inset = pane_interior(outer, kChromeSubs);
    const surface::RegionFit was =
        surface::fit_region_subs(cell_inset.x, cell_inset.y, cell_inset.w, cell_inset.h,
                                 sdl.text_advance_px, sdl.text_line_px);
    CHECK(thin.rows > was.rows);
    CHECK(thin.columns > was.columns);

    // AND NOTHING WAS PADDED BACK. ⚔ MUTATION: an invisible cell-sized reservation kept "so
    // the row counts do not move" -- the body would then be a whole cell narrower than the
    // pixels it was granted.
    CHECK(thin.inside.w == outer.w - 2 * chrome_grain(sdl));
    CHECK(thin.inside.h == outer.h - 2 * chrome_grain(sdl));
}

TEST_CASE("WUX-8: a face that describes an interior in CELLS pays the cell") {
    // THE HALF THAT MAKES THE THIN BOUNDARY HONEST. A boundary nobody can see is not a
    // boundary: where a face projects a pane's interior onto covered CELLS -- a terminal
    // always, a window whose font never opened, a window whose face is too tall for this
    // pane -- an inset finer than a cell is projected away, and the interior spills back
    // over its own left and top edge leaving a ring on two sides.
    //
    // ⚔ MUTATION: `pane_inside` returning the thin candidate unconditionally. Every
    // symmetric-ring check below fails, on exactly the faces that cannot draw the pixel.
    const FineRect outer = fine_of_cells(ui::Rect{4, 3, 20, 9});
    const ui::Rect box = cells_covered(outer);

    struct Face {
        const char* what;
        Screen sc;
    };
    const std::vector<Face> cell_faces{
        {"a terminal", tui_screen()},
        {"a window whose font never opened",
         screen_of(screen_session(kScreenMinW, kScreenMinH, 0, 0, surface::kCanvasCellPx))},
    };
    for (const Face& face : cell_faces) {
        INFO(face.what);
        const PaneInside in = pane_inside(outer, face.sc);
        CHECK(in.chrome_subs == kChromeSubs);
        const ui::Rect body = cells_covered(in.rect);
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
        screen_of(screen_session(kScreenMinW, kScreenMinH, 8, 40, surface::kCanvasCellPx));
    const FineRect small = fine_of_cells(ui::Rect{4, 3, 20, 3});
    const PaneInside cramped = pane_inside(small, tall);
    CHECK_FALSE(cramped.fit.graphical());
    CHECK(cramped.chrome_subs == kChromeSubs);
    const PaneInside roomy = pane_inside(fine_of_cells(ui::Rect{4, 3, 20, 12}), tall);
    REQUIRE(roomy.fit.graphical());
    CHECK(roomy.chrome_subs == chrome_grain(tall));
    CHECK(roomy.chrome_subs < kChromeSubs);
}

TEST_CASE("WUX-8: the ring IS the backdrop the interior did not cover, on both faces") {
    // ⚔ MUTATION: a painter that strokes a border of its own. There is no thickness on
    // `paint_panel_frame` to get wrong -- it pushes the OUTER rect and the body's own
    // ground clears what it occupies -- so what a maker sees is a subtraction rather than
    // a drawing, and it is the same subtraction the room and the press inverse spend.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const Screen tui = screen_of(t.session());
    const FineRect outer =
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, tui).rect;
    REQUIRE_FALSE(outer.empty());

    const surface::SurfaceCanvas& c = t.canvases.back();
    const ui::Rect box = cells_covered(outer);
    CHECK(has_rect(c, box.x, box.y, box.w, box.h, kPaneChrome)); // the whole pane, once

    // ON A TERMINAL the interior it publishes leaves a one-CELL ring...
    const ui::Rect body = pane_body_cells(outer);
    CHECK(body == ui::Rect{box.x + 1, box.y + 1, box.w - 2, box.h - 2});
    const std::vector<surface::SurfaceTextRegion> at = regions_at(c, body.x, body.y);
    REQUIRE(at.size() == 1);
    CHECK(at.front().w == body.w);
    CHECK(at.front().h == body.h);

    // ...AND ON THE SHIPPED WINDOW the same authored pane leaves a one-PIXEL one, from the
    // same rect and the same subtraction. The region's fine origin is what a graphical
    // medium clips and fills to (`fit_region`), so this is the ring a maker sees.
    const Screen sdl = sdl_screen();
    const PaneInside on_sdl = pane_inside(outer, sdl);
    const surface::SurfaceRect wire = wire_rect_of(on_sdl.rect, surface::role::kFill);
    CHECK(surface::px_of_subs(surface::subs_of_wire(wire.x, wire.sub_x)) ==
          surface::px_of_subs(outer.x) + 1);
    CHECK(on_sdl.fit.view.x == surface::px_of_subs(outer.x) + 1);
    CHECK(on_sdl.fit.view.y == surface::px_of_subs(outer.y) + 1);
    CHECK(on_sdl.fit.view.w == surface::px_of_subs(surface::add_cells(outer.x, outer.w)) -
                                   surface::px_of_subs(outer.x) - 2);
    CHECK(on_sdl.fit.view.h == surface::px_of_subs(surface::add_cells(outer.y, outer.h)) -
                                   surface::px_of_subs(outer.y) - 2);
}

TEST_CASE("WUX-8: selected and ordinary differ in INK, and in nothing else") {
    // ⚔ MUTATION: a painter that insets a SELECTED pane further, so its boundary reads
    // heavier. The pane's contents would then jump the moment a maker pointed at it, which
    // is the one thing a selection must never do. The picture is compared field by field:
    // same backdrop rectangle, same published region, different ROLE.
    Live t;
    t.publish(loom::to_value(
        surface::SurfaceExtent{160, 60, 8, 18, surface::kCanvasCellPx}));
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    REQUIRE(sc.cell_px == surface::kCanvasCellPx);
    const FineRect outer =
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect;
    REQUIRE_FALSE(outer.empty());
    const ui::Rect box = cells_covered(outer);
    const ui::Rect body = pane_body_cells(outer, sc);

    const auto picture = [&](const surface::SurfaceCanvas& c) {
        // `regions_at` keys on the region's own published corner, which on this face is the
        // pane's cell with a sub-cell remainder -- the cell the thin boundary shares with the
        // body it no longer displaces.
        const std::vector<surface::SurfaceTextRegion> at = regions_at(c, body.x, body.y);
        REQUIRE_FALSE(at.empty());
        return at.front();
    };

    REQUIRE(t.session().panels.selected == kNoPaneKind);
    const surface::SurfaceTextRegion ordinary = picture(t.canvases.back());
    CHECK(has_rect(t.canvases.back(), box.x, box.y, box.w, box.h, kPaneChrome));

    t.press_canvas(box.x, box.y); // the pane's own border cell: choosing it, nothing else
    REQUIRE(t.session().panels.selected == stock::kKind);
    const surface::SurfaceTextRegion chosen = picture(t.canvases.back());

    // THE INK CHANGED...
    CHECK(has_rect(t.canvases.back(), box.x, box.y, box.w, box.h, kPaneChromeSelected));
    CHECK_FALSE(has_rect(t.canvases.back(), box.x, box.y, box.w, box.h, kPaneChrome));

    // ...AND NOT ONE NUMBER OF THE GEOMETRY DID, sub-cell remainders included -- which is
    // where a one-pixel change would hide.
    CHECK(chosen.x == ordinary.x);
    CHECK(chosen.y == ordinary.y);
    CHECK(chosen.w == ordinary.w);
    CHECK(chosen.h == ordinary.h);
    CHECK(chosen.sub_x == ordinary.sub_x);
    CHECK(chosen.sub_y == ordinary.sub_y);
    CHECK(chosen.sub_w == ordinary.sub_w);
    CHECK(chosen.sub_h == ordinary.sub_h);
    CHECK(chosen.rows.size() == ordinary.rows.size());

    // AND THE BOUNDARY IS STILL ONE PIXEL: the region begins one device pixel inside the
    // pane on both axes, selected or not.
    CHECK(surface::px_of_subs(surface::subs_of_wire(chosen.x, chosen.sub_x)) ==
          surface::px_of_subs(outer.x) + 1);
    CHECK(surface::px_of_subs(surface::subs_of_wire(chosen.y, chosen.sub_y)) ==
          surface::px_of_subs(outer.y) + 1);
}

TEST_CASE("WUX-8: a content-sized surface reserves the coarsest boundary, once") {
    // THE CONTEXTUAL POPUP measures its rows and asks for a rectangle to hold them INSIDE
    // its chrome, and that rectangle is ONE whole-cell `ui::Rect` shown on whichever face
    // draws it -- so what it reserves is the boundary the COARSEST face spends. A graphical
    // face draws a thinner ring inside that reservation and hands the difference to the
    // popup's own interior; a popup sized for pixels would cut a row off itself the moment
    // a terminal drew it.
    //
    // ⚔ MUTATION: making `chrome_outer_of` medium-specific. The popup's rectangle -- and
    // therefore its placement -- would then depend on the face, which is exactly what this
    // phase promised not to touch.
    const ui::Rect grown = chrome_outer_of(0, 0, 18, 7);
    CHECK(grown.w == 18 + 2 * kChromeCells);
    CHECK(grown.h == 7 + 2 * kChromeCells);
    CHECK(cells_covered(pane_interior(fine_of_cells(grown), kChromeSubs)) ==
          ui::Rect{1, 1, 18, 7}); // exact on the face that reserved it

    // AND THE POPUP'S RECTANGLE IS THE SAME RECTANGLE ON BOTH FACES. ⚔ MUTATION: anchoring
    // the contextual surface to a pane's BODY rather than to the pane.
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect pane = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(pane.x + 1, pane.y + 1);
    REQUIRE(t.session().context.open);
    const FineRect on_cells = context_bounds(t.session(), screen_of(t.session()));
    Session graphical = t.session();
    graphical.cell_px = surface::kCanvasCellPx;
    graphical.text_advance_px = 8;
    graphical.text_line_px = 18;
    const FineRect on_window = context_bounds(graphical, screen_of(graphical));
    CHECK(on_window.x == on_cells.x);
    CHECK(on_window.y == on_cells.y);
}

// ---- WUX-7: two presses, one gesture ------------------------------------------------------

TEST_CASE("WUX-7: what makes two presses one double-click, and what does not") {
    // THE QUALIFICATION IS PURE AND TOTAL, so every one of its conditions is stated here as
    // an ordinary comparison rather than raced against a stopwatch. ⚔ MUTATION: dropping any
    // one of the five reddens exactly the subcase that names it.
    // ⚠ THE ARMED PLACE USED TO BE `kTerminalLine` (VD-24). This case is over a PURE
    // function and the place is one of its inputs; the Terminal's line is a pane's now, so
    // the arming is over a line this host still owns and the claim is unchanged.
    const component::WordSpan word{4, 9};
    const component::WordSpan elsewhere{12, 16};
    const ClickMemory armed = click_landed(text_drag_place::kPropertyDraft, 3, word, 1000);
    REQUIRE(armed.armed);

    SUBCASE("the ordinary case: same line, same draft, same word, soon enough") {
        CHECK(doubles_a_click(armed, text_drag_place::kPropertyDraft, 3, word, 1000));
        CHECK(doubles_a_click(armed, text_drag_place::kPropertyDraft, 3, word, 1399));
        CHECK(doubles_a_click(armed, text_drag_place::kPropertyDraft, 3, word,
                              1000 + kDoubleClickMs)); // the boundary itself doubles
    }
    SUBCASE("too long apart is two presses") {
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kPropertyDraft, 3, word,
                                    1001 + kDoubleClickMs));
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kPropertyDraft, 3, word, 99999));
    }
    SUBCASE("a different editable line is a different place") {
        // `kEditorBody` was the third place and is gone with the Editor (VD-25); an external
        // pane's sweep is a place of this host's record and arms nothing.
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kExternalPane, 3, word, 1000));
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kPaneEditorDraft, 3, word, 1000));
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kNone, 3, word, 1000));
    }
    SUBCASE("a different DRAFT of the same line is a different box") {
        // Closing a property draft and opening another bumps the epoch, so an arming from
        // the one a maker just left cannot be spent on the one they just opened.
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kPropertyDraft, 4, word, 1000));
    }
    SUBCASE("a different word target is an ordinary click") {
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kPropertyDraft, 3, elsewhere, 1000));
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kPropertyDraft, 3,
                                    component::WordSpan{4, 8}, 1000));
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kPropertyDraft, 3,
                                    component::WordSpan{5, 9}, 1000));
    }
    SUBCASE("a position in NO word never doubles, however fast the hand") {
        const ClickMemory nothing =
            click_landed(text_drag_place::kPropertyDraft, 3, component::WordSpan{7, 7}, 1000);
        CHECK_FALSE(doubles_a_click(nothing, text_drag_place::kPropertyDraft, 3,
                                    component::WordSpan{7, 7}, 1000));
    }
    SUBCASE("nothing armed is nothing to double") {
        CHECK_FALSE(doubles_a_click(ClickMemory{}, text_drag_place::kPropertyDraft, 0,
                                    component::WordSpan{0, 0}, 0));
        ClickMemory disarmed = armed;
        disarmed.armed = false;
        CHECK_FALSE(doubles_a_click(disarmed, text_drag_place::kPropertyDraft, 3, word, 1000));
    }
    SUBCASE("a reading that went backwards is not a fast hand") {
        CHECK_FALSE(doubles_a_click(armed, text_drag_place::kPropertyDraft, 3, word, 999));
    }
    SUBCASE("a fresh session has armed nothing") {
        Session fresh;
        CHECK_FALSE(fresh.click.armed);
        CHECK(fresh.click.place == text_drag_place::kNone);
    }
}

// ---- WUX-7: reading past the ellipsis ------------------------------------------------------

// (...and the three WUX-7 reveal cases -- a revealed row as a window, the pointer's column as
// the offset, and the four things that must agree before a row is scrolled -- went with the
// feature itself. `screen_reveal.cpp` carries the reason where the code was.)

TEST_CASE("WUX-7: contextual Arrange lifts the pane it addressed, not the one in front") {
    // THE OVERLAP FALSIFIER, and a non-overlapping desk would not be evidence for it: the
    // Builder is authored UNDER the Info panel and the two share cells, so "which pane did
    // this select" and "which pane was on top" are two different answers before the gesture
    // and have to be one answer after it.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    // THE PANE IN THE RIGHT COLUMN IS AUTHORED. It was Info, a built-in the catalog put
    // there; the place is unchanged and its occupant is a desk row now.
    open_at_right_column(t, panel::kPaneEditor);
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    const ui::Rect side = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc).rect);
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind),
                              surface::subs_of_cells(side.x - 4),
                              surface::subs_of_cells(side.y + 2))
                .accepted);
    REQUIRE(send_to_back(live(t).setup.active, ref_of(stock::kKind)));

    // BEFORE: Info owns the overlap, and it is Info a press there would reach.
    const std::int64_t over_x = side.x + 1;
    const std::int64_t over_y = side.y + 3;
    const std::vector<std::int64_t> authored = authored_order(t.session());
    const std::string desk = setup_persist::to_text(t.session().setup.active);
    const std::vector<std::int64_t> ranks = ranks_of(t.session().setup.active);
    t.press_canvas(over_x, over_y);
    REQUIRE(t.session().panels.selected == panel::kPaneEditor);
    REQUIRE(occupied_at(t.session().panels, t.session().setup.active, sc, over_x, over_y).kind ==
            panel::kPaneEditor);
    const std::int64_t keyboard_before = t.session().panels.keyboard;

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
    // writing `Panels::selected`.
    CHECK(t.session().arrange.open);
    CHECK_FALSE(t.session().arrange.desk);
    CHECK(t.session().arrange.pane == ref_of(stock::kKind));
    CHECK(t.session().panels.selected == stock::kKind);
    CHECK(selected_pane(t.session().panels) == stock::kKind);
    CHECK(painted_order(t.session()).back() == stock::kKind);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, screen_of(t.session()),
                      over_x, over_y)
              .kind == stock::kKind);

    // ...AND THE LIFT IS STILL A ROTATION. No rank moved, no authored byte changed, and a
    // save right now writes the desk it would have written before the gesture.
    CHECK(authored_order(t.session()) == authored);
    CHECK(ranks_of(t.session().setup.active) == ranks);
    CHECK(setup_persist::to_text(t.session().setup.active) == desk);

    // ...AND THE KEYBOARD CANDIDATE IS A SEPARATE FACT, untouched by an entrance that
    // happens to change the selection. ⚔ MUTATION: writing `panels.keyboard` here too.
    CHECK(t.session().panels.keyboard == keyboard_before);

    // THE ARRANGEMENT OPERATION ADDRESSES THE SAME PANE the selection names.
    const ui::Rect before = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    const ui::Rect info_before = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor,
                  screen_of(t.session()))
            .rect);
    t.key(input::scan::kLeft);
    const ui::Rect after = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    CHECK(after.x < before.x);
    CHECK(cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor,
                                  screen_of(t.session()))
                            .rect)
              .x == info_before.x);
    CHECK(t.session().panels.selected == stock::kKind); // moving it did not lose it

    // LEAVING KEEPS THE SELECTION, because a selection is not a mode: it lives and dies by
    // the same rules an ordinary press's does.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().arrange.open);
    CHECK(t.session().panels.selected == stock::kKind);
    CHECK(painted_order(t.session()).back() == stock::kKind);
    CHECK(ranks_of(t.session().setup.active) == ranks);
}

TEST_CASE("WUX-7: every pane a maker can point at can be arranged, and the refusals are blind") {
    // ADMISSION PRECEDES BINDING, and after WUX-7 that has to include the selection: a
    // refusal that had quietly re-selected something would have moved the desk while saying
    // it changed nothing. ⚔ MUTATION: selecting before `arrange_geometry_ready`.
    //
    // ⚠ AND THE REFUSAL THIS CASE USED TO DRIVE IS RETIRED. It right-pressed Info and read
    // back "is in the reserved side column -- the screen owns its place"; the screen owns no
    // place now (`the-room-is-the-screen`), so the sentence is gone and Info arranges like
    // every other pane. What the case says instead is the stronger thing that replaced it,
    // and it is TWO facts rather than one: every pane a maker can point at is arrangeable,
    // and every refusal that remains belongs to a pane that has no rectangle to point at --
    // so the admission and the pointer cannot disagree, which is what WUX-7 was protecting.
    Live t;
    // THE PANE IN THE RIGHT COLUMN IS AUTHORED. It was Info, a built-in the catalog put
    // there; the place is unchanged and its occupant is a desk row now.
    open_at_right_column(t, panel::kPaneEditor);
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(slot.x, slot.y);
    REQUIRE(t.session().panels.selected == stock::kKind);

    // ONE: Arrange on the right column's pane, through the maker's own gesture, is accepted.
    const ui::Rect side = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(side.x + side.w - 1, side.y + 1);
    REQUIRE(t.menu().pane == ref_of(panel::kPaneEditor));
    t.key(input::scan::kReturn);
    CHECK(t.session().arrange.open);
    CHECK(t.notice().find("reserved side column") == std::string::npos);

    // TWO: THE REFUSAL THAT REMAINS IS BLIND. A pane sized in pixels cannot be projected on
    // any medium here, and `project_pane` answers that with an empty rectangle -- so the
    // refusal and the invisibility are ONE fact, and there is no cell a maker could press to
    // reach it. That is why no live gesture in this suite can produce a refused Arrange any
    // more, and saying so is better than leaving the absence for a reader to notice.
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(panel::kPaneEditor),
                             PaneSize{pane_unit::kPixels, 300}, PaneSize{pane_unit::kDefault, 0})
                .accepted);
    const Screen sc = screen_of(t.session());
    const PanelBounds blind =
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc);
    CHECK(blind.open);
    CHECK_FALSE(blind.projected);
    CHECK(blind.rect.empty());
    CHECK_FALSE(pane_unit_projectable(pane_of(t.session().setup.active, ref_of(panel::kPaneEditor))));
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, side.x, side.y)
                    .kind == panel::kPaneEditor);
}

TEST_CASE("WUX-5/WUX-7: the arrangement desk's pointer takes what is visibly in front") {
    // ⚔ THE MASK THIS CASE EXISTS TO CLOSE. Making the desk's pointer walk spend the
    // AUTHORED order instead of the effective one left the whole lane green: every
    // arrangement case reached its pane either with no selection at all or with no pane
    // over it, so the one arrangement of facts that can tell the two orders apart -- a
    // LIFTED pane under the arrangement pointer, in an overlap it does not authoredly own
    // -- was never arranged. WUX-5's claim that all four consumers share one order was
    // therefore three-quarters proven.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(t, ref_of(panel::kPaneEditor));
    open_pane(t, ref_of(stock::kKind));
    // THE PANE MANAGER LEAVES THE STACK FIRST, and only then is the other pane's rectangle read:
    // a pane taken out of the composition lets the ones under it move up, so a rectangle
    // read before that is a rectangle about a desk that no longer exists.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kPaneEditor),
                              surface::subs_of_cells(1), surface::subs_of_cells(30))
                .accepted);
    const PanelBounds second_at = bounds_of(t.session().panels, t.session().setup.active,
                                            stock::kKind, screen_of(t.session()));
    REQUIRE(second_at.open);
    const ui::Rect files = cells_covered(second_at.rect);
    REQUIRE(files.w > 4);
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kPaneEditor),
                              surface::subs_of_cells(files.x + 2),
                              surface::subs_of_cells(files.y + 2))
                .accepted);
    REQUIRE(send_to_back(live(t).setup.active, ref_of(panel::kPaneEditor)));

    // AUTHORED: the second pane owns the overlap.
    const std::int64_t ox = files.x + 3;
    const std::int64_t oy = files.y + 3;
    REQUIRE(occupied_at(t.session().panels, t.session().setup.active, screen_of(t.session()),
                        ox, oy)
                .kind == stock::kKind);

    // SELECTED: a press on a strip only the Builder covers lifts it, and the lift reaches
    // the overlap -- which is the state every other arrangement case is missing.
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + builder.w - 1, builder.y + builder.h - 1);
    REQUIRE(t.session().panels.selected == panel::kPaneEditor);
    REQUIRE(occupied_at(t.session().panels, t.session().setup.active, screen_of(t.session()),
                        ox, oy)
                .kind == panel::kPaneEditor);

    // ...AND THE ARRANGEMENT DESK'S OWN WALK AGREES WITH THE PICTURE.
    //
    // ⚠ THE KEYBOARD IS PUT BACK BY HAND, and it is the one thing here that is not a
    // maker's gesture. Since the Builder panel became a weave, every overlay-stack built-in
    // TAKES THE KEYBOARD, so the press that lifts a pane also points the keys at it -- and
    // `w` is a command-mode row. The maker's own way back (a press on a pane that takes no
    // keyboard) would clear `panels.selected`, which IS the lift this case is about, so the
    // two cannot both be spent through the pointer any more. The selection is left exactly
    // as the press made it; only the keyboard address is reset, which is the fact `w` reads
    // and the fact the walk below does not.
    live(t).panels.keyboard = kNoPaneKind;
    enter_arrange_desk(t);
    t.press_canvas(ox, oy);
    CHECK(t.session().arrange.pane == ref_of(panel::kPaneEditor));
    CHECK(t.session().pane_drag.active);
    CHECK(t.session().pane_drag.pane == ref_of(panel::kPaneEditor));
}

// ============================================================================
// ---- WUX-9: the layout tabs ------------------------------------------------
//
// The run of desk arrangements this Workshop is holding, on the left of the band's
// existing status row. Everything here is composition: what the run says, what it does
// when it does not fit, and that the press inverse spends the painter's own spans.

namespace {

/// A `SetupState` holding `names.size()` layouts in that order, with `live` active.
SetupState shelf_of(const std::vector<std::string>& names, std::size_t live) {
    REQUIRE(!names.empty());
    REQUIRE(live < names.size());
    SetupState s;
    s.active = setup_of(names[live], {panel::kPaneEditor});
    s.active_at = live;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != live) {
            s.shelved.push_back(Layout{setup_of(names[i], {panel::kPaneEditor}), SetupLink{}});
        }
    }
    return s;
}

/// The maker's order, read back out of the run -- the one thing switching may never move.
std::vector<std::string> order_of(const SetupState& s) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < layout_count(s); ++i) {
        out.push_back(layout_at(s, i).name);
    }
    return out;
}

} // namespace

TEST_CASE("WUX-9/SC-2: a layout is a Setup and the run is the shelf plus the live value") {
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

TEST_CASE("WUX-9/SC-3: switching never reorders the run, and the live value never doubles") {
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

TEST_CASE("WUX-11/SC-1: new is BLANK and appended, and it is a value of its own") {
    // ⭐ NEW MEANS NEW. WUX-9 shipped `layout.new` as a COPY of the live desk; WUX-11 split
    // the two meanings, because copying is what `duplicate_layout` is for and a maker asking
    // for a new desk is asking for an empty one.
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
    // SC-2: ITS ASSOCIATION IS NONE. A desk that has just been made has never been written
    // to or read from any artifact.
    CHECK(link_status(s.active, s.active_link) == setup_link::kNone);

    // THE CEILING IS A BOUND ON WORK AND IT REFUSES RATHER THAN DROPPING ANYTHING.
    while (layout_count(s) < kMaxLayouts) {
        REQUIRE(add_layout(s));
    }
    CHECK(layout_count(s) == kMaxLayouts);
    CHECK_FALSE(add_layout(s));
    CHECK(layout_count(s) == kMaxLayouts);
}

TEST_CASE("WUX-9/SC-3+SC-11: a new layout appends however far into the run you stand") {
    // FOUND BY A MUTATION THE FIRST SUITE DID NOT CATCH. `add_layout` puts the ORIGINAL back
    // at `active_at` and takes the appended position; a `push_back` of the original is
    // identical while the live layout is LAST -- which is where every other case in this file
    // stands, because that is where `new` leaves you -- and swaps two layouts the moment a
    // maker adds one from the middle of their own run.
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

    // ...and the layout a maker was STANDING on goes back on its own position with every
    // byte of it intact -- which is the half a `push_back` of the live value gets wrong.
    SetupState middle = shelf_of({"A", "B", "C"}, 1);
    REQUIRE(add_pane(middle.active, ref_of(stock::kKind)));
    const Setup standing = middle.active;
    REQUIRE(add_layout(middle));
    CHECK(layout_at(middle, 1) == standing);
    CHECK_FALSE(has_pane(layout_at(middle, 0), ref_of(stock::kKind)));
    CHECK_FALSE(has_pane(middle.active, ref_of(stock::kKind)));
}

TEST_CASE("WUX-9/SC-11: removing takes the next neighbour, the previous only at the end") {
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

// ---- WUX-11: the value operations the new gestures spend --------------------------------

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

TEST_CASE("WUX-11/SC-2: duplicate copies the desk exactly and always clears the association") {
    // ⭐ THE ONE MANDATORY LAW ABOUT COPYING. An inherited association would have the copy
    // claim an artifact it has never been written to, and the first `s` would overwrite the
    // very file the maker duplicated in order not to touch.
    SetupState s = linked_shelf({"Home", "Code", "Art"}, 1, "/w/code.json");
    REQUIRE(add_pane(s.active, ref_of(stock::kKind)));
    s.active_link.known = s.active; // saved again after the edit: `current`
    REQUIRE(link_status(s.active, s.active_link) == setup_link::kCurrent);
    const Setup source = s.active;

    REQUIRE(duplicate_layout(s, 1));

    // THE DESK IS COPIED WHOLE -- the name included, because duplicate names are legal and
    // position is a layout's identity. Inventing `Code (copy)` would be this file authoring
    // a maker's word for them.
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

    // SC-25: THE CEILING REFUSES A DUPLICATE WITHOUT DROPPING ANYTHING.
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

TEST_CASE("WUX-11/SC-3: rename writes one layout's name and touches nothing else") {
    SetupState s = linked_shelf({"Home", "Code", "Art"}, 1, "/w/code.json");
    const Setup was = s.active;
    const std::vector<std::string> shelf_before = order_of(s);

    // THE LIVE ONE, BY POSITION.
    REQUIRE(rename_layout(s, 1, "Build"));
    CHECK(s.active.name == "Build");
    CHECK(s.active.panes == was.panes); // only the name moved
    CHECK(order_of(s) == std::vector<std::string>{"Home", "Build", "Art"});
    // ⭐ AND THE ASSOCIATION IS NOT REPLACED OR DETACHED -- it is the same artifact, and the
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

TEST_CASE("WUX-11/SC-4: moving a layout changes order and nothing else") {
    // ⭐ EVERY POSITION TO EVERY OTHER, SWEPT. `move_layout` goes through the inverse pair,
    // so what this measures is that the run a maker sees is exactly the run with one
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

TEST_CASE("WUX-11/SC-5: closing an inactive tab leaves the live desk exactly where it was") {
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

    // SC-13: THE ASSOCIATION DIES WITH THE LAYOUT AND ONLY WITH IT.
    SetupState two = shelf_of({"One", "Two"}, 0);
    two.active_link = SetupLink{"/w/one.json", two.active};
    two.shelved[0].link = SetupLink{"/w/two.json", two.shelved[0].desk};
    REQUIRE(remove_layout(two, 1));
    CHECK(layout_count(two) == 1);
    CHECK(two.active.name == "One");
    CHECK(two.active_link.path == "/w/one.json"); // the survivor kept its own
}

TEST_CASE("WUX-11/SC-6+SC-12: switching carries the association, sharing keeps it honest") {
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

    // ⭐ SC-12: WRITING THE ARTIFACT TEACHES EVERY ASSOCIATION TO IT. Without the sweep the
    // first layout would go on claiming `current` against bytes this write just replaced,
    // which is a status wrong about the only thing it is for.
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

TEST_CASE("WUX-11/SC-7: the three verdicts, and what makes a fresh desk `none`") {
    // ⭐ `none` DOES NOT MEAN UNSAVED. A layout with no association is safely persisted by
    // the session; what it has not got is an explicit standalone Setup artifact.
    const Setup desk = setup_of("Code", {panel::kPaneEditor});
    CHECK(link_status(desk, SetupLink{}) == setup_link::kNone);
    CHECK(link_status(desk, SetupLink{"/w/code.json", desk}) == setup_link::kCurrent);
    CHECK(link_status(desk, SetupLink{"/w/code.json", setup_of("Other", {panel::kPaneEditor})}) ==
          setup_link::kModified);
    // AND A DEFAULT-CONSTRUCTED KNOWN VALUE IS STRUCTURALLY UNREACHABLE AS A DESK: no
    // legal setup equals it, because `check_setup_name` refuses an empty name -- which is
    // what makes "there is no known value" structural rather than a rule somebody keeps.
    CHECK_FALSE(check_setup_name(Setup{}.name).accepted);
    CHECK(link_status(desk, SetupLink{"/w/code.json", Setup{}}) == setup_link::kModified);
}

TEST_CASE("WUX-9/SC-10: stepping wraps over the whole population, painted or not") {
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

TEST_CASE("WUX-9/SC-7: the run marks the live layout and its width does not move") {
    const SetupState first = shelf_of({"Code", "Build"}, 0);
    const SetupState second = shelf_of({"Code", "Build"}, 1);
    const LayoutTabRun a = layout_tab_run(first, 80);
    const LayoutTabRun b = layout_tab_run(second, 80);

    // THE MARKER BRACKETS THE LIVE NAME AND IS TIGHT TO IT (QR-15/SC-3), and every other
    // tab wears the same two cells as blanks -- so the run reads as a run of names.
    // ...FOLLOWED BY THE CREATE AFFORDANCE, out of what the budget had left (WUX-11).
    CHECK(a.text == ">Code< Build  +");
    CHECK(b.text == " Code >Build< +");
    // NO QUOTATION MARK ANYWHERE IN IT (QR-15/SC-2): the authored bytes, and nothing a
    // maker did not type.
    CHECK(a.text.find('"') == std::string::npos);
    CHECK(b.text.find('"') == std::string::npos);
    // ...AND NO SPACE BETWEEN A MARKER AND THE NAME IT IS ABOUT (QR-15/SC-3). The cell
    // before the live name is `>` and the cell after it is `<`, with nothing in between.
    CHECK(a.text.find("> ") == std::string::npos);
    CHECK(a.text.find(" <") == std::string::npos);
    CHECK(b.text.find("> ") == std::string::npos);
    CHECK(b.text.find(" <") == std::string::npos);
    // THE SAME WIDTH EITHER WAY (QR-15/SC-4), which is why the status to the right of it
    // does not slide sideways every time a maker switches. It is one cell, the name, one
    // cell, on both -- so the equality is the TYPE's rather than two literals' agreement
    // (`kLayoutLiveOpen` and its neighbours are `char`).
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
    // AND THE CREATE AFFORDANCE IS ONE CELL WITH A SPAN OF ITS OWN (WUX-11) -- an action,
    // never a layout: it is not in `tabs`, not counted, and not steppable.
    CHECK(a.create_columns == 1);
    CHECK(a.create_column == b.create_column);
    CHECK(a.text.substr(static_cast<std::size_t>(a.create_column), 1) ==
          std::string(1, kLayoutCreate));
}

TEST_CASE("QR-15/SC-5: a multi-word name is delimited by its own cells, not by quotes") {
    // A layout name may hold spaces, and WUX-9 quoted every name because of it. QR-15 pays
    // for that delimiter in GEOMETRY instead: every tab reserves one cell on each side of
    // its name, so the gap BETWEEN two tabs is two cells and a space INSIDE a name is one.
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
    // THE MAKER'S OWN EXAMPLE, whole: an active multi-word name between two ordinary ones.
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

TEST_CASE("WUX-9/SC-8: the visible window is derived, keeps the live tab, and marks its ends") {
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
    // THE HONEST RIGHT-HAND COUNT, asked for by its own wording rather than by the glyph:
    // since QR-15 the live tab carries a `>` of its own, so a bare search for one would
    // pass with no omission marker on the row at all.
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

    // ...AND IN THE MIDDLE BOTH ENDS ARE MARKED.
    // ⚠ THE WIDTH IS RE-DERIVED FOR QR-15's NARROWER TAB, not inherited. A one-letter
    // layout costs three cells now (` A `) where it cost five (`  "A"`), so eighteen
    // columns -- which used to leave four of the eight out -- now holds the whole run and
    // marks neither end. Twelve is where both markers are owed again.
    REQUIRE(activate_layout(s, 4));
    CHECK(layout_tab_run(s, 18).after == 0); // the width this case used to be written at
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

TEST_CASE("WUX-9/SC-8: the live tab is cut rather than dropped when even it will not fit") {
    const SetupState s = shelf_of({"short", std::string(kMaxSetupNameLen, 'z')}, 1);
    const LayoutTabRun run = layout_tab_run(s, 12);
    REQUIRE(run.tabs.size() == 1);
    CHECK(run.tabs[0].active);
    CHECK(run.tabs[0].at == 1);
    CHECK(run.before == 1);
    CHECK(static_cast<std::int64_t>(run.text.size()) <= 12);
    CHECK(run.text.find("...") != std::string::npos); // the cut is marked, never silent
}

TEST_CASE("WUX-9/SC-7: the status row is tabs on the left and the existing status right") {
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.setup = shelf_of({"Code", "Build"}, 0);
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);

    // ⭐ THE TABS, THE CREATE AFFORDANCE, THEN THE ACTIVE LAYOUT'S OWN ASSOCIATION
    // (WUX-11). The row used to say `UNSAVED | workshop-setup.json` -- one comparison for a
    // whole Workshop against one file, on a screen showing several desks.
    CHECK(row.text.rfind(">Code< Build  +", 0) == 0);
    CHECK(row.text.find("setup: none") != std::string::npos);
    // ...AND THE HOST'S CONFIGURED SETUP PATH IS NOT ON THE ROW. This layout is related to
    // no artifact, and `none` is the whole truth about it.
    CHECK(row.text.find("workshop-setup.json") == std::string::npos);
    CHECK(row.text.find("UNSAVED") == std::string::npos);
    CHECK(static_cast<std::int64_t>(row.text.size()) <= sc.w);
    // THE NAME IS SAID ONCE. The tabs carry it; the status half does not repeat it.
    CHECK(row.text.find("setup ") == std::string::npos);
    REQUIRE(row.tabs.size() == 2);
    CHECK(row.text.find("Code", static_cast<std::size_t>(row.tabs[0].column +
                                                         row.tabs[0].columns)) ==
          std::string::npos);
    // AND THE GEOMETRY DID NOT MOVE TO SEAT THEM: the band is the rows it always was.
    CHECK(band_bounds(sc).h == kBottomRows);
    CHECK(sc.room_w == kMinScreen.room_w);
    CHECK(sc.room_h == kMinScreen.room_h);

    // THE ASSOCIATION IS STILL ABOUT THE LIVE LAYOUT, and it survives a run of names long
    // enough to want the whole row.
    Session crowded = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    crowded.setup = shelf_of({std::string(20, 'a'), std::string(20, 'b'),
                              std::string(20, 'c'), std::string(20, 'd')},
                             0);
    const BandStatus tight = band_status(crowded, screen_of(crowded));
    CHECK(tight.text.find("setup: none") != std::string::npos);
    CHECK(static_cast<std::int64_t>(tight.text.size()) <= kMinScreen.w);
}

TEST_CASE("WUX-11/SC-24: the association's verdict survives the row's cut, at every width") {
    // FOUND BY THE LIVE TUI WITNESS, NOT BY A CASE (WUX-9's finding, re-earned by WUX-11's
    // longer sentence). The reservation is against the tabs, and the price it has to cover
    // includes the mark `detail::fit` spends on saying it cut the row -- a reservation that
    // stopped at the last real character leaves the verdict three cells short, and a maker
    // at the minimum extent with a full-budget run reads `| modifi...`.
    //
    // ⭐ WHAT MUST SURVIVE IS THE MEANING, NOT THE PATH. `setup:` and the verdict are what
    // distinguish `none` from `current` from `modified`; WHICH artifact is the part a narrow
    // row may elide (§9's ordering). So this sweep asserts the words and lets the path go.
    //
    // SWEPT over every name length, because the defect lives at exactly one of them: the run
    // has to be wide enough to reach its budget and no wider, which the suite's other crowded
    // case missed by three tabs.
    const std::string artifact = "/home/maker/projects/zen/layouts/workshop-setup.json";
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
                none.setup = shelf_of(names, live);
                const BandStatus bare = band_status(none, screen_of(none));
                REQUIRE(bare.text.find("setup: none") != std::string::npos);
                REQUIRE(static_cast<std::int64_t>(bare.text.size()) <= kMinScreen.w);

                // ...and the two associated verdicts, whose sentence is the longer one.
                Session live_current = none;
                live_current.setup.active_link =
                    SetupLink{artifact, live_current.setup.active};
                const BandStatus fresh = band_status(live_current, screen_of(live_current));
                REQUIRE(fresh.text.find("setup: ") != std::string::npos);
                REQUIRE(fresh.text.find("| current") != std::string::npos);
                REQUIRE(static_cast<std::int64_t>(fresh.text.size()) <= kMinScreen.w);

                Session diverged = none;
                diverged.setup.active_link = SetupLink{artifact, setup_of("other", {})};
                const BandStatus moved = band_status(diverged, screen_of(diverged));
                REQUIRE(moved.text.find("setup: ") != std::string::npos);
                REQUIRE(moved.text.find("| modified") != std::string::npos);
                REQUIRE(static_cast<std::int64_t>(moved.text.size()) <= kMinScreen.w);
            }
        }
    }
}

TEST_CASE("WUX-9/SC-9: a press answers a painted tab and nothing else on the band") {
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.setup = shelf_of({"Code", "Build", "Inspect"}, 0);
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);
    // ⚠ THE TOP BAND, because that is where QR-14 paints the run. A press resolved against
    // the bottom band's origin would be the stale-geometry lie this case exists to refuse.
    const ui::Rect b = top_band_bounds(sc);
    REQUIRE(b.y == 0);
    REQUIRE(band_tab_row(s, sc) == 0);
    REQUIRE(row.tabs.size() == 3);

    const auto press = [&](std::int64_t column, std::int64_t band_row) {
        return band_tab_at(s, sc, input::space::kCells,
                           b.x + column, b.y + band_row + surface::kTuiCanvasTopRow);
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
    // THE STATUS TO THE RIGHT OF THE RUN SELECTS NOTHING, and neither does the blank
    // beyond the end of the row.
    // ⚠ PAST THE CREATE AFFORDANCE TOO (WUX-11): `+` is the one other span the run owns,
    // and it answers `create` rather than a layout -- which is asserted here rather than
    // stepped over, because a case that landed on it by accident would read as a hole.
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
    // including every row the run used to be painted on before QR-14 moved it (the whole of
    // the bottom band), which is the stale vertical hit map this sweep exists to kill.
    for (std::int64_t r = 1; r < sc.h; ++r) {
        CAPTURE(r);
        CHECK_FALSE(press(row.tabs[1].column, r).hit);
    }
    CHECK_FALSE(press(row.tabs[1].column, -1).hit);

    // AND WHILE A MAKER IS NAMING THE SETUP THERE ARE NO TABS ON SCREEN TO PRESS.
    Session naming = s;
    naming.setup.naming.open = true;
    CHECK(band_tab_row(naming, sc) == kNoBandRow);
    CHECK_FALSE(band_tab_at(naming, sc, input::space::kCells,
                            b.x + row.tabs[1].column, b.y + surface::kTuiCanvasTopRow)
                    .hit);
}

TEST_CASE("WUX-9/SC-8+SC-9: an omitted tab has no span and cannot be pressed") {
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.setup = shelf_of({"A", "B", "C", "D", "E", "F", "G", "H"}, 0);
    // Names long enough that the reserved row cannot hold all eight.
    for (std::size_t i = 0; i < s.setup.shelved.size(); ++i) {
        s.setup.shelved[i].desk.name = std::string(12, static_cast<char>('a' + i));
    }
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);
    REQUIRE(row.after > 0);
    CHECK(row.tabs.size() + row.before + row.after == layout_count(s.setup));
    // MEASURED AT THE MINIMUM, and pinned so the number a report quotes is the suite's.
    // ⚠ RE-DERIVED FOR WUX-11, whose reservation is longer than the saved marker's was: the
    // association's words and its elision marks reserve 27 columns of 78, leaving 51 for the
    // run, where QR-15 had 65. A 3-column live tab (`>A<`), three 14-column neighbours and a
    // 4-column ` 4>` come to 49, where a fifth tab would want 63.
    CHECK(row.tabs.size() == 4);
    CHECK(row.before == 0);
    CHECK(row.after == 4);
    CHECK(row.text.find("setup: none") != std::string::npos);
    // Nothing painted claims a layout the window left out, and no press anywhere on the
    // row can reach one: the spans are exactly the painted population.
    for (const LayoutTab& tab : row.tabs) {
        CHECK(tab.at < layout_count(s.setup) - row.after);
    }
    const ui::Rect b = top_band_bounds(sc);
    for (std::int64_t c = 0; c < sc.w; ++c) {
        const LayoutTabPress hit =
            band_tab_at(s, sc, input::space::kCells, b.x + c,
                        b.y + surface::kTuiCanvasTopRow);
        if (hit.hit) {
            CAPTURE(c);
            CHECK(hit.at < layout_count(s.setup) - row.after);
        }
    }
}

TEST_CASE("WUX-9/SC-7: the tab run is one composition on both media") {
    // The band is a REGION, so a face's row holds however many characters the face fits;
    // the run is composed against that number rather than against canvas cells, and the
    // press inverse resolves through the same `prose_at` every other region press does.
    Session cells = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    cells.setup = shelf_of({"Code", "Build"}, 1);
    Session face = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    face.setup = shelf_of({"Code", "Build"}, 1);

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
    // A window pixel inside the second tab answers the second layout.
    const ui::Rect fb = top_band_bounds(screen_of(face));
    const LayoutTabPress hit =
        band_tab_at(face, screen_of(face), input::space::kPixels,
                    fb.x * surface::kCanvasCellPx + face_row.tabs[1].column * 8 + 4,
                    fb.y * surface::kCanvasCellPx + 4);
    CHECK(hit.hit);
    CHECK(hit.at == 1);
}

TEST_CASE("WUX-9/SC-8: the run never spends more columns than it was given") {
    // THE BOUND, AS A PROPERTY over every population this run can hold and every width a
    // band row can offer -- including the degenerate ones, where a name too wide for the
    // room sits between two omitted populations and both markers must still be paid for out
    // of the SAME budget. A marker appended after the budget was spent is a bound that grows
    // when it is exceeded, which is what rule 3 exists to refuse.
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
                // ...and whatever it did paint is inside what it wrote, in the maker's
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
// ---- QR-14: the layout selector is the first row of Workshop ---------------
//
// WUX-9 composed the run correctly and left it in the footer. This is the move, and what
// the move must not have disturbed: the reserved total, the body's own extent, and the
// agreement between every owner of where the body begins.

TEST_CASE("QR-14/SC-2: the layout selector is the first Workshop row, on both media") {
    for (const std::int64_t line : {std::int64_t{0}, std::int64_t{18}}) {
        CAPTURE(line);
        Session s = screen_session(kScreenMinW, kScreenMinH, line == 0 ? 0 : 8, line);
        s.setup = shelf_of({"Code", "Build"}, 0);
        const Screen sc = screen_of(s);

        // THE BAND IS AT THE CANVAS'S OWN ORIGIN, and the run is inside it.
        CHECK(top_band_bounds(sc).y == 0);
        CHECK(top_band_bounds(sc).x == 0);
        CHECK(top_band_bounds(sc).w == sc.w);
        CHECK(band_tab_row(s, sc) == 0);
        CHECK(band_status(s, sc).text.rfind(">Code<", 0) == 0);

        // ...AND NOT IN THE FOOTER. The bottom band still exists and still speaks; what it
        // does not carry any more is the identity, and no tab is painted anywhere in it.
        CHECK(band_bounds(sc).y == sc.h - kBottomRows);
        CHECK(band_bounds(sc).y > top_band_bounds(sc).y + top_band_bounds(sc).h);
        const surface::SurfaceCanvas c = paint(WorkshopDoc{}, s);
        for (const surface::SurfaceTextRegion& r : all_texts(c)) {
            if (r.y == band_bounds(sc).y) {
                for (const surface::SurfaceTextRow& row : r.rows) {
                    CAPTURE(row.text);
                    // ⚠ ASKED BY THE TAB'S OWN SPELLING since QR-15, because a bare name
                    // is an ordinary word: the foot's legend may honestly say `build`,
                    // and only `>Code<` / ` Code ` is a layout TAB.
                    CHECK(row.text.find(layout_tab_text(s.setup, 0)) == std::string::npos);
                    CHECK(row.text.find(layout_tab_text(s.setup, 1)) == std::string::npos);
                }
            }
        }
    }
}

TEST_CASE("QR-14/SC-2: the move re-homed reserved rows and did not add one") {
    // THE PROPERTY THAT MATTERS MOST, because the workspace's extent is what a share
    // resolves against: a chrome change that resized the body would resize every `%` object
    // a maker authored. Two reserved bands and no blank row between them, at every extent.
    CHECK(kTopRows + kBottomRows == 6);
    for (std::int64_t h = kScreenMinH; h <= 120; ++h) {
        CAPTURE(h);
        const Screen sc = screen_of(kScreenMinW, h);
        // The three regions tile the screen exactly: no cell reserved twice, none left over.
        CHECK(top_band_bounds(sc).y + top_band_bounds(sc).h == kWorkspaceY);
        CHECK(kWorkspaceY + sc.room_h == band_bounds(sc).y);
        CHECK(band_bounds(sc).y + band_bounds(sc).h == sc.h);
        // ...and the body is exactly the body it was before the move.
        CHECK(sc.room_h == h - 6);
    }
    // The minimum composition's own numbers are the ones every golden is written against --
    // and the width is 78 rather than 48 since the right column stopped coming off the room
    // (`the-room-is-the-screen`), which is a change to the WIDTH and left this case's own
    // subject, the two reserved BANDS, exactly where QR-14 put them.
    CHECK(kMinScreen.room_w == 78);
    CHECK(kMinScreen.room_h == 16);
}

TEST_CASE("QR-14/SC-6: every owner of the body agrees about where it begins") {
    // THE ONE-ROW DISAGREEMENT THIS REPAIR MUST NOT LEAVE BEHIND. Paint, occupancy, the
    // placement path, the overlay column, the contextual popup and the hotkey view all
    // resolve the body from the same constants -- so the question is asked of each of them
    // rather than of the constant they share.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    const Screen sc = screen_of(t.session());

    const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, sc);
    CHECK(slot.y == kWorkspaceY); // the stack begins at the body's own top
    const ui::Rect side = placement_bounds(placement::kSideRegion, 0, sc);
    CHECK(side.y == kWorkspaceY);
    CHECK(side.y + side.h == kWorkspaceY + sc.room_h);
    CHECK(surface::cell_of_subs(overlay_column(sc).y) == kWorkspaceY);

    // THE PANEL IS PAINTED WHERE IT IS HIT. Its top-left cell answers the pointer with the
    // Builder, and the cell directly above it -- the body's own first row minus one, which
    // is the reserved top row's last -- answers with the pane standing THERE.
    //
    // ⚠ IT USED TO ANSWER WITH NOTHING (WUX-12). Those two rows were the top band's, and
    // the band painted in front of every pane while occupying no pointer space at all --
    // so the boundary this case is about was a boundary between a pane and a hole. It is
    // now a boundary between two panes, which is the only version of it that can be
    // checked by one rule.
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
    CHECK(builder.y == kWorkspaceY);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, builder.x, builder.y)
              .kind == stock::kKind);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, builder.x,
                      builder.y - 1)
              .kind == panel::kLayouts);

    // AND A REAL PRESS AGREES WITH BOTH. A press on the panel's own first row selects it;
    // one row higher selects the pane that owns that row.
    t.press_canvas(builder.x + 2, builder.y);
    CHECK(t.session().panels.selected == stock::kKind);
    t.press_canvas(builder.x + 2, builder.y - 1);
    CHECK(t.session().panels.selected == panel::kLayouts);

    // THE WORKSPACE'S OWN PROJECTION IS THE SAME ONE THE POINTER INVERTS. Authored cell
    // (0,0) is canvas row `kWorkspaceY`, and the inverse of that canvas row is 0.
    CHECK(workspace_cell_y(kWorkspaceY) == 0);
    CHECK(workspace_cell_y(kWorkspaceY - 1) == -1);
}

// ============================================================================
// ---- QR-15: the selection hugs the name ------------------------------------
//
// WUX-9 spelled a tab `> "Code"` / `  "Build"` -- both marker cells on the left, and every
// authored name in quotation marks a maker never typed. QR-15 keeps the WIDTH law that
// spelling existed to protect and spends the two cells symmetrically instead, bare:
//
//     Home >Code< Art
//
// What must not have moved: the derived window, the omission counts, the reservation, the
// band's own geometry, and the agreement between what is painted and what a press answers.

TEST_CASE("QR-15/SC-2+SC-3+SC-4: every tab is one cell, the name, one cell") {
    // THE WHOLE PRESENTATION LAW AS A PROPERTY, over every population this run can hold,
    // every live position in it, and names of every length -- because each falsifier the
    // phase named is a different way for ONE of these spans to come out wrong: a quote
    // retained, only the left marker kept, a space between a marker and its name, or an
    // active tab that costs more than an inactive one.
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
                // spelling and not the degradation (that is WUX-9's own sweep, below).
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
                // ...AND NOT ONE QUOTATION MARK IN THE WHOLE RUN (SC-2).
                REQUIRE(run.text.find('"') == std::string::npos);
            }
        }
    }
}

TEST_CASE("QR-15/SC-4: switching the live layout moves nothing to the right of it") {
    // THE DEFECT THE EQUAL WIDTH EXISTS TO REFUSE, asked of the composed ROW rather than of
    // the run: a maker stepping through their layouts must not watch the `setup:` slot, its
    // verdict or the gestures slide sideways under the marker. Since WUX-11 the right-hand
    // block is also adjusted to the row's edge, so it is doubly still -- but the property
    // asserted is the one QR-15 bought: EQUAL TAB WIDTH, whichever layout is live.
    const std::vector<std::string> names{"Home", "Code", "Art"};
    std::vector<std::int64_t> status_at;
    std::vector<std::string> spans;
    for (std::size_t live = 0; live < names.size(); ++live) {
        CAPTURE(live);
        Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
        s.setup = shelf_of(names, live);
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
        // AND THE RUN READS AS THE MAKER'S OWN SENTENCE.
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

TEST_CASE("QR-15/SC-7: the closing marker belongs to the layout it closes") {
    // THE POINTER FALSIFIER THE PHASE NAMED. `<` is the last cell of the ACTIVE tab and
    // the cell after it is the first of its neighbour; an inverse that handed either one
    // to the wrong layout would be a press that disagrees with the paint (HD-3).
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.setup = shelf_of({"Home", "Code", "Art"}, 1);
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);
    REQUIRE(row.tabs.size() == 3);
    REQUIRE(row.tabs[1].active);
    CHECK(row.text.substr(static_cast<std::size_t>(row.tabs[1].column),
                          static_cast<std::size_t>(row.tabs[1].columns)) == ">Code<");

    const ui::Rect b = top_band_bounds(sc);
    const auto press = [&](std::int64_t column) {
        return band_tab_at(s, sc, input::space::kCells, b.x + column,
                           b.y + surface::kTuiCanvasTopRow);
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
    // AND THE OLD QUOTE POSITIONS MEAN NOTHING: there are none, anywhere on the row.
    CHECK(row.text.find('"') == std::string::npos);
}

TEST_CASE("QR-15: the maker reads `Home >Code< Art` on Workshop's first row") {
    // THE COMPLETION SENTENCE, through the real rasterizer rather than the composition --
    // the bytes a maker's terminal actually receives, on the row QR-14 put the selector on.
    for (const std::size_t live : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
        CAPTURE(live);
        Session s;
        s.setup = shelf_of({"Home", "Code", "Art"}, live);
        const std::vector<std::string> rows = rasterized(paint(WorkshopDoc{}, s));
        REQUIRE(rows.size() == static_cast<std::size_t>(kMinScreen.h));
        CHECK(rows[0].rfind(live == 0   ? ">Home< Code  Art "
                            : live == 1 ? " Home >Code< Art "
                                        : " Home  Code >Art<",
                            0) == 0);
        CHECK(rows[0].find('"') == std::string::npos);
    }
    // A MULTI-WORD NAME IS STILL ONE TAB, and reads as one without a quotation mark.
    Session wordy;
    wordy.setup = shelf_of({"Home", "My Layout", "Art"}, 1);
    const std::vector<std::string> rows = rasterized(paint(WorkshopDoc{}, wordy));
    CHECK(rows[0].rfind(" Home >My Layout< Art ", 0) == 0);
}

TEST_CASE("QR-14/SC-5: no press outside the painted run reaches a layout") {
    // THE STALE HIT MAP, SWEPT. Every cell of the screen is offered to the tab inverse, and
    // the only ones that answer are cells of a tab the composition actually painted -- which
    // after the move are on row 0 and nowhere else.
    Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    s.setup = shelf_of({"Code", "Build", "Inspect"}, 1);
    const Screen sc = screen_of(s);
    const BandStatus row = band_status(s, sc);
    REQUIRE(row.tabs.size() == 3);

    std::size_t answered = 0;
    for (std::int64_t y = 0; y < sc.h; ++y) {
        for (std::int64_t x = 0; x < sc.w; ++x) {
            const LayoutTabPress hit =
                band_tab_at(s, sc, input::space::kCells, x,
                            y + surface::kTuiCanvasTopRow);
            if (!hit.hit) {
                continue;
            }
            CAPTURE(x);
            CAPTURE(y);
            CHECK(y == 0); // the first Workshop row, and no other
            // ...OR THE CREATE AFFORDANCE, which is the run's one other span (WUX-11) and
            // answers an ACTION rather than a layout.
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
// ---- WUX-12: the layout surface is an ordinary pane -------------------------
//
// WHAT THIS SECTION IS ABOUT. Until WUX-12 the layout tab run, the active layout's Setup
// association and the workspace fact were painted into two reserved rows by `paint` itself,
// out of a rectangle nothing could name: no catalog row, no setup row, no place in
// `occupied_at`, no coverage, no persistence, and two bespoke global pointer arms that
// answered ABOVE every pane. They are one built-in pane now -- `panel::kLayouts` -- and
// every one of those sentences is the ordinary one. The cases below are about the seams
// that changed, not about the composition, which WUX-9/WUX-11 already pin above.
// ============================================================================

TEST_CASE("WUX-12/SC-2: the Layouts pane's developer default IS the historical rectangle") {
    // ⭐ THE EQUIVALENCE CLAIM, ON BOTH SHIPPED FACES. A maker who never authors anything
    // must see the layout run exactly where the band put it -- so the pane's default
    // rectangle is asked of the one place resolver and compared against the reservation
    // itself, rather than against a transcription of two numbers.
    for (const auto& metric : {std::pair<std::int64_t, std::int64_t>{0, 0},
                               std::pair<std::int64_t, std::int64_t>{8, 18}}) {
        CAPTURE(metric.first);
        Session s = screen_session(kScreenMinW, kScreenMinH, metric.first, metric.second);
        WorkshopDoc empty;
        refocus(empty, s);
        const Screen sc = screen_of(s);
        const PanelBounds where =
            bounds_of(s.panels, s.setup.active, panel::kLayouts, sc);
        REQUIRE(where.open);
        CHECK(where.placed_in == placement::kTopBand);
        // THE OUTER RECTANGLE IS THE RESERVATION'S, to the sub-unit.
        CHECK(where.rect == fine_of_cells(top_band_bounds(sc)));
        CHECK(cells_covered(where.rect) == ui::Rect{0, 0, sc.w, kTopRows});
    }
}

TEST_CASE("WUX-12/SC-2: a two-cell pane keeps its content and drops its boundary") {
    // ⭐ THE ONE THING THE CONVERSION COST, AND WHERE IT IS PAID. A pane's chrome is one
    // unit of the active face on every side; the historical rectangle is two canvas rows,
    // and on a character medium one cell a side leaves ZERO rows of interior -- a pane that
    // draws a boundary and nothing else, which is a pane that has stopped presenting.
    // `pane_inside`'s last candidate is therefore no boundary at all, and this is the
    // measurement: the terminal pays nothing and keeps both rows; the shipped face pays one
    // device pixel and keeps its one row of type.
    //
    // ⚔ MUTATION: dropping that candidate. The terminal's `rows` goes to 0, the tab run
    // stops being composed at all, and `band_status(...).text` is empty.
    Session cells = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    WorkshopDoc no_document;
    refocus(no_document, cells);
    const PaneInside on_cells =
        pane_inside(fine_of_cells(top_band_bounds(screen_of(cells))), screen_of(cells));
    CHECK(on_cells.chrome_subs == 0);
    CHECK(on_cells.fit.rows == kTopRows);
    CHECK(layouts_body(cells, screen_of(cells)).rows == kTopRows);
    CHECK_FALSE(band_status(cells, screen_of(cells)).text.empty());

    // ⚠ THE SHIPPED FACE REPORTS ITS CELL SIZE AS WELL AS ITS TYPE (WUX-6). `cell_px` is
    // what `chrome_grain` spends: a medium that does not publish one can show nothing
    // thinner than a cell, which is the terminal's answer above.
    Session face =
        screen_session(kScreenMinW, kScreenMinH, 8, 18, surface::kCanvasCellPx);
    refocus(no_document, face);
    const PaneInside on_face =
        pane_inside(fine_of_cells(top_band_bounds(screen_of(face))), screen_of(face));
    CHECK(on_face.chrome_subs == surface::subs_of_one_device(screen_of(face).cell_px));
    CHECK(on_face.chrome_subs > 0);
    CHECK(layouts_body(face, screen_of(face)).rows == 1);
    CHECK_FALSE(band_status(face, screen_of(face)).text.empty());

    // ...AND A PANE WITH ROOM FOR A BOUNDARY STILL PAYS ONE, on the same face. The rung is
    // a floor for a rectangle that cannot hold an edge, not a repeal of the edge.
    const PaneInside roomy =
        pane_inside(fine_of_cells(ui::Rect{0, 0, 40, 9}), screen_of(cells));
    CHECK(roomy.chrome_subs == kChromeSubs);
}

TEST_CASE("WUX-12/SC-3: authored geometry moves the Layouts pane, and the tabs with it") {
    // ⭐ THE POINT OF THE CONVERSION, in one case: place and size are the maker's, through
    // the SAME doors every other pane's geometry goes through -- and paint, the press
    // inverse and occupancy all follow, because there is one resolution.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{120, 40, 0, 0}));
    const PaneRef layouts = ref_of(panel::kLayouts);
    const Screen sc = screen_of(t.session());
    REQUIRE(bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc)
                .rect == fine_of_cells(top_band_bounds(sc)));

    REQUIRE(author_pane_place(live(t).setup.active, layouts, subs(10), subs(20)).accepted);
    REQUIRE(author_pane_size(live(t).setup.active, layouts,
                             PaneSize{pane_unit::kSubcells, subs(40)},
                             PaneSize{pane_unit::kSubcells, subs(4)})
                .accepted);
    const ui::Rect moved = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc).rect);
    CHECK(moved == ui::Rect{10, 20, 40, 4});

    // THE PRESS INVERSE FOLLOWED IT. A tab press at the pane's new first interior row lands
    // on a tab; the row the band used to own answers nothing at all.
    const ExternalBodyPlace body = layouts_body(t.session(), sc);
    REQUIRE(body.present);
    // ...AND THIS ONE IS TALL ENOUGH TO WEAR A BOUNDARY, which is the other half of the
    // chrome rung: four rows less one cell a side leaves two, so the edge is drawn and the
    // interior begins one row inside the rectangle the maker authored.
    CHECK(body.region_y == 21);
    const BandStatus row = band_status(t.session(), sc);
    REQUIRE_FALSE(row.tabs.empty());
    CHECK(band_tab_at(t.session(), sc, input::space::kCells,
                      body.region_x + row.tabs[0].column,
                      body.region_y + surface::kTuiCanvasTopRow)
              .hit);
    CHECK_FALSE(band_tab_at(t.session(), sc, input::space::kCells,
                            row.tabs[0].column,
                            surface::kTuiCanvasTopRow)
                    .hit);

    // AND OCCUPANCY FOLLOWED IT TOO: the pane is where it was authored and nowhere else.
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, 12, 21).kind ==
          panel::kLayouts);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, 12, 0).occupied);
}

TEST_CASE("WUX-12/SC-5+SC-7: a pane in front of the Layouts pane takes the press") {
    // ⭐ THE FALSIFIER THE PHASE EXISTS FOR. Before the conversion the band painted in
    // FRONT of every pane and answered presses on the tabs alone, so a pane a maker had
    // authored over the tab run was drawn over and still lost the press to it: see-here,
    // press-there, at the one boundary HD-3 forbids. Both halves are gone.
    //
    // ⚔ MUTATION: restoring the old global arm -- asking `band_tab_at` before the occupancy
    // walk. The Builder stops being selected, `switch_layout` runs, and both checks below
    // move.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{120, 40, 0, 0}));
    const LayoutKeys k = layout_keys(t);
    press_gesture(t, k.make); // two layouts, so switching is observable
    REQUIRE(layout_count(t.session().setup) == 2);
    const std::size_t live_at = t.session().setup.active_at;

    open_pane(t, ref_of(stock::kKind));
    // Author the Builder OVER the tab run, covering it whole. It is already front-most --
    // the picker appends the front-most rank -- which is exactly the arrangement a maker
    // gets by opening a pane and dragging it up there, and is why `send_to_front` would
    // answer "already".
    const Screen sc = screen_of(t.session());
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind), 0, 0).accepted);
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(stock::kKind),
                             PaneSize{pane_unit::kSubcells, surface::subs_of_cells(sc.w)},
                             PaneSize{pane_unit::kSubcells, surface::subs_of_cells(kTopRows)})
                .accepted);
    REQUIRE(index_of(authored_order(t.session()), stock::kKind) >
            index_of(authored_order(t.session()), panel::kLayouts));

    // THE POINT IS THE BUILDER'S, on the effective order the paint walk spends.
    const BandStatus row = band_status(t.session(), sc);
    REQUIRE_FALSE(row.tabs.empty());
    const std::int64_t at_x = row.tabs.front().column;
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, at_x, 0).kind ==
          stock::kKind);
    // ...AND SO IS THE PRESS. The layout does not switch and the Builder is what the maker
    // is pointing at.
    t.press_canvas(at_x, 0);
    CHECK(t.session().panels.selected == stock::kKind);
    CHECK(t.session().setup.active_at == live_at);

    // AND THE LAYOUTS PANE KNOWS IT IS COVERED, which is a fact it could not have before:
    // coverage counts panes, and the band was not one.
    CHECK(pane_state_of(t.session().panels, t.session().setup.active, sc,
                        CatalogRow{panel::kLayouts, ref_of(panel::kLayouts), "Layouts", ""}) ==
          pane_state::kCovered);

    // THE OTHER DIRECTION, from the same desk: send the Layouts pane to the front and the
    // same point is its again -- and the press switches the layout.
    //
    // ⚠ THE STANDING SELECTION IS CLEARED FIRST, because it is a real part of the answer:
    // `effective_pane_order` lifts the pane a maker is pointing at, so a desk whose front
    // rank says Layouts still answers Builder while the Builder is the selected one. The
    // press below is about the AUTHORED order, so the case puts the selection back where a
    // fresh press would find it rather than measuring a lift it did not mean.
    // ⚠ JUST LEFT OF THE RIGHT COLUMN, NOT AT THE ROOM'S EDGE. The room is the surface now,
    // so the room's last column is under the pane standing in that place and is not bare.
    t.press_canvas(sc.panel_x - 1, sc.h - kBottomRows - 1); // bare workspace: selects nothing
    REQUIRE(t.session().panels.selected == kNoPaneKind);
    REQUIRE(send_to_front(live(t).setup.active, ref_of(panel::kLayouts)));
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, at_x, 0).kind ==
          panel::kLayouts);
    t.press_canvas(at_x, 0);
    CHECK(t.session().panels.selected == panel::kLayouts);
    CHECK(t.session().setup.active_at == row.tabs.front().at);
}

TEST_CASE("WUX-12/SC-9: the reservation does not follow the Layouts pane") {
    // ⭐ THE FIXED-RESERVATION PROOF, and it is a claim about the DOCUMENT rather than
    // about chrome. `room_w`/`room_h` are what a `%`-sized object resolves against (PNL-0),
    // so a maker who moves, resizes or removes the layout surface must not watch their
    // material change size. `screen_of` cannot see a pane and this says so.
    //
    // ⚔ MUTATION: making `screen_of` subtract `kTopRows` only when the pane participates.
    // Every comparison below moves, and so does the resolved document rectangle.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    const Screen before = screen_of(t.session());
    const std::int64_t doc_w = t.session().workspace_w;
    const std::int64_t doc_h = t.session().workspace_h;

    const PaneRef layouts = ref_of(panel::kLayouts);
    // MOVED, RESIZED, AND THEN REMOVED ALTOGETHER -- three presentation changes, one
    // unchanged basis.
    REQUIRE(author_pane_place(live(t).setup.active, layouts, subs(20), subs(30)).accepted);
    CHECK(screen_of(t.session()).room_w == before.room_w);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    REQUIRE(author_pane_size(live(t).setup.active, layouts,
                             PaneSize{pane_unit::kSubcells, subs(9)},
                             PaneSize{pane_unit::kSubcells, subs(9)})
                .accepted);
    CHECK(screen_of(t.session()).room_w == before.room_w);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    pick(t, panel::kLayouts);
    REQUIRE_FALSE(t.session().panels.has(panel::kLayouts));
    CHECK(screen_of(t.session()).room_w == before.room_w);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    CHECK(screen_of(t.session()).notice_y == before.notice_y);
    CHECK(surface::cell_of_subs(overlay_column(screen_of(t.session())).y) == kWorkspaceY);
    // ...and what the DOCUMENT resolves against did not move either.
    CHECK(t.session().workspace_w == doc_w);
    CHECK(t.session().workspace_h == doc_h);
}

TEST_CASE("WUX-12/SC-10: removing the Layouts pane strands nobody") {
    // ⭐ THE RECOVERY CLAIM. A pane a maker can remove is a pane a maker can lose, and the
    // answer is the one that already exists: the picker lists it (the catalog is the
    // picker's population), the keyboard's layout gestures never went through it, and the
    // desk reset brings the default back. No new recovery framework.
    Live t;
    t.host.setup_path = "workshop-setup.json";
    const LayoutKeys k = layout_keys(t);
    pick(t, panel::kLayouts);
    REQUIRE_FALSE(t.session().panels.has(panel::kLayouts));
    const Screen sc = screen_of(t.session());
    // NOTHING IS PAINTED AND NOTHING IS PRESSABLE, honestly.
    CHECK(band_status(t.session(), sc).text.empty());
    CHECK(band_tab_row(t.session(), sc) == kNoBandRow);
    CHECK_FALSE(band_tab_at(t.session(), sc, input::space::kCells,
                            0, surface::kTuiCanvasTopRow)
                    .hit);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, 0, 0).occupied);

    // ...AND THE KEYS STILL REACH EVERY LAYOUT, because they never went through the
    // presentation. A maker with no tab run still makes, names and steps between desks.
    press_gesture(t, k.make);
    CHECK(layout_count(t.session().setup) == 2);
    press_gesture(t, k.next);
    CHECK(t.session().setup.active_at == 0);

    // AND THE PICKER BRINGS IT BACK, at the developer default it was born with.
    open_pane(t, ref_of(panel::kLayouts));
    CHECK(t.session().panels.has(panel::kLayouts));
    CHECK(bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts,
                    screen_of(t.session()))
              .rect == fine_of_cells(top_band_bounds(screen_of(t.session()))));
    CHECK_FALSE(band_status(t.session(), screen_of(t.session())).text.empty());
}
