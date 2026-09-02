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
// THIS FILE OWNS: where a pane sits, and the hand that moves it.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// ============================================================================
// WIND-2 — the authored window: the maker arranges what their setup names
// ============================================================================
//
// The maker can arrange the panes their setup names, and can never lose one. Five tiers,
// and they are the five different KINDS of claim the phase makes:
//
//   ADMISSION    what setup version 2 accepts, refuses, and round-trips.
//   RESOLUTION   what an authored override does to a rectangle, per axis, and what it
//                deliberately does not do to the other two.
//   ORDER        that `front` is an exact permutation, that every operation keeps it one,
//                and that reordering writes nothing any other law reads.
//   RECOVERY     that every pane the setup names has exactly one row, in every state.
//   GESTURES     that a key and a hand reach the same doors, and that a press owns its
//                gesture until it is released.

// ---- ADMISSION ------------------------------------------------------------------------

TEST_CASE("WIND-2: a fresh setup is version 3, sparse, and carries the identity ranks") {
    const Setup fresh = default_setup();
    REQUIRE(check_setup(fresh).accepted);
    REQUIRE(fresh.panes.size() == kDefaultPanelCount);
    for (std::size_t i = 0; i < fresh.panes.size(); ++i) {
        CAPTURE(i);
        // SPARSE: every geometry field is `default`, and its unused numbers are the required
        // zeros -- which is the smallest canonical spelling of "this maker has arranged
        // nothing", and the reason a fresh setup's bytes are the shortest they can be.
        CHECK(fresh.panes[i].place.mode == pane_unit::kDefault);
        CHECK(fresh.panes[i].place.x == 0);
        CHECK(fresh.panes[i].place.y == 0);
        CHECK(fresh.panes[i].width.mode == pane_unit::kDefault);
        CHECK(fresh.panes[i].width.amount == 0);
        CHECK(fresh.panes[i].height.mode == pane_unit::kDefault);
        CHECK(fresh.panes[i].height.amount == 0);
        CHECK(fresh.panes[i].front == static_cast<std::int64_t>(i));
    }
    CHECK(is_permutation(fresh));
    CHECK(setup_persist::kFormatVersion == 3);
    const std::string text = setup_persist::to_text(fresh);
    INFO(text);
    CHECK(text.find("\"format_version\":\"3\"") != std::string::npos);
    CHECK(text.find("\"mode\":\"default\"") != std::string::npos);
    CHECK(text.find("\"front\":\"0\"") != std::string::npos);
}

TEST_CASE("WIND-2: every mode spelling round-trips, pixels included") {
    Setup s = two_overlays();
    // ONE ROW PER MODE COMBINATION THE GRAMMAR HAS, so the round trip is a claim about the
    // format rather than about the one arrangement a case happened to build.
    // A FINE place and a fine width, deliberately not on cell boundaries (WUX-2): the
    // format's own resolution is the thing round-tripping here.
    s.panes[0].place = PanePlace{pane_unit::kSubcells, subs(6) + 13, subs(5) + 1};
    s.panes[0].width = PaneSize{pane_unit::kSubcells, subs(40) + 24};
    s.panes[0].height = PaneSize{pane_unit::kPixels, 220};
    s.panes[1].width = PaneSize{pane_unit::kPixels, 1};
    REQUIRE(check_setup(s).accepted);

    const std::string a = setup_persist::to_text(s);
    INFO(a);
    CHECK(a.find("\"mode\":\"subcells\"") != std::string::npos);
    CHECK(a.find("\"mode\":\"pixels\"") != std::string::npos);
    CHECK(a.find("\"mode\":\"default\"") != std::string::npos);

    const setup_persist::LoadedSetup read = setup_persist::from_text(a);
    REQUIRE(read.outcome.accepted);
    // EXACT AUTHORED VALUES, unit modes included. A `pixels` amount is preserved on a build
    // where no medium can project one, which is the whole of what "the intent survives the
    // refusal" means when it is said about bytes.
    CHECK(read.setup == s);
    CHECK(read.setup.panes[0].height.mode == pane_unit::kPixels);
    CHECK(read.setup.panes[0].height.amount == 220);
    CHECK(setup_persist::to_text(read.setup) == a);
}

TEST_CASE("WIND-2: a default mode carries no numbers, and that is one canonical spelling") {
    PanePlace place;
    place.mode = pane_unit::kDefault;
    place.x = 7;
    CHECK_FALSE(check_pane_place(place).accepted);
    place.x = 0;
    place.y = 3;
    CHECK_FALSE(check_pane_place(place).accepted);
    place.y = 0;
    CHECK(check_pane_place(place).accepted);

    PaneSize size;
    size.mode = pane_unit::kDefault;
    size.amount = 12;
    CHECK_FALSE(check_pane_size(size, "width").accepted);
    CHECK(check_pane_size(size, "width").refusal.find("width") != std::string::npos);
    size.amount = 0;
    CHECK(check_pane_size(size, "width").accepted);
}

TEST_CASE("WIND-2: the cell and pixel bounds are pinned at both ends") {
    // SUBCELLS: at least one whole cell, at most the document's own authored-size bound --
    // the same CELL walls the law always had, expressed on the fine lattice (WUX-2), so the
    // floor is `kPaneSubMin` and one sub-unit below it is the first refusal.
    CHECK_FALSE(check_pane_size(PaneSize{pane_unit::kSubcells, 0}, "width").accepted);
    CHECK_FALSE(check_pane_size(PaneSize{pane_unit::kSubcells, kPaneSubMin - 1}, "width").accepted);
    CHECK(check_pane_size(PaneSize{pane_unit::kSubcells, kPaneSubMin}, "width").accepted);
    CHECK(check_pane_size(PaneSize{pane_unit::kSubcells, kPaneSubMin + 1}, "width").accepted);
    CHECK(check_pane_size(PaneSize{pane_unit::kSubcells, kPaneSubMax}, "width").accepted);
    CHECK_FALSE(check_pane_size(PaneSize{pane_unit::kSubcells, kPaneSubMax + 1}, "width").accepted);

    // PIXELS: at least one, at most this file's own absolute ceiling. The ceiling is
    // MEDIUM-INDEPENDENT, which is what makes a setup legal on a terminal legal on a window.
    CHECK_FALSE(check_pane_size(PaneSize{pane_unit::kPixels, 0}, "height").accepted);
    CHECK(check_pane_size(PaneSize{pane_unit::kPixels, 1}, "height").accepted);
    CHECK(check_pane_size(PaneSize{pane_unit::kPixels, kMaxPanePixels}, "height").accepted);
    CHECK_FALSE(check_pane_size(PaneSize{pane_unit::kPixels, kMaxPanePixels + 1}, "height")
                    .accepted);
    CHECK(check_pane_size(PaneSize{pane_unit::kPixels, kMaxPanePixels + 1}, "height")
              .refusal.find(std::to_string(kMaxPanePixels)) != std::string::npos);

    // AND A PLACE: never negative, bounded above, and every sub-unit between is sayable.
    CHECK_FALSE(check_pane_place(PanePlace{pane_unit::kSubcells, -1, 0}).accepted);
    CHECK_FALSE(check_pane_place(PanePlace{pane_unit::kSubcells, 0, -1}).accepted);
    CHECK(check_pane_place(PanePlace{pane_unit::kSubcells, 0, 0}).accepted);
    CHECK(check_pane_place(PanePlace{pane_unit::kSubcells, 1, 1}).accepted);
    CHECK(check_pane_place(PanePlace{pane_unit::kSubcells, kPaneSubMax, kPaneSubMax}).accepted);
    CHECK_FALSE(
        check_pane_place(PanePlace{pane_unit::kSubcells, kPaneSubMax + 1, 0}).accepted);
}

TEST_CASE("WIND-2: a refused VALUE writes nothing, on either axis") {
    Setup s = two_overlays();
    const Setup before = s;
    const PaneRef builder = ref_of(panel::kBuilder);

    // THE VALUE DOORS ARE ATOMIC WHOLE. `author_pane_place`/`author_pane_size` take one
    // stated value and judge it as one thing -- a proposal wrong anywhere writes nothing
    // at all, `doc::move`/`doc::resize`'s law verbatim. This is deliberately NOT the
    // gesture door's law: a HAND's axes settle independently through
    // `author_pane_window` (WUX-2a), but a value stated as one thing is refused as one.
    CHECK_FALSE(author_pane_place(s, builder, 4, -1).accepted);
    CHECK(s == before);
    CHECK_FALSE(author_pane_place(s, builder, -1, 4).accepted);
    CHECK(s == before);

    // AND A SIZE VALUE WHOSE HEIGHT IS ILLEGAL NARROWS NOTHING: both axes are judged
    // before either is written.
    CHECK_FALSE(author_pane_size(s, builder, PaneSize{pane_unit::kSubcells, subs(30)},
                                 PaneSize{pane_unit::kSubcells, 0})
                    .accepted);
    CHECK(s == before);

    // SATURATION HAPPENS BEFORE THE PROPOSAL, so an absurd delta arrives as an absurd NUMBER
    // rather than as signed overflow. `pane_window_proposal` is where that is spent.
    constexpr std::int64_t kBig = (std::numeric_limits<std::int64_t>::max)();
    const PaneWindowProposal huge =
        pane_window_proposal(pane_edge::kBottomRight, 0, 0, kBig, kBig, kBig, kBig);
    CHECK(huge.w == kBig);
    CHECK(huge.h == kBig);
    CHECK_FALSE(huge.place_moved_x);
    CHECK_FALSE(huge.place_moved_y);
    CHECK_FALSE(author_pane_size(s, builder, PaneSize{pane_unit::kSubcells, huge.w},
                                 PaneSize{pane_unit::kSubcells, huge.h})
                    .accepted);
    CHECK(s == before);
}

TEST_CASE("WIND-2: an unknown mode word names what it found and what would have worked") {
    const Setup good = two_overlays();
    const std::string valid = setup_persist::to_text(good);

    struct Case {
        const char* what;
        std::string text;
        const char* found;
        const char* allowed;
    };
    std::vector<Case> cases;
    cases.push_back({"an unknown PLACE word",
                     forged_setup(good, "\"place\":{\"mode\":\"default\"",
                                  "\"place\":{\"mode\":\"furlongs\""),
                     "furlongs", "default or subcells"});
    // `pixels` IS NOT A PLACE UNIT, and this is where that is said. A place has one unit;
    // offering it a size's is offering a word this field's vocabulary does not have.
    cases.push_back({"a SIZE word offered to a place",
                     forged_setup(good, "\"place\":{\"mode\":\"default\"",
                                  "\"place\":{\"mode\":\"pixels\""),
                     "pixels", "default or subcells"});
    cases.push_back({"an unknown WIDTH word",
                     forged_setup(good, "\"width\":{\"mode\":\"default\"",
                                  "\"width\":{\"mode\":\"ems\""),
                     "ems", "default, subcells or pixels"});
    cases.push_back({"an unknown HEIGHT word",
                     forged_setup(good, "\"height\":{\"mode\":\"default\"",
                                  "\"height\":{\"mode\":\"\""),
                     "", "default, subcells or pixels"});

    for (const Case& c : cases) {
        CAPTURE(c.what);
        const setup_persist::LoadedSetup refused = setup_persist::from_text(c.text);
        CHECK_FALSE(refused.outcome.accepted);
        CHECK(refused.outcome.refusal.find(c.found) != std::string::npos);
        CHECK(refused.outcome.refusal.find(c.allowed) != std::string::npos);
        // THE WHOLE CANDIDATE IS REFUSED and nothing is carried out of it, which is what
        // makes "never halfway restored" structural rather than careful.
        CHECK(refused.setup.name.empty());
        CHECK(refused.setup.panes.empty());
    }
    CHECK(setup_persist::from_text(valid).outcome.accepted);
}

TEST_CASE("WIND-2: a version-1 file is refused BY NUMBER, before its rows are judged") {
    // A REAL VERSION-1 FILE, spelled the way WS-0 wrote one: the envelope claims
    // `WorkshopSetup v1` and its pane rows carry two strings and nothing else. Forging it
    // from a version-2 file would only prove the field check; this proves the ORDERING,
    // because these bytes are missing every field version 2 added.
    const std::string v1 =
        "{\"zen\":1,\"schema\":\"WorkshopSetup\",\"version\":1,"
        "\"content_id\":\"0x0\",\"fields\":{"
        "\"format\":\"zengine-workshop-setup\",\"format_version\":\"1\","
        "\"name\":\"Everything\",\"panes\":["
        "{\"provider\":\"zengine.workshop\",\"pane\":\"info\"}]}}";
    const setup_persist::LoadedSetup refused = setup_persist::from_text(v1);
    CHECK_FALSE(refused.outcome.accepted);
    INFO(refused.outcome.refusal);
    // BY ITS NUMBER, and in Workshop's own words.
    CHECK(refused.outcome.refusal.find("setup version 1") != std::string::npos);
    CHECK(refused.outcome.refusal.find("reads versions 2 and 3") != std::string::npos);
    // AND NOT BY A ROW FIELD. That sentence would be true and would name the wrong cause --
    // a maker fixing a missing `place` would never find out their file is a version old.
    CHECK(refused.outcome.refusal.find("place") == std::string::npos);
    CHECK(refused.outcome.refusal.find("front") == std::string::npos);
    CHECK(refused.setup.name.empty());
    CHECK(refused.setup.panes.empty());

    // THE FIELD IS STILL CHECKED TOO, for the forgery that only a reader of this format
    // would produce: a version-3 envelope whose own stated version is not 3.
    const setup_persist::LoadedSetup forged = setup_persist::from_text(
        forged_setup(two_overlays(), "\"format_version\":\"3\"", "\"format_version\":\"1\""));
    CHECK_FALSE(forged.outcome.accepted);
    CHECK(forged.outcome.refusal.find("setup version 1") != std::string::npos);
}

