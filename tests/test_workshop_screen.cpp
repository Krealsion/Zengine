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

// ============================================================================
// Tier 3 — the whole screen, as a value
// ============================================================================

TEST_CASE("the screen shows the selected object, ringed, listed, and inspected") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);

    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(c.width == kMinScreen.w);
    CHECK(c.height == kMinScreen.h);

    // The workspace, then the selected object's ring UNDER its fill, then the
    // unselected object with no ring.
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, kWorkspaceW, kWorkspaceH, surface::role::kMuted));
    // #1 is authored at (3,2) as 60% x 6 -- 60% of 48 is 28 cells.
    CHECK(has_rect(c, 3, kWorkspaceY + 2, 28, 6, surface::role::kFill));
    CHECK(has_rect(c, 2, kWorkspaceY + 1, 30, 8, surface::role::kAccent)); // the ring
    // #2 is authored at (6,10) as 14 x 4, and is NOT selected.
    CHECK(has_rect(c, 6, kWorkspaceY + 10, 14, 4, surface::role::kFill));
    CHECK_FALSE(has_rect(c, 5, kWorkspaceY + 9, 16, 6, surface::role::kAccent));

    // The ring is drawn BEFORE the fill, so it reads as a ring rather than as a
    // border the object grew.
    std::size_t ring = 0;
    std::size_t fill = 0;
    const std::vector<surface::SurfaceRect> rects = all_rects(c);
    for (std::size_t i = 0; i < rects.size(); ++i) {
        if (rects[i].role == surface::role::kAccent) {
            ring = i;
        }
        if (rects[i].role == surface::role::kFill && rects[i].w == 28) {
            fill = i;
        }
    }
    CHECK(ring < fill);

    // The object list names the SAME objects by identity, and marks the same
    // selection the ring does.
    CHECK(object_row(c, d, s, 0) == "> #1 panel");
    CHECK(object_row(c, d, s, 1) == "  #2 panel");

    // The inspector reads the real object's real properties. The cursor sits on
    // the first row a maker can author, which is Name, not Identity.
    CHECK(s.cursor == 1);
    CHECK(property_row(c, d, s, 0) == " Identity #1");
    CHECK(property_row(c, d, s, 1) == ">Name     panel");
    CHECK(property_row(c, d, s, 5) == " Width    60%");
    CHECK(property_row(c, d, s, 7) == " Resolved 28 x 6 cells");
}

TEST_CASE("selecting the other object moves the ring, the list marker, and the inspector") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[1].id;
    refocus(d, s);

    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(has_rect(c, 5, kWorkspaceY + 9, 16, 6, surface::role::kAccent));
    CHECK_FALSE(has_rect(c, 2, kWorkspaceY + 1, 30, 8, surface::role::kAccent));
    CHECK(object_row(c, d, s, 0) == "  #1 panel");
    CHECK(object_row(c, d, s, 1) == "> #2 panel");
    CHECK(property_row(c, d, s, 0) == " Identity #2");
    CHECK(property_row(c, d, s, 5) == " Width    14");
    // #2's width is authored in CELLS, so its authored and resolved facts
    // coincide -- which the inspector still reports as two rows, because
    // coinciding is not the same as being one fact.
    CHECK(property_row(c, d, s, 7) == " Resolved 14 x 4 cells");
}

TEST_CASE("a live draft is visible AS a draft, and a refusal reaches the screen") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);

    s.cursor = 5; // Width
    s.rows[5].begin();
    while (!s.rows[5].draft().empty()) {
        s.rows[5].backspace();
    }
    type_all(s.rows[5], "500p");

    const surface::SurfaceCanvas drafting = paint(d, s);
    // The row carries a caret and the alert role: a draft cannot be mistaken for a committed
    // value on the screen either.
    //
    // SINCE HD-6 THE WHOLE BODY IS ONE BOUNDED REGION and a property row is one of its rows,
    // mark and name included -- HD-5 had the mark and the name as an ordinary label beside a
    // one-cell region carrying only the value, which is the shape that could not hold a line
    // of this repository's face. What a maker SEES is unchanged: `cell_text_of` runs the real
    // cell projection, which inserts the caret glyph at the caret's own column.
    CHECK(property_row(drafting, d, s, 5) == ">Width    500p_");
    const InfoBodyPlace place = body_of(d, s);
    REQUIRE(place.present);
    // HD-7: the property list begins one row under `PROPERTIES`, which begins one row under
    // the object list -- so this is resolved and never `kRowsY + 5`.
    CHECK(prose_row_of_property(place, 5) == place.heading_row + 1 + 5);
    for (const surface::SurfaceLabel& l : cell_text_of(drafting)) {
        if (l.x == place.region_x &&
            l.y == place.region_y + kInfoHeadingRows + prose_row_of_property(place, 5)) {
            CHECK(l.role == surface::role::kAlert);
        }
    }
    // And the resolved row still reports the COMMITTED width, not the draft.
    CHECK(property_row(drafting, d, s, 7) == " Resolved 28 x 6 cells");

    CHECK(s.rows[5].commit() == Commit::Refused);
    s.notice = "Width: " + s.rows[5].refusal();
    s.notice_is_bad = true;

    const surface::SurfaceCanvas refused = paint(d, s);
    // THE NOTICE IS A BOUNDED REGION SINCE TYPE-0, so it is read the way every other
    // region is -- through the real cell projection, whose padding this trims exactly as
    // `inspector_row` does for the Info body.
    CHECK(notice_line(refused, kMinScreen) == "Width: a share is 1% to 100%");
    for (const surface::SurfaceLabel& l : cell_text_of(refused)) {
        if (l.y == kMinScreen.notice_y) {
            CHECK(l.role == surface::role::kAlert);
        }
    }
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentPercent, 60}); // nothing was written
}

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

TEST_CASE("the whole screen survives an empty document, and SAYS it is empty") {
    WorkshopDoc d;
    Session s;
    refocus(d, s);
    CHECK(s.rows.empty()); // nothing selected, so there are no properties to show

    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(c.width == kMinScreen.w);
    // Nothing is INVENTED here: no row, no object, no identity. And emptiness
    // ANNOUNCES itself, because a maker can reach this state by deleting their
    // own work -- and a panel that merely goes blank is indistinguishable from a
    // tool that has broken.
    CHECK(object_row(c, d, s, 0) == "(none) -- n makes one");
    CHECK(property_row(c, d, s, 0) == "(nothing selected)");
    // The workspace and the two headings are still there: an empty document is a
    // document, and the tool does not disappear with it.
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, kWorkspaceW, kWorkspaceH, surface::role::kMuted));
    CHECK(inspector_row(c, kMinScreen.panel_x, kSideY) == "OBJECTS");
    CHECK(properties_heading(c, d, s) == "PROPERTIES");
    // And nothing was authored to make the message: the document is still empty.
    CHECK(d.elements.empty());
}

TEST_CASE("the screen a maker actually sees: one canvas, through the real rasterizer") {
    // The end-to-end shape of the thing, in one assertion a human can read: the
    // canvas paint() produced, rasterized by the terminal medium's own pure
    // function. Not a golden byte-for-byte pin (the layout is meant to move);
    // what it pins is that the pieces MEET -- a ring around a rectangle, an
    // object list, and an inspector, all on one surface.
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);

    const std::vector<std::string> rows = rasterized(paint(d, s));
    REQUIRE(rows.size() == static_cast<std::size_t>(kMinScreen.h));
    // The workspace fact is the band's own row since WUX-1 -- the shared top row is retired.
    CHECK(rows[static_cast<std::size_t>(kMinScreen.notice_y + 1)].rfind("workspace 48x16 cells",
                                                                        0) == 0);
    // The ring's top edge, above the selected rectangle.
    CHECK(rows[2].rfind("..**", 0) == 0);
    // The rectangle itself, with its name written on it and the ring beside it.
    CHECK(rows[3].rfind("..*panel", 0) == 0);
    // The object list and the inspector, on the same surface, to the right: the
    // heading shares row 0 with the workspace heading, and the two objects sit on
    // the rows beside the workspace's own top edge.
    CHECK(rows[0].find("OBJECTS") != std::string::npos);
    CHECK(rows[1].find("> #1 panel") != std::string::npos);
    CHECK(rows[2].find("  #2 panel") != std::string::npos);
    // The inspector begins under `PROPERTIES`, which begins under the object list -- two
    // objects here, so it is four rows down rather than the eight `kRowsY` used to name.
    const InfoBodyPlace shown_at = body_of(d, s);
    const auto at = [&](std::size_t property) {
        return static_cast<std::size_t>(shown_at.region_y + kInfoHeadingRows +
                                        prose_row_of_property(shown_at, property));
    };
    CHECK(at(0) == 4);
    CHECK(rows[at(0)].find("Identity #1") != std::string::npos);
    CHECK(rows[at(5)].find("Width    60%") != std::string::npos);
    CHECK(rows[at(7)].find("Resolved 28 x 6 cells") != std::string::npos);
}

// ----------------------------------------------------------------------------
// Tier 3b — a document larger than the panel, and a notice longer than its line
// ----------------------------------------------------------------------------
//
// Both defects here are the same one said twice: a bounded presentation that
// omits something and does not say so. Neither had a case, and neither could
// have: nothing in this suite had ever painted more than five objects, and
// nothing had ever painted a notice the screen could not hold. Every case below
// fails on the code that shipped before the bound and its marker existed.

TEST_CASE("a document that FITS the object list is shown whole, with no chrome at all") {
    // The simple case must stay the simple case. Five objects and eight properties both fit
    // the minimum screen's body with room to spare, and that is the size most likely to grow
    // accidental furniture when a bigger one gets some.
    WorkshopDoc d = many(5);
    Session s;
    s.selected = d.elements.back().id; // the LAST one -- the row that used to lose its marker
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s), d, s);
    REQUIRE(lines.size() == 5); // HD-7: its share is what it ASKED for, not a constant
    CHECK(lines[0] == "  #1 panel");
    CHECK(lines[1] == "  #2 panel");
    CHECK(lines[2] == "  #3 panel");
    CHECK(lines[3] == "  #4 panel");
    CHECK(lines[4] == "> #5 panel");
    for (const std::string& line : lines) {
        CHECK(line.find("...") == std::string::npos); // nothing was left out, so nothing says so
    }
}

TEST_CASE("an object past the list's share cannot vanish: it says what it left out") {
    // The exact reproduction of the defect this bound was built for -- a list that stopped and
    // said nothing -- at the population that reaches it now.
    //
    // HD-7 MOVED THE NUMBER AND NOT THE RULE. The list's share was `kListRows = 5` at every
    // extent; it is now what the room affords. HD-8 moved it once more and for the same kind
    // of reason: the footer's two control rows come off the budget before either list is
    // offered anything, so the minimum screen's share is six rather than seven. The rule is
    // untouched -- a list that cannot show everything says what it left out.
    WorkshopDoc d = many(9);
    Session s;
    s.selected = d.elements.back().id; // #9
    refocus(d, s);

    const surface::SurfaceCanvas c = paint(d, s);
    const std::vector<std::string> lines = object_lines(c, d, s);
    REQUIRE(lines.size() == 6);
    CHECK(lines[0] == "... 4 earlier");
    CHECK(lines[1] == "  #5 panel");
    CHECK(lines[4] == "  #8 panel");
    CHECK(lines[5] == "> #9 panel");

    // The three on-screen statements that used to disagree. Before the bound existed the
    // panel showed the first five with the marker on NONE of them, while the object count and
    // the inspector both said the last -- and the only one a maker could act on was the one
    // that was wrong.
    CHECK(d.elements.size() == 9);
    CHECK(property_row(c, d, s, 0) == " Identity #9");

    // The marker is the panel's own furniture: muted, like `(none) -- n makes one`, and it
    // mints no identity and invents no name. It is a ROW of the body since HD-7, so its role
    // is read off the region rather than off a canvas label -- the body's first row, under
    // the `OBJECTS` heading the region carries since WUX-1.
    const InfoBodyPlace place = body_of(d, s);
    const surface::SurfaceTextRegion* body = body_on(c, place);
    REQUIRE(body != nullptr);
    CHECK(body->rows[kInfoHeadingRows].role == surface::role::kMuted);
}

TEST_CASE("a selection in the middle of a long document names BOTH walls") {
    // A window that can sit in the middle can be silent on two sides, and silence on either
    // is the same defect. 6 + 5 shown + 9 accounts for all twenty, with nothing counted twice.
    //
    // A MIDDLE WINDOW NEEDS A DOCUMENT MORE THAN TWICE THE SHARE, which is another number
    // HD-7 moved and HD-8 moved again: at a five-row list nine objects reached it, at a
    // seven-row one it took more than twelve, and at HD-8's six-row share it takes more than
    // ten. 7 + 4 shown + 9 accounts for all twenty, with nothing counted twice.
    WorkshopDoc d = many(20);
    Session s;
    s.selected = 11;
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s), d, s);
    REQUIRE(lines.size() == 6);
    CHECK(lines[0] == "... 7 earlier");
    CHECK(lines[1] == "  #8 panel");
    CHECK(lines[4] == "> #11 panel");
    CHECK(lines[5] == "... 9 more");
}

TEST_CASE("a selection near the end keeps the document's tail visible") {
    // The second witness: nine objects with #8 selected drew five OTHER objects and no marker
    // at all.
    WorkshopDoc d = many(9);
    Session s;
    s.selected = 8;
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s), d, s);
    REQUIRE(lines.size() == 6);
    CHECK(lines[0] == "... 4 earlier");
    CHECK(lines[1] == "  #5 panel");
    CHECK(lines[4] == "> #8 panel");
    CHECK(lines[5] == "  #9 panel");
}

TEST_CASE("the visible window is a RUN of document order, never a reordering of it") {
    // A document whose identities are in no order at all, because document order
    // is the vector's order and nothing else -- the same fact the paint, hit and
    // list cases pin for a document that fits, said about one that does not.
    WorkshopDoc d;
    WorkshopDoc candidate;
    candidate.next_id = 91;
    for (const std::int64_t id : {90, 10, 70, 20, 60, 30, 50, 40, 80}) {
        candidate.elements.push_back(ui::Element{id, "panel", ui::kRootContext, 0, 0,
                                                 ui::Extent{ui::kExtentCells, 2},
                                                 ui::Extent{ui::kExtentCells, 1}});
    }
    REQUIRE(doc::restore(d, candidate).accepted);

    Session s;
    s.selected = 40; // the EIGHTH in the file, and neither eighth nor last by identity
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s), d, s);
    REQUIRE(lines.size() == 6);
    CHECK(lines[0] == "... 4 earlier");
    CHECK(lines[1] == "  #60 panel");
    CHECK(lines[2] == "  #30 panel");
    CHECK(lines[3] == "  #50 panel");
    CHECK(lines[4] == "> #40 panel");
    CHECK(lines[5] == "  #80 panel");
    // Sorted by identity the window would have run #30 #40 #50 #60 #70; sorted by
    // anything else it would have begun somewhere else again. It runs the way
    // the file runs.
}

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

TEST_CASE("a truncated notice still fits the terminal, through the real rasterizer") {
    // Not a string algorithm: the canvas a Skin actually rasterizes. What the
    // canvas does with an overlong label is DROP the cells past its edge and say
    // nothing -- which is precisely why the mark has to be put on before the
    // canvas ever sees it.
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);
    s.notice = std::string(200, 'x') + "END";
    s.notice_is_bad = true;

    const std::string body = surface::canvas_body(paint(d, s));
    std::vector<std::string> rows;
    std::size_t at = 0;
    while (at < body.size()) {
        const std::size_t end = body.find("\r\n", at);
        if (end == std::string::npos) {
            break;
        }
        std::string row;
        for (std::size_t i = at; i < end; ++i) {
            if (body[i] == '\x1b') { // skip the SGR/erase escapes; keep the picture
                while (i < end && body[i] != 'm' && body[i] != 'K') {
                    ++i;
                }
                continue;
            }
            row += body[i];
        }
        rows.push_back(row);
        at = end + 2;
    }

    REQUIRE(rows.size() == static_cast<std::size_t>(kMinScreen.h));
    const std::string line = rows[static_cast<std::size_t>(kMinScreen.notice_y)];
    CHECK(line.size() == static_cast<std::size_t>(kMinScreen.w)); // the row is the screen's width
    CHECK(line.compare(line.size() - 3, 3, "...") == 0);      // and it ENDS by saying so
    CHECK(line.find("END") == std::string::npos);             // the tail really is gone
    CHECK(s.notice.find("END") != std::string::npos);         // and Workshop still has it
}

// ============================================================================
// Tier 7 — composition: one authored object read in another's frame
// ============================================================================
//
// Everything above resolves against one workspace. This tier is the phase's
// whole subject, and it is arranged around the four things that could have gone
// wrong and did not:
//
//   1. THE AUTHORED VALUES STAY AUTHORED. Moving or resizing a source changes
//      what a dependent RESOLVES to and rewrites nothing it was authored with.
//      That is the authored/resolved split meeting composition, which is the
//      first real test it has had.
//   2. THE HAND PROJECTS THROUGH THE CONTEXT. A drag speaks workspace cells and
//      a dependent's position is authored in its source's frame, so the gesture
//      subtracts the frame's origin -- asked of the resolver, never
//      reconstructed -- and a Percent resize asks for a share of the SOURCE's
//      span, through the same projection a root resize uses.
//   3. THE RELATIONSHIP IS AN IDENTITY. It survives reordering, deletion of
//      other objects, save, process death and load, and it is refused when it
//      names nothing or comes back around.
//   4. THE ORDINARY CASE DID NOT GET MORE EXPENSIVE. Every case above this
//      comment is a flat document, unchanged, and not one of them mentions a
//      context.

namespace {

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

/// Open an inspector row and REPLACE its draft, the way a maker retypes a value
/// -- `begin` seeds the draft with the committed one, so typing alone appends.
void retype(Live& t, const std::string& label, const std::string& text) {
    t.begin_editing(label);
    const Row* row = t.row(label);
    REQUIRE(row != nullptr);
    const std::size_t had = row->draft().size();
    for (std::size_t i = 0; i < had; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text(text);
    t.key(input::scan::kReturn);
}

} // namespace

// ---- Authoring the relationship ---------------------------------------------

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
    CHECK(rect_of(d, s, 1).w == 24); // 50% of a 48-cell workspace
    CHECK(rect_of(d, s, 2).w == 12); // 50% of that
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
    CHECK(rect_of(d, wide, 1).w == 24);
    CHECK(rect_of(d, wide, 2).w == 12);
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
    const surface::SurfaceCanvas canvas = paint(d, s);
    CHECK(object_row(canvas, d, s, 0) == "  #" + std::to_string(c) + " C");
    CHECK(object_row(canvas, d, s, 1) == "> #" + std::to_string(a) + " A");
    CHECK(object_row(canvas, d, s, 2) == "  #" + std::to_string(b) + " B");

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
    CHECK(rect_of(loaded, wide, 1).w == 24);
    CHECK(rect_of(loaded, wide, 2).w == 12);
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

TEST_CASE("a maker authors a context in the inspector, and the picture follows") {
    Live t; // the opening document: #1 and #2, both measured against the root
    REQUIRE(t.doc().elements.size() == 2);
    CHECK(t.doc().elements[1].context == ui::kRootContext);

    // Select #2 and read its Context row: `root`, before anything is authored.
    t.key(input::scan::kTab);
    REQUIRE(t.session().selected == 2);
    REQUIRE(t.row("Context") != nullptr);
    CHECK(t.row("Context")->value() == "root");
    CHECK(t.row("Context")->editable());

    // Author it exactly the way a width is authored: enter, retype, enter.
    retype(t, "Context", "#1");
    CHECK(t.row("Context")->value() == "#1");
    CHECK(t.doc().elements[1].context == 1);
    CHECK(t.notice() == "committed Context = #1");

    // The picture follows: #2's rectangle is its authored offset from #1's.
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* a = ui::placed_for(scene, 1);
    const ui::Placed* b = ui::placed_for(scene, 2);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(b->rect.x == a->rect.x + t.doc().elements[1].x);
    CHECK(b->rect.y == a->rect.y + t.doc().elements[1].y);

    // Move #1 with the keyboard; #2 follows and its authored values do not move.
    const ui::Element b_before = t.doc().elements[1];
    t.key(input::scan::kTab); // back to #1
    REQUIRE(t.session().selected == 1);
    t.key(input::scan::kL);
    CHECK(t.doc().elements[1] == b_before);
    CHECK(ui::placed_for(workspace_scene(t.doc(), t.session()), 2)->rect.x == b->rect.x + 1);

    // A refusal reaches the maker as a refusal, and writes nothing.
    t.key(input::scan::kTab);
    REQUIRE(t.session().selected == 2);
    retype(t, "Context", "#99");
    CHECK(t.notice() == "Context: no object #99 to take context from");
    CHECK(t.session().notice_is_bad);
    CHECK(t.doc().elements[1].context == 1);
    t.key(input::scan::kEscape);

    // ...and an unparseable draft is the OTHER outcome, still told apart: a
    // bare `2` is not a context, because a context is an identity and not the
    // second object.
    retype(t, "Context", "2");
    CHECK(t.notice() == "Context: not root or an identity (#1)");
    CHECK(t.doc().elements[1].context == 1);
    t.key(input::scan::kEscape);
}

TEST_CASE("a delete refusal reaches the maker with the dependents named") {
    Live t;
    t.key(input::scan::kTab);
    retype(t, "Context", "#1");
    REQUIRE(t.doc().elements[1].context == 1);

    t.key(input::scan::kTab); // select #1, the source
    REQUIRE(t.session().selected == 1);
    t.key(input::scan::kD);
    CHECK(t.notice() == "#2 takes context from #1 -- change or delete it first");
    CHECK(t.session().notice_is_bad);
    CHECK(t.doc().elements.size() == 2);
    CHECK(t.session().selected == 1);
}

TEST_CASE("a composed document survives save, a new process, and a load, through the keys") {
    // The cross-process claim, driven entirely by messages: run A composes and
    // saves, run B is a fresh weave with a DIFFERENT workspace that loads it.
    TempDir dir("compose");
    ui::Rect a_wide{};
    ui::Rect b_wide{};
    {
        Live run_a;
        run_a.host.document_path = dir.document();
        run_a.key(input::scan::kTab);
        retype(run_a, "Context", "#1");
        retype(run_a, "Width", "50%");
        REQUIRE(run_a.doc().elements[1].context == 1);
        REQUIRE(run_a.doc().elements[1].width == ui::Extent{ui::kExtentPercent, 50});

        const ui::Scene scene = workspace_scene(run_a.doc(), run_a.session());
        a_wide = ui::placed_for(scene, 1)->rect;
        b_wide = ui::placed_for(scene, 2)->rect;

        run_a.key(input::scan::kS, input::mod::kCtrl);
        REQUIRE(run_a.notice() == "saved " + dir.document());
    }

    Live run_b;
    run_b.host.document_path = dir.document();
    // A different window onto the same work: `[` narrows the workspace.
    run_b.key(input::scan::kLeftBracket);
    run_b.key(input::scan::kLeftBracket);
    run_b.key(input::scan::kLeftBracket);
    REQUIRE(run_b.session().workspace_w < kWorkspaceW);
    run_b.key(input::scan::kO, input::mod::kCtrl);
    REQUIRE(run_b.notice().rfind("loaded", 0) == 0);

    // IDENTITY, RELATIONSHIP and AUTHORED VALUES: the same.
    REQUIRE(run_b.doc().elements.size() == 2);
    CHECK(run_b.doc().elements[0].id == 1);
    CHECK(run_b.doc().elements[1].id == 2);
    CHECK(run_b.doc().elements[1].context == 1);
    CHECK(run_b.doc().elements[1].width == ui::Extent{ui::kExtentPercent, 50});
    CHECK(run_b.doc().elements[1].x == 6);

    // RESOLVED: rebuilt, and different, transitively.
    const ui::Scene scene = workspace_scene(run_b.doc(), run_b.session());
    const ui::Rect a_now = ui::placed_for(scene, 1)->rect;
    const ui::Rect b_now = ui::placed_for(scene, 2)->rect;
    CHECK(a_now.w < a_wide.w);
    CHECK(b_now.w < b_wide.w);
    CHECK(b_now.w == a_now.w / 2); // still half of #1, whatever #1 came to

    // And it is the same relationship, not a lookalike: move #1 and #2 follows.
    run_b.key(input::scan::kL);
    CHECK(ui::placed_for(workspace_scene(run_b.doc(), run_b.session()), 2)->rect.x ==
          b_now.x + 1);
}

// ---- Tier 7: the terminal overlay (WT-1) -----------------------------------------------
//
// WORKSHOP PRESENTS AN ORDINARY `loom::TerminalSession` ON ITS EXISTING LOOM. Six claims the
// prompt named, and four this composition has to keep for those six to mean anything:
//
//   the terminal toggle (ctrl+t since KEY-0) opens it and closes it
//   a closed overlay leaves every ordinary Workshop gesture exactly as it was
//   a typed line reaches the PARTICIPANT -- its own record, its own door
//   the transcript becomes visible through Workshop's own canvas vocabulary
//   the participant is on the SAME bus, not a second Loom
//   ...and the toggle's own keystroke never becomes text
//   ...and a target sees the PARTICIPANT as sender, never Workshop
//   ...and the two grants do not merge, in either direction     <- canary'd
//   ...and SUBMITTED never quietly becomes "delivered" on this screen
//   ...and the pane says what it is not showing
//
// The pointer path is not re-proven here: it is the same `canvas_point_of` chain Tier 4
// walks, and the only thing WT-1 changed about it is that an open overlay ignores it.

TEST_CASE("the terminal toggle opens the overlay, and the same toggle closes it") {
    Live t;
    t.mount_terminal();
    REQUIRE_FALSE(t.pane().open);

    t.toggle_terminal();
    CHECK(t.pane().open);
    CHECK(t.pane().attached);
    CHECK(t.pane().id == t.terminal_id);

    t.toggle_terminal();
    CHECK_FALSE(t.pane().open);
    CHECK(t.notice() == "terminal closed -- ^t reopens it");

    // A bare Space is not the gesture, and neither is Shift+anything-else.
    t.key(input::scan::kSpace);
    CHECK_FALSE(t.pane().open);
    t.key(input::scan::kJ, input::mod::kShift);
    CHECK_FALSE(t.pane().open);
}

TEST_CASE("the toggle's own keystroke never becomes text, in either direction") {
    Live t;
    t.mount_terminal();
    // The backends report Shift+Space as a key AND a space. Opening must not seed the command
    // line with one, and closing must not append one to whatever is underneath.
    t.toggle_terminal();
    REQUIRE(t.pane().open);
    CHECK(t.pane().input.empty());

    t.text("s");
    CHECK(t.pane().input.text() == "s");
    t.toggle_terminal(); // close
    REQUIRE_FALSE(t.pane().open);

    // ...and the space did not land in the inspector either. Open an editor draft first, so
    // there is somewhere for a leaked space to go, and look at it after a full open/close.
    t.begin_editing("Name");
    const Row* name = t.row("Name");
    REQUIRE(name != nullptr);
    REQUIRE(name->editing());
    const std::string draft = name->display();
    t.toggle_terminal();
    t.toggle_terminal();
    CHECK(t.row("Name")->display() == draft);

    // A ONE-SHOT FLAG THAT OUTLIVED ITS MOMENT WOULD EAT A SPACE A MAKER MEANT. The next
    // ordinary key ends the toggle's moment, so a space typed after one is ordinary text.
    t.text("a");
    t.text(" ");
    t.text("b");
    CHECK(t.row("Name")->display().find("a b") != std::string::npos);
}

TEST_CASE("a closed overlay leaves every ordinary Workshop gesture exactly as it was") {
    Live t;
    t.mount_terminal();
    const std::int64_t before_x = t.first()->x;

    // Open, type something that would be four commands with the pane closed, close.
    t.toggle_terminal();
    t.type_line("hjkl");
    t.toggle_terminal();
    REQUIRE_FALSE(t.pane().open);

    t.key(input::scan::kL);
    CHECK(t.first()->x == before_x + 1);
    t.key(input::scan::kN);
    CHECK(t.doc().elements.size() == 3);
    t.key(input::scan::kTab);
    CHECK(t.session().selected != 0);
    t.press(t.first()->x + 1, t.first()->y + 1);
    CHECK(t.session().drag.active);
    t.release(t.first()->x + 1, t.first()->y + 1);
    CHECK_FALSE(t.session().drag.active);
}

TEST_CASE("while the overlay is open the keys and the pointer belong to it") {
    Live t;
    t.mount_terminal();
    const std::int64_t x = t.first()->x;
    const std::size_t objects = t.doc().elements.size();
    t.toggle_terminal();

    // `h`, `n` and `d` are Workshop commands with the pane closed. With it open they are text.
    t.key(input::scan::kH);
    t.text("h");
    t.key(input::scan::kN);
    t.text("n");
    t.key(input::scan::kD);
    t.text("d");
    CHECK(t.pane().input.text() == "hnd");
    CHECK(t.first()->x == x);
    CHECK(t.doc().elements.size() == objects);

    // ...and a press does not take hold of an object the maker cannot see.
    t.press(t.first()->x + 1, t.first()->y + 1);
    CHECK_FALSE(t.session().drag.active);
    t.motion(t.first()->x + 4, t.first()->y + 1);
    CHECK(t.first()->x == x);

    // Backspace erases, escape clears the line and LEAVES THE PANE OPEN.
    t.key(input::scan::kBackspace);
    CHECK(t.pane().input.text() == "hn");
    t.key(input::scan::kEscape);
    CHECK(t.pane().input.empty());
    CHECK(t.pane().open);
}

TEST_CASE("^s still means save with the overlay open, and ^c means copy there (TEXT-0)") {
    Live t;
    t.mount_terminal();
    t.toggle_terminal();
    // ^s is not text -- a control byte is never TextEntered -- so it cannot be typed into a
    // command line, and its meaning is deliberately mode-independent: the TextBox
    // vocabulary never binds it, and the global branch answers it above every mode.
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "no document file -- start Workshop with --document <path>");
    CHECK(t.pane().open);
    // ^c, by contrast, IS the editing surface's chord now (TEXT-0): while the overlay's
    // line has the keyboard it means copy, is consumed by the line's own vocabulary even
    // with nothing selected, and must never fall through to quit -- "copy nothing" ending
    // the whole application is the exact accident the consumed-bool contract exists to
    // make unwritable. Quit stays one mode away: close the overlay and the same chord
    // quits, exactly as it always did.
    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK_FALSE(t.host.quit);
    CHECK(t.pane().open);
    // With text selected the same chord takes the selection, so paste brings it back.
    t.text("abc");
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK(t.session().clipboard.text == "abc");
    CHECK_FALSE(t.host.quit);
    t.toggle_terminal();
    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK(t.host.quit);
}

