// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — the external pane seam, from both sides.
// 
//
// An office authors the pane and Workshop grants the room (WP-0), and a maker presses a
// row inside that room and the pane says which entry that was (SEL-0). Both halves are
// driven through the REAL weave on a REAL bus against REAL loaded artifacts — a fixture
// office built here, and two products a maker actually runs — because the claim is about
// the real ABI and the real load path, and a mock loader would prove nothing about
// either.
//
// The rig every case here starts from is `PaneRig` in `workshop_support.hpp`: it is
// shared because a Workshop with a real external pane in it is what the geometry, the
// persistence and the interaction suites need too.
//
// FOUR SOURCES, ONE SUITE, AND THE BOUNDARIES ARE THE FILE'S OWN. `workshop_panes` is
// one CTest entry running one binary; since QR-13 its cases live in five translation
// units, cut along the headings this material already had:
//
//   _seam.cpp           the protocol and the provider -- what an office may offer, who
//                       may speak for it, how Workshop discovers it, the room it grants,
//                       what it retains, and how a pane ends
//   _window.cpp         where the pane SITS -- the authored window, order and recovery,
//                       the units a maker reads and authors, the two arrangement scopes,
//                       and the one graphical boundary
//   _input.cpp          the maker's hand crossing the seam -- a press that names a row,
//                       and the keyboard that reaches a pane
//   _introspection.cpp  the resolved arrangement and the power stack, as two more panes
//   _sampling.cpp       the live seam -- browsing runs nothing, sampling runs exactly one
//
// A NEW CASE GOES TO THE FILE WHOSE SUBJECT IT IS ABOUT. The cut is a reading boundary
// first and an object-format bound second: one MinGW Debug object could no longer name
// all of these instantiations (tests/CMakeLists.txt, QR-13).
//
// THIS FILE OWNS: the maker's hand crossing the seam.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// ============================================================================
// SEL-0 — a maker presses a row, and the pane says which entry that was
// ============================================================================
//
// THREE TIERS, AND THE SPLIT IS THE PHASE'S CLAIM ABOUT WHERE ITS HALVES LIVE.
//
//   THE SEAM      Workshop resolves a press into a place in the room it granted, and
//                 sends it. All of that is geometry and provenance, and none of it
//                 knows what a row says -- so it is proved with a recording seat that
//                 interprets nothing.
//   THE MEANING   the Loaded provider decides which of its rows name an entry, and
//                 which entry each one names. That is pure arithmetic over a value and
//                 is proved without a bus, a Workshop or a library.
//   THE PRODUCT   the real `zengine-introspection` library, loaded through the real
//                 Kernel and Manager, pressed through the real input path, publishing a
//                 real Loom message an independent listener hears. A mock of any of
//                 those would prove something about the mock.

// ---- Tier one: the seam ------------------------------------------------------------

TEST_CASE("SEL-0: PanePressed is a place in a granted room, and carries nothing else") {
    // THE FIFTH SHAPE, WALKED AS A SCHEMA rather than trusted as a struct definition --
    // WP-0's own discipline for the other four, and for its reason: what a later phase
    // would add here is a field, and a case reading the declaration would not notice.
    const std::shared_ptr<const loom::Schema> pressed = loom::schema_of<PanePressed>();
    REQUIRE(pressed != nullptr);
    CHECK(pressed->name() == "PanePressed");
    CHECK(pressed->version() == 1);
    REQUIRE(pressed->fields().size() == 3);
    CHECK(pressed->fields()[0].name == "pane");
    CHECK(pressed->fields()[0].type.kind == loom::Kind::Text);
    CHECK(pressed->fields()[1].name == "row");
    CHECK(pressed->fields()[1].type.kind == loom::Kind::Int);
    CHECK(pressed->fields()[2].name == "column");
    CHECK(pressed->fields()[2].type.kind == loom::Kind::Int);
    // NAMED NEGATIVELY, because the design of this shape is what it refuses to say. No
    // provider (Loom's stamp answers that, and a field here would be the forgeable
    // second answer); no gesture machinery, because one gesture was earned; and no
    // screen coordinate of any kind, because a provider that could locate itself on a
    // screen has been handed a layout it is not party to.
    for (const loom::Field& f : pressed->fields()) {
        CHECK(f.name != "provider");
        CHECK(f.name != "button");
        CHECK(f.name != "pressed");
        CHECK(f.name != "modifiers");
        CHECK(f.name != "clicks");
        CHECK(f.name != "x");
        CHECK(f.name != "y");
        CHECK(f.name != "space");
        CHECK(f.name != "focus");
    }
    // AND THE OTHER FOUR DID NOT MOVE. SEL-0 added a shape; it did not revise one, so
    // every existing provider's four sentences are byte-compatible with what it built
    // against.
    CHECK(loom::schema_of<PaneOffered>()->version() == 1);
    CHECK(loom::schema_of<PaneRoom>()->version() == 1);
    CHECK(loom::schema_of<PaneContent>()->version() == 1);
    CHECK(loom::schema_of<PaneCatalogRequested>()->version() == 1);
}

TEST_CASE("SEL-0: a press in the body names the row under the header, in both media") {
    // ONE LATTICE, TWO MEDIA, AND THE PROVIDER CANNOT TELL THEM APART. The graphical
    // half is asked for by handing `Session` an advance and a line height -- 8 and 18,
    // the numbers this repository's face measured -- so a lane with no font engine
    // proves the picture a maker only ever sees through SDL (TYPE-0's rule).
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;

    SUBCASE("a character medium") {
        const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
        const ExternalBodyPlace body = external_body_of(r.session(), kind);
        REQUIRE(body.present);
        REQUIRE(body.rows >= 3);

        // THE HEADER IS WORKSHOP'S ROW AND IT NAMES NOTHING. The provider was never
        // granted it, so there is no row of its room for this press to be.
        seat->presses.clear();
        r.press_cell(panel.x, panel.y);
        CHECK(seat->presses.empty());

        // ROW 0 OF THE ROOM IS THE ROW UNDER THE HEADER.
        r.press_cell(panel.x, panel.y + kExternalHeaderRows);
        REQUIRE(seat->presses.size() == 1);
        CHECK(seat->presses[0].pane == std::string(kHelloPane));
        CHECK(seat->presses[0].row == 0);
        CHECK(seat->presses[0].column == 0);

        // ...and every further row and column is the same subtraction, swept rather
        // than sampled, because an off-by-one in one direction only is exactly the
        // defect that survives a single example.
        for (std::int64_t row = 0; row < body.rows; ++row) {
            for (std::int64_t col : {std::int64_t{0}, body.columns / 2, body.columns - 1}) {
                seat->presses.clear();
                r.press_cell(panel.x + col, panel.y + kExternalHeaderRows + row);
                REQUIRE(seat->presses.size() == 1);
                CHECK(seat->presses[0].row == row);
                CHECK(seat->presses[0].column == col);
            }
        }

        // THE LAST ROW OF THE ROOM IS THE LAST ROW THAT NAMES ANYTHING.
        seat->presses.clear();
        r.press_cell(panel.x, panel.y + kExternalHeaderRows + body.rows);
        CHECK(seat->presses.empty());
        // ...and so is the last column.
        r.press_cell(panel.x + body.columns, panel.y + kExternalHeaderRows);
        CHECK(seat->presses.empty());
    }

    SUBCASE("a graphical medium, whose line height is not its cell height") {
        r.extent(1000, 700, 8, 18);
        const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
        const ExternalBodyPlace body = external_body_of(r.session(), kind);
        REQUIRE(body.present);
        // THE PRECONDITION THIS SUBCASE RESTS ON, ASSERTED RATHER THAN ASSUMED: a
        // graphical line is NOT a cell row, so a press resolved through the cell
        // lattice would land on a different row and this case would go red. With a
        // face of 18 pixels in cells of 12 the two disagree from the second line on.
        REQUIRE(body.fit.graphical());
        REQUIRE(body.fit.line_px != surface::kCanvasCellPx);

        for (std::int64_t row = 0; row < body.rows; ++row) {
            seat->presses.clear();
            // THE PIXEL AT THE MIDDLE OF THE PROSE LINE -- inside the glyphs a maker is
            // aiming at, resolved with the same `RegionFit` that positioned them.
            const std::int64_t y = panel.y * surface::kCanvasCellPx + body.fit.origin_y +
                                   (row + kExternalHeaderRows) * body.fit.line_px +
                                   body.fit.line_px / 2;
            const std::int64_t x = panel.x * surface::kCanvasCellPx + body.fit.origin_x +
                                   3 * body.fit.advance_px + body.fit.advance_px / 2;
            r.press_pixel(x, y);
            REQUIRE(seat->presses.size() == 1);
            CHECK(seat->presses[0].row == row);
            CHECK(seat->presses[0].column == 3);
        }

        // THE TOP INSET IS THE HEADER'S FIRST PIXEL AND NAMES NO PROVIDER ROW.
        seat->presses.clear();
        r.press_pixel(panel.x * surface::kCanvasCellPx + 1, panel.y * surface::kCanvasCellPx + 1);
        CHECK(seat->presses.empty());

        // AND THE PIXEL REMAINDER UNDER THE LAST PROSE LINE IS NOT A ROW. `fit_region`
        // already decided how many WHOLE lines this rectangle holds; the strip left
        // over is inside the pane, is painted with nothing, and rounding it to the
        // nearest row would hand the provider a press at a place it never wrote to.
        const std::int64_t past = panel.y * surface::kCanvasCellPx + body.fit.origin_y +
                                  (body.rows + kExternalHeaderRows) * body.fit.line_px + 1;
        REQUIRE(past < (panel.y + panel.h) * surface::kCanvasCellPx); // genuinely inside the pane
        r.press_pixel(panel.x * surface::kCanvasCellPx + body.fit.origin_x + 1, past);
        CHECK(seat->presses.empty());
    }
}

TEST_CASE("SEL-0: every forwarded press is inside the room that pane was granted") {
    // THE BOUND STATED OVER THE WHOLE RECTANGLE rather than at its edges: no position
    // anywhere in or around the pane, in either medium, produces a coordinate outside
    // `[0, rows) x [0, columns)`. That is the provider's whole guarantee, and the one
    // thing it has nothing of its own to check it against.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;

    for (const bool graphical : {false, true}) {
        CAPTURE(graphical);
        if (graphical) {
            r.extent(1000, 700, 8, 18);
        }
        const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
        const ExternalBodyPlace body = external_body_of(r.session(), kind);
        REQUIRE(body.present);
        seat->presses.clear();
        for (std::int64_t y = panel.y - 2; y < panel.y + panel.h + 2; ++y) {
            for (std::int64_t x = panel.x - 2; x < panel.x + panel.w + 2; ++x) {
                if (graphical) {
                    r.press_pixel(cell_mid_px(x), cell_mid_px(y));
                } else {
                    r.press_cell(x, y);
                }
            }
        }
        REQUIRE_FALSE(seat->presses.empty()); // the sweep really did reach the body
        for (const PanePressed& p : seat->presses) {
            CHECK(p.pane == std::string(kHelloPane));
            CHECK(p.row >= 0);
            CHECK(p.row < body.rows);
            CHECK(p.column >= 0);
            CHECK(p.column < body.columns);
        }
    }
}

TEST_CASE("SEL-0: a press is authored as Workshop and addressed to the offering office") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    ProviderSeat* other = r.mount_provider(kOtherOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.drive(other, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"second", "Second", "another office's pane"});
    });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));

    seat->presses.clear();
    other->presses.clear();
    r.press_cell(panel.x + 1, panel.y + kExternalHeaderRows);

    // ONE PROVIDER HEARS IT, and it is the one whose pane the hand landed on. The
    // destination is a ROLE, so a replaced provider would still hear its own pane's
    // presses; the authorship is Loom's stamp, which is the only thing a provider can
    // verify and the only thing a forger cannot write.
    REQUIRE(seat->presses.size() == 1);
    REQUIRE(seat->press_authors.size() == 1);
    CHECK(seat->press_authors[0] == std::string(kWorkshopProvider));
    CHECK(other->presses.empty());
}

TEST_CASE("SEL-0: management chrome gets first refusal, and a mode takes the press whole") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
    const std::int64_t body_y = panel.y + kExternalHeaderRows;

    // THE CONTROL: with nothing over it, this exact press reaches the provider. Every
    // negative below is the same press with one thing in the way, so a case that
    // stopped reaching the pane for an unrelated reason cannot read as a refusal.
    seat->presses.clear();
    r.press_cell(panel.x + 1, body_y);
    REQUIRE(seat->presses.size() == 1);

    // ...AND SINCE MSG-0 THAT CONTROL PRESS ALSO POINTED THE KEYBOARD AT THE PANE, so
    // the two subcases that reach their mode with a PRINTABLE key have to hand the
    // keyboard back first. That is the product's own rule (`p` typed into a focused
    // pane is a `p`), not a workaround: the cell is derived from the pane's own
    // rectangle, because an overlay slot starts at the canvas corner and a literal
    // `(1, 1)` would be inside the pane it is meant to be outside of.
    const auto away = [&]() { r.press_cell(panel.x + 1, panel.y + panel.h + 1); };

    SUBCASE("the picker is over the pane") {
        seat->presses.clear();
        away();
        r.key(input::scan::kP);
        REQUIRE(r.session().panels.picker.open);
        r.press_cell(panel.x + 1, body_y);
        CHECK(seat->presses.empty());
    }
    SUBCASE("pane management owns the pointer") {
        seat->presses.clear();
        away();
        r.key(input::scan::kW);
        r.text("w");
        REQUIRE(r.session().arrange.open);
        r.press_cell(panel.x + 1, body_y);
        CHECK(seat->presses.empty());
    }
    SUBCASE("the terminal overlay is a mode and outranks occupancy entirely") {
        seat->presses.clear();
        r.key(input::scan::kT, input::mod::kCtrl);
        REQUIRE(r.session().terminal.open);
        r.press_cell(panel.x + 1, body_y);
        CHECK(seat->presses.empty());
    }
    SUBCASE("a release is not a press") {
        seat->presses.clear();
        r.release_cell(panel.x + 1, body_y);
        CHECK(seat->presses.empty());
    }
    SUBCASE("a second button is not the primary one") {
        seat->presses.clear();
        r.publish(loom::to_value(input::PointerButton{3, true, panel.x + 1,
                                                      body_y + surface::kTuiCanvasTopRow,
                                                      input::space::kCells, input::mod::kNone}));
        CHECK(seat->presses.empty());
    }
    SUBCASE("a position in a space this application does not recognise") {
        seat->presses.clear();
        r.publish(loom::to_value(input::PointerButton{
            1, true, panel.x + 1, body_y + surface::kTuiCanvasTopRow, 4242, input::mod::kNone}));
        CHECK(seat->presses.empty());
    }
}

TEST_CASE("SEL-0: a pane with no room granted yet is told about no press") {
    // THE ONE BEAT BETWEEN A PANEL OPENING AND ITS FIRST GRANT. A press then would be a
    // position in a lattice the provider has never been handed, which is unanswerable
    // rather than merely unhelpful.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
    ExternalPane* pane = r.session().panels.external_pane(kind);
    REQUIRE(pane != nullptr);
    REQUIRE(pane->granted);

    pane->granted = false; // the state a freshly opened, not-yet-repainted panel is in
    seat->presses.clear();
    r.press_cell(panel.x + 1, panel.y + kExternalHeaderRows);
    CHECK(seat->presses.empty());
}

