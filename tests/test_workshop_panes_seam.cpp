// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- the protocol and the provider seam: what an office may offer, who may
// speak for it, how Workshop discovers it, where its area stands and the room the window gives
// it, and how a pane ends. One source of the panes program, run in `workshop_panes_seam`, whose units split by subject
// where one object cannot hold them all (VM-POP-12); a new case goes to its subject's. Cases drive
// the real weave on a real bus against real loaded artifacts, from `PaneRig`
// (`workshop_support.hpp`).

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"
#include "office_manual.hpp"
#include "pane_window.hpp"

// ============================================================================
// The office authors the pane; the window gives its area the room
// ============================================================================
// Against: PROVENANCE (no payload carries a provider; the office is Loom's stamp, so personal
// speech, another office and a forged room are the negatives); BOUNDS (each number has a case
// one past it); SILENCE (unavailable is never said); THE REAL SEAM (an attested real library).

// ---- The protocol itself ------------------------------------------------------

TEST_CASE("the pane protocol's offer and its catalog ask carry no provider") {
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

    const std::shared_ptr<const loom::Schema> ask = loom::schema_of<PaneCatalogRequested>();
    REQUIRE(ask != nullptr);
    CHECK(ask->name() == "PaneCatalogRequested");
    CHECK(ask->version() == 1);
    CHECK(ask->fields().empty()); // a filter would be a policy nobody asked for
}

TEST_CASE("a pane's offer round-trips through the wire with its semantics intact") {
    PaneOffered offer{"hello", "Hello", "a bounded external greeting"};
    const PaneOffered offer_back = loom::from_value<PaneOffered>(loom::to_value(offer));
    CHECK(offer_back.pane == "hello");
    CHECK(offer_back.name == "Hello");
    CHECK(offer_back.summary == "a bounded external greeting");
}

// ---- The office's manual -------------------------------------------------------

TEST_CASE("Workshop's office answers every shape it accepts from its manual, and the page names no other") {
    // Asked as any weave asks an office (manual/docs/manual.md#an-office-answers-for-a-shape):
    // every version Workshop's gate accepts is answered from a section under Commands or Hears,
    // and every such section is about a shape the gate lets in.
    PaneRig r;
    r.mount_workshop();
    const zengine::manual::Manual& page = zengine::manual::pages::zengine_workshop::kManual;
    CHECK(std::string(page.office) == kWorkshopProvider);
    const std::vector<std::string> accepted =
        described_shapes_of(r.bus, r.bus.role_holder(kWorkshopProvider));
    REQUIRE(accepted.size() >= 60);
    CHECK(check_office_manual(r.bus, kWorkshopProvider) >= accepted.size());
    check_page_names_accepted(page, accepted);
}

// ---- Descriptor law and the catalog bound ---------------------------------------

TEST_CASE("a valid offer is admitted under the office that stamped it") {
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
    const Admission a = admit_pane_offer(cat, kHelloOffice, good_offer());
    REQUIRE(a.written.accepted);
    CHECK_FALSE(a.refreshed);
    REQUIRE(cat.entries.size() == 1);
    CHECK(cat.entries[0].provider == std::string(kHelloOffice));
    CHECK(cat.entries[0].pane == "hello");
    CHECK(cat.entries[0].name == "Hello");
    CHECK(cat.entries[0].summary == "a bounded external greeting");
    // THE HANDLE IS SESSION-LOCAL AND OUTSIDE THE BUILT-IN NUMBER SPACE, which is
    // what stops it reaching `builtin_pane`'s Builder fall-through.
    CHECK(is_runtime_kind(a.kind));
    CHECK(a.kind >= kFirstRuntimeKind);
    CHECK(cat.entries[0].kind == a.kind);
    // AND THE DURABLE IDENTITY IS THE STAMPED OFFICE PLUS THE PAYLOAD'S PANE KEY.
    CHECK(resolve_pane(hello_ref(), panes).value_or(0) == a.kind);
}

TEST_CASE("an offer with no stamped office is refused, and retains nothing") {
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
    const Admission a = admit_pane_offer(cat, "", good_offer());
    CHECK_FALSE(a.written.accepted);
    CHECK(cat.entries.empty());
}

TEST_CASE("a descriptor's name and summary are bounded, and a refusal keeps nothing") {
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
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
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
    // THE SAME `check_pane_key` THE PERSISTED GRAMMAR USES, and it is the same
    // function rather than a second one: a runtime key that a saved setup could not
    // spell would be an identity a weaver could never keep.
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
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
    // Offered by whoever holds `zengine.workshop`, `layouts` names the row this build compiled
    // in -- and a live message may not move it. The forgery this refuses has to be one of the
    // rows this host still has.
    CHECK(admit_pane_offer(cat, kWorkshopProvider,
                           PaneOffered{pane_key::kLayouts, "Not Layouts", "a forgery"})
              .written.refusal == "`zengine.workshop/layouts` is a built-in pane");
    CHECK(cat.entries.empty());
    // ...and the built-in still resolves to itself.
    CHECK(resolve_pane(PaneRef{kWorkshopProvider, pane_key::kLayouts}, panes).value_or(-1) ==
          pane_kind::kLayouts);

    // A DIFFERENT OFFICE OFFERING THE SAME PANE KEY IS A DIFFERENT PANE, and is
    // admitted normally -- the `PaneRef` is the PAIR.
    CHECK(admit_pane_offer(cat, kHelloOffice, PaneOffered{pane_key::kLayouts, "Layouts", "theirs"})
              .written.accepted);
    REQUIRE(cat.entries.size() == 1);
    CHECK(is_runtime_kind(cat.entries[0].kind));
}

TEST_CASE("re-offering one reference refreshes it in place and grows nothing") {
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
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
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
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
    CHECK(resolve_pane(PaneRef{kHelloOffice, "hello"}, panes).value() == a.kind);
    CHECK(resolve_pane(PaneRef{kOtherOffice, "hello"}, panes).value() == b.kind);
}

TEST_CASE("the combined catalog stops at its bound, built-ins included") {
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
    // THE BOUND LESS THE BUILT-INS IN RUNTIME ROWS, because the built-ins are part of the total.
    for (std::size_t i = 0; i < kMaxPaneCatalogEntries - kBuiltinPaneCount; ++i) {
        const Admission a = admit_pane_offer(
            cat, kHelloOffice, PaneOffered{"pane" + std::to_string(i), "P", "one of many"});
        INFO("offer ", i);
        REQUIRE(a.written.accepted);
    }
    CHECK(cat.entries.size() == kMaxPaneCatalogEntries - kBuiltinPaneCount);
    CHECK(cat.entries.size() + kBuiltinPaneCount == kMaxPaneCatalogEntries);

    // THE ROW PAST THE BOUND IS REFUSED, VISIBLY, AND CHANGES NOTHING.
    const Admission over =
        admit_pane_offer(cat, kHelloOffice, PaneOffered{"one-too-many", "Extra", "over"});
    CHECK_FALSE(over.written.accepted);
    CHECK(over.written.refusal ==
          "Workshop holds at most " + std::to_string(kMaxPaneCatalogEntries) +
              " panes -- `zengine.test.workshop-hello/one-too-many` was not added");
    CHECK(cat.entries.size() == kMaxPaneCatalogEntries - kBuiltinPaneCount);
    CHECK_FALSE(resolve_pane(PaneRef{kHelloOffice, "one-too-many"}, panes).has_value());

    // ...AND AN EXISTING ENTRY MAY STILL BE REFRESHED WHILE FULL. The bound is on how
    // many DISTINCT panes are held, not on how often a provider may correct itself.
    const Admission refresh =
        admit_pane_offer(cat, kHelloOffice, PaneOffered{"pane0", "P0", "corrected"});
    CHECK(refresh.written.accepted);
    CHECK(refresh.refreshed);
    CHECK(cat.entries.size() == kMaxPaneCatalogEntries - kBuiltinPaneCount);
    CHECK(cat.find(kHelloOffice, "pane0")->summary == "corrected");
}