TEST_CASE("a typed line reaches the participant's own record, understood or not") {
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    t.type_line("this is not a verb");

    // RECORDED BEFORE IT IS UNDERSTOOD. A chronology of effects with no causes is not a
    // session, so the line is on the participant even though it authored nothing.
    const std::vector<loom::TranscriptEntry> log = me->transcript().entries();
    REQUIRE(log.size() >= 2);
    CHECK(log[0].kind == loom::TranscriptKind::LocalCommand);
    CHECK(log[0].text == "this is not a verb");
    CHECK(log[1].kind == loom::TranscriptKind::LocalNotice);
    CHECK(log[1].text.find("two verbs") != std::string::npos);
    CHECK(of_kind(*me, loom::TranscriptKind::Submitted).empty());
}

TEST_CASE("a typed send leaves through the PARTICIPANT's door, on Workshop's own bus") {
    Live t;
    SkinSeat* seat = t.mount_skin_seat();
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"from the pane\"");
    t.bus.drain_until_idle();

    // IT ARRIVED, AND IT ARRIVED ON THIS BUS. The seat is registered on the same Switchboard
    // Workshop's own weave is; nothing here constructs a second Loom, and a participant on a
    // second one could not have reached this weave at all.
    REQUIRE_FALSE(seat->heard.empty());
    bool arrived = false;
    for (const surface::SurfaceText& said : seat->heard) {
        arrived = arrived || (said.slot == "score" && said.text == "from the pane");
    }
    CHECK(arrived);

    // ...AND THE BUS STAMPED THE PARTICIPANT, NOT WORKSHOP. This is the whole authority
    // claim, measured at the one place that cannot be talked out of it.
    CHECK(seat->who_said("from the pane") == t.terminal_id);
    CHECK(seat->who_said("from the pane") != t.workshop_id);
    CHECK(me->id() == t.terminal_id);

    // TWO IDENTITIES, ONE SCREEN, AND STILL TWO. The same seat also heard Workshop's own
    // status line, from every repaint -- and those are stamped with WORKSHOP's id. One
    // keyboard, one screen, one bus, two senders, told apart by Loom and by nothing else.
    bool workshop_spoke = false;
    for (const loom::WeaveId who : seat->from) {
        workshop_spoke = workshop_spoke || who == t.workshop_id;
    }
    CHECK(workshop_spoke);

    // The participant's own record says SUBMITTED, addressed to the office, and says nothing
    // whatever about what happened next.
    const std::vector<loom::TranscriptEntry> sent =
        of_kind(*me, loom::TranscriptKind::Submitted);
    REQUIRE(sent.size() == 1);
    CHECK(sent.back().shape == "SurfaceText");
    CHECK(sent.back().addressing == loom::Addressing::Role);
    CHECK(sent.back().role == surface::kSkinRole);
}

TEST_CASE("holding the pointer merged nothing: the two grants stay different") {
    // The pane holds a C++ pointer to a participant. That is PRESENTATION. What each of the
    // two may SAY is decided by the Kernel against that one's own grant, separately -- and
    // here the two differ in the same shape, by their rule alone: Workshop may publish
    // SurfaceText to anybody; the participant may say it only to an office.
    //
    // CANARY: `mount_terminal(true)` gives the participant Workshop's wider rule, and the
    // publication below then arrives -- which is what makes this a measurement rather than a
    // restatement of the fixture.
    const bool widen = false;
    Live t;
    SkinSeat* seat = t.mount_skin_seat();
    loom::TerminalSession* me = t.mount_terminal(widen);
    t.toggle_terminal();

    // The office: authorized, and it lands.
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"to the office\"");
    t.bus.drain_until_idle();
    CHECK(seat->who_said("to the office") == t.terminal_id);

    // A PUBLICATION: composed, authored, SUBMITTED -- and refused at delivery for want of a
    // rule this participant does not hold. The participant is not told that, and does not
    // claim to know it; what the transcript says is that it was submitted.
    t.type_line("send * SurfaceText 1 slot=\"score\" text=\"to everyone\"");
    t.bus.drain_until_idle();
    CHECK(of_kind(*me, loom::TranscriptKind::Submitted).size() == 2);
    CHECK_FALSE(seat->who_said("to everyone").valid());

    // ...while Workshop's own publications of the very same shape reached the seat all along.
    // Nothing the participant holds widened Workshop, and nothing Workshop holds widened the
    // participant.
    bool workshop_published = false;
    for (const loom::WeaveId who : seat->from) {
        workshop_published = workshop_published || who == t.workshop_id;
    }
    CHECK(workshop_published);
}

TEST_CASE("the address grammar the pane reads is Loom's own, not a second one") {
    // The pane parses `@office` with `loom::parse_address` out of <zen/terminal/input_lex.hpp>,
    // the same parser the standalone terminal uses. Two claims, joined here: the parser says
    // what it says, and a line typed into Workshop is addressed by it.
    loom::Address a;
    REQUIRE(loom::parse_address("@zengine.skin", a));
    REQUIRE(a.mode == loom::Addressing::Role);

    Live t;
    SkinSeat* seat = t.mount_skin_seat();
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    // A BAREWORD IS NOT AN ADDRESS, and the pane invents no default for one.
    t.type_line("send zengine.skin SurfaceText 1 slot=\"score\" text=\"no sigil\"");
    t.bus.drain_until_idle();
    CHECK_FALSE(seat->who_said("no sigil").valid());
    CHECK(of_kind(*me, loom::TranscriptKind::Submitted).empty());
    CHECK(me->transcript().entries().back().text.find("#12 for one weave") != std::string::npos);

    // ...and the same line with the sigil is authored.
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"with sigil\"");
    t.bus.drain_until_idle();
    CHECK(seat->who_said("with sigil") == t.terminal_id);
}

TEST_CASE("an ask waits, and LOOM's own answer settles it on the screen") {
    Live t;
    (void)t.mount_skin_seat();
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    // AWAITING IS ALL AN OUTSTANDING ASK MEANS. Not delivered, not being worked on, not seen
    // by anybody -- so an ask nobody answers simply stays outstanding, and the pane pumps
    // nothing to change that: the HOST owns the loop here exactly as it does in the
    // standalone terminal.
    t.type_line("ask #99 SurfaceText 1 slot=\"ask\" text=\"anyone at 99\"");
    CHECK(me->awaiting());
    CHECK(me->outstanding() == 1);
    t.bus.drain_until_idle();
    t.bus.drain_until_idle();
    CHECK(me->awaiting()); // still. Nothing here can tell it whether that was even delivered.

    t.type_line("ask @zengine.skin SurfaceText 1 slot=\"ask\" text=\"is anybody there\"");
    CHECK(me->outstanding() == 1); // the second settled inside the fixture's own pump

    // The answer is settled by LOOM's provenance and correlation, not by shape: the entry is
    // an AnswerReceived, it names the local ask number, and the pane renders both.
    const std::vector<loom::TranscriptEntry> answers =
        of_kind(*me, loom::TranscriptKind::AnswerReceived);
    REQUIRE(answers.size() == 1);
    CHECK(answers.front().answers_ask);
    CHECK(answers.front().shape == "zen.Ack");
    CHECK(answers.front().sender != t.workshop_id);
    CHECK(terminal_line(answers.front()).find("[Loom: answers ask 2]") != std::string::npos);

    // Escape: a repaint, and a cleared line. `t.text(...)` would repaint too and would leave
    // what it typed in the buffer -- which is how this case first read `xask * ...` and
    // measured the wrong verb.
    t.key(input::scan::kEscape);
    CHECK(pane_text(t.canvases.back()).find("v zen.Ack v1 from #") != std::string::npos);
    CHECK(pane_text(t.canvases.back()).find("[Loom: answers ask 2]") != std::string::npos);

    // A PUBLICATION CANNOT BE AN ASK: it has no one respondent, so there is no conversation
    // for Loom to authorize. Refused LOCALLY, and the participant says so in its own record.
    const std::size_t before = me->outstanding();
    t.type_line("ask * SurfaceText 1 slot=\"ask\" text=\"everyone\"");
    CHECK(me->outstanding() == before);
    std::string said;
    for (const loom::TranscriptEntry& e : me->transcript().entries()) {
        if (e.kind == loom::TranscriptKind::LocalNotice ||
            e.kind == loom::TranscriptKind::LocalRefusal) {
            said = e.text;
        }
    }
    CHECK(said.find("bad-address") != std::string::npos);
}

TEST_CASE("the transcript becomes visible through Workshop's own canvas") {
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    t.toggle_terminal();
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"hello\"");
    t.bus.drain_until_idle();
    const surface::SurfaceCanvas& c = t.canvases.back();

    // ANCHORED TO THE BOTTOM-RIGHT CORNER OF THE ROOM, exactly (HD-10). The bottom edge is
    // still the screen's; the right edge is the WORKSPACE's, which is the whole of what this
    // phase moved -- the 28 columns beyond it are reserved for the side region and are not
    // this pane's to spend, whether or not anything is standing in them.
    CHECK(kMinScreen.terminal_x + kMinScreen.terminal_w == kMinScreen.room_w);
    CHECK(kMinScreen.terminal_y + kMinScreen.terminal_h == kMinScreen.h);
    CHECK(label_at(c, kMinScreen.terminal_x, kMinScreen.terminal_y).rfind("TERMINAL -- weave #", 0) == 0);
    CHECK(label_at(c, kMinScreen.terminal_x, kMinScreen.terminal_y).find(std::to_string(t.terminal_id.value)) !=
          std::string::npos);

    // The line a maker typed, and the message it authored, both on the screen.
    const std::string body = pane_text(c);
    CHECK(body.find("> send @zengine.skin") != std::string::npos);
    CHECK(body.find("^ SurfaceText v1 -> @zengine.skin") != std::string::npos);

    // SUBMITTED IS NOT DELIVERED, and a renderer is the one place that contract can be broken
    // by prose alone. The pane says what SUBMITTED means, once, on a row it always shows --
    // and nowhere on it does it say the other thing.
    //
    // AND AT THIS EXTENT IT IS ELIDED BY THREE CHARACTERS, WHICH IS HD-10'S PRICE AND IS
    // MEASURED HERE RATHER THAN DISCOVERED. The statement is 51 characters and the narrowest
    // pane is now 48, so `detail::fit` marks it -- the same mark it puts on every other row
    // it cuts. What is asserted is therefore the row the pane actually writes, and the two
    // halves of the contract are asserted separately: the word SUBMITTED survives the cut at
    // every width this pane has, and the word `delivered` appears nowhere at any width.
    CHECK(body.find("SUBMITTED") != std::string::npos);
    CHECK(body.find(detail::fit(terminal_legend(), kMinScreen.terminal_cols)) != std::string::npos);
    CHECK(body.find("delivered") == std::string::npos);
    CHECK(label_at(c, kMinScreen.terminal_x, kMinScreen.terminal_y + 1)
              .rfind(detail::fit(terminal_legend(), kMinScreen.terminal_cols), 0) == 0);
    // The whole statement is on the row from 81 columns up, where the room first holds it.
    CHECK(static_cast<std::int64_t>(terminal_legend().size()) == 51);
    CHECK(screen_of(81, kScreenMinH).terminal_cols == 51);
    CHECK(detail::fit(terminal_legend(), screen_of(81, kScreenMinH).terminal_cols) ==
          terminal_legend());

    // The cursor row shows what is being typed, and every row is exactly the pane's width --
    // which is what clears the furniture underneath in a medium whose ink is one cell.
    t.text("ab");
    const surface::SurfaceCanvas& c2 = t.canvases.back();
    CHECK(label_at(c2, kMinScreen.terminal_x, kMinScreen.h - 1).rfind("> ab_", 0) == 0);
    for (const surface::SurfaceLabel& l : all_labels(c2)) {
        if (l.x == kMinScreen.terminal_x && l.y >= kMinScreen.terminal_y) {
            CHECK(static_cast<std::int64_t>(l.text.size()) == kMinScreen.terminal_w);
        }
    }

    // The bottom band is the pane's while it is open: the notice and the two help lines are
    // not painted under it, because their right-hand two thirds would be covered and a
    // sentence beheaded mid-word is worse than no sentence.
    //
    // SAID AS CONTENT AND NOT AS POSITION (HD-10). It used to be `nothing is at column 0 on
    // those rows`, which was true only because the pane began at column 22; the pane begins
    // at column 0 on the minimum screen now, so that spelling would have been asking whether
    // the PANE was there. What the claim was always about is the two sentences.
    CHECK(label_at(c2, 0, kMinScreen.help_y).rfind("n new | d delete", 0) != 0);
    CHECK(label_at(c2, 0, kMinScreen.help_y + 1).rfind("enter edit | esc cancel", 0) != 0);
    // ...and with the pane closed they are exactly as they were.
    t.toggle_terminal();
    CHECK(label_at(t.canvases.back(), 0, kMinScreen.help_y).rfind("n new | d delete", 0) == 0);
}

TEST_CASE("the pane is published as ONE bounded region, placed in cells") {
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    // ESCAPE FIRST, so this case measures the PANE and not the pane plus the completion
    // list HD-2 put over it -- which is a second region belonging to this same pane, and
    // is asserted where it belongs, a few cases down.
    t.text("s");
    t.key(input::scan::kEscape);
    const surface::SurfaceCanvas& c = t.canvases.back();

    // TWO REGIONS, AND THE SECOND IS THE INSPECTOR'S PROPERTY BODY (HD-6). No other panel
    // migrated: the object list, the picker and the help band are all still labels, and the
    // completion list is not on this canvas (Escape dismissed it above). Asked WITHOUT the
    // workspace plane since TYPE-1, where an authored object's name became a region of its
    // own -- this case is about the pane, and the document's own text is not a panel.
    REQUIRE(all_texts(without_workspace(c)).size() == 2);
    CHECK(list_of(c, kMinScreen) == nullptr);
    const surface::SurfaceTextRegion& pane = *pane_of(c, kMinScreen);
    CHECK(pane.x == kMinScreen.terminal_x);
    CHECK(pane.y == kMinScreen.terminal_y);
    CHECK(pane.w == kMinScreen.terminal_w);
    CHECK(pane.h == kMinScreen.terminal_h);
    CHECK(pane.rows.size() == kMinScreen.terminal_lines);

    // The backdrop is STILL A RECT IN CELLS. What got finer is the interior, not the
    // furniture -- a region carries no background of its own and was not given one.
    bool backdrop = false;
    for (const surface::SurfaceRect& r : all_rects(c)) {
        if (r.x == pane.x && r.y == pane.y && r.w == pane.w && r.h == pane.h) {
            backdrop = true;
            CHECK(r.role == surface::role::kMuted);
        }
    }
    CHECK(backdrop);

    // The chrome is where it has always been, counted in the pane's own rows.
    CHECK(pane.rows[0].text.rfind("TERMINAL -- weave #", 0) == 0);
    CHECK(pane.rows[0].role == surface::role::kAccent);
    // Fitted to the pane's own columns since HD-10 -- 48 of them on the minimum screen, three
    // short of this statement, so the row carries the mark `detail::fit` leaves.
    CHECK(pane.rows[1].text == detail::fit(terminal_legend(), kMinScreen.terminal_cols));
    // THE LINE, AND THE CARET SAID SEPARATELY FROM IT (HD-3). The row no longer carries a
    // trailing `_`: the caret is a fact ABOUT the region, in the region's own prose
    // lattice, so each medium can answer it in its own type. The projection below is where
    // a character medium's answer is measured.
    CHECK(pane.rows[pane.rows.size() - 1].text.rfind("> s", 0) == 0);
    CHECK(pane.rows[pane.rows.size() - 1].text.find('_') == std::string::npos);
    CHECK(pane.caret_row == static_cast<std::int64_t>(pane.rows.size()) - 1);
    CHECK(pane.caret_col == kTerminalPromptCols + 1); // `> s` -- after the one character

    // A ROW IS TRUNCATED BY THE PUBLISHER AND PADDED BY THE PROJECTION. The pane decides
    // what it can show (a presentation decision); making a space erase the furniture
    // underneath is every medium's business and is done for all of them at once.
    for (const surface::SurfaceTextRow& row : pane.rows) {
        CHECK(static_cast<std::int64_t>(row.text.size()) <= kMinScreen.terminal_cols);
    }
    const surface::SurfaceCanvas only_pane = canvas_of_region(c, pane);
    for (const surface::ProjectedRow& p : projected_of(only_pane)) {
        CHECK(static_cast<std::int64_t>(p.label.text.size()) == kMinScreen.terminal_w);
    }

    // ...and with the pane closed there is no region of ITS but the Info panel's body is
    // still there, because the body is not an overlay: it is what the Info panel has been
    // drawing all along, bounded (HD-6, widened to both lists by HD-7).
    //
    // ASKED BY PLACE RATHER THAN BY COUNT (TYPE-0). The screen's own notice is a bounded
    // region too since TYPE-0, and it is present here because closing the pane says so --
    // so "the only region on the canvas" stopped being a question about the pane and became
    // one about how many other things happen to be speaking.
    t.toggle_terminal();
    const std::vector<surface::SurfaceTextRegion> left = all_texts(t.canvases.back());
    CHECK(std::count_if(left.begin(), left.end(),
                        [&](const surface::SurfaceTextRegion& r) {
                            return r.x == kMinScreen.terminal_x && r.y == kMinScreen.terminal_y;
                        }) == 0);
    CHECK(std::count_if(left.begin(), left.end(),
                        [&](const surface::SurfaceTextRegion& r) {
                            return r.y == body_place(t).region_y;
                        }) == 1);
}

TEST_CASE("a medium that sets real type reflows the pane, and the omission stays true") {
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();

    // Forty entries, each a sentence long enough to WRAP at 48 cells and not at 71
    // columns -- which is exactly the width difference the metric buys, so the same
    // record produces a different number of rows in the two media. (56 and 83 before
    // HD-10 narrowed the pane's placement by eight cells at this extent; the difference
    // the case is about is between the two MEDIA and is unmoved by that.)
    for (int i = 0; i < 40; ++i) {
        (void)me->record_notice("entry " + std::to_string(i) +
                                ": a sentence of some length, wrapping at the cell width");
    }
    // `x` completes nothing, and HD-2's truthful "nothing here begins with that" is a
    // second region. Dismissed, so this case still measures the pane on its own.
    t.text("x");
    t.key(input::scan::kEscape);
    const std::size_t cell_shown = t.pane().shown.size();
    const std::uint64_t cell_earlier = t.pane().earlier;

    // THE MEDIUM SAYS IT HAS TYPE. Same cells, a real advance and a real line height.
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    const Screen sc = screen_of(t.session());
    REQUIRE(sc.terminal_cols == 71);
    REQUIRE(sc.terminal_rows == 4);

    // THE SNAPSHOT AND THE PICTURE AGREE, which is the whole of HD-1's correctness
    // requirement. Both sides asked `screen_of` for the same two numbers, so what the pane
    // CHOSE to show is exactly what the pane SPENDS its rows on -- and `... N earlier` is
    // therefore arithmetic rather than a hope.
    const surface::SurfaceCanvas& c = t.canvases.back();
    const surface::SurfaceTextRegion& pane = *pane_of(c, sc);
    CHECK(pane.rows.size() == sc.terminal_lines);
    CHECK(pane.w == kMinScreen.terminal_w); // the PLACEMENT did not move
    CHECK(pane.h == kMinScreen.terminal_h);

    std::size_t spent = 0;
    for (const loom::TranscriptEntry& e : t.pane().shown) {
        spent += terminal_wrapped(e, sc.terminal_cols).size();
    }
    CHECK(spent <= sc.terminal_rows);
    CHECK(t.pane().earlier ==
          me->transcript().size() - static_cast<std::uint64_t>(t.pane().shown.size()));
    CHECK(terminal_omission(t.pane()) ==
          "... " + std::to_string(t.pane().earlier) + " earlier");

    // THE RECORD REFLOWED RATHER THAN BEING REWRITTEN. The same entries cost FEWER ROWS at
    // the wider prose measure, which is the whole reason a pane four rows tall can still
    // hold roughly what a pane nine cell-rows tall held.
    std::size_t cell_cost = 0;
    for (const loom::TranscriptEntry& e : t.pane().shown) {
        cell_cost += terminal_wrapped(e, kMinScreen.terminal_w).size();
    }
    CHECK(cell_cost > spent);
    CHECK(t.pane().shown.size() >= 1);
    CHECK(t.pane().earlier > 0);
    CHECK(t.pane().dropped == 0);
    CHECK(cell_shown >= 1);
    CHECK(cell_earlier > 0);

    // A LONGER LINE IS THE PROOF THE WIDTH IS REAL: at 71 columns this is one row, and at
    // 48 cells it is more than one. (Sixty characters and a three-character sigil, chosen
    // against the two widths HD-10 left this pane rather than the two it had before.)
    const std::string long_line(60, 'w');
    (void)me->record_notice(long_line);
    t.text("y");
    const loom::TranscriptEntry newest = me->transcript().tail(1)[0];
    CHECK(terminal_wrapped(newest, sc.terminal_cols).size() == 1);
    CHECK(terminal_wrapped(newest, kMinScreen.terminal_w).size() > 1);

    // AND THE FACE GOING AWAY PUTS IT BACK. A metric of zero is the sentence "text is a
    // cell", which is what the bitmap fallback draws -- so a font that fails to open leaves
    // Workshop wrapping against the thing that is actually being painted, never against the
    // metric of a face that is not there.
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 0, 0}));
    const Screen back = screen_of(t.session());
    CHECK(back.terminal_cols == kMinScreen.terminal_cols);
    CHECK(back.terminal_lines == kMinScreen.terminal_lines);
    CHECK(back.terminal_rows == kMinScreen.terminal_rows);
    CHECK(pane_of(t.canvases.back(), back)->rows.size() == kMinScreen.terminal_lines);
}