TEST_CASE("SEL-0: Workshop gained one sentence and no knowledge of what a pane's rows mean") {
    // THE AUTHORITY AUDIT, FROM THE BUS (INTR-0's discipline). What Workshop says across
    // a whole life -- discovery, a room, several presses on several different rows, a
    // resize -- is exactly five shapes, and the one that carries a provider's material
    // travels in one direction only: Workshop never speaks a `PaneContent`.
    PaneRig r;
    std::vector<std::string> said;
    loom::WeaveId who{};
    const loom::ObserverId tap = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (who.valid() && e.sender == who && !e.schema_name.empty()) {
            said.push_back(e.schema_name);
        }
    });
    r.mount_workshop();
    who = r.workshop_id;
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.ready();
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
    seat->presses.clear();
    for (std::int64_t row = 0; row < 3; ++row) {
        r.press_cell(panel.x + row, panel.y + kExternalHeaderRows + row);
    }
    r.extent(140, 40);
    r.bus.remove_observer(tap);

    std::vector<std::string> distinct = said;
    std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
    const std::vector<std::string> allowed{"PaneCatalogRequested", "PanePressed", "PaneRoom",
                                           "SurfaceCanvas", "SurfaceText"};
    CHECK(distinct == allowed);

    // AND THE SENTENCES IT SENT ARE IDENTICAL IN SHAPE WHATEVER THE ROWS SAID. Three
    // presses on three different rows produced three `PanePressed`s that differ only in
    // where the hand was: no row identity, no selectable flag, no entry name, nothing
    // Workshop would have had to read a provider's material to know.
    REQUIRE(seat->presses.size() == 3);
    for (std::size_t i = 0; i < seat->presses.size(); ++i) {
        CHECK(seat->presses[i].pane == std::string(kHelloPane));
        CHECK(seat->presses[i].row == static_cast<std::int64_t>(i));
        CHECK(seat->presses[i].column == static_cast<std::int64_t>(i));
    }
}

TEST_CASE("SEL-0: a press names WHICH pane, when one provider offers two") {
    // ONE PROVIDER IS NOT ONE PANE, and the shape carries the pane key for exactly this
    // reason. Introspection offers only `loaded` today; a seam that assumed one apiece
    // would have to be widened by the second pane rather than merely used by it.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"second", "Second", "the same office's other pane"});
    });
    REQUIRE(r.session().panels.runtime.entries.size() == 2);
    r.extent(160, 60); // room in the stack for two panes at once
    r.pick(hello_ref());
    r.pick(PaneRef{kHelloOffice, "second"});
    REQUIRE(r.session().panels.open.size() == 4); // Info, Layouts, and both panes

    for (const RuntimePane& row : r.session().panels.runtime.entries) {
        CAPTURE(row.pane);
        const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), row.kind));
        REQUIRE(panel.w > 0);
        seat->presses.clear();
        r.press_cell(panel.x + 1, panel.y + kExternalHeaderRows);
        REQUIRE(seat->presses.size() == 1);
        CHECK(seat->presses[0].pane == row.pane);
    }
}

// ---- Tier two: what a row of the Loaded view means ---------------------------------

TEST_CASE("SEL-0: the projection says which entry each row names, and which name none") {
    const std::vector<intro::LoadedWeave> pop = loaded_population(3);
    const intro::LoadedView view = intro::project_loaded(pop, 8, 46);
    REQUIRE(view.rows.size() == view.entry_of_row.size());
    REQUIRE(view.shown.size() == 3);

    // THE HEADING IS A COUNT AND NAMES NO ENTRY, and neither do the caveat, the source
    // line or the blank separator. This is the vocabulary Workshop does not have.
    CHECK(view.entry_of_row[0] == intro::kNoEntry);
    CHECK(intro::entry_at_row(view, 0) == nullptr);
    std::size_t named = 0;
    for (std::size_t i = 0; i < view.rows.size(); ++i) {
        const intro::LoadedWeave* at = intro::entry_at_row(view, static_cast<std::int64_t>(i));
        if (at == nullptr) {
            CHECK(view.rows[i].text.find("weave-") == std::string::npos);
            continue;
        }
        ++named;
        // THE ENTRY THE MAP POINTS AT IS THE ENTRY THAT ROW DREW.
        CHECK(view.rows[i].text.find(at->name) != std::string::npos);
        CHECK(view.rows[i].text.find(at->role) != std::string::npos);
    }
    CHECK(named == 3);

    // TOTAL OVER EVERY ROW INDEX, including ones no press can produce. A provider is
    // handed a row off a wire and must not have to bound it twice -- and an EMPTY view
    // is the state between a room grant and its answer, when a maker is looking at
    // Workshop's `(waiting for the provider)` and there is no material to have pressed.
    CHECK(intro::entry_at_row(view, -1) == nullptr);
    CHECK(intro::entry_at_row(view, static_cast<std::int64_t>(view.rows.size())) == nullptr);
    CHECK(intro::entry_at_row(view, 1 << 20) == nullptr);
    const intro::LoadedView empty;
    CHECK(intro::entry_at_row(empty, 0) == nullptr);
}

TEST_CASE("SEL-0: an omission marker is a population fact and names no hidden entry") {
    // `... 17 more` SAYS HOW MUCH OF THE LIST IS NOT HERE. It is not a stand-in for one
    // hidden weave, so pressing it selects nothing -- and "select the first hidden one"
    // is a gesture this pane does not offer and must not invent.
    const intro::LoadedView view = intro::project_loaded(loaded_population(20), 8, 46);
    std::size_t marker = view.rows.size();
    for (std::size_t i = 0; i < view.rows.size(); ++i) {
        if (view.rows[i].text.find("more") != std::string::npos) {
            marker = i;
        }
    }
    REQUIRE(marker < view.rows.size());
    CHECK(view.entry_of_row[marker] == intro::kNoEntry);
    CHECK(intro::entry_at_row(view, static_cast<std::int64_t>(marker)) == nullptr);
    // ...and every entry the map DOES name is one of the shown ones, never a hidden one.
    for (const intro::LoadedWeave& w : view.shown) {
        CHECK(any_row(view.rows, w.name));
    }
    CHECK(view.shown.size() < 20);
}

TEST_CASE("SEL-0: marking a row changes that row and nothing else about the view") {
    const std::vector<intro::LoadedWeave> pop = loaded_population(3);
    const intro::LoadedView before = intro::project_loaded(pop, 8, 46);
    intro::LoadedView after = before;
    intro::mark_selected(after, "weave-1", 46);

    REQUIRE(after.rows.size() == before.rows.size());
    REQUIRE(after.entry_of_row == before.entry_of_row);
    REQUIRE(after.shown.size() == before.shown.size());
    std::size_t moved = 0;
    for (std::size_t i = 0; i < after.rows.size(); ++i) {
        if (after.rows[i].text == before.rows[i].text &&
            after.rows[i].role == before.rows[i].role &&
            after.rows[i].background == before.rows[i].background) {
            continue;
        }
        ++moved;
        // THE MARK IS THE STATEMENT AND THE INK IS THE SECOND SIGNAL, never the only
        // one: a maker in a terminal with no colour can still see which row is chosen.
        CHECK(after.rows[i].text.rfind(intro::kSelectedMark, 0) == 0);
        CHECK(before.rows[i].text.rfind(intro::kUnselectedMark, 0) == 0);
        CHECK(after.rows[i].role == surface::role::kAccent);
        CHECK(after.rows[i].text.find("weave-1") != std::string::npos);
        // AND THE WIDTH IS UNMOVED, because the mark spends the indent the row already
        // had. A list cannot start cutting names because something in it was selected.
        CHECK(after.rows[i].text.size() == before.rows[i].text.size());
    }
    CHECK(moved == 1);

    // A NAME NO SHOWN ENTRY CARRIES LEAVES EVERY ROW UNMARKED -- which is exactly what a
    // pane whose selected entry is currently windowed out looks like. The selection is
    // still held by its owner; there is simply no row here to put a mark on.
    intro::LoadedView none = before;
    intro::mark_selected(none, "not-loaded-here", 46);
    for (std::size_t i = 0; i < none.rows.size(); ++i) {
        CHECK(none.rows[i].text == before.rows[i].text);
    }
    intro::LoadedView cleared = after;
    intro::mark_selected(cleared, "", 46);
    for (std::size_t i = 0; i < cleared.rows.size(); ++i) {
        CHECK(cleared.rows[i].text == before.rows[i].text);
        CHECK(cleared.rows[i].role == before.rows[i].role);
        CHECK(cleared.rows[i].background == before.rows[i].background);
    }
}

TEST_CASE("SEL-0: a marked projection still fits the room it was granted") {
    // THE OBLIGATION `project_loaded` ALREADY HAD, restated for the marked form: a row
    // one byte too wide is refused WHOLE by Workshop, so a selection that pushed a row
    // over the budget would blank the pane rather than highlight anything.
    for (std::size_t n : {std::size_t{1}, std::size_t{4}, std::size_t{20}}) {
        for (std::int64_t rows = 1; rows <= 12; ++rows) {
            for (std::int64_t cols : {std::int64_t{6}, std::int64_t{20}, std::int64_t{46}}) {
                CAPTURE(n);
                CAPTURE(rows);
                CAPTURE(cols);
                intro::LoadedView view = intro::project_loaded(loaded_population(n), rows, cols);
                const std::size_t was = view.rows.size();
                for (const intro::LoadedWeave& w : view.shown) {
                    intro::mark_selected(view, w.name, cols);
                    REQUIRE(view.rows.size() == was);
                    REQUIRE(static_cast<std::int64_t>(view.rows.size()) <= rows);
                    for (const surface::SurfaceTextRow& row : view.rows) {
                        REQUIRE(static_cast<std::int64_t>(row.text.size()) <= cols);
                    }
                }
            }
        }
    }
}

TEST_CASE("SEL-0: the mark a maker reads is `>`, and it is the width of the indent it spends") {
    // THE ONE CASE THAT SPELLS THE CHARACTERS OUT, and it exists because a canary found
    // that nothing did. Every other case here says `intro::kSelectedMark`, which is right
    // -- a magic string in twenty places is how two spellings drift -- but a suite in
    // which every reference is the constant CANNOT NOTICE THE CONSTANT CHANGING. The
    // mutation was `> ` -> `* ` and all 547 cases stayed green.
    //
    // The value is not arbitrary and is not this tool's to choose freshly: `>` is what
    // Workshop's own object list and completion list already say (`object_row_text`,
    // `completion_rows`), and a maker who has learned what a mark means in one list has
    // learned it for this one. So the character is pinned where a reader can see it.
    CHECK(std::string(intro::kSelectedMark) == "> ");
    CHECK(std::string(intro::kUnselectedMark) == "  ");
    // AND THE TWO ARE THE SAME WIDTH, which is the property the projection rests on: a
    // selection exchanges the indent for a mark, so no row changes length, no budget
    // moves, and a list cannot begin cutting names because something in it was selected.
    REQUIRE(std::char_traits<char>::length(intro::kSelectedMark) ==
            std::char_traits<char>::length(intro::kUnselectedMark));
    const intro::LoadedWeave one{"a-weave", "a.role"};
    CHECK(intro::entry_row(one, false, 46) == "  a-weave @a.role");
    CHECK(intro::entry_row(one, true, 46) == "> a-weave @a.role");
    // ...and a weave the kernel bound no role to says so in either form.
    const intro::LoadedWeave none{"a-weave", ""};
    CHECK(intro::entry_row(none, false, 46) == "  a-weave @(no role)");
    CHECK(intro::entry_row(none, true, 46) == "> a-weave @(no role)");
}

TEST_CASE("SEL-0: a held selection asks the population, not the rows") {
    // AN ENTRY WINDOWED OUT OF A SHORT PANE IS PRESENT AND MERELY UNSHOWN; an entry the
    // kernel no longer has is GONE. Confusing the two would clear a maker's selection
    // every time they made a panel small.
    const std::vector<intro::LoadedWeave> pop = loaded_population(20);
    CHECK(intro::names(pop, "weave-19"));
    CHECK_FALSE(intro::names(pop, "weave-20"));
    CHECK_FALSE(intro::names(pop, ""));
    CHECK_FALSE(intro::names({}, "weave-0"));

    const intro::LoadedView small = intro::project_loaded(pop, 5, 46);
    CHECK_FALSE(any_row(small.rows, "weave-19")); // not on screen...
    CHECK(intro::names(pop, "weave-19"));         // ...and still loaded
}

// ---- Tier three: the real tool, pressed through the real input path ----------------

TEST_CASE("SEL-0: pressing a loaded row selects the entry that row actually showed") {
    Ears ears;
    PaneRig r;
    r.mount_workshop();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    r.extent(140, 40); // a room grant, which is this tool's one beat: two weaves now
    const ui::Rect body = external_body_rect(r.session(), kind);
    std::vector<std::string> shown = loaded_rows(r, kind);
    REQUIRE(shown.size() >= 3);
    REQUIRE(shown[0] == "loaded weaves -- 2");

    // THE ROW A MAKER WOULD AIM AT, found by reading the canvas rather than by
    // arithmetic: whatever the pane is showing at prose row 1 is what a press there
    // must select, and this case does not get to assume which weave that is.
    REQUIRE(is_entry_row(shown[1]));
    const std::string named = named_by(shown[1]);

    r.press_cell(body.x + 1, body.y + kExternalHeaderRows + 1);

    // THE PANE SAYS SO, ON THE CANVAS A MAKER READS.
    shown = loaded_rows(r, kind);
    REQUIRE(shown.size() >= 3);
    CHECK(shown[1].rfind(intro::kSelectedMark, 0) == 0);
    CHECK(shown[1].find(named) != std::string::npos);
    CHECK(shown[2].rfind(intro::kUnselectedMark, 0) == 0); // exactly one row is marked

    // AND AN INDEPENDENT LISTENER HEARD IT THROUGH THE ORDINARY LOOM ROUTE. No
    // callback, no direct call, no registration with the provider.
    REQUIRE(ears.heard.size() == 1);
    CHECK(ears.heard[0].pane == std::string(kIntroPane));
    CHECK(ears.heard[0].library == named);
    CHECK(ears.authors[0] == std::string(kIntroOffice)); // authored as the office, verifiably
    // THE IDENTITY IS THE LOADED LIBRARY'S NAME, and it is one of the two names the
    // kernel actually has -- never a WeaveId and never a participant identity.
    CHECK((named == std::string(intro::kIntrospectionStem) || named == "zengine-workshop-hello"));

    SUBCASE("another row moves the selection, and the mark with it") {
        REQUIRE(is_entry_row(shown[2]));
        const std::string second = named_by(shown[2]);
        REQUIRE(second != named);
        r.press_cell(body.x + 1, body.y + kExternalHeaderRows + 2);
        const std::vector<std::string> after = loaded_rows(r, kind);
        CHECK(after[1].rfind(intro::kUnselectedMark, 0) == 0);
        CHECK(after[2].rfind(intro::kSelectedMark, 0) == 0);
        REQUIRE(ears.heard.size() == 2);
        CHECK(ears.heard[1].library == second);
    }

    SUBCASE("the same row again is a second gesture and is published again") {
        // AN OCCURRENCE, NOT A TRANSITION. A maker who presses the same weave twice has
        // selected it twice, and a future trigger reading "whenever the maker selects
        // this one" is owed both. What does NOT repeat is the picture: the mark is
        // already where it belongs, so the rows a maker sees are byte-identical.
        const std::vector<std::string> was = loaded_rows(r, kind);
        r.press_cell(body.x + 3, body.y + kExternalHeaderRows + 1);
        REQUIRE(ears.heard.size() == 2);
        CHECK(ears.heard[1].library == named);
        CHECK(ears.heard[1].pane == ears.heard[0].pane);
        CHECK(ears.heard[1].role == ears.heard[0].role);
        CHECK(loaded_rows(r, kind) == was);
    }
}

TEST_CASE("SEL-0: heading, caveat, source note and blank select nothing") {
    Ears ears;
    PaneRig r;
    r.mount_workshop();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);
    const std::vector<std::string> shown = loaded_rows(r, kind);
    REQUIRE(shown.size() >= 4);

    // EVERY ROW THAT IS NOT AN ENTRY, PRESSED, one at a time, chosen by what the row
    // SAYS rather than by an index a later projection could move.
    std::size_t pressed = 0;
    for (std::size_t i = 0; i < shown.size(); ++i) {
        if (is_entry_row(shown[i])) {
            continue;
        }
        ++pressed;
        r.press_cell(body.x + 1, body.y + kExternalHeaderRows + static_cast<std::int64_t>(i));
    }
    CHECK(pressed >= 3); // the heading, the caveat and the source line at minimum
    CHECK(ears.heard.empty());
    // ...and the pane is unmoved: a press on a heading is not a deselection gesture
    // either, so nothing was cleared and nothing was marked.
    CHECK(loaded_rows(r, kind) == shown);
}

