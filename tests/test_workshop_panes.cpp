// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — the external pane seam, from both sides.
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
    CHECK(shown.find("Builder   closed") != std::string::npos);
    CHECK(shown.find("Info      open") != std::string::npos);
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
    const ui::Rect slot = cells_covered(picker_bounds(screen_of(r.session())));
    CHECK(closed.find(detail::fit(
              "  " + picker_entry_text("Hello", "closed", "a bounded external greeting"),
              slot.w)) != std::string::npos);
    CHECK(closed.find(detail::kElided) != std::string::npos);

    r.key(input::scan::kEscape);
    r.pick(hello_ref());
    r.key(input::scan::kP);
    const std::string open = stack_text(r.last_canvas());
    CHECK(open.find("Hello     open") != std::string::npos);
    r.key(input::scan::kEscape);

    // AND SELECTING IT AGAIN REMOVES IT -- the picker is still the one owner of
    // presence, and an external pane earns no second door.
    r.pick(hello_ref());
    CHECK_FALSE(has_pane(r.session().setup.active, hello_ref()));
    r.key(input::scan::kP);
    CHECK(stack_text(r.last_canvas()).find("Hello     closed") != std::string::npos);
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
    const ui::Rect box = cells_covered(picker_bounds(sc));
    const std::size_t budget = static_cast<std::size_t>(box.h - 1);
    // A CAPACITY FACT AND NOT A CATALOG ONE. The picker asks for the stack's first slot and
    // spends its top row on the heading; eight is what this composition leaves for the list.
    // It is asserted because everything below is derived from it, and it moves only when the
    // composition does. Before WP-0 the picker's height was `1 + kPanelKinds` -- a catalog
    // census standing in for a capacity, right until a catalog could outgrow the box.
    REQUIRE(budget == 8);

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
                                      detail::pad(catalog[at].name, 10)) == 0);
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

TEST_CASE("the overlay floor is the setup line's row, not the notice line's") {
    // THE BOUNDARY, STATED IN BOTH SPELLINGS AND MEASURED AGAINST THE COMPOSITION.
    // Checking only against `notice_y` would call a second slot legal at the minimum
    // extent and let it erase the row naming the arrangement a maker is in.
    for (std::int64_t h : {22, 23, 24, 30, 40, 60}) {
        const Screen sc = screen_of(78, h);
        INFO("height ", h);
        CHECK(kWorkspaceY + sc.room_h == sc.notice_y - 1);
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
        CHECK(b.y + b.h < sc.notice_y);
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
    CHECK(shown.find("Hello     waiting") != std::string::npos);
    CHECK(shown.find("Builder   open") != std::string::npos);
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
    CHECK(stack_text(r.last_canvas()).find("Hello     closed") != std::string::npos);
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
    CHECK(panel == ui::Rect{0, 1, 48, 9});
    const ExternalBodyPlace body = external_body_place(
        fine_of_cells(panel), sc,
        external_title_rows(r.session().panels, kind, r.session().pane_titles));
    CHECK(body.region_x == 0);
    CHECK(body.region_y == 1);
    CHECK(body.region_w == 48);
    CHECK(body.region_h == 9);
    const surface::RegionFit fit = surface::fit_region(0, 1, 48, 9, sc.text_advance_px,
                                                       sc.text_line_px);
    CHECK(seat->rooms[0].rows == fit.rows - kExternalHeaderRows);
    CHECK(seat->rooms[0].columns == fit.columns);
    CHECK(seat->rooms[0].rows == 8);
    CHECK(seat->rooms[0].columns == 48);
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
          fine_of_cells(ui::Rect{0, 1, 59, 9}));
    REQUIRE(seat->rooms.size() == 2);
    CHECK(seat->rooms[1].rows == 8);       // unchanged: the rows are the slot's
    CHECK(seat->rooms[1].columns == 59);   // moved: the columns are the room's share
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
          fine_of_cells(ui::Rect{0, 1, 59, 9}));
    CHECK(seat->rooms.size() == 2);

    // A TEXT METRIC MOVES IT TOO: the same cells, set in a real face, hold fewer rows and
    // more columns. Said exactly once, and through the same one call.
    r.extent(100, 40, 9, 18);
    REQUIRE(seat->rooms.size() == 3);
    const Screen typed = screen_of(r.session());
    CHECK(typed.text_advance_px == 9);
    const ui::Rect graphical = external_body_rect(r.session(), kind);
    CHECK(graphical.w == 59); // the widened body, in cells, before the face is consulted
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
    said.rows.push_back(surface::SurfaceTextRow{std::string(48, 'w'), surface::role::kFill});
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
    CHECK(wider->columns == 69);
    CHECK(wider->rows == 8);
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
    CHECK(seat->rooms[0].rows == 8);
    CHECK(seat->rooms[0].columns == 48);

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
    for (const Grant& g : std::vector<Grant>{{120, 40, 0, 0, 8, 69},
                                             {200, 60, 0, 0, 8, 109},
                                             {78, 22, 0, 0, 8, 48},
                                             {78, 22, 8, 18, 4, 71},
                                             {120, 40, 8, 18, 4, 103},
                                             {200, 60, 8, 18, 4, 163}}) {
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

    // AND THE MANAGEMENT SURFACE, over the same slot, with the same answer -- the heading
    // that names what a maker is arranging, and the row they arrange it from.
    r.key(input::scan::kW);
    r.text("w");
    REQUIRE(r.session().manage.open);
    const std::string manage = stack_text(r.last_canvas());
    INFO(manage);
    CHECK(manage.find("+ WINDOW") != std::string::npos);
    CHECK(manage.find("Hello") != std::string::npos);
    CHECK(manage.find("ZZZZZZZZ") == std::string::npos);

    // THE CONTROL (Z0a): the provider's rows really are still being published, so the two
    // absences above are a surface covering another and not a pane that stopped saying
    // anything.
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
    REQUIRE(pane->rows == 8);
    REQUIRE(pane->columns == 48);

    PaneContent good;
    good.pane = kHelloPane;
    good.rows.push_back(surface::SurfaceTextRow{"a good row", surface::role::kFill});
    r.drive(seat, [good](ProviderSeat& s, loom::Mail& m) { s.say(m, good); });
    REQUIRE(external_rows(r.last_canvas(), body).size() == 1);

    // ONE ROW TOO MANY.
    PaneContent tall;
    tall.pane = kHelloPane;
    for (int i = 0; i < 9; ++i) {
        tall.rows.push_back(surface::SurfaceTextRow{"r", surface::role::kFill});
    }
    r.drive(seat, [tall](ProviderSeat& s, loom::Mail& m) { s.say(m, tall); });
    pane = r.session().panels.external_pane(kind);
    CHECK(pane->shown.empty()); // NOT ONE of the nine rows was kept
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
        CHECK(refused->detail.find("9 rows into a pane granted 8") != std::string::npos);
        CHECK(refused->role == surface::role::kAlert);
    }
    CHECK(r.attention_note().find("zengine.test.workshop-hello/hello") != std::string::npos);
    // THE STALE ROW IS GONE FROM THE PICTURE, replaced by Workshop's own sentence.
    std::vector<std::string> after = external_rows(r.last_canvas(), body);
    REQUIRE(after.size() == 1);
    CHECK(after[0] == detail::fit(kExternalRefused, 48)); // fitted, and it marks its cut
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
    wide.rows.push_back(surface::SurfaceTextRow{std::string(49, 'x'), surface::role::kFill});
    r.drive(seat, [wide](ProviderSeat& s, loom::Mail& m) { s.say(m, wide); });
    pane = r.session().panels.external_pane(kind);
    CHECK(pane->shown.empty());
    {
        const std::vector<Condition> now = r.conditions();
        const Condition* refused = condition_by_key(now, content_key);
        REQUIRE(refused != nullptr);
        CHECK(refused->detail.find("49 bytes into a pane granted 48 columns") !=
              std::string::npos);
    }
    // ...and the 48-byte row on the boundary IS accepted.
    PaneContent edge;
    edge.pane = kHelloPane;
    edge.rows.push_back(surface::SurfaceTextRow{std::string(48, 'e'), surface::role::kFill});
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