TEST_CASE("a fresh skin's hello does NOT clear the presentation context -- measured, not blessed") {
    // HD-1 §18, answered by measurement rather than by hope. The question is what happens if
    // a metric-reporting graphical skin disappears and a non-reporting one becomes active in
    // the same process, and the answer is: NOTHING HAPPENS, which is the problem.
    //
    // A skin's HELLO carries no room and no metric -- it is one sentence, "I have claimed a
    // surface" -- so nothing about it can correct a presentation context left behind by a
    // skin that is gone, and the session keeps the dead window's cell extent AND, since HD-1,
    // its text metric. That is why this is reported here rather than repaired here: fixing it
    // through the hello means deciding what a skin's hello does to a screen, which is a
    // skin-handoff decision with its own evidence, not a detail of the extent's lifetime.
    //
    // TUI-0 NARROWED THIS WITHOUT CLOSING IT, and the distinction is worth keeping exact. A
    // terminal skin that can measure its terminal now publishes its own extent on its first
    // pump, which arrives within a beat of the swap and overwrites both numbers -- so the
    // window a maker actually swaps into a real terminal repairs itself. What is unrepaired
    // is the swap into a medium with NO opinion (a piped run, a captured run), where the
    // silence below is still the whole of what arrives. The seam moved from "always" to
    // "when the new medium cannot measure itself", and this case pins the second.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();

    t.publish(loom::to_value(surface::SurfaceExtent{115, 63, 8, 18}));
    CHECK(t.session().screen_w == 115);
    CHECK(t.session().text_advance_px == 8);
    CHECK(t.session().text_line_px == 18);

    // A NEW SKIN SAYS HELLO. This is exactly what a freshly loaded incarnation publishes --
    // once, on its first message -- and it is the only thing a terminal skin ever says about
    // itself.
    t.publish(loom::to_value(surface::SurfaceReady{}));

    // ...and the graphical numbers are all still here. Workshop repaints, at the extent and
    // the metric of a surface that may no longer exist.
    CHECK(t.session().screen_w == 115);
    CHECK(t.session().text_advance_px == 8);
    CHECK(t.session().text_line_px == 18);

    // WHAT THAT COSTS, CONCRETELY, so the seam is a measurement rather than a worry: the pane
    // chooses its rows at the graphical width and the cell projection then cuts them at the
    // region's cell width, so a character medium inheriting a graphical context loses the
    // right-hand end of every long row -- silently, because both halves are behaving.
    const Screen sc = screen_of(t.session());
    CHECK(sc.terminal_cols > sc.terminal_w);
    CHECK(list_of(t.canvases.back(), sc) == nullptr);
}

TEST_CASE("the pane says what it is not showing, in the two senses that differ") {
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    CHECK(terminal_omission(t.pane()) == "[the whole of this session's record is on screen]");

    // More lines than the pane is tall: the rest is EARLIER, and still in the record.
    for (int i = 0; i < 20; ++i) {
        (void)me->record_notice("line " + std::to_string(i));
    }
    t.text("x"); // any event repaints, and a repaint re-snapshots
    CHECK(t.pane().shown.size() == kMinScreen.terminal_rows);
    CHECK(t.pane().earlier > 0);
    CHECK(t.pane().dropped == 0);
    CHECK(terminal_omission(t.pane()).rfind("... ", 0) == 0);

    // Past the transcript's own bound the earliest lines are gone for good, and the pane says
    // THAT differently -- "you cannot see it here" and "nobody can see it any more" are
    // answers to different questions.
    for (std::size_t i = 0; i < loom::kTranscriptCapacity + 5; ++i) {
        (void)me->record_notice("flood " + std::to_string(i));
    }
    t.text("y");
    CHECK(t.pane().dropped > 0);
    CHECK(terminal_omission(t.pane()).find("dropped for good") != std::string::npos);
    CHECK(pane_text(t.canvases.back()).find("dropped for good") != std::string::npos);
}

TEST_CASE("a Workshop with no participant says so and authors nothing") {
    // The host may mount none -- `HostContext::terminal` is a plain pointer with a null
    // default, and a suite constructs one either way. The pane must be a refusal, not a crash.
    Live t;
    REQUIRE(t.host.terminal == nullptr);
    t.toggle_terminal();
    CHECK(t.pane().open);
    CHECK_FALSE(t.pane().attached);
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"x\"");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("no terminal participant is mounted") != std::string::npos);
    CHECK(label_at(t.canvases.back(), kMinScreen.terminal_x, kMinScreen.terminal_y)
              .rfind("TERMINAL -- no participant", 0) == 0);
}

TEST_CASE("the pane's snapshot outlives the participant it came from") {
    // THE LIFETIME CLAIM, and the reason the sanitizer lane is obliged for this phase. The bus
    // owns the participant; Workshop holds a non-owning pointer to it. Every entry the pane
    // paints from is a COPY taken inside a handler, so a canvas already published cannot come
    // to read a freed transcript -- and this case is what lets ASan say so.
    std::vector<loom::TranscriptEntry> held;
    std::string painted;
    {
        Live t;
        (void)t.mount_terminal();
        t.toggle_terminal();
        t.type_line("hello");
        held = t.pane().shown;
        painted = pane_text(t.canvases.back());
        REQUIRE_FALSE(held.empty());

        // The host's explicit act: end the participant, and LET THE LAST OWNER GO. The inner
        // scope is load-bearing -- `unregister_weave` hands the weave back, so holding that
        // unique_ptr would keep the participant alive and this case would prove nothing about
        // a dead one. Measured: with the pointer kept, deleting the line below is invisible to
        // ASan; with it released, deleting the line below is a heap-use-after-free.
        {
            const std::unique_ptr<loom::Weave> gone = t.bus.unregister_weave(t.terminal_id);
            CHECK(gone != nullptr);
        }
        t.host.terminal = nullptr;

        // Workshop keeps working, and now paints a pane with no participant behind it.
        t.text("z");
        CHECK_FALSE(t.pane().attached);
        CHECK(t.pane().shown.empty());
        CHECK(t.canvases.back().width == kMinScreen.w);
    }
    CHECK(held.front().text == "hello");
    CHECK(painted.find("> hello") != std::string::npos);
}

TEST_CASE("the overlay a maker actually sees: a solid pane, through the real rasterizer") {
    // THE CASE THE FIRST LIVE RASTERIZATION EARNED. Every check on the canvas passes with a
    // pane full of holes: a transcript row with no entry was simply not written, so the
    // backdrop's own glyph showed through into the workspace behind it, and only a picture
    // says so. This is that picture, pinned -- and it is a pure `paint()` of a hand-built
    // pane, so it needs no bus, no participant and no keystrokes to say it.
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);
    s.notice = "this notice is under the pane and must not be painted";
    s.terminal.open = true;
    s.terminal.attached = true;
    s.terminal.id = loom::WeaveId{3};
    s.terminal.input.set("send @", 6);
    loom::TranscriptEntry typed;
    typed.kind = loom::TranscriptKind::LocalCommand;
    typed.text = "send @zengine.skin SurfaceText 1";
    s.terminal.shown.push_back(typed);

    const std::vector<std::string> rows = rasterized(paint(d, s));
    REQUIRE(rows.size() == static_cast<std::size_t>(kMinScreen.h));

    // THE PANE IS A SOLID BLOCK. One transcript row is filled and eight are empty, and an
    // EMPTY one is what the defect hid in: unwritten, it showed the backdrop's `.` straight
    // through into the workspace behind. Asserted as blank rather than as "no dots anywhere",
    // because a dot is perfectly legitimate CONTENT -- `zengine.skin` has one.
    const auto band = [&rows](std::int64_t y) {
        return rows[static_cast<std::size_t>(y)].substr(static_cast<std::size_t>(kMinScreen.terminal_x),
                                                        static_cast<std::size_t>(kMinScreen.terminal_w));
    };
    const std::string blank(static_cast<std::size_t>(kMinScreen.terminal_w), ' ');
    for (std::size_t i = 1; i < kMinScreen.terminal_rows; ++i) {
        CHECK(band(kMinScreen.terminal_y + 2 + static_cast<std::int64_t>(i)) == blank);
    }
    for (std::int64_t y = kMinScreen.terminal_y; y < kMinScreen.h; ++y) {
        CHECK(band(y).size() == static_cast<std::size_t>(kMinScreen.terminal_w));
    }

    // ...and it says the things it is for.
    CHECK(rows[static_cast<std::size_t>(kMinScreen.terminal_y)].find("TERMINAL -- weave #3") !=
          std::string::npos);
    CHECK(rows[static_cast<std::size_t>(kMinScreen.terminal_y) + 1].find(
              detail::fit(terminal_legend(), kMinScreen.terminal_cols)) != std::string::npos);
    CHECK(rows[static_cast<std::size_t>(kMinScreen.terminal_y) + 2].find("> send @zengine.skin") !=
          std::string::npos);
    CHECK(rows[static_cast<std::size_t>(kMinScreen.h) - 1].find("> send @_") != std::string::npos);

    // The workspace ABOVE the pane is untouched -- an overlay covers, it does not rearrange.
    CHECK(rows[3].rfind("..*panel", 0) == 0);
    // ...and the notice underneath it was not painted at all, in either half of its row.
    CHECK(rows[static_cast<std::size_t>(kMinScreen.notice_y)].find("this notice is under") ==
          std::string::npos);
}

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

    // The bottom band keeps its shape against the bottom edge.
    CHECK(big.h - big.notice_y == kMinScreen.h - kMinScreen.notice_y);
    CHECK(big.h - big.help_y == kMinScreen.h - kMinScreen.help_y);
    CHECK(big.help_y + 1 == big.h - 1); // the second help line is the last row

    // Nothing overlaps: the workspace ends, then a gap, then the panel.
    CHECK(big.room_w + kPanelGap == big.panel_x);
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
        CHECK(sc.terminal_x >= 0);
        CHECK(sc.terminal_y >= 0);
        // THE RESERVATION, OVER EVERY EXTENT THIS SCREEN CAN BE ASKED FOR (HD-10). The pane's
        // right edge is the WORKSPACE's, so the reserved side column is never any part of it
        // -- said twice on purpose, because the two say different things: the first is where
        // the pane's edge IS, the second is the law that edge exists to keep, and a later
        // placement rule that satisfied one without the other would be caught by the other.
        CHECK(sc.terminal_x + sc.terminal_w == sc.room_w);
        CHECK(sc.terminal_x + sc.terminal_w <= sc.panel_x - kPanelGap);
        CHECK(sc.terminal_y + sc.terminal_h == sc.h);
        CHECK(sc.terminal_rows >= 1);
        CHECK(sc.notice_y < sc.h);
        CHECK(sc.help_y + 1 < sc.h);
        // THE PANE'S INTERIOR IS NEVER NOTHING. A medium could report a line taller than
        // the whole pane, and a pane with no rows is indistinguishable from a broken tool
        // -- so there is a floor, and `wrap` and `fit` are never handed a width of zero.
        CHECK(sc.terminal_cols >= kTerminalMinCols);
        CHECK(sc.terminal_lines >= sc.terminal_rows);
        CHECK(sc.terminal_lines - sc.terminal_rows == static_cast<std::size_t>(kTerminalChrome));
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

TEST_CASE("the pane's interior follows the metric, and its placement does not") {
    // HD-1's central claim, as arithmetic. A medium that sets real type changes how much
    // PROSE the pane holds; it changes nothing about where the pane IS.
    const Screen cells = screen_of(78, 22);
    const Screen typed = screen_of(78, 22, 8, 18);

    CHECK(typed.terminal_x == cells.terminal_x);
    CHECK(typed.terminal_y == cells.terminal_y);
    CHECK(typed.terminal_w == cells.terminal_w);
    CHECK(typed.terminal_h == cells.terminal_h);

    // 48 cells is 576 device pixels; less the inset, at 8 px a character, that is 71
    // columns -- half again as much of every transcript line as the cell grid could show.
    // (56 and 83 before HD-10; the RATIO this case is about is a fact about the metric and
    // is what survives the pane getting eight cells narrower at this extent.)
    CHECK(cells.terminal_cols == 48);
    CHECK(typed.terminal_cols == 71);
    // ...and 156 pixels at an 18 px line is 8 rows where 13 cells used to be, which is the
    // measured cost of real type at the minimum window and is stated rather than hidden.
    CHECK(cells.terminal_lines == 13);
    CHECK(typed.terminal_lines == 8);
    CHECK(typed.terminal_rows == 8 - static_cast<std::size_t>(kTerminalChrome));

    // AND IT MOVES THE RIGHT WAY WITH THE WINDOW. Every pixel a person drags the edge by is
    // a pixel the metric spends on the record.
    const Screen bigger = screen_of(115, 63, 8, 18);
    CHECK(bigger.terminal_cols > typed.terminal_cols);
    CHECK(bigger.terminal_rows > typed.terminal_rows);

    // THE RESOLUTION IS THE SURFACE PACKAGE'S, NOT A SECOND COPY OF IT. This is what makes
    // "one measurer" a structural fact rather than a convention: the number the pane budgets
    // with is the number `fit_region` produces, and the graphical medium draws with the same
    // call on the same inputs.
    const surface::RegionFit fit = surface::fit_region(typed.terminal_x, typed.terminal_y,
                                                      typed.terminal_w, typed.terminal_h, 8, 18);
    CHECK(fit.columns == typed.terminal_cols);
    CHECK(static_cast<std::size_t>(fit.rows) == typed.terminal_lines);
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

namespace {

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

TEST_CASE("a visible panel occupies the pointer space it covers") {
    Live t;
    (void)mount_tool(t, "zengine-snake");
    // A SELECTION THAT IS NOT THE COVERED OBJECT, so "cannot select" is a claim
    // this case can actually make: the press below is on #1, and #2 has to still
    // be the selection afterwards.
    t.key(input::scan::kTab);
    const std::int64_t other = t.session().selected;
    REQUIRE(other == 2);

    open_builder(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect panel =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect);
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
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    CHECK(t.session().selected == other);
    CHECK_FALSE(t.session().drag.active);
    CHECK_FALSE(t.session().drag.resizing);
    CHECK(doc::find(t.doc(), 1)->x == before.x);
    CHECK(doc::find(t.doc(), 1)->y == before.y);
    t.release(wx, wy);
    CHECK_FALSE(t.session().drag.active);

    // AND THE SAME OBJECT IS REACHABLE AGAIN THE MOMENT THE PANEL IS REMOVED --
    // the same cell, the same document, the same gesture. The occlusion is the
    // panel's presence and nothing else.
    pick(t, panel::kBuilder);
    REQUIRE_FALSE(t.session().panels.has(panel::kBuilder));
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
    open_builder(t);
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
    open_builder(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect panel =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect);

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
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.active);
}

TEST_CASE("WIND-1: the columns the panel took are its own, and the band is the maker's") {
    // BOTH SIDES OF THE TRADE, AT ONE EXTENT, THROUGH THE LIVE DOORS. WIND-1 widens a stack
    // slot from 48 to 109 cells at 200x60, and the honest account of that is two sentences
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
    open_builder(t);

    const Screen sc = screen_of(t.session());
    REQUIRE(sc.room_w == 170);
    const ui::Rect panel =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect);
    REQUIRE(panel == ui::Rect{0, 1, 109, 9});

    // ---- INSIDE THE NEWLY OWNED AREA. Workspace column 60 was free before this phase (the
    // slot was 48 wide) and is the panel's now. #1 is underneath it, so there is genuinely
    // something for the press to have reached.
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    REQUIRE(ui::hit(scene, 60, 3) != nullptr);
    REQUIRE(ui::hit(scene, 60, 3)->id == 1);
    REQUIRE(panel.contains(60 + kWorkspaceX, 3 + kWorkspaceY));
    REQUIRE_FALSE(placement_bounds(placement::kOverlayStack, 0, kMinScreen)
                      .contains(60 + kWorkspaceX, 3 + kWorkspaceY)); // it was free before

    // IT IS PAINTED, and the paint is the whole rectangle rather than the old 48 columns:
    // one opaque backdrop at the panel's bounds, and every row padded to its width so a
    // character medium's spaces erase what is under them.
    const surface::SurfaceCanvas& c = t.canvases.back();
    CHECK(has_rect(c, panel.x, panel.y, panel.w, panel.h, surface::role::kMuted));
    CHECK_FALSE(has_rect(c, panel.x, panel.y, kStackW, panel.h, surface::role::kMuted));
    std::size_t padded = 0;
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == panel.x && l.y >= panel.y && l.y < panel.y + panel.h) {
            CHECK(l.text.size() == static_cast<std::size_t>(panel.w));
            ++padded;
        }
    }
    CHECK(padded == static_cast<std::size_t>(panel.h));

    // AND IT IS OCCUPIED. The press does not reach `take_hold`, so it cannot select, cannot
    // move and cannot resize -- all three at once, because they are that one call.
    const ui::Element before = *doc::find(t.doc(), 1);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, 60 + kWorkspaceX, 3 + kWorkspaceY).occupied);
    t.press(60, 3);
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    CHECK(t.session().selected == other);
    CHECK_FALSE(t.session().drag.active);
    CHECK(doc::find(t.doc(), 1)->x == before.x);
    CHECK(doc::find(t.doc(), 1)->y == before.y);
    t.release(60, 3);

    // ---- INSIDE THE RETAINED FREE BAND. Put #2 at workspace 120 -- past the panel's right
    // edge and inside the room -- on one of the panel's OWN rows, which is the only place
    // the claim means anything. It is carried there by the pointer rather than authored, so
    // the case does not need a door the maker does not have.
    t.press(6, 10); // canvas row 11: below the panel, so the grab is legal
    REQUIRE(t.session().drag.active);
    REQUIRE(t.session().drag.id == 2);
    t.motion(120, 3);
    t.release(120, 3);

    const ui::Scene moved = workspace_scene(t.doc(), t.session());
    const ui::Placed* two = ui::placed_for(moved, 2);
    REQUIRE(two != nullptr);
    REQUIRE(two->rect.x == 120);
    REQUIRE(two->rect.y == 3);
    CHECK(two->rect.x >= panel.x + panel.w); // outside width 109...
    CHECK(two->rect.x < sc.room_w);          // ...and inside room width 170
    CHECK_FALSE(panel.contains(two->rect.x + kWorkspaceX, two->rect.y + kWorkspaceY));
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, two->rect.x + kWorkspaceX,
                            two->rect.y + kWorkspaceY)
                    .occupied);

    // ITS TOP-LEFT PRESS REACHES THE WORKSPACE OBJECT. Same row as the refused press above,
    // 60 columns further right, and the answer is the opposite one.
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

    open_builder(t);
    const Screen sc = screen_of(t.session());
    REQUIRE(cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                    panel::kBuilder, sc)
                              .rect)
                .contains(grip.x + kWorkspaceX, grip.y + kWorkspaceY));
    const ui::Element before = *doc::find(t.doc(), 1);

    t.press(grip.x, grip.y);
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.resizing);
    CHECK_FALSE(t.session().drag.active);
    t.motion(grip.x + 6, grip.y + 2); // a resize that never began authors nothing
    CHECK(doc::find(t.doc(), 1)->width.amount == before.width.amount);
    CHECK(doc::find(t.doc(), 1)->height.amount == before.height.amount);
    t.release(grip.x + 6, grip.y + 2);

    // Remove the panel and the very same press is the resize it always was.
    pick(t, panel::kBuilder);
    t.press(grip.x, grip.y);
    CHECK(t.notice() == "holding #1 -- drag to resize it");
    CHECK(t.session().drag.resizing);
    t.motion(grip.x + 6, grip.y + 2);
    CHECK(doc::find(t.doc(), 1)->height.amount != before.height.amount);
    t.release(grip.x + 6, grip.y + 2);
}

TEST_CASE("Info's column occupies pointer space, and an object can walk under it") {
    // INFO PARTICIPATES IN THE SAME MODEL, and this is not a manufactured case:
    // the side region is outside the WORKSPACE's extent, but an authored position
    // is not bounded by that extent -- `check_coord` guards the first cell and
    // nothing guards the last -- so an object dragged or nudged rightwards walks
    // straight under the column, and before PNL-2 a press there took hold of it.
    Live t;
    const Screen sc = screen_of(t.session());
    const ui::Rect side =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc).rect);
    REQUIRE(side == ui::Rect{50, 0, 28, 17});

    // Walk #1 under the column with the pointer -- the press begins on the
    // workspace, so the gesture is not occluded, and the release lands INSIDE the
    // side region, which is the release-completes-a-gesture rule again.
    t.press(10, 4);
    REQUIRE(t.session().drag.active);
    t.motion(40, 4);
    t.motion(59, 4);
    t.release(59, 4);
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* under = ui::placed_for(scene, 1);
    REQUIRE(under != nullptr);
    CHECK(under->rect.x == 52);
    const std::int64_t ox = under->rect.x;
    const std::int64_t oy = under->rect.y;
    REQUIRE(side.contains(ox + kWorkspaceX, oy + kWorkspaceY));

    // IT IS THERE, AND IT IS OUT OF REACH.
    t.press(ox + 2, oy + 1);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.active);

    // Remove Info and the column is ordinary space again -- the object under it
    // was always there, and now a hand can reach it.
    pick(t, panel::kInfo);
    REQUIRE_FALSE(t.session().panels.has(panel::kInfo));
    t.press(ox + 2, oy + 1);
    CHECK(t.notice() == "holding #1 -- drag to move it");
    CHECK(t.session().drag.active);
    t.release(ox + 2, oy + 1);
}

// ----------------------------------------------------------------------------
// Tier 6b -- PNL-2a: a region a hand cannot reach through, an eye cannot either
// ----------------------------------------------------------------------------
//
// PNL-2 left exactly one thing measured and unrepaired. Occupancy is by BOUNDS;
// visibility was by whatever the painter happened to write -- so Info refused a
// press across the whole of 28x17 cells while showing the document through most
// of them. An object walked under the column painted its body and its selection
// ring straight through the panel, and the terminal medium read
// `*#####PROPERTIES#############*` on one row: one rectangle saying two things.
//
// The repair is one rect, from the primitive the Builder and the picker already
// take theirs from, at the bounds this painter was already handed. What the cases
// below pin is that it is the SAME rectangle -- not a second one that agrees today
// -- and that occlusion did NOT become a per-cell painted mask, which would have
// made what a maker can press depend on the length of a label.

TEST_CASE("Info's backdrop is the rectangle the pointer meets, on whatever screen it is") {
    Live t;
    t.key(input::scan::kN); // something to look at, so this is not the empty column
    const Screen sc = screen_of(t.session());
    const ui::Rect side =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc).rect);
    REQUIRE(side == ui::Rect{50, 0, 28, 17});

    // ONE backdrop, and it IS the bounds. An equality, so a panel that painted
    // most of its region -- which is the defect, one degree weaker -- cannot pass.
    const surface::SurfaceCanvas& c = t.canvases.back();
    CHECK(has_rect(c, side.x, side.y, side.w, side.h, surface::role::kMuted));
    std::size_t backdrops = 0;
    for (const surface::SurfaceRect& r : all_rects(c)) {
        if (r.x == side.x && r.y == side.y && r.w == side.w && r.h == side.h) {
            ++backdrops;
        }
    }
    CHECK(backdrops == 1);

    // The two answers stop together: both corners of the rectangle are occupied,
    // and the gutter cell one column to its left is neither painted by it nor
    // occupied. A backdrop that ran one cell wide of the bounds would say so here.
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, side.x, side.y).occupied);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, side.x + side.w - 1, side.y + side.h - 1).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, side.x - 1, side.y).occupied);

    // AND IT IS RESOLVED, NOT REMEMBERED. A bigger surface moves the side region;
    // the backdrop is at the new answer and is not at the old one. A second copy of
    // the geometry would still be painting the column a maker no longer has.
    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    const Screen big = screen_of(t.session());
    const ui::Rect moved =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, big).rect);
    REQUIRE(moved.x > side.x);
    const surface::SurfaceCanvas& after = t.canvases.back();
    CHECK(has_rect(after, moved.x, moved.y, moved.w, moved.h, surface::role::kMuted));
    CHECK_FALSE(has_rect(after, side.x, side.y, side.w, side.h, surface::role::kMuted));
}

TEST_CASE("what is inside Info's bounds is what Info painted, and nothing underneath it") {
    // THE PICTURE PNL-2 RECORDED, ASSERTED AWAY -- and this is the case that fails
    // the moment Info goes back to label-only painting.
    WorkshopDoc d;
    // Authored at 46 and 20 cells wide, this runs from the workspace, across the
    // two-cell gutter, and well into the side region -- so this is a region
    // something is genuinely trying to bleed into rather than one nothing reaches.
    // An authored position is not bounded by the workspace extent (PNL-2), which is
    // why a maker can reach this state with their own hand.
    doc::add(d, "under", 46, 3, ui::Extent{ui::kExtentCells, 20},
             ui::Extent{ui::kExtentCells, 5});
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);

    const Screen sc = screen_of(s);
    const ui::Rect side =