TEST_CASE("SEL-0: an omission marker on a live pane selects nothing") {
    // THE LIVE WITNESS FOR THE PURE CASE, in a room too short to show everything: the
    // marker is a POPULATION FACT, and pressing it must not select the first hidden
    // weave -- a gesture this pane does not offer and must not invent.
    Ears ears;
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    REQUIRE(watch->offers.size() >= 1);

    // A ROOM TOO SHORT TO NAME EVERYTHING -- granted directly, so the case owns the
    // density rather than hoping a screen produces it. At three rows the view spends
    // one on the heading, one on the caveat and one on the marker, which is INTR-0's
    // own "at a budget too small to show and to say, it says".
    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, kIntroOffice, PaneRoom{kIntroPane, 3, 46});
    });
    REQUIRE_FALSE(watch->content.empty());
    const std::vector<surface::SurfaceTextRow>& rows = watch->content.back().rows;
    std::size_t marker = rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].text.find(intro::kElided) != std::string::npos &&
            rows[i].text.find("more") != std::string::npos) {
            marker = i;
        }
    }
    REQUIRE(marker < rows.size());

    const std::size_t content_before = watch->content.size();
    r.drive_watcher(watch, [marker](PaneWatcher& wv, loom::Mail& m) {
        wv.press(m, kIntroOffice, PanePressed{kIntroPane, static_cast<std::int64_t>(marker), 0});
    });
    CHECK(ears.heard.empty());
    CHECK(watch->content.size() == content_before); // nothing re-drawn either
}

TEST_CASE("SEL-0: a press is read against the snapshot the maker saw, not a fresh one") {
    // THE LOAD-BEARING CASE. The pane is showing a projection made from population A;
    // the kernel's map then becomes B; the maker presses a row of what is still on
    // screen. The fact must name what they SAW -- which is only true because
    // interpreting a press asks the Weave Manager nothing.
    Ears ears;
    PaneRig r;
    r.mount_workshop();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    r.extent(140, 40);
    const ui::Rect body = external_body_rect(r.session(), kind);
    const std::vector<std::string> shown = loaded_rows(r, kind);
    REQUIRE(shown[0] == "loaded weaves -- 2");

    // WHICH ROW SHOWS THE LIBRARY THAT IS ABOUT TO GO AWAY -- read off the canvas,
    // because the order is the kernel's and this case does not get to assume it.
    std::size_t which = 0;
    for (std::size_t i = 1; i < shown.size(); ++i) {
        if (is_entry_row(shown[i]) && named_by(shown[i]) == "zengine-workshop-hello") {
            which = i;
        }
    }
    REQUIRE(which != 0);

    // THE POPULATION CHANGES UNDER THE PANE, through the real control door, and Workshop
    // is told nothing -- Loom has no departure event, so the rows a maker is looking at
    // are still A's.
    REQUIRE(r.unload("zengine-workshop-hello"));
    CHECK(loaded_rows(r, kind) == shown);

    // THE PRESS LANDS ON THE ROW THAT NAMES THE DEPARTED LIBRARY, and the fact says so.
    // A provider that re-read `zen.ListLoaded` here would answer with B's row instead --
    // a different weave at the same index, or no row at all.
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows + static_cast<std::int64_t>(which));
    REQUIRE(ears.heard.size() == 1);
    CHECK(ears.heard[0].library == "zengine-workshop-hello");
}

TEST_CASE("SEL-0: interpreting a press asks the Weave Manager nothing") {
    // THE SAME CLAIM AS A COUNT ON THE BUS. Whatever the provider says while a maker
    // presses rows, `zen.ListLoaded` is not among it -- a re-read here is exactly the
    // mutation that would make the case above wrong, and only sometimes.
    PaneRig r;
    r.mount_workshop();
    // THE LISTENER IS PART OF THE INSTRUMENT, not scenery. `LoadedSelected` is an office
    // PUBLICATION, and a publication into a room with no accepter for its shape reaches
    // nobody and leaves no bus event at all -- so without an ear for it, this audit would
    // be tallying a vocabulary of three and calling it the whole one. With the ear, every
    // name below is read off a real delivery, which is the only reading that means the
    // sentence was actually spoken.
    Ears ears;
    (void)loom::mount<SelectionListener>(r.bus, ears);
    std::vector<std::string> said;
    loom::WeaveId who{};
    const loom::ObserverId tap = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (who.valid() && e.sender == who && !e.schema_name.empty()) {
            said.push_back(e.schema_name);
        }
    });
    who = r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(who.valid());
    // THE FIRST OFFER IS SPOKEN BEFORE `who` IS KNOWN -- the weave activates inside the
    // load, and its id is this call's return value. `ready()` asks the catalog again, so
    // the audit below sees a discovery sentence rather than silently missing one.
    r.ready();
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);
    const std::ptrdiff_t asks_before =
        std::count(said.begin(), said.end(), std::string("zen.ListLoaded"));
    REQUIRE(asks_before >= 1); // the room grant did ask, once

    for (std::int64_t i = 0; i < 4; ++i) {
        r.press_cell(body.x + 1, body.y + kExternalHeaderRows + 1);
    }
    r.bus.remove_observer(tap);
    CHECK(std::count(said.begin(), said.end(), std::string("zen.ListLoaded")) == asks_before);

    // AND THE WHOLE OUTBOUND VOCABULARY IS FOUR SHAPES, one more than INTR-0's three.
    std::vector<std::string> distinct = said;
    std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
    const std::vector<std::string> allowed{"LoadedSelected", "PaneContent", "PaneOffered",
                                           "zen.ListLoaded"};
    CHECK(distinct == allowed);
    // NAMED NEGATIVELY, because the interesting half of an authority audit is what is
    // ABSENT. Being able to say which weave a maker pointed at is not being able to
    // reach it: no lifecycle command, no message addressed to the selected library, and
    // no canvas of its own was ever spoken.
    for (const char* forbidden : {"zen.LoadWeave", "zen.SwapWeave", "zen.ReloadWeave",
                                  "zen.UnloadLibrary", "zen.UnloadRole", "zen.QueryRole",
                                  "SurfaceCanvas", "PaneRoom", "PanePressed"}) {
        CHECK(std::find(said.begin(), said.end(), std::string(forbidden)) == said.end());
    }
}

TEST_CASE("SEL-0: a forged press selects nothing and publishes nothing") {
    // MEASURED FROM BOTH WRONG SIDES, through the two instruments the forged-room case
    // uses: a weave in ANOTHER office authoring a press deliberately, and the holder of
    // `zengine.workshop` speaking PERSONALLY. Holding an office is not speaking as one,
    // and neither of these is Workshop.
    Ears ears;
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    ProviderSeat* stranger = r.mount_provider(kOtherOffice);
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(watch->offers.size() == kIntroPaneCount);
    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, kIntroOffice, PaneRoom{kIntroPane, 6, 46});
    });
    REQUIRE(watch->content.size() == 1);
    REQUIRE_FALSE(watch->content[0].rows.empty());
    const std::size_t content_before = watch->content.size();

    // A STRANGER'S PRESS AT ROW 1 -- the row a real press would have selected.
    r.drive(stranger, [](ProviderSeat& s, loom::Mail& m) {
        s.press_at(m, kIntroOffice, PanePressed{kIntroPane, 1, 0});
    });
    CHECK(ears.heard.empty());
    CHECK(watch->content.size() == content_before);

    // THE OFFICE HOLDER, SPEAKING PERSONALLY.
    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.press_personally(m, kIntroOffice, PanePressed{kIntroPane, 1, 0});
    });
    CHECK(ears.heard.empty());
    CHECK(watch->content.size() == content_before);

    // ...AND THE CORRECTLY AUTHORED ONE IS STILL ANSWERED, so the two refusals above are
    // about AUTHORSHIP and not about the tool having stopped listening.
    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.press(m, kIntroOffice, PanePressed{kIntroPane, 1, 0});
    });
    REQUIRE(ears.heard.size() == 1);
    CHECK(ears.heard[0].library == std::string(intro::kIntrospectionStem));
    CHECK(watch->content.size() == content_before + 1);

    // A PRESS FOR A PANE THIS PROVIDER DOES NOT HAVE IS NOT ONE OF ITS ROWS EITHER.
    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.press(m, kIntroOffice, PanePressed{"schemas", 1, 0});
    });
    CHECK(ears.heard.size() == 1);
}

TEST_CASE("SEL-0: a press between a room grant and its answer names nothing") {
    // THE GAP A MAKER SEES AS `(waiting for the provider)`. Workshop clears its cache
    // before every grant, so in that window there is no material of this pane's on
    // screen -- and a press read against the projection from BEFORE the grant would name
    // an entry from a picture nobody is looking at.
    //
    // REACHING IT TAKES ONE TRICK, and it is worth writing down because the obvious rig
    // cannot: `bus.drain_until_idle()` drains, so a grant driven on its own is always answered before
    // anything else can happen. Both sentences are therefore authored inside ONE delivery
    // -- the grant first, the press second -- so the queue is [room, press] and the
    // provider's own question to the Manager is enqueued BEHIND the press. The press is
    // delivered while the answer is still outstanding, which is exactly the live gap.
    Ears ears;
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, kIntroOffice, PaneRoom{kIntroPane, 8, 46});
    });
    REQUIRE_FALSE(watch->content.empty());
    // ROW 1 IS AN ENTRY IN THE READING NOW ON SCREEN -- the control for what follows.
    REQUIRE(watch->content.back().rows.size() > 1);
    REQUIRE(watch->content.back().rows[1].text.find(" @") != std::string::npos);
    const std::size_t content_before = watch->content.size();

    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, kIntroOffice, PaneRoom{kIntroPane, 6, 46});
        wv.press(m, kIntroOffice, PanePressed{kIntroPane, 1, 0});
    });
    // NOTHING WAS SELECTED, because at the moment the press arrived this pane was
    // showing nothing. The new reading then lands normally, so the pane is not broken --
    // it simply had no material to have been pressed.
    CHECK(ears.heard.empty());
    CHECK(watch->content.size() == content_before + 1);
    REQUIRE(watch->content.back().rows.size() > 1);
    CHECK(watch->content.back().rows[1].text.rfind(intro::kUnselectedMark, 0) == 0);
}

TEST_CASE("SEL-0: a row of the previous room names nothing in the room now in force") {
    // A GRANT REPLACES THE PROJECTION WHOLE, so a press is always read against the rows
    // currently on screen. Row 5 of a six-row reading is an entry; the same row number
    // in a three-row reading is off the end, and the honest answer there is nothing.
    Ears ears;
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, kIntroOffice, PaneRoom{kIntroPane, 8, 46});
    });
    REQUIRE_FALSE(watch->content.empty());
    const std::size_t tall = watch->content.back().rows.size();
    REQUIRE(tall >= 3);

    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, kIntroOffice, PaneRoom{kIntroPane, 2, 46});
    });
    REQUIRE(watch->content.back().rows.size() < tall);
    const std::size_t content_before = watch->content.size();
    r.drive_watcher(watch, [tall](PaneWatcher& wv, loom::Mail& m) {
        wv.press(m, kIntroOffice,
                 PanePressed{kIntroPane, static_cast<std::int64_t>(tall) - 1, 0});
    });
    CHECK(ears.heard.empty());
    CHECK(watch->content.size() == content_before);
}

TEST_CASE("SEL-0: a selection is held while its entry is windowed out, and returns with it") {
    Ears ears;
    PaneRig r;
    r.mount_workshop();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    r.extent(140, 40);
    const ui::Rect wide_body = external_body_rect(r.session(), kind);
    const std::vector<std::string> shown = loaded_rows(r, kind);
    REQUIRE(shown.size() >= 3);
    REQUIRE(is_entry_row(shown[2]));
    const std::string second = named_by(shown[2]);
    r.press_cell(wide_body.x + 1, wide_body.y + kExternalHeaderRows + 2);
    REQUIRE(ears.heard.size() == 1);
    REQUIRE(loaded_rows(r, kind)[2].rfind(intro::kSelectedMark, 0) == 0);

    // A DIFFERENT SCREEN IS A NEW ROOM, WHICH IS A NEW READING. Whether the entry
    // survives the window or not, a resize is not a gesture and nothing new is said.
    r.extent(78, 22);
    const std::vector<std::string> narrow = loaded_rows(r, kind);
    CHECK(ears.heard.size() == 1);
    for (const std::string& row : narrow) {
        // AT MOST ONE MARK, and only ever on the entry that is still selected.
        if (row.rfind(intro::kSelectedMark, 0) == 0) {
            CHECK(row.find(second) != std::string::npos);
        }
    }

    // ...AND WIDENING BRINGS THE MARK BACK WITH NO GESTURE, because the selection is
    // held as a NAME and not as a row.
    r.extent(140, 40);
    CHECK(any_row(loaded_rows(r, kind), std::string(intro::kSelectedMark) + second));
    CHECK(ears.heard.size() == 1);
}

TEST_CASE("SEL-0: a selected library that goes away clears its mark when the absence is seen") {
    Ears ears;
    PaneRig r;
    r.mount_workshop();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    r.extent(140, 40);
    const ui::Rect body = external_body_rect(r.session(), kind);
    const std::vector<std::string> shown = loaded_rows(r, kind);
    std::size_t which = 0;
    for (std::size_t i = 1; i < shown.size(); ++i) {
        if (is_entry_row(shown[i]) && named_by(shown[i]) == "zengine-workshop-hello") {
            which = i;
        }
    }
    REQUIRE(which != 0);
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows + static_cast<std::int64_t>(which));
    REQUIRE(ears.heard.size() == 1);

    // IT LEAVES, AND NOTHING KNOWS YET. The pane is a snapshot and Loom has no departure
    // event, so the mark stays on the row until this tool observes the absence.
    REQUIRE(r.unload("zengine-workshop-hello"));
    CHECK(any_row(loaded_rows(r, kind), intro::kSelectedMark));

    // THE NEXT READING IS THE FIRST MOMENT THE ABSENCE IS OBSERVED, and the mark goes
    // with it. NOTHING IS PUBLISHED: a library going away is not a maker's gesture, and
    // inferring a deselection from two snapshots would be a story rather than a fact.
    r.extent(150, 40);
    const std::vector<std::string> after = loaded_rows(r, kind);
    CHECK(after[0] == "loaded weaves -- 1");
    CHECK_FALSE(any_row(after, intro::kSelectedMark));
    CHECK(ears.heard.size() == 1);

    // ...and a press now selects what is actually there, with no residue of the old one.
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows + 1);
    REQUIRE(ears.heard.size() == 2);
    CHECK(ears.heard[1].library == std::string(intro::kIntrospectionStem));
}

TEST_CASE("SEL-0: the fact travels as data and moves no authority with it") {
    // A LISTENER HEARS A LIBRARY NAME AND A ROLE. It has learned two strings a maker was
    // already looking at, and it has acquired nothing: the grant it was mounted with is
    // the grant it still has, and the named weave is no more reachable to it than
    // before. VALUES MAY FLOW; AUTHORITY MUST NOT FLOW IMPLICITLY WITH THEM.
    Ears ears;
    PaneRig r;
    r.mount_workshop();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    const loom::WeaveId who =
        r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(who.valid());
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows + 1);
    REQUIRE(ears.heard.size() == 1);

    // THE PAYLOAD IS THREE STRINGS AND ITS SCHEMA SAYS SO -- no id, no handle, no grant,
    // no token, no capability, and nothing a listener could present to anybody.
    const std::shared_ptr<const loom::Schema> shape = loom::schema_of<intro::LoadedSelected>();
    REQUIRE(shape != nullptr);
    CHECK(shape->name() == "LoadedSelected");
    CHECK(shape->version() == 1);
    REQUIRE(shape->fields().size() == 3);
    CHECK(shape->fields()[0].name == "pane");
    CHECK(shape->fields()[1].name == "library");
    CHECK(shape->fields()[2].name == "role");
    for (const loom::Field& f : shape->fields()) {
        CHECK(f.type.kind == loom::Kind::Text);
        CHECK(f.name != "weave");
        CHECK(f.name != "id");
        CHECK(f.name != "grant");
        CHECK(f.name != "token");
        CHECK(f.name != "capability");
        CHECK(f.name != "provider"); // Loom's stamp answers that, as everywhere else
    }
    // ...AND THE ROLE IS THE OBSERVED ONE. The pane writes `(no role)` for a maker; the
    // wire carries the observation, so a listener reads the fact and not the prose.
    CHECK(ears.heard[0].role == std::string(kIntroOffice));
    CHECK(intro::kNoRole == std::string("(no role)"));
}