TEST_CASE("the runtime catalog is beside the compile-time one and never inside it") {
    // THE BUILT-IN HALF IS MEASURED, NOT COUNTED. What is claimed is an ORDER: every
    // compile-time row, in the catalog's own order, then the runtime rows AFTER them. A third
    // built-in satisfies that for free, and nothing below is told how many built-ins there are
    // or what any of them is called -- naming them by hand would be a catalog census.
    Panes bare;
    const std::vector<CatalogRow> before = combined_catalog(bare);
    REQUIRE(before.size() == kBuiltinPaneCount);
    // THE PRIOR FACT IS ANCHORED IN THE CONSTANT ARRAY rather than in the function under
    // test, so the comparison further down is not `combined_catalog` agreeing with itself.
    for (std::size_t i = 0; i < kBuiltinPaneCount; ++i) {
        INFO("built-in row ", i);
        CHECK(before[i].kind == kBuiltinPanes[i].kind);
        CHECK(before[i].ref == PaneRef{kBuiltinPanes[i].provider, kBuiltinPanes[i].pane});
        CHECK(before[i].name == kBuiltinPanes[i].name);
        CHECK(before[i].summary == kBuiltinPanes[i].summary);
        CHECK_FALSE(is_runtime_kind(before[i].kind));
    }

    Panes panes;
    REQUIRE(admit_pane_offer(panes.runtime, kHelloOffice, good_offer()).written.accepted);
    const std::vector<CatalogRow> rows = combined_catalog(panes);
    REQUIRE(rows.size() == kBuiltinPaneCount + 1);

    // BUILT-INS FIRST AND UNTOUCHED -- an EXACT PREFIX of what the catalog offered before
    // any offer arrived: same rows, same fields, same order. A runtime offer is not an edit
    // to the constant array, so it cannot replace a built-in row, rewrite one, or be
    // interleaved among them; it can only follow them.
    for (std::size_t i = 0; i < kBuiltinPaneCount; ++i) {
        INFO("built-in row ", i);
        CHECK(rows[i].kind == before[i].kind);
        CHECK(rows[i].ref == before[i].ref);
        CHECK(rows[i].name == before[i].name);
        CHECK(rows[i].summary == before[i].summary);
        CHECK_FALSE(is_runtime_kind(rows[i].kind));
    }

    // ...THEN THE RUNTIME ROWS, IN FIRST-ACCEPTED-OFFER ORDER, BEGINNING AT `kBuiltinPaneCount`.
    CHECK(rows[kBuiltinPaneCount].ref == hello_ref());
    CHECK(rows[kBuiltinPaneCount].name == "Hello");
    CHECK(rows[kBuiltinPaneCount].summary == "a bounded external greeting");
    CHECK(is_runtime_kind(rows[kBuiltinPaneCount].kind));
    CHECK(rows[kBuiltinPaneCount].kind == panes.runtime.entries[0].kind);

    // AND EXACTLY ONE ROW OF THE COMBINED POPULATION IS A RUNTIME ONE, so `kBuiltinPaneCount` is
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
    // `RuntimeCatalog::find` takes two `std::string_view`s so that a door can ask
    // WHO THIS IS with Loom's stamp exactly as it arrived, owning nothing to do it. It compares
    // against the row's own string -- admitted under `check_pane_key` and owned by the vector --
    // so the comparison moves no ownership in either direction.
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
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
    Panes panes;
    RuntimeCatalog& cat = panes.runtime;
    REQUIRE(admit_pane_offer(cat, kHelloOffice, good_offer()).written.accepted);
    const std::int64_t hello = cat.entries[0].kind;

    // THE NEGATIVE CONTROL THE FALLIBLE DOOR EXISTS FOR, said about a runtime kind.
    CHECK_FALSE(resolve_pane(PaneRef{kHelloOffice, "never-offered"}, panes).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{"nobody", "hello"}, panes).has_value());
    CHECK_FALSE(resolve_builtin_pane(hello_ref()).has_value());

    // AND `placement_of` DOES NOT REACH THE CATALOG'S FIRST ROW FOR A RUNTIME KIND. The total
    // lookup still answers that row for an unknown kind. Which row that is, is an accident of
    // order, asserted as one: Layouts, placed in the TOP BAND, so the two lines below disagree on
    // purpose -- a `placement_of` that reached `builtin_pane` for a runtime handle would put a
    // stranger's pane in the band. The control that survives any reordering is that
    // `placement_of` branches on `is_runtime_kind` BEFORE it reaches `builtin_pane` at all.
    CHECK(builtin_pane(hello).kind == kBuiltinPanes[0].kind); // the fall-through, still total
    CHECK(builtin_pane(hello).placed_in == placement::kTopBand);
    CHECK(placement_of(hello) == placement::kOverlayStack); // ...and a runtime kind never gets there
    // AND NO KIND ANSWERS `kSideRegion`: the right column is a place a DESK names rather than a
    // kind's default (`kinds_placed_in(kSideRegion) == 0`, panes.hpp); the one built-in is the
    // band's.
    CHECK(placement_of(pane_kind::kLayouts) == placement::kTopBand);
    // ...and the NAME a weaver reads is the offered one rather than the fall-through's.
    CHECK(kind_name(panes, hello) == "Hello");
    CHECK(kind_name(panes, pane_kind::kLayouts) == "Layouts");
    CHECK(kind_name(panes, 9999).empty());
}

// ---- Provenance: the office authors, and holding is not speaking-for --------------

TEST_CASE("a personal offer from the actual role holder registers nothing") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);

    // THE SHARPEST NEGATIVE HERE. This weave HOLDS `zengine.test.workshop-hello` at this instant
    // -- Loom would confirm it -- and it speaks with `mail.send_to_role`, which is personal speech.
    // `authored_role()` is empty, so there is no office to derive a `PaneRef`'s provider half
    // from, and the offer is not a fact about anybody's arrangement.
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer_personally(m, good_offer()); });
    CHECK(r.session().panes.runtime.entries.empty());
    CHECK(combined_catalog(r.session().panes).size() == kBuiltinPaneCount);

    // THE SAME SENTENCE, DELIBERATELY AUTHORED, IS ADMITTED. One `as_role` apart.
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    REQUIRE(r.session().panes.runtime.entries.size() == 1);
    CHECK(r.session().panes.runtime.entries[0].provider == std::string(kHelloOffice));
}