cells_covered(bounds_of(s.panels, s.setup.active, panel::kInfo, sc).rect);
    const ui::Scene scene = workspace_scene(d, s);
    REQUIRE(scene.items.size() == 1);
    const ui::Rect obj = scene.items[0].rect;
    REQUIRE(kWorkspaceX + obj.x + obj.w > side.x);

    const std::vector<std::string> screen = rasterized(paint(d, s));
    REQUIRE(screen.size() == static_cast<std::size_t>(sc.h));
    for (const std::string& line : screen) {
        REQUIRE(line.size() == static_cast<std::size_t>(sc.w));
    }

    // The object really is drawn right up to the panel's left edge...
    const std::size_t row = static_cast<std::size_t>(kWorkspaceY + obj.y + 1);
    CHECK(screen[row][static_cast<std::size_t>(side.x - 1)] == '#');

    // ...and inside the panel every cell is the cell INFO painted, compared against
    // one canvas holding nothing but this panel, through the same rasterizer.
    surface::SurfaceCanvas alone;
    alone.width = sc.w;
    alone.height = sc.h;
    paint_info(plane(alone), d, s, fine_of_cells(side), sc);
    const std::vector<std::string> only_info = rasterized(alone);
    REQUIRE(only_info.size() == screen.size());

    // ROW 0 IS THE ONE EXCEPTION AND IT IS NOT INFO'S DOING: `paint` writes the
    // screen's own `shift+space terminal` hint measured from the right edge, so it
    // lands on this region's top row beside OBJECTS. It survives because a backdrop
    // is a RECT and every label is drawn over every rect -- which is also the reason
    // Info's rows are not padded to its width the way a stacked panel's are.
    std::size_t compared = 0;
    std::size_t differed = 0;
    std::int64_t first_bad_x = -1;
    std::int64_t first_bad_y = -1;
    for (std::int64_t y = side.y + 1; y < side.y + side.h; ++y) {
        for (std::int64_t x = side.x; x < side.x + side.w; ++x) {
            ++compared;
            const std::size_t yi = static_cast<std::size_t>(y);
            const std::size_t xi = static_cast<std::size_t>(x);
            if (screen[yi][xi] != only_info[yi][xi]) {
                ++differed;
                if (first_bad_x < 0) {
                    first_bad_x = x;
                    first_bad_y = y;
                }
            }
        }
    }
    CHECK(compared == static_cast<std::size_t>((side.h - 1) * side.w));
    CHECK(first_bad_x == -1); // named, so a failure says WHICH cell showed through
    CHECK(first_bad_y == -1);
    CHECK(differed == 0);
    CHECK(screen[0].find("OBJECTS") != std::string::npos);
}

TEST_CASE("removing Info takes its backdrop with it, and reopening brings both back") {
    // A PANEL THAT IS NOT THERE HIDES NOTHING, which is the other half of the same
    // claim: the backdrop belongs to the panel, so it leaves when the panel does and
    // the document underneath is exactly the document it always was.
    Live t;
    const Screen sc = screen_of(t.session());
    const ui::Rect side =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc).rect);

    // Walk #1 under the column by hand, so the column has something to be hiding.
    t.press(10, 4);
    REQUIRE(t.session().drag.active);
    t.motion(40, 4);
    t.motion(59, 4);
    t.release(59, 4);
    // The scene is held in a named value: `placed_for` answers with a pointer INTO
    // it, so asking a temporary would hand back a pointer to something already gone.
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* under = ui::placed_for(scene, 1);
    REQUIRE(under != nullptr);
    const std::int64_t ox = kWorkspaceX + under->rect.x + 1;
    const std::int64_t oy = kWorkspaceY + under->rect.y + 1;
    REQUIRE(side.contains(ox, oy));
    const std::size_t row = static_cast<std::size_t>(oy);

    const std::string shown = info_text(t.canvases.back(), sc);
    REQUIRE(shown.find("OBJECTS") != std::string::npos);
    REQUIRE(has_rect(t.canvases.back(), side.x, side.y, side.w, side.h, surface::role::kMuted));
    // HIDDEN BY THE BODY, not by the backdrop's dot (HD-7). A region owns its interior and
    // writes every row it was given, so the cell over the object now carries whatever the
    // body's own row carries there rather than the panel's `.` fill. Either way the object
    // underneath is covered -- which is what this case is about -- and the check is against
    // what the panel PUBLISHED rather than against a character somebody expected.
    const auto body_char = [&](const surface::SurfaceCanvas& canvas) {
        const InfoBodyPlace place = body_place(t);
        const surface::SurfaceTextRegion* published = body_on(canvas, place);
        REQUIRE(published != nullptr);
        const std::size_t prose = static_cast<std::size_t>(oy - place.region_y);
        REQUIRE(prose < published->rows.size());
        const std::string& text = published->rows[prose].text;
        const std::size_t col = static_cast<std::size_t>(ox - place.region_x);
        return col < text.size() ? text[col] : ' ';
    };
    CHECK(rasterized(t.canvases.back())[row][static_cast<std::size_t>(ox)] ==
          body_char(t.canvases.back()));

    pick(t, panel::kInfo);
    REQUIRE_FALSE(t.session().panels.has(panel::kInfo));
    const surface::SurfaceCanvas& gone = t.canvases.back();
    CHECK_FALSE(has_rect(gone, side.x, side.y, side.w, side.h, surface::role::kMuted));
    CHECK(info_text(gone, sc).empty());
    // AND THE OBJECT IS BACK IN VIEW, on the very cell the panel was covering.
    CHECK(rasterized(gone)[row][static_cast<std::size_t>(ox)] == '#');

    pick(t, panel::kInfo);
    const surface::SurfaceCanvas& back = t.canvases.back();
    CHECK(info_text(back, sc) == shown);
    CHECK(has_rect(back, side.x, side.y, side.w, side.h, surface::role::kMuted));
    CHECK(rasterized(back)[row][static_cast<std::size_t>(ox)] == body_char(back));
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
    REQUIRE_FALSE(t.session().panels.has(panel::kBuilder)); // nothing else is in that slot

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
    open_builder(t);
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    const Screen sc = screen_of(t.session());
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, 10, 6).occupied);
    CHECK(std::string(occupied_at(t.session().panels, t.session().setup.active, sc, 10, 6).what) == kPickerName);
    CHECK(picker_bounds(sc) == bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect);
}

TEST_CASE("the terminal overlay outranks panels for the pointer too, and by a wider rule") {
    // A MODE, NOT A PLACE. While the pane is open the pointer does nothing
    // ANYWHERE -- inside a panel, outside every panel, on an object in the clear.
    // That ordering is unchanged by PNL-2 and is deliberately wider than
    // occupancy: a maker typing into the pane is not also authoring in the
    // workspace, and a maker with a panel open is.
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    t.toggle_terminal();
    REQUIRE(t.session().terminal.open);

    const std::string said = t.notice();
    const ui::Element before = *doc::find(t.doc(), 1);
    t.press(10, 5); // inside the Builder's bounds
    t.press(7, 11); // on #2, in the clear
    t.motion(9, 12);
    t.release(9, 12);
    CHECK(t.notice() == said); // not one of them wrote a word
    CHECK_FALSE(t.session().drag.active);
    CHECK(doc::find(t.doc(), 1)->x == before.x);

    // Closing it restores both the panel's occupancy and the workspace's.
    t.toggle_terminal();
    REQUIRE_FALSE(t.session().terminal.open);
    t.press(10, 5);
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    t.press(7, 11);
    CHECK(t.notice() == "holding #2 -- drag to move it");
    t.release(7, 11);
}

TEST_CASE("what a panel is painted at and what it occupies are one resolved truth") {
    // THE STRUCTURAL CLAIM, swept over every cell of the canvas rather than
    // sampled: the occupancy answer IS the union of the open panels' bounds, and
    // those are the same rectangles `paint_panels` hands the painters. There is no
    // second geometry to drift.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    const Screen sc = screen_of(t.session());
    const Panels& panels = t.session().panels;
    REQUIRE(panels.open.size() == 2);

    const Sweep swept = sweep_canvas(panels, sc);
    CHECK(swept.cells == static_cast<std::size_t>(sc.w * sc.h));
    CHECK(swept.first_bad_x == -1); // named, so a failure says WHICH cell disagreed
    CHECK(swept.first_bad_y == -1);
    CHECK(swept.disagreed == 0);
    // Both places, exactly: 48x9 of stack and 28x17 of side region.
    CHECK(swept.occupied == static_cast<std::size_t>(48 * 9 + 28 * 17));
    CHECK(swept.painted == swept.occupied);

    // AND WHAT THE PAINTERS ACTUALLY WROTE IS INSIDE THE SPACE THAT IS OCCUPIED --
    // painted through the same one call the screen makes, into a canvas holding
    // nothing else.
    surface::SurfaceCanvas only_panels;
    paint_panels(only_panels, t.doc(), t.session(), sc);
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
        CHECK(has_rect(only_panels, pb.x, pb.y, pb.w, pb.h, surface::role::kMuted));
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
    const Screen small = screen_of(t.session());
    const ui::Rect side_small =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, small).rect);
    REQUIRE(occupied_at(t.session().panels, t.session().setup.active, small, side_small.x, 4).occupied);

    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    const Screen big = screen_of(t.session());
    REQUIRE(big.w == 100);
    const ui::Rect side_big =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, big).rect);
    CHECK(side_big.x == big.panel_x);
    CHECK(side_big.x > side_small.x);

    // The column a maker presses is the column they can see, on either screen.
    CHECK(occupied_at(t.session().panels, t.session().setup.active, big, side_big.x, 4).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, big, side_small.x, 4).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, big, side_big.x - 1, 4).occupied);

    // AND THE PRESS FOLLOWS IT, through the whole live path: what was the Info
    // column on the small screen is ordinary workspace on the big one.
    t.press(side_small.x, 3);
    CHECK(t.notice() == "nothing there");
    t.press(side_big.x, 3);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
}

TEST_CASE("a closed panel occupies nothing, and neither does a screen with none open") {
    // THE OTHER HALF OF THE INVARIANT, and the one a maker feels every second: a
    // panel that is not open takes nothing away. `bounds_of` answers a closed
    // panel with an empty rectangle, `contains` says an empty rectangle holds
    // nothing, and the whole canvas is therefore the workspace's again.
    Live t;
    pick(t, panel::kInfo); // remove the one panel a fresh session opens with
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
    open_builder(t);
    t.press_px(10, 5);
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
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
    open_builder(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect stack =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect);
    const ui::Rect side =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc).rect);

    // The four edges of each place, in canvas cells: inside, then one cell out.
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, stack.x, stack.y + stack.h - 1).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, stack.x, stack.y + stack.h).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, stack.x, stack.y - 1).occupied);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, stack.x + stack.w - 1, stack.y).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, stack.x + stack.w, stack.y).occupied);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, side.x, side.y + side.h - 1).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, side.x - 1, side.y).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, side.x, side.y + side.h).occupied);

    // AND THROUGH THE LIVE PRESS, which is the half that would catch a question
    // asked in the wrong space: the row under the panel is empty workspace, and
    // it says the empty-workspace sentence rather than the panel's.
    const std::int64_t below = stack.y + stack.h - kWorkspaceY; // workspace row under it
    REQUIRE_FALSE(stack.contains(10 + kWorkspaceX, below + kWorkspaceY));
    t.press(10, below);
    CHECK(t.notice() == "nothing there");
    t.press(10, below - 1); // the panel's own last row
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
}

// ---- HD-3: the caret, and the pointer inside the pane -----------------------------------
//
// Everything below drives the same functions `workshop.cpp` binds and the same functions
// `paint_terminal` draws with. There is no second geometry anywhere in these cases: where a
// case needs a pixel it computes it from `terminal_input_place`, which is the function under
// test as well as the one the painter uses -- so a case that agreed with a private copy of
// the arithmetic would be agreeing with itself.

/// The window pixel at the middle of a given prose column/row of the pane's own region.
/// Deliberately the INVERSE of `terminal_input_place`'s resolution rather than a second
/// spelling of it: a helper that recomputed the layout could be wrong in the same direction
/// as the code it is checking.
static std::int64_t pane_pixel_x(const TerminalInputPlace& p, std::int64_t column) {
    return p.region_x * surface::kCanvasCellPx + p.fit.origin_x + column * p.fit.advance_px +
           p.fit.advance_px / 2;
}
static std::int64_t pane_pixel_y(const TerminalInputPlace& p, std::int64_t row) {
    return p.region_y * surface::kCanvasCellPx + p.fit.origin_y + row * p.fit.line_px +
           p.fit.line_px / 2;
}

TEST_CASE("caret geometry: a byte index and a prose column are one number, both ways") {
    // §22. PURE ARITHMETIC, across four faces rather than the live 8x18 one -- what is being
    // pinned is the mapping, and a case written against a single metric would pass for a
    // reason it does not claim. JetBrains Mono's raster is not pinned anywhere here.
    struct Face {
        std::int64_t advance;
        std::int64_t line;
    };
    for (const Face f : {Face{0, 0}, Face{6, 14}, Face{8, 18}, Face{11, 23}, Face{15, 31}}) {
        const Screen sc = screen_of(kScreenMinW, kScreenMinH, f.advance, f.line);
        const TerminalInputPlace p = terminal_input_place(sc);
        CAPTURE(f.advance);

        // THE LINE IS THE PANE'S LAST PROSE ROW, whichever lattice this medium counts in.
        CHECK(p.prose_row == static_cast<std::int64_t>(sc.terminal_lines) - 1);
        CHECK(p.region_x == sc.terminal_x);
        CHECK(p.region_y == sc.terminal_y);
        CHECK(p.first_column == kTerminalPromptCols);
        CHECK(p.columns == sc.terminal_cols - kTerminalPromptCols - kTerminalCaretCols);

        // CARET 0, MIDDLE, END -- one column per byte, offset by the prompt.
        CHECK(terminal_caret_column(p, box_of(0, 0, 0)) == kTerminalPromptCols);
        CHECK(terminal_caret_column(p, box_of(5, 5, 0)) == kTerminalPromptCols + 5);
        CHECK(terminal_caret_column(p, box_of(40, 40, 0)) == kTerminalPromptCols + 40);

        // ...AND BACK. The two are inverses over every position a line of this length has,
        // which is the property a click-then-type depends on.
        for (std::size_t at = 0; at <= 12; ++at) {
            CHECK(terminal_caret_of_column(p, box_of(12, 12, 0),
                                          terminal_caret_column(p, box_of(at, at, 0))) == at);
        }

        // THE BOUNDARIES, written down rather than left to the arithmetic.
        CHECK(terminal_caret_of_column(p, box_of(12, 12, 0), 0) == 0);                     // before the prompt
        CHECK(terminal_caret_of_column(p, box_of(12, 12, 0), 1) == 0);                     // inside the prompt
        CHECK(terminal_caret_of_column(p, box_of(12, 12, 0), -400) == 0);                  // far to the left
        CHECK(terminal_caret_of_column(p, box_of(12, 12, 0), kTerminalPromptCols + 99) == 12); // past the text
        CHECK(terminal_caret_of_column(p, box_of(0, 0, 0), kTerminalPromptCols) == 0);    // an empty line

        // AN EMPTY LINE HAS EXACTLY ONE POSITION, and every column on the row names it.
        for (std::int64_t col = -3; col < 20; ++col) {
            CHECK(terminal_caret_of_column(p, box_of(0, 0, 0), col) == 0);
        }

        // THE ROW IS THE WHOLE HIT TEST, and the column deliberately is not: a maker aiming
        // past the end of a short command clicks empty room, and that is the most obvious
        // gesture the row has.
        CHECK(terminal_input_hit(p, kTerminalPromptCols, p.prose_row));
        CHECK(terminal_input_hit(p, 0, p.prose_row));
        CHECK(terminal_input_hit(p, p.fit.columns, p.prose_row));
        CHECK_FALSE(terminal_input_hit(p, kTerminalPromptCols, p.prose_row - 1));
        CHECK_FALSE(terminal_input_hit(p, kTerminalPromptCols, 0));
        CHECK_FALSE(terminal_input_hit(p, -1, p.prose_row));
        CHECK_FALSE(terminal_input_hit(p, p.fit.columns + 1, p.prose_row));
    }

    // TOTAL over the number line on both sides: a column arrives from a pointer off the bus
    // and a length from a line a maker typed.
    const TerminalInputPlace p = terminal_input_place(screen_of(kScreenMinW, kScreenMinH, 8, 18));
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    CHECK(terminal_caret_of_column(p, box_of(5, 5, 0), kMin) == 0);
    CHECK(terminal_caret_of_column(p, box_of(5, 5, 0), kMax) == 5);
    CHECK(terminal_caret_column(p, box_of(0, 0, 0)) == kTerminalPromptCols);
}

TEST_CASE("HD-3: typing lands at the caret, and submission does not depend on where it is") {
    // §24, driven end to end through the real weave: keys in, a line out.
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("send * SurfaceText 1 slot=s")) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.pane().input.text() == "send * SurfaceText 1 slot=s");
    REQUIRE(t.pane().input.caret() == t.pane().input.size());

    // FIVE STEPS BACK, then repair the middle -- the workflow this phase exists for.
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kLeft);
    }
    CHECK(t.pane().input.caret() == t.pane().input.size() - 5);
    t.text("X");
    CHECK(t.pane().input.text() == "send * SurfaceText 1 sXlot=s"); // at byte 22, inside `slot`
    t.key(input::scan::kBackspace);
    CHECK(t.pane().input.text() == "send * SurfaceText 1 slot=s");

    // DELETE ERASES FORWARD AND HOME/END JUMP -- all three arrive on the SDL wire today
    // (translate_sdl passes SDL's scancode through), which is why they are bound.
    t.key(input::scan::kHome);
    CHECK(t.pane().input.caret() == 0);
    t.key(input::scan::kDelete);
    CHECK(t.pane().input.text() == "end * SurfaceText 1 slot=s");
    t.text("s");
    CHECK(t.pane().input.text() == "send * SurfaceText 1 slot=s");
    t.key(input::scan::kEnd);
    CHECK(t.pane().input.at_end());

    // SUBMISSION IS THE WHOLE LINE, EXACTLY ONCE, whatever the caret was doing. Return is
    // not "submit up to the caret" -- which is what makes a mis-typed middle repairable
    // without the repair changing what gets sent.
    t.key(input::scan::kHome);
    for (int i = 0; i < 4; ++i) {
        t.key(input::scan::kRight);
    }
    REQUIRE(t.pane().input.caret() == 4);
    const std::size_t before = me->transcript().size();
    t.key(input::scan::kReturn);
    CHECK(t.pane().input.empty());
    CHECK(t.pane().input.caret() == 0);
    std::size_t submitted = 0;
    for (const loom::TranscriptEntry& e : me->transcript().entries()) {
        if (e.text.find("send * SurfaceText 1 slot=s") != std::string::npos) {
            ++submitted;
        }
    }
    CHECK(submitted == 1);
    CHECK(me->transcript().size() > before);
}

TEST_CASE("HD-3: a press on the input row places the caret where the maker aimed") {
    // §9 and §23, in RAW PIXELS through the real bus -- the sub-cell precision the wire has
    // carried since G-1 is finally the thing being spent, and it is spent without rounding
    // to a cell first.
    Live t;
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.toggle_terminal();
    for (const char c : std::string("send @builder BuildRequsted")) {
        t.text(std::string(1, c));
    }
    const Screen sc = screen_of(t.session());
    REQUIRE(sc.text_advance_px == 8);
    const TerminalInputPlace p = terminal_input_place(sc);
    REQUIRE(p.fit.graphical());

    // ON A CHARACTER: the caret goes before the character that was pressed. `BuildRequsted`
    // starts at byte 14, so the missing `e` belongs at byte 23 -- before the `s` of `sted`.
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 23), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == 23);

    // ...AND TYPING THERE REPAIRS THE MIDDLE OF THE LINE WITHOUT CLEARING IT. This is §25's
    // acceptance workflow, in one line of test.
    t.text("e");
    CHECK(t.pane().input.text() == "send @builder BuildRequested");
    CHECK(t.pane().input.caret() == 24);

    // Backspace and Delete both work from where the pointer put it.
    t.key(input::scan::kBackspace);
    CHECK(t.pane().input.text() == "send @builder BuildRequsted");
    t.key(input::scan::kDelete);
    CHECK(t.pane().input.text() == "send @builder BuildRequted");
    t.text("s");
    t.text("e");
    CHECK(t.pane().input.text() == "send @builder BuildRequseted");
    // ...and back to the command a maker actually meant, from the middle, without the line
    // ever having been cleared.
    t.key(input::scan::kBackspace);
    t.key(input::scan::kBackspace);
    REQUIRE(t.pane().input.text() == "send @builder BuildRequted");
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 23), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    t.text("e");
    t.text("s");
    CHECK(t.pane().input.text() == "send @builder BuildRequested");

    // BEFORE THE FIRST CHARACTER -> 0, including the inset and the prompt.
    t.press_at(pane_pixel_x(p, 0), pane_pixel_y(p, p.prose_row), input::space::kPixels);
    CHECK(t.pane().input.caret() == 0);
    t.press_at(p.region_x * surface::kCanvasCellPx, pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == 0);

    // AFTER THE LAST CHARACTER -> the end of the line, wherever on the row it landed.
    t.press_at(pane_pixel_x(p, p.fit.columns), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == t.pane().input.size());

    // A PRESS ON ANOTHER ROW OF THE PANE DOES NOT MOVE IT. The pane is one region; only its
    // last prose row is the line.
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 3), pane_pixel_y(p, 0),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == t.pane().input.size());
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 3), pane_pixel_y(p, p.prose_row - 3),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == t.pane().input.size());

    // ...and nor does a press outside the pane entirely, which is the same answer for a
    // different reason: it is consumed by the MODE and reaches none of its regions.
    t.press_at(4, 4, input::space::kPixels);
    CHECK(t.pane().input.caret() == t.pane().input.size());
}

TEST_CASE("HD-3: a press in the pane never reaches the workspace underneath it") {
    // §12 and §23's click-through proof. The pane covers the bottom-right of the screen,
    // workspace included, so this is not hypothetical: these presses land on cells that hold
    // real authored objects.
    Live t;
    (void)t.mount_terminal();
    const std::int64_t was_x = t.first()->x;
    const std::int64_t was_y = t.first()->y;
    const std::int64_t was_selected = t.session().selected;
    const std::string was_notice = t.notice();
    t.toggle_terminal();

    // EVERY PART OF THE SCREEN, with the pane open: on the pane, off the pane, on the
    // workspace, on an object, on the side panel. None of them may author or select.
    const Screen sc = screen_of(t.session());
    for (const std::pair<std::int64_t, std::int64_t>& cell :
         {std::pair<std::int64_t, std::int64_t>{sc.terminal_x + 4, sc.terminal_y + 4},
          {sc.terminal_x, sc.terminal_y},
          {2, 2},
          {was_x, was_y},
          {sc.panel_x + 2, 3},
          {0, 0}}) {
        t.publish(loom::to_value(input::PointerButton{
            1, true, cell.first, cell.second + surface::kTuiCanvasTopRow, input::space::kCells,
            input::mod::kNone}));
        t.publish(loom::to_value(input::PointerButton{
            1, false, cell.first, cell.second + surface::kTuiCanvasTopRow,
            input::space::kCells, input::mod::kNone}));
    }
    CHECK(t.first()->x == was_x);
    CHECK(t.first()->y == was_y);
    CHECK(t.session().selected == was_selected);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice() == was_notice); // not even a sentence: the workspace was never asked

    // MOTION IS STILL NOTHING AT ALL, which is what keeps a moved mouse from costing a
    // repaint (§13, §28). Measured as canvases published rather than argued.
    const std::size_t frames = t.canvases.size();
    for (std::int64_t i = 0; i < 20; ++i) {
        t.motion(i, i);
    }
    CHECK(t.canvases.size() == frames);

    // ...and closing the pane restores every gesture exactly.
    t.toggle_terminal();
    t.press(was_x + 1, was_y + 1);
    CHECK(t.session().drag.active);
    t.release(was_x + 1, was_y + 1);
}

TEST_CASE("HD-3: opening the pane mid-drag does not strand the gesture") {
    // REPRODUCED ON THE PRISTINE TREE FIRST (constitution rule 7), where the second check
    // below moved the object from x=3 to x=22 on a motion with no button held. PNL-2 already
    // wrote the rule this repairs: "a gesture that began on the workspace owns the pointer
    // until it ends, so its release must end it wherever the maker's hand happens to be --
    // occluding the release would strand `drag.active` true with the button up". The overlay
    // was occluding it by arriving first.
    Live t;
    (void)t.mount_terminal();
    const std::int64_t x = t.first()->x;
    const std::int64_t y = t.first()->y;
    t.press(x + 1, y + 1);
    REQUIRE(t.session().drag.active);

    t.toggle_terminal(); // shift+space, with the button still down
    REQUIRE(t.session().terminal.open);
    t.release(x + 1, y + 1);
    CHECK_FALSE(t.session().drag.active);

    // THE PROOF THAT IT MATTERS: with the pane closed again, a bare motion moves nothing,
    // because nobody is holding anything.
    t.toggle_terminal();
    const std::int64_t settled = t.first()->x;
    t.motion(x + 20, y + 1);
    CHECK(t.first()->x == settled);

    // ...AND THE OTHER DIRECTION IS UNTOUCHED (PNL-2): a drag that began on the workspace and
    // was never interrupted still ends where the hand is, panel or no panel.
    t.press(x + 1, y + 1);
    REQUIRE(t.session().drag.active);
    t.motion(x + 4, y + 1);
    t.release(x + 4, y + 1);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice().rfind("released #", 0) == 0);
}

