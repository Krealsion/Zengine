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
// THIS FILE OWNS: the protocol and the provider seam.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// ============================================================================
// WP-0 — the office authors the pane; Workshop grants the room
// ============================================================================
//
// One dynamically loaded external weave offers a read-only pane; Workshop
// discovers it in either load order, resolves it through the unchanged two-string
// `PaneRef`, lists it in the existing picker, opens it in Workshop-chosen room,
// grants it a prose budget, shows the rows it answers with, and closes it without
// touching the provider.
//
// WHAT EVERY CASE HERE IS ARRANGED AGAINST, and what makes them evidence rather
// than description:
//
//   PROVENANCE      no payload carries a provider. The office is Loom's stamp, so
//                   the negative cases -- personal speech from the actual holder,
//                   a different office, a forged room -- are the cases that say
//                   what the design bought.
//   BOUNDS          a descriptor, a catalog, a row count and a column width each
//                   have a number, and each has a case that exceeds it by one.
//   SILENCE         waiting and unresolved are said; unavailable is never said,
//                   because nothing in this process can prove it.
//   THE REAL SEAM   the Hello provider is a real shared library loaded through the
//                   real Kernel and Manager under an ATTESTED activation. A mock
//                   loader would prove nothing about the ABI, and a registration
//                   hook would prove nothing about the protocol.

// ---- The protocol itself ------------------------------------------------------

TEST_CASE("the pane protocol is four shapes, and none of them carries a provider") {
    // THE ABSENCE IS THE ENFORCEMENT. Workshop derives the provider half of a
    // `PaneRef` from `mail.authored_role()` -- Loom's stamp -- and the only way to
    // guarantee it never reads one off a payload is for there to be no such field
    // to read. So this case walks the declared schemas rather than trusting the
    // struct definitions to stay as they are.
    const std::shared_ptr<const loom::Schema> offered = loom::schema_of<PaneOffered>();
    REQUIRE(offered != nullptr);
    CHECK(offered->name() == "PaneOffered");
    CHECK(offered->version() == 1);
    REQUIRE(offered->fields().size() == 3);
    CHECK(offered->fields()[0].name == "pane");
    CHECK(offered->fields()[1].name == "name");
    CHECK(offered->fields()[2].name == "summary");
    for (const loom::Field& f : offered->fields()) {
        CHECK(f.type.kind == loom::Kind::Text);
        CHECK(f.name != "provider");
        CHECK(f.name != "author");
        CHECK(f.name != "weave");
        CHECK(f.name != "placement");
    }

    const std::shared_ptr<const loom::Schema> content = loom::schema_of<PaneContent>();
    REQUIRE(content != nullptr);
    CHECK(content->name() == "PaneContent");
    CHECK(content->version() == 1);
    REQUIRE(content->fields().size() == 2);
    CHECK(content->fields()[0].name == "pane");
    CHECK(content->fields()[0].type.kind == loom::Kind::Text);
    // THE ROWS ARE `surface::SurfaceTextRow` AND NOT A PARALLEL TYPE, so a
    // provider's row carries the same semantic role and ground every first-party row
    // does and the Skin's palette answers for it unchanged.
    CHECK(content->fields()[1].name == "rows");
    CHECK(content->fields()[1].type.kind == loom::Kind::List);
    REQUIRE(content->fields()[1].type.element != nullptr);
    REQUIRE(content->fields()[1].type.element->message != nullptr);
    CHECK(content->fields()[1].type.element->message->name() == "SurfaceTextRow");
    for (const loom::Field& f : content->fields()) {
        CHECK(f.name != "provider");
    }

    const std::shared_ptr<const loom::Schema> room = loom::schema_of<PaneRoom>();
    REQUIRE(room != nullptr);
    CHECK(room->name() == "PaneRoom");
    CHECK(room->version() == 1);
    REQUIRE(room->fields().size() == 3);
    CHECK(room->fields()[0].name == "pane");
    CHECK(room->fields()[1].name == "rows");
    CHECK(room->fields()[1].type.kind == loom::Kind::Int);
    CHECK(room->fields()[2].name == "columns");
    CHECK(room->fields()[2].type.kind == loom::Kind::Int);
    // NO GEOMETRY OF ANY KIND. A budget of prose, and not a rectangle, a cell, a
    // pixel, an extent, a font or the identity of the medium that answered.
    for (const loom::Field& f : room->fields()) {
        CHECK(f.name != "x");
        CHECK(f.name != "y");
        CHECK(f.name != "width");
        CHECK(f.name != "height");
        CHECK(f.name != "text_advance_px");
        CHECK(f.name != "text_line_px");
    }

    const std::shared_ptr<const loom::Schema> ask = loom::schema_of<PaneCatalogRequested>();
    REQUIRE(ask != nullptr);
    CHECK(ask->name() == "PaneCatalogRequested");
    CHECK(ask->version() == 1);
    CHECK(ask->fields().empty()); // a filter would be a policy nobody asked for
}

TEST_CASE("a pane's content round-trips through the wire with its semantics intact") {
    PaneContent said;
    said.pane = "hello";
    said.rows.push_back(surface::SurfaceTextRow{"first", surface::role::kAccent,
                                                surface::role::kMuted});
    said.rows.push_back(surface::SurfaceTextRow{"second", surface::role::kMuted});
    const loom::Value v = loom::to_value(said);
    const PaneContent back = loom::from_value<PaneContent>(v);
    CHECK(back.pane == "hello");
    REQUIRE(back.rows.size() == 2);
    CHECK(back.rows[0].text == "first");
    CHECK(back.rows[0].role == surface::role::kAccent);
    CHECK(back.rows[0].background == surface::role::kMuted);
    CHECK(back.rows[1].text == "second");
    CHECK(back.rows[1].background == surface::role::kNone);

    PaneOffered offer{"hello", "Hello", "a bounded external greeting"};
    const PaneOffered offer_back = loom::from_value<PaneOffered>(loom::to_value(offer));
    CHECK(offer_back.pane == "hello");
    CHECK(offer_back.name == "Hello");
    CHECK(offer_back.summary == "a bounded external greeting");

    const PaneRoom room_back =
        loom::from_value<PaneRoom>(loom::to_value(PaneRoom{"hello", 7, 46}));
    CHECK(room_back.pane == "hello");
    CHECK(room_back.rows == 7);
    CHECK(room_back.columns == 46);
}

// ---- Descriptor law and the catalog bound ---------------------------------------

TEST_CASE("a valid offer is admitted under the office that stamped it") {
    RuntimeCatalog cat;
    const Admission a = admit_pane_offer(cat, kHelloOffice, good_offer());
    REQUIRE(a.written.accepted);
    CHECK_FALSE(a.refreshed);
    REQUIRE(cat.entries.size() == 1);
    CHECK(cat.entries[0].provider == std::string(kHelloOffice));
    CHECK(cat.entries[0].pane == "hello");
    CHECK(cat.entries[0].name == "Hello");
    CHECK(cat.entries[0].summary == "a bounded external greeting");
    // THE HANDLE IS SESSION-LOCAL AND OUTSIDE THE BUILT-IN NUMBER SPACE, which is
    // what stops it reaching `panel_kind`'s Builder fall-through.
    CHECK(is_runtime_kind(a.kind));
    CHECK(a.kind >= kFirstRuntimeKind);
    CHECK(cat.entries[0].kind == a.kind);
    // AND THE DURABLE IDENTITY IS THE STAMPED OFFICE PLUS THE PAYLOAD'S PANE KEY.
    CHECK(resolve_pane(hello_ref(), cat).value_or(0) == a.kind);
}

TEST_CASE("an offer with no stamped office is refused, and retains nothing") {
    RuntimeCatalog cat;
    const Admission a = admit_pane_offer(cat, "", good_offer());
    CHECK_FALSE(a.written.accepted);
    CHECK(cat.entries.empty());
}

TEST_CASE("a descriptor's name and summary are bounded, and a refusal keeps nothing") {
    RuntimeCatalog cat;
    const auto refused = [&cat](const PaneOffered& o) {
        const std::size_t before = cat.entries.size();
        const Admission a = admit_pane_offer(cat, kHelloOffice, o);
        CHECK_FALSE(a.written.accepted);
        CHECK(cat.entries.size() == before);
        return a.written.refusal;
    };

    CHECK(refused(PaneOffered{"hello", "", "fine"}) == "a pane's name cannot be empty");
    CHECK(refused(PaneOffered{"hello", "   ", "fine"}) ==
          "a pane's name needs more than spaces in it");
    CHECK(refused(PaneOffered{"hello", std::string("Hel\nlo"), "fine"}) ==
          "a pane's name cannot contain control characters");
    CHECK(refused(PaneOffered{"hello", bytes(kMaxPaneNameLen + 1, 'n'), "fine"}) ==
          "a pane's name is at most 32 bytes");
    CHECK(refused(PaneOffered{"hello", "Hello", ""}) == "a pane's summary cannot be empty");
    CHECK(refused(PaneOffered{"hello", "Hello", "  "}) ==
          "a pane's summary needs more than spaces in it");
    CHECK(refused(PaneOffered{"hello", "Hello", std::string("a\x7F" "b")}) ==
          "a pane's summary cannot contain control characters");
    CHECK(refused(PaneOffered{"hello", "Hello", bytes(kMaxPaneSummaryLen + 1, 's')}) ==
          "a pane's summary is at most 64 bytes");

    // THE BOUNDS ARE BYTE COUNTS AND THE LAST ACCEPTED BYTE IS ACCEPTED.
    CHECK(admit_pane_offer(cat, kHelloOffice,
                           PaneOffered{"hello", bytes(kMaxPaneNameLen, 'n'),
                                       bytes(kMaxPaneSummaryLen, 's')})
              .written.accepted);
    REQUIRE(cat.entries.size() == 1);
}

TEST_CASE("a descriptor's two keys are judged by the setup file's own law") {
    RuntimeCatalog cat;
    // THE SAME `check_pane_key` THE PERSISTED GRAMMAR USES, and it is the same
    // function rather than a second one: a runtime key that a saved setup could not
    // spell would be an identity a maker could never keep.
    CHECK(admit_pane_offer(cat, "has space", good_offer()).written.refusal ==
          "a pane reference's provider cannot contain spaces or control characters");
    CHECK(admit_pane_offer(cat, bytes(kMaxPaneKeyLen + 1, 'p'), good_offer()).written.refusal ==
          "a pane reference's provider is at most 64 bytes");
    CHECK(admit_pane_offer(cat, kHelloOffice, PaneOffered{"", "Hello", "fine"})
              .written.refusal == "a pane reference's pane key cannot be empty");
    CHECK(admit_pane_offer(cat, kHelloOffice,
                           PaneOffered{bytes(kMaxPaneKeyLen + 1, 'k'), "Hello", "fine"})
              .written.refusal == "a pane reference's pane key is at most 64 bytes");
    CHECK(cat.entries.empty());
}

