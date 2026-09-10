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

#include "editor-pane/vocabulary.hpp"
#include "weavelib/legacy_pane_protocol.hpp"

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
/// it from the picker. Answers the pane's runtime handle.
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

/// The band's legend rows, padding trimmed -- the MSG-0 reading, in one place.
std::vector<std::string> band_lines(PaneRig& r) {
    std::vector<std::string> out;
    const Screen sc = screen_of(r.session());
    for (const std::int64_t y : {sc.help_y, sc.help_y + 1}) {
        const std::string row = inspector_row(r.last_canvas(), 0, y);
        if (!row.empty()) {
            out.push_back(row);
        }
    }
    return out;
}

std::string hotkeys_text(PaneRig& r) {
    std::string out;
    for (const HotkeyRow& row : hotkeys_rows(r.session())) {
        out += row.text;
        out += '\n';
    }
    return out;
}

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
        // A GLOBAL CHORD: refused, in the file's own words, naming both ids.
        const Gesture hotkeys = k.gesture_of(Act::kHotkeys);
        CHECK(refused({declared("x.k", "keys", hotkeys.scancode, hotkeys.modifiers)}) ==
              collision_sentence(hotkeys, "workshop.hotkeys", "x.k"));
        // ⭐ `document.save` (^s) IS REFUSED HERE AGAIN, AND ONE DECLARATION BUYS IT (VD-26).
        // The row is active while a pane holds the keys, so a pane taking its chord for an
        // unrelated operation really would be two meanings on one gesture -- unless the pane
        // says the row is standing in for it, which is one meaning in two scopes.
        const Gesture save = k.gesture_of(Act::kSaveDocument);
        CHECK(refused({declared("x.s", "save", save.scancode, save.modifiers)}) ==
              collision_sentence(save, "document.save", "x.s"));
        const Keymap with_save = accepted(
            {declared("x.s", "save", save.scancode, save.modifiers, kOwnableDocumentSave)});
        REQUIRE(with_save.pane_rows(kSomePane) != nullptr);
        CHECK(with_save.pane_supersedes(kSomePane, kOwnableDocumentSave));
        CHECK(with_save.above_mode_action(KeyContext::kPane, save.scancode, save.modifiers,
                                          kSomePane) == Act::kNone);
        // ...AND ANOTHER PANE'S KEYS ARE NOT THIS ONE'S DECLARATION.
        CHECK(with_save.above_mode_action(KeyContext::kPane, save.scancode, save.modifiers,
                                          kSomePane + 1) == Act::kSaveDocument);
        // AN ID WORKSHOP DOES NOT DECLARE, AND ONE A PANE MAY NOT OWN, ARE BOTH REFUSED.
        CHECK(refused({declared("x.z", "z", input::scan::kZ, input::mod::kCtrl, "no.such")})
                  .find("not one of Workshop's action ids") != std::string::npos);
        CHECK(refused({declared("x.o", "o", input::scan::kUnknown, input::mod::kNone,
                                "document.open")})
                  .find("not an action a pane may own") != std::string::npos);
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
        // AN OVERRIDE THAT LANDS ON A GLOBAL IS THE SAME COLLISION, said the same way.
        k.authored[0].gesture = "ctrl+k";
        CHECK(refused({declared("x.up", "row up", input::scan::kUp)}) ==
              collision_sentence(k.gesture_of(Act::kHotkeys), "workshop.hotkeys", "x.up"));
        // A GESTURE OUTSIDE THE GRAMMAR, and an id authored twice: `apply_overrides`' words.
        k.authored[0].gesture = "hyper+u";
        CHECK(refused({declared("x.up", "row up", input::scan::kUp)}).find("`x.up`: `hyper`") !=
              std::string::npos);
        k.authored[0].gesture = "ctrl+u";
        k.authored.push_back(AuthoredOverride{"x.up", "ctrl+j"});
        CHECK(refused({declared("x.up", "row up", input::scan::kUp)})
                  .find("`x.up` is authored twice") != std::string::npos);
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
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_declared(r, seat, kHelloOffice, kHelloPane,
                                                 {declared("hello.up", "row up", input::scan::kUp)});
    REQUIRE(r.session().keymap.pane_rows(kind) != nullptr);

    const Gesture hotkeys = r.session().keymap.gesture_of(Act::kHotkeys);
    redeclare(r, seat, kHelloPane,
              {declared("hello.up", "row up", input::scan::kUp),
               declared("hello.keys", "keys", hotkeys.scancode, hotkeys.modifiers)});
    // THE REFUSAL NAMES THE PANE AND BOTH ACTIONS, in the keymap file's own words.
    CHECK(r.last_notice() == "Seat @" + std::string(kHelloOffice) + ": " +
                                 collision_sentence(hotkeys, "workshop.hotkeys", "hello.keys"));
    // AND THE PREVIOUS ROWS STAND, on the map and on the catalog row alike.
    REQUIRE(r.session().keymap.pane_rows(kind) != nullptr);
    REQUIRE(r.session().keymap.pane_rows(kind)->rows.size() == 1);
    CHECK(r.session().keymap.pane_rows(kind)->rows[0].id == "hello.up");
    REQUIRE(retained(r, kind).size() == 1);
    CHECK(retained(r, kind)[0].id == "hello.up");
    // ...and the global still answers.
    press_body(r, kind);
    r.key(hotkeys.scancode, hotkeys.modifiers);
    CHECK(r.session().hotkeys.open);
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
    // AND THE ABOVE-MODE CHORDS STILL OUTRANK THE PANE: `^k` opens the hotkey view here.
    r.key(input::scan::kK, input::mod::kCtrl);
    CHECK(r.session().hotkeys.open);
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
    // The maker moved the hotkey view onto ctrl+u; the pane's default is ctrl+u too.
    write_keymap_file(path, keymap_file_text("default", {{"workshop.hotkeys", "ctrl+u"}}));
    const Gesture moved{input::scan::kU, input::mod::kCtrl};

    SUBCASE("the file first: the declaration is refused at its admission") {
        PaneRig r;
        r.host.keymap_path = path;
        r.mount_workshop();
        r.ready();
        ProviderSeat* seat = r.mount_provider(kHelloOffice);
        r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
            s.offer(m, PaneOffered{kHelloPane, "Seat", "a recording provider"});
            s.declare(m, actions_for(kHelloPane, {declared("hello.up", "row up", input::scan::kU,
                                                           input::mod::kCtrl)}));
        });
        // THE REFUSAL IS SAID AT THE DECLARATION, before any later gesture writes over
        // the one notice line.
        CHECK(r.last_notice() == "Seat @" + std::string(kHelloOffice) + ": " +
                                     collision_sentence(moved, "workshop.hotkeys", "hello.up"));
        const RuntimePane* row = r.session().panels.runtime.find(kHelloOffice, kHelloPane);
        REQUIRE(row != nullptr);
        const std::int64_t kind = row->kind;
        CHECK(r.session().keymap.pane_rows(kind) == nullptr);
        CHECK(retained(r, kind).empty());
        r.pick(PaneRef{kHelloOffice, kHelloPane});
        press_body(r, kind);
        r.key(input::scan::kU, input::mod::kCtrl);
        CHECK(r.session().hotkeys.open); // the maker's file is what is in force
        CHECK(seat->actions.empty());
    }
    SUBCASE("the pane first: the load re-joins, refuses that pane's rows, and says so once") {
        PaneRig r;
        r.host.keymap_path = path;
        r.mount_workshop();
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
        CHECK(retained(r, kind).size() == 1);                  // the declaration is kept
        CHECK(r.last_notice().find("1 override") != std::string::npos);
        CHECK(r.last_notice().find("Seat @" + std::string(kHelloOffice) + ": " +
                                   collision_sentence(moved, "workshop.hotkeys", "hello.up")) !=
              std::string::npos);
        r.pick(PaneRef{kHelloOffice, kHelloPane});
        press_body(r, kind);
        r.key(input::scan::kU, input::mod::kCtrl);
        CHECK(r.session().hotkeys.open);
        CHECK(seat->actions.empty());
    }
}

