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
// SIX SOURCES, ONE SUITE, AND THE BOUNDARIES ARE THE FILE'S OWN. `workshop_panes` is
// one CTest entry running one binary; its cases live in six translation
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
//   _actions.cpp        a pane declares its actions -- the join, the legend, the resolved id
//
// A NEW CASE GOES TO THE FILE WHOSE SUBJECT IT IS ABOUT. The cut is a reading boundary
// first and an object-format bound second: one MinGW Debug object could no longer name
// all of these instantiations (tests/CMakeLists.txt, QR-13).
//
// THIS FILE OWNS: a pane's declared actions (WL-KEY-15) -- the three shapes, the join
// under the office stamp and the collision law, the maker's override reaching a pane in
// both load orders, the legend and the hotkey view, and the resolved id crossing the seam
// instead of the key -- and the first consumer, the real Powers pane acting on the id.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "desktop-pane/vocabulary.hpp"
#include "editor-pane/vocabulary.hpp"
#include "weavelib/legacy_pane_protocol.hpp"
#include "workshop/pane_text.hpp"

#include <zen/registry.hpp>

namespace {

v2::PaneActionRow declared(const char* id, const char* label, std::int64_t scancode,
                       std::int64_t modifiers = input::mod::kNone,
                       const char* supersedes = "") {
    v2::PaneActionRow row;
    row.id = id;
    row.label = label;
    row.scancode = scancode;
    row.modifiers = modifiers;
    row.supersedes = supersedes;
    return row;
}

/// A VERSION-ONE DECLARATION, built from rows written in the host's own row type -- see
/// `narrowed` below for why the suite writes them that way and sends them this way (VD-27).
PaneActions actions_for(const char* pane, const std::vector<v2::PaneActionRow>& rows);

/// Offer a pane AND declare its actions in one breath, as a real provider does, then open
/// it through the launch door. Answers the pane's runtime handle.
/// ⚠ THESE SEND VERSION ONE, WHICH IS THE POINT (VD-27). The rows are written in the host's
/// own (later) row type for one spelling across the suite, and NARROWED here to exactly the
/// four fields a provider built before ownership existed can say. So every case below that is
/// not about ownership drives the legacy door, with the real shape, through the real seam.
inline PaneActions actions_for(const char* pane, const std::vector<v2::PaneActionRow>& rows) {
    PaneActions out;
    out.pane = pane;
    for (const v2::PaneActionRow& row : rows) {
        REQUIRE_MESSAGE(row.supersedes.empty(),
                        "a row that owns a host action cannot be said in version one");
        out.rows.push_back(PaneActionRow{row.id, row.label, row.scancode, row.modifiers});
    }
    return out;
}

std::int64_t seat_pane_declared(PaneRig& r, ProviderSeat* seat, const char* office,
                                const char* pane, std::vector<v2::PaneActionRow> rows) {
    r.drive(seat, [pane, rows](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{pane, "Seat", "a recording provider"});
        s.declare(m, actions_for(pane, rows));
    });
    r.pick(PaneRef{office, pane});
    const RuntimePane* row = r.session().panels.runtime.find(office, pane);
    return row == nullptr ? kNoPaneKind : row->kind;
}

/// ...AND THE SECOND VERSION, for a pane that owns one of Workshop's actions.
std::int64_t seat_pane_declared_v2(PaneRig& r, ProviderSeat* seat, const char* office,
                                   const char* pane, std::vector<v2::PaneActionRow> rows) {
    r.drive(seat, [pane, rows](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{pane, "Seat", "a recording provider"});
        s.declare_v2(m, v2::PaneActions{pane, rows});
    });
    r.pick(PaneRef{office, pane});
    const RuntimePane* row = r.session().panels.runtime.find(office, pane);
    return row == nullptr ? kNoPaneKind : row->kind;
}

/// Declare again, for a pane already offered -- a refresh, or an attempt at one.
void redeclare(PaneRig& r, ProviderSeat* seat, const char* pane,
               std::vector<v2::PaneActionRow> rows) {
    r.drive(seat, [pane, rows](ProviderSeat& s, loom::Mail& m) {
        s.declare(m, actions_for(pane, rows));
    });
}

const std::vector<v2::PaneActionRow>& retained(PaneRig& r, std::int64_t kind) {
    const RuntimePane* row = r.session().panels.runtime.of_kind(kind);
    REQUIRE(row != nullptr);
    return row->actions;
}

std::string hotkeys_text(PaneRig& r) { return keymap_text(r.session()); }

constexpr std::int64_t kSomePane = 1024; // a runtime handle, for the value-level cases

} // namespace

// ============================================================================
// The three shapes
// ============================================================================

TEST_CASE("a pane's actions are three shapes: rows of id, label and two numbers; a "
          "declaration; a resolved id -- and none carries a provider or a key name") {
    const std::shared_ptr<const loom::Schema> row = loom::schema_of<PaneActionRow>();
    REQUIRE(row != nullptr);
    CHECK(row->name() == "PaneActionRow");
    CHECK(row->version() == 1u);
    REQUIRE(row->fields().size() == 4);
    CHECK(row->fields()[0].name == "id");
    CHECK(row->fields()[0].type.kind == loom::Kind::Text);
    CHECK(row->fields()[1].name == "label");
    CHECK(row->fields()[1].type.kind == loom::Kind::Text);
    // THE GESTURE IS THE TWO NUMBERS `PaneKey` CARRIES, and never a key NAME -- a name
    // is a spelling the host's grammar owns, and a provider that declared one would be
    // declaring in the host's grammar.
    CHECK(row->fields()[2].name == "scancode");
    CHECK(row->fields()[2].type.kind == loom::Kind::Int);
    CHECK(row->fields()[3].name == "modifiers");
    CHECK(row->fields()[3].type.kind == loom::Kind::Int);
    for (const loom::Field& f : row->fields()) {
        CHECK(f.name != "key");
        CHECK(f.name != "name");
        CHECK(f.name != "gesture");
        CHECK(f.name != "context");
    }

    const std::shared_ptr<const loom::Schema> declared = loom::schema_of<PaneActions>();
    REQUIRE(declared != nullptr);
    CHECK(declared->name() == "PaneActions");
    CHECK(declared->version() == 1u);
    REQUIRE(declared->fields().size() == 2);
    CHECK(declared->fields()[0].name == "pane");
    CHECK(declared->fields()[1].name == "rows");
    CHECK(declared->fields()[1].type.kind == loom::Kind::List);
    REQUIRE(declared->fields()[1].type.element != nullptr);
    REQUIRE(declared->fields()[1].type.element->message != nullptr);
    CHECK(declared->fields()[1].type.element->message->name() == "PaneActionRow");
    CHECK(declared->fields()[1].type.element->message->version() == 1u);
    for (const loom::Field& f : declared->fields()) {
        CHECK(f.name != "provider"); // WHOSE it is, is `mail.authored_role()`
    }

    // ⭐ AND THE SECOND PUBLISHED VERSION, BESIDE IT AND NOT INSTEAD OF IT (VD-27). Version
    // one above is exactly the four fields it has always had -- the field that lets a pane
    // name an action it owns lives here, in a version of its own, because a published
    // `(name, version)` is frozen and its identity is derived from the shape (Loom GATE-04).
    const std::shared_ptr<const loom::Schema> row2 = loom::schema_of<v2::PaneActionRow>();
    REQUIRE(row2 != nullptr);
    CHECK(row2->name() == "PaneActionRow");
    CHECK(row2->version() == 2u);
    REQUIRE(row2->fields().size() == 5);
    for (std::size_t i = 0; i < row->fields().size(); ++i) {
        CHECK(row2->fields()[i].name == row->fields()[i].name); // the four, in order
        CHECK(row2->fields()[i].type.kind == row->fields()[i].type.kind);
    }
    CHECK(row2->fields()[4].name == "supersedes");
    CHECK(row2->fields()[4].type.kind == loom::Kind::Text);
    CHECK_FALSE(loom::same_identity(*row, *row2)); // two shapes, and the bus knows it

    const std::shared_ptr<const loom::Schema> declared2 = loom::schema_of<v2::PaneActions>();
    REQUIRE(declared2 != nullptr);
    CHECK(declared2->name() == "PaneActions");
    CHECK(declared2->version() == 2u);
    REQUIRE(declared2->fields().size() == 2);
    CHECK(declared2->fields()[0].name == "pane");
    REQUIRE(declared2->fields()[1].type.element != nullptr);
    REQUIRE(declared2->fields()[1].type.element->message != nullptr);
    CHECK(declared2->fields()[1].type.element->message->version() == 2u);
    CHECK_FALSE(loom::same_identity(*declared, *declared2));

    const std::shared_ptr<const loom::Schema> asked = loom::schema_of<PaneActionRequested>();
    REQUIRE(asked != nullptr);
    CHECK(asked->name() == "PaneActionRequested");
    CHECK(asked->version() == 1u);
    REQUIRE(asked->fields().size() == 2);
    CHECK(asked->fields()[0].name == "pane");
    CHECK(asked->fields()[1].name == "id");
    CHECK(asked->fields()[1].type.kind == loom::Kind::Text);
    for (const loom::Field& f : asked->fields()) {
        CHECK(f.name != "scancode"); // the RESOLVED id crosses; the key does not
        CHECK(f.name != "modifiers");
    }

    // AND THE EIGHT OLDER SHAPES DID NOT MOVE: three were added and none revised.
    CHECK(loom::schema_of<PaneCatalogRequested>()->version() == 1u);
    CHECK(loom::schema_of<PaneOffered>()->version() == 1u);
    CHECK(loom::schema_of<PaneRoom>()->version() == 1u);
    CHECK(loom::schema_of<PaneContent>()->version() == 1u);
    CHECK(loom::schema_of<PanePressed>()->version() == 1u);
    CHECK(loom::schema_of<PaneKey>()->version() == 1u);
    CHECK(loom::schema_of<PaneTextInput>()->version() == 1u);
    CHECK(loom::schema_of<PaneWheel>()->version() == 1u);
}

// ============================================================================
// The join, over a value
// ============================================================================

TEST_CASE("the join judges a declaration whole, in order, and a refusal writes nothing") {
    Keymap k;
    const auto refused = [&k](std::vector<v2::PaneActionRow> rows) {
        Keymap candidate = k;
        const Written w = join_pane_rows(candidate, kSomePane, rows);
        CHECK_FALSE(w.accepted);
        CHECK(candidate.pane_rows(kSomePane) == nullptr); // nothing written
        return w.refusal;
    };
    const auto accepted = [&k](std::vector<v2::PaneActionRow> rows) {
        Keymap candidate = k;
        const Written w = join_pane_rows(candidate, kSomePane, rows);
        CHECK_MESSAGE(w.accepted, w.refusal);
        REQUIRE(candidate.pane_rows(kSomePane) != nullptr);
        return candidate;
    };

    SUBCASE("the row count is bounded, and the bound is said") {
        std::vector<v2::PaneActionRow> many;
        for (std::size_t i = 0; i <= kMaxPaneActionRows; ++i) {
            many.push_back(declared(("x.r" + std::to_string(i)).c_str(), "row",
                                    input::scan::kUnknown));
        }
        CHECK(refused(many).find("at most 32 actions") != std::string::npos);
        many.pop_back();
        (void)accepted(many);
    }
    SUBCASE("an id is present, bounded, printable, has no space, and is unique") {
        CHECK(refused({declared("", "row", input::scan::kUp)}).find("id cannot be empty") !=
              std::string::npos);
        CHECK(refused({declared("x up", "row", input::scan::kUp)}).find("cannot contain a space") !=
              std::string::npos);
        CHECK(refused({declared("x.\x01", "row", input::scan::kUp)})
                  .find("must be printable ASCII") != std::string::npos);
        CHECK(refused({declared(std::string(65, 'a').c_str(), "row", input::scan::kUp)})
                  .find("id is at most 64 bytes") != std::string::npos);
        CHECK(refused({declared("x.up", "row", input::scan::kUp),
                       declared("x.up", "row again", input::scan::kDown)})
                  .find("`x.up` is declared twice") != std::string::npos);
    }
    SUBCASE("an id that is one of Workshop's own is refused: one authored row would name two") {
        CHECK(refused({declared("workshop.quit", "quit", input::scan::kUp)})
                  .find("`workshop.quit` is Workshop's own action id") != std::string::npos);
    }
    SUBCASE("a label is present, bounded and printable; spaces are fine") {
        CHECK(refused({declared("x.up", "", input::scan::kUp)}).find("label cannot be empty") !=
              std::string::npos);
        CHECK(refused({declared("x.up", std::string(33, 'l').c_str(), input::scan::kUp)})
                  .find("label is at most 32 bytes") != std::string::npos);
        CHECK(refused({declared("x.up", "row\tup", input::scan::kUp)})
                  .find("must be printable ASCII") != std::string::npos);
        CHECK(refused({declared("x.up", "   ", input::scan::kUp)})
                  .find("needs more than spaces") != std::string::npos);
        (void)accepted({declared("x.up", "row up", input::scan::kUp)});
    }
    SUBCASE("a default gesture is one the keymap file could spell back, or no gesture at all") {
        CHECK(refused({declared("x.up", "row", 200)}).find("scancode 200 is not a key this keymap can name") !=
              std::string::npos);
        CHECK(refused({declared("x.up", "row", input::scan::kUp, 16)})
                  .find("modifier bits this keymap does not know") != std::string::npos);
        CHECK(refused({declared("x.up", "row", input::scan::kUnknown, input::mod::kShift)})
                  .find("a row with no default key cannot carry modifiers") != std::string::npos);
        const Keymap joined = accepted({declared("x.rename", "rename", input::scan::kUnknown)});
        // UNBOUND JOINS UNBOUND, and answers to no key -- the `kUnknown` key that the
        // wire reports for a key this build cannot name requests nothing (WL-KEY-13).
        REQUIRE(joined.pane_rows(kSomePane)->rows.size() == 1);
        CHECK_FALSE(is_bound(joined.pane_rows(kSomePane)->rows[0].gesture));
        CHECK(joined.pane_action_for(kSomePane, input::scan::kUnknown, input::mod::kNone) ==
              nullptr);
    }
    SUBCASE("the collision law runs over what is active while a pane holds the keys") {
        // ⭐ AN APPLICATION ROW ABOVE EVERY MODE IS REFUSED HERE, AND ONE DECLARATION BUYS IT
        // (VD-26). It is active while a pane holds the keys, so a pane taking its chord for an
        // unrelated operation really would be two meanings on one gesture -- unless the pane
        // says the row is standing in for it, which is one meaning in two scopes. (The host's
        // `document.save` and `document.open` were the rows here until they retired with the
        // object document; no host row is active in a pane any more but the no-text quit.)
        REQUIRE(join_app_rows(k, std::vector<AppRow>{
                                     AppRow{"desktop.terminal", "terminal",
                                            Gesture{input::scan::kT, input::mod::kCtrl}, 0},
                                     AppRow{"desktop.panes", "panes",
                                            Gesture{input::scan::kP, input::mod::kCtrl}, 0}})
                    .accepted);
        const Gesture term{input::scan::kT, input::mod::kCtrl};
        CHECK(refused({declared("x.t", "term", term.scancode, term.modifiers)}) ==
              collision_sentence(term, "desktop.terminal", "x.t"));
        const Keymap with_term = accepted(
            {declared("x.t", "term", term.scancode, term.modifiers, "desktop.terminal")});
        REQUIRE(with_term.pane_rows(kSomePane) != nullptr);
        CHECK(with_term.pane_supersedes(kSomePane, "desktop.terminal"));
        CHECK_FALSE(with_term.app_row_active(*with_term.app_row_of_id("desktop.terminal"),
                                             KeyContext::kPane, kSomePane));
        // ...AND ANOTHER PANE'S KEYS ARE NOT THIS ONE'S DECLARATION.
        CHECK(with_term.app_row_active(*with_term.app_row_of_id("desktop.terminal"),
                                       KeyContext::kPane, kSomePane + 1));
        // AN ID NOTHING DECLARES, AND ONE A PANE MAY NOT OWN, ARE BOTH REFUSED.
        CHECK(refused({declared("x.z", "z", input::scan::kZ, input::mod::kCtrl, "no.such")})
                  .find("not an action id this Workshop knows") != std::string::npos);
        CHECK(refused({declared("x.k", "k", input::scan::kUnknown, input::mod::kNone,
                                "workshop.quit")})
                  .find("not an action a pane may own") != std::string::npos);
        // ...BUT ONE THAT RETIRED IS ADMITTED, STANDING IN FOR NOTHING: `document.save` was
        // published as ownable before the object document retired, and a pane built then keeps
        // its keys. Its row is simply its own.
        const Keymap with_retired = accepted({declared("x.s", "save", input::scan::kS,
                                                       input::mod::kCtrl, kOwnableDocumentSave)});
        CHECK_FALSE(with_retired.pane_supersedes(kSomePane, kOwnableDocumentSave));
        REQUIRE(with_retired.pane_action_for(kSomePane, input::scan::kS, input::mod::kCtrl) !=
                nullptr);
        // ...AND A RETIRED ID IS STILL NOBODY'S TO DECLARE AS ITS OWN: a maker's row for it is
        // kept, and must not come to move a stranger's key.
        CHECK(refused({declared("document.save", "save", input::scan::kS, input::mod::kCtrl)})
                  .find("retired with the object document") != std::string::npos);
        // A NO-TEXT ROW (`workshop.quit`, ^c) is NOT active while a text-taking pane
        // holds the keys (WL-FOCUS-09): a pane may declare the chord.
        const Gesture quit = k.gesture_of(Act::kQuit);
        (void)accepted({declared("x.copy", "copy", quit.scancode, quit.modifiers)});
        // A COMMAND-MODE ROW is another mode's: `n` is free in a pane.
        (void)accepted({declared("x.new", "new", input::scan::kN)});
        // TWO ROWS OF ONE PANE ON ONE GESTURE: refused, naming both.
        CHECK(refused({declared("x.up", "up", input::scan::kUp),
                       declared("x.also", "also", input::scan::kUp)}) ==
              collision_sentence(Gesture{input::scan::kUp, input::mod::kNone}, "x.up", "x.also"));
        // ...and two UNBOUND rows are not two rows holding one gesture (WL-KEY-13).
        (void)accepted({declared("x.a", "a", input::scan::kUnknown),
                        declared("x.b", "b", input::scan::kUnknown)});
    }
    SUBCASE("another pane's rows are another context, and never meet these") {
        Keymap both = k;
        REQUIRE(join_pane_rows(both, kSomePane, {declared("a.up", "up", input::scan::kUp)}).accepted);
        REQUIRE(join_pane_rows(both, kSomePane + 1, {declared("b.up", "up", input::scan::kUp)})
                    .accepted);
        REQUIRE(both.pane_action_for(kSomePane, input::scan::kUp, input::mod::kNone) != nullptr);
        CHECK(both.pane_action_for(kSomePane, input::scan::kUp, input::mod::kNone)->id == "a.up");
        CHECK(both.pane_action_for(kSomePane + 1, input::scan::kUp, input::mod::kNone)->id ==
              "b.up");
    }
    SUBCASE("the maker's authored override moves a declared row, through the file's grammar") {
        k.authored.push_back(AuthoredOverride{"x.up", "ctrl+u"});
        const Keymap moved = accepted({declared("x.up", "row up", input::scan::kUp)});
        CHECK(moved.pane_rows(kSomePane)->rows[0].gesture ==
              Gesture{input::scan::kU, input::mod::kCtrl});
        CHECK(moved.pane_action_for(kSomePane, input::scan::kUp, input::mod::kNone) == nullptr);
        REQUIRE(moved.pane_action_for(kSomePane, input::scan::kU, input::mod::kCtrl) != nullptr);
        // AN OVERRIDE THAT LANDS ON A ROW ACTIVE ABOVE THE MODES IS THE SAME COLLISION, said the
        // same way -- an application row's, now that no host row but the quit is active here.
        REQUIRE(join_app_rows(k, std::vector<AppRow>{AppRow{
                                     "desktop.terminal", "terminal",
                                     Gesture{input::scan::kT, input::mod::kCtrl}, 0}})
                    .accepted);
        k.authored[0].gesture = "ctrl+t";
        CHECK(refused({declared("x.up", "row up", input::scan::kUp)}) ==
              collision_sentence(Gesture{input::scan::kT, input::mod::kCtrl}, "desktop.terminal",
                                 "x.up"));
        // A GESTURE OUTSIDE THE GRAMMAR, and an id authored twice: `apply_overrides`' words.
        k.authored[0].gesture = "hyper+u";
        CHECK(refused({declared("x.up", "row up", input::scan::kUp)}).find("`x.up`: `hyper`") !=
              std::string::npos);
        // ...AND TWO ROWS FOR ONE ID ARE TWO KEYS FOR ONE ACTION (WL-KEY-08): the pane's row
        // is repeated, one per key, and both dispatch; the same key twice is what is refused.
        k.authored[0].gesture = "ctrl+u";
        k.authored.push_back(AuthoredOverride{"x.up", "ctrl+j"});
        Keymap two = k;
        REQUIRE(join_pane_rows(two, kSomePane, {declared("x.up", "row up", input::scan::kUp)})
                    .accepted);
        REQUIRE(two.pane_rows(kSomePane) != nullptr);
        CHECK(two.pane_rows(kSomePane)->rows.size() == 2);
        CHECK(two.pane_action_for(kSomePane, input::scan::kU, input::mod::kCtrl) != nullptr);
        CHECK(two.pane_action_for(kSomePane, input::scan::kJ, input::mod::kCtrl) != nullptr);
        CHECK(two.pane_action_for(kSomePane, input::scan::kUp, input::mod::kNone) == nullptr);
        k.authored[1].gesture = "ctrl+u";
        CHECK(refused({declared("x.up", "row up", input::scan::kUp)})
                  .find("`x.up` is authored twice with `ctrl+u`") != std::string::npos);
    }
    SUBCASE("a second join for the same pane replaces its rows whole; dropping forgets them") {
        Keymap twice = k;
        REQUIRE(join_pane_rows(twice, kSomePane, {declared("x.up", "up", input::scan::kUp)}).accepted);
        REQUIRE(join_pane_rows(twice, kSomePane, {declared("x.down", "down", input::scan::kDown),
                                                  declared("x.mark", "mark", input::scan::kM)})
                    .accepted);
        REQUIRE(twice.pane_rows(kSomePane)->rows.size() == 2);
        CHECK(twice.pane_action_for(kSomePane, input::scan::kUp, input::mod::kNone) == nullptr);
        CHECK(twice.pane_action_for(kSomePane, input::scan::kM, input::mod::kNone) != nullptr);
        CHECK(twice.panes.size() == 1);
        drop_pane_rows(twice, kSomePane);
        CHECK(twice.pane_rows(kSomePane) == nullptr);
        CHECK(twice.panes.empty());
    }
}