TEST_CASE("a runtime offer cannot shadow a built-in pane") {
    RuntimeCatalog cat;
    // Offered by whoever holds `zengine.workshop`, `info` names the row this build
    // compiled in -- and a live message may not move it.
    CHECK(admit_pane_offer(cat, kWorkshopProvider,
                           PaneOffered{pane_key::kInfo, "Not Info", "a forgery"})
              .written.refusal == "`zengine.workshop/info` is a built-in pane");
    CHECK(admit_pane_offer(cat, kWorkshopProvider,
                           PaneOffered{pane_key::kBuilder, "Not Builder", "a forgery"})
              .written.accepted == false);
    CHECK(cat.entries.empty());
    // ...and the built-ins still resolve to themselves.
    CHECK(resolve_pane(PaneRef{kWorkshopProvider, pane_key::kInfo}, cat).value_or(-1) ==
          panel::kInfo);

    // A DIFFERENT OFFICE OFFERING THE SAME PANE KEY IS A DIFFERENT PANE, and is
    // admitted normally -- the `PaneRef` is the PAIR.
    CHECK(admit_pane_offer(cat, kHelloOffice, PaneOffered{pane_key::kInfo, "Info", "theirs"})
              .written.accepted);
    REQUIRE(cat.entries.size() == 1);
    CHECK(is_runtime_kind(cat.entries[0].kind));
}

TEST_CASE("re-offering one reference refreshes it in place and grows nothing") {
    RuntimeCatalog cat;
    const Admission first = admit_pane_offer(cat, kHelloOffice, good_offer());
    REQUIRE(first.written.accepted);
    const std::int64_t handle = first.kind;

    const Admission again =
        admit_pane_offer(cat, kHelloOffice, PaneOffered{"hello", "Hello!", "a better line"});
    CHECK(again.written.accepted);
    CHECK(again.refreshed);
    CHECK(again.kind == handle); // THE HANDLE IS KEPT, so an open pane stays the pane it was
    REQUIRE(cat.entries.size() == 1);
    CHECK(cat.entries[0].name == "Hello!");
    CHECK(cat.entries[0].summary == "a better line");

    // AN INVALID REFRESH LEAVES THE LAST ACCEPTED DESCRIPTOR WHOLE.
    CHECK_FALSE(admit_pane_offer(cat, kHelloOffice, PaneOffered{"hello", "", "x"})
                    .written.accepted);
    REQUIRE(cat.entries.size() == 1);
    CHECK(cat.entries[0].name == "Hello!");
    CHECK(cat.entries[0].summary == "a better line");
}

TEST_CASE("two offices offering one pane key stay two panes, and neither can move the other") {
    RuntimeCatalog cat;
    const Admission a = admit_pane_offer(cat, kHelloOffice, good_offer());
    const Admission b =
        admit_pane_offer(cat, kOtherOffice, PaneOffered{"hello", "Hello", "somebody else's"});
    REQUIRE(a.written.accepted);
    REQUIRE(b.written.accepted);
    CHECK(a.kind != b.kind);
    REQUIRE(cat.entries.size() == 2);

    // Office A refreshing its own row leaves office B's untouched.
    CHECK(admit_pane_offer(cat, kHelloOffice, PaneOffered{"hello", "Hello", "mine, corrected"})
              .refreshed);
    CHECK(cat.entries.size() == 2);
    CHECK(cat.find(kHelloOffice, "hello")->summary == "mine, corrected");
    CHECK(cat.find(kOtherOffice, "hello")->summary == "somebody else's");
    CHECK(resolve_pane(PaneRef{kHelloOffice, "hello"}, cat).value() == a.kind);
    CHECK(resolve_pane(PaneRef{kOtherOffice, "hello"}, cat).value() == b.kind);
}

TEST_CASE("the combined catalog stops at thirty-two entries, built-ins included") {
    RuntimeCatalog cat;
    // THIRTY RUNTIME ROWS, because the two compile-time ones are part of the total.
    for (std::size_t i = 0; i < kMaxPaneCatalogEntries - kPanelKinds; ++i) {
        const Admission a = admit_pane_offer(
            cat, kHelloOffice, PaneOffered{"pane" + std::to_string(i), "P", "one of many"});
        INFO("offer ", i);
        REQUIRE(a.written.accepted);
    }
    CHECK(cat.entries.size() == kMaxPaneCatalogEntries - kPanelKinds);
    CHECK(cat.entries.size() + kPanelKinds == kMaxPaneCatalogEntries);

    // THE THIRTY-THIRD TOTAL ROW IS REFUSED, VISIBLY, AND CHANGES NOTHING.
    const Admission over =
        admit_pane_offer(cat, kHelloOffice, PaneOffered{"one-too-many", "Extra", "over"});
    CHECK_FALSE(over.written.accepted);
    CHECK(over.written.refusal ==
          "Workshop holds at most 32 panes -- `zengine.test.workshop-hello/one-too-many` was "
          "not added");
    CHECK(cat.entries.size() == kMaxPaneCatalogEntries - kPanelKinds);
    CHECK_FALSE(resolve_pane(PaneRef{kHelloOffice, "one-too-many"}, cat).has_value());

    // ...AND AN EXISTING ENTRY MAY STILL BE REFRESHED WHILE FULL. The bound is on how
    // many DISTINCT panes are held, not on how often a provider may correct itself.
    const Admission refresh =
        admit_pane_offer(cat, kHelloOffice, PaneOffered{"pane0", "P0", "corrected"});
    CHECK(refresh.written.accepted);
    CHECK(refresh.refreshed);
    CHECK(cat.entries.size() == kMaxPaneCatalogEntries - kPanelKinds);
    CHECK(cat.find(kHelloOffice, "pane0")->summary == "corrected");
}

TEST_CASE("the runtime catalog is beside the compile-time one and never inside it") {
    // THE BUILT-IN HALF IS MEASURED, NOT COUNTED (WG-1a). This case used to name two rows
    // by hand, call `rows[2]` the first RUNTIME row and close with `CHECK(kPanelKinds == 2)`
    // -- a catalog census, which is the ceremony WG-0 removed from its own tier and this
    // one reintroduced in order to compute an index. What is actually claimed here is an
    // ORDER: every compile-time row, in the catalog's own order, and then the runtime rows
    // AFTER them. A third built-in satisfies that for free, and nothing below is told how
    // many built-ins there are or what any of them is called.
    Panels bare;
    const std::vector<CatalogRow> before = combined_catalog(bare);
    REQUIRE(before.size() == kPanelKinds);
    // THE PRIOR FACT IS ANCHORED IN THE CONSTANT ARRAY rather than in the function under
    // test, so the comparison further down is not `combined_catalog` agreeing with itself.
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        INFO("built-in row ", i);
        CHECK(before[i].kind == kPanelCatalog[i].kind);
        CHECK(before[i].ref == PaneRef{kPanelCatalog[i].provider, kPanelCatalog[i].pane});
        CHECK(before[i].name == kPanelCatalog[i].name);
        CHECK(before[i].summary == kPanelCatalog[i].summary);
        CHECK_FALSE(is_runtime_kind(before[i].kind));
    }

    Panels panels;
    REQUIRE(admit_pane_offer(panels.runtime, kHelloOffice, good_offer()).written.accepted);
    const std::vector<CatalogRow> rows = combined_catalog(panels);
    REQUIRE(rows.size() == kPanelKinds + 1);

    // BUILT-INS FIRST AND UNTOUCHED -- an EXACT PREFIX of what the catalog offered before
    // any offer arrived: same rows, same fields, same order. A runtime offer is not an edit
    // to the constant array, so it cannot replace a built-in row, rewrite one, or be
    // interleaved among them; it can only follow them.
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        INFO("built-in row ", i);
        CHECK(rows[i].kind == before[i].kind);
        CHECK(rows[i].ref == before[i].ref);
        CHECK(rows[i].name == before[i].name);
        CHECK(rows[i].summary == before[i].summary);
        CHECK_FALSE(is_runtime_kind(rows[i].kind));
    }

    // ...THEN THE RUNTIME ROWS, IN FIRST-ACCEPTED-OFFER ORDER, BEGINNING AT `kPanelKinds`.
    CHECK(rows[kPanelKinds].ref == hello_ref());
    CHECK(rows[kPanelKinds].name == "Hello");
    CHECK(rows[kPanelKinds].summary == "a bounded external greeting");
    CHECK(is_runtime_kind(rows[kPanelKinds].kind));
    CHECK(rows[kPanelKinds].kind == panels.runtime.entries[0].kind);

    // AND EXACTLY ONE ROW OF THE COMBINED POPULATION IS A RUNTIME ONE, so `kPanelKinds` is
    // where the runtime TAIL begins rather than merely where one runtime row happens to sit.
    std::size_t runtime_rows = 0;
    for (const CatalogRow& row : rows) {
        if (is_runtime_kind(row.kind)) {
            ++runtime_rows;
        }
    }
    CHECK(runtime_rows == 1);
}

TEST_CASE("the catalog is asked with VIEWS, and only an exact pair is a row") {
    // WP-0a. `RuntimeCatalog::find` takes two `std::string_view`s so that the
    // `PaneContent` door can ask WHO THIS IS with Loom's stamp exactly as it arrived,
    // owning nothing to do it. What it compares against is the row's own string --
    // admitted under `check_pane_key` and owned by the vector -- so the comparison
    // moves no ownership in either direction.
    RuntimeCatalog cat;
    REQUIRE(admit_pane_offer(cat, kHelloOffice, good_offer()).written.accepted);
    REQUIRE(admit_pane_offer(cat, kOtherOffice, PaneOffered{"hello", "Theirs", "theirs"})
                .written.accepted);

    // THE EXACT PAIR, AND IT IS THE PAIR: each office finds its own row and neither
    // finds the other's, which is the identity claim said through the lookup itself.
    const RuntimePane* mine = cat.find(std::string_view(kHelloOffice), std::string_view("hello"));
    REQUIRE(mine != nullptr);
    CHECK(mine->name == "Hello");
    const RuntimePane* theirs = cat.find(std::string_view(kOtherOffice), std::string_view("hello"));
    REQUIRE(theirs != nullptr);
    CHECK(theirs->name == "Theirs");
    CHECK(mine->kind != theirs->kind);

    // A NEAR MISS IS NOTHING, and the two directions of near are both asked: a
    // prefix of a real office, and a real office with an unoffered pane key.
    const std::string almost = std::string(kHelloOffice) + "x";
    CHECK(cat.find(std::string_view(almost), std::string_view("hello")) == nullptr);
    CHECK(cat.find(std::string_view(kHelloOffice), std::string_view("hell")) == nullptr);
    CHECK(cat.find(std::string_view(kHelloOffice), std::string_view("")) == nullptr);
    CHECK(cat.find(std::string_view(""), std::string_view("hello")) == nullptr);

    // AND THE VIEW'S LENGTH IS WHAT IS COMPARED, not a terminator the lookup has no
    // right to assume is there. `window` is `zengine.test.workshop-hello` spelled as
    // a slice of a LONGER buffer, so the byte after the view is a real byte -- and it
    // finds the same row a null-terminated spelling finds.
    const std::string_view window(almost.data(), std::string(kHelloOffice).size());
    REQUIRE(window == std::string_view(kHelloOffice));
    REQUIRE(window.data()[window.size()] == 'x');
    CHECK(cat.find(window, std::string_view("hello")) == mine);
    // ...and the same buffer read one byte longer is the near miss above.
    CHECK(cat.find(std::string_view(almost.data(), window.size() + 1),
                   std::string_view("hello")) == nullptr);
}