// ---- WIND-2: THE AUTHORED WINDOW ------------------------------------------------------
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
    REQUIRE(t.session().setup.saved());
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
    CHECK(t.session().setup.on_file == live);
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
    REQUIRE(t.session().setup.saved());

    enter_management(t);
    select_pane(t, ref_of(panel::kBuilder));
    // A GEOMETRY EDIT DIRTIES IT...
    t.key(input::scan::kM);
    t.key(input::scan::kRight);
    CHECK_FALSE(t.session().setup.saved());
    // ...AND THE EXACT INVERSE MAKES IT CLEAN, because `saved()` COMPARES rather than
    // remembering that a gesture happened. A dirty FLAG would still be set here.
    t.key(input::scan::kLeft);
    CHECK_FALSE(t.session().setup.saved()); // the place is now authored, and it was not
    t.key(input::scan::kEscape);
    t.key(input::scan::k0);
    t.key(input::scan::kP);
    CHECK(t.session().setup.saved());

    // THE SAME, SAID ABOUT ORDER: a permutation returned to the identity is byte-identical
    // to a setup that was never reordered.
    t.key(input::scan::kF);
    t.key(input::scan::k0);
    t.key(input::scan::kO);
    CHECK(t.session().setup.saved());
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
    const Seating reactive = seat_panes(s, panels.runtime, stack_capacity(kMinScreen));
    REQUIRE(reactive.wanted.size() == 1);
    CHECK(reactive.wanted[0] == panel::kBuilder);
    REQUIRE(reactive.waiting.size() == 1);
    CHECK(reactive.waiting[0] == got.kind);

    // NOW PLACE THE FIRST ONE. It stops spending the tile -- a pane the maker put somewhere
    // is not in the tiling -- so the reactive one behind it gets the slot, and the placed one
    // is not waiting either, because it never asked the stack for anything.
    Setup placed = s;
    REQUIRE(author_pane_place(placed, ref_of(panel::kBuilder), subs(2), subs(2)).accepted);
    const Seating after = seat_panes(placed, panels.runtime, stack_capacity(kMinScreen));
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
    const Seating back = seat_panes(placed, panels.runtime, stack_capacity(kMinScreen));
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
    const Seating seated = seat_panes(s, panels.runtime, stack_capacity(kMinScreen));
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
    CHECK(seat_panes(s.setup.active, s.panels.runtime, stack_capacity(sc)).wanted ==
          seat_panes(geometry_before, s.panels.runtime, stack_capacity(sc)).wanted);
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

TEST_CASE("WIND-2: the `w` that opens pane management does not type itself") {
    Live t;
    enter_management(t);
    CHECK(t.session().manage.open);
    CHECK(t.session().manage.doing == pane_manage::kSelect);
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

    enter_management(t);
    // SELECT: cycling reaches every setup-named pane, and only those.
    const PaneRef first = t.session().manage.selected;
    t.key(input::scan::kTab);
    CHECK_FALSE(t.session().manage.selected == first);
    t.key(input::scan::kTab);
    CHECK(t.session().manage.selected == first);

    // ...on the Builder, which is the one this composition lets a maker arrange.
    select_pane(t, ref_of(panel::kBuilder));

    // MOVE.
    t.key(input::scan::kM);
    REQUIRE(t.session().manage.doing == pane_manage::kMove);
    const Screen sc = screen_of(t.session());
    const ui::Rect before =
cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                      panel::kBuilder, sc).rect);
    // FOUR STEPS EACH WAY, deliberately: the size sweep below pulls the left edge three
    // times and the top edge three times, and an outward pull at the canvas origin is a
    // REFUSED proposal (a pane place cannot be negative) — the wall working, not the sweep.
    // Starting four cells in keeps every one of the twenty-four pulls legal.
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
    t.key(input::scan::kEscape);
    CHECK(t.session().manage.doing == pane_manage::kSelect);

    // SIZE, by every edge and every corner — AND EVERY EDGE PRESERVES ITS OPPOSITE ANCHOR
    // (WUX-2, reversing WIND-2's size-only rule). Pulling a left or top edge outward moves
    // the place WITH the size so the far edge holds still; pulling right or bottom leaves
    // the place untouched, which is the same anchor said by not writing. The expectations
    // are tracked in sub-units across the sweep, so the loop is the invariant for all
    // eight handles rather than eight local facts.
    t.key(input::scan::kS);
    REQUIRE(t.session().manage.doing == pane_manage::kSize);
    std::int64_t expect_x = subs(before.x + 4);
    std::int64_t expect_y = subs(before.y + 4);
    for (std::int64_t seen = 0; seen < pane_edge::kCount; ++seen) {
        CAPTURE(std::string(pane_edge_name(t.session().manage.edge)));
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
        // PULLING THE EDGE OUTWARD WIDENS OR HEIGHTENS, whichever axes it names.
        const std::int64_t chosen = t.session().manage.edge;
        const bool wide = chosen != pane_edge::kTop && chosen != pane_edge::kBottom;
        const bool tall = chosen != pane_edge::kLeft && chosen != pane_edge::kRight;
        const bool leftwards = chosen == pane_edge::kLeft || chosen == pane_edge::kTopLeft ||
                               chosen == pane_edge::kBottomLeft;
        const bool upwards = chosen == pane_edge::kTop || chosen == pane_edge::kTopLeft ||
                             chosen == pane_edge::kTopRight;
        if (wide) {
            t.key(leftwards ? input::scan::kLeft : input::scan::kRight);
        }
        if (tall) {
            t.key(upwards ? input::scan::kUp : input::scan::kDown);
        }
        if (leftwards) {
            expect_x -= subs(1); // the left edge moved out one cell; the RIGHT edge held
        }
        if (upwards) {
            expect_y -= subs(1); // the top edge moved out one cell; the BOTTOM edge held
        }
        const SetupPane* now = pane_of(t.session().setup.active, ref_of(panel::kBuilder));
        if (wide) {
            CHECK(now->width.mode == pane_unit::kSubcells);
            CHECK(now->width.amount == w0 + subs(1));
        }
        if (tall) {
            CHECK(now->height.mode == pane_unit::kSubcells);
            CHECK(now->height.amount == h0 + subs(1));
        }
        // THE ANCHOR LAW, all eight ways: the place moved exactly when a left or top
        // edge did, by exactly what the size grew — so the opposite edge's position
        // (place + size on each pulled axis) is unchanged by construction.
        CHECK(now->place.x == expect_x);
        CHECK(now->place.y == expect_y);
        if (leftwards) {
            CHECK(now->place.x + now->width.amount == expect_x + subs(1) + w0);
        }
        if (upwards) {
            CHECK(now->place.y + now->height.amount == expect_y + subs(1) + h0);
        }
        t.key(input::scan::kTab);
        CHECK(t.session().manage.edge == (chosen + 1) % pane_edge::kCount);
    }
    // EIGHT TABS RETURN TO WHERE THE CYCLE STARTED, which is what makes the loop above a
    // sweep of every affordance rather than of eight arbitrary ones.
    CHECK(t.session().manage.edge == pane_edge::kBottomRight);
    t.key(input::scan::kEscape);

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
    REQUIRE(t.session().manage.doing == pane_manage::kReset);
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
    CHECK(ranks_of(t.session().setup.active) == std::vector<std::int64_t>{0, 1});

    // AND ESCAPE LEAVES, one level at a time. No pointer event has been published at all.
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().manage.open);
}

TEST_CASE("WIND-2: escape unwinds one level and rolls nothing back") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    enter_management(t);
    select_pane(t, ref_of(panel::kInfo));
    // Info is the reserved side column, so a geometry step REFUSES LEGIBLY and writes
    // nothing -- the recovery-versus-authoring line, said about the one pane a maker may
    // never move.
    t.key(input::scan::kM);
    CHECK(t.session().manage.doing == pane_manage::kSelect);
    CHECK(t.notice().find("reserved side column") != std::string::npos);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kInfo))->place.mode ==
          pane_unit::kDefault);

    // ORDER STILL WORKS ON IT, which is what makes the refusal narrow rather than a dead end.
    REQUIRE(t.session().setup.active.panes.size() == 1);
    t.key(input::scan::kF);
    CHECK(t.notice().find("already where") != std::string::npos);

    // AND ESCAPE FROM A SUBMODE RETURNS ONE LEVEL WITHOUT UNDOING THE EDIT THAT WAS MADE.
    // `p` is a COMMAND-mode key, so a maker leaves the mode to reach the picker -- which is
    // the priority chain doing exactly its job.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().manage.open);
    open_pane(t, ref_of(panel::kBuilder));
    enter_management(t);
    select_pane(t, ref_of(panel::kBuilder));
    t.key(input::scan::kM);
    t.key(input::scan::kRight);
    const PanePlace committed =
        pane_of(t.session().setup.active, ref_of(panel::kBuilder))->place;
    t.key(input::scan::kEscape);
    CHECK(t.session().manage.doing == pane_manage::kSelect);
    CHECK(t.session().manage.open);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->place == committed);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().manage.open);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kBuilder))->place == committed);
}

