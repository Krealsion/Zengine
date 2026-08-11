// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop suite — the property connection, the document's refusals, the
// selection mechanism, and one whole screen.
//
// Everything here is headless and pure. That is not a limitation of the suite,
// it is a property of the design: the authored objects are plain data, the
// operations on them are functions that can refuse, the inspector is a list of
// values, and the screen is a SurfaceCanvas returned by a function. Nothing in
// W-0's own logic needs a terminal, so nothing here has one.
//
// W-5 ADDED TWO MORE TIERS AND ONE OF THEM TOUCHES A DISK. Tier 5 is the
// document and its format — pure, and asserted mostly as bytes and values. Tier
// 6 drives save and load through the real weave on a real bus, because the
// interesting half of persistence is not the codec but what the SESSION does
// when the document under it is replaced. The cases that need a file use a
// temporary directory of their own and remove it; nothing here writes into the
// source tree, and no case shares a path with another.
//
// Three original tiers:
//   1. THE VOCABULARY — the document is ordinary content, and identity is not
//      the name.
//   2. THE PROPERTY CONNECTION — read through the semantic surface, commit
//      through it, and the two ways a commit can fail told apart. Includes the
//      reuse pin: two properties of one type share every line of conversion.
//   3. THE SCREEN — one canvas, asserted as a value: the rectangle, the ring,
//      the object list, the inspector, the authored-versus-resolved split, and a
//      refusal visible on it.
//
// AFTER W-1, THE GEOMETRY CLAIMS HERE ARE INTEGRATION CLAIMS. Resolution and hit
// testing belong to the UI package and are proven in the `ui` suite; what these
// cases prove is that Workshop's answers COME from there — that the painted
// rectangle, the inspector's resolved reading and the reply to a click are all
// derived from one ui::Scene. That is the property W-0 could only get by having
// one person write all three call sites.
//
// What headless cannot prove is that a Skin carries the canvas to a human's
// eyes. That is the Surface suite's job (a canvas is a frame: same hello, same
// counter) plus the live run in the report.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "workshop/document.hpp"
#include "workshop/persist.hpp"
#include "workshop/property.hpp"
#include "workshop/screen.hpp"
#include "workshop/weave.hpp"
#include "workshop/vocabulary.hpp"

#include "surface/skin_tui.hpp"
#include "surface/vocabulary.hpp"
#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace zengine::workshop;
using loom::schema_of;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ui = zengine::ui;

namespace {

/// Two rectangles that SHARE A NAME. The fixture is the point: if a name were
/// identity, this document could not exist.
WorkshopDoc two_panels() {
    WorkshopDoc d;
    doc::add(d, "panel", 3, 2, ui::Extent{ui::kExtentPercent, 60}, ui::Extent{ui::kExtentCells, 6});
    doc::add(d, "panel", 6, 10, ui::Extent{ui::kExtentCells, 14}, ui::Extent{ui::kExtentCells, 4});
    return d;
}

/// A document of `n` objects in creation order, identities 1..n.
///
/// The fixture no phase had, and the reason P26 lived through seven of them:
/// every live run and every screen case used two objects, and W-6's 300- and
/// 500-link documents were exercised headlessly, where nothing paints. More
/// objects than the OBJECTS panel is tall is a shape the suite could not
/// previously express.
WorkshopDoc many(std::int64_t n) {
    WorkshopDoc d;
    for (std::int64_t i = 0; i < n; ++i) {
        doc::add(d, "panel", 0, 0, ui::Extent{ui::kExtentCells, 2},
                 ui::Extent{ui::kExtentCells, 1});
    }
    return d;
}

/// Type the whole of `text` into a row, one character at a time -- the way a
/// maker's keystrokes actually arrive.
void type_all(Row& row, const std::string& text) {
    for (const char c : text) {
        row.type(c);
    }
}

/// Find a label's text at a canvas cell, or "" -- how the screen tier asks what
/// a maker would see at a place.
std::string label_at(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
    for (const surface::SurfaceLabel& l : c.labels) {
        if (l.x == x && l.y == y) {
            return l.text;
        }
    }
    return {};
}

/// The OBJECTS panel exactly as a maker reads it: every line of the list block,
/// in order, including the ones that are empty.
std::vector<std::string> object_lines(const surface::SurfaceCanvas& c) {
    std::vector<std::string> lines;
    for (std::int64_t i = 0; i < kListRows; ++i) {
        lines.push_back(label_at(c, kPanelX, kListY + i));
    }
    return lines;
}

bool has_rect(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y, std::int64_t w,
              std::int64_t h, std::int64_t role) {
    for (const surface::SurfaceRect& r : c.rects) {
        if (r.x == x && r.y == y && r.w == w && r.h == h && r.role == role) {
            return true;
        }
    }
    return false;
}

} // namespace

// ============================================================================
// Tier 1 — the authored vocabulary
// ============================================================================

TEST_CASE("contract: the authored shapes derive their declared spellings exactly") {
    using loom::Kind;
    using loom::SchemaBuilder;

    // The element and its extents are the UI package's shapes now (their own
    // contract case lives in the ui suite). What is asserted HERE is that
    // Workshop's document is still ordinary content built out of them -- the
    // relocation moved a vocabulary, not the weave state's nature.
    const auto extent = SchemaBuilder("Extent", 1)
                            .field("mode", Kind::Int)
                            .field("amount", Kind::Int)
                            .build();
    const auto element = SchemaBuilder("Element", 2)
                             .field("id", Kind::Int)
                             .field("label", Kind::Text)
                             .field("context", Kind::Int)
                             .field("x", Kind::Int)
                             .field("y", Kind::Int)
                             .message("width", extent)
                             .message("height", extent)
                             .build();

    // Version 2 since W-6, and the document's own two fields did not change:
    // what moved is the element inside it, and a content-id is over the WHOLE
    // shape. A `WorkshopDoc v1` that now serialises differently would be the
    // immutable-published-schema invariant broken quietly, so the version says
    // so out loud.
    const auto document = SchemaBuilder("WorkshopDoc", 2)
                              .list("elements", loom::type_message(element))
                              .field("next_id", Kind::Int)
                              .build();
    CHECK(schema_of<WorkshopDoc>()->content_id() == document->content_id());
}

TEST_CASE("identity is the id, not the name: two objects may be called the same thing") {
    WorkshopDoc d = two_panels();
    REQUIRE(d.elements.size() == 2);
    CHECK(d.elements[0].label == d.elements[1].label);
    CHECK(d.elements[0].id != d.elements[1].id);

    // Renaming does NOT refuse a duplicate -- refusing one would quietly make
    // the name an identifier, which is exactly the old builder's mistake.
    CHECK(doc::rename(d, d.elements[1].id, "panel").accepted);

    // And each is still separately reachable BY ID after the rename.
    const ui::Element* first = doc::find(d, d.elements[0].id);
    const ui::Element* second = doc::find(d, d.elements[1].id);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK(first->x == 3);
    CHECK(second->x == 6);

    // An id nothing carries is a normal answer, not an error: a selection can
    // outlive its object.
    CHECK(doc::find(d, 9999) == nullptr);
}

// ============================================================================
// Tier 2 — the typed property connection
// ============================================================================

TEST_CASE("a property reads the current typed value through the semantic surface") {
    WorkshopDoc d = two_panels();
    const std::int64_t id = d.elements[0].id;

    CHECK(doc::name_of(d, id).read() == "panel");
    CHECK(doc::x_of(d, id).read() == 3);
    CHECK(doc::width_of(d, id).read() == ui::Extent{ui::kExtentPercent, 60});

    // A property is a LIVE connection, not a snapshot: a change made anywhere
    // else is what the next read returns. This is why no Row caches a value and
    // why nothing in this package has a "refresh the inspector" call.
    REQUIRE(doc::rename(d, id, "renamed").accepted);
    CHECK(doc::name_of(d, id).read() == "renamed");
}

TEST_CASE("a successful commit writes through the semantic setter") {
    WorkshopDoc d = two_panels();
    const std::int64_t id = d.elements[0].id;

    Row row = Row::edit("Width", doc::width_of(d, id));
    row.begin();
    CHECK(row.editing());
    CHECK(row.draft() == "60%");

    // The draft alone changes nothing.
    type_all(row, "x");
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentPercent, 60});

    row.cancel();
    row.begin();
    for (int i = 0; i < 3; ++i) {
        row.backspace();
    }
    type_all(row, "24");
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentPercent, 60}); // still nothing

    CHECK(row.commit() == Commit::Accepted);
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentCells, 24});
    CHECK_FALSE(row.editing());
    CHECK(row.refusal().empty());
    CHECK(row.value() == "24");
}

TEST_CASE("an unparseable draft leaves the property untouched and says so") {
    WorkshopDoc d = two_panels();
    const std::int64_t id = d.elements[0].id;
    const ui::Extent before = d.elements[0].width;

    Row row = Row::edit("Width", doc::width_of(d, id));
    row.begin();
    for (int i = 0; i < 3; ++i) {
        row.backspace();
    }
    type_all(row, "banana");

    CHECK(row.commit() == Commit::Unparseable);
    CHECK(d.elements[0].width == before);       // the property never moved
    CHECK(row.editing());                    // still in the draft, so it can be fixed
    CHECK(row.draft() == "banana");           // and the draft was NOT thrown away
    CHECK_FALSE(row.refusal().empty());       // the refusal is observable
    CHECK(row.display() == "banana_");        // ...and marked as a draft, never as committed
    CHECK(row.value() == "60%");              // the committed value is still the real one
}

TEST_CASE("a parseable value the property refuses is a DIFFERENT outcome, with its reason") {
    WorkshopDoc d = two_panels();
    const std::int64_t id = d.elements[0].id;

    Row row = Row::edit("Width", doc::width_of(d, id));
    row.begin();
    for (int i = 0; i < 3; ++i) {
        row.backspace();
    }
    // `500%` IS an extent -- it parses. The setter is what says no, and a maker
    // needs to tell that from "not a width at all": one is fixed by retyping,
    // the other by wanting something else.
    type_all(row, "500%");
    CHECK(row.commit() == Commit::Refused);
    CHECK(row.refusal() == "a share is 1% to 100%");
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentPercent, 60});
    // The draft survives a REFUSAL too, not only an unparseable draft -- both
    // failures leave the maker looking at what they typed, so both are pinned.
    // (A mutation that cleared the draft here was green until this line.)
    CHECK(row.editing());
    CHECK(row.draft() == "500%");
    CHECK(row.display() == "500%_");
    CHECK(row.value() == "60%");

    // Zero cells is the other half of the same invariant, through the same row.
    row.cancel();
    row.begin();
    for (int i = 0; i < 3; ++i) {
        row.backspace();
    }
    type_all(row, "0");
    CHECK(row.commit() == Commit::Refused);
    CHECK(row.refusal() == "at least 1 cell");
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentPercent, 60});
}

TEST_CASE("an unparseable draft writes nothing even where a default WOULD be accepted") {
    // The sharp version of the previous case, and the one a mutation found
    // missing. On Width, a commit that wrongly wrote a default-constructed value
    // is INVISIBLE: the default extent is 0 cells, which set_width refuses
    // anyway, so the setter masks the bug. X has no such luck -- 0 is a
    // perfectly legal position -- so this is where "an unparseable draft does
    // not write" is actually observable.
    WorkshopDoc d = two_panels();
    const std::int64_t id = d.elements[0].id;
    REQUIRE(d.elements[0].x == 3);

    Row row = Row::edit("X", doc::x_of(d, id));
    row.begin();
    row.backspace();
    type_all(row, "banana");

    CHECK(row.commit() == Commit::Unparseable);
    CHECK(d.elements[0].x == 3); // not 0, and not anything else
    CHECK(row.draft() == "banana");
    CHECK(row.value() == "3");

    // And the whole-number form's own edges, on the same row.
    row.cancel();
    row.begin();
    row.backspace();
    type_all(row, "-1");
    CHECK(row.commit() == Commit::Refused); // parses; the setter refuses it
    CHECK(row.refusal() == "the workspace starts at 0");
    CHECK(d.elements[0].x == 3);

    row.cancel();
    row.begin();
    row.backspace();
    type_all(row, "0");
    CHECK(row.commit() == Commit::Accepted); // 0 IS a legal position
    CHECK(d.elements[0].x == 0);
}

TEST_CASE("cancel abandons the draft and never touched the property") {
    WorkshopDoc d = two_panels();
    const std::int64_t id = d.elements[0].id;

    Row row = Row::edit("Name", doc::name_of(d, id));
    row.begin();
    type_all(row, "zzz");
    row.cancel();

    CHECK_FALSE(row.editing());
    CHECK(row.display() == "panel");
    CHECK(d.elements[0].label == "panel");
}

TEST_CASE("the name property's own refusals: empty and too long") {
    WorkshopDoc d = two_panels();
    const std::int64_t id = d.elements[0].id;

    Row row = Row::edit("Name", doc::name_of(d, id));
    row.begin();
    for (int i = 0; i < 5; ++i) {
        row.backspace();
    }
    CHECK(row.commit() == Commit::Refused); // any text parses; empty is REFUSED
    CHECK(row.refusal() == "a name cannot be empty");
    CHECK(d.elements[0].label == "panel");

    row.cancel();
    row.begin();
    type_all(row, std::string(doc::kMaxNameLen, 'x'));
    CHECK(row.commit() == Commit::Refused);
    CHECK(d.elements[0].label == "panel");
}

TEST_CASE("reuse: two properties of one type share every line of conversion") {
    WorkshopDoc d = two_panels();
    const std::int64_t id = d.elements[0].id;

    // Width and Height are both extents. Building rows for them is one call
    // each, and NEITHER call names a parse, a format, or a refusal wording --
    // that is what TextForm<ui::Extent> already is. The old builder needed a
    // whole row implementation per property; this pins that it no longer does.
    Row width = Row::edit("Width", doc::width_of(d, id));
    Row height = Row::edit("Height", doc::height_of(d, id));

    // The same typed draft behaviour on both, including the percent spelling...
    for (Row* row : {&width, &height}) {
        row->begin();
        while (!row->draft().empty()) {
            row->backspace();
        }
        type_all(*row, "40%");
        CHECK(row->commit() == Commit::Accepted);
    }
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentPercent, 40});
    CHECK(d.elements[0].height == ui::Extent{ui::kExtentPercent, 40});

    // ...and the same refusal, in the same words, from the shared check.
    for (Row* row : {&width, &height}) {
        row->begin();
        while (!row->draft().empty()) {
            row->backspace();
        }
        type_all(*row, "101%");
        CHECK(row->commit() == Commit::Refused);
        CHECK(row->refusal() == "a share is 1% to 100%");
    }
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentPercent, 40});
    CHECK(d.elements[0].height == ui::Extent{ui::kExtentPercent, 40});
}

TEST_CASE("the extent text form: canonical out, and the typeable spelling in") {
    using Form = TextForm<ui::Extent>;
    CHECK(Form::format(ui::Extent{ui::kExtentCells, 12}) == "12");
    CHECK(Form::format(ui::Extent{ui::kExtentPercent, 70}) == "70%");

    // Both spellings parse to the SAME value -- and `p` is not a convenience:
    // `%` is Shift+5, and the input vocabulary has no modifiers, so the
    // canonical display form is a form no maker can currently type.
    CHECK(Form::parse("70%") == ui::Extent{ui::kExtentPercent, 70});
    CHECK(Form::parse("70p") == ui::Extent{ui::kExtentPercent, 70});
    CHECK(Form::parse("12") == ui::Extent{ui::kExtentCells, 12});
    CHECK(Form::parse("-3") == ui::Extent{ui::kExtentCells, -3}); // parses; the setter refuses

    CHECK_FALSE(Form::parse("").has_value());
    CHECK_FALSE(Form::parse("%").has_value());
    CHECK_FALSE(Form::parse("banana").has_value());
    CHECK_FALSE(Form::parse("12x").has_value());
    CHECK_FALSE(Form::parse("99999999999999999999").has_value()); // too big to be a number
}

TEST_CASE("a resolved row cannot be edited, because it has nothing to write to") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);
    REQUIRE(s.rows.size() == 8);

    // The identity row and the resolved row are `show`s; every property is an
    // `edit`. The structural answer to "never present a resolved value as though
    // it were the authored property": begin() on one does nothing at all.
    CHECK(s.rows[0].label() == "Identity");
    CHECK_FALSE(s.rows[0].editable());
    CHECK(s.rows[7].label() == "Resolved");
    CHECK_FALSE(s.rows[7].editable());
    for (std::size_t i = 1; i <= 5; ++i) {
        CHECK(s.rows[i].editable());
    }

    s.rows[7].begin();
    CHECK_FALSE(s.rows[7].editing());
}

// ============================================================================
// Tier 2b — authored intent versus resolved value, as a maker meets it
// ============================================================================
//
// WHAT resolution does is the ui suite's claim. What these cases pin is that
// Workshop's inspector reports the UI package's answer and never a second one.

TEST_CASE("authored and resolved are different facts, and only one of them moves") {
    WorkshopDoc d;
    const std::int64_t id =
        doc::add(d, "wide", 0, 0, ui::Extent{ui::kExtentPercent, 50}, ui::Extent{ui::kExtentCells, 4});

    Session s;
    s.selected = id;
    s.workspace_w = 48;
    refocus(d, s);
    CHECK(s.rows[5].value() == "50%");       // the authored property
    CHECK(s.rows[7].value() == "24 x 4 cells"); // what this workspace makes of it

    // The resolved row is the SCENE's reading, not a second calculation that
    // happens to agree. Ask the package directly and the numbers are the same
    // ones, because they are the same numbers.
    const ui::Scene scene = workspace_scene(d, s);
    REQUIRE(ui::placed_for(scene, id) != nullptr);
    CHECK(ui::placed_for(scene, id)->rect.w == 24);
    CHECK(ui::placed_for(scene, id)->rect.h == 4);

    // Narrow the workspace: the RESOLVED row changes, the AUTHORED row does not,
    // and no authored value was written. This is the whole distinction in four
    // lines, and it is the one the inspector must never collapse.
    s.workspace_w = 24;
    refocus(d, s);
    CHECK(s.rows[5].value() == "50%");
    CHECK(s.rows[7].value() == "12 x 4 cells");
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentPercent, 50});

    // A cells extent's two facts COINCIDE, which is not the same as being one
    // fact -- the inspector still reports them as two rows.
    CHECK(s.rows[6].value() == "4");
}

TEST_CASE("the painted rectangle IS the resolved rectangle, and the panel is not") {
    WorkshopDoc d;
    const std::int64_t id =
        doc::add(d, "wide", 3, 2, ui::Extent{ui::kExtentPercent, 50}, ui::Extent{ui::kExtentCells, 6});
    Session s;
    s.selected = id;
    s.workspace_w = 48;
    s.workspace_h = 16;
    refocus(d, s);

    // Every authored element on the canvas comes from the scene, offset by the
    // workspace's origin ON THE CANVAS and by nothing else. A rectangle a maker
    // can see is therefore one ui::hit can find, by construction rather than by
    // two functions agreeing.
    const ui::Scene wide = workspace_scene(d, s);
    REQUIRE(ui::placed_for(wide, id) != nullptr);
    const ui::Rect r = ui::placed_for(wide, id)->rect;
    CHECK(has_rect(paint(d, s), kWorkspaceX + r.x, kWorkspaceY + r.y, r.w, r.h,
                   surface::role::kFill));

    // Resize the workspace and the same derivation still holds -- the painted
    // rectangle followed the resolved one without anyone telling it to.
    s.workspace_w = 24;
    refocus(d, s);
    const ui::Scene narrow = workspace_scene(d, s);
    const ui::Rect r2 = ui::placed_for(narrow, id)->rect;
    CHECK(r2.w == 12);
    CHECK(has_rect(paint(d, s), kWorkspaceX + r2.x, kWorkspaceY + r2.y, r2.w, r2.h,
                   surface::role::kFill));

    // The workspace backdrop is NOT an authored element: it is a session fact
    // painted as furniture, and it must not appear in the scene.
    CHECK(narrow.items.size() == 1);
}

// ============================================================================
// Tier 2c — selection, against the real authored objects
// ============================================================================

