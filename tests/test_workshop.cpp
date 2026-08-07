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
// Three tiers:
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
#include "workshop/property.hpp"
#include "workshop/screen.hpp"
#include "workshop/weave.hpp"
#include "workshop/vocabulary.hpp"

#include "surface/skin_tui.hpp"
#include "surface/vocabulary.hpp"
#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <zen/schema.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <memory>
#include <limits>
#include <string>
#include <vector>

using namespace zengine::workshop;
using loom::schema_of;
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
    const auto element = SchemaBuilder("Element", 1)
                             .field("id", Kind::Int)
                             .field("label", Kind::Text)
                             .field("x", Kind::Int)
                             .field("y", Kind::Int)
                             .message("width", extent)
                             .message("height", extent)
                             .build();

    const auto document = SchemaBuilder("WorkshopDoc", 1)
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
    REQUIRE(s.rows.size() == 7);

    // The identity row and the resolved row are `show`s; every property is an
    // `edit`. The structural answer to "never present a resolved value as though
    // it were the authored property": begin() on one does nothing at all.
    CHECK(s.rows[0].label() == "Identity");
    CHECK_FALSE(s.rows[0].editable());
    CHECK(s.rows[6].label() == "Resolved");
    CHECK_FALSE(s.rows[6].editable());
    for (std::size_t i = 1; i <= 5; ++i) {
        CHECK(s.rows[i].editable());
    }

    s.rows[6].begin();
    CHECK_FALSE(s.rows[6].editing());
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
    CHECK(s.rows[4].value() == "50%");       // the authored property
    CHECK(s.rows[6].value() == "24 x 4 cells"); // what this workspace makes of it

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
    CHECK(s.rows[4].value() == "50%");
    CHECK(s.rows[6].value() == "12 x 4 cells");
    CHECK(d.elements[0].width == ui::Extent{ui::kExtentPercent, 50});

    // A cells extent's two facts COINCIDE, which is not the same as being one
    // fact -- the inspector still reports them as two rows.
    CHECK(s.rows[5].value() == "4");
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

    CHECK(doc::check_coord(e->x).accepted);
    CHECK(doc::check_coord(e->y).accepted);
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
    REQUIRE(s.rows.size() == 7); // a selection survived, so the inspector has a subject
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
    REQUIRE(s.rows.size() == 7);
    CHECK(s.rows[1].label() == "Name");
    CHECK(s.rows[4].label() == "Width");

    Row& name = s.rows[1];
    name.begin();
    while (!name.draft().empty()) {
        name.backspace();
    }
    type_all(name, "mine");
    CHECK(name.commit() == Commit::Accepted);
    CHECK(doc::find(d, made)->label == "mine");

    Row& width = s.rows[4];
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
    REQUIRE(s.rows.size() == 7);
    CHECK(s.rows[2].value() == "4"); // X
    CHECK(s.rows[3].value() == "3"); // Y
    CHECK(s.rows[6].value() == "24 x 4 cells"); // resolved size unchanged by a move

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
    REQUIRE(s.rows.size() == 7);

    // A typed X of -1, through the Row the inspector actually holds.
    Row& x_row = s.rows[2];
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
    CHECK(workspace_cell_y(kCanvasTopRow + kWorkspaceY) == 0);

    // The saturating ends, which is where a plain subtraction would be UB.
    CHECK(workspace_cell_y(kMin) == kMin);
    CHECK(workspace_cell_x(kMin) == kMin);
    CHECK(workspace_cell_y(kMax) == kMax - kCanvasTopRow - kWorkspaceY);

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
    // The canvas starts at terminal row 3 and the workspace one row into the
    // canvas, so a pointer on terminal row 3 is workspace row -1 and terminal row
    // 4 is workspace row 0. An off-by-one here would select the object above the
    // one a maker clicked, which is the kind of wrong that looks like a bug in
    // the hit test.
    CHECK(workspace_cell_x(0) == kWorkspaceX);
    CHECK(workspace_cell_y(kCanvasTopRow + kWorkspaceY) == 0);
    CHECK(workspace_cell_y(kCanvasTopRow) == -kWorkspaceY);

    WorkshopDoc d;
    const std::int64_t id = doc::add(d, "one", 3, 2, ui::Extent{ui::kExtentCells, 4},
                                     ui::Extent{ui::kExtentCells, 4});
    Session s;
    s.selected = id;
    // The object's authored top-left (3,2) is terminal (3, 2+1+2) = (3, 5).
    CHECK(begin_drag(d, s, workspace_cell_x(3), workspace_cell_y(5)) == id);
    CHECK(s.drag.grab_dx == 0);
    CHECK(s.drag.grab_dy == 0);
    end_drag(s);
    CHECK(begin_drag(d, s, workspace_cell_x(3), workspace_cell_y(4)) == 0); // one row above
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
        REQUIRE(s.rows.size() == 7);
        // canvas: the fill and the ring are where the scene put it
        CHECK(has_rect(c, kWorkspaceX + placed->rect.x, kWorkspaceY + placed->rect.y,
                       placed->rect.w, placed->rect.h, surface::role::kFill));
        CHECK(has_rect(c, kWorkspaceX + placed->rect.x - 1, kWorkspaceY + placed->rect.y - 1,
                       placed->rect.w + 2, placed->rect.h + 2, surface::role::kAccent));
        // list: the marker is on this identity
        CHECK(label_at(c, kPanelX, kListY + row).rfind("> #" + std::to_string(id), 0) == 0);
        // inspector: the same identity, and the authored position it is drawn at
        CHECK(s.rows[0].value() == "#" + std::to_string(id));
        CHECK(s.rows[2].value() == std::to_string(placed->rect.x));
        CHECK(s.rows[3].value() == std::to_string(placed->rect.y));
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
    REQUIRE(s.rows.size() == 7);
    CHECK(s.rows[4].value() == "17");           // authored Width
    CHECK(s.rows[5].value() == "6");            // authored Height
    CHECK(s.rows[6].value() == "17 x 6 cells"); // and what the workspace makes of it

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
    REQUIRE(s.rows.size() == 7);

    // A typed Width of 0, through the Row the inspector actually holds.
    Row& width = s.rows[4];
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
    CHECK(s.rows[4].value() == TextForm<ui::Extent>::format(doc::find(d, id)->width));
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
    const Handled far = size_to(d, s, id, 400, 4);
    CHECK(far.accepted());
    CHECK(far.clamped());
    CHECK(far.boundary == kAtWholeWorkspace);
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
    CHECK(grow(d, s, +5, 0).boundary == kAtWholeWorkspace);
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
    const Handle grip = size_handle(d, s);
    REQUIRE(grip.shown);
    CHECK(label_at(paint(d, s), kWorkspaceX + grip.x, kWorkspaceY + grip.y) == kHandleGlyph);

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
    CHECK(label_at(c, kPanelX, kRowsY + 4) == " Width    60%");
    CHECK(label_at(c, kPanelX, kRowsY + 6) == " Resolved 28 x 6 cells");
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
    CHECK(label_at(c, kPanelX, kRowsY + 4) == " Width    14");
    // #2's width is authored in CELLS, so its authored and resolved facts
    // coincide -- which the inspector still reports as two rows, because
    // coinciding is not the same as being one fact.
    CHECK(label_at(c, kPanelX, kRowsY + 6) == " Resolved 14 x 4 cells");
}