TEST_CASE("an unknown runtime reference never becomes the Builder") {
    Panels panels;
    RuntimeCatalog& cat = panels.runtime;
    REQUIRE(admit_pane_offer(cat, kHelloOffice, good_offer()).written.accepted);
    const std::int64_t hello = cat.entries[0].kind;

    // THE NEGATIVE CONTROL THE FALLIBLE DOOR EXISTS FOR, said about a runtime kind.
    CHECK_FALSE(resolve_pane(PaneRef{kHelloOffice, "never-offered"}, cat).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{"nobody", "hello"}, cat).has_value());
    CHECK_FALSE(resolve_builtin_pane(hello_ref()).has_value());

    // AND `placement_of` DOES NOT REACH THE BUILDER'S ROW FOR A RUNTIME KIND. The
    // total lookup still answers Builder for an unknown kind and is still allowed to;
    // what changed is that a runtime handle never gets there.
    CHECK(panel_kind(hello).kind == panel::kBuilder); // the fall-through, still total
    CHECK(placement_of(hello) == placement::kOverlayStack);
    CHECK(placement_of(panel::kInfo) == placement::kSideRegion);
    // ...and the NAME a maker reads is the offered one rather than the fall-through's.
    CHECK(kind_name(panels, hello) == "Hello");
    CHECK(kind_name(panels, panel::kBuilder) == "Builder");
    CHECK(kind_name(panels, 9999).empty());
}

// ---- Provenance: the office authors, and holding is not speaking-for --------------

TEST_CASE("a personal offer from the actual role holder registers nothing") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);

    // THE SHARPEST NEGATIVE THIS PHASE HAS. This weave HOLDS `zengine.test.workshop-hello`
    // at this instant -- Loom would confirm it -- and it speaks with `mail.send_to_role`,
    // which is personal speech. `authored_role()` is empty, so there is no office to
    // derive a `PaneRef`'s provider half from, and the offer is not a fact about
    // anybody's arrangement.
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer_personally(m, good_offer()); });
    CHECK(r.session().panels.runtime.entries.empty());
    CHECK(combined_catalog(r.session().panels).size() == kPanelKinds);

    // THE SAME SENTENCE, DELIBERATELY AUTHORED, IS ADMITTED. One `as_role` apart.
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    REQUIRE(r.session().panels.runtime.entries.size() == 1);
    CHECK(r.session().panels.runtime.entries[0].provider == std::string(kHelloOffice));
}

TEST_CASE("an office longer than the key bound is delivered whole and admitted by nobody") {
    // WP-0a, AND IT IS THE CASE THAT SAYS THE BOUNDARY IS REACHABLE. Loom preserves a
    // role name of any length and proves it does -- its own `R2E-0a/v6` carries one
    // past two hundred bytes across a dynamic seam whole -- so sixty-four bytes is
    // THIS application's law and nothing about the substrate enforces it. The only
    // honest way to ask is to seat a real weave in a real office too long for that
    // law and have it author a perfectly valid offer as itself.
    const std::string long_office = "zengine.test." + std::string(kMaxPaneKeyLen, 'z');
    REQUIRE(long_office.size() > kMaxPaneKeyLen);

    // FIRST, THAT THE SUBSTRATE CARRIES IT WHOLE -- measured, not assumed, and
    // measured where the office actually lands. A watcher holding `zengine.workshop`
    // INSTEAD of Workshop reads the same stamp off the same wire, so if this were a
    // truncation somewhere under Workshop the refusal below would be about the wrong
    // thing entirely.
    {
        PaneRig probe;
        PaneWatcher* watcher = probe.mount_watcher();
        // NOT `far`: it is an empty macro in the Windows SDK's `minwindef.h`, so a
        // variable of that name vanishes mid-declaration under MSVC (MSVC-0).
        ProviderSeat* distant = probe.mount_provider(long_office);
        probe.drive(distant, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
        REQUIRE(watcher->offers.size() == 1);
        REQUIRE(watcher->offer_authors.size() == 1);
        CHECK(watcher->offer_authors[0] == long_office);
        CHECK(watcher->offer_authors[0].size() == long_office.size());
    }

    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(long_office);
    const std::size_t panels_before = r.session().panels.open.size();
    const Setup setup_before = r.session().setup.active;

    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    // AND WORKSHOP HELD ITS OWN LINE.
    CHECK(r.session().panels.runtime.entries.empty()); // NOTHING WAS ADMITTED
    CHECK(combined_catalog(r.session().panels).size() == kPanelKinds);
    CHECK(r.session().panels.open.size() == panels_before); // no panel moved
    CHECK(r.session().setup.active == setup_before);        // and no authored intent did

    // THE REFUSAL IS THE EXISTING PROVIDER-KEY BYTE LAW, in the wording WS-0a fixed.
    CHECK(r.last_notice() == "a pane reference's provider is at most 64 bytes");
    // AND THE UNVALIDATED OFFICE IS NOT IN IT. A notice that echoed the bytes it had
    // just refused would put an unbounded stranger's string on a maker's one line.
    CHECK(r.last_notice().find(long_office) == std::string::npos);
    CHECK(r.last_notice().find("zzzz") == std::string::npos);

    // THE SAME OFFICE, ONE BYTE SHORTER THAN THE BOUND, IS ADMITTED -- so what was
    // refused was the length and not the office being a stranger.
    ProviderSeat* fits = r.mount_provider(std::string_view(long_office.data(), kMaxPaneKeyLen));
    r.drive(fits, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    REQUIRE(r.session().panels.runtime.entries.size() == 1);
    CHECK(r.session().panels.runtime.entries[0].provider.size() == kMaxPaneKeyLen);
}

TEST_CASE("one office cannot overwrite another office's descriptor") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* mine = r.mount_provider(kHelloOffice);
    ProviderSeat* theirs = r.mount_provider(kOtherOffice);

    r.drive(mine, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.drive(theirs, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"hello", "Hello", "somebody else's greeting"});
    });
    const RuntimeCatalog& cat = r.session().panels.runtime;
    REQUIRE(cat.entries.size() == 2);
    CHECK(cat.find(kHelloOffice, "hello")->summary == "a bounded external greeting");
    CHECK(cat.find(kOtherOffice, "hello")->summary == "somebody else's greeting");

    // AND NEITHER CAN REACH THE OTHER'S ROW. The office is stamped by Loom, so the
    // second provider cannot name the first even by trying: there is no field for it.
    r.drive(theirs, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"hello", "Hijacked", "theirs, rewritten"});
    });
    REQUIRE(cat.entries.size() == 2);
    CHECK(cat.find(kHelloOffice, "hello")->name == "Hello");
    CHECK(cat.find(kHelloOffice, "hello")->summary == "a bounded external greeting");
    CHECK(cat.find(kOtherOffice, "hello")->name == "Hijacked");
}

TEST_CASE("personal and wrong-office content cannot alter a valid cache") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* mine = r.mount_provider(kHelloOffice);
    ProviderSeat* theirs = r.mount_provider(kOtherOffice);
    r.drive(mine, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());

    PaneContent real;
    real.pane = kHelloPane;
    real.rows.push_back(surface::SurfaceTextRow{"the true row", surface::role::kFill});
    r.drive(mine, [real](ProviderSeat& s, loom::Mail& m) { s.say(m, real); });

    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ExternalPane* pane = r.session().panels.external_pane(kind);
    REQUIRE(pane != nullptr);
    REQUIRE(pane->heard);
    REQUIRE(pane->shown.size() == 1);
    CHECK(pane->shown[0].text == "the true row");

    // PERSONAL SPEECH from the actual holder changes nothing...
    PaneContent forged;
    forged.pane = kHelloPane;
    forged.rows.push_back(surface::SurfaceTextRow{"a forged row", surface::role::kFill});
    r.drive(mine, [forged](ProviderSeat& s, loom::Mail& m) { s.say_personally(m, forged); });
    CHECK(r.session().panels.external_pane(kind)->shown[0].text == "the true row");

    // ...and neither does another office speaking about this pane key. Its own
    // `PaneRef` is a different one and it has no open pane, so there is nothing for
    // this to land on at all.
    r.drive(theirs, [forged](ProviderSeat& s, loom::Mail& m) { s.say(m, forged); });
    CHECK(r.session().panels.external_pane(kind)->shown[0].text == "the true row");
    CHECK(r.session().panels.runtime.entries.size() == 1);
}

// ---- Discovery, through the REAL dynamic seam -------------------------------------

TEST_CASE("Workshop first, then the provider: an attested activation announces the pane") {
    PaneRig r;
    r.mount_workshop();
    // Workshop asks before anybody can answer. The ask reaches nobody and is gone --
    // nothing is retried, buffered or queued for a provider that does not exist yet.
    r.ready();
    CHECK(r.session().panels.runtime.entries.empty());

    // THE REAL LIBRARY, THROUGH THE REAL KERNEL AND MANAGER. Loom sends the freshly
    // committed incarnation an ATTESTED `zen.Activated`, `ActivationCursor` accepts it,
    // and the provider announces.
    const loom::WeaveId hello =
        r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    REQUIRE(r.load_refusals.empty());
    REQUIRE(hello.valid());
    REQUIRE(r.session().panels.runtime.entries.size() == 1);
    CHECK(r.session().panels.runtime.entries[0].provider == std::string(kHelloOffice));
    CHECK(r.session().panels.runtime.entries[0].pane == std::string(kHelloPane));
    CHECK(r.session().panels.runtime.entries[0].name == "Hello");
    CHECK(r.session().panels.runtime.entries[0].summary == "a bounded external greeting");
}

TEST_CASE("the provider first, then Workshop: the catalog request recovers the lost offer") {
    PaneRig r;
    // THE AWKWARD ORDER, and the one ask-and-announce exists for. The provider's
    // activation offer is addressed to the `zengine.workshop` office, which nobody
    // holds yet -- so it reaches nobody and is gone.
    const loom::WeaveId hello =
        r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    REQUIRE(hello.valid());
    r.mount_workshop();
    CHECK(r.session().panels.runtime.entries.empty()); // the first offer really was lost

    // ...and Workshop's own startup ask brings it back, because the provider verifies
    // the authorship and re-offers.
    r.ready();
    REQUIRE(r.session().panels.runtime.entries.size() == 1);
    CHECK(r.session().panels.runtime.entries[0].name == "Hello");
}

TEST_CASE("asking twice and announcing twice still yields one catalog row") {
    PaneRig r;
    r.mount_workshop();
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    REQUIRE(r.session().panels.runtime.entries.size() == 1);
    const std::int64_t handle = r.session().panels.runtime.entries[0].kind;

    r.ready();
    r.ready();
    r.ready();
    // REPETITION IS HARMLESS BY CONSTRUCTION: identity does the de-duplication, so the
    // protocol needs none of its own.
    REQUIRE(r.session().panels.runtime.entries.size() == 1);
    CHECK(r.session().panels.runtime.entries[0].kind == handle);
    CHECK(combined_catalog(r.session().panels).size() == kPanelKinds + 1);
}