TEST_CASE("HD-3: clicking a completion row selects it, and Tab accepts what was clicked") {
    // §10. ONE SELECTION, WHICHEVER HAND MOVED IT -- the click writes the field Up/Down
    // write, so `accept_completion` needs to know nothing about a pointer and the renderer
    // cannot tell which gesture chose the row.
    Live t;
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{115, 63, 8, 18}));
    t.toggle_terminal();
    t.text("s");
    t.text("e");
    t.text("n");
    t.text("d");
    t.text(" ");
    REQUIRE(t.pane().completion.open);
    REQUIRE(t.pane().completion.candidates.size() >= 3);
    REQUIRE(t.pane().completion.selected == 0);

    const Screen sc = screen_of(t.session());
    const CompletionPlace list =
        completion_place(sc, t.pane().completion.candidates.size() + 1);
    REQUIRE(list.visible);
    REQUIRE(list.rows >= 3); // heading plus at least two candidates
    const surface::RegionFit fit = surface::fit_region(list.x, list.y, list.w, list.h,
                                                       sc.text_advance_px, sc.text_line_px);
    const auto row_pixel_y = [&](std::int64_t r) {
        return list.y * surface::kCanvasCellPx + fit.origin_y + r * fit.line_px +
               fit.line_px / 2;
    };
    const auto row_pixel_x = [&](std::int64_t col) {
        return list.x * surface::kCanvasCellPx + fit.origin_x + col * fit.advance_px +
               fit.advance_px / 2;
    };

    // ROW 2 IS THE SECOND CANDIDATE (row 0 is the heading), and clicking it selects it.
    t.press_at(row_pixel_x(4), row_pixel_y(2), input::space::kPixels);
    CHECK(t.pane().completion.selected == 1);
    t.press_at(row_pixel_x(4), row_pixel_y(1), input::space::kPixels);
    CHECK(t.pane().completion.selected == 0);

    // THE HEADING IS NOT A CANDIDATE. A press on it is consumed by the list and changes
    // nothing -- and in particular does not fall through to the input line, which is not
    // underneath it at all.
    t.press_at(row_pixel_x(4), row_pixel_y(0), input::space::kPixels);
    CHECK(t.pane().completion.selected == 0);
    CHECK(t.pane().input.text() == "send ");
    CHECK(t.pane().input.at_end());

    // CLICK SELECTS; TAB ACCEPTS. The smallest unsurprising contract -- no double-click
    // machinery, no mouse-only acceptance path, and the acceptance is the one HD-2 shipped.
    t.press_at(row_pixel_x(4), row_pixel_y(2), input::space::kPixels);
    REQUIRE(t.pane().completion.selected == 1);
    const std::string wanted = t.pane().completion.candidates[1].insert;
    t.key(input::scan::kTab);
    CHECK(t.pane().input.text() == "send " + wanted);
    CHECK(t.pane().input.at_end()); // §14: the caret follows the inserted result

    // A ROW THE LIST DOES NOT HAVE selects nothing and authors nothing.
    const std::size_t chosen = t.pane().completion.selected;
    t.press_at(row_pixel_x(4), row_pixel_y(static_cast<std::int64_t>(list.rows) + 40),
               input::space::kPixels);
    CHECK(t.pane().completion.selected == chosen);
}

TEST_CASE("HD-3: the click reads the SAME window the rows were drawn with") {
    // §10's real risk, and the reason `completion_first_shown` was lifted out of
    // `completion_rows`: once the list has SCROLLED, "row 1" is not candidate 0. A second
    // copy of the windowing would be right until the first scroll and wrong afterwards --
    // which is to say wrong only when nobody is looking for it.
    for (std::size_t capacity : {std::size_t{1}, std::size_t{2}, std::size_t{4},
                                 std::size_t{9}}) {
        CAPTURE(capacity);
        for (std::size_t selected = 0; selected < 12; ++selected) {
            const std::size_t first = completion_first_shown(selected, capacity);
            if (capacity <= 1) {
                CHECK(first == 0); // no candidate row at all: the heading is the whole list
                continue;
            }
            const std::size_t room = capacity - 1;
            // THE SELECTED CANDIDATE IS ALWAYS ON A VISIBLE ROW, which is the property the
            // hit test inverts.
            CHECK(selected >= first);
            CHECK(selected < first + room);
        }
    }

    // ...and the inversion agrees with the rows actually produced, at every selection.
    Completion comp;
    comp.open = true;
    comp.heading = "shapes";
    for (int i = 0; i < 9; ++i) {
        Candidate c;
        c.display = "cand" + std::to_string(i);
        c.insert = "cand" + std::to_string(i) + " ";
        comp.candidates.push_back(c);
    }
    for (std::size_t sel = 0; sel < comp.candidates.size(); ++sel) {
        comp.selected = sel;
        const std::size_t capacity = 4;
        const std::vector<surface::SurfaceTextRow> rows = completion_rows(comp, capacity, 60);
        const std::size_t first = completion_first_shown(sel, capacity);
        REQUIRE(rows.size() >= 2);
        for (std::size_t r = 1; r < rows.size(); ++r) {
            // The row a press on visible row `r` would resolve to is the row whose text is
            // actually there.
            const std::size_t candidate = first + (r - 1);
            REQUIRE(candidate < comp.candidates.size());
            CHECK(rows[r].text.find(comp.candidates[candidate].display) != std::string::npos);
        }
    }
}

TEST_CASE("HD-3: a click on a SCROLLED list picks the row that is actually there") {
    // THE CASE THE WINDOWING EXISTS FOR, and the one a first=0 hit test passes without.
    // In a large pane the list holds every candidate, so "row 1 is candidate 0" is true by
    // accident; here the pane is the minimum one and the list is shorter than the vocabulary,
    // so the window has to slide and row 1 stops being candidate 0.
    Live t;
    (void)t.mount_terminal(/*widen=*/false, /*shapes=*/8);
    t.toggle_terminal(); // no SurfaceExtent: the minimum pane, a character IS a cell
    for (const char c : std::string("send * ")) {
        t.text(std::string(1, c));
    }
    const std::size_t n = t.pane().completion.candidates.size();
    REQUIRE(n >= 3);
    const Screen sc = screen_of(t.session());
    const CompletionPlace list = completion_place(sc, n + 1);
    REQUIRE(list.visible);
    REQUIRE(list.rows < n + 1); // it must SCROLL, or this case proves nothing
    const std::size_t room = list.rows - 1;

    // Drive the selection to the last candidate: the window has slid as far as it goes.
    for (std::size_t i = 0; i + 1 < n; ++i) {
        t.key(input::scan::kDown);
    }
    REQUIRE(t.pane().completion.selected == n - 1);
    const std::size_t first = completion_first_shown(n - 1, list.rows);
    REQUIRE(first > 0); // ...and it is no longer showing candidate 0

    // THE ROWS SAY WHICH CANDIDATES ARE THERE, and the top visible one is `first`.
    const std::vector<surface::SurfaceTextRow> rows =
        completion_rows(t.pane().completion, list.rows, sc.terminal_cols);
    REQUIRE(rows.size() == list.rows);
    CHECK(rows[1].text.find(t.pane().completion.candidates[first].display) !=
          std::string::npos);

    // ...so a press on visible row 1 must select candidate `first`, not candidate 0. That is
    // the assertion a hit test which ignored the window would fail, and it can only fail
    // once the list has scrolled -- which is to say only when nobody is looking for it.
    t.publish(loom::to_value(input::PointerButton{
        1, true, list.x + 4, list.y + 1 + surface::kTuiCanvasTopRow, input::space::kCells,
        input::mod::kNone}));
    CHECK(t.pane().completion.selected == first);
    CHECK(t.pane().completion.selected != 0);

    // EVERY VISIBLE ROW, not only the first: the mapping is the whole window, inverted.
    for (std::size_t r = 1; r <= room && first + (r - 1) < n; ++r) {
        const std::size_t before = t.pane().completion.selected;
        const std::size_t window = completion_first_shown(before, list.rows);
        t.publish(loom::to_value(input::PointerButton{
            1, true, list.x + 4, list.y + static_cast<std::int64_t>(r) + surface::kTuiCanvasTopRow,
            input::space::kCells, input::mod::kNone}));
        CHECK(t.pane().completion.selected == window + (r - 1));
    }
}

TEST_CASE("HD-3: completion follows the END of the line, and says so when it cannot") {
    // §5. HD-2's completer rests on an assumption that was free when the caret could not
    // move -- the token being completed is the LAST one, so accepting is "drop what has been
    // typed of this token, append what it was going to be". With the caret in the middle,
    // that edit would delete everything after it. The smallest honest policy is to say so.
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{115, 63, 8, 18}));
    t.toggle_terminal();
    for (const char c : std::string("send * Surface")) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.pane().completion.open);
    REQUIRE_FALSE(t.pane().completion.candidates.empty());
    const std::size_t offered = t.pane().completion.candidates.size();

    // MOVE THE CARET OFF THE END, and the list stops answering a question the maker is no
    // longer standing in.
    t.key(input::scan::kLeft);
    CHECK(t.pane().completion.open); // it still SAYS something...
    CHECK(t.pane().completion.candidates.empty()); // ...and it is not a candidate
    CHECK(t.pane().completion.heading.find("END of the line") != std::string::npos);

    // A HEADING WITH NO CANDIDATES IS EXACTLY HD-2's SHAPE, so every gesture that depends on
    // "is there something to choose" is unchanged: Tab does not accept, Escape still clears.
    t.key(input::scan::kTab);
    CHECK(t.pane().input.text() == "send * Surface"); // nothing was accepted
    t.key(input::scan::kUp);
    CHECK(t.pane().completion.selected == 0);

    // BACK TO THE END, and the same candidates come back -- the policy is about WHERE the
    // caret is, and holds nothing of its own.
    t.key(input::scan::kEnd);
    CHECK(t.pane().completion.candidates.size() == offered);
    CHECK(t.pane().completion.heading.find("END of the line") == std::string::npos);

    // A CLICK MOVES IT THE SAME WAY A KEY DOES -- one state, two hands.
    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 2), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    REQUIRE(t.pane().input.caret() == 2);
    CHECK(t.pane().completion.candidates.empty());
    t.press_at(pane_pixel_x(p, p.fit.columns), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    CHECK(t.pane().input.at_end());
    CHECK(t.pane().completion.candidates.size() == offered);

    // AND NONE OF IT AUTHORS ANYTHING (§23, §20). Browsing, clicking, moving the caret --
    // measured by SENDER, because Workshop's own weave publishes to the same office on every
    // repaint.
    std::size_t authored = 0;
    for (const loom::TranscriptEntry& e : me->transcript().entries()) {
        (void)e;
        ++authored;
    }
    CHECK(authored == 0);
    CHECK(me->pending().empty());
    CHECK_FALSE(me->awaiting());
}

TEST_CASE("HD-3: the pane publishes a caret, and both media answer it in their own type") {
    // §6. The publisher stopped appending `_`; the position is a fact ABOUT the region now,
    // so a window fills a bar and a cell medium inserts a character -- and for a caret at the
    // end of the line the character medium's answer is byte-for-byte the row that was there
    // before this phase.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("send * S")) {
        t.text(std::string(1, c));
    }
    // ESCAPE DISMISSES THE LIST AND LEAVES THE LINE (HD-2), which is why the prefix is one
    // the catalog actually answers: over a prefix that matched nothing the list is
    // heading-only, and Escape goes on meaning "clear the line" there.
    REQUIRE_FALSE(t.pane().completion.candidates.empty());
    t.key(input::scan::kEscape);
    REQUIRE(t.pane().input.text() == "send * S");

    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_row == static_cast<std::int64_t>(pane.rows.size()) - 1);
        CHECK(pane.caret_col == kTerminalPromptCols + 8);
        CHECK(pane.rows.back().text == "> send * S");
        // The pane alone: the list and the property body are other presentations.
        const surface::SurfaceCanvas only_pane = canvas_of_region(t.canvases.back(), pane);
        const std::vector<surface::ProjectedRow> shown =
            projected_of(only_pane);
        CHECK(shown.back().label.text.rfind("> send * S_", 0) == 0);
    }

    // IN THE MIDDLE: the region says where, the row does not move, and the cell projection
    // puts the mark between the two characters the next keystroke lands between.
    t.key(input::scan::kLeft);
    t.key(input::scan::kLeft);
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col == kTerminalPromptCols + 6);
        CHECK(pane.rows.back().text == "> send * S"); // the LINE is unchanged
        // The pane alone: the list and the property body are other presentations.
        const surface::SurfaceCanvas only_pane = canvas_of_region(t.canvases.back(), pane);
        const std::vector<surface::ProjectedRow> shown =
            projected_of(only_pane);
        CHECK(shown.back().label.text.rfind("> send *_ S", 0) == 0);
    }

    // AN EMPTY LINE still names the gesture that asks, and the caret sits at its start --
    // which is the identical row HD-2 published, reassembled from two facts instead of one.
    t.key(input::scan::kEscape);
    REQUIRE(t.pane().input.empty());
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col == kTerminalPromptCols);
        // The pane alone: the list and the property body are other presentations.
        const surface::SurfaceCanvas only_pane = canvas_of_region(t.canvases.back(), pane);
        const std::vector<surface::ProjectedRow> shown =
            projected_of(only_pane);
        CHECK(shown.back().label.text.rfind("> _   tab: what can this terminal say?", 0) == 0);
    }

    // THE COMPLETION LIST CARRIES NO CARET. It is a second region of the same pane and it is
    // not where anybody is typing -- said here because "there is exactly one caret on this
    // canvas" is the closest thing this application has to a focus fact, and it is a
    // consequence of who publishes rather than of a service that arbitrates.
    t.text("s");
    const Screen sc = screen_of(t.session());
    const surface::SurfaceTextRegion* list = list_of(t.canvases.back(), sc);
    REQUIRE(list != nullptr);
    CHECK(pane_of(t.canvases.back(), sc)->caret_row >= 0);
    CHECK(list->caret_row == surface::kNoCaret);
    // ...AND NEITHER DOES THE INSPECTOR'S PROPERTY BODY while the pane has the keys, which
    // is what "exactly one caret on this canvas" means now that there are three regions.
    CHECK(all_texts(t.canvases.back()).front().caret_row == surface::kNoCaret);
}

TEST_CASE("HD-3: hit geometry follows presentation geometry across a resize") {
    // §25 item 12 and the report's resize witness: the same click lands on the same
    // character before and after the window changes size, because both the picture and the
    // hit test are resolved from the one `Screen` the medium's extent produced.
    Live t;
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.toggle_terminal();
    for (const char c : std::string("send * SurfaceText")) {
        t.text(std::string(1, c));
    }

    for (const surface::SurfaceExtent& e :
         {surface::SurfaceExtent{78, 22, 8, 18}, surface::SurfaceExtent{115, 63, 8, 18},
          surface::SurfaceExtent{140, 40, 11, 23}, surface::SurfaceExtent{78, 22, 0, 0}}) {
        t.publish(loom::to_value(e));
        const Screen sc = screen_of(t.session());
        const TerminalInputPlace p = terminal_input_place(sc);
        CAPTURE(sc.terminal_x);
        CAPTURE(sc.text_advance_px);

        // THE PANE ITSELF MOVED -- this is not a case where nothing changed.
        CHECK(p.region_x == sc.terminal_x);

        for (const std::size_t want : {std::size_t{0}, std::size_t{7}, std::size_t{18}}) {
            // THE WINDOW THE PANE ACTUALLY HOLDS, never a literal zero (HD-4): this line is
            // short enough not to scroll at any of these extents, and reading the offset
            // rather than assuming it is what makes that a fact of the run instead of an
            // assumption of the case.
            const std::size_t from = t.pane().input.first_visible();
            if (p.fit.graphical()) {
                t.press_at(pane_pixel_x(p, terminal_caret_column(p, box_of(want, want, from))),
                           pane_pixel_y(p, p.prose_row), input::space::kPixels);
            } else {
                // No metric: a character IS a cell, and the medium reports cells.
                t.press_at(p.region_x + terminal_caret_column(p, box_of(want, want, from)),
                           p.region_y + p.prose_row + surface::kTuiCanvasTopRow,
                           input::space::kCells);
            }
            CHECK(t.pane().input.caret() == want);
        }

        // ...and the CARET THE PANE DREW is at the column the press resolved to, which is
        // the whole of "the geometry that drew it is the geometry that hit it".
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col == terminal_caret_column(p, t.pane().input));
        CHECK(pane.caret_row == p.prose_row);
    }
}

TEST_CASE("HD-3: a terminal medium's press reaches the same local hit model") {
    // §17. The graphical lane is where this phase closes, but the routing is stamped with
    // `space` rather than with a backend identity -- so a cell-reporting medium takes the
    // other branch of `prose_at` and lands on the same caret. Whether a terminal BACKEND can
    // report a press is a separate question this phase did not touch (no escape-sequence
    // work, no terminal mouse protocol); what is proven here is that the Terminal's own hit
    // model does not stand in the way.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal(); // no SurfaceExtent: a character IS a cell here
    for (const char c : std::string("send * Ping")) {
        t.text(std::string(1, c));
    }
    const Screen sc = screen_of(t.session());
    REQUIRE(sc.text_advance_px == 0);
    const TerminalInputPlace p = terminal_input_place(sc);
    REQUIRE_FALSE(p.fit.graphical());

    for (const std::size_t want : {std::size_t{0}, std::size_t{4}, std::size_t{11}}) {
        t.publish(loom::to_value(input::PointerButton{
            1, true, p.region_x + terminal_caret_column(p, box_of(want, want, t.pane().input.first_visible())),
            p.region_y + p.prose_row + surface::kTuiCanvasTopRow, input::space::kCells,
            input::mod::kNone}));
        CHECK(t.pane().input.caret() == want);
    }

    // A SPACE THIS APPLICATION DOES NOT RECOGNISE MOVES NOTHING, and is still consumed by
    // the mode -- guessing is how a click lands twelve cells from where a maker pointed.
    const std::size_t held = t.pane().input.caret();
    t.publish(loom::to_value(input::PointerButton{1, true, 4, 4, input::space::kUnknown,
                                                   input::mod::kNone}));
    CHECK(t.pane().input.caret() == held);
}

// ---- HD-4: the input line's horizontal viewport ------------------------------------------
//
// Everything below drives the same three answers the application does: `component::TextBox`'s own
// window, `terminal_input_place`'s capacity, and the pair of pure functions that turn a byte
// index into a column and back. There is no second scroll model anywhere in these cases --
// where a case needs a column it asks the function the painter asks.

/// A REAL COMMAND LONGER THAN THE ROW, at every extent these cases use. Eighty-eight bytes:
/// long enough to scroll in the pane's widest configuration here and in its narrowest, which
/// is the difference between a case that watches the scrolling and one that passes because
/// the string happened to fit (§25).
static const std::string kLongLine =
    "send @zengine.skin SurfaceText 1 slot=1 text=the quick brown fox jumps over the lazy dog";

TEST_CASE("HD-4: a long line is shown as a slice, and both media draw the caret against it") {
    // §5, §7 and §11. The authored command is untouched behind the window; the row carries
    // the part that fits; the published caret column is relative to THAT, and the cell
    // projection's `_` lands in the same place a window's bar would.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal(); // no SurfaceExtent: a character IS a cell, 45 columns of line
    for (const char c : kLongLine) {
        t.text(std::string(1, c));
    }
    REQUIRE(kLongLine.size() == 88);

    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    // FORTY-FIVE, and it was fifty-three until HD-10 stopped the pane reaching into the
    // reserved side column: 48 cells of pane, less the prompt's two and the caret's one.
    REQUIRE(p.columns == 45);
    REQUIRE(t.pane().input.text() == kLongLine); // the whole command is still here
    REQUIRE(t.pane().input.at_end());
    REQUIRE(t.pane().input.first_visible() == 43); // 88 - 45, the tail

    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.rows.back().text == "> t=the quick brown fox jumps over the lazy dog");
        CHECK(pane.caret_col == kTerminalPromptCols + 45);
        CHECK(pane.caret_row == p.prose_row);
        // THE ROW IS SHORTER THAN THE REGION BY EXACTLY THE CARET'S OWN COLUMN, which is what
        // `kTerminalCaretCols` buys: in a cell medium the mark is a character and the last
        // cell has to be free for it.
        CHECK(static_cast<std::int64_t>(pane.rows.back().text.size()) == sc.terminal_cols - 1);

        // The pane alone: the list and the property body are other presentations.
        const surface::SurfaceCanvas only_pane = canvas_of_region(t.canvases.back(), pane);
        const std::vector<surface::ProjectedRow> shown =
            projected_of(only_pane);
        CHECK(shown[static_cast<std::size_t>(p.prose_row)].label.text ==
              "> t=the quick brown fox jumps over the lazy dog_");
    }

    // HOME BRINGS THE BEGINNING BACK, and the caret with it.
    t.key(input::scan::kHome);
    CHECK(t.pane().input.first_visible() == 0);
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.rows.back().text == "> send @zengine.skin SurfaceText 1 slot=1 text=");
        CHECK(pane.caret_col == kTerminalPromptCols);
        // The pane alone: the list and the property body are other presentations.
        const surface::SurfaceCanvas only_pane = canvas_of_region(t.canvases.back(), pane);
        const std::vector<surface::ProjectedRow> shown =
            projected_of(only_pane);
        CHECK(shown[static_cast<std::size_t>(p.prose_row)].label.text ==
              "> _send @zengine.skin SurfaceText 1 slot=1 text=");
    }

    // END TAKES IT BACK TO THE TAIL, and the authored line never changed once.
    t.key(input::scan::kEnd);
    CHECK(t.pane().input.first_visible() == 43);
    CHECK(t.pane().input.text() == kLongLine);

    // A CARET IN THE MIDDLE OF THE HIDDEN LEFT drags the window to meet it, minimally: the
    // window's start becomes the caret and not one byte further.
    // FORTY-FIVE OF THEM ARE FREE -- the window is 45 columns wide and the caret starts at
    // its right edge, so it walks the whole width before the window has to move at all.
    for (int i = 0; i < 45; ++i) {
        t.key(input::scan::kLeft);
    }
    CHECK(t.pane().input.caret() == 43);
    CHECK(t.pane().input.first_visible() == 43);
    for (int i = 0; i < 7; ++i) {
        t.key(input::scan::kLeft);
    }
    CHECK(t.pane().input.caret() == 36);
    CHECK(t.pane().input.first_visible() == 36); // one character per keystroke, from here
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col == kTerminalPromptCols); // ...at the left edge of the row
        CHECK(pane.rows.back().text == "> t=1 text=the quick brown fox jumps over the l");
    }

    // THE GRAPHICAL MEDIUM IS THE SAME ANSWER IN ITS OWN NUMBERS. A wider row means a
    // different window, not a different rule.
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.key(input::scan::kEnd);
    const TerminalInputPlace g = terminal_input_place(screen_of(t.session()));
    REQUIRE(g.fit.graphical());
    REQUIRE(g.columns == 68);
    CHECK(t.pane().input.first_visible() == 20); // 88 - 68
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.rows.back().text ==
              "> urfaceText 1 slot=1 text=the quick brown fox jumps over the lazy dog");
        CHECK(pane.caret_col == kTerminalPromptCols + 68);
        // ...AND THE BAR IS ACTUALLY DRAWN, which is the half of this the cell projection
        // cannot answer: `plan_caret` refuses a column the region has no room for, and that
        // refusal is exactly what the defect looked like before HD-4.
        // The pane alone: the list and the property body are other presentations.
        const surface::SurfaceCanvas only_pane = canvas_of_region(t.canvases.back(), pane);
        const std::vector<surface::PlanTextRegion> plan = plan_regions_of(
            only_pane, surface::SurfaceExtent{78, 22, 8, 18}, surface::PlanSize{4000, 4000});
        REQUIRE(plan.size() == 1);
        CHECK(plan[0].caret.present);
    }
}

TEST_CASE("HD-4: a press on a SCROLLED line lands in the full authored string") {
    // §8, and the canary target. A visible column names `first_visible + offset` of the whole
    // command; a hit test that forgot the offset is right for exactly as long as no line is
    // long enough to scroll, which is to say wrong only when nobody is looking.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : kLongLine) {
        t.text(std::string(1, c));
    }
    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);

    // THE PREMISE, ASSERTED BEFORE ANYTHING ELSE. Without this the case would pass against a
    // hit test that ignores the window, for the most boring reason there is.
    REQUIRE(t.pane().input.first_visible() == 43);
    const std::size_t from = t.pane().input.first_visible();

    // EVERY VISIBLE COLUMN, and each one resolves to the byte of the WHOLE line that is
    // actually drawn there.
    for (std::int64_t k = 0; k <= p.columns; ++k) {
        t.press_at(pane_cell_x(p, p.first_column + k), pane_cell_y(p, p.prose_row),
                   input::space::kCells);
        CAPTURE(k);
        CHECK(t.pane().input.caret() == from + static_cast<std::size_t>(k));
        // ...and the window did not move, because every one of these is already inside it.
        CHECK(t.pane().input.first_visible() == from);
    }

    // THE FIRST VISIBLE CHARACTER, BY NAME. Column 2 of the row is byte 43, which is the `t`
    // of `text=` -- not byte 0, which is what a viewport-blind hit test would answer.
    t.press_at(pane_cell_x(p, p.first_column), pane_cell_y(p, p.prose_row),
               input::space::kCells);
    REQUIRE(t.pane().input.caret() == 43);
    CHECK(kLongLine[43] == 't');

    // A PRESS IN THE PROMPT, OR LEFT OF IT, MEANS THE START OF WHAT THE MAKER CAN SEE, which
    // on a scrolled line is not the start of the line. Clamping to 0 there would jump the
    // window a screenful away from where the press landed.
    t.key(input::scan::kEnd);
    REQUIRE(t.pane().input.first_visible() == 43);
    t.press_at(pane_cell_x(p, 0), pane_cell_y(p, p.prose_row), input::space::kCells);
    CHECK(t.pane().input.caret() == 43);
    CHECK(t.pane().input.first_visible() == 43);

    // A PRESS PAST THE LAST VISIBLE CHARACTER MEANS THE END OF THE LINE. On a scrolled line
    // the last visible character IS the last character, because the window never sits
    // further right than the last full screenful -- so there is no empty room that could
    // falsely imply the command ended early (§18).
    t.press_at(pane_cell_x(p, p.fit.columns), pane_cell_y(p, p.prose_row),
               input::space::kCells);
    CHECK(t.pane().input.caret() == kLongLine.size());

    // AND THE PRESS REPAIRS THE LINE WHERE THE MAKER AIMED. `text=the quick` -> `text=the
    // slick`: click the `q`, delete it, type an `s`.
    const std::size_t q = kLongLine.find("quick");
    REQUIRE(q > from); // the target is inside the visible window
    t.press_at(pane_cell_x(p, p.first_column + static_cast<std::int64_t>(q - from)),
               pane_cell_y(p, p.prose_row), input::space::kCells);
    REQUIRE(t.pane().input.caret() == q);
    t.key(input::scan::kDelete);
    t.text("s");
    CHECK(t.pane().input.text() ==
          "send @zengine.skin SurfaceText 1 slot=1 text=the suick brown fox jumps over the lazy dog");
}