TEST_CASE("an office longer than the key bound is delivered whole and admitted by nobody") {
    // THE CASE THAT SAYS THE BOUNDARY IS REACHABLE. Loom preserves a role name of any length
    // across a dynamic seam, and proves it past two hundred bytes -- so sixty-four bytes is THIS
    // application's law, which nothing in the substrate enforces. The honest way to ask is to seat
    // a real weave in a real office too long for that law and have it author a valid offer.
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
        // NOT `far`: it is an empty macro in the Windows SDK's `minwindef.h`, so a variable of
        // that name vanishes mid-declaration under MSVC.
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
    const std::size_t panes_before = r.session().panes.open.size();
    const Setup setup_before = r.session().setup.active;

    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    // AND WORKSHOP HELD ITS OWN LINE.
    CHECK(r.session().panes.runtime.entries.empty()); // NOTHING WAS ADMITTED
    CHECK(combined_catalog(r.session().panes).size() == kBuiltinPaneCount);
    CHECK(r.session().panes.open.size() == panes_before); // no pane moved
    CHECK(r.session().setup.active == setup_before);        // and no authored intent did

    // THE REFUSAL IS THE EXISTING PROVIDER-KEY BYTE LAW, in its own wording.
    CHECK(r.last_notice() == "a pane reference's provider is at most 64 bytes");
    // AND THE UNVALIDATED OFFICE IS NOT IN IT. A notice that echoed the bytes it had
    // just refused would put an unbounded stranger's string on a weaver's one line.
    CHECK(r.last_notice().find(long_office) == std::string::npos);
    CHECK(r.last_notice().find("zzzz") == std::string::npos);

    // THE SAME OFFICE, ONE BYTE SHORTER THAN THE BOUND, IS ADMITTED -- so what was
    // refused was the length and not the office being a stranger.
    ProviderSeat* fits = r.mount_provider(std::string_view(long_office.data(), kMaxPaneKeyLen));
    r.drive(fits, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    REQUIRE(r.session().panes.runtime.entries.size() == 1);
    CHECK(r.session().panes.runtime.entries[0].provider.size() == kMaxPaneKeyLen);
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
    const RuntimeCatalog& cat = r.session().panes.runtime;
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

// ---- Discovery, through the REAL dynamic seam -------------------------------------

TEST_CASE("Workshop first, then the provider: an attested activation announces the pane") {
    PaneRig r;
    r.mount_workshop();
    // Workshop asks before anybody can answer. The ask reaches nobody and is gone --
    // nothing is retried, buffered or queued for a provider that does not exist yet.
    r.ready();
    CHECK(r.session().panes.runtime.entries.empty());

    // THE REAL LIBRARY, THROUGH THE REAL KERNEL AND MANAGER. Loom sends the freshly
    // committed incarnation an ATTESTED `zen.Activated`, `ActivationCursor` accepts it,
    // and the provider announces.
    const loom::WeaveId hello =
        r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    REQUIRE(r.load_refusals.empty());
    REQUIRE(hello.valid());
    REQUIRE(r.session().panes.runtime.entries.size() == 1);
    CHECK(r.session().panes.runtime.entries[0].provider == std::string(kHelloOffice));
    CHECK(r.session().panes.runtime.entries[0].pane == std::string(kHelloPane));
    CHECK(r.session().panes.runtime.entries[0].name == "Hello");
    CHECK(r.session().panes.runtime.entries[0].summary == "a bounded external greeting");
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
    CHECK(r.session().panes.runtime.entries.empty()); // the first offer really was lost

    // ...and Workshop's own startup ask brings it back, because the provider verifies
    // the authorship and re-offers.
    r.ready();
    REQUIRE(r.session().panes.runtime.entries.size() == 1);
    CHECK(r.session().panes.runtime.entries[0].name == "Hello");
}

TEST_CASE("asking twice and announcing twice still yields one catalog row") {
    PaneRig r;
    r.mount_workshop();
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    REQUIRE(r.session().panes.runtime.entries.size() == 1);
    const std::int64_t handle = r.session().panes.runtime.entries[0].kind;

    r.ready();
    r.ready();
    r.ready();
    // REPETITION IS HARMLESS BY CONSTRUCTION: identity does the de-duplication, so the
    // protocol needs none of its own.
    REQUIRE(r.session().panes.runtime.entries.size() == 1);
    CHECK(r.session().panes.runtime.entries[0].kind == handle);
    CHECK(combined_catalog(r.session().panes).size() == kBuiltinPaneCount + 1);
}

TEST_CASE("the provider answers Workshop and nobody else") {
    PaneRig r;
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.mount_workshop();
    REQUIRE(r.session().panes.runtime.entries.empty());

    // AN UNAUTHENTICATED CATALOG REQUEST -- a root publication, carrying no authored
    // role at all. A provider that answered this would hand its catalog to whoever
    // asked, including a weave holding no office.
    (void)r.bus.publish(loom::Message(loom::to_value(PaneCatalogRequested{}), loom::WeaveId{},
                                      loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
    CHECK(r.session().panes.runtime.entries.empty());

    // The same shape, authored as `zengine.workshop`, is answered.
    r.ready();
    CHECK(r.session().panes.runtime.entries.size() == 1);
}

TEST_CASE("a forged room grants the provider nothing") {
    // THE PROVIDER'S OWN CHECK, MEASURED FROM THE OTHER SIDE. A watcher holds
    // `zengine.workshop` to hear the offer; a room no window's office authored -- one sent with no
    // office at all -- draws nothing, and the room a window gives its area is drawn there.
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    const loom::WeaveId hello = r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    REQUIRE(hello.valid());
    REQUIRE(watch->offers.size() == 1); // the activation announcement landed here
    zengine::testing::pane_window::Window window(r.bus);
    const std::size_t before = window.weave().heard.size();

    r.bus.send(hello, loom::Message(loom::to_value(gfx::AreaRoom{kHelloOffice, window.name(), 1, 20 * gfx::kCellPx,
                                                                 4 * gfx::kCellPx, {}, true}),
                                    loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
    CHECK(window.weave().heard.size() == before); // no room was believed, so nothing was drawn

    window.room(kHelloOffice, kHelloOffice, 20, 4);
    r.bus.drain_until_idle();
    const std::vector<std::string> rows = window.rows(kHelloOffice);
    REQUIRE(rows.size() == 4);
    CHECK(rows[0] == "hello -- 4x20");
}

// ---- Setup resolution: an unchanged reference, resolved later ---------------------

TEST_CASE("an authored external reference is unresolved until its office offers it") {
    PaneRig r;
    r.mount_workshop();
    // The weaver authored this before any provider existed -- which the setup grammar allows,
    // and a pane protocol consumer resolves.
    REQUIRE(add_pane(r.session().setup.active, hello_ref()));
    link_live_setup(r.session().setup, "setup.json");
    r.key(input::scan::kP);
    r.key(input::scan::kEscape); // a repaint, so the setup line is current

    // TWO: the reference this case authored, and the Info row the shipped desk names -- Info
    // is a weave and no office has offered it in this rig.
    CHECK(unresolved_panes(r.session().setup.active, r.session().panes).size() == 2);
    CHECK(setup_rest_text(r.session().setup, r.session().panes,
                            r.session().keymap, KeySpelling::kShort)
              .find("2 unresolved") != std::string::npos);
    CHECK_FALSE(r.session().panes.has(kFirstRuntimeKind));

    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    // THE SAME UNCHANGED REFERENCE NOW RESOLVES, and reconciliation opened it through
    // the one path -- the file was not touched and no second door exists.
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;
    CHECK(r.session().setup.active.panes.back().ref == hello_ref());
    CHECK(r.session().panes.has(kind));
    CHECK(unresolved_panes(r.session().setup.active, r.session().panes).size() == 1);
    // AND A PANE A WEAVER CAN SEE IS NOT COUNTED AS UNRESOLVED BENEATH IT: the count went from
    // two to one, and the one that remains is the shipped desk's Info row.
    CHECK(setup_rest_text(r.session().setup, r.session().panes,
                            r.session().keymap, KeySpelling::kShort)
              .find("1 unresolved") != std::string::npos);
    // THE SETUP IS STILL SAVED: resolving is not an edit.
    CHECK((live_status(r.session().setup) == setup_link::kCurrent));
}

TEST_CASE("a fresh session with no provider leaves the same reference unresolved again") {
    // A SECOND PROCESS, spelled the only way a suite can: a second everything. The
    // runtime catalog is session state and earns nothing from the last run.
    Setup saved;
    saved.name = "Future";
    REQUIRE(add_pane(saved, info_ref()));
    REQUIRE(add_pane(saved, hello_ref()));
    REQUIRE(check_setup(saved).accepted);

    PaneRig fresh;
    fresh.mount_workshop();
    fresh.session().setup.active = saved;
    fresh.session().setup.active_link = SetupLink{"setup.json", saved};
    CHECK(fresh.session().panes.runtime.entries.empty());
    const std::vector<PaneRef> waiting =
        unresolved_panes(saved, fresh.session().panes);
    REQUIRE(waiting.size() == 2);
    CHECK(waiting[0] == info_ref()); // the shipped desk's row, offered by no office here
    CHECK(waiting[1] == hello_ref());
    // NOT DROPPED, NOT REMAPPED, AND NOT CALLED UNAVAILABLE. Workshop knows it has no
    // row for the reference and knows nothing at all about whoever could present it.
    CHECK(fresh.session().setup.active.panes.size() == 2);
    CHECK(fresh.session().setup.active.panes[1].ref == hello_ref());
    CHECK(setup_rest_text(fresh.session().setup, fresh.session().panes,
                            fresh.session().keymap, KeySpelling::kShort)
              .find("unavailable") == std::string::npos);
}

TEST_CASE("setup bytes carry no descriptor, room or handle") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    SeatRows said;
    said.pane = kHelloPane;
    said.rows.push_back(TextRow{"cached prose", gfx::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    REQUIRE_FALSE(held_row_texts(*r.session().panes.external_pane(r.session().panes.runtime.entries[0].kind))
                      .empty());

    const std::string text = setup_persist::to_text(r.session().setup.active);
    // THE FILE IS THE AUTHORED REFERENCE AND THE AUTHORED WINDOW, and nothing else: a live offer's
    // descriptor, its granted room and its session handle reach no byte of it, and authored
    // INTENT beside the reference moved that line not at all -- which the assertions measure.
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
    CHECK(setup_persist::kFormatVersion == 5);
}

// ---- Runtime spatial capacity -----------------------------------------------------

TEST_CASE("the overlay floor is the workspace's own bottom, which is the band's top row") {
    // THE BOUNDARY, STATED IN BOTH SPELLINGS AND MEASURED AGAINST THE COMPOSITION. A slot allowed
    // past it erases the row the tool speaks in -- the NOTICE's row.
    for (std::int64_t h : {22, 23, 24, 30, 40, 60}) {
        const Screen sc = screen_of(cells_px(78), cells_px(h));
        INFO("height ", h);
        CHECK(kRoomCellY + cells_of(sc).room_h == cells_of(sc).notice_y);
        const std::size_t fits = stack_slots_that_fit(sc);
        for (std::size_t slot = 0; slot < fits; ++slot) {
            const ui::Rect b = cells_covered(placement_bounds(placement::kOverlayStack, slot, sc));
            CHECK(b.y + b.h <= kRoomCellY + cells_of(sc).room_h);
        }
        const ui::Rect over = cells_covered(placement_bounds(placement::kOverlayStack, fits, sc));
        CHECK(over.y + over.h > kRoomCellY + cells_of(sc).room_h);
    }
    CHECK(stack_slots_that_fit(kMinScreen) == 1);
    CHECK(stack_slots_that_fit(screen_of(cells_px(78), cells_px(42))) >= 2);
}

TEST_CASE("a pane launched when the stack's column is spent lands at the column's top, in front") {
    // ⚔ MUTATION: the launch judged by a stack rationed to its column's height -- "no room for
    // Other", and the second pane is never seen.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t hello = r.session().panes.runtime.entries[0].kind;
    REQUIRE(r.session().panes.has(hello));
    const Screen sc = screen_of(r.session());
    REQUIRE(stack_slots_that_fit(sc) == 1); // the minimum composition's column holds one
    const PixelRect first = bounds_of(r.session().panes, r.session().setup.active, hello, sc).rect;
    REQUIRE(first.y == sc.room_y);
    // A SECOND PROVIDER OFFERING A SECOND STACK PANE, launched as the Pane Manager, a launch key
    // or `n` launches one: through the launch door.
    ProviderSeat* other = r.mount_provider(kOtherOffice);
    r.drive(other, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"other", "Other", "a second stack pane"});
    });
    const PaneRef other_ref{kOtherOffice, "other"};
    const PaneLaunchAnswered said = hand_launch(r, other_ref);
    CHECK(said.refusal.empty());
    CHECK(said.opened);
    CHECK(r.last_notice() == "showed Other");
    REQUIRE(has_pane(r.session().setup.active, other_ref));
    const std::int64_t second = r.session().panes.runtime.entries[1].kind;
    REQUIRE(r.session().panes.has(second));
    // THE COLUMN BEGINS AGAIN AT ITS TOP, so the second pane stands where the first does, in
    // front of it and wholly in sight; the first did not move.
    const PixelRect landed = bounds_of(r.session().panes, r.session().setup.active, second, sc).rect;
    CHECK(landed.x == first.x);
    CHECK(landed.y == sc.room_y);
    CHECK(landed.h > 0);
    CHECK(effective_pane_order(r.session().setup.active, r.session().panes).back() == second);
    CHECK(bounds_of(r.session().panes, r.session().setup.active, hello, sc).rect == first);
    const auto state_of = [&r, &sc](const PaneRef& ref) {
        for (const CatalogRow& row : inventory_rows(r.session().setup.active, r.session().panes)) {
            if (row.ref == ref) {
                return std::string(
                    pane_state_word(pane_state_of(r.session().panes, r.session().setup.active, sc, row)));
            }
        }
        return std::string("absent");
    };
    CHECK(state_of(other_ref) == "open");
    // NO PANE INTERSECTS THE SETUP ROW OR THE BOTTOM BAND.
    for (const OpenPane& p : r.session().panes.open) {
        const ui::Rect b =
cells_covered(bounds_of(r.session().panes, r.session().setup.active, p.kind, sc).rect);
        INFO("kind ", p.kind);
        CHECK(b.y + b.h <= kRoomCellY + cells_of(sc).room_h);
        CHECK(b.y + b.h <= cells_of(sc).notice_y); // the band's first row is not the pane's
    }
}

TEST_CASE("a pane launched where its own place stands off this screen stays there, and the band says so and how to bring it back") {
    // ⚔ MUTATION: a launch of a pane already on the desk that focuses it and says nothing -- the
    // keys go to a pane no one can see, and nothing says where it went.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t hello = r.session().panes.runtime.entries[0].kind;
    REQUIRE(r.session().panes.has(hello));
    // ITS OWN PLACE, past the room's right edge.
    REQUIRE(author_pane_place(r.session().setup.active, hello_ref(), cells_px(300), 0).accepted);
    r.key(input::scan::kUnknown); // a delivery, so the desk claims what it now is
    const Screen sc = screen_of(r.session());
    REQUIRE(bounds_of(r.session().panes, r.session().setup.active, hello, sc).rect.empty());
    const PanePlace authored = pane_of(r.session().setup.active, hello_ref())->place;
    const PaneLaunchAnswered said = hand_launch(r, hello_ref());
    CHECK(said.refusal.empty());
    CHECK(r.session().panes.selected == hello);
    CHECK(r.last_notice() == "Hello is off this screen -- Pane Manager > manage it > Reset > place, "
                             "or hide and show it");
    CHECK(r.session().notice_is_bad);
    // AND IT STAYS WHERE IT WAS PUT: a launch moves no authored place.
    CHECK(pane_of(r.session().setup.active, hello_ref())->place == authored);
    CHECK(bounds_of(r.session().panes, r.session().setup.active, hello, sc).rect.empty());
}

TEST_CASE("an oversubscribed authored setup seats every reference, the column beginning again at its top") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);

    // AUTHORED FIRST, THEN OFFERED -- the order a restored file meets a provider that loads
    // afterwards. The offer's own admission runs the ONE reconciliation path, so nothing here
    // reaches past a message boundary to open anything.
    const PaneRef other_ref{kOtherOffice, "other"};
    Setup both = r.session().setup.active;
    (void)add_pane(both, other_ref); // a second stack pane, authored first
    (void)add_pane(both, hello_ref());
    r.session().setup.active = both;
    r.session().setup.active_link = SetupLink{"setup.json", both};
    ProviderSeat* seat2 = r.mount_provider(kOtherOffice);
    r.drive(seat2, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"other", "Other", "a second stack pane"});
    });
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });

    const std::int64_t other = r.session().panes.runtime.entries[0].kind;
    const std::int64_t hello = r.session().panes.runtime.entries[1].kind;
    // BOTH SEATED on a screen whose column holds one: the stack is not rationed.
    REQUIRE(stack_slots_that_fit(screen_of(r.session())) == 1);
    CHECK(r.session().panes.has(other));
    CHECK(r.session().panes.has(hello));
    // AND THE AUTHORED REFERENCES ARE UNTOUCHED -- authored validity does not depend on extent,
    // so a setup legal on a tall screen is legal on a short one.
    CHECK(check_setup(r.session().setup.active).accepted);
    CHECK((live_status(r.session().setup) == setup_link::kCurrent));

    // THE SECOND BEGINS THE COLUMN AGAIN AT ITS TOP, in front of the first by its rank.
    const auto top_of = [&r](std::int64_t kind) {
        return bounds_of(r.session().panes, r.session().setup.active, kind, screen_of(r.session()))
            .rect.y;
    };
    const auto state_of = [&r](const PaneRef& ref) {
        for (const CatalogRow& row :
             inventory_rows(r.session().setup.active, r.session().panes)) {
            if (row.ref == ref) {
                return std::string(pane_state_word(pane_state_of(
                    r.session().panes, r.session().setup.active, screen_of(r.session()), row)));
            }
        }
        return std::string("absent");
    };
    CHECK(top_of(other) == screen_of(r.session()).room_y);
    CHECK(top_of(hello) == screen_of(r.session()).room_y);
    CHECK(state_of(hello_ref()) == "open");

    // GROWTH LAYS THE SAME PANES OUT DOWN ONE COLUMN, with no gesture at all...
    SeatRows said;
    said.pane = kHelloPane;
    said.rows.push_back(TextRow{"present", gfx::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    r.extent(78, 42);
    CHECK(top_of(hello) > top_of(other));
    CHECK(state_of(other_ref) == "open");
    CHECK(state_of(hello_ref()) == "open");
    // ...AND A SHRINK BEGINS THE COLUMN AGAIN, closing nothing: the pane keeps what it showed.
    r.extent(78, 22);
    CHECK(r.session().panes.has(hello));
    CHECK(top_of(hello) == screen_of(r.session()).room_y);
    REQUIRE(r.session().panes.external_pane(hello) != nullptr);
    CHECK_FALSE(held_row_texts(*r.session().panes.external_pane(hello)).empty());
}

// ---- The room contract ------------------------------------------------------------

TEST_CASE("opening an external pane gives its area exactly the fit_region room, authored as the window") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());

    REQUIRE(seat->rooms.size() == 1);
    CHECK(seat->rooms[0].pane == std::string(kHelloPane));
    // AUTHORED AS THE WINDOW THAT SHOWS THE AREA -- which is what lets the provider verify it.
    REQUIRE_FALSE(seat->room_authors.empty());
    CHECK(seat->room_authors.back() == r.windows.office());

    // THE NUMBERS ARE `fit_region`'S AND NOBODY MULTIPLIES A METRIC. At the minimum composition
    // the pane's region is the WHOLE of the first overlay slot, and the header row is subtracted
    // from the PROSE the medium fits in it rather than from the cells. In a character medium the
    // two spellings answer the same number: nine cells of slot is nine rows, less the header.
    const Screen sc = screen_of(r.session());
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;
    const ui::Rect pane_rect =