TEST_CASE("a click selects the same authored object the maker can see") {
    WorkshopDoc d;
    const std::int64_t back = doc::add(d, "back", 0, 0, ui::Extent{ui::kExtentCells, 10},
                                       ui::Extent{ui::kExtentCells, 6});
    const std::int64_t front = doc::add(d, "front", 4, 2, ui::Extent{ui::kExtentCells, 4},
                                        ui::Extent{ui::kExtentCells, 2});
    Session s;
    s.selected = back;
    refocus(d, s);

    // Workshop asks the package, over the same scene it paints from, and gets
    // back the AUTHORED IDENTITY -- not a rectangle index, not a label.
    const ui::Scene scene = workspace_scene(d, s);
    REQUIRE(ui::hit(scene, 0, 0) != nullptr);
    CHECK(ui::hit(scene, 0, 0)->id == back);
    REQUIRE(ui::hit(scene, 5, 3) != nullptr);
    CHECK(ui::hit(scene, 5, 3)->id == front); // overlap: the last painted wins
    CHECK(ui::hit(scene, 10, 0) == nullptr);  // one cell past the edge is nothing

    // And what it selects with that identity is the object the whole screen
    // agrees about: the ring, the list marker and the inspector's Identity row.
    s.selected = ui::hit(scene, 5, 3)->id;
    refocus(d, s);
    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(label_at(c, kPanelX, kListY + 1) == "> #" + std::to_string(front) + " front");
    CHECK(s.rows[0].value() == "#" + std::to_string(front));
    const ui::Rect fr = ui::placed_for(scene, front)->rect;
    CHECK(has_rect(c, kWorkspaceX + fr.x - 1, kWorkspaceY + fr.y - 1, fr.w + 2, fr.h + 2,
                   surface::role::kAccent));
}

TEST_CASE("a share's hit area follows the workspace, because both read one scene") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "half", 0, 0, ui::Extent{ui::kExtentPercent, 50},
                                     ui::Extent{ui::kExtentCells, 2});
    Session s;
    s.selected = id;

    // There is no second copy of the geometry to fall out of step, because
    // Workshop keeps no copy at all.
    s.workspace_w = 48;
    REQUIRE(ui::hit(workspace_scene(d, s), 20, 0) != nullptr); // 50% of 48 == 24 cells
    CHECK(ui::hit(workspace_scene(d, s), 20, 0)->id == id);

    s.workspace_w = 24;
    CHECK(ui::hit(workspace_scene(d, s), 20, 0) == nullptr); // 50% of 24 == 12 cells
    REQUIRE(ui::hit(workspace_scene(d, s), 5, 0) != nullptr);
    CHECK(ui::hit(workspace_scene(d, s), 5, 0)->id == id);
}

// ============================================================================
// Tier 2d — the maker's own hands: create, move, delete  (W-2)
// ============================================================================
//
// Every case here drives the SAME functions workshop.cpp binds keys and pointer
// events to. Nothing in this tier reaches into the document behind a gesture's
// back, which is what makes it evidence about what a maker can do rather than
// about what the data permits.

TEST_CASE("creating mints a fresh identity, and the identity is not the label or the index") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);

    const std::int64_t made = create(d, s);
    REQUIRE(d.elements.size() == 3);

    // A NEW identity: not any existing one, and not derived from where it landed
    // in the vector (index 2) or from what it is called.
    CHECK(made != d.elements[0].id);
    CHECK(made != d.elements[1].id);
    CHECK(made != 2);
    CHECK(d.elements[2].id == made);

    // Duplicate labels remain legal, and the default IS a duplicate -- so a maker
    // meets "the name is not the identity" by making something, which is the
    // cheapest moment to learn it.
    CHECK(d.elements[2].label == doc::kNewLabel);
    CHECK(d.elements[0].label == d.elements[2].label);
    CHECK(doc::find(d, made) != nullptr);
    CHECK(doc::find(d, made) != doc::find(d, d.elements[0].id));

    // The new object is immediately the selected one, and the whole screen agrees
    // about it: canvas ring, list marker, inspector Identity row.
    CHECK(s.selected == made);
    REQUIRE_FALSE(s.rows.empty());
    CHECK(s.rows[0].value() == "#" + std::to_string(made));
    CHECK(s.cursor == first_editable(s.rows));

    const surface::SurfaceCanvas c = paint(d, s);
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, made);
    REQUIRE(placed != nullptr);
    CHECK(has_rect(c, kWorkspaceX + placed->rect.x, kWorkspaceY + placed->rect.y, placed->rect.w,
                   placed->rect.h, surface::role::kFill));
    CHECK(has_rect(c, kWorkspaceX + placed->rect.x - 1, kWorkspaceY + placed->rect.y - 1,
                   placed->rect.w + 2, placed->rect.h + 2, surface::role::kAccent));
    CHECK(label_at(c, kPanelX, kListY + 2) == "> #" + std::to_string(made) + " panel");
}

TEST_CASE("creation cannot author a state this document would refuse") {
    // The defaults are values somebody chose, and the document's own checks are
    // the only thing that can say whether they were chosen well. Without this,
    // `add` -- which deliberately cannot refuse -- would be the one door through
    // which an illegal object enters.
    WorkshopDoc d;
    Session s;
    const std::int64_t made = create(d, s);
    const ui::Element* e = doc::find(d, made);
    REQUIRE(e != nullptr);

    CHECK(doc::check_coord(e->x, e->context).accepted);
    CHECK(doc::check_coord(e->y, e->context).accepted);
    CHECK(doc::check_context(d.elements, e->id, e->context).accepted);
    CHECK(doc::check_extent(e->width).accepted);
    CHECK(doc::check_extent(e->height).accepted);
    CHECK(doc::rename(d, made, e->label).accepted);
}

TEST_CASE("an identity is never handed out twice, even after its object is deleted") {
    WorkshopDoc d;
    Session s;

    const std::int64_t first = create(d, s);
    REQUIRE(delete_selected(d, s).accepted);
    CHECK(d.elements.empty());

    const std::int64_t second = create(d, s);
    // The mint does not rewind. A notice, a selection, or a half-finished thought
    // that still says "#1" can therefore never come to mean a different object --
    // which is what an id being an IDENTITY rather than a slot number means.
    CHECK(second != first);
    CHECK(second > first);
    CHECK(s.selected == second);
}

TEST_CASE("delete removes exactly one identity, and the other duplicate label survives") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);
    const std::int64_t kept = d.elements[1].id;
    REQUIRE(d.elements[0].label == d.elements[1].label); // the fixture's whole point

    REQUIRE(delete_selected(d, s).accepted);
    REQUIRE(d.elements.size() == 1);
    REQUIRE(s.rows.size() == 8); // a selection survived, so the inspector has a subject
    CHECK(doc::find(d, kept) != nullptr);
    CHECK(doc::find(d, kept)->label == "panel"); // the twin is untouched
    CHECK(doc::find(d, 1) == nullptr);

    // No stale geometry, no stale list row, no stale inspector subject.
    CHECK(ui::placed_for(workspace_scene(d, s), 1) == nullptr);
    CHECK(workspace_scene(d, s).items.size() == 1);
    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(label_at(c, kPanelX, kListY) == "> #" + std::to_string(kept) + " panel");
    CHECK(label_at(c, kPanelX, kListY + 1).empty());
    CHECK(s.rows[0].value() == "#" + std::to_string(kept));
}

TEST_CASE("the post-delete selection rule: the one that took its place, then the last, then none") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);
    const std::int64_t third = create(d, s);
    REQUIRE(d.elements.size() == 3);
    const std::int64_t first = d.elements[0].id;
    const std::int64_t second = d.elements[1].id;

    // Deleting from the middle: the selection goes to whatever took that place.
    s.selected = second;
    refocus(d, s);
    REQUIRE(delete_selected(d, s).accepted);
    CHECK(s.selected == third);

    // Deleting the LAST one: there is no successor, so the selection falls back.
    REQUIRE(s.selected == d.elements.back().id);
    REQUIRE(delete_selected(d, s).accepted);
    CHECK(s.selected == first);

    // Deleting the only one: the selection becomes NONE, explicitly, and the
    // inspector has nothing to show rather than something stale to show.
    REQUIRE(delete_selected(d, s).accepted);
    CHECK(s.selected == 0);
    CHECK(s.rows.empty());
    CHECK(doc::find(d, s.selected) == nullptr);
}

TEST_CASE("deleting nothing is a refusal a maker can read, not a crash or a silence") {
    WorkshopDoc empty;
    Session s;
    refocus(empty, s);
    const Written on_empty = delete_selected(empty, s);
    CHECK_FALSE(on_empty.accepted);
    CHECK(on_empty.refusal == "no such object");
    CHECK(empty.elements.empty());
    CHECK(s.selected == 0);

    // A selection can outlive its object; deleting it again must not take a
    // second one with it.
    WorkshopDoc d = two_panels();
    Session stale;
    stale.selected = 9999;
    refocus(d, stale);
    CHECK_FALSE(delete_selected(d, stale).accepted);
    CHECK(d.elements.size() == 2);
    CHECK(stale.selected == 9999); // a refusal changes neither the document nor the session
}

TEST_CASE("create after empty: the document comes back, and the tool never left") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);

    while (!d.elements.empty()) {
        REQUIRE(delete_selected(d, s).accepted);
    }
    REQUIRE(s.selected == 0);

    const std::int64_t made = create(d, s);
    REQUIRE(d.elements.size() == 1);
    CHECK(s.selected == made);
    CHECK(doc::find(d, made) != nullptr);
    REQUIRE_FALSE(s.rows.empty());
    CHECK(s.rows[0].value() == "#" + std::to_string(made));

    // It resolves, it paints, and it can be hit -- a created-from-empty object is
    // not a lesser object.
    const ui::Scene scene = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(scene, made);
    REQUIRE(placed != nullptr);
    CHECK(ui::hit(scene, placed->rect.x, placed->rect.y)->id == made);
    CHECK(has_rect(paint(d, s), kWorkspaceX + placed->rect.x, kWorkspaceY + placed->rect.y,
                   placed->rect.w, placed->rect.h, surface::role::kFill));
}

TEST_CASE("a newly created object works with the existing property machinery, unchanged") {
    WorkshopDoc d;
    Session s;
    const std::int64_t made = create(d, s);

    // No per-object registration, no reflection, no inspector framework: the
    // rows for a created object are built by exactly the call that builds them
    // for a seeded one, and they write through exactly the same setters.
    REQUIRE(s.rows.size() == 8);
    CHECK(s.rows[1].label() == "Name");
    CHECK(s.rows[5].label() == "Width");

    Row& name = s.rows[1];
    name.begin();
    while (!name.draft().empty()) {
        name.backspace();
    }
    type_all(name, "mine");
    CHECK(name.commit() == Commit::Accepted);
    CHECK(doc::find(d, made)->label == "mine");

    Row& width = s.rows[5];
    width.begin();
    while (!width.draft().empty()) {
        width.backspace();
    }
    type_all(width, "70p");
    CHECK(width.commit() == Commit::Accepted);
    CHECK(doc::find(d, made)->width == ui::Extent{ui::kExtentPercent, 70});

    // And the refusals are the same refusals, in the same words.
    width.begin();
    while (!width.draft().empty()) {
        width.backspace();
    }
    type_all(width, "500p");
    CHECK(width.commit() == Commit::Refused);
    CHECK(width.refusal() == "a share is 1% to 100%");
}

TEST_CASE("a nudge authors placement, and every reading of the object follows") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "one", 3, 2, ui::Extent{ui::kExtentPercent, 50},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    s.workspace_w = 48;
    refocus(d, s);

    const ui::Scene before = workspace_scene(d, s);
    REQUIRE(ui::hit(before, 3, 2) != nullptr);
    CHECK(ui::hit(before, 3, 2)->id == id);

    REQUIRE(nudge(d, s, +1, 0).accepted());
    CHECK(doc::find(d, id)->x == 4);
    CHECK(doc::find(d, id)->y == 2); // the other coordinate did NOT move
    REQUIRE(nudge(d, s, 0, +1).accepted());
    CHECK(doc::find(d, id)->x == 4);
    CHECK(doc::find(d, id)->y == 3);
    CHECK(s.selected == id); // moving is not selecting

    // The authored EXTENT is untouched: a move moves, it does not resize.
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 50});

    // The resolved scene, the canvas, the inspector and the hit test all followed,
    // because all four are derived from the one authored value that changed.
    const ui::Scene after = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(after, id);
    REQUIRE(placed != nullptr);
    CHECK(placed->rect == ui::Rect{4, 3, 24, 4});
    CHECK(has_rect(paint(d, s), kWorkspaceX + 4, kWorkspaceY + 3, 24, 4, surface::role::kFill));
    refocus(d, s);
    REQUIRE(s.rows.size() == 8);
    CHECK(s.rows[3].value() == "4"); // X
    CHECK(s.rows[4].value() == "3"); // Y
    CHECK(s.rows[7].value() == "24 x 4 cells"); // resolved size unchanged by a move

    REQUIRE(ui::hit(after, 4, 3) != nullptr);
    CHECK(ui::hit(after, 4, 3)->id == id);
    // And the cell it used to occupy is no longer it -- the old position stopped
    // being true rather than merely stopping being painted.
    CHECK(ui::hit(after, 3, 2) == nullptr);
}

TEST_CASE("a move is ONE authored change: a refused move writes neither coordinate") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "one", 0, 5, ui::Extent{ui::kExtentCells, 4},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;

    // A diagonal gesture toward the top-left corner: y is legal, x is not. With
    // two independent setters this would slide the object DOWN the left edge and
    // report a refusal at the same time -- a refusal message beside a successful
    // write.
    const Written refused = doc::move(d, id, -1, 6);
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal == "the workspace starts at 0");
    CHECK(doc::find(d, id)->x == 0);
    CHECK(doc::find(d, id)->y == 5); // NOT 6

    // The same, with the illegal coordinate second.
    CHECK_FALSE(doc::move(d, id, 7, -1).accepted);
    CHECK(doc::find(d, id)->x == 0); // NOT 7
    CHECK(doc::find(d, id)->y == 5);

    // A nudge reaches the same operation, but it is a HAND, so W-3's boundary
    // policy applies before the proposal is made: it stops at the workspace edge
    // and SLIDES along it rather than refusing, and it says which wall it met.
    // The other coordinate still moves -- that is the whole difference a clamp
    // buys, and the reason P15 was worth answering.
    const Handled slid = nudge(d, s, -1, +1);
    CHECK(slid.accepted());
    CHECK(slid.clamped());
    CHECK(slid.boundary == kAtWorkspaceStart);
    CHECK(doc::find(d, id)->x == 0); // already at the edge; stayed there
    CHECK(doc::find(d, id)->y == 6); // and the legal half of the gesture happened

    // And a legal move still writes both at once.
    REQUIRE(doc::move(d, id, 0, 5).accepted);
    REQUIRE(doc::move(d, id, 7, 6).accepted);
    CHECK(doc::find(d, id)->x == 7);
    CHECK(doc::find(d, id)->y == 6);
}

TEST_CASE("the inspector and the maker's hand write through ONE position operation") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "one", 4, 4, ui::Extent{ui::kExtentCells, 4},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    refocus(d, s);
    REQUIRE(s.rows.size() == 8);

    // A typed X of -1, through the Row the inspector actually holds.
    Row& x_row = s.rows[3];
    x_row.begin();
    while (!x_row.draft().empty()) {
        x_row.backspace();
    }
    type_all(x_row, "-1");
    CHECK(x_row.commit() == Commit::Refused);
    const std::string typed_refusal = x_row.refusal();

    // The same illegal position, proposed to the document instead.
    const Written dragged = doc::move(d, id, -1, 4);
    const Handled nudged = nudge(d, s, -1, 0) /* 4 -> 3, legal */;
    CHECK(nudged.accepted());
    CHECK_FALSE(nudged.clamped());
    REQUIRE(doc::set_x(d, id, 4).accepted); // put it back

    // Identical wording, because it is not two rules that agree -- set_x IS
    // doc::move holding y still, so there is no second place a gesture could
    // acquire its own opinion about what a legal position is.
    CHECK(typed_refusal == dragged.refusal);
    CHECK(typed_refusal == doc::set_x(d, id, -1).refusal);
    CHECK(typed_refusal == doc::set_y(d, id, -1).refusal);
    CHECK(typed_refusal == "the workspace starts at 0");

    // set_x moves only x; set_y moves only y. Delegating to a two-coordinate
    // operation did not quietly make them move both.
    REQUIRE(doc::set_x(d, id, 9).accepted);
    CHECK(doc::find(d, id)->x == 9);
    CHECK(doc::find(d, id)->y == 4);
    REQUIRE(doc::set_y(d, id, 7).accepted);
    CHECK(doc::find(d, id)->x == 9);
    CHECK(doc::find(d, id)->y == 7);
}

TEST_CASE("a drag takes hold of what the maker can see, and the grabbed point follows") {
    WorkshopDoc d;
    const std::int64_t back = doc::add(d, "back", 0, 0, ui::Extent{ui::kExtentCells, 10},
                                       ui::Extent{ui::kExtentCells, 6});
    const std::int64_t front = doc::add(d, "front", 4, 2, ui::Extent{ui::kExtentCells, 4},
                                        ui::Extent{ui::kExtentCells, 2});
    Session s;
    s.selected = back;
    refocus(d, s);

    // Press inside the overlap: the drag takes the TOPMOST object, because it
    // asks the same ui::hit the click path asks, over the same painted scene.
    CHECK(begin_drag(d, s, 5, 3) == front);
    CHECK(s.drag.active);
    CHECK_FALSE(s.drag.resizing); // a press on a BODY moves it
    CHECK(s.drag.id == front);
    CHECK(s.drag.grab_dx == 1); // 5 - 4
    CHECK(s.drag.grab_dy == 1); // 3 - 2

    // Drag to a new cell. The grabbed point ends up under the pointer, which is
    // the whole behavioural claim of a drag, and it is checked against the
    // RESOLVED scene rather than against the arithmetic that produced it.
    REQUIRE(drag_to(d, s, 20, 9).accepted());
    CHECK(doc::find(d, front)->x == 19);
    CHECK(doc::find(d, front)->y == 8);
    // The scene is NAMED, and that is not style: `placed_for` answers with a
    // pointer INTO the scene, so binding it to a variable while the scene was a
    // temporary is a use-after-free the plain lane cannot see. It shipped in this
    // file's first draft and a sanitizer found it -- the same dangling-observation
    // hazard W-1 designed `Placed` to carry an id against, one phase later, in the
    // test that proves the design.
    const ui::Scene moved = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(moved, front);
    REQUIRE(placed != nullptr);
    CHECK(placed->rect.x + s.drag.grab_dx == 20);
    CHECK(placed->rect.y + s.drag.grab_dy == 9);
    REQUIRE(ui::hit(workspace_scene(d, s), 20, 9) != nullptr);
    CHECK(ui::hit(workspace_scene(d, s), 20, 9)->id == front);

    // The object that was NOT grabbed did not move, and the drag holds no
    // position of its own to have gone stale.
    CHECK(doc::find(d, back)->x == 0);
    CHECK(doc::find(d, back)->y == 0);

    end_drag(s);
    CHECK_FALSE(s.drag.active);
    CHECK_FALSE(drag_to(d, s, 30, 9).accepted()); // and nothing moves after release
    CHECK(doc::find(d, front)->x == 19);
}

TEST_CASE("a press on empty space grabs nothing, and a drag against the edge slides along it") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "one", 5, 5, ui::Extent{ui::kExtentCells, 4},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;

    CHECK(begin_drag(d, s, 40, 12) == 0);
    CHECK_FALSE(s.drag.active);
    CHECK_FALSE(drag_to(d, s, 41, 12).accepted());

    // Grab the top-left cell, then drag OFF the left edge of the workspace while
    // still moving down. W-2 refused this outright and left the object put, which
    // was truthful and brusque (P15). W-3's boundary policy stops the HAND at the
    // wall instead: the proposal is reduced to the first cell the workspace has,
    // the legal half of the gesture still happens, and the notice says which wall
    // was met -- so nothing was silently corrected, it was openly stopped.
    REQUIRE(begin_drag(d, s, 5, 5) == id);
    CHECK(s.drag.grab_dx == 0);
    const Handled slid = drag_to(d, s, -1, 7);
    CHECK(slid.accepted());
    CHECK(slid.clamped());
    CHECK(slid.boundary == kAtWorkspaceStart);
    CHECK(doc::find(d, id)->x == 0); // stopped at the edge, not left where it was
    CHECK(doc::find(d, id)->y == 7); // and it kept sliding down it

    // The TYPED path is untouched by that decision, and the two now differ in
    // exactly the way the policy says they should: a hand is stopped, a written
    // value is refused, and the document still holds the only opinion about what
    // is legal.
    const Written typed = doc::move(d, id, -1, 7);
    CHECK_FALSE(typed.accepted);
    CHECK(typed.refusal == "the workspace starts at 0");
    CHECK(doc::find(d, id)->x == 0);

    // The drag is still live afterwards, so the maker can keep going without
    // re-pressing.
    CHECK(s.drag.active);
    REQUIRE(drag_to(d, s, 2, 5).accepted());
    CHECK(doc::find(d, id)->x == 2);
}