TEST_CASE("the provider answers Workshop and nobody else") {
    PaneRig r;
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.mount_workshop();
    REQUIRE(r.session().panels.runtime.entries.empty());

    // AN UNAUTHENTICATED CATALOG REQUEST -- a root publication, carrying no authored
    // role at all. A provider that answered this would hand its catalog to whoever
    // asked, including a weave holding no office.
    r.publish(loom::to_value(PaneCatalogRequested{}));
    CHECK(r.session().panels.runtime.entries.empty());

    // The same shape, authored as `zengine.workshop`, is answered.
    r.ready();
    CHECK(r.session().panels.runtime.entries.size() == 1);
}

TEST_CASE("a forged room grants the provider nothing") {
    // THE PROVIDER'S OWN CHECK, MEASURED FROM THE OTHER SIDE. A watcher holds
    // `zengine.workshop` so it can author a room deliberately, and can also send one
    // personally -- which is exactly the sentence a weave that merely held the office
    // would produce by reaching for `send_to_role`.
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    REQUIRE(watch->offers.size() == 1); // the activation announcement landed here

    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant_personally(m, kHelloOffice, PaneRoom{kHelloPane, 4, 20});
    });
    CHECK(watch->content.empty()); // no room was believed, so no content was produced

    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, kHelloOffice, PaneRoom{kHelloPane, 4, 20});
    });
    REQUIRE(watch->content.size() == 1);
    REQUIRE(watch->content[0].rows.size() == 4);
    CHECK(watch->content[0].rows[0].text == "hello -- 4x20");
}

// ---- The picker, over the combined population -------------------------------------

TEST_CASE("with no provider the picker is byte-for-byte the picker it was") {
    // THE CONTROL FOR THE WHOLE WINDOWING CHANGE. A population that fits renders
    // exactly as it did before `list_window` was spent here.
    Session before;
    before.panels.picker.open = true;
    surface::SurfaceCanvas c;
    paint_picker(plane(c), before.panels, before.setup.active, screen_of(before),
                 before.keymap);
    const std::string shown = stack_text(c);
    CHECK(shown.find("+ PANEL") != std::string::npos);
    CHECK(shown.find(detail::pad("Builder", kPickerNameCols) + "closed") != std::string::npos);
    CHECK(shown.find(detail::pad("Info", kPickerNameCols) + "open") != std::string::npos);
    CHECK(shown.find("... ") == std::string::npos); // no omission marker at this population
}

TEST_CASE("the combined picker lists an offered pane with its name, summary and state") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    r.key(input::scan::kP);
    const std::string closed = stack_text(r.last_canvas());
    // THROUGH THE ROW'S OWN OWNER, so the case reads what the painter wrote rather than a
    // second spelling of it (WIND-2). At the minimum composition this row is two cells
    // longer than the slot -- WIND-2's state column carries `unresolved`, which is ten bytes
    // where the old one held eight -- so the summary is FITTED and the cut is MARKED. That is
    // `detail::fit` doing exactly its job, and asserting the unfitted string here would be a
    // case measuring a row nobody paints.
    const ui::Rect slot = pane_body_cells(picker_bounds(screen_of(r.session())));
    CHECK(closed.find(detail::fit(
              "  " + picker_entry_text("Hello", "closed", "a bounded external greeting"),
              slot.w)) != std::string::npos);
    CHECK(closed.find(detail::kElided) != std::string::npos);

    r.key(input::scan::kEscape);
    r.pick(hello_ref());
    r.key(input::scan::kP);
    const std::string open = stack_text(r.last_canvas());
    CHECK(open.find(detail::pad("Hello", kPickerNameCols) + "open") != std::string::npos);
    r.key(input::scan::kEscape);

    // AND SELECTING IT AGAIN REMOVES IT -- the picker is still the one owner of
    // presence, and an external pane earns no second door.
    r.pick(hello_ref());
    CHECK_FALSE(has_pane(r.session().setup.active, hello_ref()));
    r.key(input::scan::kP);
    CHECK(stack_text(r.last_canvas()).find(detail::pad("Hello", kPickerNameCols) + "closed") != std::string::npos);
}

TEST_CASE("a picker population larger than its rows is windowed, not truncated") {
    // THE POPULATION IS THE CATALOG'S OWN CAPACITY, NOT A CENSUS (WG-1a). This case used
    // to be written around "the two built-ins plus eighteen offers", with every cursor,
    // marker count and conservation sum computed by hand from that twenty. That is the
    // same defect as `kPanelKinds == 2` one tier over: an unrelated new built-in moves the
    // arithmetic and reddens a law that is still perfectly true. So the offers fill the
    // combined catalog to `kMaxPaneCatalogEntries` instead -- a later built-in simply takes
    // one runtime row's place and the total does not move.
    Panels panels;
    for (std::size_t i = 0; i < kMaxPaneCatalogEntries - kPanelKinds; ++i) {
        REQUIRE(admit_pane_offer(panels.runtime, kHelloOffice,
                                 PaneOffered{"p" + std::to_string(i),
                                             "Pane" + std::to_string(i), "one of many"})
                    .written.accepted);
    }
    const std::vector<CatalogRow> catalog = combined_catalog(panels);
    const std::size_t total = catalog.size();
    REQUIRE(total == kMaxPaneCatalogEntries);

    panels.picker.open = true;
    const Screen sc = kMinScreen;
    const ui::Rect box = pane_body_cells(picker_bounds(sc));
    const std::size_t budget = static_cast<std::size_t>(box.h - 1);
    // A CAPACITY FACT AND NOT A CATALOG ONE. The picker asks for the stack's first slot,
    // spends one cell on every side for the boundary it draws (WUX-5) and its top row on
    // the heading; six is what this composition leaves for the list. It is asserted because
    // everything below is derived from it, and it moves only when the composition does.
    // Before WP-0 the picker's height was `1 + kPanelKinds` -- a catalog census standing in
    // for a capacity, right until a catalog could outgrow the box.
    REQUIRE(budget == 6);

    // THE WINDOW RULES, STATED HERE rather than borrowed from `list_window`: an expected
    // value computed by the function under test is not an expectation. A budget of `budget`
    // rows shows `budget - 1` names beside ONE marker and `budget - 2` between TWO, because
    // a marker is paid for OUT of the budget rather than added beneath it; the window is a
    // contiguous run of the catalog's order; and every omitted row is counted on the side it
    // was omitted on.
    const std::size_t one_marker = budget - 1;
    const std::size_t two_markers = budget - 2;
    const std::size_t tail_first = total - one_marker;
    // ...AND THE TWO-MARKER BRANCH MUST BE REACHABLE, which needs a cursor neither end's
    // window can reach. If a future catalog capacity or picker height ever left no such
    // cursor, this fails LOUDLY here rather than quietly testing a single-marker window
    // twice: at a budget of eight it wants a population above fourteen.
    REQUIRE(one_marker < tail_first);
    const std::size_t mid_cursor = (one_marker + tail_first) / 2;

    const auto rows_of = [&](std::size_t cursor) {
        panels.picker.cursor = cursor;
        surface::SurfaceCanvas c;
        paint_picker(plane(c), panels, setup_for(panels), sc, Keymap{});
        std::vector<std::string> out;
        // THROUGH THE REAL CELL PROJECTION (TYPE-0): the picker is one bounded region now,
        // so what a maker reads at a cell is `project_text_regions`' answer and not a
        // label the painter wrote. The rows are byte-for-byte the ones it used to write.
        for (const surface::SurfaceLabel& l : cell_text_of(c)) {
            // THE PICKER DID NOT GET TALLER, asserted at every cursor this case takes --
            // markers coming out of the budget rather than being added beneath it is
            // exactly what would fail here.
            CHECK(l.y >= box.y);
            CHECK(l.y < box.y + box.h);
            out.push_back(l.text);
        }
        return out;
    };
    const auto earlier_marker = [](std::size_t n) {
        return "... " + std::to_string(n) + " earlier";
    };
    const auto more_marker = [](std::size_t n) { return "... " + std::to_string(n) + " more"; };
    // WHAT THIS CASE MEANS BY A WINDOW: `count` names from the catalog's order beginning at
    // `first`, on consecutive rows from `line0`, with the cursor's row marked `> ` and every
    // other `  ` -- and the cursor INSIDE that run, which is the rule a truncating list
    // breaks.
    const auto check_window = [&](const std::vector<std::string>& out, std::size_t line0,
                                  std::size_t first, std::size_t count, std::size_t cursor) {
        CHECK(cursor >= first);
        CHECK(cursor < first + count);
        for (std::size_t k = 0; k < count; ++k) {
            const std::size_t at = first + k;
            INFO("catalog row ", at, " on picker line ", line0 + k);
            CHECK(out[line0 + k].find((at == cursor ? "> " : "  ") +
                                      detail::pad(catalog[at].name, kPickerNameCols)) == 0);
        }
        std::size_t marked = 0;
        for (const std::string& row : out) {
            if (row.rfind("> ", 0) == 0) {
                ++marked;
            }
        }
        CHECK(marked == 1);
    };

    // AT THE HEAD: no `earlier` marker, one `more` marker on the budget's last row, and the
    // window anchored at the top -- `one_marker` names beginning at the first.
    const std::vector<std::string> head = rows_of(0);
    REQUIRE(head.size() == static_cast<std::size_t>(box.h));
    check_window(head, 1, 0, one_marker, 0);
    CHECK(head[1 + one_marker].find(more_marker(total - one_marker)) != std::string::npos);
    for (const std::string& row : head) {
        CHECK(row.find("earlier") == std::string::npos);
    }
    CHECK(0 + one_marker + (total - one_marker) == total);

    // AT THE TAIL: an `earlier` marker on the first row, no `more`, and the window anchored
    // at the bottom -- `one_marker` names ending on the last of the population.
    const std::vector<std::string> tail = rows_of(total - 1);
    REQUIRE(tail.size() == static_cast<std::size_t>(box.h));
    CHECK(tail[1].find(earlier_marker(tail_first)) != std::string::npos);
    check_window(tail, 2, tail_first, one_marker, total - 1);
    for (const std::string& row : tail) {
        CHECK(row.find(" more") == std::string::npos);
    }
    CHECK(tail_first + one_marker + 0 == total);

    // IN THE MIDDLE: both walls are real so both are said, both counts are exact, and both
    // markers are paid for out of the same budget rather than added beneath it. The window
    // is the earliest run that reaches the cursor, so the cursor is its LAST name.
    const std::size_t mid_first = mid_cursor + 1 - two_markers;
    const std::size_t mid_after = total - mid_first - two_markers;
    const std::vector<std::string> mid = rows_of(mid_cursor);
    REQUIRE(mid.size() == static_cast<std::size_t>(box.h));
    CHECK(mid[1].find(earlier_marker(mid_first)) != std::string::npos);
    check_window(mid, 2, mid_first, two_markers, mid_cursor);
    CHECK(mid[1 + 1 + two_markers].find(more_marker(mid_after)) != std::string::npos);
    // CONSERVATION: omitted-before + shown + omitted-after is the WHOLE population, and the
    // box is still `box.h` rows. That is `list_window`'s third rule and the reason the
    // picker did not grow to fit what it could not show.
    CHECK(mid_first + two_markers + mid_after == total);
}