cells_covered(bounds_of(r.session().panes, r.session().setup.active, kind, sc).rect);
    CHECK(pane_rect == ui::Rect{0, 2, 63, 9});
    const ExternalBodyPlace body = external_body_place(
        pixels_of_cells(pane_rect), sc,
        external_title_rows(r.session().panes, kind, r.session().pane_titles));
    // THE ROOM IS THE PANE'S INTERIOR: the rectangle the placement path gives it is unchanged,
    // and the one cell of visible boundary on every side comes off before the provider is told
    // what it has -- the same reservation the header is.
    const ui::Rect inside = pane_body_cells(pane_rect);
    CHECK(inside == ui::Rect{1, 3, 61, 7});
    CHECK(cells_covered(PixelRect{body.region_x, body.region_y, body.region_w, body.region_h}) ==
          inside);
    const RegionFit fit = fit_region(cells_px(inside.x), cells_px(inside.y), cells_px(inside.w), cells_px(inside.h),
                                                       sc.text_advance_px, sc.text_line_px);
    CHECK(seat->rooms[0].rows == fit.rows - kExternalHeaderRows);
    CHECK(seat->rooms[0].columns == fit.columns);
    CHECK(seat->rooms[0].rows == 6);
    CHECK(seat->rooms[0].columns == 61);
}