TEST_CASE("a nudge survives an authored coordinate no setter would have produced") {
    // WorkshopDoc is ZEN_EXPOSE()d, so a poke writes x directly, past every
    // refusal in document.hpp. A nudge COMPUTES its proposal, so it meets values
    // a typed edit never could -- and the neighbour of the largest representable
    // cell is not representable. (The same widening W-1 found one layer down.)
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "poked", 0, 0, ui::Extent{ui::kExtentCells, 2},
                                     ui::Extent{ui::kExtentCells, 2});
    Session s;
    s.selected = id;

    doc::find_mut(d, id)->x = kMax; // the poke
    CHECK(nudge(d, s, +1, 0).accepted());
    CHECK(doc::find(d, id)->x == kMax); // saturated: it went nowhere, and legally
    CHECK(nudge(d, s, -1, 0).accepted());
    CHECK(doc::find(d, id)->x == kMax - 1);

    // The other end is only reachable by a poke at all, and it must not wrap
    // either -- a nudge up from the most negative cell would otherwise become the
    // most positive one. Under W-3's boundary policy the gesture then does what it
    // does at any wall: it stops at the first cell the workspace HAS, and says so.
    // A hand can therefore recover an object a poke put somewhere impossible,
    // which is the same rule as everywhere else rather than a special case.
    doc::find_mut(d, id)->y = (std::numeric_limits<std::int64_t>::min)();
    const Handled down = nudge(d, s, 0, -1);
    CHECK(down.accepted());
    CHECK(down.clamped());
    CHECK(doc::find(d, id)->y == doc::kFirstCell);
    // and the document itself still refuses the impossible value outright
    CHECK_FALSE(doc::move(d, id, 0, (std::numeric_limits<std::int64_t>::min)()).accepted);
}

TEST_CASE("a reported pointer position survives every integer the wire can carry") {
    // W-4 replaced the double coordinates with int64 cells, so the old hazard --
    // `static_cast<int64_t>` of a double that does not fit, which is UNDEFINED --
    // is gone at the type. The REMAINING hazard is the one the type change cannot
    // remove: the numbers still come from whichever weave holds the input role,
    // and `INT64_MIN - 3` is undefined behaviour produced by data.
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();

    CHECK(workspace_cell_x(12) == 12);
    CHECK(workspace_cell_y(kWorkspaceY) == 0);

    // The saturating ends, which is where a plain subtraction would be UB.
    CHECK(workspace_cell_y(kMin) == kMin);
    CHECK(workspace_cell_x(kMin) == kMin);
    CHECK(workspace_cell_y(kMax) == kMax - kWorkspaceY);

    // And a saturated position is still just a cell: it hits nothing, and it
    // cannot make the arithmetic downstream of it overflow either.
    WorkshopDoc d;
    doc::add(d, "one", 0, 0, ui::Extent{ui::kExtentCells, 4}, ui::Extent{ui::kExtentCells, 4});
    Session s;
    CHECK(begin_drag(d, s, workspace_cell_x(kMin), workspace_cell_y(kMin)) == 0);
    CHECK(begin_drag(d, s, workspace_cell_x(kMax), workspace_cell_y(kMax)) == 0);
    CHECK(take_hold(d, s, workspace_cell_x(kMin), workspace_cell_y(kMin)) == 0);
    CHECK(take_hold(d, s, workspace_cell_x(kMax), workspace_cell_y(kMax)) == 0);
}

TEST_CASE("the pointer lands where the Skin actually drew the workspace") {
    // Two translations, and only the second one is Workshop's (screen.hpp).
    // A terminal Skin starts the canvas at terminal row 3; the workspace is one
    // row into the canvas. So a pointer on terminal row 3 is workspace row -1 and
    // terminal row 4 is workspace row 0. An off-by-one here would select the
    // object above the one a maker clicked, which is the kind of wrong that looks
    // like a bug in the hit test.
    const auto term = [](std::int64_t x, std::int64_t y) {
        const PointedAt at = canvas_point_of(input::space::kCells, x, y);
        REQUIRE(at.understood);
        return std::pair<std::int64_t, std::int64_t>{workspace_cell_x(at.cell.x),
                                                     workspace_cell_y(at.cell.y)};
    };
    CHECK(term(0, surface::kTuiCanvasTopRow + kWorkspaceY).first == -kWorkspaceX);
    CHECK(term(0, surface::kTuiCanvasTopRow + kWorkspaceY).second == 0);
    CHECK(term(0, surface::kTuiCanvasTopRow).second == -kWorkspaceY);

    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "one", 3, 2, ui::Extent{ui::kExtentCells, 4},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    // The object's authored top-left (3,2) is terminal (3, 2+1+2) = (3, 5).
    CHECK(begin_drag(d, s, term(3, 5).first, term(3, 5).second) == id);
    CHECK(s.drag.grab_dx == 0);
    CHECK(s.drag.grab_dy == 0);
    end_drag(s);
    CHECK(begin_drag(d, s, term(3, 4).first, term(3, 4).second) == 0); // one row above
}

TEST_CASE("the SAME object is under the pointer whichever medium reported it") {
    // G-1's projection question, asserted as the only thing that actually
    // matters: a maker pointing at the middle of an object hits THAT object, and
    // which medium they were looking through is not the document's business.
    //
    // The two media disagree about every number on the way in -- the terminal
    // reports canvas cells offset by two rows, the window reports pixels at
    // kCanvasCellPx each with no offset -- and agree about the answer. That is
    // what makes `12 / 12 == 1` the wrong test and this the right one.
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "one", 3, 2, ui::Extent{ui::kExtentCells, 6},
                                     ui::Extent{ui::kExtentCells, 4});
    // The object's own middle, in workspace cells, then on the canvas.
    const std::int64_t wx = 3 + 6 / 2;
    const std::int64_t wy = 2 + 4 / 2;
    const std::int64_t cx = wx + kWorkspaceX;
    const std::int64_t cy = wy + kWorkspaceY;

    const auto hit_through = [&d](std::int64_t space, std::int64_t x, std::int64_t y) {
        const PointedAt at = canvas_point_of(space, x, y);
        REQUIRE(at.understood);
        Session s;
        return begin_drag(d, s, workspace_cell_x(at.cell.x), workspace_cell_y(at.cell.y));
    };

    // Terminal: canvas cells, two rows down.
    CHECK(hit_through(input::space::kCells, cx, cy + surface::kTuiCanvasTopRow) == id);
    // Window: the pixel at that cell's top-left, its middle, and its last pixel
    // are all the same cell and therefore the same object.
    const std::int64_t px = cx * surface::kCanvasCellPx;
    const std::int64_t py = cy * surface::kCanvasCellPx;
    CHECK(hit_through(input::space::kPixels, px, py) == id);
    CHECK(hit_through(input::space::kPixels, px + surface::kCanvasCellPx / 2,
                      py + surface::kCanvasCellPx / 2) == id);
    CHECK(hit_through(input::space::kPixels, px + surface::kCanvasCellPx - 1,
                      py + surface::kCanvasCellPx - 1) == id);
    // One pixel further is the NEXT cell, and at the object's right edge that is
    // off it. (The object spans cells 3..8; cell 9 is not the object.)
    const std::int64_t right_px = (3 + 6 + kWorkspaceX) * surface::kCanvasCellPx;
    CHECK(hit_through(input::space::kPixels, right_px, py) == 0);
    CHECK(hit_through(input::space::kPixels, right_px - 1, py) == id);

    // A space this application cannot place is not guessed at.
    CHECK_FALSE(canvas_point_of(input::space::kUnknown, px, py).understood);
    CHECK_FALSE(canvas_point_of(9999, px, py).understood);
}

TEST_CASE("canvas, object list and inspector agree after every gesture in a session") {
    // One maker's session, end to end, with the three views checked against each
    // other after each act. This is the case that would catch a gesture that
    // updated the document but left one of the three reading something else.
    WorkshopDoc d;
    Session s;
    refocus(d, s);

    const auto agree = [&](std::int64_t id, std::int64_t row) {
        const surface::SurfaceCanvas c = paint(d, s);
        const ui::Scene scene = workspace_scene(d, s);
        const ui::Placed* placed = ui::placed_for(scene, id);
        REQUIRE(placed != nullptr);
        // REQUIRE before indexing, and not CHECK: a mutation that loses the
        // selection would otherwise walk off an empty vector and take the whole
        // binary down, and a crashed run is not a red -- it is a run that
        // stopped reporting. (Found by the mutation harness doing exactly that.)
        REQUIRE(s.rows.size() == 8);
        // canvas: the fill and the ring are where the scene put it
        CHECK(has_rect(c, kWorkspaceX + placed->rect.x, kWorkspaceY + placed->rect.y,
                       placed->rect.w, placed->rect.h, surface::role::kFill));
        CHECK(has_rect(c, kWorkspaceX + placed->rect.x - 1, kWorkspaceY + placed->rect.y - 1,
                       placed->rect.w + 2, placed->rect.h + 2, surface::role::kAccent));
        // list: the marker is on this identity
        CHECK(label_at(c, kPanelX, kListY + row).rfind("> #" + std::to_string(id), 0) == 0);
        // inspector: the same identity, and the authored position it is drawn at
        CHECK(s.rows[0].value() == "#" + std::to_string(id));
        CHECK(s.rows[3].value() == std::to_string(placed->rect.x));
        CHECK(s.rows[4].value() == std::to_string(placed->rect.y));
        // and the hit test answers with it at its own top-left cell
        REQUIRE(ui::hit(scene, placed->rect.x, placed->rect.y) != nullptr);
        CHECK(ui::hit(scene, placed->rect.x, placed->rect.y)->id == id);
    };

    const std::int64_t a = create(d, s);
    agree(a, 0);

    REQUIRE(nudge(d, s, +2, +3).accepted()); // hjkl arrives as repeated single steps;
    REQUIRE(nudge(d, s, +1, 0).accepted());  // the operation is the same either way
    refocus(d, s);
    agree(a, 0);

    const std::int64_t b = create(d, s);
    refocus(d, s);
    agree(b, 1);

    REQUIRE(begin_drag(d, s, doc::find(d, b)->x, doc::find(d, b)->y) == b);
    REQUIRE(drag_to(d, s, 22, 11).accepted());
    end_drag(s);
    refocus(d, s);
    agree(b, 1);

    // ...and a resize, through the handle, in the same session: the three views
    // still agree afterwards, and the object is still the same identity.
    const Handle grip = size_handle(d, s);
    REQUIRE(grip.shown);
    REQUIRE(take_hold(d, s, grip.x, grip.y) == b);
    REQUIRE(s.drag.resizing);
    REQUIRE(drag_to(d, s, 30, 16).accepted());
    end_drag(s);
    refocus(d, s);
    agree(b, 1);

    REQUIRE(delete_selected(d, s).accepted);
    CHECK(s.selected == a);
    refocus(d, s);
    agree(a, 0);
}

// ============================================================================
// Tier 2e — the maker's hand on an EXTENT: resize  (W-3)
// ============================================================================
//
// The tier the phase exists for. A move could be dragged because authored and
// resolved placement are the same number; an EXTENT is interpreted, so a hand on
// an edge has to decide what to author. Every case here drives the same
// functions workshop.cpp binds the handle and the four resize keys to.

TEST_CASE("resizing a cells extent authors cells, and every reading of the object follows") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "box", 3, 2, ui::Extent{ui::kExtentCells, 12},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    s.workspace_w = 48;
    s.workspace_h = 16;
    refocus(d, s);

    // The handle is the cell just past the object's bottom-right corner, so a
    // pointer resting ON it proposes the size the object already has.
    const Handle grip = size_handle(d, s);
    REQUIRE(grip.shown);
    CHECK(grip.id == id);
    CHECK(grip.x == 3 + 12);
    CHECK(grip.y == 2 + 4);
    REQUIRE(take_hold(d, s, grip.x, grip.y) == id);
    REQUIRE(s.drag.resizing);
    REQUIRE(drag_to(d, s, grip.x, grip.y).accepted());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, 12}); // unchanged
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 4});

    // Pull it five cells right and two down. 12 -> 17, exactly the phase prompt's
    // arithmetic, and the AUTHORED value is what changed.
    REQUIRE(drag_to(d, s, grip.x + 5, grip.y + 2).accepted());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, 17});
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 6});

    // Identity, label and POSITION are untouched: a resize resizes.
    CHECK(doc::find(d, id)->id == id);
    CHECK(doc::find(d, id)->label == "box");
    CHECK(doc::find(d, id)->x == 3);
    CHECK(doc::find(d, id)->y == 2);
    CHECK(s.selected == id);
    CHECK(d.elements.size() == 1);

    // The resolved rect, the canvas, the hit region and the inspector all follow,
    // because all four are derived from the one authored value that changed.
    const ui::Scene after = workspace_scene(d, s);
    const ui::Placed* placed = ui::placed_for(after, id);
    REQUIRE(placed != nullptr);
    CHECK(placed->rect == ui::Rect{3, 2, 17, 6});
    CHECK(has_rect(paint(d, s), kWorkspaceX + 3, kWorkspaceY + 2, 17, 6, surface::role::kFill));
    REQUIRE(ui::hit(after, 3 + 16, 2 + 5) != nullptr); // the newly occupied corner
    CHECK(ui::hit(after, 3 + 16, 2 + 5)->id == id);
    CHECK(ui::hit(after, 3 + 17, 2) == nullptr); // one past it is still nothing
    refocus(d, s);
    REQUIRE(s.rows.size() == 8);
    CHECK(s.rows[5].value() == "17");           // authored Width
    CHECK(s.rows[6].value() == "6");            // authored Height
    CHECK(s.rows[7].value() == "17 x 6 cells"); // and what the workspace makes of it

    // Shrinking is the same gesture backwards, and the cell it no longer occupies
    // STOPPED BEING TRUE rather than merely stopping being painted.
    REQUIRE(drag_to(d, s, 3 + 5, 2 + 2).accepted());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, 5});
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 2});
    CHECK(ui::hit(workspace_scene(d, s), 3 + 16, 2 + 5) == nullptr);
    end_drag(s);
}

TEST_CASE("resizing a share authors a SHARE, and the number is the smallest one that fits") {
    // The phase's central experiment, with the phase prompt's own numbers.
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "wide", 0, 0, ui::Extent{ui::kExtentPercent, 60},
                                     ui::Extent{ui::kExtentCells, 6});
    Session s;
    s.selected = id;
    s.workspace_w = 48;
    s.workspace_h = 16;
    REQUIRE(ui::placed_for(workspace_scene(d, s), id)->rect.w == 28); // 60% of 48

    // Pull the edge out to 34 resolved cells. The maker manipulated a property
    // they authored as a SHARE, so a share is what gets written -- and the number
    // is 71%, which is the smallest share this workspace resolves to 34 cells.
    REQUIRE(size_to(d, s, id, 34, 6).accepted());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 71});
    CHECK(ui::placed_for(workspace_scene(d, s), id)->rect.w == 34); // it means what it says
    CHECK(ui::resolve_extent(ui::Extent{ui::kExtentPercent, 70}, 48) == 33); // and 70% would not

    // The height was authored in CELLS and stays cells, in the same operation.
    // Two extents, two modes, one gesture: mode is per-property, not per-object.
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 6});

    // A share is never converted to cells behind the maker's back, at any size.
    for (const std::int64_t want : {1, 5, 24, 47, 48}) {
        REQUIRE(size_to(d, s, id, want, 6).accepted());
        CHECK(doc::find(d, id)->width.mode == ui::kExtentPercent);
        CHECK(ui::placed_for(workspace_scene(d, s), id)->rect.w == want);
    }
}

TEST_CASE("the projection is stable: the same resolved size always names the same share") {
    // §11's property, and the reason the rounding rule is not a taste. The
    // resolver FLOORS, so the projection must take the SMALLEST share that
    // reaches the asked-for size. `nearest` would send 28 cells to 58%, which
    // resolves to 27 -- so merely grabbing an edge and letting go would shrink the
    // object, and repeated resizes to the same place would walk the number down.
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "wide", 0, 0, ui::Extent{ui::kExtentPercent, 60},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    s.workspace_w = 48;

    // Asking for the size it already has re-authors NOTHING: the share it carries
    // already says exactly that, and rewriting it to the canonical spelling would
    // change a maker's number for no reason a maker could see.
    REQUIRE(size_to(d, s, id, 28, 4).accepted());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 60});

    // Resize away and back: the same PICTURE, and — because 60% was never the
    // canonical name for 28 cells — a different NUMBER. That is a true fact about
    // shares (58%, 59% and 60% are all 28 cells at this workspace), not a defect,
    // and the projection is honest that it authors a NEW value rather than
    // reconstructing the old one.
    REQUIRE(size_to(d, s, id, 34, 4).accepted());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 71});
    REQUIRE(size_to(d, s, id, 28, 4).accepted());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 59});
    CHECK(ui::placed_for(workspace_scene(d, s), id)->rect.w == 28); // the picture came back

    // And from there it does not move again, however many times the maker asks
    // for the same place. One resolved size, one authored share, forever.
    for (int i = 0; i < 5; ++i) {
        REQUIRE(size_to(d, s, id, 28, 4).accepted());
        CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 59});
    }

    // Every reachable size round-trips EXACTLY, at every workspace this tool has:
    // a percent is finer than a cell below 100 cells of span, so the ceil rule is
    // not merely stable but exact. Asked of the resolver, so the two cannot drift.
    for (std::int64_t span = kWorkspaceMinW; span <= kWorkspaceW; ++span) {
        s.workspace_w = span;
        for (std::int64_t want = 1; want <= span; ++want) {
            std::string boundary;
            const ui::Extent got =
                extent_from_drag(ui::Extent{ui::kExtentPercent, 50}, want, span, boundary);
            REQUIRE(got.mode == ui::kExtentPercent);
            CHECK(ui::resolve_extent(got, span) == want);
            CHECK(doc::check_extent(got).accepted);
        }
    }
}

TEST_CASE("a resize is ONE authored change: a refused proposal writes neither extent") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "box", 0, 0, ui::Extent{ui::kExtentCells, 10},
                                     ui::Extent{ui::kExtentCells, 5});

    // A diagonal proposal whose width is legal and whose height is not. With two
    // independent setters this would narrow the object AND report a refusal -- a
    // refusal message beside a successful write, which is exactly the bug W-2
    // removed from placement, at the other property.
    const Written refused = doc::resize(d, id, ui::Extent{ui::kExtentCells, 7},
                                        ui::Extent{ui::kExtentCells, 0});
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal == "at least 1 cell");
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, 10}); // NOT 7
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 5});

    // The same, with the illegal extent first.
    CHECK_FALSE(doc::resize(d, id, ui::Extent{ui::kExtentPercent, 500},
                            ui::Extent{ui::kExtentCells, 9})
                    .accepted);
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, 10});
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 5}); // NOT 9

    // And a legal resize still writes both at once.
    REQUIRE(doc::resize(d, id, ui::Extent{ui::kExtentCells, 7}, ui::Extent{ui::kExtentCells, 9})
                .accepted);
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, 7});
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 9});
}

TEST_CASE("the inspector and the maker's hand write through ONE size operation") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "box", 0, 0, ui::Extent{ui::kExtentCells, 10},
                                     ui::Extent{ui::kExtentCells, 5});
    Session s;
    s.selected = id;
    refocus(d, s);
    REQUIRE(s.rows.size() == 8);

    // A typed Width of 0, through the Row the inspector actually holds.
    Row& width = s.rows[5];
    width.begin();
    while (!width.draft().empty()) {
        width.backspace();
    }
    type_all(width, "0");
    CHECK(width.commit() == Commit::Refused);
    const std::string typed_refusal = width.refusal();

    // Identical wording from every door, because it is not several rules that
    // agree -- set_width IS doc::resize holding the height still, so there is no
    // second place a gesture could acquire its own opinion about a legal extent.
    CHECK(typed_refusal == "at least 1 cell");
    CHECK(typed_refusal ==
          doc::set_width(d, id, ui::Extent{ui::kExtentCells, 0}).refusal);
    CHECK(typed_refusal ==
          doc::set_height(d, id, ui::Extent{ui::kExtentCells, 0}).refusal);
    CHECK(typed_refusal == doc::resize(d, id, ui::Extent{ui::kExtentCells, 0},
                                       ui::Extent{ui::kExtentCells, 5})
                               .refusal);

    // set_width writes only the width; set_height only the height. Delegating to
    // a two-extent operation did not quietly make them write both.
    REQUIRE(doc::set_width(d, id, ui::Extent{ui::kExtentPercent, 40}).accepted);
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 40});
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 5});
    REQUIRE(doc::set_height(d, id, ui::Extent{ui::kExtentCells, 8}).accepted);
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 40});
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 8});

    // And a value the hand authors is a value the inspector will accept back:
    // the gesture never produces an extent the document's own check refuses.
    REQUIRE(size_to(d, s, id, 34, 6).accepted());
    CHECK(doc::check_extent(doc::find(d, id)->width).accepted);
    CHECK(doc::check_extent(doc::find(d, id)->height).accepted);
    refocus(d, s);
    CHECK(s.rows[5].value() == TextForm<ui::Extent>::format(doc::find(d, id)->width));
}