// ============================================================================
// Admission, through the real seam
// ============================================================================

TEST_CASE("a pane's actions are admitted under the office that offered it, and retained on its row") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_declared(
        r, seat, kHelloOffice, kHelloPane,
        {declared("hello.up", "row up", input::scan::kUp),
         declared("hello.pick", "pick", input::scan::kReturn),
         declared("hello.mark", "mark this", input::scan::kM)});
    REQUIRE(is_runtime_kind(kind));

    const PaneRows* rows = r.session().keymap.pane_rows(kind);
    REQUIRE(rows != nullptr);
    REQUIRE(rows->rows.size() == 3);
    CHECK(rows->rows[0].id == "hello.up");
    CHECK(rows->rows[0].label == "row up");
    CHECK(rows->rows[0].gesture == Gesture{input::scan::kUp, input::mod::kNone});
    CHECK(rows->rows[2].id == "hello.mark");
    CHECK(rows->rows[2].gesture == Gesture{input::scan::kM, input::mod::kNone});
    // RETAINED ON THE CATALOG ROW, beside the descriptor it came with.
    CHECK(retained(r, kind).size() == 3);
    CHECK(retained(r, kind)[1].id == "hello.pick");
    // AND NOTHING WAS SAID: an accepted declaration is not news on the notice line.
    CHECK(r.last_notice().find("hello.") == std::string::npos);
}

TEST_CASE("a declaration with no stamped office is refused, and retains nothing") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    REQUIRE(is_runtime_kind(kind));
    r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
        s.declare_personally(m, actions_for(kHelloPane, {declared("hello.up", "up", input::scan::kUp)}));
    });
    CHECK(r.session().keymap.pane_rows(kind) == nullptr);
    CHECK(retained(r, kind).empty());
    CHECK(r.last_notice().find("hello") == std::string::npos); // silence, not a refusal notice
}

TEST_CASE("a declaration for a pane the office never offered is refused by name") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    REQUIRE(is_runtime_kind(kind));
    redeclare(r, seat, "ghost", {declared("ghost.up", "up", input::scan::kUp)});
    CHECK(r.last_notice() == "`" + std::string(kHelloOffice) +
                                 "/ghost` is not a pane that office has offered -- its "
                                 "actions were not taken");
    CHECK(r.session().keymap.pane_rows(kind) == nullptr);
    CHECK(r.session().keymap.panes.empty());
    // ...AND ONE OFFICE CANNOT DECLARE FOR ANOTHER'S PANE: the pair is the identity.
    ProviderSeat* other = r.mount_provider(kOtherOffice);
    r.drive(other, [](ProviderSeat& s, loom::Mail& m) {
        s.declare(m, actions_for(kHelloPane, {declared("hello.up", "up", input::scan::kUp)}));
    });
    CHECK(r.last_notice().find(std::string(kOtherOffice) + "/hello` is not a pane") !=
          std::string::npos);
    CHECK(r.session().keymap.pane_rows(kind) == nullptr);
}

TEST_CASE("a row colliding with a chord answered above every mode refuses the whole shape and "
          "keeps the previous rows") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    // THE CHORD ABOVE EVERY MODE IS AN APPLICATION ROW: `ctrl+k`, the desktop's key list.
    DesktopSeat* desk = mount_desktop(r);
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_declared(r, seat, kHelloOffice, kHelloPane,
                                                 {declared("hello.up", "row up", input::scan::kUp)});
    REQUIRE(r.session().keymap.pane_rows(kind) != nullptr);

    const Gesture hotkeys{input::scan::kK, input::mod::kCtrl};
    redeclare(r, seat, kHelloPane,
              {declared("hello.up", "row up", input::scan::kUp),
               declared("hello.keys", "keys", hotkeys.scancode, hotkeys.modifiers)});
    // THE REFUSAL NAMES THE PANE AND BOTH ACTIONS, in the keymap file's own words.
    CHECK(r.last_notice() == "Seat @" + std::string(kHelloOffice) + ": " +
                                 collision_sentence(hotkeys, "desktop.hotkeys", "hello.keys"));
    // AND THE PREVIOUS ROWS STAND, on the map and on the catalog row alike.
    REQUIRE(r.session().keymap.pane_rows(kind) != nullptr);
    REQUIRE(r.session().keymap.pane_rows(kind)->rows.size() == 1);
    CHECK(r.session().keymap.pane_rows(kind)->rows[0].id == "hello.up");
    REQUIRE(retained(r, kind).size() == 1);
    CHECK(retained(r, kind)[0].id == "hello.up");
    // ...and the application row still answers, inside the pane.
    press_body(r, kind);
    const std::size_t asked = desk->asked().size();
    r.key(hotkeys.scancode, hotkeys.modifiers);
    REQUIRE(desk->asked().size() == asked + 1);
    CHECK(desk->asked().back() == DesktopSeat::kHotkeysId);
    CHECK(seat->keys.empty());
    CHECK(seat->actions.empty());
}

TEST_CASE("a refresh replaces a pane's rows whole, and an empty declaration clears them") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_declared(r, seat, kHelloOffice, kHelloPane,
                                                 {declared("hello.up", "row up", input::scan::kUp)});
    redeclare(r, seat, kHelloPane,
              {declared("hello.down", "row down", input::scan::kDown),
               declared("hello.mark", "mark", input::scan::kM)});
    REQUIRE(r.session().keymap.pane_rows(kind) != nullptr);
    REQUIRE(r.session().keymap.pane_rows(kind)->rows.size() == 2);
    CHECK(r.session().keymap.pane_rows(kind)->rows[0].id == "hello.down");
    CHECK(retained(r, kind).size() == 2);
    CHECK(r.session().keymap.panes.size() == 1); // replaced, never a second entry

    redeclare(r, seat, kHelloPane, {});
    REQUIRE(r.session().keymap.pane_rows(kind) != nullptr);
    CHECK(r.session().keymap.pane_rows(kind)->rows.empty());
    CHECK(retained(r, kind).empty());
    press_body(r, kind);
    r.key(input::scan::kM);
    CHECK(seat->actions.empty());
    REQUIRE(seat->keys.size() == 1); // raw again, as a pane that declared nothing
}

TEST_CASE("two panes declaring one bare key coexist, and each is asked for its own") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 60); // room for two overlay slots
    ProviderSeat* a = r.mount_provider(kHelloOffice);
    ProviderSeat* b = r.mount_provider(kOtherOffice);
    const std::int64_t first = seat_pane_declared(r, a, kHelloOffice, kHelloPane,
                                                  {declared("a.up", "up", input::scan::kUp)});
    const std::int64_t second = seat_pane_declared(r, b, kOtherOffice, kHelloPane,
                                                   {declared("b.up", "up", input::scan::kUp)});
    REQUIRE(is_runtime_kind(first));
    REQUIRE(is_runtime_kind(second));
    REQUIRE(first != second);
    REQUIRE(r.session().keymap.pane_rows(first) != nullptr);
    REQUIRE(r.session().keymap.pane_rows(second) != nullptr);
    CHECK(r.last_notice().find("authored for both") == std::string::npos);

    press_body(r, first);
    r.key(input::scan::kUp);
    REQUIRE(a->actions.size() == 1);
    CHECK(a->actions[0].pane == std::string(kHelloPane));
    CHECK(a->actions[0].id == "a.up");
    CHECK(b->actions.empty());

    press_body(r, second);
    r.key(input::scan::kUp);
    REQUIRE(b->actions.size() == 1);
    CHECK(b->actions[0].id == "b.up");
    CHECK(a->actions.size() == 1);
}

// ============================================================================
// Dispatch: the resolved id crosses instead of the key
// ============================================================================

TEST_CASE("a declared gesture arrives as the resolved id and an undeclared one as the key; "
          "typing still crosses raw") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    DesktopSeat* desk = mount_desktop(r);
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_declared(r, seat, kHelloOffice, kHelloPane,
                                                 {declared("hello.up", "row up", input::scan::kUp),
                                                  declared("hello.mark", "mark", input::scan::kM)});
    // BEFORE THE PRESS NOBODY HAS THE KEYBOARD: the declaration pointed nothing at the pane.
    CHECK(r.session().panels.keyboard == kNoPaneKind);
    r.key(input::scan::kUp);
    CHECK(seat->actions.empty());
    CHECK(seat->keys.empty());

    press_body(r, kind);
    // A DECLARED KEY: the id, authored as Workshop, and NOT the key.
    r.key(input::scan::kUp);
    REQUIRE(seat->actions.size() == 1);
    CHECK(seat->actions[0].pane == std::string(kHelloPane));
    CHECK(seat->actions[0].id == "hello.up");
    CHECK(seat->action_authors[0] == std::string(kWorkshopProvider));
    CHECK(seat->keys.empty());
    // A DECLARED PRINTABLE: the id, and the character it produced is swallowed -- the
    // pane's `m` row does not also type an `m` into its field.
    r.key(input::scan::kM);
    r.text("m");
    REQUIRE(seat->actions.size() == 2);
    CHECK(seat->actions[1].id == "hello.mark");
    CHECK(seat->keys.empty());
    CHECK(seat->typed.empty());
    // AN UNDECLARED PRINTABLE: the key and the text, exactly as before -- a `p` typed
    // into a field is still a `p`.
    r.key(input::scan::kP);
    r.text("p");
    CHECK(seat->actions.size() == 2);
    REQUIRE(seat->keys.size() == 1);
    CHECK(seat->keys[0].scancode == input::scan::kP);
    REQUIRE(seat->typed.size() == 1);
    CHECK(seat->typed[0].text == "p");
    // THE MODIFIERS ARE PART OF THE MATCH (WL-KEY-04): shift+m is not `hello.mark`.
    r.key(input::scan::kM, input::mod::kShift);
    r.text("M");
    CHECK(seat->actions.size() == 2);
    CHECK(seat->keys.size() == 2);
    CHECK(seat->typed.size() == 2);
    // AND THE ABOVE-MODE CHORDS STILL OUTRANK THE PANE: `^k` asks the desktop for its key list.
    r.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE_FALSE(desk->asked().empty());
    CHECK(desk->asked().back() == DesktopSeat::kHotkeysId);
    CHECK(seat->keys.size() == 2);
    CHECK(seat->actions.size() == 2);
}

// ============================================================================
// The maker's own keymap reaches a pane, in both load orders
// ============================================================================

TEST_CASE("an override authored before the pane arrives is applied when it does, and one loaded "
          "after the pane declared is applied at the load") {
    TempDir dir("pane-actions-override");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"hello.up", "ctrl+u"}}));

    SUBCASE("the file first: its row is preserved as unknown, and moves the pane's row on arrival") {
        PaneRig r;
        r.host.keymap_path = path;
        r.mount_workshop();
        r.ready();
        // Preserved unjudged: nobody has declared `hello.up` yet (WL-KEY-06).
        REQUIRE(r.session().keymap.authored.size() == 1);
        CHECK(r.session().keymap.overrides.empty());
        ProviderSeat* seat = r.mount_provider(kHelloOffice);
        const std::int64_t kind = seat_pane_declared(
            r, seat, kHelloOffice, kHelloPane, {declared("hello.up", "row up", input::scan::kUp)});
        REQUIRE(r.session().keymap.pane_rows(kind) != nullptr);
        CHECK(r.session().keymap.pane_rows(kind)->rows[0].gesture ==
              Gesture{input::scan::kU, input::mod::kCtrl});
        press_body(r, kind);
        r.key(input::scan::kU, input::mod::kCtrl);
        REQUIRE(seat->actions.size() == 1);
        CHECK(seat->actions[0].id == "hello.up");
        // The declared default no longer requests it: an override MOVES a binding.
        r.key(input::scan::kUp);
        CHECK(seat->actions.size() == 1);
        REQUIRE(seat->keys.size() == 1);
        CHECK(seat->keys[0].scancode == input::scan::kUp);
    }
    SUBCASE("the pane first: the file's load re-joins its rows under the file") {
        PaneRig r;
        r.host.keymap_path = path;
        r.mount_workshop();
        ProviderSeat* seat = r.mount_provider(kHelloOffice);
        // Offered and declared BEFORE any surface exists, as a provider loaded before
        // the Skin is: joined under the defaults for now.
        r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
            s.offer(m, PaneOffered{kHelloPane, "Seat", "a recording provider"});
            s.declare(m, actions_for(kHelloPane, {declared("hello.up", "row up", input::scan::kUp)}));
        });
        const RuntimePane* row = r.session().panels.runtime.find(kHelloOffice, kHelloPane);
        REQUIRE(row != nullptr);
        const std::int64_t kind = row->kind;
        REQUIRE(r.session().keymap.pane_rows(kind) != nullptr);
        CHECK(r.session().keymap.pane_rows(kind)->rows[0].gesture ==
              Gesture{input::scan::kUp, input::mod::kNone});
        r.ready(); // the file loads now, and the pane's rows are joined again under it
        REQUIRE(r.session().keymap.authored.size() == 1);
        REQUIRE(r.session().keymap.pane_rows(kind) != nullptr);
        CHECK(r.session().keymap.pane_rows(kind)->rows[0].gesture ==
              Gesture{input::scan::kU, input::mod::kCtrl});
        r.pick(PaneRef{kHelloOffice, kHelloPane});
        press_body(r, kind);
        r.key(input::scan::kU, input::mod::kCtrl);
        REQUIRE(seat->actions.size() == 1);
        CHECK(seat->actions[0].id == "hello.up");
    }
}

TEST_CASE("the keymap file wins: a pane whose rows its bindings collide with is refused in words, "
          "in both orders") {
    TempDir dir("pane-actions-file-wins");
    const std::string path = dir.file("keymap.json");
    // The maker moved the key list onto ctrl+u; the pane's default is ctrl+u too.
    write_keymap_file(path, keymap_file_text("default", {{"desktop.hotkeys", "ctrl+u"}}));
    const Gesture moved{input::scan::kU, input::mod::kCtrl};

    SUBCASE("the file first: the declaration is refused at its admission") {
        PaneRig r;
        r.host.keymap_path = path;
        r.mount_workshop();
        r.ready();
        DesktopSeat* desk = mount_desktop(r); // its key-list row lands where the file put it
        ProviderSeat* seat = r.mount_provider(kHelloOffice);
        r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
            s.offer(m, PaneOffered{kHelloPane, "Seat", "a recording provider"});
            s.declare(m, actions_for(kHelloPane, {declared("hello.up", "row up", input::scan::kU,
                                                           input::mod::kCtrl)}));
        });
        // THE REFUSAL IS SAID AT THE DECLARATION, before any later gesture writes over
        // the one notice line.
        CHECK(r.last_notice() == "Seat @" + std::string(kHelloOffice) + ": " +
                                     collision_sentence(moved, "desktop.hotkeys", "hello.up"));
        const RuntimePane* row = r.session().panels.runtime.find(kHelloOffice, kHelloPane);
        REQUIRE(row != nullptr);
        const std::int64_t kind = row->kind;
        CHECK(r.session().keymap.pane_rows(kind) == nullptr);
        CHECK(retained(r, kind).empty());
        r.pick(PaneRef{kHelloOffice, kHelloPane});
        press_body(r, kind);
        r.key(input::scan::kU, input::mod::kCtrl);
        REQUIRE_FALSE(desk->asked().empty()); // the maker's file is what is in force
        CHECK(desk->asked().back() == DesktopSeat::kHotkeysId);
        CHECK(seat->actions.empty());
    }
    SUBCASE("the pane first: the load re-joins, refuses that pane's rows, and says so once") {
        PaneRig r;
        r.host.keymap_path = path;
        r.mount_workshop();
        DesktopSeat* desk = mount_desktop(r); // declared under the defaults, before the file
        ProviderSeat* seat = r.mount_provider(kHelloOffice);
        r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
            s.offer(m, PaneOffered{kHelloPane, "Seat", "a recording provider"});
            s.declare(m, actions_for(kHelloPane, {declared("hello.up", "row up", input::scan::kU,
                                                           input::mod::kCtrl)}));
        });
        const RuntimePane* row = r.session().panels.runtime.find(kHelloOffice, kHelloPane);
        REQUIRE(row != nullptr);
        const std::int64_t kind = row->kind;
        REQUIRE(r.session().keymap.pane_rows(kind) != nullptr); // joined under the defaults
        r.ready();
        CHECK(r.session().keymap.pane_rows(kind) == nullptr); // dropped under the file
        // ...AND WITHDRAWN, NOT KEPT: a kept declaration would come back into force at a later
        // re-join without its declarer being told (WL-DESK-06). It may declare again.
        CHECK(retained(r, kind).empty());
        CHECK(r.last_notice().find("1 override") != std::string::npos);
        CHECK(r.last_notice().find("Seat @" + std::string(kHelloOffice) + ": " +
                                   collision_sentence(moved, "desktop.hotkeys", "hello.up")) !=
              std::string::npos);
        r.pick(PaneRef{kHelloOffice, kHelloPane});
        press_body(r, kind);
        r.key(input::scan::kU, input::mod::kCtrl);
        REQUIRE_FALSE(desk->asked().empty());
        CHECK(desk->asked().back() == DesktopSeat::kHotkeysId);
        CHECK(seat->actions.empty());
    }
}