TEST_CASE("HD-4: the window follows the caret across a resize") {
    // §19. Narrower, narrower again, then wider -- and at each one the caret is still on the
    // row and a press still reaches the byte a maker aimed at. There is no resize-specific
    // path: the same reconcile runs on the repaint the new extent causes.
    Live t;
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.toggle_terminal();
    for (const char c : kLongLine) {
        t.text(std::string(1, c));
    }

    struct Step {
        surface::SurfaceExtent extent;
        std::int64_t columns;
        std::size_t first;
    };
    // 68 columns, then 49, then 107 -- the last of which is wider than the whole command, so
    // the window has to come all the way back to the beginning. (The first two were 80 and 57
    // before HD-10 narrowed the pane's placement by eight cells at the minimum extent; the
    // third is unmoved, because at 115 columns the room is wider than the pane's want.)
    for (const Step s : {Step{{78, 22, 8, 18}, 68, 20}, Step{{78, 22, 11, 23}, 49, 39},
                         Step{{115, 63, 8, 18}, 107, 0}}) {
        t.publish(loom::to_value(s.extent));
        const Screen sc = screen_of(t.session());
        const TerminalInputPlace p = terminal_input_place(sc);
        CAPTURE(s.extent.text_advance_px);
        CAPTURE(sc.terminal_cols);
        REQUIRE(p.columns == s.columns);

        // THE WINDOW MOVED ONLY AS FAR AS IT HAD TO, and the caret is on the row.
        CHECK(t.pane().input.at_end());
        CHECK(t.pane().input.first_visible() == s.first);
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col ==
              terminal_caret_column(p, t.pane().input));
        CHECK(pane.caret_col <= p.fit.columns);
        CHECK(pane.rows.back().text ==
              "> " + t.pane().input.visible(p.columns));

        // WIDENING REVEALS MORE TEXT: the widest of the three shows the whole command.
        if (s.first == 0) {
            CHECK(pane.rows.back().text == "> " + kLongLine);
        }

        // AND A PRESS AFTER THE RESIZE STILL REACHES THE INTENDED POSITION OF THE FULL LINE.
        for (const std::int64_t k : {std::int64_t{0}, std::int64_t{9}, s.columns}) {
            t.press_at(pane_pixel_x(p, p.first_column + k), pane_pixel_y(p, p.prose_row),
                       input::space::kPixels);
            CAPTURE(k);
            CHECK(t.pane().input.caret() ==
                  (s.first + static_cast<std::size_t>(k) < kLongLine.size()
                       ? s.first + static_cast<std::size_t>(k)
                       : kLongLine.size()));
        }
        t.key(input::scan::kEnd);
    }
}

TEST_CASE("HD-4: completion sees the whole line, and acceptance brings the tail into view") {
    // §9 and §10. The completer is asked about the AUTHORED input and never about the slice
    // on the row -- a completer fed the visible substring would offer candidates for a
    // prefix that only looks like the end of the line.
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal(); // 45 columns
    // FIFTY-EIGHT BYTES, ending in a token the completer answers: the length is spent in a
    // field VALUE so the line scrolls while the thing being completed is still a field name.
    const std::string prefix =
        "send * SurfaceText 1 text=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa s";
    for (const char c : prefix) {
        t.text(std::string(1, c));
    }
    REQUIRE(prefix.size() == 58);
    REQUIRE(t.pane().input.first_visible() == 13); // 58 - 45: the line has scrolled
    REQUIRE(t.pane().input.at_end());

    // THE PARTIAL IS A TOKEN OF THE WHOLE LINE, not of the row: `send *` has already
    // scrolled off the left, and the completer still knows this is a field of `SurfaceText`
    // because it was handed `input.text()`.
    CHECK(t.pane().completion.partial == "s");
    REQUIRE_FALSE(t.pane().completion.candidates.empty());

    // ACCEPTING PRODUCES A LONGER LINE, and the window follows to its tail.
    const std::string chosen = t.pane().completion.candidates[t.pane().completion.selected].insert;
    t.key(input::scan::kTab);
    const std::string after = t.pane().input.text();
    CHECK(after.size() > prefix.size());
    CHECK(after.rfind(chosen) == after.size() - chosen.size());
    CHECK(t.pane().input.at_end());

    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    CHECK(t.pane().input.first_visible() == after.size() - static_cast<std::size_t>(p.columns));
    const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
    CHECK(pane.rows.back().text == "> " + after.substr(t.pane().input.first_visible()));
    CHECK(pane.caret_col == kTerminalPromptCols + p.columns);

    // ...AND NOTHING WAS AUTHORED BY ANY OF IT.
    CHECK(me->transcript().entries().empty());
    CHECK(me->pending().empty());
    CHECK_FALSE(me->awaiting());
}

TEST_CASE("HD-4: a whole long-line edit authors nothing, and Return still submits all of it") {
    // §22. Typing, scrolling, clicking, repairing, Home, End and a resize are presentation
    // to the last one; the only effectful gesture in this pane is still Return, and what it
    // submits is the AUTHORED line rather than the part that was on the row.
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.toggle_terminal();
    for (const char c : kLongLine) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.pane().input.first_visible() > 0);

    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    t.key(input::scan::kHome);
    t.key(input::scan::kEnd);
    for (int i = 0; i < 30; ++i) {
        t.key(input::scan::kLeft);
    }
    for (int i = 0; i < 10; ++i) {
        t.key(input::scan::kRight);
    }
    t.press_at(pane_pixel_x(p, p.first_column + 4), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    t.publish(loom::to_value(surface::SurfaceExtent{115, 63, 8, 18}));
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.key(input::scan::kEnd);

    CHECK(t.pane().input.text() == kLongLine);
    CHECK(me->transcript().entries().empty());
    CHECK(me->pending().empty());
    CHECK_FALSE(me->awaiting());

    // RETURN, ONCE, AND THE WHOLE OF IT.
    t.key(input::scan::kReturn);
    const std::vector<loom::TranscriptEntry> record = me->transcript().entries();
    REQUIRE(record.size() >= 1);
    CHECK(record.front().text == kLongLine);
    CHECK(t.pane().input.empty());
    CHECK(t.pane().input.first_visible() == 0);
}

TEST_CASE("HD-4: a column and a byte index are inverses THROUGH the window") {
    // §7 and §8 as pure arithmetic, across four faces rather than the live one -- what is
    // being pinned is the mapping, and a case written against a single metric would pass for
    // a reason it does not claim. The eight positions §7 names are all here: start, middle,
    // end, both scrolled edges, an empty line, an exact-capacity line and one byte over.
    struct Face {
        std::int64_t advance;
        std::int64_t line;
    };
    for (const Face f : {Face{0, 0}, Face{6, 14}, Face{8, 18}, Face{11, 23}, Face{15, 31}}) {
        const Screen sc = screen_of(kScreenMinW, kScreenMinH, f.advance, f.line);
        const TerminalInputPlace p = terminal_input_place(sc);
        const std::size_t room = static_cast<std::size_t>(p.columns);
        CAPTURE(f.advance);

        // AN UNSCROLLED WINDOW IS HD-3's ANSWER, BYTE FOR BYTE. This is the compatibility
        // claim the whole change rests on: a short command is presented exactly as it was.
        CHECK(terminal_caret_column(p, box_of(0, 0, 0)) == kTerminalPromptCols);
        CHECK(terminal_caret_column(p, box_of(7, 7, 0)) == kTerminalPromptCols + 7);
        CHECK(terminal_caret_of_column(p, box_of(40, 40, 0), kTerminalPromptCols + 7) == 7);
        CHECK(terminal_caret_of_column(p, box_of(40, 40, 0), 0) == 0);

        // A SCROLLED WINDOW SUBTRACTS ITSELF FROM THE COLUMN AND ADDS ITSELF BACK.
        const std::size_t from = 25;
        const std::size_t length = from + room + 11; // eleven bytes hidden past the right
        CHECK(terminal_caret_column(p, box_of(from, from, from)) == kTerminalPromptCols);
        CHECK(terminal_caret_column(p, box_of(from + 3, from + 3, from)) == kTerminalPromptCols + 3);
        CHECK(terminal_caret_column(p, box_of(from + room, from + room, from)) == kTerminalPromptCols + p.columns);

        // ...AND THE TWO ARE INVERSES OVER EVERY POSITION THE WINDOW SHOWS, which is the
        // property a click-then-type depends on once the line is longer than the row.
        for (std::size_t at = from; at <= from + room; ++at) {
            CAPTURE(at);
            CHECK(terminal_caret_of_column(p, box_of(length, length, from),
                                           terminal_caret_column(p, box_of(at, at, from))) == at);
        }

        // THE BOUNDARIES, ON A SCROLLED LINE. Left of the prompt is the first byte the maker
        // can SEE -- not byte zero, which is a screenful away from where they pressed.
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), 0) == from);
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), 1) == from);
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), -400) == from);
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), kTerminalPromptCols - 1) == from);

        // ...and past the last byte is the end of the WHOLE line, never the end of the slice.
        CHECK(terminal_caret_of_column(p, box_of(length, length, from),
                                       kTerminalPromptCols + p.columns + 400) == length);

        // AN EMPTY LINE, AN EXACT FIT AND ONE BYTE OVER -- the three lengths §7 asks for.
        CHECK(terminal_caret_of_column(p, box_of(0, 0, 0), kTerminalPromptCols) == 0);
        CHECK(terminal_caret_column(p, box_of(room, room, 0)) == kTerminalPromptCols + p.columns);
        CHECK(terminal_caret_column(p, box_of(room + 1, room + 1, 1)) == kTerminalPromptCols + p.columns);

        // TOTAL AT BOTH ENDS OF THE NUMBER LINE, because the column came off the bus and the
        // window is a byte index the presentation chose.
        constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
        constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), kMin) == from);
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), kMax) == length);
        // A WINDOW PAST THE END OF THE LINE cannot name a position the line does not have.
        CHECK(terminal_caret_of_column(p, box_of(5, 5, 900), kMax) == 5);
        CHECK(terminal_caret_of_column(p, box_of(5, 5, 900), kTerminalPromptCols) == 5);
    }
}

TEST_CASE("HD-4: clicking a SCROLLED multibyte line snaps exactly as HD-3's did") {
    // §6 and §24's character-safety row, through the weave: the window and the caret obey
    // one rule about what a character is, and a press between the two bytes of an `é` means
    // the `é` it landed on -- scrolled or not.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal(); // 45 columns
    for (int i = 0; i < 30; ++i) {
        t.text("\xC3\xA9"); // a whole character per TextEntered, as a backend reports it
    }
    REQUIRE(t.pane().input.size() == 60);

    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    // 60 - 45 = 15, which is the second byte of an `é` -- so the window snaps FORWARD to 16
    // and shows one character fewer rather than half of one.
    REQUIRE(t.pane().input.first_visible() == 16);
    CHECK(t.pane().input.visible(p.columns).size() == 44);
    CHECK_FALSE(component::is_continuation_byte(t.pane().input.visible(p.columns)[0]));

    // EVERY VISIBLE COLUMN, and the caret only ever lands between characters -- at an even
    // byte, because every character here is two bytes and the line starts on one.
    for (std::int64_t k = 0; k <= p.columns; ++k) {
        t.press_at(pane_cell_x(p, p.first_column + k), pane_cell_y(p, p.prose_row),
                   input::space::kCells);
        CAPTURE(k);
        CHECK(t.pane().input.caret() % 2 == 0);
        // ...and it is the character the press landed ON: an odd column is the second byte
        // of the character at the even column before it.
        CHECK(t.pane().input.caret() ==
              16 + static_cast<std::size_t>(k) - static_cast<std::size_t>(k % 2));
    }

    // AND THE HIDDEN CHARACTERS ARE STILL THERE, whole, in the authored line.
    CHECK(t.pane().input.size() == 60);
    for (std::size_t i = 0; i < 60; i += 2) {
        CHECK(t.pane().input.text()[i] == '\xC3');
        CHECK(t.pane().input.text()[i + 1] == '\xA9');
    }
}

// ============================================================================================
// HD-10 — the reserved column is nobody's to spend
// ============================================================================================
//
// HD-9's live run recorded a defect it did not cause: with the Terminal open, the Info panel
// published its lower rows -- properties, and the whole `[ Create ]` / `[ Delete ]` footer --
// and the pane's region, later in `c.texts`, erased them. In canvas-coloured ground, so what a
// maker saw was a panel that STOPPED rather than a panel something was covering, and the three
// answers they could not tell apart were `omitted`, `hidden` and `destroyed`.
//
// WHERE THE OVERLAP CAME FROM, measured before anything was designed:
//
//     the Info panel's REGION is exactly its granted bounds less the heading row
//         (`info_body_place`: region_x = panel.x, region_w = panel.w) -- nothing escaped
//     the two RECTANGLES overlapped, at every extent this composition lays out
//         78x22   pane 56x13 at (22, 9)    side region 28x17 at (50, 0)   overlap 28 x 8
//         98x60   pane 66x32 at (32, 28)   side region 28x55 at (70, 0)   overlap 28 x 27
//         240x80  pane 137x42 at (103, 38) side region 28x75 at (212, 0)  overlap 28 x 37
//
// So it was never an internal presentation exceeding its grant, and never a rule about paint
// order: it was two rectangles claiming the same cells, and publication order was the only
// thing deciding which one a maker got to see.
//
// THE OWNER OF THAT CONFLICT IS `screen_of`, AND IT ALREADY KNEW THE ANSWER. It reserves the
// side column and hands the workspace `room_w = panel_x - kPanelGap` three lines before it
// places the pane; `placement_bounds` puts the overlay stack inside that same number and
// asserts it. The pane was the one placement in the file older than that discipline. HD-10 is
// two expressions: the pane's want is bounded by the room, and its right edge IS the room's.
//
// WHAT THIS IS NOT. It is not a rule that regions may not overlap -- three of them still do,
// every one on purpose and every one inside a single owner's room (the completion list over
// the pane's transcript, the picker over the stack slot beneath it, and the pane itself over
// the workspace and the bottom band). It is the narrower claim that the room reserved BESIDE
// the workspace is nobody's, which is the one overlap that had no owner and no reason.

namespace {

/// A screen and the two rectangles HD-10 is about, so a case can ask about their relationship
/// without either of them computing a rectangle for itself.
struct Places {
    Screen sc{};
    ui::Rect pane{};
    ui::Rect side{};
};
Places places_of(std::int64_t w, std::int64_t h, std::int64_t advance = 0,
                 std::int64_t line = 0) {
    Places p;
    p.sc = screen_of(w, h, advance, line);
    p.pane = ui::Rect{p.sc.terminal_x, p.sc.terminal_y, p.sc.terminal_w, p.sc.terminal_h};
    p.side = placement_bounds(placement::kSideRegion, 0, p.sc);
    return p;
}

/// How many CELLS two rectangles share. Zero is the answer HD-10 requires of the pane and the
/// side region; it is COUNTED rather than compared as edges so a case that gets the arithmetic
/// subtly wrong reports how badly rather than passing.
std::int64_t shared_cells(const ui::Rect& a, const ui::Rect& b) {
    std::int64_t n = 0;
    for (std::int64_t y = b.y; y < b.y + b.h; ++y) {
        for (std::int64_t x = b.x; x < b.x + b.w; ++x) {
            if (a.contains(x, y)) {
                ++n;
            }
        }
    }
    return n;
}

/// A document and a session with Info open and `n` objects, at a stated extent.
struct Room {
    WorkshopDoc d;
    Session s;
};
Room room_of(std::size_t n, std::int64_t w, std::int64_t h, std::int64_t advance = 0,
             std::int64_t line = 0) {
    Room r;
    for (std::size_t i = 0; i < n; ++i) {
        REQUIRE(doc::add_default(r.d) != 0);
    }
    adopt_screen(r.s, w, h, advance, line);
    if (!r.d.elements.empty()) {
        r.s.selected = r.d.elements.back().id;
    }
    refocus(r.d, r.s);
    return r;
}

/// The extents HD-10 measures over: the minimum, the widths where the room is narrower than
/// the pane's want, the width where the two first agree, and up to the largest surface this
/// composition lays out.
const std::vector<std::pair<std::int64_t, std::int64_t>> kHd10Extents = {
    {78, 22},  {79, 22},  {80, 23},  {81, 24},  {94, 30},  {98, 60},
    {100, 33}, {120, 40}, {160, 60}, {240, 80}, {640, 400}};

} // namespace

TEST_CASE("HD-10: the pane and the side region share no cell, at any extent or metric") {
    // THE PHASE'S WHOLE CLAIM, counted in cells rather than compared as edges -- the same
    // question a maker's eye asks, and the one the pristine tree answered with 224 shared
    // cells at the minimum extent and 1,036 at 240x80.
    for (const auto& wh : kHd10Extents) {
        for (const auto& metric : std::vector<std::pair<std::int64_t, std::int64_t>>{
                 {0, 0}, {8, 18}, {11, 23}}) {
            CAPTURE(wh.first);
            CAPTURE(wh.second);
            CAPTURE(metric.first);
            const Places p = places_of(wh.first, wh.second, metric.first, metric.second);
            CHECK(shared_cells(p.pane, p.side) == 0);
            // ...and the two ways of saying it agree, which is what stops a later placement
            // rule from satisfying the arithmetic while breaking the law.
            CHECK(p.pane.x + p.pane.w == p.sc.room_w);
            CHECK(p.pane.x + p.pane.w <= p.side.x - kPanelGap);
            CHECK(p.pane.x >= 0);
            // THE PANE IS NEVER NARROWER THAN THE WORKSPACE'S OWN FLOOR, so the clamp has no
            // degenerate end: the room it is bounded by is the room the workspace has.
            CHECK(p.pane.w >= kWorkspaceMinW);
        }
    }
}

TEST_CASE("HD-10: the want is unchanged and the room is the ceiling") {
    // G-2'S HALF-SHARE RULE SURVIVED. What HD-10 added is a ceiling, and the two agree from 94
    // columns up -- so the rule a maker experiences on any ordinary window is the rule G-2
    // wrote, and the ceiling is what happens on a surface with no half to give away.
    for (std::int64_t w = kScreenMinW; w <= 200; ++w) {
        CAPTURE(w);
        const Screen sc = screen_of(w, kScreenMinH);
        const std::int64_t want = kTerminalWantW + (w - kScreenMinW) / 2;
        CHECK(sc.terminal_w == (want < sc.room_w ? want : sc.room_w));
        CHECK(sc.terminal_w == (w < 94 ? sc.room_w : want));
    }
    // The extents where the ceiling actually binds, named, because "only the narrowest
    // screens" is a claim and these are the whole of it.
    CHECK(screen_of(78, 22).terminal_w == 48);
    CHECK(screen_of(79, 22).terminal_w == 49);
    CHECK(screen_of(80, 22).terminal_w == 50);
    CHECK(screen_of(81, 22).terminal_w == 51); // want 57, room 51
    CHECK(screen_of(94, 22).terminal_w == 64); // want 64, room 64 -- they meet
    CHECK(screen_of(95, 22).terminal_w == 64); // want 64, room 65 -- the want, from here on
}

TEST_CASE("HD-10: the reservation is the SCREEN's, and holds with no panel in it") {
    // THE TEST FOR THE WRONG OWNER, AS A CASE. If the repair had been "the pane must not cover
    // Info", closing Info would hand the pane the column back -- and the column is reserved
    // whether or not anything is standing in it (PNL-0's own decision, and the reason hiding
    // Info moves nothing). Nothing in `screen_of` can see a panel, and this says so.
    Session open_info;
    Session no_info;
    REQUIRE(close_panel(no_info.panels, panel::kInfo));
    REQUIRE(no_info.panels.open.empty());
    for (const auto& wh : kHd10Extents) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        adopt_screen(open_info, wh.first, wh.second, 0, 0);
        adopt_screen(no_info, wh.first, wh.second, 0, 0);
        const Screen a = screen_of(open_info);
        const Screen b = screen_of(no_info);
        CHECK(a.terminal_x == b.terminal_x);
        CHECK(a.terminal_w == b.terminal_w);
        CHECK(b.terminal_x + b.terminal_w == b.room_w);
    }
}

TEST_CASE("HD-10: the Info panel is byte-identical with the pane open") {
    // THE OTHER HALF OF THE OWNERSHIP ANSWER. The repair moved the PANE; it did not make the
    // panel reflow, shrink, or float its footer upwards. So the composition a maker reads on
    // the right is the same composition whether the Terminal is open or closed -- which is
    // what stops opening a mode from moving a target under the hand aiming at it.
    for (const auto& wh : kHd10Extents) {
        for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
            CAPTURE(wh.first);
            CAPTURE(wh.second);
            CAPTURE(line);
            Room closed = room_of(6, wh.first, wh.second, line == 0 ? 0 : 8, line);
            Room opened = room_of(6, wh.first, wh.second, line == 0 ? 0 : 8, line);
            opened.s.terminal.open = true;
            opened.s.terminal.attached = true;

            const InfoBodyPlace a = body_of(closed.d, closed.s);
            const InfoBodyPlace b = body_of(opened.d, opened.s);
            REQUIRE(a.present);
            REQUIRE(b.present);
            CHECK(a.region_x == b.region_x);
            CHECK(a.region_y == b.region_y);
            CHECK(a.region_w == b.region_w);
            CHECK(a.region_h == b.region_h);
            CHECK(a.capacity == b.capacity);
            CHECK(a.objects_rows == b.objects_rows);
            CHECK(a.properties_rows == b.properties_rows);
            CHECK(a.heading_row == b.heading_row);
            CHECK(a.action_row == b.action_row);

            // AND THE ROWS THEMSELVES, one by one, published on both canvases -- HD-9's
            // grounds included, so an occlusion repair cannot quietly regress them.
            const surface::SurfaceCanvas ca = paint(closed.d, closed.s);
            const surface::SurfaceCanvas cb = paint(opened.d, opened.s);
            const surface::SurfaceTextRegion* ra = body_on(ca, a);
            const surface::SurfaceTextRegion* rb = body_on(cb, b);
            REQUIRE(ra != nullptr);
            REQUIRE(rb != nullptr);
            REQUIRE(ra->rows.size() == rb->rows.size());
            for (std::size_t i = 0; i < ra->rows.size(); ++i) {
                CAPTURE(i);
                CHECK(ra->rows[i].text == rb->rows[i].text);
                CHECK(ra->rows[i].role == rb->rows[i].role);
                CHECK(ra->rows[i].background == rb->rows[i].background);
            }
        }
    }
}

TEST_CASE("HD-10: the footer a maker can SEE, with the pane open, on the real rasterizer") {
    // THE DEFECT ITSELF, AS A PICTURE. Everything above is arithmetic; this is the thing HD-9
    // photographed. The screen is rasterized by the terminal medium's own pure function, the
    // Info column is cut out of it, and the rows that used to be erased are read back.
    for (const auto& wh : kHd10Extents) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        Room r = room_of(4, wh.first, wh.second);
        r.s.terminal.open = true;
        r.s.terminal.attached = true;
        r.s.terminal.id = loom::WeaveId{3};
        const InfoBodyPlace body = body_of(r.d, r.s);
        REQUIRE(body.present);
        const surface::SurfaceCanvas c = paint(r.d, r.s);
        const std::vector<std::string> rows = rasterized(c);
        REQUIRE(rows.size() == static_cast<std::size_t>(screen_of(r.s).h));

        // A BODY prose row: the heading the region carries above the body offsets every
        // canvas row by its reservation (WUX-1).
        const auto column = [&rows, &body](std::int64_t prose_row) {
            const std::int64_t y = body.region_y + kInfoHeadingRows + prose_row;
            return rows[static_cast<std::size_t>(y)].substr(
                static_cast<std::size_t>(body.region_x),
                static_cast<std::size_t>(body.region_w));
        };
        CHECK(column(prose_row_of_action(body, kActionCreate)).rfind("[ Create ]", 0) == 0);
        CHECK(column(prose_row_of_action(body, kActionDelete)).rfind("[ Delete ]", 0) == 0);
        CHECK(column(body.heading_row).rfind("PROPERTIES", 0) == 0);
        CHECK(column(0).find("panel") != std::string::npos);
        // ...AND NOT ONE ROW OF THE BODY IS BLANKED BY SOMETHING ELSE. Every row the region
        // published is the row the screen shows, which is the sentence the pristine tree
        // could not say for anything below the pane's top edge.
        const surface::SurfaceTextRegion* published = body_on(c, body);
        REQUIRE(published != nullptr);
        for (std::size_t i = 0; i < published->rows.size(); ++i) {
            CAPTURE(i);
            // Region row i is body row i - kInfoHeadingRows; `column` adds the offset back.
            CHECK(column(static_cast<std::int64_t>(i) - kInfoHeadingRows)
                      .rfind(published->rows[i].text, 0) == 0);
        }
    }
}