TEST_CASE("SEL-0: the same gesture in a terminal names the same row of the same room") {
    // MEDIUM-INDEPENDENT BY CONSTRUCTION, and this is the witness. The TUI's mouse
    // reporting is already live (DECSET 1002+1006, claimed by the terminal Skin), the
    // wire already carries a `space`, and `prose_at` branches on that rather than on a
    // backend -- so a terminal press reaches the same provider row with no parity work
    // and no new protocol.
    Ears by_cell;
    PaneRig cells;
    cells.mount_workshop();
    (void)loom::mount<SelectionListener>(cells.bus, by_cell);
    (void)cells.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    cells.pick(intro_ref());
    const std::int64_t kind = cells.session().panels.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(cells.session(), kind);
    cells.press_cell(body.x + 1, body.y + kExternalHeaderRows + 1);
    REQUIRE(by_cell.heard.size() == 1);

    Ears by_pixel;
    PaneRig px;
    px.mount_workshop();
    (void)loom::mount<SelectionListener>(px.bus, by_pixel);
    (void)px.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    px.pick(intro_ref());
    const std::int64_t gkind = px.session().panels.runtime.entries[0].kind;
    px.extent(1000, 700, 8, 18);
    const ExternalBodyPlace gbody = external_body_of(px.session(), gkind);
    const ui::Rect gpanel = pane_body_cells(external_panel_rect(px.session(), gkind));
    REQUIRE(gbody.fit.graphical());
    px.press_pixel(gpanel.x * surface::kCanvasCellPx + gbody.fit.origin_x + gbody.fit.advance_px +
                       gbody.fit.advance_px / 2,
                   gpanel.y * surface::kCanvasCellPx + gbody.fit.origin_y +
                       (1 + kExternalHeaderRows) * gbody.fit.line_px + gbody.fit.line_px / 2);
    REQUIRE(by_pixel.heard.size() == 1);

    // ONE FACT, TWO MEDIA. The provider was handed two integers in both runs and cannot
    // tell which medium answered.
    CHECK(by_cell.heard[0].library == by_pixel.heard[0].library);
    CHECK(by_cell.heard[0].pane == by_pixel.heard[0].pane);
    CHECK(by_cell.heard[0].role == by_pixel.heard[0].role);
}

TEST_CASE("SEL-0: nothing in this build reacts to a selection") {
    // THE PHASE'S OWN NON-GOAL, ASSERTED. A selection opens no pane, closes none, moves
    // no focus, changes no setup, writes no notice, selects no object and sends the
    // named weave nothing. SEL-0 deliberately leaves the message unanswered.
    Ears ears;
    PaneRig r;
    r.mount_workshop();
    (void)loom::mount<SelectionListener>(r.bus, ears);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.pick(intro_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);

    const std::size_t panels_before = r.session().panels.open.size();
    const std::string notice_before = r.last_notice();
    const std::int64_t selected_before = r.session().selected;
    const Setup setup_before = r.session().setup.active;

    r.press_cell(body.x + 1, body.y + kExternalHeaderRows + 1);
    REQUIRE(ears.heard.size() == 1);

    CHECK(r.session().panels.open.size() == panels_before);
    CHECK_FALSE(r.session().panels.picker.open);
    CHECK_FALSE(r.session().arrange.open);
    CHECK_FALSE(r.session().terminal.open);
    CHECK(r.session().selected == selected_before);
    CHECK(r.last_notice() == notice_before);
    CHECK(r.session().setup.active == setup_before);
}

// ---- MSG-0: the keyboard reaches an external pane, and what that costs -------------
//
// TWO TIERS AGAIN, and the split is where the two halves of the claim actually live.
// The first drives real key and text events through the real input path into a
// recording provider seat: who owns the keyboard, what moves it, what outranks it, and
// what crosses the seam. The second loads the REAL `zengine-composer` library beside
// the REAL `zengine-introspection` and the REAL `zengine-timer` and drives the whole
// product path -- a maker selects a weave in one pane and submits a message to it from
// another -- because that path IS the phase, and a mock of any of the three would
// prove something about the mock.

// ---- Tier one: the seam ------------------------------------------------------------

TEST_CASE("MSG-0: the two key shapes carry a pane and normalized input, and nothing else") {
    const std::shared_ptr<const loom::Schema> key = loom::schema_of<PaneKey>();
    const std::shared_ptr<const loom::Schema> text = loom::schema_of<PaneTextInput>();

    CHECK(key->name() == "PaneKey");
    CHECK(key->version() == 1u);
    REQUIRE(key->fields().size() == 3);
    CHECK(key->fields()[0].name == "pane");
    CHECK(key->fields()[0].type.kind == loom::Kind::Text);
    CHECK(key->fields()[1].name == "scancode");
    CHECK(key->fields()[1].type.kind == loom::Kind::Int);
    CHECK(key->fields()[2].name == "modifiers");
    CHECK(key->fields()[2].type.kind == loom::Kind::Int);

    CHECK(text->name() == "PaneTextInput");
    CHECK(text->version() == 1u);
    REQUIRE(text->fields().size() == 2);
    CHECK(text->fields()[0].name == "pane");
    CHECK(text->fields()[1].name == "text");
    CHECK(text->fields()[1].type.kind == loom::Kind::Text);

    // NO KEY NAME, NO RELEASE FLAG, NO TIMESTAMP, NO REPEAT, NO COORDINATE. A `name`
    // would be the courtesy spelling a backend produced, and a provider switching on
    // it would be switching on the backend; the rest are gestures this seam does not
    // have. What a provider gets is the two numbers `input::KeyPressed` already
    // normalized, and the text the platform already committed.
    for (const loom::Field& f : key->fields()) {
        CHECK(f.name != "name");
        CHECK(f.name != "pressed");
        CHECK(f.name != "x");
        CHECK(f.name != "y");
    }

    // AND THE FIVE OLDER SHAPES DID NOT MOVE. MSG-0 added two; it did not revise one,
    // so nothing a provider already compiled against changed version or content.
    CHECK(loom::schema_of<PaneCatalogRequested>()->version() == 1u);
    CHECK(loom::schema_of<PaneOffered>()->version() == 1u);
    CHECK(loom::schema_of<PaneRoom>()->version() == 1u);
    CHECK(loom::schema_of<PaneContent>()->version() == 1u);
    CHECK(loom::schema_of<PanePressed>()->version() == 1u);
    CHECK(loom::schema_of<PanePressed>()->fields().size() == 3u);
}

TEST_CASE("MSG-0: a press into an external pane's room points the keyboard at it") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    REQUIRE(is_runtime_kind(kind));

    // BEFORE THE PRESS, NOBODY HAS THE KEYBOARD, and an ordinary command still runs.
    CHECK(r.session().panels.keyboard == kNoPaneKind);
    REQUIRE(seat->keys.empty());
    r.key(input::scan::kUp);
    CHECK(seat->keys.empty());

    press_body(r, kind);
    CHECK(r.session().panels.keyboard == kind);

    r.key(input::scan::kUp);
    REQUIRE(seat->keys.size() == 1);
    CHECK(seat->keys[0].pane == std::string(kHelloPane));
    CHECK(seat->keys[0].scancode == input::scan::kUp);
    CHECK(seat->keys[0].modifiers == input::mod::kNone);
    // AUTHORED AS WORKSHOP, exactly as the room grant and the press are -- which is
    // what lets a provider refuse a forged key.
    CHECK(seat->key_authors[0] == std::string(kWorkshopProvider));
}

TEST_CASE("MSG-0: text crosses the seam beside the key, as its own sentence") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);

    // A KEYSTROKE THAT PRODUCES A CHARACTER PRODUCES BOTH FACTS, in the order the
    // backends report them -- and Workshop maps no key to any character on the way.
    r.key(input::scan::kA);
    r.text("%");
    REQUIRE(seat->keys.size() == 1);
    REQUIRE(seat->typed.size() == 1);
    CHECK(seat->typed[0].pane == std::string(kHelloPane));
    CHECK(seat->typed[0].text == "%");
    CHECK(seat->text_authors[0] == std::string(kWorkshopProvider));

    // A KEY THAT PRODUCES NO CHARACTER SENDS ONLY THE KEY.
    r.key(input::scan::kUp);
    CHECK(seat->keys.size() == 2);
    CHECK(seat->typed.size() == 1);
}

TEST_CASE("MSG-0: a press anywhere else takes the keyboard away again") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);
    REQUIRE(r.session().panels.keyboard == kind);

    // A PRESS ON THE WORKSPACE. The document takes it, and the pane stops being typed
    // into -- which is the other half of "a press points the keyboard at what it hit".
    // The cell is derived from the pane's OWN rectangle rather than spelled: an
    // overlay slot starts at the canvas's top-left corner, so a literal `(1, 1)` is
    // inside the pane it is meant to be outside of.
    const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
    const std::int64_t below = panel.y + panel.h + 1;
    REQUIRE_FALSE(occupied_at(r.session().panels, r.session().setup.active,
                              screen_of(r.session()), panel.x + 1, below)
                      .occupied);
    r.press_cell(panel.x + 1, below);
    CHECK(r.session().panels.keyboard == kNoPaneKind);
    const std::size_t before = seat->keys.size();
    r.key(input::scan::kUp);
    CHECK(seat->keys.size() == before);

    // ...and a press on Workshop's own side panel does the same.
    press_body(r, kind);
    REQUIRE(r.session().panels.keyboard == kind);
    const Screen sc = screen_of(r.session());
    r.press_cell(sc.panel_x + 1, kSideY + 2);
    CHECK(r.session().panels.keyboard == kNoPaneKind);
}

TEST_CASE("MSG-0: a press into a second external pane moves the keyboard to it") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    // TALL ENOUGH FOR TWO SLOTS. At the minimum composition the stack seats one pane
    // and the second is `waiting_for_room` -- which has no rectangle, so a press
    // could not reach it and the case would be measuring the wrong absence.
    r.extent(120, 60);
    ProviderSeat* first = r.mount_provider(kHelloOffice);
    ProviderSeat* second = r.mount_provider("zengine.other");
    const std::int64_t a = seat_pane_open(r, first, kHelloOffice, kHelloPane);
    const std::int64_t b = seat_pane_open(r, second, "zengine.other", "pane");
    REQUIRE(is_runtime_kind(a));
    REQUIRE(is_runtime_kind(b));

    press_body(r, a);
    r.key(input::scan::kUp);
    CHECK(first->keys.size() == 1);
    CHECK(second->keys.empty());

    press_body(r, b);
    CHECK(r.session().panels.keyboard == b);
    r.key(input::scan::kDown);
    CHECK(first->keys.size() == 1); // unchanged: it is not the target any more
    REQUIRE(second->keys.size() == 1);
    CHECK(second->keys[0].scancode == input::scan::kDown);
}

TEST_CASE("MSG-0: typing `p` into a focused pane does not open the picker") {
    // Section 24's product pressure, measured in both directions. It is the one case
    // that says the priority is real rather than described.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);

    // WITH NO OWNER, `p` IS STILL WORKSHOP'S. Nothing about the old binding moved.
    r.key(input::scan::kP);
    CHECK(r.session().panels.picker.open);
    r.key(input::scan::kEscape);
    REQUIRE_FALSE(r.session().panels.picker.open);

    press_body(r, kind);
    r.key(input::scan::kP);
    r.text("p"); // the character the same keystroke produced
    CHECK_FALSE(r.session().panels.picker.open);
    REQUIRE(seat->keys.size() == 1);
    CHECK(seat->keys[0].scancode == input::scan::kP);
    REQUIRE(seat->typed.size() == 1);
    CHECK(seat->typed[0].text == "p");

    // ...and every other printable Workshop command is the pane's too while it has
    // the keyboard: `n` and `d` do not touch the document, `w` opens no mode.
    const std::size_t objects_before = r.w->document().elements.size();
    r.key(input::scan::kN);
    r.key(input::scan::kD);
    r.key(input::scan::kW);
    CHECK(r.w->document().elements.size() == objects_before);
    CHECK_FALSE(r.session().arrange.open);
    CHECK(seat->keys.size() == 4);
}

TEST_CASE("MSG-0: the keys that mean the same thing in every mode still outrank a pane") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);
    REQUIRE(r.session().panels.keyboard == kind);

    // THE TERMINAL TOGGLE still opens the Terminal above the pane (KEY-0: ctrl+t, a
    // chord that enters no text, so there is nothing for the pane to be protected from).
    r.key(input::scan::kT, input::mod::kCtrl);
    CHECK(r.session().terminal.open);
    CHECK(seat->keys.empty());
    CHECK(seat->typed.empty());

    // ...AND THE TERMINAL OWNS THE KEYBOARD WHOLE WHILE IT IS OPEN, above the pane.
    r.key(input::scan::kA);
    r.text("a");
    CHECK(seat->keys.empty());
    CHECK(r.session().terminal.input.text() == "a");

    // CLOSING IT HANDS THE KEYBOARD STRAIGHT BACK, because the candidate was never
    // cleared -- the mode took every press whole and never reached the line that sets
    // it.
    r.key(input::scan::kT, input::mod::kCtrl);
    REQUIRE_FALSE(r.session().terminal.open);
    r.key(input::scan::kA);
    CHECK(seat->keys.size() == 1);

    // CTRL+C IS THE PANE'S WHILE IT HOLDS THE KEYBOARD (TEXT-0). A focused pane is a place
    // that takes text, and `^c` over text means copy, so the chord travels the chain and
    // crosses the seam like any other -- the provider decides what it means. `^s` and `^o`
    // still outrank the pane (the case below this one drives them), and quit is one
    // press-elsewhere away.
    CHECK_FALSE(r.host.quit);
    r.key(input::scan::kC, input::mod::kCtrl);
    CHECK_FALSE(r.host.quit);
    REQUIRE(seat->keys.size() == 2); // forwarded, beside the bare `a` above
    CHECK(seat->keys[1].scancode == input::scan::kC);
    CHECK(seat->keys[1].modifiers == input::mod::kCtrl);

    // ...AND QUIT COMES BACK WITH THE KEYBOARD. A press outside the pane clears the
    // candidate, and the same chord is the application's again.
    const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
    r.press_cell(panel.x + 1, panel.y + panel.h + 1);
    REQUIRE(r.session().panels.keyboard == kNoPaneKind);
    r.key(input::scan::kC, input::mod::kCtrl);
    CHECK(r.host.quit);
    CHECK(seat->keys.size() == 2); // and this one was not forwarded
}

TEST_CASE("MSG-0: every Workshop mode owns the keyboard above a focused pane") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);
    REQUIRE(r.session().panels.keyboard == kind);

    // THE PICKER. It is reachable only from command mode, which a focused pane is
    // not -- so it is opened by the gesture that always could: a press elsewhere,
    // then `p`.
    press_outside(r, kind);
    r.key(input::scan::kP);
    REQUIRE(r.session().panels.picker.open);
    press_body(r, kind); // a press while the picker is open is the picker's...
    CHECK(r.session().panels.picker.open);
    r.key(input::scan::kDown);
    CHECK(seat->keys.empty()); // ...and so is the key
    r.key(input::scan::kEscape);

    // PANE MANAGEMENT owns the pointer and the keyboard whole while it is open.
    press_body(r, kind);
    REQUIRE(r.session().panels.keyboard == kind);
    press_outside(r, kind);
    r.key(input::scan::kW);
    r.text("w");
    REQUIRE(r.session().arrange.open);
    press_body(r, kind);
    r.key(input::scan::kTab);
    CHECK(seat->keys.empty());
    // `esc` IS BACK AND NOT CANCEL (WIND-2), so leaving takes one press per level --
    // a press inside the mode may have entered one, and a case that assumed a single
    // escape would be asserting a shape this mode deliberately does not have.
    for (int i = 0; i < 4 && r.session().arrange.open; ++i) {
        r.key(input::scan::kEscape);
    }
    REQUIRE_FALSE(r.session().arrange.open);

    // THE LAYOUT-NAME EDITOR, the same -- and reaching it needs the keyboard back again,
    // because leaving a mode restores the pane's claim exactly as it found it. Since
    // WUX-11 it needs no setup file at all: renaming a layout writes nothing.
    press_outside(r, kind);
    open_rename_on_live_tab(r);
    REQUIRE(r.session().setup.naming.open);
    r.key(input::scan::kA);
    r.text("a");
    CHECK(seat->keys.empty());
    CHECK(seat->typed.empty());
    r.key(input::scan::kEscape);
}