TEST_CASE("WIND-2: a hand and a key author the same setup values") {
    const auto arrange_by_key = [](Live& t) {
        enter_management(t);
        select_pane(t, ref_of(panel::kBuilder));
        t.key(input::scan::kM);
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
    enter_management(handed);
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
    enter_management(t);
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

TEST_CASE("WIND-2: outside management, a selected pane behind another clicks through nothing") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    enter_management(t);
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
    REQUIRE_FALSE(t.session().manage.open);
    // THE SELECTION SURVIVED LEAVING THE MODE...
    CHECK(t.session().manage.selected == ref_of(panel::kBuilder));

    // ...AND THE PRIORITY DID NOT. A press inside the covered Builder is answered by the
    // VISIBLE pane on top of it, which is what stops a selection becoming a click-through.
    const Occupancy here = occupied_at(t.session().panels, t.session().setup.active, sc,
                                       side.x + 1, side.y + 3);
    REQUIRE(here.occupied);
    CHECK(here.what == std::string("Info"));
    t.press_at(side.x + 1, side.y + 3 + surface::kTuiCanvasTopRow, input::space::kCells);
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
    enter_management(t);
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

TEST_CASE("WIND-2: the management surface names the pane, the state and the authored window") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{200, 60, 0, 0}));
    enter_management(t);
    const std::string shown = management_text(t);
    INFO(shown);
    CHECK(shown.find("+ WINDOW") != std::string::npos);
    CHECK(shown.find("Info") != std::string::npos);
    CHECK(shown.find("open") != std::string::npos);
    // A REACTIVE PANE SAYS `default` IN CHARACTERS rather than leaving the column blank.
    CHECK(shown.find("-x-") != std::string::npos);

    // THE SUBMODE AND THE CHOSEN EDGE ARE SAID IN CHARACTERS TOO, so a medium with no colour
    // carries the whole answer (HD-8's law).
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().manage.open);
    open_pane(t, ref_of(panel::kBuilder));
    enter_management(t);
    select_pane(t, ref_of(panel::kBuilder));
    t.key(input::scan::kS);
    const std::string sizing = management_text(t);
    INFO(sizing);
    CHECK(sizing.find(pane_edge_name(t.session().manage.edge)) != std::string::npos);
    CHECK(sizing.find(pane_edge_mark(t.session().manage.edge)) != std::string::npos);

    // AND RETURN IS NOT THE PICKER'S RETURN HERE. The two surfaces share a list and a
    // renderer and NOT a purpose: selecting an open row in the picker REMOVES it (PNL-0),
    // and the gesture a maker reaches for while arranging is . So management binds no
    // toggle at all, and pressing Return changes the setup's membership not at all.
    const Setup membership = t.session().setup.active;
    t.key(input::scan::kEscape);
    t.key(input::scan::kReturn);
    CHECK(t.session().manage.open);
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
    CHECK(seat->rooms.back().columns == 30);
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
        const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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
        const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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
        const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));

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
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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
        REQUIRE(r.session().manage.open);
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
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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
    REQUIRE(r.session().panels.open.size() == 3); // Info and both panes

    for (const RuntimePane& row : r.session().panels.runtime.entries) {
        CAPTURE(row.pane);
        const ui::Rect panel = cells_covered(external_panel_rect(r.session(), row.kind));
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
    const ui::Rect gpanel = cells_covered(external_panel_rect(px.session(), gkind));
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
    CHECK_FALSE(r.session().manage.open);
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
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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
    CHECK_FALSE(r.session().manage.open);
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
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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
    REQUIRE(r.session().manage.open);
    press_body(r, kind);
    r.key(input::scan::kTab);
    CHECK(seat->keys.empty());
    // `esc` IS BACK AND NOT CANCEL (WIND-2), so leaving takes one press per level --
    // a press inside the mode may have entered one, and a case that assumed a single
    // escape would be asserting a shape this mode deliberately does not have.
    for (int i = 0; i < 4 && r.session().manage.open; ++i) {
        r.key(input::scan::kEscape);
    }
    REQUIRE_FALSE(r.session().manage.open);

    // THE SETUP-NAME EDITOR, the same -- and reaching it needs the keyboard back
    // again, because leaving a mode restores the pane's claim exactly as it found it.
    // It also needs a setup file, which `open_setup_name` refuses without.
    r.host.setup_path = "msg0-not-written.json";
    press_outside(r, kind);
    r.key(input::scan::kS);
    r.text("s");
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
            const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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

    ComposeRig() {
        r.mount_workshop();
        r.ready();
        // A PANE BIG ENOUGH TO READ WHOLE. The windowing is the pure suite's claim;
        // these cases are about the conversation, and a case that had to scroll to
        // find a row would be measuring the window twice.
        r.extent(240, 80);
        (void)r.load(zengine::composer::kComposerStem, WORKSHOP_SO_COMPOSER, kComposerOffice);
        r.pick(composer_ref());
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
    CHECK(r.session().panels.open.size() == 3); // Info, Loaded, Compose
    CHECK(r.session().panels.has(panel::kInfo));
    CHECK_FALSE(r.session().panels.picker.open);
    CHECK_FALSE(r.session().manage.open);
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
    CHECK_FALSE(r.session().manage.open);
    CHECK_FALSE(r.session().terminal.open);
}

// ---- INTR-1: the resolved arrangement and the power stack, as two more panes ------
//
// THREE TIERS AGAIN, AND THE SPLIT IS INTR-0'S. The first asks what a projection
// MEANS -- pure functions over a value, no bus, no library, no Workshop -- because
// "every displayed fact is true" is a statement about a projection. The second drives
// the real `zengine-introspection` library through the real pane seam over a real
// authored arrangement and a real `op::Catalog`, because "the fact has an
// authoritative owner" is a statement about a path. The third reads Workshop's own
// source, because "the host knows nothing about these panes" is a statement about a
// file and every rig here would stay green if it stopped being true.
//
// ⚠ THE PANE TIER NEVER LOADS THE TIMER. `PaneRig` pumps to EMPTY and a live Timer
// re-arms its own beat inside its own handler. The arrangement facts that need a
// provider+weave artifact -- the Timer appearing ONCE, the operator handoff outcome --
// are proved in `test_workshop_load.cpp`, whose rigs drain in bounded turns.

// ---- Tier one: what a projection means --------------------------------------------

TEST_CASE("INTR-1: the arrangement's summary is DERIVED from the rows it is printed over") {
    const ws::ResolvedArrangement said = shaped_arrangement();
    const std::vector<surface::SurfaceTextRow> rows = intro::project_arrangement(said, 40, 80);
    REQUIRE_FALSE(rows.empty());

    // EXACT CARDINALITY, NEVER A PERCENTAGE. Four artifacts, all performed, two of
    // which mounted a provider and three of which loaded a weave -- and every one of
    // those numbers is counted over the very list below it, so the summary and the
    // rows cannot come to disagree.
    CHECK(rows[0].text == "4 of 4 artifacts resolved -- 2 providers, 3 weaves");
    CHECK(rows[0].role == surface::role::kAccent);
    CHECK(row_with(rows, "%") == -1);
    CHECK(row_with(rows, "100") == -1);
}

TEST_CASE("INTR-1: a partial arrangement cannot read as a complete one") {
    ws::ResolvedArrangement said = shaped_arrangement();
    said.artifacts[2].state = ws::kRefusedToken;
    said.artifacts[2].provider.clear();
    said.artifacts[2].weave = 0;
    said.artifacts[3].state = ws::kAuthoredToken;
    said.artifacts[3].weave = 0;
    const std::vector<surface::SurfaceTextRow> rows = intro::project_arrangement(said, 40, 80);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows[0].text == "2 of 4 artifacts resolved -- 1 providers, 1 weaves");
    // ...and the two unresolved rows say so where a maker reads them, IN TWO DIFFERENT
    // SENTENCES (BOOT-0). One artifact refused and one was never reached; before
    // realization had a persistent owner both were `performed = false` and both read
    // `(not reached)`, which told a maker the wrong story about which one broke.
    const std::int64_t refused = row_with(rows, intro::kRefusedRow);
    REQUIRE(refused >= 0);
    const std::int64_t untried = row_with(rows, intro::kNotReached);
    REQUIRE(untried >= 0);
    CHECK(refused != untried);
    // AND ONLY ONE OF THEM IS AN ALERT. `kAlert` is "something the maker must see"; a
    // row nothing has reached yet in a project that is still coming up is not that.
    CHECK(rows[static_cast<std::size_t>(refused)].role == surface::role::kAlert);
    CHECK(rows[static_cast<std::size_t>(untried)].role == surface::role::kMuted);
}