TEST_CASE("the picker cursor is bounded by the combined population") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    r.key(input::scan::kP);
    for (int i = 0; i < 10; ++i) {
        r.key(input::scan::kDown);
    }
    // THREE ROWS NOW, and the cursor stops on the last of them rather than on the last
    // BUILT-IN, which is what `kPanelKinds` would have bounded it to.
    CHECK(r.session().panels.picker.cursor == kPanelKinds);
    CHECK(combined_catalog(r.session().panels).size() == kPanelKinds + 1);
}

// ---- Setup resolution: an unchanged reference, resolved later ---------------------

TEST_CASE("an authored external reference is unresolved until its office offers it") {
    PaneRig r;
    r.mount_workshop();
    // The maker authored this before any provider existed -- which is exactly the
    // shape WS-0 made legal and WP-0 finally has a consumer for.
    REQUIRE(add_pane(r.session().setup.active, hello_ref()));
    r.session().setup.on_file = r.session().setup.active;
    r.key(input::scan::kP);
    r.key(input::scan::kEscape); // a repaint, so the setup line is current

    CHECK(unresolved_panes(r.session().setup.active, r.session().panels.runtime).size() == 1);
    CHECK(setup_status_text(r.session().setup, "", r.session().panels.runtime,
                            r.session().keymap)
              .find("1 unresolved") != std::string::npos);
    CHECK_FALSE(r.session().panels.has(kFirstRuntimeKind));

    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    // THE SAME UNCHANGED REFERENCE NOW RESOLVES, and reconciliation opened it through
    // the one path -- the file was not touched and no second door exists.
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    CHECK(r.session().setup.active.panes.back().ref == hello_ref());
    CHECK(r.session().panels.has(kind));
    CHECK(unresolved_panes(r.session().setup.active, r.session().panels.runtime).empty());
    // AND A PANE A MAKER CAN SEE IS NOT COUNTED AS UNRESOLVED BENEATH IT.
    CHECK(setup_status_text(r.session().setup, "", r.session().panels.runtime,
                            r.session().keymap)
              .find("unresolved") == std::string::npos);
    // THE SETUP IS STILL SAVED: resolving is not an edit.
    CHECK(r.session().setup.saved());
}

TEST_CASE("a fresh session with no provider leaves the same reference unresolved again") {
    // A SECOND PROCESS, spelled the only way a suite can: a second everything. The
    // runtime catalog is session state and earns nothing from the last run.
    Setup saved;
    saved.name = "Future";
    REQUIRE(add_pane(saved, ref_of(panel::kInfo)));
    REQUIRE(add_pane(saved, hello_ref()));
    REQUIRE(check_setup(saved).accepted);

    PaneRig fresh;
    fresh.mount_workshop();
    fresh.session().setup.active = saved;
    fresh.session().setup.on_file = saved;
    CHECK(fresh.session().panels.runtime.entries.empty());
    const std::vector<PaneRef> waiting =
        unresolved_panes(saved, fresh.session().panels.runtime);
    REQUIRE(waiting.size() == 1);
    CHECK(waiting[0] == hello_ref());
    // NOT DROPPED, NOT REMAPPED, AND NOT CALLED UNAVAILABLE. Workshop knows it has no
    // row for the reference and knows nothing at all about whoever could present it.
    CHECK(fresh.session().setup.active.panes.size() == 2);
    CHECK(fresh.session().setup.active.panes[1].ref == hello_ref());
    CHECK(setup_status_text(fresh.session().setup, "", fresh.session().panels.runtime,
                            fresh.session().keymap)
              .find("unavailable") == std::string::npos);
}

TEST_CASE("setup bytes carry no descriptor, room or handle") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    PaneContent said;
    said.pane = kHelloPane;
    said.rows.push_back(surface::SurfaceTextRow{"cached prose", surface::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    REQUIRE(r.session().panels.external_pane(r.session().panels.runtime.entries[0].kind)->heard);

    const std::string text = setup_persist::to_text(r.session().setup.active);
    // THE FILE IS THE AUTHORED REFERENCE AND THE AUTHORED WINDOW, and nothing else. WP-0's
    // claim was that a live offer's descriptor, its granted room and its session handle
    // reach no byte of it; WIND-2 added authored INTENT beside the reference and did not
    // move that line at all -- which is what the assertions below still measure.
    CHECK(text.find("\"zengine-workshop-setup\"") != std::string::npos);
    CHECK(text.find("\"version\"") != std::string::npos);
    CHECK(text.find(kHelloOffice) != std::string::npos);
    CHECK(text.find("\"hello\"") != std::string::npos);
    // ...AND NOT ONE BYTE OF WHAT THIS SESSION LEARNED AT RUNTIME.
    CHECK(text.find("\"Hello\"") == std::string::npos); // the display name is not saved
    CHECK(text.find("a bounded external greeting") == std::string::npos);
    CHECK(text.find("cached prose") == std::string::npos);
    CHECK(text.find("rows") == std::string::npos);
    CHECK(text.find("columns") == std::string::npos);
    CHECK(text.find(std::to_string(kFirstRuntimeKind)) == std::string::npos);

    // SAVE -> LOAD -> SAVE IS BYTE-IDENTICAL, with an external reference in it.
    const setup_persist::LoadedSetup back = setup_persist::from_text(text);
    REQUIRE(back.outcome.accepted);
    CHECK(back.setup == r.session().setup.active);
    CHECK(setup_persist::to_text(back.setup) == text);
    // AND THE VERSION IS THE ONE THIS BUILD READS AND WRITES, said here because this case
    // is where an external reference meets the file.
    CHECK(setup_persist::kFormatVersion == 3);
}

// ---- Runtime spatial capacity -----------------------------------------------------

TEST_CASE("the overlay floor is the workspace's own bottom, which is the band's top row") {
    // THE BOUNDARY, STATED IN BOTH SPELLINGS AND MEASURED AGAINST THE COMPOSITION. A slot
    // allowed past it erases the row the tool speaks in -- the setup line's row before
    // QR-14 moved the identity to the top band, and the NOTICE's row since.
    for (std::int64_t h : {22, 23, 24, 30, 40, 60}) {
        const Screen sc = screen_of(78, h);
        INFO("height ", h);
        CHECK(kWorkspaceY + sc.room_h == sc.notice_y);
        const std::size_t fits = stack_slots_that_fit(sc);
        for (std::size_t slot = 0; slot < fits; ++slot) {
            const ui::Rect b = placement_bounds(placement::kOverlayStack, slot, sc);
            CHECK(b.y + b.h <= kWorkspaceY + sc.room_h);
        }
        const ui::Rect over = placement_bounds(placement::kOverlayStack, fits, sc);
        CHECK(over.y + over.h > kWorkspaceY + sc.room_h);
    }
    CHECK(stack_slots_that_fit(kMinScreen) == 1);
    CHECK(stack_slots_that_fit(screen_of(78, 42)) >= 2);
}

TEST_CASE("a second overlay at the minimum screen is refused before it reaches Panels::open") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t hello = r.session().panels.runtime.entries[0].kind;
    REQUIRE(r.session().panels.has(hello));
    const Setup before = r.session().setup.active;

    // The Builder is placed in the same stack, and there is room for one slot.
    r.pick(ref_of(panel::kBuilder));
    CHECK_FALSE(r.session().panels.has(panel::kBuilder));
    // THE REFUSAL IS VISIBLE...
    CHECK(r.last_notice().find("no room for Builder") != std::string::npos);
    // ...AND IT DID NOT MUTATE THE AUTHORED SETUP. A picker that added first and read
    // `waiting` afterwards would have authored a pane the maker never saw.
    CHECK(r.session().setup.active == before);
    CHECK_FALSE(has_pane(r.session().setup.active, ref_of(panel::kBuilder)));
    // NO PANEL INTERSECTS THE SETUP ROW OR THE BOTTOM BAND.
    const Screen sc = screen_of(r.session());
    for (const Panel& p : r.session().panels.open) {
        const ui::Rect b =
cells_covered(bounds_of(r.session().panels, r.session().setup.active, p.kind, sc).rect);
        INFO("kind ", p.kind);
        CHECK(b.y + b.h <= kWorkspaceY + sc.room_h);
        CHECK(b.y + b.h <= sc.notice_y); // the band's first row is not the panel's
    }
}

TEST_CASE("an oversubscribed authored setup keeps the extra reference, waiting for room") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);

    // AUTHORED FIRST, THEN OFFERED -- the order a restored file meets a provider that
    // loads afterwards, and the one the picker cannot produce (it refuses to author a
    // pane it could not seat). The offer's own admission runs the ONE reconciliation
    // path, so nothing here reaches past a message boundary to open anything.
    Setup both = r.session().setup.active;
    (void)add_pane(both, ref_of(panel::kBuilder));
    (void)add_pane(both, hello_ref());
    r.session().setup.active = both;
    r.session().setup.on_file = both;
    ProviderSeat* seat2 = r.mount_provider(kOtherOffice);
    (void)seat2;
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    const std::int64_t hello = r.session().panels.runtime.entries[0].kind;
    CHECK(r.session().panels.has(panel::kBuilder));  // first come, first served
    CHECK_FALSE(r.session().panels.has(hello));
    CHECK(r.session().panels.waiting(hello));
    // NOT UNRESOLVED: this build knows exactly what it would draw.
    CHECK(unresolved_panes(r.session().setup.active, r.session().panels.runtime).empty());
    // AND THE AUTHORED REFERENCE IS UNTOUCHED -- authored validity does not depend on
    // extent, so a setup legal on a tall screen is legal on a short one.
    CHECK(has_pane(r.session().setup.active, hello_ref()));
    CHECK(check_setup(r.session().setup.active).accepted);
    CHECK(r.session().setup.saved());

    // THE PICKER SAYS `waiting`, WHICH IS NEITHER `open` NOR `closed`.
    r.key(input::scan::kP);
    const std::string shown = stack_text(r.last_canvas());
    CHECK(shown.find(detail::pad("Hello", kPickerNameCols) + "waiting") != std::string::npos);
    CHECK(shown.find(detail::pad("Builder", kPickerNameCols) + "open") != std::string::npos);
    r.key(input::scan::kEscape);

    // GROWTH OPENS IT, with no gesture at all.
    r.extent(78, 42);
    CHECK(r.session().panels.has(hello));
    CHECK_FALSE(r.session().panels.waiting(hello));

    // ...AND A SHRINK CLOSES THE PRESENTATION, DESTROYS ITS CACHE AND RETAINS THE REF.
    PaneContent said;
    said.pane = kHelloPane;
    said.rows.push_back(surface::SurfaceTextRow{"present", surface::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    REQUIRE(r.session().panels.external_pane(hello) != nullptr);
    r.extent(78, 22);
    CHECK_FALSE(r.session().panels.has(hello));
    CHECK(r.session().panels.external_pane(hello) == nullptr); // the copy is gone
    CHECK(has_pane(r.session().setup.active, hello_ref()));    // the intent is not
    CHECK(r.session().panels.runtime.of_kind(hello) != nullptr); // nor is the catalog row
}

TEST_CASE("selecting a waiting row removes the intent, exactly as selecting an open one does") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    Setup both = r.session().setup.active;
    (void)add_pane(both, ref_of(panel::kBuilder));
    (void)add_pane(both, hello_ref());
    r.session().setup.active = both;
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    const std::int64_t hello = r.session().panels.runtime.entries[0].kind;
    REQUIRE(r.session().panels.waiting(hello));

    r.pick(hello_ref());
    // THE MAKER AUTHORED IT; WHETHER THIS SCREEN CAN SEAT IT IS WORKSHOP'S PROBLEM AND
    // NOT A REASON TO MAKE THE INTENT UNREMOVABLE.
    CHECK_FALSE(has_pane(r.session().setup.active, hello_ref()));
    CHECK_FALSE(r.session().panels.waiting(hello));
    r.key(input::scan::kP);
    CHECK(stack_text(r.last_canvas()).find(detail::pad("Hello", kPickerNameCols) + "closed") != std::string::npos);
}