// ============================================================================
// The legend and the hotkey view
// ============================================================================

TEST_CASE("the band's legend and the hotkey view print the pane's rows while it holds the keys") {
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
        // THE PANE'S OWN ROWS FIRST, then the chorded survivors; an unbound row teaches
        // no key (WL-KEY-13).
        // (`^s save` is back in this row: `document.save` is requestable while a pane that
        // did not declare it owns the keys; VD-26.)
        CHECK(lines[1] == "up row up | m mark | ^s save | ^o open | ^k hotkeys");
    }
    // THE HOTKEY VIEW: the rows, then the ownership sentence for everything else.
    r.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(r.session().hotkeys.open);
    const std::string view = hotkeys_text(r);
    CHECK(view.find("pane Seat @" + std::string(kHelloOffice)) != std::string::npos);
    CHECK(view.find("row up") != std::string::npos);
    CHECK(view.find("mark") != std::string::npos);
    CHECK(view.find(detail::pad("unbound", 14) + "rename") != std::string::npos);
    CHECK(view.find("every other ordinary key and character goes to the pane") !=
          std::string::npos);
    CHECK(view.find("the provider's own") != std::string::npos);
    r.key(input::scan::kEscape);

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
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    press_body(r, kind);
    CHECK(band_lines(r).at(1) == "^s save | ^o open | ^k hotkeys"); // `^s` is back (VD-26)
    r.key(input::scan::kK, input::mod::kCtrl);
    const std::string view = hotkeys_text(r);
    CHECK(view.find("every ordinary key and character goes to the pane") != std::string::npos);
    CHECK(view.find("every other") == std::string::npos);
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

    // ...AND THE OLD PANE, WHICH OWNS NOTHING, LEAVES `^s` TO THE OBJECT DOCUMENT.
    const Gesture save = r.session().keymap.gesture_of(Act::kSaveDocument);
    const std::size_t before_save = old_pane->said.size();
    r.key(save.scancode, save.modifiers);
    CHECK(old_pane->said.size() == before_save);
    CHECK(r.session().keymap.above_mode_action(KeyContext::kPane, save.scancode, save.modifiers,
                                               kind) == Act::kSaveDocument);

    // BESIDE IT, A PANE THAT OWNS `document.save` -- the second version, on the same host and
    // in the same session. (This rig's screen holds one stack pane, so the old one steps out.)
    r.press_cell(0, screen_of(r.session()).h - 1); // the keys back to the desk, for the picker
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
    press_body(r, kind);
    r.key(input::scan::kM);
    REQUIRE(pane_rows(r, kind).size() >= 2);
    CHECK(pane_rows(r, kind)[1] == "acted 1: old.mark"); // the resolved id reached the image
    // ...AND `^s` THERE IS THE OBJECT DOCUMENT'S: the old pane owns nothing.
    const Gesture save = r.session().keymap.gesture_of(Act::kSaveDocument);
    r.key(save.scancode, save.modifiers);
    CHECK(pane_rows(r, kind)[1] == "acted 1: old.mark");
    CHECK(r.session().keymap.above_mode_action(KeyContext::kPane, save.scancode, save.modifiers,
                                               kind) == Act::kSaveDocument);
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