// ============================================================================
// The legend and the effective keymap
// ============================================================================

TEST_CASE("the band's legend and the effective keymap print the pane's rows while it holds the "
          "keys") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_declared(
        r, seat, kHelloOffice, kHelloPane,
        {declared("hello.up", "row up", input::scan::kUp), declared("hello.mark", "mark", input::scan::kM),
         declared("hello.rename", "rename", input::scan::kUnknown)});

    // BEFORE THE PRESS the band is Workshop's own, and no pane row is on it.
    {
        const std::vector<std::string> lines = band_lines(r);
        REQUIRE(lines.size() == 2);
        CHECK(lines[0].find("row up") == std::string::npos);
        CHECK(lines[0].find("q quit") != std::string::npos);
    }
    press_body(r, kind);
    {
        const std::vector<std::string> lines = band_lines(r);
        REQUIRE(lines.size() == 2);
        CHECK(lines[0].find("typing goes to Seat @" + std::string(kHelloOffice)) !=
              std::string::npos);
        // THE PANE'S OWN ROWS, and nothing after them: an unbound row teaches no key
        // (WL-KEY-13), and no host row is requestable while a pane holds the keys since the
        // object document's `^s save` and `^o open` retired (VD-26 kept them here until then).
        CHECK(lines[1] == "up row up | m mark");
    }
    // THE EFFECTIVE KEYMAP: the pane's rows under its own name, the unbound one with no key.
    const std::string group = "pane Seat @" + std::string(kHelloOffice);
    const std::string view = hotkeys_text(r);
    CHECK(view.find(group + " | up | row up | hello.up") != std::string::npos);
    CHECK(view.find(group + " | m | mark | hello.mark") != std::string::npos);
    CHECK(view.find(group + " |  | rename | hello.rename") != std::string::npos);

    // AND AN OVERRIDE IS WHAT BOTH SPELL: the band and the view project the effective map.
    ProviderSeat* other = r.mount_provider(kOtherOffice);
    (void)other;
    Keymap moved = r.session().keymap;
    moved.authored.push_back(AuthoredOverride{"hello.mark", "ctrl+u"});
    REQUIRE(join_pane_rows(moved, kind, retained(r, kind)).accepted);
    r.session().keymap = moved; // the value the loader would have produced (a suite's door)
    r.key(input::scan::kP);     // an undeclared key: crosses raw, and the turn repaints
    CHECK(band_lines(r).at(1).find("^u mark") != std::string::npos);
    CHECK(band_lines(r).at(1).find("m mark") == std::string::npos);
}

TEST_CASE("a pane that declared nothing is described as ownership only, exactly as before") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    (void)mount_desktop(r); // the application's launches: what survives above a pane
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);
    // The survivors are the application's (the object document's `^s save | ^o open` were,
    // until it retired), and nothing of the pane's own.
    CHECK(band_lines(r).at(1) == "^t terminal | ^p panes | ^k hotkeys");
    // ...AND THE EFFECTIVE KEYMAP HOLDS NO ROW FOR IT: every key it gets is its own to read.
    const std::string view = hotkeys_text(r);
    CHECK(view.find("pane Seat @" + std::string(kHelloOffice)) == std::string::npos);
}

// ============================================================================
// The first consumer: the real Powers pane acts on the id
// ============================================================================

TEST_CASE("Powers declares four actions, acts on the resolved id, and its typing still crosses raw") {
    PaneRig r;
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);

    // DECLARED BESIDE THE OFFER, and joined: four rows, the pane's own ids, on their
    // shipped defaults -- and the two read-only panes declared nothing.
    REQUIRE(retained(r, kind).size() == 4);
    CHECK(retained(r, kind)[0].id == intro::kPowersActionView);
    CHECK(retained(r, kind)[3].id == intro::kPowersActionSample);
    for (const char* pane : {intro::kLoadedPane, intro::kArrangementPane}) {
        const RuntimePane* row = intro_row(r, pane);
        REQUIRE(row != nullptr);
        CHECK(row->actions.empty());
        CHECK(r.session().keymap.pane_rows(row->kind) == nullptr);
    }
    const PaneRows* rows = r.session().keymap.pane_rows(kind);
    REQUIRE(rows != nullptr);
    REQUIRE(rows->rows.size() == 4);
    CHECK(rows->rows[0].gesture == Gesture{input::scan::kTab, input::mod::kNone});
    CHECK(rows->rows[1].gesture == Gesture{input::scan::kUp, input::mod::kNone});
    CHECK(rows->rows[2].gesture == Gesture{input::scan::kDown, input::mod::kNone});
    CHECK(rows->rows[3].gesture == Gesture{input::scan::kReturn, input::mod::kNone});

    focus_pane(r, kind);
    REQUIRE(pane_rows(r, kind).at(0).find("[Sources]") != std::string::npos);
    // THE VIEW SWITCHES ON THE ID -- Workshop resolved Tab to `powers.view`.
    r.key(input::scan::kTab);
    CHECK(pane_rows(r, kind).at(0).find("[Operators]") != std::string::npos);
    r.key(input::scan::kTab);
    CHECK(pane_rows(r, kind).at(0).find("[Sources]") != std::string::npos);
    // THE CURSOR STEPS ON THE IDS.
    r.key(input::scan::kDown);
    CHECK(pane_rows(r, kind).at(0).find("1/") != std::string::npos);
    r.key(input::scan::kDown);
    CHECK(pane_rows(r, kind).at(0).find("2/") != std::string::npos);
    r.key(input::scan::kUp);
    CHECK(pane_rows(r, kind).at(0).find("1/") != std::string::npos);
    // AND TYPING STILL CROSSES RAW: the query is the component's, not a command.
    r.text("anchor");
    CHECK(pane_rows(r, kind).at(0).find("find:anchor") != std::string::npos);
    r.key(input::scan::kBackspace); // the field's own editing vocabulary, raw
    CHECK(pane_rows(r, kind).at(0).find("find:ancho") != std::string::npos);
    // THE BAND TEACHES THE PANE'S ROWS.
    CHECK(band_lines(r).at(1).find("tab switch view") != std::string::npos);
}

TEST_CASE("a maker's override moves a Powers action, and the key it left no longer acts") {
    // THE PROOF THAT THE ID PATH IS WHAT ACTS: with `powers.view` moved to ctrl+u, the
    // chord switches the view and Tab -- which now crosses as a raw `PaneKey` -- does
    // nothing, because the pane has no raw arm for it any more.
    TempDir dir("powers-override");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{intro::kPowersActionView, "ctrl+u"}}));
    PaneRig r;
    r.host.keymap_path = path;
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);
    const PaneRows* rows = r.session().keymap.pane_rows(kind);
    REQUIRE(rows != nullptr);
    CHECK(rows->rows[0].gesture == Gesture{input::scan::kU, input::mod::kCtrl});

    focus_pane(r, kind);
    REQUIRE(pane_rows(r, kind).at(0).find("[Sources]") != std::string::npos);
    r.key(input::scan::kTab);
    CHECK(pane_rows(r, kind).at(0).find("[Sources]") != std::string::npos); // nothing
    r.key(input::scan::kU, input::mod::kCtrl);
    CHECK(pane_rows(r, kind).at(0).find("[Operators]") != std::string::npos);
    CHECK(band_lines(r).at(1).find("^u switch view") != std::string::npos);
}

// ============================================================================
// A PANE BUILT BEFORE OWNERSHIP EXISTED (VD-27)
// ============================================================================

/// THE PANE-ACTION PROTOCOL EXACTLY AS `84b6bc0` PUBLISHED IT -- one definition, in
/// `weavelib/legacy_pane_protocol.hpp`, shared with the image built from it. A provider compiled
/// against that header derives THESE schemas; if the shipped v1 ever drifts from them again,
/// `same_identity` below says so, and every separately built pane in the world stops registering
/// with this host (Loom GATE-04).
namespace legacy = legacy_protocol;

/// A PROVIDER THAT KNOWS ONLY THE OLD PROTOCOL: it offers a pane and declares its rows through
/// the shapes above, and it has never heard of ownership.
class LegacySeat : public loom::WeaveBase<LegacySeat, SeatState,
                                          loom::Accept<PaneCatalogRequested, PaneRoom,
                                                       PaneActionRequested, SeatDo>,
                                          loom::Emit<PaneOffered, PaneContent, legacy::PaneActions>> {
public:
    explicit LegacySeat(std::string office) : office_(std::move(office)) {}

    std::vector<std::string> said;
    std::function<void(LegacySeat&, loom::Mail&)> next;

    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom& room, loom::Mail& mail) {
        std::vector<surface::SurfaceTextRow> rows;
        rows.push_back(surface::SurfaceTextRow{"an old pane", surface::role::kFill});
        (void)room;
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider,
                                                 PaneContent{"old", std::move(rows)});
    }
    void on(const PaneActionRequested& a, loom::Mail&) { said.push_back(a.id); }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            auto what = next;
            next = nullptr;
            what(*this, mail);
        }
    }

    void offer_and_declare(loom::Mail& mail) {
        (void)mail.as_role(office_).send_to_role(
            kWorkshopProvider, PaneOffered{"old", "Old", "a pane from before ownership"});
        legacy::PaneActions a;
        a.pane = "old";
        a.rows.push_back(legacy::PaneActionRow{"old.up", "row up", input::scan::kUp, 0});
        a.rows.push_back(legacy::PaneActionRow{"old.mark", "mark", input::scan::kM, 0});
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, a);
    }

private:
    std::string office_;
};

TEST_CASE("a pane built against the published version one still registers, declares and dispatches") {
    // ⚔ THE DEFECT: `supersedes` was added to `PaneActionRow` v1, which changed the content-id
    // of that shape AND of the `PaneActions` v1 enclosing it. A provider built against the old
    // header and a host built against the new one could not both register -- ordinary Registry
    // registration refuses the pair with `SchemaConflict` -- and only rebuilding every shipped
    // pane in lockstep hid it. Version one is version one again, and ownership is version two.
    CHECK(loom::same_identity(*loom::schema_of<legacy::PaneActionRow>(),
                              *loom::schema_of<PaneActionRow>()));
    CHECK(loom::same_identity(*loom::schema_of<legacy::PaneActions>(),
                              *loom::schema_of<PaneActions>()));
    CHECK_FALSE(loom::same_identity(*loom::schema_of<legacy::PaneActions>(),
                                    *loom::schema_of<v2::PaneActions>()));

    // ...AND IT WORKS, ON A REAL BUS, WITH THE REAL HOST, BESIDE A PANE THAT OWNS AN ACTION.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    auto seat = std::make_unique<LegacySeat>(std::string(kOtherOffice));
    LegacySeat* old_pane = seat.get();
    loom::Grant say;
    say.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    say.allow_to_any(PaneContent::zen_name, PaneContent::zen_version);
    say.allow_to_any(legacy::PaneActions::zen_name, legacy::PaneActions::zen_version);
    const loom::WeaveId id =
        r.bus.register_weave(std::move(seat), std::move(say), std::string(kOtherOffice));
    old_pane->zen_set_self(id);
    old_pane->next = [](LegacySeat& s, loom::Mail& m) { s.offer_and_declare(m); };
    (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();

    const RuntimePane* row = r.session().panels.runtime.find(kOtherOffice, "old");
    REQUIRE_MESSAGE(row != nullptr, "the old provider's offer was not admitted");
    REQUIRE(row->actions.size() == 2); // its rows, widened into the host's own row type
    CHECK(row->actions[0].id == "old.up");
    CHECK(row->actions[0].supersedes.empty()); // version one owns nothing, and says so
    r.pick(PaneRef{kOtherOffice, "old"});
    const std::int64_t kind = row->kind;
    REQUIRE(r.session().panels.has(kind));
    press_body(r, kind);
    r.key(input::scan::kM);
    REQUIRE_MESSAGE(!old_pane->said.empty(), "the old pane's own row did not dispatch");
    CHECK(old_pane->said.back() == "old.mark");

    // ...AND `^s`, WHICH THE OLD PANE DID NOT DECLARE, REACHES IT AS NOTHING -- and is nothing of
    // the host's either: the object document's save retired with the document.
    const Gesture save{input::scan::kS, input::mod::kCtrl};
    const std::size_t before_save = old_pane->said.size();
    r.key(save.scancode, save.modifiers);
    CHECK(old_pane->said.size() == before_save);
    CHECK(r.session().keymap.above_mode_action(KeyContext::kPane, save.scancode, save.modifiers,
                                               kind) == Act::kNone);

    // BESIDE IT, A PANE THAT STILL NAMES `document.save` AS THE ROW IT STANDS IN FOR -- the second
    // version, on the same host and in the same session. The id retired, and the pane is admitted
    // standing in for nothing: its row is its own. (This rig's screen holds one stack pane, so the
    // old one steps out.)
    r.press_cell(0, screen_of(r.session()).h - 1); // the keys back to the desk
    r.pick(PaneRef{kOtherOffice, "old"});
    REQUIRE_FALSE(r.session().panels.has(kind));
    ProviderSeat* modern = r.mount_provider(kHelloOffice);
    const std::int64_t owner = seat_pane_declared_v2(
        r, modern, kHelloOffice, kHelloPane,
        {declared("hello.save", "save mine", save.scancode, save.modifiers,
                  kOwnableDocumentSave)});
    REQUIRE(owner != kNoPaneKind);
    REQUIRE(r.session().panels.has(owner));
    press_body(r, owner);
    r.key(save.scancode, save.modifiers);
    REQUIRE_MESSAGE(!modern->actions.empty(), "the owning pane's row did not dispatch");
    CHECK(modern->actions.back().id == "hello.save");
    CHECK(r.session().keymap.above_mode_action(KeyContext::kPane, save.scancode, save.modifiers,
                                               owner) == Act::kNone);
}

TEST_CASE("a pane provider built as its own image against the published protocol alone loads, declares and dispatches beside the Editor") {
#ifndef WORKSHOP_SO_LEGACY_PANE
    MESSAGE("no legacy pane image was built for this tree");
#else
    // ⭐ THE BINARY WITNESS, THROUGH THE REAL HOST. The `legacy::` shapes above are compiled
    // into THIS executable and prove schema agreement; `zengine-legacy-pane` is an image of its
    // own, built by tests/CMakeLists.txt from a source that never includes the current pane
    // vocabulary -- read here as a file, so the claim is about the artifact and not about a
    // namespace this suite happens to contain.
    const std::string source = slurp(LEGACY_PANE_SOURCE);
    REQUIRE_FALSE(source.empty());
    // THE DIRECTIVE, not the name: the fixture's own comment names the header it refuses to
    // include, and a tripwire that read the comment would refuse the explanation.
    CHECK(source.find("#include \"workshop/pane_vocabulary.hpp\"") == std::string::npos);
    CHECK(source.find("#include <workshop/pane_vocabulary.hpp>") == std::string::npos);
    CHECK(source.find("#include \"legacy_pane_protocol.hpp\"") != std::string::npos);
    MESSAGE("legacy image: ", WORKSHOP_SO_LEGACY_PANE, ", ",
            std::filesystem::file_size(WORKSHOP_SO_LEGACY_PANE), " bytes");

    PaneRig r;
    r.mount_workshop();
    r.ready();
    // ONE PLAN, TWO IMAGES: the old provider and the current Editor, which declares version two.
    load::LoadPlan plan;
    {
        load::ArtifactIntent old;
        old.stem = "zengine-legacy-pane";
        old.weave = load::WeaveIntent{"zengine.test.legacy"};
        plan.artifacts.push_back(old);
        load::ArtifactIntent editor;
        editor.stem = zengine::editor_pane::kEditorPaneStem;
        editor.weave = load::WeaveIntent{zengine::editor_pane::kEditorPaneRole};
        plan.artifacts.push_back(editor);
    }
    const load::Executed done = r.run_plan(plan);
    REQUIRE_MESSAGE(done.ok, done.refusal);
    r.extent(160, 48);
    const RuntimePane* old = r.session().panels.runtime.find("zengine.test.legacy", "old");
    REQUIRE_MESSAGE(old != nullptr, "the legacy image's offer was not admitted");
    REQUIRE(old->actions.size() == 2); // its version-one rows, widened into the host's own type
    CHECK(old->actions[1].id == "old.mark");
    CHECK(old->actions[1].supersedes.empty()); // version one owns nothing, and says so
    const RuntimePane* editor = r.session().panels.runtime.find(
        zengine::editor_pane::kEditorPaneRole, zengine::editor_pane::kEditorPane);
    REQUIRE_MESSAGE(editor != nullptr, "the Editor image's offer was not admitted");
    bool owns = false;
    for (const v2::PaneActionRow& row : editor->actions) {
        owns = owns || row.supersedes == kOwnableDocumentSave;
    }
    CHECK(owns); // the version-two declaration, admitted in the same session

    // THE OLD PANE, SEATED, PRESSED INTO, AND ASKED FOR ITS OWN ROW BY KEY.
    const std::int64_t kind = old->kind;
    r.pick(PaneRef{"zengine.test.legacy", "old"});
    REQUIRE(r.session().panels.has(kind));
    REQUIRE_FALSE(pane_rows(r, kind).empty());
    CHECK(pane_rows(r, kind)[0].rfind("an old pane", 0) == 0);
    // ...AND THE PRESS IT IS SENT IS THE FIRST VERSION, ONCE, AS IT ALWAYS WAS: the image declares
    // no press door of either version, so the gate refuses it exactly as before a second version
    // existed, and a host that can say more says nothing more to a pane with no door for it.
    std::vector<std::pair<std::uint32_t, loom::RefusalReason>> pressed;
    const loom::WeaveId image = r.kernel.weave_id("zengine-legacy-pane");
    const loom::ObserverId tap = r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.target == image && ev.schema_name == PanePressed::zen_name) {
            pressed.emplace_back(ev.schema_version, ev.refusal.reason);
        }
    });
    press_body(r, kind);
    r.bus.remove_observer(tap);
    REQUIRE(pressed.size() == 1);
    CHECK(pressed[0].first == 1u);
    CHECK(pressed[0].second == loom::RefusalReason::NotAccepted);
    r.key(input::scan::kM);
    REQUIRE(pane_rows(r, kind).size() >= 2);
    CHECK(pane_rows(r, kind)[1] == "acted 1: old.mark"); // the resolved id reached the image
    // ...AND `^s` THERE IS NOBODY'S: the old pane owns nothing, and the host's save retired.
    const Gesture save{input::scan::kS, input::mod::kCtrl};
    r.key(save.scancode, save.modifiers);
    CHECK(pane_rows(r, kind)[1] == "acted 1: old.mark");
    CHECK(r.session().keymap.above_mode_action(KeyContext::kPane, save.scancode, save.modifiers,
                                               kind) == Act::kNone);
#endif
}

/// THE SHAPE AS THIS PR HAD IT BEFORE THE CORRECTION: version one with the ownership field in
/// it. Nothing sends this; it exists so a case can put it in a Registry beside the published
/// version one and watch what a separately built pane provider would have met.
namespace broken {

struct PaneActionRow {
    std::string id;
    std::string label;
    std::int64_t scancode = 0;
    std::int64_t modifiers = 0;
    std::string supersedes;
    ZEN_SHAPE(PaneActionRow, 1, ZEN_FIELD(id), ZEN_FIELD(label), ZEN_FIELD(scancode),
              ZEN_FIELD(modifiers), ZEN_FIELD(supersedes));
};

struct PaneActions {
    std::string pane;
    std::vector<PaneActionRow> rows;
    ZEN_SHAPE(PaneActions, 1, ZEN_FIELD(pane), ZEN_FIELD(rows));
};

} // namespace broken