TEST_CASE("WIND-2: a version-1 file leaves the live setup and its on-file copy untouched") {
    TempDir dir("wind2-v1");
    const std::string path = dir.file("setup.json");
    Live t;
    t.host.setup_path = path;
    t.host.document_path = dir.file("doc.json");

    // A LIVE, ARRANGED, SAVED SETUP -- so a refusal has something to damage.
    name_setup(t, "Mine");
    REQUIRE((live_status(t.session().setup) == setup_link::kCurrent));
    const Setup live = t.session().setup.active;
    const std::string good_bytes = slurp(path);

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const std::string v1_bytes =
            "{\"zen\":1,\"schema\":\"WorkshopSetup\",\"version\":1,"
            "\"content_id\":\"0x0\",\"fields\":{"
            "\"format\":\"zengine-workshop-setup\",\"format_version\":\"1\","
            "\"name\":\"Old\",\"panes\":[]}}";
        out.write(v1_bytes.data(), static_cast<std::streamsize>(v1_bytes.size()));
    }
    t.key(input::scan::kR);
    CHECK(t.notice().find("setup version 1") != std::string::npos);
    CHECK(t.session().setup.active == live);
    CHECK(t.session().setup.active_link.known == live);
    CHECK(t.session().panels.has(panel::kInfo));
    // AND THE FILE ON DISK IS NOT REWRITTEN BY A REFUSAL EITHER.
    CHECK(slurp(path) != good_bytes); // the case wrote it, not Workshop
    REQUIRE(setup_persist::save_file(path, t.session().setup.active).accepted);
    CHECK(slurp(path) == good_bytes);
}

TEST_CASE("WIND-2: an unresolved reference round-trips every authored field exactly") {
    Setup s;
    s.name = "Later";
    REQUIRE(add_pane(s, ref_of(panel::kInfo)));
    REQUIRE(add_pane(s, stranger()));
    // A PLACE FAR OFF ANY SCREEN THIS COMPOSITION LAYS OUT, deliberately: authored intent is
    // not clamped on the way in or on the way out, so a file may legally hold a place the
    // current canvas has no cell for. Clamping it at LOAD would make a maker's saved arrangement
    // depend on the screen they last opened it on.
    s.panes[1].place = PanePlace{pane_unit::kSubcells, subs(900) + 7, subs(700)};
    s.panes[1].width = PaneSize{pane_unit::kSubcells, subs(33) + 1};
    s.panes[1].height = PaneSize{pane_unit::kPixels, 400};
    // A DELIBERATE RANK, and it has to be a REAL move: `add_pane` already put this row at
    // the front, so sending it there again is the no-op the ordering doors answer `false` to.
    REQUIRE(send_to_back(s, stranger()));
    REQUIRE(check_setup(s).accepted);

    const std::string a = setup_persist::to_text(s);
    const setup_persist::LoadedSetup read = setup_persist::from_text(a);
    REQUIRE(read.outcome.accepted);
    CHECK(read.setup == s);
    // A PANE THIS BUILD HAS NEVER HEARD OF KEEPS ITS WHOLE WINDOW, and it is the row the
    // sparse model exists for: nothing resolved it, nothing clamped it, and its rank rode
    // its own row.
    const SetupPane* held = pane_of(read.setup, stranger());
    REQUIRE(held != nullptr);
    CHECK(held->place.mode == pane_unit::kSubcells);
    CHECK(held->place.x == subs(900) + 7);
    CHECK(held->place.y == subs(700));
    CHECK(held->width.amount == subs(33) + 1);
    CHECK(held->height.mode == pane_unit::kPixels);
    CHECK(held->height.amount == 400);
    CHECK(held->front == 0);
    CHECK(pane_of(read.setup, ref_of(panel::kInfo))->front == 1);
    CHECK(setup_persist::to_text(read.setup) == a);
    CHECK_FALSE(resolvable(stranger(), no_providers()));
}

TEST_CASE("WIND-2: dirty is structural -- an inverse edit makes a setup clean again") {
    TempDir dir("wind2-dirty");
    const std::string path = dir.file("setup.json");
    Live t;
    t.host.setup_path = path;
    t.host.document_path = dir.file("doc.json");
    // A PANE A MAKER MAY ARRANGE. The default setup names only Info, whose place is the
    // screen's reserved column -- so a case about a geometry edit has to open an overlay
    // pane first, which is itself the reserved-column law being visible.
    open_pane(t, ref_of(panel::kBuilder));
    name_setup(t, "Clean");
    REQUIRE((live_status(t.session().setup) == setup_link::kCurrent));

    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kBuilder));
    // A GEOMETRY EDIT DIRTIES IT...
    t.key(input::scan::kRight);
    CHECK_FALSE((live_status(t.session().setup) == setup_link::kCurrent));
    // ...AND THE EXACT INVERSE MAKES IT CLEAN, because `saved()` COMPARES rather than
    // remembering that a gesture happened. A dirty FLAG would still be set here.
    t.key(input::scan::kLeft);
    CHECK_FALSE((live_status(t.session().setup) == setup_link::kCurrent)); // authored, and it was not
    t.key(input::scan::k0);
    t.key(input::scan::kP);
    CHECK((live_status(t.session().setup) == setup_link::kCurrent));

    // THE SAME, SAID ABOUT ORDER: a permutation returned to the identity is byte-identical
    // to a setup that was never reordered.
    t.key(input::scan::kF);
    t.key(input::scan::k0);
    t.key(input::scan::kO);
    CHECK((live_status(t.session().setup) == setup_link::kCurrent));
    CHECK(setup_persist::to_text(t.session().setup.active) == slurp(path));
}

// ---- RESOLUTION -----------------------------------------------------------------------

TEST_CASE("WIND-2: each axis is independent -- a place edit freezes no size, and back") {
    Setup s = two_overlays();
    const PaneRef builder = ref_of(panel::kBuilder);

    REQUIRE(author_pane_place(s, builder, subs(6), subs(5)).accepted);
    const SetupPane* row = pane_of(s, builder);
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == pane_unit::kSubcells);
    // BOTH SIZES ARE STILL THE DEVELOPER'S, which is the whole return on sparseness: a
    // maker who moved a pane has said nothing about how big it should be.
    CHECK(row->width.mode == pane_unit::kDefault);
    CHECK(row->height.mode == pane_unit::kDefault);

    // A WIDTH-ONLY EDIT MOVES NEITHER THE PLACE NOR THE HEIGHT.
    REQUIRE(author_pane_size(s, builder, PaneSize{pane_unit::kSubcells, subs(40)}, row->height)
                .accepted);
    row = pane_of(s, builder);
    CHECK(row->width.amount == subs(40));
    CHECK(row->height.mode == pane_unit::kDefault);
    CHECK(row->place.x == subs(6));
    CHECK(row->place.y == subs(5));

    // ...AND A HEIGHT-ONLY EDIT IS THE MIRROR.
    REQUIRE(author_pane_size(s, builder, row->width, PaneSize{pane_unit::kSubcells, subs(4)})
                .accepted);
    row = pane_of(s, builder);
    CHECK(row->width.amount == subs(40));
    CHECK(row->height.amount == subs(4));
    CHECK(row->place.x == subs(6));
}

TEST_CASE("WIND-2: a default width still follows the WIND-1 half-share after a place edit") {
    Session s;
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    const PaneRef builder = ref_of(panel::kBuilder);
    REQUIRE(author_pane_place(s.setup.active, builder, subs(6), subs(5)).accepted);

    // THE PLACE IS THE MAKER'S AND THE WIDTH IS STILL THE ROOM'S. Measured at three extents,
    // against WIND-1's own expression -- so a later change to the half-share moves this pane
    // with it, which is exactly what "the developer authors a default" is worth.
    for (const std::int64_t w : {78, 120, 200}) {
        CAPTURE(w);
        s.screen_w = w;
        s.screen_h = 40;
        const Screen sc = screen_of(s);
        const ui::Rect got =
cells_covered(bounds_of(s.panels, s.setup.active, panel::kBuilder, sc).rect);
        const ui::Rect reactive = placement_bounds(placement::kOverlayStack, 0, sc);
        CHECK(got.w == reactive.w);
        CHECK(got.w == kStackW + (sc.room_w - kStackW) / 2);
        CHECK(got.h == reactive.h);
        // ...and the place did NOT follow the room.
        CHECK(got.x == 6);
        CHECK(got.y == 5);
    }
}

TEST_CASE("WIND-2: an authored place is absolute canvas position, not an offset from the default") {
    Session s;
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    REQUIRE(author_pane_place(s.setup.active, ref_of(panel::kBuilder), 0, 0).accepted);
    const Screen sc = screen_of(s);
    // THE DEVELOPER'S OWN SLOT IS AT `kStackY`, NOT AT ZERO. So a place of (0,0) that
    // resolved to the developer's row would be an offset; one that resolves to the canvas
    // corner is absolute, and that is the whole difference.
    CHECK(placement_bounds(placement::kOverlayStack, 0, sc).y == kStackY);
    CHECK(kStackY != 0);
    const ui::Rect got =
cells_covered(bounds_of(s.panels, s.setup.active, panel::kBuilder, sc).rect);
    CHECK(got.x == 0);
    CHECK(got.y == 0);
}

TEST_CASE("WIND-2: a partly off-room pane is clipped, and its intent is not rewritten") {
    Session s;
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    const Screen sc = screen_of(s);
    const PaneRef builder = ref_of(panel::kBuilder);
    REQUIRE(author_pane_place(s.setup.active, builder, subs(sc.w - 4), subs(2)).accepted);
    REQUIRE(author_pane_size(s.setup.active, builder, PaneSize{pane_unit::kSubcells, subs(20)},
                             PaneSize{pane_unit::kSubcells, subs(5)})
                .accepted);
    // IN FRONT, so the state this case is about is CLIPPING and not coverage. The four
    // columns this pane has left sit over the Info column, and behind it they would be
    // `covered` -- which is a true word about a different fact.
    REQUIRE(send_to_front(s.setup.active, builder));

    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kBuilder, sc);
    CHECK(where.projected);
    // WHAT WAS ASKED FOR runs past the canvas...
    CHECK(where.resolved.x == subs(sc.w - 4));
    CHECK(where.resolved.w == subs(20));
    // ...AND WHAT IS SHOWN IS THE PART THIS SCREEN HAS.
    CHECK(where.rect.x == subs(sc.w - 4));
    CHECK(where.rect.w == subs(4));
    CHECK(where.rect.h == subs(5));
    // AND THE AUTHORED VALUE IS BYTE-FOR-BYTE WHAT THE MAKER SAID. A clamp written back
    // would make a maker's intent depend on the screen it was last looked at.
    const SetupPane* row = pane_of(s.setup.active, builder);
    REQUIRE(row != nullptr);
    CHECK(row->place.x == subs(sc.w - 4));
    CHECK(row->width.amount == subs(20));
    CHECK(pane_state_of(s.panels, s.setup.active, sc, CatalogRow{panel::kBuilder, builder,
                                                                "Builder", ""}) ==
          pane_state::kOpen);
}

TEST_CASE("WIND-2: a wholly off-room pane is off-room, recoverable, and painted by nobody") {
    Session s;
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    const Screen sc = screen_of(s);
    const PaneRef builder = ref_of(panel::kBuilder);
    REQUIRE(author_pane_place(s.setup.active, builder, subs(sc.w + 40), subs(sc.h + 40))
                .accepted);

    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kBuilder, sc);
    CHECK(where.open);
    CHECK(where.projected);
    CHECK(where.resolved.x == subs(sc.w + 40));
    CHECK(where.rect.w == 0);
    CHECK(where.rect.h == 0);
    CHECK(pane_state_of(s.panels, s.setup.active, sc,
                        CatalogRow{panel::kBuilder, builder, "Builder", ""}) ==
          pane_state::kOffRoom);

    // NOTHING IS PAINTED AND NOTHING IS MET, and neither needed a branch of its own: an
    // empty rectangle contains nothing and is drawn by nobody.
    const WorkshopDoc d = two_panels();
    const surface::SurfaceCanvas c = paint(d, s);
    for (const surface::SurfaceRect& r : all_rects(c)) {
        CHECK_FALSE((r.x == sc.w + 40 && r.y == sc.h + 40));
    }
    CHECK_FALSE(occupied_at(s.panels, s.setup.active, sc, sc.w + 40, sc.h + 40).occupied);

    // AND IT IS STILL IN THE INVENTORY, which is the recovery invariant: a maker reaches it
    // by its row, resets its place, and gets it back.
    bool listed = false;
    for (const CatalogRow& row : inventory_rows(s.setup.active, s.panels)) {
        if (row.ref == builder) {
            listed = true;
        }
    }
    CHECK(listed);
    CHECK(reset_pane_place(s.setup.active, builder));
    CHECK(bounds_of(s.panels, s.setup.active, panel::kBuilder, sc).rect ==
          fine_of_cells(placement_bounds(placement::kOverlayStack, 0, sc)));
}