TEST_CASE("an unchanged prose capacity sends no second room; a changed one sends exactly one") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    // THE PANE HAS DRAWN, so its area holds a drawing and a room that did not change is not said
    // again.
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
        s.say(m, SeatRows{kHelloPane, {TextRow{"hello", gfx::role::kFill}}});
    });
    REQUIRE(seat->rooms.size() == 1);

    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;

    // REPAINTS ALONE SAY NOTHING. The room is a fact about the screen, not about how
    // many frames were drawn.
    r.key(input::scan::kP);
    r.key(input::scan::kEscape);
    r.ready();
    CHECK(seat->rooms.size() == 1);

    // A LARGER SCREEN SAYS SOMETHING, EXACTLY ONCE. A slot takes half the room's surplus, so a
    // wider surface moves the body's COLUMNS -- 100 columns of surface is a room of 100, a surplus
    // of 52 and a slot of 74 -- and the grant follows through the same `fit_region` call. The
    // taller half of the resize changes nothing: the slot's height is `kStackRows` at every
    // extent and the header takes one row of it.
    r.extent(100, 40);
    CHECK(screen_of(r.session()).w == cells_px(100));
    CHECK(bounds_of(r.session().panes, r.session().setup.active, kind, screen_of(r.session())).rect ==
          pixels_of_cells(ui::Rect{0, 2, 74, 9}));
    REQUIRE(seat->rooms.size() == 2);
    CHECK(seat->rooms[1].rows == 6);       // unchanged: the rows are the slot's interior's
    CHECK(seat->rooms[1].columns == 72);   // moved: the columns are the room's share, inside
    CHECK(seat->room_authors.back() == r.windows.office());

    // ...and saying it again is not a second answer. The same extent resolves the same
    // capacity, and an unchanged capacity is noise a provider would have to parse.
    r.extent(100, 40);
    r.key(input::scan::kP);
    r.key(input::scan::kEscape);
    CHECK(seat->rooms.size() == 2);

    // A TALLER SCREEN ALONE SAYS NOTHING: the slot's height is `kStackRows` whatever the surface
    // does.
    r.extent(100, 52);
    CHECK(bounds_of(r.session().panes, r.session().setup.active, kind, screen_of(r.session())).rect ==
          pixels_of_cells(ui::Rect{0, 2, 74, 9}));
    CHECK(seat->rooms.size() == 2);

    // A TEXT METRIC MOVES IT TOO: the same cells, set in a real face, hold fewer rows and
    // more columns. Said exactly once, and through the same one call.
    r.extent(100, 40, 9, 18);
    REQUIRE(seat->rooms.size() == 3);
    const Screen typed = screen_of(r.session());
    CHECK(typed.text_advance_px == 9);
    const ExternalBodyPlace graphical = external_body_of(r.session(), kind);
    // the widened body's INTERIOR, before the face is consulted: inside a border one window
    // pixel wide, so it touches every cell of the slot but its first and last
    CHECK(cells_covered(PixelRect{graphical.region_x, graphical.region_y, graphical.region_w,
                                  graphical.region_h})
              .w == 73);
    const RegionFit gfit = fit_region(
        graphical.region_x, graphical.region_y, graphical.region_w, graphical.region_h, 9, 18);
    CHECK(gfit.graphical());
    CHECK(seat->rooms[2].rows == gfit.rows - kExternalHeaderRows);
    CHECK(seat->rooms[2].columns == gfit.columns);
    CHECK(seat->rooms[2].rows != seat->rooms[1].rows);       // a real face fits fewer rows
    CHECK(seat->rooms[2].columns != seat->rooms[1].columns); // ...and more of them across

    // ...and repeating that exact metric says nothing at all.
    r.extent(100, 40, 9, 18);
    CHECK(seat->rooms.size() == 3);
}