TEST_CASE("the authored minimum is the DOCUMENT's, not the resolution floor") {
    // §12. `ui::kMinCells` is what resolution refuses to round a share BELOW; it
    // is not the editor's authoring rule, and the two are different questions
    // that happen to agree at one end.
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "box", 0, 0, ui::Extent{ui::kExtentCells, 6},
                                     ui::Extent{ui::kExtentCells, 6});
    Session s;
    s.selected = id;
    s.workspace_w = 48;
    s.workspace_h = 16;

    // What the document permits a maker to author, by its own check and nothing
    // else: no zero-cell extent, no zero-percent share.
    CHECK_FALSE(doc::check_extent(ui::Extent{ui::kExtentCells, 0}).accepted);
    CHECK_FALSE(doc::check_extent(ui::Extent{ui::kExtentPercent, 0}).accepted);
    CHECK(doc::check_extent(ui::Extent{ui::kExtentCells, ui::kMinCells}).accepted);
    CHECK(doc::check_extent(ui::Extent{ui::kExtentPercent, 1}).accepted);

    // A hand that asks for less stops at that minimum -- the SAME minimum, not a
    // handle-specific one -- and says so.
    Handled done = size_to(d, s, id, -40, 0);
    CHECK(done.accepted());
    CHECK(done.boundary == kAtSmallest);
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, ui::kMinCells});
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, ui::kMinCells});
    CHECK(doc::check_extent(doc::find(d, id)->width).accepted);

    // The two floors are different rules, and here they visibly disagree: 1% of a
    // 12-cell workspace is 0 cells by arithmetic, and resolution floors it to
    // kMinCells so a maker never loses an object they authored. The AUTHORING
    // rule never sees that number at all.
    s.workspace_w = kWorkspaceMinW;
    CHECK(ui::resolve_extent(ui::Extent{ui::kExtentPercent, 1}, kWorkspaceMinW) == ui::kMinCells);
    CHECK(doc::check_extent(ui::Extent{ui::kExtentPercent, 1}).accepted);
    REQUIRE(doc::set_width(d, id, ui::Extent{ui::kExtentPercent, 1}).accepted);
    done = size_to(d, s, id, 0, 1);
    CHECK(done.accepted());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 1}); // still the smallest share
}

TEST_CASE("a hand STOPS at a boundary and a written value is REFUSED, and they are told apart") {
    // §15, and the answer to W-2's P15. The distinction is not decoration: after a
    // clamp something WAS written (the boundary value), after a refusal nothing
    // was -- so a maker who cannot tell them apart cannot tell what their document
    // now says.
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "wide", 0, 0, ui::Extent{ui::kExtentPercent, 50},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    s.workspace_w = 48;
    s.workspace_h = 16;

    // PHYSICAL OVERSHOOT, on a share: the far wall is 100%, and it is the
    // VOCABULARY's wall rather than the workspace being a barrier -- a share OF
    // the viewport cannot be more than the whole of it.
    // `far_end`, not `far`: the Windows SDK still carries the 16-bit memory-model
    // keywords as empty object-like macros (`#define far` in minwindef.h), so a
    // variable named `far` vanishes mid-declaration under MSVC and the line after it
    // becomes a syntax error. Same in the two other places this name was natural.
    const Handled far_end = size_to(d, s, id, 400, 4);
    CHECK(far_end.accepted());
    CHECK(far_end.clamped());
    CHECK(far_end.boundary == kAtWholeContext);
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 100});
    CHECK(ui::placed_for(workspace_scene(d, s), id)->rect.w == 48);

    // A CELLS extent has no such wall, because an absolute size has nothing to do
    // with the viewport: the same overshoot authors 400 cells and the canvas
    // simply clips, exactly as W-2 let an object be positioned past the edge.
    REQUIRE(doc::set_width(d, id, ui::Extent{ui::kExtentCells, 10}).accepted);
    const Handled past = size_to(d, s, id, 400, 4);
    CHECK(past.accepted());
    CHECK_FALSE(past.clamped());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, 400});
    // ...until the document's OWN limit, which is where it does stop.
    const Handled huge = size_to(d, s, id, doc::kMaxCells + 1000, 4);
    CHECK(huge.accepted());
    CHECK(huge.boundary == kAtLargest);
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, doc::kMaxCells});

    // SEMANTIC INVALID AUTHORING, through the typed path, at the same extremes:
    // refused, and the authored state does not move.
    REQUIRE(doc::set_width(d, id, ui::Extent{ui::kExtentPercent, 50}).accepted);
    const Written typed = doc::set_width(d, id, ui::Extent{ui::kExtentPercent, 400});
    CHECK_FALSE(typed.accepted);
    CHECK(typed.refusal == "a share is 1% to 100%");
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 50}); // untouched

    // The gesture never hands the document a proposal it would refuse -- that is
    // what makes "the clamp lives in the gesture and the judgement in the
    // document" a division of labour rather than two chances to be wrong.
    for (const std::int64_t want : {-99999, 0, 1, 47, 48, 49, 100000}) {
        REQUIRE(size_to(d, s, id, want, want).accepted());
        CHECK(doc::check_extent(doc::find(d, id)->width).accepted);
        CHECK(doc::check_extent(doc::find(d, id)->height).accepted);
    }
}

TEST_CASE("move and resize meet the workspace with one policy, in two different walls") {
    // §16. The philosophy is shared: a hand stops and says so, a written value is
    // refused. The WALLS differ, and they differ because the properties do --
    // which is a fact about the vocabulary rather than a rule Workshop invented.
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "box", 0, 0, ui::Extent{ui::kExtentCells, 4},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    s.workspace_w = 48;
    s.workspace_h = 16;

    // Position: the origin is a wall (there are no cells before it) and the far
    // edge is not (the canvas clips).
    CHECK(nudge(d, s, -1, -1).clamped());
    CHECK(doc::find(d, id)->x == doc::kFirstCell);
    REQUIRE(doc::move(d, id, 100, 100).accepted);
    const Handled beyond = nudge(d, s, +1, +1);
    CHECK(beyond.accepted());
    CHECK_FALSE(beyond.clamped()); // past the far edge is not a wall for a position
    CHECK(doc::find(d, id)->x == 101);

    // Size: the smallest authorable extent is a wall for both modes; the far wall
    // exists only for a share, and it is 100%.
    REQUIRE(doc::move(d, id, 0, 0).accepted);
    CHECK(grow(d, s, -99, -99).clamped());
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentCells, ui::kMinCells});
    REQUIRE(doc::set_width(d, id, ui::Extent{ui::kExtentPercent, 99}).accepted);
    CHECK(grow(d, s, +5, 0).boundary == kAtWholeContext);
    CHECK(doc::find(d, id)->width == ui::Extent{ui::kExtentPercent, 100});

    // And every one of those is a CLAMP, never a refusal: the document's refusals
    // still belong to typed values only.
    CHECK(nudge(d, s, -1, -1).accepted());
    CHECK(grow(d, s, -99, -99).accepted());
    CHECK_FALSE(doc::move(d, id, -1, 0).accepted);
    CHECK_FALSE(doc::set_width(d, id, ui::Extent{ui::kExtentCells, 0}).accepted);
}

TEST_CASE("a share keeps following the workspace after a resize; cells do not") {
    // §23 and §24, as one contrast. This is the maker-facing consequence of
    // preserving the authored mode, and the reason converting a dragged share to
    // cells would have destroyed something the maker cannot get back.
    WorkshopDoc d;
    const std::int64_t share = doc::add(d, "share", 0, 0, ui::Extent{ui::kExtentPercent, 60},
                                        ui::Extent{ui::kExtentCells, 3});
    const std::int64_t fixed = doc::add(d, "fixed", 0, 8, ui::Extent{ui::kExtentCells, 28},
                                        ui::Extent{ui::kExtentCells, 3});
    Session s;
    s.workspace_w = 48;
    s.workspace_h = 16;

    // Both are 28 cells wide right now, by two different truths.
    CHECK(ui::placed_for(workspace_scene(d, s), share)->rect.w == 28);
    CHECK(ui::placed_for(workspace_scene(d, s), fixed)->rect.w == 28);

    // Resize BOTH to 34 resolved cells, by hand.
    s.selected = share;
    REQUIRE(size_to(d, s, share, 34, 3).accepted());
    s.selected = fixed;
    REQUIRE(size_to(d, s, fixed, 34, 3).accepted());
    CHECK(doc::find(d, share)->width == ui::Extent{ui::kExtentPercent, 71});
    CHECK(doc::find(d, fixed)->width == ui::Extent{ui::kExtentCells, 34});
    CHECK(ui::placed_for(workspace_scene(d, s), share)->rect.w == 34);
    CHECK(ui::placed_for(workspace_scene(d, s), fixed)->rect.w == 34);

    // Now narrow the workspace, exactly as `[` does. NO authored value changes,
    // and the two objects part company: the one authored as a share is still a
    // share OF THE NEW WORKSPACE, the one authored in cells is still 34 cells.
    s.workspace_w = 24;
    CHECK(doc::find(d, share)->width == ui::Extent{ui::kExtentPercent, 71});
    CHECK(doc::find(d, fixed)->width == ui::Extent{ui::kExtentCells, 34});
    CHECK(ui::placed_for(workspace_scene(d, s), share)->rect.w == 17); // 71% of 24
    CHECK(ui::placed_for(workspace_scene(d, s), fixed)->rect.w == 34); // clipped, not resized

    // Had the drag written cells, the share object would now be indistinguishable
    // from the other one -- and nothing in the document would remember that a
    // maker ever asked for a proportion. That is the intent the mode carries.
    CHECK(doc::find(d, share)->width.mode != doc::find(d, fixed)->width.mode);
}

TEST_CASE("the size handle is derived, is Workshop's, and grabs the right identity") {
    WorkshopDoc d;
    const std::int64_t back = doc::add(d, "back", 0, 0, ui::Extent{ui::kExtentCells, 20},
                                       ui::Extent{ui::kExtentCells, 8});
    const std::int64_t front = doc::add(d, "front", 2, 2, ui::Extent{ui::kExtentCells, 4},
                                        ui::Extent{ui::kExtentCells, 3});
    Session s;
    s.workspace_w = 48;
    s.workspace_h = 16;

    // No selection, no handle: the affordance belongs to the selected object.
    CHECK_FALSE(size_handle(d, s).shown);

    // It follows the RESOLVED rectangle, so it moves when the object moves,
    // resizes when the object resizes, and follows a share when the workspace
    // changes -- without anything telling it to.
    s.selected = back;
    CHECK(size_handle(d, s).x == 20);
    REQUIRE(doc::move(d, back, 3, 1).accepted);
    CHECK(size_handle(d, s).x == 23);
    CHECK(size_handle(d, s).y == 9);
    REQUIRE(doc::set_width(d, back, ui::Extent{ui::kExtentPercent, 50}).accepted);
    CHECK(size_handle(d, s).x == 3 + 24);
    s.workspace_w = 24;
    CHECK(size_handle(d, s).x == 3 + 12);
    s.workspace_w = 48;

    // It is PAINTED, as a glyph a maker can tell from the ring, the body and the
    // workspace -- and it is painted at exactly the cell it is grabbed at.
    //
    // The glyph is written out as the LITERAL a maker sees, deliberately NOT as
    // `kHandleGlyph`. An expectation read from the constant under test moves with
    // it, so it can only ever agree with it: Z0a's first canary changed
    // `kHandleGlyph "+" -> "*"` and this whole suite stayed GREEN, because both
    // sides of the comparison changed together. The duplication is the point. What
    // it duplicates is the README's contract -- the handle is the `+` on the
    // selection ring's bottom-right corner -- and `*` is precisely what the glyph
    // must not become, since that is already the accent role's own character in the
    // terminal medium, which is the ring this affordance has to be told apart from.
    // The position below stays DERIVED; only the appearance is pinned from outside.
    const Handle grip = size_handle(d, s);
    REQUIRE(grip.shown);
    CHECK(label_at(paint(d, s), kWorkspaceX + grip.x, kWorkspaceY + grip.y) == "+");

    // The handle sits one cell outside its own object, so it can lie over a
    // neighbour. It wins -- and that priority is WORKSHOP's, in Workshop's own
    // gesture: ui::hit still answers with the topmost authored element, which at
    // that cell is the object underneath.
    s.selected = front;
    const Handle small = size_handle(d, s);
    REQUIRE(small.shown);
    CHECK(small.x == 6);
    CHECK(small.y == 5);
    REQUIRE(ui::hit(workspace_scene(d, s), small.x, small.y) != nullptr);
    CHECK(ui::hit(workspace_scene(d, s), small.x, small.y)->id == back); // the package's answer
    CHECK(take_hold(d, s, small.x, small.y) == front);                   // Workshop's
    CHECK(s.drag.resizing);
    end_drag(s);

    // One cell away from the handle is an ordinary press: the body under the
    // pointer, moved and not resized.
    CHECK(take_hold(d, s, small.x + 1, small.y) == back);
    CHECK_FALSE(s.drag.resizing);
    end_drag(s);

    // An object whose handle would fall outside the workspace has none -- what a
    // maker cannot see, a maker cannot grab. The keyboard and the inspector still
    // reach it, so it is never stuck.
    s.selected = back;
    REQUIRE(doc::set_width(d, back, ui::Extent{ui::kExtentCells, 200}).accepted);
    CHECK_FALSE(size_handle(d, s).shown);
    CHECK(take_hold(d, s, 60, 9) == 0);
    CHECK(grow(d, s, -190, 0).accepted());
    CHECK(size_handle(d, s).shown);
}

TEST_CASE("the four resize keys and the handle are ONE gesture, reached two ways") {
    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "wide", 1, 1, ui::Extent{ui::kExtentPercent, 60},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    s.workspace_w = 48;
    s.workspace_h = 16;

    // The keyboard asks for one more RESOLVED cell than it can see, which is the
    // same question the pointer asks by landing one cell further out -- so both
    // go through one projection and one document operation, and a share dragged
    // and a share grown cannot become different numbers.
    REQUIRE(ui::placed_for(workspace_scene(d, s), id)->rect.w == 28);
    REQUIRE(grow(d, s, +1, 0).accepted());
    CHECK(ui::placed_for(workspace_scene(d, s), id)->rect.w == 29);
    const ui::Extent by_key = doc::find(d, id)->width;

    REQUIRE(doc::set_width(d, id, ui::Extent{ui::kExtentPercent, 60}).accepted);
    const Handle grip = size_handle(d, s);
    REQUIRE(take_hold(d, s, grip.x, grip.y) == id);
    REQUIRE(drag_to(d, s, grip.x + 1, grip.y).accepted());
    CHECK(doc::find(d, id)->width == by_key); // the same authored value, to the digit
    end_drag(s);

    // Growing and shrinking returns the same SIZE. On a share it need not return
    // the same number, because the projection authors the one canonical share for
    // a given cell count -- so the value settles instead of walking.
    const std::int64_t was = ui::placed_for(workspace_scene(d, s), id)->rect.w;
    REQUIRE(grow(d, s, +3, +2).accepted());
    REQUIRE(grow(d, s, -3, -2).accepted());
    CHECK(ui::placed_for(workspace_scene(d, s), id)->rect.w == was);
    CHECK(doc::find(d, id)->height == ui::Extent{ui::kExtentCells, 4});
    const ui::Extent settled = doc::find(d, id)->width;
    REQUIRE(grow(d, s, +3, 0).accepted());
    REQUIRE(grow(d, s, -3, 0).accepted());
    CHECK(doc::find(d, id)->width == settled);
}

TEST_CASE("a resize survives every extent and pointer a poke or a wire can produce") {
    // §25. A resize's proposal is a DIFFERENCE between a pointer off the bus and
    // an authored edge a poke can write, so both terms are untrusted numeric
    // input and the arithmetic must be total before anything is validated.
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    CHECK(detail::minus(0, kMin) == kMax);       // saturates instead of wrapping
    CHECK(detail::minus(kMin, 1) == kMin);
    CHECK(detail::minus(5, 3) == 2);
    CHECK(detail::minus(-5, 3) == -8);

    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "one", 0, 0, ui::Extent{ui::kExtentCells, 4},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    s.workspace_w = 48;
    s.workspace_h = 16;

    // A poked extent no setter would ever have produced: the handle refuses to be
    // shown somewhere the object is not, and the projection still lands on a legal
    // authored value.
    for (const std::int64_t poked :
         std::initializer_list<std::int64_t>{kMin, kMin + 1, -1, 0, kMax - 1, kMax}) {
        doc::find_mut(d, id)->width = ui::Extent{ui::kExtentCells, poked};
        doc::find_mut(d, id)->height = ui::Extent{ui::kExtentCells, poked};
        // Shown or not, the grip never lands somewhere the object is not: the
        // additions that would say where it goes are not representable for most of
        // these, and the answer is "nowhere" rather than a wrapped cell.
        const Handle grip = size_handle(d, s);
        if (grip.shown) {
            CHECK(grip.x >= 0);
            CHECK(grip.x < s.workspace_w);
            CHECK(grip.y >= 0);
            CHECK(grip.y < s.workspace_h);
        }
        REQUIRE(grow(d, s, +1, -1).accepted());
        CHECK(doc::check_extent(doc::find(d, id)->width).accepted);
        CHECK(doc::check_extent(doc::find(d, id)->height).accepted);
    }

    // A poked MODE is neither cells nor a share. The document refuses to accept
    // one, and the gesture repairs rather than propagates it: what a hand writes
    // is always something the document would have accepted.
    doc::find_mut(d, id)->width = ui::Extent{7, 3};
    CHECK_FALSE(doc::check_extent(doc::find(d, id)->width).accepted);
    REQUIRE(grow(d, s, +1, 0).accepted());
    CHECK(doc::check_extent(doc::find(d, id)->width).accepted);

    // A poked POSITION, with the pointer at the far ends of what the wire can
    // carry: the difference saturates rather than wrapping, and the clamp lands.
    REQUIRE(doc::resize(d, id, ui::Extent{ui::kExtentCells, 4}, ui::Extent{ui::kExtentCells, 4})
                .accepted);
    doc::find_mut(d, id)->x = kMax;
    doc::find_mut(d, id)->y = kMin;
    s.drag = Drag{true, true, id, 0, 0};
    for (const std::int64_t at : std::initializer_list<std::int64_t>{kMin, kMax, 0, -1, 1}) {
        const Handled done = drag_to(d, s, workspace_cell_x(at), workspace_cell_y(at));
        CHECK(done.accepted());
        CHECK(doc::check_extent(doc::find(d, id)->width).accepted);
        CHECK(doc::check_extent(doc::find(d, id)->height).accepted);
    }
    end_drag(s);

    // And a hostile WORKSPACE: resolution is total over those, so the projection
    // it is written in terms of is too.
    doc::find_mut(d, id)->width = ui::Extent{ui::kExtentPercent, 60};
    for (const std::int64_t span : std::initializer_list<std::int64_t>{kMin, -1, 0, 1, kMax}) {
        s.workspace_w = span;
        REQUIRE(size_to(d, s, id, 34, 4).accepted());
        CHECK(doc::check_extent(doc::find(d, id)->width).accepted);
        CHECK(doc::find(d, id)->width.mode == ui::kExtentPercent);
    }
}

// ============================================================================
// Tier 3 — the whole screen, as a value
// ============================================================================