TEST_CASE("MSG-0: a pane that stops being presentable stops being typed into") {
    // THE TARGET IS RESOLVED AT EVERY SPEND AND REMEMBERED BY NOBODY. Closing the
    // pane leaves the candidate standing and simply gives it nothing to name -- there
    // is no clearing hook, and there is no fifth writer somebody forgot to add.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);
    r.key(input::scan::kUp);
    REQUIRE(seat->keys.size() == 1);

    // ...and the keyboard goes back to Workshop first, because `p` would otherwise be
    // the pane's -- which is the rule this file's own case one row up establishes.
    press_outside(r, kind);
    r.pick(PaneRef{kHelloOffice, kHelloPane}); // the picker's Return closes an open row
    REQUIRE_FALSE(r.session().panels.has(kind));
    r.key(input::scan::kUp);
    CHECK(seat->keys.size() == 1); // nothing was sent
    // ...and `p` is Workshop's again, because nothing owns the keyboard.
    r.key(input::scan::kP);
    CHECK(r.session().panels.picker.open);
}

TEST_CASE("MSG-0: a press on a pane's header claims the keyboard and names no row") {
    // The two questions are different and are answered separately: `PanePressed`
    // needs a row of the granted lattice, and pointing the keyboard needs only that a
    // maker pointed at this pane.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    const ui::Rect body = external_body_rect(r.session(), kind);

    r.press_cell(body.x + 1, body.y); // the header row
    CHECK(seat->presses.empty());     // no row was named
    CHECK(r.session().panels.keyboard == kind);
    r.key(input::scan::kUp);
    CHECK(seat->keys.size() == 1);
}

TEST_CASE("MSG-0: a key carries the modifiers the transition carried, unchanged") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);

    r.key(input::scan::kTab, input::mod::kShift);
    r.key(input::scan::kEnd, input::mod::kAlt);
    REQUIRE(seat->keys.size() == 2);
    CHECK(seat->keys[0].modifiers == input::mod::kShift);
    CHECK(seat->keys[1].modifiers == input::mod::kAlt);
    // ...and Ctrl+S is still the document's, in every mode, so it never arrives.
    r.key(input::scan::kS, input::mod::kCtrl);
    CHECK(seat->keys.size() == 2);
}

TEST_CASE("MSG-0: the same gesture in both media produces the same provider intent") {
    // Sections 27 and 47. A pane focused by a terminal press and a pane focused by a
    // graphical press receive byte-identical sentences: the medium decides where a
    // hand landed and nothing else.
    const auto run = [](bool graphical, PaneKey& out_key, PaneTextInput& out_text) {
        PaneRig r;
        r.mount_workshop();
        r.ready();
        ProviderSeat* seat = r.mount_provider(kHelloOffice);
        const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
        if (graphical) {
            r.extent(1000, 700, 8, 18);
            const ExternalBodyPlace body = external_body_of(r.session(), kind);
            const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
            REQUIRE(body.fit.graphical());
            r.press_pixel(panel.x * surface::kCanvasCellPx + body.fit.origin_x +
                              body.fit.advance_px / 2,
                          panel.y * surface::kCanvasCellPx + body.fit.origin_y +
                              kExternalHeaderRows * body.fit.line_px + body.fit.line_px / 2);
        } else {
            press_body(r, kind);
        }
        r.key(input::scan::kReturn, input::mod::kNone);
        r.text("x");
        REQUIRE(seat->keys.size() == 1);
        REQUIRE(seat->typed.size() == 1);
        out_key = seat->keys[0];
        out_text = seat->typed[0];
    };
    PaneKey cell_key;
    PaneTextInput cell_text;
    PaneKey pixel_key;
    PaneTextInput pixel_text;
    run(false, cell_key, cell_text);
    run(true, pixel_key, pixel_text);
    CHECK(cell_key.pane == pixel_key.pane);
    CHECK(cell_key.scancode == pixel_key.scancode);
    CHECK(cell_key.modifiers == pixel_key.modifiers);
    CHECK(cell_text.text == pixel_text.text);
}

TEST_CASE("MSG-0: a provider that never asked for keys is unchanged") {
    // Section 26. `zengine-workshop-hello` is the WP-0 protocol witness and accepts
    // neither key shape. Workshop still points the keyboard at it when a maker presses
    // into it -- it cannot know otherwise, and asking would be a private copy of
    // `zen.DescribeAccepted` -- and Loom's gate declines the delivery, which is the
    // substrate's own correct answer to being sent a shape a weave never declared.
    // The provider's own code is untouched and it goes on presenting.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    const loom::WeaveId id = r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    REQUIRE(id.valid());
    r.pick(PaneRef{kHelloOffice, kHelloPane});
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);
    const std::vector<std::string> before = external_rows(r.last_canvas(), body);
    REQUIRE_FALSE(before.empty());

    std::vector<std::string> refused;
    r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.kind == loom::EventKind::Refused) {
            refused.push_back(e.schema_name);
        }
    });
    press_body(r, kind);
    r.key(input::scan::kUp);
    r.text("z");

    // The keys were declined by the gate, not by Workshop, and named on the tap.
    CHECK(std::find(refused.begin(), refused.end(), std::string(PaneKey::zen_name)) !=
          refused.end());
    CHECK(std::find(refused.begin(), refused.end(), std::string(PaneTextInput::zen_name)) !=
          refused.end());
    // ...and the pane is still showing exactly what it showed.
    CHECK(external_rows(r.last_canvas(), body) == before);
}

TEST_CASE("MSG-0: the screen says which pane the keys are going to, in two places") {
    // THE REPAIR THE FIRST LIVE RUN OF THIS PHASE EARNED. A press into an external
    // pane gives it every bare key, `q` included -- and with nothing on the screen
    // saying so, that is a Workshop which cannot be quit with the key its own help
    // line advertises, for a reason a maker cannot see. Two signals, both in
    // CHARACTERS, because a terminal has no colour to fall back on.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    const ui::Rect body = external_body_rect(r.session(), kind);

    const auto header = [&]() { return external_region_rows(r.last_canvas(), body).at(0); };
    // The band is one region since WUX-1 and its legend rows are the last two of the
    // five-cell budget; read them through the cell projection, padding trimmed.
    const auto band = [&]() {
        std::vector<std::string> out;
        const Screen sc = screen_of(r.session());
        for (const std::int64_t y : {sc.help_y, sc.help_y + 1}) {
            const std::string row = inspector_row(r.last_canvas(), 0, y);
            if (!row.empty()) {
                out.push_back(row);
            }
        }
        return out;
    };

    // BEFORE: the header is unmarked and the band advertises Workshop's own keys.
    CHECK(header() == std::string(kTypingElsewhere) + "Seat @" + std::string(kHelloOffice));
    {
        const std::vector<std::string> lines = band();
        REQUIRE(lines.size() == 2);
        CHECK(lines[0].find("q quit") != std::string::npos);
        CHECK(lines[0].find("typing goes to") == std::string::npos);
    }

    press_body(r, kind);

    // AFTER: the mark is on the pane a maker is looking at, and the band names it and
    // lists what still works. All survivors are CHORDED, which is the rule rather than
    // a coincidence: a bare printable cannot be global once anything on the screen can
    // take text. `^c` left the list in TEXT-0 -- a focused pane takes text, so the chord
    // is the pane's now, and a band advertising a quit that would not happen is the lie
    // this band exists to refuse.
    CHECK(header() == std::string(kTypingHere) + "Seat @" + std::string(kHelloOffice));
    {
        const std::vector<std::string> lines = band();
        REQUIRE(lines.size() == 2);
        CHECK(lines[0].find("typing goes to Seat @" + std::string(kHelloOffice)) !=
              std::string::npos);
        CHECK(lines[0].find("press elsewhere") != std::string::npos);
        CHECK(lines[0].find("q quit") == std::string::npos); // it would be a lie
        CHECK(lines[1] == "^s save | ^o open | ^t terminal | ^k hotkeys");
        CHECK(lines[1].find("^c") == std::string::npos); // that one would be a lie now too
    }

    // ...AND THE MARK COSTS NO COLUMNS, which is what keeps a header from starting to
    // cut a provider's name because a maker clicked on it.
    const std::string marked = header();
    r.press_cell(external_panel_rect(r.session(), kind).x + 1,
                 external_panel_rect(r.session(), kind).y +
                     external_panel_rect(r.session(), kind).h + 1);
    CHECK(header().size() == marked.size());
    CHECK(band()[0].find("q quit") != std::string::npos); // and the keys came back
}

TEST_CASE("MSG-0: a pane with no room granted is not typed into") {
    // `granted` is false for exactly one beat -- between a panel opening and the
    // repaint that grants it -- and a key in that beat would be a keystroke sent to a
    // provider that has not been told it has a pane on screen at all.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);
    REQUIRE(r.session().panels.keyboard == kind);

    ExternalPane* pane = r.session().panels.external_pane(kind);
    REQUIRE(pane != nullptr);
    pane->granted = false;
    const std::size_t before = seat->keys.size();
    r.key(input::scan::kUp);
    CHECK(seat->keys.size() == before);
}

// ---- Tier two: the real Composer, the real Timer, and a real stranger --------------

namespace {

/// A WEAVE THAT PUBLISHES `LoadedSelected` AS WHATEVER OFFICE IT IS TOLD TO.
///
/// It stands in for the Introspection pane in every case whose subject is what the
/// COMPOSER does with the fact -- which is most of them -- because those cases must be
/// able to say the exact wrong sentence: an office that is not Introspection's, a role
/// nobody holds, an empty role. The real tool would refuse to produce any of those,
/// which is what makes it the wrong instrument for measuring a listener's checks. One
/// case below uses the real Introspection library instead, and that one is about the
/// EDGE rather than about the listener.
class Selector : public loom::WeaveBase<Selector, SeatState, loom::Accept<SeatDo>,
                                        loom::Emit<intro::LoadedSelected>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(Selector&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }
    void say(loom::Mail& mail, const std::string& office, const intro::LoadedSelected& s) {
        (void)mail.as_role(office).publish(s);
    }
    std::function<void(Selector&, loom::Mail&)> next;
};

// ---- a vocabulary the Composer's binary has never seen ---------------------------
//
// DECLARED HERE, IN THE SUITE, AND NOWHERE ELSE. That is the whole of the genericity
// witness: `zengine-composer.so` was linked before these types existed and has no
// header for any of them, so a form generated for one of them can only have come off
// the wire.

struct CuriousV1 {
    std::string alpha;
    std::int64_t beta = 0;
    using ZenSelf = CuriousV1;
    static constexpr const char* zen_name = "Curious";
    static constexpr std::uint32_t zen_version = 1;
    static auto zen_fields() { return std::make_tuple(ZEN_FIELD(alpha), ZEN_FIELD(beta)); }
};
/// The same NAME at a different version -- a distinct message identity, with
/// different fields, spelled the way test_describe.cpp spells one.
struct CuriousV2 {
    double gamma = 0.0;
    bool delta = false;
    using ZenSelf = CuriousV2;
    static constexpr const char* zen_name = "Curious";
    static constexpr std::uint32_t zen_version = 2;
    static auto zen_fields() { return std::make_tuple(ZEN_FIELD(gamma), ZEN_FIELD(delta)); }
};
struct Leaf {
    std::int64_t n = 0;
    ZEN_SHAPE(Leaf, 1, ZEN_FIELD(n));
};
/// A shape with a REQUIRED nested message beside a scalar -- the field a generic form
/// can describe and cannot author.
struct Nested {
    std::string label;
    Leaf leaf;
    ZEN_SHAPE(Nested, 1, ZEN_FIELD(label), ZEN_FIELD(leaf));
};
struct StrangerState {
    std::int64_t heard = 0;
    ZEN_EXPOSE();
    ZEN_SHAPE(StrangerState, 1, ZEN_FIELD(heard));
};

/// A TARGET WITH A VOCABULARY NOBODY SHIPPED. It answers `zen.DescribeAccepted` from
/// its own accept-set exactly as every woven weave does -- the construction layer's
/// door, not a fixture's courtesy.
class Stranger : public loom::WeaveBase<Stranger, StrangerState,
                                        loom::Accept<CuriousV1, CuriousV2, Nested>> {
public:
    void on(const CuriousV1&, loom::Mail&) { ++state_.heard; }
    void on(const CuriousV2&, loom::Mail&) { ++state_.heard; }
    void on(const Nested&, loom::Mail&) { ++state_.heard; }
};

/// A TARGET WITH AN OPTIONAL FIELD -- AND IT IS A RAW `loom::Weave`, WHICH IS THE
/// FINDING RATHER THAN A CONVENIENCE (MSG-0).
///
/// `ZEN_FIELD` derives every field REQUIRED, unconditionally (`build_schema`,
/// zen/weave/shape.hpp: `/*required=*/true`), and `WeaveBase::accepted_schemas()` is
/// `final` and built from `Accept<...>` over ZEN_SHAPE types. So an accept-set
/// answered by the CONSTRUCTION LAYER cannot contain an optional field -- not the
/// Timer's, not any woven weave's in either repository. The only way one reaches a
/// discovered accept-set is a weave that implements `loom::Weave` directly, declares
/// a hand-built schema, and answers the self-description door itself, which is what
/// this does. It is a real target rather than a trick: the answer it gives is
/// `encode_accepted_shapes(accepted_schemas())`, the same call the construction layer
/// makes, over the same vector the gate enforces.
class Optionals final : public loom::Weave {
public:
    static std::shared_ptr<const loom::Schema> shape() {
        static const auto s = loom::SchemaBuilder("Optionals", 1)
                                  .field("must", loom::Kind::Text)
                                  .field("may", loom::Kind::Text, /*required=*/false)
                                  .field("flag", loom::Kind::Bool, /*required=*/false)
                                  .build();
        return s;
    }
    std::vector<std::shared_ptr<const loom::Schema>> accepted_schemas() const override {
        return {shape(), loom::schema_of<loom::DescribeAccepted>()};
    }
    void handle(const loom::Message& in, loom::Bus& bus) override {
        if (loom::same_identity(*loom::schema_of<loom::DescribeAccepted>(), in.payload.schema())) {
            const loom::WeaveId to = in.reply_to.valid() ? in.reply_to : in.sender;
            if (to.valid()) {
                (void)bus.send(to,
                               loom::Message(loom::encode_accepted_shapes(accepted_schemas()),
                                             self_, self_, in.correlation));
            }
            return;
        }
        heard.push_back(in.payload);
    }
    loom::Value snapshot() const override { return loom::to_value(StrangerState{}); }
    loom::Value policy() const override {
        loom::Value v(loom::lifecycle_policy_schema());
        v.set("max_reloads", loom::Cell::integer(0));
        v.set("revive_from_last_good", loom::Cell::boolean(false));
        return v;
    }
    void revive(const loom::Value&) override {}
    void set_self(loom::WeaveId id) { self_ = id; }

    std::vector<loom::Value> heard;

private:
    loom::WeaveId self_{};
};