TEST_CASE("HD-10: what the pane DOES cover is unchanged, and is on purpose") {
    // THE CLASSIFICATION, MEASURED. Forbidding overlap globally would forbid three things this
    // application does deliberately, so the three are asserted as still true rather than left
    // to a reader to notice they survived.
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    t.toggle_terminal();
    const Screen sc = screen_of(t.session());

    // ONE: the pane covers the WORKSPACE, which is what an overlay over the material IS.
    const ui::Rect pane{sc.terminal_x, sc.terminal_y, sc.terminal_w, sc.terminal_h};
    const ui::Rect workspace{kWorkspaceX, kWorkspaceY, sc.room_w, sc.room_h};
    CHECK(shared_cells(pane, workspace) > 0);

    // TWO: the pane covers the bottom band, and the SCREEN answers for that by not painting
    // the notice or the help lines at all while it is open -- an occlusion with an owner, and
    // the precedent HD-10's own answer was modelled on.
    CHECK(sc.terminal_y + sc.terminal_h == sc.h);
    CHECK(label_at(t.canvases.back(), 0, sc.help_y).rfind("n new | d delete", 0) != 0);

    // THREE: the completion list covers the pane's own transcript, inside one owner's room.
    t.text("s");
    const surface::SurfaceCanvas& c = t.canvases.back();
    const surface::SurfaceTextRegion* list = list_of(c, sc);
    REQUIRE(list != nullptr);
    const surface::SurfaceTextRegion* body = pane_of(c, sc);
    REQUIRE(body != nullptr);
    const ui::Rect list_rect{list->x, list->y, list->w, list->h};
    const ui::Rect pane_rect{body->x, body->y, body->w, body->h};
    CHECK(shared_cells(list_rect, pane_rect) > 0);
    CHECK(list->x >= body->x);
    CHECK(list->x + list->w <= body->x + body->w);
    // ...and NONE of them reaches the side region.
    const ui::Rect side = placement_bounds(placement::kSideRegion, 0, sc);
    CHECK(shared_cells(list_rect, side) == 0);
    CHECK(shared_cells(pane_rect, side) == 0);
}

TEST_CASE("HD-10: the pane and the overlay stack meet on one row, at one height, and only there") {
    // THE ONE OVERLAP HD-10 LEAVES BETWEEN TWO INDEPENDENT PRESENTATIONS, measured exactly so
    // that it is a known fact rather than a later discovery. Both are OVERLAYS in the room the
    // workspace has: the stack grows down from the top-left, the pane up from the bottom-right,
    // and at the shortest screen this composition lays out their edges touch for one row.
    //
    // IT IS NOT REPAIRED HERE, and the reason is that repairing it means reserving the stack's
    // rows from the pane -- a SECOND reservation, which `screen_of` does not make and which
    // would tie the pane's height to `kStackRows`. The reserved side column is a subtraction
    // the screen already performs; the overlay stack is not. HD-10's report says the rest.
    for (std::int64_t h = kScreenMinH; h <= 40; ++h) {
        CAPTURE(h);
        const Places p = places_of(kScreenMinW, h);
        const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, p.sc);
        CHECK(shared_cells(p.pane, slot) == (h == kScreenMinH ? slot.w : 0));
    }
    // And the row they share is the stack slot's LAST row, which the Builder spends on
    // `[ Build ]` -- a label naming the key `b` rather than a control, since `[ Build ]` has
    // never been pressable (PNL-2 says so in its own words).
    const Places min = places_of(kScreenMinW, kScreenMinH);
    const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, min.sc);
    CHECK(min.pane.y == slot.y + slot.h - 1);
}

TEST_CASE("WIND-1: the stack/pane overlap grew by a bounded amount, and stayed in the room") {
    // THE PRICE OF THE HALF-SHARE, PAID WHERE IT IS PAID. HD-10 left one deliberate overlap
    // between two independent overlays -- the stack growing down from the top-left, the pane
    // up from the bottom-right -- and WIND-1 widens one of them, so the overlap gets bigger.
    // This is not repaired here for HD-10's own reason (repairing it means reserving the
    // stack's rows from the pane, a SECOND reservation `screen_of` does not make). What it
    // is instead is MEASURED, and the measurement is the argument for this width rather than
    // the obvious wider one.
    //
    // THE BOUND IS A CONSTANT AND NOT A SHARE OF THE SURFACE, which is the whole point: the
    // pane's own left edge moves right at exactly the rate the stack's right edge does, so
    // the columns they contest never exceed `kTerminalWantW` however large the surface gets.
    // A full-width stack would contest the pane's ENTIRE width, growing with the supported
    // surface and reaching 3,033 shared cells at the 640-column maximum.
    const auto columns_shared = [](std::int64_t stack_w, const Screen& sc) {
        const std::int64_t lo = sc.terminal_x > kStackX ? sc.terminal_x : kStackX;
        const std::int64_t hi = (kStackX + stack_w) < (sc.terminal_x + sc.terminal_w)
                                    ? (kStackX + stack_w)
                                    : (sc.terminal_x + sc.terminal_w);
        return hi > lo ? hi - lo : std::int64_t{0};
    };
    const auto rows_shared = [](const ui::Rect& slot, const Screen& sc) {
        const std::int64_t lo = sc.terminal_y > slot.y ? sc.terminal_y : slot.y;
        const std::int64_t hi = (slot.y + slot.h) < (sc.terminal_y + sc.terminal_h)
                                    ? (slot.y + slot.h)
                                    : (sc.terminal_y + sc.terminal_h);
        return hi > lo ? hi - lo : std::int64_t{0};
    };

    // A SWEEP THAT REPORTS TALLIES rather than a case that asserts per seated slot: this
    // walks every extent this composition lays out against every slot that fits in it, which
    // is over four hundred thousand of them, and four hundred thousand assertions saying the
    // same thing would drown the suite's own totals. The counts below are zero or the case
    // says how badly, which diagnoses a failure exactly as precisely (PNL-2's own `Sweep`
    // made the same trade).
    std::int64_t worst = 0;
    std::int64_t worst_w = 0;
    std::int64_t worst_h = 0;
    std::size_t worst_slot = 0;
    std::int64_t worst_full = 0;
    std::size_t seated = 0;
    std::size_t past_the_room = 0;
    std::size_t over_the_bound = 0;
    for (std::int64_t w = kScreenMinW; w <= kScreenMaxW; ++w) {
        for (std::int64_t h = kScreenMinH; h <= kScreenMaxH; ++h) {
            const Screen sc = screen_of(w, h);
            const std::int64_t floor_y = kWorkspaceY + sc.room_h;
            for (std::size_t n = 0; n < kMaxSetupPanes; ++n) {
                const ui::Rect slot = placement_bounds(placement::kOverlayStack, n, sc);
                if (slot.y + slot.h > floor_y) {
                    break; // past the room: `stack_slots_that_fit`'s own condition
                }
                ++seated;
                // THE OVERLAP IS ALWAYS INSIDE THE ROOM, never in the reserved column, and
                // that is the invariant HD-10 established and this width must not spend.
                if (slot.x + slot.w > sc.room_w || sc.terminal_x + sc.terminal_w > sc.room_w) {
                    ++past_the_room;
                }
                // THE COLUMN BOUND, at every extent and every seated slot.
                if (columns_shared(slot.w, sc) > kTerminalWantW) {
                    ++over_the_bound;
                }
                const std::int64_t cells = columns_shared(slot.w, sc) * rows_shared(slot, sc);
                if (cells > worst) {
                    worst = cells;
                    worst_w = w;
                    worst_h = h;
                    worst_slot = n;
                }
                const std::int64_t full = columns_shared(sc.room_w, sc) * rows_shared(slot, sc);
                if (full > worst_full) {
                    worst_full = full;
                }
            }
        }
    }
    CHECK(seated > 400000); // the sweep really did walk the domain
    CHECK(past_the_room == 0);
    CHECK(over_the_bound == 0);
    // THE WORST CASE, EXACTLY -- and the alternative it was chosen against. 504 cells is 72
    // more than the 432 this composition already carried; a full-width stack would have made
    // it 3,033, which is six times the pane and is why "just take the room" was refused.
    CHECK(worst == 504);
    CHECK(worst_slot == 1);
    CHECK(worst_full == 3033);

    // ONE OF THE EXTENTS THAT ATTAINS IT, COUNTED CELL BY CELL through `shared_cells` rather
    // than through the arithmetic above -- so the sweep's own helper cannot be the thing that
    // is wrong. 640x26 is the accepted witness; the sweep finds the same maximum earlier in
    // the domain (94x25) because the bound is a constant, which is the finding itself.
    const Places big = places_of(640, 26);
    const ui::Rect second = placement_bounds(placement::kOverlayStack, 1, big.sc);
    CHECK(second == ui::Rect{0, 11, 329, 9});
    CHECK(shared_cells(big.pane, second) == 504);
    CHECK(shared_cells(big.pane, second) == worst);
    CHECK(shared_cells(big.pane, big.side) == 0);
    CHECK(shared_cells(second, big.side) == 0);
    CAPTURE(worst_w);
    CAPTURE(worst_h);

    // AND THE PANE STILL OUTRANKS THE PANEL FOR PAINT AND FOR THE POINTER, unchanged: the
    // overlay is drawn last and, while it is open, the pointer does nothing ANYWHERE -- which
    // is a strictly wider rule than occupancy and is why `occupied_at` never mentions it.
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    (void)mount_tool(t, "zengine-snake");
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60}));
    open_builder(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect panel =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect);
    REQUIRE(panel.w == 109);
    t.toggle_terminal();
    REQUIRE(t.session().terminal.open);
    const std::string before = t.notice();
    const std::int64_t selected = t.session().selected;
    t.press(60, 3); // squarely inside the columns WIND-1 gave the panel
    CHECK(t.notice() == before); // not even the panel's own sentence: the mode has the input
    CHECK(t.session().selected == selected);
    CHECK_FALSE(t.session().drag.active);
}

TEST_CASE("HD-10: the composition is DERIVED, and a resize recomputes all of it") {
    // Nothing is stored: no occlusion state, no remembered rectangle, no cached room. The
    // proof is that walking a live session through seven extents and back gives, at each one,
    // exactly what a freshly computed screen of that extent gives.
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const auto& wh : std::vector<std::pair<std::int64_t, std::int64_t>>{
             {78, 22}, {240, 80}, {94, 30}, {78, 22}, {160, 45}, {80, 23}, {78, 22}}) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        t.publish(loom::to_value(surface::SurfaceExtent{wh.first, wh.second}));
        const Screen live = screen_of(t.session());
        const Places fresh = places_of(wh.first, wh.second);
        CHECK(live.terminal_x == fresh.sc.terminal_x);
        CHECK(live.terminal_w == fresh.sc.terminal_w);
        CHECK(live.terminal_y == fresh.sc.terminal_y);
        CHECK(live.terminal_h == fresh.sc.terminal_h);
        CHECK(shared_cells(fresh.pane, fresh.side) == 0);
        // AND THE PICTURE AGREES WITH THE PLACEMENT at every one of them: the pane's own
        // header is at the pane's corner and the panel's footer is readable beside it.
        const std::vector<std::string> rows = rasterized(t.canvases.back());
        REQUIRE(rows.size() == static_cast<std::size_t>(live.h));
        CHECK(rows[static_cast<std::size_t>(live.terminal_y)].substr(
                  static_cast<std::size_t>(live.terminal_x), 8) == "TERMINAL");
        const InfoBodyPlace body = body_of(t.doc(), t.session());
        REQUIRE(body.present);
        CHECK(rows[static_cast<std::size_t>(body.region_y + kInfoHeadingRows +
                                            prose_row_of_action(body, kActionCreate))]
                  .substr(static_cast<std::size_t>(body.region_x), 10) == "[ Create ]");
    }
}

TEST_CASE("HD-10: the same composition truth in a medium that sets type") {
    // THE ANSWER IS MEDIUM-INDEPENDENT because it is made ENTIRELY in canvas cells, before any
    // metric is consulted: `screen_of` places the pane against `room_w`, and a metric only ever
    // changes how much PROSE fits inside a placement it did not choose. So the disjointness is
    // the same fact in both media, and the graphical medium's own resolution says it in device
    // pixels without anybody converting a rectangle twice.
    Room r = room_of(6, 120, 40, 8, 18);
    r.s.terminal.open = true;
    r.s.terminal.attached = true;
    const Screen sc = screen_of(r.s);
    const surface::SurfaceCanvas c = paint(r.d, r.s);
    const InfoBodyPlace body = body_of(r.d, r.s);
    REQUIRE(body.present);

    const surface::RegionFit pane_fit =
        surface::fit_region(sc.terminal_x, sc.terminal_y, sc.terminal_w, sc.terminal_h,
                            sc.text_advance_px, sc.text_line_px);
    REQUIRE(pane_fit.graphical());
    REQUIRE(body.fit.graphical());
    CHECK(pane_fit.view.x + pane_fit.view.w <= body.fit.view.x);

    // AND THE CELL PROJECTION SAYS THE SAME THING: every row of the body is on the screen.
    const std::vector<std::string> rows = rasterized(c);
    CHECK(rows[static_cast<std::size_t>(body.region_y + kInfoHeadingRows +
                                        prose_row_of_action(body, kActionDelete))]
              .substr(static_cast<std::size_t>(body.region_x), 10) == "[ Delete ]");
}

// ============================================================================
// Tier — WIND-2a: THE FRONT THE HOST HITS IS THE FRONT THE MEDIUM PAINTS
// ============================================================================
//
// WIND-2 authored a canonical `front` rank and walked it ascending to paint and descending
// to hit. What it did not do is make that order the order either shipped Skin actually
// executes: the canvas held three root lists and both media drew all the rects, then all
// the labels, then all the text regions -- so a back-ranked region covered a front-ranked
// label and the hit answer and the picture disagreed. These cases are the seven review
// findings, each stated as the behaviour a maker can observe.

namespace {

/// Remove a pane THROUGH THE PICKER, the way a maker does. `open_pane`'s twin, and bounded
/// for the same reason: a case that cannot reach its row fails with a sentence.
void remove_through_picker(Live& t, const PaneRef& ref) {
    REQUIRE_FALSE(t.session().manage.open); // `p` belongs to command mode
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    const std::vector<CatalogRow> rows =
        inventory_rows(t.session().setup.active, t.session().panels);
    std::size_t want = rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].ref == ref) {
            want = i;
        }
    }
    REQUIRE(want < rows.size());
    for (std::size_t guard = 0; guard <= rows.size(); ++guard) {
        if (t.session().panels.picker.cursor == want) {
            break;
        }
        t.key(input::scan::kDown);
    }
    REQUIRE(t.session().panels.picker.cursor == want);
    t.key(input::scan::kReturn);
}

/// One cell of a canvas AS THE TERMINAL MEDIUM ACTUALLY DRAWS IT -- through `canvas_body`,
/// the Skin's own rasterizer, rather than through any reading of the canvas invented here.
char seen_at(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
    const std::vector<std::string> rows = rasterized(c);
    REQUIRE(y >= 0);
    REQUIRE(static_cast<std::size_t>(y) < rows.size());
    const std::string& row = rows[static_cast<std::size_t>(y)];
    REQUIRE(x >= 0);
    REQUIRE(static_cast<std::size_t>(x) < row.size());
    return row[static_cast<std::size_t>(x)];
}

} // namespace

TEST_CASE("WIND-2a: an overlapping pane is painted where it is hit, in both front orders") {
    WorkshopDoc d;
    Session s;
    s.screen_w = 120;
    s.screen_h = 40;
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};

    // THE ONE CELL TWO PRESENTATIONS CAN BOTH CLAIM in this composition. The side region is
    // reserved and the stack's slots are disjoint, so an overlap has to be AUTHORED -- which
    // is exactly what WIND-2 made possible and exactly what it never measured.
    const InfoBodyPlace body = body_of(d, s);
    REQUIRE(body.present);
    const std::int64_t x = body.region_x;
    const std::int64_t y = body.region_y;
    REQUIRE(author_pane_place(s.setup.active, ref_of(panel::kBuilder), subs(x), subs(y))
                .accepted);
    const Screen sc = screen_of(s);
    REQUIRE(cells_covered(bounds_of(s.panels, s.setup.active, panel::kBuilder, sc).rect)
                .contains(x, y));
    REQUIRE(cells_covered(bounds_of(s.panels, s.setup.active, panel::kInfo, sc).rect)
                .contains(x, y));

    // THE TWO CONTROLS: what each pane draws there WITH THE OTHER ABSENT. Neither pane's
    // rectangle depends on the other (Info's place is reserved, the Builder's is authored),
    // so these are the same two pictures the overlap is made of.
    const auto alone = [&](std::int64_t kind) {
        Session one = s;
        one.setup.active = Setup{};
        one.setup.active.name = "one";
        REQUIRE(add_pane(one.setup.active, ref_of(kind)));
        if (kind == panel::kBuilder) {
            REQUIRE(author_pane_place(one.setup.active, ref_of(kind), subs(x), subs(y))
                        .accepted);
        }
        one.panels.open = {Panel{kind}};
        return seen_at(paint(d, one), x, y);
    };
    const char builder_alone = alone(panel::kBuilder);
    const char info_alone = alone(panel::kInfo);
    // AND THE CONTROL ON THE CONTROLS (Z0a): the two draw DIFFERENT characters there, so
    // neither assertion below can pass by the media agreeing about nothing.
    REQUIRE(builder_alone != info_alone);

    for (const std::int64_t front : {panel::kBuilder, panel::kInfo}) {
        CAPTURE(front);
        REQUIRE(send_to_front(s.setup.active, ref_of(front)));
        // WHAT THE HAND MEETS...
        CHECK(occupied_at(s.panels, s.setup.active, sc, x, y).what == kind_name(s.panels, front));
        // ...IS WHAT THE MEDIUM PAINTS.
        CHECK(seen_at(paint(d, s), x, y) ==
              (front == panel::kBuilder ? builder_alone : info_alone));
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
    CHECK(has_pane(t.session().setup.active, ref_of(panel::kInfo)));
}

TEST_CASE("WIND-2a: a clipped default resize begins from the full resolved size") {
    // ---- THE RIGHT EDGE. A default width of eighty-nine, four cells of it on the canvas.
    {
        Live t;
        t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
        open_pane(t, ref_of(panel::kBuilder));
        const PaneRef builder = ref_of(panel::kBuilder);
        REQUIRE(author_pane_place(live(t).setup.active, builder, subs(156), subs(2))
                    .accepted);

        const Screen sc = screen_of(t.session());
        const PanelBounds where =
            bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc);
        // THE DEVELOPER'S HALF-SHARE AT THIS EXTENT, of which four cells are on the canvas.
        REQUIRE(where.resolved.w == subs(89));
        REQUIRE(where.rect.w == subs(4));

        // THE KEY. One cell wider than what the pane RESOLVES to, not one cell wider than
        // the sliver of it a maker can currently see.
        enter_management(t);
        select_pane(t, builder);
        t.key(input::scan::kS);
        REQUIRE(t.session().manage.doing == pane_manage::kSize);
        t.key(input::scan::kRight);
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
        t.key(input::scan::kEscape);
        const ui::Rect vis =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
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
        open_pane(t, ref_of(panel::kBuilder));
        const PaneRef builder = ref_of(panel::kBuilder);
        REQUIRE(author_pane_place(live(t).setup.active, builder, 0, subs(42)).accepted);

        const Screen sc = screen_of(t.session());
        const PanelBounds where =
            bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc);
        // THE DEVELOPER'S STACK HEIGHT, of which two rows are on the canvas.
        REQUIRE(where.resolved.h == subs(9));
        REQUIRE(where.rect.h == subs(2));

        enter_management(t);
        select_pane(t, builder);
        t.key(input::scan::kS);
        REQUIRE(t.session().manage.doing == pane_manage::kSize);
        t.key(input::scan::kDown);
        const SetupPane* row = pane_of(t.session().setup.active, builder);
        REQUIRE(row != nullptr);
        CHECK(row->height.mode == pane_unit::kSubcells);
        CHECK(row->height.amount == where.resolved.h + subs(1));
        CHECK(row->width.mode == pane_unit::kDefault);
        CHECK(row->width.amount == 0);

        REQUIRE(reset_pane_height(live(t).setup.active, builder));
        t.key(input::scan::kEscape);
        const ui::Rect vis =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
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
        open_pane(t, ref_of(panel::kBuilder));
        const PaneRef builder = ref_of(panel::kBuilder);
        enter_management(t);
        select_pane(t, builder);
        const PanelBounds where =
            bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
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
    open_pane(t, ref_of(panel::kBuilder));
    const PaneRef builder = ref_of(panel::kBuilder);
    enter_management(t);
    select_pane(t, builder);
    const Screen sc = screen_of(t.session());
    const ui::Rect rect =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect);
    REQUIRE(rect.w > 4);

    // A MOVE BEGUN, THEN THE OVERLAY ARRIVES OVER THE TOP OF IT.
    t.press_at(rect.x + 1, rect.y + 1 + surface::kTuiCanvasTopRow, input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    t.toggle_terminal();
    REQUIRE(t.session().terminal.open);
    t.release(0, 0);
    // THE GESTURE IS OVER. Occluding a release is the one thing this file already knew not
    // to do for a document drag; a pane gesture is the same sentence about a different hand.
    CHECK_FALSE(t.session().pane_drag.active);

    // AND A LATER BARE MOTION MOVES NOTHING, which is what a stranded gesture would do.
    const SetupPane* before = pane_of(t.session().setup.active, builder);
    REQUIRE(before != nullptr);
    const std::int64_t place_mode = before->place.mode;
    const std::int64_t place_x = before->place.x;
    t.toggle_terminal();
    REQUIRE_FALSE(t.session().terminal.open);
    t.motion(20, 20);
    const SetupPane* after = pane_of(t.session().setup.active, builder);
    REQUIRE(after != nullptr);
    CHECK(after->place.mode == place_mode);
    CHECK(after->place.x == place_x);

    // THE SAME FOR A SIZE GESTURE, which is a different record with the same custody.
    t.press_at(rect.x + rect.w - 1, rect.y + rect.h - 1 + surface::kTuiCanvasTopRow,
               input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE(t.session().pane_drag.sizing);
    t.toggle_terminal();
    t.release(0, 0);
    CHECK_FALSE(t.session().pane_drag.active);
}

TEST_CASE("WIND-2a: a removed target leaves no stale selection, submode or heading") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    const PaneRef builder = ref_of(panel::kBuilder);
    enter_management(t);
    select_pane(t, builder);
    const Screen sc = screen_of(t.session());
    const ui::Rect rect =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect);
    t.press_at(rect.x + 1, rect.y + 1 + surface::kTuiCanvasTopRow, input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    REQUIRE(t.session().manage.doing == pane_manage::kMove);

    // THE TARGET LEAVES THE SETUP UNDER THE HAND.
    REQUIRE(remove_pane(live(t).setup.active, builder));
    t.publish(loom::to_value(input::PointerMoved{rect.x + 4,
                                                 rect.y + 4 + surface::kTuiCanvasTopRow, 0, 0,
                                                 input::space::kCells, input::mod::kNone}));
    CHECK_FALSE(t.session().pane_drag.active);
    // MEMBERSHIP IS THE LAW: the pane is not in the setup, so it is not selected, and the
    // submode that was about it is over.
    CHECK_FALSE(t.session().manage.has_selection());
    CHECK(t.session().manage.doing == pane_manage::kSelect);
    // AND THE SURFACE SAYS SO: no heading names the removed pane, and no remaining row is
    // marked as though it were the one that was chosen.
    const std::string surface_text = management_text(t);
    INFO(surface_text);
    CHECK(surface_text.find("builder") == std::string::npos);
    CHECK(surface_text.find("> ") == std::string::npos);

    // AND AN UNRESOLVED SELECTION IS KEPT, because unresolved is recoverable and still
    // authored -- membership, not presentation, is what clears one.
    REQUIRE(add_pane(live(t).setup.active, stranger()));
    for (std::size_t guard = 0; guard < 8; ++guard) {
        if (t.session().manage.selected == stranger()) {
            break;
        }
        t.key(input::scan::kTab);
    }
    REQUIRE(t.session().manage.selected == stranger());
    t.key(input::scan::kEscape);
    t.key(input::scan::kW);
    t.text("w");
    CHECK(t.session().manage.selected == stranger());
}

TEST_CASE("WIND-2a: the ordinary picker removal clears a stale management selection") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    const PaneRef builder = ref_of(panel::kBuilder);
    enter_management(t);
    select_pane(t, builder);
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().manage.open);
    // THE SELECTION SURVIVES LEAVING THE MODE (WIND-2's own rule) -- and it may not survive
    // the pane leaving the setup.
    REQUIRE(t.session().manage.selected == builder);
    remove_through_picker(t, builder);
    REQUIRE_FALSE(has_pane(t.session().setup.active, builder));
    CHECK_FALSE(t.session().manage.has_selection());
}