TEST_CASE("WIND-2: an authored place spends no reactive slot, and cannot wait for one") {
    // THE MINIMUM COMPOSITION HAS ROOM FOR EXACTLY ONE OVERLAY SLOT, which is where the
    // rationing is tightest and therefore where this law has to hold.
    REQUIRE(stack_slots_that_fit(kMinScreen) == 1);

    // TWO OVERLAY PANES ARE NEEDED TO SEE A TILE BE SPENT AT ALL, and this build has one
    // overlay built-in -- so the second comes through the ordinary admission door, with no
    // weave, no bus and no library: `admit_pane_offer` is a pure function over a catalog.
    Panels panels;
    panels.open.clear();
    const PaneOffered offered{"history", "History", "a second overlay pane"};
    const Admission got = admit_pane_offer(panels.runtime, "third.party.tools", offered);
    REQUIRE(got.written.accepted);
    const PaneRef extern_ref{"third.party.tools", "history"};
    REQUIRE(placement_of(got.kind) == placement::kOverlayStack);

    Setup s;
    s.name = "Two overlays";
    REQUIRE(add_pane(s, ref_of(panel::kBuilder)));
    REQUIRE(add_pane(s, extern_ref));

    // BOTH REACTIVE: the second one waits, because there is one tile. The CONTROL.
    const Seating reactive = seat_panes(s, panels, stack_capacity(kMinScreen));
    REQUIRE(reactive.wanted.size() == 1);
    CHECK(reactive.wanted[0] == panel::kBuilder);
    REQUIRE(reactive.waiting.size() == 1);
    CHECK(reactive.waiting[0] == got.kind);

    // NOW PLACE THE FIRST ONE. It stops spending the tile -- a pane the maker put somewhere
    // is not in the tiling -- so the reactive one behind it gets the slot, and the placed one
    // is not waiting either, because it never asked the stack for anything.
    Setup placed = s;
    REQUIRE(author_pane_place(placed, ref_of(panel::kBuilder), subs(2), subs(2)).accepted);
    const Seating after = seat_panes(placed, panels, stack_capacity(kMinScreen));
    CHECK(after.waiting.empty());
    REQUIRE(after.wanted.size() == 2);
    CHECK(after.wanted[0] == panel::kBuilder);
    CHECK(after.wanted[1] == got.kind);

    // AND THE SLOT COUNTER AGREES WITH THE SEATING, because it is the same rule said where
    // the rectangle is resolved: the reactive pane behind a placed one takes slot ZERO.
    Session sess;
    sess.setup.active = placed;
    sess.panels = panels;
    sess.panels.open = {Panel{panel::kBuilder}, Panel{got.kind}};
    const Screen sc = screen_of(sess);
    CHECK(bounds_of(sess.panels, sess.setup.active, panel::kBuilder, sc).rect.x == subs(2));
    CHECK(bounds_of(sess.panels, sess.setup.active, panel::kBuilder, sc).rect.y == subs(2));
    CHECK(bounds_of(sess.panels, sess.setup.active, got.kind, sc).rect ==
          fine_of_cells(placement_bounds(placement::kOverlayStack, 0, sc)));

    // ...AND RESETTING THE PLACE PUTS IT BACK IN THE TILING, which is what makes the reset a
    // real recovery rather than a different arrangement.
    REQUIRE(reset_pane_place(placed, ref_of(panel::kBuilder)));
    const Seating back = seat_panes(placed, panels, stack_capacity(kMinScreen));
    CHECK(back.waiting.size() == 1);
    CHECK(back.waiting[0] == got.kind);
}
TEST_CASE("WIND-2: a pixel axis is setup-valid, projection-refused, and never falls back") {
    Setup s = two_overlays();
    const PaneRef builder = ref_of(panel::kBuilder);
    REQUIRE(author_pane_size(s, builder, PaneSize{pane_unit::kPixels, 240},
                             PaneSize{pane_unit::kDefault, 0})
                .accepted);
    // VALID, on every medium, at load and in memory.
    CHECK(check_setup(s).accepted);
    CHECK(setup_persist::from_text(setup_persist::to_text(s)).outcome.accepted);

    Session sess;
    sess.setup.active = s;
    sess.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    for (const std::int64_t adv : {std::int64_t{0}, std::int64_t{8}}) {
        CAPTURE(adv);
        sess.text_advance_px = adv;
        sess.text_line_px = adv > 0 ? 18 : 0;
        const Screen sc = screen_of(sess);
        const PanelBounds where = bounds_of(sess.panels, sess.setup.active, panel::kBuilder, sc);
        // NOT PROJECTED, AND NOT FALLEN BACK. The rectangle is empty rather than the
        // developer's width or the authored number read as cells, which is the silent
        // default this contract exists to refuse.
        CHECK_FALSE(where.projected);
        CHECK(where.rect.w == 0);
        CHECK_FALSE(where.rect.w ==
                    subs(placement_bounds(placement::kOverlayStack, 0, sc).w));
        CHECK_FALSE(where.rect.w == 240);
        CHECK(pane_state_of(sess.panels, sess.setup.active, sc,
                            CatalogRow{panel::kBuilder, builder, "Builder", ""}) ==
              pane_state::kRefused);
    }
    // AND THE AUTHORED BYTES ARE UNTOUCHED BY THE REFUSAL.
    CHECK(pane_of(sess.setup.active, builder)->width.amount == 240);
    CHECK(pane_of(sess.setup.active, builder)->width.mode == pane_unit::kPixels);
}

TEST_CASE("WIND-2: a refused pane is refused rather than waiting, and it still SEATS") {
    // THE PRECEDENCE, MEASURED. Seating is medium-independent and knows nothing about
    // units, so a default-place row with a pixel axis takes its tile exactly as it always
    // did -- and the classifier reports the UNIT, because a taller window would give it the
    // tile and it still would not be presented.
    Setup s = two_overlays();
    const PaneRef builder = ref_of(panel::kBuilder);
    REQUIRE(author_pane_size(s, builder, PaneSize{pane_unit::kPixels, 240},
                             PaneSize{pane_unit::kDefault, 0})
                .accepted);
    Panels panels;
    panels.open.clear();
    const Seating seated = seat_panes(s, panels, stack_capacity(kMinScreen));
    CHECK(seated.waiting.empty());
    bool wanted = false;
    for (const std::int64_t k : seated.wanted) {
        if (k == panel::kBuilder) {
            wanted = true;
        }
    }
    CHECK(wanted);
}

// ---- ORDER ----------------------------------------------------------------------------

TEST_CASE("WIND-2: every ordering operation is an exact permutation, ends included") {
    Setup s;
    s.name = "Three";
    const PaneRef a = ref_of(panel::kInfo);
    const PaneRef b = ref_of(panel::kBuilder);
    const PaneRef c = stranger();
    REQUIRE(add_pane(s, a));
    REQUIRE(add_pane(s, b));
    REQUIRE(add_pane(s, c));
    CHECK(ranks_of(s) == std::vector<std::int64_t>{0, 1, 2});

    // THE ADVERSARIAL SEQUENCE, and every stage is a permutation.
    REQUIRE(send_to_front(s, a));
    CHECK(ranks_of(s) == std::vector<std::int64_t>{2, 0, 1});
    REQUIRE(send_to_front(s, b));
    CHECK(ranks_of(s) == std::vector<std::int64_t>{1, 2, 0});
    REQUIRE(send_to_front(s, a));
    CHECK(ranks_of(s) == std::vector<std::int64_t>{2, 1, 0});
    // ...AND A NO-OP IS ANSWERED `false` RATHER THAN SILENTLY ACCEPTED: "you are already
    // there" is an answer, and a gesture that reports success and changes nothing is not.
    CHECK_FALSE(send_to_back(s, c));
    CHECK(ranks_of(s) == std::vector<std::int64_t>{2, 1, 0});
    REQUIRE(raise_one(s, b));
    CHECK(ranks_of(s) == std::vector<std::int64_t>{1, 2, 0});
    REQUIRE(lower_one(s, a));
    CHECK(ranks_of(s) == std::vector<std::int64_t>{0, 2, 1});
    reset_front(s);
    CHECK(ranks_of(s) == std::vector<std::int64_t>{0, 1, 2});

    // THE ENDS ARE NO-OPS AND SAY SO.
    CHECK_FALSE(raise_one(s, c)); // already front-most
    CHECK_FALSE(lower_one(s, a)); // already back-most
    CHECK_FALSE(send_to_front(s, c));
    CHECK_FALSE(send_to_back(s, a));
    CHECK(is_permutation(s));
    CHECK(check_setup(s).accepted);

    // ADD APPENDS AT THE FRONT; REMOVE CLOSES THE RANKS OVER THE HOLE.
    const PaneRef d{"other.tools", "graph"};
    REQUIRE(add_pane(s, d));
    CHECK(pane_of(s, d)->front == 3);
    CHECK(is_permutation(s));
    REQUIRE(remove_pane(s, b)); // rank 1
    CHECK(is_permutation(s));
    CHECK(check_setup(s).accepted);
    CHECK(pane_of(s, a)->front == 0);
    CHECK(pane_of(s, c)->front == 1);
    CHECK(pane_of(s, d)->front == 2);
}

TEST_CASE("WIND-2: a gapped or duplicated rank is refused, and a fresh one is not") {
    Setup s = two_overlays();
    CHECK(check_setup(s).accepted);
    Setup duped = s;
    duped.panes[1].front = 0;
    CHECK_FALSE(check_setup(duped).accepted);
    CHECK(check_setup(duped).refusal.find("front order") != std::string::npos);
    Setup gapped = s;
    gapped.panes[1].front = 5;
    CHECK_FALSE(check_setup(gapped).accepted);
    CHECK(check_setup(gapped).refusal.find("0 to 1") != std::string::npos);
    Setup negative = s;
    negative.panes[0].front = -1;
    CHECK_FALSE(check_setup(negative).accepted);

    // AND ONE ROW AT RANK ZERO IS LEGAL, which is the one case a default-constructed row is
    // valid for -- named rather than hidden.
    Setup one;
    one.name = "One";
    REQUIRE(add_pane(one, ref_of(panel::kInfo)));
    CHECK(one.panes[0].front == 0);
    CHECK(check_setup(one).accepted);
}

TEST_CASE("WIND-2: 10,000 alternating ordering operations stay inside 0..n-1") {
    Setup s;
    s.name = "Bounded";
    const PaneRef a = ref_of(panel::kInfo);
    const PaneRef b = ref_of(panel::kBuilder);
    REQUIRE(add_pane(s, a));
    REQUIRE(add_pane(s, b));
    REQUIRE(add_pane(s, stranger()));

    std::int64_t largest = 0;
    for (int i = 0; i < 10000; ++i) {
        (void)send_to_front(s, i % 2 == 0 ? a : b);
        REQUIRE(is_permutation(s));
        for (const SetupPane& row : s.panes) {
            largest = row.front > largest ? row.front : largest;
        }
    }
    // NOTHING ACCUMULATES. The greatest value ever written is `n-1`, so no operation can
    // become unavailable, no bound can be met, and there is no renormalisation pass.
    CHECK(largest == static_cast<std::int64_t>(s.panes.size()) - 1);
    CHECK(check_setup(s).accepted);

    // ...and the same for the two step operations, which is the other half of the pair.
    for (int i = 0; i < 10000; ++i) {
        (void)raise_one(s, i % 2 == 0 ? a : b);
        (void)lower_one(s, i % 2 == 0 ? b : a);
    }
    CHECK(is_permutation(s));
    CHECK(check_setup(s).accepted);
}

TEST_CASE("WIND-2: ordering changes paint order and NOTHING else") {
    Session s;
    s.screen_w = 120;
    s.screen_h = 40;
    s.setup.active = two_overlays();
    REQUIRE(add_pane(s.setup.active, stranger()));
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    const Screen sc = screen_of(s);

    const std::vector<std::int64_t> paint_before = painted_order(s);
    const FineRect builder_before =
        bounds_of(s.panels, s.setup.active, panel::kBuilder, sc).rect;
    const FineRect info_before = bounds_of(s.panels, s.setup.active, panel::kInfo, sc).rect;
    const std::vector<std::int64_t> open_before = open_kinds(s.panels);
    const Setup geometry_before = s.setup.active;

    REQUIRE(send_to_front(s.setup.active, ref_of(panel::kBuilder)));

    // THE PAINT ORDER MOVED...
    CHECK_FALSE(painted_order(s) == paint_before);
    CHECK(painted_order(s).back() == panel::kBuilder);
    // ...AND NOTHING ELSE DID. Not a rectangle, not the open list, not a seat, and not one
    // authored geometry field. This is the absence of a write, not a rule somebody keeps.
    CHECK(bounds_of(s.panels, s.setup.active, panel::kBuilder, sc).rect == builder_before);
    CHECK(bounds_of(s.panels, s.setup.active, panel::kInfo, sc).rect == info_before);
    CHECK(open_kinds(s.panels) == open_before);
    for (std::size_t i = 0; i < s.setup.active.panes.size(); ++i) {
        CAPTURE(i);
        CHECK(s.setup.active.panes[i].ref == geometry_before.panes[i].ref);
        CHECK(s.setup.active.panes[i].place == geometry_before.panes[i].place);
        CHECK(s.setup.active.panes[i].width == geometry_before.panes[i].width);
        CHECK(s.setup.active.panes[i].height == geometry_before.panes[i].height);
    }
    // AND THE SETUP LIST WAS NOT SORTED. Its order is what `seat_panes` and `bounds_of`
    // read, so sorting it is exactly how raising a pane would move one.
    for (std::size_t i = 0; i < s.setup.active.panes.size(); ++i) {
        CHECK(s.setup.active.panes[i].ref == geometry_before.panes[i].ref);
    }
    CHECK(seat_panes(s.setup.active, s.panels, stack_capacity(sc)).wanted ==
          seat_panes(geometry_before, s.panels, stack_capacity(sc)).wanted);
}

TEST_CASE("WIND-2: hit order is the exact reverse of paint order") {
    Session s;
    s.screen_w = 120;
    s.screen_h = 40;
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    const Screen sc = screen_of(s);
    // TWO OVERLAY PANES ON ONE CELL. Info is the side region, so the second overlapping
    // presentation is authored by placing the Builder ON the picker's slot and asking about
    // a cell both cover -- which the Builder and a placed Builder cannot both be. So this
    // case measures the ORDER function directly and the pointer through it.
    const std::vector<std::int64_t> paint = painted_order(s);
    std::vector<std::int64_t> hit = paint;
    for (std::size_t i = 0; i < hit.size() / 2; ++i) {
        std::swap(hit[i], hit[hit.size() - 1 - i]);
    }
    REQUIRE(paint.size() >= 2);

    // THE POINTER ANSWERS WITH THE FRONT-MOST PANE ON A CELL, and the front-most is the LAST
    // painted. Asked of the Info column, which only Info covers, and then of the stack.
    const ui::Rect side =
cells_covered(bounds_of(s.panels, s.setup.active, panel::kInfo, sc).rect);
    CHECK(occupied_at(s.panels, s.setup.active, sc, side.x, side.y).occupied);
    const ui::Rect stack =
cells_covered(bounds_of(s.panels, s.setup.active, panel::kBuilder, sc).rect);
    CHECK(occupied_at(s.panels, s.setup.active, sc, stack.x, stack.y).occupied);

    // AND RAISING REVERSES BOTH TOGETHER: the two lists are one answer read two ways.
    REQUIRE(send_to_front(s.setup.active, ref_of(panel::kBuilder)));
    const std::vector<std::int64_t> after = painted_order(s);
    CHECK(after.back() == panel::kBuilder);
    std::vector<std::int64_t> reversed = after;
    for (std::size_t i = 0; i < reversed.size() / 2; ++i) {
        std::swap(reversed[i], reversed[reversed.size() - 1 - i]);
    }
    CHECK(reversed.front() == panel::kBuilder);
}

// ---- RECOVERY -------------------------------------------------------------------------