TEST_CASE("BOOT-0: a project still coming up shows LOADING, and it is not an alert") {
    // ⭐ THE STATE THAT COULD NOT EXIST BEFORE. Realization used to finish inside a
    // single stack frame before any pane could be mounted, so no maker could ever see a
    // row mid-flight. It proceeds through ordinary deliveries now, and this is what the
    // pane says while it does.
    ws::ResolvedArrangement said = shaped_arrangement();
    said.artifacts[1].state = ws::kLoadingToken;
    said.artifacts[1].provider.clear();
    said.artifacts[1].weave = 0;
    said.artifacts[2].state = ws::kAuthoredToken;
    said.artifacts[2].provider.clear();
    said.artifacts[2].weave = 0;
    said.artifacts[3].state = ws::kAuthoredToken;
    said.artifacts[3].weave = 0;
    const std::vector<surface::SurfaceTextRow> rows = intro::project_arrangement(said, 40, 80);
    REQUIRE_FALSE(rows.empty());

    // EXACT COUNTS, AND NOT A PERCENTAGE OR A BAR. One of four has resolved; the rest
    // are three different states and the heading claims nothing about them.
    CHECK(rows[0].text == "1 of 4 artifacts resolved -- 1 providers, 0 weaves");
    CHECK(row_with(rows, "%") == -1);
    const std::int64_t loading = row_with(rows, intro::kLoadingNow);
    REQUIRE(loading >= 0);
    CHECK(rows[static_cast<std::size_t>(loading)].role == surface::role::kMuted);
    // ...AND NOTHING SAYS REFUSED, because nothing refused.
    CHECK(row_with(rows, intro::kRefusedRow) == -1);
}

TEST_CASE("INTR-1: AUTHORED and RESOLVED are two labelled rows, and never one") {
    const std::vector<surface::SurfaceTextRow> rows =
        intro::project_arrangement(shaped_arrangement(), 40, 80);
    const std::vector<std::string> text = texts_of(rows);

    // THE TIMER'S BLOCK, WHOLE: a stem, what a person asked for, and two resolved
    // participations -- so `zengine.timer` the ROLE and `zengine.timer` the PROVIDER
    // IDENTITY, which read alike, are never on one row where a maker could take them
    // for one fact.
    const std::int64_t stem = row_with(rows, "zengine-timer");
    REQUIRE(stem >= 0);
    const std::size_t at = static_cast<std::size_t>(stem);
    REQUIRE(at + 3 < text.size());
    CHECK(text[at] == "  zengine-timer");
    CHECK(text[at + 1] == "    authored  provider normal, weave zengine.timer");
    CHECK(text[at + 2] == "    resolved  provider zengine.timer, 1 power");
    CHECK(text[at + 3] == "    resolved  weave #7, operator host offered");

    // A RESOLVED PROVIDER IDENTITY IS NEVER PRESENTED AS SOMETHING THE PLAN AUTHORED,
    // and a minted WeaveId is never presented as durable intent: neither appears on an
    // `authored` row anywhere in the projection.
    for (const std::string& row : text) {
        if (row.find("    authored ") == 0) {
            CHECK(row.find("#") == std::string::npos);
            CHECK(row.find("power") == std::string::npos);
        }
    }
}

TEST_CASE("INTR-1: one artifact is ONE block, however many surfaces it participates as") {
    const std::vector<std::string> text =
        texts_of(intro::project_arrangement(shaped_arrangement(), 40, 80));
    std::size_t stems = 0;
    for (const std::string& row : text) {
        stems += row == "  zengine-timer" ? 1u : 0u;
    }
    // LOAD-0'S CENTRAL RESULT, IN PRESENTATION. Two participations, one row naming it.
    CHECK(stems == 1);
}

TEST_CASE("INTR-1: a provider-only artifact is visible and wears no weave") {
    const std::vector<surface::SurfaceTextRow> rows =
        intro::project_arrangement(shaped_arrangement(), 40, 80);
    const std::vector<std::string> text = texts_of(rows);
    const std::int64_t at = row_with(rows, "zengine-operators-basic");
    REQUIRE(at >= 0);
    const std::size_t i = static_cast<std::size_t>(at);
    REQUIRE(i + 3 < text.size());
    CHECK(text[i] == "  zengine-operators-basic");
    CHECK(text[i + 1] == "    authored  provider normal");
    CHECK(text[i + 2] == "    resolved  provider zengine.operators.basic, 2 powers");
    // `provider != weave`, ALL THE WAY INTO PRESENTATION. The block ENDS after those
    // three rows: the next one is the next artifact's stem, so there is no WeaveId, no
    // role and -- the trap this case is really about -- no operator-host outcome for a
    // load that never happened.
    CHECK(text[i + 3].rfind("    ", 0) != 0);
    CHECK(text[i + 3].find("weave") == std::string::npos);
}

TEST_CASE("INTR-1: a weave-only artifact is visible and wears no provider") {
    const std::vector<surface::SurfaceTextRow> rows =
        intro::project_arrangement(shaped_arrangement(), 40, 80);
    const std::vector<std::string> text = texts_of(rows);
    const std::int64_t at = row_with(rows, "zengine-composer");
    REQUIRE(at >= 0);
    const std::size_t i = static_cast<std::size_t>(at);
    REQUIRE(i + 2 < text.size());
    CHECK(text[i + 1] == "    authored  weave zengine.composer");
    CHECK(text[i + 2] == "    resolved  weave #9, operator host not-a-consumer");
    CHECK(text[i + 2].find("provider") == std::string::npos);
    // ...and the block is TWO rows under its stem, so nothing was invented for a
    // surface the plan never asked for.
    CHECK((i + 3 == text.size() || text[i + 3].rfind("    ", 0) != 0));
}

TEST_CASE("INTR-1: an artifact BLOCK is shown whole or counted, never half") {
    const ws::ResolvedArrangement said = shaped_arrangement();
    // THE ONE-ROW FLOOR FIRST, because it is the budget at which the rule below does
    // not apply and the accounting still holds: there is no list, no note and no
    // marker -- and the HEADING states the population, so nothing is hidden without
    // being counted even there.
    const std::vector<surface::SurfaceTextRow> floor = intro::project_arrangement(said, 1, 60);
    REQUIRE(floor.size() == 1);
    CHECK(floor[0].text.rfind("4 of 4 artifacts", 0) == 0);

    // ...AND FROM TWO ROWS UP, SHOWING PART OF A LIST OBLIGES SAYING HOW MUCH WAS HIDDEN.
    for (std::int64_t rows = 2; rows <= 24; ++rows) {
        const std::vector<surface::SurfaceTextRow> shown =
            intro::project_arrangement(said, rows, 60);
        CHECK(static_cast<std::int64_t>(shown.size()) <= rows);
        const std::vector<std::string> text = texts_of(shown);
        // EVERY STEM ROW IS FOLLOWED BY ITS OWN `authored` ROW. A block cut in half
        // would leave a stem naming nothing, which is a different and wrong answer
        // rather than a shorter one.
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i].rfind("  zengine-", 0) == 0 && text[i].rfind("    ", 0) != 0) {
                REQUIRE(i + 1 < text.size());
                CHECK(text[i + 1].rfind("    authored  ", 0) == 0);
            }
        }
        // AND WHAT IS NOT SHOWN IS COUNTED. Nothing is hidden in silence at any budget.
        std::size_t blocks = 0;
        for (const std::string& row : text) {
            blocks += (row.rfind("  zengine-", 0) == 0 && row.rfind("    ", 0) != 0) ? 1u : 0u;
        }
        if (blocks < said.artifacts.size()) {
            const std::int64_t marker = row_with(shown, "more");
            REQUIRE_MESSAGE(marker >= 0, "rows=", rows, " showed ", blocks, " and said nothing");
            CHECK(text[static_cast<std::size_t>(marker)] ==
                  "  ... " + std::to_string(said.artifacts.size() - blocks) + " more");
        }
    }
}

TEST_CASE("INTR-1: the population bound is reserved before the list gets a second row") {
    const ws::ResolvedArrangement said = shaped_arrangement();
    // AT EVERY BUDGET THAT SHOWS A LIST AT ALL, the sentence saying what these rows are
    // NOT is on the canvas -- INTR-0's reservation argument, in a third place. A maker
    // reading `4 of 4 artifacts` beside a running Builder would otherwise be right to
    // conclude the Builder is not running.
    for (std::int64_t rows = 3; rows <= 30; ++rows) {
        const std::vector<surface::SurfaceTextRow> shown =
            intro::project_arrangement(said, rows, 70);
        CHECK_MESSAGE(row_with(shown, intro::kNotAuthored) >= 0, "rows=", rows);
    }
    // ...AND THE LIST'S FIRST ROW OUTRANKS THE CAVEAT, which is the other half of the
    // same rule and is why the sweep starts at three. At a TWO-row body there is one
    // row left after the heading and it goes to the list -- a pane showing only its own
    // small print has stopped being a view of anything. What that one row can carry
    // here is not a block, so it carries the count of what is hidden.
    const std::vector<surface::SurfaceTextRow> two = intro::project_arrangement(said, 2, 70);
    REQUIRE(two.size() == 2);
    CHECK(two[1].text == "  ... 4 more");
    // ...and at a ONE-row body there is no list and no note, and the population is
    // still stated: the heading is the floor of the accounting.
    const std::vector<surface::SurfaceTextRow> floor = intro::project_arrangement(said, 1, 70);
    REQUIRE(floor.size() == 1);
    CHECK(floor[0].text.rfind("4 of 4 artifacts", 0) == 0);
}