TEST_CASE("the screen shows the selected object, ringed, listed, and inspected") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);

    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(c.width == kScreenW);
    CHECK(c.height == kScreenH);

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
    for (std::size_t i = 0; i < c.rects.size(); ++i) {
        if (c.rects[i].role == surface::role::kAccent) {
            ring = i;
        }
        if (c.rects[i].role == surface::role::kFill && c.rects[i].w == 28) {
            fill = i;
        }
    }
    CHECK(ring < fill);

    // The object list names the SAME objects by identity, and marks the same
    // selection the ring does.
    CHECK(label_at(c, kPanelX, kListY) == "> #1 panel");
    CHECK(label_at(c, kPanelX, kListY + 1) == "  #2 panel");

    // The inspector reads the real object's real properties. The cursor sits on
    // the first row a maker can author, which is Name, not Identity.
    CHECK(s.cursor == 1);
    CHECK(label_at(c, kPanelX, kRowsY) == " Identity #1");
    CHECK(label_at(c, kPanelX, kRowsY + 1) == ">Name     panel");
    CHECK(label_at(c, kPanelX, kRowsY + 5) == " Width    60%");
    CHECK(label_at(c, kPanelX, kRowsY + 7) == " Resolved 28 x 6 cells");
}

TEST_CASE("selecting the other object moves the ring, the list marker, and the inspector") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[1].id;
    refocus(d, s);

    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(has_rect(c, 5, kWorkspaceY + 9, 16, 6, surface::role::kAccent));
    CHECK_FALSE(has_rect(c, 2, kWorkspaceY + 1, 30, 8, surface::role::kAccent));
    CHECK(label_at(c, kPanelX, kListY) == "  #1 panel");
    CHECK(label_at(c, kPanelX, kListY + 1) == "> #2 panel");
    CHECK(label_at(c, kPanelX, kRowsY) == " Identity #2");
    CHECK(label_at(c, kPanelX, kRowsY + 5) == " Width    14");
    // #2's width is authored in CELLS, so its authored and resolved facts
    // coincide -- which the inspector still reports as two rows, because
    // coinciding is not the same as being one fact.
    CHECK(label_at(c, kPanelX, kRowsY + 7) == " Resolved 14 x 4 cells");
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
    // The row carries a cursor and the alert role: a draft cannot be mistaken
    // for a committed value on the screen either.
    CHECK(label_at(drafting, kPanelX, kRowsY + 5) == ">Width    500p_");
    for (const surface::SurfaceLabel& l : drafting.labels) {
        if (l.y == kRowsY + 5) {
            CHECK(l.role == surface::role::kAlert);
        }
    }
    // And the resolved row still reports the COMMITTED width, not the draft.
    CHECK(label_at(drafting, kPanelX, kRowsY + 7) == " Resolved 28 x 6 cells");

    CHECK(s.rows[5].commit() == Commit::Refused);
    s.notice = "Width: " + s.rows[5].refusal();
    s.notice_is_bad = true;

    const surface::SurfaceCanvas refused = paint(d, s);
    CHECK(label_at(refused, 0, kNoticeY) == "Width: a share is 1% to 100%");
    for (const surface::SurfaceLabel& l : refused.labels) {
        if (l.y == kNoticeY) {
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
    // 48 - 44 = 4 cells of room. Workshop does its own layout, so the clip is
    // Workshop's job -- the canvas would happily have run it into the panel.
    CHECK(label_at(c, 44, kWorkspaceY) == "a-na");
}

TEST_CASE("the whole screen survives an empty document, and SAYS it is empty") {
    WorkshopDoc d;
    Session s;
    refocus(d, s);
    CHECK(s.rows.empty()); // nothing selected, so there are no properties to show

    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(c.width == kScreenW);
    // W-0 asserted that nothing was INVENTED here, and nothing is: no row, no
    // object, no identity. What W-2 added is that emptiness now announces
    // itself, because W-2 is the phase in which a maker can reach this state by
    // deleting their own work -- and a panel that merely goes blank is
    // indistinguishable from a tool that has broken.
    CHECK(label_at(c, kPanelX, kListY) == "(none) -- n makes one");
    CHECK(label_at(c, kPanelX, kRowsY) == "(nothing selected)");
    // The workspace and the two headings are still there: an empty document is a
    // document, and the tool does not disappear with it.
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, kWorkspaceW, kWorkspaceH, surface::role::kMuted));
    CHECK(label_at(c, kPanelX, kListY - 1) == "OBJECTS");
    CHECK(label_at(c, kPanelX, kRowsY - 1) == "PROPERTIES");
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

    REQUIRE(rows.size() == static_cast<std::size_t>(kScreenH));
    CHECK(rows[0].rfind("WORKSPACE 48x16 cells", 0) == 0);
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
    CHECK(rows[8].find("Identity #1") != std::string::npos);
    CHECK(rows[13].find("Width    60%") != std::string::npos);
    CHECK(rows[15].find("Resolved 28 x 6 cells") != std::string::npos);
}

// ----------------------------------------------------------------------------
// Tier 3b — a document larger than the panel, and a notice longer than its line
// ----------------------------------------------------------------------------
//
// Both defects here are the same one said twice: a bounded presentation that
// omits something and does not say so. Neither had a case, and neither could
// have: nothing in this suite had ever painted more than five objects, and
// nothing had ever painted a notice the screen could not hold. Every case below
// fails on the code that shipped through W-6.

TEST_CASE("a document that FITS the object list is shown whole, with no chrome at all") {
    // The simple case must stay the simple case. Five objects is the last size
    // that fits, and it is the size most likely to grow accidental furniture
    // when a bigger one gets some.
    WorkshopDoc d = many(kListRows);
    Session s;
    s.selected = d.elements.back().id; // the LAST one -- the row that used to lose its marker
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s));
    CHECK(lines[0] == "  #1 panel");
    CHECK(lines[1] == "  #2 panel");
    CHECK(lines[2] == "  #3 panel");
    CHECK(lines[3] == "  #4 panel");
    CHECK(lines[4] == "> #5 panel");
    for (const std::string& line : lines) {
        CHECK(line.find("...") == std::string::npos); // nothing was left out, so nothing says so
    }
}

TEST_CASE("a sixth object cannot vanish: the list says what it left out and keeps the selection") {
    // Cold-Z0's exact reproduction, and the smallest one there is: press `n`
    // four times on an opening document and the tool stops telling the truth.
    WorkshopDoc d = many(6);
    Session s;
    s.selected = d.elements.back().id; // #6
    refocus(d, s);

    const surface::SurfaceCanvas c = paint(d, s);
    const std::vector<std::string> lines = object_lines(c);
    CHECK(lines[0] == "... 2 earlier");
    CHECK(lines[1] == "  #3 panel");
    CHECK(lines[2] == "  #4 panel");
    CHECK(lines[3] == "  #5 panel");
    CHECK(lines[4] == "> #6 panel");

    // The three on-screen statements that used to disagree. Before this phase
    // the panel showed #1..#5 with the marker on NONE of them, while the object
    // count and the inspector both said #6 -- and the only one a maker could act
    // on was the one that was wrong.
    CHECK(d.elements.size() == 6);
    CHECK(label_at(c, kPanelX, kRowsY) == " Identity #6");

    // The marker is the panel's own furniture: muted, like `(none) -- n makes
    // one`, and it mints no identity and invents no name.
    for (const surface::SurfaceLabel& l : c.labels) {
        if (l.x == kPanelX && l.y == kListY) {
            CHECK(l.role == surface::role::kMuted);
        }
    }
}

TEST_CASE("a selection in the middle of a long document names BOTH walls") {
    // A window that can sit in the middle can be silent on two sides, and
    // silence on either is the same defect. 2 + 3 shown + 4 accounts for all
    // nine, with nothing counted twice.
    WorkshopDoc d = many(9);
    Session s;
    s.selected = 5;
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s));
    CHECK(lines[0] == "... 2 earlier");
    CHECK(lines[1] == "  #3 panel");
    CHECK(lines[2] == "  #4 panel");
    CHECK(lines[3] == "> #5 panel");
    CHECK(lines[4] == "... 4 more");
}

TEST_CASE("a selection near the end keeps the document's tail visible") {
    // Cold-Z0's second witness: nine objects with #8 selected drew five OTHER
    // objects and no marker at all.
    WorkshopDoc d = many(9);
    Session s;
    s.selected = 8;
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s));
    CHECK(lines[0] == "... 5 earlier");
    CHECK(lines[1] == "  #6 panel");
    CHECK(lines[2] == "  #7 panel");
    CHECK(lines[3] == "> #8 panel");
    CHECK(lines[4] == "  #9 panel");
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
    s.selected = 60; // the fifth in the file, and neither the fifth nor the middle by identity
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s));
    CHECK(lines[0] == "... 2 earlier");
    CHECK(lines[1] == "  #70 panel");
    CHECK(lines[2] == "  #20 panel");
    CHECK(lines[3] == "> #60 panel");
    CHECK(lines[4] == "... 4 more");
    // Sorted by identity the window would have begun #10 #20 #30; sorted by
    // anything else it would have begun somewhere else again. It begins where
    // the file does.
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
    // THE STRONG WITNESS. W-6 met this live and answered it by shortening one
    // producer's wording: the rendered cycle is budgeted in characters
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
    REQUIRE(refused.refusal.size() > static_cast<std::size_t>(kScreenW));

    Session s;
    s.selected = 9000000000000001;
    refocus(d, s);
    s.notice = refused.refusal;
    s.notice_is_bad = true;

    const surface::SurfaceCanvas c = paint(d, s);
    const std::string shown = label_at(c, 0, kNoticeY);
    CHECK(shown.size() == static_cast<std::size_t>(kScreenW)); // it fits the line it has
    CHECK(shown.compare(shown.size() - 3, 3, "...") == 0);     // and says it did not fit
    // What IS shown is the message's own head, unaltered -- the presentation
    // shortened it and did not reword it.
    CHECK(refused.refusal.compare(0, shown.size() - 3, shown, 0, shown.size() - 3) == 0);
    // truth != presentation capacity. The screen is bounded; the message is not,
    // and a wider screen would need nothing from anybody but room.
    CHECK(s.notice == refused.refusal);
    CHECK(s.notice.size() > shown.size());
    // The role is untouched: fitting a refusal does not make it less of one.
    for (const surface::SurfaceLabel& l : c.labels) {
        if (l.y == kNoticeY) {
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

    REQUIRE(rows.size() == static_cast<std::size_t>(kScreenH));
    const std::string line = rows[static_cast<std::size_t>(kNoticeY)];
    CHECK(line.size() == static_cast<std::size_t>(kScreenW)); // the row is the screen's width
    CHECK(line.compare(line.size() - 3, 3, "...") == 0);      // and it ENDS by saying so
    CHECK(line.find("END") == std::string::npos);             // the tail really is gone
    CHECK(s.notice.find("END") != std::string::npos);         // and Workshop still has it
}

// ============================================================================
// Tier 4 — the WEAVE, through a real bus: input moments become maker gestures
// ============================================================================
//
// This tier exists because W-4 closed P16. `WorkshopWeave` used to live in
// workshop.cpp's anonymous namespace, so every phase since W-0 could prove
// `gesture -> document` and never `message -> gesture` -- and the binding was
// the one part of the pointer path nothing witnessed. Moving the class into
// workshop/weave.hpp is the whole change; there is no test hook, no framework,
// and no seam that exists only for this file.
//
// What it buys is the phase's own headline, asserted rather than argued: a
// press carries the position it happened at, so the chain from a published
// message to a semantic document operation can be walked end to end WITHOUT the
// weave ever being told where the pointer is by anything except the event it is
// handling.

namespace {

struct SeenState {
    std::int64_t frames = 0;
    ZEN_SHAPE(SeenState, 1, ZEN_FIELD(frames));
};

/// An ordinary Skin's ears: whatever Workshop published, kept as values.
class Painter : public loom::WeaveBase<Painter, SeenState,
                                       loom::Accept<surface::SurfaceCanvas, surface::SurfaceText>,
                                       loom::Emit<>> {
public:
    Painter(std::vector<surface::SurfaceCanvas>& canvases,
            std::vector<surface::SurfaceText>& notes)
        : canvases_(&canvases), notes_(&notes) {}
    void on(const surface::SurfaceCanvas& c, loom::Mail&) {
        ++state_.frames;
        canvases_->push_back(c);
    }
    void on(const surface::SurfaceText& t, loom::Mail&) { notes_->push_back(t); }

private:
    std::vector<surface::SurfaceCanvas>* canvases_;
    std::vector<surface::SurfaceText>* notes_;
};



/// A live Workshop: the real weave on a real bus, driven only by published
/// input messages. Nothing here reaches past the message boundary except to
/// READ the result.
struct Live {
    loom::Switchboard bus;
    HostContext host;
    std::vector<surface::SurfaceCanvas> canvases;
    std::vector<surface::SurfaceText> notes;
    WorkshopWeave* w = nullptr;

    Live() {
        auto weave = std::make_unique<WorkshopWeave>(host);
        w = weave.get();
        loom::Grant grant = loom::emit_default_grant(*w);
        loom::allow_poke_answers(grant);
        const loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant));
        w->zen_set_self(id);
        (void)loom::mount<Painter>(bus, canvases, notes);
    }

    void publish(const loom::Value& v) {
        (void)bus.publish(loom::Message(v, loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.pump();
    }

    void key(std::int64_t sc, std::int64_t mods = input::mod::kNone) {
        publish(loom::to_value(input::KeyPressed{sc, "", mods}));
    }
    void text(const std::string& s) { publish(loom::to_value(input::TextEntered{s})); }

    /// A pointer event AT A WORKSPACE CELL. The translation from workspace cell
    /// to the terminal position a backend reports is the inverse of the weave's
    /// own, done here so every case below reads in the coordinates a maker
    /// thinks in.
    static std::int64_t term_x(std::int64_t wx) { return wx + kWorkspaceX; }
    static std::int64_t term_y(std::int64_t wy) {
        return wy + kWorkspaceY + surface::kTuiCanvasTopRow;
    }

    /// The same workspace cell, as the WINDOW would report it: the pixel at the
    /// cell's top-left corner. Deliberately the inverse of the graphical Skin's
    /// own layout and not of the terminal's -- the whole point of G-1 is that
    /// these two are different numbers naming one place.
    static std::int64_t px_x(std::int64_t wx) {
        return (wx + kWorkspaceX) * surface::kCanvasCellPx;
    }
    static std::int64_t px_y(std::int64_t wy) {
        return (wy + kWorkspaceY) * surface::kCanvasCellPx;
    }

    void press(std::int64_t wx, std::int64_t wy, std::int64_t mods = input::mod::kNone) {
        publish(loom::to_value(input::PointerButton{1, true, term_x(wx), term_y(wy),
                                                    input::space::kCells, mods}));
    }
    void release(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerButton{1, false, term_x(wx), term_y(wy),
                                                    input::space::kCells, input::mod::kNone}));
    }
    void motion(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerMoved{term_x(wx), term_y(wy), 0, 0,
                                                   input::space::kCells, input::mod::kNone}));
    }

    /// The same three gestures, arriving from the graphical Skin's window. The
    /// `+ cell/2` puts the event in the MIDDLE of the cell rather than on its
    /// corner, which is where a maker's pointer actually is and is what makes a
    /// truncating (rather than flooring) projection visible.
    static std::int64_t mid(std::int64_t p) { return p + surface::kCanvasCellPx / 2; }
    void press_px(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerButton{1, true, mid(px_x(wx)), mid(px_y(wy)),
                                                    input::space::kPixels, input::mod::kNone}));
    }
    void release_px(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerButton{1, false, mid(px_x(wx)), mid(px_y(wy)),
                                                    input::space::kPixels, input::mod::kNone}));
    }
    void motion_px(std::int64_t wx, std::int64_t wy) {
        publish(loom::to_value(input::PointerMoved{mid(px_x(wx)), mid(px_y(wy)), 0, 0,
                                                   input::space::kPixels, input::mod::kNone}));
    }

    /// The window manager asked the surface to close.
    void close_requested() { publish(loom::to_value(surface::SurfaceCloseRequested{})); }

    const WorkshopDoc& doc() const { return w->document(); }
    const Session& session() const { return w->session(); }
    std::string notice() const { return w->session().notice; }
    const ui::Element* first() const { return &w->document().elements.front(); }
    const ui::Element* second() const { return &w->document().elements[1]; }

    /// The inspector row with this label, as a maker would read it.
    const Row* row(const std::string& label) const {
        for (const Row& r : w->session().rows) {
            if (r.label() == label) {
                return &r;
            }
        }
        return nullptr;
    }

    /// Put the cursor on a named row and open it for editing, by keys only.
    void begin_editing(const std::string& label) {
        for (int guard = 0; guard < 32; ++guard) {
            const Session& s = session();
            REQUIRE(s.cursor < s.rows.size());
            if (s.rows[s.cursor].label() == label) {
                key(input::scan::kReturn);
                return;
            }
            key(input::scan::kDown);
        }
        FAIL("no inspector row labelled ", label);
    }
};

/// The selected object's RESOLVED width, read the way the canvas reads it.
std::int64_t resolved_w(const Live& t) {
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* p = ui::placed_for(scene, t.session().selected);
    REQUIRE(p != nullptr);
    return p->rect.w;
}

} // namespace

TEST_CASE("a maker types `70%` through the canonical text route, and 70p is history") {
    // P4's headline, end to end. W-0 could not reach `%` at all, so W-2's live
    // evidence had to commit `70p` and call it a workaround. The characters now
    // arrive as text the platform produced, and Workshop appends them without
    // owning one line of keyboard knowledge.
    Live t;
    t.begin_editing("Width");
    REQUIRE(t.row("Width")->editing());
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }

    t.text("7");
    t.text("0");
    t.text("%");
    CHECK(t.row("Width")->draft() == "70%");

    t.key(input::scan::kReturn);
    CHECK(t.first()->width.mode == ui::kExtentPercent);
    CHECK(t.first()->width.amount == 70);
    CHECK(t.notice() == "committed Width = 70%");
    CHECK(t.row("Width")->value() == "70%");
}

TEST_CASE("entered text is text; editing controls are keys; and the two never swap jobs") {
    Live t;
    t.begin_editing("Name");
    REQUIRE(t.row("Name")->editing());
    CHECK(t.row("Name")->draft() == "panel");

    // Backspace is a KEY and it erases; it is never text.
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kBackspace);
    }
    CHECK(t.row("Name")->draft().empty());

    // Capitals and symbols -- both unreachable before W-4 -- and `q`, which is
    // the quit command in the other mode and is simply a letter here.
    t.text("Panel");
    t.text("-");
    t.text("q");
    CHECK(t.row("Name")->draft() == "Panel-q");
    CHECK_FALSE(t.host.quit); // the `q` typed, it did not quit

    // Escape is a key and it cancels: nothing was written.
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.row("Name")->editing());
    CHECK(t.first()->label == "panel");
    CHECK(t.notice() == "edit cancelled -- nothing was written");

    // And in COMMAND mode, text is not a command: `n` as text creates nothing.
    const std::size_t before = t.doc().elements.size();
    t.text("n");
    t.text("d");
    CHECK(t.doc().elements.size() == before);
}

TEST_CASE("a multi-byte character survives typing and erasing as ONE character") {
    Live t;
    t.begin_editing("Name");
    REQUIRE(t.row("Name")->editing());
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text("caf\xc3\xa9"); // cafe-acute, the last character two bytes
    CHECK(t.row("Name")->draft() == "caf\xc3\xa9");
    t.key(input::scan::kBackspace);
    // One press erased the whole character, not half of it -- a draft holding
    // half a character is not text and no setter could parse it.
    CHECK(t.row("Name")->draft() == "caf");
    t.key(input::scan::kReturn);
    CHECK(t.first()->label == "caf");
}