// ---- The room contract ------------------------------------------------------------

TEST_CASE("opening an external pane grants exactly the fit_region room, authored as Workshop") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());

    REQUIRE(seat->rooms.size() == 1);
    CHECK(seat->rooms[0].pane == std::string(kHelloPane));
    // AUTHORED AS `zengine.workshop` -- which is what lets the provider verify the ask.
    REQUIRE(seat->room_authors.size() == 1);
    CHECK(seat->room_authors[0] == std::string(kWorkshopProvider));

    // THE NUMBERS ARE `fit_region`'S AND NOBODY MULTIPLIES A METRIC. At the minimum
    // composition the pane's region is the WHOLE of the first overlay slot, and the header
    // row is subtracted from the PROSE the medium fits in it rather than from the cells
    // (TYPE-0). In a character medium the two spellings answer the same number: nine cells
    // of slot is nine rows, less one for the header, is the eight the provider always had.
    const Screen sc = screen_of(r.session());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect panel =
cells_covered(bounds_of(r.session().panels, r.session().setup.active, kind, sc).rect);
    CHECK(panel == ui::Rect{0, 2, 48, 9});
    const ExternalBodyPlace body = external_body_place(
        fine_of_cells(panel), sc,
        external_title_rows(r.session().panels, kind, r.session().pane_titles));
    // THE ROOM IS THE PANE'S INTERIOR SINCE WUX-5: the rectangle the placement path gives
    // it is unchanged, and the one cell of visible boundary on every side comes off before
    // the provider is told what it has -- the same reservation the header already was.
    const ui::Rect inside = pane_body_cells(panel);
    CHECK(inside == ui::Rect{1, 3, 46, 7});
    CHECK(body.region_x == inside.x);
    CHECK(body.region_y == inside.y);
    CHECK(body.region_w == inside.w);
    CHECK(body.region_h == inside.h);
    const surface::RegionFit fit = surface::fit_region(inside.x, inside.y, inside.w, inside.h,
                                                       sc.text_advance_px, sc.text_line_px);
    CHECK(seat->rooms[0].rows == fit.rows - kExternalHeaderRows);
    CHECK(seat->rooms[0].columns == fit.columns);
    CHECK(seat->rooms[0].rows == 6);
    CHECK(seat->rooms[0].columns == 46);
}

TEST_CASE("an unchanged prose capacity sends no second room; a changed one sends exactly one") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    REQUIRE(seat->rooms.size() == 1);

    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;

    // REPAINTS ALONE SAY NOTHING. The room is a fact about the screen, not about how
    // many frames were drawn.
    r.key(input::scan::kP);
    r.key(input::scan::kEscape);
    r.ready();
    CHECK(seat->rooms.size() == 1);

    // A LARGER SCREEN DOES SAY SOMETHING NOW, AND EXACTLY ONCE (WIND-1). This block used
    // to assert the opposite, on the strength of a sentence that has stopped being true: an
    // overlay slot's rectangle was `kStackW` by `kStackRows` at every extent, so only a text
    // metric could move an external pane's budget. A slot now takes half the room's surplus,
    // so a wider surface moves the body's COLUMNS -- 100 columns of surface is a room of 70,
    // a surplus of 22, and a slot of 59 -- and the grant follows it through the same
    // `fit_region` call. The taller half of the resize still changes nothing: the slot's
    // height is `kStackRows` at every extent and the header still takes one row of it.
    r.extent(100, 40);
    CHECK(screen_of(r.session()).w == 100);
    CHECK(bounds_of(r.session().panels, r.session().setup.active, kind, screen_of(r.session())).rect ==
          fine_of_cells(ui::Rect{0, 2, 59, 9}));
    REQUIRE(seat->rooms.size() == 2);
    CHECK(seat->rooms[1].rows == 6);       // unchanged: the rows are the slot's interior's
    CHECK(seat->rooms[1].columns == 57);   // moved: the columns are the room's share, inside
    CHECK(seat->room_authors[1] == std::string(kWorkshopProvider));

    // ...and saying it again is not a second answer. The same extent resolves the same
    // capacity, and an unchanged capacity is noise a provider would have to parse.
    r.extent(100, 40);
    r.key(input::scan::kP);
    r.key(input::scan::kEscape);
    CHECK(seat->rooms.size() == 2);

    // A TALLER SCREEN ALONE STILL SAYS NOTHING, which is the half of the old claim that
    // survived: the slot's height is `kStackRows` whatever the surface does.
    r.extent(100, 52);
    CHECK(bounds_of(r.session().panels, r.session().setup.active, kind, screen_of(r.session())).rect ==
          fine_of_cells(ui::Rect{0, 2, 59, 9}));
    CHECK(seat->rooms.size() == 2);

    // A TEXT METRIC MOVES IT TOO: the same cells, set in a real face, hold fewer rows and
    // more columns. Said exactly once, and through the same one call.
    r.extent(100, 40, 9, 18);
    REQUIRE(seat->rooms.size() == 3);
    const Screen typed = screen_of(r.session());
    CHECK(typed.text_advance_px == 9);
    const ui::Rect graphical = external_body_rect(r.session(), kind);
    CHECK(graphical.w == 57); // the widened body's INTERIOR, before the face is consulted
    const surface::RegionFit gfit = surface::fit_region(graphical.x, graphical.y, graphical.w,
                                                        graphical.h, 9, 18);
    CHECK(gfit.graphical());
    CHECK(seat->rooms[2].rows == gfit.rows - kExternalHeaderRows);
    CHECK(seat->rooms[2].columns == gfit.columns);
    CHECK(seat->rooms[2].rows != seat->rooms[1].rows);       // a real face fits fewer rows
    CHECK(seat->rooms[2].columns != seat->rooms[1].columns); // ...and more of them across

    // ...and repeating that exact metric says nothing at all.
    r.extent(100, 40, 9, 18);
    CHECK(seat->rooms.size() == 3);
}

TEST_CASE("a new room clears the old rows before it is sent") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;

    PaneContent said;
    said.pane = kHelloPane;
    said.rows.push_back(surface::SurfaceTextRow{
        std::string(static_cast<std::size_t>(external_body_of(r.session(), kind).columns), 'w'),
        surface::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    REQUIRE(r.session().panels.external_pane(kind)->shown.size() == 1);

    // A WIDER SURFACE (WIND-1). The cached row was admitted under 48 columns and 8 rows;
    // a room of 90 gives the slot 69, so the new grant is a different shape and keeping the
    // old rows would put material admitted under one budget into another -- the one thing
    // this design must not do. Until WIND-1 an extent could not do this at all and the
    // METRIC was the only lever; the metric half is measured immediately below.
    r.extent(120, 40);
    const ExternalPane* wider = r.session().panels.external_pane(kind);
    REQUIRE(wider != nullptr);
    CHECK(wider->columns == 67);
    CHECK(wider->rows == 6);
    CHECK(wider->shown.empty());
    CHECK_FALSE(wider->heard);
    CHECK(wider->awaiting);
    // AND THE PANE SAYS THE SENTENCE IT HAS ALWAYS SAID WHILE IT WAITS. `waiting` is a fact
    // about THIS PANEL -- a room has been granted and nothing valid has answered it -- and
    // a wider window is not news about the provider.
    CHECK(stack_text(r.last_canvas()).find(kExternalWaiting) != std::string::npos);

    // A REAL FACE, over the same widened body: the other lever, and the cache is cleared
    // for the identical reason.
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    r.extent(120, 40, 9, 18);
    const ExternalPane* pane = r.session().panels.external_pane(kind);
    REQUIRE(pane != nullptr);
    CHECK(pane->shown.empty());
    CHECK_FALSE(pane->heard);
    CHECK(pane->awaiting);
    for (const surface::SurfaceTextRow& row : pane->shown) {
        CHECK(static_cast<std::int64_t>(row.text.size()) <= pane->columns);
    }
}

TEST_CASE("WIND-1: an external grant follows the widened body through fit_region") {
    // THE P50 WITNESS, IN BOTH MEDIA. WIND-1 exists because an external pane's room was
    // fixed at the minimum composition's 48 columns however much surface a maker had. It is
    // the room's share now, and this walks one live pane through six resolutions of it --
    // three extents in a cell medium and the same three under a real face -- checking each
    // grant against `fit_region` over the body Workshop actually resolved, never against
    // arithmetic this case performed for itself.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    REQUIRE(seat->rooms.size() == 1);
    CHECK(seat->rooms[0].rows == 6);
    CHECK(seat->rooms[0].columns == 46);

    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    struct Grant {
        std::int64_t w;
        std::int64_t h;
        std::int64_t advance;
        std::int64_t line;
        std::int64_t rows;
        std::int64_t columns;
    };
    std::size_t said = seat->rooms.size();
    // WUX-5 TOOK TWO CELLS OFF EVERY ONE OF THESE, on both axes: the pane's rectangle is
    // unchanged and its visible boundary comes out of it.
    for (const Grant& g : std::vector<Grant>{{120, 40, 0, 0, 6, 67},
                                             {200, 60, 0, 0, 6, 107},
                                             {78, 22, 0, 0, 6, 46},
                                             {78, 22, 8, 18, 3, 68},
                                             {120, 40, 8, 18, 3, 100},
                                             {200, 60, 8, 18, 3, 160}}) {
        CAPTURE(g.w);
        CAPTURE(g.h);
        CAPTURE(g.advance);
        r.extent(g.w, g.h, g.advance, g.line);
        REQUIRE(seat->rooms.size() == said + 1);
        said = seat->rooms.size();
        // DERIVED, NOT DUPLICATED: the body Workshop resolved, put through the one function
        // production puts it through.
        const ui::Rect body = external_body_rect(r.session(), kind);
        const surface::RegionFit fit =
            surface::fit_region(body.x, body.y, body.w, body.h, g.advance, g.line);
        CHECK(seat->rooms.back().rows == fit.rows - kExternalHeaderRows);
        CHECK(seat->rooms.back().columns == fit.columns);
        // ...and the answers themselves, so a `fit_region` that changed would be named here
        // rather than agreed with.
        CHECK(seat->rooms.back().rows == g.rows);
        CHECK(seat->rooms.back().columns == g.columns);
        CHECK(seat->rooms.back().pane == std::string(kHelloPane));
        // REPEATING IT IS NOT A SECOND ANSWER.
        r.extent(g.w, g.h, g.advance, g.line);
        CHECK(seat->rooms.size() == said);
    }
}

// ---- Retained content, bounded before it is kept ----------------------------------

TEST_CASE("valid content is shown through a region at the exact granted body bounds") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;

    // BEFORE ANYTHING ARRIVES the pane says it is waiting -- a fact about this PANEL,
    // never about the provider.
    const ui::Rect body = external_body_rect(r.session(), kind);
    std::vector<std::string> shown = external_rows(r.last_canvas(), body);
    REQUIRE(shown.size() == 1);
    CHECK(shown[0] == std::string(kExternalWaiting));

    PaneContent said;
    said.pane = kHelloPane;
    said.rows.push_back(surface::SurfaceTextRow{"one", surface::role::kAccent,
                                                surface::role::kMuted});
    said.rows.push_back(surface::SurfaceTextRow{"two", surface::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });

    shown = external_rows(r.last_canvas(), body);
    REQUIRE(shown.size() == 2);
    CHECK(shown[0] == "one");
    CHECK(shown[1] == "two");
    // THE SEMANTIC ROLE AND GROUND SURVIVE UNTRANSLATED. Workshop makes no palette
    // decision for a provider and mints no provider theme.
    const ExternalPane* pane = r.session().panels.external_pane(kind);
    CHECK(pane->shown[0].role == surface::role::kAccent);
    CHECK(pane->shown[0].background == surface::role::kMuted);
    CHECK(pane->shown[1].background == surface::role::kNone);

    // ONE CANVAS, AND THE HEADER IS WORKSHOP'S. A maker can tell whose pane this is.
    const std::string stack = stack_text(r.last_canvas());
    CHECK(stack.find("Hello @zengine.test.workshop-hello") != std::string::npos);
}