TEST_CASE("INTR-1: the plan line is spent only out of genuine slack") {
    const ws::ResolvedArrangement said = shaped_arrangement();
    // A pane that had to window its own project spends that row on the project.
    const std::vector<surface::SurfaceTextRow> tight = intro::project_arrangement(said, 8, 70);
    CHECK(row_with(tight, "plan: ") == -1);
    CHECK(row_with(tight, "more") >= 0);
    // ...and one with room over says where the authored rows came from.
    const std::vector<surface::SurfaceTextRow> roomy = intro::project_arrangement(said, 30, 70);
    const std::int64_t at = row_with(roomy, "plan: default-load-plan.json");
    REQUIRE(at >= 0);
    CHECK(roomy[static_cast<std::size_t>(at)].role == surface::role::kMuted);
}

TEST_CASE("INTR-1: every arrangement projection fits the room it was given") {
    // THE WHOLE DOMAIN, because being exactly inside the grant is this function's
    // obligation rather than a courtesy: Workshop refuses an over-budget update WHOLE.
    const ws::ResolvedArrangement said = shaped_arrangement();
    for (std::int64_t rows = 0; rows <= 26; ++rows) {
        for (std::int64_t columns = 0; columns <= 90; columns += 3) {
            const std::vector<surface::SurfaceTextRow> shown =
                intro::project_arrangement(said, rows, columns);
            REQUIRE(static_cast<std::int64_t>(shown.size()) <= (rows < 0 ? 0 : rows));
            for (const surface::SurfaceTextRow& row : shown) {
                REQUIRE(static_cast<std::int64_t>(row.text.size()) <= columns);
            }
        }
    }
}

TEST_CASE("INTR-1: an empty arrangement is an observed zero, and says what it is not") {
    ws::ResolvedArrangement empty;
    const std::vector<surface::SurfaceTextRow> shown = intro::project_arrangement(empty, 10, 60);
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0].text == "0 of 0 artifacts resolved -- 0 providers, 0 weaves");
    CHECK(row_with(shown, intro::kNotAuthored) >= 0);
}

// ---- Tier one, the powers half ----------------------------------------------------

TEST_CASE("INTR-1: a power's ACTIVE contribution is read first, and the shadowed ones under it") {
    ws::ResolvedPowers said = shaped_powers();
    // The overlay's own shape: two contributions, active LAST on the wire.
    said.powers[1].contributions = {supplied_by("zengine.operators.basic"),
                                    supplied_by("zengine.operators.test.min")};
    const std::vector<surface::SurfaceTextRow> rows = intro::project_powers(said, 40, 70);
    const std::vector<std::string> text = texts_of(rows);
    const std::int64_t at = row_with(rows, "  math.max");
    REQUIRE(at >= 0);
    const std::size_t i = static_cast<std::size_t>(at);
    REQUIRE(i + 2 < text.size());

    // THE WORD IS THE STATEMENT AND THE INK IS THE SECOND SIGNAL. A monochrome terminal
    // reads the same fact a coloured one does.
    CHECK(text[i] == "  math.max");
    CHECK(text[i + 1] == "      active    zengine.operators.test.min");
    CHECK(text[i + 2] == "      shadowed  zengine.operators.basic");
    CHECK(rows[i + 1].role == surface::role::kFill);
    CHECK(rows[i + 2].role == surface::role::kMuted);
}

TEST_CASE("INTR-1: composite is shown where the definition says so, and nowhere else") {
    const std::vector<std::string> text = texts_of(intro::project_powers(shaped_powers(), 40, 70));
    bool composite_marked = false;
    for (const std::string& row : text) {
        if (row.find("zengine.timer") != std::string::npos) {
            composite_marked = composite_marked || row.find("(composite)") != std::string::npos;
        }
        if (row.find("zengine.operators.basic") != std::string::npos) {
            CHECK(row.find("(composite)") == std::string::npos);
        }
    }
    CHECK(composite_marked);
}

TEST_CASE("INTR-1: a contribution the host published itself is named as the host") {
    ws::ResolvedPowers said;
    said.powers.push_back(power_of("host.thing", {supplied_by("")}));
    const std::vector<std::string> text = texts_of(intro::project_powers(said, 10, 60));
    CHECK(row_with(intro::project_powers(said, 10, 60), intro::kHostItself) >= 0);
    // ...and the empty provider never reaches a maker's eye as an empty column.
    for (const std::string& row : text) {
        CHECK(row != "      active    ");
    }
}

TEST_CASE("INTR-1: the powers summary counts identities and providers, exactly") {
    const std::vector<surface::SurfaceTextRow> rows = intro::project_powers(shaped_powers(), 40, 70);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows[0].text == "3 powers resolve here -- from 2 providers");
    CHECK(rows[0].role == surface::role::kAccent);
    CHECK(row_with(rows, "%") == -1);
}

TEST_CASE("QR-4: the powers heading agrees with its own counts, at one and at many") {
    // THROUGH THE PRESENTATION BOTH MEDIA SPEND, not through the count word on its own:
    // what a maker actually reads is `project_powers`' first row, and that is what is
    // asserted here. A helper tested alone would have stayed correct while the heading
    // that concatenated around it went on saying `1 providers`.
    ws::ResolvedPowers one;
    one.providers = {"zengine.operators.basic"};
    one.powers.push_back(power_of("math.max", {supplied_by("zengine.operators.basic")}));
    const std::vector<surface::SurfaceTextRow> lone = intro::project_powers(one, 40, 70);
    REQUIRE_FALSE(lone.empty());
    CHECK(lone[0].text == "1 power resolves here -- from 1 provider");
    CHECK(lone[0].text.find("1 providers") == std::string::npos);
    CHECK(lone[0].text.find("1 powers") == std::string::npos);

    // TWO PROVIDERS AND MANY POWERS STAY PLURAL, which is the half a singular repair is
    // most likely to break.
    const std::vector<surface::SurfaceTextRow> many =
        intro::project_powers(shaped_powers(), 40, 70);
    REQUIRE_FALSE(many.empty());
    CHECK(many[0].text == "3 powers resolve here -- from 2 providers");

    // AND EVERY OTHER CARDINALITY THE PAIR CAN TAKE, including the empty host, so no
    // count in this heading can disagree with the word beside it.
    ws::ResolvedPowers none;
    CHECK(intro::project_powers(none, 40, 70)[0].text ==
          "0 powers resolve here -- from 0 providers");
    ws::ResolvedPowers two_from_one;
    two_from_one.providers = {"zengine.operators.basic"};
    two_from_one.powers.push_back(power_of("math.max", {supplied_by("zengine.operators.basic")}));
    two_from_one.powers.push_back(
        power_of("logic.select_int", {supplied_by("zengine.operators.basic")}));
    CHECK(intro::project_powers(two_from_one, 40, 70)[0].text ==
          "2 powers resolve here -- from 1 provider");
}

TEST_CASE("QR-4: the powers bound claims one host's resolution and nothing about a weave") {
    // ⭐ THE SENTENCE MAY ONLY SAY WHAT THIS PANE READ. It read ONE catalog: the host's.
    // The wording it replaced -- `a weave that took no offer holds its own catalog` --
    // is true of the Timer's supported local fallback and is not a law of every weave
    // that accepts no operator host, so a pane in no position to know it must not
    // appear to settle it.
    CHECK(std::string(intro::kHostResolution) ==
          "this pane describes this host's operator resolution only");

    const std::vector<surface::SurfaceTextRow> shown =
        intro::project_powers(shaped_powers(), 40, 70);
    REQUIRE(row_with(shown, intro::kHostResolution) >= 0);
    // ...AND THE OLD CLAIM IS GONE FROM THE ROWS, by its own words rather than by the
    // name of the constant that used to hold them.
    CHECK(row_with(shown, "a weave that took no offer holds its own catalog") == -1);
    for (const char* claim : {"took no offer", "its own catalog", "holds its own"}) {
        CHECK_MESSAGE(row_with(shown, claim) == -1, claim);
    }
}