TEST_CASE("Shift turns the move gesture into the resize gesture, and hjkl still moves") {
    // W-3 paid four literal keys (`,` `.` `-` `=`) for a second directional
    // gesture, because the wire could not say "with Shift held". It can now.
    Live t;
    const std::int64_t x0 = t.first()->x;
    const std::int64_t y0 = t.first()->y;
    const std::int64_t w0 = t.first()->width.amount;

    t.key(input::scan::kL); // bare: moves
    CHECK(t.first()->x == x0 + 1);
    CHECK(t.first()->width.amount == w0);

    t.key(input::scan::kL, input::mod::kShift); // shifted: resizes
    CHECK(t.first()->x == x0 + 1);              // position untouched
    CHECK(t.first()->width.amount > w0);
    CHECK(t.first()->width.mode == ui::kExtentPercent); // and the MODE is preserved

    t.key(input::scan::kJ);
    CHECK(t.first()->y == y0 + 1);
    const std::int64_t h0 = t.first()->height.amount;
    t.key(input::scan::kJ, input::mod::kShift);
    CHECK(t.first()->y == y0 + 1);
    CHECK(t.first()->height.amount == h0 + 1);

    t.key(input::scan::kK, input::mod::kShift);
    CHECK(t.first()->height.amount == h0);

    // Shrinking back returns the same SIZE and not necessarily the same NUMBER,
    // which is W-3's canonical-share property arriving through the new binding
    // rather than a new behaviour: 60% and 59% both resolve to 28 cells at a
    // 48-cell workspace, and the projection authors the canonical one.
    const std::int64_t wide = resolved_w(t);
    t.key(input::scan::kH, input::mod::kShift);
    CHECK(resolved_w(t) == wide - 1);
    CHECK(t.first()->width.mode == ui::kExtentPercent);
    t.key(input::scan::kL, input::mod::kShift);
    CHECK(resolved_w(t) == wide);

    // The four W-3 workaround keys are GONE, not merely unrecommended: they do
    // nothing at all now, so nothing can quietly keep depending on them.
    const std::int64_t w1 = t.first()->width.amount;
    const std::int64_t h1 = t.first()->height.amount;
    for (const std::int64_t sc : {input::scan::kComma, input::scan::kPeriod,
                                  input::scan::kMinus, input::scan::kEquals}) {
        t.key(sc);
    }
    CHECK(t.first()->width.amount == w1);
    CHECK(t.first()->height.amount == h1);
}

TEST_CASE("a press grabs from ITS OWN position, with no motion event anywhere") {
    // THE PHASE, in one case. Not one PointerMoved is published before the
    // press -- which under W-2's vocabulary meant the weave believed the pointer
    // was at 0,0 and grabbed whatever was there.
    Live t;
    const std::int64_t id2 = t.second()->id;
    const std::int64_t x = t.second()->x;
    const std::int64_t y = t.second()->y;

    t.press(x + 2, y + 1);
    CHECK(t.session().selected == id2);
    CHECK(t.session().drag.active);
    CHECK(t.session().drag.id == id2);
    CHECK_FALSE(t.session().drag.resizing);
    CHECK(t.notice() == "holding #" + std::to_string(id2) + " -- drag to move it");
    // The grab offset proves the position was believed: the press was 2 cells
    // right and 1 down of the object's own corner.
    CHECK(t.session().drag.grab_dx == 2);
    CHECK(t.session().drag.grab_dy == 1);

    // Drag, and the object follows the pointer, still with no prior motion state.
    t.motion(x + 7, y + 1);
    CHECK(t.second()->x == x + 5);
    CHECK(t.second()->y == y);
    t.release(x + 7, y + 1);
    CHECK_FALSE(t.session().drag.active);
}

TEST_CASE("a press after a long silence uses the press, not the last thing seen") {
    // W-2's exact failure, recreated at the weave: the pointer is reported
    // somewhere, then the platform goes quiet (a console reports no motion while
    // it lacks focus), then a click arrives somewhere else entirely. The old
    // reconstruction answered with the stale position.
    Live t;
    const std::int64_t id1 = t.first()->id;
    const std::int64_t id2 = t.second()->id;

    // The pointer is last SEEN over object #1...
    t.motion(t.first()->x + 1, t.first()->y + 1);
    CHECK_FALSE(t.session().drag.active); // motion outside a drag does nothing at all

    // ...and the click, with nothing in between, lands on object #2.
    t.press(t.second()->x + 1, t.second()->y + 1);
    CHECK(t.session().selected == id2);
    CHECK(t.session().drag.id == id2);
    CHECK(t.session().selected != id1);

    // And the release is its own moment too: it does not need a motion to know
    // where it happened.
    t.release(t.second()->x + 1, t.second()->y + 1);
    CHECK(t.notice() == "released #" + std::to_string(id2));
}

TEST_CASE("a press on the body reaches move; a press on the size handle reaches resize") {
    Live t;
    const std::int64_t id2 = t.second()->id;
    t.press(t.second()->x, t.second()->y); // select it so its handle is shown
    t.release(t.second()->x, t.second()->y);
    REQUIRE(t.session().selected == id2);

    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* p = ui::placed_for(scene, id2);
    REQUIRE(p != nullptr);
    const std::int64_t px = p->rect.x;
    const std::int64_t py = p->rect.y;
    const std::int64_t hx = px + p->rect.w;
    const std::int64_t hy = py + p->rect.h;
    const std::int64_t w0 = t.second()->width.amount;

    // The handle: one cell past the bottom-right corner.
    t.press(hx, hy);
    CHECK(t.session().drag.active);
    CHECK(t.session().drag.resizing);
    CHECK(t.notice() == "holding #" + std::to_string(id2) + " -- drag to resize it");
    t.motion(hx + 3, hy);
    CHECK(t.second()->width.amount == w0 + 3);
    CHECK(t.second()->x == px); // a resize moved nothing
    t.release(hx + 3, hy);

    // The body: the same object, inside its own rectangle.
    t.press(px + 1, py);
    CHECK(t.session().drag.active);
    CHECK_FALSE(t.session().drag.resizing);
    t.release(px + 1, py);

    // Empty space: nothing grabbed, and it says so.
    t.press(kWorkspaceW - 1, kWorkspaceH - 1);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice() == "nothing there");
}

TEST_CASE("the semantic operations are still the only authority, through the message path") {
    // The gesture layer got better facts; it did not get new powers. Everything
    // below travels message -> gesture -> doc::, and doc:: still decides.
    Live t;
    const std::int64_t id1 = t.first()->id;

    // CLAMP: a hand that reaches past the origin stops at it, authors the
    // boundary value, and says which wall it met (W-3's policy, unchanged).
    t.press(t.first()->x, t.first()->y);
    t.motion(-50, -50);
    CHECK(t.first()->x == 0);
    CHECK(t.first()->y == 0);
    CHECK(t.notice() == "#" + std::to_string(id1) + " is at 0,0 -- stopped at the workspace edge");
    CHECK_FALSE(t.session().notice_is_bad); // a clamp WROTE something
    t.release(0, 0);

    // REFUSE: a typed value that is not allowed is refused, the authored state
    // is untouched, and the draft survives on screen.
    t.begin_editing("Width");
    REQUIRE(t.row("Width")->editing());
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text("0");
    t.key(input::scan::kReturn);
    CHECK(t.session().notice_is_bad); // red means NOTHING WAS WRITTEN
    CHECK(t.notice() == "Width: at least 1 cell");
    CHECK(t.row("Width")->editing());
    CHECK(t.row("Width")->draft() == "0");
    CHECK(t.first()->width.mode == ui::kExtentPercent); // untouched
    t.key(input::scan::kEscape);
}

TEST_CASE("a pointer in a space Workshop does not speak is ignored, not mis-placed") {
    // `space` earning its field, and G-1 is where it finally earns it in BOTH
    // directions. A backend reporting a space this application cannot place is
    // not talking about anything the document has, and guessing an equivalence is
    // exactly the mistake the field exists to prevent -- while a backend
    // reporting one it CAN place is now projected rather than refused.
    Live t;
    const std::int64_t id2 = t.second()->id;
    t.press(t.second()->x, t.second()->y);
    t.release(t.second()->x, t.second()->y);
    REQUIRE(t.session().selected == id2);

    const std::int64_t before_x = t.second()->x;
    // An unstamped event -- the default -- names no medium and is placed by none.
    t.publish(loom::to_value(input::PointerButton{1, true, Live::term_x(t.second()->x),
                                                  Live::term_y(t.second()->y),
                                                  input::space::kUnknown, input::mod::kNone}));
    CHECK_FALSE(t.session().drag.active);
    t.publish(loom::to_value(input::PointerMoved{9999, 9999, 0, 0, input::space::kUnknown,
                                                  input::mod::kNone}));
    CHECK(t.second()->x == before_x);
    // Nor is a space nobody has defined. A future medium must be READ, not
    // assumed into whichever projection happens to be nearest.
    t.publish(loom::to_value(input::PointerButton{1, true, 0, 0, 4242, input::mod::kNone}));
    CHECK_FALSE(t.session().drag.active);
}

TEST_CASE("a click in the WINDOW selects the object the maker is pointing at") {
    // The graphical half of "a click selects the same authored object the maker
    // can see". Same weave, same gesture, same document operation -- the only
    // difference is that the numbers arrived in pixels, and the projection is
    // what makes that not matter.
    Live t;
    const std::int64_t id1 = t.first()->id;
    const std::int64_t id2 = t.second()->id;
    REQUIRE(t.session().selected == id1);

    t.press_px(t.second()->x + 1, t.second()->y + 1);
    CHECK(t.session().selected == id2);
    CHECK(t.session().drag.active);
    CHECK_FALSE(t.session().drag.resizing);
    t.release_px(t.second()->x + 1, t.second()->y + 1);
    CHECK_FALSE(t.session().drag.active);

    // And empty space is still empty space: no selection change, no grab.
    t.press_px(kWorkspaceW - 1, kWorkspaceH - 1);
    CHECK(t.session().selected == id2);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice() == "nothing there");
}

TEST_CASE("a drag in the WINDOW authors placement, through the same document operation") {
    Live t;
    const std::int64_t id2 = t.second()->id;
    const std::int64_t x0 = t.second()->x;
    const std::int64_t y0 = t.second()->y;

    t.press_px(x0, y0); // the object's own top-left: grab offset 0,0
    REQUIRE(t.session().selected == id2);
    REQUIRE(t.session().drag.active);
    CHECK(t.session().drag.grab_dx == 0);
    CHECK(t.session().drag.grab_dy == 0);

    t.motion_px(x0 + 4, y0 + 3);
    CHECK(t.second()->x == x0 + 4);
    CHECK(t.second()->y == y0 + 3);
    t.release_px(x0 + 4, y0 + 3);
    CHECK_FALSE(t.session().drag.active);

    // The document is the authority, not the pointer: a drag past the workspace
    // edge stops at the edge and SAYS it stopped, exactly as the terminal's does.
    t.press_px(t.second()->x, t.second()->y);
    t.motion_px(-6, 0);
    CHECK(t.second()->x == -kWorkspaceX);
    CHECK(t.notice().find(kAtWorkspaceStart) != std::string::npos);
    t.release_px(0, 0);
}

TEST_CASE("the resize handle can be grabbed in the WINDOW, and authors an extent") {
    // W-3's semantics, reached through pixels: the handle is one cell past the
    // object's own last cell, a pointer resting ON it proposes the size the
    // object already has, and the mode a maker authored is preserved.
    Live t;
    t.press_px(t.second()->x, t.second()->y); // select #2, whose extents are cells
    t.release_px(t.second()->x, t.second()->y);
    const std::int64_t id2 = t.session().selected;
    REQUIRE(t.second()->id == id2);
    REQUIRE(t.second()->width.mode == ui::kExtentCells);

    const Handle h = size_handle(t.doc(), t.session());
    REQUIRE(h.shown);
    t.press_px(h.x, h.y);
    CHECK(t.session().drag.active);
    CHECK(t.session().drag.resizing);

    const std::int64_t w0 = t.second()->width.amount;
    const std::int64_t h0 = t.second()->height.amount;
    t.motion_px(h.x + 3, h.y + 2);
    CHECK(t.second()->width.mode == ui::kExtentCells); // the authored MODE survives
    CHECK(t.second()->width.amount == w0 + 3);
    CHECK(t.second()->height.amount == h0 + 2);
    t.release_px(h.x + 3, h.y + 2);
}

TEST_CASE("the maker's hands reach the same object whichever medium they arrive through") {
    // The sharpest form of "no shadow interaction model": drive HALF a gesture
    // from the terminal and half from the window. If the graphical path had its
    // own selection, its own drag state or its own hit test, this could not work
    // -- and it is one document, one session and one set of operations, so it
    // does.
    Live t;
    t.press(t.second()->x, t.second()->y); // press: terminal cells
    REQUIRE(t.session().drag.active);
    const std::int64_t id2 = t.session().selected;

    t.motion_px(t.second()->x + 2, t.second()->y); // drag: window pixels
    CHECK(t.second()->x == 6 + 2);

    t.release(t.second()->x, t.second()->y); // release: terminal cells again
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice() == "released #" + std::to_string(id2));
}

TEST_CASE("the native close request reaches the quit policy `q` already had") {
    // The lifecycle half. It is NOT a key: nothing below publishes a scancode,
    // and the weave has no branch that turns this shape into one. What it shares
    // with `q` is the POLICY, which is the thing that should be shared.
    Live t;
    bool stopped = false;
    t.host.request_stop = [&stopped] { stopped = true; };
    REQUIRE_FALSE(t.host.quit);

    t.close_requested();
    CHECK(t.host.quit);
    CHECK(stopped);
}

TEST_CASE("a close request does not care what the maker was in the middle of") {
    // A half-typed width is a draft, and a draft is not a reason to refuse to
    // close: `^s` refuses to SAVE over one (W-5) because writing an unconfirmed
    // value would be the tool putting words in a maker's mouth, and quitting
    // writes nothing at all. P22 -- whether closing should ask about unsaved work
    // -- is a separate product question and G-1 deliberately does not answer it.
    Live t;
    t.begin_editing("Width");
    t.text("70");
    REQUIRE(t.row("Width")->editing());

    t.close_requested();
    CHECK(t.host.quit);
    // And nothing was written on the way out.
    CHECK(t.first()->width.amount == 60);
}

TEST_CASE("Ctrl+C quits by MODIFIER, and a bare c does not") {
    Live t;
    bool stopped = false;
    t.host.request_stop = [&stopped] { stopped = true; };

    t.key(input::scan::kC); // a plain c is not a command at all
    CHECK_FALSE(t.host.quit);
    t.key(input::scan::kC, input::mod::kShift); // nor a shifted one
    CHECK_FALSE(t.host.quit);

    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK(t.host.quit);
    CHECK(stopped);
}

TEST_CASE("canvas, object list and inspector stay coherent through a message-driven session") {
    // W-2 and W-3 proved this over the gesture functions. This proves it over
    // the MESSAGES, which is the layer that changed.
    Live t;
    const std::size_t frames_at_start = t.canvases.size();

    t.key(input::scan::kN); // create
    const std::int64_t made = t.session().selected;
    CHECK(t.doc().elements.size() == 3);
    CHECK(t.notice() == "created #" + std::to_string(made) + " -- a new identity, not a new name");

    t.key(input::scan::kL);                     // move it
    t.key(input::scan::kL, input::mod::kShift); // and resize it

    // The canvas is republished on every one of those, and it is derived from
    // the document rather than patched: what the hit test answers about is what
    // was painted.
    CHECK(t.canvases.size() > frames_at_start);
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* p = ui::placed_for(scene, made);
    REQUIRE(p != nullptr);
    const ui::Placed* under = ui::hit(scene, p->rect.x, p->rect.y);
    REQUIRE(under != nullptr);
    CHECK(under->id == made);

    // The inspector follows the selection, and reads through the properties.
    CHECK(t.row("Identity")->value() == "#" + std::to_string(made));
    CHECK(t.row("X")->value() == std::to_string(t.doc().elements.back().x));

    // Tab moves the selection and the inspector follows.
    t.key(input::scan::kTab);
    CHECK(t.session().selected != made);
    CHECK(t.row("Identity")->value() == "#" + std::to_string(t.session().selected));

    // Delete says where the selection went.
    const std::int64_t doomed = t.session().selected;
    t.key(input::scan::kD);
    CHECK(t.doc().elements.size() == 2);
    CHECK(t.notice().rfind("deleted #" + std::to_string(doomed), 0) == 0);
    CHECK(t.session().selected != doomed);

    // The workspace keys still work, and change no authored value.
    const std::int64_t authored = t.first()->width.amount;
    t.key(input::scan::kLeftBracket);
    CHECK(t.session().workspace_w == kWorkspaceW - 4);
    CHECK(t.first()->width.amount == authored);
    t.key(input::scan::kRightBracket);
    CHECK(t.session().workspace_w == kWorkspaceW);

    // And a status line went out with every frame.
    REQUIRE_FALSE(t.notes.empty());
    CHECK(t.notes.back().slot == surface::kSlotStatus);
    CHECK(t.notes.back().text.rfind("[workshop] 2 objects", 0) == 0);
}

TEST_CASE("the selection marker never disappears as a maker walks past the fifth object") {
    // The contradiction Cold-Z0 measured, driven the way a maker reaches it:
    // `tab` cycles the whole document, and before this phase it selected objects
    // the list simply did not draw. Nine objects, made and walked by keystroke,
    // and the screen is read after every single one.
    Live t;
    for (int i = 0; i < 7; ++i) { // two to begin with, so seven more makes nine
        t.key(input::scan::kN);
    }
    REQUIRE(t.doc().elements.size() == 9);

    for (int step = 0; step < 9; ++step) {
        t.key(input::scan::kTab);
        const std::int64_t id = t.session().selected;
        CAPTURE(step);
        CAPTURE(id);

        bool marked = false;
        bool omission_said = false;
        std::size_t drawn = 0;
        for (const std::string& line : object_lines(t.canvases.back())) {
            if (line.empty()) {
                continue;
            }
            ++drawn;
            if (line == "> #" + std::to_string(id) + " panel") {
                marked = true;
            }
            if (line.rfind("... ", 0) == 0) {
                omission_said = true;
            }
        }
        CHECK(marked);                                // what the status line names, the list shows
        CHECK(omission_said);                         // and what it cannot show, it counts
        CHECK(drawn == static_cast<std::size_t>(kListRows)); // inside the budget it already had

        // The status line and the inspector name the same object, on the same
        // frame the list was read from.
        CHECK(t.notes.back().text.rfind("[workshop] 9 objects | selected #" + std::to_string(id),
                                        0) == 0);
        CHECK(label_at(t.canvases.back(), kPanelX, kRowsY) == " Identity #" + std::to_string(id));
    }
}

TEST_CASE("a notice a maker's own path makes too long is marked on screen, not cut in the session") {
    // P24 through the real message path, on a notice Workshop produces honestly.
    // A document path is the maker's own input and may be any length the
    // platform allows, so `cannot read <path>` is a sentence this tool can be
    // asked to say and cannot show. Nothing is forged and nothing is distorted
    // to produce it: one ordinary keystroke, on a path that is simply not there.
    Live t;
    t.host.document_path =
        "a-workshop-document-with-a-name-its-maker-chose-and-this-terminal-cannot-show-all-of.json";

    t.key(input::scan::kO, input::mod::kCtrl);

    // The refusal is whole in the session, and it is genuinely longer than a
    // line -- the path alone overruns the screen, whatever the platform's own
    // wording for a missing file happens to be.
    REQUIRE(t.notice().size() > static_cast<std::size_t>(kScreenW));
    CHECK(t.notice().find(t.host.document_path) != std::string::npos);

    const std::string shown = label_at(t.canvases.back(), 0, kNoticeY);
    CHECK(shown.size() == static_cast<std::size_t>(kScreenW));
    CHECK(shown.compare(shown.size() - 3, 3, "...") == 0);
    CHECK(t.notice().compare(0, shown.size() - 3, shown, 0, shown.size() - 3) == 0);

    // And the refusal cost the maker nothing but the notice, exactly as before:
    // the live document, the selection and the mint are all untouched.
    CHECK(t.doc().elements.size() == 2);
    CHECK(t.session().selected == t.first()->id);
}

// ============================================================================
// Tier 5 — PERSISTENCE: what survives a process, and what deliberately does not
// ============================================================================
//
// W-5's subject. Three questions, and every case below answers one of them:
//
//   1. WHAT IS THE DOCUMENT?  Everything a maker authored — identity, name,
//      place, both halves of each extent, the ORDER, and the mint — comes back
//      exactly. Nothing else is in the file, and the strongest proof of that is
//      the workspace: save under one and load under another, and the authored
//      share is identical while the resolved cells are not.
//   2. WHAT IS "THE SAME OBJECT" ACROSS PROCESS DEATH?  The identity, and only
//      the identity. A loaded #2 IS the saved #2 — the same number, findable,
//      selectable, editable — and the mint does not rewind, so an identity that
//      died before the save cannot come back after the load.
//   3. WHAT HAPPENS WHEN THE FILE IS WRONG?  Nothing. Every refusal below
//      asserts the live document is untouched, because "a malformed file must
//      never leave Workshop halfway loaded" is the claim, and "the parser
//      returned an error" is not that claim.