TEST_CASE("an external grant follows the widened body through fit_region") {
    // AN EXTERNAL PANE'S ROOM IS THE ROOM'S SHARE, not the minimum composition's 48 columns: one
    // live pane through six resolutions -- three extents in a cell medium and the same three under
    // a real face -- each grant checked against `fit_region` over the body Workshop resolved,
    // never against arithmetic this case performed for itself.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    // THE PANE HAS DRAWN, so its area holds a drawing and a room that did not change is not said
    // again.
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
        s.say(m, SeatRows{kHelloPane, {TextRow{"hello", gfx::role::kFill}}});
    });
    REQUIRE(seat->rooms.size() == 1);
    CHECK(seat->rooms[0].rows == 6);
    CHECK(seat->rooms[0].columns == 61);

    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;
    struct Grant {
        std::int64_t w;
        std::int64_t h;
        std::int64_t advance;
        std::int64_t line;
        std::int64_t rows;
        std::int64_t columns;
    };
    std::size_t said = seat->rooms.size();
    // EACH IS THE PANE'S INTERIOR: two cells less on both axes than its rectangle in a cell
    // medium, its visible boundary coming out of it; a pixel less on each side in the OS window,
    // which shows every pixel.
    for (const Grant& g : std::vector<Grant>{{120, 40, 0, 0, 6, 82},
                                             {200, 60, 0, 0, 6, 122},
                                             {78, 22, 0, 0, 6, 61},
                                             {78, 22, 8, 18, 4, 93},
                                             {120, 40, 8, 18, 4, 125},
                                             {200, 60, 8, 18, 4, 185}}) {
        CAPTURE(g.w);
        CAPTURE(g.h);
        CAPTURE(g.advance);
        r.extent(g.w, g.h, g.advance, g.line);
        REQUIRE(seat->rooms.size() == said + 1);
        said = seat->rooms.size();
        // DERIVED, NOT DUPLICATED: the body Workshop resolved, put through the one function
        // production puts it through.
        const ExternalBodyPlace place = external_body_of(r.session(), kind);
        const RegionFit fit = fit_region(
            place.region_x, place.region_y, place.region_w, place.region_h, g.advance, g.line);
        CHECK(seat->rooms.back().rows == fit.rows - kExternalHeaderRows);
        if (g.advance == 0) {
            CHECK(seat->rooms.back().columns == fit.columns);
            // ...and the answers themselves, so a `fit_region` that changed would be named here
            // rather than agreed with.
            CHECK(seat->rooms.back().rows == g.rows);
            CHECK(seat->rooms.back().columns == g.columns);
        } else {
            // IN THE OS WINDOW THE ROOM IS THE AREA THE WINDOW GIVES, its whole width in the face's
            // characters: the body's inset is the area's to draw in.
            const ExternalPane* placed = r.session().panes.external_pane(kind);
            REQUIRE(placed != nullptr);
            CHECK(seat->rooms.back().columns == placed->placed_w / g.advance);
            CHECK(seat->rooms.back().rows == g.rows);
            CHECK(seat->rooms.back().columns >= g.columns);
        }
        CHECK(seat->rooms.back().pane == std::string(kHelloPane));
        // REPEATING IT IS NOT A SECOND ANSWER.
        r.extent(g.w, g.h, g.advance, g.line);
        CHECK(seat->rooms.size() == said);
    }
}

// ---- Retained content, bounded before it is kept ----------------------------------

TEST_CASE("a pane's rows are shown in its area at the exact body bounds") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;

    // BEFORE ANYTHING ARRIVES the pane says it is waiting -- a fact about this PANE,
    // never about the provider.
    const ui::Rect body = external_body_rect(r.session(), kind);
    std::vector<std::string> shown = external_rows(r.shown(), body);
    REQUIRE(shown.size() == 1);
    CHECK(shown[0] == std::string(kExternalWaiting) + "Hello");

    SeatRows said;
    said.pane = kHelloPane;
    said.rows.push_back(TextRow{"one", gfx::role::kAccent, gfx::role::kMuted});
    said.rows.push_back(TextRow{"two", gfx::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });

    shown = external_rows(r.shown(), body);
    REQUIRE(shown.size() == 2);
    // THE FIRST ROW'S GROUND RUNS TO THE BODY'S EDGE AND NO FURTHER: a muted ground is drawn as
    // the window's muted fill, to exactly the granted bounds.
    CHECK(shown[0].rfind("one", 0) == 0);
    CHECK(shown[0].find_first_not_of('.', 3) == std::string::npos);
    CHECK(static_cast<std::int64_t>(shown[0].size()) == body.w);
    CHECK(shown[1] == "two");

    // ONE CANVAS, AND THE HEADER IS WORKSHOP'S. A weaver can tell whose pane this is.
    const std::string stack = stack_text(r.shown());
    CHECK(stack.find("Hello @zengine.test.workshop-hello") != std::string::npos);
}