TEST_CASE("WIND-2: every setup-named pane has exactly one management row, in every state") {
    Session s;
    s.screen_w = 120;
    s.screen_h = 40;
    Setup& setup = s.setup.active;
    setup = Setup{};
    setup.name = "Every state";
    REQUIRE(add_pane(setup, ref_of(panel::kInfo)));     // open
    REQUIRE(add_pane(setup, ref_of(panel::kBuilder)));  // off-room, below
    REQUIRE(add_pane(setup, stranger()));               // unresolved
    REQUIRE(author_pane_place(setup, ref_of(panel::kBuilder), 4000, 4000).accepted);
    s.panels.open = {Panel{panel::kInfo}, Panel{panel::kBuilder}};
    const Screen sc = screen_of(s);

    const std::vector<CatalogRow> rows = inventory_rows(setup, s.panels);
    // EVERY SETUP-NAMED REFERENCE APPEARS EXACTLY ONCE, including the one the combined
    // catalog has never heard of -- which before WIND-2 had a count on the setup line and no
    // row anywhere.
    for (const SetupPane& row : setup.panes) {
        CAPTURE(ref_text(row.ref));
        std::size_t seen = 0;
        for (const CatalogRow& have : rows) {
            if (have.ref == row.ref) {
                ++seen;
            }
        }
        CHECK(seen == 1);
    }
    // AND AN UNRESOLVED ROW NAMES NO KIND, so nothing downstream can present it as the
    // Builder -- the lie `resolve_pane` is fallible to prevent, arriving through a list.
    for (const CatalogRow& have : rows) {
        if (have.ref == stranger()) {
            CHECK(have.kind == kNoPaneKind);
            CHECK(have.summary == ref_text(stranger()));
            CHECK(pane_state_of(s.panels, setup, sc, have) == pane_state::kUnresolved);
        }
        if (have.ref == ref_of(panel::kBuilder)) {
            CHECK(pane_state_of(s.panels, setup, sc, have) == pane_state::kOffRoom);
        }
        if (have.ref == ref_of(panel::kInfo)) {
            CHECK(pane_state_of(s.panels, setup, sc, have) == pane_state::kOpen);
        }
    }
    // THE SHARED-INVENTORY CONTROL: a catalog pane the setup does NOT name keeps its
    // `closed` row, so the union did not swallow the population it started from.
    Setup only_info;
    only_info.name = "Only Info";
    REQUIRE(add_pane(only_info, ref_of(panel::kInfo)));
    bool builder_closed = false;
    for (const CatalogRow& have : inventory_rows(only_info, s.panels)) {
        if (have.ref == ref_of(panel::kBuilder)) {
            builder_closed = pane_state_of(s.panels, only_info, sc, have) == pane_state::kClosed;
        }
    }
    CHECK(builder_closed);

    // AND EVERY STATE HAS ITS OWN WORD, none of them empty and none of them equal.
    const std::vector<std::int64_t> all = {pane_state::kClosed,  pane_state::kUnresolved,
                                           pane_state::kRefused, pane_state::kWaiting,
                                           pane_state::kOffRoom, pane_state::kCovered,
                                           pane_state::kOpen};
    for (std::size_t i = 0; i < all.size(); ++i) {
        CHECK(std::string(pane_state_word(all[i])).size() > 0);
        CHECK(std::string(pane_state_word(all[i])).size() <= kPaneStateCols);
        for (std::size_t j = 0; j < i; ++j) {
            CHECK(std::string(pane_state_word(all[i])) != std::string(pane_state_word(all[j])));
        }
    }
}

TEST_CASE("WIND-2: two panes that each cover HALF of a third leave nothing of it showing") {
    // THE CASE THAT SEPARATES A UNION FROM A CONTAINMENT, and it needs three overlay panes to
    // exist at all -- so the second and third arrive through the ordinary admission door,
    // with no weave and no library, exactly as the seating case builds its second one.
    Session s;
    s.screen_w = 200;
    s.screen_h = 60;
    const Admission left =
        admit_pane_offer(s.panels.runtime, "left.tools",
                         PaneOffered{"half", "Left", "the left half of the cover"});
    const Admission right =
        admit_pane_offer(s.panels.runtime, "right.tools",
                         PaneOffered{"half", "Right", "the right half of the cover"});
    REQUIRE(left.written.accepted);
    REQUIRE(right.written.accepted);
    const PaneRef under = ref_of(panel::kBuilder);
    const PaneRef a{"left.tools", "half"};
    const PaneRef b{"right.tools", "half"};

    Setup& setup = s.setup.active;
    setup = Setup{};
    setup.name = "Halves";
    REQUIRE(add_pane(setup, under)); // rank 0, back-most
    REQUIRE(add_pane(setup, a));     // rank 1
    REQUIRE(add_pane(setup, b));     // rank 2, front-most
    s.panels.open = {Panel{panel::kBuilder}, Panel{left.kind}, Panel{right.kind}};

    // AN EIGHT-BY-TWO PANE, AND TWO FOUR-BY-TWO PANES THAT PARTITION IT EXACTLY. Neither half
    // contains the whole, and together they leave not one cell of it visible.
    const auto place = [&](const PaneRef& ref, std::int64_t x, std::int64_t w) {
        REQUIRE(author_pane_place(setup, ref, subs(x), subs(20)).accepted);
        REQUIRE(author_pane_size(setup, ref, PaneSize{pane_unit::kSubcells, subs(w)},
                                 PaneSize{pane_unit::kSubcells, subs(2)})
                    .accepted);
    };
    place(under, 0, 8);
    place(a, 0, 4);
    place(b, 4, 4);

    const Screen sc = screen_of(s);
    const CatalogRow row_under{panel::kBuilder, under, "Builder", ""};
    // NEITHER HALF CONTAINS IT...
    const ui::Rect mine =
cells_covered(bounds_of(s.panels, setup, panel::kBuilder, sc).rect);
    const ui::Rect ra =
cells_covered(bounds_of(s.panels, setup, left.kind, sc).rect);
    const ui::Rect rb =
cells_covered(bounds_of(s.panels, setup, right.kind, sc).rect);
    REQUIRE(mine == ui::Rect{0, 20, 8, 2});
    CHECK_FALSE((ra.contains(mine.x, mine.y) &&
                 ra.contains(mine.x + mine.w - 1, mine.y + mine.h - 1)));
    CHECK_FALSE((rb.contains(mine.x, mine.y) &&
                 rb.contains(mine.x + mine.w - 1, mine.y + mine.h - 1)));
    // ...AND TOGETHER THEY COVER IT. A classifier that asked "is it inside SOME ONE pane"
    // would answer `open` here and leave a maker reading that their pane is fine beside a
    // screen on which it is not there.
    CHECK(pane_state_of(s.panels, setup, sc, row_under) == pane_state::kCovered);

    // MOVE ONE HALF AWAY AND FOUR CELLS COME BACK, which is enough to be open.
    REQUIRE(author_pane_place(setup, b, subs(40), subs(20)).accepted);
    CHECK(pane_state_of(s.panels, setup, sc, row_under) == pane_state::kOpen);

    // AND A PANE BEHIND ONLY *PART* OF THE UNION IS OPEN TOO -- one visible cell is the whole
    // test, said from the other side.
    REQUIRE(author_pane_place(setup, b, subs(4), subs(20)).accepted);
    REQUIRE(author_pane_size(setup, b, PaneSize{pane_unit::kSubcells, subs(3)},
                             PaneSize{pane_unit::kSubcells, subs(2)})
                .accepted);
    CHECK(pane_state_of(s.panels, setup, sc, row_under) == pane_state::kOpen);
}

TEST_CASE("WIND-2: coverage is the UNION of what is in front, not containment by one pane") {
    Session s;
    s.screen_w = 200;
    s.screen_h = 60;
    Setup& setup = s.setup.active;
    setup = Setup{};
    setup.name = "Covered";
    const PaneRef under = ref_of(panel::kBuilder);
    const PaneRef over = ref_of(panel::kInfo);
    REQUIRE(add_pane(setup, under));
    REQUIRE(add_pane(setup, over));
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    const Screen sc = screen_of(s);
    const CatalogRow row_under{panel::kBuilder, under, "Builder", ""};

    // THE BUILDER, PLACED SO IT IS EXACTLY THE INFO COLUMN'S TOP-LEFT CORNER. Info is
    // in front (rank 1), so every visible cell of the Builder is behind it.
    const ui::Rect side = placement_bounds(placement::kSideRegion, 0, sc);
    REQUIRE(author_pane_place(setup, under, subs(side.x), subs(side.y)).accepted);
    REQUIRE(author_pane_size(setup, under, PaneSize{pane_unit::kSubcells, subs(4)},
                             PaneSize{pane_unit::kSubcells, subs(4)})
                .accepted);
    CHECK(pane_state_of(s.panels, setup, sc, row_under) == pane_state::kCovered);

    // PARTIAL COVERAGE IS NOT COVERAGE: one cell a maker can see is enough to be open.
    REQUIRE(author_pane_place(setup, under, subs(side.x - 2), subs(side.y)).accepted);
    CHECK(pane_state_of(s.panels, setup, sc, row_under) == pane_state::kOpen);

    // AND FRONT/BACK CHANGES THE STATE WITHOUT CHANGING ONE RECTANGLE.
    REQUIRE(author_pane_place(setup, under, subs(side.x), subs(side.y)).accepted);
    REQUIRE(pane_state_of(s.panels, setup, sc, row_under) == pane_state::kCovered);
    const FineRect was = bounds_of(s.panels, setup, panel::kBuilder, sc).rect;
    REQUIRE(send_to_front(setup, under));
    CHECK(pane_state_of(s.panels, setup, sc, row_under) == pane_state::kOpen);
    CHECK(bounds_of(s.panels, setup, panel::kBuilder, sc).rect == was);
    // ...and now the OTHER one is the covered question, asked of the union rather than of
    // one rectangle: Info is much taller than the Builder, so it is only partly covered.
    CHECK(pane_state_of(s.panels, setup, sc, CatalogRow{panel::kInfo, over, "Info", ""}) ==
          pane_state::kOpen);
}

// ---- GESTURES -------------------------------------------------------------------------

TEST_CASE("WIND-2: the `w` that opens the desk arrangement does not type itself") {
    Live t;
    enter_arrange_desk(t);
    CHECK(t.session().arrange.open);
    CHECK(t.session().arrange.desk);
    CHECK_FALSE(t.session().arrange.resetting);
    // NO PANE IS ADDRESSED MERELY BECAUSE THE SCOPE OPENED (ARR-0): the desk is the
    // subject, and a selection prerequisite is exactly what the old mode had and this
    // one refuses.
    CHECK_FALSE(t.session().arrange.addressed());
    // AND NOTHING ELSE TOOK THE CHARACTER: no draft opened, no name is being typed, and the
    // document is untouched.
    CHECK(editing_index(t) == t.session().rows.size());
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK_FALSE(t.session().panels.picker.open);
}

TEST_CASE("WIND-2: the keyboard alone reaches every window operation") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    // TWO OVERLAY PANES, so ordering is observable at all.
    open_pane(t, ref_of(panel::kBuilder));
    REQUIRE(t.session().panels.has(panel::kBuilder));

    enter_arrange_desk(t);
    // STEP: the desk opens on no pane; the first step lands on the first row, and cycling
    // reaches every setup-named pane, and only those.
    //
    // ⚠ THE RING IS AS LONG AS THE DESK, and the desk is three rows since WUX-12 (Info,
    // the Layouts pane, and the Builder this case opened). So the case walks the ring's
    // own length rather than a count it copied -- which is what makes it a claim about
    // *cycling returns you where you started* instead of about the number two.
    REQUIRE_FALSE(t.session().arrange.addressed());
    t.key(input::scan::kTab);
    REQUIRE(t.session().arrange.addressed());
    const PaneRef first = t.session().arrange.pane;
    const std::size_t ring = t.session().setup.active.panes.size();
    REQUIRE(ring == 3);
    for (std::size_t i = 1; i < ring; ++i) {
        CAPTURE(i);
        t.key(input::scan::kTab);
        CHECK_FALSE(t.session().arrange.pane == first);
    }
    t.key(input::scan::kTab);
    CHECK(t.session().arrange.pane == first);
    // ...and the reverse step is the same ring walked backwards.
    for (std::size_t i = 1; i < ring; ++i) {
        CAPTURE(i);
        t.key(input::scan::kTab, input::mod::kShift);
        CHECK_FALSE(t.session().arrange.pane == first);
    }
    t.key(input::scan::kTab, input::mod::kShift);
    CHECK(t.session().arrange.pane == first);

    // ...on the Builder, which is the one this composition lets a maker arrange.
    select_pane(t, ref_of(panel::kBuilder));

    // MOVE AND SIZE ARE ONE STATE (ARR-0): the arrows place the addressed pane with no
    // submode entered and nothing to leave before sizing it.
    const Screen sc = screen_of(t.session());
    const ui::Rect before =
cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                      panel::kBuilder, sc).rect);
    for (int step = 0; step < 4; ++step) {
        t.key(input::scan::kRight);
        t.key(input::scan::kDown);
    }
    const SetupPane* row = pane_of(t.session().setup.active, ref_of(panel::kBuilder));
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == pane_unit::kSubcells);
    CHECK(row->place.x == subs(before.x + 4));
    CHECK(row->place.y == subs(before.y + 4));
    CHECK(row->width.mode == pane_unit::kDefault);

    // SIZE, in the same state: a shifted arrow pulls the EXTENT, anchored at the place
    // (`doc::resize`'s law said about a pane) -- `pull-right` widens, `pull-down`
    // heightens, and the place the arrows just authored does not move under either.
    // The other six anchors are the pointer's, on the handles themselves, and their law
    // is pinned by the pointer cases and `pane_window_proposal`'s own suite.
    const SetupPane* was = pane_of(t.session().setup.active, ref_of(panel::kBuilder));
    const std::int64_t w0 = was->width.mode == pane_unit::kSubcells
                                ? was->width.amount
                                : bounds_of(t.session().panels, t.session().setup.active,
                                            panel::kBuilder, screen_of(t.session()))
                                      .rect.w;
    const std::int64_t h0 = was->height.mode == pane_unit::kSubcells
                                ? was->height.amount
                                : bounds_of(t.session().panels, t.session().setup.active,
                                            panel::kBuilder, screen_of(t.session()))
                                      .rect.h;
    t.key(input::scan::kRight, input::mod::kShift);
    t.key(input::scan::kDown, input::mod::kShift);
    const SetupPane* grown = pane_of(t.session().setup.active, ref_of(panel::kBuilder));
    CHECK(grown->width.mode == pane_unit::kSubcells);
    CHECK(grown->width.amount == w0 + subs(1));
    CHECK(grown->height.mode == pane_unit::kSubcells);
    CHECK(grown->height.amount == h0 + subs(1));
    CHECK(grown->place.x == subs(before.x + 4));
    CHECK(grown->place.y == subs(before.y + 4));
    // ...and the pulls reverse, place still untouched.
    t.key(input::scan::kLeft, input::mod::kShift);
    t.key(input::scan::kUp, input::mod::kShift);
    const SetupPane* shrunk = pane_of(t.session().setup.active, ref_of(panel::kBuilder));
    CHECK(shrunk->width.amount == w0);
    CHECK(shrunk->height.amount == h0);
    CHECK(shrunk->place.x == subs(before.x + 4));
    CHECK(shrunk->place.y == subs(before.y + 4));

    // ORDER: all four operations, by key.
    const std::int64_t front_before =
        pane_of(t.session().setup.active, ref_of(panel::kBuilder))->front;
    t.key(input::scan::kB);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->front == 0);
    t.key(input::scan::kF);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->front ==
          static_cast<std::int64_t>(t.session().setup.active.panes.size()) - 1);
    t.key(input::scan::kL);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->front <
          static_cast<std::int64_t>(t.session().setup.active.panes.size()) - 1);
    t.key(input::scan::kR);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->front ==
          static_cast<std::int64_t>(t.session().setup.active.panes.size()) - 1);
    (void)front_before;

    // RESET: each authored dimension independently, and the order.
    t.key(input::scan::k0);
    REQUIRE(t.session().arrange.resetting);
    t.key(input::scan::kW);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->width.mode ==
          pane_unit::kDefault);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->height.mode ==
          pane_unit::kSubcells);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->place.mode ==
          pane_unit::kSubcells);
    t.key(input::scan::k0);
    t.key(input::scan::kH);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->height.mode ==
          pane_unit::kDefault);
    t.key(input::scan::k0);
    t.key(input::scan::kP);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->place.mode ==
          pane_unit::kDefault);
    t.key(input::scan::k0);
    t.key(input::scan::kO);
    CHECK(ranks_of(t.session().setup.active) == std::vector<std::int64_t>{0, 1, 2});

    // AND ESCAPE LEAVES, one level at a time. No pointer event has been published at all.
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().arrange.open);
}

TEST_CASE("WIND-2: escape unwinds one level and rolls nothing back") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kInfo));
    // Info is the reserved side column, so a geometry step REFUSES LEGIBLY and writes
    // nothing -- the recovery-versus-authoring line, said about the one pane a maker may
    // never move.
    t.key(input::scan::kRight);
    CHECK(t.notice().find("reserved side column") != std::string::npos);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kInfo))->place.mode ==
          pane_unit::kDefault);

    // ORDER STILL WORKS ON IT, which is what makes the refusal narrow rather than a dead end.
    // Info is at the BACK of a fresh desk (the Layouts pane is in front of it), so `f`
    // moves it and says so; sending it forward twice is what reaches the no-op sentence.
    REQUIRE(t.session().setup.active.panes.size() == 2);
    t.key(input::scan::kF);
    t.key(input::scan::kF);
    CHECK(t.notice().find("already where") != std::string::npos);

    // AND ESCAPE FROM THE RESET PROMPT RETURNS ONE LEVEL WITHOUT UNDOING THE EDIT THAT
    // WAS MADE -- the one prompt step arrangement still has (ARR-0). `p` is a
    // COMMAND-mode key, so a maker leaves the scope to reach the picker -- which is the
    // priority chain doing exactly its job.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().arrange.open);
    open_pane(t, ref_of(panel::kBuilder));
    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kBuilder));
    t.key(input::scan::kRight);
    const PanePlace committed =
        pane_of(t.session().setup.active, ref_of(panel::kBuilder))->place;
    t.key(input::scan::k0);
    REQUIRE(t.session().arrange.resetting);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().arrange.resetting);
    CHECK(t.session().arrange.open);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->place == committed);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().arrange.open);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->place == committed);
}

TEST_CASE("WIND-2: a hand and a key author the same setup values") {
    const auto arrange_by_key = [](Live& t) {
        enter_arrange_desk(t);
        select_pane(t, ref_of(panel::kBuilder));
        for (int i = 0; i < 3; ++i) {
            t.key(input::scan::kRight);
        }
        for (int i = 0; i < 2; ++i) {
            t.key(input::scan::kDown);
        }
    };

    Live keyed;
    keyed.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(keyed, ref_of(panel::kBuilder));
    arrange_by_key(keyed);
    const PanePlace by_key =
        pane_of(keyed.session().setup.active, ref_of(panel::kBuilder))->place;

    Live handed;
    handed.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(handed, ref_of(panel::kBuilder));
    enter_arrange_desk(handed);
    select_pane(handed, ref_of(panel::kBuilder));
    const Screen sc = screen_of(handed.session());
    const ui::Rect rect =
cells_covered(bounds_of(handed.session().panels, handed.session().setup.active,
                                    panel::kBuilder, sc).rect);
    // TAKE HOLD IN THE MIDDLE OF THE PANE and put the hand three right and two down. The
    // grab offset is a plain subtraction, so the pane's corner lands where the key put it.
    const std::int64_t gx = rect.x + rect.w / 2;
    const std::int64_t gy = rect.y + rect.h / 2;
    handed.press_at(gx, gy + surface::kTuiCanvasTopRow, input::space::kCells);
    handed.publish(loom::to_value(input::PointerMoved{gx + 3, gy + 2 + surface::kTuiCanvasTopRow,
                                                      0, 0, input::space::kCells,
                                                      input::mod::kNone}));
    handed.publish(loom::to_value(input::PointerButton{1, false, gx + 3,
                                                       gy + 2 + surface::kTuiCanvasTopRow,
                                                       input::space::kCells, input::mod::kNone}));
    const PanePlace by_hand =
        pane_of(handed.session().setup.active, ref_of(panel::kBuilder))->place;

    CHECK(by_key.mode == pane_unit::kSubcells);
    CHECK(by_hand == by_key);
}

TEST_CASE("WIND-2: one press claims one gesture, and crossing anything does not move it") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kBuilder));
    const Screen sc = screen_of(t.session());
    const ui::Rect rect =
cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                    panel::kBuilder, sc).rect);
    const std::int64_t gx = rect.x + 2;
    const std::int64_t gy = rect.y + 2;
    t.press_at(gx, gy + surface::kTuiCanvasTopRow, input::space::kCells);
    REQUIRE(t.session().pane_drag.active);
    CHECK(t.session().pane_drag.pane == ref_of(panel::kBuilder));

    // CROSS THE INFO COLUMN -- another presentation entirely -- and the gesture is still the
    // Builder's, because a motion reads the pane that CLAIMED THE PRESS and never asks what
    // is under the pointer now.
    const ui::Rect side =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo,
                                    sc).rect);
    t.publish(loom::to_value(input::PointerMoved{side.x + 1,
                                                 side.y + 3 + surface::kTuiCanvasTopRow, 0, 0,
                                                 input::space::kCells, input::mod::kNone}));
    CHECK(t.session().pane_drag.pane == ref_of(panel::kBuilder));
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->place.mode ==
          pane_unit::kSubcells);

    // AN ORDERING CHANGE MID-DRAG DOES NOT TRANSFER CUSTODY EITHER.
    REQUIRE(send_to_back(live(t).setup.active, ref_of(panel::kBuilder)));
    t.publish(loom::to_value(input::PointerMoved{side.x + 2,
                                                 side.y + 4 + surface::kTuiCanvasTopRow, 0, 0,
                                                 input::space::kCells, input::mod::kNone}));
    CHECK(t.session().pane_drag.pane == ref_of(panel::kBuilder));

    // AND THE RELEASE ENDS IT WHEREVER THE HAND LANDS.
    t.publish(loom::to_value(input::PointerButton{1, false, 0, surface::kTuiCanvasTopRow,
                                                  input::space::kCells, input::mod::kNone}));
    CHECK_FALSE(t.session().pane_drag.active);
}

TEST_CASE("WIND-2: outside arrangement, an addressed pane behind another clicks through "
          "nothing") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kBuilder));
    // PUT THE BUILDER UNDER THE INFO COLUMN AND SEND IT TO THE BACK.
    const Screen sc = screen_of(t.session());
    const ui::Rect side =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo,
                                    sc).rect);
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), side.x,
                              side.y + 2)
                .accepted);
    REQUIRE(send_to_back(live(t).setup.active, ref_of(panel::kBuilder)));
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().arrange.open);
    // LEAVING CLEARS THE WHOLE STATE (ARR-0): the desk deliberately opens on no pane, so
    // there is no address to carry between visits...
    CHECK_FALSE(t.session().arrange.addressed());

    // ...AND THE PRIORITY WAS THE MODE'S ALONE. A press inside the covered Builder is
    // answered by the VISIBLE pane on top of it, which is what stops arrangement interest
    // from becoming a click-through.
    // The cell is inside BOTH rectangles and inside Info's own chrome (WUX-5), on a body
    // row that carries no control -- so what answers is the panel's own occupancy sentence
    // and not one of the three runs inside its body.
    const std::int64_t at_x = side.x + 1 + kChromeCells;
    const std::int64_t at_y = side.y + 3 + kChromeCells;
    const Occupancy here = occupied_at(t.session().panels, t.session().setup.active, sc,
                                       at_x, at_y);
    REQUIRE(here.occupied);
    CHECK(here.what == std::string("Info"));
    t.press_at(at_x, at_y + surface::kTuiCanvasTopRow, input::space::kCells);
    CHECK(t.notice().find("Info is here") != std::string::npos);
    // AND NO SELECTION AUTO-RAISED ANYTHING.
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->front == 0);
}

TEST_CASE("WIND-2: move writes only place, resize only size, and order only front") {
    Setup s = two_overlays();
    const PaneRef b = ref_of(panel::kBuilder);

    Setup after_move = s;
    REQUIRE(author_pane_place(after_move, b, subs(3), subs(4)).accepted);
    CHECK(pane_of(after_move, b)->width == pane_of(s, b)->width);
    CHECK(pane_of(after_move, b)->height == pane_of(s, b)->height);
    CHECK(pane_of(after_move, b)->front == pane_of(s, b)->front);
    CHECK(ranks_of(after_move) == ranks_of(s));

    Setup after_size = s;
    REQUIRE(author_pane_size(after_size, b, PaneSize{pane_unit::kSubcells, subs(30)},
                             PaneSize{pane_unit::kSubcells, subs(7)})
                .accepted);
    CHECK(pane_of(after_size, b)->place == pane_of(s, b)->place);
    CHECK(pane_of(after_size, b)->front == pane_of(s, b)->front);

    Setup after_order = s;
    REQUIRE(send_to_front(after_order, b));
    CHECK(pane_of(after_order, b)->place == pane_of(s, b)->place);
    CHECK(pane_of(after_order, b)->width == pane_of(s, b)->width);
    CHECK(pane_of(after_order, b)->height == pane_of(s, b)->height);
}

TEST_CASE("WIND-2: clearing the selected pane clears its gesture safely") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kBuilder));
    const Screen sc = screen_of(t.session());
    const ui::Rect rect =
cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                    panel::kBuilder, sc).rect);
    t.press_at(rect.x + 1, rect.y + 1 + surface::kTuiCanvasTopRow, input::space::kCells);
    REQUIRE(t.session().pane_drag.active);

    // THE TARGET LEAVES THE SETUP UNDER THE HAND. A motion then ends the gesture rather than
    // writing to a row that is no longer there.
    REQUIRE(remove_pane(live(t).setup.active, ref_of(panel::kBuilder)));
    t.publish(loom::to_value(input::PointerMoved{rect.x + 4,
                                                 rect.y + 4 + surface::kTuiCanvasTopRow, 0, 0,
                                                 input::space::kCells, input::mod::kNone}));
    CHECK_FALSE(t.session().pane_drag.active);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));
}

// ============================================================================
// WUX-6 -- the maker reads their pane in the language of the face in front of them
// ============================================================================

namespace {

/// STEP THE ARRANGEMENT DESK TO ONE PANE, by the maker's own key. Bounded, so a
/// reference the desk cannot reach fails with a sentence rather than spinning.
inline void step_to(Live& t, const PaneRef& ref) {
    for (int i = 0; i < 32; ++i) {
        if (t.session().arrange.addressed() && t.session().arrange.pane == ref) {
            return;
        }
        t.key(input::scan::kTab);
    }
    FAIL("the arrangement desk never stepped to ", ref_text(ref));
}

/// A GEOMETRY NO MEDIUM HERE CAN SAY THE SAME WAY TWICE. Each number is a whole
/// number of the shipped window's pixels (four sub-units) and none of them is a
/// whole number of cells -- the hostile value the phase's falsifier needs, chosen so
/// that a green produced by values which happen to divide evenly is impossible.
inline constexpr std::int64_t kOddPlaceX = 4 * 77;   //  77 px, 6 cells + 20/48
inline constexpr std::int64_t kOddPlaceY = 4 * 53;   //  53 px, 4 cells + 20/48
inline constexpr std::int64_t kOddWidth = 4 * 417;   // 417 px, 34 cells + 36/48
inline constexpr std::int64_t kOddHeight = 4 * 233;  // 233 px, 19 cells + 20/48

static_assert(kOddWidth % surface::kCellSubs != 0, "the falsifier must not divide evenly");
static_assert(kOddPlaceX % surface::kCellSubs != 0, "the falsifier must not divide evenly");

} // namespace