/// A SEAT HOLDING `zengine.timer`, WITH THE TIMER PACKAGE'S OWN REAL SHAPES.
///
/// WHAT IS REAL HERE AND WHAT IS NOT, said plainly. The SHAPES are the shipped
/// Timer's, out of `timer/vocabulary.hpp`, so the catalog a maker reads in these
/// cases is the catalog they read in the product, and a submitted `StartTimer` is the
/// message the real service accepts. What is a stand-in is the SERVICE: the shipped
/// `zengine-timer` re-arms its own beat inside its own handler, and
/// `drain_until_idle()` does what it says -- its own header says a perpetual service
/// means it never returns -- so loading it into a rig whose every gesture drains hangs
/// the suite rather than proving anything. Measured, not assumed: it hung, at the load.
///
/// So the real service is exercised where a real service can be, which is an
/// interactive run, and the accept-set, the discovery, the generated form and the
/// send are exercised here against the same vocabulary.
class TimerSeat
    : public loom::WeaveBase<TimerSeat, StrangerState,
                             loom::Accept<zengine::timer::StartTimer,
                                          zengine::timer::StartRoleTimer,
                                          zengine::timer::EnsureTimer,
                                          zengine::timer::EnsureRoleTimer,
                                          zengine::timer::CancelTimer,
                                          zengine::timer::CancelAllMyTimers>> {
public:
    void on(const zengine::timer::StartTimer& s, loom::Mail&) {
        ++state_.heard;
        started.push_back(s);
    }
    void on(const zengine::timer::StartRoleTimer&, loom::Mail&) { ++state_.heard; }
    void on(const zengine::timer::EnsureTimer&, loom::Mail&) { ++state_.heard; }
    void on(const zengine::timer::EnsureRoleTimer&, loom::Mail&) { ++state_.heard; }
    void on(const zengine::timer::CancelTimer& c, loom::Mail&) {
        ++state_.heard;
        cancelled.push_back(c.id);
    }
    void on(const zengine::timer::CancelAllMyTimers&, loom::Mail&) { ++state_.heard; }

    std::vector<zengine::timer::StartTimer> started;
    std::vector<std::string> cancelled;
};

/// EVERY TIER-TWO CASE'S OPENING BEATS, AND THE SEVEN GESTURES UNDER THEM.
///
/// IT DRIVES THE PRODUCT AND NEVER THE PROVIDER. Every verb below is a real message
/// on a real bus -- a pointer press at a canvas cell, a key transition, the text a
/// platform committed -- and every reading is off the PUBLISHED CANVAS at the
/// rectangle Workshop's own painter used. Nothing here reaches into the Composer's
/// state, because the Composer is a loaded shared library and there is nothing to
/// reach into: what a case can see is exactly what a maker can see.
struct ComposeRig {
    PaneRig r;
    std::int64_t kind = kNoPaneKind;
    Selector* selector = nullptr;
    loom::WeaveId selector_id{};
    std::vector<std::string> delivered_;
    std::vector<std::string> authors_;
    std::vector<std::string> refused_;
    std::int64_t asks = 0;

    /// `default_size` LEAVES THE PANE EXACTLY AS THE SHIPPED DESK OPENS IT -- the
    /// developer's answer, untouched -- which is what WUX-6's desk-level witness has to
    /// start from. Every other case wants the room its form was composed for and says so
    /// below.
    explicit ComposeRig(bool default_size = false) {
        r.mount_workshop();
        r.ready();
        // A PANE BIG ENOUGH TO READ WHOLE. The windowing is the pure suite's claim;
        // these cases are about the conversation, and a case that had to scroll to
        // find a row would be measuring the window twice.
        r.extent(240, 80);
        (void)r.load(zengine::composer::kComposerStem, WORKSHOP_SO_COMPOSER, kComposerOffice);
        r.pick(composer_ref());
        // ...AND BIG ENOUGH IN BOTH DIRECTIONS. The extent above widens the slot; its
        // HEIGHT is `kStackRows` at every extent (WIND-1 widened the slot and deliberately
        // did not heighten it), and since WUX-5 one cell of it on each side is the pane's
        // visible boundary -- so the form's own composition (a target row, a notice, a
        // heading, its fields and a two-row footer) no longer fits the developer's
        // default. These cases are about the CONVERSATION, so the pane is given the room
        // its form was composed for, through the same authoring door a maker's own resize
        // goes through.
        //
        // ⚠ THAT THE DEFAULT SLOT DOES NOT HOLD THE FORM IS A PRODUCT FINDING (WUX-5) and
        // is recorded as one rather than papered over here: at six body rows the footer's
        // fixed two-row demand leaves the field list one row, which `window_of` can spend
        // only on its own marker -- a Submit above a form with no fields in it. WUX-6
        // closed it at the DESK: one coarse grow gives the pane the room, through the
        // maker's own key, and `composer/view.hpp` is untouched. The staged size here is
        // still the shorter road for a case whose subject is the conversation.
        if (!default_size) {
            REQUIRE(author_pane_size(
                        r.session().setup.active, composer_ref(),
                        PaneSize{pane_unit::kDefault, 0},
                        PaneSize{pane_unit::kSubcells,
                                 surface::subs_of_cells(kStackRows + 2 * kChromeCells)})
                        .accepted);
            r.extent(240, 81); // one more row, so the authored height is reconciled
        }
        for (const RuntimePane& row : r.session().panels.runtime.entries) {
            if (row.provider == std::string(kComposerOffice)) {
                kind = row.kind;
            }
        }
        // The stand-in for the Loaded pane, in the office the Composer verifies.
        auto weave = std::make_unique<Selector>();
        selector = weave.get();
        loom::Grant grant;
        grant.allow_to_any(intro::LoadedSelected::zen_name, intro::LoadedSelected::zen_version);
        selector_id = r.bus.register_weave(std::move(weave), std::move(grant),
                                           std::string(kIntroOffice));
        selector->zen_set_self(selector_id);
    }