TEST_CASE("INTR-1: a power BLOCK is shown whole or counted, and its bound is reserved") {
    const ws::ResolvedPowers said = shaped_powers();
    for (std::int64_t rows = 3; rows <= 20; ++rows) {
        const std::vector<surface::SurfaceTextRow> shown = intro::project_powers(said, rows, 60);
        CHECK(static_cast<std::int64_t>(shown.size()) <= rows);
        CHECK_MESSAGE(row_with(shown, intro::kHostResolution) >= 0, "rows=", rows);
        const std::vector<std::string> text = texts_of(shown);
        std::size_t blocks = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i].rfind("  ", 0) == 0 && text[i].rfind("      ", 0) != 0 &&
                text[i].rfind("  ...", 0) != 0 && !text[i].empty()) {
                ++blocks;
                // A POWER NAMED IS A POWER WHOSE ACTIVE CONTRIBUTION IS ON THE CANVAS.
                REQUIRE(i + 1 < text.size());
                CHECK(text[i + 1].rfind("      active    ", 0) == 0);
            }
        }
        if (blocks < said.powers.size()) {
            CHECK_MESSAGE(row_with(shown, "more") >= 0, "rows=", rows);
        }
    }
}

TEST_CASE("INTR-1: every powers projection fits the room it was given") {
    const ws::ResolvedPowers said = shaped_powers();
    for (std::int64_t rows = 0; rows <= 20; ++rows) {
        for (std::int64_t columns = 0; columns <= 80; columns += 3) {
            const std::vector<surface::SurfaceTextRow> shown =
                intro::project_powers(said, rows, columns);
            REQUIRE(static_cast<std::int64_t>(shown.size()) <= (rows < 0 ? 0 : rows));
            for (const surface::SurfaceTextRow& row : shown) {
                REQUIRE(static_cast<std::int64_t>(row.text.size()) <= columns);
            }
        }
    }
}

TEST_CASE("INTR-1: an entry and its omission marker are ONE demand on the budget") {
    // THE ARITHMETIC INTR-0 WAS MEASURED GETTING WRONG, generalised to blocks and
    // spelled once. Three blocks of two rows in a budget of five: two blocks fit, and
    // the marker's row takes one of them back rather than being added on top.
    const std::vector<std::int64_t> heights{2, 2, 2};
    CHECK(intro::lay_blocks(heights, 6).shown == 3);
    CHECK_FALSE(intro::lay_blocks(heights, 6).marker);
    CHECK(intro::lay_blocks(heights, 5).shown == 2);
    CHECK(intro::lay_blocks(heights, 5).marker);
    CHECK(intro::lay_blocks(heights, 4).shown == 1);
    CHECK(intro::lay_blocks(heights, 4).marker);
    // AT A BUDGET TOO SMALL FOR ONE BLOCK, IT SAYS. Nothing is hidden in silence.
    CHECK(intro::lay_blocks(heights, 1).shown == 0);
    CHECK(intro::lay_blocks(heights, 1).marker);
    // TOTAL over every budget, including ones no pane has.
    CHECK(intro::lay_blocks(heights, 0).shown == 0);
    CHECK_FALSE(intro::lay_blocks(heights, 0).marker);
    CHECK(intro::lay_blocks({}, 5).shown == 0);
    CHECK_FALSE(intro::lay_blocks({}, 5).marker);
}

// ---- Tier two: the real library, over a real arrangement and a real catalog --------

TEST_CASE("INTR-1: the Arrangement pane shows what THIS host actually resolved") {
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kArrangementPane);

    // ---- AT THE DEVELOPER'S DEFAULT PANE SIZE, AND THIS IS THE HONEST HALF -------
    //
    // `kStackRows` is nine, so a pane's body is EIGHT prose rows. An artifact's block
    // is three or four, so a default-sized pane shows the count, one artifact, and how
    // many it could not show. That is a real presentation limitation and it is reported
    // rather than papered over: what a maker must never read is a list that looks
    // complete, and what they read here is `... 2 more`.
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        REQUIRE_FALSE(shown.empty());
        CHECK(shown[0] == "3 of 3 artifacts resolved -- 1 providers, 2 weaves");
        CHECK(any_row(shown, "zengine-operators-basic"));
        CHECK(any_row(shown, "... 2 more"));
        CHECK(any_row(shown, intro::kNotAuthored));
    }

    // ---- AND A MAKER WHO WANTS THE WHOLE PROJECT MAKES THE PANE TALLER (WIND-2) ---
    //
    // No pane code changed, no projection changed, and nothing was told: a bigger room
    // is a room grant, which is this tool's one beat.
    make_taller(r, intro::kArrangementPane, 22);
    const std::vector<std::string> shown = pane_rows(r, kind);
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0] == "3 of 3 artifacts resolved -- 1 providers, 2 weaves");
    CHECK(any_row(shown, "zengine-operators-basic"));
    CHECK(any_row(shown, "zengine-plain-weave"));
    CHECK(any_row(shown, intro::kIntrospectionStem));
    CHECK_FALSE(any_row(shown, "more"));
    // ...and the resolved provider identity is the one the ARTIFACT declared about
    // itself, which is nowhere in the plan.
    CHECK(any_row(shown, "zengine.operators.basic"));
    // BOTH KINDS OF FACT, LABELLED, WHERE A MAKER READS THEM.
    CHECK(any_row(shown, "authored  provider normal"));
    CHECK(any_row(shown, "resolved  provider zengine.operators.basic, 2 powers"));
    CHECK(any_row(shown, "authored  weave " + std::string(kIntroOffice)));
    // AND THE FACT IS BOUNDED, AND ITS SOURCE NAMED.
    CHECK(any_row(shown, intro::kNotAuthored));
    CHECK(any_row(shown, "plan: default-load-plan.json"));
}

TEST_CASE("INTR-1: a provider-only artifact is in Arrangement and NOT in Loaded") {
    // ⭐ THE APPARENT DISAGREEMENT, MEASURED. `zengine-operators-basic` is a provider
    // and not a weave: no Kernel loads it, it has no WeaveId and no role. It is a row
    // of one pane and absent from the other, and a build in which both listed it would
    // be a build in which one of them had started guessing.
    PaneRig r;
    const std::int64_t arrangement = open_intro_pane(r, intro::kArrangementPane);
    const std::vector<std::string> project = pane_rows(r, arrangement);
    CHECK(any_row(project, "zengine-operators-basic"));

    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::vector<std::string> loaded = pane_rows(r, intro_row(r, kIntroPane)->kind);
    REQUIRE_FALSE(loaded.empty());
    CHECK_FALSE(any_row(loaded, "zengine-operators-basic"));
    // ...and the Loaded pane is still exactly what it always was: the KERNEL's map.
    CHECK(loaded[0] == "loaded weaves -- 2");
    CHECK(any_row(loaded, std::string(intro::kIntrospectionStem) + " @" + kIntroOffice));
    CHECK(any_row(loaded, intro::kNotInProcess));
    CHECK_FALSE(r.kernel.is_loaded("zengine-operators-basic"));
}

TEST_CASE("INTR-1: THE OVERLAY WITNESS, through the pane a maker actually reads") {
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);

    // ---- BASELINE -------------------------------------------------------------
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        REQUIRE_FALSE(shown.empty());
        CHECK(shown[0] == "2 powers resolve here -- from 1 provider");
        const std::int64_t at = row_with_text(shown, "  math.max");
        REQUIRE(at >= 0);
        CHECK(shown[static_cast<std::size_t>(at) + 1] ==
              "      active    zengine.operators.basic");
        CHECK_FALSE(any_row(shown, "shadowed"));
    }

    // ---- THE OVERLAY, MOUNTED INTO THE HOST'S OWN CATALOG ----------------------
    //
    // NOBODY IS TOLD. No event exists, nothing polls, and the pane's rows are unmoved
    // until it is re-granted room -- which is a resize, which is this tool's one beat.
    const op::MountResult covered =
        op::mount_provider(r.catalog, PROVIDER_MIN_SO, op::MountMode::Overlay);
    REQUIRE_MESSAGE(covered.ok, covered.reason);
    CHECK(pane_rows(r, kind)[0] == "2 powers resolve here -- from 1 provider");

    r.extent(150, 44);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        REQUIRE_FALSE(shown.empty());
        CHECK(shown[0] == "2 powers resolve here -- from 2 providers");
        const std::int64_t at = row_with_text(shown, "  math.max");
        REQUIRE(at >= 0);
        const std::size_t i = static_cast<std::size_t>(at);
        CHECK(shown[i + 1] == "      active    zengine.operators.test.min");
        CHECK(shown[i + 2] == "      shadowed  zengine.operators.basic");
        // AND THE OTHER POWER IS UNTOUCHED, which is what "covers one identity" means.
        const std::int64_t other = row_with_text(shown, "  logic.select_int");
        REQUIRE(other >= 0);
        CHECK(shown[static_cast<std::size_t>(other) + 1] ==
              "      active    zengine.operators.basic");
    }

    // ---- UNMOUNTED: THE ONE UNDERNEATH IS REVEALED ----------------------------
    REQUIRE(r.catalog.unmount("zengine.operators.test.min"));
    r.extent(160, 48);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        REQUIRE_FALSE(shown.empty());
        CHECK(shown[0] == "2 powers resolve here -- from 1 provider");
        const std::int64_t at = row_with_text(shown, "  math.max");
        REQUIRE(at >= 0);
        CHECK(shown[static_cast<std::size_t>(at) + 1] ==
              "      active    zengine.operators.basic");
        CHECK_FALSE(any_row(shown, "shadowed"));
    }
    // NOTHING IN THE PANE'S SOURCE MOVED BETWEEN THOSE THREE READINGS.
}

