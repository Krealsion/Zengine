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
#include "workshop/vocabulary.hpp"

#include "surface/skin_tui.hpp"
#include "surface/vocabulary.hpp"
#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <zen/schema.hpp>
#include <zen/weave.hpp>

#include <cstdint>
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

TEST_CASE("the whole screen survives an empty document without inventing anything") {
    WorkshopDoc d;
    Session s;
    refocus(d, s);
    CHECK(s.rows.empty()); // nothing selected, so there are no properties to show

    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(c.width == kScreenW);
    CHECK(label_at(c, kPanelX, kListY).empty());
    CHECK(label_at(c, kPanelX, kRowsY).empty());
    // The workspace and the two headings are still there: an empty document is a
    // document, and the tool does not disappear with it.
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, kWorkspaceW, kWorkspaceH, surface::role::kMuted));
    CHECK(label_at(c, kPanelX, kListY - 1) == "OBJECTS");
    CHECK(label_at(c, kPanelX, kRowsY - 1) == "PROPERTIES");
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