TEST_CASE("WIND-2a: a pixel axis refuses every current pane projection, Info included") {
    Session s;
    s.setup.active = Setup{};
    s.setup.active.name = "Pixels";
    REQUIRE(add_pane(s.setup.active, ref_of(panel::kInfo)));
    const PaneRef info = ref_of(panel::kInfo);
    REQUIRE(author_pane_size(s.setup.active, info, PaneSize{pane_unit::kPixels, 240},
                             PaneSize{pane_unit::kDefault, 0})
                .accepted);
    REQUIRE(check_setup(s.setup.active).accepted);
    s.panels.open = {Panel{panel::kInfo}};

    for (const std::int64_t adv : {std::int64_t{0}, std::int64_t{8}}) {
        CAPTURE(adv);
        s.text_advance_px = adv;
        s.text_line_px = adv > 0 ? 18 : 0;
        const Screen sc = screen_of(s);
        const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kInfo, sc);
        // FIXED PLACEMENT IS NOT PERMISSION TO PRESENT AN UNSUPPORTED UNIT AS UNDERSTOOD.
        CHECK_FALSE(where.projected);
        CHECK(where.rect.w == 0);
        CHECK(pane_state_of(s.panels, s.setup.active, sc,
                            CatalogRow{panel::kInfo, info, "Info", ""}) == pane_state::kRefused);
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
    CHECK(has_pair("w window"));
    CHECK(has_pair("p + panel"));
    CHECK(has_pair("^t terminal"));
    CHECK(has_pair("^k hotkeys")); // the recovery key: the full list is one keystroke away

    const std::vector<std::string> raster = rasterized(paint(d, s));
    REQUIRE_FALSE(raster.empty());
    const std::string& top = raster[0];
    INFO(top);
    CHECK(top.find("[window]") == std::string::npos);
    CHECK(top.find("[+ panel]") == std::string::npos);
    CHECK(top.find("terminal") == std::string::npos);
    CHECK(top.find("WORKSPACE") == std::string::npos);
    CHECK(top.find("OBJECTS") != std::string::npos);
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
    // The repair is `detail::fit` before `detail::pad`: the mark for the truth, the pad
    // for the alignment. Both halves are asserted, because a fix that marked the cut and
    // moved the state column would have traded one defect for another.
    const std::string wide = picker_entry_text("a-very-long-provider-name", "closed", "tail");
    CHECK(wide.rfind("a-very-...", 0) == 0);
    CHECK(wide.find("closed") == kPickerNameCols);
    // A name that FITS is untouched -- not padded differently, not marked, not moved.
    const std::string narrow = picker_entry_text("Loaded", "closed", "tail");
    CHECK(narrow.rfind("Loaded    closed", 0) == 0);
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
    const std::vector<std::string> allowed{"PaneContent", "PaneOffered", "zen.ListLoaded"};
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
    CHECK_FALSE(resolve_pane(intro_ref(), fresh.session().panels.runtime).has_value());
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
    const ui::Rect box = cells_covered(picker_bounds(sc));

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
    const ui::Rect box_cells = cells_covered(box);

    const PanelProsePlace cell_place = panel_prose_place(box, cells);
    const PanelProsePlace typed_place = panel_prose_place(box, typed);
    CHECK(cell_place.rows == box_cells.h);
    CHECK(cell_place.columns == box_cells.w);
    CHECK(typed_place.rows ==
          (box_cells.h * surface::kCanvasCellPx - 2 * surface::kTextInsetPx) / 18);
    CHECK(typed_place.rows < cell_place.rows);
    CHECK(typed_place.columns > cell_place.columns); // ...and more characters across each one

    // A CATALOG TALLER THAN THE GRAPHICAL BUDGET IS WINDOWED THERE AND WHOLE IN CELLS, which
    // is the assertion that actually spends the difference: six offers plus two built-ins fit
    // the nine rows a terminal has and not the five an 18-pixel face has, so the two media
    // show different lists of one population and each says what it left out. The marker is
    // paid for OUT of the budget rather than added beneath it -- `list_window`'s rule, which
    // this migration spends rather than reimplements.
    Panels crowded = panels;
    for (std::int64_t i = 0; i < 6; ++i) {
        crowded.runtime.entries.push_back(
            RuntimePane{kFirstRuntimeKind + i, "zengine.probe",
                        "p" + std::to_string(i), "Probe" + std::to_string(i), "a summary"});
    }
    const std::vector<CatalogRow> crowd = inventory_rows(setup_for(crowded), crowded);
    REQUIRE(crowd.size() == 8);
    const auto published = [&](const Screen& medium) {
        surface::SurfaceCanvas to;
        paint_picker(plane(to), crowded, setup_for(crowded), medium, Keymap{});
        const ui::Rect at = cells_covered(picker_bounds(medium));
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
    REQUIRE(static_cast<std::int64_t>(catalog.size()) <= typed_place.rows - 1);

    surface::SurfaceCanvas c;
    paint_picker(plane(c), panels, setup_for(panels), typed, Keymap{});
    const std::vector<surface::SurfaceTextRegion> at_slot =
        regions_at(c, box_cells.x, box_cells.y);
    REQUIRE(at_slot.size() == 1);
    const surface::SurfaceTextRegion& list = at_slot.front();
    // Every row the publisher said fits the room it was told about -- the medium truncating
    // for it would be the second measurer this whole seam exists not to have.
    CHECK(static_cast<std::int64_t>(list.rows.size()) <= typed_place.rows);
    for (const surface::SurfaceTextRow& row : list.rows) {
        CHECK(static_cast<std::int64_t>(row.text.size()) <= typed_place.columns);
    }
    // AND A SUMMARY THAT WAS CUT AT 48 CELLS IS WHOLE AT 71 COLUMNS: the same room, read by
    // a medium that fits more characters into it. Measured on a real offered pane rather than
    // on the two built-ins, whose summaries are short enough to fit either -- the live case is
    // exactly INTR-0's `Loaded`, whose sentence a maker read as `what the kernel has lo...`.
    const surface::RegionFit fit = surface::fit_region_subs(box.x, box.y, box.w, box.h, 8, 18);
    CHECK(fit.columns == 71);
    Panels offered = panels;
    offered.runtime.entries.push_back(
        RuntimePane{kFirstRuntimeKind, "zengine.introspection", "loaded", "Loaded",
                    "what the kernel has loaded, and each one's role"});
    const auto loaded_row = [&](const Screen& medium) {
        surface::SurfaceCanvas paint_to;
        paint_picker(plane(paint_to), offered, setup_for(offered), medium, Keymap{});
        const ui::Rect at = cells_covered(picker_bounds(medium));
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
    CHECK(in_cells.find(detail::kElided) != std::string::npos); // cut, and MARKED, at 48 cells
    CHECK(in_type.find(detail::kElided) == std::string::npos);  // whole at 71 columns
    CHECK(in_type.find("each one's role") != std::string::npos);
}

TEST_CASE("TYPE-0: the pane-management surface is a region on the same terms") {
    Panels panels;
    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    s.manage.open = true;
    const Screen sc = screen_of(s);
    const ui::Rect box = cells_covered(picker_bounds(sc));

    surface::SurfaceCanvas c;
    paint_management(plane(c), panels, s.setup.active, s.manage, sc, s.keymap);
    const std::vector<surface::SurfaceTextRegion> at_slot = regions_at(c, box.x, box.y);
    REQUIRE(at_slot.size() == 1);
    const surface::SurfaceTextRegion& list = at_slot.front();
    CHECK(list.rows[0].text.rfind("+ WINDOW", 0) == 0);
    CHECK(list.rows[0].role == surface::role::kAccent);
    for (const surface::SurfaceLayer& l : c.layers) {
        CHECK(l.labels.empty());
    }
}

TEST_CASE("TYPE-0/WUX-1: the notice is a band row, and the SENTENCE is never shortened") {
    WorkshopDoc d;
    doc::add_default(d);
    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    s.selected = d.elements.front().id;
    refocus(d, s);
    const Screen sc = screen_of(s);

    // THE BAND IS ONE REGION SINCE WUX-1 -- the whole five reserved cells, published
    // whether or not there is a notice; an empty notice is a blank ROW of it, not a
    // missing region, because the band owns its rectangle in every state.
    const std::vector<surface::SurfaceTextRegion> quiet =
        regions_at(paint(d, s), 0, sc.notice_y - 1);
    REQUIRE(quiet.size() == 1);
    CHECK(quiet.front().rows.size() >= 2);
    CHECK(quiet.front().rows[1].text.empty());

    // A SENTENCE THAT FITS: one prose row of the band, whole.
    s.notice = "created #1 -- a new identity, not a new name";
    const surface::SurfaceCanvas said = paint(d, s);
    const std::vector<surface::SurfaceTextRegion> at_band =
        regions_at(said, 0, sc.notice_y - 1);
    REQUIRE(at_band.size() == 1);
    const surface::SurfaceTextRegion& band = at_band.front();
    CHECK(band.w == sc.w);
    CHECK(band.h == kBottomRows);
    CHECK(band.y + band.h == sc.h); // the band ends at the screen's own foot
    REQUIRE(band.rows.size() >= 2);
    CHECK(band.rows[1].text == s.notice);
    CHECK(band.rows[1].role == surface::role::kFill);
    // THE BAND'S FIVE CELLS HOLD THREE ROWS OF THIS FACE -- the budget the composition
    // spends: the status row, the notice, one legend row.
    const surface::RegionFit fit = band_fit(sc);
    CHECK(fit.graphical());
    CHECK(fit.rows == 3);

    // A BAD ONE WEARS THE ALERT ROLE, which is the second signal and not a second sentence.
    s.notice_is_bad = true;
    const surface::SurfaceCanvas bad = paint(d, s);
    const std::vector<surface::SurfaceTextRegion> at_bad =
        regions_at(bad, 0, sc.notice_y - 1);
    REQUIRE(at_bad.size() == 1);
    CHECK(at_bad.front().rows[1].role == surface::role::kAlert);

    // A SENTENCE TOO LONG FOR THE ROOM IS MARKED, AND `Session::notice` STILL HOLDS ALL OF
    // IT. What a maker sees is bounded; what Workshop knows is not.
    s.notice = std::string(400, 'x') + "-END";
    s.notice_is_bad = false;
    const surface::SurfaceCanvas cut = paint(d, s);
    const std::vector<surface::SurfaceTextRegion> at_cut =
        regions_at(cut, 0, sc.notice_y - 1);
    REQUIRE(at_cut.size() == 1);
    const surface::SurfaceTextRow& row = at_cut.front().rows[1];
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
        regions_at(in_cells, 0, cell_sc.notice_y - 1);
    REQUIRE(at_cells.size() == 1);
    const surface::SurfaceTextRow& cell_row = at_cells.front().rows[1];
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
                                                 "Probe", "a summary"});
    ExternalPane live;
    live.kind = kFirstRuntimeKind;
    live.heard = true;
    live.shown.push_back(surface::SurfaceTextRow{"a provider row", surface::role::kFill});
    panels.external.push_back(live);

    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    const Screen sc = screen_of(s);
    // TWO CELLS: one prose row of an 18-pixel face, which the header takes whole.
    const ui::Rect tiny{0, 1, 48, 2};
    const ExternalBodyPlace body = external_body_place(
        fine_of_cells(tiny), sc,
        external_title_rows(panels, kFirstRuntimeKind, /*titles_shown=*/true));
    CHECK(body.fit.rows == 1);
    CHECK_FALSE(body.present); // no room was granted, and none is invented
    CHECK(body.rows == 0);

    surface::SurfaceCanvas c;
    paint_external(plane(c), panels, kFirstRuntimeKind, fine_of_cells(tiny), sc,
                   /*titles=*/true);
    const std::vector<surface::SurfaceTextRegion> at = regions_at(c, tiny.x, tiny.y);
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
    WorkshopDoc over;
    (void)doc::add(over, "a name much longer than its object", 44, 0,
                   ui::Extent{ui::kExtentCells, 12}, ui::Extent{ui::kExtentCells, 1});
    Session os = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    const std::vector<surface::SurfaceTextRegion> clipped = object_names(paint(over, os));
    REQUIRE(clipped.size() == 1);
    CHECK(clipped.front().w == os.workspace_w - 44); // 4, not the object's 12

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

TEST_CASE("QR-3: a long name through the REAL property editor, and what each surface shows") {
    // THE WHOLE PHASE IN ONE MAKER'S HANDS, driven through the live weave: a rename past the
    // old bound, committed through the same Enter/type/Enter every property takes; the
    // Inspector holding the complete authored text; the workspace showing a MARKED projection
    // of it bounded by the object's own material; and a resize revealing more of the same
    // bytes. No gesture here is special-cased for a name.
    Live t;
    const std::int64_t id = t.session().selected;
    REQUIRE(id != 0);

    // A body to write on, authored through the ordinary Width property. The inspector's
    // cursor only walks DOWN in `begin_editing`, so each edit starts from the top row.
    const auto set_property = [&t](const char* which, const std::string& value) {
        for (int i = 0; i < 12; ++i) {
            t.key(input::scan::kUp);
        }
        t.begin_editing(which);
        for (int i = 0; i < 12; ++i) {
            t.key(input::scan::kBackspace);
        }
        t.text(value);
        t.key(input::scan::kReturn);
    };
    set_property("Width", "20");

    // TYPED PAST THE OLD BOUND AND ACCEPTED. 43 bytes -- the name HD-7's own case wanted and
    // could only get by writing the element's field directly, because no maker could type it.
    const std::string wanted = "the-quick-brown-fox-jumps-over-the-lazy-dog";
    REQUIRE(wanted.size() > 32);
    REQUIRE(wanted.size() <= doc::kMaxNameLen);
    set_property("Name", wanted);
    REQUIRE(doc::find(t.doc(), id) != nullptr);
    CHECK(doc::find(t.doc(), id)->label == wanted); // committed, every byte
    CHECK_FALSE(t.session().notice_is_bad);        // and not by way of a refusal

    // THE INSPECTOR HOLDS THE COMPLETE AUTHORED TEXT -- its row is the property, and what a
    // narrow panel shows of it is the panel's own fitting/windowing question.
    const auto inspector_name = [&t]() {
        for (const Row& r : t.session().rows) {
            if (r.label() == "Name") {
                return r.display();
            }
        }
        FAIL("no Name row");
        return std::string();
    };
    CHECK(inspector_name() == wanted);

    // THE WORKSPACE SHOWS A BOUNDED, MARKED PROJECTION OF IT -- 20 cells of material, so 20
    // characters, the last three of them the mark that says there is more.
    const auto workspace_name = [&t]() {
        const std::vector<surface::SurfaceTextRegion> names = object_names(t.canvases.back());
        REQUIRE(!names.empty());
        return names.front();
    };
    CHECK(workspace_name().w == 20);
    CHECK(workspace_name().rows.front().text == "the-quick-brown-f...");
    CHECK(workspace_name().rows.front().text.size() == 20);

    // RESIZING REVEALS MORE OF THE SAME AUTHORED VALUE, and nothing else about it moves.
    set_property("Width", "30");
    CHECK(workspace_name().w == 30);
    CHECK(workspace_name().rows.front().text == "the-quick-brown-fox-jumps-o...");
    CHECK(doc::find(t.doc(), id)->label == wanted); // not one byte was cut

    set_property("Width", "44");
    CHECK(workspace_name().w == 44);
    CHECK(workspace_name().rows.front().text == wanted); // whole, and no mark
    CHECK(inspector_name() == wanted);

    // AND NARROWING PUTS THE MARK BACK, from the same authored bytes.
    set_property("Width", "4");
    CHECK(workspace_name().w == 4);
    CHECK(workspace_name().rows.front().text == "t...");
    CHECK(doc::find(t.doc(), id)->label == wanted);

    // A NAME PAST THE NEW BOUND IS STILL REFUSED THROUGH THE SAME EDITOR, and the refusal
    // leaves the committed value alone -- the draft is what a maker is holding, not the value.
    set_property("Name", std::string(doc::kMaxNameLen + 1, 'x'));
    CHECK(doc::find(t.doc(), id)->label == wanted);
    CHECK(t.session().notice_is_bad);
    CHECK(t.session().notice.find("at most 64") != std::string::npos);
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
    // AND THE OBJECTS LIST STILL NAMES IT WHOLE, which is where a maker reads a name that has
    // no material to sit on. The two answers are different because the questions are.
    CHECK(surface::canvas_body(c).find("bodyless") != std::string::npos);
    CHECK(doc::find(d, d.elements[0].id)->label == "bodyless");
}

TEST_CASE("TYPE-1: moving and resizing an object move the name and change no authored byte") {
    // §18. The name is derived from the current resolved placement every time `paint` runs;
    // there is no cached typography anywhere, which is what makes this a re-derivation rather
    // than an invalidation problem.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    const std::int64_t id = t.session().selected;
    REQUIRE(id != 0);
    t.begin_editing("Name");
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text("dial");
    t.key(input::scan::kReturn);
    REQUIRE(doc::find(t.doc(), id)->label == "dial");

    // THE OPENING DOCUMENT CARRIES TWO OBJECTS, so the name is found by its own text rather
    // than by being the only one -- which is also the fixture for "a name is not an identity".
    const auto name_now = [&t]() {
        surface::SurfaceTextRegion found;
        std::size_t how_many = 0;
        for (const surface::SurfaceTextRegion& r : object_names(t.canvases.back())) {
            if (!r.rows.empty() && r.rows.front().text == "dial") {
                found = r;
                ++how_many;
            }
        }
        REQUIRE(how_many == 1);
        return found;
    };
    const surface::SurfaceTextRegion before = name_now();
    CHECK(before.rows.front().text == "dial");
    CHECK(before.ground == surface::kGroundBeneath);
    const ui::Element started = *doc::find(t.doc(), id); // every authored byte, before a hand

    // A NUDGE, through the same document operation a typed X goes through.
    t.key(input::scan::kL);
    const surface::SurfaceTextRegion moved = name_now();
    CHECK(moved.x == before.x + 1);
    CHECK(moved.y == before.y);
    // AND ITS ROOM DID NOT CHANGE, which is QR-3's half of this case: the name's room is the
    // OBJECT'S resolved width now, and a nudge moves an object without resizing it. Before
    // QR-3 this read `before.w - 1`, because the room was the distance to the workspace's
    // right edge and moving right spent one cell of it.
    CHECK(moved.w == before.w);
    CHECK(moved.h == before.h);

    // SHORTER, THEN TALLER, through the typed property. The room the name has follows the
    // material it is written on, and nothing about the name is cached across either.
    t.begin_editing("Height");
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text("2");
    t.key(input::scan::kReturn);
    const surface::SurfaceTextRegion shorter = name_now();
    CHECK(shorter.h == 2);
    CHECK(shorter.rows.front().text == "dial");

    t.begin_editing("Height");
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text("9");
    t.key(input::scan::kReturn);
    CHECK(name_now().h == 9);

    // NOT ONE AUTHORED BYTE MOVED for any of it beyond what the gestures themselves wrote,
    // and the name is still the document's own.
    const ui::Element* authored = doc::find(t.doc(), id);
    REQUIRE(authored != nullptr);
    CHECK(authored->label == "dial");
    CHECK(authored->width == started.width);  // untouched: only x and the height were authored
    CHECK(authored->height.mode == ui::kExtentCells);
    CHECK(authored->height.amount == 9);
    CHECK(authored->x == started.x + 1);
    CHECK(authored->y == started.y);
    CHECK(authored->context == started.context);

    // AND A SAVE/RESTORE ROUND TRIP CARRIES THE SAME NAME, because persistence never learned
    // that typography exists.
    const std::string text = persist::to_text(t.doc());
    const persist::Loaded back = persist::from_text(text);
    REQUIRE(back.outcome.accepted);
    CHECK(doc::find(back.document, id)->label == "dial");
    CHECK(doc::find(back.document, id)->height.amount == 9);
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
        open_pane(*this, ref_of(panel::kBuilder));
        enter_management(*this);
        select_pane(*this, ref_of(panel::kBuilder));
    }

    void motion_at(std::int64_t x, std::int64_t y, std::int64_t space) {
        publish(loom::to_value(input::PointerMoved{x, y, 0, 0, space, input::mod::kNone}));
    }

    const SetupPane* builder_row() {
        return pane_of(session().setup.active, ref_of(panel::kBuilder));
    }

    FineRect builder_rect() {
        return bounds_of(session().panels, session().setup.active, panel::kBuilder,
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
        REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), subs(6),
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
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), 0, subs(20))
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
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), subs(4),
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
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), subs(1),
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
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), 5, subs(20))
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
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), subs(20), 5)
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
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), 5, 5)
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
    t.key(input::scan::kM);
    REQUIRE(t.session().manage.doing == pane_manage::kMove);
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
    const PaneRef builder = ref_of(panel::kBuilder);

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
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), subs(6) + 13,
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
    CHECK(on.kind == panel::kBuilder);
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
    open_pane(t, ref_of(panel::kBuilder));
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), subs(6) + 24,
                              subs(6) + 40)
                .accepted);
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(panel::kBuilder),
                             PaneSize{pane_unit::kSubcells, subs(20) + 30},
                             PaneSize{pane_unit::kSubcells, subs(5) + 20})
                .accepted);
    const std::string authored_before = setup_persist::to_text(t.session().setup.active);

    const Screen sc = screen_of(t.session());
    const FineRect fine = bounds_of(t.session().panels, t.session().setup.active,
                                    panel::kBuilder, sc)
                              .rect;
    const ui::Rect covered = cells_covered(fine);
    // floor(6.5) = 6; floor(6.5 + 20.625) = 27 -> 21 covered columns. floor(6+40/48) = 6;
    // floor(y + h) = floor(6.833 + 5.416 = 12.25) = 12 -> 6 covered rows.
    CHECK(covered == ui::Rect{6, 6, 21, 6});

    // THE PAINTED BYTES SIT ON EXACTLY THOSE CELLS: the pane's backdrop begins at the
    // covered corner in the cell rasterization, and the column before it is not the
    // pane's.
    const surface::SurfaceCanvas c = paint(t.doc(), t.session());
    CHECK_FALSE(label_at(c, covered.x, covered.y).empty());
    const Occupancy at_corner = occupied_at(t.session().panels, t.session().setup.active,
                                            sc, covered.x, covered.y);
    CHECK(at_corner.occupied);
    CHECK(at_corner.kind == panel::kBuilder);
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
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), subs(6),
                              subs(6))
                .accepted);
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(panel::kBuilder),
                             PaneSize{pane_unit::kSubcells, subs(20)},
                             PaneSize{pane_unit::kSubcells, subs(5)})
                .accepted);
    const FineRect exact = bounds_of(t.session().panels, t.session().setup.active,
                                     panel::kBuilder, screen_of(t.session()))
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

TEST_CASE("WUX-2: a version-1 session restores a whole-cell desk through the legacy road") {
    session_persist::v1::WorkshopSession old;
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

    const session_persist::LoadedSession read = session_persist::from_text(bytes);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.present);
    CHECK(read.honoured);
    CHECK(read.viewport_w == 120);
    CHECK(read.viewport_h == 44);
    CHECK(read.desk.name == "Yesterday");
    REQUIRE(read.desk.panes.size() == 1);
    CHECK(read.desk.panes[0].width.mode == pane_unit::kSubcells);
    CHECK(read.desk.panes[0].width.amount == subs(28));

    // A LEGACY ROAD CARRIES NO PLACEMENT: nothing in a v1 file could have said one.
    CHECK_FALSE(read.placement.known);

    // AND THE NEXT CLOSE WRITES VERSION 3, byte-stable thereafter.
    const std::string saved = session_persist::to_text(read.desk, read.viewport_w,
                                                       read.viewport_h, read.placement);
    CHECK(saved.find("\"format_version\":\"3\"") != std::string::npos);
    const session_persist::LoadedSession back = session_persist::from_text(saved);
    REQUIRE(back.outcome.accepted);
    CHECK(back.desk == read.desk);
    CHECK(session_persist::to_text(back.desk, back.viewport_w, back.viewport_h,
                                   back.placement) == saved);
}

TEST_CASE("WUX-2: fine geometry survives the setup file without losing a sub-unit") {
    // EVERY REMAINDER VALUE IS A DIFFERENT AUTHORED FACT, and the file must keep each: a
    // sweep across the lattice's finest steps, through save -> load -> save.
    Setup s = two_overlays();
    const PaneRef builder = ref_of(panel::kBuilder);
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

TEST_CASE("WUX-2: the management row says a fine value exactly, as a reduced fraction") {
    // SC-9: introspection must not present a rounded whole-cell value as the stored one.
    // A whole-cell value reads as the bare count it always did; a finer one reads as an
    // EXACT mixed number — never a decimal wearing precision 1/48 does not have.
    CHECK(subcell_text(subs(40)) == "40");
    CHECK(subcell_text(0) == "0");
    CHECK(subcell_text(subs(10) + 24) == "10+1/2");
    CHECK(subcell_text(subs(10) + 12) == "10+1/4");
    CHECK(subcell_text(subs(10) + 36) == "10+3/4");
    CHECK(subcell_text(subs(10) + 16) == "10+1/3");
    CHECK(subcell_text(subs(10) + 8) == "10+1/6");
    CHECK(subcell_text(subs(10) + 1) == "10+1/48");
    CHECK(subcell_text(subs(10) + 47) == "10+47/48");

    SetupPane row;
    row.place = PanePlace{pane_unit::kSubcells, subs(6) + 24, subs(5)};
    row.width = PaneSize{pane_unit::kSubcells, subs(40) + 12};
    row.height = PaneSize{pane_unit::kPixels, 220};
    row.front = 2;
    CHECK(pane_window_text(&row) == "@6+1/2,5 40+1/4x220px f2");

    // AND THE WHOLE-CELL SPELLING IS BYTE-IDENTICAL TO WIND-2's, so nothing a maker
    // learned to read has moved.
    SetupPane plain;
    plain.place = PanePlace{pane_unit::kSubcells, subs(6), subs(5)};
    plain.width = PaneSize{pane_unit::kSubcells, subs(40)};
    plain.height = PaneSize{pane_unit::kDefault, 0};
    plain.front = 0;
    CHECK(pane_window_text(&plain) == "@6,5 40x- f0");
}