TEST_CASE("QR-4: the corrected wording reaches a maker's eye WHOLE, off the real canvas") {
    // THE SAME TWO REPAIRS READ BACK FROM THE PUBLISHED CANVAS, because a projection
    // that is right and a pane that is too narrow to say it are not the same result --
    // `fit` would have marked the difference with `...` and no tier-one case could see
    // it. This host resolves two powers from ONE provider, which is the singular fixture.
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);
    const std::vector<std::string> shown = pane_rows(r, kind);
    REQUIRE_FALSE(shown.empty());

    CHECK(shown[0] == "2 powers resolve here -- from 1 provider");
    CHECK_FALSE(any_row(shown, "1 providers"));

    const std::int64_t bound = row_with_text(shown, intro::kHostResolution);
    REQUIRE_MESSAGE(bound >= 0, "rows=", shown.size());
    CHECK(shown[static_cast<std::size_t>(bound)] == intro::kHostResolution);
    CHECK_FALSE(any_row(shown, "took no offer"));
    CHECK_FALSE(any_row(shown, "its own catalog"));
}

TEST_CASE("INTR-1: a provider nobody named appears in the pane with no source edit") {
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);
    CHECK_FALSE(any_row(pane_rows(r, kind), "prov.function"));

    const op::MountResult added =
        op::mount_provider(r.catalog, PROVIDER_A_SO, op::MountMode::Ordinary);
    REQUIRE_MESSAGE(added.ok, added.reason);
    make_taller(r, intro::kPowersPane, 16);

    const std::vector<std::string> shown = pane_rows(r, kind);
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0] == "5 powers resolve here -- from 2 providers");
    for (const char* power : {"prov.function.1", "prov.function.2", "prov.function.3"}) {
        CHECK_MESSAGE(any_row(shown, power), power);
    }
    CHECK(any_row(shown, "zengine.provider.a"));
    // ...AND THE COMPOSITES SAY SO, off the definitions and with nothing here
    // classifying anything. This is the feature: a power added later is a row.
    CHECK(any_row(shown, "(composite)"));
}

TEST_CASE("INTR-1: the two new panes carry no control, and a press changes nothing") {
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);
    const std::vector<std::string> before = pane_rows(r, kind);
    const std::vector<std::string> providers_before = r.catalog.providers();
    const std::vector<std::string> identities_before = r.catalog.identities();
    const std::size_t panels_before = r.session().panels.open.size();

    // KNOWLEDGE DOES NOT GRANT MUTATION. Every prose row of the pane is pressed --
    // including the one naming a provider -- and nothing mounts, unmounts, loads,
    // unloads, opens or closes.
    const ui::Rect body = external_body_rect(r.session(), kind);
    for (std::int64_t row = 0; row < static_cast<std::int64_t>(before.size()); ++row) {
        r.press_cell(body.x + 1, body.y + kExternalHeaderRows + row);
    }
    CHECK(r.catalog.providers() == providers_before);
    CHECK(r.catalog.identities() == identities_before);
    CHECK(pane_rows(r, kind) == before);
    CHECK(r.session().panels.open.size() == panels_before);
    CHECK(r.load_refusals.empty());
    // AND NO ROW OF EITHER PANE OFFERS ONE.
    for (const std::string& row : before) {
        for (const char* verb : {"unmount", "replace", "reload", "disable", "activate",
                                 "[x]", "(x)"}) {
            CHECK(row.find(verb) == std::string::npos);
        }
    }
}

TEST_CASE("INTR-1: opening a pane asks ONCE, and a quiet bus asks nothing") {
    // NO POLLING LOOP, MEASURED FROM THE BUS. The two questions are counted across a
    // long quiet, an open, and a resize: a pane that polled would show a rising count
    // with nothing having happened.
    PaneRig r;
    r.mount_workshop();
    const load::Executed done = r.run_plan(pane_plan());
    REQUIRE_MESSAGE(done.ok, done.refusal);
    const loom::WeaveId door = r.mount_arrangement();

    std::vector<std::string> asked;
    std::vector<std::string> answered;
    const loom::ObserverId tap = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.schema_name == "PowersRequested" || e.schema_name == "ArrangementRequested") {
            asked.push_back(e.schema_name);
        }
        if (e.sender == door && !e.schema_name.empty()) {
            answered.push_back(e.schema_name);
        }
    });
    r.ready();
    r.extent(160, 48);
    for (int turn = 0; turn < 40; ++turn) {
        r.bus.drain_until_idle();
    }
    // NOTHING IS OPEN, SO NOTHING IS ASKED. A tool that read on a beat would already
    // have spoken here.
    CHECK(asked.empty());
    CHECK(answered.empty());

    r.pick(PaneRef{kIntroOffice, intro::kPowersPane});
    const std::size_t after_open = asked.size();
    CHECK(after_open >= 1);
    for (int turn = 0; turn < 40; ++turn) {
        r.bus.drain_until_idle();
    }
    // ...AND A LONG QUIET AFTER IT ASKS NOTHING MORE. The one beat is the room grant.
    CHECK(asked.size() == after_open);
    r.bus.remove_observer(tap);
    CHECK(answered.size() == after_open);
}

TEST_CASE("INTR-1: all three panes may be open at once, each answering its own room") {
    PaneRig r;
    r.mount_workshop();
    const load::Executed done = r.run_plan(pane_plan());
    REQUIRE_MESSAGE(done.ok, done.refusal);
    r.mount_arrangement("p.json");
    r.ready();
    r.extent(240, 60);
    r.pick(intro_ref());
    r.pick(PaneRef{kIntroOffice, intro::kArrangementPane});
    r.pick(PaneRef{kIntroOffice, intro::kPowersPane});

    // THREE ROOMS, THREE QUESTIONS, THREE ANSWERS -- and each projected against ITS
    // OWN budget. One shared room would have made the last grant decide how the other
    // two were drawn.
    const std::vector<std::string> loaded = pane_rows(r, intro_row(r, kIntroPane)->kind);
    const std::vector<std::string> project =
        pane_rows(r, intro_row(r, intro::kArrangementPane)->kind);
    const std::vector<std::string> powers =
        pane_rows(r, intro_row(r, intro::kPowersPane)->kind);
    REQUIRE_FALSE(loaded.empty());
    REQUIRE_FALSE(project.empty());
    REQUIRE_FALSE(powers.empty());
    CHECK(loaded[0].rfind("loaded weaves -- ", 0) == 0);
    CHECK(project[0].find("artifacts resolved") != std::string::npos);
    CHECK(powers[0].find("powers resolve here") != std::string::npos);
    // ...and each stayed inside the room it was granted.
    for (const char* pane : {kIntroPane, intro::kArrangementPane, intro::kPowersPane}) {
        const std::int64_t kind = intro_row(r, pane)->kind;
        const ExternalPane* room = r.session().panels.external_pane(kind);
        REQUIRE(room != nullptr);
        CHECK(static_cast<std::int64_t>(pane_rows(r, kind).size()) <= room->rows);
    }
}

TEST_CASE("INTR-1: the graphical medium grants a different room and both panes spend it") {
    // BOTH MEDIA, ONE PANE SEMANTIC. The provider is handed `rows` and `columns` and
    // never a cell, a pixel, a font or the identity of the medium that answered -- so
    // what differs between a terminal reading and a graphical one is a pair of integers
    // `fit_region` resolved on Workshop's side, and nothing else.
    //
    // The metric arrives as a NUMBER, which is how every medium-dependent claim in this
    // suite since HD-6 has been proved on a lane with no font engine.
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);
    const ExternalPane* cells = r.session().panels.external_pane(kind);
    REQUIRE(cells != nullptr);
    const std::int64_t cell_cols = cells->columns;
    const std::vector<std::string> in_cells = pane_rows(r, kind);
    REQUIRE_FALSE(in_cells.empty());

    // A REAL FACE'S METRIC over the same surface: a 10-pixel advance is more columns in
    // the same rectangle, and an 18-pixel line in a 12-pixel cell is fewer prose rows.
    r.extent(1200, 500, 10, 18);
    const ExternalPane* graphical = r.session().panels.external_pane(kind);
    REQUIRE(graphical != nullptr);
    CHECK(graphical->columns != cell_cols);

    const std::vector<std::string> in_pixels = pane_rows(r, kind);
    REQUIRE_FALSE(in_pixels.empty());
    // THE SAME TRUTH IN BOTH, which is the honesty claim: two projections of one fact.
    CHECK(in_cells[0] == in_pixels[0]);
    CHECK(in_pixels[0] == "2 powers resolve here -- from 1 provider");
    // ...AND THE COUNT IS THE POPULATION'S IN BOTH, whatever the room could fit: a
    // graphical row is taller than a cell, so this projection windows where the cell one
    // did not -- and every power it could not name is counted on its own row.
    CHECK(any_row(in_pixels, intro::kHostResolution));
    if (!any_row(in_pixels, "zengine.operators.basic")) {
        CHECK(any_row(in_pixels, "more"));
    }
    // ...AND THE VIEW ANSWERED THE NEW ROOM rather than the old one. Workshop clears its
    // cache before every grant, so a projection that had not moved would read as
    // `waiting` here instead.
    CHECK(static_cast<std::int64_t>(in_pixels.size()) <= graphical->rows);
    for (const std::string& row : in_pixels) {
        CHECK(static_cast<std::int64_t>(row.size()) <= graphical->columns);
    }
}