TEST_CASE("two builds of one published version cannot both register, and that is what a field added in place did") {
    // ⚔ THE DEFECT, REPRODUCED AT ITS OWN LAYER. A published `(name, version)` is frozen and
    // identity across a `.so` seam is the content-id derived from the shape (Loom GATE-04). The
    // ownership field was first added to `PaneActionRow` v1 in place; this is what a provider
    // built against the old header would then have met in an ordinary Registry.
    CHECK_FALSE(loom::same_identity(*loom::schema_of<broken::PaneActionRow>(),
                                    *loom::schema_of<legacy::PaneActionRow>()));
    {
        loom::Registry vocabulary;
        loom::SchemaClaimScope old_pane = vocabulary.claim({loom::schema_of<legacy::PaneActions>()});
        CHECK_THROWS_AS((void)vocabulary.claim({loom::schema_of<broken::PaneActions>()}),
                        loom::SchemaConflict);
    }
    // ...AND THE TWO SHAPES THIS HOST PUBLISHES TODAY LIVE IN ONE REGISTRY WITHOUT A WORD.
    {
        loom::Registry vocabulary;
        loom::SchemaClaimScope old_pane = vocabulary.claim({loom::schema_of<legacy::PaneActions>()});
        loom::SchemaClaimScope current = vocabulary.claim(
            {loom::schema_of<PaneActions>(), loom::schema_of<v2::PaneActions>()});
        CHECK(true); // no conflict: one identity for v1, a different one for v2
    }
}

// =============================================================================
// THE DESKTOP SEAM — the application's own defaults, declared by a participant
//
// The desktop register owns these laws. What these cases are about is the OWNERSHIP move: every
// behaviour below existed before this arc as a line compiled into the host, and what is
// asserted is that it now belongs to a party a maker can replace, with the chain's order
// unchanged.
// =============================================================================

TEST_CASE("WL-KEY-16: an application row is joined, is requested above the modes, and reaches "
          "its declarer as the resolved id") {
    // MUTATION (D1): deleting the above-modes arm in `on(KeyPressed)` -- `asked()` stays empty.
    // MUTATION (D2): joining app rows without applying `authored` -- the moved-key half fails.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    DesktopSeat* desk = mount_desktop(t);
    REQUIRE(desk != nullptr);
    REQUIRE(t.session().keymap.app.size() == 4);

    // ⭐ CTRL+T IS BACK, AND IT IS A PARTICIPANT'S ROW. The retired `workshop.terminal` was a
    // global in this host's own closed catalog; this one is spelled in a weave.
    t.key(input::scan::kT, input::mod::kCtrl);
    REQUIRE(desk->asked().size() == 1);
    CHECK(desk->asked()[0] == DesktopSeat::kTerminalId);

    // ...AND IT IS ANSWERED ABOVE THE MODES, which is the whole difference from a pane's row:
    // the maker's hands are in a pane, and the chord still reaches the desktop.
    const std::int64_t kind = live_offer_pane(t, "zengine.hello", "hello", "Hello");
    REQUIRE(kind != kNoPaneKind);
    open_pane(t, PaneRef{"zengine.hello", "hello"});
    const ui::Rect body = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, kind, screen_of(t.session()))
            .rect);
    t.press_canvas(body.x + 1, body.y + 1);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPane);
    t.key(input::scan::kT, input::mod::kCtrl);
    REQUIRE(desk->asked().size() == 2);
    CHECK(desk->asked()[1] == DesktopSeat::kTerminalId);
}

TEST_CASE("WL-KEY-16: a maker's authored row moves an application row, and `none` disables it") {
    // MUTATION (D3): dropping the `none` arm of `parse_gesture` -- the disable half goes red.
    // MUTATION (D4): dropping the override loop in `join_app_rows` -- the moved half goes red.
    //
    // THE FILE'S OWN ROUND TRIP IS THE CASE ABOVE'S, over a pane's rows. What is new here is
    // that an APPLICATION row is owed the same treatment: the maker's file names ids nobody
    // has declared yet, they are preserved unjudged (WL-KEY-06), and the join applies them.
    Keymap k;
    k.authored.push_back(AuthoredOverride{"desktop.terminal", "ctrl+g"});
    k.authored.push_back(AuthoredOverride{"desktop.deselect", "none"});
    const Written joined = join_app_rows(
        k, std::vector<AppRow>{
               AppRow{"desktop.terminal", "terminal",
                      Gesture{input::scan::kT, input::mod::kCtrl}, 0},
               AppRow{"desktop.deselect", "put down",
                      Gesture{input::scan::kEscape, input::mod::kNone}, 1}});
    REQUIRE_MESSAGE(joined.accepted, joined.refusal);
    REQUIRE(k.app.size() == 2);

    const AppRow* moved = k.app_row_of_id("desktop.terminal");
    REQUIRE(moved != nullptr);
    CHECK(moved->gesture.scancode == input::scan::kG);
    CHECK(moved->gesture.modifiers == input::mod::kCtrl);
    // THE OLD GESTURE REQUESTS NOTHING NOW, and the authored one requests the row.
    CHECK(k.app_action_for(0, KeyContext::kCommand, input::scan::kT, input::mod::kCtrl) ==
          nullptr);
    const AppRow* by_key =
        k.app_action_for(0, KeyContext::kCommand, input::scan::kG, input::mod::kCtrl);
    REQUIRE(by_key != nullptr);
    CHECK(by_key->id == "desktop.terminal");

    // ⭐ AND A DISABLED DEFAULT IS DISABLED, WITH NO COMPILED-IN COPY BEHIND IT. The row is
    // still declared, still listed and still nameable for a later edit; what it has is no
    // key, so `app_action_for` refuses it before it compares anything (`is_bound`).
    const AppRow* off = k.app_row_of_id("desktop.deselect");
    REQUIRE(off != nullptr);
    CHECK_FALSE(is_bound(off->gesture));
    CHECK(k.app_action_for(1, KeyContext::kCommand, input::scan::kEscape, input::mod::kNone) ==
          nullptr);
}

TEST_CASE("WL-DESK-02: the host asks the desktop for the default row, and only an answer that "
          "echoes the ask puts the selection down") {
    // MUTATION (D5): dropping the correlation test in `on(DeselectRequested)` -- the wrong-number
    // answer below puts the selection down and the case goes red. MEASURED: an earlier shape of
    // this case let the STAND-IN answer automatically, which reset the host's record before the
    // wrong answer arrived -- so the gesture test refused it and D5 stayed GREEN. The ask has to
    // be left outstanding at a known gesture for this to be a case about correlation at all.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    DesktopSeat* desk = mount_desktop(t);
    REQUIRE(desk != nullptr);
    desk->autoanswer = false;
    const std::int64_t kind = live_offer_pane(t, "zengine.hello", "hello", "Hello");
    REQUIRE(kind != kNoPaneKind);
    open_pane(t, PaneRef{"zengine.hello", "hello"});
    // PICKED UP THE WAY A MAKER PICKS A PANE UP, so what is put down below is a selection this
    // desk actually made; then the keys are put down, so Escape is command mode's.
    const ui::Rect body = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, kind, screen_of(t.session()))
            .rect);
    t.press_canvas(body.x + 1, body.y + 1);
    release_keys(t);
    REQUIRE(t.session().panels.selected == kind);

    // THE KEY RESOLVES TO THE DECLARED ROW AND THE HOST ASKS ITS OWNER -- and then waits, with
    // the ask outstanding and the maker's latest gesture unchanged.
    t.key(input::scan::kEscape);
    REQUIRE(desk->asked().size() == 1);
    CHECK(desk->asked()[0] == DesktopSeat::kDeselectId);
    const std::uint64_t ask = desk->last_ask();
    CHECK(ask != 0);
    CHECK(t.session().panels.selected == kind); // nothing moved on the ask alone

    // AN ANSWER ECHOING A NUMBER THIS HOST NEVER MINTED MOVES NOTHING, at the very gesture the
    // real ask is outstanding at -- which is the one arrangement in which only the correlation
    // can tell the two apart.
    desktop_does(t, desk,
                 [ask](DesktopSeat& d, loom::Mail& m) { d.deselect_answering(m, ask + 7); });
    CHECK(t.session().panels.selected == kind);
    // ...AND NEITHER DOES ONE THAT ECHOES NOTHING: zero is never an ask.
    desktop_does(t, desk, [](DesktopSeat& d, loom::Mail& m) { d.deselect_answering(m, 0); });
    CHECK(t.session().panels.selected == kind);

    // THE NUMBER THE ASK WENT OUT UNDER PUTS IT DOWN, and only then.
    desktop_does(t, desk,
                 [ask](DesktopSeat& d, loom::Mail& m) { d.deselect_answering(m, ask); });
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.notice().find("unselected") != std::string::npos);

    // ...AND THE ANSWER IS SPENT: the same number again is about a keystroke that is over.
    t.press_canvas(body.x + 1, body.y + 1);
    release_keys(t);
    REQUIRE(t.session().panels.selected == kind);
    desktop_does(t, desk,
                 [ask](DesktopSeat& d, loom::Mail& m) { d.deselect_answering(m, ask); });
    CHECK(t.session().panels.selected == kind);
}

TEST_CASE("WL-KEY-16: the collision law is precedence-aware, and a pane may stand in by name") {
    Keymap k;
    // AN ABOVE-THE-MODES ROW MEETS EVERY HOST ROW, whatever mode it lives in: `ctrl+w` is
    // command mode's `layout.remove`.
    {
        Keymap candidate = k;
        const Written no = join_app_rows(
            candidate, std::vector<AppRow>{AppRow{"x.hot", "x", Gesture{input::scan::kW, input::mod::kCtrl}, 0}});
        CHECK_FALSE(no.accepted);
        CHECK(no.refusal.find("layout.remove") != std::string::npos);
        CHECK(candidate.app.empty()); // atomic: a refusal joins nothing
    }
    // ⭐ A DEFAULT-CLASS ROW MEETS NONE OF THEM, and Escape is the shipped instance: a mode
    // owns Escape while it is open, the default row owns it where nothing did.
    {
        Keymap candidate = k;
        const Written yes = join_app_rows(
            candidate,
            std::vector<AppRow>{AppRow{"x.esc", "x", Gesture{input::scan::kEscape, input::mod::kNone}, 1}});
        CHECK(yes.accepted);
        CHECK(candidate.app.size() == 1);
    }
    // TWO ROWS OF ONE CLASS ON ONE GESTURE IS A COLLISION; two of different classes is not.
    {
        Keymap candidate = k;
        const Written no = join_app_rows(
            candidate,
            std::vector<AppRow>{
                AppRow{"x.a", "a", Gesture{input::scan::kSemicolon, input::mod::kCtrl}, 0},
                AppRow{"x.b", "b", Gesture{input::scan::kSemicolon, input::mod::kCtrl}, 0}});
        CHECK_FALSE(no.accepted);
    }
    {
        Keymap candidate = k;
        const Written yes = join_app_rows(
            candidate,
            std::vector<AppRow>{
                AppRow{"x.a", "a", Gesture{input::scan::kSemicolon, input::mod::kCtrl}, 0},
                AppRow{"x.b", "b", Gesture{input::scan::kSemicolon, input::mod::kCtrl}, 1}});
        CHECK(yes.accepted);
    }
    // A PRECEDENCE THIS BUILD CANNOT NAME IS REFUSED RATHER THAN GUESSED.
    {
        Keymap candidate = k;
        const Written no = join_app_rows(
            candidate,
            std::vector<AppRow>{
                AppRow{"x.c", "c", Gesture{input::scan::kGrave, input::mod::kNone}, 7}});
        CHECK_FALSE(no.accepted);
        CHECK(no.refusal.find("precedence 7") != std::string::npos);
    }
    // A WORKSHOP ID IS REFUSED: an application's ids live in its own namespace.
    {
        Keymap candidate = k;
        const Written no = join_app_rows(
            candidate, std::vector<AppRow>{AppRow{"workshop.quit", "q",
                                                  Gesture{input::scan::kSlash,
                                                          input::mod::kNone},
                                                  0}});
        CHECK_FALSE(no.accepted);
    }
}

TEST_CASE("WL-KEY-16: a pane's row and an above-the-modes application row collide unless the "
          "pane declares it stands in, in both arrival orders") {
    const auto row = [](const char* id, std::int64_t sc, std::int64_t mods,
                        const char* supersedes) {
        v2::PaneActionRow r;
        r.id = id;
        r.label = "x";
        r.scancode = sc;
        r.modifiers = mods;
        r.supersedes = supersedes;
        return r;
    };
    // THE APPLICATION ROW FIRST, THEN THE PANE'S: refused.
    {
        Keymap k;
        REQUIRE(join_app_rows(k, std::vector<AppRow>{
                                     AppRow{"desktop.terminal", "t",
                                            Gesture{input::scan::kT, input::mod::kCtrl}, 0}})
                    .accepted);
        const Written no =
            join_pane_rows(k, 1024, {row("p.tag", input::scan::kT, input::mod::kCtrl, "")});
        CHECK_FALSE(no.accepted);
        CHECK(no.refusal.find("desktop.terminal") != std::string::npos);
    }
    // THE PANE'S FIRST, THEN THE APPLICATION ROW: refused, in the same words.
    {
        Keymap k;
        REQUIRE(
            join_pane_rows(k, 1024, {row("p.tag", input::scan::kT, input::mod::kCtrl, "")})
                .accepted);
        const Written no = join_app_rows(
            k, std::vector<AppRow>{AppRow{"desktop.terminal", "t",
                                          Gesture{input::scan::kT, input::mod::kCtrl}, 0}});
        CHECK_FALSE(no.accepted);
        CHECK(no.refusal.find("desktop.terminal") != std::string::npos);
    }
}

// =============================================================================
// THE REVIEW'S SIX, REPRODUCED AND PINNED -- against the real desktop image, not a stand-in
//
// Each case below is a failure an independent review reproduced on the first pass's head:
// a reloaded desktop left waiting, a launcher cursor on a row it never showed, a refusal
// that named no attempt, and a departed provider presented as available. The images are the
// ones this tree built; the paths are the ones a maker's gestures take.
// =============================================================================

namespace {

namespace dp = zengine::desktop_pane;

/// LOAD THE SHIPPED DESKTOP THROUGH THE REAL KERNEL, in its office.
loom::WeaveId load_real_desktop(PaneRig& r) {
    const loom::WeaveId id = r.load(dp::kDesktopStem, WORKSHOP_SO_DESKTOP_PANE, kDesktopRole);
    REQUIRE(id.valid());
    REQUIRE(r.load_refusals.empty());
    return id;
}

/// WHAT THE LAUNCHER PANE IS SHOWING, one row per line -- the rows Workshop admitted from the
/// weave, read off the presentation's own copy.
std::string launcher_text(PaneRig& r) {
    const RuntimePane* row = r.session().panels.runtime.find(kDesktopRole, dp::kLauncherPane);
    REQUIRE(row != nullptr);
    const ExternalPane* shown = r.session().panels.external_pane(row->kind);
    REQUIRE(shown != nullptr);
    CHECK(shown->refusal.empty()); // every publication fit the room it was granted
    std::string text;
    for (const surface::SurfaceTextRow& line : shown->shown) {
        text += line.text + "\n";
    }
    return text;
}

/// THE MANAGER OPEN AND HOLDING THE KEYS. The desktop's chord is a strict visibility toggle
/// (WL-DESK-13): it opens a closed Manager and closes an open one, so an open Manager is pressed
/// into instead -- on its heading row, which chooses nothing and only points the keys.
void manager_here(PaneRig& r) {
    const PaneRef ref{kDesktopRole, dp::kLauncherPane};
    if (!has_pane(r.session().setup.active, ref)) {
        r.key(input::scan::kP, input::mod::kCtrl);
        return;
    }
    const RuntimePane* row = r.session().panels.runtime.find(kDesktopRole, dp::kLauncherPane);
    REQUIRE(row != nullptr);
    const ui::Rect body = external_body_rect(r.session(), row->kind);
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows);
}

/// THE ROW OF THE LAUNCHER CARRYING THE MARKER, or empty.
std::string marked_row(PaneRig& r) {
    const std::string text = launcher_text(r);
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t end = text.find('\n', at);
        const std::string line = text.substr(at, end - at);
        if (line.rfind("> ", 0) == 0 || line.rfind("? ", 0) == 0) {
            return line;
        }
        at = end == std::string::npos ? text.size() : end + 1;
    }
    return std::string();
}

/// A PRESENTER THAT LISTENS TO THE INVENTORY, AND ASKS FOR IT. It counts publications and
/// keeps each answer with Loom's word on whether it answers this weave's ask.
struct InventoryEarState {
    ZEN_SHAPE(InventoryEarState, 1);
};
class InventoryEar
    : public loom::WeaveBase<InventoryEar, InventoryEarState,
                             loom::Accept<PaneInventory, SeatDo>,
                             loom::Emit<PaneInventoryRequested, PaneOffered>> {
public:
    void on(const PaneInventory& said, loom::Mail& mail) {
        (mail.answers_ask() ? answers : publications).push_back(said);
    }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(InventoryEar&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }
    std::vector<PaneInventory> answers;
    std::vector<PaneInventory> publications;
    std::function<void(InventoryEar&, loom::Mail&)> next;
    static constexpr const char* kOffice = "zengine.test.inventory-ear";
};

InventoryEar* mount_ear(PaneRig& r, loom::WeaveId& id) {
    auto seat = std::make_unique<InventoryEar>();
    InventoryEar* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_any(PaneInventoryRequested::zen_name, PaneInventoryRequested::zen_version);
    grant.allow_to_any(PaneOffered::zen_name, PaneOffered::zen_version);
    id = r.bus.register_weave(std::move(seat), std::move(grant), InventoryEar::kOffice);
    raw->zen_set_self(id);
    return raw;
}

void ear_does(PaneRig& r, loom::WeaveId id, InventoryEar* ear,
              std::function<void(InventoryEar&, loom::Mail&)> what) {
    ear->next = std::move(what);
    (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{},
                                       0));
    r.bus.drain_until_idle();
}

} // namespace

TEST_CASE("a desktop reloaded in place is not left waiting: its new image asks for the inventory "
          "and shows it, though nothing about the inventory changed") {
    TempDir copy("desktop-reload");
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    load_real_desktop(r);
    manager_here(r);
    REQUIRE(launcher_text(r).find("PANES -- ") != std::string::npos);
    const std::string held = marked_row(r);
    REQUIRE_FALSE(held.empty());

    // THE SAME IMAGE UNDER A FRESH PATH, reloaded through the control door: the inventory is
    // exactly what it was, so no publication is owed on its account.
    const std::string image =
        copy.file(("zengine-desktop-again" +
                   std::filesystem::path(WORKSHOP_SO_DESKTOP_PANE).extension().string())
                      .c_str());
    std::filesystem::copy_file(WORKSHOP_SO_DESKTOP_PANE, image);
    r.enqueue_reload(dp::kDesktopStem, image);
    r.bus.drain_until_idle();
    REQUIRE(r.load_refusals.empty());

    const std::string text = launcher_text(r);
    CHECK(text.find("PANES (waiting)") == std::string::npos);
    CHECK(text.find("PANES -- ") != std::string::npos);
    // ...AND THE ROW THE MAKER WAS ON IS STILL THE ONE MARKED: the state kept its identity.
    CHECK(marked_row(r) == held);
}