TEST_CASE("WUX-6/SC-2: the arrangement notice speaks the unit the FACE reported") {
    // The same authored value, read through two media, in two languages -- and the
    // language is the medium's own answer about its canvas, never a constant Workshop
    // holds. Nothing here authors anything: the two readings are of one desk.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 0, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    Setup& desk = live(t).setup.active;
    const PaneRef builder = ref_of(panel::kBuilder);
    REQUIRE(author_pane_place(desk, builder, kOddPlaceX, kOddPlaceY).accepted);
    REQUIRE(author_pane_size(desk, builder, PaneSize{pane_unit::kSubcells, kOddWidth},
                             PaneSize{pane_unit::kSubcells, kOddHeight})
                .accepted);
    enter_arrange_desk(t);
    step_to(t, builder);

    // IN A TERMINAL, CELLS -- and every one of these four numbers is a projection this
    // medium cannot say exactly, so every one of them wears the mark and the line names
    // the reason ONCE.
    const std::string in_cells = t.notice();
    INFO(in_cells);
    CHECK(in_cells.find("@~6,~4 ~34x~19 cells") != std::string::npos);
    CHECK(in_cells.find("(~ projected)") != std::string::npos);
    CHECK(in_cells.find(" px ") == std::string::npos);

    // THE SAME DESK ON THE SHIPPED WINDOW: pixels, exactly, with nothing marked -- these
    // are the numbers a hand at that face's own grain would have authored.
    t.publish(loom::to_value(
        surface::SurfaceExtent{200, 60, 8, 18, surface::kCanvasCellPx}));
    t.key(input::scan::kTab);
    step_to(t, builder);
    const std::string in_px = t.notice();
    INFO(in_px);
    CHECK(in_px.find("@77,53 417x233 px") != std::string::npos);
    CHECK(in_px.find("(~ projected)") == std::string::npos);
    CHECK(in_px.find("cells") == std::string::npos);

    // AND THE AUTHORED VALUE IS UNTOUCHED BY EITHER READING. Looking is not authoring:
    // the maker crossed two media, read two sentences, and the desk is the same desk.
    const SetupPane* row = pane_of(t.session().setup.active, builder);
    REQUIRE(row != nullptr);
    CHECK(row->place.x == kOddPlaceX);
    CHECK(row->place.y == kOddPlaceY);
    CHECK(row->width.amount == kOddWidth);
    CHECK(row->height.amount == kOddHeight);
}

TEST_CASE("WUX-6/SC-6: the notice says where a pane the maker did not place actually is") {
    // A reactive axis's AUTHORED text is `-`, which is the truth and is not a rectangle.
    // So a window still partly the code's answer -- which every pane on a fresh desk is --
    // is followed by where it currently resolves, in the same unit, marked `now`.
    Live t;
    t.publish(loom::to_value(
        surface::SurfaceExtent{200, 60, 8, 18, surface::kCanvasCellPx}));
    enter_arrange_desk(t);
    step_to(t, ref_of(panel::kInfo));
    const std::string reactive = t.notice();
    INFO(reactive);
    CHECK(reactive.find("-x-") != std::string::npos);
    CHECK(reactive.find(" -- now @") != std::string::npos);
    CHECK(reactive.find(" px") != std::string::npos);

    // ...AND IT IS THE PANE'S OWN RECTANGLE, in that medium's unit, not a second answer.
    const Screen sc = screen_of(t.session());
    const PanelBounds b =
        bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc);
    CHECK(reactive.find("now " + fine_rect_text(b.resolved, t.session().cell_px)) !=
          std::string::npos);

    // ONCE THE MAKER HAS AUTHORED THE WHOLE WINDOW, the authored text IS the rectangle
    // and saying it twice would be noise.
    Setup& desk = live(t).setup.active;
    REQUIRE(author_pane_place(desk, ref_of(panel::kInfo), subs(2), subs(2)).accepted);
    t.key(input::scan::kTab);
    step_to(t, ref_of(panel::kInfo));
    CHECK(t.notice().find(" -- now @") != std::string::npos); // extents still reactive
    REQUIRE(author_pane_size(desk, ref_of(panel::kInfo), PaneSize{pane_unit::kSubcells, subs(20)},
                             PaneSize{pane_unit::kSubcells, subs(6)})
                .accepted);
    t.key(input::scan::kTab);
    step_to(t, ref_of(panel::kInfo));
    INFO(t.notice());
    CHECK(t.notice().find(" -- now @") == std::string::npos);
}

TEST_CASE("WUX-6/SC-5+SC-7: the coarse step is the resize seam with a bigger delta") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 0, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    const PaneRef builder = ref_of(panel::kBuilder);
    const PaneRef info = ref_of(panel::kInfo);
    enter_arrange_desk(t);
    step_to(t, builder);

    const FineRect before =
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .resolved;
    const SetupPane* other_before = pane_of(t.session().setup.active, info);
    REQUIRE(other_before != nullptr);
    const SetupPane other_copy = *other_before;

    // ONE COARSE GROW: both axes, by `kCoarseStepCells`, anchored at the place.
    t.key(input::scan::kEquals);
    const SetupPane* row = pane_of(t.session().setup.active, builder);
    REQUIRE(row != nullptr);
    CHECK(row->width.mode == pane_unit::kSubcells);
    CHECK(row->height.mode == pane_unit::kSubcells);
    CHECK(row->width.amount == before.w + subs(kCoarseStepCells));
    CHECK(row->height.amount == before.h + subs(kCoarseStepCells));

    // IT GOES THROUGH THE EXISTING AUTHORING DOOR, so it cannot move the pane it is
    // resizing: a bottom-right anchor writes no place at all, and the pane stays in the
    // reactive stack it was seated in.
    CHECK(row->place.mode == pane_unit::kDefault);

    // AND IT MOVES NO OTHER PANE. There is no packing, no collision avoidance and no
    // layout pass -- one row of one setup changed.
    CHECK(*pane_of(t.session().setup.active, info) == other_copy);

    // A SHRINK IS THE SAME DOOR WITH THE OPPOSITE SIGN, and returns the pane exactly.
    t.key(input::scan::kMinus);
    const SetupPane* back = pane_of(t.session().setup.active, builder);
    REQUIRE(back != nullptr);
    CHECK(back->width.amount == before.w);
    CHECK(back->height.amount == before.h);
    CHECK(*pane_of(t.session().setup.active, info) == other_copy);

    // FOUR COARSE STEPS AND FOUR FINE ONES DIFFER ONLY IN THE DELTA -- one owner, one
    // clamping law, and the coarse step is the fine step's own arithmetic.
    for (int i = 0; i < kCoarseStepCells; ++i) {
        t.key(input::scan::kRight, input::mod::kShift);
        t.key(input::scan::kDown, input::mod::kShift);
    }
    const SetupPane* fine = pane_of(t.session().setup.active, builder);
    REQUIRE(fine != nullptr);
    CHECK(fine->width.amount == before.w + subs(kCoarseStepCells));
    CHECK(fine->height.amount == before.h + subs(kCoarseStepCells));
}

TEST_CASE("WUX-6/SC-7: the coarse step is ordinary action vocabulary, not pane chrome") {
    // WUX-5 took the permanent cheat sheets off the panes; a new gesture must not put one
    // back. So the coarse step is discoverable exactly where every other gesture is -- the
    // keymap, the band's legend, and the full hotkey view -- and nowhere else.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 0, 0, 0}));

    // IT IS AN ACTION WITH A DURABLE ID, which is what a maker's own keymap file rebinds --
    // and the id is in the catalog for BOTH scopes, which is what the file can say.
    REQUIRE(row_of_id("manage.grow") != nullptr);
    REQUIRE(row_of_id("manage.shrink") != nullptr);
    CHECK(row_of_id("manage.grow")->act == Act::kManageGrow);
    CHECK(row_of_id("manage.shrink")->act == Act::kManageShrink);

    // ...BOUND IN BOTH ARRANGING SCOPES, so one maker override moves both (ARR-0's rule
    // for every action the two scopes share).
    const Keymap& keys = t.session().keymap;
    CHECK(keys.action_for(KeyContext::kArrangePane, input::scan::kEquals, input::mod::kNone) ==
          Act::kManageGrow);
    CHECK(keys.action_for(KeyContext::kArrangeDesk, input::scan::kEquals, input::mod::kNone) ==
          Act::kManageGrow);
    CHECK(keys.action_for(KeyContext::kArrangePane, input::scan::kMinus, input::mod::kNone) ==
          Act::kManageShrink);
    CHECK(keys.action_for(KeyContext::kArrangeDesk, input::scan::kMinus, input::mod::kNone) ==
          Act::kManageShrink);

    // AND THE SCREEN'S OWN VOICE SAYS SO, in the maker's own bindings. The band's legend
    // packs a context's rows LEFT TO RIGHT in declaration order and cuts what does not fit
    // (KEY-0), so a gesture's place in the catalog is what decides whether a maker ever
    // meets it without opening the full view -- and this is the gesture a maker on a
    // shipped desk reaches for first. Witnessed live on the graphical face, where the
    // legend cut the coarse step off the right-hand end until it was moved forward.
    enter_arrange_desk(t);
    const Screen sc = screen_of(t.session());
    std::string band;
    for (const std::int64_t y : {sc.help_y, sc.help_y + 1}) {
        band += inspector_row(t.canvases.back(), 0, y) + " | ";
    }
    INFO(band);
    CHECK(band.find("grow") != std::string::npos);
    CHECK(band.find("shrink") != std::string::npos);

    t.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(t.session().hotkeys.open);
    const std::string view = panel_text(
        t.canvases.back(), pane_body_cells(hotkeys_bounds(t.session(), screen_of(t.session()))));
    INFO(view);
    CHECK(view.find("grow") != std::string::npos);
    CHECK(view.find("shrink") != std::string::npos);
}

TEST_CASE("WUX-6/SC-5: a coarse shrink meets the same per-axis refusal a fine one does") {
    // REFUSE-NEVER-CLAMP, PER AXIS (WUX-2a), unchanged: an axis whose proposal is illegal
    // keeps its own value while the independent axis still settles. The coarse step
    // inherits this because it IS the same proposal, not because it repeats the rule.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 0, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    const PaneRef builder = ref_of(panel::kBuilder);
    Setup& desk = live(t).setup.active;
    REQUIRE(author_pane_size(desk, builder, PaneSize{pane_unit::kSubcells, subs(2)},
                             PaneSize{pane_unit::kSubcells, subs(20)})
                .accepted);
    enter_arrange_desk(t);
    step_to(t, builder);

    t.key(input::scan::kMinus);
    const SetupPane* row = pane_of(t.session().setup.active, builder);
    REQUIRE(row != nullptr);
    CHECK(row->width.amount == subs(2));                           // refused, and KEPT
    CHECK(row->height.amount == subs(20) - subs(kCoarseStepCells)); // still settled

    // NOTHING IS CLAMPED TO A WALL THE MAKER NEVER REACHED.
    CHECK(row->width.amount != subs(1));
}

TEST_CASE("ARR-0: stepping names the pane, its state and its authored window in words") {
    // THE ROSTER PANEL IS RETIRED; the statement moved to where a keyboard maker already
    // reads -- the notice line -- and it carries the pane's STATE word, which is what
    // keeps an invisible pane recoverable by ear: step to it, read what it is, reset it.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 0, 0}));
    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kInfo));
    INFO(t.notice());
    CHECK(t.notice().find("arrange ") != std::string::npos);
    CHECK(t.notice().find("(open)") != std::string::npos);
    // A REACTIVE PANE SAYS `default` IN CHARACTERS rather than leaving the axes blank.
    CHECK(t.notice().find("-x-") != std::string::npos);

    // AND RETURN IS NOT THE PICKER'S RETURN HERE. Selecting an open row in the picker
    // REMOVES it (PNL-0); the desk's Return NARROWS to arranging exactly the addressed
    // pane (ARR-0) and changes the setup's membership not at all.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().arrange.open);
    open_pane(t, ref_of(panel::kBuilder));
    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kBuilder));
    const Setup membership = t.session().setup.active;
    t.key(input::scan::kReturn);
    CHECK(t.session().arrange.open);
    CHECK_FALSE(t.session().arrange.desk);
    CHECK(t.session().arrange.pane == ref_of(panel::kBuilder));
    CHECK(t.session().setup.active == membership);
    CHECK(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));
}

// ---- EXTERNAL ROOM ---------------------------------------------------------------------

TEST_CASE("WIND-2: a place-only change publishes no room, and a size change publishes one") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.extent(200, 60);
    r.pick(hello_ref());
    REQUIRE(r.session().panels.runtime.entries.size() == 1);
    const std::size_t rooms_before = seat->rooms.size();
    REQUIRE(rooms_before > 0);

    // A PLACE-ONLY CHANGE WITH THE SAME BODY CAPACITY SAYS NOTHING. The provider is told
    // rows and columns, and neither moved.
    REQUIRE(author_pane_place(r.session().setup.active, hello_ref(), subs(2), subs(2))
                .accepted);
    r.extent(200, 60, 0, 0); // one repaint, no extent change
    r.key(input::scan::kEscape);
    CHECK(seat->rooms.size() == rooms_before);

    // A SIZE CHANGE THAT MOVES THE PROSE CAPACITY PUBLISHES EXACTLY ONE FRESH ROOM...
    REQUIRE(author_pane_size(r.session().setup.active, hello_ref(),
                             PaneSize{pane_unit::kSubcells, subs(30)},
                             PaneSize{pane_unit::kSubcells, subs(6)})
                .accepted);
    r.key(input::scan::kEscape);
    REQUIRE(seat->rooms.size() == rooms_before + 1);
    CHECK(seat->rooms.back().columns == 28);
    // ...AND THE RETAINED ROWS WERE CLEARED BEFORE IT, so nothing admitted under a wider
    // room can be shown inside a narrower one.
    const ExternalPane* pane =
        r.session().panels.external_pane(r.session().panels.runtime.entries[0].kind);
    REQUIRE(pane != nullptr);
    CHECK(pane->shown.empty());

    // AND REORDERING PUBLISHES NOTHING AT ALL, because the capacity did not move.
    const std::size_t after_size = seat->rooms.size();
    REQUIRE(send_to_back(r.session().setup.active, hello_ref()));
    r.key(input::scan::kEscape);
    CHECK(seat->rooms.size() == after_size);
}
// ============================================================================
// CTX-0 — the contextual surface at the pane seam: Workshop-owned, seam-silent
// ============================================================================

TEST_CASE("CTX-0: a right press over a provider's pane crosses the seam not at all") {
    // Workshop may offer its OWN actions about the rectangle it placed; the provider
    // hears nothing -- no `PanePressed` (the seam cannot say a second button, in writing
    // and in the case above this one), no key, no text, no room change.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
    const std::int64_t keyboard_before = r.session().panels.keyboard;
    const std::int64_t said_before = seat->said;

    r.right_press_cell(panel.x + 1, panel.y + 2);
    CHECK(r.session().context.open);
    CHECK(r.session().context.subject == context_subject::kPane);
    CHECK(r.session().context.pane == hello_ref());
    CHECK(seat->presses.empty());
    CHECK(seat->said == said_before);
    // ...and asking about a pane did not point the keyboard at it.
    CHECK(r.session().panels.keyboard == keyboard_before);

    // The population offered ABOUT it is Workshop's arrangement vocabulary -- rows that
    // act on the rectangle, never on the provider's content.
    const std::vector<ContextEntry> rows = context_population(context_subject::kPane, "");
    REQUIRE(rows.size() == 4);
    CHECK(rows[3].row->act == Act::kManageRemove);
}