TEST_CASE("INTR-1: a host with no arrangement door leaves the two panes WAITING") {
    // THE TOOL IS LOADABLE INTO ANY LOOM HOST, and only one in this repository answers
    // for its own project. A host that mounts no door holds no `zengine.arrangement`
    // office, the ask reaches nobody, and the pane says the honest thing -- never
    // `unavailable`, which is a fate nothing here has observed.
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.ready();
    r.extent(160, 48); // room for two stacked panes, so both are really on the canvas
    r.pick(PaneRef{kIntroOffice, intro::kArrangementPane});
    const std::vector<std::string> shown =
        pane_rows(r, intro_row(r, intro::kArrangementPane)->kind);
    REQUIRE(shown.size() == 1);
    CHECK(shown[0] == std::string(kExternalWaiting));
    // ...and the Loaded pane, whose owner is the Kernel and is always there, is fine.
    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::vector<std::string> loaded = pane_rows(r, intro_row(r, kIntroPane)->kind);
    REQUIRE_FALSE(loaded.empty());
    CHECK(loaded[0] == "loaded weaves -- 1");
}

// ---- Tier three: Workshop, read as a source file -----------------------------------

TEST_CASE("INTR-1: Workshop knows no pane, and the two new ones are no exception") {
    // DEFENCE IN DEPTH, AND SAID TO BE. Every rig above drives the real seam; what a
    // source read adds is that "Workshop discovers panes rather than being taught
    // them" cannot quietly stop being true while every rig stays green.
    //
    // THE FORBIDDEN FORMS ARE QUOTED LITERALS AND IDENTIFIERS, never bare words. Prose
    // about a pane is not a branch on one, and a tripwire that could not tell the
    // difference would be a tripwire that forbids explaining the code.
    for (const char* path : {WORKSHOP_HOST_CPP, WORKSHOP_WEAVE_HPP, WORKSHOP_SCREEN_HPP,
                             WORKSHOP_PANEL_HPP}) {
        const std::string source = file_source(path);
        for (const char* forbidden : {"\"arrangement\"", "\"powers\"", "\"loaded\"",
                                      "kArrangementPane", "kPowersPane", "kLoadedPane",
                                      "introspection/", "kIntrospectionRole"}) {
            CHECK_MESSAGE(source.find(forbidden) == std::string::npos, path, " names '",
                          forbidden, "'");
        }
    }
    // ...AND NO `panel::k*` WAS MINTED FOR ANY OF THEM. Every handle these panes carry
    // is a runtime one, minted from a live offer.
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(r.session().panels.runtime.entries.size() == kIntroPaneCount);
    for (const RuntimePane& row : r.session().panels.runtime.entries) {
        CHECK(is_runtime_kind(row.kind));
    }
}

TEST_CASE("BLD-2: the presentation holds no realization or build-runner reach") {
    // THE FRONTIER MADE THE PRESENTATION AWARE OF REALIZATION, and this is the wire
    // that keeps "aware" from quietly becoming "in charge". What the weave holds is
    // one host-wired READING (`HostContext::frontier`); the one route from a maker's
    // gesture to a realized artifact is still `BuildRequested` to the Builder office,
    // and every rig above stays green if a shortcut arrives -- only a source read can
    // say the shortcut is not there.
    //
    // THE FORBIDDEN FORMS ARE IDENTIFIERS, never bare words: the owner's type, its
    // file, the offer only the TOOL may make, the runner's own order, and the runner's
    // office. A presentation that could spell any of them is a presentation one edit
    // away from building or realizing on its own authority.
    for (const char* path : {WORKSHOP_WEAVE_HPP, WORKSHOP_SCREEN_HPP, WORKSHOP_PANEL_HPP}) {
        const std::string source = file_source(path);
        for (const char* forbidden : {"PlanExecutor", "load_execute", "OfferArtifact",
                                      "RunBuild", "kBuildRunnerRole"}) {
            CHECK_MESSAGE(source.find(forbidden) == std::string::npos, path, " names '",
                          forbidden, "'");
        }
    }
}

TEST_CASE("INTR-1: the host mounts a door and injects no host-owned object into an artifact") {
    const std::string host = file_source(WORKSHOP_HOST_CPP);

    // THE DOOR IS MOUNTED BEFORE THE PLAN IS PERFORMED, because the tool that asks is
    // an artifact THIS PLAN LOADS: a door mounted afterwards would be absent during the
    // window in which a pane might first be granted room.
    const std::size_t door = host.find("mount_in_office<ArrangementDoor>");
    const std::size_t begin = host.find("executor.begin(read_plan.plan)");
    REQUIRE(door != std::string::npos);
    REQUIRE(begin != std::string::npos);
    CHECK(door < begin);
    // ...AND THE HOST NO LONGER BLOCKS UNTIL THE PROJECT IS REALIZED (BOOT-0). The
    // line that used to be here returned only once every row had settled.
    CHECK(host.find("executor.run(") == std::string::npos);

    // ...AND ITS GRANT IS THE TWO ANSWERS AND NOTHING ELSE.
    CHECK(host.find("say_resolved.allow_to_any(ResolvedArrangement::zen_name") !=
          std::string::npos);
    CHECK(host.find("say_resolved.allow_to_any(ResolvedPowers::zen_name") != std::string::npos);
    CHECK(host.find("say_resolved.allow(") == std::string::npos);

    // NOTHING HOST-OWNED CROSSES INTO A LOADED ARTIFACT. The host's operator surface is
    // handed to the offer machinery and nowhere else; there is no second injected
    // capability, no `ZenHostApi` widening and no service locator.
    for (const char* forbidden : {"ZengineEverythingHostApi", "service_locator",
                                  "IntrospectionRegistry", "ProjectMirror", "OperatorMirror"}) {
        CHECK_MESSAGE(host.find(forbidden) == std::string::npos, forbidden);
    }
}

TEST_CASE("INTR-1: neither projection names a power, a provider or an artifact") {
    // ⭐ THE GENERICITY CLAIM, READ OFF THE SOURCE. A test may name `math.max` because
    // it is verifying known production state; the projection may not, because a
    // provider added later must appear without an edit. That is the feature.
    //
    // QUOTED LITERALS AND IDENTIFIERS, for the tripwire above's reason exactly: these
    // files EXPLAIN what they refuse to branch on, and a check that could not tell a
    // sentence from a special case would forbid the explanation.
    for (const char* path : {INTROSPECTION_RESOLVED_HPP, WORKSHOP_ARRANGEMENT_HPP}) {
        const std::string source = file_source(path);
        for (const char* forbidden : {"\"math.max\"", "\"logic.select_int\"",
                                      "\"timer.normalize_delay\"", "\"zengine.operators",
                                      "\"zengine.timer\"", "\"zengine-timer\"",
                                      "\"zengine-operators-basic\"", "kMaxInt", "kSelectInt",
                                      "kNormalizeDelay", "timer/normalize.hpp",
                                      "operator/primitives.hpp"}) {
            CHECK_MESSAGE(source.find(forbidden) == std::string::npos, path, " names '",
                          forbidden, "'");
        }
    }
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
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
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
    REQUIRE(rows.size() == 5);
    CHECK(rows[4].row->act == Act::kManageRemove);
}

TEST_CASE("CTX-0: input spent on the open surface reaches no provider") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panels.runtime.entries[0].kind;
    const ui::Rect panel = cells_covered(external_panel_rect(r.session(), kind));
    r.right_press_cell(panel.x + 1, panel.y + 2);
    REQUIRE(r.session().context.open);
    const std::size_t presses_before = seat->presses.size();

    // The surface's column and the pane's slot share cells; a primary press there is the
    // surface's to spend while it is open, and the provider hears nothing of it. The
    // heading row: the surface's own furniture, consumed silently.
    r.press_cell(panel.x + 1, panel.y);
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