TEST_CASE("an arriving presenter is answered the inventory as it is now, to itself alone, and an "
          "offer makes the next reading be said again") {
    // MUTATION (R1a): `on(PaneInventoryRequested)` answering nothing -- `answers` stays empty.
    // MUTATION (R1b): `on(PaneOffered)` not clearing `inventory_published_` -- the SECOND,
    // identical offer below is followed by no publication, since nothing in the reading changed.
    // (The first offer adds the ear's own row, so it is said on its own account; asserting only
    // that one let this mutation survive -- measured, which is why there are two.)
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    loom::WeaveId id{};
    InventoryEar* ear = mount_ear(r, id);
    r.key(input::scan::kA); // any gesture: the reading is said once, before anything is asked
    const std::size_t heard = ear->publications.size();

    ear_does(r, id, ear, [](InventoryEar&, loom::Mail& m) {
        (void)m.as_role(InventoryEar::kOffice)
            .send_to_role(kWorkshopProvider, PaneInventoryRequested{});
    });
    REQUIRE(ear->answers.size() == 1);
    CHECK_FALSE(ear->answers[0].panes.empty());
    CHECK(ear->publications.size() == heard); // an answer is not a publication to everyone

    // ...AND AN ASK THAT IS PERSONAL SPEECH IS ANSWERED BY NOBODY.
    ear_does(r, id, ear, [](InventoryEar&, loom::Mail& m) {
        (void)m.send_to_role(kWorkshopProvider, PaneInventoryRequested{});
    });
    CHECK(ear->answers.size() == 1);

    // AN OFFER IS THE MOMENT A NEW LISTENER CERTAINLY EXISTS: the reading is said again even when
    // nothing in it changed. The first offer adds this ear's own row, which changes the reading;
    // the same offer again changes nothing, and is what the record's clearing is for.
    const auto offer = [](InventoryEar&, loom::Mail& m) {
        (void)m.as_role(InventoryEar::kOffice)
            .send_to_role(kWorkshopProvider, PaneOffered{"ear", "Ear", "an inventory listener"});
    };
    ear_does(r, id, ear, offer);
    const std::size_t first = ear->publications.size();
    CHECK(first > heard);
    ear_does(r, id, ear, offer);
    REQUIRE(ear->publications.size() > first);
    // ...AND IT IS THE SAME READING: the same rows, in the same order.
    const std::vector<InventoryPane>& was = ear->publications[first - 1].panes;
    const std::vector<InventoryPane>& now = ear->publications.back().panes;
    REQUIRE(now.size() == was.size());
    for (std::size_t i = 0; i < now.size(); ++i) {
        CHECK(now[i].office == was[i].office);
        CHECK(now[i].pane == was[i].pane);
        CHECK(now[i].available == was[i].available);
    }
}

TEST_CASE("the launcher keeps the row it will open in view, and its feedback on a row of its own") {
    // MUTATION (L1): `window_for` answering the first rows whatever the cursor -- the marker and
    // the last tool's name are gone from the text below.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    ProviderSeat* tools = r.mount_provider("zengine.test.tools");
    r.drive(tools, [](ProviderSeat& s, loom::Mail& m) {
        for (int i = 0; i < 12; ++i) {
            s.offer(m, PaneOffered{"tool-" + std::to_string(i), "Tool " + std::to_string(i),
                                   "a review fixture"});
        }
    });
    load_real_desktop(r);
    manager_here(r);
    const std::vector<CatalogRow> inventory =
        inventory_rows(r.session().setup.active, r.session().panels);
    REQUIRE(inventory.size() > 8); // longer than the launcher's room, which is the point
    CHECK(marked_row(r).find(inventory.front().name) != std::string::npos);

    for (std::size_t i = 1; i < inventory.size(); ++i) {
        r.key(input::scan::kDown);
    }
    const std::string text = launcher_text(r);
    CHECK(marked_row(r).find(inventory.back().name) != std::string::npos);
    CHECK(text.find("more above") != std::string::npos); // the cut is counted, not hidden
    CHECK(text.find(inventory.front().name + "\n") == std::string::npos);

    // BACK TO THE TOP, and the window follows.
    for (std::size_t i = 1; i < inventory.size(); ++i) {
        r.key(input::scan::kUp);
    }
    CHECK(marked_row(r).find(inventory.front().name) != std::string::npos);
    CHECK(launcher_text(r).find("more below") != std::string::npos);
}

TEST_CASE("the launcher's cursor is an identity: rows moving under it do not retarget Return, and "
          "a row that left the list is said, not replaced") {
    // MUTATION (L2): `find_cursor` keeping the index and ignoring the identity -- after the row
    // above it leaves, the marker lands on the row below and Return opens that one.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 60);
    load_real_desktop(r);
    Session& s = const_cast<Session&>(r.session());
    for (const char* ghost : {"g1", "g2", "g3"}) {
        REQUIRE(add_pane(s.setup.active, PaneRef{"zengine.test.ghost", ghost}));
    }
    manager_here(r);
    std::vector<CatalogRow> inventory = inventory_rows(s.setup.active, s.panels);
    std::size_t g2 = inventory.size();
    for (std::size_t i = 0; i < inventory.size(); ++i) {
        g2 = inventory[i].ref.pane == "g2" ? i : g2;
    }
    REQUIRE(g2 < inventory.size());
    for (std::size_t i = 0; i < g2; ++i) {
        r.key(input::scan::kDown);
    }
    REQUIRE(marked_row(r).find("g2") != std::string::npos);

    // THE ROW ABOVE LEAVES THE LIST: g2 now has g1's index, and the marker follows g2.
    REQUIRE(remove_pane(s.setup.active, PaneRef{"zengine.test.ghost", "g1"}));
    manager_here(r); // a gesture: the changed inventory is said
    CHECK(marked_row(r).find("g2") != std::string::npos);
    CHECK(marked_row(r).find("g3") == std::string::npos);
    r.key(input::scan::kReturn);
    CHECK(r.last_notice().find("g2 is not available") != std::string::npos); // g2, by identity

    // ...AND THE ROW THE MARKER HOLDS LEAVES: said, and Return waits for a choice.
    REQUIRE(remove_pane(s.setup.active, PaneRef{"zengine.test.ghost", "g2"}));
    manager_here(r);
    CHECK(marked_row(r).rfind("? ", 0) == 0);
    CHECK(launcher_text(r).find("g2 left the list") != std::string::npos);
    const std::string before = r.last_notice();
    r.key(input::scan::kReturn);
    CHECK(r.last_notice() == before); // no launch was asked for
    CHECK(launcher_text(r).find("Return opened nothing") != std::string::npos);
    CHECK(launcher_text(r).find("g2 left the list") != std::string::npos); // still said
}

TEST_CASE("a verdict answers the declaration it judges: a refused attempt is named by its own "
          "number after a later one was accepted, and an accepted one is given Workshop's") {
    // MUTATION (V1): answering the verdict with an ordinary send -- the correlation is zero and
    // `answers_ask()` is false.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    DesktopSeat* desk = mount_desktop(t);
    REQUIRE(desk->verdicts().size() == 1); // the mount's own declaration
    const std::int64_t first = desk->verdicts()[0].said.declaration;
    CHECK(desk->verdicts()[0].said.accepted);
    CHECK(first > 0);

    AppActions bad;
    bad.rows.push_back(AppActionRow{"demo.open", "launch", input::scan::kC, input::mod::kCtrl,
                                    app_precedence::kAboveModes}); // ctrl+c is the host's quit
    AppActions good;
    good.rows.push_back(AppActionRow{"demo.open", "launch", input::scan::kG, input::mod::kCtrl,
                                     app_precedence::kAboveModes});
    desktop_does(t, desk, [bad, good](DesktopSeat& d, loom::Mail& m) {
        d.declare(m, bad, 101);
        d.declare(m, good, 102);
    });
    REQUIRE(desk->verdicts().size() == 3);
    const DesktopSeat::Verdict& refused = desk->verdicts()[1];
    const DesktopSeat::Verdict& accepted = desk->verdicts()[2];
    CHECK(refused.correlation == 101);
    CHECK(refused.answer);
    CHECK_FALSE(refused.said.accepted);
    CHECK(refused.said.declaration == 0);
    CHECK(refused.said.refusal.find("demo.open") != std::string::npos);
    CHECK(accepted.correlation == 102);
    CHECK(accepted.answer);
    CHECK(accepted.said.accepted);
    CHECK(accepted.said.declaration > first); // a number of its own, never reused
    const AppRow* row = t.session().keymap.app_row_of_id("demo.open");
    REQUIRE(row != nullptr);
    CHECK(row->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
}

TEST_CASE("a declaration the keymap file displaces is withdrawn by the number its verdict gave "
          "it, and a pane that reads verdicts learns both") {
    // MUTATION (V2): `rejoin_app_rows` keeping the rows it could not join -- no withdrawal, and
    // the application rows stay out of the keymap with their declarer believing them in force.
    TempDir dir("desktop-withdrawn");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"desktop.terminal", "ctrl+c"}}));
    PaneRig r;
    r.host.keymap_path = path;
    r.mount_workshop();
    DesktopSeat* desk = mount_desktop(r);
    REQUIRE(desk->verdicts().size() == 1);
    const std::int64_t in_force = desk->verdicts()[0].said.declaration;
    REQUIRE(in_force > 0);
    REQUIRE(r.session().keymap.app_row_of_id("desktop.terminal") != nullptr);

    r.ready(); // the file loads, and moves `desktop.terminal` onto the host's quit
    REQUIRE(desk->withdrawals().size() == 1);
    CHECK(desk->withdrawals()[0].declaration == in_force);
    CHECK(desk->withdrawals()[0].pane.empty());
    CHECK(desk->withdrawals()[0].refusal.find("desktop.terminal") != std::string::npos);
    CHECK(r.session().keymap.app_row_of_id("desktop.terminal") == nullptr);
}

TEST_CASE("a withdrawal naming a predecessor's declaration does not reach the successor's rows: "
          "the reloaded desktop shows only its own verdict") {
    // THE CROSSING, ORDERED EXACTLY: the file's load withdraws the first image's declaration
    // while a reload of the desktop is queued behind it, so the withdrawal is delivered to the
    // SECOND image -- which never held that number, and says nothing about it.
    TempDir dir("desktop-crossing");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"desktop.terminal", "ctrl+c"}}));
    PaneRig r;
    r.host.keymap_path = path;
    r.mount_workshop();
    r.extent(160, 48);
    load_real_desktop(r);
    const std::string image =
        dir.file(("zengine-desktop-successor" +
                  std::filesystem::path(WORKSHOP_SO_DESKTOP_PANE).extension().string())
                     .c_str());
    std::filesystem::copy_file(WORKSHOP_SO_DESKTOP_PANE, image);
    (void)r.bus.publish(loom::Message(loom::to_value(surface::SurfaceReady{}), loom::WeaveId{},
                                      loom::WeaveId{}, 0));
    r.enqueue_reload(dp::kDesktopStem, image);
    r.bus.drain_until_idle();
    REQUIRE(r.load_refusals.empty());

    std::string floor;
    for (const surface::SurfaceTextRow& row : r.session().backdrop) {
        floor += row.text + "\n";
    }
    CAPTURE(floor);
    const std::string notice = r.last_notice();
    CAPTURE(notice);
    const bool app_rows_in_force = r.session().keymap.app_row_of_id("desktop.terminal") != nullptr;
    CAPTURE(app_rows_in_force);
    const std::string word = r.session().keymap.authored.empty() ? "no authored rows" : "authored";
    CAPTURE(word);
    // THE SUCCESSOR'S OWN DECLARATION MEETS THE SAME FILE AND IS REFUSED -- its verdict, shown.
    CHECK(floor.find("keys refused:") != std::string::npos);
    // THE PREDECESSOR'S WITHDRAWAL NAMES A NUMBER THIS IMAGE NEVER HELD, and is not shown.
    CHECK(floor.find("keys withdrawn:") == std::string::npos);
}

TEST_CASE("a pane whose provider left is unavailable in the launcher and refused at launch, while "
          "its identity and the desk row naming it stay") {
    // MUTATION (A1): `provider_present` answering from the catalog alone -- Info reads `[open]`
    // after its library is unloaded, and its launch is not refused.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    load_real_desktop(r);
    REQUIRE(r.load("zengine-info-pane", WORKSHOP_SO_INFO_PANE, "zengine.info").valid());
    REQUIRE(r.host.holder_accepts("zengine.info", *loom::schema_of<PaneRoom>()));
    const PaneRef info{"zengine.info", "info"};
    REQUIRE(has_pane(r.session().setup.active, info)); // the shipped desk names it

    manager_here(r);
    CHECK(launcher_text(r).find("[open] Info") != std::string::npos);

    REQUIRE(r.unload("zengine-info-pane"));
    REQUIRE_FALSE(r.host.holder_accepts("zengine.info", *loom::schema_of<PaneRoom>()));
    manager_here(r); // a gesture: the reading is taken again, now
    CHECK(launcher_text(r).find("[gone] Info") != std::string::npos);
    std::string floor;
    for (const surface::SurfaceTextRow& row : r.session().backdrop) {
        floor += row.text + "\n";
    }
    CHECK(floor.find("unavailable: Info") != std::string::npos);

    // THE LAUNCH JUDGMENT ASKS THE SAME FACT, AT ITS OWN MOMENT.
    const std::vector<CatalogRow> rows = inventory_rows(r.session().setup.active,
                                                        r.session().panels);
    std::size_t at = rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        at = rows[i].ref == info ? i : at;
    }
    REQUIRE(at < rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        r.key(input::scan::kUp);
    }
    for (std::size_t i = 0; i < at; ++i) {
        r.key(input::scan::kDown);
    }
    r.key(input::scan::kReturn);
    CHECK(r.last_notice().find("nothing holds `zengine.info` now") != std::string::npos);
    // ...AND NOTHING AUTHORED MOVED: the desk still names Info, and its identity is known.
    CHECK(has_pane(r.session().setup.active, info));
    CHECK(r.session().panels.runtime.find("zengine.info", "info") != nullptr);

    // PRESENCE COMES BACK WITH A HOLDER: loaded again, it is available again.
    REQUIRE(r.load("zengine-info-pane", WORKSHOP_SO_INFO_PANE, "zengine.info").valid());
    manager_here(r);
    CHECK(launcher_text(r).find("[gone] Info") == std::string::npos);
}

TEST_CASE("a pane the run is still loading is pending, not unavailable: the launcher marks it "
          "`[load]`, the floor names nothing to build, and a launch says it is not here yet") {
    // MUTATION (Q1): `inventory_reading` leaving `pending` unset -- the launcher says `[gone]`
    // and the floor tells a maker to build a tool whose plan row has not been reached.
    // MUTATION (Q3): the floor listing every unavailable row, pending or not -- the floor half.
    PaneRig r;
    bool info_to_come = true; // the host's answer, as `workshop.cpp` wires it over the executor
    r.host.office_pending = [&info_to_come](std::string_view office) {
        return info_to_come && office == "zengine.info";
    };
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    load_real_desktop(r);
    const PaneRef info{"zengine.info", "info"};
    REQUIRE(has_pane(r.session().setup.active, info)); // the shipped desk names it; nothing offers it
    const auto floor_text = [&r] {
        std::string floor;
        for (const surface::SurfaceTextRow& row : r.session().backdrop) {
            floor += row.text + "\n";
        }
        return floor;
    };

    manager_here(r);
    CHECK(launcher_text(r).find("[load] info") != std::string::npos);
    CHECK(launcher_text(r).find("[gone]") == std::string::npos);
    CHECK(floor_text().find("unavailable") == std::string::npos);

    // THE LAUNCH IS STILL REFUSED -- a launch loads nothing -- IN WORDS THAT ARE NOT A VERDICT.
    const std::vector<CatalogRow> rows = inventory_rows(r.session().setup.active,
                                                        r.session().panels);
    std::size_t at = rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        at = rows[i].ref == info ? i : at;
    }
    REQUIRE(at < rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        r.key(input::scan::kUp);
    }
    for (std::size_t i = 0; i < at; ++i) {
        r.key(input::scan::kDown);
    }
    r.key(input::scan::kReturn);
    CHECK(r.last_notice().find("is not here yet") != std::string::npos);
    CHECK(r.last_notice().find("build it") == std::string::npos);

    // ...AND ONCE THE RUN HAS SETTLED WITHOUT IT, THE VERDICT IS SAID: gone, and on the floor.
    info_to_come = false;
    manager_here(r); // a gesture: the reading is taken again, now
    CHECK(launcher_text(r).find("[gone] info") != std::string::npos);
    CHECK(floor_text().find("unavailable: info") != std::string::npos);
}

TEST_CASE("WL-DESK-12: a close takes a pane off the desk and leaves its provider holding; a close "
          "of a pane that is not there is refused and opens nothing") {
    // MUTATION (C1): `close_pane` answering `closed` without taking the row off the desk -- the
    // pane stays seated. MUTATION (C2): a close that is not on the desk falling through to the
    // launch door -- the second close opens it.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    DesktopSeat* desk = mount_desktop(r);
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    const PaneRef hello{kHelloOffice, kHelloPane};
    REQUIRE(r.session().panels.has(kind));

    desktop_does(r, desk, [](DesktopSeat& d, loom::Mail& m) {
        d.close(m, kHelloOffice, kHelloPane);
    });
    REQUIRE(desk->closed().size() == 1);
    CHECK(desk->closed().back().closed);
    CHECK(desk->closed().back().refusal.empty());
    CHECK_FALSE(has_pane(r.session().setup.active, hello));
    CHECK_FALSE(r.session().panels.has(kind));
    CHECK(r.last_notice().rfind("closed ", 0) == 0);
    // ⚠ NOTHING WAS UNLOADED: the office is still held, and the catalog still knows the pane.
    CHECK(r.host.holder_accepts(kHelloOffice, *loom::schema_of<PaneRoom>()));
    CHECK(r.session().panels.runtime.find(kHelloOffice, kHelloPane) != nullptr);

    // A SECOND CLOSE IS REFUSED IN WORDS, and it opens nothing.
    desktop_does(r, desk, [](DesktopSeat& d, loom::Mail& m) {
        d.close(m, kHelloOffice, kHelloPane);
    });
    REQUIRE(desk->closed().size() == 2);
    CHECK_FALSE(desk->closed().back().closed);
    CHECK(desk->closed().back().refusal.find("is not on this desk") != std::string::npos);
    CHECK_FALSE(r.session().panels.has(kind));
    CHECK(r.session().notice_is_bad);

    // ...AND A LAUNCH BRINGS IT BACK FROM THE SAME PROVIDER, which never went anywhere.
    desktop_does(r, desk, [](DesktopSeat& d, loom::Mail& m) {
        d.launch(m, kHelloOffice, kHelloPane);
    });
    REQUIRE_FALSE(desk->launched().empty());
    CHECK(desk->launched().back().opened);
    CHECK(r.session().panels.has(kind));
}

TEST_CASE("the shipped desktop's x closes the row its marker holds, and Return opens it again: "
          "the mark says which, and the provider never left") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    load_real_desktop(r);
    REQUIRE(r.load("zengine-info-pane", WORKSHOP_SO_INFO_PANE, "zengine.info").valid());
    const PaneRef info{"zengine.info", "info"};
    REQUIRE(has_pane(r.session().setup.active, info)); // the shipped desk names it

    manager_here(r); // the launcher, open and holding the keys
    const std::vector<CatalogRow> rows =
        inventory_rows(r.session().setup.active, r.session().panels);
    std::size_t at = rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        at = rows[i].ref == info ? i : at;
    }
    REQUIRE(at < rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        r.key(input::scan::kUp);
    }
    for (std::size_t i = 0; i < at; ++i) {
        r.key(input::scan::kDown);
    }
    REQUIRE(marked_row(r).find("[open] Info") != std::string::npos);

    r.key(input::scan::kX);
    CHECK_FALSE(has_pane(r.session().setup.active, info));
    CHECK(marked_row(r).find("[    ] Info") != std::string::npos);
    CHECK(r.host.holder_accepts("zengine.info", *loom::schema_of<PaneRoom>()));

    // A SECOND x ON THE SAME ROW: the host's refusal is the launcher's notice, and nothing opened.
    r.key(input::scan::kX);
    CHECK(launcher_text(r).find("is not on this desk") != std::string::npos);
    CHECK_FALSE(has_pane(r.session().setup.active, info));

    r.key(input::scan::kReturn);
    CHECK(has_pane(r.session().setup.active, info));
    CHECK(marked_row(r).find("[open] Info") != std::string::npos);
}