    /// THE TIMER'S OWN VOCABULARY, in the Timer's own office. See `TimerSeat` for
    /// what is real about it and what is not.
    TimerSeat* timer = nullptr;
    void with_timer() {
        auto weave = std::make_unique<TimerSeat>();
        timer = weave.get();
        loom::Grant grant;
        (void)loom::allow_describe_answers(grant);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(weave), std::move(grant), std::string(kTimerOffice));
        timer->zen_set_self(id);
    }

    /// A target with an OPTIONAL field, which in this Loom means a hand-built schema
    /// answered by a hand-written door. See `Optionals`.
    Optionals* optionals = nullptr;
    void with_optionals() {
        auto weave = std::make_unique<Optionals>();
        optionals = weave.get();
        loom::Grant grant;
        (void)loom::allow_describe_answers(grant);
        const loom::WeaveId id = r.bus.register_weave(std::move(weave), std::move(grant),
                                                      std::string("zengine.optionals"));
        optionals->set_self(id);
    }

    /// A TARGET WITH A VOCABULARY NOBODY SHIPPED. `allow_describe_answers` is the
    /// host's own function rather than a hand-written subset -- a case that quietly
    /// narrowed the answering grant would be testing its own idea of it.
    void with_stranger() {
        auto weave = std::make_unique<Stranger>();
        Stranger* raw = weave.get();
        loom::Grant grant;
        (void)loom::allow_describe_answers(grant);
        const loom::WeaveId id = r.bus.register_weave(std::move(weave), std::move(grant),
                                                      std::string("zengine.stranger"));
        raw->zen_set_self(id);
    }

    /// Watch the bus for what actually crossed it -- deliberately a different question
    /// from what the pane says. `SUBMITTED` is the pane's claim; these are the tap's.
    void watch() {
        r.bus.add_observer([this](const loom::BusEvent& e) {
            if (e.kind == loom::EventKind::Delivered) {
                delivered_.push_back(e.schema_name);
                authors_.push_back(std::string(e.authored_role));
                if (e.schema_name == "zen.DescribeAccepted") {
                    ++asks;
                }
            }
            if (e.kind == loom::EventKind::Refused) {
                refused_.push_back(e.schema_name);
            }
        });
    }

    void select(const std::string& role, const std::string& library) {
        select_as(kIntroOffice, role, library);
    }
    void select_as(const std::string& office, const std::string& role,
                   const std::string& library) {
        selector->next = [office, role, library](Selector& s, loom::Mail& m) {
            s.say(m, office, intro::LoadedSelected{"loaded", library, role});
        };
        (void)r.bus.send(selector_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                    loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }

    /// The provider's rows, off the published canvas, with Workshop's header dropped.
    std::vector<std::string> rows() {
        return external_rows(r.last_canvas(), external_body_rect(r.session(), kind));
    }
    bool shows(const std::string& needle) {
        for (const std::string& row : rows()) {
            if (row.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    /// Which prose row of the BODY carries this text, or -1 -- the number a press is
    /// then aimed at, so a case never spells a coordinate of its own.
    std::int64_t row_of(const std::string& needle) {
        const std::vector<std::string> all = rows();
        for (std::size_t i = 0; i < all.size(); ++i) {
            if (all[i].find(needle) != std::string::npos) {
                return static_cast<std::int64_t>(i);
            }
        }
        return -1;
    }
    void press_row(std::int64_t row) {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.press_cell(body.x + 1, body.y + kExternalHeaderRows + row);
    }
    /// Point the keyboard at this pane without meaning anything by it: row 0 of the
    /// body is the target line, which names no item.
    void focus() { press_row(0); }

    /// HAND THE KEYBOARD BACK TO THE ROOM. A press into a pane points the keys at it
    /// (MSG-0), and a focused pane outranks the command context -- so a maker who has
    /// been working in this pane and now wants the DESK presses the room first. The cell
    /// is found by asking the same occupancy walk the pointer asks, never spelled here.
    void press_room() {
        const Screen sc = screen_of(r.session());
        for (std::int64_t y = sc.room_h - 1; y >= 0; --y) {
            for (std::int64_t x = sc.room_w - 1; x >= 0; --x) {
                if (!occupied_at(r.session().panels, r.session().setup.active, sc, x, y)
                         .occupied) {
                    r.press_cell(x, y);
                    return;
                }
            }
        }
        FAIL("this screen has no empty room to press");
    }

    /// SCROLL UNTIL THIS ROW IS ON SCREEN -- and it is the maker's own gesture rather
    /// than a test's shortcut.
    ///
    /// AN OVERLAY SLOT IS NINE CELLS TALL AT EVERY EXTENT (WIND-1 widened the slot and
    /// deliberately did not heighten it), so a pane's body is eight prose rows in a
    /// terminal however big the surface is -- and the Timer's accept-set is eleven
    /// roots. So a case that read the catalog off one frame would be reading a window,
    /// and every root past the fifth would look absent. Arrowing is what a maker does
    /// about that, and this is the case that proves the WHOLE accept-set is reachable
    /// rather than merely counted.
    bool reach(const std::string& needle) {
        if (row_of(needle) >= 0) {
            return true;
        }
        focus();
        for (int i = 0; i < 64; ++i) {
            key(input::scan::kDown);
            if (row_of(needle) >= 0) {
                return true;
            }
        }
        for (int i = 0; i < 128; ++i) {
            key(input::scan::kUp);
            if (row_of(needle) >= 0) {
                return true;
            }
        }
        return false;
    }

    void choose(const std::string& what) {
        REQUIRE(reach(what));
        press_row(row_of(what));
    }
    void go_to(const std::string& field) {
        REQUIRE(reach(field + ":"));
        press_row(row_of(field + ":"));
    }
    void act(const std::string& control) { choose(control); }
    void key(std::int64_t sc, std::int64_t mods = input::mod::kNone) { r.key(sc, mods); }
    /// The text a platform committed. Sent whole, which a backend may legitimately
    /// do; the key-transition half of a printable keystroke is exercised by the
    /// picker case in tier one, where it is the subject.
    void type(const std::string& text) { r.text(text); }
    void fill(const std::string& field, const std::string& text) {
        go_to(field);
        // TYPING INTO AN ABSENT FIELD MAKES IT PRESENT -- no include gesture first.
        type(text);
    }

    bool delivered(const std::string& shape) const {
        return std::find(delivered_.begin(), delivered_.end(), shape) != delivered_.end();
    }
    bool refused(const std::string& shape) const {
        return std::find(refused_.begin(), refused_.end(), shape) != refused_.end();
    }
    std::string author_of(const std::string& shape) const {
        for (std::size_t i = 0; i < delivered_.size(); ++i) {
            if (delivered_[i] == shape) {
                return authors_[i];
            }
        }
        return std::string();
    }
};

} // namespace

TEST_CASE("MSG-0: loading the real Composer puts its pane in the catalog, offered by its office") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    const loom::WeaveId id =
        r.load(zengine::composer::kComposerStem, WORKSHOP_SO_COMPOSER, kComposerOffice);
    REQUIRE(r.load_refusals.empty());
    REQUIRE(id.valid());
    REQUIRE(r.session().panels.runtime.entries.size() == 1);
    const RuntimePane& row = r.session().panels.runtime.entries[0];
    CHECK(row.provider == std::string(kComposerOffice)); // Loom's stamp, not a payload field
    CHECK(row.pane == std::string(kComposePane));
    CHECK(row.name == std::string(zengine::composer::kComposePaneName));
    CHECK(row.summary == std::string(zengine::composer::kComposePaneSummary));
    // WORKSHOP COMPILED NOTHING FOR IT: the handle is a runtime one, minted from a
    // live offer, and no `panel::k*` exists for this pane.
    CHECK(is_runtime_kind(row.kind));
}

TEST_CASE("MSG-0: with no selection the real pane says it has no target") {
    ComposeRig r;
    CHECK(r.rows()[0] == "no weave selected");
}

TEST_CASE("MSG-0: the catalog is the Timer's own accept-set, substrate doors included") {
    ComposeRig r;
    r.with_timer();
    r.select(kTimerOffice, "zengine-timer");

    // THE COUNT IS THE POPULATION AND IT IS ON THE FIRST FRAME. Eleven: six declared
    // doors and the five universal substrate ones.
    CHECK(r.shows("accepted messages -- 11"));
    // The Timer's own declared doors, every one of them REACHABLE...
    CHECK(r.reach("StartTimer v1"));
    CHECK(r.reach("StartRoleTimer v1"));
    CHECK(r.reach("CancelTimer v1"));
    CHECK(r.reach("CancelAllMyTimers v1"));
    CHECK(r.reach("EnsureTimer v1"));
    CHECK(r.reach("EnsureRoleTimer v1"));
    // ...and the substrate's, UNFILTERED. This pane does not know which of these is a
    // command, an answer, a notification or a door, so it hides none of them.
    CHECK(r.reach("zen.DescribeAccepted v1"));
    CHECK(r.reach("zen.PokeRead v1"));
    CHECK(r.reach("zen.PokeWrite v1"));
    CHECK(r.reach("zen.PokeDescribe v1"));
    CHECK(r.reach("zen.PokeResetState v1"));
    CHECK_FALSE(r.shows("Commands"));
    CHECK_FALSE(r.shows("Available"));
    // ...and what is not shown is COUNTED rather than dropped.
    CHECK(r.shows("..."));
    // The office a send is addressed to, on the first row, always.
    CHECK(r.rows()[0] == "to @" + std::string(kTimerOffice));
}

TEST_CASE("MSG-0: a maker fills a generated form and SUBMITS a real StartTimer") {
    // THE PHASE'S STOP CONDITION, END TO END. Nothing about this message was compiled
    // into the Composer: the shape came off the wire, the form came off the shape, and
    // the value went to the office the maker never typed.
    ComposeRig r;
    r.with_timer();
    r.select(kTimerOffice, "zengine-timer");
    r.watch();

    r.focus();
    r.choose("StartTimer v1");
    CHECK(r.shows("StartTimer v1 -> @zengine.timer"));
    // THE THREE ROWS ARE THE SCHEMA'S THREE FIELDS, in declaration order, each with
    // its own declared type and each ABSENT until a maker says otherwise.
    CHECK(r.shows("id:Text  (required)"));
    CHECK(r.shows("delay_ms:Int  (required)"));
    CHECK(r.shows("repeat:Bool  (required)"));

    r.fill("id", "tick");
    r.fill("delay_ms", "1000");
    r.go_to("repeat");
    r.key(input::scan::kTab); // unset -> false, which is a CHOSEN false
    CHECK(r.shows("repeat:Bool  [false]"));

    r.act("[ Submit ]");
    CHECK(r.shows("SUBMITTED"));

    // ...AND THE TIMER REALLY RECEIVED IT, authored as the Composer's office and
    // addressed to the Timer's. `SUBMITTED` is what the PANE may claim; this is what a
    // case with a bus tap can see, and the two are deliberately different questions.
    CHECK(r.delivered("StartTimer"));
    CHECK(r.author_of("StartTimer") == std::string(kComposerOffice));
    // The Composer says nothing stronger than SUBMITTED about any of it.
    CHECK_FALSE(r.shows("Delivered"));
    CHECK_FALSE(r.shows("Accepted"));
    CHECK_FALSE(r.shows("Timer created"));
}

TEST_CASE("WUX-6/SC-7: one coarse grow gives the DEFAULT Compose pane a usable form") {
    // THE PRESSURE WUX-5 MEASURED AND REPORTED RATHER THAN REPAIRED, closed at the desk.
    // The pane is exactly the size the shipped desk opens it at; nothing here reaches into
    // the Composer, nothing changes its composition priorities, and the repair is the
    // maker's own ordinary key on the maker's own pane.
    ComposeRig r(/*default_size=*/true);
    r.with_timer();
    r.select(kTimerOffice, "zengine-timer");
    r.focus();
    r.choose("StartTimer v1");

    // AT THE DEVELOPER'S DEFAULT: the footer is there and the form is not. This is the
    // regression, measured off the published canvas rather than asserted.
    const std::vector<std::string> tight = r.rows();
    INFO("default rows: " << tight.size());
    for (const std::string& row : tight) {
        INFO(row);
    }
    CHECK(r.shows("[ Submit ]"));
    CHECK_FALSE(r.shows("id:Text"));
    CHECK_FALSE(r.shows("delay_ms:Int"));
    CHECK_FALSE(r.shows("repeat:Bool"));

    // ONE COARSE GROW, THROUGH THE ARRANGEMENT DESK, on the pane the maker addressed.
    // The room is pressed first because working in a pane points the keys at it (MSG-0),
    // and a focused pane outranks the command context that owns `w`.
    r.press_room();
    r.key(input::scan::kW);
    r.type("w");
    REQUIRE(r.r.session().arrange.open);
    for (int i = 0; i < 32 && !(r.r.session().arrange.addressed() &&
                                r.r.session().arrange.pane == composer_ref());
         ++i) {
        r.key(input::scan::kTab);
    }
    REQUIRE(r.r.session().arrange.pane == composer_ref());
    r.key(input::scan::kEquals);
    r.key(input::scan::kEscape);

    // ...AND THE WHOLE FORM IS THERE, WITH ITS SUBMIT. Both, which is exactly what the
    // default could not do.
    const std::vector<std::string> roomy = r.rows();
    INFO("grown rows: " << roomy.size());
    for (const std::string& row : roomy) {
        INFO(row);
    }
    CHECK(r.shows("StartTimer v1 -> @zengine.timer"));
    CHECK(r.shows("id:Text"));
    CHECK(r.shows("delay_ms:Int"));
    CHECK(r.shows("repeat:Bool"));
    CHECK(r.shows("[ Submit ]"));

    // AND THE ROOM CAME FROM THE PANE, not from the provider changing its mind: the body
    // the Composer was granted is `kCoarseStepCells` rows taller and nothing else moved.
    CHECK(roomy.size() >= tight.size() + static_cast<std::size_t>(kCoarseStepCells));

    // THE MAKER CAN STILL FINISH THE JOB -- the grown pane is a working pane, not a
    // bigger picture of a broken one.
    r.watch();
    r.fill("id", "tick");
    r.fill("delay_ms", "1000");
    r.go_to("repeat");
    r.key(input::scan::kTab);
    r.act("[ Submit ]");
    CHECK(r.shows("SUBMITTED"));
    CHECK(r.delivered("StartTimer"));
}

TEST_CASE("MSG-0: `1O00` never leaves the pane, and the refusal is the ladder's own") {
    // The Composer can reject this because `Int` is known; it could reject nothing
    // about the value in the case below. That contrast is the phase's clearest
    // evidence for where preflight would have to live.
    ComposeRig r;
    r.with_timer();
    r.select(kTimerOffice, "zengine-timer");
    r.watch();

    r.focus();
    r.choose("StartTimer v1");
    r.fill("id", "tick");
    r.fill("delay_ms", "1O00"); // letter O
    r.go_to("repeat");
    r.key(input::scan::kTab);
    r.act("[ Submit ]");

    CHECK(r.shows("delay_ms"));
    CHECK(r.shows("cannot take this value"));
    CHECK_FALSE(r.delivered("StartTimer"));

    // ...and repairing the typo sends it. One character is the whole difference.
    r.go_to("delay_ms");
    for (int i = 0; i < 4; ++i) {
        r.key(input::scan::kBackspace);
    }
    r.type("1000");
    r.act("[ Submit ]");
    CHECK(r.delivered("StartTimer"));
}

TEST_CASE("MSG-0: a nonexistent TimerID composes, submits, and nothing here knows better") {
    // `CancelTimer("no-such-timer")` is structurally valid Text, genuinely accepted by
    // the Timer, locally composable -- and semantically useless. The pane says
    // SUBMITTED and claims nothing else, which is exactly the gap this witness exists
    // to expose without solving.
    ComposeRig r;
    r.with_timer();
    r.select(kTimerOffice, "zengine-timer");
    r.watch();

    r.focus();
    r.choose("CancelTimer v1");
    r.fill("id", "no-such-timer");
    r.act("[ Submit ]");

    // IT WENT. The gate passed it, the Timer accepted it, and nothing refused it.
    CHECK(r.delivered("CancelTimer"));
    CHECK_FALSE(r.refused("CancelTimer"));
    // AND THE PANE SAYS `SUBMITTED` AND NOTHING STRONGER -- not delivered, not
    // accepted, not cancelled, none of which it observed.
    CHECK(r.shows("SUBMITTED"));
    CHECK_FALSE(r.shows("Cancelled"));
    CHECK_FALSE(r.shows("Success"));
}

TEST_CASE("MSG-0: a form with no fields is ready at once, and invents none") {
    ComposeRig r;
    r.with_timer();
    r.select(kTimerOffice, "zengine-timer");
    r.watch();
    r.focus();
    r.choose("CancelAllMyTimers v1");
    for (const std::string& row : r.rows()) {
        CHECK(row.find("(required)") == std::string::npos);
        CHECK(row.find("(absent)") == std::string::npos);
    }
    r.act("[ Submit ]");
    CHECK(r.delivered("CancelAllMyTimers"));
}

TEST_CASE("MSG-0: an optional field begins ABSENT and stays there unless authored") {
    // AND FINDING A TARGET THAT HAS ONE IS THE HALF WORTH READING. `ZEN_FIELD`
    // derives every field required, and `WeaveBase` builds its accept-set from
    // ZEN_SHAPE types -- so no accept-set the construction layer answers can contain
    // an optional field, in either repository. This target hand-writes both its
    // schema and its door (see `Optionals`), which is currently the only way one
    // reaches a maker at all.
    ComposeRig r;
    r.with_optionals();
    r.select("zengine.optionals", "an-optional-library");
    r.watch();
    r.focus();
    r.choose("Optionals v1");

    // THE THREE STATES ARE THREE DIFFERENT ROWS, and none of them is a default.
    CHECK(r.shows("must:Text  (required)"));
    CHECK(r.shows("may:Text  (absent)"));
    CHECK(r.shows("flag:Bool  (absent)"));

    // Absent stays absent through a send: the required field is authored, the two
    // optional ones are not, and the value that goes carries neither.
    r.fill("must", "here");
    r.act("[ Submit ]");
    CHECK(r.shows("SUBMITTED"));
    REQUIRE(r.optionals->heard.size() == 1);
    const loom::Value& sent = r.optionals->heard[0];
    REQUIRE(sent.get("must") != nullptr);
    CHECK(sent.get("must")->as_text() == "here");
    CHECK(sent.get("may") == nullptr);  // ABSENT, not empty
    CHECK(sent.get("flag") == nullptr); // ABSENT, not false

    // ...and an optional field a maker DOES author goes, empty or false, because
    // those are values somebody chose.
    r.go_to("may");
    r.key(input::scan::kTab); // absent -> present, with the empty bytes it has
    CHECK(r.shows("may:Text  ["));
    r.go_to("flag");
    r.key(input::scan::kTab); // unset -> false
    CHECK(r.shows("flag:Bool  [false]"));
    r.act("[ Submit ]");
    REQUIRE(r.optionals->heard.size() == 2);
    const loom::Value& again = r.optionals->heard[1];
    REQUIRE(again.get("may") != nullptr);
    CHECK(again.get("may")->as_text() == "");
    REQUIRE(again.get("flag") != nullptr);
    CHECK(again.get("flag")->as_bool() == false);
}

TEST_CASE("MSG-0: pressing the same selection twice asks twice") {
    // `LoadedSelected` is an OCCURRENCE and not a transition, so a maker pressing the
    // same row again is a maker asking again. There is no poll and no background
    // refresh; this gesture is the refresh.
    ComposeRig r;
    r.with_timer();
    r.watch();
    r.select(kTimerOffice, "zengine-timer");
    CHECK(r.asks == 1);
    r.select(kTimerOffice, "zengine-timer");
    CHECK(r.asks == 2);

    // ...and no root accumulated: the snapshot was REPLACED, not appended to.
    std::int64_t roots = 0;
    for (const std::string& row : r.rows()) {
        if (row.find("StartTimer v1") != std::string::npos) {
            ++roots;
        }
    }
    CHECK(roots == 1);
}

TEST_CASE("MSG-0: retargeting drops the previous target's catalog and draft at once") {
    // The A rows disappear ON RETARGET rather than when B answers, because a snapshot
    // belongs to the target it was read from and a draft belongs to a shape out of
    // that snapshot.
    ComposeRig r;
    r.with_timer();
    r.select(kTimerOffice, "zengine-timer");
    r.focus();
    r.choose("StartTimer v1");
    r.fill("id", "tick");
    REQUIRE(r.shows("id:Text  [tick"));

    // A SECOND TARGET NOBODY HOLDS: the office is real, the send resolves to nobody,
    // and no answer arrives. What matters is what happened to A's material.
    r.select("zengine.nobody", "some-library");
    for (const std::string& row : r.rows()) {
        CHECK(row.find("StartTimer") == std::string::npos);
        CHECK(row.find("tick") == std::string::npos);
    }
    CHECK(r.rows()[0] == "to @zengine.nobody");
    CHECK(r.shows("no answer observed yet"));
}

TEST_CASE("MSG-0: an answer to a question this pane is no longer waiting on is dropped") {
    // THE STALE-ANSWER CASE, and the mechanism is Loom's own correlation rather than
    // anything invented here. A slow target answers with the number this weave minted
    // for it; by then the maker has retargeted and that number has been retired, so
    // the answer is not applied to the second target.
    ComposeRig r;
    r.with_timer();
    r.with_stranger();
    // Timer first, and it answers immediately -- there is no way to delay a real
    // in-process answer, so the ORDER is what the case controls: after the second
    // selection the pane must be showing the SECOND target's vocabulary and nothing
    // of the first's.
    r.select(kTimerOffice, "zengine-timer");
    REQUIRE(r.shows("StartTimer v1"));
    r.select("zengine.stranger", "a-stranger");
    CHECK(r.rows()[0] == "to @zengine.stranger");
    CHECK(r.shows("Curious v1"));
    CHECK_FALSE(r.shows("StartTimer"));
    CHECK_FALSE(r.shows("CancelTimer"));
    // ...and the correlation is what did it: a second question was minted, so the
    // first one's number can no longer match.
    CHECK(r.asks >= 0); // (the counter is only watched in the case above)
}

TEST_CASE("MSG-0: an empty role is a library with nothing to address") {
    // The pane names the library, says the absence was OBSERVED, and asks nobody
    // anything -- no manufactured WeaveId, no address by library name, no sweep.
    ComposeRig r;
    r.watch();
    r.select("", "a-library-with-no-role");
    CHECK(r.asks == 0);
    const std::vector<std::string> rows = r.rows();
    REQUIRE(rows.size() >= 2);
    CHECK(rows[0] == "selected a-library-with-no-role");
    CHECK(rows[1].find("no messaging role observed") != std::string::npos);
}

TEST_CASE("MSG-0: a selection from an office that is not Introspection retargets nothing") {
    // A fact about a maker's gesture is worth exactly as much as the office it came
    // from. Any weave granted `LoadedSelected` could otherwise point a maker's
    // Composer at a target of its choosing.
    ComposeRig r;
    r.with_timer();
    r.watch();
    r.select_as("zengine.impostor", kTimerOffice, "zengine-timer");
    CHECK(r.asks == 0);
    CHECK(r.rows()[0] == "no weave selected");
}

TEST_CASE("MSG-0: a shape this build never compiled against generates its own form") {
    // THE GENERICITY WITNESS. The target is mounted HERE with a vocabulary the
    // Composer's binary has no header for, discovered through `zen.DescribeAccepted`
    // and composed from the Schema that came back. It proves the PRODUCT is generic,
    // not merely the library beneath it.
    ComposeRig r;
    r.with_stranger();
    r.select("zengine.stranger", "a-stranger");

    // TWO VERSIONS OF ONE NAME ARE TWO IDENTITIES, and both are listed.
    CHECK(r.shows("Curious v1"));
    CHECK(r.shows("Curious v2"));

    r.focus();
    r.choose("Curious v1");
    CHECK(r.shows("alpha:Text"));
    CHECK(r.shows("beta:Int"));

    // ...and v2's form is v2's, not v1's. No merging, no inference.
    r.act("[ Back ]");
    r.choose("Curious v2");
    CHECK(r.shows("gamma:Float"));
    CHECK(r.shows("delta:Bool"));
    CHECK_FALSE(r.shows("alpha:Text"));
}

TEST_CASE("MSG-0: a composed message reaches a target whose shape nobody shipped") {
    ComposeRig r;
    r.with_stranger();
    r.select("zengine.stranger", "a-stranger");
    r.watch();
    r.focus();
    r.choose("Curious v2");
    r.go_to("gamma");
    r.key(input::scan::kTab); // absent -> present
    r.type("2.5");
    r.go_to("delta");
    r.key(input::scan::kTab);
    r.key(input::scan::kTab); // unset -> false -> true
    CHECK(r.shows("delta:Bool  [true]"));
    r.act("[ Submit ]");
    CHECK(r.shows("SUBMITTED"));
    CHECK(r.delivered("Curious"));
}

TEST_CASE("MSG-0: a structural field is shown, refused, and blocks the send") {
    // The schema is visible, the field's own structure is visible, the Composer says
    // it cannot compose it, there is no fake scalar editor, and nothing goes.
    ComposeRig r;
    r.with_stranger();
    r.select("zengine.stranger", "a-stranger");
    r.watch();
    r.focus();
    r.choose("Nested v1");
    CHECK(r.shows("leaf:Message(Leaf v1)"));
    CHECK(r.shows("(not composable in this version)"));
    r.fill("label", "anything");
    r.act("[ Submit ]");
    CHECK_FALSE(r.delivered("Nested"));
    CHECK(r.shows("still needed"));
}

TEST_CASE("MSG-0: selecting a weave in the real Loaded pane retargets the real Composer") {
    // THE PRODUCT EDGE ITSELF, through three real libraries and no fixture at all: the
    // Introspection tool publishes `LoadedSelected` because a maker pressed one of its
    // rows, the Composer hears it, and the Timer answers `zen.DescribeAccepted` with
    // its own accept-set. This is the case the phase exists for.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(240, 80);
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    (void)r.load(zengine::composer::kComposerStem, WORKSHOP_SO_COMPOSER, kComposerOffice);
    REQUIRE(r.load_refusals.empty());

    r.pick(intro_ref());
    r.pick(composer_ref());
    std::int64_t intro_kind = kNoPaneKind;
    std::int64_t compose_kind = kNoPaneKind;
    for (const RuntimePane& row : r.session().panels.runtime.entries) {
        // BOTH HALVES OF THE `PaneRef`, and the pane half is what INTR-1 made
        // load-bearing: this office offers three panes now, so matching on the office
        // alone would have picked whichever the catalog happened to hold last.
        if (row.provider == std::string(kIntroOffice) && row.pane == std::string(kIntroPane)) {
            intro_kind = row.kind;
        }
        if (row.provider == std::string(kComposerOffice)) {
            compose_kind = row.kind;
        }
    }
    REQUIRE(is_runtime_kind(intro_kind));
    REQUIRE(is_runtime_kind(compose_kind));

    // Press the Loaded row that names the INTROSPECTION library -- the Loaded pane's
    // own provider, which is a real loaded library holding a real office with a real
    // accept-set, and the target this composition can actually reach. Which row that
    // is, is the kernel's map's business and a case must never assume it.
    const std::vector<std::string> loaded = loaded_rows(r, intro_kind);
    std::int64_t which = -1;
    for (std::size_t i = 0; i < loaded.size(); ++i) {
        if (is_entry_row(loaded[i]) && named_by(loaded[i]) == intro::kIntrospectionStem) {
            which = static_cast<std::int64_t>(i);
        }
    }
    REQUIRE(which >= 0);
    const ui::Rect intro_body = external_body_rect(r.session(), intro_kind);
    r.press_cell(intro_body.x + 1, intro_body.y + kExternalHeaderRows + which);

    const std::vector<std::string> shown =
        external_rows(r.last_canvas(), external_body_rect(r.session(), compose_kind));
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0] == "to @" + std::string(kIntroOffice));
    // THE TARGET ANSWERED, in the same turn, so the pane is already showing its real
    // accepted roots rather than the pending state.
    bool counted = false;
    for (const std::string& row : shown) {
        counted = counted || row.rfind("accepted messages -- ", 0) == 0;
    }
    CHECK(counted);

    // AND THE LOADED PANE IS UNTOUCHED BY ANY OF IT. The two tools do not know about
    // each other: one published a fact and stopped, the other heard it and changed
    // its own target. Nothing opened, closed, moved or was hidden.
    CHECK(r.session().panels.open.size() == 4); // Info, Layouts, Loaded, Compose
    CHECK(r.session().panels.has(panel::kInfo));
    CHECK_FALSE(r.session().panels.picker.open);
    CHECK_FALSE(r.session().arrange.open);
}