namespace {

/// A directory of this run's own, removed when the case ends. Tests never write
/// into the source tree and never share a path with each other.
class TempDir {
public:
    explicit TempDir(const char* tag) {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("zengine-w5-" + std::string(tag) + "-" + std::to_string(++counter));
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    std::string file(const char* name) const { return (path_ / name).string(); }
    std::string document() const { return file("document.json"); }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

/// Read a whole file as bytes, for a case that wants to look at what was
/// actually written rather than at what the writer said it wrote.
std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/// Put bytes at a path -- how a case forges a file that Workshop then meets as
/// an ordinary maker would.
void spillout(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

/// A document with everything W-5 has to preserve in it: two objects sharing a
/// name, one authored in cells and one as a share, and a mint that has already
/// been past a deleted identity.
WorkshopDoc rich_document() {
    WorkshopDoc d;
    doc::add(d, "panel", 3, 2, ui::Extent{ui::kExtentPercent, 60},
             ui::Extent{ui::kExtentCells, 6});
    doc::add(d, "panel", 6, 10, ui::Extent{ui::kExtentCells, 14},
             ui::Extent{ui::kExtentCells, 4});
    const std::int64_t doomed = doc::add(d, "temporary", 0, 0, ui::Extent{ui::kExtentCells, 2},
                                         ui::Extent{ui::kExtentCells, 2});
    REQUIRE(doc::remove(d, doomed).accepted);
    return d;
}

/// The text of a saved document, with one substring replaced — how the refusal
/// cases forge a file that the honest writer could never produce.
std::string forged(const WorkshopDoc& d, const std::string& from, const std::string& to) {
    std::string text = persist::to_text(d);
    const std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
    return text;
}

} // namespace

TEST_CASE("an empty document survives a round trip as an empty document") {
    // The boring case first, because it is the one a format is most likely to
    // get wrong: an empty list and a mint that has never minted are still facts.
    WorkshopDoc empty;
    const std::string text = persist::to_text(empty);

    WorkshopDoc live = two_panels();
    REQUIRE(persist::load_into(live, text).accepted);
    CHECK(live.elements.empty());
    CHECK(live.next_id == 1);
    CHECK(live == empty);
}

TEST_CASE("every authored fact survives, and the identities are the SAME identities") {
    const WorkshopDoc original = rich_document();
    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);

    // Not "equivalent". Equal.
    CHECK(live == original);

    // And the identities are usable AS identities on the other side, which is
    // the operational form of the claim: #2 can be found, hit, and edited.
    const std::int64_t id = original.elements[1].id;
    REQUIRE(doc::find(live, id) != nullptr);
    CHECK(doc::find(live, id)->label == "panel");
    CHECK(doc::set_x(live, id, 9).accepted);
    CHECK(doc::find(live, id)->x == 9);

    Session s;
    const ui::Scene scene = workspace_scene(live, s);
    const ui::Placed* placed = ui::placed_for(scene, id);
    REQUIRE(placed != nullptr);
    const ui::Placed* under = ui::hit(scene, placed->rect.x, placed->rect.y);
    REQUIRE(under != nullptr);
    CHECK(under->id == id);
}

TEST_CASE("two objects called `panel` come back as two objects called `panel`") {
    // W-0's fixture, asked across a process boundary: a duplicate name is legal
    // and stays legal, because the identity is the id. A format that keyed on
    // the name would silently merge these two.
    const WorkshopDoc original = two_panels();
    REQUIRE(original.elements[0].label == original.elements[1].label);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);
    REQUIRE(live.elements.size() == 2);
    CHECK(live.elements[0].label == live.elements[1].label);
    CHECK(live.elements[0].id != live.elements[1].id);
}

TEST_CASE("object ORDER round-trips, and it is order a maker can see") {
    // Ordering is semantic in this application in four ways -- paint order,
    // which object a click finds where two overlap, the object list, and where
    // the selection lands after a delete. So the file writes document order and
    // never sorts. The proof is the one a maker would notice: two rectangles on
    // top of each other answer a click with the SAME id after a reload.
    WorkshopDoc original;
    const std::int64_t under = doc::add(original, "under", 1, 1, ui::Extent{ui::kExtentCells, 10},
                                        ui::Extent{ui::kExtentCells, 5});
    const std::int64_t over = doc::add(original, "over", 1, 1, ui::Extent{ui::kExtentCells, 10},
                                       ui::Extent{ui::kExtentCells, 5});
    Session s;
    REQUIRE(ui::hit(workspace_scene(original, s), 3, 3)->id == over);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);
    CHECK(live.elements[0].id == under);
    CHECK(live.elements[1].id == over);
    CHECK(ui::hit(workspace_scene(live, s), 3, 3)->id == over);

    // And the negative control: a file whose objects are in the OTHER order is
    // a DIFFERENT document, and the click says so. This is what makes the claim
    // "order is meaning" a measurement rather than an assertion.
    WorkshopDoc swapped = original;
    std::swap(swapped.elements[0], swapped.elements[1]);
    WorkshopDoc other;
    REQUIRE(persist::load_into(other, persist::to_text(swapped)).accepted);
    CHECK(ui::hit(workspace_scene(other, s), 3, 3)->id == under);
}

TEST_CASE("a cells extent stays cells and a share stays a share") {
    // The semantic distinction, preserved even where both happen to resolve to
    // the same number of cells in the workspace that saved them.
    WorkshopDoc original;
    const std::int64_t cells = doc::add(original, "cells", 0, 0, ui::Extent{ui::kExtentCells, 28},
                                        ui::Extent{ui::kExtentCells, 3});
    const std::int64_t share = doc::add(original, "share", 0, 5, ui::Extent{ui::kExtentPercent, 60},
                                        ui::Extent{ui::kExtentCells, 3});
    Session s; // the default 48-cell workspace: 60% of 48 IS 28
    const ui::Scene scene = workspace_scene(original, s);
    REQUIRE(ui::placed_for(scene, cells)->rect.w == ui::placed_for(scene, share)->rect.w);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);
    CHECK(doc::find(live, cells)->width == ui::Extent{ui::kExtentCells, 28});
    CHECK(doc::find(live, share)->width == ui::Extent{ui::kExtentPercent, 60});

    // The file says which is which in words, so a person reading it does not
    // have to know what 0 and 1 mean this week.
    const std::string text = persist::to_text(original);
    CHECK(text.find("\"mode\":\"cells\"") != std::string::npos);
    CHECK(text.find("\"mode\":\"percent\"") != std::string::npos);
}

TEST_CASE("the mint survives, so an identity that died before the save stays dead") {
    // THE PROMPT'S HARD CASE, and the reason `next_id` is in the file at all.
    //
    //   create #1, #2, #3 -> delete #3 -> save -> load -> create
    //
    // must produce #4. A loader that reconstructed the mint the only way it
    // could without this field -- one past the largest surviving id -- would
    // produce #3 again, and a notice, a selection or a half-finished thought
    // still saying "#3" would quietly come to mean a different object.
    WorkshopDoc original;
    CHECK(doc::add_default(original) == 1);
    CHECK(doc::add_default(original) == 2);
    CHECK(doc::add_default(original) == 3);
    REQUIRE(doc::remove(original, 3).accepted);
    REQUIRE(original.next_id == 4);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(original)).accepted);
    CHECK(live.next_id == 4);

    // Stated the way a defect would take, BEFORE creating anything: the largest
    // id that survived the save is 2, and the mint is NOT 3. `max(live)+1` is
    // the only reconstruction available to a loader without this field, and it
    // is exactly the one that recycles a dead identity.
    std::int64_t largest = 0;
    for (const ui::Element& e : live.elements) {
        largest = e.id > largest ? e.id : largest;
    }
    CHECK(largest == 2);
    CHECK(live.next_id != largest + 1);

    const std::int64_t next = doc::add_default(live);
    CHECK(next == 4);
    CHECK(next != 3);
}

TEST_CASE("save -> load -> save is byte-identical") {
    // Canonical serialization, without a canonicalization framework: the writer
    // emits fields in declared schema order and the document's own object order,
    // so the same document is always the same bytes. That is what makes a saved
    // document diffable and archivable.
    const WorkshopDoc original = rich_document();
    const std::string first = persist::to_text(original);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, first).accepted);
    const std::string second = persist::to_text(live);
    CHECK(first == second);

    // And again, so "stable" means stable rather than "the same twice".
    WorkshopDoc again;
    REQUIRE(persist::load_into(again, second).accepted);
    CHECK(persist::to_text(again) == first);
}

TEST_CASE("saving does not touch the document") {
    // Serialization is OBSERVATION. Nothing is renumbered, re-ordered, rounded,
    // clamped or tidied on the way out -- including a value some later rule
    // might not like, because a save that edits the work it was asked to
    // preserve is worse than one that refuses.
    WorkshopDoc d = rich_document();
    const WorkshopDoc before = d;
    const std::string text = persist::to_text(d);
    CHECK(d == before);
    CHECK(d.next_id == before.next_id);
    CHECK(text.size() > 0);
}

TEST_CASE("the file a person can read: identity, version, objects, and the mint") {
    // Legibility is a requirement, not a nicety: a maker owns this file. A
    // technically literate person should recognise every fact in it without
    // reverse-engineering Workshop's memory layout.
    WorkshopDoc d;
    doc::add(d, "sidebar", 3, 2, ui::Extent{ui::kExtentPercent, 60},
             ui::Extent{ui::kExtentCells, 6});
    const std::string text = persist::to_text(d);

    CHECK(text.find("\"format\":\"zengine-workshop\"") != std::string::npos);
    CHECK(text.find("\"format_version\":\"1\"") != std::string::npos);
    CHECK(text.find("\"next_id\":\"2\"") != std::string::npos);
    CHECK(text.find("\"name\":\"sidebar\"") != std::string::npos);
    CHECK(text.find("\"id\":\"1\"") != std::string::npos);
    CHECK(text.find("\"x\":\"3\"") != std::string::npos);
    CHECK(text.find("\"y\":\"2\"") != std::string::npos);
    CHECK(text.find("\"mode\":\"percent\",\"amount\":\"60\"") != std::string::npos);
    // The relationship, written as the identity it is. An ordinary root-context
    // object says `0`, which is not an identity any object can carry.
    CHECK(text.find("\"context\":\"0\"") != std::string::npos);

    // It is real JSON, and it says whose value it is -- the Loom's envelope,
    // which is the claim `admit()` checks. That is a DIFFERENT claim from the
    // `format` field above: one is about the shape of the bytes, the other is
    // about what the document means -- and W-6 is where the two visibly diverge.
    // The SHAPE went to version 2 (an object grew a `context`); `format_version`
    // stayed 1, because the meaning of every field is what it was and there is
    // still exactly one Workshop format in the world.
    CHECK(text.rfind("{\"zen\":1,\"schema\":\"WorkshopDocument\",\"version\":2,", 0) == 0);
}

TEST_CASE("the file carries no resolved geometry, and the scene is rebuilt from what it does") {
    // The claim, in the only form that can actually be checked: none of the
    // resolved vocabulary appears anywhere in the file, and deleting the whole
    // scene and resolving again from the loaded document gives the same live
    // answer -- the same picture, the same hit test.
    const WorkshopDoc original = rich_document();
    const std::string text = persist::to_text(original);

    for (const char* resolved : {"\"rect\"", "\"w\":", "\"h\":", "\"scene\"", "\"resolved\"",
                                 "\"handle\"", "\"viewport\"", "\"workspace\"", "\"selected\"",
                                 "\"pixels\"", "\"cursor\"", "\"drag\""}) {
        CAPTURE(resolved);
        CHECK(text.find(resolved) == std::string::npos);
    }

    Session s;
    const ui::Scene from_original = workspace_scene(original, s);
    WorkshopDoc live;
    REQUIRE(persist::load_into(live, text).accepted);
    const ui::Scene rebuilt = workspace_scene(live, s);
    CHECK(rebuilt == from_original);

    // And the PICTURE, which is the form a maker would notice: every rectangle
    // the original screen showed is on the loaded one, in the same place, at
    // the same size, in the same ink.
    const surface::SurfaceCanvas was = paint(original, s);
    const surface::SurfaceCanvas now = paint(live, s);
    REQUIRE(was.rects.size() == now.rects.size());
    for (const surface::SurfaceRect& r : was.rects) {
        CHECK(has_rect(now, r.x, r.y, r.w, r.h, r.role));
    }
}

TEST_CASE("the same share, loaded into a different workspace, resolves differently") {
    // THE PROOF THAT THE FILE KEPT INTENT AND NOT CELLS. Save under workspace
    // A; load under workspace B. The authored numbers are identical and the
    // resolved ones are not, and that is not a bug -- it is the whole reason
    // authored and resolved are two facts.
    WorkshopDoc original;
    const std::int64_t share = doc::add(original, "share", 0, 0,
                                        ui::Extent{ui::kExtentPercent, 60},
                                        ui::Extent{ui::kExtentCells, 4});
    const std::int64_t fixed = doc::add(original, "fixed", 0, 6, ui::Extent{ui::kExtentCells, 20},
                                        ui::Extent{ui::kExtentCells, 4});

    Session wide;   // 48 cells, the default
    Session narrow; // half of it
    narrow.workspace_w = 24;

    const std::int64_t share_wide = ui::placed_for(workspace_scene(original, wide), share)->rect.w;
    const std::string text = persist::to_text(original);

    WorkshopDoc live;
    REQUIRE(persist::load_into(live, text).accepted);

    // AUTHORED: identical, both of them.
    CHECK(doc::find(live, share)->width == ui::Extent{ui::kExtentPercent, 60});
    CHECK(doc::find(live, fixed)->width == ui::Extent{ui::kExtentCells, 20});

    // RESOLVED: the share moved with the workspace, the cells did not.
    const std::int64_t share_narrow = ui::placed_for(workspace_scene(live, narrow), share)->rect.w;
    CHECK(share_wide == 28);
    CHECK(share_narrow == 14);
    CHECK(share_narrow != share_wide);
    CHECK(ui::placed_for(workspace_scene(live, narrow), fixed)->rect.w == 20);

    // And the file is the same file either way: the workspace is nowhere in it.
    CHECK(persist::to_text(live) == text);
}

TEST_CASE("a save normalizes nothing: 60% is not written as the cells it happens to be") {
    // W-3's no-op stability rule, in its persistence form. At a 48-cell
    // workspace both 59% and 60% resolve to 28 cells, so a loader that
    // "helpfully" canonicalised would be free to pick either. Each is written
    // and read as itself.
    for (const std::int64_t pct : {59, 60}) {
        CAPTURE(pct);
        WorkshopDoc d;
        const std::int64_t id = doc::add(d, "p", 0, 0, ui::Extent{ui::kExtentPercent, pct},
                                         ui::Extent{ui::kExtentCells, 3});
        Session s;
        REQUIRE(ui::placed_for(workspace_scene(d, s), id)->rect.w == 28);

        WorkshopDoc live;
        REQUIRE(persist::load_into(live, persist::to_text(d)).accepted);
        CHECK(doc::find(live, id)->width.mode == ui::kExtentPercent);
        CHECK(doc::find(live, id)->width.amount == pct);
    }
}

// ---- Refusal: every one of these leaves the live document untouched ---------

TEST_CASE("a malformed document never leaves Workshop halfway loaded") {
    // The claim is NOT "the parser returned an error". It is that the document
    // a maker is working on is exactly what it was -- which is the persistence
    // form of the rule the property editor has kept since W-0.
    const WorkshopDoc good = rich_document();
    const std::string valid = persist::to_text(good);

    struct Case {
        const char* what;
        std::string text;
    };
    std::vector<Case> cases;
    cases.push_back({"not JSON at all", "{ this is not a document"});
    cases.push_back({"empty file", ""});
    cases.push_back({"a JSON array", "[1,2,3]"});
    cases.push_back({"someone else's value",
                     loom::compat::serialize(loom::to_value(ui::Extent{0, 4}))});
    cases.push_back({"wrong format identity",
                     forged(good, "\"zengine-workshop\"", "\"someone-elses-editor\"")});
    cases.push_back({"unsupported format version",
                     forged(good, "\"format_version\":\"1\"", "\"format_version\":\"2\"")});
    cases.push_back({"a missing required field", forged(good, "\"next_id\":\"4\",", "")});
    cases.push_back({"a field of the wrong kind",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":4")});
    cases.push_back({"a field the document does not declare",
                     forged(good, "\"next_id\":", "\"colour\":\"red\",\"next_id\":")});
    cases.push_back({"an integer larger than an integer",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":\"99999999999999999999\"")});
    cases.push_back({"two objects with one identity",
                     forged(good, "\"id\":\"2\"", "\"id\":\"1\"")});
    cases.push_back({"a mint that has already been spent",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":\"2\"")});
    cases.push_back({"a mint below the first identity",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":\"0\"")});
    cases.push_back({"a mint at the bottom of the number line",
                     forged(good, "\"next_id\":\"4\"", "\"next_id\":\"-9223372036854775808\"")});
    cases.push_back({"an identity of zero", forged(good, "\"id\":\"1\"", "\"id\":\"0\"")});
    cases.push_back({"a negative identity", forged(good, "\"id\":\"1\"", "\"id\":\"-1\"")});
    cases.push_back({"an extent mode with no meaning",
                     forged(good, "\"mode\":\"percent\"", "\"mode\":\"pixels\"")});
    cases.push_back({"an empty extent mode", forged(good, "\"mode\":\"cells\"", "\"mode\":\"\"")});
    cases.push_back({"a share of more than everything",
                     forged(good, "\"mode\":\"percent\",\"amount\":\"60\"",
                            "\"mode\":\"percent\",\"amount\":\"500\"")});
    cases.push_back({"a share of nothing",
                     forged(good, "\"mode\":\"percent\",\"amount\":\"60\"",
                            "\"mode\":\"percent\",\"amount\":\"0\"")});
    cases.push_back({"a size larger than the document allows",
                     forged(good, "\"mode\":\"cells\",\"amount\":\"6\"",
                            "\"mode\":\"cells\",\"amount\":\"999999\"")});
    cases.push_back({"a size at the top of the number line",
                     forged(good, "\"mode\":\"cells\",\"amount\":\"6\"",
                            "\"mode\":\"cells\",\"amount\":\"9223372036854775807\"")});
    cases.push_back({"a position that does not exist",
                     forged(good, "\"x\":\"3\"", "\"x\":\"-1\"")});
    cases.push_back({"a name that is not a name",
                     forged(good, "\"name\":\"panel\"", "\"name\":\"\"")});
    cases.push_back({"a name longer than a name",
                     forged(good, "\"name\":\"panel\"",
                            "\"name\":\"" + std::string(doc::kMaxNameLen + 1, 'x') + "\"")});
    cases.push_back({"an integer at the bottom of the number line",
                     forged(good, "\"y\":\"2\"", "\"y\":\"-9223372036854775808\"")});

    for (const Case& c : cases) {
        CAPTURE(c.what);
        WorkshopDoc live = good;
        const Written refused = persist::load_into(live, c.text);
        CHECK_FALSE(refused.accepted);
        CHECK_FALSE(refused.refusal.empty()); // a refusal without a reason is not one
        CHECK(live == good);                  // THE claim
    }

    // The control: the unforged text is accepted, so the loop above is not
    // measuring a loader that refuses everything.
    WorkshopDoc live = two_panels();
    CHECK(persist::load_into(live, valid).accepted);
    CHECK(live == good);
}

TEST_CASE("a refusal says which fact was wrong, in words a maker can act on") {
    // Diagnostics matter more once state survives a process: the file is a
    // thing a maker owns and can open, so a refusal that names the field and
    // the object is a refusal they can fix.
    const WorkshopDoc good = rich_document();

    const Written unknown =
        persist::from_text(forged(good, "\"next_id\":", "\"colour\":\"red\",\"next_id\":"))
            .outcome;
    CHECK_FALSE(unknown.accepted);
    CHECK(unknown.refusal.find("colour") != std::string::npos);

    const Written version =
        persist::from_text(forged(good, "\"format_version\":\"1\"", "\"format_version\":\"7\""))
            .outcome;
    CHECK_FALSE(version.accepted);
    CHECK(version.refusal.find("7") != std::string::npos);
    CHECK(version.refusal.find("version 1") != std::string::npos);

    const Written mode =
        persist::from_text(forged(good, "\"mode\":\"percent\"", "\"mode\":\"furlongs\"")).outcome;
    CHECK_FALSE(mode.accepted);
    CHECK(mode.refusal.find("furlongs") != std::string::npos);
    CHECK(mode.refusal.find("percent") != std::string::npos); // and what would have worked

    const Written foreign =
        persist::from_text(forged(good, "\"zengine-workshop\"", "\"blender\"")).outcome;
    CHECK_FALSE(foreign.accepted);
    CHECK(foreign.refusal.find("blender") != std::string::npos);

    // A document-law refusal names the object it is about.
    WorkshopDoc live;
    const Written duplicate =
        persist::load_into(live, forged(good, "\"id\":\"2\"", "\"id\":\"1\""));
    CHECK_FALSE(duplicate.accepted);
    CHECK(duplicate.refusal.find("#1") != std::string::npos);
}