TEST_CASE("a choice whose row left stays unchosen across a desktop replacement and the "
          "publications after it: Return and x reach no neighbour, and a row chosen then is "
          "obeyed") {
    // MUTATION (K1): `find_cursor` clearing the keys of a choice whose row left, as the reviewed
    // image did -- the successor reads them as never chosen and gives its marker to the pane that
    // slid into the place: Return then hands Info the keys, and x takes it off the desk.
    TempDir copy("desktop-lost-choice");
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 60);
    load_real_desktop(r);
    REQUIRE(r.load("zengine-info-pane", WORKSHOP_SO_INFO_PANE, "zengine.info").valid());
    Session& s = const_cast<Session&>(r.session());
    const PaneRef info{"zengine.info", "info"};
    const PaneRef ghost{"zengine.test.ghost", "removed-pane"};
    REQUIRE(add_pane(s.setup.active, ghost)); // authored, and offered by nobody: unresolved

    // THE MAKER CHOOSES THE UNRESOLVED ROW AND CLOSES IT WITH THE ORDINARY `x`, which takes it off
    // the desk -- and, since nothing offers it, out of the list.
    manager_here(r);
    const std::vector<CatalogRow> rows = inventory_rows(s.setup.active, s.panels);
    std::size_t at = rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        at = rows[i].ref == ghost ? i : at;
    }
    REQUIRE(at < rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        r.key(input::scan::kUp);
    }
    for (std::size_t i = 0; i < at; ++i) {
        r.key(input::scan::kDown);
    }
    REQUIRE(marked_row(r).find("removed-pane") != std::string::npos);
    r.key(input::scan::kX);
    REQUIRE_FALSE(has_pane(s.setup.active, ghost));
    // INFO SLID INTO THE PLACE THE CHOSEN ROW HELD: the neighbour a replacement must not take.
    REQUIRE(marked_row(r).rfind("? [open] Info", 0) == 0);
    REQUIRE(launcher_text(r).find("removed-pane left the list") != std::string::npos);

    // AN UNCHANGED COPY OF THE DESKTOP, RELOADED THROUGH THE KERNEL -- then one publication more.
    const std::string image =
        copy.file(("zengine-desktop-again" +
                   std::filesystem::path(WORKSHOP_SO_DESKTOP_PANE).extension().string())
                      .c_str());
    std::filesystem::copy_file(WORKSHOP_SO_DESKTOP_PANE, image);
    r.enqueue_reload(dp::kDesktopStem, image);
    r.bus.drain_until_idle();
    REQUIRE(r.load_refusals.empty());
    CHECK(marked_row(r).rfind("? ", 0) == 0);
    CHECK(launcher_text(r).find("removed-pane left the list") != std::string::npos);
    ProviderSeat* tools = r.mount_provider("zengine.test.tools");
    r.drive(tools, [](ProviderSeat& p, loom::Mail& m) {
        p.offer(m, PaneOffered{"tool", "Tool", "a publication after the replacement"});
    });
    CHECK(marked_row(r).rfind("? ", 0) == 0);
    CHECK(launcher_text(r).find("removed-pane left the list") != std::string::npos);

    // x AND RETURN, EACH WITH THE PANE MANAGER HOLDING THE KEYS: Info stays on the desk, the keys
    // stay where they are, and the desk is as it was. Each is pressed from the Pane Manager, so
    // neither can hide the other by moving the keys first.
    const RuntimePane* manager = s.panels.runtime.find(kDesktopRole, dp::kLauncherPane);
    const RuntimePane* inspector = s.panels.runtime.find(info.provider, info.pane);
    REQUIRE(manager != nullptr);
    REQUIRE(inspector != nullptr);
    manager_here(r);
    REQUIRE(s.panels.keyboard == manager->kind);
    const std::vector<SetupPane> desk = s.setup.active.panes;
    r.key(input::scan::kX);
    CHECK(has_pane(s.setup.active, info));
    CHECK(s.setup.active.panes == desk);
    CHECK(launcher_text(r).find("x closed nothing") != std::string::npos);
    manager_here(r);
    REQUIRE(s.panels.keyboard == manager->kind);
    r.key(input::scan::kReturn);
    CHECK(s.panels.keyboard == manager->kind);
    CHECK(s.setup.active.panes == desk);
    CHECK(launcher_text(r).find("Return opened nothing") != std::string::npos);

    // A ROW THE MAKER CHOOSES NOW IS THE CHOICE, and Return obeys it.
    const std::vector<CatalogRow> now = inventory_rows(s.setup.active, s.panels);
    std::size_t info_at = now.size();
    for (std::size_t i = 0; i < now.size(); ++i) {
        info_at = now[i].ref == info ? i : info_at;
    }
    REQUIRE(info_at < now.size());
    for (std::size_t i = 0; i < now.size(); ++i) {
        r.key(input::scan::kUp);
    }
    for (std::size_t i = 0; i < info_at; ++i) {
        r.key(input::scan::kDown);
    }
    REQUIRE(marked_row(r).rfind("> [open] Info", 0) == 0);
    r.key(input::scan::kReturn);
    CHECK(s.panels.keyboard == inspector->kind);
}

TEST_CASE("the shipped desktop shows Workshop's verdict on its own declaration, and only for the "
          "attempt it is waiting on") {
    TempDir dir("desktop-own-verdict");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"desktop.terminal", "ctrl+c"}}));
    PaneRig r;
    r.host.keymap_path = path;
    r.mount_workshop();
    r.ready(); // the file first: the desktop's declaration meets it at admission
    r.extent(160, 48);
    load_real_desktop(r);
    std::string floor;
    for (const surface::SurfaceTextRow& row : r.session().backdrop) {
        floor += row.text + "\n";
    }
    CAPTURE(floor);
    CHECK(floor.find("keys refused:") != std::string::npos);
    CHECK(r.session().keymap.app_row_of_id("desktop.terminal") == nullptr);
}

// =============================================================================
// THE PANE CREATOR, PRESENTED BY THE SHIPPED DESKTOP
//
// The keys and the name line were the host Pane Manager's until it retired; they are the desktop
// Pane Manager's own rows now (WL-MAKER-11). The host's half -- the definition, its file and
// every refusal -- is pinned in the creator source; what is pinned here is that the real image's
// rows reach those doors and show what they answered, and that its name line is a line.
// =============================================================================

namespace {

/// THE LAUNCHER, OPEN AND HOLDING THE KEYS, by the desktop's own chord.
void open_launcher(PaneRig& r) {
    manager_here(r);
    const RuntimePane* row = r.session().panels.runtime.find(kDesktopRole, dp::kLauncherPane);
    REQUIRE(row != nullptr);
    REQUIRE(r.session().panels.keyboard == row->kind);
}

/// A BARE LETTER AS THE PLATFORM DELIVERS IT: the key, then the character it produced -- which
/// the host swallows when the key is a declared row (WL-KEY-15) and hands on when it is not.
void press_letter(PaneRig& r, std::int64_t code, const char* produced) {
    r.key(code);
    r.text(produced);
}

void type_into(PaneRig& r, const std::string& text) {
    for (const char c : text) {
        r.text(std::string(1, c));
    }
}

/// A RIG WITH THE SHIPPED DESKTOP LOADED AND A PANE FILE THE HOST MAY WRITE.
struct CreatorRig {
    TempDir dir;
    PaneRig r;
    explicit CreatorRig(const char* name) : dir(name) {
        r.host.pane_path = dir.file("pane.json");
        r.mount_workshop();
        r.ready();
        r.extent(160, 48);
        load_real_desktop(r);
    }
};

/// A KEY OR TYPED TEXT QUEUED AND NOT DISPATCHED. Several queued, then one drain, are what a
/// platform delivers in one poll: every one is in the queue before the first is dispatched, so
/// the replies to the first cross the later ones. (`r.key` and `r.text` drain, one poll each.)
void queue_key(PaneRig& r, std::int64_t code, std::int64_t mods = input::mod::kNone) {
    (void)r.bus.publish(loom::Message(loom::to_value(input::KeyPressed{code, "", mods}),
                                      loom::WeaveId{}, loom::WeaveId{}, 0));
}

void queue_text(PaneRig& r, const std::string& typed) {
    (void)r.bus.publish(loom::Message(loom::to_value(input::TextEntered{typed}), loom::WeaveId{},
                                      loom::WeaveId{}, 0));
}

/// ONE OF THE PANE MANAGER'S OWN IDS SAID AS WORKSHOP'S OFFICE, QUEUED -- the verified door
/// `workshop_action` spends, undrained. A key resolves against the rows Workshop holds, and those
/// trail the pane's own re-declaration by a delivery, so a burst that must open a new draft before
/// an answer arrives says the ids Workshop would send in the order it would send them.
void queue_as_workshop(PaneRig& r, const char* id) {
    const loom::Ticket sent = r.bus.office_send_to_role_as(
        r.workshop_id, kWorkshopProvider, kDesktopRole,
        loom::Message(loom::to_value(PaneActionRequested{dp::kLauncherPane, id}), r.workshop_id,
                      r.workshop_id, 0));
    REQUIRE(sent.valid());
}

/// WHAT CROSSED THE CREATOR'S DOOR, read at delivery: the name each make the host's door received
/// carried, and how many answers reached the desktop -- the asks that LEFT the pane, not the ones
/// it meant to send.
struct MakerTap {
    loom::Switchboard& bus;
    loom::ObserverId tap{};
    std::vector<std::string> asked;
    int answered = 0;
    MakerTap(loom::Switchboard& on, loom::WeaveId host, loom::WeaveId desktop) : bus(on) {
        tap = bus.add_observer([this, host, desktop](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered) {
                return;
            }
            if (ev.target == host && ev.schema_name == MakerPaneRequested::zen_name &&
                ev.payload != nullptr) {
                asked.push_back(loom::from_value<MakerPaneRequested>(*ev.payload).name);
            } else if (ev.target == desktop && ev.schema_name == MakerPaneAnswered::zen_name) {
                ++answered;
            }
        });
    }
    ~MakerTap() { bus.remove_observer(tap); }
    MakerTap(const MakerTap&) = delete;
    MakerTap& operator=(const MakerTap&) = delete;
};

/// WHAT THE BUS DID WITH ONE SHAPE THE DESKTOP SENT, read off its tap: each refusal of the
/// desktop's own send (Loom's safe reason, and the attempt it refused), each delivery to anybody,
/// and each of Loom's notices delivered back to the desktop naming a refused attempt of it.
struct SendTap {
    loom::Switchboard& bus;
    loom::ObserverId tap{};
    std::vector<std::string> reasons;
    std::vector<std::uint64_t> attempts;
    int delivered = 0;
    int notices = 0;
    SendTap(loom::Switchboard& on, loom::WeaveId desktop, const char* shape) : bus(on) {
        tap = bus.add_observer([this, desktop, shape](const loom::BusEvent& ev) {
            if (ev.schema_name == shape) {
                if (ev.kind == loom::EventKind::Refused && ev.sender == desktop) {
                    reasons.push_back(loom::name_of(ev.refusal.reason));
                    attempts.push_back(ev.seq);
                } else if (ev.kind == loom::EventKind::Delivered) {
                    ++delivered;
                }
            } else if (ev.kind == loom::EventKind::Delivered && ev.target == desktop &&
                       ev.schema_name == loom::DispatchRefused::zen_name &&
                       ev.payload != nullptr &&
                       loom::from_value<loom::DispatchRefused>(*ev.payload).shape == shape) {
                ++notices;
            }
        });
    }
    ~SendTap() { bus.remove_observer(tap); }
    SendTap(const SendTap&) = delete;
    SendTap& operator=(const SendTap&) = delete;
};

/// LOOM REFUSES THE DESKTOP'S NEXT MAKE AT DISPATCH -- Info's `refuse_next_commit`, for the Pane
/// Creator. The turn stops where the desktop has heard `heard` of Workshop's resolved actions, its
/// make queued behind them and not delivered; Workshop's weave is killed before that delivery, a
/// real lifecycle change by the host's own authority, so the bus refuses the queued make and tells
/// its author by that attempt. Workshop is then revived in place from its own snapshot -- the same
/// session -- and asks the room who has panes.
void refuse_next_make(PaneRig& r, int heard) {
    const loom::WeaveId desk = r.bus.role_holder(kDesktopRole);
    loom::Switchboard& bus = r.bus;
    int actions = 0;
    bool stopped = false;
    const loom::ObserverId tap = bus.add_observer([&](const loom::BusEvent& ev) {
        if (!stopped && ev.kind == loom::EventKind::Delivered && ev.target == desk &&
            ev.schema_name == PaneActionRequested::zen_name && ++actions == heard) {
            stopped = true;
            bus.stop();
        }
    });
    for (int turns = 0; turns < 16 && !stopped; ++turns) {
        (void)bus.pump_pending();
    }
    REQUIRE(stopped);
    const std::string bytes = bus.snapshot_bytes(r.workshop_id);
    bus.kill(r.workshop_id);
    bus.drain_until_idle();
    bus.remove_observer(tap);
    REQUIRE(bus.swap_state(r.workshop_id, bytes).revived);
    r.ready();
}

/// WHAT A STAND-IN IN WORKSHOP'S OFFICE KEEPS AND SAYS: the launcher's last rows as the office
/// was told them, and Workshop's resolved name id, said to the desktop under that office.
struct StandInVoice {
    std::vector<std::string> rows;
    void heard(const PaneContent& said, const loom::Mail& mail) {
        if (!mail.authored_from_role(kDesktopRole) || said.pane != dp::kLauncherPane) {
            return;
        }
        rows.clear();
        for (const surface::SurfaceTextRow& row : said.rows) {
            rows.push_back(row.text);
        }
    }
    static void say_name(loom::Mail& mail) {
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(kDesktopRole, PaneActionRequested{dp::kLauncherPane, kCreatorNameId});
    }
};

/// AN OFFICE HOLDING `zengine.workshop` IN WORKSHOP'S PLACE, WITH NO MAKER DOOR. A make sent to
/// it is queued (the desktop declares the shape, so it resolves) and refused at dispatch as
/// NotAccepted; a paste, with no skin office held, is refused at dispatch as NoSuchTarget. Either
/// way Loom's own notice names the attempt. `paste` makes the next `SeatDo` hand the name line
/// Ctrl+V instead of saying the name id.
class DoorlessDesk
    : public loom::WeaveBase<DoorlessDesk, SeatState,
                             loom::Accept<PaneOffered, PaneActions, PaneContent,
                                          v3::PaneContent, SeatDo>,
                             loom::Emit<PaneActionRequested, PaneKey>> {
public:
    void on(const PaneOffered&, loom::Mail&) {}
    void on(const PaneActions&, loom::Mail&) {}
    void on(const PaneContent& said, loom::Mail& mail) { voice.heard(said, mail); }
    /// ...AND THE SHIPPED DESKTOP'S NUMBERED CONTENT, heard as the same rows.
    void on(const v3::PaneContent& said, loom::Mail& mail) {
        voice.heard(PaneContent{said.pane, said.rows}, mail);
    }
    void on(const SeatDo&, loom::Mail& mail) {
        if (!paste) {
            StandInVoice::say_name(mail);
            return;
        }
        (void)mail.as_role(kWorkshopProvider)
            .send_to_role(kDesktopRole,
                          PaneKey{dp::kLauncherPane, input::scan::kV, input::mod::kCtrl});
    }
    StandInVoice voice;
    bool paste = false;
};

/// ...AND ONE THAT TAKES THE PANE CREATOR'S ASKS AND ANSWERS NOTHING, EVER: delivered silence.
class SilentDesk
    : public loom::WeaveBase<SilentDesk, SeatState,
                             loom::Accept<PaneOffered, PaneActions, PaneContent, v3::PaneContent,
                                          MakerPaneRequested, SeatDo>,
                             loom::Emit<PaneActionRequested>> {
public:
    void on(const PaneOffered&, loom::Mail&) {}
    void on(const PaneActions&, loom::Mail&) {}
    void on(const PaneContent& said, loom::Mail& mail) { voice.heard(said, mail); }
    void on(const v3::PaneContent& said, loom::Mail& mail) {
        voice.heard(PaneContent{said.pane, said.rows}, mail);
    }
    void on(const MakerPaneRequested&, loom::Mail&) { ++received; }
    void on(const SeatDo&, loom::Mail& mail) { StandInVoice::say_name(mail); }
    StandInVoice voice;
    int received = 0;
};

/// PUT A STAND-IN IN WORKSHOP'S OFFICE, granted to say the launcher's ids to the desktop.
template <class Office>
Office* stand_in(PaneRig& r, loom::WeaveId& id) {
    auto held = std::make_unique<Office>();
    Office* raw = held.get();
    loom::Grant say;
    say.allow_to_role(PaneActionRequested::zen_name, PaneActionRequested::zen_version,
                      kDesktopRole);
    say.allow_to_role(PaneKey::zen_name, PaneKey::zen_version, kDesktopRole);
    id = r.bus.register_weave(std::move(held), std::move(say), std::string(kWorkshopProvider));
    raw->zen_set_self(id);
    return raw;
}

void stand_in_says(PaneRig& r, loom::WeaveId id) {
    (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                       loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
}

/// A STRANGER: an ordinary weave granted the refusal notice's shape, as any weave may be. What it
/// sends is its own speech, whatever the payload claims.
class NoticeStranger : public loom::WeaveBase<NoticeStranger, SeatState, loom::Accept<SeatDo>,
                                              loom::Emit<loom::DispatchRefused>> {
public:
    void on(const SeatDo&, loom::Mail&) {}
};

} // namespace

TEST_CASE("WL-MAKER-11: the shipped Pane Manager makes a pane from a typed name -- `n` opens its "
          "line, the trigger is not typed, and Return asks the host's door") {
    CreatorRig c("creator-make");
    PaneRig& r = c.r;
    open_launcher(r);
    press_letter(r, input::scan::kN, "n");
    // THE LINE IS OPEN AND EMPTY: the `n` that opened it was the row's, and the host ate it.
    CHECK(launcher_text(r).find("new pane: \n") != std::string::npos);
    type_into(r, "MyPane");
    CHECK(launcher_text(r).find("new pane: MyPane") != std::string::npos);
    CHECK_FALSE(r.session().panels.maker.open()); // nothing is made while a name is typed
    r.key(input::scan::kReturn);
    // THE HOST MADE IT, AND THE LINE CLOSED ON THE ACCEPTANCE, under the host's own sentence.
    REQUIRE(r.session().panels.maker.open());
    CHECK(r.session().panels.maker.definition.name == "MyPane");
    CHECK(has_pane(r.session().setup.active, maker_pane_ref("MyPane")));
    const std::string shown = launcher_text(r);
    CHECK(shown.find("new pane:") == std::string::npos);
    CHECK(shown.find("Pane Creator: MyPane is on this layout") != std::string::npos);
}

TEST_CASE("WL-MAKER-11: a name the host refuses keeps the line and what was typed, with the "
          "refusal under it; Escape cancels and makes nothing") {
    CreatorRig c("creator-refuse");
    PaneRig& r = c.r;
    open_launcher(r);
    press_letter(r, input::scan::kN, "n");
    type_into(r, "My Pane");
    r.key(input::scan::kReturn);
    const std::string refused = launcher_text(r);
    CHECK(refused.find("new pane: My Pane") != std::string::npos); // still open, still holding it
    CHECK(refused.find("no spaces") != std::string::npos);        // in the host's own words
    CHECK_FALSE(r.session().panels.maker.open());
    // THE LINE IS CORRECTED IN PLACE, by its own keys, and asked again.
    for (int i = 0; i < 5; ++i) {
        r.key(input::scan::kBackspace);
    }
    type_into(r, "Pane");
    CHECK(launcher_text(r).find("new pane: MyPane") != std::string::npos);
    r.key(input::scan::kReturn);
    REQUIRE(r.session().panels.maker.open());
    CHECK(r.session().panels.maker.definition.name == "MyPane");
    // ESCAPE CANCELS A LINE, SAYS SO, AND ASKS NOTHING.
    const std::size_t rows = r.session().setup.active.panes.size();
    press_letter(r, input::scan::kN, "n");
    type_into(r, "Other");
    r.key(input::scan::kEscape);
    const std::string cancelled = launcher_text(r);
    CHECK(cancelled.find("new pane:") == std::string::npos);
    CHECK(cancelled.find("no pane was made") != std::string::npos);
    CHECK(r.session().panels.maker.definition.name == "MyPane");
    CHECK(r.session().setup.active.panes.size() == rows);
}