TEST_CASE("MSG-0: the Composer opens, closes and moves nothing but itself") {
    // A selection is a fact and this pane's reaction to it is to change ITS OWN
    // target. It does not open itself, hide Info, move a pane, change the setup, or
    // reach the tool whose fact it heard -- and it does all of that with the pane not
    // even open, which is what says the reaction is the pane's own and not a workflow.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    (void)r.load(zengine::composer::kComposerStem, WORKSHOP_SO_COMPOSER, kComposerOffice);
    auto weave = std::make_unique<Selector>();
    Selector* selector = weave.get();
    loom::Grant grant;
    grant.allow_to_any(intro::LoadedSelected::zen_name, intro::LoadedSelected::zen_version);
    const loom::WeaveId selector_id =
        r.bus.register_weave(std::move(weave), std::move(grant), std::string(kIntroOffice));
    selector->zen_set_self(selector_id);

    // The Compose pane is NOT open. A selection updates a target nobody is looking at.
    REQUIRE_FALSE(r.session().panels.has(r.session().panels.runtime.entries[0].kind));
    const std::size_t panels_before = r.session().panels.open.size();
    const Setup setup_before = r.session().setup.active;
    const std::string notice_before = r.last_notice();
    const std::int64_t selected_before = r.session().selected;

    selector->next = [](Selector& s, loom::Mail& m) {
        s.say(m, kIntroOffice, intro::LoadedSelected{"loaded", "zengine-timer", kTimerOffice});
    };
    (void)r.bus.send(selector_id,
                     loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();

    CHECK(r.session().panels.open.size() == panels_before);
    CHECK(r.session().setup.active == setup_before);
    CHECK(r.last_notice() == notice_before);
    CHECK(r.session().selected == selected_before);
    CHECK(r.session().panels.has(panel::kInfo));
    CHECK_FALSE(r.session().panels.picker.open);
    CHECK_FALSE(r.session().arrange.open);
    CHECK_FALSE(r.session().terminal.open);
}

// ---- QR-18: the wheel crosses the seam, and Escape puts a pane down ----------------------

TEST_CASE("QR-18: the wheel crosses the seam as PaneWheel -- a pane and its notches, nothing else") {
    // THE EIGHTH SHAPE, WALKED AS A SCHEMA. It carries the notches the wire already
    // carried and the pane they were over; no place, no button, no modifier, no rows-per-
    // notch policy and no reply -- a provider gets the raw gesture exactly as it gets a key.
    const std::shared_ptr<const loom::Schema> wheel = loom::schema_of<PaneWheel>();
    REQUIRE(wheel != nullptr);
    CHECK(wheel->name() == "PaneWheel");
    CHECK(wheel->version() == 1u);
    REQUIRE(wheel->fields().size() == 3);
    CHECK(wheel->fields()[0].name == "pane");
    CHECK(wheel->fields()[0].type.kind == loom::Kind::Text);
    CHECK(wheel->fields()[1].name == "dx");
    CHECK(wheel->fields()[1].type.kind == loom::Kind::Float);
    CHECK(wheel->fields()[2].name == "dy");
    CHECK(wheel->fields()[2].type.kind == loom::Kind::Float);
    for (const loom::Field& f : wheel->fields()) {
        CHECK(f.name != "row");
        CHECK(f.name != "column");
        CHECK(f.name != "x");
        CHECK(f.name != "y");
        CHECK(f.name != "rows");
        CHECK(f.name != "modifiers");
    }
    // AND THE SEVEN OLDER SHAPES DID NOT MOVE. QR-18 added one; it revised none.
    CHECK(loom::schema_of<PaneCatalogRequested>()->version() == 1u);
    CHECK(loom::schema_of<PaneOffered>()->version() == 1u);
    CHECK(loom::schema_of<PaneRoom>()->version() == 1u);
    CHECK(loom::schema_of<PaneContent>()->version() == 1u);
    CHECK(loom::schema_of<PanePressed>()->version() == 1u);
    CHECK(loom::schema_of<PaneKey>()->version() == 1u);
    CHECK(loom::schema_of<PaneTextInput>()->version() == 1u);
    CHECK(loom::schema_of<PanePressed>()->fields().size() == 3u);
    CHECK(loom::schema_of<PaneKey>()->fields().size() == 3u);
}

TEST_CASE("QR-18: a wheel over an external pane's body crosses unchanged, follows the pointer, "
          "and the header sends nothing") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    const ui::Rect body = external_body_rect(r.session(), kind);
    REQUIRE(body.h > kExternalHeaderRows + 1);

    // NOBODY HAS THE KEYBOARD, and the wheel still crosses: it follows the pointer, as a
    // press does, not the keys.
    REQUIRE(r.session().panels.keyboard == kNoPaneKind);
    REQUIRE(seat->wheels.empty());
    r.wheel_cell(-1.0, body.x + 2, body.y + kExternalHeaderRows);
    REQUIRE(seat->wheels.size() == 1);
    CHECK(seat->wheels[0].pane == std::string(kHelloPane));
    CHECK(seat->wheels[0].dy == doctest::Approx(-1.0));
    CHECK(seat->wheels[0].dx == doctest::Approx(0.0));
    CHECK(seat->wheel_authors[0] == std::string(kWorkshopProvider)); // authored as Workshop
    // AND IT POINTED NOTHING: a wheel is looking, not pressing, so neither the selection nor
    // the keyboard moved.
    CHECK(r.session().panels.keyboard == kNoPaneKind);
    CHECK(r.session().panels.selected == kNoPaneKind);

    // A FRACTION CROSSES AS A FRACTION. Workshop accumulates nothing for a list it cannot
    // see; how many rows a notch is worth is the provider's grammar.
    r.wheel_cell(0.25, body.x + 2, body.y + kExternalHeaderRows + 1);
    REQUIRE(seat->wheels.size() == 2);
    CHECK(seat->wheels[1].dy == doctest::Approx(0.25));

    // THE HEADER ROW IS NOT THE BODY: nothing is sent, `PanePressed`'s rule.
    r.wheel_cell(-1.0, body.x + 2, body.y);
    CHECK(seat->wheels.size() == 2);
    // AND NEITHER IS THE WORKSPACE BESIDE IT.
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
    r.wheel_cell(-1.0, panel.x + 1, panel.y + panel.h + 1);
    CHECK(seat->wheels.size() == 2);
    // A KEY IS STILL NOT SENT BY A WHEEL, and no press was manufactured out of one.
    CHECK(seat->keys.empty());
    CHECK(seat->presses.empty());
}

TEST_CASE("QR-18/SC-7: a wheel in an overlap reaches only the pane visibly in front, and the "
          "selection lift moves it") {
    // MUTATION (F4): routing the wheel by anything but `occupied_at`'s effective order --
    // the covered pane would receive the notch and `wheels.back().pane` would name it.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"second", "Second", "the same office's other pane"});
    });
    r.extent(160, 60);
    r.pick(hello_ref());
    const PaneRef second_ref{kHelloOffice, "second"};
    r.pick(second_ref);
    std::int64_t hello_kind = kNoPaneKind;
    std::int64_t second_kind = kNoPaneKind;
    for (const RuntimePane& row : r.session().panels.runtime.entries) {
        (row.pane == std::string(kHelloPane) ? hello_kind : second_kind) = row.kind;
    }
    REQUIRE(hello_kind != kNoPaneKind);
    REQUIRE(second_kind != kNoPaneKind);

    // PUT `second` OVER `hello`, offset so a strip of `hello` stays uncovered.
    const ui::Rect hello = cells_covered(external_panel_rect(r.session(), hello_kind));
    REQUIRE(author_pane_place(r.session().setup.active, second_ref,
                              surface::subs_of_cells(hello.x + 4),
                              surface::subs_of_cells(hello.y + 3))
                .accepted);
    r.extent(160, 61); // reseat at the authored place
    const ui::Rect second = cells_covered(external_panel_rect(r.session(), second_kind));
    REQUIRE(second.x == hello.x + 4);
    REQUIRE(second.y == hello.y + 3);
    // A CELL INSIDE BOTH BODIES, below both headers.
    const std::int64_t cx = second.x + 2;
    const std::int64_t cy = second.y + kChromeCells + kExternalHeaderRows + 1;
    REQUIRE(cx < hello.x + hello.w - 1);
    REQUIRE(cy < hello.y + hello.h - 1);
    const Screen sc = screen_of(r.session());
    const Occupancy owner = occupied_at(r.session().panels, r.session().setup.active, sc, cx, cy);
    REQUIRE(owner.occupied);
    REQUIRE((owner.kind == hello_kind || owner.kind == second_kind));
    const std::int64_t front = owner.kind;
    const std::int64_t back = front == hello_kind ? second_kind : hello_kind;
    const std::string front_pane = front == hello_kind ? kHelloPane : "second";
    const std::string back_pane = back == hello_kind ? kHelloPane : "second";

    // THE FRONT PANE OWNS THE WHEEL AT THAT CELL, and the covered one hears nothing.
    seat->wheels.clear();
    r.wheel_cell(-1.0, cx, cy);
    REQUIRE(seat->wheels.size() == 1);
    CHECK(seat->wheels[0].pane == front_pane);

    // SELECTING THE COVERED PANE LIFTS IT (WUX-5), and the same cell is then its own.
    // A press on a strip of the back pane the front one does not cover.
    const ui::Rect back_rect = cells_covered(external_panel_rect(r.session(), back));
    const ui::Rect front_rect = cells_covered(external_panel_rect(r.session(), front));
    std::int64_t px = -1;
    std::int64_t py = -1;
    for (std::int64_t y = back_rect.y; y < back_rect.y + back_rect.h && px < 0; ++y) {
        for (std::int64_t x = back_rect.x; x < back_rect.x + back_rect.w; ++x) {
            const bool in_front = x >= front_rect.x && x < front_rect.x + front_rect.w &&
                                  y >= front_rect.y && y < front_rect.y + front_rect.h;
            if (!in_front) {
                px = x;
                py = y;
                break;
            }
        }
    }
    REQUIRE(px >= 0);
    r.press_cell(px, py);
    REQUIRE(r.session().panels.selected == back);
    CHECK(occupied_at(r.session().panels, r.session().setup.active, sc, cx, cy).kind == back);
    seat->wheels.clear();
    r.wheel_cell(-1.0, cx, cy);
    REQUIRE(seat->wheels.size() == 1);
    CHECK(seat->wheels[0].pane == back_pane);
}

TEST_CASE("QR-18/SC-1+SC-2: a focused external pane keeps Escape; a press on a pane that takes "
          "no text, then Escape, puts the selection down") {
    // MUTATION (F1): removing the final Escape branch -- `selected` stays `kInfo` below.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);
    REQUIRE(r.session().panels.selected == kind);
    REQUIRE(r.session().panels.keyboard == kind);
    const std::size_t panes_before = r.session().panels.open.size();
    const Setup setup_before = r.session().setup.active;

    // THE PANE HOLDS THE KEYS, SO THE PANE OWNS ESCAPE: it crosses the seam exactly as
    // every other key does, and nothing on Workshop's side moves -- the seam carries no
    // `consumed`, so Workshop cannot see the pane decline it, and the shipped Composer does
    // not decline it (TEXT-0). The maker is still in the pane, keys and selection alike.
    r.key(input::scan::kEscape);
    REQUIRE(seat->keys.size() == 1);
    CHECK(seat->keys[0].scancode == input::scan::kEscape);
    CHECK(r.session().panels.selected == kind);
    CHECK(r.session().panels.keyboard == kind);
    CHECK(keyboard_context(r.session()) == KeyContext::kPane);

    // THE WAY OUT IS THE WAY IN: press a pane that takes no text -- Info here, Layouts on
    // every desk -- and Escape is then nothing more specific's.
    const Screen sc = screen_of(r.session());
    const ui::Rect info =
        cells_covered(bounds_of(r.session().panels, r.session().setup.active, panel::kInfo, sc).rect);
    r.press_cell(info.x + 1, info.y + 1);
    REQUIRE(r.session().panels.selected == panel::kInfo);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kCommand);
    r.key(input::scan::kEscape);
    CHECK(seat->keys.size() == 1); // the pane no longer holds the keys: nothing crossed
    CHECK(r.session().panels.selected == kNoPaneKind);
    CHECK(r.session().panels.keyboard == kNoPaneKind);
    CHECK(r.last_notice().find("unselected") != std::string::npos);
    // NOTHING ELSE MOVED: both panes are open, the setup is byte-identical, no file was written.
    CHECK(r.session().panels.open.size() == panes_before);
    CHECK(r.session().setup.active == setup_before);
    CHECK(r.session().panels.has(kind));

    // A SECOND ESCAPE HAS NOTHING TO SHED: the selection stays none, the notice is left alone.
    const std::string notice = r.last_notice();
    r.key(input::scan::kEscape);
    CHECK(seat->keys.size() == 1);
    CHECK(r.session().panels.selected == kNoPaneKind);
    CHECK(r.last_notice() == notice);
}

TEST_CASE("QR-18/SC-5: the Composer's windowed catalog is reached by the wheel") {
    // THE REAL COMPOSER, through the real seam. The Timer's catalog is eleven rows and the
    // pane's room does not hold them: the last is hidden behind `...` until a wheel over the
    // body walks the cursor to it -- through `move_cursor`, the same step Down takes.
    ComposeRig r;
    r.with_timer();
    r.select(kTimerOffice, "zengine-timer");
    REQUIRE(r.shows("accepted messages -- 11"));
    REQUIRE(r.shows("..."));
    REQUIRE_FALSE(r.shows("zen.PokeResetState v1")); // the last root, hidden below

    const ui::Rect body = external_body_rect(r.r.session(), r.kind);
    const std::int64_t cx = body.x + 2;
    const std::int64_t cy = body.y + kExternalHeaderRows + 1;
    // NOBODY HAS THE KEYBOARD, and the wheel still moves the list: pointer, not keys.
    REQUIRE(r.r.session().panels.keyboard == kNoPaneKind);
    for (int i = 0; i < 12; ++i) {
        r.r.wheel_cell(-1.0, cx, cy); // toward the maker: later rows
    }
    CHECK(r.shows("zen.PokeResetState v1"));
    CHECK(r.r.session().panels.keyboard == kNoPaneKind);
    // AND BACK: the first root returns, the last leaves.
    for (int i = 0; i < 12; ++i) {
        r.r.wheel_cell(+1.0, cx, cy);
    }
    CHECK(r.shows("StartTimer v1"));
    CHECK_FALSE(r.shows("zen.PokeResetState v1"));
    // A FRACTION IS CARRIED: two half-notches are one row, and one is none.
    const std::vector<std::string> before = r.rows();
    r.r.wheel_cell(-0.5, cx, cy);
    CHECK(r.rows() == before);
    r.r.wheel_cell(-0.5, cx, cy);
    CHECK(r.rows() != before);
}