TEST_CASE("an external pane's own text cannot bury the surface that recovers it") {
    // THE OTHER HALF OF THE ORDERING CLAIM, with the sharpest consequence. The contextual surface
    // opens OVER the pane it names -- an intentional overlap -- so the pane is underneath it by
    // construction. An external pane fills its room with a REGION of a provider's rows, and a
    // region drawn topmost would put the provider's text over the recovery surface's labels: the
    // row a weaver reaches for to remove a pane would be under the pane it removes.
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    REQUIRE(has_pane(r.session().setup.active, hello_ref()));

    // THE PROVIDER FILLS EVERY ROW OF ITS ROOM, so there is no gap for a recovery row to
    // survive in by luck. Every row is a distinctive byte a case can look for.
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);
    // EVERY ROW OF ITS ROOM, and the room is what the window gave its area -- the body's rows
    // below Workshop's own header row, not the region's cell height.
    const std::int64_t granted = seat->room_rows(kHelloPane);
    REQUIRE(granted > 0);
    SeatRows loud;
    loud.pane = kHelloPane;
    for (std::int64_t i = 0; i < granted; ++i) {
        loud.rows.push_back(TextRow{"ZZZZZZZZ", gfx::role::kFill});
    }
    r.drive(seat, [loud](ProviderSeat& s, loom::Mail& m) { s.say(m, loud); });
    REQUIRE(external_rows(r.shown(), body).size() == static_cast<std::size_t>(granted));

    // THE CONTEXTUAL SURFACE, OVER IT. A right press on the pane opens the pane's own menu at the
    // press -- `remove` among its rows, the recovery a weaver reaches for -- and what a weaver
    // reads in the menu is the menu's own rows, not one row of the provider's. The chrome (title
    // row) opens the pane's host menu; the body is empty by default (WL-CTX-08).
    r.right_press_cell(body.x + 1, body.y);
    REQUIRE(r.session().context.open);
    std::string menu;
    for (const std::string& row : context_rows_on(r.shown(), r.session())) {
        menu += row + "\n";
    }
    INFO(menu);
    CHECK(menu.find("hide pane") != std::string::npos);
    CHECK(menu.find("ZZZZZZZZ") == std::string::npos);
    r.key(input::scan::kEscape);

    // THE DESK ARRANGEMENT COVERS NOTHING: entering the scope leaves the provider's text
    // visible -- the state's visible statement is the affordance ring ON the pane and the band's
    // own rows, not a pane over it -- and the recovery surface for PARTICIPATION is the Pane
    // Manager's close.
    r.key(input::scan::kW);
    r.text("w");
    REQUIRE(r.session().arrange.open);
    const std::string arranging = stack_text(r.shown());
    INFO(arranging);
    CHECK(arranging.find("+ WINDOW") == std::string::npos);
    CHECK(arranging.find("ZZZZZZZZ") != std::string::npos);

    // THE CONTROL: the provider's rows really are still being published either way.
    CHECK(external_rows(r.shown(), body).size() == static_cast<std::size_t>(granted));
    r.key(input::scan::kEscape);
    CHECK(stack_text(r.shown()).find("ZZZZZZZZ") != std::string::npos);
}

TEST_CASE("rows for a closed or never-offered pane do nothing at all") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    const std::size_t catalog_before = r.session().panes.runtime.entries.size();

    // THE PANE IS IN THE CATALOG AND NOT OPEN. A provider cannot make a pane appear
    // by drawing it: discovery and presentation are two doors.
    SeatRows said;
    said.pane = kHelloPane;
    said.rows.push_back(TextRow{"unasked for", gfx::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    CHECK(r.session().panes.open.size() == 1); // the Layouts pane, and nothing else
    CHECK(r.session().panes.external.empty());
    CHECK(r.session().panes.runtime.entries.size() == catalog_before);

    // A PANE KEY THIS OFFICE NEVER OFFERED CREATES NO CATALOG ROW EITHER.
    SeatRows unknown;
    unknown.pane = "never-offered";
    unknown.rows.push_back(TextRow{"nor this", gfx::role::kFill});
    r.drive(seat, [unknown](ProviderSeat& s, loom::Mail& m) { s.say(m, unknown); });
    CHECK(r.session().panes.runtime.entries.size() == catalog_before);
    CHECK(r.session().panes.external.empty());
}

// ---- Presentation and the pointer --------------------------------------------------

TEST_CASE("an external pane occupies its whole rectangle, and takes hold of nothing under it") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;
    const Screen sc = screen_of(r.session());
    const ui::Rect pane_rect =
cells_covered(bounds_of(r.session().panes, r.session().setup.active, kind, sc).rect);

    // THE SAME RECTANGLE THE PAINTER WAS HANDED. One geometry, and the occupancy
    // answer names the OFFERED pane rather than `builtin_pane`'s Builder fall-through.
    CHECK(occupied_at(r.session().panes, r.session().setup.active, sc, pane_rect.x, pane_rect.y).occupied);
    CHECK(occupied_at(r.session().panes, r.session().setup.active, sc, pane_rect.x, pane_rect.y).what == "Hello");
    CHECK(occupied_at(r.session().panes, r.session().setup.active, sc, pane_rect.x + pane_rect.w - 1, pane_rect.y + pane_rect.h - 1)
              .what == "Hello");
    CHECK_FALSE(occupied_at(r.session().panes, r.session().setup.active, sc, pane_rect.x, pane_rect.y + pane_rect.h).occupied);
    // ...and one cell to the RIGHT is not this pane's either -- said as "not this pane's" rather
    // than "not anybody's", because the slot reaches into the right column's place at this
    // extent, the room being the surface (`the-room-is-the-screen`).
    CHECK(occupied_at(r.session().panes, r.session().setup.active, sc, pane_rect.x + pane_rect.w, pane_rect.y)
              .what != "Hello");

    // ...AND IT CARRIES THE HANDLE IT MET, so the one caller that needs a further question of
    // this answer asks it of THIS walk rather than resolving the pane a second time. Nothing at
    // all is `kNoKind`, and says so.
    CHECK(occupied_at(r.session().panes, r.session().setup.active, sc, pane_rect.x, pane_rect.y).kind ==
          kind);
    CHECK(occupied_at(r.session().panes, r.session().setup.active, sc, pane_rect.x, pane_rect.y + pane_rect.h)
              .kind == kNoKind);

    // THE PRESS IS THE PANE'S, WHATEVER ELSE HAPPENS TO IT: it selects the pane, and nothing
    // behind the pane is reached. ⚠ THE POSITION IS A TERMINAL POSITION: `{pane_rect.x,
    // pane_rect.y}` in `space::kCells` is read by the medium's inverse as canvas row `pane_rect.y -
    // kTuiCanvasTopRow`
    // -- two rows ABOVE the pane -- so a press published that way never touches the pane, and
    // an assertion about it holds for a reason that has nothing to do with the claim.
    r.press_cell(pane_rect.x, pane_rect.y);
    CHECK(r.session().panes.selected == kind);
    CHECK(r.session().notice != "nothing there");
    r.press_cell(pane_rect.x + 1, pane_rect.y + 1);
    CHECK(r.session().panes.selected == kind);
    CHECK(r.session().notice != "nothing there");
}

TEST_CASE("a read-only pane that ignores presses is unchanged when the window lands them") {
    // THE HELLO FIXTURE ACCEPTS NO PRESS AT ALL -- it is the pane protocol's witness and was
    // deliberately not widened. A pane that never asked for input goes on receiving none: the
    // shape is undeliverable to a weave that does not accept it, so the window landing a press in
    // its area changes nothing about what this provider does, says, or shows.
    PaneRig r;
    r.mount_workshop();
    (void)r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice);
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;
    const ui::Rect body = external_body_rect(r.session(), kind);
    const std::vector<std::string> before = external_rows(r.shown(), body);
    REQUIRE_FALSE(before.empty());

    for (std::int64_t row = 0; row < 4; ++row) {
        r.press_cell(body.x + 2, body.y + kExternalHeaderRows + row);
    }
    CHECK(external_rows(r.shown(), body) == before);
    CHECK(r.session().notice != "nothing there"); // the presses were the pane's, not the room's
}

// ---- Lifecycle, and the exact limit of what silence proves --------------------------