TEST_CASE("WL-MAKER-11: the shipped Pane Manager's `s` and `ctrl+d` save and put back through the "
          "host's door, and a dirty pane's refusal names those keys where they are") {
    CreatorRig c("creator-save");
    PaneRig& r = c.r;
    open_launcher(r);
    press_letter(r, input::scan::kN, "n");
    type_into(r, "MyPane");
    r.key(input::scan::kReturn);
    REQUIRE(r.session().panels.maker.dirty()); // never saved
    // SAVE: the host writes the file, and the launcher shows the host's sentence.
    press_letter(r, input::scan::kS, "s");
    CHECK_FALSE(r.session().panels.maker.dirty());
    CHECK(std::filesystem::exists(r.host.pane_path));
    CHECK(launcher_text(r).find("saved pane MyPane") != std::string::npos);
    // AN EDIT THROUGH THE INSPECTOR'S DOOR (Info's path) makes it dirty again...
    REQUIRE(hand_inspect(r, maker_pane_ref("MyPane")).accepted);
    REQUIRE(hand_commit(r, "Text", "changed", "INTERIOR").accepted);
    REQUIRE(r.session().panels.maker.dirty());
    // ...AND THE QUIT IT REFUSES NAMES THE LAUNCHER'S OWN KEYS, as that pane declared them. The
    // keys are put elsewhere first, as a press on the room puts them: a pane holding them takes
    // `q` as its own.
    r.session().panels.keyboard = kNoPaneKind;
    press_letter(r, input::scan::kQ, "q");
    CHECK_FALSE(r.host.quit);
    CHECK(r.session().notice.find("s in Pane Manager saves it, ^d discards them") !=
          std::string::npos);
    // PUT BACK: the definition is the file's again.
    open_launcher(r);
    r.key(input::scan::kD, input::mod::kCtrl);
    CHECK_FALSE(r.session().panels.maker.dirty());
    CHECK(r.session().panels.maker.definition.regions[0].text.empty());
    CHECK(launcher_text(r).find("back to what") != std::string::npos);
}

TEST_CASE("WL-MAKER-11: the name line pastes what the platform holds -- asked once, landing in the "
          "line that asked") {
    CreatorRig c("creator-paste");
    PaneRig& r = c.r;
    SkinSeat* skin = r.mount_skin_seat();
    skin->platform = "Pasted";
    open_launcher(r);
    press_letter(r, input::scan::kN, "n");
    type_into(r, "My");
    r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(skin->clipboard_reads == 1);
    CHECK(launcher_text(r).find("new pane: MyPasted") != std::string::npos);
    r.key(input::scan::kReturn);
    REQUIRE(r.session().panels.maker.open());
    CHECK(r.session().panels.maker.definition.name == "MyPasted");
}

TEST_CASE("a Pane Creator make and a paste in one poll: the host's make is said, the pasted text "
          "stays in the line that asked, and closing that line takes no pane back") {
    // MUTATION (P1): the paste numbered as the make's record, one counter for both as reviewed --
    // the make's answer is dropped unread: no sentence, though the host made Alpha.
    // MUTATION (P2): an accepted make closing its line while a paste is on its way into it -- the
    // line closes holding `Alpha`, and `Tail` lands nowhere.
    // MUTATION (P3): the cancel sentence forgetting what the line made -- Escape says no pane was
    // made, of a line whose make the host carried out.
    CreatorRig c("creator-make-paste");
    PaneRig& r = c.r;
    SkinSeat* skin = r.mount_skin_seat();
    skin->platform = "Tail";
    open_launcher(r);
    press_letter(r, input::scan::kN, "n");
    type_into(r, "Alpha");
    REQUIRE(launcher_text(r).find("new pane: Alpha\n") != std::string::npos);
    MakerTap tap(r.bus, r.workshop_id, r.bus.role_holder(kDesktopRole));
    // RETURN AND CTRL+V IN ONE POLL: the paste is asked for before the make is answered.
    queue_key(r, input::scan::kReturn);
    queue_key(r, input::scan::kV, input::mod::kCtrl);
    r.bus.drain_until_idle();
    // THE HOST MADE WHAT THE LINE ASKED FOR, ONCE...
    CHECK(tap.asked == std::vector<std::string>{"Alpha"});
    CHECK(tap.answered == 1);
    CHECK(skin->clipboard_reads == 1);
    REQUIRE(r.session().panels.maker.open());
    CHECK(r.session().panels.maker.definition.name == "Alpha");
    // ...THE PANE SAYS SO, AND THE TEXT PASTED AFTER RETURN IS STILL IN THE LINE.
    const std::string shown = launcher_text(r);
    CHECK(shown.find("Pane Creator: Alpha is on this layout") != std::string::npos);
    CHECK(shown.find("new pane: AlphaTail") != std::string::npos);
    // CLOSING THE LINE SAYS WHAT IT MADE, and nothing the host made is taken back.
    r.key(input::scan::kEscape);
    const std::string closed = launcher_text(r);
    CHECK(closed.find("new pane:") == std::string::npos);
    CHECK(closed.find("Alpha was already made") != std::string::npos);
    CHECK(closed.find("no pane was made") == std::string::npos);
    CHECK(r.session().panels.maker.definition.name == "Alpha");
    CHECK(has_pane(r.session().setup.active, maker_pane_ref("Alpha")));
}

TEST_CASE("a second Pane Creator make while the first is unanswered is not sent, text typed after "
          "Return outlives the answer, and a refusal keeps the line and all it holds") {
    // MUTATION (P4): an accepted make closing its line whatever the line now holds -- `Alpha2`, a
    // name nobody asked for, is cleared.
    // MUTATION (P5): a make sent while one is unanswered -- the second takes the first's record, the
    // first's answer is dropped, and the second's refusal is said over the pane the first made.
    CreatorRig c("creator-make-twice");
    PaneRig& r = c.r;
    open_launcher(r);
    press_letter(r, input::scan::kN, "n");
    type_into(r, "Alpha");
    MakerTap tap(r.bus, r.workshop_id, r.bus.role_holder(kDesktopRole));
    queue_key(r, input::scan::kReturn);
    queue_key(r, input::scan::kReturn);
    queue_text(r, "2");
    r.bus.drain_until_idle();
    // ONE MAKE LEFT THE PANE, and the host made that one.
    CHECK(tap.asked == std::vector<std::string>{"Alpha"});
    REQUIRE(r.session().panels.maker.open());
    CHECK(r.session().panels.maker.definition.name == "Alpha");
    // ITS ANSWER IS SAID, and the line keeps the text typed after Return.
    const std::string shown = launcher_text(r);
    CHECK(shown.find("Pane Creator: Alpha is on this layout") != std::string::npos);
    CHECK(shown.find("new pane: Alpha2") != std::string::npos);
    // THAT TEXT ASKED FOR NOW IS REFUSED -- Alpha is unsaved -- AND THE LINE KEEPS ALL OF IT.
    r.key(input::scan::kReturn);
    CHECK(tap.asked.size() == 2);
    const std::string refused = launcher_text(r);
    CHECK(refused.find("new pane: Alpha2") != std::string::npos);
    CHECK(refused.find("unsaved changes") != std::string::npos);
    CHECK(r.session().panels.maker.definition.name == "Alpha");
    // ...AND CLOSING IT SAYS WHAT IT MADE.
    r.key(input::scan::kEscape);
    CHECK(launcher_text(r).find("Alpha was already made") != std::string::npos);
    CHECK(has_pane(r.session().setup.active, maker_pane_ref("Alpha")));
}

TEST_CASE("a Pane Creator make and a cancel in one poll say the make was already asked for until its "
          "answer takes that sentence's place, and an answer closes no newer draft") {
    // MUTATION (P6): the cancel sentence ignoring a make still unanswered -- the rows Workshop
    // holds say no pane was made while the host is making it.
    // MUTATION (P7): an accepted make closing whichever line is open when it arrives -- the newer
    // draft, holding the same text, closes and loses it.
    {
        CreatorRig c("creator-make-cancel");
        PaneRig& r = c.r;
        open_launcher(r);
        press_letter(r, input::scan::kN, "n");
        type_into(r, "Alpha");
        MakerTap tap(r.bus, r.workshop_id, r.bus.role_holder(kDesktopRole));
        queue_key(r, input::scan::kReturn);
        queue_key(r, input::scan::kEscape);
        // TURN BY TURN: the rows Workshop holds show the line closed before the answer arrives.
        for (int turns = 0; turns < 16 && tap.answered == 0 &&
                            launcher_text(r).find("new pane:") != std::string::npos;
             ++turns) {
            (void)r.bus.pump_pending();
        }
        CHECK(tap.answered == 0);
        const std::string waiting = launcher_text(r);
        CHECK(waiting.find("new pane:") == std::string::npos);
        CHECK(waiting.find("Alpha was already asked for, and may still be made") !=
              std::string::npos);
        CHECK(waiting.find("no pane was made") == std::string::npos);
        r.bus.drain_until_idle();
        CHECK(tap.answered == 1);
        REQUIRE(r.session().panels.maker.open());
        CHECK(r.session().panels.maker.definition.name == "Alpha");
        const std::string answered = launcher_text(r);
        CHECK(answered.find("Pane Creator: Alpha is on this layout") != std::string::npos);
        CHECK(answered.find("no pane was made") == std::string::npos);
    }
    {
        // A MAKE, A CANCEL AND A NEW LINE IN ONE BURST, the new line typed into with the very name
        // the first one sent: the answer arrives while the newer draft is open, and is not its.
        CreatorRig c("creator-make-newer");
        PaneRig& r = c.r;
        open_launcher(r);
        press_letter(r, input::scan::kN, "n");
        type_into(r, "Alpha");
        MakerTap tap(r.bus, r.workshop_id, r.bus.role_holder(kDesktopRole));
        queue_as_workshop(r, kCreatorNameId);
        queue_as_workshop(r, kCreatorCancelId);
        queue_as_workshop(r, kCreatorNewId);
        queue_text(r, "Alpha");
        r.bus.drain_until_idle();
        CHECK(tap.asked == std::vector<std::string>{"Alpha"});
        CHECK(tap.answered == 1);
        CHECK(r.session().panels.maker.definition.name == "Alpha");
        const std::string shown = launcher_text(r);
        CHECK(shown.find("new pane: Alpha\n") != std::string::npos);
        CHECK(shown.find("Pane Creator: Alpha is on this layout") != std::string::npos);
    }
}

namespace {

std::string joined(const std::vector<std::string>& rows) {
    std::string all;
    for (const std::string& row : rows) {
        all += row + "\n";
    }
    return all;
}

} // namespace

TEST_CASE("a Pane Creator make Loom refuses at dispatch is released: the line and the text typed "
          "since stand, the reason is said, and once Workshop is back the next Return makes it") {
    // MUTATION (R1): the desktop deaf to Loom's refusal, as reviewed -- the refused make holds the
    // Creator's one slot, and the next Return says `make not sent`.
    // MUTATION (R2): a refusal closing the name line -- the text typed since goes with it.
    CreatorRig c("creator-make-refused");
    PaneRig& r = c.r;
    open_launcher(r);
    press_letter(r, input::scan::kN, "n");
    type_into(r, "Alpha");
    const loom::WeaveId desk = r.bus.role_holder(kDesktopRole);

    SUBCASE("the line that asked stands") {
        SendTap makes(r.bus, desk, MakerPaneRequested::zen_name);
        queue_key(r, input::scan::kReturn);
        refuse_next_make(r, 1);
        REQUIRE(makes.attempts.size() == 1);
        CHECK(makes.reasons == std::vector<std::string>{"TargetUnavailable"});
        CHECK(makes.delivered == 0);
        CHECK(makes.notices == 1);
        CHECK_FALSE(r.session().panels.maker.open());
        r.extent(150, 44); // a new room: the launcher says its rows to the revived host
        const std::string refused = launcher_text(r);
        CHECK(refused.find("make not delivered -- nothing changed (TargetUnavailable)") !=
              std::string::npos);
        CHECK(refused.find("new pane: Alpha\n") != std::string::npos);
        // ...THE LINE TAKES MORE TEXT, AND THE NEXT RETURN IS A FRESH MAKE, NOT ONE HELD BEHIND AN
        // ANSWER THAT CANNOT COME.
        type_into(r, "2");
        MakerTap tap(r.bus, r.workshop_id, desk);
        r.key(input::scan::kReturn);
        CHECK(tap.asked == std::vector<std::string>{"Alpha2"});
        REQUIRE(r.session().panels.maker.open());
        CHECK(r.session().panels.maker.definition.name == "Alpha2");
        const std::string made = launcher_text(r);
        CHECK(made.find("new pane:") == std::string::npos);
        CHECK(made.find("Pane Creator: Alpha2 is on this layout") != std::string::npos);
    }
    SUBCASE("a line closed while it waited") {
        // Return and Escape: the desktop hears the make and the cancel before the death, so the
        // cancel promised the make may still be made. Loom's word replaces that promise.
        SendTap makes(r.bus, desk, MakerPaneRequested::zen_name);
        queue_key(r, input::scan::kReturn);
        queue_key(r, input::scan::kEscape);
        refuse_next_make(r, 2);
        CHECK(makes.delivered == 0);
        CHECK(makes.notices == 1);
        r.extent(150, 44);
        const std::string refused = launcher_text(r);
        CHECK(refused.find("new pane:") == std::string::npos);
        CHECK(refused.find("make not delivered -- nothing changed (TargetUnavailable)") !=
              std::string::npos);
        CHECK(refused.find("may still be made") == std::string::npos);
        // ...AND CLOSING THE NEXT LINE CLAIMS NOTHING OUTSTANDING.
        press_letter(r, input::scan::kN, "n");
        type_into(r, "Beta");
        r.key(input::scan::kEscape);
        CHECK(launcher_text(r).find("no pane was made") != std::string::npos);
    }
}

TEST_CASE("a Pane Creator make queued to a doorless office and refused at dispatch is released by "
          "Loom's notice and tried afresh, while one delivered and never answered stays "
          "outstanding: no timeout, no retry, no guess") {
    // MUTATION (Q1): the ticket discarded -- a make nothing queued holds the slot for good, and the
    // next name is `make not sent`.
    CreatorRig c("creator-make-unqueued");
    PaneRig& r = c.r;
    open_launcher(r);
    press_letter(r, input::scan::kN, "n");
    type_into(r, "Alpha");
    const loom::WeaveId desk = r.bus.role_holder(kDesktopRole);
    // WORKSHOP'S WEAVE LEAVES THE BUS FOR AN INTERVAL, its session untouched; a stand-in holds its
    // office meanwhile and says Workshop's resolved name id to the desktop.
    //
    // The desktop DECLARES `MakerPaneRequested` and `ClipboardTextRequested` in its `Emit<...>`,
    // and since Loom's ABI v9 a declared shape is registered by its emitter at load, for as long
    // as it lives -- so with Workshop gone both shapes still resolve, the make and the paste are
    // QUEUED, refused at dispatch (a doorless office; an unheld skin office), and released by
    // Loom's own notice naming the attempt. The desktop's ticket-not-valid branch ("nothing was
    // queued") is no longer reachable through a shape it declares, and stays source-traced
    // (`ask_maker` in desktop-pane/pane.cpp).
    std::unique_ptr<loom::Weave> workshop = r.take_workshop_off();

    SUBCASE("refused at dispatch: released by Loom's notice, and made once the door is back") {
        REQUIRE(r.bus.resolve_schema(MakerPaneRequested::zen_name,
                                     MakerPaneRequested::zen_version) != nullptr);
        loom::WeaveId office_id{};
        DoorlessDesk* office = stand_in<DoorlessDesk>(r, office_id);
        SendTap makes(r.bus, desk, MakerPaneRequested::zen_name);
        stand_in_says(r, office_id);
        CHECK(makes.reasons == std::vector<std::string>{"NotAccepted"});
        CHECK(makes.delivered == 0);
        CHECK(makes.notices == 1); // queued, refused at dispatch, and the desktop was told
        const std::string first = joined(office->voice.rows);
        CHECK(first.find("make not delivered -- nothing changed (NotAccepted)") !=
              std::string::npos);
        CHECK(first.find("new pane: Alpha\n") != std::string::npos);
        // THE RECORD WAS RELEASED: the next name is attempted again, not held as the second of two.
        stand_in_says(r, office_id);
        CHECK(makes.reasons.size() == 2);
        CHECK(makes.notices == 2);
        CHECK(joined(office->voice.rows).find("not sent") == std::string::npos);
        // THE DOOR COMES BACK: the stand-in leaves, and the same Workshop weave holds the office.
        REQUIRE(r.bus.unregister_weave(office_id) != nullptr);
        r.put_workshop_back(std::move(workshop));
        r.extent(150, 44);
        CHECK(launcher_text(r).find("new pane: Alpha\n") != std::string::npos);
        MakerTap tap(r.bus, r.workshop_id, desk);
        r.key(input::scan::kReturn);
        CHECK(tap.asked == std::vector<std::string>{"Alpha"});
        CHECK(r.session().panels.maker.definition.name == "Alpha");
    }
    SUBCASE("a paste refused at dispatch is not on its way: the next make closes its line") {
        // MUTATION (QP): a paste left `awaiting` whatever Loom said of it -- the accepted make then
        // keeps open a line nothing will ever paste into. No skin office is held, so the paste
        // is queued to `zengine.skin` and refused at dispatch NoSuchTarget; the notice releases it.
        REQUIRE(r.bus.resolve_schema(surface::ClipboardTextRequested::zen_name,
                                     surface::ClipboardTextRequested::zen_version) != nullptr);
        loom::WeaveId office_id{};
        DoorlessDesk* office = stand_in<DoorlessDesk>(r, office_id);
        office->paste = true;
        SendTap pastes(r.bus, desk, surface::ClipboardTextRequested::zen_name);
        stand_in_says(r, office_id);
        CHECK(pastes.reasons == std::vector<std::string>{"NoSuchTarget"});
        CHECK(pastes.notices == 1);
        REQUIRE(r.bus.unregister_weave(office_id) != nullptr);
        r.put_workshop_back(std::move(workshop));
        r.extent(150, 44);
        MakerTap tap(r.bus, r.workshop_id, desk);
        r.key(input::scan::kReturn);
        CHECK(tap.asked == std::vector<std::string>{"Alpha"});
        REQUIRE(r.session().panels.maker.open());
        const std::string shown = launcher_text(r);
        CHECK(shown.find("Pane Creator: Alpha is on this layout") != std::string::npos);
        CHECK(shown.find("new pane:") == std::string::npos);
    }
    SUBCASE("delivered and never answered: outstanding, and nothing guesses its fate") {
        loom::WeaveId office_id{};
        SilentDesk* office = stand_in<SilentDesk>(r, office_id);
        SendTap makes(r.bus, desk, MakerPaneRequested::zen_name);
        stand_in_says(r, office_id);
        CHECK(office->received == 1);
        CHECK(makes.reasons.empty());
        stand_in_says(r, office_id);
        CHECK(office->received == 1); // the second is not sent
        CHECK(joined(office->voice.rows)
                  .find("make not sent -- an earlier ask is still unanswered") !=
              std::string::npos);
        // ...AND WORKSHOP COMING BACK ANSWERS NOTHING THE STAND-IN HEARD: the act stays outstanding.
        REQUIRE(r.bus.unregister_weave(office_id) != nullptr);
        r.put_workshop_back(std::move(workshop));
        r.extent(150, 44);
        MakerTap tap(r.bus, r.workshop_id, desk);
        r.key(input::scan::kReturn);
        CHECK(tap.asked.empty());
        CHECK_FALSE(r.session().panels.maker.open());
        CHECK(launcher_text(r).find("make not sent -- an earlier ask is still unanswered") !=
              std::string::npos);
    }
}