TEST_CASE("the document law is the maker's law, and it is stated in one place") {
    // A loaded document meets the SAME rules a maker's edits meet. The proof is
    // that each per-value refusal below is worded by the very function the
    // interactive path calls -- so the two cannot come to disagree about what a
    // legal extent, coordinate or name is.
    WorkshopDoc d;
    doc::add(d, "p", 0, 0, ui::Extent{ui::kExtentCells, 4}, ui::Extent{ui::kExtentCells, 4});

    WorkshopDoc bad_extent = d;
    bad_extent.elements[0].width = ui::Extent{ui::kExtentPercent, 500};
    CHECK(doc::check_document(bad_extent).refusal ==
          "#1: width: " + doc::check_extent(ui::Extent{ui::kExtentPercent, 500}).refusal);

    WorkshopDoc bad_coord = d;
    bad_coord.elements[0].x = -1;
    CHECK(doc::check_document(bad_coord).refusal ==
          "#1: " + doc::check_coord(-1, ui::kRootContext).refusal);

    WorkshopDoc bad_name = d;
    bad_name.elements[0].label.clear();
    CHECK(doc::check_document(bad_name).refusal == "#1: " + doc::check_name("").refusal);

    // And the two laws that are new -- the ones a maker's path holds by
    // construction and therefore never had to state.
    WorkshopDoc twice = d;
    twice.elements.push_back(twice.elements[0]);
    twice.next_id = 9;
    CHECK_FALSE(doc::check_document(twice).accepted);

    WorkshopDoc behind = d;
    behind.next_id = 1;
    CHECK_FALSE(doc::check_document(behind).accepted);

    CHECK(doc::check_document(d).accepted);
}

TEST_CASE("restore keeps the candidate's identities rather than minting new ones") {
    // The difference between "the same document came back" and "a lookalike was
    // built from it". A loader written on `create` would produce the second and
    // display the first.
    WorkshopDoc live = two_panels(); // ids 1, 2, mint 3
    WorkshopDoc candidate;
    candidate.next_id = 41;
    candidate.elements.push_back(ui::Element{7, "seven", ui::kRootContext, 1, 1,
                                             ui::Extent{ui::kExtentCells, 5},
                                             ui::Extent{ui::kExtentCells, 5}});
    candidate.elements.push_back(ui::Element{40, "forty", ui::kRootContext, 2, 2,
                                             ui::Extent{ui::kExtentCells, 5},
                                             ui::Extent{ui::kExtentCells, 5}});

    REQUIRE(doc::restore(live, candidate).accepted);
    CHECK(live.elements[0].id == 7);
    CHECK(live.elements[1].id == 40);
    CHECK(live.next_id == 41);
    CHECK(doc::add_default(live) == 41);
}

TEST_CASE("the mint can be spent, and creating says so rather than overflowing") {
    // A document arriving from a file can say its mint is at the end of the
    // number line. `next_id++` there is signed overflow -- undefined behaviour
    // produced by data, reachable through an ordinary maker gesture -- so the
    // exhausted mint is an ANSWER. (The sanitizer lane is what would catch the
    // version of this that merely looked fine.)
    WorkshopDoc spent;
    spent.next_id = doc::kMaxIdentity;
    CHECK(doc::check_document(spent).accepted); // it is a legal document
    CHECK_FALSE(doc::can_mint(spent));

    Session s;
    CHECK(create(spent, s) == 0);
    CHECK(spent.elements.empty());
    CHECK(spent.next_id == doc::kMaxIdentity);
    CHECK(s.selected == 0); // a gesture that could not happen moved nothing

    // One below the end still works, and lands exactly on the last identity.
    WorkshopDoc last;
    last.next_id = doc::kMaxIdentity - 1;
    CHECK(doc::add_default(last) == doc::kMaxIdentity - 1);
    CHECK(last.next_id == doc::kMaxIdentity);
    CHECK(doc::add_default(last) == 0);

    // And it survives the file: a spent mint is written and read as a spent mint.
    WorkshopDoc live;
    REQUIRE(persist::load_into(live, persist::to_text(last)).accepted);
    CHECK(live.next_id == doc::kMaxIdentity);
    CHECK_FALSE(doc::can_mint(live));
}

// ---- The file on disk -------------------------------------------------------

TEST_CASE("a document written to a file is the document read back from it") {
    TempDir dir("roundtrip");
    const WorkshopDoc original = rich_document();
    REQUIRE(persist::save_file(dir.document(), original).accepted);

    WorkshopDoc live = two_panels();
    REQUIRE(persist::load_file(dir.document(), live).accepted);
    CHECK(live == original);

    // The bytes on disk are the bytes the writer produced -- no wrapper, no
    // trailer, nothing added by the file layer.
    CHECK(slurp(dir.document()) == persist::to_text(original));

    // And the sibling it was written through is gone.
    CHECK_FALSE(std::filesystem::exists(persist::pending_path(dir.document())));

    // Saving again over an existing document replaces it. Stated as a case
    // because the replace is the platform's `rename` and a platform that
    // refused an existing destination would otherwise fail only on the SECOND
    // save a maker ever performs.
    WorkshopDoc second = original;
    REQUIRE(doc::rename(second, second.elements[0].id, "renamed").accepted);
    REQUIRE(persist::save_file(dir.document(), second).accepted);
    WorkshopDoc reread;
    REQUIRE(persist::load_file(dir.document(), reread).accepted);
    CHECK(reread == second);
}

TEST_CASE("a missing file is an ordinary refusal, not a crash and not an empty document") {
    TempDir dir("missing");
    WorkshopDoc live = two_panels();
    const WorkshopDoc before = live;
    const Written refused = persist::load_file(dir.file("never-written.json"), live);
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal.find("never-written.json") != std::string::npos);
    CHECK(live == before);
}

TEST_CASE("a detected write failure leaves the last good save readable and unchanged") {
    // The reason the writer never opens the destination: a save that fails must
    // not be able to turn a maker's document into an empty or half-written file.
    TempDir dir("failsave");
    const WorkshopDoc first = two_panels();
    REQUIRE(persist::save_file(dir.document(), first).accepted);
    const std::string good_bytes = slurp(dir.document());
    REQUIRE_FALSE(good_bytes.empty());

    // A controlled, deterministic, non-destructive failure: the sibling path the
    // writer must use is occupied by a DIRECTORY, so the write cannot open. No
    // permission games, and the same result on every supported platform.
    std::filesystem::create_directories(persist::pending_path(dir.document()));

    WorkshopDoc second = first;
    REQUIRE(doc::rename(second, second.elements[0].id, "renamed").accepted);
    REQUIRE(doc::add_default(second) != 0);
    REQUIRE_FALSE(second == first);

    const Written refused = persist::save_file(dir.document(), second);
    CHECK_FALSE(refused.accepted);
    CHECK_FALSE(refused.refusal.empty());

    // The last good save is intact, byte for byte, and still loads.
    CHECK(slurp(dir.document()) == good_bytes);
    WorkshopDoc reloaded;
    REQUIRE(persist::load_file(dir.document(), reloaded).accepted);
    CHECK(reloaded == first);

    // And the document in memory is still the one the maker is working on.
    CHECK_FALSE(second == first);
    CHECK(second.elements[0].label == "renamed");

    std::error_code ec;
    std::filesystem::remove_all(persist::pending_path(dir.document()), ec);
    CHECK(persist::save_file(dir.document(), second).accepted); // and it works again
    WorkshopDoc now;
    REQUIRE(persist::load_file(dir.document(), now).accepted);
    CHECK(now == second);
}

TEST_CASE("a save into a place that does not exist refuses before it writes anything") {
    TempDir dir("nowhere");
    const std::string path = (dir.path() / "no-such-directory" / "document.json").string();
    const Written refused = persist::save_file(path, two_panels());
    CHECK_FALSE(refused.accepted);
    CHECK_FALSE(std::filesystem::exists(path));
    CHECK_FALSE(std::filesystem::exists(persist::pending_path(path)));
}

TEST_CASE("a file too large to be a document is refused before it is read") {
    TempDir dir("huge");
    const std::string path = dir.document();
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const std::string chunk(1u << 16, 'x');
        for (int i = 0; i < 80; ++i) { // 5 MiB, past the 4 MiB ceiling
            out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        }
    }
    REQUIRE(std::filesystem::file_size(path) > persist::kMaxDocumentBytes);

    WorkshopDoc live = two_panels();
    const WorkshopDoc before = live;
    const Written refused = persist::load_file(path, live);
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal.find("larger") != std::string::npos);
    CHECK(live == before);
}

// ============================================================================
// Tier 6 — persistence THROUGH THE WEAVE, on a real bus
// ============================================================================
//
// Everything above is about the document and the file. These are about the
// APPLICATION: a maker presses ^s, and what the session does about it.

TEST_CASE("^s saves and ^o loads, through the real message path") {
    TempDir dir("live");
    Live t;
    t.host.document_path = dir.document();

    // Nothing has been saved yet, and the status line says so.
    t.key(input::scan::kN); // republish, so the note reflects the path
    REQUIRE_FALSE(t.notes.empty());
    CHECK(t.notes.back().text.find(dir.document()) != std::string::npos);
    CHECK(t.notes.back().text.find("UNSAVED") != std::string::npos);

    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "saved " + dir.document());
    CHECK(std::filesystem::exists(dir.document()));
    CHECK(t.notes.back().text.find(dir.document() + " saved") != std::string::npos);
    const WorkshopDoc as_saved = t.doc();

    // Change it. The status line notices without anyone setting a flag.
    t.key(input::scan::kL);
    CHECK(t.notes.back().text.find("UNSAVED") != std::string::npos);
    CHECK_FALSE(t.doc() == as_saved);

    // And ^o brings the saved one back.
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.doc() == as_saved);
    CHECK(t.notice() == "loaded " + dir.document() + " -- 3 objects");
    CHECK(t.notes.back().text.find(dir.document() + " saved") != std::string::npos);

    // Editing back to what was saved says `saved` again -- a comparison cannot
    // drift from the thing it describes, and a dirty flag would have said
    // otherwise here.
    t.key(input::scan::kL);
    CHECK(t.notes.back().text.find("UNSAVED") != std::string::npos);
    t.key(input::scan::kH);
    CHECK(t.doc() == as_saved);
    CHECK(t.notes.back().text.find(dir.document() + " saved") != std::string::npos);
}

TEST_CASE("^s refuses while a row is being edited, and writes nothing") {
    // The draft policy. A save that quietly wrote the OLD width while a NEW one
    // is on the screen with a cursor after it would put the file and the
    // maker's eyes in disagreement, with nothing to say so.
    TempDir dir("draft");
    Live t;
    t.host.document_path = dir.document();

    t.begin_editing("Width");
    REQUIRE(t.row("Width")->editing());
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text("7");

    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "Width is still being edited -- enter commits, esc cancels; "
                        "nothing was saved");
    CHECK(t.session().notice_is_bad);
    CHECK_FALSE(std::filesystem::exists(dir.document())); // nothing was written
    CHECK(t.row("Width")->editing());                     // and the draft is intact
    CHECK(t.row("Width")->draft() == "7");

    // Cancel and it saves. (Commit would too; the point is that the maker says
    // which, rather than the save deciding for them.)
    t.key(input::scan::kEscape);
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "saved " + dir.document());
    CHECK(std::filesystem::exists(dir.document()));
}

TEST_CASE("a successful load cancels a drag and cannot continue an old resize") {
    // No dangling reference may survive a document replacement. A pointer
    // already down held an identity and an offset from an object that is gone.
    TempDir dir("drag");
    Live t;
    t.host.document_path = dir.document();
    t.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE(t.notice().rfind("saved", 0) == 0);

    // Take hold of the second object's body and start moving it.
    //
    // The coordinates are copied out as NUMBERS and not held as a pointer into
    // the document, and that is this phase's own hazard rather than style: a
    // load REPLACES the element vector, so every pointer into it dangles the
    // instant the load succeeds. The first draft of this case kept
    // `const ui::Element*` across the load and the sanitizer lane called it a
    // heap-use-after-free while the ordinary lane passed -- the third time that
    // pairing has paid for itself here (W-2, W-3, now W-5).
    const std::int64_t held_id = t.second()->id;
    const std::int64_t held_x = t.second()->x;
    const std::int64_t held_y = t.second()->y;
    t.press(held_x + 1, held_y + 1);
    REQUIRE(t.session().drag.active);
    REQUIRE(t.session().drag.id == held_id);
    REQUIRE_FALSE(t.session().drag.resizing);

    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.session().drag.id == 0);
    CHECK(t.notice().rfind("loaded", 0) == 0);

    // The pointer is still down; moving it now must author nothing.
    const WorkshopDoc after_load = t.doc();
    t.motion(held_x + 20, held_y + 20);
    CHECK(t.doc() == after_load);

    // The same, for a resize: take the handle, then load.
    const Handle handle = size_handle(t.doc(), t.session());
    REQUIRE(handle.shown);
    t.press(handle.x, handle.y);
    REQUIRE(t.session().drag.resizing);
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK_FALSE(t.session().drag.active);
    CHECK_FALSE(t.session().drag.resizing);
    const WorkshopDoc after_second_load = t.doc();
    t.motion(handle.x + 6, handle.y + 6);
    CHECK(t.doc() == after_second_load);
}

TEST_CASE("selection after a load is re-established, never inherited") {
    // A loaded document is a DIFFERENT document. Keeping the old selected id
    // would silently alias whatever new object happened to carry that number --
    // the identity confusion the whole arc is arranged to prevent, arriving
    // through the back door. So the selection is set by the rule that opens a
    // fresh Workshop: the first object.
    TempDir dir("selection");
    Live t;
    t.host.document_path = dir.document();
    t.key(input::scan::kS, input::mod::kCtrl);

    // Select the SECOND object, so "kept" and "re-established" differ.
    t.key(input::scan::kTab);
    const std::int64_t was = t.session().selected;
    REQUIRE(was == t.second()->id);
    REQUIRE(was != t.first()->id);

    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().selected == t.first()->id);
    CHECK(t.session().selected != was);
    CHECK(t.row("Identity")->value() == "#" + std::to_string(t.first()->id));

    // A load into an empty document selects nothing, and the screen says so
    // rather than going blank.
    WorkshopDoc empty;
    spillout(dir.document(), persist::to_text(empty));
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.doc().elements.empty());
    CHECK(t.session().selected == 0);
    CHECK(t.session().rows.empty());
    CHECK(label_at(t.canvases.back(), kPanelX, kListY) == "(none) -- n makes one");
}

TEST_CASE("a failed load costs a maker nothing but the notice") {
    // Failure must not destroy valid session state. The document, the
    // selection, the cursor and any draft are exactly what they were.
    TempDir dir("failload");
    Live t;
    t.host.document_path = dir.document();
    t.key(input::scan::kN); // make a third object, so the document is the maker's
    t.key(input::scan::kTab);

    const WorkshopDoc before_doc = t.doc();
    const std::int64_t before_selected = t.session().selected;
    const std::size_t before_cursor = t.session().cursor;

    // No file at all.
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.doc() == before_doc);
    CHECK(t.session().selected == before_selected);
    CHECK(t.session().cursor == before_cursor);

    // A file that is not a document.
    spillout(dir.document(), "{\"zen\":1,\"schema\":\"Nope\"");
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.doc() == before_doc);
    CHECK(t.session().selected == before_selected);

    // A real Workshop document with one illegal fact in it.
    WorkshopDoc bad = before_doc;
    bad.elements[0].x = -1;
    spillout(dir.document(), persist::to_text(bad));
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("#" + std::to_string(before_doc.elements[0].id)) != std::string::npos);
    CHECK(t.doc() == before_doc);
    CHECK(t.session().selected == before_selected);

    // And the good one still loads, so the three refusals above are not a
    // Workshop that had stopped loading anything.
    spillout(dir.document(), persist::to_text(before_doc));
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.doc() == before_doc);
}

TEST_CASE("with no document file, save and open say so instead of guessing one") {
    Live t; // host.document_path left empty, as a suite-mounted weave has it
    const WorkshopDoc before = t.doc();

    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("--document") != std::string::npos);

    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("--document") != std::string::npos);
    CHECK(t.doc() == before);

    // And the status line does not claim a file it does not have.
    CHECK(t.notes.back().text.find("saved") == std::string::npos);
    CHECK(t.notes.back().text.find("UNSAVED") == std::string::npos);
}

TEST_CASE("a bare s and a bare o are not commands, and Ctrl is what makes them one") {
    // The same shape as W-4's Ctrl+C case: the modifier is READ, not implied by
    // the key. A bare `o` and a bare `s` do nothing at all, so nothing can come
    // to depend on them.
    TempDir dir("modifier");
    Live t;
    t.host.document_path = dir.document();
    const WorkshopDoc before = t.doc();

    t.key(input::scan::kS);
    CHECK_FALSE(std::filesystem::exists(dir.document()));
    t.key(input::scan::kO);
    CHECK(t.doc() == before);

    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(std::filesystem::exists(dir.document()));
}

TEST_CASE("the whole cross-process story, in one session") {
    // The headless twin of the live witness in the report: author, save, lose
    // the process, come back, and get the work -- identities and all -- while
    // the mint refuses to rewind.
    TempDir dir("crossprocess");

    std::string on_disk;
    std::int64_t doomed = 0;
    {
        Live run_a;
        run_a.host.document_path = dir.document();
        run_a.key(input::scan::kN); // #3
        doomed = run_a.session().selected;
        run_a.key(input::scan::kL); // move it
        run_a.key(input::scan::kL, input::mod::kShift);
        run_a.key(input::scan::kD); // and delete it again
        REQUIRE(run_a.doc().elements.size() == 2);
        REQUIRE(run_a.doc().next_id == doomed + 1);
        run_a.key(input::scan::kS, input::mod::kCtrl);
        REQUIRE(run_a.notice().rfind("saved", 0) == 0);
        on_disk = slurp(dir.document());
    } // run A is gone, with its bus, its weave and its whole session

    REQUIRE_FALSE(on_disk.empty());

    Live run_b; // a fresh process: its own opening document, its own session
    run_b.host.document_path = dir.document();
    REQUIRE_FALSE(run_b.doc().next_id == doomed + 1);

    run_b.key(input::scan::kO, input::mod::kCtrl);
    CHECK(run_b.doc().elements.size() == 2);
    CHECK(run_b.doc().next_id == doomed + 1);
    CHECK(run_b.doc().elements[0].width == ui::Extent{ui::kExtentPercent, 60});
    CHECK(run_b.doc().elements[1].width == ui::Extent{ui::kExtentCells, 14});

    // The identity that died before the save stays dead.
    run_b.key(input::scan::kN);
    CHECK(run_b.session().selected == doomed + 1);
    CHECK(run_b.session().selected != doomed);

    // And what run B saves is what run A saved, plus exactly the new object.
    run_b.key(input::scan::kD);
    run_b.key(input::scan::kS, input::mod::kCtrl);
    CHECK(slurp(dir.document()) != on_disk); // the mint moved, and that is a fact
    WorkshopDoc back;
    REQUIRE(persist::load_file(dir.document(), back).accepted);
    CHECK(back.elements.size() == 2);
    CHECK(back.next_id == doomed + 2);
}

// ============================================================================
// Tier 7 — composition: one authored object read in another's frame (W-6)
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
//      span, through the same projection W-3 built.
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
/// returned was read out of freed memory. It is the same defect W-5 shipped in
/// committed test code and the sanitizer lane found; here the ordinary lane
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
        CHECK(std::string("Context: " + no.refusal).size() <= static_cast<std::size_t>(kScreenW));
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
        CHECK(std::string("Context: " + no.refusal).size() <= static_cast<std::size_t>(kScreenW));
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
    // W-6 asked whether "the workspace starts at 0" was a law about coordinates
    // or a law about the workspace. Its own stated reason answered it.
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
    CHECK(label_at(canvas, kPanelX, kListY) == "  #" + std::to_string(c) + " C");
    CHECK(label_at(canvas, kPanelX, kListY + 1) == "> #" + std::to_string(a) + " A");
    CHECK(label_at(canvas, kPanelX, kListY + 2) == "  #" + std::to_string(b) + " B");

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

    // A ROOT object meets the same wall and authors 0, exactly as before W-6.
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
    // W-3's projection, handed the right span. There is no second projection and
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
    // W-6 changed the written shape, so a W-5 document no longer admits. That is
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