TEST_CASE("closing an external pane ends only its presentation") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;
    SeatRows said;
    said.pane = kHelloPane;
    said.rows.push_back(TextRow{"remembered", gfx::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    REQUIRE_FALSE(held_row_texts(*r.session().panes.external_pane(kind)).empty());
    const std::int64_t said_count = seat->said;

    r.pick(hello_ref()); // the close door, as the Pane Manager spends it
    CHECK_FALSE(r.session().panes.has(kind));
    CHECK(r.session().panes.external_pane(kind) == nullptr);

    // THE PROVIDER IS UNTOUCHED: no unload, no lifecycle operation, no retraction of
    // the catalog row, and its own semantic state outlives the presentation entirely.
    CHECK(seat->said == said_count);
    CHECK(r.session().panes.runtime.of_kind(kind) != nullptr);
    CHECK(resolve_pane(hello_ref(), r.session().panes).has_value());

    // REOPENING PLACES THE AREA AGAIN, and the window gives it a room the provider draws in.
    const std::size_t rooms_before = seat->rooms.size();
    r.pick(hello_ref());
    REQUIRE(r.session().panes.external_pane(kind) != nullptr);
    CHECK(seat->rooms.size() == rooms_before + 1);
    CHECK(held_row_texts(*r.session().panes.external_pane(kind)) ==
          std::vector<std::string>{"remembered"});
}

TEST_CASE("a valid re-offer refreshes the descriptor and the open pane, without duplicating") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;
    SeatRows said;
    said.pane = kHelloPane;
    said.rows.push_back(TextRow{"the old answer", gfx::role::kFill});
    r.drive(seat, [said](ProviderSeat& s, loom::Mail& m) { s.say(m, said); });
    REQUIRE_FALSE(held_row_texts(*r.session().panes.external_pane(kind)).empty());

    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"hello", "Hello", "a corrected line"});
    });
    // ONE ROW, ONE HANDLE, ONE PRESENTATION -- and the descriptor updated in place.
    REQUIRE(r.session().panes.runtime.entries.size() == 1);
    CHECK(r.session().panes.runtime.entries[0].kind == kind);
    CHECK(r.session().panes.runtime.entries[0].summary == "a corrected line");
    CHECK(r.session().panes.has(kind));
}

TEST_CASE("silence is waiting, and Workshop never says unavailable") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.pick(hello_ref());
    const std::int64_t kind = r.session().panes.runtime.entries[0].kind;

    // MANY REPAINTS AND NO ANSWER. Nothing times out, nothing polls, no catalog row is
    // withdrawn and no setup reference is deleted -- because sender silence does not
    // prove a delivery's fate and Loom gives Workshop no unload notification at all.
    for (int i = 0; i < 20; ++i) {
        r.key(input::scan::kP);
        r.key(input::scan::kEscape);
    }
    const ui::Rect body = external_body_rect(r.session(), kind);
    CHECK(external_rows(r.shown(), body)[0] == std::string(kExternalWaiting) + "Hello");
    CHECK(r.session().panes.runtime.entries.size() == 1);
    CHECK(has_pane(r.session().setup.active, hello_ref()));
    CHECK(r.session().panes.has(kind));

    // THE WORD IS NEVER SAID, on the pane or on the setup line.
    for (const surface::SurfaceText& note : r.notes) {
        CHECK(note.text.find("unavailable") == std::string::npos);
    }
    CHECK(stack_text(r.shown()).find("unavailable") == std::string::npos);
    CHECK(setup_rest_text(r.session().setup, r.session().panes,
                            r.session().keymap, KeySpelling::kShort)
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
    REQUIRE(r.session().panes.runtime.entries.size() == 2);
    CHECK(r.session().panes.runtime.entries[0].provider ==
          r.session().panes.runtime.entries[1].provider);
    CHECK(r.session().panes.runtime.entries[0].kind !=
          r.session().panes.runtime.entries[1].kind);
    // TWO CATALOG ROWS, ONE OFFICE, AND NOTHING ANYWHERE THAT COUNTS PROVIDERS. Closing
    // one presentation could not unload a weave even if this file wanted it to.
    CHECK(combined_catalog(r.session().panes).size() == kBuiltinPaneCount + 2);
}

// ---- The built-ins, unmoved --------------------------------------------------------

TEST_CASE("the built-in panes behave exactly as they did, with a provider in the room") {
    PaneRig r;
    r.mount_workshop();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
    r.extent(120, 44); // room for both stack slots, so nothing here is a capacity case

    // THE LAYOUTS PANE USES NO BUS and is open at boot: the case is about a built-in that talks
    // to nobody, and Layouts is the one this host compiles and opens.
    CHECK(r.session().panes.has(pane_kind::kLayouts));
    const std::size_t said_before = static_cast<std::size_t>(seat->said);

    // THE BUILT-IN CLOSED AND OPENED THROUGH THE DOORS the Pane Manager spends.
    r.pick(ref_of(pane_kind::kLayouts));
    CHECK_FALSE(r.session().panes.has(pane_kind::kLayouts));
    CHECK(r.last_notice().rfind("hid Layouts", 0) == 0);
    r.pick(ref_of(pane_kind::kLayouts));
    CHECK(r.session().panes.has(pane_kind::kLayouts));
    CHECK(r.last_notice().find("showed Layouts") != std::string::npos);

    // NOTHING THE BUILT-INS DID REACHED THE PROVIDER.
    CHECK(static_cast<std::size_t>(seat->said) == said_before);

    // AND `builtin_pane` IS STILL TOTAL ON ITS OWN BOUNDED PATH, which the pane protocol was
    // required to leave standing.
    CHECK(builtin_pane(9999).kind == kBuiltinPanes[0].kind);
    CHECK(placement_of(pane_kind::kLayouts) == placement::kTopBand);
    CHECK_FALSE(resolve_pane(PaneRef{"nobody", "nothing"}, r.session().panes)
                    .has_value());
}

TEST_CASE("the off-screen sentence fits a hundred-column terminal whole, both ways back included") {
    // ⚔ MUTATION: the sentence naming the pane twice and spelling each way back in full -- 133
    // characters for `Hello`, and a hundred columns end it at the Pane Manager's route.
    // A PANE OF THE SHIPPED NAMES' LENGTH AND ONE OF SIXTEEN CHARACTERS, the longest the sentence
    // is written to hold beside both ways back.
    for (const std::string& name : {std::string("Hello"), std::string("Sixteen chars ok")}) {
        CAPTURE(name);
        PaneRig r;
        r.mount_workshop();
        ProviderSeat* seat = r.mount_provider(kHelloOffice);
        r.drive(seat, [&name](ProviderSeat& s, loom::Mail& m) {
            s.offer(m, PaneOffered{kHelloPane, name, "a bounded external greeting"});
        });
        r.extent(100, 30);
        r.pick(hello_ref());
        REQUIRE(author_pane_place(r.session().setup.active, hello_ref(), cells_px(300), 0).accepted);
        r.key(input::scan::kUnknown); // a delivery, so the desk claims what it now is
        (void)hand_launch(r, hello_ref());
        const std::string& said = r.last_notice();
        REQUIRE(said.rfind(name + " is off this screen", 0) == 0);
        CHECK(said.find("Reset > place") != std::string::npos);
        CHECK(said.find("hide and show") != std::string::npos);
        // ...AND THE BAND A HUNDRED COLUMNS WIDE PAINTS IT WHOLE, no elision mark at its end.
        const surface::SurfaceTextRegion band = band_region(r.session(), screen_of(r.session()));
        REQUIRE_FALSE(band.rows.empty());
        CHECK(band.rows[0].text == said);
    }
}