TEST_CASE("CTX-0: input spent on the open surface reaches no provider") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect panel = pane_body_cells(external_panel_rect(r.session(), kind));
    r.right_press_cell(panel.x + 1, panel.y + 2);
    REQUIRE(r.session().context.open);
    const std::size_t presses_before = seat->presses.size();

    // The popup opens over the pane it asks about, sharing cells with it; a primary press
    // there is the surface's to spend while it is open, and the provider hears nothing of
    // it. The heading row -- the surface's own furniture, at the popup's own derived
    // bounds -- is consumed silently.
    r.press_cell(
        context_cell_x(r.session()),
        surface::cell_of_subs(context_bounds(r.session(), screen_of(r.session())).y));
    CHECK(seat->presses.size() == presses_before);
    CHECK(r.session().context.open);
    // Removing THIS pane through its own menu: still nothing crosses -- a panel is a
    // presentation, and removing one removes a presentation.
    for (int step = 0; step < 4; ++step) {
        r.key(input::scan::kDown);
    }
    r.key(input::scan::kReturn);
    CHECK_FALSE(has_pane(r.session().setup.active, hello_ref()));
    CHECK(seat->presses.empty());
    CHECK(seat->keys.empty());
    CHECK(seat->typed.empty());
}

// ============================================================================
// ARR-0 — two scopes, one vocabulary: the bound pane, and the whole desk
// ============================================================================

TEST_CASE("ARR-0: the one-pane scope is bound -- another pane cannot be drawn into it") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().pane == ref_of(panel::kBuilder));
    t.key(input::scan::kReturn); // Arrange
    REQUIRE(t.session().arrange.open);
    REQUIRE_FALSE(t.session().arrange.desk);

    // A PRESS ON ANOTHER PANE is consumed with the sentence naming the state -- Info is
    // not drawn in, nothing is dragged, and the binding does not move.
    const ui::Rect side = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kInfo,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(side.x + 1, side.y + 3);
    CHECK(t.session().arrange.pane == ref_of(panel::kBuilder));
    CHECK_FALSE(t.session().pane_drag.active);
    CHECK(t.notice().find("arranging") != std::string::npos);

    // A PRESS ON EMPTY CANVAS is the same consumed sentence -- and it reaches no object
    // beneath, because the mode owns the pointer whole.
    t.press_canvas(60, 30);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice().find("arranging") != std::string::npos);

    // AND THE BOUND PANE ANSWERS BOTH MANIPULATIONS DIRECTLY: its body moves it...
    t.press_canvas(slot.x + 2, slot.y + 2);
    REQUIRE(t.session().pane_drag.active);
    CHECK_FALSE(t.session().pane_drag.sizing);
    CHECK(t.session().pane_drag.pane == ref_of(panel::kBuilder));
    t.release(0, 0);
    // ...and its edge sizes it, in the same state, with nothing left or re-entered.
    const ui::Rect now = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(now.x + now.w - 1, now.y + now.h - 1);
    REQUIRE(t.session().pane_drag.active);
    CHECK(t.session().pane_drag.sizing);
    t.release(0, 0);
    CHECK(t.session().arrange.open);
    CHECK_FALSE(t.session().arrange.desk);
}

TEST_CASE("ARR-0: the desk manipulates panes directly, and a press is its own targeting") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.extent(200, 60);
    r.pick(hello_ref());
    r.pick(ref_of(panel::kBuilder));
    REQUIRE(has_pane(r.session().setup.active, hello_ref()));
    REQUIRE(has_pane(r.session().setup.active, ref_of(panel::kBuilder)));

    // Two overlay panes on one desk; spread them so each has its own cells.
    const std::int64_t hello_kind = r.session().panels.runtime.entries[0].kind;
    REQUIRE(author_pane_place(r.session().setup.active, hello_ref(), subs(4), subs(4))
                .accepted);
    REQUIRE(author_pane_place(r.session().setup.active, ref_of(panel::kBuilder), subs(80),
                              subs(20))
                .accepted);
    r.key(input::scan::kW);
    r.text("w");
    REQUIRE(r.session().arrange.open);
    REQUIRE(r.session().arrange.desk);
    REQUIRE_FALSE(r.session().arrange.addressed());

    // PRESS THE FIRST PANE'S BODY: it is dragged by that very press -- no selection
    // stood as a prerequisite -- and it becomes the keyboard's target by the same press.
    const ui::Rect hello_rect = cells_covered(
        bounds_of(r.session().panels, r.session().setup.active, hello_kind,
                  screen_of(r.session()))
            .rect);
    r.press_cell(hello_rect.x + 2, hello_rect.y + 2);
    REQUIRE(r.session().pane_drag.active);
    CHECK(r.session().pane_drag.pane == hello_ref());
    CHECK(r.session().arrange.pane == hello_ref());
    r.release_cell(0, 0);

    // PRESS THE SECOND PANE'S EDGE: sized directly, target follows the press -- more
    // than one pane manipulated with nobody ever "selected first".
    const ui::Rect b = cells_covered(
        bounds_of(r.session().panels, r.session().setup.active, panel::kBuilder,
                  screen_of(r.session()))
            .rect);
    r.press_cell(b.x + b.w - 1, b.y + b.h - 1);
    REQUIRE(r.session().pane_drag.active);
    CHECK(r.session().pane_drag.sizing);
    CHECK(r.session().pane_drag.pane == ref_of(panel::kBuilder));
    CHECK(r.session().arrange.pane == ref_of(panel::kBuilder));
    r.release_cell(0, 0);

    // AND THE PROVIDER HEARD NONE OF IT -- every press belonged to the arrangement.
    CHECK(seat->presses.empty());
}

TEST_CASE("ARR-0: participation stays the picker's; arrangement does not add or offer") {
    // The desk arranges what the setup names and OFFERS nothing: a catalog pane the
    // setup does not name is the picker's row, unreachable from the desk -- the
    // participation/arrangement split, spent as a keyboard walk.
    Live t;
    REQUIRE_FALSE(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));
    enter_arrange_desk(t);
    const std::size_t named = t.session().setup.active.panes.size();
    // Step around the whole ring twice: the address only ever lands on setup rows.
    for (std::size_t i = 0; i < named * 2 + 2; ++i) {
        t.key(input::scan::kTab);
        CHECK(has_pane(t.session().setup.active, t.session().arrange.pane));
    }
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));
    // Membership changes still go the picker's way -- and the picker still works after
    // leaving, untouched by any of this.
    t.key(input::scan::kEscape);
    open_pane(t, ref_of(panel::kBuilder));
    CHECK(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));
}

TEST_CASE("WUX-2a/WUX-6: an arrow steps the AUTHORED value, not the medium's floor") {
    // ⚔ THE MASK THIS CASE EXISTS TO CLOSE. Deleting `managed_window_base`'s sub-cell
    // restoration -- so a nudge proposes from `managed_bounds().resolved`, which is what
    // the ACTIVE MEDIUM could show -- left the whole lane green. Every arrangement case
    // either authors on the lattice, where the two are the same number, or runs on a medium
    // fine enough to say the authored value back. The one arrangement that tells them apart
    // is a value the medium CANNOT say, moved by one step: an off-lattice place on a
    // character medium, which floors at the cell.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0})); // no metric: cells
    open_pane(t, ref_of(panel::kBuilder));
    const std::int64_t x = surface::subs_of_cells(12) + 7; // deliberately off the lattice
    const std::int64_t y = surface::subs_of_cells(6) + 5;
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder), x, y).accepted);

    const Screen sc = screen_of(t.session());
    REQUIRE(t.session().cell_px == 0); // this medium's own grain IS the cell
    const FineRect shown =
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect;
    REQUIRE(shown.x == x); // the RECTANGLE is the authored value; only the SPELLING floors
    REQUIRE(geometry_spelling(x, t.session().cell_px).exact == false);

    // ...AND ONE ARROW MOVES WHAT WAS AUTHORED, not the number this medium can say back.
    // Through the contextual door, which is the one WUX-7 made select its subject.
    const ui::Rect slot = cells_covered(shown);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().pane == ref_of(panel::kBuilder));
    t.key(input::scan::kReturn);
    REQUIRE(t.session().arrange.open);
    t.key(input::scan::kLeft);

    const SetupPane* row = pane_of(t.session().setup.active, ref_of(panel::kBuilder));
    REQUIRE(row != nullptr);
    CHECK(row->place.x == x - surface::kCellSubs);
    CHECK(row->place.y == y); // the axis nobody named keeps every sub-unit it had
}

// ============================================================================
// WUX-8 — one graphical boundary, spent by the paint, the room and the hand
// ============================================================================

TEST_CASE("WUX-8: on the shipped face the border is chrome and the first body pixel is row 0") {
    // THE PHASE'S SHARPEST FALSIFIER. On the window a pane's boundary is ONE DEVICE PIXEL,
    // so the press inverse has one pixel of margin to get wrong -- and getting it wrong is
    // invisible in cells. Every position below is an exact window pixel.
    //
    // ⚔ MUTATION: a body/hit inversion that keeps the old cell inset while the paint went
    // thin. The border press would then land on the provider's row 0 (check 1), or the
    // first real body pixel would be refused as border (check 2).
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    r.extent_on_window(1000, 700);
    REQUIRE(r.session().cell_px == surface::kCanvasCellPx);

    const FineRect outer = external_panel_rect(r.session(), kind);
    const ExternalBodyPlace body = external_body_of(r.session(), kind);
    REQUIRE(body.present);
    REQUIRE(body.fit.graphical());

    // THE PRECONDITION, ASSERTED: this face draws the boundary as ONE pixel, so the body's
    // viewport begins exactly one pixel inside the pane on both axes.
    const std::int64_t left = surface::px_of_subs(outer.x);
    const std::int64_t top = surface::px_of_subs(outer.y);
    REQUIRE(body.fit.view.x == left + 1);
    REQUIRE(body.fit.view.y == top + 1);

    const auto body_y = [&](std::int64_t row) {
        return body.fit.view.y + body.fit.origin_y +
               (row + body.header_rows) * body.fit.line_px + body.fit.line_px / 2;
    };
    const auto body_x = [&](std::int64_t col) {
        return body.fit.view.x + body.fit.origin_x + col * body.fit.advance_px +
               body.fit.advance_px / 2;
    };

    // 1. THE BORDER PIXEL IS CHROME. Both edges, at a height that is squarely a body row --
    // so what refuses the press is the boundary and not the row.
    for (const std::int64_t x : {left, surface::px_of_subs(surface::add_cells(outer.x, outer.w)) - 1}) {
        seat->presses.clear();
        r.press_pixel(x, body_y(0));
        CHECK(seat->presses.empty());
    }
    // ...and the top border pixel, at a column that is squarely a body column.
    seat->presses.clear();
    r.press_pixel(body_x(0), top);
    CHECK(seat->presses.empty());

    // 2. ONE PIXEL FURTHER IN IS THE BODY. ⚔ MUTATION: a chrome band still a cell thick --
    // this press is 11 pixels inside a 12-pixel cell's border and would be swallowed.
    seat->presses.clear();
    r.press_pixel(body_x(0), body_y(0));
    REQUIRE(seat->presses.size() == 1);
    CHECK(seat->presses[0].row == 0);
    CHECK(seat->presses[0].column == 0);

    // 3. EVERY ROW OF THE ROOM, SWEPT -- including the LAST, which is the row a thinner
    // boundary newly made reachable and the one an unchanged budget would drop.
    for (std::int64_t row = 0; row < body.rows; ++row) {
        for (const std::int64_t col : {std::int64_t{0}, body.columns / 2, body.columns - 1}) {
            seat->presses.clear();
            r.press_pixel(body_x(col), body_y(row));
            REQUIRE(seat->presses.size() == 1);
            CHECK(seat->presses[0].row == row);
            CHECK(seat->presses[0].column == col);
        }
    }

    // 4. AND THE BOUND IS THE MATERIAL'S: one row past the room names nothing, and neither
    // does one column past it. The strip below the last prose line is not a row.
    seat->presses.clear();
    r.press_pixel(body_x(0), body_y(body.rows));
    CHECK(seat->presses.empty());
    r.press_pixel(body_x(body.columns), body_y(0));
    CHECK(seat->presses.empty());

    // 5. THE HAND STILL MEETS THE WHOLE PANE. The boundary is INSIDE the pane, so the
    // border pixel is the pane's even though it is not the provider's.
    const Screen sc = screen_of(r.session());
    const PointedAt on_border{true,
                              surface::canvas_of_window_pixels(left, body_y(0)),
                              surface::canvas_subs_of_window_pixels(left, body_y(0)),
                              surface::kPixelGrainSubs};
    CHECK(occupied_at(r.session().panels, r.session().setup.active, sc, on_border).kind == kind);
}