TEST_CASE("WIND-2a: an external pane's own text cannot bury the surface that recovers it") {
    // THE OTHER HALF OF THE ORDERING CLAIM, and the one with the sharpest consequence. The
    // picker and the pane-management surface open OVER the overlay stack's first slot -- an
    // intentional overlap, HD-10 names it -- so whatever is seated there is underneath them
    // by construction. An external pane fills that slot with a REGION of a provider's rows,
    // and before WIND-2a a region was the topmost thing on the whole canvas: the provider's
    // text was drawn over the recovery surface's labels, and the row a maker reaches for to
    // remove a pane was underneath the pane it removes.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    REQUIRE(has_pane(r.session().setup.active, hello_ref()));

    // THE PROVIDER FILLS EVERY ROW OF ITS ROOM, so there is no gap for a recovery row to
    // survive in by luck. Every row is a distinctive byte a case can look for.
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);
    // EVERY ROW OF ITS ROOM, and the room is what the provider was GRANTED -- which since
    // TYPE-0 is the region's prose rows less Workshop's own header row, not the region's
    // cell height. Sending `body.h` rows would exceed the grant and be refused whole.
    const ExternalPane* granted = r.session().panels.external_pane(kind);
    REQUIRE(granted != nullptr);
    PaneContent loud;
    loud.pane = kHelloPane;
    for (std::int64_t i = 0; i < granted->rows; ++i) {
        loud.rows.push_back(surface::SurfaceTextRow{"ZZZZZZZZ", surface::role::kFill});
    }
    r.drive(seat, [loud](ProviderSeat& s, loom::Mail& m) { s.say(m, loud); });
    REQUIRE(external_rows(r.last_canvas(), body).size() ==
            static_cast<std::size_t>(granted->rows));

    // THE PICKER, OVER IT. What a maker reads in that slot is the picker's own list, and
    // not one row of it is the provider's.
    r.key(input::scan::kP);
    REQUIRE(r.session().panels.picker.open);
    const std::string picker = stack_text(r.last_canvas());
    INFO(picker);
    CHECK(picker.find("+ PANEL") != std::string::npos);
    CHECK(picker.find("Hello") != std::string::npos);
    CHECK(picker.find("ZZZZZZZZ") == std::string::npos);
    r.key(input::scan::kEscape);

    // THE DESK ARRANGEMENT COVERS NOTHING (ARR-0): the roster panel is retired, so
    // entering the scope leaves the provider's text visible -- the state's visible
    // statement is the affordance ring ON the pane and the band's own rows, not a panel
    // over it -- and the recovery surface for PARTICIPATION remains the picker above.
    r.key(input::scan::kW);
    r.text("w");
    REQUIRE(r.session().arrange.open);
    const std::string arranging = stack_text(r.last_canvas());
    INFO(arranging);
    CHECK(arranging.find("+ WINDOW") == std::string::npos);
    CHECK(arranging.find("ZZZZZZZZ") != std::string::npos);

    // THE CONTROL (Z0a): the provider's rows really are still being published either way.
    CHECK(external_rows(r.last_canvas(), body).size() ==
          static_cast<std::size_t>(granted->rows));
    r.key(input::scan::kEscape);
    CHECK(stack_text(r.last_canvas()).find("ZZZZZZZZ") != std::string::npos);
}

TEST_CASE("content beyond the granted room is not cached, and cannot leave stale rows") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);
    const ExternalPane* pane = r.session().panels.external_pane(kind);
    REQUIRE(pane->rows == 6);
    REQUIRE(pane->columns == 46);
    // THE ROOM THIS PANE WAS ACTUALLY GRANTED, held once: every bound below is derived
    // from it rather than from a number this case remembers, so the pane's interior
    // moving -- WUX-5 took one cell on every side for its visible boundary -- moves the
    // case with it instead of leaving it asserting about a room nobody granted.
    const std::int64_t granted_rows = pane->rows;
    const std::int64_t granted_cols = pane->columns;

    PaneContent good;
    good.pane = kHelloPane;
    good.rows.push_back(surface::SurfaceTextRow{"a good row", surface::role::kFill});
    r.drive(seat, [good](ProviderSeat& s, loom::Mail& m) { s.say(m, good); });
    REQUIRE(external_rows(r.last_canvas(), body).size() == 1);

    // ONE ROW TOO MANY.
    PaneContent tall;
    tall.pane = kHelloPane;
    const std::int64_t too_tall = granted_rows + 1;
    for (std::int64_t i = 0; i < too_tall; ++i) {
        tall.rows.push_back(surface::SurfaceTextRow{"r", surface::role::kFill});
    }
    r.drive(seat, [tall](ProviderSeat& s, loom::Mail& m) { s.say(m, tall); });
    pane = r.session().panels.external_pane(kind);
    CHECK(pane->shown.empty()); // NOT ONE of them was kept
    CHECK_FALSE(pane->heard);
    CHECK_FALSE(pane->refusal.empty());
    // AND IT IS A CONDITION, NOT A SENTENCE SOMEBODY SAID. The refusal names the
    // pane and carries the judge's own reason, it is derived from the pane that holds it,
    // and it reaches the maker on the compact attention slot -- the notice row is for
    // things that HAPPENED and this is something that is TRUE.
    const std::string content_key = pane_content_key(hello_ref());
    {
        const std::vector<Condition> now = r.conditions();
        const Condition* refused = condition_by_key(now, content_key);
        REQUIRE(refused != nullptr);
        CHECK(refused->compact.find("zengine.test.workshop-hello/hello") != std::string::npos);
        CHECK(refused->detail.find(std::to_string(too_tall) + " rows into a pane granted " +
                                   std::to_string(granted_rows)) != std::string::npos);
        CHECK(refused->role == surface::role::kAlert);
    }
    CHECK(r.attention_note().find("zengine.test.workshop-hello/hello") != std::string::npos);
    // THE STALE ROW IS GONE FROM THE PICTURE, replaced by Workshop's own sentence.
    std::vector<std::string> after = external_rows(r.last_canvas(), body);
    REQUIRE(after.size() == 1);
    CHECK(after[0] == detail::fit(kExternalRefused, granted_cols)); // fitted, cut marked
    CHECK(after[0].find("a good row") == std::string::npos);

    // A LATER VALID UPDATE RECOVERS THE PANE -- it stayed open throughout.
    r.drive(seat, [good](ProviderSeat& s, loom::Mail& m) { s.say(m, good); });
    pane = r.session().panels.external_pane(kind);
    CHECK(pane->heard);
    CHECK(pane->refusal.empty());
    CHECK(pane->refusal_why.empty()); // the reason went with the refusal it explained
    CHECK(external_rows(r.last_canvas(), body)[0] == "a good row");
    // ...AND THE CONDITION IS GONE BECAUSE ITS TRUTH RESOLVED. Nobody retracted it
    // and nothing was said over it: it stopped being returned.
    CHECK(condition_by_key(r.conditions(), content_key) == nullptr);
    CHECK(r.attention_note().find("zengine.test.workshop-hello/hello") == std::string::npos);

    // ONE BYTE TOO WIDE, on the LAST row -- so the earlier rows would have been kept
    // by anything that copied as it validated.
    PaneContent wide;
    wide.pane = kHelloPane;
    wide.rows.push_back(surface::SurfaceTextRow{"fits", surface::role::kFill});
    wide.rows.push_back(surface::SurfaceTextRow{
        std::string(static_cast<std::size_t>(granted_cols) + 1, 'x'), surface::role::kFill});
    r.drive(seat, [wide](ProviderSeat& s, loom::Mail& m) { s.say(m, wide); });
    pane = r.session().panels.external_pane(kind);
    CHECK(pane->shown.empty());
    {
        const std::vector<Condition> now = r.conditions();
        const Condition* refused = condition_by_key(now, content_key);
        REQUIRE(refused != nullptr);
        CHECK(refused->detail.find(std::to_string(granted_cols + 1) +
                                   " bytes into a pane granted " +
                                   std::to_string(granted_cols) + " columns") !=
              std::string::npos);
    }
    // ...and the 48-byte row on the boundary IS accepted.
    PaneContent edge;
    edge.pane = kHelloPane;
    edge.rows.push_back(surface::SurfaceTextRow{
        std::string(static_cast<std::size_t>(granted_cols), 'e'), surface::role::kFill});
    r.drive(seat, [edge](ProviderSeat& s, loom::Mail& m) { s.say(m, edge); });
    CHECK(r.session().panels.external_pane(kind)->shown.size() == 1);
}

TEST_CASE("a row carrying a byte a canvas cannot draw is refused whole") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;

    // `SurfaceTextRow`'S EXISTING PLAIN-ASCII CONTRACT, enforced at the one boundary
    // where a publisher this application did not write meets a canvas. The cell
    // projection is one cell per BYTE, so a multi-byte sequence is split there and a
    // control byte would move a terminal's cursor out of the row it was given.
    for (const std::string& bad : {std::string("a\033[2Jb"), std::string("tab\there"),
                                   std::string("caf\xC3\xA9"), std::string("del\x7F")}) {
        PaneContent said;
        said.pane = kHelloPane;
        said.rows.push_back(surface::SurfaceTextRow{bad, surface::role::kFill});
        r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
        INFO("row bytes: ", bad.size());
        CHECK(r.session().panels.external_pane(kind)->shown.empty());
        const std::vector<Condition> now = r.conditions();
        const Condition* refused = condition_by_key(now, pane_content_key(hello_ref()));
        REQUIRE(refused != nullptr);
        CHECK(refused->detail.find("a byte a canvas cannot draw") != std::string::npos);
    }
}