TEST_CASE("only Loom's own refusal notice releases the Pane Creator's act: a forgery naming it "
          "exactly settles nothing, and a refused paste releases the paste alone") {
    // MUTATION (F1): provenance not checked -- the forgery releases the make, its answer is then
    // dropped, and the name said behind it sends a second make.
    // MUTATION (F2): any notice releasing the make -- the refused paste's notice takes the make's
    // slot, and the make's answer is dropped.
    // MUTATION (F3): a refused paste left on its way -- the accepted make keeps a line open that
    // nothing will ever paste into.
    SUBCASE("a forgery naming the make exactly") {
        CreatorRig c("creator-forged-notice");
        PaneRig& r = c.r;
        open_launcher(r);
        press_letter(r, input::scan::kN, "n");
        type_into(r, "Alpha");
        const loom::WeaveId desk = r.bus.role_holder(kDesktopRole);
        auto held = std::make_unique<NoticeStranger>();
        NoticeStranger* stranger = held.get();
        loom::Grant grant;
        grant.allow_to_any(loom::DispatchRefused::zen_name, loom::DispatchRefused::zen_version);
        const loom::WeaveId stranger_id =
            r.bus.register_weave(std::move(held), std::move(grant));
        stranger->zen_set_self(stranger_id);
        std::vector<std::uint64_t> seqs;
        std::vector<std::uint64_t> correlations;
        int forged_delivered = 0;
        bool stopped = false;
        const loom::ObserverId tap = r.bus.add_observer([&](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered) {
                return;
            }
            if (ev.schema_name == MakerPaneRequested::zen_name) {
                seqs.push_back(ev.seq);
                correlations.push_back(ev.correlation);
            } else if (ev.target == desk && ev.schema_name == loom::DispatchRefused::zen_name &&
                       ev.sender == stranger_id) {
                ++forged_delivered;
            } else if (!stopped && ev.target == desk &&
                       ev.schema_name == PaneActionRequested::zen_name) {
                stopped = true;
                r.bus.stop();
            }
        });
        // THE TURN STOPS WHERE THE DESKTOP HAS HEARD RETURN: its make queued, then its rows.
        queue_key(r, input::scan::kReturn);
        for (int turns = 0; turns < 16 && !stopped; ++turns) {
            (void)r.bus.pump_pending();
        }
        REQUIRE(stopped);
        // ITS ATTEMPT IS TWO SEQUENCES BEFORE THE NEXT ONE -- read off a probe now, and checked
        // against the make's delivery afterwards.
        const loom::Ticket probe = r.bus.send(
            stranger_id,
            loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
        REQUIRE(probe.valid());
        loom::DispatchRefused forged;
        forged.attempt = std::to_string(probe.seq - 2);
        forged.role = kWorkshopProvider;
        forged.shape = MakerPaneRequested::zen_name;
        forged.version = MakerPaneRequested::zen_version;
        forged.reason = "TargetUnavailable";
        REQUIRE(r.bus
                    .send_as(stranger_id, desk,
                             loom::Message(loom::to_value(forged), stranger_id, loom::WeaveId{},
                                           1))
                    .valid());
        // ...AND THE NAME ID AGAIN, queued behind the forgery and ahead of the answer.
        queue_as_workshop(r, kCreatorNameId);
        r.bus.drain_until_idle();
        r.bus.remove_observer(tap);
        // THE FORGERY WAS DELIVERED, AND IT NAMED THE MAKE THE DOOR THEN RECEIVED, EXACTLY.
        CHECK(forged_delivered == 1);
        REQUIRE(seqs.size() == 1); // one make reached the door: the name said behind it was not sent
        CHECK(seqs.front() == probe.seq - 2);
        CHECK(correlations.front() == 1);
        // IT SETTLED NOTHING: the answer came, made the pane, and closed the unchanged line.
        REQUIRE(r.session().panels.maker.open());
        CHECK(r.session().panels.maker.definition.name == "Alpha");
        const std::string shown = launcher_text(r);
        CHECK(shown.find("not delivered") == std::string::npos);
        CHECK(shown.find("new pane:") == std::string::npos);
        CHECK(shown.find("Pane Creator: Alpha is on this layout") != std::string::npos);
    }
    SUBCASE("a refused paste and a make, in one poll") {
        CreatorRig c("creator-refused-paste");
        PaneRig& r = c.r;
        // THE SKIN'S OFFICE IS HELD BY A WEAVE THAT HAS DIED: a paste is queued, and Loom refuses it
        // at dispatch -- where a missing shape would have refused it before anything was queued.
        (void)r.mount_skin_seat();
        const loom::WeaveId skin = r.bus.role_holder(surface::kSkinRole);
        REQUIRE(skin.valid());
        r.bus.kill(skin);
        open_launcher(r);
        press_letter(r, input::scan::kN, "n");
        type_into(r, "Alpha");
        const loom::WeaveId desk = r.bus.role_holder(kDesktopRole);
        SendTap pastes(r.bus, desk, surface::ClipboardTextRequested::zen_name);
        MakerTap tap(r.bus, r.workshop_id, desk);
        queue_key(r, input::scan::kV, input::mod::kCtrl);
        queue_key(r, input::scan::kReturn);
        r.bus.drain_until_idle();
        // THE PASTE WAS REFUSED, AND LOOM SAID SO TO THE DESKTOP -- before the make was answered...
        CHECK(pastes.reasons == std::vector<std::string>{"TargetUnavailable"});
        CHECK(pastes.notices == 1);
        // ...AND THE MAKE WAS ITS OWN: delivered once, answered, and its unchanged line closed.
        CHECK(tap.asked == std::vector<std::string>{"Alpha"});
        CHECK(tap.answered == 1);
        REQUIRE(r.session().panels.maker.open());
        const std::string shown = launcher_text(r);
        CHECK(shown.find("Pane Creator: Alpha is on this layout") != std::string::npos);
        CHECK(shown.find("new pane:") == std::string::npos);
        CHECK(shown.find("not delivered") == std::string::npos);
    }
}

TEST_CASE("a Pane Creator the host's admission denies the maker door says so for every attempt, "
          "and each later act is attempted afresh rather than held behind the first") {
    // THE REVIEW'S OWN SCENARIO. An admission policy grants the desktop every sentence it says
    // except `MakerPaneRequested`, so each ask is refused `CapabilityDenied` before the handler.
    // MUTATION (R1) again: the refusal unheard -- the second Return and the save are `not sent`.
    TempDir files("creator-denied");
    PaneRig r;
    r.host.pane_path = files.file("pane.json");
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    r.kernel.admit_with([](const loom::AdmissionRequest&) {
        loom::Grant g;
        const auto allow = [&g](const char* name, std::uint32_t version) {
            g.allow_to_any(name, version);
        };
        allow(PaneOffered::zen_name, PaneOffered::zen_version);
        allow(PaneActions::zen_name, PaneActions::zen_version);
        allow(PaneContent::zen_name, PaneContent::zen_version);
        allow(AppActions::zen_name, AppActions::zen_version);
        allow(PaneLaunchRequested::zen_name, PaneLaunchRequested::zen_version);
        allow(PaneCloseRequested::zen_name, PaneCloseRequested::zen_version);
        allow(DeselectRequested::zen_name, DeselectRequested::zen_version);
        allow(DesktopFace::zen_name, DesktopFace::zen_version);
        allow(PaneInventoryRequested::zen_name, PaneInventoryRequested::zen_version);
        allow(KeymapRequested::zen_name, KeymapRequested::zen_version);
        allow(surface::ClipboardCopy::zen_name, surface::ClipboardCopy::zen_version);
        allow(surface::ClipboardTextRequested::zen_name,
              surface::ClipboardTextRequested::zen_version);
        // ...and the sentences the mouse-usable desktop says: numbered content, the toggle, an
        // edit, a menu, a pass-back, a subject to manage, an inspection.
        allow(v3::PaneContent::zen_name, v3::PaneContent::zen_version);
        allow(PaneToggleRequested::zen_name, PaneToggleRequested::zen_version);
        allow(KeymapEditRequested::zen_name, KeymapEditRequested::zen_version);
        allow(PaneMenuRequested::zen_name, PaneMenuRequested::zen_version);
        allow(PanePassRequested::zen_name, PanePassRequested::zen_version);
        allow(PaneManageRequested::zen_name, PaneManageRequested::zen_version);
        allow(InspectPaneRequested::zen_name, InspectPaneRequested::zen_version);
        return loom::AdmissionVerdict::admit(std::move(g), "every desktop sentence but the maker's");
    });
    load_real_desktop(r);
    open_launcher(r);
    press_letter(r, input::scan::kN, "n");
    type_into(r, "Alpha");
    const loom::WeaveId desk = r.bus.role_holder(kDesktopRole);
    SendTap makes(r.bus, desk, MakerPaneRequested::zen_name);

    r.key(input::scan::kReturn);
    CHECK(makes.reasons == std::vector<std::string>{"CapabilityDenied"});
    CHECK(makes.delivered == 0);
    CHECK(makes.notices == 1);
    CHECK_FALSE(r.session().panels.maker.open());
    std::string shown = launcher_text(r);
    CHECK(shown.find("make not delivered -- nothing changed (CapabilityDenied)") !=
          std::string::npos);
    CHECK(shown.find("new pane: Alpha\n") != std::string::npos);

    // A FRESH ATTEMPT, DENIED AGAIN -- and said as its own, not as one held behind the first.
    r.key(input::scan::kReturn);
    CHECK(makes.reasons.size() == 2);
    CHECK(makes.notices == 2);
    CHECK(makes.delivered == 0);
    CHECK(launcher_text(r).find("not sent") == std::string::npos);

    // CLOSING THE LINE IS TRUE -- nothing was made -- AND A SAVE IS ITS OWN ATTEMPT.
    r.key(input::scan::kEscape);
    CHECK(launcher_text(r).find("no pane was made") != std::string::npos);
    press_letter(r, input::scan::kS, "s");
    CHECK(makes.reasons.size() == 3);
    shown = launcher_text(r);
    CHECK(shown.find("save not delivered -- nothing changed (CapabilityDenied)") !=
          std::string::npos);
    CHECK(shown.find("not sent") == std::string::npos);
}

TEST_CASE("the shipped Pane Manager cuts a long pane name at its room and MARKS the cut") {
    // THE DEFECT THE FIRST REAL EXTERNAL TOOL FOUND, IN ITS SUCCESSOR. The picker padded names
    // into a ten-column column and cut them in silence, so `Loaded Weaves` read as `Loaded Wea`.
    // The desktop's list writes the name last on its row and fits the row with the cut marked.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(160, 48);
    load_real_desktop(r);
    const std::string long_name = "a-very-long-provider-pane-name-x";
    REQUIRE(long_name.size() == kMaxPaneNameLen);
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    r.drive(seat, [&long_name](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{kHelloPane, long_name, "a pane with a long name"});
    });
    open_launcher(r);
    // NARROWER THAN THE ROW, through the inspector's door as a maker would write it: thirty
    // cells of pane hold the marker, the state and less than the name.
    REQUIRE(hand_inspect(r, PaneRef{kDesktopRole, dp::kLauncherPane}).accepted);
    const PaneSubjectActed narrowed = hand_commit(r, "Width", "30");
    REQUIRE_MESSAGE(narrowed.accepted, narrowed.refusal);
    for (int i = 0; i < 8; ++i) {
        r.key(input::scan::kDown); // bring the last row into the list's window
    }
    const std::string shown = launcher_text(r);
    INFO(shown);
    CHECK(shown.find(long_name) == std::string::npos); // it did not fit...
    std::string row;
    std::size_t at = 0;
    while (at < shown.size()) {
        const std::size_t end = shown.find('\n', at);
        const std::string line = shown.substr(at, end - at);
        if (line.find("a-very-long") != std::string::npos) {
            row = line;
        }
        at = end == std::string::npos ? shown.size() : end + 1;
    }
    REQUIRE_FALSE(row.empty());
    const std::string mark = pane_text::kElided;
    CHECK(row.size() > mark.size());
    CHECK(row.compare(row.size() - mark.size(), mark.size(), mark) == 0); // ...and says so
}

namespace {

std::string floor_text(PaneRig& r) {
    std::string floor;
    for (const surface::SurfaceTextRow& row : r.session().backdrop) {
        floor += row.text + "\n";
    }
    return floor;
}

/// WHAT THE DESKTOP'S HOTKEYS PANE IS SHOWING, one row per line.
std::string hotkeys_pane_text(PaneRig& r) {
    const RuntimePane* row = r.session().panels.runtime.find(kDesktopRole, dp::kHotkeysPane);
    REQUIRE(row != nullptr);
    const ExternalPane* shown = r.session().panels.external_pane(row->kind);
    REQUIRE(shown != nullptr);
    CHECK(shown->refusal.empty());
    std::string text;
    for (const surface::SurfaceTextRow& line : shown->shown) {
        text += line.text + "\n";
    }
    return text;
}

} // namespace

TEST_CASE("the floor and the Hotkeys pane teach the application's keys as they are in force: a "
          "moved row where it moved, a disabled one as having no key") {
    // MUTATION (K1): `face()` printing its own declared defaults -- the floor says `ctrl+t` for a
    // row the maker moved to `ctrl+y`, the probe's exact finding.
    TempDir dir("desktop-effective-keys");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"desktop.terminal", "ctrl+g"},
                                                         {"desktop.panes", "none"}}));
    PaneRig r;
    r.host.keymap_path = path;
    r.mount_workshop();
    r.ready();
    r.extent(160, 60);
    load_real_desktop(r);
    const AppRow* moved = r.session().keymap.app_row_of_id("desktop.terminal");
    REQUIRE(moved != nullptr);
    REQUIRE(moved->gesture == Gesture{input::scan::kG, input::mod::kCtrl}); // dispatch uses it

    const std::string floor = floor_text(r);
    CAPTURE(floor);
    CHECK(floor.find("ctrl+g  terminal") != std::string::npos);
    CHECK(floor.find("ctrl+t") == std::string::npos);
    CHECK(floor.find("panes: no key") != std::string::npos);
    CHECK(floor.find("ctrl+k  hotkeys") != std::string::npos);

    // THE HOTKEYS PANE, LAUNCHED BY ITS OWN APPLICATION ROW, lists the same truth -- with the
    // maker's two authored rows marked, and where to move one. Made tall, as a maker would.
    r.key(input::scan::kK, input::mod::kCtrl);
    const Written tall = author_pane_size(r.session().setup.active,
                                          PaneRef{kDesktopRole, dp::kHotkeysPane}, PaneSize{},
                                          PaneSize{pane_unit::kSubcells, subs(40)});
    REQUIRE_MESSAGE(tall.accepted, tall.refusal);
    r.extent(200, 60);
    const std::string keys = hotkeys_pane_text(r);
    CAPTURE(keys);
    CHECK(keys.find("HOTKEYS -- ") != std::string::npos);
    // ...AS A TABLE: the key, the label, the id and the mark of each row in their columns.
    const auto row_with = [&keys](const std::vector<const char*>& cells) {
        std::size_t at = 0;
        while (at < keys.size()) {
            const std::size_t end = keys.find('\n', at);
            const std::string line = keys.substr(at, end == std::string::npos ? std::string::npos : end - at);
            bool all = true;
            for (const char* cell : cells) {
                all = all && line.find(cell) != std::string::npos;
            }
            if (all) {
                return true;
            }
            at = end == std::string::npos ? keys.size() : end + 1;
        }
        return false;
    };
    CHECK(row_with({"ctrl+g", "terminal", "desktop.terminal", "*"}));
    CHECK(row_with({"(no key)", "panes", "desktop.panes", "*"}));
    CHECK(keys.find("keymap file: " + path) != std::string::npos);
    CHECK(keys.find("applied -- 2 authored rows") != std::string::npos);
    CHECK(keys.find("\"none\" disables") != std::string::npos);

    // ...AND IT SCROLLS: the list is longer than its room, and every cut is counted.
    r.key(input::scan::kEnd);
    CHECK(hotkeys_pane_text(r).find("more above") != std::string::npos);
    r.key(input::scan::kHome);
    CHECK(hotkeys_pane_text(r).find("more below") != std::string::npos);
}

TEST_CASE("a keymap row written for an id whose owner changed is read as its successor, once, and "
          "the load says which rename to make") {
    // MUTATION (K2): dropping the renamed-id pass in `join_app_rows` -- `desktop.terminal` stays
    // on `ctrl+t` although the maker's file moved `workshop.terminal`.
    TempDir dir("desktop-renamed-ids");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"workshop.terminal", "ctrl+g"},
                                                         {"workshop.hotkeys", "ctrl+b"}}));
    PaneRig r;
    r.host.keymap_path = path;
    r.mount_workshop();
    r.ready();
    DesktopSeat* desk = mount_desktop(r);
    (void)desk;
    const AppRow* terminal = r.session().keymap.app_row_of_id("desktop.terminal");
    const AppRow* keys = r.session().keymap.app_row_of_id("desktop.hotkeys");
    REQUIRE(terminal != nullptr);
    REQUIRE(keys != nullptr);
    CHECK(terminal->gesture == Gesture{input::scan::kG, input::mod::kCtrl});
    CHECK(keys->gesture == Gesture{input::scan::kB, input::mod::kCtrl});
    // THE LOAD SAID IT, naming the rename that would make the file say what it means.
    CHECK(r.session().keymap.note.find("`workshop.terminal` is read as `desktop.terminal`") !=
          std::string::npos);
    CHECK(keymap_text(r.session()).find("ctrl+g | terminal | desktop.terminal *") !=
          std::string::npos);

    // ...AND A FILE THAT NAMES BOTH IS READ BY THE NEW ID: one row, one meaning.
    TempDir both_dir("desktop-renamed-both");
    const std::string both = both_dir.file("keymap.json");
    write_keymap_file(both, keymap_file_text("default", {{"workshop.terminal", "ctrl+g"},
                                                         {"desktop.terminal", "ctrl+b"}}));
    PaneRig b;
    b.host.keymap_path = both;
    b.mount_workshop();
    b.ready();
    (void)mount_desktop(b);
    const AppRow* chosen = b.session().keymap.app_row_of_id("desktop.terminal");
    REQUIRE(chosen != nullptr);
    CHECK(chosen->gesture == Gesture{input::scan::kB, input::mod::kCtrl});
}

TEST_CASE("an application row answered above every mode cannot take a bare printable or a chord "
          "the text box owns, whoever wrote it") {
    Keymap k;
    const Written bare = join_app_rows(
        k, std::vector<AppRow>{AppRow{"x.t", "t", Gesture{input::scan::kT, input::mod::kNone},
                                      app_precedence::kAboveModes}});
    CHECK_FALSE(bare.accepted);
    CHECK(bare.refusal.find("bare printable") != std::string::npos);
    const Written owned = join_app_rows(
        k, std::vector<AppRow>{AppRow{"x.v", "v", Gesture{input::scan::kV, input::mod::kCtrl},
                                      app_precedence::kAboveModes}});
    CHECK_FALSE(owned.accepted);
    CHECK(owned.refusal.find("editing vocabulary") != std::string::npos);
    // ...AND A DEFAULT-CLASS ROW MAY: it is asked only where nothing took the key.
    CHECK(join_app_rows(k, std::vector<AppRow>{AppRow{"x.esc", "esc",
                                                      Gesture{input::scan::kX, input::mod::kNone},
                                                      app_precedence::kDefault}})
              .accepted);
    // A MAKER'S FILE MOVING A LAUNCH ONTO ONE IS REFUSED THE SAME WAY, and told to the desktop.
    TempDir dir("desktop-bare-override");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"desktop.terminal", "t"}}));
    PaneRig r;
    r.host.keymap_path = path;
    r.mount_workshop();
    r.ready();
    DesktopSeat* desk = mount_desktop(r);
    REQUIRE_FALSE(desk->verdicts().empty());
    CHECK_FALSE(desk->verdicts().back().said.accepted);
    CHECK(desk->verdicts().back().said.refusal.find("bare printable") != std::string::npos);
    CHECK(r.session().keymap.app_row_of_id("desktop.terminal") == nullptr);
}