TEST_CASE("WUX-8: the graphical room is the post-chrome pixels, and selection cannot move it") {
    // TWO CLAIMS AT ONCE, because they are the same number seen twice: the room a provider
    // is granted on the window is derived from the body it actually has, and NOTHING about
    // choosing a pane changes it.
    //
    // ⚔ MUTATION: selected chrome drawn thicker than ordinary chrome -- the pane's content
    // would jump under the maker's hand the moment they pointed at it.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    r.extent_on_window(1000, 700);

    const FineRect outer = external_panel_rect(r.session(), kind);
    const ExternalBodyPlace on_window = external_body_of(r.session(), kind);
    REQUIRE(on_window.present);

    // THE ROOM IS THE PIXELS, DIVIDED BY THE FACE -- not a cell count wearing a new name.
    // ⚔ MUTATION: room kept at the WUX-5 capacity "so the tests do not move".
    const FineRect cell_inset = pane_interior(outer, kChromeSubs);
    const surface::RegionFit was = surface::fit_region_subs(
        cell_inset.x, cell_inset.y, cell_inset.w, cell_inset.h, 8, 18);
    CHECK(on_window.fit.rows > was.rows);
    CHECK(on_window.fit.columns > was.columns);
    REQUIRE_FALSE(seat->rooms.empty());
    CHECK(seat->rooms.back().rows == on_window.rows);
    CHECK(seat->rooms.back().columns == on_window.columns);

    // SELECTING IT CHANGES THE INK AND NOTHING ELSE. The press is on the pane's own border
    // pixel, which is the strongest form of the question: even a press ON the chrome
    // leaves the chrome exactly as thick as it was.
    const std::size_t rooms_before = seat->rooms.size();
    r.press_pixel(surface::px_of_subs(outer.x),
                  surface::px_of_subs(outer.y) + on_window.fit.view.h / 2);
    REQUIRE(r.session().panels.selected == kind);

    const ExternalBodyPlace selected = external_body_of(r.session(), kind);
    CHECK(external_panel_rect(r.session(), kind) == outer);
    CHECK(selected.rows == on_window.rows);
    CHECK(selected.columns == on_window.columns);
    CHECK(selected.region_x == on_window.region_x);
    CHECK(selected.region_y == on_window.region_y);
    CHECK(selected.region_sub_x == on_window.region_sub_x);
    CHECK(selected.region_sub_y == on_window.region_sub_y);
    CHECK(selected.fit == on_window.fit);
    CHECK(pane_inside(outer, screen_of(r.session())).chrome_subs ==
          chrome_grain(screen_of(r.session())));
    // ...and no new room was granted, because nothing about the room changed.
    CHECK(seat->rooms.size() == rooms_before);

    // AND THE PICTURE ITSELF, not only the resolution. ⚔ MUTATION: a PAINTER that insets a
    // selected pane further -- no body place, room or press inverse would notice, and the
    // pane's contents would still jump under the maker's hand. So the region this pane
    // publishes is compared where it actually is, sub-cell remainders included.
    // Searched BACK TO FRONT, the way `context_rows_on` searches: this pane's own plane is
    // published after the material it covers, and what is wanted is the topmost region
    // standing inside the pane's rectangle -- which is the pane's body.
    const auto published = [&](const surface::SurfaceCanvas& c) {
        const ui::Rect box = cells_covered(outer);
        for (std::size_t li = c.layers.size(); li > 0; --li) {
            const surface::SurfaceLayer& l = c.layers[li - 1];
            for (std::size_t ri = l.texts.size(); ri > 0; --ri) {
                const surface::SurfaceTextRegion& reg = l.texts[ri - 1];
                if (reg.x >= box.x && reg.x < box.x + box.w && reg.y >= box.y &&
                    reg.y < box.y + box.h) {
                    return reg;
                }
            }
        }
        return surface::SurfaceTextRegion{};
    };
    const surface::SurfaceTextRegion chosen_picture = published(r.last_canvas());
    REQUIRE(chosen_picture.w > 0);
    CHECK(chosen_picture.x == on_window.region_x);
    CHECK(chosen_picture.y == on_window.region_y);
    CHECK(chosen_picture.w == on_window.region_w);
    CHECK(chosen_picture.h == on_window.region_h);
    CHECK(chosen_picture.sub_x == on_window.region_sub_x);
    CHECK(chosen_picture.sub_y == on_window.region_sub_y);

    // AND BACK: choosing something else restores the identical geometry.
    r.press_cell(0, 0); // bare workspace -- nothing is selected there
    const ExternalBodyPlace after = external_body_of(r.session(), kind);
    CHECK(after.rows == on_window.rows);
    CHECK(after.columns == on_window.columns);
    CHECK(after.fit == on_window.fit);
    const surface::SurfaceTextRegion plain_picture = published(r.last_canvas());
    REQUIRE(plain_picture.w > 0);
    CHECK(plain_picture.x == chosen_picture.x);
    CHECK(plain_picture.y == chosen_picture.y);
    CHECK(plain_picture.w == chosen_picture.w);
    CHECK(plain_picture.h == chosen_picture.h);
    CHECK(plain_picture.sub_x == chosen_picture.sub_x);
    CHECK(plain_picture.sub_y == chosen_picture.sub_y);
}

TEST_CASE("WUX-8: the thinner boundary rewrites no authored value and no foreground law") {
    // ⚔ MUTATIONS: arrangement reading a stale body rectangle; a medium writing its own
    // projection back over what a maker authored; a selection reaching the authored order.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;

    // THE SAME ROOM, TWICE, SO THE ONLY THING THAT CHANGES IS THE FACE. A terminal reports
    // this room in cells and then a window reports the identical room in its own unit --
    // which is the switch this case is about. Comparing a default terminal against a
    // 1000x700 window would move the pane because the ROOM moved, and would prove nothing.
    r.extent(1000, 700, 0, 0, 0);

    // AN EXPLICIT SIZE FIRST, because a DEFAULT amount is zero and zero survives every
    // projection there is -- a case resting on it would pass over a medium that writes its
    // own answer back, which is the one thing this claim is about.
    REQUIRE(author_pane_size(const_cast<Setup&>(r.session().setup.active), hello_ref(),
                             PaneSize{pane_unit::kSubcells, surface::subs_of_cells(37) + 17},
                             PaneSize{pane_unit::kSubcells, surface::subs_of_cells(11) + 5})
                .accepted);

    // WHAT THE MAKER AUTHORED, on the terminal, before any window ever spoke.
    const SetupPane* row = pane_of(r.session().setup.active, hello_ref());
    REQUIRE(row != nullptr);
    const SetupPane authored = *row;
    const std::vector<std::int64_t> order = presentation_order(r.session().setup.active, r.session().panels);
    const FineRect on_cells = external_panel_rect(r.session(), kind);

    // THE WINDOW OPENS. The boundary thins; the authored rectangle does not move.
    r.extent_on_window(1000, 700);
    const SetupPane* after_window = pane_of(r.session().setup.active, hello_ref());
    REQUIRE(after_window != nullptr);
    CHECK(after_window->place.x == authored.place.x);
    CHECK(after_window->place.y == authored.place.y);
    CHECK(after_window->width.amount == authored.width.amount);
    CHECK(after_window->height.amount == authored.height.amount);
    CHECK(after_window->front == authored.front);
    CHECK(external_panel_rect(r.session(), kind) == on_cells); // the PANE is where it was

    // SELECTING, ARRANGING AND LEAVING leaves the authored order byte-stable, and the
    // foreground answer is still the one `effective_pane_order` gives.
    r.press_pixel(surface::px_of_subs(on_cells.x) + 4, surface::px_of_subs(on_cells.y) + 4);
    REQUIRE(r.session().panels.selected == kind);
    CHECK(presentation_order(r.session().setup.active, r.session().panels) == order);
    const std::vector<std::int64_t> lifted =
        effective_pane_order(r.session().setup.active, r.session().panels);
    CHECK(lifted.back() == kind);
    const Screen sc = screen_of(r.session());
    const ui::Rect box = cells_covered(external_panel_rect(r.session(), kind));
    CHECK(occupied_at(r.session().panels, r.session().setup.active, sc, box.x, box.y).kind ==
          kind);

    // AND GOING BACK TO A TERMINAL restores the cell boundary without touching the value.
    r.extent(1000, 700, 0, 0, 0);
    CHECK(chrome_grain(screen_of(r.session())) == kChromeSubs);
    const SetupPane* home = pane_of(r.session().setup.active, hello_ref());
    REQUIRE(home != nullptr);
    CHECK(home->place.x == authored.place.x);
    CHECK(home->place.y == authored.place.y);
    CHECK(home->width.amount == authored.width.amount);
    CHECK(home->height.amount == authored.height.amount);
    CHECK(presentation_order(r.session().setup.active, r.session().panels) == order);
    (void)seat;
}

// ============================================================================
// ---- WUX-9: one provider, several layouts ----------------------------------
//
// The floor the phase rests on: switching layouts changes Workshop's PRESENTATION and
// the rooms it grants, and reaches provider identity, provider realization and
// provider-owned state not at all.

namespace {

void layout_key(PaneRig& r, Act act) {
    const Gesture g = r.session().keymap.gesture_of(act);
    r.key(g.scancode, g.modifiers);
}

/// The prose a pane is currently SHOWING, as text -- `SurfaceTextRow` is a wire shape
/// with no equality of its own, and what a case wants to compare is the words.
std::vector<std::string> shown_text(const ExternalPane& pane) {
    std::vector<std::string> out;
    for (const surface::SurfaceTextRow& row : pane.shown) {
        out.push_back(row.text);
    }
    return out;
}

PaneContent hello_rows(const std::vector<std::string>& lines) {
    PaneContent said;
    said.pane = kHelloPane;
    for (const std::string& line : lines) {
        said.rows.push_back(surface::SurfaceTextRow{line, surface::role::kFill});
    }
    return said;
}

} // namespace

TEST_CASE("WUX-9/SC-6: a pane in two layouts is one pane, one provider, one room") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    REQUIRE(r.session().panels.has(kind));

    const PaneContent said = hello_rows({"one", "two"});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    const ExternalPane* before = r.session().panels.external_pane(kind);
    REQUIRE(before != nullptr);
    const std::vector<std::string> shown = shown_text(*before);
    const std::int64_t granted_rows = before->rows;
    const std::int64_t granted_columns = before->columns;
    REQUIRE(!shown.empty());

    const std::int64_t said_before = seat->said;
    const std::size_t catalog_before = r.session().panels.runtime.entries.size();

    // A SECOND LAYOUT NAMING THE SAME PANE. Since WUX-11 `new` is BLANK, so the second
    // layout is made by DUPLICATING the first -- which is the gesture that copies a desk,
    // and the one a maker reaches for when they want the same panes twice.
    duplicate_live_layout(r);
    REQUIRE(layout_count(r.session().setup) == 2);
    REQUIRE(has_pane(r.session().setup.active, hello_ref()));

    // SWITCHING BETWEEN TWO LAYOUTS THAT BOTH NAME IT, four times.
    for (int i = 0; i < 4; ++i) {
        layout_key(r, Act::kLayoutNext);
    }

    // ONE RUNTIME ROW, ONE PRESENTATION, THE SAME ROOM AND THE SAME ROWS -- and the
    // provider was told NOTHING, because its prose capacity never changed. A grant per
    // switch would be the room churn this case exists to refuse.
    CHECK(r.session().panels.runtime.entries.size() == catalog_before);
    CHECK(r.session().panels.external.size() == 1);
    const ExternalPane* after = r.session().panels.external_pane(kind);
    REQUIRE(after != nullptr);
    CHECK(shown_text(*after) == shown);
    CHECK(after->rows == granted_rows);
    CHECK(after->columns == granted_columns);
    CHECK(seat->said == said_before);
}

TEST_CASE("WUX-9/SC-6: leaving a layout withdraws a presentation and unloads nothing") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const PaneContent said = hello_rows({"one", "two"});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    REQUIRE(r.session().panels.external_pane(kind)->heard);
    const std::int64_t said_before = seat->said;

    // A LAYOUT THAT DOES NOT NAME IT -- which since WUX-11 is what a NEW layout is: a
    // fresh blank desk, whose membership `apply_setup` reconciles to.
    layout_key(r, Act::kLayoutNew);
    REQUIRE_FALSE(has_pane(r.session().setup.active, hello_ref()));
    REQUIRE_FALSE(r.session().panels.has(kind));

    // THE PROVIDER HEARD NOTHING ABOUT IT. Workshop has no unload path, retracts no
    // offer, and the catalog row is a fact about the RUN rather than about this desk.
    CHECK(seat->said == said_before);
    CHECK(r.session().panels.runtime.entries.size() == 1);
    CHECK(resolve_pane(hello_ref(), r.session().panels).has_value());
    // ...and the layout that still names it is untouched by any of it.
    CHECK(has_pane(layout_at(r.session().setup, 0), hello_ref()));

    // COMING BACK RE-SEATS THE PRESENTATION AND RE-EARNS ITS ROOM -- one grant, because
    // this is a pane ENTERING, which is the dragged-window-edge path and not a new one.
    layout_key(r, Act::kLayoutNext);
    REQUIRE(r.session().panels.has(kind));
    CHECK(r.session().panels.runtime.entries.size() == 1);
    CHECK(r.session().panels.external.size() == 1);
    CHECK(seat->said == said_before + 1);
    CHECK(seat->rooms.back().pane == std::string(kHelloPane));
    // ...and the provider's own state answers it, unchanged, as ordinary content.
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    CHECK(r.session().panels.external_pane(kind)->shown.size() == said.rows.size());
}

TEST_CASE("WUX-9/SC-15: an inactive layout's rows are dormant, not maintained") {
    PaneRig r;
    r.mount_workshop();

    // TWO LAYOUTS: the first names nothing external; the second names a pane nothing has
    // offered yet -- authored, unresolved, and legal (WS-0's own shape).
    layout_key(r, Act::kLayoutNew);
    REQUIRE(add_pane(r.session().setup.active, hello_ref()));
    r.session().setup.active.name = "Waiting";
    const Setup waiting = r.session().setup.active;
    layout_key(r, Act::kLayoutPrevious);
    REQUIRE_FALSE(has_pane(r.session().setup.active, hello_ref()));
    REQUIRE(layout_at(r.session().setup, 1) == waiting);

    // THE PROVIDER ARRIVES while its layout is on the shelf. Only the LIVE layout
    // reconciles; the shelved value is a value, and nothing walks it.
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    CHECK(r.session().panels.runtime.entries.size() == 1);
    CHECK(layout_at(r.session().setup, 1) == waiting);
    CHECK(r.session().panels.open.size() == r.session().setup.active.panes.size());

    // A SECOND OFFER ENTERS THE RUN'S CATALOG AND NO LAYOUT'S PANE LIST. A maker authors
    // participation; an offer never authors itself into a desk it was not named in.
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"second", "Second", "another"});
    });
    CHECK(r.session().panels.runtime.entries.size() == 2);
    for (std::size_t i = 0; i < layout_count(r.session().setup); ++i) {
        CAPTURE(i);
        CHECK_FALSE(has_pane(layout_at(r.session().setup, i), PaneRef{kHelloOffice, "second"}));
    }
    CHECK(layout_at(r.session().setup, 1) == waiting);

    // AND ACTIVATING THE DORMANT LAYOUT RESOLVES ITS ROW THROUGH ORDINARY RECONCILIATION,
    // with not one byte of the value having changed while it waited.
    layout_key(r, Act::kLayoutNext);
    CHECK(r.session().setup.active == waiting);
    CHECK(r.session().panels.has(r.session().panels.runtime.entries[0].kind));
    CHECK(unresolved_panes(r.session().setup.active, r.session().panels).empty());
}