TEST_CASE("content for a closed or never-offered pane does nothing at all") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    const std::size_t catalog_before = r.session().panels.runtime.entries.size();

    // THE PANE IS IN THE CATALOG AND NOT OPEN. A provider cannot make a panel appear
    // by talking about it: discovery and presentation are two doors.
    PaneContent said;
    said.pane = kHelloPane;
    said.rows.push_back(surface::SurfaceTextRow{"unasked for", surface::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    CHECK(r.session().panels.open.size() == 1); // Info, and only Info
    CHECK(r.session().panels.external.empty());
    CHECK(r.session().panels.runtime.entries.size() == catalog_before);

    // A PANE KEY THIS OFFICE NEVER OFFERED CREATES NO CATALOG ROW EITHER.
    PaneContent unknown;
    unknown.pane = "never-offered";
    unknown.rows.push_back(surface::SurfaceTextRow{"nor this", surface::role::kFill});
    r.drive(seat, [unknown](ProviderSeat& s, loom::Mail& m) { s.say(m, unknown); });
    CHECK(r.session().panels.runtime.entries.size() == catalog_before);
    CHECK(r.session().panels.external.empty());
}

// ---- Presentation and the pointer --------------------------------------------------

TEST_CASE("an external pane occupies its whole panel, and takes hold of nothing under it") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const Screen sc = screen_of(r.session());
    const ui::Rect panel =
cells_covered(bounds_of(r.session().panels, r.session().setup.active, kind, sc).rect);

    // THE SAME RECTANGLE THE PAINTER WAS HANDED. One geometry, and the occupancy
    // answer names the OFFERED pane rather than `panel_kind`'s Builder fall-through.
    CHECK(occupied_at(r.session().panels, r.session().setup.active, sc, panel.x, panel.y).occupied);
    CHECK(occupied_at(r.session().panels, r.session().setup.active, sc, panel.x, panel.y).what == "Hello");
    CHECK(occupied_at(r.session().panels, r.session().setup.active, sc, panel.x + panel.w - 1, panel.y + panel.h - 1)
              .what == "Hello");
    CHECK_FALSE(occupied_at(r.session().panels, r.session().setup.active, sc, panel.x, panel.y + panel.h).occupied);
    CHECK_FALSE(occupied_at(r.session().panels, r.session().setup.active, sc, panel.x + panel.w, panel.y).occupied);

    // ...AND IT CARRIES THE HANDLE IT MET SINCE SEL-0, so the one caller that needs a
    // further question of this answer asks it of THIS walk rather than resolving the
    // pane a second time. The picker is a presentation with no kind and says so.
    CHECK(occupied_at(r.session().panels, r.session().setup.active, sc, panel.x, panel.y).kind ==
          kind);
    CHECK(occupied_at(r.session().panels, r.session().setup.active, sc, panel.x, panel.y + panel.h)
              .kind == kNoKind);

    // THE PRESS IS THE PANE'S, WHATEVER ELSE HAPPENS TO IT. Nothing under the pane is
    // selected, nothing begins a drag, and that is unchanged since PNL-2 -- SEL-0 moved
    // where a press GOES, never whether a pane swallows one.
    //
    // ⚠ THE POSITION IS A TERMINAL POSITION, and this case used to get that wrong: it
    // published `{panel.x, panel.y}` in `space::kCells`, which the medium's own inverse
    // reads as canvas row `panel.y - kTuiCanvasTopRow` -- two rows ABOVE the pane. The
    // press it asserted about never touched the panel, so the assertion held for a
    // reason that had nothing to do with the claim.
    const std::int64_t before = r.session().selected;
    r.press_cell(panel.x, panel.y);
    CHECK_FALSE(r.session().drag.active);
    CHECK(r.session().selected == before);
    r.press_cell(panel.x + 1, panel.y + 1);
    CHECK_FALSE(r.session().drag.active);
    CHECK(r.session().selected == before);
}

TEST_CASE("a read-only pane that ignores presses is unchanged by SEL-0") {
    // THE HELLO FIXTURE ACCEPTS NO `PanePressed` AT ALL -- it is the WP-0 protocol
    // witness and SEL-0 deliberately did not widen it. A pane that never asked for
    // input goes on receiving none: the shape is undeliverable to a weave that does
    // not accept it, so Workshop resolving and sending one changes nothing about what
    // this provider does, says, or shows.
    PaneRig r;
    r.mount_workshop();
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);
    const std::vector<std::string> before = external_rows(r.last_canvas(), body);
    REQUIRE_FALSE(before.empty());

    for (std::int64_t row = 0; row < 4; ++row) {
        r.press_cell(body.x + 2, body.y + kExternalHeaderRows + row);
    }
    CHECK(external_rows(r.last_canvas(), body) == before);
    CHECK_FALSE(r.session().drag.active);
}

// ---- Lifecycle, and the exact limit of what silence proves --------------------------

TEST_CASE("closing an external pane destroys only Workshop's copy") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    PaneContent said;
    said.pane = kHelloPane;
    said.rows.push_back(surface::SurfaceTextRow{"remembered", surface::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    REQUIRE(r.session().panels.external_pane(kind)->heard);
    const std::int64_t said_count = seat->said;

    r.pick(hello_ref()); // the picker is still the one door, in both directions
    CHECK_FALSE(r.session().panels.has(kind));
    CHECK(r.session().panels.external_pane(kind) == nullptr); // room, cache, heard, refusal

    // THE PROVIDER IS UNTOUCHED: no unload, no lifecycle operation, no retraction of
    // the catalog row, and its own semantic state outlives the presentation entirely.
    CHECK(seat->said == said_count);
    CHECK(r.session().panels.runtime.of_kind(kind) != nullptr);
    CHECK(resolve_pane(hello_ref(), r.session().panels.runtime).has_value());

    // REOPENING ASKS AGAIN AND STARTS WAITING -- it does not resurrect the old copy.
    const std::size_t rooms_before = seat->rooms.size();
    r.pick(hello_ref());
    REQUIRE(r.session().panels.external_pane(kind) != nullptr);
    CHECK_FALSE(r.session().panels.external_pane(kind)->heard);
    CHECK(r.session().panels.external_pane(kind)->awaiting);
    CHECK(seat->rooms.size() == rooms_before + 1);
}

TEST_CASE("a valid re-offer refreshes the descriptor and the open pane, without duplicating") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    PaneContent said;
    said.pane = kHelloPane;
    said.rows.push_back(surface::SurfaceTextRow{"the old answer", surface::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    REQUIRE(r.session().panels.external_pane(kind)->heard);
    const std::size_t rooms_before = seat->rooms.size();

    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"hello", "Hello", "a corrected line"});
    });
    // ONE ROW, ONE HANDLE, ONE PRESENTATION -- and the descriptor updated in place.
    REQUIRE(r.session().panels.runtime.entries.size() == 1);
    CHECK(r.session().panels.runtime.entries[0].kind == kind);
    CHECK(r.session().panels.runtime.entries[0].summary == "a corrected line");
    CHECK(r.session().panels.has(kind));
    // THE OLD PRESENTATION COPY IS CLEARED AND THE ROOM IS GRANTED AGAIN.
    const ExternalPane* pane = r.session().panels.external_pane(kind);
    CHECK(pane->shown.empty());
    CHECK_FALSE(pane->heard);
    CHECK(pane->awaiting);
    CHECK(seat->rooms.size() == rooms_before + 1);
    const ui::Rect body = external_body_rect(r.session(), kind);
    CHECK(external_rows(r.last_canvas(), body)[0] == std::string(kExternalWaiting));
}

TEST_CASE("silence is waiting, and Workshop never says unavailable") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;

    // MANY REPAINTS AND NO ANSWER. Nothing times out, nothing polls, no catalog row is
    // withdrawn and no setup reference is deleted -- because sender silence does not
    // prove a delivery's fate and Loom gives Workshop no unload notification at all.
    for (int i = 0; i < 20; ++i) {
        r.key(input::scan::kP);
        r.key(input::scan::kEscape);
    }
    const ui::Rect body = external_body_rect(r.session(), kind);
    CHECK(external_rows(r.last_canvas(), body)[0] == std::string(kExternalWaiting));
    CHECK(r.session().panels.runtime.entries.size() == 1);
    CHECK(has_pane(r.session().setup.active, hello_ref()));
    CHECK(r.session().panels.has(kind));

    // THE WORD IS NEVER SAID, on the pane or on the setup line.
    for (const surface::SurfaceText& note : r.notes) {
        CHECK(note.text.find("unavailable") == std::string::npos);
    }
    CHECK(stack_text(r.last_canvas()).find("unavailable") == std::string::npos);
    CHECK(setup_status_text(r.session().setup, "", r.session().panels.runtime,
                            r.session().keymap)
              .find("unavailable") == std::string::npos);
}

TEST_CASE("one provider offering several panes is still one weave") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"hello", "Hello", "one"});
        s.offer(m, PaneOffered{"goodbye", "Goodbye", "two"});
    });
    REQUIRE(r.session().panels.runtime.entries.size() == 2);
    CHECK(r.session().panels.runtime.entries[0].provider ==
          r.session().panels.runtime.entries[1].provider);
    CHECK(r.session().panels.runtime.entries[0].kind !=
          r.session().panels.runtime.entries[1].kind);
    // TWO CATALOG ROWS, ONE OFFICE, AND NOTHING ANYWHERE THAT COUNTS PROVIDERS. Closing
    // one presentation could not unload a weave even if this file wanted it to.
    CHECK(combined_catalog(r.session().panels).size() == kPanelKinds + 2);
}

// ---- The built-ins, unmoved --------------------------------------------------------

TEST_CASE("the built-in panels behave exactly as they did, with a provider in the room") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.extent(120, 44); // room for both stack slots, so nothing here is a capacity case

    // INFO STILL USES NO BUS and is open at boot.
    CHECK(r.session().panels.has(panel::kInfo));
    const std::size_t said_before = static_cast<std::size_t>(seat->said);

    r.pick(ref_of(panel::kBuilder));
    CHECK(r.session().panels.has(panel::kBuilder));
    CHECK(r.last_notice().find("opened Builder") != std::string::npos);
    r.pick(ref_of(panel::kBuilder));
    CHECK_FALSE(r.session().panels.has(panel::kBuilder));
    CHECK(r.last_notice().find("removed Builder") != std::string::npos);

    // NOTHING THE BUILT-INS DID REACHED THE PROVIDER.
    CHECK(static_cast<std::size_t>(seat->said) == said_before);

    // AND `panel_kind` IS STILL TOTAL ON ITS OWN BOUNDED PATH, which is what WS-0
    // established and WP-0 was required to leave standing.
    CHECK(panel_kind(9999).kind == panel::kBuilder);
    CHECK(placement_of(panel::kInfo) == placement::kSideRegion);
    CHECK_FALSE(resolve_pane(PaneRef{"nobody", "nothing"}, r.session().panels.runtime)
                    .has_value());
}