TEST_CASE("a live draft is visible AS a draft, and a refusal reaches the screen") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);

    s.cursor = 4; // Width
    s.rows[4].begin();
    while (!s.rows[4].draft().empty()) {
        s.rows[4].backspace();
    }
    type_all(s.rows[4], "500p");

    const surface::SurfaceCanvas drafting = paint(d, s);
    // The row carries a cursor and the alert role: a draft cannot be mistaken
    // for a committed value on the screen either.
    CHECK(label_at(drafting, kPanelX, kRowsY + 4) == ">Width    500p_");
    for (const surface::SurfaceLabel& l : drafting.labels) {
        if (l.y == kRowsY + 4) {
            CHECK(l.role == surface::role::kAlert);
        }
    }
    // And the resolved row still reports the COMMITTED width, not the draft.
    CHECK(label_at(drafting, kPanelX, kRowsY + 6) == " Resolved 28 x 6 cells");

    CHECK(s.rows[4].commit() == Commit::Refused);
    s.notice = "Width: " + s.rows[4].refusal();
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
    CHECK(rows[12].find("Width    60%") != std::string::npos);
    CHECK(rows[14].find("Resolved 28 x 6 cells") != std::string::npos);
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

namespace input = zengine::input;

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
    static std::int64_t term_y(std::int64_t wy) { return wy + kWorkspaceY + kCanvasTopRow; }

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
    // `space` earning its field. A backend reporting pixels is not talking about
    // anything this document has, and guessing an equivalence is exactly the
    // mistake the field exists to prevent.
    Live t;
    const std::int64_t id2 = t.second()->id;
    t.press(t.second()->x, t.second()->y);
    t.release(t.second()->x, t.second()->y);
    REQUIRE(t.session().selected == id2);

    const std::int64_t before_x = t.second()->x;
    t.publish(loom::to_value(input::PointerButton{1, true, Live::term_x(t.second()->x),
                                                  Live::term_y(t.second()->y),
                                                  input::space::kPixels, input::mod::kNone}));
    CHECK_FALSE(t.session().drag.active); // no grab from a space we do not speak
    t.publish(loom::to_value(input::PointerMoved{9999, 9999, 0, 0, input::space::kPixels,
                                                  input::mod::kNone}));
    CHECK(t.second()->x == before_x);
    // An unstamped event -- the default -- is likewise not cells.
    t.publish(loom::to_value(input::PointerButton{1, true, 0, 0, input::space::kUnknown,
                                                  input::mod::kNone}));
    CHECK_FALSE(t.session().drag.active);
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
