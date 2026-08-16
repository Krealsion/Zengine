// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop suite — the property connection, the document's refusals, the
// selection mechanism, and one whole screen.
//
// Everything here is headless and pure. That is not a limitation of the suite,
// it is a property of the design: the authored objects are plain data, the
// operations on them are functions that can refuse, the inspector is a list of
// values, and the screen is a SurfaceCanvas returned by a function. Nothing in
// Workshop's own logic needs a terminal, so nothing here has one.
//
// TWO OF THE TIERS ARE PERSISTENCE AND ONE OF THEM TOUCHES A DISK. Tier 5 is the
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
// THE GEOMETRY CLAIMS HERE ARE INTEGRATION CLAIMS. Resolution and hit testing
// belong to the UI package and are proven in the `ui` suite; what these cases
// prove is that Workshop's answers COME from there — that the painted rectangle,
// the inspector's resolved reading and the reply to a click are all derived from
// one ui::Scene. Three separate call sites would have that property only by one
// person having written all three.
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
#include "workshop/setup.hpp"
#include "workshop/setup_persist.hpp"
#include "workshop/weave.hpp"
#include "workshop/vocabulary.hpp"

#include "surface/skin_sdl_plan.hpp"
#include "surface/skin_tui.hpp"
#include "surface/vocabulary.hpp"
#include "ui/layout.hpp"
#include "ui/vocabulary.hpp"

#include <zen/host/terminal_wiring.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/terminal/input_lex.hpp>
#include <zen/terminal/session.hpp>
#include <zen/terminal/transcript.hpp>
#include <zen/terminal/vocabulary.hpp>
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
namespace component = zengine::component;
namespace input = zengine::input;
namespace surface = zengine::surface;
namespace ui = zengine::ui;

namespace {

/// A `component::TextBox` holding a text of this LENGTH, with its caret and its window where
/// a case wants them — the fixture the pane's geometry helpers take since HD-5 moved those
/// two indices inside the component they belong to.
///
/// THE WINDOW IS REACHED THROUGH THE REAL DOOR, never written. `keep_caret_visible(caret -
/// first)` is the only way to move it, and it lands exactly on `first` for every value a case
/// can ask for: rule 1 caps the window at `size - room`, which is `first` itself here, and
/// rule 3 pulls it up to the same place. A window past the end of the text (`first > length`)
/// collapses to the end, which is the answer the pre-HD-5 helpers reached by clamping.
component::TextBox box_of(std::size_t length, std::size_t caret, std::size_t first) {
    component::TextBox b;
    b.set(std::string(length, 'x'), caret);
    b.keep_caret_visible(caret > first ? static_cast<std::int64_t>(caret - first) : 0);
    return b;
}

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
/// The fixture that was missing for a long time, and the reason a whole class of
/// panel defect survived: every live run and every screen case used two objects,
/// and the 300- and 500-link documents were exercised headlessly, where nothing
/// paints. More objects than the OBJECTS panel is tall is the shape those cases
/// could not express.
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

/// EVERYTHING ON A CANVAS THAT A CELL MEDIUM WOULD SHOW AS TEXT, in painter's
/// order -- the labels, and then the text regions projected onto cells.
///
/// Since HD-1 a canvas may carry a bounded region whose interior a graphical
/// medium sets in real type. Every assertion in this file that asks "what would a
/// maker see at cell (x, y)" is asking the CELL question, so it goes through the
/// same projection the terminal Skins use (`surface::project_text_regions`) rather
/// than through a second reading of the region invented here. That is deliberate
/// rather than convenient: if the projection ever stopped agreeing with what a
/// character medium draws, these assertions would be describing a picture nothing
/// paints -- and it is the picture the golden-byte suite pins in test_surface.cpp.
///
/// Regions come LAST because a region is the topmost thing on a canvas.
std::vector<surface::SurfaceLabel> cell_text_of(const surface::SurfaceCanvas& c) {
    std::vector<surface::SurfaceLabel> out = c.labels;
    for (surface::ProjectedRow& p : surface::project_text_regions(c)) {
        out.push_back(std::move(p.label));
    }
    return out;
}

/// Find a label's text at a canvas cell, or "" -- how the screen tier asks what
/// a maker would see at a place.
std::string label_at(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == x && l.y == y) {
            return l.text;
        }
    }
    return {};
}

/// ONE PROSE ROW OF THE INSPECTOR'S PROPERTY BODY, as a maker reads it (HD-6).
///
/// The body is a bounded REGION since HD-6, and a region owns what is inside its bounds --
/// so its cell projection pads every row to the region's full width, exactly as the terminal
/// pane's has always done. That padding is a real fact about the picture (it is what erases
/// what was underneath) and it is noise in an assertion about what a row SAYS, so this trims
/// it and the cases below read the way they read before the body had bounds.
///
/// It goes through `label_at`, which goes through the real cell projection, so a caret is
/// still inserted at its own column and a row cut at the body's width is still cut.
std::string inspector_row(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
    std::string text = label_at(c, x, y);
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

/// THE TERMINAL PANE'S REGION on a canvas, found by the place it was drawn at.
///
/// BY PLACE, NOT BY POSITION (HD-6). `texts[0]` was the pane for as long as the pane was the
/// only region this application published; the Inspector's property body is published on
/// every paint since HD-6 and is painted BEFORE the overlay, so an index now names the
/// Inspector. That is the same defect a second copy of any geometry is, arriving in a test
/// helper: it would have gone on passing while asserting things about the wrong rectangle.
///
/// IT RETURNS A POINTER and the call sites dereference it, which is not style: GCC 13's
/// `-Wdangling-reference` fires on a function returning a REFERENCE when any argument is a
/// temporary, and `screen_of(t.session())` is one. The reference would have been perfectly
/// alive -- it points into the canvas, not into the Screen -- but a heuristic that cannot know
/// that is `-Werror` on the MinGW lane, and a pointer says the same thing without arguing.
const surface::SurfaceTextRegion* pane_of(const surface::SurfaceCanvas& c, const Screen& sc) {
    for (const surface::SurfaceTextRegion& r : c.texts) {
        if (r.x == sc.terminal_x && r.y == sc.terminal_y) {
            return &r;
        }
    }
    FAIL("no terminal pane region on this canvas");
    return nullptr;
}

/// The completion list on a canvas, or nullptr — the region that is neither the pane nor
/// the Inspector's property body.
///
/// BY PLACE, NOT BY POSITION (HD-6). It used to be "the second region", which was true for
/// exactly as long as the pane was the only other one; since HD-6 the Inspector publishes
/// its property body on every paint, so an index would name the wrong region on every canvas
/// this file paints. The pane's own x is `Screen::terminal_x` and the list sits in the same
/// column above it, so "not the pane's top row" identifies it without a second arithmetic.
const surface::SurfaceTextRegion* list_of(const surface::SurfaceCanvas& c, const Screen& sc) {
    for (const surface::SurfaceTextRegion& r : c.texts) {
        if (r.x == sc.terminal_x && r.y != sc.terminal_y) {
            return &r;
        }
    }
    return nullptr;
}

/// What is actually SEEN at a cell where several labels landed: the LAST one
/// written, because painter's order is list order and every Skin draws it that
/// way. `label_at` above answers with the first, which is the bottom of the
/// stack -- fine everywhere nothing overlaps, and exactly wrong for asking
/// whether an overlay covered what is under it.
std::string topmost_at(const surface::SurfaceCanvas& c, std::int64_t x, std::int64_t y) {
    std::string seen;
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == x && l.y == y) {
            seen = l.text;
        }
    }
    return seen;
}

/// THE INFO PANEL'S BODY, resolved the way the painter resolves it — through `bounds_of` and
/// `info_body_place`, never through a second arithmetic (HD-6, widened by HD-7). A case that
/// computed the rectangle or the row of a heading for itself would pass while the picture and
/// the hit test disagreed.
InfoBodyPlace body_of(const WorkshopDoc& d, const Session& s) {
    const Screen sc = screen_of(s);
    return info_body_place(bounds_of(s.panels, panel::kInfo, sc).rect, sc, d, s);
}

/// The body region a canvas actually published, or nullptr.
const surface::SurfaceTextRegion* body_on(const surface::SurfaceCanvas& c,
                                          const InfoBodyPlace& p) {
    for (const surface::SurfaceTextRegion& r : c.texts) {
        if (r.x == p.region_x && r.y == p.region_y) {
            return &r;
        }
    }
    return nullptr;
}

/// ONE OBJECT ROW OF THE BODY, as a maker reads it in a CELL medium (HD-7).
///
/// A prose row of the body is a cell row when the medium has no type, so this is the same
/// arithmetic `project_text_regions` performs and no second copy of it.
std::string object_row(const surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                       std::int64_t n) {
    const InfoBodyPlace p = body_of(d, s);
    return inspector_row(c, p.region_x, p.region_y + n);
}

/// ONE PROPERTY ROW OF THE BODY, and the row `PROPERTIES` itself sits on (HD-7).
///
/// RESOLVED, NEVER ADDED TO A CONSTANT. `kRowsY = 8` used to be the property body's first row
/// and these cases used to say `kRowsY + n`; the heading now moves with the object list above
/// it, so a case that kept a constant would be asserting about a row nobody drew.
std::string property_row(const surface::SurfaceCanvas& c, const WorkshopDoc& d, const Session& s,
                         std::int64_t n) {
    const InfoBodyPlace p = body_of(d, s);
    return inspector_row(c, p.region_x, p.region_y + p.heading_row + 1 + n);
}

std::string properties_heading(const surface::SurfaceCanvas& c, const WorkshopDoc& d,
                               const Session& s) {
    const InfoBodyPlace p = body_of(d, s);
    return inspector_row(c, p.region_x, p.region_y + p.heading_row);
}

/// The OBJECTS list exactly as a maker reads it: every row of its share of the body, in
/// order, including the ones that are empty.
std::vector<std::string> object_lines(const surface::SurfaceCanvas& c, const WorkshopDoc& d,
                                      const Session& s) {
    const InfoBodyPlace p = body_of(d, s);
    std::vector<std::string> lines;
    for (std::size_t i = 0; i < p.objects_rows; ++i) {
        lines.push_back(inspector_row(c, p.region_x, p.region_y + static_cast<std::int64_t>(i)));
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

    // Version 2, and the document's own two fields have never changed: what
    // changed is the element inside it, and a content-id is over the WHOLE
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
    // ...and it is shown AS a draft. Since HD-5 that is not a `_` this row appends: the
    // insertion point is a published fact about the region the row is drawn in, and the
    // caret is where it says the next keystroke lands rather than always at the end.
    CHECK(row.display() == "banana");
    CHECK(row.editor().caret() == 6);         // the end, because that is where typing left it
    CHECK(row.editor().text() == "banana");
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
    CHECK(row.display() == "500%");
    // AND THE COMPONENT SURVIVES WITH IT (HD-5). A refused commit leaves the maker looking at
    // what they typed AND at where they were typing it, which is the half a caret adds: a
    // draft preserved with its insertion point thrown away would have to be re-navigated.
    CHECK(row.editor().caret() == 4);
    CHECK(row.editor().first_visible() == 0);
    row.left();
    row.left();
    CHECK(row.editor().caret() == 2); // still an editor, not a preserved string
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
    CHECK(object_row(c, d, s, 1) == "> #" + std::to_string(front) + " front");
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
// Tier 2d — the maker's own hands: create, move, delete
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
    CHECK(object_row(c, d, s, 2) == "> #" + std::to_string(made) + " panel");
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
    CHECK(object_row(c, d, s, 0) == "> #" + std::to_string(kept) + " panel");
    // HD-7: one object is a one-row share, so there is no second list row to go stale in --
    // what follows it is the heading the composition put there.
    CHECK(body_of(d, s).objects_rows == 1);
    CHECK(properties_heading(c, d, s) == "PROPERTIES");
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

    // A nudge reaches the same operation, but it is a HAND, so the boundary
    // policy applies before the proposal is made: it stops at the workspace edge
    // and SLIDES along it rather than refusing, and it says which wall it met.
    // The other coordinate still moves -- that is the whole difference a clamp
    // buys, and the reason the two acts are worth telling apart.
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
    // hazard `Placed` carries an id against -- met here, in the test that proves
    // the design.
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
    // still moving down. Refusing this outright and leaving the object put would
    // be truthful and brusque; the boundary policy stops the HAND at the wall
    // instead: the proposal is reduced to the first cell the workspace has,
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
    // cell is not representable. (The same widening `resolve_extent` meets one
    // layer down.)
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
    // most positive one. Under the boundary policy the gesture then does what it
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
    // Coordinates are int64 cells and not doubles, so one hazard --
    // `static_cast<int64_t>` of a double that does not fit, which is UNDEFINED --
    // is gone at the type. The REMAINING hazard is the one the type cannot
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
    // The projection question, asserted as the only thing that actually matters:
    // a maker pointing at the middle of an object hits THAT object, and which
    // medium they were looking through is not the document's business
    // (docs/reference/pointer-spaces.md).
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
        CHECK(object_row(c, d, s, row).rfind("> #" + std::to_string(id), 0) == 0);
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
// Tier 2e — the maker's hand on an EXTENT: resize
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
    // refusal message beside a successful write, which is exactly what placement
    // refuses to produce, met here at the other property.
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
    // The distinction is not decoration: after a
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
    // simply clips, exactly as an object may be positioned past the edge.
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
    // it, so it can only ever agree with it: a canary that changed
    // `kHandleGlyph "+" -> "*"` left this whole suite GREEN, because both
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
    CHECK(c.width == kMinScreen.w);
    CHECK(c.height == kMinScreen.h);

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
    CHECK(object_row(c, d, s, 0) == "> #1 panel");
    CHECK(object_row(c, d, s, 1) == "  #2 panel");

    // The inspector reads the real object's real properties. The cursor sits on
    // the first row a maker can author, which is Name, not Identity.
    CHECK(s.cursor == 1);
    CHECK(property_row(c, d, s, 0) == " Identity #1");
    CHECK(property_row(c, d, s, 1) == ">Name     panel");
    CHECK(property_row(c, d, s, 5) == " Width    60%");
    CHECK(property_row(c, d, s, 7) == " Resolved 28 x 6 cells");
}

TEST_CASE("selecting the other object moves the ring, the list marker, and the inspector") {
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[1].id;
    refocus(d, s);

    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(has_rect(c, 5, kWorkspaceY + 9, 16, 6, surface::role::kAccent));
    CHECK_FALSE(has_rect(c, 2, kWorkspaceY + 1, 30, 8, surface::role::kAccent));
    CHECK(object_row(c, d, s, 0) == "  #1 panel");
    CHECK(object_row(c, d, s, 1) == "> #2 panel");
    CHECK(property_row(c, d, s, 0) == " Identity #2");
    CHECK(property_row(c, d, s, 5) == " Width    14");
    // #2's width is authored in CELLS, so its authored and resolved facts
    // coincide -- which the inspector still reports as two rows, because
    // coinciding is not the same as being one fact.
    CHECK(property_row(c, d, s, 7) == " Resolved 14 x 4 cells");
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
    // The row carries a caret and the alert role: a draft cannot be mistaken for a committed
    // value on the screen either.
    //
    // SINCE HD-6 THE WHOLE BODY IS ONE BOUNDED REGION and a property row is one of its rows,
    // mark and name included -- HD-5 had the mark and the name as an ordinary label beside a
    // one-cell region carrying only the value, which is the shape that could not hold a line
    // of this repository's face. What a maker SEES is unchanged: `cell_text_of` runs the real
    // cell projection, which inserts the caret glyph at the caret's own column.
    CHECK(property_row(drafting, d, s, 5) == ">Width    500p_");
    const InfoBodyPlace place = body_of(d, s);
    REQUIRE(place.present);
    // HD-7: the property list begins one row under `PROPERTIES`, which begins one row under
    // the object list -- so this is resolved and never `kRowsY + 5`.
    CHECK(prose_row_of_property(place, 5) == place.heading_row + 1 + 5);
    for (const surface::SurfaceLabel& l : cell_text_of(drafting)) {
        if (l.x == place.region_x && l.y == place.region_y + prose_row_of_property(place, 5)) {
            CHECK(l.role == surface::role::kAlert);
        }
    }
    // And the resolved row still reports the COMMITTED width, not the draft.
    CHECK(property_row(drafting, d, s, 7) == " Resolved 28 x 6 cells");

    CHECK(s.rows[5].commit() == Commit::Refused);
    s.notice = "Width: " + s.rows[5].refusal();
    s.notice_is_bad = true;

    const surface::SurfaceCanvas refused = paint(d, s);
    CHECK(label_at(refused, 0, kMinScreen.notice_y) == "Width: a share is 1% to 100%");
    for (const surface::SurfaceLabel& l : refused.labels) {
        if (l.y == kMinScreen.notice_y) {
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
    CHECK(c.width == kMinScreen.w);
    // Nothing is INVENTED here: no row, no object, no identity. And emptiness
    // ANNOUNCES itself, because a maker can reach this state by deleting their
    // own work -- and a panel that merely goes blank is indistinguishable from a
    // tool that has broken.
    CHECK(object_row(c, d, s, 0) == "(none) -- n makes one");
    CHECK(property_row(c, d, s, 0) == "(nothing selected)");
    // The workspace and the two headings are still there: an empty document is a
    // document, and the tool does not disappear with it.
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, kWorkspaceW, kWorkspaceH, surface::role::kMuted));
    CHECK(label_at(c, kMinScreen.panel_x, kInfoBodyY - 1) == "OBJECTS");
    CHECK(properties_heading(c, d, s) == "PROPERTIES");
    // And nothing was authored to make the message: the document is still empty.
    CHECK(d.elements.empty());
}

/// One canvas as the terminal medium's own pure function draws it, one string per screen row,
/// with the SGR and erase escapes taken out so what is left is the picture.
std::vector<std::string> rasterized(const surface::SurfaceCanvas& canvas) {
    const std::string body = surface::canvas_body(canvas);
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
    return rows;
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

    const std::vector<std::string> rows = rasterized(paint(d, s));
    REQUIRE(rows.size() == static_cast<std::size_t>(kMinScreen.h));
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
    // The inspector begins under `PROPERTIES`, which begins under the object list -- two
    // objects here, so it is four rows down rather than the eight `kRowsY` used to name.
    const InfoBodyPlace shown_at = body_of(d, s);
    const auto at = [&](std::size_t property) {
        return static_cast<std::size_t>(shown_at.region_y +
                                        prose_row_of_property(shown_at, property));
    };
    CHECK(at(0) == 4);
    CHECK(rows[at(0)].find("Identity #1") != std::string::npos);
    CHECK(rows[at(5)].find("Width    60%") != std::string::npos);
    CHECK(rows[at(7)].find("Resolved 28 x 6 cells") != std::string::npos);
}

// ----------------------------------------------------------------------------
// Tier 3b — a document larger than the panel, and a notice longer than its line
// ----------------------------------------------------------------------------
//
// Both defects here are the same one said twice: a bounded presentation that
// omits something and does not say so. Neither had a case, and neither could
// have: nothing in this suite had ever painted more than five objects, and
// nothing had ever painted a notice the screen could not hold. Every case below
// fails on the code that shipped before the bound and its marker existed.

TEST_CASE("a document that FITS the object list is shown whole, with no chrome at all") {
    // The simple case must stay the simple case. Five objects and eight properties both fit
    // the minimum screen's body with room to spare, and that is the size most likely to grow
    // accidental furniture when a bigger one gets some.
    WorkshopDoc d = many(5);
    Session s;
    s.selected = d.elements.back().id; // the LAST one -- the row that used to lose its marker
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s), d, s);
    REQUIRE(lines.size() == 5); // HD-7: its share is what it ASKED for, not a constant
    CHECK(lines[0] == "  #1 panel");
    CHECK(lines[1] == "  #2 panel");
    CHECK(lines[2] == "  #3 panel");
    CHECK(lines[3] == "  #4 panel");
    CHECK(lines[4] == "> #5 panel");
    for (const std::string& line : lines) {
        CHECK(line.find("...") == std::string::npos); // nothing was left out, so nothing says so
    }
}

TEST_CASE("an object past the list's share cannot vanish: it says what it left out") {
    // The exact reproduction of the defect this bound was built for -- a list that stopped and
    // said nothing -- at the population that reaches it now.
    //
    // HD-7 MOVED THE NUMBER AND NOT THE RULE. The list's share was `kListRows = 5` at every
    // extent; it is now what the room affords. HD-8 moved it once more and for the same kind
    // of reason: the footer's two control rows come off the budget before either list is
    // offered anything, so the minimum screen's share is six rather than seven. The rule is
    // untouched -- a list that cannot show everything says what it left out.
    WorkshopDoc d = many(9);
    Session s;
    s.selected = d.elements.back().id; // #9
    refocus(d, s);

    const surface::SurfaceCanvas c = paint(d, s);
    const std::vector<std::string> lines = object_lines(c, d, s);
    REQUIRE(lines.size() == 6);
    CHECK(lines[0] == "... 4 earlier");
    CHECK(lines[1] == "  #5 panel");
    CHECK(lines[4] == "  #8 panel");
    CHECK(lines[5] == "> #9 panel");

    // The three on-screen statements that used to disagree. Before the bound existed the
    // panel showed the first five with the marker on NONE of them, while the object count and
    // the inspector both said the last -- and the only one a maker could act on was the one
    // that was wrong.
    CHECK(d.elements.size() == 9);
    CHECK(property_row(c, d, s, 0) == " Identity #9");

    // The marker is the panel's own furniture: muted, like `(none) -- n makes one`, and it
    // mints no identity and invents no name. It is a ROW of the body since HD-7, so its role
    // is read off the region rather than off a canvas label.
    const InfoBodyPlace place = body_of(d, s);
    const surface::SurfaceTextRegion* body = body_on(c, place);
    REQUIRE(body != nullptr);
    CHECK(body->rows[0].role == surface::role::kMuted);
}

TEST_CASE("a selection in the middle of a long document names BOTH walls") {
    // A window that can sit in the middle can be silent on two sides, and silence on either
    // is the same defect. 6 + 5 shown + 9 accounts for all twenty, with nothing counted twice.
    //
    // A MIDDLE WINDOW NEEDS A DOCUMENT MORE THAN TWICE THE SHARE, which is another number
    // HD-7 moved and HD-8 moved again: at a five-row list nine objects reached it, at a
    // seven-row one it took more than twelve, and at HD-8's six-row share it takes more than
    // ten. 7 + 4 shown + 9 accounts for all twenty, with nothing counted twice.
    WorkshopDoc d = many(20);
    Session s;
    s.selected = 11;
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s), d, s);
    REQUIRE(lines.size() == 6);
    CHECK(lines[0] == "... 7 earlier");
    CHECK(lines[1] == "  #8 panel");
    CHECK(lines[4] == "> #11 panel");
    CHECK(lines[5] == "... 9 more");
}

TEST_CASE("a selection near the end keeps the document's tail visible") {
    // The second witness: nine objects with #8 selected drew five OTHER objects and no marker
    // at all.
    WorkshopDoc d = many(9);
    Session s;
    s.selected = 8;
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s), d, s);
    REQUIRE(lines.size() == 6);
    CHECK(lines[0] == "... 4 earlier");
    CHECK(lines[1] == "  #5 panel");
    CHECK(lines[4] == "> #8 panel");
    CHECK(lines[5] == "  #9 panel");
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
    s.selected = 40; // the EIGHTH in the file, and neither eighth nor last by identity
    refocus(d, s);

    const std::vector<std::string> lines = object_lines(paint(d, s), d, s);
    REQUIRE(lines.size() == 6);
    CHECK(lines[0] == "... 4 earlier");
    CHECK(lines[1] == "  #60 panel");
    CHECK(lines[2] == "  #30 panel");
    CHECK(lines[3] == "  #50 panel");
    CHECK(lines[4] == "> #40 panel");
    CHECK(lines[5] == "  #80 panel");
    // Sorted by identity the window would have run #30 #40 #50 #60 #70; sorted by
    // anything else it would have begun somewhere else again. It runs the way
    // the file runs.
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
    // THE STRONG WITNESS. Met live, and answered by shortening one producer's
    // wording: the rendered cycle is budgeted in characters
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
    REQUIRE(refused.refusal.size() > static_cast<std::size_t>(kMinScreen.w));

    Session s;
    s.selected = 9000000000000001;
    refocus(d, s);
    s.notice = refused.refusal;
    s.notice_is_bad = true;

    const surface::SurfaceCanvas c = paint(d, s);
    const std::string shown = label_at(c, 0, kMinScreen.notice_y);
    CHECK(shown.size() == static_cast<std::size_t>(kMinScreen.w)); // it fits the line it has
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
        if (l.y == kMinScreen.notice_y) {
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

    REQUIRE(rows.size() == static_cast<std::size_t>(kMinScreen.h));
    const std::string line = rows[static_cast<std::size_t>(kMinScreen.notice_y)];
    CHECK(line.size() == static_cast<std::size_t>(kMinScreen.w)); // the row is the screen's width
    CHECK(line.compare(line.size() - 3, 3, "...") == 0);      // and it ENDS by saying so
    CHECK(line.find("END") == std::string::npos);             // the tail really is gone
    CHECK(s.notice.find("END") != std::string::npos);         // and Workshop still has it
}

// ============================================================================
// Tier 4 — the WEAVE, through a real bus: input moments become maker gestures
// ============================================================================
//
// This tier exists because `WorkshopWeave` lives in workshop/weave.hpp rather
// than in the host's anonymous namespace. In the host it would be unreachable,
// leaving `gesture -> document` provable and `message -> gesture` not -- and the
// binding is the one part of the pointer path nothing else witnesses. The
// location is the whole mechanism; there is no test hook, no framework,
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



/// WHOEVER HOLDS `zengine.skin` -- an ordinary weave that records not only WHAT it
/// was told but WHO told it. `mail.sender()` is the BUS STAMP: it cannot be
/// written by a payload and cannot be chosen by whoever composed the message,
/// which is what makes it the one right instrument for "which identity spoke".
class SkinSeat : public loom::WeaveBase<SkinSeat, SeenState,
                                        loom::Accept<surface::SurfaceText>,
                                        loom::Emit<loom::Ack>> {
public:
    void on(const surface::SurfaceText& t, loom::Mail& mail) {
        ++state_.frames;
        heard.push_back(t);
        from.push_back(mail.sender());
        // Answers ONE slot, so Workshop's own status publications -- which are not asks and
        // which arrive here constantly -- are never answered by accident.
        if (t.slot == "ask") {
            (void)mail.answer(loom::Ack{});
        }
    }

    /// Who said this exact text, or the invalid id if nobody did.
    loom::WeaveId who_said(const std::string& text) const {
        for (std::size_t i = 0; i < heard.size(); ++i) {
            if (heard[i].text == text) {
                return from[i];
            }
        }
        return loom::WeaveId{};
    }

    std::vector<surface::SurfaceText> heard;
    std::vector<loom::WeaveId> from;
};

/// One participant's own record, filtered -- the transcript is a MODEL, so a test asks it
/// structured questions rather than grepping rendered prose.
std::vector<loom::TranscriptEntry> of_kind(const loom::TerminalSession& me,
                                           loom::TranscriptKind kind) {
    std::vector<loom::TranscriptEntry> out;
    for (const loom::TranscriptEntry& e : me.transcript().entries()) {
        if (e.kind == kind) {
            out.push_back(e);
        }
    }
    return out;
}

/// Everything the overlay is showing, as one string -- the pane's own column, top to bottom.
std::string pane_text(const surface::SurfaceCanvas& c) {
    std::string out;
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == kMinScreen.terminal_x && l.y >= kMinScreen.terminal_y) {
            out += l.text;
            out += '\n';
        }
    }
    return out;
}

/// A live Workshop: the real weave on a real bus, driven only by published
/// input messages. Nothing here reaches past the message boundary except to
/// READ the result.
struct Live {
    loom::Switchboard bus;
    HostContext host;
    std::vector<surface::SurfaceCanvas> canvases;
    std::vector<surface::SurfaceText> notes;
    WorkshopWeave* w = nullptr;
    loom::WeaveId workshop_id{};
    loom::WeaveId terminal_id{};

    Live() {
        auto weave = std::make_unique<WorkshopWeave>(host);
        w = weave.get();
        loom::Grant grant = loom::emit_default_grant(*w);
        loom::allow_poke_answers(grant);
        const loom::WeaveId id = bus.register_weave(std::move(weave), std::move(grant));
        w->zen_set_self(id);
        workshop_id = id;
        (void)loom::mount<Painter>(bus, canvases, notes);
    }

    /// MOUNT THE PARTICIPANT THE WAY THE HOST DOES -- on THIS bus, the one that already
    /// carries Workshop's own weave, and hand the weave the non-owning pointer through the
    /// same HostContext `request_stop` travels through.
    ///
    /// Its baseline is the host's own: one rule, SurfaceText to whoever holds `zengine.skin`
    /// AT DELIVERY. Workshop's own grant, minted above from its Emit set, is `to_any` for the
    /// same shape -- so the two identities differ by their RULE rather than by their
    /// vocabulary, which is the sharpest form the difference can take.
    ///
    /// `widen` is the CANARY LEVER: it gives the participant Workshop's wider rule. The case
    /// that asserts a publication from the pane reaches nobody is only a measurement if this
    /// makes it fail.
    /// `shapes` mounts EXTRA declared shapes beyond the default three. Zero is the default
    /// and every pre-existing case keeps exactly the vocabulary it had; a case that needs the
    /// completion list to be longer than the room it has (HD-3, the scrolled hit test) asks
    /// for more, because a window that never slides proves nothing about the sliding.
    loom::TerminalSession* mount_terminal(bool widen = false, int shapes = 0) {
        loom::TerminalVocabulary vocab;
        vocab.knows(loom::schema_of<surface::SurfaceText>())
            .accepts(loom::schema_of<loom::Ack>())
            .accepts(loom::schema_of<loom::Refused>());
        for (int i = 0; i < shapes; ++i) {
            vocab.knows(loom::SchemaBuilder("Extra" + std::to_string(i), 1)
                            .field("seq", loom::Kind::Int)
                            .build());
        }
        loom::Grant grant;
        grant.allow_to_role(surface::SurfaceText::zen_name, surface::SurfaceText::zen_version,
                            surface::kSkinRole);
        if (widen) {
            grant.allow_to_any(surface::SurfaceText::zen_name,
                               surface::SurfaceText::zen_version);
        }
        const loom::MountedTerminal mounted = loom::host_mount_terminal(
            bus, std::make_unique<loom::TerminalSession>("workshop", std::move(vocab)),
            std::move(grant));
        host.terminal = mounted.session;
        terminal_id = mounted.id;
        return mounted.session;
    }

    /// The office the participant is allowed to reach, held by a weave that remembers who
    /// spoke to it.
    SkinSeat* mount_skin_seat() {
        auto seat = std::make_unique<SkinSeat>();
        SkinSeat* raw = seat.get();
        loom::Grant grant;
        grant.allow_to_any(loom::Ack::zen_name, loom::Ack::zen_version);
        const loom::WeaveId id =
            bus.register_weave(std::move(seat), std::move(grant), surface::kSkinRole);
        raw->zen_set_self(id);
        return raw;
    }

    /// Shift+Space, AS THE BACKENDS ACTUALLY REPORT IT: the key transition and the character
    /// it produced, both, in the order they arrive. Getting this wrong in the fixture would
    /// hide the whole reason the toggle swallows text.
    void toggle_terminal() {
        key(input::scan::kSpace, input::mod::kShift);
        text(" ");
    }

    /// Type a whole line into whatever is taking text, then press Return.
    void type_line(const std::string& line) {
        for (const char c : line) {
            text(std::string(1, c));
        }
        key(input::scan::kReturn);
    }

    const TerminalPane& pane() const { return w->session().terminal; }

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
    /// own layout and not of the terminal's -- the two media report different
    /// numbers for one place (docs/reference/pointer-spaces.md).
    static std::int64_t px_x(std::int64_t wx) {
        return (wx + kWorkspaceX) * surface::kCanvasCellPx;
    }
    static std::int64_t px_y(std::int64_t wy) {
        return (wy + kWorkspaceY) * surface::kCanvasCellPx;
    }

    /// A PRESS AT AN EXACT POSITION IN THE MEDIUM'S OWN NUMBERS -- a window pixel or a
    /// terminal cell, untranslated. Every other helper here speaks WORKSPACE cells because
    /// that is what a maker thinks in for the document; the Terminal's interior is finer
    /// than a cell (HD-1), so its cases have to be able to say a pixel.
    void press_at(std::int64_t x, std::int64_t y, std::int64_t space) {
        publish(loom::to_value(input::PointerButton{1, true, x, y, space, input::mod::kNone}));
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

/// The index of the inspector row a session is editing, or `rows.size()` when none is.
std::size_t editing_index(const Live& t) {
    const Session& s = t.session();
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (s.rows[i].editing()) {
            return i;
        }
    }
    return s.rows.size();
}

/// THE INSPECTOR'S PROPERTY BODY, resolved the way the painter resolves it — through
/// `bounds_of` and `info_body_place`, never through a second arithmetic. A case that
/// computed the rectangle for itself would pass while the picture and the hit test disagreed.
InfoBodyPlace body_place(const Live& t) {
    const Screen sc = screen_of(t.session());
    return info_body_place(bounds_of(t.session().panels, panel::kInfo, sc).rect, sc, t.doc(),
                           t.session());
}

/// What the last canvas actually published for that body, or nullptr if it published none.
const surface::SurfaceTextRegion* body_region(const surface::SurfaceCanvas& c,
                                              const InfoBodyPlace& p) {
    for (const surface::SurfaceTextRegion& r : c.texts) {
        if (r.x == p.region_x && r.y == p.region_y) {
            return &r;
        }
    }
    return nullptr;
}

/// The prose row of the body the editing property is drawn on. `kNoProseRow` when none is.
std::int64_t editing_prose_row(const Live& t, const InfoBodyPlace& p) {
    return prose_row_of_property(p, editing_index(t));
}

/// The window pixel a graphical medium, and the terminal cell a character medium, would
/// report for a VALUE column of a body row. The inverse of what `prose_at` does with them,
/// and it goes through the same `RegionFit` — a helper that assumed cells would pass on the
/// TUI lane and lie on the SDL one.
std::int64_t body_pixel_x(const InfoBodyPlace& p, std::int64_t column) {
    if (!p.fit.graphical()) {
        return (p.region_x + column) * surface::kCanvasCellPx + surface::kCanvasCellPx / 2;
    }
    return p.region_x * surface::kCanvasCellPx + p.fit.origin_x + column * p.fit.advance_px +
           p.fit.advance_px / 2;
}
std::int64_t body_pixel_y(const InfoBodyPlace& p, std::int64_t prose_row) {
    if (!p.fit.graphical()) {
        return (p.region_y + prose_row) * surface::kCanvasCellPx + surface::kCanvasCellPx / 2;
    }
    return p.region_y * surface::kCanvasCellPx + p.fit.origin_y + prose_row * p.fit.line_px +
           p.fit.line_px / 2;
}
std::int64_t value_pixel_x(const InfoBodyPlace& p, std::int64_t value_column) {
    return body_pixel_x(p, kPropertyMarkCols + kPropertyLabelCols + value_column);
}
std::int64_t value_pixel_y(const InfoBodyPlace& p, std::int64_t prose_row) {
    return body_pixel_y(p, prose_row);
}

/// A long value that cannot fit an Inspector row at any extent this composition has.
const std::string kLongValue = "the quick brown fox jumps over the lazy dog";

/// The selected object's RESOLVED width, read the way the canvas reads it.
std::int64_t resolved_w(const Live& t) {
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* p = ui::placed_for(scene, t.session().selected);
    REQUIRE(p != nullptr);
    return p->rect.w;
}

} // namespace

TEST_CASE("a maker types `70%` through the canonical text route, and 70p is history") {
    // The headline, end to end. A vocabulary of scancodes alone cannot reach `%`
    // at all, which is what made `70p` a workaround worth committing. The
    // characters arrive as text the platform produced, and Workshop appends them
    // without owning one line of keyboard knowledge.
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

    // Capitals and symbols -- both unreachable from a scancode -- and `q`, which
    // is the quit command in the other mode and is simply a letter here.
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
    // A wire that cannot say "with Shift held" makes a second directional gesture
    // cost four literal keys (`,` `.` `-` `=`). This one can say it.
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
    // which is the canonical-share property arriving through the binding rather
    // than a behaviour of its own: 60% and 59% both resolve to 28 cells at a
    // 48-cell workspace, and the projection authors the canonical one.
    const std::int64_t wide = resolved_w(t);
    t.key(input::scan::kH, input::mod::kShift);
    CHECK(resolved_w(t) == wide - 1);
    CHECK(t.first()->width.mode == ui::kExtentPercent);
    t.key(input::scan::kL, input::mod::kShift);
    CHECK(resolved_w(t) == wide);

    // The four workaround keys are GONE, not merely unrecommended: they do
    // nothing at all, so nothing can quietly keep depending on them.
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
    // press -- which under a vocabulary whose buttons carry no position would
    // mean the weave believed the pointer was at 0,0 and grabbed what was there.
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
    // The exact failure a reconstruction produces, recreated at the weave: the
    // pointer is reported somewhere, then the platform goes quiet (a console
    // reports no motion while it lacks focus), then a click arrives somewhere
    // else entirely. A reconstruction answers with the stale position.
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
    // boundary value, and says which wall it met (screen.hpp's boundary policy).
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
    // `space` earning its field, in BOTH directions
    // (docs/reference/pointer-spaces.md). A backend reporting a space this
    // application cannot place is
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
    // The resize semantics, reached through pixels: the handle is one cell past the
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
    // close: `^s` refuses to SAVE over one because writing an unconfirmed value
    // would be the tool putting words in a maker's mouth, and quitting writes
    // nothing at all. Whether closing should ask about unsaved work is a separate
    // product question, and nothing here answers it.
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
    // The tiers above prove this over the gesture functions. This proves it over
    // the MESSAGES.
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

TEST_CASE("the selection marker never disappears as a maker walks past the list's share") {
    // The contradiction, driven the way a maker reaches it: `tab` cycles the
    // whole document, and an unwindowed list selects objects it does not draw.
    // Nine objects, made and walked by keystroke,
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
        const InfoBodyPlace place = body_place(t);
        for (const std::string& line : object_lines(t.canvases.back(), t.doc(), t.session())) {
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
        CHECK(drawn == place.objects_rows); // inside the budget the ROOM gave it (HD-7)

        // The status line and the inspector name the same object, on the same
        // frame the list was read from.
        CHECK(t.notes.back().text.rfind("[workshop] 9 objects | selected #" + std::to_string(id),
                                        0) == 0);
        CHECK(property_row(t.canvases.back(), t.doc(), t.session(), 0) ==
              " Identity #" + std::to_string(id));
    }
}

TEST_CASE("a notice a maker's own path makes too long is marked on screen, not cut in the session") {
    // The overlong notice through the real message path, on one Workshop produces
    // honestly. A document path is the maker's own input and may be any length the
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
    REQUIRE(t.notice().size() > static_cast<std::size_t>(kMinScreen.w));
    CHECK(t.notice().find(t.host.document_path) != std::string::npos);

    const std::string shown = label_at(t.canvases.back(), 0, kMinScreen.notice_y);
    CHECK(shown.size() == static_cast<std::size_t>(kMinScreen.w));
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
// Three questions, and every case below answers one of them:
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

/// A document with everything persistence has to preserve in it: two objects sharing a
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
    // The two-panels fixture, asked across a process boundary: a duplicate name is legal
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
    // about what the document means -- and here the two visibly diverge.
    // The SHAPE is at version 2 (an object grew a `context`); `format_version`
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
    // The no-op stability rule, in its persistence form. At a 48-cell
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
    // form of the rule the property editor keeps.
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
    // pairing has paid for itself in this file.
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
    CHECK(object_row(t.canvases.back(), t.doc(), t.session(), 0) == "(none) -- n makes one");
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
    // The same shape as the Ctrl+C case: the modifier is READ, not implied by
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
// Tier 7 — composition: one authored object read in another's frame
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
//      span, through the same projection a root resize uses.
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
/// returned was read out of freed memory. It is the same defect the sanitizer
/// lane has found in committed test code before; here the ordinary lane
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
        CHECK(std::string("Context: " + no.refusal).size() <= static_cast<std::size_t>(kMinScreen.w));
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
        CHECK(std::string("Context: " + no.refusal).size() <= static_cast<std::size_t>(kMinScreen.w));
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
    // "The workspace starts at 0" is a law about the WORKSPACE and not about
    // coordinates, and its own stated reason is what says so.
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
    CHECK(object_row(canvas, d, s, 0) == "  #" + std::to_string(c) + " C");
    CHECK(object_row(canvas, d, s, 1) == "> #" + std::to_string(a) + " A");
    CHECK(object_row(canvas, d, s, 2) == "  #" + std::to_string(b) + " B");

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

    // A ROOT object meets the same wall and authors 0.
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
    // The one projection, handed the right span. There is no second one and
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
    // The written shape changed, so an older document no longer admits. That is
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


// ---- Tier 7: the terminal overlay (WT-1) -----------------------------------------------
//
// WORKSHOP PRESENTS AN ORDINARY `loom::TerminalSession` ON ITS EXISTING LOOM. Six claims the
// prompt named, and four this composition has to keep for those six to mean anything:
//
//   shift+space opens it, shift+space closes it
//   a closed overlay leaves every ordinary Workshop gesture exactly as it was
//   a typed line reaches the PARTICIPANT -- its own record, its own door
//   the transcript becomes visible through Workshop's own canvas vocabulary
//   the participant is on the SAME bus, not a second Loom
//   ...and the toggle's own keystroke never becomes text
//   ...and a target sees the PARTICIPANT as sender, never Workshop
//   ...and the two grants do not merge, in either direction     <- canary'd
//   ...and SUBMITTED never quietly becomes "delivered" on this screen
//   ...and the pane says what it is not showing
//
// The pointer path is not re-proven here: it is the same `canvas_point_of` chain Tier 4
// walks, and the only thing WT-1 changed about it is that an open overlay ignores it.

TEST_CASE("shift+space opens the terminal overlay, and shift+space closes it") {
    Live t;
    t.mount_terminal();
    REQUIRE_FALSE(t.pane().open);

    t.toggle_terminal();
    CHECK(t.pane().open);
    CHECK(t.pane().attached);
    CHECK(t.pane().id == t.terminal_id);

    t.toggle_terminal();
    CHECK_FALSE(t.pane().open);
    CHECK(t.notice() == "terminal closed -- shift+space reopens it");

    // A bare Space is not the gesture, and neither is Shift+anything-else.
    t.key(input::scan::kSpace);
    CHECK_FALSE(t.pane().open);
    t.key(input::scan::kJ, input::mod::kShift);
    CHECK_FALSE(t.pane().open);
}

TEST_CASE("the toggle's own keystroke never becomes text, in either direction") {
    Live t;
    t.mount_terminal();
    // The backends report Shift+Space as a key AND a space. Opening must not seed the command
    // line with one, and closing must not append one to whatever is underneath.
    t.toggle_terminal();
    REQUIRE(t.pane().open);
    CHECK(t.pane().input.empty());

    t.text("s");
    CHECK(t.pane().input.text() == "s");
    t.toggle_terminal(); // close
    REQUIRE_FALSE(t.pane().open);

    // ...and the space did not land in the inspector either. Open an editor draft first, so
    // there is somewhere for a leaked space to go, and look at it after a full open/close.
    t.begin_editing("Name");
    const Row* name = t.row("Name");
    REQUIRE(name != nullptr);
    REQUIRE(name->editing());
    const std::string draft = name->display();
    t.toggle_terminal();
    t.toggle_terminal();
    CHECK(t.row("Name")->display() == draft);

    // A ONE-SHOT FLAG THAT OUTLIVED ITS MOMENT WOULD EAT A SPACE A MAKER MEANT. The next
    // ordinary key ends the toggle's moment, so a space typed after one is ordinary text.
    t.text("a");
    t.text(" ");
    t.text("b");
    CHECK(t.row("Name")->display().find("a b") != std::string::npos);
}

TEST_CASE("a closed overlay leaves every ordinary Workshop gesture exactly as it was") {
    Live t;
    t.mount_terminal();
    const std::int64_t before_x = t.first()->x;

    // Open, type something that would be four commands with the pane closed, close.
    t.toggle_terminal();
    t.type_line("hjkl");
    t.toggle_terminal();
    REQUIRE_FALSE(t.pane().open);

    t.key(input::scan::kL);
    CHECK(t.first()->x == before_x + 1);
    t.key(input::scan::kN);
    CHECK(t.doc().elements.size() == 3);
    t.key(input::scan::kTab);
    CHECK(t.session().selected != 0);
    t.press(t.first()->x + 1, t.first()->y + 1);
    CHECK(t.session().drag.active);
    t.release(t.first()->x + 1, t.first()->y + 1);
    CHECK_FALSE(t.session().drag.active);
}

TEST_CASE("while the overlay is open the keys and the pointer belong to it") {
    Live t;
    t.mount_terminal();
    const std::int64_t x = t.first()->x;
    const std::size_t objects = t.doc().elements.size();
    t.toggle_terminal();

    // `h`, `n` and `d` are Workshop commands with the pane closed. With it open they are text.
    t.key(input::scan::kH);
    t.text("h");
    t.key(input::scan::kN);
    t.text("n");
    t.key(input::scan::kD);
    t.text("d");
    CHECK(t.pane().input.text() == "hnd");
    CHECK(t.first()->x == x);
    CHECK(t.doc().elements.size() == objects);

    // ...and a press does not take hold of an object the maker cannot see.
    t.press(t.first()->x + 1, t.first()->y + 1);
    CHECK_FALSE(t.session().drag.active);
    t.motion(t.first()->x + 4, t.first()->y + 1);
    CHECK(t.first()->x == x);

    // Backspace erases, escape clears the line and LEAVES THE PANE OPEN.
    t.key(input::scan::kBackspace);
    CHECK(t.pane().input.text() == "hn");
    t.key(input::scan::kEscape);
    CHECK(t.pane().input.empty());
    CHECK(t.pane().open);
}

TEST_CASE("Ctrl+C and ^s still mean the same thing with the overlay open") {
    Live t;
    t.mount_terminal();
    t.toggle_terminal();
    // ^s is not text -- a control byte is never TextEntered -- so it cannot be typed into a
    // command line, and its meaning is deliberately mode-independent.
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "no document file -- start Workshop with --document <path>");
    CHECK(t.pane().open);
    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK(t.host.quit);
}

TEST_CASE("a typed line reaches the participant's own record, understood or not") {
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    t.type_line("this is not a verb");

    // RECORDED BEFORE IT IS UNDERSTOOD. A chronology of effects with no causes is not a
    // session, so the line is on the participant even though it authored nothing.
    const std::vector<loom::TranscriptEntry> log = me->transcript().entries();
    REQUIRE(log.size() >= 2);
    CHECK(log[0].kind == loom::TranscriptKind::LocalCommand);
    CHECK(log[0].text == "this is not a verb");
    CHECK(log[1].kind == loom::TranscriptKind::LocalNotice);
    CHECK(log[1].text.find("two verbs") != std::string::npos);
    CHECK(of_kind(*me, loom::TranscriptKind::Submitted).empty());
}

TEST_CASE("a typed send leaves through the PARTICIPANT's door, on Workshop's own bus") {
    Live t;
    SkinSeat* seat = t.mount_skin_seat();
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"from the pane\"");
    t.bus.pump();

    // IT ARRIVED, AND IT ARRIVED ON THIS BUS. The seat is registered on the same Switchboard
    // Workshop's own weave is; nothing here constructs a second Loom, and a participant on a
    // second one could not have reached this weave at all.
    REQUIRE_FALSE(seat->heard.empty());
    bool arrived = false;
    for (const surface::SurfaceText& said : seat->heard) {
        arrived = arrived || (said.slot == "score" && said.text == "from the pane");
    }
    CHECK(arrived);

    // ...AND THE BUS STAMPED THE PARTICIPANT, NOT WORKSHOP. This is the whole authority
    // claim, measured at the one place that cannot be talked out of it.
    CHECK(seat->who_said("from the pane") == t.terminal_id);
    CHECK(seat->who_said("from the pane") != t.workshop_id);
    CHECK(me->id() == t.terminal_id);

    // TWO IDENTITIES, ONE SCREEN, AND STILL TWO. The same seat also heard Workshop's own
    // status line, from every repaint -- and those are stamped with WORKSHOP's id. One
    // keyboard, one screen, one bus, two senders, told apart by Loom and by nothing else.
    bool workshop_spoke = false;
    for (const loom::WeaveId who : seat->from) {
        workshop_spoke = workshop_spoke || who == t.workshop_id;
    }
    CHECK(workshop_spoke);

    // The participant's own record says SUBMITTED, addressed to the office, and says nothing
    // whatever about what happened next.
    const std::vector<loom::TranscriptEntry> sent =
        of_kind(*me, loom::TranscriptKind::Submitted);
    REQUIRE(sent.size() == 1);
    CHECK(sent.back().shape == "SurfaceText");
    CHECK(sent.back().addressing == loom::Addressing::Role);
    CHECK(sent.back().role == surface::kSkinRole);
}

TEST_CASE("holding the pointer merged nothing: the two grants stay different") {
    // The pane holds a C++ pointer to a participant. That is PRESENTATION. What each of the
    // two may SAY is decided by the Kernel against that one's own grant, separately -- and
    // here the two differ in the same shape, by their rule alone: Workshop may publish
    // SurfaceText to anybody; the participant may say it only to an office.
    //
    // CANARY: `mount_terminal(true)` gives the participant Workshop's wider rule, and the
    // publication below then arrives -- which is what makes this a measurement rather than a
    // restatement of the fixture.
    const bool widen = false;
    Live t;
    SkinSeat* seat = t.mount_skin_seat();
    loom::TerminalSession* me = t.mount_terminal(widen);
    t.toggle_terminal();

    // The office: authorized, and it lands.
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"to the office\"");
    t.bus.pump();
    CHECK(seat->who_said("to the office") == t.terminal_id);

    // A PUBLICATION: composed, authored, SUBMITTED -- and refused at delivery for want of a
    // rule this participant does not hold. The participant is not told that, and does not
    // claim to know it; what the transcript says is that it was submitted.
    t.type_line("send * SurfaceText 1 slot=\"score\" text=\"to everyone\"");
    t.bus.pump();
    CHECK(of_kind(*me, loom::TranscriptKind::Submitted).size() == 2);
    CHECK_FALSE(seat->who_said("to everyone").valid());

    // ...while Workshop's own publications of the very same shape reached the seat all along.
    // Nothing the participant holds widened Workshop, and nothing Workshop holds widened the
    // participant.
    bool workshop_published = false;
    for (const loom::WeaveId who : seat->from) {
        workshop_published = workshop_published || who == t.workshop_id;
    }
    CHECK(workshop_published);
}

TEST_CASE("the address grammar the pane reads is Loom's own, not a second one") {
    // The pane parses `@office` with `loom::parse_address` out of <zen/terminal/input_lex.hpp>,
    // the same parser the standalone terminal uses. Two claims, joined here: the parser says
    // what it says, and a line typed into Workshop is addressed by it.
    loom::Address a;
    REQUIRE(loom::parse_address("@zengine.skin", a));
    REQUIRE(a.mode == loom::Addressing::Role);

    Live t;
    SkinSeat* seat = t.mount_skin_seat();
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    // A BAREWORD IS NOT AN ADDRESS, and the pane invents no default for one.
    t.type_line("send zengine.skin SurfaceText 1 slot=\"score\" text=\"no sigil\"");
    t.bus.pump();
    CHECK_FALSE(seat->who_said("no sigil").valid());
    CHECK(of_kind(*me, loom::TranscriptKind::Submitted).empty());
    CHECK(me->transcript().entries().back().text.find("#12 for one weave") != std::string::npos);

    // ...and the same line with the sigil is authored.
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"with sigil\"");
    t.bus.pump();
    CHECK(seat->who_said("with sigil") == t.terminal_id);
}

TEST_CASE("an ask waits, and LOOM's own answer settles it on the screen") {
    Live t;
    (void)t.mount_skin_seat();
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    // AWAITING IS ALL AN OUTSTANDING ASK MEANS. Not delivered, not being worked on, not seen
    // by anybody -- so an ask nobody answers simply stays outstanding, and the pane pumps
    // nothing to change that: the HOST owns the loop here exactly as it does in the
    // standalone terminal.
    t.type_line("ask #99 SurfaceText 1 slot=\"ask\" text=\"anyone at 99\"");
    CHECK(me->awaiting());
    CHECK(me->outstanding() == 1);
    t.bus.pump();
    t.bus.pump();
    CHECK(me->awaiting()); // still. Nothing here can tell it whether that was even delivered.

    t.type_line("ask @zengine.skin SurfaceText 1 slot=\"ask\" text=\"is anybody there\"");
    CHECK(me->outstanding() == 1); // the second settled inside the fixture's own pump

    // The answer is settled by LOOM's provenance and correlation, not by shape: the entry is
    // an AnswerReceived, it names the local ask number, and the pane renders both.
    const std::vector<loom::TranscriptEntry> answers =
        of_kind(*me, loom::TranscriptKind::AnswerReceived);
    REQUIRE(answers.size() == 1);
    CHECK(answers.front().answers_ask);
    CHECK(answers.front().shape == "zen.Ack");
    CHECK(answers.front().sender != t.workshop_id);
    CHECK(terminal_line(answers.front()).find("[Loom: answers ask 2]") != std::string::npos);

    // Escape: a repaint, and a cleared line. `t.text(...)` would repaint too and would leave
    // what it typed in the buffer -- which is how this case first read `xask * ...` and
    // measured the wrong verb.
    t.key(input::scan::kEscape);
    CHECK(pane_text(t.canvases.back()).find("v zen.Ack v1 from #") != std::string::npos);
    CHECK(pane_text(t.canvases.back()).find("[Loom: answers ask 2]") != std::string::npos);

    // A PUBLICATION CANNOT BE AN ASK: it has no one respondent, so there is no conversation
    // for Loom to authorize. Refused LOCALLY, and the participant says so in its own record.
    const std::size_t before = me->outstanding();
    t.type_line("ask * SurfaceText 1 slot=\"ask\" text=\"everyone\"");
    CHECK(me->outstanding() == before);
    std::string said;
    for (const loom::TranscriptEntry& e : me->transcript().entries()) {
        if (e.kind == loom::TranscriptKind::LocalNotice ||
            e.kind == loom::TranscriptKind::LocalRefusal) {
            said = e.text;
        }
    }
    CHECK(said.find("bad-address") != std::string::npos);
}

TEST_CASE("the transcript becomes visible through Workshop's own canvas") {
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    t.toggle_terminal();
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"hello\"");
    t.bus.pump();
    const surface::SurfaceCanvas& c = t.canvases.back();

    // ANCHORED TO THE BOTTOM-RIGHT CORNER OF THE ROOM, exactly (HD-10). The bottom edge is
    // still the screen's; the right edge is the WORKSPACE's, which is the whole of what this
    // phase moved -- the 28 columns beyond it are reserved for the side region and are not
    // this pane's to spend, whether or not anything is standing in them.
    CHECK(kMinScreen.terminal_x + kMinScreen.terminal_w == kMinScreen.room_w);
    CHECK(kMinScreen.terminal_y + kMinScreen.terminal_h == kMinScreen.h);
    CHECK(label_at(c, kMinScreen.terminal_x, kMinScreen.terminal_y).rfind("TERMINAL -- weave #", 0) == 0);
    CHECK(label_at(c, kMinScreen.terminal_x, kMinScreen.terminal_y).find(std::to_string(t.terminal_id.value)) !=
          std::string::npos);

    // The line a maker typed, and the message it authored, both on the screen.
    const std::string body = pane_text(c);
    CHECK(body.find("> send @zengine.skin") != std::string::npos);
    CHECK(body.find("^ SurfaceText v1 -> @zengine.skin") != std::string::npos);

    // SUBMITTED IS NOT DELIVERED, and a renderer is the one place that contract can be broken
    // by prose alone. The pane says what SUBMITTED means, once, on a row it always shows --
    // and nowhere on it does it say the other thing.
    //
    // AND AT THIS EXTENT IT IS ELIDED BY THREE CHARACTERS, WHICH IS HD-10'S PRICE AND IS
    // MEASURED HERE RATHER THAN DISCOVERED. The statement is 51 characters and the narrowest
    // pane is now 48, so `detail::fit` marks it -- the same mark it puts on every other row
    // it cuts. What is asserted is therefore the row the pane actually writes, and the two
    // halves of the contract are asserted separately: the word SUBMITTED survives the cut at
    // every width this pane has, and the word `delivered` appears nowhere at any width.
    CHECK(body.find("SUBMITTED") != std::string::npos);
    CHECK(body.find(detail::fit(terminal_legend(), kMinScreen.terminal_cols)) != std::string::npos);
    CHECK(body.find("delivered") == std::string::npos);
    CHECK(label_at(c, kMinScreen.terminal_x, kMinScreen.terminal_y + 1)
              .rfind(detail::fit(terminal_legend(), kMinScreen.terminal_cols), 0) == 0);
    // The whole statement is on the row from 81 columns up, where the room first holds it.
    CHECK(static_cast<std::int64_t>(terminal_legend().size()) == 51);
    CHECK(screen_of(81, kScreenMinH).terminal_cols == 51);
    CHECK(detail::fit(terminal_legend(), screen_of(81, kScreenMinH).terminal_cols) ==
          terminal_legend());

    // The cursor row shows what is being typed, and every row is exactly the pane's width --
    // which is what clears the furniture underneath in a medium whose ink is one cell.
    t.text("ab");
    const surface::SurfaceCanvas& c2 = t.canvases.back();
    CHECK(label_at(c2, kMinScreen.terminal_x, kMinScreen.h - 1).rfind("> ab_", 0) == 0);
    for (const surface::SurfaceLabel& l : c2.labels) {
        if (l.x == kMinScreen.terminal_x && l.y >= kMinScreen.terminal_y) {
            CHECK(static_cast<std::int64_t>(l.text.size()) == kMinScreen.terminal_w);
        }
    }

    // The bottom band is the pane's while it is open: the notice and the two help lines are
    // not painted under it, because their right-hand two thirds would be covered and a
    // sentence beheaded mid-word is worse than no sentence.
    //
    // SAID AS CONTENT AND NOT AS POSITION (HD-10). It used to be `nothing is at column 0 on
    // those rows`, which was true only because the pane began at column 22; the pane begins
    // at column 0 on the minimum screen now, so that spelling would have been asking whether
    // the PANE was there. What the claim was always about is the two sentences.
    CHECK(label_at(c2, 0, kMinScreen.help_y).rfind("n new | d delete", 0) != 0);
    CHECK(label_at(c2, 0, kMinScreen.help_y + 1).rfind("enter edit | esc cancel", 0) != 0);
    // ...and with the pane closed they are exactly as they were.
    t.toggle_terminal();
    CHECK(label_at(t.canvases.back(), 0, kMinScreen.help_y).rfind("n new | d delete", 0) == 0);
}

TEST_CASE("the pane is published as ONE bounded region, placed in cells") {
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    // ESCAPE FIRST, so this case measures the PANE and not the pane plus the completion
    // list HD-2 put over it -- which is a second region belonging to this same pane, and
    // is asserted where it belongs, a few cases down.
    t.text("s");
    t.key(input::scan::kEscape);
    const surface::SurfaceCanvas& c = t.canvases.back();

    // TWO REGIONS, AND THE SECOND IS THE INSPECTOR'S PROPERTY BODY (HD-6). No other panel
    // migrated: the workspace, the object list, the picker and the help band are all still
    // labels, and the completion list is not on this canvas (Escape dismissed it above).
    REQUIRE(c.texts.size() == 2);
    CHECK(list_of(c, kMinScreen) == nullptr);
    const surface::SurfaceTextRegion& pane = *pane_of(c, kMinScreen);
    CHECK(pane.x == kMinScreen.terminal_x);
    CHECK(pane.y == kMinScreen.terminal_y);
    CHECK(pane.w == kMinScreen.terminal_w);
    CHECK(pane.h == kMinScreen.terminal_h);
    CHECK(pane.rows.size() == kMinScreen.terminal_lines);

    // The backdrop is STILL A RECT IN CELLS. What got finer is the interior, not the
    // furniture -- a region carries no background of its own and was not given one.
    bool backdrop = false;
    for (const surface::SurfaceRect& r : c.rects) {
        if (r.x == pane.x && r.y == pane.y && r.w == pane.w && r.h == pane.h) {
            backdrop = true;
            CHECK(r.role == surface::role::kMuted);
        }
    }
    CHECK(backdrop);

    // The chrome is where it has always been, counted in the pane's own rows.
    CHECK(pane.rows[0].text.rfind("TERMINAL -- weave #", 0) == 0);
    CHECK(pane.rows[0].role == surface::role::kAccent);
    // Fitted to the pane's own columns since HD-10 -- 48 of them on the minimum screen, three
    // short of this statement, so the row carries the mark `detail::fit` leaves.
    CHECK(pane.rows[1].text == detail::fit(terminal_legend(), kMinScreen.terminal_cols));
    // THE LINE, AND THE CARET SAID SEPARATELY FROM IT (HD-3). The row no longer carries a
    // trailing `_`: the caret is a fact ABOUT the region, in the region's own prose
    // lattice, so each medium can answer it in its own type. The projection below is where
    // a character medium's answer is measured.
    CHECK(pane.rows[pane.rows.size() - 1].text.rfind("> s", 0) == 0);
    CHECK(pane.rows[pane.rows.size() - 1].text.find('_') == std::string::npos);
    CHECK(pane.caret_row == static_cast<std::int64_t>(pane.rows.size()) - 1);
    CHECK(pane.caret_col == kTerminalPromptCols + 1); // `> s` -- after the one character

    // A ROW IS TRUNCATED BY THE PUBLISHER AND PADDED BY THE PROJECTION. The pane decides
    // what it can show (a presentation decision); making a space erase the furniture
    // underneath is every medium's business and is done for all of them at once.
    for (const surface::SurfaceTextRow& row : pane.rows) {
        CHECK(static_cast<std::int64_t>(row.text.size()) <= kMinScreen.terminal_cols);
    }
    surface::SurfaceCanvas only_pane = c;
    only_pane.texts = {pane};
    for (const surface::ProjectedRow& p : surface::project_text_regions(only_pane)) {
        CHECK(static_cast<std::int64_t>(p.label.text.size()) == kMinScreen.terminal_w);
    }

    // ...and with the pane closed there is no region of ITS but the Info panel's body is
    // still there, because the body is not an overlay: it is what the Info panel has been
    // drawing all along, bounded (HD-6, widened to both lists by HD-7).
    t.toggle_terminal();
    CHECK(t.canvases.back().texts.size() == 1);
    CHECK(t.canvases.back().texts.front().y == body_place(t).region_y);
}

TEST_CASE("a medium that sets real type reflows the pane, and the omission stays true") {
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();

    // Forty entries, each a sentence long enough to WRAP at 48 cells and not at 71
    // columns -- which is exactly the width difference the metric buys, so the same
    // record produces a different number of rows in the two media. (56 and 83 before
    // HD-10 narrowed the pane's placement by eight cells at this extent; the difference
    // the case is about is between the two MEDIA and is unmoved by that.)
    for (int i = 0; i < 40; ++i) {
        (void)me->record_notice("entry " + std::to_string(i) +
                                ": a sentence of some length, wrapping at the cell width");
    }
    // `x` completes nothing, and HD-2's truthful "nothing here begins with that" is a
    // second region. Dismissed, so this case still measures the pane on its own.
    t.text("x");
    t.key(input::scan::kEscape);
    const std::size_t cell_shown = t.pane().shown.size();
    const std::uint64_t cell_earlier = t.pane().earlier;

    // THE MEDIUM SAYS IT HAS TYPE. Same cells, a real advance and a real line height.
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    const Screen sc = screen_of(t.session());
    REQUIRE(sc.terminal_cols == 71);
    REQUIRE(sc.terminal_rows == 4);

    // THE SNAPSHOT AND THE PICTURE AGREE, which is the whole of HD-1's correctness
    // requirement. Both sides asked `screen_of` for the same two numbers, so what the pane
    // CHOSE to show is exactly what the pane SPENDS its rows on -- and `... N earlier` is
    // therefore arithmetic rather than a hope.
    const surface::SurfaceCanvas& c = t.canvases.back();
    const surface::SurfaceTextRegion& pane = *pane_of(c, sc);
    CHECK(pane.rows.size() == sc.terminal_lines);
    CHECK(pane.w == kMinScreen.terminal_w); // the PLACEMENT did not move
    CHECK(pane.h == kMinScreen.terminal_h);

    std::size_t spent = 0;
    for (const loom::TranscriptEntry& e : t.pane().shown) {
        spent += terminal_wrapped(e, sc.terminal_cols).size();
    }
    CHECK(spent <= sc.terminal_rows);
    CHECK(t.pane().earlier ==
          me->transcript().size() - static_cast<std::uint64_t>(t.pane().shown.size()));
    CHECK(terminal_omission(t.pane()) ==
          "... " + std::to_string(t.pane().earlier) + " earlier");

    // THE RECORD REFLOWED RATHER THAN BEING REWRITTEN. The same entries cost FEWER ROWS at
    // the wider prose measure, which is the whole reason a pane four rows tall can still
    // hold roughly what a pane nine cell-rows tall held.
    std::size_t cell_cost = 0;
    for (const loom::TranscriptEntry& e : t.pane().shown) {
        cell_cost += terminal_wrapped(e, kMinScreen.terminal_w).size();
    }
    CHECK(cell_cost > spent);
    CHECK(t.pane().shown.size() >= 1);
    CHECK(t.pane().earlier > 0);
    CHECK(t.pane().dropped == 0);
    CHECK(cell_shown >= 1);
    CHECK(cell_earlier > 0);

    // A LONGER LINE IS THE PROOF THE WIDTH IS REAL: at 71 columns this is one row, and at
    // 48 cells it is more than one. (Sixty characters and a three-character sigil, chosen
    // against the two widths HD-10 left this pane rather than the two it had before.)
    const std::string long_line(60, 'w');
    (void)me->record_notice(long_line);
    t.text("y");
    const loom::TranscriptEntry newest = me->transcript().tail(1)[0];
    CHECK(terminal_wrapped(newest, sc.terminal_cols).size() == 1);
    CHECK(terminal_wrapped(newest, kMinScreen.terminal_w).size() > 1);

    // AND THE FACE GOING AWAY PUTS IT BACK. A metric of zero is the sentence "text is a
    // cell", which is what the bitmap fallback draws -- so a font that fails to open leaves
    // Workshop wrapping against the thing that is actually being painted, never against the
    // metric of a face that is not there.
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 0, 0}));
    const Screen back = screen_of(t.session());
    CHECK(back.terminal_cols == kMinScreen.terminal_cols);
    CHECK(back.terminal_lines == kMinScreen.terminal_lines);
    CHECK(back.terminal_rows == kMinScreen.terminal_rows);
    CHECK(pane_of(t.canvases.back(), back)->rows.size() == kMinScreen.terminal_lines);
}

TEST_CASE("a fresh skin's hello does NOT clear the presentation context -- measured, not blessed") {
    // HD-1 §18, answered by measurement rather than by hope. The question is what happens if
    // a metric-reporting graphical skin disappears and a non-reporting one becomes active in
    // the same process, and the answer is: NOTHING HAPPENS, which is the problem.
    //
    // A skin's HELLO carries no room and no metric -- it is one sentence, "I have claimed a
    // surface" -- so nothing about it can correct a presentation context left behind by a
    // skin that is gone, and the session keeps the dead window's cell extent AND, since HD-1,
    // its text metric. That is why this is reported here rather than repaired here: fixing it
    // through the hello means deciding what a skin's hello does to a screen, which is a
    // skin-handoff decision with its own evidence, not a detail of the extent's lifetime.
    //
    // TUI-0 NARROWED THIS WITHOUT CLOSING IT, and the distinction is worth keeping exact. A
    // terminal skin that can measure its terminal now publishes its own extent on its first
    // pump, which arrives within a beat of the swap and overwrites both numbers -- so the
    // window a maker actually swaps into a real terminal repairs itself. What is unrepaired
    // is the swap into a medium with NO opinion (a piped run, a captured run), where the
    // silence below is still the whole of what arrives. The seam moved from "always" to
    // "when the new medium cannot measure itself", and this case pins the second.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();

    t.publish(loom::to_value(surface::SurfaceExtent{115, 63, 8, 18}));
    CHECK(t.session().screen_w == 115);
    CHECK(t.session().text_advance_px == 8);
    CHECK(t.session().text_line_px == 18);

    // A NEW SKIN SAYS HELLO. This is exactly what a freshly loaded incarnation publishes --
    // once, on its first message -- and it is the only thing a terminal skin ever says about
    // itself.
    t.publish(loom::to_value(surface::SurfaceReady{}));

    // ...and the graphical numbers are all still here. Workshop repaints, at the extent and
    // the metric of a surface that may no longer exist.
    CHECK(t.session().screen_w == 115);
    CHECK(t.session().text_advance_px == 8);
    CHECK(t.session().text_line_px == 18);

    // WHAT THAT COSTS, CONCRETELY, so the seam is a measurement rather than a worry: the pane
    // chooses its rows at the graphical width and the cell projection then cuts them at the
    // region's cell width, so a character medium inheriting a graphical context loses the
    // right-hand end of every long row -- silently, because both halves are behaving.
    const Screen sc = screen_of(t.session());
    CHECK(sc.terminal_cols > sc.terminal_w);
    CHECK(list_of(t.canvases.back(), sc) == nullptr);
}

TEST_CASE("the pane says what it is not showing, in the two senses that differ") {
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    CHECK(terminal_omission(t.pane()) == "[the whole of this session's record is on screen]");

    // More lines than the pane is tall: the rest is EARLIER, and still in the record.
    for (int i = 0; i < 20; ++i) {
        (void)me->record_notice("line " + std::to_string(i));
    }
    t.text("x"); // any event repaints, and a repaint re-snapshots
    CHECK(t.pane().shown.size() == kMinScreen.terminal_rows);
    CHECK(t.pane().earlier > 0);
    CHECK(t.pane().dropped == 0);
    CHECK(terminal_omission(t.pane()).rfind("... ", 0) == 0);

    // Past the transcript's own bound the earliest lines are gone for good, and the pane says
    // THAT differently -- "you cannot see it here" and "nobody can see it any more" are
    // answers to different questions.
    for (std::size_t i = 0; i < loom::kTranscriptCapacity + 5; ++i) {
        (void)me->record_notice("flood " + std::to_string(i));
    }
    t.text("y");
    CHECK(t.pane().dropped > 0);
    CHECK(terminal_omission(t.pane()).find("dropped for good") != std::string::npos);
    CHECK(pane_text(t.canvases.back()).find("dropped for good") != std::string::npos);
}

TEST_CASE("a Workshop with no participant says so and authors nothing") {
    // The host may mount none -- `HostContext::terminal` is a plain pointer with a null
    // default, and a suite constructs one either way. The pane must be a refusal, not a crash.
    Live t;
    REQUIRE(t.host.terminal == nullptr);
    t.toggle_terminal();
    CHECK(t.pane().open);
    CHECK_FALSE(t.pane().attached);
    t.type_line("send @zengine.skin SurfaceText 1 slot=\"score\" text=\"x\"");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("no terminal participant is mounted") != std::string::npos);
    CHECK(label_at(t.canvases.back(), kMinScreen.terminal_x, kMinScreen.terminal_y)
              .rfind("TERMINAL -- no participant", 0) == 0);
}

TEST_CASE("the pane's snapshot outlives the participant it came from") {
    // THE LIFETIME CLAIM, and the reason the sanitizer lane is obliged for this phase. The bus
    // owns the participant; Workshop holds a non-owning pointer to it. Every entry the pane
    // paints from is a COPY taken inside a handler, so a canvas already published cannot come
    // to read a freed transcript -- and this case is what lets ASan say so.
    std::vector<loom::TranscriptEntry> held;
    std::string painted;
    {
        Live t;
        (void)t.mount_terminal();
        t.toggle_terminal();
        t.type_line("hello");
        held = t.pane().shown;
        painted = pane_text(t.canvases.back());
        REQUIRE_FALSE(held.empty());

        // The host's explicit act: end the participant, and LET THE LAST OWNER GO. The inner
        // scope is load-bearing -- `unregister_weave` hands the weave back, so holding that
        // unique_ptr would keep the participant alive and this case would prove nothing about
        // a dead one. Measured: with the pointer kept, deleting the line below is invisible to
        // ASan; with it released, deleting the line below is a heap-use-after-free.
        {
            const std::unique_ptr<loom::Weave> gone = t.bus.unregister_weave(t.terminal_id);
            CHECK(gone != nullptr);
        }
        t.host.terminal = nullptr;

        // Workshop keeps working, and now paints a pane with no participant behind it.
        t.text("z");
        CHECK_FALSE(t.pane().attached);
        CHECK(t.pane().shown.empty());
        CHECK(t.canvases.back().width == kMinScreen.w);
    }
    CHECK(held.front().text == "hello");
    CHECK(painted.find("> hello") != std::string::npos);
}


TEST_CASE("the overlay a maker actually sees: a solid pane, through the real rasterizer") {
    // THE CASE THE FIRST LIVE RASTERIZATION EARNED. Every check on the canvas passes with a
    // pane full of holes: a transcript row with no entry was simply not written, so the
    // backdrop's own glyph showed through into the workspace behind it, and only a picture
    // says so. This is that picture, pinned -- and it is a pure `paint()` of a hand-built
    // pane, so it needs no bus, no participant and no keystrokes to say it.
    WorkshopDoc d = two_panels();
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);
    s.notice = "this notice is under the pane and must not be painted";
    s.terminal.open = true;
    s.terminal.attached = true;
    s.terminal.id = loom::WeaveId{3};
    s.terminal.input.set("send @", 6);
    loom::TranscriptEntry typed;
    typed.kind = loom::TranscriptKind::LocalCommand;
    typed.text = "send @zengine.skin SurfaceText 1";
    s.terminal.shown.push_back(typed);

    const std::vector<std::string> rows = rasterized(paint(d, s));
    REQUIRE(rows.size() == static_cast<std::size_t>(kMinScreen.h));

    // THE PANE IS A SOLID BLOCK. One transcript row is filled and eight are empty, and an
    // EMPTY one is what the defect hid in: unwritten, it showed the backdrop's `.` straight
    // through into the workspace behind. Asserted as blank rather than as "no dots anywhere",
    // because a dot is perfectly legitimate CONTENT -- `zengine.skin` has one.
    const auto band = [&rows](std::int64_t y) {
        return rows[static_cast<std::size_t>(y)].substr(static_cast<std::size_t>(kMinScreen.terminal_x),
                                                        static_cast<std::size_t>(kMinScreen.terminal_w));
    };
    const std::string blank(static_cast<std::size_t>(kMinScreen.terminal_w), ' ');
    for (std::size_t i = 1; i < kMinScreen.terminal_rows; ++i) {
        CHECK(band(kMinScreen.terminal_y + 2 + static_cast<std::int64_t>(i)) == blank);
    }
    for (std::int64_t y = kMinScreen.terminal_y; y < kMinScreen.h; ++y) {
        CHECK(band(y).size() == static_cast<std::size_t>(kMinScreen.terminal_w));
    }

    // ...and it says the things it is for.
    CHECK(rows[static_cast<std::size_t>(kMinScreen.terminal_y)].find("TERMINAL -- weave #3") !=
          std::string::npos);
    CHECK(rows[static_cast<std::size_t>(kMinScreen.terminal_y) + 1].find(
              detail::fit(terminal_legend(), kMinScreen.terminal_cols)) != std::string::npos);
    CHECK(rows[static_cast<std::size_t>(kMinScreen.terminal_y) + 2].find("> send @zengine.skin") !=
          std::string::npos);
    CHECK(rows[static_cast<std::size_t>(kMinScreen.h) - 1].find("> send @_") != std::string::npos);

    // The workspace ABOVE the pane is untouched -- an overlay covers, it does not rearrange.
    CHECK(rows[3].rfind("..*panel", 0) == 0);
    // ...and the notice underneath it was not painted at all, in either half of its row.
    CHECK(rows[static_cast<std::size_t>(kMinScreen.notice_y)].find("this notice is under") ==
          std::string::npos);
}


// ---- tier 7: the surface's own extent (G-2) ----------------------------------------------
//
// Until G-2 the screen was 78x22 because a canvas publisher had no way to learn how much room
// its medium had. The Surface package now says so, and everything below is what a Workshop
// makes of the answer: where the extra columns and rows go, what a maker's authored work does
// while they arrive, and what a pane does with room it did not have before.

TEST_CASE("a bigger surface is a bigger workspace, not a bigger picture of a small one") {
    // THE PHASE'S CENTRAL CLAIM, as arithmetic. Every number here is derived in one place
    // (`screen_of`) and the minimum reproduces the composition that existed before.
    CHECK(kMinScreen.w == 78);
    CHECK(kMinScreen.h == 22);

    const Screen big = screen_of(100, 33);
    CHECK(big.w == 100);
    CHECK(big.h == 33);

    // THE WORKSPACE TAKES THE EXTRA ROOM. Twenty-two more columns of surface are twenty-two
    // more columns a maker can build in; eleven more rows are eleven more rows.
    CHECK(big.room_w == kMinScreen.room_w + 22);
    CHECK(big.room_h == kMinScreen.room_h + 11);

    // ...AND THE PANEL DOES NOT. Its width is a fact about how much of a name is worth
    // showing, so it is the same column count anchored to the new right edge.
    CHECK(big.w - big.panel_x == kMinScreen.w - kMinScreen.panel_x);
    CHECK(big.panel_x == 72);

    // The bottom band keeps its shape against the bottom edge.
    CHECK(big.h - big.notice_y == kMinScreen.h - kMinScreen.notice_y);
    CHECK(big.h - big.help_y == kMinScreen.h - kMinScreen.help_y);
    CHECK(big.help_y + 1 == big.h - 1); // the second help line is the last row

    // Nothing overlaps: the workspace ends, then a gap, then the panel.
    CHECK(big.room_w + kPanelGap == big.panel_x);
}

TEST_CASE("the screen's extent is TOTAL over whatever a medium published") {
    // It arrives as a ZEN_SHAPE off the bus, so its fields are whatever the sender put in
    // them -- W-1's lesson at the other end of the same telescope. Nothing below may produce
    // a negative extent, an inverted layout, or a multiply that leaves the number line.
    const std::int64_t hostile[] = {(std::numeric_limits<std::int64_t>::min)(),
                                    -1,
                                    0,
                                    1,
                                    kScreenMinW - 1,
                                    kScreenMaxW,
                                    kScreenMaxW + 1,
                                    (std::numeric_limits<std::int64_t>::max)()};
    // The metric is on the same wire and gets the same treatment (HD-1), so it is tried
    // against the same hostile set -- as its own axis rather than as a cross product with
    // the extent's. Sixty-four extents against sixty-four metrics is four thousand screens
    // and seventy thousand assertions to say a thing that neither axis needs the other to
    // say; a suite's assertion total is evidence about itself, and inflating it by two
    // orders of magnitude for one property is how that evidence stops meaning anything.
    const auto judge = [](const Screen& sc) {
        CHECK(sc.w >= kScreenMinW);
        CHECK(sc.w <= kScreenMaxW);
        CHECK(sc.h >= kScreenMinH);
        CHECK(sc.h <= kScreenMaxH);
        // and the furniture stays a layout rather than becoming a shape
        CHECK(sc.room_w >= kWorkspaceMinW);
        CHECK(sc.room_h >= 1);
        CHECK(sc.panel_x > 0);
        CHECK(sc.terminal_x >= 0);
        CHECK(sc.terminal_y >= 0);
        // THE RESERVATION, OVER EVERY EXTENT THIS SCREEN CAN BE ASKED FOR (HD-10). The pane's
        // right edge is the WORKSPACE's, so the reserved side column is never any part of it
        // -- said twice on purpose, because the two say different things: the first is where
        // the pane's edge IS, the second is the law that edge exists to keep, and a later
        // placement rule that satisfied one without the other would be caught by the other.
        CHECK(sc.terminal_x + sc.terminal_w == sc.room_w);
        CHECK(sc.terminal_x + sc.terminal_w <= sc.panel_x - kPanelGap);
        CHECK(sc.terminal_y + sc.terminal_h == sc.h);
        CHECK(sc.terminal_rows >= 1);
        CHECK(sc.notice_y < sc.h);
        CHECK(sc.help_y + 1 < sc.h);
        // THE PANE'S INTERIOR IS NEVER NOTHING. A medium could report a line taller than
        // the whole pane, and a pane with no rows is indistinguishable from a broken tool
        // -- so there is a floor, and `wrap` and `fit` are never handed a width of zero.
        CHECK(sc.terminal_cols >= kTerminalMinCols);
        CHECK(sc.terminal_lines >= sc.terminal_rows);
        CHECK(sc.terminal_lines - sc.terminal_rows == static_cast<std::size_t>(kTerminalChrome));
    };

    for (const std::int64_t w : hostile) {
        for (const std::int64_t h : hostile) {
            judge(screen_of(w, h));                 // no metric: the pre-HD-1 domain
            judge(screen_of(w, h, 8, 18));          // and the shipped face's
        }
    }
    // The metric's own axis, against the minimum screen and a large one -- an advance of
    // INT64_MIN and a line height of zero are as ordinary here as a width of -1.
    for (const std::int64_t advance : hostile) {
        for (const std::int64_t line : hostile) {
            judge(screen_of(kScreenMinW, kScreenMinH, advance, line));
            judge(screen_of(200, 90, advance, line));
        }
    }
}

TEST_CASE("the pane's interior follows the metric, and its placement does not") {
    // HD-1's central claim, as arithmetic. A medium that sets real type changes how much
    // PROSE the pane holds; it changes nothing about where the pane IS.
    const Screen cells = screen_of(78, 22);
    const Screen typed = screen_of(78, 22, 8, 18);

    CHECK(typed.terminal_x == cells.terminal_x);
    CHECK(typed.terminal_y == cells.terminal_y);
    CHECK(typed.terminal_w == cells.terminal_w);
    CHECK(typed.terminal_h == cells.terminal_h);

    // 48 cells is 576 device pixels; less the inset, at 8 px a character, that is 71
    // columns -- half again as much of every transcript line as the cell grid could show.
    // (56 and 83 before HD-10; the RATIO this case is about is a fact about the metric and
    // is what survives the pane getting eight cells narrower at this extent.)
    CHECK(cells.terminal_cols == 48);
    CHECK(typed.terminal_cols == 71);
    // ...and 156 pixels at an 18 px line is 8 rows where 13 cells used to be, which is the
    // measured cost of real type at the minimum window and is stated rather than hidden.
    CHECK(cells.terminal_lines == 13);
    CHECK(typed.terminal_lines == 8);
    CHECK(typed.terminal_rows == 8 - static_cast<std::size_t>(kTerminalChrome));

    // AND IT MOVES THE RIGHT WAY WITH THE WINDOW. Every pixel a person drags the edge by is
    // a pixel the metric spends on the record.
    const Screen bigger = screen_of(115, 63, 8, 18);
    CHECK(bigger.terminal_cols > typed.terminal_cols);
    CHECK(bigger.terminal_rows > typed.terminal_rows);

    // THE RESOLUTION IS THE SURFACE PACKAGE'S, NOT A SECOND COPY OF IT. This is what makes
    // "one measurer" a structural fact rather than a convention: the number the pane budgets
    // with is the number `fit_region` produces, and the graphical medium draws with the same
    // call on the same inputs.
    const surface::RegionFit fit = surface::fit_region(typed.terminal_x, typed.terminal_y,
                                                      typed.terminal_w, typed.terminal_h, 8, 18);
    CHECK(fit.columns == typed.terminal_cols);
    CHECK(static_cast<std::size_t>(fit.rows) == typed.terminal_lines);
}

// ============================================================================
// HD-2 — what this terminal can say next
// ============================================================================
//
// The pure half first (the model), then the pane (the picture), then the two
// claims that are about EFFECTS rather than about either: browsing authors
// nothing, and the list does not touch what the transcript says it is omitting.

namespace {

/// A participant with a vocabulary this file chose — so a case can pin the
/// duplicate-version behaviour the live Workshop's own catalog happens not to
/// exercise, without pretending the live catalog has one.
///
/// UNATTACHED, DELIBERATELY. Completion never authors, so it never needs a
/// channel; a session with none is the sharpest possible statement of that, and
/// every case below that only reads candidates uses one.
struct Vocab {
    static std::shared_ptr<const loom::Schema> ping(std::uint32_t version) {
        return loom::SchemaBuilder("Ping", version).field("seq", loom::Kind::Int).build();
    }
};

loom::TerminalVocabulary two_versions() {
    loom::TerminalVocabulary v;
    v.knows(Vocab::ping(1)).knows(Vocab::ping(2)).accepts(loom::schema_of<loom::Ack>());
    return v;
}

loom::TerminalVocabulary workshop_vocab() {
    loom::TerminalVocabulary v;
    v.knows(loom::schema_of<surface::SurfaceText>())
        .knows(loom::schema_of<surface::SurfaceCanvas>())
        .accepts(loom::schema_of<loom::Ack>());
    return v;
}

std::vector<std::string> displays(const Completion& c) {
    std::vector<std::string> out;
    for (const Candidate& k : c.candidates) {
        out.push_back(k.display);
    }
    return out;
}

} // namespace

TEST_CASE("a half-typed line says which part of it the maker is standing in") {
    // THE ONE THING A SUBMITTER NEVER HAS TO ASK, and the whole of what HD-2 added to the
    // grammar: not what the line SAYS but which slot the caret is in. The token positions
    // are `submit_terminal_line`'s own -- verb, address, shape, version, then arguments.
    CHECK(read_command_line("").slot == LineSlot::Verb);
    CHECK(read_command_line("se").slot == LineSlot::Verb);
    CHECK(read_command_line("se").partial == "se");
    // A SEPARATOR IS WHAT MOVES THE CARET ON. `send` is still the verb being typed; `send `
    // is a finished verb and an address about to start, and the difference is one space.
    CHECK(read_command_line("send").slot == LineSlot::Verb);
    CHECK(read_command_line("send ").slot == LineSlot::Address);
    CHECK(read_command_line("send ").partial.empty());
    CHECK(read_command_line("send #1").slot == LineSlot::Address);
    CHECK(read_command_line("send #1 ").slot == LineSlot::Shape);
    CHECK(read_command_line("send #1 Pi").slot == LineSlot::Shape);
    CHECK(read_command_line("send #1 Ping ").slot == LineSlot::Version);
    CHECK(read_command_line("send #1 Ping 2 ").slot == LineSlot::Arguments);
    CHECK(read_command_line("send #1 Ping 2 a=1 b=").slot == LineSlot::Arguments);

    // WHAT HAS ALREADY BEEN SAID, as far as it parses -- and `has_version` is a real
    // question rather than a formality: the fourth word is a `std::uint32_t` on the wire.
    const CommandLine done = read_command_line("send @office Ping 2 ");
    CHECK(done.verb == "send");
    CHECK(done.shape == "Ping");
    CHECK(done.has_version);
    CHECK(done.version == 2);
    CHECK_FALSE(read_command_line("send @office Ping x ").has_version);
    CHECK_FALSE(read_command_line("send @office Ping -1 ").has_version);
    CHECK_FALSE(read_command_line("send @office Ping 4294967296 ").has_version);
}

TEST_CASE("the verbs a maker is offered are the verbs the submitter runs") {
    // ONE TABLE, TWO CONSUMERS. `submit_terminal_line` resolves through `terminal_verb`
    // and reads `ask`; the completer lists the same rows. A third verb is one line here
    // and cannot be learned by only one of them -- which is the whole reason the two
    // string literals became a table.
    REQUIRE(kTerminalVerbCount == 2);
    REQUIRE(terminal_verb("send") != nullptr);
    REQUIRE(terminal_verb("ask") != nullptr);
    CHECK_FALSE(terminal_verb("send")->ask);
    CHECK(terminal_verb("ask")->ask);
    // EXACT, NEVER A PREFIX. An abbreviation that ran a different verb than the maker
    // typed is the one convenience this pane must not have.
    CHECK(terminal_verb("sen") == nullptr);
    CHECK(terminal_verb("") == nullptr);
    CHECK(terminal_verb("SEND") == nullptr);

    loom::TerminalSession me("t", workshop_vocab());
    CHECK(displays(complete_line(me, "")) == std::vector<std::string>{"send", "ask"});
    CHECK(displays(complete_line(me, "a")) == std::vector<std::string>{"ask"});
    CHECK(displays(complete_line(me, "s")) == std::vector<std::string>{"send"});
    // ...and every candidate's INSERT is what the submitter would then read as that verb.
    for (const Candidate& c : complete_line(me, "").candidates) {
        const std::vector<loom::Token> tok = loom::tokenize(c.insert);
        REQUIRE(tok.size() == 1);
        CHECK(terminal_verb(tok[0].text) != nullptr);
    }
}

TEST_CASE("an address offers the three forms and never pretends to know the values") {
    loom::TerminalSession me("t", workshop_vocab());

    // THE THREE FORMS, and the two of them that are FORMS say so. This participant holds
    // no weave directory and no role directory -- deliberately, session.hpp -- so `#12`
    // and `@office` are shapes of an answer and never a list of answers.
    const Completion at = complete_line(me, "send ");
    CHECK(displays(at) == std::vector<std::string>{"*", "#<id>", "@<office>"});
    CHECK(at.candidates[0].insert == "* ");
    CHECK(at.candidates[1].insert == "#"); // no separator: an id follows immediately
    CHECK(at.candidates[2].insert == "@");
    CHECK(at.candidates[1].detail.find("cannot list") != std::string::npos);

    // A SIGIL ALREADY CHOSEN IS SILENCE, NOT A REFUSAL. `#1` is a perfectly good address
    // and a list that answered "no match" to it would be wrong; what the heading offers
    // instead is Loom's own grammar (`parse_address`) saying whether it is one YET.
    const Completion typed = complete_line(me, "send #1");
    CHECK(typed.candidates.empty());
    CHECK(typed.heading == "'#1' is an address");
    CHECK(complete_line(me, "send #").heading.find("not an address yet") != std::string::npos);
    CHECK(complete_line(me, "send @").heading.find("not an address yet") != std::string::npos);
    CHECK(complete_line(me, "send @skin").heading == "'@skin' is an address");

    // A BAREWORD IS NONE OF THE THREE, and that is the one address answer that IS a
    // refusal -- `12` is not an address, because the sigil is what says which kind it is.
    const Completion bare = complete_line(me, "send 12");
    CHECK(bare.candidates.empty());
    CHECK(bare.heading.find("#12, @office or *") != std::string::npos);
}

TEST_CASE("shape candidates are the catalog, in the host's order, and versions stay apart") {
    loom::TerminalSession me("t", workshop_vocab());

    // THE HOST'S DECLARED ORDER, preserved -- no ranking, no sorting, no learned order.
    const Completion all = complete_line(me, "send * ");
    CHECK(displays(all) == std::vector<std::string>{"SurfaceText v1", "SurfaceCanvas v4",
                                                    "zen.Ack v1"});
    // ACCEPTANCE WRITES THE VERSION TOO, because a shape without one is never a command
    // this pane can run: the grammar wants four words and the version is the fourth.
    CHECK(all.candidates[0].insert == "SurfaceText 1 ");
    CHECK(all.candidates[0].detail.find("slot:Text") != std::string::npos);

    // CASE FOLLOWS THE WIRE. A schema name is identity; matching `surfacetext` against
    // `SurfaceText` would offer a completion that composes to UnknownShape.
    CHECK(displays(complete_line(me, "send * Surface")) ==
          std::vector<std::string>{"SurfaceText v1", "SurfaceCanvas v4"});
    CHECK(complete_line(me, "send * surface").candidates.empty());
    CHECK(complete_line(me, "send * surface").heading.find("(3 known)") != std::string::npos);

    // A DOOR IS THE ONE AUTHORITY-ADJACENT FACT THAT IS HONESTLY KNOWABLE, and it is
    // about the direction that cannot be mistaken for permission: `zen.Ack` may ARRIVE
    // here. Whether any of these may be SENT is the Kernel's answer at delivery.
    CHECK_FALSE(all.candidates[0].door);
    CHECK(all.candidates[2].door);
    CHECK(all.candidates[2].detail.find("[door]") != std::string::npos);
    CHECK(all.heading.find("knowing one is not authority to send it") != std::string::npos);

    // TWO VERSIONS OF ONE NAME STAY TWO ANSWERS. The Workshop host's own catalog happens
    // to hold no such pair, so this is the vocabulary a test chose -- and the mechanism is
    // the thing being pinned, not the host's inventory.
    loom::TerminalSession twice("t", two_versions());
    const Completion pings = complete_line(twice, "send * Pi");
    CHECK(displays(pings) == std::vector<std::string>{"Ping v1", "Ping v2"});
    CHECK(pings.candidates[0].insert == "Ping 1 ");
    CHECK(pings.candidates[1].insert == "Ping 2 ");

    // ...and the VERSION slot answers the same question for a hand-typed name.
    CHECK(displays(complete_line(twice, "send * Ping ")) == std::vector<std::string>{"v1", "v2"});
    CHECK(displays(complete_line(twice, "send * Ping 2")) == std::vector<std::string>{"v2"});
    CHECK(complete_line(twice, "send * Pong ").candidates.empty());
}

TEST_CASE("arguments offer field NAMES, never values, and the heading is compose()'s verdict") {
    loom::TerminalSession me("t", workshop_vocab());

    const Completion fresh = complete_line(me, "send * SurfaceText 1 ");
    CHECK(displays(fresh) == std::vector<std::string>{"slot=", "text="});
    // NO TRAILING SEPARATOR on a field, because a value follows immediately -- the one
    // slot where the grammar's separator rule differs, honoured per slot.
    CHECK(fresh.candidates[0].insert == "slot=");
    CHECK(fresh.candidates[0].detail.find("required") != std::string::npos);
    // THE HEADING IS THE LADDER'S OWN VERDICT, run over the arguments already finished.
    CHECK(fresh.heading == "SurfaceText v1 -- missing: slot, text");

    // ONCE THERE IS AN `=` THE MAKER IS TYPING A VALUE, and this file has nothing to say
    // about values. The preview stays; the list stops.
    const Completion valuing = complete_line(me, "send * SurfaceText 1 slot=sc");
    CHECK(valuing.candidates.empty());
    CHECK(valuing.heading == "SurfaceText v1 -- missing: slot, text");

    // A FIELD ALREADY NAMED IS NOT OFFERED AGAIN: the composer refuses a double
    // assignment, and offering one would be offering a command that cannot run.
    CHECK(displays(complete_line(me, "send * SurfaceText 1 slot=score ")) ==
          std::vector<std::string>{"text="});
    CHECK(complete_line(me, "send * SurfaceText 1 slot=score ").heading ==
          "SurfaceText v1 -- missing: text");

    // READY IS A REAL VERDICT AND IT MEANS WHAT SUBMISSION WILL MEAN, because it is the
    // same ladder -- `compose()` stopped one step before anything is authored.
    CHECK(complete_line(me, "send * SurfaceText 1 slot=score text=hi ").heading ==
          "SurfaceText v1 -- ready; Return submits it");
    CHECK(complete_line(me, "send * SurfaceText 1 score hi ").heading ==
          "SurfaceText v1 -- ready; Return submits it");

    // ...and an ERROR is the ladder's words, not a second vocabulary invented here.
    CHECK(complete_line(me, "send * SurfaceText 1 nope=1 ").heading.find("no field 'nope'") !=
          std::string::npos);
    // A shape this terminal does not know has no fields to offer and says so.
    CHECK(complete_line(me, "send * Nothing 1 ").candidates.empty());
    CHECK(complete_line(me, "send * Nothing 1 ").heading.find("does not know Nothing v1") !=
          std::string::npos);
    // A version that is not a number never reaches the ladder at all.
    CHECK(complete_line(me, "send * SurfaceText x ").heading.find("fourth word") !=
          std::string::npos);
}

TEST_CASE("a quoted token is left alone, because the quote is not on the line the completer sees") {
    // `loom::tokenize` drops the quote characters, so the partial this completer can see
    // (`Sur`) is not the text on the line (`"Sur`). Replacing one with the other would
    // leave a dangling quote in a line the maker can no longer see the whole of.
    loom::TerminalSession me("t", workshop_vocab());
    const Completion quoted = complete_line(me, "send * \"Sur");
    CHECK_FALSE(quoted.open);
    CHECK(quoted.candidates.empty());
    CHECK(quoted.heading.empty());
    // ...and the same prefix unquoted is completed normally, which is what says the
    // refusal is about the quote rather than about the text.
    CHECK(complete_line(me, "send * Sur").candidates.size() == 2);
}

TEST_CASE("browsing candidates authors NOTHING -- no traffic, no ask, no transcript entry") {
    // THE PHASE'S LOAD-BEARING NEGATIVE. Completion runs on every keystroke against a
    // participant that CAN author, so the claim has to be measured against a real bus with
    // a real listener rather than argued from the const qualifiers alone.
    Live t;
    SkinSeat* seat = t.mount_skin_seat();
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    // COUNTED BY SENDER, and that is not a nicety. Workshop's own weave publishes a
    // status line on every repaint and it lands on the same office, so a bare count of
    // what the seat heard measures how many times the screen was painted. `mail.sender()`
    // is the BUS STAMP -- it cannot be written by a payload and cannot be chosen by
    // whoever composed the message -- which is what makes "did the PARTICIPANT speak" a
    // different question from "did anything arrive".
    const auto from_participant = [&] {
        std::size_t n = 0;
        for (const loom::WeaveId& who : seat->from) {
            if (who == t.terminal_id) {
                ++n;
            }
        }
        return n;
    };
    REQUIRE(from_participant() == 0);
    const std::size_t record_before = me->transcript().size();

    // Type a whole command, one character at a time, and browse at every stage: the
    // verb list, the address forms, the shapes, the versions, the fields. Move the
    // selection, accept candidates, dismiss, ask again.
    // Addressed to the ONE office this participant's grant names, so the submission at
    // the end is a real authored delivery rather than a refusal that would prove nothing.
    const auto browse = [&](const std::string& text) {
        for (const char ch : text) {
            t.text(std::string(1, ch));
            t.key(input::scan::kDown);
            t.key(input::scan::kUp);
        }
    };
    browse("send ");
    t.key(input::scan::kEscape); // the address list is showing: dismissed, line untouched
    t.key(input::scan::kTab);    // nothing showing: asked for again, line untouched
    REQUIRE(t.pane().input.text() == "send ");
    browse("@zengine.skin SurfaceText 1 slot=score text=hi");
    t.bus.pump();

    CHECK(from_participant() == 0); // nothing of ITS landed on the office it may reach
    CHECK(me->outstanding() == 0);  // no ask was created
    CHECK_FALSE(me->awaiting());
    CHECK(me->pending().empty());
    // AND THE PARTICIPANT'S OWN RECORD IS UNTOUCHED. The transcript is where an authored
    // act would appear even if nothing were listening, so this is the check that does not
    // depend on anybody being there to hear it.
    CHECK(me->transcript().size() == record_before);
    CHECK(of_kind(*me, loom::TranscriptKind::Submitted).empty());
    CHECK(of_kind(*me, loom::TranscriptKind::LocalCommand).empty());

    // ...AND THE ORDINARY SUBMISSION STILL AUTHORS, which is what makes the zeros above a
    // measurement rather than a broken pane. The canary: delete the send and this fails.
    t.key(input::scan::kReturn);
    t.bus.pump();
    CHECK(from_participant() == 1);
    CHECK(of_kind(*me, loom::TranscriptKind::Submitted).size() == 1);
    CHECK(of_kind(*me, loom::TranscriptKind::LocalCommand).size() == 1);
}

TEST_CASE("accepting a candidate edits the line, and the grammar's separators stay right") {
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();

    // AN END-OF-LINE EDIT, which is what makes this compatible with the caret this pane
    // actually has: the token being completed is the last one and the caret is at the end,
    // so accepting is "drop what has been typed of this token, append what it was to be".
    t.text("s");
    t.key(input::scan::kTab);
    CHECK(t.pane().input.text() == "send ");

    // A FORM, ACCEPTED WITH NO SEPARATOR, because an id follows it immediately. This is
    // the address slot on purpose: it is the one this fixture's vocabulary makes three
    // candidates deep, so the selection has somewhere to move.
    REQUIRE(t.pane().completion.candidates.size() == 3);
    t.key(input::scan::kDown);
    CHECK(t.pane().completion.selected == 1);
    t.key(input::scan::kTab);
    CHECK(t.pane().input.text() == "send #");
    t.text("7 ");

    // A SHAPE, ACCEPTED WITH ITS VERSION, because four words is what the grammar wants.
    t.text("Surface");
    CHECK(t.pane().completion.slot == LineSlot::Shape);
    t.key(input::scan::kTab);
    CHECK(t.pane().input.text() == "send #7 SurfaceText 1 ");
    t.key(input::scan::kTab); // the first field
    CHECK(t.pane().input.text() == "send #7 SurfaceText 1 slot=");

    // NO SEPARATOR WAS DUPLICATED AND NONE WAS SWALLOWED -- the line still tokenizes to
    // exactly the words that were meant, which is the only definition of that claim that
    // does not depend on counting spaces by eye.
    const std::vector<loom::Token> tok = loom::tokenize(t.pane().input.text());
    REQUIRE(tok.size() == 5);
    CHECK(tok[0].text == "send");
    CHECK(tok[1].text == "#7");
    CHECK(tok[2].text == "SurfaceText");
    CHECK(tok[3].text == "1");
    CHECK(tok[4].text == "slot=");

    // AND SURROUNDING AUTHORED TEXT SURVIVES. Accepting a shape after an address the maker
    // typed by hand leaves the address exactly as they wrote it.
    t.key(input::scan::kEscape); // dismiss
    t.key(input::scan::kEscape); // clear the line
    CHECK(t.pane().input.empty());
    for (const char ch : std::string("ask @loom.weaver Surface")) {
        t.text(std::string(1, ch));
    }
    t.key(input::scan::kTab);
    CHECK(t.pane().input.text() == "ask @loom.weaver SurfaceText 1 ");
}

TEST_CASE("the completion keys were unbound in this mode, and the ones that were not still work") {
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();

    // TAB ON AN EMPTY LINE IS THE ONE GESTURE DISCOVERY NEEDS. Typing is the other entry
    // point, and an untouched line asks nothing -- see the case below on why.
    CHECK_FALSE(t.pane().completion.open);
    t.key(input::scan::kTab);
    CHECK(t.pane().completion.open);
    CHECK(t.pane().completion.candidates.size() == 2);
    CHECK(t.pane().completion.selected == 0);

    // UP AND DOWN MOVE AND DO NOT WRAP. A list that wrapped would answer Up on the first
    // row by scrolling the whole thing out from under the maker's eye.
    t.key(input::scan::kUp);
    CHECK(t.pane().completion.selected == 0);
    t.key(input::scan::kDown);
    CHECK(t.pane().completion.selected == 1);
    t.key(input::scan::kDown);
    CHECK(t.pane().completion.selected == 1);
    t.key(input::scan::kUp);
    CHECK(t.pane().completion.selected == 0);
    // ...and neither touched the line.
    CHECK(t.pane().input.empty());

    // ESCAPE DISMISSES THE LIST AND LEAVES THE LINE; a second Escape clears the line;
    // NEITHER closes the pane, which is the property that makes this key safe to press.
    t.text("s");
    CHECK(t.pane().completion.candidates.size() == 1);
    t.key(input::scan::kEscape);
    CHECK(t.pane().input.text() == "s");
    CHECK(t.pane().dismissed);
    CHECK(list_of(t.canvases.back(), kMinScreen) == nullptr); // gone from the picture
    t.key(input::scan::kEscape);
    CHECK(t.pane().input.empty());
    CHECK(t.pane().open);

    // A DISMISSAL BELONGS TO THE PART OF THE LINE IT WAS MADE IN. More of the same word
    // leaves it dismissed; the next word is a new question and brings the list back.
    CHECK_FALSE(t.pane().dismissed); // clearing the line abandoned the dismissal with it
    for (const char ch : std::string("sen")) {
        t.text(std::string(1, ch));
    }
    t.key(input::scan::kEscape);
    CHECK(t.pane().dismissed);
    t.text("d");
    CHECK(t.pane().dismissed); // still the verb
    t.text(" ");
    CHECK_FALSE(t.pane().dismissed); // the address is a different question
    CHECK(list_of(t.canvases.back(), kMinScreen) != nullptr);

    // AND THE THREE KEYS THIS MODE ALREADY OWNED ARE UNCHANGED.
    t.key(input::scan::kBackspace);
    CHECK(t.pane().input.text() == "send");
    t.key(input::scan::kReturn);
    CHECK(t.pane().input.empty());
    t.toggle_terminal();
    CHECK_FALSE(t.pane().open); // shift+space still closes it
}

TEST_CASE("an untouched line asks nothing, so the answer to the last command stays readable") {
    // MEASURED, NOT REASONED. Submitting clears the line, so "show candidates whenever
    // there are any" put the verb list on top of the reply to the command just typed --
    // found by the case that asserts the pane states its whole grammar with nothing
    // elided, which began finding `...` in a pane that had elided nothing.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    t.type_line("help");
    t.bus.pump();
    CHECK(t.pane().input.empty());
    CHECK_FALSE(t.pane().completion.open);
    CHECK(list_of(t.canvases.back(), kMinScreen) == nullptr);

    // ...and the gesture that asks anyway is on the input row, where it erases itself the
    // moment there is anything to erase it.
    const surface::SurfaceTextRegion& pane = *pane_of(t.canvases.back(), kMinScreen);
    CHECK(pane.rows.back().text.find("tab: what can this terminal say?") != std::string::npos);
    t.text("s");
    CHECK(pane_of(t.canvases.back(), kMinScreen)->rows.back().text.rfind("> s", 0) == 0);
    // ...AND THE CELL PROJECTION STILL DRAWS THE SAME PICTURE IT ALWAYS DID (HD-3). The
    // publisher stopped appending `_` and the projection started inserting it at the
    // caret's column, which for a caret at the end of the line is the identical byte in the
    // identical place -- said here rather than argued, because "unchanged" is a claim.
    surface::SurfaceCanvas only_pane = t.canvases.back();
    // The list HD-2 raised over it and the property body HD-6 bounded are other regions.
    only_pane.texts = {*pane_of(only_pane, kMinScreen)};
    const std::vector<surface::ProjectedRow> shown = surface::project_text_regions(only_pane);
    CHECK(shown.back().label.text.rfind("> s_", 0) == 0);
}

TEST_CASE("the list is a bounded region inside the pane, and never over the input line") {
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    t.text("s");

    const surface::SurfaceCanvas& c = t.canvases.back();
    const surface::SurfaceTextRegion& pane = *pane_of(c, kMinScreen);
    const surface::SurfaceTextRegion* list = list_of(c, kMinScreen);
    REQUIRE(list != nullptr);

    // INSIDE THE PANE'S OWN BOUNDS, on every edge. The Terminal owns this interior, which
    // is what makes an internal overlay allowed without a z-order framework anywhere.
    CHECK(list->x >= pane.x);
    CHECK(list->y > pane.y); // below the header naming the identity
    CHECK(list->x + list->w <= pane.x + pane.w);
    CHECK(list->y + list->h <= pane.y + pane.h);

    // AND ABOVE THE TWO ROWS THE PANE ALWAYS SPENDS ON ITSELF. Those are cell rows here
    // because this medium has no metric; the same claim under a real metric is the case
    // below, and it is the one where the two lattices differ.
    const Screen sc = screen_of(t.session());
    CHECK(list->y + list->h <= sc.terminal_y +
                                   static_cast<std::int64_t>(sc.terminal_lines) - 2);

    // THE LIST IS LAST, so it is the topmost thing on the canvas -- painter's order across
    // `texts` is list order, the same rule every other list already states. (The Info
    // panel's body is the FIRST since HD-6: the panels are painted, then the overlay.)
    CHECK(&c.texts.back() == list);
    CHECK(c.texts.front().y == body_place(t).region_y);
    CHECK(list->rows[0].text.rfind("verbs", 0) == 0);
    CHECK(list->rows[1].text.rfind("> send", 0) == 0);

    // THE SELECTED ROW IS UNAMBIGUOUS IN BOTH DIRECTIONS A MEDIUM MIGHT HAVE: a ground for
    // one that paints, and a marker for one that has only characters. Colour alone would
    // be a lie on a monochrome terminal, which is the argument `glyph_for_role` already
    // makes in the Skin next door.
    CHECK(list->rows[1].background == surface::role::kMuted);
    CHECK(list->rows[1].role == surface::role::kAccent);
    CHECK(list->rows[0].background == surface::role::kNone);
    for (std::size_t i = 2; i < list->rows.size(); ++i) {
        CHECK(list->rows[i].background == surface::role::kNone);
        CHECK(list->rows[i].text.rfind("  ", 0) == 0);
    }

    // AND IT DISAPPEARS CLEANLY. Nothing is left behind on the next repaint.
    t.key(input::scan::kEscape);
    CHECK(list_of(t.canvases.back(), kMinScreen) == nullptr);
}

TEST_CASE("the list clears the input line under a real metric too, where a row is not a cell") {
    // THE CASE THE TWO LATTICES MAKE INTERESTING. A region is placed in CELLS and filled
    // in PROSE ROWS, and under a real metric the pane's input row begins part-way down
    // some cell -- so "above the input line" is arithmetic rather than a subtraction.
    for (const std::int64_t line_px : {14, 18, 23, 31}) {
        const Screen sc = screen_of(140, 60, 8, line_px);
        REQUIRE(sc.terminal_lines >= kTerminalChrome + 1);
        const CompletionPlace p = completion_place(sc, 6);
        REQUIRE(p.visible);
        CHECK(p.x == sc.terminal_x);
        CHECK(p.w == sc.terminal_w);
        CHECK(p.y > sc.terminal_y);
        CHECK(p.y + p.h <= sc.terminal_y + sc.terminal_h);

        // The pane's omission row and its input row both begin below the list's last
        // pixel -- which is the claim, in the unit the medium actually draws in.
        const surface::RegionFit pane = surface::fit_region(
            sc.terminal_x, sc.terminal_y, sc.terminal_w, sc.terminal_h, 8, line_px);
        const std::int64_t omission_top_px =
            surface::px_of_cells(sc.terminal_y) + pane.origin_y +
            (static_cast<std::int64_t>(sc.terminal_lines) - 2) * pane.line_px;
        CHECK(surface::px_of_cells(p.y + p.h) <= omission_top_px);

        // ...and the list's own interior was resolved with the SAME metric, so what it
        // says it can show is what a medium will draw.
        const surface::RegionFit fit =
            surface::fit_region(p.x, p.y, p.w, p.h, 8, line_px);
        CHECK(static_cast<std::size_t>(fit.rows) == p.rows);
    }
}

TEST_CASE("the list says which slice of a long vocabulary it is showing") {
    // A LIST THAT SCROLLED WITHOUT SAYING SO WOULD BE THE OMISSION LIE ONE REGION OVER.
    Completion comp;
    comp.heading = "shapes";
    for (int i = 0; i < 9; ++i) {
        Candidate c;
        c.display = "S" + std::to_string(i);
        comp.candidates.push_back(c);
    }

    // Room for the heading and three candidates.
    const std::vector<surface::SurfaceTextRow> top = completion_rows(comp, 4, 60);
    REQUIRE(top.size() == 4);
    CHECK(top[0].text.rfind("1-3 of 9  shapes", 0) == 0);
    CHECK(top[1].text.rfind("> S0", 0) == 0);

    // WINDOWED AROUND THE SELECTION, so a maker on the seventh candidate can see it.
    comp.selected = 6;
    const std::vector<surface::SurfaceTextRow> mid = completion_rows(comp, 4, 60);
    REQUIRE(mid.size() == 4);
    CHECK(mid[0].text.rfind("5-7 of 9  shapes", 0) == 0);
    CHECK(mid[3].text.rfind("> S6", 0) == 0);

    // AND THE SLICE THAT IS NOTHING AT ALL still says so, which is what a pane too short
    // for one candidate row shows.
    const std::vector<surface::SurfaceTextRow> none = completion_rows(comp, 1, 60);
    REQUIRE(none.size() == 1);
    CHECK(none[0].text.rfind("none of 9  shapes", 0) == 0);

    // A vocabulary that FITS says nothing about slices at all.
    comp.candidates.resize(2);
    comp.selected = 0;
    CHECK(completion_rows(comp, 4, 60)[0].text.rfind("shapes", 0) == 0);
}

TEST_CASE("the list covers transcript rows and changes nothing about what the pane omits") {
    // THE HD-2 §22 CLAIM, and the reason the list is a second region rather than rows
    // taken out of the pane's own budget: covering and taking are different acts, and
    // only one of them leaves "... N earlier" meaning what it meant.
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    for (int i = 0; i < 30; ++i) {
        me->record_notice("entry number " + std::to_string(i));
    }
    t.key(input::scan::kBackspace); // a no-op that refreshes the snapshot
    const std::uint64_t earlier = t.pane().earlier;
    const std::size_t shown = t.pane().shown.size();
    const std::string omission = terminal_omission(t.pane());
    REQUIRE(earlier > 0);

    t.text("s"); // the list opens over the transcript
    REQUIRE(list_of(t.canvases.back(), kMinScreen) != nullptr);
    CHECK(t.pane().earlier == earlier);
    CHECK(t.pane().shown.size() == shown);
    CHECK(terminal_omission(t.pane()) == omission);

    // ...and the pane's OWN region is unchanged: same rows, same words, same count. The
    // list is on top of it, not inside it.
    const Screen sc = screen_of(t.session());
    const surface::SurfaceTextRegion& pane = *pane_of(t.canvases.back(), sc);
    CHECK(pane.rows.size() == sc.terminal_lines);
    CHECK(pane.rows[sc.terminal_lines - 2].text.rfind(omission, 0) == 0);
}

TEST_CASE("the terminal medium projects the list honestly, ground and all") {
    // A CHARACTER MEDIUM HAS ONE ATTRIBUTE PER CELL AND NOW HAS TWO. What it does with a
    // ground is its own answer -- an SGR background -- and what it does with the rest is
    // exactly what it did before, which is why every existing golden is unmoved.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    t.text("s");
    const surface::SurfaceCanvas& c = t.canvases.back();
    const surface::SurfaceTextRegion* list = list_of(c, kMinScreen);
    REQUIRE(list != nullptr);

    const std::vector<std::string> rows = rasterized(c);
    const std::size_t heading_row = static_cast<std::size_t>(list->y);
    CHECK(rows[heading_row].substr(static_cast<std::size_t>(list->x), 5) == "verbs");
    CHECK(rows[heading_row + 1].substr(static_cast<std::size_t>(list->x), 6) == "> send");

    // THE GROUND IS IN THE BYTES, at the start of the selected row's run, and is put back
    // afterwards. `\x1b[100m` is bright black -- the selection bar.
    const std::string body = surface::canvas_body(c);
    CHECK(body.find("\x1b[100m") != std::string::npos);
    // ...and the row above it asked for none, so nothing between them says otherwise.
    const std::size_t verbs = body.find("verbs");
    REQUIRE(verbs != std::string::npos);
    CHECK(verbs < body.find("\x1b[100m", verbs));

    // EXACTLY ONE ROW OF THE LIST WEARS IT, which is the claim this case owns and is asserted
    // over the LIST's rows rather than over the whole canvas. Since HD-9 the Info panel below
    // sets its `PROPERTIES` heading and its available controls on the same ground, so a
    // whole-canvas count would now be counting somebody else's rows.
    std::size_t grounded = 0;
    for (const surface::SurfaceTextRow& row : list->rows) {
        if (row.background != surface::role::kNone) {
            ++grounded;
        }
    }
    CHECK(grounded == 1);

    // AND CLOSING THE LIST TAKES ITS GROUND WITH IT -- nothing is left behind on the next
    // repaint. (The canvas still carries the Info panel's, which is why this asks the list.
    // The claim that a canvas with NO ground emits not one background byte is owned by
    // test_surface.cpp, over a canvas built by hand for exactly that question.)
    t.key(input::scan::kEscape);
    CHECK(list_of(t.canvases.back(), kMinScreen) == nullptr);
    const Screen closed = screen_of(t.session());
    for (const surface::SurfaceTextRow& row : pane_of(t.canvases.back(), closed)->rows) {
        CHECK(row.background == surface::role::kNone); // and the pane never had one
    }
}

TEST_CASE("taking the room refits the workspace, and says whether anything moved") {
    Session s;
    CHECK(s.screen_w == kScreenMinW);
    CHECK(s.workspace_w == kWorkspaceW);

    CHECK(adopt_screen(s, 100, 33));
    CHECK(s.screen_w == 100);
    CHECK(s.workspace_w == screen_of(100, 33).room_w);
    CHECK(s.workspace_h == screen_of(100, 33).room_h);

    // THE SAME EXTENT AGAIN IS NOT A CHANGE. It is what lets a caller decline to repaint a
    // screen nothing happened to -- and it is what makes the clamps safe to state, because
    // two different extents that clamp to one screen are one screen.
    CHECK_FALSE(adopt_screen(s, 100, 33));
    CHECK_FALSE(adopt_screen(s, 100, 33));

    // Below the minimum is not a smaller screen; it is the minimum.
    CHECK(adopt_screen(s, 4, 4));
    CHECK(s.screen_w == kScreenMinW);
    CHECK(s.screen_h == kScreenMinH);
    CHECK(s.workspace_w == kWorkspaceW);
    CHECK_FALSE(adopt_screen(s, 0, 0));
    CHECK_FALSE(adopt_screen(s, -9, -9));
}

TEST_CASE("a maker's authored work keeps its place while the surface grows") {
    // WHAT MUST NOT MOVE. An authored cell coordinate is a fact the maker wrote down; a share
    // is a fact about its context. Growing the surface changes the CONTEXT and nothing else,
    // which is the authored/resolved discipline meeting a window edge.
    WorkshopDoc d = two_panels();
    const ui::Element before_share = d.elements[0]; // 60% wide
    const ui::Element before_cells = d.elements[1]; // 14 cells wide, at 6,10

    Session small;
    Session large;
    REQUIRE(adopt_screen(large, 100, 33));

    const ui::Scene s1 = workspace_scene(d, small);
    const ui::Scene s2 = workspace_scene(d, large);

    // Nothing authored changed. Not one field.
    CHECK(d.elements[0] == before_share);
    CHECK(d.elements[1] == before_cells);

    // The cell-authored object is in the same cell, the same size, on both screens.
    const ui::Placed* p1 = ui::placed_for(s2, before_cells.id);
    REQUIRE(p1 != nullptr);
    CHECK(p1->rect.x == 6);
    CHECK(p1->rect.y == 10);
    CHECK(p1->rect.w == 14);
    CHECK(p1->rect.h == 4);
    CHECK(*p1 == *ui::placed_for(s1, before_cells.id));

    // ...and the SHARE resolves to more cells, because 60% of a wider workspace is wider.
    // That is the whole reason a share is a different kind of value from a cell count.
    const ui::Placed* q1 = ui::placed_for(s1, before_share.id);
    const ui::Placed* q2 = ui::placed_for(s2, before_share.id);
    REQUIRE(q1 != nullptr);
    REQUIRE(q2 != nullptr);
    CHECK(q1->rect.w == 28);
    CHECK(q2->rect.w > q1->rect.w);
    CHECK(q2->rect.x == q1->rect.x); // its authored position did not move
}

TEST_CASE("the surface says how much room it has, and Workshop paints that much") {
    // END TO END, on a real bus: the one message that travels medium -> application, and the
    // canvas that comes back out.
    Live t;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE_FALSE(t.canvases.empty());
    CHECK(t.canvases.back().width == kMinScreen.w);
    CHECK(t.canvases.back().height == kMinScreen.h);

    const std::size_t before = t.canvases.size();
    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    REQUIRE(t.canvases.size() > before);

    const surface::SurfaceCanvas& c = t.canvases.back();
    const Screen sc = screen_of(t.session());
    CHECK(c.width == 100);
    CHECK(c.height == 33);
    // MORE USABLE SURFACE, not a stretched picture: the workspace rectangle a maker builds
    // inside is genuinely bigger, in cells.
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, 70, 27, surface::role::kMuted));
    CHECK(sc.room_w == 70);
    CHECK(label_at(c, 0, 0) == "WORKSPACE 70x27 cells");
    // The panel came with the right edge rather than staying at column 50.
    CHECK(label_at(c, sc.panel_x, kInfoBodyY - 1) == "OBJECTS");
    CHECK(properties_heading(c, t.doc(), t.session()) == "PROPERTIES");
    CHECK(sc.panel_x == 72);
    // ...and the hint that was twenty cells from the right edge still is.
    CHECK(label_at(c, c.width - 20, 0) == "shift+space terminal");

    // AN EXTENT THAT CHANGES NOTHING REPAINTS NOTHING. Two different extents clamp to one
    // screen, and a maker dragging across that boundary must not see the tool flicker.
    const std::size_t settled = t.canvases.size();
    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    CHECK(t.canvases.size() == settled);
    t.publish(loom::to_value(surface::SurfaceExtent{2, 2}));
    REQUIRE(t.canvases.size() > settled);
    CHECK(t.canvases.back().width == kMinScreen.w); // back to the minimum, not to 2
    t.publish(loom::to_value(surface::SurfaceExtent{1, 1}));
    CHECK(t.canvases.size() == settled + 1);

    // An absurd one is bounded before any arithmetic touches it.
    t.publish(loom::to_value(surface::SurfaceExtent{
        (std::numeric_limits<std::int64_t>::max)(), (std::numeric_limits<std::int64_t>::max)()}));
    CHECK(t.canvases.back().width == kScreenMaxW);
    CHECK(t.canvases.back().height == kScreenMaxH);
}

TEST_CASE("`]` reaches the room a bigger surface gave, and `[` still narrows") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    CHECK(t.session().workspace_w == 70);

    // The ceiling moved with the screen: before G-2 this stopped at 48.
    for (int i = 0; i < 60; ++i) {
        t.key(input::scan::kRightBracket);
    }
    CHECK(t.session().workspace_w == 70);
    CHECK(t.notice().find("70 cells wide") != std::string::npos);
    CHECK(t.notice().find("authored values unchanged") != std::string::npos);

    // ...and the floor did not.
    for (int i = 0; i < 100; ++i) {
        t.key(input::scan::kLeftBracket);
    }
    CHECK(t.session().workspace_w == kWorkspaceMinW);

    // A NARROWING DOES NOT SURVIVE A RESIZE, and that is the stated trade rather than an
    // oversight: `[` and a hand on the window edge are the same sentence, and the one that
    // happened last wins. There is no second remembered width for them to disagree about.
    t.publish(loom::to_value(surface::SurfaceExtent{120, 33}));
    CHECK(t.session().workspace_w == screen_of(120, 33).room_w);
}

TEST_CASE("a run no medium measures is exactly the run Workshop had before") {
    // THE DETERMINISTIC FALLBACK, from the application's side. Since TUI-0 a terminal skin
    // DOES have an opinion when there is a terminal to measure -- but a run whose output is
    // a pipe, a file, a capture or a CI log has none, the medium says so, and
    // `SkinT::report_extent` turns "no opinion" into SILENCE rather than into a claim. So
    // this Workshop never hears the message and paints the minimum screen: the 78x22
    // composition, unchanged, which is what keeps every golden projection in this repository
    // independent of the machine that runs it. `paint` is the whole of the evidence: a
    // default Session is what a run with no medium opinion has.
    //
    // THE FALLBACK IS NOT A MEASUREMENT AND IS NOT SPELLED LIKE ONE. Nothing anywhere
    // manufactures a 78x22 terminal; this is Workshop's own documented minimum standing
    // because nobody offered anything else, which is a different fact and stays legible as
    // one (`kScreenMinW`/`kScreenMinH`, screen.hpp).
    WorkshopDoc d = two_panels();
    Session s;
    refocus(d, s);
    const surface::SurfaceCanvas c = paint(d, s);
    CHECK(c.width == 78);
    CHECK(c.height == 22);
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, 48, 16, surface::role::kMuted));
    CHECK(label_at(c, 50, kInfoBodyY - 1) == "OBJECTS");
    CHECK(label_at(c, 0, 20).rfind("n new | d delete", 0) == 0);
    CHECK(label_at(c, 0, 21).rfind("enter edit | esc cancel", 0) == 0);
    const std::vector<std::string> rows = rasterized(c);
    REQUIRE(rows.size() == 22);
    for (const std::string& row : rows) {
        CHECK(row.size() == 78);
    }
}

// ---- tier 7b: the pane can state its own grammar -----------------------------------------

TEST_CASE("wrapping is a presentation act: as many rows as the sentence needs") {
    using zengine::workshop::detail::wrap;

    // It fits: one row, untouched -- not even a mark.
    REQUIRE(wrap("short", 20).size() == 1);
    CHECK(wrap("short", 20)[0] == "short");

    // AT SPACES, with the continuation indented so the pane still reads as a list of entries.
    const std::vector<std::string> two = wrap("alpha beta gamma delta", 14);
    REQUIRE(two.size() == 2);
    CHECK(two[0] == "alpha beta");
    CHECK(two[1] == "  gamma delta");
    for (const std::string& row : two) {
        CHECK(row.size() <= 14);
    }
    // The indent is real room, so a continuation holds fewer characters than the first row --
    // which is why the same text at twelve cells needs three rows, not two.
    CHECK(wrap("alpha beta gamma delta", 12).size() == 3);

    // A WORD WITH NO SPACE IN IT IS BROKEN RATHER THAN LOST. Refusing to break is how a tail
    // disappears silently, which is the defect this function exists to end.
    const std::vector<std::string> hard = wrap("aaaaaaaaaaaaaaaaaaaa", 8);
    REQUIRE(hard.size() >= 3);
    std::string rejoined;
    for (const std::string& row : hard) {
        rejoined += row.substr(row.find_first_not_of(' ') == std::string::npos
                                   ? 0
                                   : row.find_first_not_of(' '));
        CHECK(row.size() <= 8);
    }
    CHECK(rejoined == "aaaaaaaaaaaaaaaaaaaa");

    // NOTHING IS DROPPED: every non-space character of the input survives, in order.
    const std::string sentence =
        "this pane speaks two verbs, and `ask` takes the same form as `send`";
    for (std::int64_t width = 1; width <= 80; ++width) {
        std::string seen;
        for (const std::string& row : wrap(sentence, width)) {
            CHECK(static_cast<std::int64_t>(row.size()) <= width);
            for (const char ch : row) {
                if (ch != ' ') {
                    seen += ch;
                }
            }
        }
        std::string want;
        for (const char ch : sentence) {
            if (ch != ' ') {
                want += ch;
            }
        }
        CHECK(seen == want);
    }

    // Total at widths no pane has, and an empty line is still a line.
    CHECK(wrap("anything", 0).empty());
    CHECK(wrap("anything", -5).empty());
    REQUIRE(wrap("", 20).size() == 1);
    CHECK(wrap("", 20)[0].empty());
}

TEST_CASE("the pane states its whole grammar, wrapped, with nothing elided") {
    // THE PHASE'S STOP CONDITION, as a screen. Before G-2 this notice was `fit` into one
    // 56-cell row: a maker who asked the pane how to say something got the first fifty-three
    // characters of the answer and `...`, which is the point at which it stopped being one.
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    t.toggle_terminal();
    t.type_line("help");
    t.bus.pump();

    const std::vector<std::string> rows = rasterized(t.canvases.back());
    const Screen sc = screen_of(t.session());
    std::string pane;
    for (std::int64_t y = sc.terminal_y; y < sc.h; ++y) {
        pane += rows[static_cast<std::size_t>(y)].substr(
            static_cast<std::size_t>(sc.terminal_x), static_cast<std::size_t>(sc.terminal_w));
        pane += "\n";
    }

    // THE REQUIRED SYNTAX IS ON THE SCREEN, in the pane, whole. Read the way a person reads
    // it: across the row boundaries the wrap put in, which is what "wrapped rather than
    // truncated" MEANS. Runs of whitespace collapse to one, so a phrase broken across two
    // rows still reads as itself -- and a phrase broken mid-WORD would not, which is the
    // failure this flattening deliberately still catches.
    std::string flat;
    for (const char ch : pane) {
        if (ch == ' ' || ch == '\n') {
            if (!flat.empty() && flat.back() != ' ') {
                flat += ' ';
            }
            continue;
        }
        flat += ch;
    }
    CHECK(flat.find("send <addr> <Shape> <version> [args]") != std::string::npos);
    CHECK(flat.find("#12 for one weave") != std::string::npos);
    CHECK(flat.find("@office for whoever holds a role") != std::string::npos);
    CHECK(flat.find("* for everyone") != std::string::npos);
    CHECK(flat.find("`ask` takes the same form as `send`") != std::string::npos);

    // ...AND NOTHING OF THE ANSWER WAS ELIDED TO PUT IT THERE. `...` is the mark a one-row
    // fit leaves, and its absence on the transcript is the difference between an answer and
    // the beginning of one.
    //
    // THE CHROME IS ASKED SEPARATELY, AND ONE ROW OF IT DOES CARRY THE MARK (HD-10). The
    // pane's standing statement is 51 characters and the pane is 48 cells wide at this
    // extent, so `detail::fit` cuts it -- which is why this claim is now made over the
    // TRANSCRIPT rows rather than over the whole pane. The distinction is the point: an
    // elided ANSWER is the failure this case exists to catch, and an elided legend is a
    // three-character price the phase paid deliberately and pinned above. Asserted both
    // ways, so neither can drift into the other.
    std::string transcript;
    for (std::int64_t y = sc.terminal_y + 2; y < sc.h - 2; ++y) {
        transcript += rows[static_cast<std::size_t>(y)].substr(
            static_cast<std::size_t>(sc.terminal_x), static_cast<std::size_t>(sc.terminal_w));
        transcript += "\n";
    }
    CHECK(transcript.find("...") == std::string::npos);
    CHECK(rows[static_cast<std::size_t>(sc.terminal_y) + 1].find("...") != std::string::npos);

    // The command a maker typed is above the answer, and every row is still the pane's width.
    CHECK(pane.find("> help") != std::string::npos);
    for (std::int64_t y = sc.terminal_y; y < sc.h; ++y) {
        CHECK(rows[static_cast<std::size_t>(y)].size() == static_cast<std::size_t>(sc.w));
    }

    // ONE ENTRY, SEVERAL ROWS -- and the pane's own accounting knows it. The notice takes
    // three of the nine transcript rows at this width, so a pane holding two entries is not
    // a pane that has scrolled anything away.
    const std::size_t entries = t.pane().shown.size();
    CHECK(entries == 2); // the typed command, and the answer
    CHECK(t.pane().earlier == 0);
    CHECK(terminal_omission(t.pane()) == "[the whole of this session's record is on screen]");
}

TEST_CASE("a pane fits ENTRIES, not lines, and says what it could not show") {
    // The shared arithmetic, driven directly. `entries_that_fit` is the one place the choice
    // is made; `refresh_terminal` and `paint_terminal` both call it, which is what stops the
    // omission marker from lying about a pane whose rows were spent on wrapping.
    std::vector<loom::TranscriptEntry> record;
    for (int i = 0; i < 20; ++i) {
        loom::TranscriptEntry e;
        e.kind = loom::TranscriptKind::LocalNotice;
        e.text = "line " + std::to_string(i);
        record.push_back(e);
    }
    CHECK(entries_that_fit(record, 56, 9) == 9); // one row apiece
    CHECK(entries_that_fit(record, 56, 1) == 1);
    CHECK(entries_that_fit({}, 56, 9) == 0);

    // One long entry costs several rows, and the entries before it lose their place.
    loom::TranscriptEntry big;
    big.kind = loom::TranscriptKind::LocalNotice;
    big.text = std::string(200, 'x');
    record.push_back(big);
    const std::size_t cost = detail::wrap(terminal_line(big), 56).size();
    CHECK(cost > 1);
    CHECK(entries_that_fit(record, 56, 9) == 9 - cost + 1);

    // AT LEAST ONE, ALWAYS: a pane gone blank because its newest line was too long is
    // indistinguishable from a broken tool.
    CHECK(entries_that_fit(record, 56, 1) == 1);
    CHECK(entries_that_fit(record, 4, 2) == 1);
}

TEST_CASE("the pane keeps its corner and gains its half of a bigger surface") {
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    t.toggle_terminal();

    const Screen sc = screen_of(t.session());
    // BOTTOM-RIGHT IS STILL BOTTOM-RIGHT -- OF THE ROOM (HD-10). The bottom edge is the
    // screen's; the right edge is the workspace's, and the 28 reserved columns beyond it are
    // not the pane's at any extent.
    CHECK(sc.terminal_x + sc.terminal_w == sc.room_w);
    CHECK(sc.terminal_y + sc.terminal_h == sc.h);
    // ...and it took half the new room, so both it and the workspace are better off. At 100
    // columns the want (56 + 11) is 67 and the room is 70, so the want is what it gets: this
    // is an extent where HD-10's ceiling does NOT bind, which is what makes it the right
    // witness for the half-share rule still being the rule.
    CHECK(sc.terminal_w == kTerminalWantW + 11);
    CHECK(sc.terminal_w < sc.room_w);
    CHECK(sc.terminal_h == kMinScreen.terminal_h + 5);
    CHECK(sc.terminal_rows == kMinScreen.terminal_rows + 5);
    CHECK(sc.terminal_x > kMinScreen.terminal_x); // the workspace to its left grew too

    // And it is a solid block in the right place, on the real rasterizer.
    const std::vector<std::string> rows = rasterized(t.canvases.back());
    REQUIRE(rows.size() == static_cast<std::size_t>(sc.h));
    CHECK(rows[static_cast<std::size_t>(sc.terminal_y)].substr(
              static_cast<std::size_t>(sc.terminal_x), 8) == "TERMINAL");
    CHECK(rows[static_cast<std::size_t>(sc.h) - 1].substr(
              static_cast<std::size_t>(sc.terminal_x), 3) == "> _");
    // The pane's own last row IS the screen's last row, at every extent.
    const std::string blank(static_cast<std::size_t>(sc.terminal_w), ' ');
    CHECK(rows[static_cast<std::size_t>(sc.terminal_y) + 3].substr(
              static_cast<std::size_t>(sc.terminal_x),
              static_cast<std::size_t>(sc.terminal_w)) == blank);
}

// ============================================================================
// Tier 8 — the dynamic panels (BLD-0)
// ============================================================================
//
// A WEAVE MAY PROVIDE A TOOL; A PANEL IS ITS PRESENTATION. Everything in this
// tier is about that sentence, and the cases are arranged so that the split
// would be visible if it broke:
//
//   the CATALOG and the picker are Workshop's own furniture, and pure.
//   OPEN / CLOSE / REOPEN go through the real weave on a real bus, driven by
//     published input messages, exactly as every other gesture in this file is.
//   the TOOL is a stand-in weave holding `zengine.builder`. It records what it
//     was asked and answers whatever the case wants -- so what is pinned here is
//     Workshop's half of the conversation, and the Builder package's own half (a
//     real process, a real exit status, and who may cause one) is the `builder`
//     suite's, next door.
//
// The panel's own state is deliberately NOT reachable from Workshop's document,
// its persistence, or its authored material, and the cases that would notice
// otherwise are the ones that close a panel and open it again.

namespace {

/// A stand-in for the Builder tool: it holds the office, records what it was
/// asked, and answers with whatever status the case has set up.
///
/// It is not a mock of the real tool's LOGIC (that has its own suite); it is the
/// other end of the conversation, so that "Workshop asked" and "Workshop showed
/// what it was told" are two facts a case can separate.
class ToolSeat : public loom::WeaveBase<ToolSeat, SeenState,
                                        loom::Accept<zengine::builder::StatusRequested,
                                                     zengine::builder::BuildRequested>,
                                        loom::Emit<zengine::builder::BuildStatus>> {
public:
    void on(const zengine::builder::StatusRequested&, loom::Mail& mail) {
        ++described;
        (void)mail.publish(next);
    }
    void on(const zengine::builder::BuildRequested& ask, loom::Mail& mail) {
        asked.push_back(ask.target);
        if (answers_builds) {
            (void)mail.publish(next);
        }
    }

    /// What this tool will say next time it is asked anything.
    zengine::builder::BuildStatus next{};
    /// ...and whether it answers a build at all. A real build takes seconds and
    /// answers when the process exits; a stand-in that always answers instantly
    /// would make the panel's `waiting` state unreachable from any case.
    bool answers_builds = true;
    std::int64_t described = 0;
    std::vector<std::string> asked;
};

/// Mount the stand-in into the Builder office on this Workshop's bus.
ToolSeat* mount_tool(Live& t, const std::string& target) {
    auto seat = std::make_unique<ToolSeat>();
    ToolSeat* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_any(zengine::builder::BuildStatus::zen_name,
                       zengine::builder::BuildStatus::zen_version);
    const loom::WeaveId id = t.bus.register_weave(std::move(seat), std::move(grant),
                                                  std::string(zengine::builder::kBuilderRole));
    raw->zen_set_self(id);
    raw->next.target = target;
    return raw;
}

/// EVERYTHING AT A PANEL'S BOUNDS, top to bottom.
///
/// One helper for both kinds, and that is PNL-1 arriving in the suite: "what is
/// this panel showing" used to be two questions with two different answers --
/// one walked the stack's hard-coded column and rows, the other walked every
/// label at `Screen::panel_x` -- and it is now one question about a rectangle.
/// THROUGH `cell_text_of`, NOT THROUGH `c.labels` (HD-7). The Info panel's object names used
/// to be ordinary canvas labels and are rows of a bounded region now, so a helper reading only
/// the label list would have gone on passing while asserting about half a panel.
std::string panel_text(const surface::SurfaceCanvas& c, const ui::Rect& b) {
    std::string out;
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == b.x && l.y >= b.y && l.y < b.y + b.h) {
            out += l.text;
            out += '\n';
        }
    }
    return out;
}

/// What the overlay stack's first slot is showing, whatever is in it. The stack
/// is anchored to the canvas's top-left, so its bounds are the same on every
/// screen and the minimum one answers for all of them.
std::string stack_text(const surface::SurfaceCanvas& c) {
    return panel_text(c, placement_bounds(placement::kOverlayStack, 0, kMinScreen));
}

/// Where a kind sits in the catalog, so a case names a KIND rather than a row
/// number that a later catalog entry would silently invalidate.
std::size_t catalog_at(std::int64_t kind) {
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        if (kPanelCatalog[i].kind == kind) {
            return i;
        }
    }
    return kPanelKinds; // walked off the end: the case that used it will fail loudly
}

/// SELECT A KIND IN THE PICKER, the way a maker does: `p`, down to it, Return.
///
/// One helper for both directions, because since PNL-0 there is one gesture for
/// both directions: what this does to a closed kind is open it, and what it does
/// to an open kind is remove it.
void pick(Live& t, std::int64_t kind) {
    const std::size_t at = catalog_at(kind);
    REQUIRE(at < kPanelKinds);
    t.key(input::scan::kP);
    for (std::size_t i = 0; i < at; ++i) {
        t.key(input::scan::kDown);
    }
    t.key(input::scan::kReturn);
}

/// Open the Builder panel the way a maker does.
void open_builder(Live& t) { pick(t, panel::kBuilder); }

/// THE BUILDER IS IN THE STACK'S FIRST SLOT, asked through the placement path
/// and read off the canvas at the answer -- so the case cannot agree with the
/// screen by both of them holding the same constant.
bool first_slot_shows_builder(Live& t) {
    const Screen sc = screen_of(t.session());
    const PanelBounds at = bounds_of(t.session().panels, panel::kBuilder, sc);
    return at.open && at.rect == placement_bounds(placement::kOverlayStack, 0, sc) &&
           label_at(t.canvases.back(), at.rect.x, at.rect.y).find("BUILDER") == 0;
}

/// Everything the Info panel is showing, top to bottom -- through the same path,
/// asked about the other place.
std::string info_text(const surface::SurfaceCanvas& c, const Screen& sc) {
    return panel_text(c, placement_bounds(placement::kSideRegion, 0, sc));
}

} // namespace

TEST_CASE("the catalog is what the picker offers, and it says which kinds are open") {
    // The catalog is Workshop's own and complete: the picker walks it, so a kind
    // that is not here cannot be opened by any gesture at all.
    REQUIRE(kPanelKinds == 2);
    CHECK(kPanelCatalog[0].kind == panel::kBuilder);
    CHECK(std::string(kPanelCatalog[0].name) == "Builder");
    CHECK(kPanelCatalog[1].kind == panel::kInfo);
    CHECK(std::string(kPanelCatalog[1].name) == "Info");

    Session s; // a fresh session: Info open, Builder not
    s.panels.picker.open = true;
    surface::SurfaceCanvas c;
    paint_picker(c, s.panels, screen_of(s));
    const std::string shown = stack_text(c);
    CHECK(shown.find("+ PANEL") != std::string::npos);
    CHECK(shown.find("Builder") != std::string::npos);
    CHECK(shown.find(kPanelCatalog[0].summary) != std::string::npos);
    CHECK(shown.find("Info") != std::string::npos);
    CHECK(shown.find(kPanelCatalog[1].summary) != std::string::npos);
    // THE STATE COLUMN, and it is what makes the picker usable as the one owner
    // of presence: Return does one of two opposite things, so the list has to
    // say which one it is about to do.
    CHECK(shown.find("Builder   closed") != std::string::npos);
    CHECK(shown.find("Info      open") != std::string::npos);
}

TEST_CASE("a panel opens from the picker, is removed, and opens again") {
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    REQUIRE_FALSE(t.w->session().panels.has(panel::kBuilder));

    t.key(input::scan::kP);
    CHECK(t.w->session().panels.picker.open);
    // The picker is a question, not a panel: opening it opens nothing.
    CHECK_FALSE(t.w->session().panels.has(panel::kBuilder));

    t.key(input::scan::kReturn);
    CHECK_FALSE(t.w->session().panels.picker.open);
    CHECK(t.w->session().panels.has(panel::kBuilder));
    CHECK(stack_text(t.canvases.back()).find("BUILDER") != std::string::npos);

    // THE SAME DOOR REMOVES IT (PNL-0). There is no close key; selecting a kind
    // that is open is what takes it away.
    open_builder(t);
    CHECK_FALSE(t.w->session().panels.has(panel::kBuilder));
    CHECK(stack_text(t.canvases.back()).find("BUILDER") == std::string::npos);

    open_builder(t);
    CHECK(t.w->session().panels.has(panel::kBuilder));
    CHECK(stack_text(t.canvases.back()).find("BUILDER") != std::string::npos);
    // ...and every one of those opens ASKED the tool, which is the half that
    // makes a reopened panel show a live tool rather than a remembered one.
    CHECK(tool->described == 2);
}

TEST_CASE("the picker can be dismissed without opening anything, two ways") {
    for (const std::int64_t out : {input::scan::kEscape, input::scan::kP}) {
        Live t;
        (void)mount_tool(t, "zengine-snake");
        t.key(input::scan::kP);
        REQUIRE(t.w->session().panels.picker.open);
        t.key(out);
        CHECK_FALSE(t.w->session().panels.picker.open);
        // NOTHING WAS OPENED AND NOTHING WAS REMOVED -- the second half matters
        // now that one gesture does both, and Info was open when the picker was.
        CHECK_FALSE(t.w->session().panels.has(panel::kBuilder));
        CHECK(t.w->session().panels.has(panel::kInfo));
    }
}

TEST_CASE("opening a Builder panel ASKS the tool, and shows what the tool answers") {
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    tool->next.outcome = zengine::builder::outcome::kNeverBuilt;
    open_builder(t);

    CHECK(tool->described == 1);
    const std::string shown = stack_text(t.canvases.back());
    // The target's NAME is the one fact that exists before any build, and it
    // came from the tool -- Workshop holds no target of its own.
    CHECK(shown.find("zengine-snake") != std::string::npos);
    CHECK(shown.find("not built yet") != std::string::npos);
    CHECK(shown.find("nothing has run yet") != std::string::npos);
}

TEST_CASE("Build asks for the name the TOOL gave, and asks for nothing without one") {
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    // WITH NO PANEL OPEN, `b` is the unbound key it has always been.
    t.key(input::scan::kB);
    CHECK(tool->asked.empty());

    open_builder(t);
    tool->answers_builds = false; // a real build answers when the process exits
    t.key(input::scan::kB);
    REQUIRE(tool->asked.size() == 1);
    CHECK(tool->asked[0] == "zengine-snake");
    // And Workshop says what it did, in its own voice, before anything ran --
    // on the notice line AND on the panel.
    //
    // BOTH SENTENCES CHANGED WITH ASYNC-1, and the change is the phase. The
    // notice used to say the screen would wait until the build was done, and the
    // panel used to say it was waiting for it to FINISH; both were true of a
    // runner that built inside its own handler and both are false of one that
    // holds a child across turns. What the panel is waiting for now is the much
    // shorter moment before a process exists.
    CHECK(t.w->session().notice.find("asked the Builder") != std::string::npos);
    CHECK(t.w->session().notice.find("stays live") != std::string::npos);
    CHECK(t.w->session().panels.builder.awaiting);
    CHECK(stack_text(t.canvases.back()).find("waiting for it to start") != std::string::npos);
}

TEST_CASE("a running build is on the panel, with its operation and its output count") {
    // THE PANEL'S HALF OF ASYNC-1. A build now has a middle, and the two numbers
    // on this row are what make that middle VISIBLE rather than asserted: a
    // maker who watches `out` climb while moving a rectangle has watched
    // Workshop stay alive while a real child process ran.
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    open_builder(t);

    tool->next.outcome = zengine::builder::outcome::kRunning;
    tool->next.op = 17;
    tool->next.chunks = 4;
    tool->next.recipe = "cmake --build . --target zengine-snake";
    tool->next.detail = "[ 45%] Building CXX object";
    t.key(input::scan::kB);

    const std::string shown = stack_text(t.canvases.back());
    CHECK(shown.find("running -- op #17, 4 out") != std::string::npos);
    CHECK(shown.find("Building CXX object") != std::string::npos);
    // NOTHING IS ANNOUNCED FOR A BUILD THAT IS STILL HAPPENING. A notice is for
    // an event that is over; `running` is a condition, and a maker who is told
    // it every hundred milliseconds is a maker who cannot read the notice line.
    CHECK(t.w->session().notice.find("asked the Builder") != std::string::npos);
    // ...and the panel is STILL WATCHING, so the ending will be news when it
    // comes. That is the whole reason `awaiting` survives an intermediate status.
    CHECK(t.w->session().panels.builder.awaiting);

    // THE CANARY: the ending IS announced, to the panel that watched it begin.
    tool->next.outcome = zengine::builder::outcome::kSucceeded;
    tool->next.status = 0;
    t.publish(loom::to_value(tool->next));
    CHECK(t.w->session().notice == "built zengine-snake -- exit 0");
    CHECK_FALSE(t.w->session().panels.builder.awaiting);
}

TEST_CASE("a panel opened mid-build is TOLD it is running, and announces nothing") {
    // THE REGRESSION THE FIRST LIVE RUN PRODUCED, ASKED ABOUT THE ONE CONDITION
    // BLD-0 COULD NOT REACH. Learning that a build is running and watching one
    // start are different, and only the second is news -- so a panel opened
    // while a child is alive shows the running build and says nothing about it.
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    tool->next.outcome = zengine::builder::outcome::kRunning;
    tool->next.op = 3;
    tool->next.chunks = 12;
    tool->next.builds = 5;

    open_builder(t);
    const std::string shown = stack_text(t.canvases.back());
    CHECK(shown.find("running -- op #3, 12 out") != std::string::npos);
    CHECK(shown.find("asks 5 ever") != std::string::npos);
    CHECK(t.w->session().notice == "opened Builder -- p removes it");
    // IT IS NOT WATCHING, because it did not ask -- so the ending it did not
    // witness will be shown and not announced either.
    CHECK_FALSE(t.w->session().panels.builder.awaiting);
    tool->next.outcome = zengine::builder::outcome::kSucceeded;
    t.publish(loom::to_value(tool->next));
    CHECK(t.w->session().notice == "opened Builder -- p removes it");
    CHECK(stack_text(t.canvases.back()).find("succeeded") != std::string::npos);
}

TEST_CASE("a panel that has not heard from its tool cannot ask for a build") {
    Live t;
    // NO TOOL AT THE OFFICE. The panel opens, asks, and is never answered -- so
    // Workshop has no target to name, and names none.
    open_builder(t);
    CHECK(t.w->session().panels.has(panel::kBuilder));
    CHECK(stack_text(t.canvases.back()).find("has not answered yet") != std::string::npos);

    t.key(input::scan::kB);
    CHECK(t.w->session().notice.find("has not said what it builds") != std::string::npos);
}

TEST_CASE("success and failure are both on the screen, and both true") {
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    open_builder(t);

    SUBCASE("a build that succeeded") {
        tool->next.outcome = zengine::builder::outcome::kSucceeded;
        tool->next.status = 0;
        tool->next.recipe = "cmake --build . --target zengine-snake";
        tool->next.detail = "Built target zengine-snake";
        tool->next.builds = 1;
        t.key(input::scan::kB);

        const std::string shown = stack_text(t.canvases.back());
        CHECK(shown.find("succeeded") != std::string::npos);
        CHECK(shown.find("Built target zengine-snake") != std::string::npos);
        CHECK(t.w->session().notice == "built zengine-snake -- exit 0");
        CHECK_FALSE(t.w->session().notice_is_bad);
    }

    SUBCASE("a build that failed") {
        tool->next.outcome = zengine::builder::outcome::kFailed;
        tool->next.status = 2;
        tool->next.recipe = "cmake --build . --target zengine-snake";
        tool->next.detail = "gmake: *** [all] Error 2";
        tool->next.builds = 1;
        t.key(input::scan::kB);

        const std::string shown = stack_text(t.canvases.back());
        CHECK(shown.find("FAILED") != std::string::npos);
        CHECK(shown.find("Error 2") != std::string::npos);
        CHECK(t.w->session().notice.find("BUILD FAILED") != std::string::npos);
        CHECK(t.w->session().notice_is_bad);
    }

    SUBCASE("a build that never started says so, and shows no exit status") {
        tool->next.outcome = zengine::builder::outcome::kNotStarted;
        tool->next.status = 0;
        tool->next.detail = "could not run it (not found, or not executable)";
        t.key(input::scan::kB);

        const std::string shown = stack_text(t.canvases.back());
        CHECK(shown.find("did not start") != std::string::npos);
        // A `0` in the exit column after a build that never began would read as
        // success at the exact moment a maker most needs the right answer.
        CHECK(shown.find("exit     --") != std::string::npos);
        CHECK(t.w->session().notice_is_bad);
    }
}

TEST_CASE("a status this panel did not ask for is SHOWN, and never ANNOUNCED") {
    // THE REGRESSION THE FIRST LIVE RUN PRODUCED. Reopening the panel asks the
    // tool, the tool answers with the outcome of a build that finished minutes
    // ago, and the notice line announced `built zengine-snake -- exit 0` as
    // though it had just happened. Learning a fact and witnessing an event are
    // different, and only the second is news.
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    tool->next.outcome = zengine::builder::outcome::kSucceeded;
    tool->next.detail = "Built target zengine-snake";
    tool->next.builds = 7;

    open_builder(t);
    // The panel SHOWS the tool's history...
    const std::string shown = stack_text(t.canvases.back());
    CHECK(shown.find("succeeded") != std::string::npos);
    CHECK(shown.find("asks 7 ever") != std::string::npos);
    // ...and says nothing about a build happening, because none did.
    CHECK(t.w->session().notice == "opened Builder -- p removes it");

    // THE CANARY: the same arriving status, for a build this panel DID ask for,
    // is announced. Without it the case above would be satisfied by a Workshop
    // that had simply stopped announcing outcomes at all.
    t.key(input::scan::kB);
    CHECK(t.w->session().notice.find("built zengine-snake") != std::string::npos);
}

TEST_CASE("closing forgets the panel's copy; the TOOL keeps its own count") {
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    tool->next.outcome = zengine::builder::outcome::kSucceeded;
    tool->next.builds = 3;
    open_builder(t);
    REQUIRE(t.w->session().panels.builder.heard);

    open_builder(t); // the picker's second selection: remove
    // The panel's view went with the panel. Nothing here reached the tool.
    CHECK_FALSE(t.w->session().panels.builder.heard);
    CHECK(t.w->session().panels.builder.shown.builds == 0);

    // WHILE IT IS CLOSED, THE TOOL GOES ON. A publication with nothing
    // presenting it is not remembered -- keeping a copy against the possibility
    // of a panel being opened later is how a presentation becomes a second owner.
    tool->next.builds = 9;
    t.publish(loom::to_value(tool->next));
    CHECK_FALSE(t.w->session().panels.builder.heard);

    // And reopening asks, and is answered with the TOOL's running total -- which
    // a panel that owned the state could not produce.
    open_builder(t);
    CHECK(t.w->session().panels.builder.shown.builds == 9);
    CHECK(stack_text(t.canvases.back()).find("asks 9 ever") != std::string::npos);
}

TEST_CASE("selecting an open kind REMOVES it, and says what was not touched") {
    // BLD-0 refused this selection (`Builder is already open -- x closes it`)
    // because `x` was the removal and the picker had nothing to add. PNL-0 gave
    // the picker both directions, so the refusal became the removal and `x` went
    // back to being unbound. Both kinds, because the whole point is that the
    // gesture does not know which kind it is operating on.
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    open_builder(t);
    REQUIRE(t.w->session().panels.has(panel::kBuilder));

    open_builder(t);
    CHECK_FALSE(t.w->session().panels.has(panel::kBuilder));
    CHECK(t.w->session().notice ==
          "removed Builder -- p brings it back; nothing behind it was touched");
    CHECK_FALSE(t.w->session().notice_is_bad); // a removal a maker asked for is not a refusal

    // ...AND THE THING BEHIND IT REALLY WAS NOT TOUCHED. The tool never heard
    // about the removal: no message reached the office, and reopening finds it
    // exactly where it was.
    const std::int64_t asked_of_tool = tool->described;
    pick(t, panel::kInfo);
    CHECK_FALSE(t.w->session().panels.has(panel::kInfo));
    CHECK(t.w->session().notice ==
          "removed Info -- p brings it back; nothing behind it was touched");
    CHECK(tool->described == asked_of_tool);
}

TEST_CASE("a stacked panel covers the workspace and never reaches the side region") {
    Live t;
    (void)mount_tool(t, "zengine-snake");
    t.key(input::scan::kEscape); // an unbound key: it repaints and changes nothing

    // WITHOUT A PANEL, the screen is the one Workshop already had plus exactly
    // one new thing: the row-0 hint that says how to open one.
    const surface::SurfaceCanvas bare = t.canvases.back();
    CHECK(label_at(bare, 24, 0) == "[+ panel]  p");
    CHECK(stack_text(bare).empty());

    open_builder(t);
    const surface::SurfaceCanvas with = t.canvases.back();
    const Screen sc = screen_of(t.session());
    // THE BOUNDS THE PLACEMENT PATH GIVES IT, and the rows are read against those
    // rather than against a column this case knows independently.
    const ui::Rect stack = bounds_of(t.session().panels, panel::kBuilder, sc).rect;
    for (const surface::SurfaceLabel& l : with.labels) {
        if (l.x == stack.x && l.y >= stack.y && l.y < stack.y + stack.h) {
            // Every stacked row is padded to the panel's own width, so in a
            // character medium the spaces erase the workspace under it rather
            // than punching holes through to it.
            CHECK(l.text.size() == static_cast<std::size_t>(stack.w));
            CHECK(stack.x + stack.w <= sc.panel_x);
        }
    }
    // The OBJECTS and PROPERTIES columns are untouched by any of this.
    CHECK(label_at(with, sc.panel_x, kInfoBodyY - 1) == "OBJECTS");
    CHECK(properties_heading(with, t.doc(), t.session()) == "PROPERTIES");
}

// ---- PNL-1: the two places, said once -------------------------------------------------
//
// WHAT THESE CASES ARE ABOUT is not where the two panels are -- the case above and the
// 186 that came before it already pin that, cell by cell, and every one of them still
// passes unchanged. It is WHERE THAT ANSWER COMES FROM: a kind declares one of two
// places in the catalog, `placement_bounds` turns a place into a rectangle on a screen,
// and each painter is handed the rectangle. Before PNL-1 the same answer was arrived at
// twice, by two painters that each knew a column of their own.

TEST_CASE("a panel kind declares its place, and the place resolves to bounds") {
    // THE INTENT IS AUTHORED IN THE CATALOG. It is a fact about the kind, known
    // before anything is open and readable without a screen anywhere near it.
    CHECK(placement_of(panel::kBuilder) == placement::kOverlayStack);
    CHECK(placement_of(panel::kInfo) == placement::kSideRegion);
    CHECK(kinds_placed_in(placement::kSideRegion) == 1); // asserted at compile time too
    CHECK(kinds_placed_in(placement::kOverlayStack) == 1);

    // THE BOUNDS ARE RESOLVED AGAINST A SCREEN, and they are the two places
    // Workshop has always had: the column beside the workspace, and the top of
    // the workspace itself.
    const ui::Rect side = placement_bounds(placement::kSideRegion, 0, kMinScreen);
    const ui::Rect stack = placement_bounds(placement::kOverlayStack, 0, kMinScreen);
    CHECK(side == ui::Rect{50, 0, 28, 17});
    CHECK(stack == ui::Rect{0, 1, 48, 9});

    // AND THEY DO NOT OVERLAP -- one comparison of two rectangles, which is what
    // the model bought: it was a hand-checked relation between four separate
    // constants before, and nothing would have noticed one of them moving.
    CHECK(stack.x + stack.w <= side.x);
    CHECK(side.x + side.w == kMinScreen.w);          // the region reaches the right edge
    CHECK(stack.y + stack.h <= kMinScreen.notice_y); // neither reaches the bottom band
    CHECK(side.y + side.h <= kMinScreen.notice_y);
    // The side region is the SCREEN'S own reservation made into a rectangle: the
    // workspace measures against that column whether or not a panel is in it,
    // which is why removing Info moves nothing.
    CHECK(side.x == kMinScreen.panel_x);
    CHECK(side.w == kPanelCols);
    CHECK(kMinScreen.room_w == side.x - kPanelGap);
}

TEST_CASE("each panel is painted where the placement path says it is") {
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    const Screen sc = screen_of(t.session());
    const surface::SurfaceCanvas c = t.canvases.back();

    const PanelBounds info = bounds_of(t.session().panels, panel::kInfo, sc);
    const PanelBounds builder = bounds_of(t.session().panels, panel::kBuilder, sc);
    REQUIRE(info.open);
    REQUIRE(builder.open);
    CHECK(info.placed_in == placement::kSideRegion);
    CHECK(builder.placed_in == placement::kOverlayStack);

    // INFO'S REGION COMES FROM THE PATH: its two headings are at the rectangle it
    // was handed, on the rows it keeps inside that rectangle.
    CHECK(label_at(c, info.rect.x, info.rect.y + kInfoBodyY - 1) == "OBJECTS");
    CHECK(properties_heading(c, t.doc(), t.session()) == "PROPERTIES");
    // BUILDER'S REGION COMES FROM THE PATH, at the first slot of the stack.
    CHECK(label_at(c, builder.rect.x, builder.rect.y).find("BUILDER") == 0);
    CHECK(builder.rect == placement_bounds(placement::kOverlayStack, 0, sc));

    // AND NEITHER PANEL PAINTS OUTSIDE ITS OWN BOUNDS. Every label in a panel's
    // column is on one of that panel's rows, and every row of the stacked panel
    // is padded to the width its bounds gave it -- which is the erasing mechanism
    // following the panel rather than a constant.
    std::size_t seen_info = 0;
    std::size_t seen_stack = 0;
    for (const surface::SurfaceLabel& l : c.labels) {
        if (l.x == info.rect.x) {
            CHECK(l.y >= info.rect.y);
            CHECK(l.y < info.rect.y + info.rect.h);
            ++seen_info;
        }
        if (l.x == builder.rect.x && l.y >= builder.rect.y &&
            l.y < builder.rect.y + builder.rect.h) {
            CHECK(l.text.size() == static_cast<std::size_t>(builder.rect.w));
            ++seen_stack;
        }
    }
    CHECK(seen_info > 0);
    CHECK(seen_stack == static_cast<std::size_t>(builder.rect.h));
}

TEST_CASE("a closed panel is not anywhere") {
    // A PANEL THAT IS NOT OPEN HAS NO BOUNDS, and the empty rectangle is the
    // deliberate answer rather than the first slot's: a caller that forgets to
    // ask gets a rectangle that contains nothing, not one that contains the place
    // this panel WOULD have had.
    Live t;
    const Screen sc = screen_of(t.session());
    const PanelBounds absent = bounds_of(t.session().panels, panel::kBuilder, sc);
    CHECK_FALSE(absent.open);
    CHECK(absent.rect == ui::Rect{});
    CHECK_FALSE(absent.rect.contains(0, 1));
    // Its KIND still has a declared place, because that is a fact about the
    // catalog rather than about this session.
    CHECK(absent.placed_in == placement::kOverlayStack);
    // The place it would have had is perfectly well defined -- what is absent is
    // the PANEL, not the place.
    CHECK(placement_bounds(placement::kOverlayStack, 0, sc).contains(0, 1));
}

TEST_CASE("a slot is earned by being in the stack, not by being early in the list") {
    // THE RULE THAT USED TO BE A COUNTER INSIDE THE PAINTING LOOP. It named a
    // kind; it counts placements now, so the answer cannot depend on the order a
    // maker happened to open two unalike panels in.
    const Screen sc = kMinScreen;
    Panels info_first;
    info_first.open = {Panel{panel::kInfo}, Panel{panel::kBuilder}};
    Panels builder_first;
    builder_first.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};

    const ui::Rect first_slot = placement_bounds(placement::kOverlayStack, 0, sc);
    CHECK(bounds_of(info_first, panel::kBuilder, sc).rect == first_slot);
    CHECK(bounds_of(builder_first, panel::kBuilder, sc).rect == first_slot);
    // And Info is in the same column either way: the side region has no slots.
    CHECK(bounds_of(info_first, panel::kInfo, sc).rect ==
          bounds_of(builder_first, panel::kInfo, sc).rect);
}

TEST_CASE("a painter goes where its bounds say, not where a constant says") {
    // THE STRUCTURAL HALF OF THE THIRD-PANEL CLAIM, and it costs no fake catalog
    // entry: both painters are called with bounds that are NOT the ones their kind
    // resolves to, and both land there. A painter that still knew its own column
    // would ignore this and paint where it always did.
    //
    // The rectangles are the same HEIGHT as the real ones. A panel's content is
    // written for its place's row budget -- the Builder's nine rows are nine
    // statements about a build -- so what a moved rectangle proves is that the
    // WHERE is owned by the placement path. A place of a different height is a
    // layout question nothing has asked yet.
    WorkshopDoc d = two_panels();
    Session s;
    refocus(d, s);

    const ui::Rect moved_info{7, 2, kPanelCols, 17};
    surface::SurfaceCanvas ic;
    paint_info(ic, d, s, moved_info, kMinScreen);
    CHECK(label_at(ic, moved_info.x, moved_info.y + kInfoBodyY - 1) == "OBJECTS");
    // The heading is a ROW of the body since HD-7, so it moves with the rectangle too.
    const InfoBodyPlace moved = info_body_place(moved_info, kMinScreen, d, s);
    REQUIRE(moved.present);
    CHECK(inspector_row(ic, moved.region_x, moved.region_y + moved.heading_row) == "PROPERTIES");
    // AND ITS BACKDROP GOES WITH IT (PNL-2a). This assertion used to read
    // `ic.rects.empty()` -- Info had no backdrop at all, pinned deliberately by
    // PNL-1 and named by PNL-2 as the case that would have to change if anybody
    // ever gave it one. Somebody did. It is one rect, it is the whole of the
    // rectangle this painter was HANDED, and it is not the rectangle this kind
    // would have resolved to -- so the backdrop is owned by the placement path
    // exactly as much as the labels are.
    REQUIRE(ic.rects.size() == 1);
    CHECK(ic.rects[0].x == moved_info.x);
    CHECK(ic.rects[0].y == moved_info.y);
    CHECK(ic.rects[0].w == moved_info.w);
    CHECK(ic.rects[0].h == moved_info.h);
    CHECK(ic.rects[0].role == surface::role::kMuted);
    for (const surface::SurfaceLabel& l : ic.labels) {
        CHECK(l.x == moved_info.x);
        CHECK(l.y >= moved_info.y);
        CHECK(l.y < moved_info.y + moved_info.h);
    }

    BuilderPane pane;
    pane.heard = true;
    pane.shown.target = "zengine-snake";
    const ui::Rect moved_stack{4, 6, 30, kStackRows};
    surface::SurfaceCanvas bc;
    paint_builder(bc, pane, moved_stack);
    CHECK(label_at(bc, moved_stack.x, moved_stack.y).find("BUILDER") == 0);
    REQUIRE(bc.rects.size() == 1);
    CHECK(bc.rects[0].x == moved_stack.x);
    CHECK(bc.rects[0].y == moved_stack.y);
    CHECK(bc.rects[0].w == moved_stack.w);
    CHECK(bc.rects[0].h == moved_stack.h);
    for (const surface::SurfaceLabel& l : bc.labels) {
        CHECK(l.x == moved_stack.x);
        CHECK(l.y >= moved_stack.y);
        CHECK(l.y < moved_stack.y + moved_stack.h);
        // Fitted and padded to the bounds it was handed and not to the stack's own
        // width, so a narrower panel erases exactly what it covers.
        CHECK(l.text.size() == static_cast<std::size_t>(moved_stack.w));
    }
}

TEST_CASE("the stack has a second slot, and the minimum screen has no room for it") {
    // WHAT A THIRD KIND WOULD FIND IF IT DECLARED THE STACK. The path answers for
    // slot 1 without anything being added to it: same column, same width, one
    // blank row below the first.
    const ui::Rect first = placement_bounds(placement::kOverlayStack, 0, kMinScreen);
    const ui::Rect second = placement_bounds(placement::kOverlayStack, 1, kMinScreen);
    CHECK(second.x == first.x);
    CHECK(second.w == first.w);
    CHECK(second.h == first.h);
    CHECK(second.y == first.y + first.h + kStackGap);

    // AND IT DOES NOT FIT on the screen this composition is written for. That is a
    // measured limit rather than a surprise waiting for whoever adds a third kind:
    // the second slot's last row is past the notice line, and the screen has to be
    // three rows taller than the minimum before the stack can hold two panels.
    CHECK(second.y + second.h > kMinScreen.notice_y);
    const Screen tall = screen_of(kScreenMinW, 25);
    const ui::Rect on_tall = placement_bounds(placement::kOverlayStack, 1, tall);
    CHECK(on_tall.y + on_tall.h <= tall.notice_y);
    // Nothing here clamps, refuses or rearranges. The model SAYS where a second
    // slot is; whether Workshop should ever put a panel there is the layout
    // question PNL-1 did not answer.
}

TEST_CASE("a wider screen moves the side region and leaves the stack where it is") {
    // THE TWO PLACES ANSWER THE EXTENT QUESTION DIFFERENTLY, and the path is where
    // that difference lives now: the region is anchored to the right edge and
    // keeps its width, the stack is anchored to the top-left corner and keeps
    // everything. That is G-2's rule, restated over rectangles.
    const Screen big = screen_of(100, 30);
    const ui::Rect side = placement_bounds(placement::kSideRegion, 0, big);
    const ui::Rect stack = placement_bounds(placement::kOverlayStack, 0, big);
    CHECK(side.x == big.panel_x);
    CHECK(side.w == kPanelCols);
    CHECK(side.x + side.w == big.w);
    CHECK(side.h > placement_bounds(placement::kSideRegion, 0, kMinScreen).h);
    CHECK(stack == placement_bounds(placement::kOverlayStack, 0, kMinScreen));
    CHECK(stack.x + stack.w <= side.x);
}

TEST_CASE("the terminal overlay still outranks everything, panels included") {
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    open_builder(t);
    REQUIRE(t.w->session().panels.has(panel::kBuilder));

    t.toggle_terminal();
    REQUIRE(t.w->session().terminal.open);
    // `b` and `p` are Workshop's command-mode keys; while the pane is open the
    // pane has the input, so neither reaches the panels.
    t.key(input::scan::kB);
    t.key(input::scan::kP);
    CHECK(tool->asked.empty());
    CHECK_FALSE(t.w->session().panels.picker.open);
    CHECK(t.w->session().panels.has(panel::kBuilder));

    t.toggle_terminal();
    CHECK_FALSE(t.w->session().terminal.open);
    t.key(input::scan::kB);
    CHECK(tool->asked.size() == 1); // and closing it restores them exactly
}

// ============================================================================
// Tier 9 -- Info, the second panel kind (PNL-0)
// ============================================================================
//
// THE OBJECTS AND PROPERTIES COLUMNS ARE NO LONGER FURNITURE. They were painted
// unconditionally by `paint` from W-0 until now; they are a panel like the
// Builder beside them, and the whole of this tier is the difference that makes.
//
// WHAT MAKES INFO THE INTERESTING SECOND KIND is what it is NOT:
//
//   it has no weave.        Opening it sends nothing and asks nobody. A Workshop
//                           hosting no tools at all opens it and it works.
//   it has no state.        There is no `InfoPane`. Removing it destroys nothing,
//                           because everything it shows belongs to the document
//                           or to the session and outlives the panel by a lot.
//   it is not in the dock.  It sits in the right-hand column, which is a
//                           different place with different rules, so the two
//                           kinds together are what the placement question will
//                           eventually have to be answered from.
//
// The cases that would notice if any of those quietly stopped being true are the
// ones that remove Info with no tool on the bus, that compare the column before
// and after a removal, and that measure the document while it is absent.

TEST_CASE("Info is open at boot, and it is a panel rather than furniture") {
    Live t;
    t.key(input::scan::kEscape); // an unbound key: it repaints and changes nothing

    // A FRESH SESSION HAS IT OPEN, and it is the only thing open.
    const Panels& panels = t.w->session().panels;
    REQUIRE(panels.open.size() == 1);
    CHECK(panels.open[0].kind == panel::kInfo);
    CHECK(panels.has(panel::kInfo));
    CHECK_FALSE(panels.has(panel::kBuilder));

    // ...and the screen a maker boots into is the screen they have always booted
    // into: the two column headings, in the column they have always been in.
    const Screen sc = screen_of(t.session());
    const surface::SurfaceCanvas& c = t.canvases.back();
    CHECK(label_at(c, sc.panel_x, kInfoBodyY - 1) == "OBJECTS");
    CHECK(properties_heading(c, t.doc(), t.session()) == "PROPERTIES");
    CHECK(object_row(c, t.doc(), t.session(), 0) == "> #1 panel");
    CHECK(property_row(c, t.doc(), t.session(), 0) == " Identity #1");

    // AND IT IS IN THE CATALOG, which is what makes it a panel rather than a
    // fixed thing that happens to be listed: the picker is the only door, and
    // this kind is behind it.
    CHECK(catalog_at(panel::kInfo) < kPanelKinds);
}

TEST_CASE("Info can be removed, and takes its whole column with it") {
    Live t;
    t.key(input::scan::kN); // something to look at, so an empty column is not the empty case
    const Screen sc = screen_of(t.session());
    REQUIRE_FALSE(info_text(t.canvases.back(), sc).empty());

    pick(t, panel::kInfo);
    CHECK_FALSE(t.w->session().panels.has(panel::kInfo));

    // NOT ONE LABEL IS LEFT IN THAT COLUMN. Not the headings, not the object
    // list, not the inspector, not the "(none)" marker -- the panel is gone
    // rather than emptied.
    const surface::SurfaceCanvas& gone = t.canvases.back();
    CHECK(info_text(gone, sc).empty());
    CHECK(label_at(gone, sc.panel_x, kInfoBodyY - 1).empty());
    CHECK(properties_heading(gone, t.doc(), t.session()).empty());

    // AND NOTHING ELSE MOVED. The screen around the hole is the screen it was:
    // the workspace title, the two hints on row 0, and the help lines at the
    // bottom are all exactly where they were, because removing a panel is not a
    // re-layout.
    CHECK(label_at(gone, 0, 0) == "WORKSPACE 48x16 cells");
    CHECK(label_at(gone, 24, 0) == "[+ panel]  p");
    CHECK(label_at(gone, sc.w - 20, 0) == "shift+space terminal");
    CHECK(label_at(gone, 0, sc.help_y).find("n new | d delete") == 0);
}

TEST_CASE("reopening Info brings back the column it had, cell for cell") {
    Live t;
    t.key(input::scan::kN);
    t.key(input::scan::kN);
    const Screen sc = screen_of(t.session());
    const std::string before = info_text(t.canvases.back(), sc);
    REQUIRE(before.find("OBJECTS") != std::string::npos);

    pick(t, panel::kInfo);
    REQUIRE(info_text(t.canvases.back(), sc).empty());

    pick(t, panel::kInfo);
    CHECK(t.w->session().panels.has(panel::kInfo));
    // THE SAME COLUMN, not a reconstructed one. It is byte-identical because
    // there was never a copy to go stale: the panel reads the document and the
    // session, both of which went on being true while it was absent.
    CHECK(info_text(t.canvases.back(), sc) == before);
}

TEST_CASE("removing Info changes nothing about the document, not even its picture") {
    // THE SHARPEST CLAIM IN THIS TIER, and the reason the vacated 28 columns
    // stay vacant. The workspace's extent is what a share resolves against, so a
    // workspace that grew into the empty column would make every %-width object
    // on the screen change size because a maker hid a list of names. A panel's
    // presence must not be visible in the picture of the document.
    // The document Workshop boots on already carries the case this needs: object
    // #1 is authored as a SHARE, so its resolved width is a function of the
    // workspace and nothing else.
    Live t;
    REQUIRE(t.w->document().elements.size() == 2);
    REQUIRE(t.w->document().elements[0].width.mode == ui::kExtentPercent);

    const std::int64_t authored_w = t.w->document().elements[0].width.amount;
    const std::int64_t workspace_w = t.session().workspace_w;
    const ui::Scene before = workspace_scene(t.w->document(), t.session());
    REQUIRE(before.items.size() == 2);

    pick(t, panel::kInfo);
    REQUIRE_FALSE(t.w->session().panels.has(panel::kInfo));

    // The authored value, the workspace it resolves against, and the rectangle
    // it resolves to -- all three unchanged.
    CHECK(t.w->document().elements[0].width.amount == authored_w);
    CHECK(t.session().workspace_w == workspace_w);
    const ui::Scene after = workspace_scene(t.w->document(), t.session());
    REQUIRE(after.items.size() == 2);
    CHECK(after.items[0].rect.x == before.items[0].rect.x);
    CHECK(after.items[0].rect.y == before.items[0].rect.y);
    CHECK(after.items[0].rect.w == before.items[0].rect.w);
    CHECK(after.items[0].rect.h == before.items[0].rect.h);

    // ...and the same rectangle is still on the canvas, at the same cells. The
    // Screen itself is untouched too: the column is reserved, not reclaimed.
    const Screen sc = screen_of(t.session());
    CHECK(sc.panel_x == kMinScreen.panel_x);
    CHECK(sc.room_w == kMinScreen.room_w);
    bool found = false;
    for (const surface::SurfaceRect& r : t.canvases.back().rects) {
        if (r.role == surface::role::kFill && r.w == before.items[0].rect.w &&
            r.h == before.items[0].rect.h) {
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("Builder and Info are present independently -- all four states") {
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    const Screen sc = screen_of(t.session());
    const auto shows_info = [&t, &sc]() { return !info_text(t.canvases.back(), sc).empty(); };
    const auto shows_builder = [&t]() {
        return stack_text(t.canvases.back()).find("BUILDER") != std::string::npos;
    };

    // Info alone -- how Workshop boots.
    t.key(input::scan::kEscape);
    CHECK(shows_info());
    CHECK_FALSE(shows_builder());

    // Both. The Builder is over the workspace, Info is in its column, and
    // neither knows the other exists.
    open_builder(t);
    CHECK(shows_info());
    CHECK(shows_builder());

    // Builder alone. Removing Info leaves the Builder exactly where it was --
    // a slot is earned by being PLACED in the stack, so an Info ahead of it in
    // the open list never pushed it down and removing one never pulls it up.
    pick(t, panel::kInfo);
    CHECK_FALSE(shows_info());
    CHECK(shows_builder());
    CHECK(first_slot_shows_builder(t));

    // Neither. An empty screen around a live document is a legitimate state, and
    // the document is still all there.
    open_builder(t);
    CHECK_FALSE(shows_info());
    CHECK_FALSE(shows_builder());
    CHECK(label_at(t.canvases.back(), 0, 0) == "WORKSPACE 48x16 cells");

    // And back to both, in the other order.
    pick(t, panel::kInfo);
    open_builder(t);
    CHECK(shows_info());
    CHECK(shows_builder());
    CHECK(first_slot_shows_builder(t));
    // Every Builder OPEN asked the tool; neither removal did, and neither did
    // anything Info was part of.
    CHECK(tool->described == 2);
}

TEST_CASE("Info needs no weave to be a panel") {
    // NOTHING IS MOUNTED IN THE BUILDER OFFICE, and nothing else is either. If
    // being a panel required a tool behind it, this is the case that could not
    // pass.
    Live t;
    const Screen sc = screen_of(t.session());
    t.key(input::scan::kN);
    CHECK(info_text(t.canvases.back(), sc).find("OBJECTS") != std::string::npos);

    pick(t, panel::kInfo);
    CHECK_FALSE(t.w->session().panels.has(panel::kInfo));
    pick(t, panel::kInfo);
    CHECK(t.w->session().panels.has(panel::kInfo));
    CHECK(info_text(t.canvases.back(), sc).find("OBJECTS") != std::string::npos);

    // AND NO MESSAGE WENT TO THE ONE OFFICE WORKSHOP KNOWS ABOUT. With the
    // stand-in mounted, opening and removing Info leaves its counters at zero --
    // the sends in `choose_panel` belong to the Builder kind and to no other.
    Live u;
    ToolSeat* tool = mount_tool(u, "zengine-snake");
    pick(u, panel::kInfo);
    pick(u, panel::kInfo);
    pick(u, panel::kInfo);
    CHECK(tool->described == 0);
    CHECK(tool->asked.empty());
}

TEST_CASE("the document is still a document with Info removed") {
    Live t;
    const std::size_t born = t.w->document().elements.size();
    REQUIRE(born == 2);
    pick(t, panel::kInfo);
    REQUIRE_FALSE(t.w->session().panels.has(panel::kInfo));

    // Every gesture that authors: they are the WORKSPACE's, not the panel's, and
    // removing the panel that lists the results does not remove the results.
    t.key(input::scan::kN);
    REQUIRE(t.w->document().elements.size() == born + 1);
    const std::int64_t made = t.w->document().elements.back().id;
    CHECK(t.session().selected == made); // creating still selects what it made

    t.key(input::scan::kTab);
    CHECK(t.session().selected == t.w->document().elements[0].id);

    const std::int64_t x0 = t.w->document().elements[0].x;
    t.key(input::scan::kL);
    CHECK(t.w->document().elements[0].x == x0 + 1);
    const std::int64_t w0 = t.w->document().elements[0].width.amount;
    t.key(input::scan::kL, input::mod::kShift);
    CHECK(t.w->document().elements[0].width.amount != w0);

    t.key(input::scan::kD);
    CHECK(t.w->document().elements.size() == born);

    // ...and the picture kept up the whole time, in the workspace where it lives.
    CHECK(label_at(t.canvases.back(), 0, 0) == "WORKSPACE 48x16 cells");

    // Reopening finds the document that was authored while nobody was showing it.
    pick(t, panel::kInfo);
    const Screen sc = screen_of(t.session());
    CHECK(info_text(t.canvases.back(), sc).find("#" + std::to_string(made)) !=
          std::string::npos);
}

TEST_CASE("the inspector's keys say so when Info is not showing, and open no draft") {
    // THE TRAP THIS CLOSES. A draft opened with Info removed would put Workshop
    // into editing mode, where `p` types a p instead of opening the picker -- so
    // the maker could not reopen the panel to find what they were editing -- and
    // Ctrl+S would then refuse to save, naming a row that is not on the screen.
    Live t;
    t.key(input::scan::kN);
    pick(t, panel::kInfo);
    REQUIRE_FALSE(t.w->session().panels.has(panel::kInfo));

    t.key(input::scan::kReturn);
    CHECK(t.w->session().notice == "the properties are not showing -- p opens the Info panel");
    CHECK(t.w->session().notice_is_bad);
    for (const Row& r : t.session().rows) {
        CHECK_FALSE(r.editing());
    }

    // The cursor keys answer for the same panel, for the same reason: they used
    // to do something, so silence would be indistinguishable from a broken tool.
    const std::size_t cursor = t.session().cursor;
    t.key(input::scan::kDown);
    CHECK(t.session().cursor == cursor);
    CHECK(t.w->session().notice == "the properties are not showing -- p opens the Info panel");
    t.key(input::scan::kUp);
    CHECK(t.session().cursor == cursor);

    // AND THE CANARY: with Info back, all three do exactly what they always did.
    pick(t, panel::kInfo);
    t.key(input::scan::kDown);
    CHECK(t.session().cursor == cursor + 1);
    t.key(input::scan::kReturn);
    CHECK(t.w->session().notice.find("editing ") == 0);
    bool drafting = false;
    for (const Row& r : t.session().rows) {
        drafting = drafting || r.editing();
    }
    CHECK(drafting);
}

TEST_CASE("x is an unbound key again") {
    // BLD-0 bound it to "close the Builder"; the second kind made that a choice
    // the key could not make, so presence moved to the picker and this went back
    // to meaning nothing. A key that still half-worked would be the worst of the
    // three available outcomes.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    REQUIRE(t.w->session().panels.has(panel::kBuilder));
    const std::string notice = t.w->session().notice;

    t.key(input::scan::kX);
    CHECK(t.w->session().panels.has(panel::kBuilder));
    CHECK(t.w->session().panels.has(panel::kInfo));
    CHECK(t.w->session().notice == notice); // it said nothing, because it means nothing
}

TEST_CASE("the picker's state column follows the panels, not a memory of them") {
    Live t;
    (void)mount_tool(t, "zengine-snake");

    t.key(input::scan::kP);
    CHECK(stack_text(t.canvases.back()).find("Builder   closed") != std::string::npos);
    CHECK(stack_text(t.canvases.back()).find("Info      open") != std::string::npos);
    t.key(input::scan::kEscape);

    open_builder(t);
    pick(t, panel::kInfo);
    t.key(input::scan::kP);
    CHECK(stack_text(t.canvases.back()).find("Builder   open") != std::string::npos);
    CHECK(stack_text(t.canvases.back()).find("Info      closed") != std::string::npos);
    t.key(input::scan::kEscape);
}

TEST_CASE("the picker covers the whole slot it opens over, so nothing reads through it") {
    // FOUND LIVE, IN THE GRAPHICAL MEDIUM. The picker was as tall as its own
    // contents, so over a nine-row Builder it left six of that panel's rows
    // showing underneath -- with no edge between them, because in a character
    // medium there is none. The screen read:
    //
    //     + PANEL -- up/down, enter opens or removes
    //     > Builder   open    build one known target
    //       Info      closed  objects and properties
    //     exit     --         asks 0 ever
    //
    // One box, two unrelated statements. The second catalog entry did not create
    // this, but it made it long enough to notice.
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    tool->next.outcome = zengine::builder::outcome::kNeverBuilt;
    open_builder(t);
    REQUIRE(stack_text(t.canvases.back()).find("recipe") != std::string::npos);

    t.key(input::scan::kP);
    const surface::SurfaceCanvas& c = t.canvases.back();

    // WHAT A MAKER SEES, row by row: the topmost label at every row of the slot,
    // which is the mechanism -- a row written last, padded to the dock's width,
    // is what a character medium leaves on the screen.
    std::string visible;
    const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, screen_of(t.session()));
    for (std::int64_t row = 0; row < slot.h; ++row) {
        const std::string top = topmost_at(c, slot.x, slot.y + row);
        CHECK(top.size() == static_cast<std::size_t>(slot.w));
        visible += top;
        visible += '\n';
    }
    CHECK(visible.find("+ PANEL") != std::string::npos);
    CHECK(visible.find("Builder   open") != std::string::npos);
    // Not one row of the panel underneath survives.
    CHECK(visible.find("recipe") == std::string::npos);
    CHECK(visible.find("BUILDER") == std::string::npos);
    CHECK(visible.find("Build ]") == std::string::npos);

    // AND THE CANARY: dismissing the picker gives the panel back whole.
    t.key(input::scan::kEscape);
    CHECK(stack_text(t.canvases.back()).find("recipe") != std::string::npos);
}

// ============================================================================
// Tier 11 -- a panel occupies POINTER space, not only pixels (PNL-2)
// ============================================================================
//
// WHAT PNL-1 MEASURED AND DID NOT REPAIR: with the Builder open, a press at a
// canvas cell that panel was visibly covering took hold of the object underneath
// it, selected it, and began a drag a maker could not see. The panel was a
// picture of a thing rather than a thing.
//
// THE RULE THESE CASES PIN, in the order the press is asked:
//
//     the terminal overlay, while it is open   -- it has the pointer entirely
//     a visible panel, by its resolved bounds  -- it occupies what it covers
//     the workspace and the document underneath
//
// AND THE TWO ASYMMETRIES THAT MEAN NO CAPTURE STATE EXISTS: a press on a panel
// begins nothing, so nothing later needs cancelling; a gesture that began on the
// workspace owns the pointer until its release, wherever that release lands.
//
// The boot document is #1 at workspace 3,2 (28x6) and #2 at 6,10 (14x4); the
// overlay stack's first slot is canvas {0,1,48,9} and the side region is canvas
// {50,0,28,17} on the minimum screen. So workspace (10,5) is canvas (10,6):
// inside the Builder's bounds AND on top of #1, which is the whole overlap this
// tier is about.

namespace {

/// Is this canvas cell inside any OPEN panel's bounds, worked out from the
/// painting path alone -- `bounds_of` per open kind, exactly as `paint_panels`
/// asks it. The point of computing it a second way is that `occupied_at` has to
/// agree with it cell for cell.
bool inside_a_painted_panel(const Panels& panels, const Screen& sc, std::int64_t x,
                            std::int64_t y) {
    for (const Panel& p : panels.open) {
        if (bounds_of(panels, p.kind, sc).rect.contains(x, y)) {
            return true;
        }
    }
    return false;
}

/// EVERY CELL OF THE CANVAS, ASKED BOTH WAYS -- what the painting path says is
/// inside a panel, and what the pointer path says is occupied.
///
/// It is a SWEEP that reports a tally rather than a case that asserts per cell:
/// seventeen hundred assertions saying the same thing would drown the suite's
/// own totals, and a count plus the first cell that disagreed diagnoses a failure
/// just as precisely. `first_bad` is `{-1,-1}` when nothing disagreed.
struct Sweep {
    std::size_t cells = 0;    ///< how many were asked
    std::size_t occupied = 0; ///< how many the pointer path called occupied
    std::size_t painted = 0;  ///< how many the painting path called covered
    std::size_t disagreed = 0;
    std::int64_t first_bad_x = -1;
    std::int64_t first_bad_y = -1;
};

Sweep sweep_canvas(const Panels& panels, const Screen& sc) {
    Sweep out;
    for (std::int64_t y = 0; y < sc.h; ++y) {
        for (std::int64_t x = 0; x < sc.w; ++x) {
            const bool painted = inside_a_painted_panel(panels, sc, x, y);
            const bool occupied = occupied_at(panels, sc, x, y).occupied;
            ++out.cells;
            out.painted += painted ? 1u : 0u;
            out.occupied += occupied ? 1u : 0u;
            if (painted != occupied) {
                if (out.disagreed == 0) {
                    out.first_bad_x = x;
                    out.first_bad_y = y;
                }
                ++out.disagreed;
            }
        }
    }
    return out;
}

} // namespace

TEST_CASE("a visible panel occupies the pointer space it covers") {
    Live t;
    (void)mount_tool(t, "zengine-snake");
    // A SELECTION THAT IS NOT THE COVERED OBJECT, so "cannot select" is a claim
    // this case can actually make: the press below is on #1, and #2 has to still
    // be the selection afterwards.
    t.key(input::scan::kTab);
    const std::int64_t other = t.session().selected;
    REQUIRE(other == 2);

    open_builder(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect panel = bounds_of(t.session().panels, panel::kBuilder, sc).rect;
    // The press cell, named from the panel's OWN bounds and from the object's own
    // placement, so neither the panel nor the object is where this case guessed.
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* covered = ui::placed_for(scene, 1);
    REQUIRE(covered != nullptr);
    const std::int64_t wx = covered->rect.x + 7;
    const std::int64_t wy = covered->rect.y + 3;
    REQUIRE(panel.contains(wx + kWorkspaceX, wy + kWorkspaceY)); // the panel covers it
    REQUIRE(ui::hit(scene, wx, wy) != nullptr);                  // and so does the object

    const ui::Element before = *doc::find(t.doc(), 1);
    t.press(wx, wy);
    // NOTHING UNDER IT WAS REACHED: not selected, not held, not moved -- and all
    // three at once, because `take_hold` is the one door to all of them.
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    CHECK(t.session().selected == other);
    CHECK_FALSE(t.session().drag.active);
    CHECK_FALSE(t.session().drag.resizing);
    CHECK(doc::find(t.doc(), 1)->x == before.x);
    CHECK(doc::find(t.doc(), 1)->y == before.y);
    t.release(wx, wy);
    CHECK_FALSE(t.session().drag.active);

    // AND THE SAME OBJECT IS REACHABLE AGAIN THE MOMENT THE PANEL IS REMOVED --
    // the same cell, the same document, the same gesture. The occlusion is the
    // panel's presence and nothing else.
    pick(t, panel::kBuilder);
    REQUIRE_FALSE(t.session().panels.has(panel::kBuilder));
    t.press(wx, wy);
    CHECK(t.notice() == "holding #1 -- drag to move it");
    CHECK(t.session().selected == 1);
    CHECK(t.session().drag.active);
    t.release(wx, wy);
}

TEST_CASE("a press that lands on a panel begins nothing, so a hand that leaves it drags nothing") {
    // PRESS BEGINS ON THE PANEL, POINTER LATER LEAVES IT. There is no capture
    // state to get this right: a press on a panel never calls `take_hold`, so
    // there is no drag for the motion to continue, and the absence of the drag is
    // the whole of the memory.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    const ui::Element one_before = *doc::find(t.doc(), 1);
    const ui::Element two_before = *doc::find(t.doc(), 2);

    t.press(10, 5); // on the Builder
    CHECK_FALSE(t.session().drag.active);
    t.motion(8, 11); // off it, over #2
    t.motion(9, 12);
    t.release(9, 12);

    CHECK_FALSE(t.session().drag.active);
    CHECK(doc::find(t.doc(), 1)->x == one_before.x);
    CHECK(doc::find(t.doc(), 1)->y == one_before.y);
    CHECK(doc::find(t.doc(), 2)->x == two_before.x);
    CHECK(doc::find(t.doc(), 2)->y == two_before.y);
    // Not one frame of this said "holding": the gesture never began.
    CHECK(t.notice().find("holding") == std::string::npos);
}

TEST_CASE("a gesture that began on the workspace is not interrupted by a panel") {
    // PRESS BEGINS ON THE WORKSPACE, POINTER LATER MOVES BENEATH A PANEL. The
    // drag owns the pointer until it ends, so it keeps authoring -- and the
    // object goes where the maker's hand put it, UNDER the panel, because a panel
    // that stopped a drag at its own edge would be clamping the document. That is
    // a panel's presence becoming visible in what a maker can author, which is
    // the rule that also keeps a removed Info's column empty.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect panel = bounds_of(t.session().panels, panel::kBuilder, sc).rect;

    // #2 sits at workspace 6,10 -- canvas row 11, below the panel's last row.
    REQUIRE_FALSE(panel.contains(7 + kWorkspaceX, 11 + kWorkspaceY));
    t.press(7, 11);
    REQUIRE(t.session().drag.active);
    REQUIRE(t.session().drag.id == 2);

    t.motion(7, 6);
    t.motion(7, 3); // under the panel now
    CHECK(t.session().drag.active);
    const ui::Scene mid = workspace_scene(t.doc(), t.session());
    const ui::Placed* moved = ui::placed_for(mid, 2);
    REQUIRE(moved != nullptr);
    CHECK(panel.contains(moved->rect.x + kWorkspaceX, moved->rect.y + kWorkspaceY));
    const std::int64_t rested_x = moved->rect.x;
    const std::int64_t rested_y = moved->rect.y;

    // AND THE RELEASE ENDS IT, wherever the hand is. Occluding the release would
    // strand the drag with the button up, and the next motion would carry an
    // object nobody was holding.
    t.release(7, 3);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice() == "released #2");
    t.motion(20, 14); // no button down: nothing follows the pointer
    const ui::Scene after = workspace_scene(t.doc(), t.session());
    const ui::Placed* still = ui::placed_for(after, 2);
    REQUIRE(still != nullptr);
    CHECK(still->rect.x == rested_x);
    CHECK(still->rect.y == rested_y);

    // The object is genuinely under the panel now -- and genuinely out of reach
    // there, which closes the loop: the document may hold what the picture does
    // not show, and the pointer answers about the picture.
    t.press(rested_x + 1, rested_y + 1);
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.active);
}

TEST_CASE("a resize a panel covers cannot be started either") {
    // THE OTHER GESTURE, and the reason one is not enough: a press has two
    // possible meanings (take the size handle, or take the body) and the panel
    // has to refuse both. It does, structurally -- `take_hold` is what decides
    // between them and the press never reaches it.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    const Handle grip = size_handle(t.doc(), t.session());
    REQUIRE(grip.shown);
    REQUIRE(grip.id == 1);

    open_builder(t);
    const Screen sc = screen_of(t.session());
    REQUIRE(bounds_of(t.session().panels, panel::kBuilder, sc)
                .rect.contains(grip.x + kWorkspaceX, grip.y + kWorkspaceY));
    const ui::Element before = *doc::find(t.doc(), 1);

    t.press(grip.x, grip.y);
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.resizing);
    CHECK_FALSE(t.session().drag.active);
    t.motion(grip.x + 6, grip.y + 2); // a resize that never began authors nothing
    CHECK(doc::find(t.doc(), 1)->width.amount == before.width.amount);
    CHECK(doc::find(t.doc(), 1)->height.amount == before.height.amount);
    t.release(grip.x + 6, grip.y + 2);

    // Remove the panel and the very same press is the resize it always was.
    pick(t, panel::kBuilder);
    t.press(grip.x, grip.y);
    CHECK(t.notice() == "holding #1 -- drag to resize it");
    CHECK(t.session().drag.resizing);
    t.motion(grip.x + 6, grip.y + 2);
    CHECK(doc::find(t.doc(), 1)->height.amount != before.height.amount);
    t.release(grip.x + 6, grip.y + 2);
}

TEST_CASE("Info's column occupies pointer space, and an object can walk under it") {
    // INFO PARTICIPATES IN THE SAME MODEL, and this is not a manufactured case:
    // the side region is outside the WORKSPACE's extent, but an authored position
    // is not bounded by that extent -- `check_coord` guards the first cell and
    // nothing guards the last -- so an object dragged or nudged rightwards walks
    // straight under the column, and before PNL-2 a press there took hold of it.
    Live t;
    const Screen sc = screen_of(t.session());
    const ui::Rect side = bounds_of(t.session().panels, panel::kInfo, sc).rect;
    REQUIRE(side == ui::Rect{50, 0, 28, 17});

    // Walk #1 under the column with the pointer -- the press begins on the
    // workspace, so the gesture is not occluded, and the release lands INSIDE the
    // side region, which is the release-completes-a-gesture rule again.
    t.press(10, 4);
    REQUIRE(t.session().drag.active);
    t.motion(40, 4);
    t.motion(59, 4);
    t.release(59, 4);
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* under = ui::placed_for(scene, 1);
    REQUIRE(under != nullptr);
    CHECK(under->rect.x == 52);
    const std::int64_t ox = under->rect.x;
    const std::int64_t oy = under->rect.y;
    REQUIRE(side.contains(ox + kWorkspaceX, oy + kWorkspaceY));

    // IT IS THERE, AND IT IS OUT OF REACH.
    t.press(ox + 2, oy + 1);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.active);

    // Remove Info and the column is ordinary space again -- the object under it
    // was always there, and now a hand can reach it.
    pick(t, panel::kInfo);
    REQUIRE_FALSE(t.session().panels.has(panel::kInfo));
    t.press(ox + 2, oy + 1);
    CHECK(t.notice() == "holding #1 -- drag to move it");
    CHECK(t.session().drag.active);
    t.release(ox + 2, oy + 1);
}

// ----------------------------------------------------------------------------
// Tier 6b -- PNL-2a: a region a hand cannot reach through, an eye cannot either
// ----------------------------------------------------------------------------
//
// PNL-2 left exactly one thing measured and unrepaired. Occupancy is by BOUNDS;
// visibility was by whatever the painter happened to write -- so Info refused a
// press across the whole of 28x17 cells while showing the document through most
// of them. An object walked under the column painted its body and its selection
// ring straight through the panel, and the terminal medium read
// `*#####PROPERTIES#############*` on one row: one rectangle saying two things.
//
// The repair is one rect, from the primitive the Builder and the picker already
// take theirs from, at the bounds this painter was already handed. What the cases
// below pin is that it is the SAME rectangle -- not a second one that agrees today
// -- and that occlusion did NOT become a per-cell painted mask, which would have
// made what a maker can press depend on the length of a label.

TEST_CASE("Info's backdrop is the rectangle the pointer meets, on whatever screen it is") {
    Live t;
    t.key(input::scan::kN); // something to look at, so this is not the empty column
    const Screen sc = screen_of(t.session());
    const ui::Rect side = bounds_of(t.session().panels, panel::kInfo, sc).rect;
    REQUIRE(side == ui::Rect{50, 0, 28, 17});

    // ONE backdrop, and it IS the bounds. An equality, so a panel that painted
    // most of its region -- which is the defect, one degree weaker -- cannot pass.
    const surface::SurfaceCanvas& c = t.canvases.back();
    CHECK(has_rect(c, side.x, side.y, side.w, side.h, surface::role::kMuted));
    std::size_t backdrops = 0;
    for (const surface::SurfaceRect& r : c.rects) {
        if (r.x == side.x && r.y == side.y && r.w == side.w && r.h == side.h) {
            ++backdrops;
        }
    }
    CHECK(backdrops == 1);

    // The two answers stop together: both corners of the rectangle are occupied,
    // and the gutter cell one column to its left is neither painted by it nor
    // occupied. A backdrop that ran one cell wide of the bounds would say so here.
    CHECK(occupied_at(t.session().panels, sc, side.x, side.y).occupied);
    CHECK(occupied_at(t.session().panels, sc, side.x + side.w - 1, side.y + side.h - 1).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, sc, side.x - 1, side.y).occupied);

    // AND IT IS RESOLVED, NOT REMEMBERED. A bigger surface moves the side region;
    // the backdrop is at the new answer and is not at the old one. A second copy of
    // the geometry would still be painting the column a maker no longer has.
    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    const Screen big = screen_of(t.session());
    const ui::Rect moved = bounds_of(t.session().panels, panel::kInfo, big).rect;
    REQUIRE(moved.x > side.x);
    const surface::SurfaceCanvas& after = t.canvases.back();
    CHECK(has_rect(after, moved.x, moved.y, moved.w, moved.h, surface::role::kMuted));
    CHECK_FALSE(has_rect(after, side.x, side.y, side.w, side.h, surface::role::kMuted));
}

TEST_CASE("what is inside Info's bounds is what Info painted, and nothing underneath it") {
    // THE PICTURE PNL-2 RECORDED, ASSERTED AWAY -- and this is the case that fails
    // the moment Info goes back to label-only painting.
    WorkshopDoc d;
    // Authored at 46 and 20 cells wide, this runs from the workspace, across the
    // two-cell gutter, and well into the side region -- so this is a region
    // something is genuinely trying to bleed into rather than one nothing reaches.
    // An authored position is not bounded by the workspace extent (PNL-2), which is
    // why a maker can reach this state with their own hand.
    doc::add(d, "under", 46, 3, ui::Extent{ui::kExtentCells, 20},
             ui::Extent{ui::kExtentCells, 5});
    Session s;
    s.selected = d.elements[0].id;
    refocus(d, s);

    const Screen sc = screen_of(s);
    const ui::Rect side = bounds_of(s.panels, panel::kInfo, sc).rect;
    const ui::Scene scene = workspace_scene(d, s);
    REQUIRE(scene.items.size() == 1);
    const ui::Rect obj = scene.items[0].rect;
    REQUIRE(kWorkspaceX + obj.x + obj.w > side.x);

    const std::vector<std::string> screen = rasterized(paint(d, s));
    REQUIRE(screen.size() == static_cast<std::size_t>(sc.h));
    for (const std::string& line : screen) {
        REQUIRE(line.size() == static_cast<std::size_t>(sc.w));
    }

    // The object really is drawn right up to the panel's left edge...
    const std::size_t row = static_cast<std::size_t>(kWorkspaceY + obj.y + 1);
    CHECK(screen[row][static_cast<std::size_t>(side.x - 1)] == '#');

    // ...and inside the panel every cell is the cell INFO painted, compared against
    // one canvas holding nothing but this panel, through the same rasterizer.
    surface::SurfaceCanvas alone;
    alone.width = sc.w;
    alone.height = sc.h;
    paint_info(alone, d, s, side, sc);
    const std::vector<std::string> only_info = rasterized(alone);
    REQUIRE(only_info.size() == screen.size());

    // ROW 0 IS THE ONE EXCEPTION AND IT IS NOT INFO'S DOING: `paint` writes the
    // screen's own `shift+space terminal` hint measured from the right edge, so it
    // lands on this region's top row beside OBJECTS. It survives because a backdrop
    // is a RECT and every label is drawn over every rect -- which is also the reason
    // Info's rows are not padded to its width the way a stacked panel's are.
    std::size_t compared = 0;
    std::size_t differed = 0;
    std::int64_t first_bad_x = -1;
    std::int64_t first_bad_y = -1;
    for (std::int64_t y = side.y + 1; y < side.y + side.h; ++y) {
        for (std::int64_t x = side.x; x < side.x + side.w; ++x) {
            ++compared;
            const std::size_t yi = static_cast<std::size_t>(y);
            const std::size_t xi = static_cast<std::size_t>(x);
            if (screen[yi][xi] != only_info[yi][xi]) {
                ++differed;
                if (first_bad_x < 0) {
                    first_bad_x = x;
                    first_bad_y = y;
                }
            }
        }
    }
    CHECK(compared == static_cast<std::size_t>((side.h - 1) * side.w));
    CHECK(first_bad_x == -1); // named, so a failure says WHICH cell showed through
    CHECK(first_bad_y == -1);
    CHECK(differed == 0);
    CHECK(screen[0].find("OBJECTS") != std::string::npos);
    CHECK(screen[0].find("shift+space terminal") != std::string::npos);
}

TEST_CASE("removing Info takes its backdrop with it, and reopening brings both back") {
    // A PANEL THAT IS NOT THERE HIDES NOTHING, which is the other half of the same
    // claim: the backdrop belongs to the panel, so it leaves when the panel does and
    // the document underneath is exactly the document it always was.
    Live t;
    const Screen sc = screen_of(t.session());
    const ui::Rect side = bounds_of(t.session().panels, panel::kInfo, sc).rect;

    // Walk #1 under the column by hand, so the column has something to be hiding.
    t.press(10, 4);
    REQUIRE(t.session().drag.active);
    t.motion(40, 4);
    t.motion(59, 4);
    t.release(59, 4);
    // The scene is held in a named value: `placed_for` answers with a pointer INTO
    // it, so asking a temporary would hand back a pointer to something already gone.
    const ui::Scene scene = workspace_scene(t.doc(), t.session());
    const ui::Placed* under = ui::placed_for(scene, 1);
    REQUIRE(under != nullptr);
    const std::int64_t ox = kWorkspaceX + under->rect.x + 1;
    const std::int64_t oy = kWorkspaceY + under->rect.y + 1;
    REQUIRE(side.contains(ox, oy));
    const std::size_t row = static_cast<std::size_t>(oy);

    const std::string shown = info_text(t.canvases.back(), sc);
    REQUIRE(shown.find("OBJECTS") != std::string::npos);
    REQUIRE(has_rect(t.canvases.back(), side.x, side.y, side.w, side.h, surface::role::kMuted));
    // HIDDEN BY THE BODY, not by the backdrop's dot (HD-7). A region owns its interior and
    // writes every row it was given, so the cell over the object now carries whatever the
    // body's own row carries there rather than the panel's `.` fill. Either way the object
    // underneath is covered -- which is what this case is about -- and the check is against
    // what the panel PUBLISHED rather than against a character somebody expected.
    const auto body_char = [&](const surface::SurfaceCanvas& canvas) {
        const InfoBodyPlace place = body_place(t);
        const surface::SurfaceTextRegion* published = body_on(canvas, place);
        REQUIRE(published != nullptr);
        const std::size_t prose = static_cast<std::size_t>(oy - place.region_y);
        REQUIRE(prose < published->rows.size());
        const std::string& text = published->rows[prose].text;
        const std::size_t col = static_cast<std::size_t>(ox - place.region_x);
        return col < text.size() ? text[col] : ' ';
    };
    CHECK(rasterized(t.canvases.back())[row][static_cast<std::size_t>(ox)] ==
          body_char(t.canvases.back()));

    pick(t, panel::kInfo);
    REQUIRE_FALSE(t.session().panels.has(panel::kInfo));
    const surface::SurfaceCanvas& gone = t.canvases.back();
    CHECK_FALSE(has_rect(gone, side.x, side.y, side.w, side.h, surface::role::kMuted));
    CHECK(info_text(gone, sc).empty());
    // AND THE OBJECT IS BACK IN VIEW, on the very cell the panel was covering.
    CHECK(rasterized(gone)[row][static_cast<std::size_t>(ox)] == '#');

    pick(t, panel::kInfo);
    const surface::SurfaceCanvas& back = t.canvases.back();
    CHECK(info_text(back, sc) == shown);
    CHECK(has_rect(back, side.x, side.y, side.w, side.h, surface::role::kMuted));
    CHECK(rasterized(back)[row][static_cast<std::size_t>(ox)] == body_char(back));
}

TEST_CASE("the picker occupies the slot it opens over, and answers for it while it is there") {
    // THE PICKER IS A MODE AND NOT A PANEL -- no catalog row, no instance -- but
    // it is a box a maker can read, and PNL-0 went to the trouble of padding it to
    // a whole slot precisely so it could not be read through. A box that cannot be
    // read through and CAN be pressed through is the same defect wearing the other
    // half of its costume.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    REQUIRE_FALSE(t.session().panels.has(panel::kBuilder)); // nothing else is in that slot

    t.press(10, 5);
    CHECK(t.notice() == "+ panel is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.active);

    // Dismissed, the slot is workspace again.
    t.key(input::scan::kEscape);
    t.press(10, 5);
    CHECK(t.notice() == "holding #1 -- drag to move it");
    t.release(10, 5);

    // AND WHEN A PANEL IS UNDER IT, THE ANSWER IS THE PICKER -- what a maker would
    // say is there, which is the topmost and not the first. The two rectangles are
    // the same one; the names are not.
    open_builder(t);
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    const Screen sc = screen_of(t.session());
    CHECK(occupied_at(t.session().panels, sc, 10, 6).occupied);
    CHECK(std::string(occupied_at(t.session().panels, sc, 10, 6).what) == kPickerName);
    CHECK(picker_bounds(sc) == bounds_of(t.session().panels, panel::kBuilder, sc).rect);
}

TEST_CASE("the terminal overlay outranks panels for the pointer too, and by a wider rule") {
    // A MODE, NOT A PLACE. While the pane is open the pointer does nothing
    // ANYWHERE -- inside a panel, outside every panel, on an object in the clear.
    // That ordering is unchanged by PNL-2 and is deliberately wider than
    // occupancy: a maker typing into the pane is not also authoring in the
    // workspace, and a maker with a panel open is.
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    t.toggle_terminal();
    REQUIRE(t.session().terminal.open);

    const std::string said = t.notice();
    const ui::Element before = *doc::find(t.doc(), 1);
    t.press(10, 5); // inside the Builder's bounds
    t.press(7, 11); // on #2, in the clear
    t.motion(9, 12);
    t.release(9, 12);
    CHECK(t.notice() == said); // not one of them wrote a word
    CHECK_FALSE(t.session().drag.active);
    CHECK(doc::find(t.doc(), 1)->x == before.x);

    // Closing it restores both the panel's occupancy and the workspace's.
    t.toggle_terminal();
    REQUIRE_FALSE(t.session().terminal.open);
    t.press(10, 5);
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    t.press(7, 11);
    CHECK(t.notice() == "holding #2 -- drag to move it");
    t.release(7, 11);
}

TEST_CASE("what a panel is painted at and what it occupies are one resolved truth") {
    // THE STRUCTURAL CLAIM, swept over every cell of the canvas rather than
    // sampled: the occupancy answer IS the union of the open panels' bounds, and
    // those are the same rectangles `paint_panels` hands the painters. There is no
    // second geometry to drift.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    const Screen sc = screen_of(t.session());
    const Panels& panels = t.session().panels;
    REQUIRE(panels.open.size() == 2);

    const Sweep swept = sweep_canvas(panels, sc);
    CHECK(swept.cells == static_cast<std::size_t>(sc.w * sc.h));
    CHECK(swept.first_bad_x == -1); // named, so a failure says WHICH cell disagreed
    CHECK(swept.first_bad_y == -1);
    CHECK(swept.disagreed == 0);
    // Both places, exactly: 48x9 of stack and 28x17 of side region.
    CHECK(swept.occupied == static_cast<std::size_t>(48 * 9 + 28 * 17));
    CHECK(swept.painted == swept.occupied);

    // AND WHAT THE PAINTERS ACTUALLY WROTE IS INSIDE THE SPACE THAT IS OCCUPIED --
    // painted through the same one call the screen makes, into a canvas holding
    // nothing else.
    surface::SurfaceCanvas only_panels;
    paint_panels(only_panels, t.doc(), t.session(), sc);
    REQUIRE_FALSE(only_panels.labels.empty());
    for (const surface::SurfaceLabel& l : only_panels.labels) {
        CHECK(occupied_at(panels, sc, l.x, l.y).occupied);
    }
    // ONE BACKDROP PER OPEN PANEL, AND EACH IS ITS OWN BOUNDS (PNL-2a). Asserted as
    // an EQUALITY against `bounds_of` rather than as containment, because the thing
    // that went wrong for two phases was a panel painting less than it occupied --
    // and "inside the occupied space" is satisfied by a backdrop over half of it.
    REQUIRE(only_panels.rects.size() == panels.open.size());
    for (const Panel& p : panels.open) {
        const ui::Rect pb = bounds_of(panels, p.kind, sc).rect;
        CHECK(has_rect(only_panels, pb.x, pb.y, pb.w, pb.h, surface::role::kMuted));
    }
    for (const surface::SurfaceRect& r : only_panels.rects) {
        CHECK(occupied_at(panels, sc, r.x, r.y).occupied);
        CHECK(occupied_at(panels, sc, r.x + r.w - 1, r.y + r.h - 1).occupied);
    }
}

TEST_CASE("occupancy is resolved against the screen, not remembered from one") {
    // A BIGGER SURFACE MOVES THE SIDE REGION, and the cells it occupies move with
    // it -- because the occupancy answer is `bounds_of` on the CURRENT screen and
    // is cached nowhere. The stack is anchored to the other corner and does not
    // move, which is the same asymmetry the painting has.
    Live t;
    const Screen small = screen_of(t.session());
    const ui::Rect side_small = bounds_of(t.session().panels, panel::kInfo, small).rect;
    REQUIRE(occupied_at(t.session().panels, small, side_small.x, 4).occupied);

    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    const Screen big = screen_of(t.session());
    REQUIRE(big.w == 100);
    const ui::Rect side_big = bounds_of(t.session().panels, panel::kInfo, big).rect;
    CHECK(side_big.x == big.panel_x);
    CHECK(side_big.x > side_small.x);

    // The column a maker presses is the column they can see, on either screen.
    CHECK(occupied_at(t.session().panels, big, side_big.x, 4).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, big, side_small.x, 4).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, big, side_big.x - 1, 4).occupied);

    // AND THE PRESS FOLLOWS IT, through the whole live path: what was the Info
    // column on the small screen is ordinary workspace on the big one.
    t.press(side_small.x, 3);
    CHECK(t.notice() == "nothing there");
    t.press(side_big.x, 3);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
}

TEST_CASE("a closed panel occupies nothing, and neither does a screen with none open") {
    // THE OTHER HALF OF THE INVARIANT, and the one a maker feels every second: a
    // panel that is not open takes nothing away. `bounds_of` answers a closed
    // panel with an empty rectangle, `contains` says an empty rectangle holds
    // nothing, and the whole canvas is therefore the workspace's again.
    Live t;
    pick(t, panel::kInfo); // remove the one panel a fresh session opens with
    REQUIRE(t.session().panels.open.empty());
    const Screen sc = screen_of(t.session());
    const Sweep swept = sweep_canvas(t.session().panels, sc);
    CHECK(swept.cells == static_cast<std::size_t>(sc.w * sc.h));
    CHECK(swept.occupied == 0);
    CHECK(swept.disagreed == 0);
    // Including the cells the two places WOULD have had.
    CHECK_FALSE(
        occupied_at(t.session().panels, sc, picker_bounds(sc).x, picker_bounds(sc).y).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, sc, sc.panel_x, 0).occupied);

    // And a press in the vacated column reaches the workspace, which is what "the
    // panel was the only thing in the way" means.
    t.press(10, 5);
    CHECK(t.notice() == "holding #1 -- drag to move it");
    t.release(10, 5);
}

TEST_CASE("the window's pixels meet the same panel the terminal's cells do") {
    // THE OTHER MEDIUM, through the graphical Skin's own projection. Occupancy is
    // asked in CANVAS cells, so both media arrive at the same question -- and the
    // case is worth having because the two report different numbers for one place,
    // which is exactly where a second geometry would hide.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    t.press_px(10, 5);
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.active);
    t.release_px(10, 5);

    t.press_px(7, 11); // below the panel, on #2
    CHECK(t.notice() == "holding #2 -- drag to move it");
    t.release_px(7, 11);
}

TEST_CASE("the cells just outside a panel are ordinary workspace, on every edge") {
    // WHERE THE OCCLUSION STOPS, asked at the boundary rather than in the middle.
    // A press one row below the stack's last row is the cell that separates "the
    // panel occupies what it covers" from "the panel occupies a bit more", and it
    // is also the cell that separates CANVAS cells from WORKSPACE cells: the two
    // spaces are one row apart, so a question asked in the wrong one is invisible
    // everywhere except here.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);
    const Screen sc = screen_of(t.session());
    const ui::Rect stack = bounds_of(t.session().panels, panel::kBuilder, sc).rect;
    const ui::Rect side = bounds_of(t.session().panels, panel::kInfo, sc).rect;

    // The four edges of each place, in canvas cells: inside, then one cell out.
    CHECK(occupied_at(t.session().panels, sc, stack.x, stack.y + stack.h - 1).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, sc, stack.x, stack.y + stack.h).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, sc, stack.x, stack.y - 1).occupied);
    CHECK(occupied_at(t.session().panels, sc, stack.x + stack.w - 1, stack.y).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, sc, stack.x + stack.w, stack.y).occupied);
    CHECK(occupied_at(t.session().panels, sc, side.x, side.y + side.h - 1).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, sc, side.x - 1, side.y).occupied);
    CHECK_FALSE(occupied_at(t.session().panels, sc, side.x, side.y + side.h).occupied);

    // AND THROUGH THE LIVE PRESS, which is the half that would catch a question
    // asked in the wrong space: the row under the panel is empty workspace, and
    // it says the empty-workspace sentence rather than the panel's.
    const std::int64_t below = stack.y + stack.h - kWorkspaceY; // workspace row under it
    REQUIRE_FALSE(stack.contains(10 + kWorkspaceX, below + kWorkspaceY));
    t.press(10, below);
    CHECK(t.notice() == "nothing there");
    t.press(10, below - 1); // the panel's own last row
    CHECK(t.notice() == "Builder is here -- nothing under it can be taken hold of");
}

// ---- HD-3: the caret, and the pointer inside the pane -----------------------------------
//
// Everything below drives the same functions `workshop.cpp` binds and the same functions
// `paint_terminal` draws with. There is no second geometry anywhere in these cases: where a
// case needs a pixel it computes it from `terminal_input_place`, which is the function under
// test as well as the one the painter uses -- so a case that agreed with a private copy of
// the arithmetic would be agreeing with itself.

/// The window pixel at the middle of a given prose column/row of the pane's own region.
/// Deliberately the INVERSE of `terminal_input_place`'s resolution rather than a second
/// spelling of it: a helper that recomputed the layout could be wrong in the same direction
/// as the code it is checking.
static std::int64_t pane_pixel_x(const TerminalInputPlace& p, std::int64_t column) {
    return p.region_x * surface::kCanvasCellPx + p.fit.origin_x + column * p.fit.advance_px +
           p.fit.advance_px / 2;
}
static std::int64_t pane_pixel_y(const TerminalInputPlace& p, std::int64_t row) {
    return p.region_y * surface::kCanvasCellPx + p.fit.origin_y + row * p.fit.line_px +
           p.fit.line_px / 2;
}

TEST_CASE("caret geometry: a byte index and a prose column are one number, both ways") {
    // §22. PURE ARITHMETIC, across four faces rather than the live 8x18 one -- what is being
    // pinned is the mapping, and a case written against a single metric would pass for a
    // reason it does not claim. JetBrains Mono's raster is not pinned anywhere here.
    struct Face {
        std::int64_t advance;
        std::int64_t line;
    };
    for (const Face f : {Face{0, 0}, Face{6, 14}, Face{8, 18}, Face{11, 23}, Face{15, 31}}) {
        const Screen sc = screen_of(kScreenMinW, kScreenMinH, f.advance, f.line);
        const TerminalInputPlace p = terminal_input_place(sc);
        CAPTURE(f.advance);

        // THE LINE IS THE PANE'S LAST PROSE ROW, whichever lattice this medium counts in.
        CHECK(p.prose_row == static_cast<std::int64_t>(sc.terminal_lines) - 1);
        CHECK(p.region_x == sc.terminal_x);
        CHECK(p.region_y == sc.terminal_y);
        CHECK(p.first_column == kTerminalPromptCols);
        CHECK(p.columns == sc.terminal_cols - kTerminalPromptCols - kTerminalCaretCols);

        // CARET 0, MIDDLE, END -- one column per byte, offset by the prompt.
        CHECK(terminal_caret_column(p, box_of(0, 0, 0)) == kTerminalPromptCols);
        CHECK(terminal_caret_column(p, box_of(5, 5, 0)) == kTerminalPromptCols + 5);
        CHECK(terminal_caret_column(p, box_of(40, 40, 0)) == kTerminalPromptCols + 40);

        // ...AND BACK. The two are inverses over every position a line of this length has,
        // which is the property a click-then-type depends on.
        for (std::size_t at = 0; at <= 12; ++at) {
            CHECK(terminal_caret_of_column(p, box_of(12, 12, 0),
                                          terminal_caret_column(p, box_of(at, at, 0))) == at);
        }

        // THE BOUNDARIES, written down rather than left to the arithmetic.
        CHECK(terminal_caret_of_column(p, box_of(12, 12, 0), 0) == 0);                     // before the prompt
        CHECK(terminal_caret_of_column(p, box_of(12, 12, 0), 1) == 0);                     // inside the prompt
        CHECK(terminal_caret_of_column(p, box_of(12, 12, 0), -400) == 0);                  // far to the left
        CHECK(terminal_caret_of_column(p, box_of(12, 12, 0), kTerminalPromptCols + 99) == 12); // past the text
        CHECK(terminal_caret_of_column(p, box_of(0, 0, 0), kTerminalPromptCols) == 0);    // an empty line

        // AN EMPTY LINE HAS EXACTLY ONE POSITION, and every column on the row names it.
        for (std::int64_t col = -3; col < 20; ++col) {
            CHECK(terminal_caret_of_column(p, box_of(0, 0, 0), col) == 0);
        }

        // THE ROW IS THE WHOLE HIT TEST, and the column deliberately is not: a maker aiming
        // past the end of a short command clicks empty room, and that is the most obvious
        // gesture the row has.
        CHECK(terminal_input_hit(p, kTerminalPromptCols, p.prose_row));
        CHECK(terminal_input_hit(p, 0, p.prose_row));
        CHECK(terminal_input_hit(p, p.fit.columns, p.prose_row));
        CHECK_FALSE(terminal_input_hit(p, kTerminalPromptCols, p.prose_row - 1));
        CHECK_FALSE(terminal_input_hit(p, kTerminalPromptCols, 0));
        CHECK_FALSE(terminal_input_hit(p, -1, p.prose_row));
        CHECK_FALSE(terminal_input_hit(p, p.fit.columns + 1, p.prose_row));
    }

    // TOTAL over the number line on both sides: a column arrives from a pointer off the bus
    // and a length from a line a maker typed.
    const TerminalInputPlace p = terminal_input_place(screen_of(kScreenMinW, kScreenMinH, 8, 18));
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    CHECK(terminal_caret_of_column(p, box_of(5, 5, 0), kMin) == 0);
    CHECK(terminal_caret_of_column(p, box_of(5, 5, 0), kMax) == 5);
    CHECK(terminal_caret_column(p, box_of(0, 0, 0)) == kTerminalPromptCols);
}

TEST_CASE("HD-3: typing lands at the caret, and submission does not depend on where it is") {
    // §24, driven end to end through the real weave: keys in, a line out.
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("send * SurfaceText 1 slot=s")) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.pane().input.text() == "send * SurfaceText 1 slot=s");
    REQUIRE(t.pane().input.caret() == t.pane().input.size());

    // FIVE STEPS BACK, then repair the middle -- the workflow this phase exists for.
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kLeft);
    }
    CHECK(t.pane().input.caret() == t.pane().input.size() - 5);
    t.text("X");
    CHECK(t.pane().input.text() == "send * SurfaceText 1 sXlot=s"); // at byte 22, inside `slot`
    t.key(input::scan::kBackspace);
    CHECK(t.pane().input.text() == "send * SurfaceText 1 slot=s");

    // DELETE ERASES FORWARD AND HOME/END JUMP -- all three arrive on the SDL wire today
    // (translate_sdl passes SDL's scancode through), which is why they are bound.
    t.key(input::scan::kHome);
    CHECK(t.pane().input.caret() == 0);
    t.key(input::scan::kDelete);
    CHECK(t.pane().input.text() == "end * SurfaceText 1 slot=s");
    t.text("s");
    CHECK(t.pane().input.text() == "send * SurfaceText 1 slot=s");
    t.key(input::scan::kEnd);
    CHECK(t.pane().input.at_end());

    // SUBMISSION IS THE WHOLE LINE, EXACTLY ONCE, whatever the caret was doing. Return is
    // not "submit up to the caret" -- which is what makes a mis-typed middle repairable
    // without the repair changing what gets sent.
    t.key(input::scan::kHome);
    for (int i = 0; i < 4; ++i) {
        t.key(input::scan::kRight);
    }
    REQUIRE(t.pane().input.caret() == 4);
    const std::size_t before = me->transcript().size();
    t.key(input::scan::kReturn);
    CHECK(t.pane().input.empty());
    CHECK(t.pane().input.caret() == 0);
    std::size_t submitted = 0;
    for (const loom::TranscriptEntry& e : me->transcript().entries()) {
        if (e.text.find("send * SurfaceText 1 slot=s") != std::string::npos) {
            ++submitted;
        }
    }
    CHECK(submitted == 1);
    CHECK(me->transcript().size() > before);
}

TEST_CASE("HD-3: a press on the input row places the caret where the maker aimed") {
    // §9 and §23, in RAW PIXELS through the real bus -- the sub-cell precision the wire has
    // carried since G-1 is finally the thing being spent, and it is spent without rounding
    // to a cell first.
    Live t;
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.toggle_terminal();
    for (const char c : std::string("send @builder BuildRequsted")) {
        t.text(std::string(1, c));
    }
    const Screen sc = screen_of(t.session());
    REQUIRE(sc.text_advance_px == 8);
    const TerminalInputPlace p = terminal_input_place(sc);
    REQUIRE(p.fit.graphical());

    // ON A CHARACTER: the caret goes before the character that was pressed. `BuildRequsted`
    // starts at byte 14, so the missing `e` belongs at byte 23 -- before the `s` of `sted`.
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 23), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == 23);

    // ...AND TYPING THERE REPAIRS THE MIDDLE OF THE LINE WITHOUT CLEARING IT. This is §25's
    // acceptance workflow, in one line of test.
    t.text("e");
    CHECK(t.pane().input.text() == "send @builder BuildRequested");
    CHECK(t.pane().input.caret() == 24);

    // Backspace and Delete both work from where the pointer put it.
    t.key(input::scan::kBackspace);
    CHECK(t.pane().input.text() == "send @builder BuildRequsted");
    t.key(input::scan::kDelete);
    CHECK(t.pane().input.text() == "send @builder BuildRequted");
    t.text("s");
    t.text("e");
    CHECK(t.pane().input.text() == "send @builder BuildRequseted");
    // ...and back to the command a maker actually meant, from the middle, without the line
    // ever having been cleared.
    t.key(input::scan::kBackspace);
    t.key(input::scan::kBackspace);
    REQUIRE(t.pane().input.text() == "send @builder BuildRequted");
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 23), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    t.text("e");
    t.text("s");
    CHECK(t.pane().input.text() == "send @builder BuildRequested");

    // BEFORE THE FIRST CHARACTER -> 0, including the inset and the prompt.
    t.press_at(pane_pixel_x(p, 0), pane_pixel_y(p, p.prose_row), input::space::kPixels);
    CHECK(t.pane().input.caret() == 0);
    t.press_at(p.region_x * surface::kCanvasCellPx, pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == 0);

    // AFTER THE LAST CHARACTER -> the end of the line, wherever on the row it landed.
    t.press_at(pane_pixel_x(p, p.fit.columns), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == t.pane().input.size());

    // A PRESS ON ANOTHER ROW OF THE PANE DOES NOT MOVE IT. The pane is one region; only its
    // last prose row is the line.
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 3), pane_pixel_y(p, 0),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == t.pane().input.size());
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 3), pane_pixel_y(p, p.prose_row - 3),
               input::space::kPixels);
    CHECK(t.pane().input.caret() == t.pane().input.size());

    // ...and nor does a press outside the pane entirely, which is the same answer for a
    // different reason: it is consumed by the MODE and reaches none of its regions.
    t.press_at(4, 4, input::space::kPixels);
    CHECK(t.pane().input.caret() == t.pane().input.size());
}

TEST_CASE("HD-3: a press in the pane never reaches the workspace underneath it") {
    // §12 and §23's click-through proof. The pane covers the bottom-right of the screen,
    // workspace included, so this is not hypothetical: these presses land on cells that hold
    // real authored objects.
    Live t;
    (void)t.mount_terminal();
    const std::int64_t was_x = t.first()->x;
    const std::int64_t was_y = t.first()->y;
    const std::int64_t was_selected = t.session().selected;
    const std::string was_notice = t.notice();
    t.toggle_terminal();

    // EVERY PART OF THE SCREEN, with the pane open: on the pane, off the pane, on the
    // workspace, on an object, on the side panel. None of them may author or select.
    const Screen sc = screen_of(t.session());
    for (const std::pair<std::int64_t, std::int64_t>& cell :
         {std::pair<std::int64_t, std::int64_t>{sc.terminal_x + 4, sc.terminal_y + 4},
          {sc.terminal_x, sc.terminal_y},
          {2, 2},
          {was_x, was_y},
          {sc.panel_x + 2, 3},
          {0, 0}}) {
        t.publish(loom::to_value(input::PointerButton{
            1, true, cell.first, cell.second + surface::kTuiCanvasTopRow, input::space::kCells,
            input::mod::kNone}));
        t.publish(loom::to_value(input::PointerButton{
            1, false, cell.first, cell.second + surface::kTuiCanvasTopRow,
            input::space::kCells, input::mod::kNone}));
    }
    CHECK(t.first()->x == was_x);
    CHECK(t.first()->y == was_y);
    CHECK(t.session().selected == was_selected);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice() == was_notice); // not even a sentence: the workspace was never asked

    // MOTION IS STILL NOTHING AT ALL, which is what keeps a moved mouse from costing a
    // repaint (§13, §28). Measured as canvases published rather than argued.
    const std::size_t frames = t.canvases.size();
    for (std::int64_t i = 0; i < 20; ++i) {
        t.motion(i, i);
    }
    CHECK(t.canvases.size() == frames);

    // ...and closing the pane restores every gesture exactly.
    t.toggle_terminal();
    t.press(was_x + 1, was_y + 1);
    CHECK(t.session().drag.active);
    t.release(was_x + 1, was_y + 1);
}

TEST_CASE("HD-3: opening the pane mid-drag does not strand the gesture") {
    // REPRODUCED ON THE PRISTINE TREE FIRST (constitution rule 7), where the second check
    // below moved the object from x=3 to x=22 on a motion with no button held. PNL-2 already
    // wrote the rule this repairs: "a gesture that began on the workspace owns the pointer
    // until it ends, so its release must end it wherever the maker's hand happens to be --
    // occluding the release would strand `drag.active` true with the button up". The overlay
    // was occluding it by arriving first.
    Live t;
    (void)t.mount_terminal();
    const std::int64_t x = t.first()->x;
    const std::int64_t y = t.first()->y;
    t.press(x + 1, y + 1);
    REQUIRE(t.session().drag.active);

    t.toggle_terminal(); // shift+space, with the button still down
    REQUIRE(t.session().terminal.open);
    t.release(x + 1, y + 1);
    CHECK_FALSE(t.session().drag.active);

    // THE PROOF THAT IT MATTERS: with the pane closed again, a bare motion moves nothing,
    // because nobody is holding anything.
    t.toggle_terminal();
    const std::int64_t settled = t.first()->x;
    t.motion(x + 20, y + 1);
    CHECK(t.first()->x == settled);

    // ...AND THE OTHER DIRECTION IS UNTOUCHED (PNL-2): a drag that began on the workspace and
    // was never interrupted still ends where the hand is, panel or no panel.
    t.press(x + 1, y + 1);
    REQUIRE(t.session().drag.active);
    t.motion(x + 4, y + 1);
    t.release(x + 4, y + 1);
    CHECK_FALSE(t.session().drag.active);
    CHECK(t.notice().rfind("released #", 0) == 0);
}

TEST_CASE("HD-3: clicking a completion row selects it, and Tab accepts what was clicked") {
    // §10. ONE SELECTION, WHICHEVER HAND MOVED IT -- the click writes the field Up/Down
    // write, so `accept_completion` needs to know nothing about a pointer and the renderer
    // cannot tell which gesture chose the row.
    Live t;
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{115, 63, 8, 18}));
    t.toggle_terminal();
    t.text("s");
    t.text("e");
    t.text("n");
    t.text("d");
    t.text(" ");
    REQUIRE(t.pane().completion.open);
    REQUIRE(t.pane().completion.candidates.size() >= 3);
    REQUIRE(t.pane().completion.selected == 0);

    const Screen sc = screen_of(t.session());
    const CompletionPlace list =
        completion_place(sc, t.pane().completion.candidates.size() + 1);
    REQUIRE(list.visible);
    REQUIRE(list.rows >= 3); // heading plus at least two candidates
    const surface::RegionFit fit = surface::fit_region(list.x, list.y, list.w, list.h,
                                                       sc.text_advance_px, sc.text_line_px);
    const auto row_pixel_y = [&](std::int64_t r) {
        return list.y * surface::kCanvasCellPx + fit.origin_y + r * fit.line_px +
               fit.line_px / 2;
    };
    const auto row_pixel_x = [&](std::int64_t col) {
        return list.x * surface::kCanvasCellPx + fit.origin_x + col * fit.advance_px +
               fit.advance_px / 2;
    };

    // ROW 2 IS THE SECOND CANDIDATE (row 0 is the heading), and clicking it selects it.
    t.press_at(row_pixel_x(4), row_pixel_y(2), input::space::kPixels);
    CHECK(t.pane().completion.selected == 1);
    t.press_at(row_pixel_x(4), row_pixel_y(1), input::space::kPixels);
    CHECK(t.pane().completion.selected == 0);

    // THE HEADING IS NOT A CANDIDATE. A press on it is consumed by the list and changes
    // nothing -- and in particular does not fall through to the input line, which is not
    // underneath it at all.
    t.press_at(row_pixel_x(4), row_pixel_y(0), input::space::kPixels);
    CHECK(t.pane().completion.selected == 0);
    CHECK(t.pane().input.text() == "send ");
    CHECK(t.pane().input.at_end());

    // CLICK SELECTS; TAB ACCEPTS. The smallest unsurprising contract -- no double-click
    // machinery, no mouse-only acceptance path, and the acceptance is the one HD-2 shipped.
    t.press_at(row_pixel_x(4), row_pixel_y(2), input::space::kPixels);
    REQUIRE(t.pane().completion.selected == 1);
    const std::string wanted = t.pane().completion.candidates[1].insert;
    t.key(input::scan::kTab);
    CHECK(t.pane().input.text() == "send " + wanted);
    CHECK(t.pane().input.at_end()); // §14: the caret follows the inserted result

    // A ROW THE LIST DOES NOT HAVE selects nothing and authors nothing.
    const std::size_t chosen = t.pane().completion.selected;
    t.press_at(row_pixel_x(4), row_pixel_y(static_cast<std::int64_t>(list.rows) + 40),
               input::space::kPixels);
    CHECK(t.pane().completion.selected == chosen);
}

TEST_CASE("HD-3: the click reads the SAME window the rows were drawn with") {
    // §10's real risk, and the reason `completion_first_shown` was lifted out of
    // `completion_rows`: once the list has SCROLLED, "row 1" is not candidate 0. A second
    // copy of the windowing would be right until the first scroll and wrong afterwards --
    // which is to say wrong only when nobody is looking for it.
    for (std::size_t capacity : {std::size_t{1}, std::size_t{2}, std::size_t{4},
                                 std::size_t{9}}) {
        CAPTURE(capacity);
        for (std::size_t selected = 0; selected < 12; ++selected) {
            const std::size_t first = completion_first_shown(selected, capacity);
            if (capacity <= 1) {
                CHECK(first == 0); // no candidate row at all: the heading is the whole list
                continue;
            }
            const std::size_t room = capacity - 1;
            // THE SELECTED CANDIDATE IS ALWAYS ON A VISIBLE ROW, which is the property the
            // hit test inverts.
            CHECK(selected >= first);
            CHECK(selected < first + room);
        }
    }

    // ...and the inversion agrees with the rows actually produced, at every selection.
    Completion comp;
    comp.open = true;
    comp.heading = "shapes";
    for (int i = 0; i < 9; ++i) {
        Candidate c;
        c.display = "cand" + std::to_string(i);
        c.insert = "cand" + std::to_string(i) + " ";
        comp.candidates.push_back(c);
    }
    for (std::size_t sel = 0; sel < comp.candidates.size(); ++sel) {
        comp.selected = sel;
        const std::size_t capacity = 4;
        const std::vector<surface::SurfaceTextRow> rows = completion_rows(comp, capacity, 60);
        const std::size_t first = completion_first_shown(sel, capacity);
        REQUIRE(rows.size() >= 2);
        for (std::size_t r = 1; r < rows.size(); ++r) {
            // The row a press on visible row `r` would resolve to is the row whose text is
            // actually there.
            const std::size_t candidate = first + (r - 1);
            REQUIRE(candidate < comp.candidates.size());
            CHECK(rows[r].text.find(comp.candidates[candidate].display) != std::string::npos);
        }
    }
}

TEST_CASE("HD-3: a click on a SCROLLED list picks the row that is actually there") {
    // THE CASE THE WINDOWING EXISTS FOR, and the one a first=0 hit test passes without.
    // In a large pane the list holds every candidate, so "row 1 is candidate 0" is true by
    // accident; here the pane is the minimum one and the list is shorter than the vocabulary,
    // so the window has to slide and row 1 stops being candidate 0.
    Live t;
    (void)t.mount_terminal(/*widen=*/false, /*shapes=*/8);
    t.toggle_terminal(); // no SurfaceExtent: the minimum pane, a character IS a cell
    for (const char c : std::string("send * ")) {
        t.text(std::string(1, c));
    }
    const std::size_t n = t.pane().completion.candidates.size();
    REQUIRE(n >= 3);
    const Screen sc = screen_of(t.session());
    const CompletionPlace list = completion_place(sc, n + 1);
    REQUIRE(list.visible);
    REQUIRE(list.rows < n + 1); // it must SCROLL, or this case proves nothing
    const std::size_t room = list.rows - 1;

    // Drive the selection to the last candidate: the window has slid as far as it goes.
    for (std::size_t i = 0; i + 1 < n; ++i) {
        t.key(input::scan::kDown);
    }
    REQUIRE(t.pane().completion.selected == n - 1);
    const std::size_t first = completion_first_shown(n - 1, list.rows);
    REQUIRE(first > 0); // ...and it is no longer showing candidate 0

    // THE ROWS SAY WHICH CANDIDATES ARE THERE, and the top visible one is `first`.
    const std::vector<surface::SurfaceTextRow> rows =
        completion_rows(t.pane().completion, list.rows, sc.terminal_cols);
    REQUIRE(rows.size() == list.rows);
    CHECK(rows[1].text.find(t.pane().completion.candidates[first].display) !=
          std::string::npos);

    // ...so a press on visible row 1 must select candidate `first`, not candidate 0. That is
    // the assertion a hit test which ignored the window would fail, and it can only fail
    // once the list has scrolled -- which is to say only when nobody is looking for it.
    t.publish(loom::to_value(input::PointerButton{
        1, true, list.x + 4, list.y + 1 + surface::kTuiCanvasTopRow, input::space::kCells,
        input::mod::kNone}));
    CHECK(t.pane().completion.selected == first);
    CHECK(t.pane().completion.selected != 0);

    // EVERY VISIBLE ROW, not only the first: the mapping is the whole window, inverted.
    for (std::size_t r = 1; r <= room && first + (r - 1) < n; ++r) {
        const std::size_t before = t.pane().completion.selected;
        const std::size_t window = completion_first_shown(before, list.rows);
        t.publish(loom::to_value(input::PointerButton{
            1, true, list.x + 4, list.y + static_cast<std::int64_t>(r) + surface::kTuiCanvasTopRow,
            input::space::kCells, input::mod::kNone}));
        CHECK(t.pane().completion.selected == window + (r - 1));
    }
}

TEST_CASE("HD-3: completion follows the END of the line, and says so when it cannot") {
    // §5. HD-2's completer rests on an assumption that was free when the caret could not
    // move -- the token being completed is the LAST one, so accepting is "drop what has been
    // typed of this token, append what it was going to be". With the caret in the middle,
    // that edit would delete everything after it. The smallest honest policy is to say so.
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{115, 63, 8, 18}));
    t.toggle_terminal();
    for (const char c : std::string("send * Surface")) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.pane().completion.open);
    REQUIRE_FALSE(t.pane().completion.candidates.empty());
    const std::size_t offered = t.pane().completion.candidates.size();

    // MOVE THE CARET OFF THE END, and the list stops answering a question the maker is no
    // longer standing in.
    t.key(input::scan::kLeft);
    CHECK(t.pane().completion.open); // it still SAYS something...
    CHECK(t.pane().completion.candidates.empty()); // ...and it is not a candidate
    CHECK(t.pane().completion.heading.find("END of the line") != std::string::npos);

    // A HEADING WITH NO CANDIDATES IS EXACTLY HD-2's SHAPE, so every gesture that depends on
    // "is there something to choose" is unchanged: Tab does not accept, Escape still clears.
    t.key(input::scan::kTab);
    CHECK(t.pane().input.text() == "send * Surface"); // nothing was accepted
    t.key(input::scan::kUp);
    CHECK(t.pane().completion.selected == 0);

    // BACK TO THE END, and the same candidates come back -- the policy is about WHERE the
    // caret is, and holds nothing of its own.
    t.key(input::scan::kEnd);
    CHECK(t.pane().completion.candidates.size() == offered);
    CHECK(t.pane().completion.heading.find("END of the line") == std::string::npos);

    // A CLICK MOVES IT THE SAME WAY A KEY DOES -- one state, two hands.
    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    t.press_at(pane_pixel_x(p, kTerminalPromptCols + 2), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    REQUIRE(t.pane().input.caret() == 2);
    CHECK(t.pane().completion.candidates.empty());
    t.press_at(pane_pixel_x(p, p.fit.columns), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    CHECK(t.pane().input.at_end());
    CHECK(t.pane().completion.candidates.size() == offered);

    // AND NONE OF IT AUTHORS ANYTHING (§23, §20). Browsing, clicking, moving the caret --
    // measured by SENDER, because Workshop's own weave publishes to the same office on every
    // repaint.
    std::size_t authored = 0;
    for (const loom::TranscriptEntry& e : me->transcript().entries()) {
        (void)e;
        ++authored;
    }
    CHECK(authored == 0);
    CHECK(me->pending().empty());
    CHECK_FALSE(me->awaiting());
}

TEST_CASE("HD-3: the pane publishes a caret, and both media answer it in their own type") {
    // §6. The publisher stopped appending `_`; the position is a fact ABOUT the region now,
    // so a window fills a bar and a cell medium inserts a character -- and for a caret at the
    // end of the line the character medium's answer is byte-for-byte the row that was there
    // before this phase.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("send * S")) {
        t.text(std::string(1, c));
    }
    // ESCAPE DISMISSES THE LIST AND LEAVES THE LINE (HD-2), which is why the prefix is one
    // the catalog actually answers: over a prefix that matched nothing the list is
    // heading-only, and Escape goes on meaning "clear the line" there.
    REQUIRE_FALSE(t.pane().completion.candidates.empty());
    t.key(input::scan::kEscape);
    REQUIRE(t.pane().input.text() == "send * S");

    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_row == static_cast<std::int64_t>(pane.rows.size()) - 1);
        CHECK(pane.caret_col == kTerminalPromptCols + 8);
        CHECK(pane.rows.back().text == "> send * S");
        surface::SurfaceCanvas only_pane = t.canvases.back();
        only_pane.texts = {pane}; // the pane alone: the list and the property body are others
        const std::vector<surface::ProjectedRow> shown =
            surface::project_text_regions(only_pane);
        CHECK(shown.back().label.text.rfind("> send * S_", 0) == 0);
    }

    // IN THE MIDDLE: the region says where, the row does not move, and the cell projection
    // puts the mark between the two characters the next keystroke lands between.
    t.key(input::scan::kLeft);
    t.key(input::scan::kLeft);
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col == kTerminalPromptCols + 6);
        CHECK(pane.rows.back().text == "> send * S"); // the LINE is unchanged
        surface::SurfaceCanvas only_pane = t.canvases.back();
        only_pane.texts = {pane}; // the pane alone: the list and the property body are others
        const std::vector<surface::ProjectedRow> shown =
            surface::project_text_regions(only_pane);
        CHECK(shown.back().label.text.rfind("> send *_ S", 0) == 0);
    }

    // AN EMPTY LINE still names the gesture that asks, and the caret sits at its start --
    // which is the identical row HD-2 published, reassembled from two facts instead of one.
    t.key(input::scan::kEscape);
    REQUIRE(t.pane().input.empty());
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col == kTerminalPromptCols);
        surface::SurfaceCanvas only_pane = t.canvases.back();
        only_pane.texts = {pane}; // the pane alone: the list and the property body are others
        const std::vector<surface::ProjectedRow> shown =
            surface::project_text_regions(only_pane);
        CHECK(shown.back().label.text.rfind("> _   tab: what can this terminal say?", 0) == 0);
    }

    // THE COMPLETION LIST CARRIES NO CARET. It is a second region of the same pane and it is
    // not where anybody is typing -- said here because "there is exactly one caret on this
    // canvas" is the closest thing this application has to a focus fact, and it is a
    // consequence of who publishes rather than of a service that arbitrates.
    t.text("s");
    const Screen sc = screen_of(t.session());
    const surface::SurfaceTextRegion* list = list_of(t.canvases.back(), sc);
    REQUIRE(list != nullptr);
    CHECK(pane_of(t.canvases.back(), sc)->caret_row >= 0);
    CHECK(list->caret_row == surface::kNoCaret);
    // ...AND NEITHER DOES THE INSPECTOR'S PROPERTY BODY while the pane has the keys, which
    // is what "exactly one caret on this canvas" means now that there are three regions.
    CHECK(t.canvases.back().texts.front().caret_row == surface::kNoCaret);
}

TEST_CASE("HD-3: hit geometry follows presentation geometry across a resize") {
    // §25 item 12 and the report's resize witness: the same click lands on the same
    // character before and after the window changes size, because both the picture and the
    // hit test are resolved from the one `Screen` the medium's extent produced.
    Live t;
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.toggle_terminal();
    for (const char c : std::string("send * SurfaceText")) {
        t.text(std::string(1, c));
    }

    for (const surface::SurfaceExtent& e :
         {surface::SurfaceExtent{78, 22, 8, 18}, surface::SurfaceExtent{115, 63, 8, 18},
          surface::SurfaceExtent{140, 40, 11, 23}, surface::SurfaceExtent{78, 22, 0, 0}}) {
        t.publish(loom::to_value(e));
        const Screen sc = screen_of(t.session());
        const TerminalInputPlace p = terminal_input_place(sc);
        CAPTURE(sc.terminal_x);
        CAPTURE(sc.text_advance_px);

        // THE PANE ITSELF MOVED -- this is not a case where nothing changed.
        CHECK(p.region_x == sc.terminal_x);

        for (const std::size_t want : {std::size_t{0}, std::size_t{7}, std::size_t{18}}) {
            // THE WINDOW THE PANE ACTUALLY HOLDS, never a literal zero (HD-4): this line is
            // short enough not to scroll at any of these extents, and reading the offset
            // rather than assuming it is what makes that a fact of the run instead of an
            // assumption of the case.
            const std::size_t from = t.pane().input.first_visible();
            if (p.fit.graphical()) {
                t.press_at(pane_pixel_x(p, terminal_caret_column(p, box_of(want, want, from))),
                           pane_pixel_y(p, p.prose_row), input::space::kPixels);
            } else {
                // No metric: a character IS a cell, and the medium reports cells.
                t.press_at(p.region_x + terminal_caret_column(p, box_of(want, want, from)),
                           p.region_y + p.prose_row + surface::kTuiCanvasTopRow,
                           input::space::kCells);
            }
            CHECK(t.pane().input.caret() == want);
        }

        // ...and the CARET THE PANE DREW is at the column the press resolved to, which is
        // the whole of "the geometry that drew it is the geometry that hit it".
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col == terminal_caret_column(p, t.pane().input));
        CHECK(pane.caret_row == p.prose_row);
    }
}

TEST_CASE("HD-3: a terminal medium's press reaches the same local hit model") {
    // §17. The graphical lane is where this phase closes, but the routing is stamped with
    // `space` rather than with a backend identity -- so a cell-reporting medium takes the
    // other branch of `prose_at` and lands on the same caret. Whether a terminal BACKEND can
    // report a press is a separate question this phase did not touch (no escape-sequence
    // work, no terminal mouse protocol); what is proven here is that the Terminal's own hit
    // model does not stand in the way.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal(); // no SurfaceExtent: a character IS a cell here
    for (const char c : std::string("send * Ping")) {
        t.text(std::string(1, c));
    }
    const Screen sc = screen_of(t.session());
    REQUIRE(sc.text_advance_px == 0);
    const TerminalInputPlace p = terminal_input_place(sc);
    REQUIRE_FALSE(p.fit.graphical());

    for (const std::size_t want : {std::size_t{0}, std::size_t{4}, std::size_t{11}}) {
        t.publish(loom::to_value(input::PointerButton{
            1, true, p.region_x + terminal_caret_column(p, box_of(want, want, t.pane().input.first_visible())),
            p.region_y + p.prose_row + surface::kTuiCanvasTopRow, input::space::kCells,
            input::mod::kNone}));
        CHECK(t.pane().input.caret() == want);
    }

    // A SPACE THIS APPLICATION DOES NOT RECOGNISE MOVES NOTHING, and is still consumed by
    // the mode -- guessing is how a click lands twelve cells from where a maker pointed.
    const std::size_t held = t.pane().input.caret();
    t.publish(loom::to_value(input::PointerButton{1, true, 4, 4, input::space::kUnknown,
                                                   input::mod::kNone}));
    CHECK(t.pane().input.caret() == held);
}


// ---- HD-4: the input line's horizontal viewport ------------------------------------------
//
// Everything below drives the same three answers the application does: `component::TextBox`'s own
// window, `terminal_input_place`'s capacity, and the pair of pure functions that turn a byte
// index into a column and back. There is no second scroll model anywhere in these cases --
// where a case needs a column it asks the function the painter asks.

/// A REAL COMMAND LONGER THAN THE ROW, at every extent these cases use. Eighty-eight bytes:
/// long enough to scroll in the pane's widest configuration here and in its narrowest, which
/// is the difference between a case that watches the scrolling and one that passes because
/// the string happened to fit (§25).
static const std::string kLongLine =
    "send @zengine.skin SurfaceText 1 slot=1 text=the quick brown fox jumps over the lazy dog";

/// The cell a terminal medium would report for a prose position of the pane's own region.
/// The inverse of `terminal_input_place`'s resolution, exactly as `pane_pixel_x` is.
static std::int64_t pane_cell_x(const TerminalInputPlace& p, std::int64_t column) {
    return p.region_x + column;
}
static std::int64_t pane_cell_y(const TerminalInputPlace& p, std::int64_t row) {
    return p.region_y + row + surface::kTuiCanvasTopRow;
}

TEST_CASE("HD-4: a long line is shown as a slice, and both media draw the caret against it") {
    // §5, §7 and §11. The authored command is untouched behind the window; the row carries
    // the part that fits; the published caret column is relative to THAT, and the cell
    // projection's `_` lands in the same place a window's bar would.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal(); // no SurfaceExtent: a character IS a cell, 45 columns of line
    for (const char c : kLongLine) {
        t.text(std::string(1, c));
    }
    REQUIRE(kLongLine.size() == 88);

    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    // FORTY-FIVE, and it was fifty-three until HD-10 stopped the pane reaching into the
    // reserved side column: 48 cells of pane, less the prompt's two and the caret's one.
    REQUIRE(p.columns == 45);
    REQUIRE(t.pane().input.text() == kLongLine); // the whole command is still here
    REQUIRE(t.pane().input.at_end());
    REQUIRE(t.pane().input.first_visible() == 43); // 88 - 45, the tail

    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.rows.back().text == "> t=the quick brown fox jumps over the lazy dog");
        CHECK(pane.caret_col == kTerminalPromptCols + 45);
        CHECK(pane.caret_row == p.prose_row);
        // THE ROW IS SHORTER THAN THE REGION BY EXACTLY THE CARET'S OWN COLUMN, which is what
        // `kTerminalCaretCols` buys: in a cell medium the mark is a character and the last
        // cell has to be free for it.
        CHECK(static_cast<std::int64_t>(pane.rows.back().text.size()) == sc.terminal_cols - 1);

        surface::SurfaceCanvas only_pane = t.canvases.back();
        only_pane.texts = {pane}; // the pane alone: the list and the property body are others
        const std::vector<surface::ProjectedRow> shown =
            surface::project_text_regions(only_pane);
        CHECK(shown[static_cast<std::size_t>(p.prose_row)].label.text ==
              "> t=the quick brown fox jumps over the lazy dog_");
    }

    // HOME BRINGS THE BEGINNING BACK, and the caret with it.
    t.key(input::scan::kHome);
    CHECK(t.pane().input.first_visible() == 0);
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.rows.back().text == "> send @zengine.skin SurfaceText 1 slot=1 text=");
        CHECK(pane.caret_col == kTerminalPromptCols);
        surface::SurfaceCanvas only_pane = t.canvases.back();
        only_pane.texts = {pane}; // the pane alone: the list and the property body are others
        const std::vector<surface::ProjectedRow> shown =
            surface::project_text_regions(only_pane);
        CHECK(shown[static_cast<std::size_t>(p.prose_row)].label.text ==
              "> _send @zengine.skin SurfaceText 1 slot=1 text=");
    }

    // END TAKES IT BACK TO THE TAIL, and the authored line never changed once.
    t.key(input::scan::kEnd);
    CHECK(t.pane().input.first_visible() == 43);
    CHECK(t.pane().input.text() == kLongLine);

    // A CARET IN THE MIDDLE OF THE HIDDEN LEFT drags the window to meet it, minimally: the
    // window's start becomes the caret and not one byte further.
    // FORTY-FIVE OF THEM ARE FREE -- the window is 45 columns wide and the caret starts at
    // its right edge, so it walks the whole width before the window has to move at all.
    for (int i = 0; i < 45; ++i) {
        t.key(input::scan::kLeft);
    }
    CHECK(t.pane().input.caret() == 43);
    CHECK(t.pane().input.first_visible() == 43);
    for (int i = 0; i < 7; ++i) {
        t.key(input::scan::kLeft);
    }
    CHECK(t.pane().input.caret() == 36);
    CHECK(t.pane().input.first_visible() == 36); // one character per keystroke, from here
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col == kTerminalPromptCols); // ...at the left edge of the row
        CHECK(pane.rows.back().text == "> t=1 text=the quick brown fox jumps over the l");
    }

    // THE GRAPHICAL MEDIUM IS THE SAME ANSWER IN ITS OWN NUMBERS. A wider row means a
    // different window, not a different rule.
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.key(input::scan::kEnd);
    const TerminalInputPlace g = terminal_input_place(screen_of(t.session()));
    REQUIRE(g.fit.graphical());
    REQUIRE(g.columns == 68);
    CHECK(t.pane().input.first_visible() == 20); // 88 - 68
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.rows.back().text ==
              "> urfaceText 1 slot=1 text=the quick brown fox jumps over the lazy dog");
        CHECK(pane.caret_col == kTerminalPromptCols + 68);
        // ...AND THE BAR IS ACTUALLY DRAWN, which is the half of this the cell projection
        // cannot answer: `plan_caret` refuses a column the region has no room for, and that
        // refusal is exactly what the defect looked like before HD-4.
        surface::SurfaceCanvas only_pane = t.canvases.back();
        only_pane.texts = {pane}; // the pane alone: the list and the property body are others
        const std::vector<surface::PlanTextRegion> plan = surface::plan_text_regions(
            only_pane, surface::SurfaceExtent{78, 22, 8, 18}, surface::PlanSize{4000, 4000});
        REQUIRE(plan.size() == 1);
        CHECK(plan[0].caret.present);
    }
}

TEST_CASE("HD-4: a press on a SCROLLED line lands in the full authored string") {
    // §8, and the canary target. A visible column names `first_visible + offset` of the whole
    // command; a hit test that forgot the offset is right for exactly as long as no line is
    // long enough to scroll, which is to say wrong only when nobody is looking.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : kLongLine) {
        t.text(std::string(1, c));
    }
    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);

    // THE PREMISE, ASSERTED BEFORE ANYTHING ELSE. Without this the case would pass against a
    // hit test that ignores the window, for the most boring reason there is.
    REQUIRE(t.pane().input.first_visible() == 43);
    const std::size_t from = t.pane().input.first_visible();

    // EVERY VISIBLE COLUMN, and each one resolves to the byte of the WHOLE line that is
    // actually drawn there.
    for (std::int64_t k = 0; k <= p.columns; ++k) {
        t.press_at(pane_cell_x(p, p.first_column + k), pane_cell_y(p, p.prose_row),
                   input::space::kCells);
        CAPTURE(k);
        CHECK(t.pane().input.caret() == from + static_cast<std::size_t>(k));
        // ...and the window did not move, because every one of these is already inside it.
        CHECK(t.pane().input.first_visible() == from);
    }

    // THE FIRST VISIBLE CHARACTER, BY NAME. Column 2 of the row is byte 43, which is the `t`
    // of `text=` -- not byte 0, which is what a viewport-blind hit test would answer.
    t.press_at(pane_cell_x(p, p.first_column), pane_cell_y(p, p.prose_row),
               input::space::kCells);
    REQUIRE(t.pane().input.caret() == 43);
    CHECK(kLongLine[43] == 't');

    // A PRESS IN THE PROMPT, OR LEFT OF IT, MEANS THE START OF WHAT THE MAKER CAN SEE, which
    // on a scrolled line is not the start of the line. Clamping to 0 there would jump the
    // window a screenful away from where the press landed.
    t.key(input::scan::kEnd);
    REQUIRE(t.pane().input.first_visible() == 43);
    t.press_at(pane_cell_x(p, 0), pane_cell_y(p, p.prose_row), input::space::kCells);
    CHECK(t.pane().input.caret() == 43);
    CHECK(t.pane().input.first_visible() == 43);

    // A PRESS PAST THE LAST VISIBLE CHARACTER MEANS THE END OF THE LINE. On a scrolled line
    // the last visible character IS the last character, because the window never sits
    // further right than the last full screenful -- so there is no empty room that could
    // falsely imply the command ended early (§18).
    t.press_at(pane_cell_x(p, p.fit.columns), pane_cell_y(p, p.prose_row),
               input::space::kCells);
    CHECK(t.pane().input.caret() == kLongLine.size());

    // AND THE PRESS REPAIRS THE LINE WHERE THE MAKER AIMED. `text=the quick` -> `text=the
    // slick`: click the `q`, delete it, type an `s`.
    const std::size_t q = kLongLine.find("quick");
    REQUIRE(q > from); // the target is inside the visible window
    t.press_at(pane_cell_x(p, p.first_column + static_cast<std::int64_t>(q - from)),
               pane_cell_y(p, p.prose_row), input::space::kCells);
    REQUIRE(t.pane().input.caret() == q);
    t.key(input::scan::kDelete);
    t.text("s");
    CHECK(t.pane().input.text() ==
          "send @zengine.skin SurfaceText 1 slot=1 text=the suick brown fox jumps over the lazy dog");
}

TEST_CASE("HD-4: the window follows the caret across a resize") {
    // §19. Narrower, narrower again, then wider -- and at each one the caret is still on the
    // row and a press still reaches the byte a maker aimed at. There is no resize-specific
    // path: the same reconcile runs on the repaint the new extent causes.
    Live t;
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.toggle_terminal();
    for (const char c : kLongLine) {
        t.text(std::string(1, c));
    }

    struct Step {
        surface::SurfaceExtent extent;
        std::int64_t columns;
        std::size_t first;
    };
    // 68 columns, then 49, then 107 -- the last of which is wider than the whole command, so
    // the window has to come all the way back to the beginning. (The first two were 80 and 57
    // before HD-10 narrowed the pane's placement by eight cells at the minimum extent; the
    // third is unmoved, because at 115 columns the room is wider than the pane's want.)
    for (const Step s : {Step{{78, 22, 8, 18}, 68, 20}, Step{{78, 22, 11, 23}, 49, 39},
                         Step{{115, 63, 8, 18}, 107, 0}}) {
        t.publish(loom::to_value(s.extent));
        const Screen sc = screen_of(t.session());
        const TerminalInputPlace p = terminal_input_place(sc);
        CAPTURE(s.extent.text_advance_px);
        CAPTURE(sc.terminal_cols);
        REQUIRE(p.columns == s.columns);

        // THE WINDOW MOVED ONLY AS FAR AS IT HAD TO, and the caret is on the row.
        CHECK(t.pane().input.at_end());
        CHECK(t.pane().input.first_visible() == s.first);
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.caret_col ==
              terminal_caret_column(p, t.pane().input));
        CHECK(pane.caret_col <= p.fit.columns);
        CHECK(pane.rows.back().text ==
              "> " + t.pane().input.visible(p.columns));

        // WIDENING REVEALS MORE TEXT: the widest of the three shows the whole command.
        if (s.first == 0) {
            CHECK(pane.rows.back().text == "> " + kLongLine);
        }

        // AND A PRESS AFTER THE RESIZE STILL REACHES THE INTENDED POSITION OF THE FULL LINE.
        for (const std::int64_t k : {std::int64_t{0}, std::int64_t{9}, s.columns}) {
            t.press_at(pane_pixel_x(p, p.first_column + k), pane_pixel_y(p, p.prose_row),
                       input::space::kPixels);
            CAPTURE(k);
            CHECK(t.pane().input.caret() ==
                  (s.first + static_cast<std::size_t>(k) < kLongLine.size()
                       ? s.first + static_cast<std::size_t>(k)
                       : kLongLine.size()));
        }
        t.key(input::scan::kEnd);
    }
}

TEST_CASE("HD-4: completion sees the whole line, and acceptance brings the tail into view") {
    // §9 and §10. The completer is asked about the AUTHORED input and never about the slice
    // on the row -- a completer fed the visible substring would offer candidates for a
    // prefix that only looks like the end of the line.
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.toggle_terminal(); // 45 columns
    // FIFTY-EIGHT BYTES, ending in a token the completer answers: the length is spent in a
    // field VALUE so the line scrolls while the thing being completed is still a field name.
    const std::string prefix =
        "send * SurfaceText 1 text=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa s";
    for (const char c : prefix) {
        t.text(std::string(1, c));
    }
    REQUIRE(prefix.size() == 58);
    REQUIRE(t.pane().input.first_visible() == 13); // 58 - 45: the line has scrolled
    REQUIRE(t.pane().input.at_end());

    // THE PARTIAL IS A TOKEN OF THE WHOLE LINE, not of the row: `send *` has already
    // scrolled off the left, and the completer still knows this is a field of `SurfaceText`
    // because it was handed `input.text()`.
    CHECK(t.pane().completion.partial == "s");
    REQUIRE_FALSE(t.pane().completion.candidates.empty());

    // ACCEPTING PRODUCES A LONGER LINE, and the window follows to its tail.
    const std::string chosen = t.pane().completion.candidates[t.pane().completion.selected].insert;
    t.key(input::scan::kTab);
    const std::string after = t.pane().input.text();
    CHECK(after.size() > prefix.size());
    CHECK(after.rfind(chosen) == after.size() - chosen.size());
    CHECK(t.pane().input.at_end());

    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    CHECK(t.pane().input.first_visible() == after.size() - static_cast<std::size_t>(p.columns));
    const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
    CHECK(pane.rows.back().text == "> " + after.substr(t.pane().input.first_visible()));
    CHECK(pane.caret_col == kTerminalPromptCols + p.columns);

    // ...AND NOTHING WAS AUTHORED BY ANY OF IT.
    CHECK(me->transcript().entries().empty());
    CHECK(me->pending().empty());
    CHECK_FALSE(me->awaiting());
}

TEST_CASE("HD-4: a whole long-line edit authors nothing, and Return still submits all of it") {
    // §22. Typing, scrolling, clicking, repairing, Home, End and a resize are presentation
    // to the last one; the only effectful gesture in this pane is still Return, and what it
    // submits is the AUTHORED line rather than the part that was on the row.
    Live t;
    loom::TerminalSession* me = t.mount_terminal();
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.toggle_terminal();
    for (const char c : kLongLine) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.pane().input.first_visible() > 0);

    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    t.key(input::scan::kHome);
    t.key(input::scan::kEnd);
    for (int i = 0; i < 30; ++i) {
        t.key(input::scan::kLeft);
    }
    for (int i = 0; i < 10; ++i) {
        t.key(input::scan::kRight);
    }
    t.press_at(pane_pixel_x(p, p.first_column + 4), pane_pixel_y(p, p.prose_row),
               input::space::kPixels);
    t.publish(loom::to_value(surface::SurfaceExtent{115, 63, 8, 18}));
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.key(input::scan::kEnd);

    CHECK(t.pane().input.text() == kLongLine);
    CHECK(me->transcript().entries().empty());
    CHECK(me->pending().empty());
    CHECK_FALSE(me->awaiting());

    // RETURN, ONCE, AND THE WHOLE OF IT.
    t.key(input::scan::kReturn);
    const std::vector<loom::TranscriptEntry> record = me->transcript().entries();
    REQUIRE(record.size() >= 1);
    CHECK(record.front().text == kLongLine);
    CHECK(t.pane().input.empty());
    CHECK(t.pane().input.first_visible() == 0);
}

TEST_CASE("HD-4: a column and a byte index are inverses THROUGH the window") {
    // §7 and §8 as pure arithmetic, across four faces rather than the live one -- what is
    // being pinned is the mapping, and a case written against a single metric would pass for
    // a reason it does not claim. The eight positions §7 names are all here: start, middle,
    // end, both scrolled edges, an empty line, an exact-capacity line and one byte over.
    struct Face {
        std::int64_t advance;
        std::int64_t line;
    };
    for (const Face f : {Face{0, 0}, Face{6, 14}, Face{8, 18}, Face{11, 23}, Face{15, 31}}) {
        const Screen sc = screen_of(kScreenMinW, kScreenMinH, f.advance, f.line);
        const TerminalInputPlace p = terminal_input_place(sc);
        const std::size_t room = static_cast<std::size_t>(p.columns);
        CAPTURE(f.advance);

        // AN UNSCROLLED WINDOW IS HD-3's ANSWER, BYTE FOR BYTE. This is the compatibility
        // claim the whole change rests on: a short command is presented exactly as it was.
        CHECK(terminal_caret_column(p, box_of(0, 0, 0)) == kTerminalPromptCols);
        CHECK(terminal_caret_column(p, box_of(7, 7, 0)) == kTerminalPromptCols + 7);
        CHECK(terminal_caret_of_column(p, box_of(40, 40, 0), kTerminalPromptCols + 7) == 7);
        CHECK(terminal_caret_of_column(p, box_of(40, 40, 0), 0) == 0);

        // A SCROLLED WINDOW SUBTRACTS ITSELF FROM THE COLUMN AND ADDS ITSELF BACK.
        const std::size_t from = 25;
        const std::size_t length = from + room + 11; // eleven bytes hidden past the right
        CHECK(terminal_caret_column(p, box_of(from, from, from)) == kTerminalPromptCols);
        CHECK(terminal_caret_column(p, box_of(from + 3, from + 3, from)) == kTerminalPromptCols + 3);
        CHECK(terminal_caret_column(p, box_of(from + room, from + room, from)) == kTerminalPromptCols + p.columns);

        // ...AND THE TWO ARE INVERSES OVER EVERY POSITION THE WINDOW SHOWS, which is the
        // property a click-then-type depends on once the line is longer than the row.
        for (std::size_t at = from; at <= from + room; ++at) {
            CAPTURE(at);
            CHECK(terminal_caret_of_column(p, box_of(length, length, from),
                                           terminal_caret_column(p, box_of(at, at, from))) == at);
        }

        // THE BOUNDARIES, ON A SCROLLED LINE. Left of the prompt is the first byte the maker
        // can SEE -- not byte zero, which is a screenful away from where they pressed.
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), 0) == from);
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), 1) == from);
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), -400) == from);
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), kTerminalPromptCols - 1) == from);

        // ...and past the last byte is the end of the WHOLE line, never the end of the slice.
        CHECK(terminal_caret_of_column(p, box_of(length, length, from),
                                       kTerminalPromptCols + p.columns + 400) == length);

        // AN EMPTY LINE, AN EXACT FIT AND ONE BYTE OVER -- the three lengths §7 asks for.
        CHECK(terminal_caret_of_column(p, box_of(0, 0, 0), kTerminalPromptCols) == 0);
        CHECK(terminal_caret_column(p, box_of(room, room, 0)) == kTerminalPromptCols + p.columns);
        CHECK(terminal_caret_column(p, box_of(room + 1, room + 1, 1)) == kTerminalPromptCols + p.columns);

        // TOTAL AT BOTH ENDS OF THE NUMBER LINE, because the column came off the bus and the
        // window is a byte index the presentation chose.
        constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
        constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), kMin) == from);
        CHECK(terminal_caret_of_column(p, box_of(length, length, from), kMax) == length);
        // A WINDOW PAST THE END OF THE LINE cannot name a position the line does not have.
        CHECK(terminal_caret_of_column(p, box_of(5, 5, 900), kMax) == 5);
        CHECK(terminal_caret_of_column(p, box_of(5, 5, 900), kTerminalPromptCols) == 5);
    }
}

TEST_CASE("HD-4: clicking a SCROLLED multibyte line snaps exactly as HD-3's did") {
    // §6 and §24's character-safety row, through the weave: the window and the caret obey
    // one rule about what a character is, and a press between the two bytes of an `é` means
    // the `é` it landed on -- scrolled or not.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal(); // 45 columns
    for (int i = 0; i < 30; ++i) {
        t.text("\xC3\xA9"); // a whole character per TextEntered, as a backend reports it
    }
    REQUIRE(t.pane().input.size() == 60);

    const Screen sc = screen_of(t.session());
    const TerminalInputPlace p = terminal_input_place(sc);
    // 60 - 45 = 15, which is the second byte of an `é` -- so the window snaps FORWARD to 16
    // and shows one character fewer rather than half of one.
    REQUIRE(t.pane().input.first_visible() == 16);
    CHECK(t.pane().input.visible(p.columns).size() == 44);
    CHECK_FALSE(component::is_continuation_byte(t.pane().input.visible(p.columns)[0]));

    // EVERY VISIBLE COLUMN, and the caret only ever lands between characters -- at an even
    // byte, because every character here is two bytes and the line starts on one.
    for (std::int64_t k = 0; k <= p.columns; ++k) {
        t.press_at(pane_cell_x(p, p.first_column + k), pane_cell_y(p, p.prose_row),
                   input::space::kCells);
        CAPTURE(k);
        CHECK(t.pane().input.caret() % 2 == 0);
        // ...and it is the character the press landed ON: an odd column is the second byte
        // of the character at the even column before it.
        CHECK(t.pane().input.caret() ==
              16 + static_cast<std::size_t>(k) - static_cast<std::size_t>(k % 2));
    }

    // AND THE HIDDEN CHARACTERS ARE STILL THERE, whole, in the authored line.
    CHECK(t.pane().input.size() == 60);
    for (std::size_t i = 0; i < 60; i += 2) {
        CHECK(t.pane().input.text()[i] == '\xC3');
        CHECK(t.pane().input.text()[i + 1] == '\xA9');
    }
}


// ============================================================================================
// TIER: THE SECOND CONSUMER — a property draft is a TextBox (HD-5)
// ============================================================================================
//
// Everything below is about the Inspector's editing row, and every one of these behaviours
// arrived because the draft became a `component::TextBox`. Before HD-5 the row could be
// appended to and backspaced from and nothing else: no caret, no window, no pointer, and a
// value longer than the row silently lost its tail at the canvas edge with no mark at all.
//
// WHAT IS NOT ASSERTED HERE is what a TextBox DOES -- that is the component suite's claim,
// which is why four cases moved out of this file. What these prove is that the property
// editor's answers COME from there, and that the property layer's own semantics -- parse,
// validate, refuse, commit, cancel -- did not follow the draft into the component.

TEST_CASE("HD-5: a property draft opens on its value with the caret at the end") {
    Live t;
    t.begin_editing("Name");
    const Row* row = t.row("Name");
    REQUIRE(row != nullptr);
    REQUIRE(row->editing());

    // The draft IS the committed value, and the caret is where a maker about to amend it
    // would put their hand.
    CHECK(row->draft() == "panel");
    CHECK(row->editor().text() == "panel");
    CHECK(row->editor().caret() == 5);
    CHECK(row->editor().first_visible() == 0);
    CHECK(row->editor().at_end());

    // ...and the property has not moved, because a draft is not a write.
    CHECK(row->value() == "panel");
    CHECK(t.doc().elements[0].label == "panel");
}

TEST_CASE("HD-5: a property value is repaired in the MIDDLE, by keys the row did not have") {
    // The user-facing target, and the exact gesture the pristine tree could not make: on the
    // START tree `hellp world` cost seven backspaces and seven retyped characters, because
    // Left, Right, Home, End and Delete were every one of them `default: break`.
    Live t;
    t.begin_editing("Name");
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : std::string("hellp world")) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.row("Name")->draft() == "hellp world");

    // Six lefts, one delete, one keystroke. The rest of the value is untouched.
    for (int i = 0; i < 7; ++i) {
        t.key(input::scan::kLeft);
    }
    CHECK(t.row("Name")->editor().caret() == 4);
    t.key(input::scan::kDelete);
    CHECK(t.row("Name")->draft() == "hell world");
    t.text("o");
    CHECK(t.row("Name")->draft() == "hello world");
    CHECK(t.row("Name")->editor().caret() == 5);

    // HOME AND END REACH BOTH ENDS, and Backspace still takes the character before the caret
    // rather than the one at the end of the value.
    t.key(input::scan::kHome);
    CHECK(t.row("Name")->editor().caret() == 0);
    t.key(input::scan::kDelete);
    CHECK(t.row("Name")->draft() == "ello world");
    t.key(input::scan::kEnd);
    CHECK(t.row("Name")->editor().caret() == 10);
    t.key(input::scan::kBackspace);
    CHECK(t.row("Name")->draft() == "ello worl");

    // AND THE PROPERTY IS STILL UNTOUCHED through all of it: none of the six gestures is a
    // write, which is the line the component was never allowed to cross.
    CHECK(t.doc().elements[0].label == "panel");
}

TEST_CASE("HD-5: a long property draft is a window, and no part of it is lost") {
    Live t;
    t.begin_editing("Name");
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : kLongValue) {
        t.text(std::string(1, c));
    }

    const InfoBodyPlace place = body_place(t);
    REQUIRE(place.present);
    REQUIRE(place.value_columns > 0);
    REQUIRE(static_cast<std::int64_t>(kLongValue.size()) > place.value_columns);

    // THE AUTHORED VALUE IS WHOLE. What the row shows is a slice of it and nothing about the
    // draft was cut, rotated or marked.
    const Row* row = t.row("Name");
    CHECK(row->draft() == kLongValue);
    CHECK(row->editor().caret() == kLongValue.size());
    CHECK(row->editor().first_visible() > 0); // it scrolled, which is the point

    // THE ROW SHOWS THE TAIL, with the caret on it, and the body says where the caret is.
    const surface::SurfaceTextRegion* shown = body_region(t.canvases.back(), place);
    REQUIRE(shown != nullptr);
    const std::int64_t at_row = editing_prose_row(t, place);
    REQUIRE(at_row != kNoProseRow);
    const std::string& drawn = shown->rows[static_cast<std::size_t>(at_row)].text;
    const std::string slice = row->editor().visible(place.value_columns);
    CHECK(drawn == property_row_text(*row, true, place.value_columns));
    CHECK(static_cast<std::int64_t>(slice.size()) <= place.value_columns);
    CHECK(kLongValue.substr(kLongValue.size() - slice.size()) == slice);
    CHECK(shown->caret_row == at_row);
    CHECK(shown->caret_col == property_caret_column(*row));
    CHECK(shown->caret_col <= place.columns);

    // EVERY BYTE IS REACHABLE. Home, then one Right at a time, and the union of what the row
    // showed along the way is the whole value -- which is the difference between a bounded
    // presentation and a truncation.
    t.key(input::scan::kHome);
    std::string seen = t.row("Name")->editor().visible(place.value_columns);
    std::size_t reached = 0;
    for (std::size_t i = 0; i < kLongValue.size(); ++i) {
        t.key(input::scan::kRight);
        const Row* r = t.row("Name");
        const std::size_t first = r->editor().first_visible();
        const std::string step = r->editor().visible(place.value_columns);
        CAPTURE(i);
        CHECK(r->editor().caret_column() <= static_cast<std::size_t>(place.value_columns));
        if (first + step.size() > reached) {
            reached = first + step.size();
            seen += step.substr(seen.size() > first ? seen.size() - first : 0);
        }
    }
    CHECK(seen == kLongValue);
    CHECK(t.row("Name")->draft() == kLongValue);
}

TEST_CASE("HD-5: a press inside a SCROLLED property draft lands in the full authored value") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18})); // a window, so the press arrives in PIXELS
    t.begin_editing("Name");
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : kLongValue) {
        t.text(std::string(1, c));
    }

    const InfoBodyPlace place = body_place(t);
    REQUIRE(place.present);
    const std::int64_t at_row = editing_prose_row(t, place);
    REQUIRE(at_row != kNoProseRow);
    const std::size_t from = t.row("Name")->editor().first_visible();
    REQUIRE(from > 0); // the value really is scrolled: this is the case's whole point

    // A COLUMN OF WHAT THE MAKER CAN SEE NAMES A BYTE OF THE WHOLE DRAFT, never the offset
    // alone. Without the window's own offset every one of these would land `from` bytes early.
    for (std::int64_t col = 0; col <= place.value_columns; ++col) {
        CAPTURE(col);
        t.press_at(value_pixel_x(place, col), value_pixel_y(place, at_row),
                   input::space::kPixels);
        const std::size_t want =
            from + static_cast<std::size_t>(col) < kLongValue.size()
                ? from + static_cast<std::size_t>(col)
                : kLongValue.size();
        CHECK(t.row("Name")->editor().caret() == want);
        CHECK(t.row("Name")->draft() == kLongValue); // a press authors nothing
    }

    // ...AND A REPAIR AT THE CLICKED PLACE CHANGES ONLY THAT PLACE.
    t.press_at(value_pixel_x(place, 4), value_pixel_y(place, at_row), input::space::kPixels);
    const std::size_t at = t.row("Name")->editor().caret();
    t.key(input::scan::kDelete);
    std::string want = kLongValue;
    want.erase(at, 1);
    CHECK(t.row("Name")->draft() == want);

    // A CELL MEDIUM REACHES THE SAME MODEL, through the other branch of `prose_at`.
    Live c;
    c.begin_editing("Name");
    for (int i = 0; i < 5; ++i) {
        c.key(input::scan::kBackspace);
    }
    for (const char ch : kLongValue) {
        c.text(std::string(1, ch));
    }
    const InfoBodyPlace cells = body_place(c);
    REQUIRE_FALSE(cells.fit.graphical());
    const std::int64_t cell_row = editing_prose_row(c, cells);
    REQUIRE(cell_row != kNoProseRow);
    const std::size_t cfrom = c.row("Name")->editor().first_visible();
    REQUIRE(cfrom > 0);
    c.press_at(cells.region_x + kPropertyMarkCols + kPropertyLabelCols + 3,
               cells.region_y + cell_row + surface::kTuiCanvasTopRow, input::space::kCells);
    CHECK(c.row("Name")->editor().caret() == cfrom + 3);
}

TEST_CASE("HD-5: the property editor paints, carets, measures and hits from one geometry") {
    // §9. There is no `paint_property_bounds()` beside a `click_property_bounds()`, and this
    // is what that buys: every extent below moves the panel, the body, the caret and the
    // answer to a press together, because there is one function that decides all of them.
    // HD-6 widened the function from the editing ROW to the whole property BODY; the claim
    // and this case's shape are unchanged.
    for (const surface::SurfaceExtent& e :
         {surface::SurfaceExtent{78, 22, 0, 0}, surface::SurfaceExtent{78, 33, 8, 18},
          surface::SurfaceExtent{140, 40, 8, 18}, surface::SurfaceExtent{110, 30, 11, 23}}) {
        Live t;
        t.publish(loom::to_value(e));
            t.begin_editing("Name");
        for (int i = 0; i < 5; ++i) {
            t.key(input::scan::kBackspace);
        }
        for (const char c : kLongValue) {
            t.text(std::string(1, c));
        }
        CAPTURE(e.width);
        CAPTURE(e.text_advance_px);

        const Screen sc = screen_of(t.session());
        const ui::Rect panel = bounds_of(t.session().panels, panel::kInfo, sc).rect;
        const InfoBodyPlace place = body_place(t);
        REQUIRE(place.present);

        // THE BODY IS THE PANEL'S, from under the OBJECTS heading to its bottom edge (HD-7).
        CHECK(place.region_x == panel.x);
        CHECK(place.region_y == panel.y + kInfoBodyY);
        CHECK(place.region_x + place.region_w == panel.x + panel.w);
        CHECK(place.region_y + place.region_h == panel.y + panel.h);
        CHECK(place.columns == place.fit.columns);
        CHECK(place.value_columns ==
              place.fit.columns - kPropertyMarkCols - kPropertyLabelCols - kPropertyCaretCols);
        CHECK(place.capacity == static_cast<std::size_t>(place.fit.rows));

        // THE PAINTER CUT THE SLICE WITH IT...
        const surface::SurfaceTextRegion* shown = body_region(t.canvases.back(), place);
        REQUIRE(shown != nullptr);
        CHECK(shown->w == place.region_w);
        CHECK(shown->h == place.region_h);
        CHECK(shown->rows.size() <= place.capacity); // §11: no row the body cannot hold
        const std::int64_t at_row = editing_prose_row(t, place);
        REQUIRE(at_row != kNoProseRow);
        CHECK(shown->rows[static_cast<std::size_t>(at_row)].text ==
              property_row_text(*t.row("Name"), true, place.value_columns));
        // ...THE CARET IS ON IT, on both axes...
        CHECK(shown->caret_row == at_row);
        CHECK(shown->caret_col == property_caret_column(*t.row("Name")));
        CHECK(shown->caret_col <= place.fit.columns);
        // ...AND A PRESS AT THE CARET'S OWN COLUMN COMES BACK TO THE CARET.
        const std::size_t was = t.row("Name")->editor().caret();
        t.press_at(value_pixel_x(place, property_value_column(shown->caret_col)),
                   value_pixel_y(place, at_row), input::space::kPixels);
        CHECK(t.row("Name")->editor().caret() == was);
    }
}

TEST_CASE("HD-5: a resize reconciles the property window with no path of its own") {
    // §27. The reconcile runs once per repaint rather than on the edits, so a new extent --
    // which is not an edit -- moves the window anyway, and the caret is on the row at every
    // size. Nothing about the draft changes because the room did.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 0, 0}));
    t.begin_editing("Name");
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : kLongValue) {
        t.text(std::string(1, c));
    }
    const std::size_t narrow = t.row("Name")->editor().first_visible();
    REQUIRE(narrow > 0);

    // The side region is a FIXED width, so a wider surface gives this panel exactly as much
    // as it had -- which makes the honest resize witness a change of MEDIUM, where the row's
    // capacity genuinely moves.
    t.publish(loom::to_value(surface::SurfaceExtent{140, 40, 8, 18}));
    CHECK(t.row("Name")->draft() == kLongValue); // the value did not move
    CHECK(t.row("Name")->editor().caret() == kLongValue.size());
    const InfoBodyPlace wide = body_place(t);
    REQUIRE(wide.present);
    CHECK(t.row("Name")->editor().caret_column() <=
          static_cast<std::size_t>(wide.value_columns));
    const surface::SurfaceTextRegion* shown = body_region(t.canvases.back(), wide);
    REQUIRE(shown != nullptr);
    CHECK(shown->caret_col <= wide.fit.columns);
    CHECK(shown->caret_row == editing_prose_row(t, wide));

    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 0, 0}));
    CHECK(t.row("Name")->editor().first_visible() == narrow);
    CHECK(t.row("Name")->draft() == kLongValue);
}

TEST_CASE("HD-5: a surface extent does not take a maker's hands off a draft") {
    // A REPAIR, and the defect was reproduced on the pristine HD-4 tree first: one
    // SurfaceExtent -- a window dragged, which is not a gesture aimed at the inspector at all
    // -- rebuilt the inspector and the half-typed value was GONE, with no notice. Since HD-5
    // the same event would also throw away the caret and the window, so the loss got worse
    // before it got fixed.
    Live t;
    t.begin_editing("Height");
    for (const char c : std::string("zz")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kLeft);
    t.key(input::scan::kReturn); // an invalid draft, so the refusal is on screen too
    REQUIRE(t.row("Height")->editing());
    const std::size_t cursor = t.session().cursor;
    const std::string draft = t.row("Height")->draft();
    const std::string refusal = t.row("Height")->refusal();
    const std::size_t caret = t.row("Height")->editor().caret();
    REQUIRE_FALSE(refusal.empty());

    t.publish(loom::to_value(surface::SurfaceExtent{140, 40, 8, 18}));

    CHECK(t.row("Height")->editing());
    CHECK(t.row("Height")->draft() == draft);
    CHECK(t.row("Height")->refusal() == refusal);
    CHECK(t.row("Height")->editor().caret() == caret);
    CHECK(t.session().cursor == cursor);

    // ...AND THE ROWS REALLY WERE REBUILT: the resolved row closes over the extent, so it is
    // reporting the new one rather than a stale answer carried over with the draft.
    CHECK(t.row("Resolved")->value() ==
          std::to_string(resolved_w(t)) + " x " +
              std::to_string(ui::placed_for(workspace_scene(t.doc(), t.session()),
                                            t.session().selected)->rect.h) +
              " cells");

    // A CHANGE OF SELECTION IS THE OTHER CASE, and it must still drop the draft. ' + chr(96) + 'Name' + chr(96) + ' is a
    // row every object has, so a draft carried across a selection would arrive on a different
    // object's property wearing the same label.
    t.key(input::scan::kEscape);
    t.begin_editing("Height");
    t.text("7");
    REQUIRE(t.row("Height")->editing());
    t.key(input::scan::kEscape);
    t.key(input::scan::kTab); // the next object
    CHECK_FALSE(t.row("Height")->editing());
    CHECK(t.row("Height")->draft().empty());
}

TEST_CASE("HD-5: both media project the property caret, in their own type") {
    Live t;
    t.begin_editing("Name");
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : std::string("abcdefghij")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kLeft);
    t.key(input::scan::kLeft);
    t.key(input::scan::kLeft);

    const InfoBodyPlace place = body_place(t);
    REQUIRE(place.present);
    const std::int64_t at_row = editing_prose_row(t, place);
    REQUIRE(at_row != kNoProseRow);
    const surface::SurfaceCanvas& c = t.canvases.back();

    // THE CELL PROJECTION INSERTS THE MARK AT THE CARET'S OWN COLUMN, which is what a caret
    // looks like in a medium with no half-cells. The row carries the mark and the property's
    // NAME as well as the value (HD-6), so the caret's column is that offset plus the
    // component's own answer -- and the row is padded to the body's width, so it also erases
    // whatever the panel had underneath it.
    CHECK(inspector_row(c, place.region_x, place.region_y + at_row) ==
          ">Name     abcdefg_hij");
    CHECK(label_at(c, place.region_x, place.region_y + at_row).size() ==
          static_cast<std::size_t>(place.region_w));

    // AND THE SAME CANVAS THROUGH THE REAL TERMINAL RASTERIZER puts the mark on the same
    // cell -- the medium's own bytes, not a model of them.
    const std::string body = surface::canvas_body(c);
    CHECK(body.find("abcdefg_hij") != std::string::npos);
}

TEST_CASE("HD-5: a refused commit keeps the draft AND the place in it") {
    // §16 and §25. The component contains the draft; it does not know the draft is invalid,
    // and it certainly does not commit. What survives a refusal is the whole editor.
    Live t;
    t.begin_editing("Width");
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : std::string("500%")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kLeft); // the caret is INSIDE the draft when the commit is attempted
    CHECK(t.row("Width")->editor().caret() == 3);

    const ui::Extent before = t.doc().elements[0].width;
    t.key(input::scan::kReturn);

    CHECK(t.doc().elements[0].width == before);     // the property was not written
    CHECK(t.row("Width")->editing());               // the editor is still open
    CHECK(t.row("Width")->draft() == "500%");       // the draft survived
    CHECK(t.row("Width")->refusal() == "a share is 1% to 100%");
    CHECK(t.notice() == "Width: a share is 1% to 100%");
    CHECK(t.row("Width")->editor().caret() == 3);   // ...and so did the caret

    // AND IT IS STILL AN EDITOR: the maker fixes what they typed from where they were.
    t.key(input::scan::kBackspace); // one 0 of 500, from where the caret already was
    CHECK(t.row("Width")->draft() == "50%");
    t.key(input::scan::kReturn);
    CHECK(t.doc().elements[0].width == ui::Extent{ui::kExtentPercent, 50});
    CHECK_FALSE(t.row("Width")->editing());

    // AN UNPARSEABLE DRAFT IS THE OTHER FAILURE, and it keeps the same two things.
    t.begin_editing("Width");
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : std::string("banana")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kHome);
    t.key(input::scan::kReturn);
    CHECK(t.doc().elements[0].width == ui::Extent{ui::kExtentPercent, 50});
    CHECK(t.row("Width")->draft() == "banana");
    CHECK(t.row("Width")->editor().caret() == 0);
    CHECK(t.row("Width")->refusal() == "not cells (12) or a share (70%)");
}

TEST_CASE("HD-5: an accepted commit writes the property and closes the editor") {
    Live t;
    t.begin_editing("Name");
    t.key(input::scan::kHome);
    for (const char c : std::string("my ")) {
        t.text(std::string(1, c));
    }
    CHECK(t.row("Name")->draft() == "my panel"); // typed at the caret, not at the end
    CHECK(t.doc().elements[0].label == "panel"); // and still not written

    t.key(input::scan::kReturn);
    CHECK(t.doc().elements[0].label == "my panel");
    CHECK_FALSE(t.row("Name")->editing());
    CHECK(t.row("Name")->draft().empty());
    CHECK(t.row("Name")->editor().caret() == 0);         // the editor was reset with the row
    CHECK(t.row("Name")->editor().first_visible() == 0);
    CHECK(t.row("Name")->refusal().empty());
    CHECK(t.notice() == "committed Name = my panel");

    // ...and nothing is painted for a row nobody is editing: the body is still there (it is
    // the whole property list) and it carries NO caret.
    const InfoBodyPlace place = body_place(t);
    const surface::SurfaceTextRegion* shown = body_region(t.canvases.back(), place);
    REQUIRE(shown != nullptr);
    CHECK(shown->caret_row == surface::kNoCaret);
    CHECK(inspector_row(t.canvases.back(), place.region_x,
                        place.region_y + prose_row_of_property(place, 1)) ==
          ">Name     my panel");
}

TEST_CASE("HD-5: cancel abandons the draft, the caret and the window together") {
    Live t;
    t.begin_editing("Name");
    for (const char c : kLongValue) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.row("Name")->editor().first_visible() > 0);

    t.key(input::scan::kEscape);
    CHECK_FALSE(t.row("Name")->editing());
    CHECK(t.row("Name")->draft().empty());
    CHECK(t.row("Name")->editor().first_visible() == 0);
    CHECK(t.row("Name")->editor().caret() == 0);
    CHECK(t.doc().elements[0].label == "panel"); // the property was never touched
    CHECK(t.notice() == "edit cancelled -- nothing was written");
}

TEST_CASE("HD-5: two TextBoxes exist and exactly one of them ever hears a keystroke") {
    // §12 and §33. Multiple TextBox instances now live in this application's object graph and
    // NO focus framework was added, because the modes Workshop already had answer the
    // question unambiguously: the overlay while it is open, then the picker, then the editing
    // row, then command mode. Four `if`s, no focused panel, no z-order, no capture.
    Live t;
    (void)t.mount_terminal();
    t.begin_editing("Name");
    for (const char c : std::string("abc")) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.row("Name")->draft() == "panelabc");

    // THE OVERLAY TAKES THE KEYS THE MOMENT IT OPENS, and the draft underneath is not
    // cancelled, not committed and not touched.
    t.toggle_terminal();
    for (const char c : std::string("send")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kLeft);
    t.key(input::scan::kBackspace);
    CHECK(t.pane().input.text() == "sed");
    CHECK(t.row("Name")->draft() == "panelabc"); // untouched, caret included
    CHECK(t.row("Name")->editor().caret() == 8);
    CHECK(t.row("Name")->editing());

    // A PRESS WHILE THE PANE IS OPEN IS THE PANE'S, wherever it lands: the property editor is
    // a PLACE and the overlay is a MODE, and a mode takes the pointer entirely.
    const InfoBodyPlace place = body_place(t);
    REQUIRE(place.present);
    t.press_at(value_pixel_x(place, 2), value_pixel_y(place, editing_prose_row(t, place)),
               input::space::kPixels);
    CHECK(t.row("Name")->editor().caret() == 8);

    // ...AND CLOSING IT GIVES THEM BACK, exactly.
    t.toggle_terminal();
    t.text("Z");
    CHECK(t.row("Name")->draft() == "panelabcZ");
    CHECK(t.pane().input.text() == "sed");
}

TEST_CASE("HD-5: a press on the panel that is not the draft is still the panel's") {
    // §34. PNL-2's rule is unchanged: a press inside a visible panel's bounds cannot take
    // hold of anything underneath it, and it says so. What HD-5 added is one PLACE inside
    // that rectangle which answers first -- and only while a draft is open on it.
    Live t;
    t.begin_editing("Name");
    const InfoBodyPlace place = body_place(t);
    REQUIRE(place.present);
    const std::int64_t at_row = editing_prose_row(t, place);
    REQUIRE(at_row != kNoProseRow);
    const std::int64_t value_x = place.region_x + kPropertyMarkCols + kPropertyLabelCols;

    // On the panel, but not on the ROW being edited: the panel's own answer, in words. Since
    // HD-6 the body is one region spanning every property, so "not the draft" is a different
    // ROW of it rather than a different rectangle -- which is exactly the distinction
    // `property_at_prose_row` draws.
    t.press_at(value_x + 2, place.region_y + at_row + 3 + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
    CHECK(t.row("Name")->editor().caret() == 5); // and the caret did not move

    // On the value: the caret moves and the notice is NOT overwritten, because the caret is
    // the statement and a sentence repeating it would push a refusal off the line.
    t.press_at(value_x + 2, place.region_y + at_row + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.row("Name")->editor().caret() == 2);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");

    // And a row that is NOT being edited is not an editor: a press on its value is the
    // panel's, which is what keeps "Return opens a draft" the only way one opens.
    t.key(input::scan::kEscape);
    t.press_at(value_x + 2, place.region_y + at_row + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.row("Name")->editing());
}

// ============================================================================
// HD-6 — the Inspector's property body: real type, real bounds, a real window
// ============================================================================
//
// HD-5 gave the editing row a component and measured the wall it stood against: a region ONE
// CELL tall holds no line of this repository's face, so the property editor was honest and
// lower-fidelity than the pane beside it, and `paint_info` had no bottom bound at all. HD-6
// takes the room ONCE, for the whole body, and every case below is about what that one
// resolution now answers: how many rows fit, how wide a value is, which rows are shown, what
// is said about the ones that are not, and where a press lands.

namespace {

/// A document whose selected object has the eight properties every object has. The HD-6
/// population witness is the SCREEN, not a manufactured row list: at the minimum extent a
/// graphical body holds five rows and there are eight properties, so the overflow is what an
/// ordinary object looks like in an ordinary window.
WorkshopDoc one_object() {
    WorkshopDoc d;
    d.elements.push_back(ui::Element{1, "panel", ui::kRootContext, 5, 2,
                                     ui::Extent{ui::kExtentPercent, 60},
                                     ui::Extent{ui::kExtentCells, 6}});
    d.next_id = 2;
    return d;
}

/// THE SAME OBJECT, IN A DOCUMENT OF SIX (HD-7).
///
/// HD-6's property-overflow cases were written when the OBJECTS list took a fixed five rows
/// and the property body took everything under it, so ONE object was enough to make the
/// graphical minimum screen overflow at five property rows. Both lists share one budget now,
/// so how much the property list gets depends on how much the object list asked for -- with
/// one object it asks for one row and all eight properties fit. Six objects is what asks for
/// enough to put the property list back at five rows on the graphical minimum screen, which is
/// where these cases' measurements were taken. The selected object is still #1 and its eight
/// properties are still the eight properties every object has: NOTHING was manufactured, and
/// the document is smaller than the two-object one a maker boots into plus four `n` presses.
WorkshopDoc six_objects() {
    WorkshopDoc d = one_object();
    for (int i = 0; i < 5; ++i) {
        REQUIRE(doc::add_default(d) != 0);
    }
    return d;
}

} // namespace

TEST_CASE("HD-6: the body's row capacity is the ACTIVE medium's, from one equation") {
    // §6 and §40 (row fitting). There is no Inspector row height, no font measurement here
    // and no 22-pixel constant: `fit_region` answered both numbers from the metric the medium
    // published, with `kTextInsetPx` already inside it.
    WorkshopDoc d = one_object();
    Session s;
    s.selected = 1;
    s.screen_w = 80;
    s.screen_h = 38;
    refocus(d, s);

    // A CHARACTER MEDIUM: a prose row IS a cell row, so the body holds its own cell height.
    const InfoBodyPlace cells = body_of(d, s);
    REQUIRE(cells.present);
    CHECK_FALSE(cells.fit.graphical());
    CHECK(cells.capacity == static_cast<std::size_t>(cells.region_h));
    CHECK(cells.columns == cells.region_w);
    CHECK(cells.value_columns ==
          cells.region_w - kPropertyMarkCols - kPropertyLabelCols - kPropertyCaretCols);

    // A GRAPHICAL MEDIUM, same bounds: taller rows, narrower characters, and BOTH numbers
    // move because both came from the same fit.
    s.text_advance_px = 8;
    s.text_line_px = 18;
    const InfoBodyPlace type = body_of(d, s);
    REQUIRE(type.present);
    CHECK(type.fit.graphical());
    CHECK(type.region_h == cells.region_h); // the same rectangle...
    CHECK(type.region_w == cells.region_w);
    CHECK(type.capacity < cells.capacity);  // ...fewer rows of a taller face...
    CHECK(type.columns > cells.columns);    // ...and more characters across it
    CHECK(type.capacity ==
          static_cast<std::size_t>((type.region_h * surface::kCanvasCellPx -
                                    2 * surface::kTextInsetPx) /
                                   s.text_line_px));
    CHECK(type.value_columns > cells.value_columns);

    // AND THE FACE'S OWN LINE IS WHAT DECIDES: a taller line is fewer rows, at the same
    // bounds, with nothing else moving. Half the rows, to within the one row an integer
    // division can leave behind -- the equation is the assertion above and this is its shape.
    s.text_line_px = 36;
    const InfoBodyPlace taller = body_of(d, s);
    CHECK(taller.capacity ==
          static_cast<std::size_t>((taller.region_h * surface::kCanvasCellPx -
                                    2 * surface::kTextInsetPx) /
                                   s.text_line_px));
    CHECK(taller.capacity * 2 <= type.capacity);
    CHECK(taller.capacity * 2 + 1 >= type.capacity);
}

TEST_CASE("HD-6: one body, two media, different row counts and the same property facts") {
    // §21 and §5. The graphical Inspector may show fewer rows than the terminal one -- taller
    // type is taller -- and that is honest as long as both say what they are not showing and
    // both let a maker reach it. What must NOT differ is the semantic row: same order, same
    // labels, same values.
    WorkshopDoc d = six_objects();
    Session tui;
    tui.selected = 1;
    tui.screen_w = kScreenMinW;
    tui.screen_h = kScreenMinH;
    refocus(d, tui);
    Session sdl = tui;
    sdl.text_advance_px = 8;
    sdl.text_line_px = 18;

    const InfoBodyPlace ct = body_of(d, tui);
    const InfoBodyPlace cs = body_of(d, sdl);
    REQUIRE(ct.capacity == 16); // sixteen cell rows of body at the minimum screen
    REQUIRE(cs.capacity == 10); // (16 * 12 - 4) / 18
    // HD-8's footer comes off both budgets, so both shares are two rows smaller than HD-6
    // measured them and the CELL body no longer seats all eight properties either. What the
    // case is about is unmoved: two media, two capacities, one set of semantic rows.
    REQUIRE(ct.properties_rows == 7);
    REQUIRE(cs.properties_rows == 4);
    REQUIRE(tui.rows.size() == 8);

    // THE TERMINAL SHOWS SIX AND SAYS SO. Before HD-8 it showed all eight and said nothing;
    // the two rows it lost are the two the maker can now press.
    CHECK(ct.properties.count == 6);
    CHECK(ct.properties.before == 0);
    CHECK(ct.properties.after == 2);

    // THE WINDOW SHOWS THREE AND SAYS SO -- the marker spends one of the four rows, which is
    // `list_window`'s rule and not a second one.
    CHECK(cs.properties.count == 3);
    CHECK(cs.properties.before == 0);
    CHECK(cs.properties.after == 5);

    const surface::SurfaceCanvas tui_canvas = paint(d, tui);
    const surface::SurfaceCanvas sdl_canvas = paint(d, sdl);
    const surface::SurfaceTextRegion* tui_body = body_on(tui_canvas, ct);
    const surface::SurfaceTextRegion* sdl_body = body_on(sdl_canvas, cs);
    REQUIRE(tui_body != nullptr);
    REQUIRE(sdl_body != nullptr);
    // The region carries BOTH lists since HD-7 and the FOOTER since HD-8, and the footer is
    // anchored to the foot -- so the body now publishes exactly its capacity in every medium,
    // with the spare room written as the blank rows it is.
    CHECK(tui_body->rows.size() == ct.capacity);
    CHECK(sdl_body->rows.size() == cs.capacity);
    CHECK(tui_body->rows[static_cast<std::size_t>(ct.action_row)].text == "[ Create ]");
    CHECK(sdl_body->rows.back().text == "[ Delete ]");
    // ...and the omission marker is still the last thing the property list says, one row
    // under the properties rather than at the end of the region.
    CHECK(sdl_body->rows[static_cast<std::size_t>(cs.heading_row) + cs.properties_rows]
              .text == "... 5 more");

    // SAME PROPERTY FACTS, in the same order, for the rows both are showing. The value
    // columns differ because the media differ; the property does not.
    for (std::size_t i = 0; i < cs.properties.count; ++i) {
        CAPTURE(i);
        const auto& tr = tui_body->rows[static_cast<std::size_t>(prose_row_of_property(ct, i))];
        const auto& sr = sdl_body->rows[static_cast<std::size_t>(prose_row_of_property(cs, i))];
        CHECK(tr.text == property_row_text(tui.rows[i], i == tui.cursor, ct.value_columns));
        CHECK(sr.text == property_row_text(sdl.rows[i], i == sdl.cursor, cs.value_columns));
        CHECK(tr.text.substr(0, 10) == sr.text.substr(0, 10));
    }

    // AND NO ROW IS PUBLISHED THAT THE BODY CANNOT HOLD, in either medium (§11).
    CHECK(tui_body->rows.size() <= ct.capacity);
    CHECK(sdl_body->rows.size() <= cs.capacity);
}

TEST_CASE("HD-6: no property row is painted below the panel it belongs to") {
    // THE DEFECT, REPRODUCED ON THE PRISTINE TREE FIRST: `paint_info` looped over every row
    // and wrote a label per row, so a property population taller than the panel ran off its
    // bottom edge -- measured at the minimum screen with twelve rows: three labels below the
    // panel and one of them on the notice band, with no marker anywhere.
    WorkshopDoc d = six_objects();
    Session s;
    s.selected = 1;
    s.screen_w = kScreenMinW;
    s.screen_h = kScreenMinH;
    s.text_advance_px = 8;
    s.text_line_px = 18;
    refocus(d, s);

    const Screen sc = screen_of(s);
    const ui::Rect panel = bounds_of(s.panels, panel::kInfo, sc).rect;
    const InfoBodyPlace body = body_of(d, s);
    REQUIRE(body.properties.after > 0); // the population really does overflow

    const surface::SurfaceCanvas c = paint(d, s);
    // NOTHING IN THE PANEL'S COLUMN IS BELOW THE PANEL, labels and region rows alike.
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x < panel.x) {
            continue; // the workspace and the bottom band are not this panel's
        }
        CAPTURE(l.text);
        CHECK(l.y < panel.y + panel.h);
        CHECK(l.y < sc.h);
    }
    // ...and the body's own rectangle ends where the panel does.
    const surface::SurfaceTextRegion* shown = body_on(c, body);
    REQUIRE(shown != nullptr);
    CHECK(shown->y + shown->h == panel.y + panel.h);
}

TEST_CASE("HD-6: what the body cannot show, it counts -- on the side it left it out") {
    // §15. The wording and the arithmetic are the OBJECTS list's own -- `list_window` and
    // `omitted_text` -- so a maker reads the same sentence in both halves of the panel and a
    // change to either moves both.
    WorkshopDoc d = six_objects();
    Session s;
    s.selected = 1;
    s.screen_w = kScreenMinW;
    s.screen_h = kScreenMinH;
    s.text_advance_px = 8;
    s.text_line_px = 18;
    refocus(d, s);
    REQUIRE(s.rows.size() == 8);
    REQUIRE(body_of(d, s).properties_rows == 4); // HD-8: two rows of it are the footer's now

    struct Want {
        std::size_t cursor;
        std::size_t before;
        std::size_t after;
        const char* first_row;
        const char* last_row;
    };
    // A FOUR-ROW WINDOW ONTO EIGHT PROPERTIES CAN SAY BOTH THINGS AT ONCE, which HD-6's
    // five-row one could not reach with this population: at the cursors in the middle the
    // list spends a row on `... N earlier` AND a row on `... N more` and shows two.
    for (const Want& w : {Want{0, 0, 5, ">Identity #1", "... 5 more"},
                          Want{3, 2, 4, "... 2 earlier", "... 4 more"},
                          Want{4, 3, 3, "... 3 earlier", "... 3 more"},
                          Want{7, 5, 0, "... 5 earlier", ">Resolved 28 x 6 cells"}}) {
        CAPTURE(w.cursor);
        s.cursor = w.cursor;
        const InfoBodyPlace body = body_of(d, s);
        CHECK(body.properties.before == w.before);
        CHECK(body.properties.after == w.after);

        // THE SELECTED ROW IS ALWAYS ON SCREEN (§12), which is the whole reason the window
        // moves at all.
        CHECK(prose_row_of_property(body, w.cursor) != kNoProseRow);

        const surface::SurfaceCanvas c = paint(d, s);
        const surface::SurfaceTextRegion* shown = body_on(c, body);
        REQUIRE(shown != nullptr);
        CHECK(shown->rows.size() == body.capacity); // HD-8: the footer is anchored to the foot
        CHECK(shown->rows[static_cast<std::size_t>(body.heading_row) + 1].text == w.first_row);
        // THE LAST ROW OF THE PROPERTY RUN, which is no longer the last row of the region --
        // the footer is under it. Asked for by the run's own arithmetic rather than by
        // `back()`, so this case cannot come to be about the control rows by accident.
        CHECK(shown->rows[static_cast<std::size_t>(body.heading_row) + body.properties_rows]
                  .text == w.last_row);

        // EVERY MARKER IS A COUNT AND A DIRECTION, and it comes out of the row budget -- the
        // PROPERTY list's share of it since HD-7, which is the number the markers are bounded
        // by and the number `list_window` was handed.
        const std::size_t first = static_cast<std::size_t>(body.heading_row) + 1;
        if (w.before > 0) {
            CHECK(shown->rows[first].text == omitted_text(w.before, "earlier"));
            CHECK(shown->rows[first].role == surface::role::kMuted);
        }
        if (w.after > 0) {
            const std::size_t last =
                static_cast<std::size_t>(body.heading_row) + body.properties_rows;
            CHECK(shown->rows[last].text == omitted_text(w.after, "more"));
            CHECK(shown->rows[last].role == surface::role::kMuted);
        }
        CHECK(body.properties.count + (w.before > 0 ? 1u : 0u) + (w.after > 0 ? 1u : 0u) ==
              body.properties_rows);
    }
}

TEST_CASE("HD-6: the selected row stays visible across the boundary, by keys only") {
    // §12 and §40 (vertical window). Driven through the real weave: Up and Down are the only
    // gestures, the window follows because the SELECTION moved, and no property becomes
    // unreachable.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    REQUIRE(t.session().rows.size() == 8);
    // THE PREMISE, STATED AS ONE: the body cannot hold every property, so the window has
    // somewhere to move to. HD-7's composition decides the number and this case does not
    // depend on which number it is.
    REQUIRE(body_place(t).properties_rows < t.session().rows.size());
    CHECK(body_place(t).properties_rows == 5); // HD-8: two of the seven are the footer's

    // DOWN through the whole population, one row at a time.
    std::vector<std::string> reached;
    for (std::size_t step = 0; step < t.session().rows.size(); ++step) {
        const InfoBodyPlace body = body_place(t);
        CAPTURE(t.session().cursor);
        CHECK(prose_row_of_property(body, t.session().cursor) != kNoProseRow);
        reached.push_back(t.session().rows[t.session().cursor].label());
        const surface::SurfaceTextRegion* shown = body_on(t.canvases.back(), body);
        REQUIRE(shown != nullptr);
        CHECK(shown->rows.size() <= body.capacity);
        // THE ROW THE CURSOR IS ON CARRIES THE MARK, wherever the window put it.
        CHECK(shown->rows[static_cast<std::size_t>(
                              prose_row_of_property(body, t.session().cursor))]
                  .text.rfind(">", 0) == 0);
        t.key(input::scan::kDown);
    }
    // The cursor opens on the first EDITABLE row (Name), so a walk down reaches every row
    // from there to the end -- and back up reaches the two above it.
    CHECK(reached.front() == "Name");
    CHECK(reached.back() == "Resolved");

    for (std::size_t step = 0; step < t.session().rows.size(); ++step) {
        const InfoBodyPlace body = body_place(t);
        CAPTURE(t.session().cursor);
        CHECK(prose_row_of_property(body, t.session().cursor) != kNoProseRow);
        t.key(input::scan::kUp);
    }
    CHECK(t.session().cursor == 0);
    CHECK(prose_row_of_property(body_place(t), 0) != kNoProseRow);
    CHECK(t.session().rows[0].label() == "Identity"); // the one above the opening cursor

    // AND THE VIEW IS NOT THE SELECTION. Nothing reordered the properties to make one
    // visible: the rows are in the order `inspector_rows` built them, at every window.
    const InfoBodyPlace body = body_place(t);
    for (std::size_t n = 0; n < body.properties.count; ++n) {
        CHECK(property_at_prose_row(body, prose_row_of_property(body, body.properties.first + n)) ==
              body.properties.first + n);
    }
}

TEST_CASE("HD-6: entering an edit and being refused both keep the row on screen") {
    // §12's other two requirements. `inspector_focus` is what makes them structural rather
    // than incidental: a live draft is the row that must be visible, and it wins over the
    // cursor if the two ever part company.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    REQUIRE(body_place(t).properties_rows < t.session().rows.size());

    t.begin_editing("Height"); // row 6 of 8 -- outside the opening window
    REQUIRE(t.row("Height")->editing());
    const InfoBodyPlace editing = body_place(t);
    CHECK(editing.properties.before > 0); // the body really did have to move
    CHECK(prose_row_of_property(editing, editing_index(t)) != kNoProseRow);
    // ON THE LAST ROW BUT ONE OF THE PROPERTY LIST'S OWN SHARE -- resolved, because the
    // share is what the composition gave it rather than the whole body (HD-7).
    CHECK(editing_prose_row(t, editing) ==
          editing.heading_row + static_cast<std::int64_t>(editing.properties_rows) - 1);

    // A REFUSAL DOES NOT MOVE IT, and the draft, the caret and the window all survive.
    for (int i = 0; i < 4; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text("z");
    t.key(input::scan::kReturn);
    REQUIRE(t.row("Height")->editing());
    REQUIRE_FALSE(t.row("Height")->refusal().empty());
    const InfoBodyPlace refused = body_place(t);
    CHECK(refused.properties.first == editing.properties.first);
    CHECK(prose_row_of_property(refused, editing_index(t)) != kNoProseRow);
    CHECK(t.row("Height")->draft() == "z");

    // AND THE CARET IS PUBLISHED ON THAT ROW, in the body's own prose lattice.
    const surface::SurfaceTextRegion* shown = body_on(t.canvases.back(), refused);
    REQUIRE(shown != nullptr);
    CHECK(shown->caret_row == editing_prose_row(t, refused));
    CHECK(shown->caret_col == property_caret_column(*t.row("Height")));
}

TEST_CASE("HD-6: a resize reconciles the row count, the window and the draft together") {
    // §20. One extent, one resolved place, and everything that depends on the room moves
    // from it: how many rows fit, which of them are shown, how wide a value is, and where
    // the horizontal window of a live draft sits.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.begin_editing("Height");
    for (int i = 0; i < 4; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.text("7");
    const InfoBodyPlace narrow = body_place(t);
    REQUIRE(narrow.properties_rows < t.session().rows.size());
    REQUIRE(narrow.properties.before > 0);

    // TALLER: more rows fit, and the omission shrinks or goes.
    t.publish(loom::to_value(surface::SurfaceExtent{78, 40, 8, 18}));
    const InfoBodyPlace tall = body_place(t);
    CHECK(tall.capacity > narrow.capacity);
    CHECK(tall.properties.count > narrow.properties.count);
    CHECK(prose_row_of_property(tall, editing_index(t)) != kNoProseRow);
    CHECK(t.row("Height")->draft() == "7");
    CHECK(t.row("Height")->editing());

    // A CHARACTER MEDIUM AT THE SAME EXTENT: more rows still, and a narrower value.
    t.publish(loom::to_value(surface::SurfaceExtent{78, 40, 0, 0}));
    const InfoBodyPlace cells = body_place(t);
    CHECK(cells.capacity > tall.capacity);
    CHECK(cells.value_columns < tall.value_columns);
    CHECK(prose_row_of_property(cells, editing_index(t)) != kNoProseRow);
    CHECK(t.row("Height")->draft() == "7");

    // BACK TO THE SMALL WINDOW: the draft is still there and still on screen.
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    const InfoBodyPlace back = body_place(t);
    CHECK(back.capacity == narrow.capacity);
    CHECK(back.properties.first == narrow.properties.first);
    CHECK(prose_row_of_property(back, editing_index(t)) != kNoProseRow);
    CHECK(t.row("Height")->draft() == "7");

    // AND NO TWO ROWS OVERLAP AT ANY OF THEM: the body publishes one row per prose row.
    const surface::SurfaceTextRegion* shown = body_on(t.canvases.back(), back);
    REQUIRE(shown != nullptr);
    CHECK(shown->rows.size() == back.capacity);
}

TEST_CASE("HD-6: a resting value that does not fit is MARKED, not dropped") {
    // §8 and §27, and the asymmetry HD-5 left open on purpose: the editing row was truthful
    // and its neighbours were not. Measured on the pristine tree: a committed 32-character
    // name published a 42-character label from column 52 of an 80-column canvas -- fourteen
    // characters past the edge, dropped by `canvas_body`'s `put`, with no marker at all.
    WorkshopDoc d = one_object();
    Session s;
    s.selected = 1;
    s.screen_w = 80;
    s.screen_h = 38;
    refocus(d, s);
    const InfoBodyPlace body = body_of(d, s);
    REQUIRE(body.value_columns == 17);

    // SHORT: unchanged, and not even fitted -- `detail::fit` returns a value that fits
    // byte-for-byte, marker included.
    CHECK(property_row_text(s.rows[1], false, body.value_columns) == " Name     panel");

    // EXACTLY THE WIDTH: still whole. The mark begins one character later than that.
    d.elements[0].label = std::string(17, 'a');
    refocus(d, s);
    CHECK(property_row_text(s.rows[1], false, body.value_columns) ==
          " Name     " + std::string(17, 'a'));
    d.elements[0].label = std::string(18, 'a');
    refocus(d, s);
    CHECK(property_row_text(s.rows[1], false, body.value_columns) ==
          " Name     " + std::string(14, 'a') + "...");

    // OVER-WIDTH: the established marker, and the whole row still fits the body.
    d.elements[0].label = "the-quick-brown-fox-jumps-over-5";
    refocus(d, s);
    const std::string row = property_row_text(s.rows[1], false, body.value_columns);
    CHECK(row == " Name     the-quick-brow...");
    CHECK(static_cast<std::int64_t>(row.size()) <= body.columns);
    const surface::SurfaceCanvas painted = paint(d, s);
    const surface::SurfaceTextRegion* shown = body_on(painted, body);
    REQUIRE(shown != nullptr);
    // The cursor opens on Name, so the row a maker SEES carries the mark; the fitting is
    // the same either way, which is the half this case is about.
    const std::size_t name_row = static_cast<std::size_t>(prose_row_of_property(body, 1));
    CHECK(shown->rows[name_row].text == property_row_text(s.rows[1], true, body.value_columns));
    CHECK(shown->rows[name_row].text.substr(1) == row.substr(1));

    // THE LABEL SURVIVES INTACT, whatever the value does to it.
    CHECK(row.substr(0, 10) == " Name     ");

    // AND THE WHOLE AUTHORED VALUE IS STILL REACHABLE: entering the edit opens the draft on
    // the committed value, not on what the row was showing.
    s.rows[1].begin();
    CHECK(s.rows[1].draft() == "the-quick-brown-fox-jumps-over-5");
    CHECK(s.rows[1].editor().caret() == 32);
    // ...and leaving it returns to the truthful bounded resting presentation.
    s.rows[1].cancel();
    CHECK(property_row_text(s.rows[1], false, body.value_columns) == row);
}

TEST_CASE("HD-6: the graphical property caret is a BAR, off the same fit that drew the row") {
    // §7. Nothing here is an Inspector caret renderer: the body is a region with room for the
    // face, so `plan_text_regions` sets it in type and `plan_caret` puts a `kCaretWidthPx`
    // bar at the caret's own column -- the identical machinery the Terminal's pane uses.
    // HD-5's row could not reach this because one cell holds no line of the face.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    t.begin_editing("Name");
    t.key(input::scan::kLeft);
    t.key(input::scan::kLeft);

    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.fit.graphical()); // the fidelity claim, before anything is drawn
    const surface::SurfaceCanvas& c = t.canvases.back();
    const std::vector<surface::PlanTextRegion> plan = surface::plan_text_regions(
        c, surface::SurfaceExtent{80, 38, 8, 18}, surface::PlanSize{4000, 4000});
    const surface::PlanTextRegion* drawn = nullptr;
    for (const surface::PlanTextRegion& r : plan) {
        if (r.view.x == body.region_x * surface::kCanvasCellPx) {
            drawn = &r;
        }
    }
    REQUIRE(drawn != nullptr);
    CHECK(drawn->line_px == 18);
    CHECK(drawn->caret.present);
    CHECK(drawn->caret.w == surface::kCaretWidthPx); // a BAR, not a glyph
    CHECK(drawn->caret.h == 18);
    CHECK(drawn->caret.x == drawn->origin_x + property_caret_column(*t.row("Name")) * 8);
    CHECK(drawn->caret.y == drawn->origin_y + editing_prose_row(t, body) * 18);

    // AND A CHARACTER MEDIUM STILL ANSWERS WITH THE GLYPH, at the same prose position: two
    // projections of one published fact.
    const std::vector<surface::ProjectedRow> cells = surface::project_text_regions(c);
    const std::string& row = cells[static_cast<std::size_t>(editing_prose_row(t, body))].label.text;
    CHECK(row.rfind(">Name     pan_el", 0) == 0);
}

TEST_CASE("HD-6: a press under HD row geometry names the property the eye is on") {
    // §16. Once a body row is 18 device pixels tall against a 12-pixel cell, rounding a press
    // to a Workshop cell would name the wrong property for most of the body -- so the press
    // goes through the same `RegionFit` the rows were positioned with, on BOTH axes.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    t.begin_editing("Height");
    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.fit.graphical());
    REQUIRE(body.properties.count == t.session().rows.size()); // this extent holds them all

    // EVERY ROW OF THE BODY, resolved from the middle of its own band of pixels.
    for (std::size_t i = 0; i < body.properties.count; ++i) {
        const std::int64_t row = prose_row_of_property(body, i);
        CAPTURE(i);
        const std::int64_t py = body.region_y * surface::kCanvasCellPx + body.fit.origin_y +
                                row * body.fit.line_px + body.fit.line_px / 2;
        CHECK(property_at_prose_row(
                  body, surface::prose_row_of_pixel(py, body.region_y, body.fit)) == i);
    }

    // A PRESS ON THE EDITING ROW PLACES THE CARET; the same pixel rounded to a Workshop cell
    // would be a different row entirely, which is what makes this a measurement.
    const std::int64_t at_row = editing_prose_row(t, body);
    t.press_at(value_pixel_x(body, 1), value_pixel_y(body, at_row), input::space::kPixels);
    CHECK(t.row("Height")->editor().caret() == 1);
    CHECK(surface::cell_of_pixel(value_pixel_y(body, at_row)) - body.region_y !=
          at_row); // the cell row and the prose row genuinely disagree here

    // A PRESS ON A DIFFERENT PROPERTY'S ROW IS NOT THE EDITOR'S -- it is the panel's, and the
    // panel answers in words (PNL-2). §18: clicking does not begin an edit.
    t.press_at(value_pixel_x(body, 1), value_pixel_y(body, at_row - 2), input::space::kPixels);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
    CHECK(t.row("Height")->editor().caret() == 1);
    CHECK_FALSE(t.row("X")->editing());

    // A PRESS ON THE MARK OR THE NAME OF THE EDITING ROW is on the row, and the component
    // clamps it to the start of what is shown.
    t.press_at(value_pixel_x(body, -4), value_pixel_y(body, at_row), input::space::kPixels);
    CHECK(t.row("Height")->editor().caret() == t.row("Height")->editor().first_visible());
}

TEST_CASE("HD-6: the body falls back to cells when it is too short for the face") {
    // §24. HD-5's repair is not special-cased away: a body with no room for one line of type
    // resolves to the CELL projection, byte-for-byte the answer a faceless medium gets, and
    // the region is still drawn by somebody.
    WorkshopDoc d = one_object();
    Session s;
    s.selected = 1;
    s.screen_w = kScreenMinW;
    s.screen_h = kScreenMinH;
    refocus(d, s);
    const ui::Rect panel = bounds_of(s.panels, panel::kInfo, screen_of(s)).rect;

    // A LINE TALLER THAN THE WHOLE BODY: nine cells is 108 pixels, so a 200-pixel line fits
    // no row at all and the body is described in cells instead.
    s.text_advance_px = 8;
    s.text_line_px = 200;
    const InfoBodyPlace squeezed = info_body_place(panel, screen_of(s), d, s);
    CHECK_FALSE(squeezed.fit.graphical());
    CHECK(squeezed.capacity == static_cast<std::size_t>(squeezed.region_h));

    Session faceless = s;
    faceless.text_advance_px = 0;
    faceless.text_line_px = 0;
    CHECK(squeezed.fit == info_body_place(panel, screen_of(faceless), d, faceless).fit);

    // AND IT IS STILL DRAWN. Both draw lists partition on `fit_region(...).graphical()`, so
    // the body is in exactly one of them and never in neither.
    const surface::SurfaceCanvas c = paint(d, s);
    const surface::SurfaceExtent metric{s.screen_w * surface::kCanvasCellPx,
                                        s.screen_h * surface::kCanvasCellPx, 8, 200};
    const std::size_t typed =
        surface::plan_text_regions(c, metric, surface::PlanSize{4000, 4000}).size();
    const std::size_t celled = surface::project_text_regions(c, metric).size();
    CHECK(typed == 0);
    CHECK(celled == static_cast<std::size_t>(squeezed.region_h));
}

TEST_CASE("HD-6: a panel with no room for a body publishes no body at all") {
    // The other end of `present`. `bounds_of` answers with an empty rectangle for a panel
    // nobody has open, a panel too narrow for a value beside a name has nowhere to put one,
    // and a panel too short to seat a row of each list around the heading between them has
    // nowhere to put either -- all three are refusals rather than a rectangle somewhere the
    // panel is not.
    WorkshopDoc d = one_object();
    Session s;
    s.selected = 1;
    refocus(d, s);
    const Screen sc = screen_of(s);
    CHECK_FALSE(info_body_place(ui::Rect{}, sc, d, s).present);
    CHECK_FALSE(info_body_place(ui::Rect{50, 0, 28, kInfoBodyY}, sc, d, s).present);
    CHECK_FALSE(info_body_place(ui::Rect{50, 0, kPropertyMarkCols + kPropertyLabelCols, 30}, sc,
                                d, s)
                    .present);
    // HD-7: one cell of body is one prose row here and the body needs three. HD-8: it needs
    // `kActionRows` more, because a panel that shows a maker two lists and no way to act on
    // either is the state this phase exists to end -- so the controls are bought at the same
    // price as the material, and the floor moves rather than the footer being dropped.
    // Unreachable at every screen this composition supports (the shortest body a face
    // resolves to here is ten prose rows); asserted because the metric arrives on the bus.
    CHECK_FALSE(info_body_place(ui::Rect{50, 0, 28, kInfoBodyY + 2}, sc, d, s).present);
    CHECK_FALSE(info_body_place(ui::Rect{50, 0, 28, kInfoBodyY + 3}, sc, d, s).present);
    CHECK_FALSE(info_body_place(ui::Rect{50, 0, 28,
                                         kInfoBodyY + static_cast<std::int64_t>(
                                                          kInfoBodyMinRows + kActionRows) - 1},
                                sc, d, s)
                    .present);
    CHECK(info_body_place(ui::Rect{50, 0, 28,
                                   kInfoBodyY +
                                       static_cast<std::int64_t>(kInfoBodyMinRows + kActionRows)},
                          sc, d, s)
              .present);

    // A CLOSED INFO PANEL PAINTS NO BODY, which is what keeps the caret count honest.
    Session closed;
    closed.selected = 1;
    refocus(d, closed);
    (void)close_panel(closed.panels, panel::kInfo);
    CHECK(paint(d, closed).texts.empty());
}

TEST_CASE("HD-6: the property layer never learned that graphical rows got taller") {
    // §1. HD-6 is a presentation phase. `Row`, `TextForm<T>`, the parse, the refusal and the
    // commit are the same code they were, and the component is untouched -- what changed is
    // the room the presentation asks for and the number it hands the component.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    t.begin_editing("Width");
    for (int i = 0; i < 3; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : std::string("500%")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kReturn);
    CHECK(t.row("Width")->editing()); // refused, and still editing
    CHECK(t.notice() == "Width: a share is 1% to 100%");
    CHECK(t.doc().elements[0].width.amount == 60); // the property is untouched

    for (int i = 0; i < 4; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : std::string("40%")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kReturn);
    CHECK_FALSE(t.row("Width")->editing());
    CHECK(t.doc().elements[0].width.amount == 40);
    CHECK(t.notice() == "committed Width = 40%");
    CHECK(t.doc().elements[0].width.mode == ui::kExtentPercent);
}

// ---- tier 8: the terminal is a medium with a size (TUI-0) ---------------------------------
//
// Every case below is written in TERMINAL sizes, because that is what a maker has, and turns
// them into canvas extents through `surface::tui_canvas_extent` -- the medium's own arithmetic
// -- rather than through a second copy of it. A case that wrote `SurfaceExtent{120, 37}` would
// be asserting against a number this suite had computed for itself, and would keep passing on
// the day the medium's own answer changed.

TEST_CASE("TUI-0: the terminal's own size becomes Workshop's screen, growing and shrinking") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE_FALSE(t.canvases.empty());

    // BEFORE ANY MEDIUM SPEAKS: the documented minimum, which is what a redirected run keeps
    // for its whole life.
    CHECK(t.canvases.back().width == kScreenMinW);
    CHECK(t.canvases.back().height == kScreenMinH);

    struct Want {
        std::int64_t cols;
        std::int64_t rows;
        std::int64_t screen_w;
        std::int64_t screen_h;
    };
    // The four sizes TUI-0 measured on a real pty, plus one absurd one as a canary for a
    // fixed constant hiding in a responsive path (§26).
    for (const Want& want : {Want{120, 40, 120, 37}, Want{160, 50, 160, 47},
                             Want{90, 28, 90, 25}, Want{240, 80, 240, 77},
                             Want{78, 25, kScreenMinW, kScreenMinH}}) {
        CAPTURE(want.cols);
        CAPTURE(want.rows);
        const std::size_t before = t.canvases.size();
        t.publish(loom::to_value(
            surface::tui_canvas_extent(surface::TerminalSize{want.cols, want.rows})));
        REQUIRE(t.canvases.size() > before);

        CHECK(t.session().screen_w == want.screen_w);
        CHECK(t.session().screen_h == want.screen_h);

        // THE CANVAS IS THE SCREEN, and the screen FITS IN THE TERMINAL: what a publisher
        // paints plus what the layout spends on itself is never more than the room there is.
        const surface::SurfaceCanvas& c = t.canvases.back();
        CHECK(c.width == want.screen_w);
        CHECK(c.height == want.screen_h);
        CHECK(c.width <= want.cols);
        CHECK(c.height + surface::kTuiReservedRows <= want.rows);

        // A CELL MEDIUM STAYS A CELL MEDIUM. No pixel is invented at any size.
        CHECK(t.session().text_advance_px == 0);
        CHECK(t.session().text_line_px == 0);

        // NO ROW PAINTS OUTSIDE ITS BOUNDS: the rasterized picture is exactly as many rows as
        // the canvas claims, each exactly as wide.
        const std::vector<std::string> rows = rasterized(c);
        REQUIRE(rows.size() == static_cast<std::size_t>(c.height));
        for (const std::string& row : rows) {
            CHECK(row.size() == static_cast<std::size_t>(c.width));
        }

        // AND THE SAME EXTENT AGAIN IS NOT A REPAINT. The skin already guards its publishing;
        // this second guard is what makes a maker dragging an edge across a clamp boundary
        // see one screen rather than a flicker.
        const std::size_t settled = t.canvases.size();
        t.publish(loom::to_value(
            surface::tui_canvas_extent(surface::TerminalSize{want.cols, want.rows})));
        CHECK(t.canvases.size() == settled);
    }

    // GROWING AND SHRINKING ARE ONE MECHANISM, walked as a hand on an edge would walk it.
    for (std::int64_t rows = 30; rows <= 44; ++rows) {
        t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{100, rows})));
        CHECK(t.session().screen_h == rows - surface::kTuiReservedRows);
    }
    for (std::int64_t rows = 44; rows >= 30; --rows) {
        t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{100, rows})));
        CHECK(t.session().screen_h == rows - surface::kTuiReservedRows);
    }
}

TEST_CASE("TUI-0: a terminal below the composition's minimum is published, not fictionalised") {
    // §8. The medium's job is to say what it measured; the clamp is Workshop's own policy and
    // has been since G-2. Keeping them separate is what makes the small case honest: nothing
    // anywhere claims a 60x15 terminal is 78x22, and what a maker sees is the documented
    // consequence of a composition with a stated minimum meeting a surface below it.
    Live t;
    const surface::SurfaceExtent small =
        surface::tui_canvas_extent(surface::TerminalSize{60, 15});

    // THE MEASUREMENT IS THE TRUTH AND IT IS SMALL. 15 rows less the layout's three is 12,
    // and the medium says twelve rather than rounding up to a number that would fit.
    CHECK(small.width == 60);
    CHECK(small.height == 12);

    t.publish(loom::to_value(small));

    // WORKSHOP CLAMPS, VISIBLY, TO THE COMPOSITION IT IS HONEST ON. `adopt_screen` bounds an
    // extent into [minimum, maximum] and the terminal clips whatever does not fit, which is
    // what a terminal has always done with output too wide for it.
    CHECK(t.session().screen_w == kScreenMinW);
    CHECK(t.session().screen_h == kScreenMinH);

    // ...AND THE CLAMP IS NOT A SECOND MEASUREMENT. Every terminal below the minimum resolves
    // to the one screen, so a maker dragging an edge around inside that region sees no
    // flicker, and Workshop never paints a size no medium reported.
    const std::size_t settled = t.canvases.size();
    for (const surface::TerminalSize& tiny :
         {surface::TerminalSize{40, 10}, surface::TerminalSize{78, 24},
          surface::TerminalSize{20, 5}}) {
        t.publish(loom::to_value(surface::tui_canvas_extent(tiny)));
    }
    CHECK(t.canvases.size() == settled);
    CHECK(t.session().screen_w == kScreenMinW);

    // A TERMINAL WITH NO ROOM AT ALL SAYS NOTHING, and nothing is exactly what a publisher
    // should hear: `{0,0}` off the bus is refused by `adopt_screen` for the same reason the
    // medium never publishes it -- "there is no room" is a sentence nobody may say.
    CHECK(surface::tui_canvas_extent(surface::TerminalSize{120, 2}).height == 0);
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{120, 2})));
    CHECK(t.canvases.size() == settled);
    CHECK(t.session().screen_w == kScreenMinW);
}

TEST_CASE("TUI-0: a terminal resize is presentation context, never an authored act") {
    // §12. A hand on a terminal edge must not become a gesture aimed at the document.
    Live t;
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{90, 28})));
    t.key(input::scan::kN); // a second object, so a SELECTION is a real choice
    t.key(input::scan::kTab);

    const std::int64_t selected = t.session().selected;
    const std::string authored = persist::to_text(t.doc());

    // A LIVE DRAFT, MID-EDIT, WITH A CARET THAT IS NOT AT THE END.
    t.begin_editing("Name");
    while (!t.row("Name")->draft().empty()) {
        t.key(input::scan::kBackspace);
    }
    t.text("edge");
    t.key(input::scan::kLeft);
    t.key(input::scan::kLeft);
    const std::size_t cursor = t.session().cursor;
    const std::size_t caret = t.row("Name")->editor().caret();
    REQUIRE(t.row("Name")->editing());
    REQUIRE(t.row("Name")->draft() == "edge");

    for (const surface::TerminalSize& size :
         {surface::TerminalSize{160, 50}, surface::TerminalSize{78, 24},
          surface::TerminalSize{120, 40}, surface::TerminalSize{90, 28}}) {
        CAPTURE(size.cols);
        t.publish(loom::to_value(surface::tui_canvas_extent(size)));

        // NOTHING AUTHORED MOVED -- compared as the bytes that would be SAVED, so a change
        // anywhere in the document would show up here whether or not a test knew to look.
        CHECK(persist::to_text(t.doc()) == authored);
        // ...AND NO DOCUMENT WAS REBUILT UNDER THE MAKER: the selection, the cursor, the
        // draft, the caret and the editing state all survive.
        CHECK(t.session().selected == selected);
        CHECK(t.session().cursor == cursor);
        REQUIRE(t.row("Name") != nullptr);
        CHECK(t.row("Name")->editing());
        CHECK(t.row("Name")->draft() == "edge");
        CHECK(t.row("Name")->editor().caret() == caret);
    }

    // A REFUSAL SURVIVES ONE TOO, and a refusal is the most fragile thing on this screen:
    // it is the notice AND the row still being editable that together say "try again".
    t.key(input::scan::kEscape);
    t.begin_editing("Width");
    while (!t.row("Width")->draft().empty()) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : std::string("500%")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kReturn);
    const std::string refusal = t.notice();
    const std::string on_the_row = t.row("Width")->refusal();
    REQUIRE_FALSE(refusal.empty());
    REQUIRE_FALSE(on_the_row.empty());
    REQUIRE(t.row("Width")->editing());
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{200, 60})));
    CHECK(t.notice() == refusal);
    CHECK(t.row("Width")->editing());
    CHECK(t.row("Width")->draft() == "500%");
    CHECK(t.row("Width")->refusal() == on_the_row);
    // ...AND THE PROPERTY IT WOULD HAVE WRITTEN IS STILL WHAT IT WAS.
    CHECK(persist::to_text(t.doc()) == authored);
}

TEST_CASE("TUI-0: more terminal is more Inspector, and the marker still tells the truth") {
    // §13. The Inspector gained no TUI-specific layout: `info_body_place` and
    // `fit_region` answer a bigger question with the same equation, and the omission marker
    // appears and disappears because the room genuinely changed.
    Live t;
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{78, 25})));
    const InfoBodyPlace minimum = body_place(t);
    const std::size_t properties = t.session().rows.size();
    REQUIRE(properties > 0);

    // A BIG TERMINAL FITS MORE ROWS...
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{120, 50})));
    const InfoBodyPlace roomy = body_place(t);
    CHECK(roomy.capacity > minimum.capacity);

    // ...AND A SMALL ONE FITS FEWER, WITH A MARKER SAYING SO. The minimum TUI screen already
    // shows all eight properties, so the witness for a marker has to come from a genuinely
    // short terminal rather than from a manufactured row list.
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{78, 25})));
    REQUIRE(body_place(t).capacity >= properties);
    CHECK(body_place(t).properties.before == 0);
    CHECK(body_place(t).properties.after == 0);

    // Shorten the panel until the body genuinely cannot hold every property. The extent is
    // clamped at the minimum screen, so the room has to come out of the SESSION the same way
    // a below-minimum medium would leave it -- which is why this drives `paint` directly.
    WorkshopDoc d = one_object();
    Session s;
    s.selected = 1;
    s.screen_w = kScreenMinW;
    s.screen_h = kScreenMinH;
    refocus(d, s);
    REQUIRE(s.rows.size() == 8);
    const InfoBodyPlace cells = info_body_place(
        bounds_of(s.panels, panel::kInfo, screen_of(s)).rect, screen_of(s), d, s);
    // A CELL MEDIUM AT THE MINIMUM HOLDS ALL EIGHT -- HD-6's own measurement, restated here
    // because it is the reason a TUI marker needs a shorter terminal to appear at all.
    CHECK(cells.capacity >= 8);
    CHECK(cells.properties.before == 0);
    CHECK(cells.properties.after == 0);

    // AND THE ROWS THE BODY PUBLISHES ARE THE ROWS IT SAID IT WOULD.
    //
    // THE CANVAS IS NAMED, and that is not a style choice: `body_on` hands back a pointer
    // INTO the canvas it was given, so passing `paint(d, s)` directly would leave `shown`
    // pointing at a temporary that died at the semicolon. The ordinary lane passed over
    // exactly that; the sanitizer lane named it as a heap-use-after-free, which is the
    // second kind of evidence W-3a built this lane to be.
    const surface::SurfaceCanvas painted = paint(d, s);
    const surface::SurfaceTextRegion* shown = body_on(painted, cells);
    REQUIRE(shown != nullptr);
    // HD-7: the region carries BOTH lists now. HD-8: it carries the footer too, anchored to
    // the foot, so what it publishes is exactly the capacity -- the two lists, the heading,
    // the spare room written as blank rows, and the two controls.
    CHECK(shown->rows.size() == cells.capacity);
    // The MATERIAL is still exactly the object rows, the heading and the property rows, and
    // everything after it to the footer is blank -- which is what "spare room stays spare"
    // looks like once a footer forces the spare rows to be written down.
    for (std::size_t i = cells.objects_rows + 1 + cells.properties.count;
         i < static_cast<std::size_t>(cells.action_row); ++i) {
        CAPTURE(i);
        CHECK(shown->rows[i].text.empty());
    }
}

TEST_CASE("TUI-0: a wider terminal is a wider command line, and the draft survives it") {
    // §14. The pane's prose width is `screen_of(session).terminal_cols`, and a TextBox is
    // handed that number as an ARGUMENT -- so `component/text_box.hpp` gains room without
    // gaining a line of code, which is the second component witness this saga has.
    Live t;
    (void)t.mount_terminal();
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{78, 25})));
    t.toggle_terminal();
    REQUIRE(t.pane().open);

    const std::string command = "send SurfaceText slot=status text=the-quick-brown-fox-jumps";
    for (const char c : command) {
        t.text(std::string(1, c));
    }
    REQUIRE(t.pane().input.text() == command);

    const std::int64_t narrow_cols = screen_of(t.session()).terminal_cols;
    const std::size_t narrow_visible = t.pane().input.visible(narrow_cols).size();
    // THE LINE IS LONGER THAN THE ROOM, which is what makes this a window at all.
    REQUIRE(narrow_visible < command.size());
    CHECK(t.pane().input.first_visible() > 0);

    // A BIGGER TERMINAL: more of the same authored text is visible, and not one byte of it
    // was lost or committed on the way.
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{160, 50})));
    const std::int64_t wide_cols = screen_of(t.session()).terminal_cols;
    CHECK(wide_cols > narrow_cols);
    CHECK(t.pane().input.text() == command);
    CHECK(t.pane().input.caret() == command.size());
    CHECK(t.pane().input.visible(wide_cols).size() > narrow_visible);
    // Wide enough for the whole line: the window slid all the way home by itself.
    CHECK(t.pane().input.first_visible() == 0);

    // NARROWING AGAIN RECONCILES THE WINDOW AND KEEPS THE CARET IN SIGHT -- HD-4's rule,
    // reached by a terminal edge instead of a window edge, with no path of its own.
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{78, 25})));
    const std::int64_t back_cols = screen_of(t.session()).terminal_cols;
    CHECK(back_cols == narrow_cols);
    CHECK(t.pane().input.text() == command);
    CHECK(t.pane().input.caret() == command.size());
    CHECK(t.pane().input.first_visible() > 0);
    CHECK(t.pane().input.caret() - t.pane().input.first_visible() <=
          static_cast<std::size_t>(back_cols));
    CHECK(t.pane().open);
}

TEST_CASE("HD-7: a bigger terminal is a bigger OBJECTS list, and the marker goes away") {
    // The claim TUI-0 wrote down as pressure and this phase closed, driven the way TUI-0
    // measured it: three real terminal sizes through the real extent message.
    //
    // ON THE PRISTINE HD-7 TREE these three screens were character-for-character identical --
    // five rows and `... 16 more` at 78x25, at 120x40 and at 240x80 -- because the list's
    // capacity was `kListRows`, a constant, while the property body beside it went from nine
    // rows to sixty-four over the same range.
    Live t;
    for (int i = 0; i < 18; ++i) {
        t.key(input::scan::kN);
    }
    REQUIRE(t.doc().elements.size() == 20);
    t.key(input::scan::kTab); // wrap the selection back to the first object

    std::vector<std::string> seen;
    std::vector<std::size_t> shares;
    for (const surface::TerminalSize& size :
         {surface::TerminalSize{78, 25}, surface::TerminalSize{120, 40},
          surface::TerminalSize{240, 80}}) {
        CAPTURE(size.rows);
        t.publish(loom::to_value(surface::tui_canvas_extent(size)));
        const InfoBodyPlace place = body_place(t);
        REQUIRE(place.present);
        shares.push_back(place.objects_rows);
        std::string tail;
        for (const std::string& line : object_lines(t.canvases.back(), t.doc(), t.session())) {
            tail += line + "|";
        }
        seen.push_back(tail);
    }

    // MORE TERMINAL IS MORE LIST, in the direction the room moved.
    CHECK(shares[0] < shares[1]);
    CHECK(shares[1] <= shares[2]);
    CHECK(seen[0] != seen[1]);

    // AND THE SMALL ONE COUNTS WHAT IT CANNOT SHOW while the big ones have nothing to count.
    CHECK(seen[0].find("... ") != std::string::npos);
    CHECK(seen[1].find("... ") == std::string::npos);
    CHECK(seen[2].find("... ") == std::string::npos);
    CHECK(seen[1].find("> #1 panel") != std::string::npos);
    CHECK(seen[1].find("  #20 panel") != std::string::npos); // all twenty, at 120x40
}

// ----------------------------------------------------------------------------
// Tier 12 — HD-7: the Info panel's two lists share one bounded body
// ----------------------------------------------------------------------------
//
// The OBJECTS list was the last information-bearing surface in this panel with a capacity a
// constant decided. `kListRows = 5` and `kRowsY = 8` are gone; both lists are rows of one
// `SurfaceTextRegion` and both capacities come from one `fit_region` and one sharing policy.

namespace {

/// A document of `n` identical objects with the selection on the `at`th, and a session
/// resolved for the given extent. The names are all `panel`, which is what `n` actually
/// produces -- see the duplicate-name case below.
struct Sample {
    WorkshopDoc d;
    Session s;
};
Sample panel_of(std::size_t n, std::size_t at, std::int64_t w, std::int64_t h,
               std::int64_t advance = 0, std::int64_t line = 0) {
    Sample p;
    for (std::size_t i = 0; i < n; ++i) {
        REQUIRE(doc::add_default(p.d) != 0);
    }
    adopt_screen(p.s, w, h, advance, line);
    if (!p.d.elements.empty()) {
        p.s.selected = p.d.elements[at].id;
    }
    refocus(p.d, p.s);
    return p;
}

} // namespace

TEST_CASE("HD-7: the sharing policy is monotonic, bounded and never starves either list") {
    // THE POLICY, ASSERTED AS A PROPERTY RATHER THAN AT THE EXTENTS THIS PHASE HAPPENED TO
    // RUN. Every claim §4 makes about the composition is here, over every budget from nothing
    // to two hundred rows and five demand pairs -- including the pair a maker actually has
    // (a growing document beside eight properties) and the two degenerate ones.
    for (const std::pair<std::size_t, std::size_t>& want :
         {std::pair<std::size_t, std::size_t>{1, 1}, {2, 8}, {20, 8}, {200, 8}, {1, 300}}) {
        CAPTURE(want.first);
        CAPTURE(want.second);
        BodyShare last{};
        for (std::size_t budget = 0; budget <= 200; ++budget) {
            CAPTURE(budget);
            const BodyShare share = share_body_rows(budget, want.first, want.second);

            // BOUNDED: the two shares are the budget, or less when neither needs it all.
            CHECK(share.objects + share.properties <= budget);
            // NEVER MORE THAN A LIST ASKED FOR -- §4.7, and the reason spare room stays spare.
            CHECK(share.objects <= want.first);
            CHECK(share.properties <= want.second);
            // MONOTONIC IN THE ROOM -- §4.5 and §4.6 together: growing the panel never takes a
            // row away from either list.
            CHECK(share.objects >= last.objects);
            CHECK(share.properties >= last.properties);
            // NEITHER IS STARVED once there is room to seat one of each.
            if (budget >= 2) {
                CHECK(share.objects >= 1);
                CHECK(share.properties >= 1);
            }
            // AND WHEN BOTH FIT, BOTH ARE WHOLE and the rest of the body is spare.
            if (want.first + want.second <= budget) {
                CHECK(share.objects == want.first);
                CHECK(share.properties == want.second);
            }
            last = share;
        }
    }

    // THE 50/50 CASE IS A CONSEQUENCE AND NOT A DECISION: it is what "share what is contested
    // equally" produces when both lists want more than half, and it stops the moment either
    // wants less.
    CHECK(share_body_rows(10, 50, 50).objects == 5);
    CHECK(share_body_rows(10, 50, 50).properties == 5);
    CHECK(share_body_rows(10, 3, 50).objects == 3);   // it took what it needed...
    CHECK(share_body_rows(10, 3, 50).properties == 7); // ...and the rest went to the other
    CHECK(share_body_rows(10, 50, 2).objects == 8);
    CHECK(share_body_rows(10, 50, 2).properties == 2);
}

TEST_CASE("HD-7: the OBJECTS list spends the room the medium reports, not a constant") {
    // THE PHASE'S HEADLINE, at the four extents §1 reproduced the ceiling on. On the pristine
    // tree every one of these was five.
    struct Want {
        std::int64_t w;
        std::int64_t h;
        std::int64_t advance;
        std::int64_t line;
        std::size_t objects_rows;
        std::size_t properties_rows;
    };
    // HD-8 TOOK TWO ROWS OFF EVERY ONE OF THESE BUDGETS, and the three that had room to
    // spare did not move at all -- which is the composition policy working: a fixed demand
    // reduces the budget, and a budget reduced by a constant still gives a list that FITS
    // exactly what it asked for.
    std::size_t previous = 0;
    for (const Want& want : {Want{78, 22, 0, 0, 6, 7},   // the minimum TUI screen
                             Want{120, 37, 0, 0, 20, 8}, // a 120x40 terminal: ALL twenty
                             Want{240, 77, 0, 0, 20, 8}, // and a 240x80 one, with room over
                             Want{78, 22, 8, 18, 3, 4},  // the minimum graphical screen
                             Want{80, 38, 8, 18, 10, 8}, // the ordinary window
                             Want{80, 70, 8, 18, 20, 8}}) {
        CAPTURE(want.h);
        CAPTURE(want.line);
        Sample p = panel_of(20, 0, want.w, want.h, want.advance, want.line);
        const InfoBodyPlace body = body_of(p.d, p.s);
        REQUIRE(body.present);
        CHECK(body.objects_rows == want.objects_rows);
        CHECK(body.properties_rows == want.properties_rows);
        // The two lists, the heading between them and the footer under them never exceed the
        // body -- and the footer is always inside it, at every extent.
        CHECK(body.objects_rows + 1 + body.properties_rows + kActionRows <= body.capacity);
        CHECK(body.action_row + static_cast<std::int64_t>(kActionRows) ==
              static_cast<std::int64_t>(body.capacity));
        if (want.advance == 0) {
            CHECK(body.objects_rows >= previous); // the cell lane, growing with the terminal
            previous = body.objects_rows;
        }
    }
}

TEST_CASE("HD-7: spare room stays spare, and the heading sits under the last name") {
    // §4.7 and §4.8. Two objects in a very tall panel: the list takes two rows, `PROPERTIES`
    // is on the third, and the sixty rows nobody asked for stay empty. A list inflated to
    // fill the panel would put the properties on the floor of it.
    Sample p = panel_of(2, 0, 240, 77);
    const InfoBodyPlace body = body_of(p.d, p.s);
    REQUIRE(body.present);
    CHECK(body.objects_rows == 2);
    CHECK(body.properties_rows == 8);
    CHECK(body.heading_row == 2);
    const std::size_t spent = body.objects_rows + 1 + body.properties_rows;
    CHECK(body.capacity > spent + 50);

    const surface::SurfaceCanvas c = paint(p.d, p.s);
    const surface::SurfaceTextRegion* shown = body_on(c, body);
    REQUIRE(shown != nullptr);
    CHECK(shown->rows[0].text == "> #1 panel");
    CHECK(shown->rows[2].text == "PROPERTIES");
    CHECK(shown->rows[3].text.rfind(" Identity", 0) == 0);
    // ELEVEN ROWS OF MATERIAL AND NO MORE. HD-7 asserted that as `rows.size() == 11`, which
    // HD-8's foot-anchored footer makes false about the REGION and still true about the
    // material: everything from the eleventh row to the controls is blank, and the controls
    // are the last two rows whatever the document does.
    CHECK(shown->rows.size() == body.capacity);
    for (std::size_t i = spent; i < static_cast<std::size_t>(body.action_row); ++i) {
        CAPTURE(i);
        CHECK(shown->rows[i].text.empty());
    }
    CHECK(shown->rows[static_cast<std::size_t>(body.action_row)].text == "[ Create ]");
    CHECK(shown->rows.back().text == "[ Delete ]");
}

TEST_CASE("HD-7: neither list paints through the other, at any extent") {
    // §4.4, and it is structural rather than checked: the two lists are disjoint RUNS of one
    // row budget rather than two rectangles somebody has to keep apart. Asserted anyway,
    // across a population and an extent sweep, because "structural" is a claim.
    for (const std::size_t objects : std::vector<std::size_t>{1, 2, 6, 20, 100}) {
        for (const std::int64_t h : std::vector<std::int64_t>{22, 30, 45, 77}) {
            for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
                CAPTURE(objects);
                CAPTURE(h);
                CAPTURE(line);
                Sample p = panel_of(objects, objects / 2, 80, h, line == 0 ? 0 : 8, line);
                const InfoBodyPlace body = body_of(p.d, p.s);
                REQUIRE(body.present);

                // NO PROSE ROW IS CLAIMED BY BOTH, and the heading is claimed by neither.
                for (std::int64_t row = -2;
                     row < static_cast<std::int64_t>(body.capacity) + 2; ++row) {
                    const bool is_object = object_at_prose_row(body, row) != kNoObject;
                    const bool is_property = property_at_prose_row(body, row) != kNoProperty;
                    const bool both = is_object && is_property;
                    CHECK_FALSE(both);
                    if (row == body.heading_row) {
                        CHECK_FALSE(is_object);
                        CHECK_FALSE(is_property);
                    }
                }
                // AND EVERY ROW EITHER LIST NAMES IS INSIDE THE BODY.
                for (std::size_t i = 0; i < p.d.elements.size(); ++i) {
                    const std::int64_t at = prose_row_of_object(body, i);
                    if (at != kNoProseRow) {
                        CHECK(at >= 0);
                        CHECK(at < body.heading_row);
                    }
                }
                for (std::size_t i = 0; i < p.s.rows.size(); ++i) {
                    const std::int64_t at = prose_row_of_property(body, i);
                    if (at != kNoProseRow) {
                        CHECK(at > body.heading_row);
                        CHECK(at < static_cast<std::int64_t>(body.capacity));
                    }
                }
                // AND THE PUBLISHED REGION NEVER EXCEEDS WHAT IT CLAIMED.
                const surface::SurfaceCanvas c = paint(p.d, p.s);
                const surface::SurfaceTextRegion* shown = body_on(c, body);
                REQUIRE(shown != nullptr);
                CHECK(shown->rows.size() <= body.capacity);
                CHECK(shown->rows[static_cast<std::size_t>(body.heading_row)].text ==
                      "PROPERTIES");
            }
        }
    }
}

TEST_CASE("HD-7: the two object row maps are inverses, and nothing else is an object") {
    // The one-measurer claim on the vertical axis, for the list a press can now reach. The
    // painter positions a name with `prose_row_of_object` and `objects_press` inverts it, so a
    // scrolled list cannot land a press one row off the name a maker aimed at.
    Sample p = panel_of(30, 15, 80, 38, 8, 18);
    const InfoBodyPlace body = body_of(p.d, p.s);
    REQUIRE(body.present);
    REQUIRE(body.objects.before > 0); // the marker really is spending the body's first row
    REQUIRE(body.objects.after > 0);

    for (std::size_t i = body.objects.first; i < body.objects.first + body.objects.count; ++i) {
        CAPTURE(i);
        const std::int64_t at = prose_row_of_object(body, i);
        REQUIRE(at != kNoProseRow);
        CHECK(object_at_prose_row(body, at) == i);
    }
    // A MARKER ROW, THE HEADING, A PROPERTY ROW AND A ROW OUTSIDE THE BODY NAME NO OBJECT.
    CHECK(object_at_prose_row(body, 0) == kNoObject); // `... N earlier`
    CHECK(object_at_prose_row(body, body.heading_row) == kNoObject);
    CHECK(object_at_prose_row(body, body.heading_row + 1) == kNoObject);
    CHECK(object_at_prose_row(body, -1) == kNoObject);
    CHECK(object_at_prose_row(body, static_cast<std::int64_t>(body.capacity)) == kNoObject);
    // An object the window is not showing has no row at all.
    CHECK(prose_row_of_object(body, 0) == kNoProseRow);
    CHECK(prose_row_of_object(body, 29) == kNoProseRow);
}

TEST_CASE("HD-7: a long object name is bounded VISIBLY, and the document keeps all of it") {
    // §10. The mark and the identity come first, so cutting the row at the body's width cuts
    // exactly the name -- and it says so with the `...` this canvas has used since W-6.
    const std::string long_name = "the-quick-brown-fox-jumps-over-the-lazy-dog";
    Sample p = panel_of(2, 0, 78, 22);
    p.d.elements[0].label = long_name;
    refocus(p.d, p.s);

    const InfoBodyPlace narrow = body_of(p.d, p.s);
    const surface::SurfaceCanvas narrow_canvas = paint(p.d, p.s);
    const std::string cut = body_on(narrow_canvas, narrow)->rows[0].text;
    CHECK(static_cast<std::int64_t>(cut.size()) <= narrow.columns);
    CHECK(cut.rfind("> #1 ", 0) == 0);                       // the mark and the identity survive
    CHECK(cut.substr(cut.size() - 3) == "...");              // and the cut is SAID
    CHECK(cut == "> #1 the-quick-brown-fox-...");
    CHECK(p.d.elements[0].label == long_name);               // the authored name is untouched
    // AND WHAT A MAKER READS IS THAT ROW: the cell projection of the body draws it whole.
    CHECK(object_row(narrow_canvas, p.d, p.s, 0) == cut);

    // A WIDER MEDIUM SHOWS MORE OF IT WITHOUT THE DOCUMENT MOVING. The panel is 28 cells at
    // both extents; what changed is how many characters of this repository's face fit across
    // them.
    Sample wide = panel_of(2, 0, 78, 22, 8, 18);
    wide.d.elements[0].label = long_name;
    refocus(wide.d, wide.s);
    const InfoBodyPlace roomier = body_of(wide.d, wide.s);
    const surface::SurfaceCanvas wide_canvas = paint(wide.d, wide.s);
    const std::string more = body_on(wide_canvas, roomier)->rows[0].text;
    CHECK(roomier.columns > narrow.columns);
    CHECK(more.size() > cut.size());
    CHECK(more.rfind("> #1 the-quick-brown-fo", 0) == 0);
    CHECK(more.substr(more.size() - 3) == "...");
    CHECK(wide.d.elements[0].label == long_name);

    CHECK(static_cast<std::int64_t>(more.size()) <= roomier.columns);

    // AND A NAME THAT FITS IS NOT MARKED, which is `detail::fit`'s own rule and not a second.
    p.d.elements[0].label = "short";
    refocus(p.d, p.s);
    CHECK(object_row(paint(p.d, p.s), p.d, p.s, 0) == "> #1 short");
}

TEST_CASE("HD-7: duplicate names cannot confuse the selected row -- identity decides") {
    // §24, and it is not a hypothetical: EVERY object `n` makes is called `panel`, so a
    // document of duplicates is the default rather than an edge case. The mark follows the
    // selected IDENTITY, and the row says which identity it is.
    Sample p = panel_of(4, 2, 78, 22);
    for (ui::Element& e : p.d.elements) {
        e.label = "twin";
    }
    refocus(p.d, p.s);

    const surface::SurfaceCanvas c = paint(p.d, p.s);
    const std::vector<std::string> lines = object_lines(c, p.d, p.s);
    CHECK(lines[0] == "  #1 twin");
    CHECK(lines[1] == "  #2 twin");
    CHECK(lines[2] == "> #3 twin");
    CHECK(lines[3] == "  #4 twin");

    // ONE MARK, and it is on the row whose IDENTITY is selected rather than on the first row
    // whose text matches.
    std::size_t marked = 0;
    for (const std::string& line : lines) {
        if (line.rfind("> ", 0) == 0) {
            ++marked;
        }
    }
    CHECK(marked == 1);
    CHECK(p.s.selected == 3);
    // AND THE PRESS PATH NAMES THE POSITION, not the text: two rows with identical text
    // resolve to different objects.
    const InfoBodyPlace body = body_of(p.d, p.s);
    CHECK(object_at_prose_row(body, 0) == 0);
    CHECK(object_at_prose_row(body, 3) == 3);
    CHECK(p.d.elements[object_at_prose_row(body, 0)].id != p.d.elements[3].id);
}

TEST_CASE("HD-7: an empty document says so, and the arithmetic stays in range") {
    // §23. Zero objects IS reachable -- `d` on the last one gets there -- so the empty state
    // is a row of the body rather than a special case in the painter, and neither list is
    // given zero rows to say nothing in.
    Sample p = panel_of(0, 0, 78, 22);
    REQUIRE(p.d.elements.empty());
    REQUIRE(p.s.selected == 0);
    REQUIRE(p.s.rows.empty());

    const InfoBodyPlace body = body_of(p.d, p.s);
    REQUIRE(body.present);
    CHECK(body.objects_rows >= 1);
    CHECK(body.properties_rows >= 1);
    CHECK(body.objects.count == 0);
    CHECK(body.objects.before == 0);
    CHECK(body.objects.after == 0); // nothing is hidden, because there is nothing

    const surface::SurfaceCanvas c = paint(p.d, p.s);
    CHECK(object_row(c, p.d, p.s, 0) == "(none) -- n makes one");
    CHECK(properties_heading(c, p.d, p.s) == "PROPERTIES");
    CHECK(property_row(c, p.d, p.s, 0) == "(nothing selected)");
    // NO BOGUS MARK, and no row claiming to be an object.
    CHECK(object_at_prose_row(body, 0) == kNoObject);
    CHECK(prose_row_of_object(body, 0) == kNoProseRow);
    for (const surface::SurfaceTextRow& row : body_on(c, body)->rows) {
        CHECK(row.text.rfind("> ", 0) != 0);
    }
}

TEST_CASE("HD-7: creating and deleting objects keeps the list truthful, by keys only") {
    // §22, driven the way a maker reaches it. Nothing is cached: the list is derived from the
    // document every paint, so there is no stale row to survive a delete.
    Live t;
    for (int i = 0; i < 18; ++i) {
        t.key(input::scan::kN);
    }
    REQUIRE(t.doc().elements.size() == 20);

    const auto reading = [&t] {
        return object_lines(t.canvases.back(), t.doc(), t.session());
    };
    const auto counted = [&t] {
        const InfoBodyPlace body = body_place(t);
        return body.objects.before + body.objects.count + body.objects.after;
    };

    // THE NEW ONE IS SELECTED AND VISIBLE, at the end of a document that overflows the share.
    CHECK(counted() == t.doc().elements.size());
    CHECK(prose_row_of_object(body_place(t), 19) != kNoProseRow);
    {
        bool marked = false;
        for (const std::string& line : reading()) {
            marked = marked || line == "> #20 panel";
        }
        CHECK(marked);
    }

    // DELETE A NON-SELECTED ONE: the counts move by exactly one and the selection follows the
    // document's own post-delete rule.
    t.key(input::scan::kTab); // wrap to #1
    REQUIRE(t.session().selected == 1);
    t.key(input::scan::kD);
    CHECK(t.doc().elements.size() == 19);
    CHECK(t.session().selected == 2);
    CHECK(counted() == 19);
    CHECK(prose_row_of_object(body_place(t), position_of(t.doc(), 2)) != kNoProseRow);
    for (const std::string& line : reading()) {
        CHECK(line.find("#1 panel") == std::string::npos); // no stale row survived
    }

    // DELETE THE SELECTED ONE, repeatedly, down to nothing -- the window never names a member
    // the document does not have, and the empty state arrives honestly.
    for (int guard = 0; guard < 40 && !t.doc().elements.empty(); ++guard) {
        t.key(input::scan::kD);
        const InfoBodyPlace body = body_place(t);
        CAPTURE(t.doc().elements.size());
        CHECK(body.objects.first + body.objects.count <= t.doc().elements.size());
        CHECK(counted() == t.doc().elements.size());
        if (!t.doc().elements.empty()) {
            CHECK(prose_row_of_object(body, position_of(t.doc(), t.session().selected)) !=
                  kNoProseRow);
        }
    }
    CHECK(t.doc().elements.empty());
    CHECK(object_row(t.canvases.back(), t.doc(), t.session(), 0) == "(none) -- n makes one");
}

TEST_CASE("HD-7: a press on a visible object row selects it, through the row's own geometry") {
    // §16 outcome B and §17. The press resolves through the SAME body the painter drew with,
    // and the row a maker sees is the row the press names -- including with the list scrolled,
    // which is the case a second copy of the window arithmetic would get wrong.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    for (int i = 0; i < 18; ++i) {
        t.key(input::scan::kN);
    }
    REQUIRE(t.doc().elements.size() == 20);
    REQUIRE(t.session().selected == 20);

    const InfoBodyPlace scrolled = body_place(t);
    REQUIRE(scrolled.present);
    REQUIRE(scrolled.objects.before > 0); // the list HAS scrolled: a marker owns row 0

    // The second name shown, which is NOT the second object in the document.
    const std::size_t second_shown = scrolled.objects.first + 1;
    const std::int64_t at = prose_row_of_object(scrolled, second_shown);
    REQUIRE(at != kNoProseRow);
    const std::int64_t want = t.doc().elements[second_shown].id;
    REQUIRE(want != t.session().selected);

    t.press_at(body_pixel_x(scrolled, 3), body_pixel_y(scrolled, at), input::space::kPixels);
    CHECK(t.session().selected == want);
    CHECK(t.notice() == "selected #" + std::to_string(want));
    // AND THE INSPECTOR FOLLOWED, because a press reaches the document through `select` --
    // the same door `tab` uses.
    CHECK(t.row("Identity")->value() == "#" + std::to_string(want));

    // NOT ROUNDED TO A WORKSHOP CELL. An 18-pixel row against a 12-pixel cell means the cell
    // a press lands in and the prose row it lands on genuinely disagree, and this asserts the
    // disagreement at the pixel it presses rather than assuming it.
    const std::int64_t y = body_pixel_y(scrolled, at);
    const std::int64_t cell_row = y / surface::kCanvasCellPx - scrolled.region_y;
    CHECK(cell_row != at);

    // A PRESS ON A ROW THAT IS NOT AN OBJECT SELECTS NOTHING, and the panel answers as it
    // does for any other press it does not own. The body is RE-RESOLVED first, because the
    // selection just moved and with it the window -- reusing the old place would be exactly
    // the second copy of the geometry this phase exists to not have.
    const InfoBodyPlace now = body_place(t);
    std::int64_t nobody = kNoProseRow;
    for (std::int64_t row = 0; row < now.heading_row + 1; ++row) {
        if (object_at_prose_row(now, row) == kNoObject) {
            nobody = row;
            break;
        }
    }
    REQUIRE(nobody != kNoProseRow);
    const std::int64_t was = t.session().selected;
    t.press_at(body_pixel_x(now, 3), body_pixel_y(now, nobody), input::space::kPixels);
    CHECK(t.session().selected == was);
    CHECK(t.notice().find("nothing under it can be taken hold of") != std::string::npos);
}

TEST_CASE("HD-7: a press on an object row is REFUSED while a property draft is live") {
    // THE MODE LAW, PINNED. Changing objects rebuilds the inspector rows, which is exactly
    // what a live draft cannot survive -- so the press changes nothing and says why. HD-6
    // refused the mirror of this question (a press does not BEGIN an edit) for the same
    // reason: three plausible answers, no measurement between them.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    REQUIRE(t.doc().elements.size() == 2);
    const std::int64_t was = t.session().selected;

    t.begin_editing("Name");
    t.text("x");
    REQUIRE(t.row("Name")->editing());
    REQUIRE(t.row("Name")->draft() == "panelx");

    const InfoBodyPlace body = body_place(t);
    const std::int64_t other = prose_row_of_object(body, 1);
    REQUIRE(other != kNoProseRow);
    t.press_at(body_pixel_x(body, 3), body_pixel_y(body, other), input::space::kPixels);

    CHECK(t.session().selected == was);                    // nothing moved...
    CHECK(t.row("Name")->editing());                       // ...the draft is untouched...
    CHECK(t.row("Name")->draft() == "panelx");
    CHECK(t.notice() == "finish the draft first -- enter commits it, esc cancels");

    // AND THE LIST IS LIVE AGAIN THE MOMENT THE DRAFT IS NOT.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.row("Name")->editing());
    t.press_at(body_pixel_x(body, 3), body_pixel_y(body, other), input::space::kPixels);
    CHECK(t.session().selected == t.doc().elements[1].id);
}

TEST_CASE("HD-7: a cell medium's press takes the other branch, and lands on the same row") {
    // §17's second half. A terminal's position is already a character, so it is never divided
    // by a pixel size its numbers were never in -- and both media resolve the press through
    // one `object_at_prose_row`.
    Live t;
    for (int i = 0; i < 18; ++i) {
        t.key(input::scan::kN);
    }
    t.publish(loom::to_value(surface::tui_canvas_extent(surface::TerminalSize{78, 25})));
    REQUIRE(t.doc().elements.size() == 20);

    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.present);
    REQUIRE_FALSE(body.fit.graphical());
    const std::size_t shown = body.objects.first + 1;
    const std::int64_t at = prose_row_of_object(body, shown);
    REQUIRE(at != kNoProseRow);
    const std::int64_t want = t.doc().elements[shown].id;

    t.press_at(body.region_x + 3, body.region_y + at + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.session().selected == want);
    CHECK(t.notice() == "selected #" + std::to_string(want));
}

TEST_CASE("HD-7: on a graphical medium the object names are set in the SAME type as the rest") {
    // §14. There is no second font, no OBJECTS-specific metric and no second region: the
    // names are rows of the body the property rows are rows of, so a medium that sets that
    // body in type sets the names in type.
    Sample p = panel_of(6, 0, 80, 38, 8, 18);
    const InfoBodyPlace body = body_of(p.d, p.s);
    REQUIRE(body.fit.graphical());
    CHECK(body.fit.advance_px == 8);
    CHECK(body.fit.line_px == 18);

    const surface::SurfaceCanvas c = paint(p.d, p.s);
    const surface::SurfaceExtent metric{80 * surface::kCanvasCellPx, 38 * surface::kCanvasCellPx,
                                        8, 18};
    // THE BODY IS IN THE TYPE LIST AND NOT IN THE CELL LIST -- one region, one partition, and
    // the object rows are inside it.
    const std::vector<surface::PlanTextRegion> typed =
        surface::plan_text_regions(c, metric, surface::PlanSize{4000, 4000});
    REQUIRE(typed.size() == 1);
    CHECK(typed.front().line_px == 18);
    bool named = false;
    for (const surface::PlanTextRow& row : typed.front().rows) {
        named = named || row.text == "> #1 panel";
    }
    CHECK(named);
    for (const surface::ProjectedRow& row : surface::project_text_regions(c, metric)) {
        CHECK(row.label.text.find("#1 panel") == std::string::npos); // not drawn as cells
    }
    // AND THE ROWS ARE POSITIONED OFF THE SAME FIT: the plan carries ONE line pitch for the
    // whole region, which is the face's, so an object name and a property row cannot be set
    // at two different pitches.
    CHECK(typed.front().rows.size() >= 2);
    CHECK(typed.front().line_px == body.fit.line_px);
}

TEST_CASE("HD-7: growing the window gives OBJECTS more and never gives PROPERTIES less") {
    // §4.5 and §4.6 on a running Workshop, with a draft alive across every step so the
    // resize path is the one HD-5 built rather than a fresh session per extent.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    for (int i = 0; i < 18; ++i) {
        t.key(input::scan::kN);
    }
    REQUIRE(t.doc().elements.size() == 20);
    t.begin_editing("Height");
    t.text("7");
    REQUIRE(t.row("Height")->draft() == "47");

    std::size_t objects = 0;
    std::size_t properties = 0;
    for (const std::int64_t h : std::vector<std::int64_t>{22, 28, 34, 40, 52, 70}) {
        CAPTURE(h);
        t.publish(loom::to_value(surface::SurfaceExtent{78, h, 8, 18}));
        const InfoBodyPlace body = body_place(t);
        REQUIRE(body.present);
        CHECK(body.objects_rows >= objects);
        CHECK(body.properties_rows >= properties);
        objects = body.objects_rows;
        properties = body.properties_rows;

        // THE DRAFT, THE CARET AND BOTH SELECTIONS SURVIVE EVERY STEP.
        REQUIRE(t.row("Height")->editing());
        CHECK(t.row("Height")->draft() == "47");
        CHECK(t.session().selected == 20);
        CHECK(prose_row_of_object(body, 19) != kNoProseRow);
        CHECK(prose_row_of_property(body, inspector_focus(t.session())) != kNoProseRow);
    }
    // The tallest extent holds every object and every property, so both markers are gone.
    const InfoBodyPlace tall = body_place(t);
    CHECK(tall.objects.after == 0);
    CHECK(tall.objects.before == 0);
    CHECK(tall.properties.after == 0);
    CHECK(tall.objects_rows == 20);
    CHECK(tall.properties_rows == 8);
}

TEST_CASE("HD-7: the object list's omission is counted and directional, in the panel's role") {
    // §12. The wording is `omitted_text`'s, the arithmetic is `list_window`'s, and both are
    // the functions the property list beside it uses -- so the two halves of one panel cannot
    // come to be worded by two hands.
    Sample p = panel_of(30, 15, 80, 38, 8, 18);
    const InfoBodyPlace body = body_of(p.d, p.s);
    REQUIRE(body.objects.before > 0);
    REQUIRE(body.objects.after > 0);

    const surface::SurfaceCanvas c = paint(p.d, p.s);
    const surface::SurfaceTextRegion* shown = body_on(c, body);
    REQUIRE(shown != nullptr);
    CHECK(shown->rows[0].text == omitted_text(body.objects.before, "earlier"));
    CHECK(shown->rows[0].role == surface::role::kMuted);
    const std::size_t last = static_cast<std::size_t>(body.heading_row) - 1;
    CHECK(shown->rows[last].text == omitted_text(body.objects.after, "more"));
    CHECK(shown->rows[last].role == surface::role::kMuted);
    // EVERY OBJECT IS ACCOUNTED FOR, once.
    CHECK(body.objects.before + body.objects.count + body.objects.after ==
          p.d.elements.size());
    // AND THE MARKERS COME OUT OF THE LIST'S OWN SHARE.
    CHECK(body.objects.count + 2 == body.objects_rows);

    // THE SELECTED OBJECT'S MARK IS TEXT AND NOT ONLY A ROLE, so a terminal with no ground to
    // tint reads the same fact (§15).
    const std::int64_t here = prose_row_of_object(body, 15);
    REQUIRE(here != kNoProseRow);
    CHECK(shown->rows[static_cast<std::size_t>(here)].text.rfind("> ", 0) == 0);
    CHECK(shown->rows[static_cast<std::size_t>(here)].role == surface::role::kAccent);
}

// ---- HD-8: the Info panel gets action controls -------------------------------------------
//
// THE PHASE'S CLAIM, IN ONE SENTENCE: a maker who does not know that `n` creates an object
// can now see that creating one is a thing this tool does, press it, and get exactly what the
// key would have given them. Everything below is that claim taken apart.
//
// THERE IS NO `component::Button`, so there is no component suite for one and every case is
// here. That is not less verification -- it is the verification arriving where the behaviour
// actually lives. What HD-5 put in the component suite was a state machine with an invariant;
// a control is a label, a bit and a row, and all three of those are Workshop's.

TEST_CASE("HD-8: the footer is reserved off the budget, and every HD-7 property survives it") {
    // The composition answer is one subtraction, so the way to prove it did not break the
    // sharing policy is to re-run HD-7's own property over the budget the lists actually get,
    // which is `capacity - 1 - kActionRows`.
    for (const std::pair<std::size_t, std::size_t>& want :
         {std::pair<std::size_t, std::size_t>{1, 1}, {2, 8}, {20, 8}, {200, 8}, {1, 300}}) {
        CAPTURE(want.first);
        CAPTURE(want.second);
        BodyShare last{};
        for (std::size_t capacity = kInfoBodyMinRows + kActionRows; capacity <= 200; ++capacity) {
            CAPTURE(capacity);
            const std::size_t budget = capacity - 1 - kActionRows;
            const BodyShare share = share_body_rows(budget, want.first, want.second);

            // THE FOOTER IS ALWAYS AFFORDED. Whatever the lists take, the two control rows and
            // the heading are still inside the body -- which is the reason the reservation is
            // a subtraction rather than a third claimant on the sharing.
            CHECK(share.objects + share.properties + 1 + kActionRows <= capacity);
            // AND EVERY HD-7 PROPERTY STILL HOLDS OF THE REDUCED BUDGET. A budget reduced by a
            // constant is still a budget: monotonic in the room, never more than a list asked
            // for, and never starving either.
            CHECK(share.objects >= last.objects);
            CHECK(share.properties >= last.properties);
            CHECK(share.objects <= want.first);
            CHECK(share.properties <= want.second);
            CHECK(share.objects >= 1);
            CHECK(share.properties >= 1);
            last = share;
        }
    }
}

TEST_CASE("HD-8: the controls are the last two rows of the body, at every extent and size") {
    // The footer is anchored to the FOOT, so it is in the same place whatever the document
    // does -- and nothing else is ever painted there, which is structural rather than checked
    // (three disjoint runs of one budget) and checked anyway, because "structural" is a claim.
    for (const std::size_t objects : std::vector<std::size_t>{0, 1, 6, 20, 100}) {
        for (const std::int64_t h : std::vector<std::int64_t>{22, 30, 45, 77}) {
            for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
                CAPTURE(objects);
                CAPTURE(h);
                CAPTURE(line);
                Sample p = panel_of(objects, objects / 2, 80, h, line == 0 ? 0 : 8, line);
                const InfoBodyPlace body = body_of(p.d, p.s);
                REQUIRE(body.present);

                // THE FOOTER IS INSIDE THE BODY AND AT ITS FOOT.
                REQUIRE(body.action_row != kNoProseRow);
                CHECK(body.action_row + static_cast<std::int64_t>(kActionRows) ==
                      static_cast<std::int64_t>(body.capacity));
                // NEITHER LIST REACHES IT, and neither does the heading.
                CHECK(static_cast<std::int64_t>(body.objects_rows) <= body.action_row);
                CHECK(body.heading_row < body.action_row);
                CHECK(body.heading_row + static_cast<std::int64_t>(body.properties_rows) <
                      body.action_row);
                // NO PROSE ROW IS CLAIMED BY A LIST AND BY THE FOOTER BOTH.
                for (std::int64_t row = 0; row < static_cast<std::int64_t>(body.capacity);
                     ++row) {
                    const bool is_action = action_at_prose_row(body, row) != kNoAction;
                    const bool is_object = object_at_prose_row(body, row) != kNoObject;
                    const bool is_property = property_at_prose_row(body, row) != kNoProperty;
                    const bool clash = is_action && (is_object || is_property);
                    CHECK_FALSE(clash);
                }

                // AND THE PAINTER PUT THEM THERE. Both labels are on the rows the geometry
                // named, in every medium and at every population -- including the empty
                // document, which is the maker who most needs to see `Create`.
                const surface::SurfaceCanvas c = paint(p.d, p.s);
                const surface::SurfaceTextRegion* shown = body_on(c, body);
                REQUIRE(shown != nullptr);
                REQUIRE(shown->rows.size() == body.capacity);
                for (std::size_t which = 0; which < kActionCount; ++which) {
                    const std::int64_t at = prose_row_of_action(body, which);
                    REQUIRE(at != kNoProseRow);
                    CHECK(shown->rows[static_cast<std::size_t>(at)].text.find(
                              action_label(which)) != std::string::npos);
                }
            }
        }
    }
}

TEST_CASE("HD-8: the action row maps are inverses, and nothing else is a control") {
    // One resolved answer used by paint and by hit, and no third copy -- the same claim HD-6
    // made for properties and HD-7 for objects, at the third and last run of this body.
    Sample p = panel_of(6, 0, 80, 38, 8, 18);
    const InfoBodyPlace body = body_of(p.d, p.s);
    REQUIRE(body.present);

    for (std::size_t which = 0; which < kActionCount; ++which) {
        CAPTURE(which);
        const std::int64_t at = prose_row_of_action(body, which);
        REQUIRE(at != kNoProseRow);
        CHECK(action_at_prose_row(body, at) == which);
        CHECK(action_press_at(body, 0, at) == which);
        CHECK(action_press_at(body, body.fit.columns, at) == which); // the whole ROW is target
    }
    CHECK(prose_row_of_action(body, kActionCount) == kNoProseRow);

    // EVERY OTHER ROW OF THE BODY NAMES NO CONTROL, including the spare ones just above it.
    for (std::int64_t row = -3; row < body.action_row; ++row) {
        CAPTURE(row);
        CHECK(action_at_prose_row(body, row) == kNoAction);
    }
    CHECK(action_at_prose_row(body, static_cast<std::int64_t>(body.capacity)) == kNoAction);
    // A COLUMN OUTSIDE THE BODY NAMES NOTHING, on either side.
    CHECK(action_press_at(body, -1, body.action_row) == kNoAction);
    CHECK(action_press_at(body, body.fit.columns + 1, body.action_row) == kNoAction);
    // AND A BODY THAT IS NOT PRESENT ANSWERS NOTHING RATHER THAN A ROW SOMEWHERE IT IS NOT.
    const InfoBodyPlace absent;
    CHECK(prose_row_of_action(absent, kActionCreate) == kNoProseRow);
    CHECK(action_at_prose_row(absent, 0) == kNoAction);
    CHECK(action_press_at(absent, 0, 0) == kNoAction);
}

TEST_CASE("HD-8: availability is two reasons, one bit, and no prediction of a refusal") {
    // The pure table. `Create` is unavailable for exactly one reason and `Delete` for two,
    // and neither answer knows anything about what the document will say.
    CHECK(action_availability(kActionCreate, false, true) == Availability::kAvailable);
    CHECK(action_availability(kActionCreate, false, false) == Availability::kAvailable);
    CHECK(action_availability(kActionDelete, false, true) == Availability::kAvailable);
    CHECK(action_availability(kActionDelete, false, false) == Availability::kNoTarget);
    // A LIVE DRAFT OUTRANKS THE TARGET QUESTION, for both controls: it is a fact about the
    // MAKER, and it is the one this application has to hold a press back for.
    CHECK(action_availability(kActionCreate, true, true) == Availability::kDraftLive);
    CHECK(action_availability(kActionDelete, true, true) == Availability::kDraftLive);
    CHECK(action_availability(kActionDelete, true, false) == Availability::kDraftLive);
    CHECK(available(Availability::kAvailable));
    CHECK_FALSE(available(Availability::kNoTarget));
    CHECK_FALSE(available(Availability::kDraftLive));

    // AND THE DOCUMENT OVERLOAD ASKS BY IDENTITY. A selection that has outlived its object is
    // exactly the state `delete_selected` refuses in, so a control reading the raw number
    // would offer a press the document has already decided against.
    WorkshopDoc d = one_object();
    Session s;
    s.selected = 1;
    refocus(d, s);
    CHECK(action_availability(kActionDelete, d, s) == Availability::kAvailable);
    s.selected = 4242; // an identity this document does not have
    CHECK(action_availability(kActionDelete, d, s) == Availability::kNoTarget);
    s.selected = 0;
    CHECK(action_availability(kActionDelete, d, s) == Availability::kNoTarget);
    CHECK(action_availability(kActionCreate, d, s) == Availability::kAvailable);
}

TEST_CASE("HD-8: unavailable is said in CHARACTERS, so a colourless medium reads it too") {
    // The brackets are the statement and the role is the second signal, which is the object
    // list's `> ` mark one run down and the same argument: a terminal has no ground to tint,
    // so the difference may not live in a Skin's ink.
    CHECK(action_row_text(kActionCreate, true, 40) == "[ Create ]");
    CHECK(action_row_text(kActionCreate, false, 40) == "( Create )");
    CHECK(action_row_text(kActionDelete, true, 40) == "[ Delete ]");
    CHECK(action_row_text(kActionDelete, false, 40) == "( Delete )");
    // THE TWO STATES DIFFER AS TEXT, which is the whole claim, and they are the same WIDTH so
    // nothing under them moves when one changes.
    CHECK(action_row_text(kActionDelete, true, 40) != action_row_text(kActionDelete, false, 40));
    CHECK(action_row_text(kActionDelete, true, 40).size() ==
          action_row_text(kActionDelete, false, 40).size());
    // AND A CONTROL IS FITTED LIKE EVERY OTHER ROW OF THIS BODY.
    CHECK(action_row_text(kActionCreate, true, 6) == "[ C...");
    CHECK(action_row_text(kActionCreate, true, 0).empty());

    // ON THE CANVAS, in both media, an unavailable control is muted and an available one is
    // not -- the panel's existing roles, with nothing added and nothing widened.
    for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
        CAPTURE(line);
        Sample p = panel_of(0, 0, 80, 38, line == 0 ? 0 : 8, line);
        const InfoBodyPlace body = body_of(p.d, p.s);
        const surface::SurfaceCanvas c = paint(p.d, p.s);
        const surface::SurfaceTextRegion* shown = body_on(c, body);
        REQUIRE(shown != nullptr);
        const surface::SurfaceTextRow& create_row =
            shown->rows[static_cast<std::size_t>(prose_row_of_action(body, kActionCreate))];
        const surface::SurfaceTextRow& delete_row =
            shown->rows[static_cast<std::size_t>(prose_row_of_action(body, kActionDelete))];
        CHECK(create_row.text == "[ Create ]");
        CHECK(create_row.role == surface::role::kFill);
        CHECK(delete_row.text == "( Delete )"); // an empty document has nothing to delete
        CHECK(delete_row.role == surface::role::kMuted);
        // AND SINCE HD-9 THE AVAILABLE ONE SITS ON SOMETHING AND THE UNAVAILABLE ONE DOES
        // NOT. The text above is what HD-8 proved and it is unchanged by that: a medium with
        // no ground at all still reads the two states apart, which is why this case still
        // asserts the characters first. (HD-8 recorded here that the field was unused by this
        // repository; that was not so even then -- the Terminal's completion list has set its
        // selected row on `kMuted` since HD-2. What HD-9 changed is that the Info panel
        // spends it too.)
        CHECK(create_row.background == surface::role::kMuted);
        CHECK(delete_row.background == surface::role::kNone);
    }
}

TEST_CASE("HD-8: pressing Create is the SAME operation the `n` key performs") {
    // Two gestures, one meaning: the pointer path and the key path are driven separately from
    // identical starts and their whole outcome is compared -- document bytes, selection,
    // inspector, notice.
    Live keyed;
    keyed.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    keyed.key(input::scan::kN);

    Live pressed;
    pressed.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    const InfoBodyPlace body = body_place(pressed);
    const std::int64_t at = prose_row_of_action(body, kActionCreate);
    REQUIRE(at != kNoProseRow);
    pressed.press_at(body_pixel_x(body, 3), body_pixel_y(body, at), input::space::kPixels);

    CHECK(persist::to_text(pressed.doc()) == persist::to_text(keyed.doc()));
    CHECK(pressed.session().selected == keyed.session().selected);
    CHECK(pressed.notice() == keyed.notice());
    CHECK(pressed.notice() == "created #3 -- a new identity, not a new name");
    CHECK(pressed.row("Identity")->value() == keyed.row("Identity")->value());
    CHECK(pressed.doc().elements.size() == 3); // the boot document's two, plus exactly one
}

TEST_CASE("HD-8: pressing Delete is the SAME operation the `d` key performs") {
    // Same shape as Create's, at a document with something to delete and a selection the
    // post-delete rule has to move.
    Live keyed;
    Live pressed;
    for (int pass = 0; pass < 2; ++pass) {
        Live& t = pass == 0 ? keyed : pressed;
        t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
        for (int i = 0; i < 3; ++i) {
            t.key(input::scan::kN);
        }
        t.key(input::scan::kTab); // wrap onto #1, so the selection has somewhere to go
        REQUIRE(t.session().selected == 1);
        if (pass == 0) {
            t.key(input::scan::kD);
        } else {
            const InfoBodyPlace body = body_place(t);
            const std::int64_t at = prose_row_of_action(body, kActionDelete);
            REQUIRE(at != kNoProseRow);
            t.press_at(body_pixel_x(body, 3), body_pixel_y(body, at), input::space::kPixels);
        }
    }

    CHECK(persist::to_text(pressed.doc()) == persist::to_text(keyed.doc()));
    CHECK(pressed.session().selected == keyed.session().selected);
    CHECK(pressed.notice() == keyed.notice());
    CHECK(pressed.notice() == "deleted #1 -- now on #2");
    CHECK(pressed.doc().elements.size() == 4);
    CHECK(pressed.row("Identity")->value() == "#2");
}

TEST_CASE("HD-8: an unavailable Delete presents as unavailable and mutates nothing") {
    // The control stays VISIBLE with nothing to delete -- a maker has to be able to discover
    // that the act exists -- and reads as not pressable before they try it.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    for (int guard = 0; guard < 40 && !t.doc().elements.empty(); ++guard) {
        t.key(input::scan::kD);
    }
    REQUIRE(t.doc().elements.empty());
    REQUIRE(t.session().selected == 0);

    const InfoBodyPlace body = body_place(t);
    const surface::SurfaceTextRegion* shown = body_region(t.canvases.back(), body);
    REQUIRE(shown != nullptr);
    // STILL THERE, and saying which of the two it is.
    CHECK(shown->rows[static_cast<std::size_t>(prose_row_of_action(body, kActionCreate))].text ==
          "[ Create ]");
    CHECK(shown->rows[static_cast<std::size_t>(prose_row_of_action(body, kActionDelete))].text ==
          "( Delete )");
    CHECK(action_availability(kActionDelete, t.doc(), t.session()) == Availability::kNoTarget);

    // AND A PRESS ON IT CHANGES NOTHING. The refusal comes from the DOCUMENT, in the
    // document's own words -- the same sentence `d` gets, because it is the same call.
    const std::string before = persist::to_text(t.doc());
    const std::int64_t at = prose_row_of_action(body, kActionDelete);
    t.press_at(body_pixel_x(body, 3), body_pixel_y(body, at), input::space::kPixels);
    CHECK(persist::to_text(t.doc()) == before);
    CHECK(t.doc().elements.empty());
    CHECK(t.session().selected == 0);
    const std::string by_pointer = t.notice();
    t.key(input::scan::kD);
    CHECK(by_pointer == t.notice()); // one state, one sentence, two gestures

    // AND CREATE IS STILL REACHABLE FROM THE EMPTY DOCUMENT, which is the only way out of
    // this state with a pointer.
    const InfoBodyPlace now = body_place(t);
    t.press_at(body_pixel_x(now, 3),
               body_pixel_y(now, prose_row_of_action(now, kActionCreate)),
               input::space::kPixels);
    CHECK(t.doc().elements.size() == 1);
}

TEST_CASE("HD-8: a live property draft survives both controls, with no implicit commit") {
    // The phase's critical interaction case. A nontrivial draft (a moved caret, a scrolled
    // window) meets each control in turn and comes back untouched, and the reason is on the
    // notice line.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.begin_editing("Name");
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : kLongValue) {
        t.text(std::string(1, c));
    }
    for (int i = 0; i < 6; ++i) {
        t.key(input::scan::kLeft);
    }
    REQUIRE(t.row("Name")->editing());
    REQUIRE(t.row("Name")->editor().first_visible() > 0); // the window really has scrolled

    const std::string draft = t.row("Name")->draft();
    const std::size_t caret = t.row("Name")->editor().caret();
    const std::size_t window = t.row("Name")->editor().first_visible();
    const std::int64_t selected = t.session().selected;
    const std::size_t cursor = t.session().cursor;
    const std::size_t objects = t.doc().elements.size();
    const std::string document = persist::to_text(t.doc());

    for (const std::size_t which : {kActionCreate, kActionDelete}) {
        CAPTURE(which);
        // BOTH READ AS UNAVAILABLE while the draft is live -- visible and temporarily not
        // pressable, rather than hidden.
        const InfoBodyPlace body = body_place(t);
        CHECK(action_availability(which, t.doc(), t.session()) == Availability::kDraftLive);
        const surface::SurfaceTextRegion* shown = body_region(t.canvases.back(), body);
        REQUIRE(shown != nullptr);
        CHECK(shown->rows[static_cast<std::size_t>(prose_row_of_action(body, which))].text ==
              action_row_text(which, false, body.columns));

        const std::int64_t at = prose_row_of_action(body, which);
        t.press_at(body_pixel_x(body, 3), body_pixel_y(body, at), input::space::kPixels);

        CHECK(t.row("Name")->editing());
        CHECK(t.row("Name")->draft() == draft);
        CHECK(t.row("Name")->editor().caret() == caret);
        CHECK(t.row("Name")->editor().first_visible() == window);
        CHECK(t.session().selected == selected);
        CHECK(t.session().cursor == cursor);
        CHECK(t.doc().elements.size() == objects);
        CHECK(persist::to_text(t.doc()) == document);
        // THE SAME SENTENCE THE OBJECT LIST SAYS, because it is the same wall and one
        // constant (HD-7 wrote it; HD-8 named it).
        CHECK(t.notice() == "finish the draft first -- enter commits it, esc cancels");
    }

    // ESC RESOLVES THE DRAFT AND THE CONTROLS COME BACK -- nothing about the draft was
    // resolved for the maker, and the way out is the way it always was.
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.row("Name")->editing());
    CHECK(action_availability(kActionCreate, t.doc(), t.session()) == Availability::kAvailable);
    CHECK(action_availability(kActionDelete, t.doc(), t.session()) == Availability::kAvailable);
    const InfoBodyPlace free_now = body_place(t);
    const surface::SurfaceTextRegion* after = body_region(t.canvases.back(), free_now);
    REQUIRE(after != nullptr);
    CHECK(after->rows[static_cast<std::size_t>(prose_row_of_action(free_now, kActionCreate))]
              .text == "[ Create ]");
    t.press_at(body_pixel_x(free_now, 3),
               body_pixel_y(free_now, prose_row_of_action(free_now, kActionCreate)),
               input::space::kPixels);
    CHECK(t.doc().elements.size() == objects + 1);

    // AND A COMMITTED DRAFT DOES THE SAME. `Return` writes the value, and the controls are
    // live again immediately after it.
    t.begin_editing("Name");
    t.text("x");
    CHECK(action_availability(kActionCreate, t.doc(), t.session()) == Availability::kDraftLive);
    t.key(input::scan::kReturn);
    CHECK_FALSE(t.row("Name")->editing());
    CHECK(action_availability(kActionCreate, t.doc(), t.session()) == Availability::kAvailable);
}

TEST_CASE("HD-8: availability is not a prediction of what the document will say") {
    // The third classification, found in source rather than in the prompt: `doc::remove`
    // refuses an object something else measures against. That is the DOCUMENT's policy, so
    // Delete stays AVAILABLE, the press goes through, and the refusal arrives in the
    // document's own words -- byte-for-byte what pressing `d` does. A control that predicted
    // it would be a second copy of that policy, re-run on every paint.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    t.key(input::scan::kN); // #3, which will take its context from #1
    REQUIRE(t.session().selected == 3);
    t.begin_editing("Context");
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    t.type_line("#1");
    REQUIRE(t.row("Context")->value() == "#1");

    t.key(input::scan::kTab); // onto #1, which #3 now depends on
    REQUIRE(t.session().selected == 1);
    CHECK(action_availability(kActionDelete, t.doc(), t.session()) == Availability::kAvailable);
    const InfoBodyPlace body = body_place(t);
    const surface::SurfaceTextRegion* shown = body_region(t.canvases.back(), body);
    REQUIRE(shown != nullptr);
    CHECK(shown->rows[static_cast<std::size_t>(prose_row_of_action(body, kActionDelete))].text ==
          "[ Delete ]");

    const std::string before = persist::to_text(t.doc());
    t.press_at(body_pixel_x(body, 3),
               body_pixel_y(body, prose_row_of_action(body, kActionDelete)),
               input::space::kPixels);
    CHECK(persist::to_text(t.doc()) == before); // refused, and nothing was written
    CHECK(t.notice() == "#3 takes context from #1 -- change or delete it first");
}

TEST_CASE("HD-8: a press on a control never reaches the object list or the workspace") {
    // One gesture, one semantic owner. A press that fell through would put the panel's own
    // `... is here` sentence on the notice line in place of the operation's own answer.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    for (int i = 0; i < 4; ++i) {
        t.key(input::scan::kN);
    }

    for (const std::size_t which : {kActionCreate, kActionDelete}) {
        CAPTURE(which);
        const InfoBodyPlace now = body_place(t);
        const std::size_t before = t.doc().elements.size();
        t.press_at(body_pixel_x(now, 3),
                   body_pixel_y(now, prose_row_of_action(now, which)), input::space::kPixels);
        // THE ACT HAPPENED...
        CHECK(t.doc().elements.size() == (which == kActionCreate ? before + 1 : before - 1));
        // ...AND NOTHING ELSE DID: no drag began, and the notice is the operation's own.
        CHECK_FALSE(t.session().drag.active);
        CHECK(t.notice().find("is here") == std::string::npos);
        CHECK(t.notice().find("holding #") == std::string::npos);
        CHECK(t.notice().find("selected #") == std::string::npos);
    }
}

TEST_CASE("HD-8: a cell medium's press takes the other branch and lands on the same control") {
    // A terminal reports a character position and a window reports a pixel; the two go down
    // different arms of `prose_at` and must name the same control. Driving only the pixel arm
    // would leave the TUI's own answer unproven.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 0, 0}));
    const InfoBodyPlace body = body_place(t);
    REQUIRE_FALSE(body.fit.graphical());
    const std::int64_t at = prose_row_of_action(body, kActionCreate);
    REQUIRE(at != kNoProseRow);

    const std::size_t before = t.doc().elements.size();
    t.press_at(body.region_x + 3, body.region_y + at + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.doc().elements.size() == before + 1);
    CHECK(t.notice().rfind("created #", 0) == 0);
}

TEST_CASE("HD-8: the graphical press is not rounded to a Workshop cell") {
    // A body row is eighteen device pixels against a twelve-pixel cell, so a press resolved
    // through cells names the wrong row for most of the body -- and the footer, at the very
    // bottom, is where that error is largest. Asserted at the pixel it presses.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.fit.graphical());
    const std::int64_t at = prose_row_of_action(body, kActionCreate);
    const std::int64_t y = body_pixel_y(body, at);
    const std::int64_t cell_row = y / surface::kCanvasCellPx - body.region_y;
    CHECK(cell_row != at); // the two genuinely disagree here

    const std::size_t before = t.doc().elements.size();
    t.press_at(body_pixel_x(body, 3), y, input::space::kPixels);
    CHECK(t.doc().elements.size() == before + 1);
}

TEST_CASE("HD-8: the terminal overlay still owns the pointer entirely") {
    // The first rule of the routing order, re-proven now that a third thing inside the Info
    // panel wants presses. The overlay is a MODE: it takes every pointer event anywhere.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    const InfoBodyPlace body = body_place(t);
    const std::int64_t at = prose_row_of_action(body, kActionCreate);
    t.toggle_terminal();
    REQUIRE(t.session().terminal.open);

    const std::size_t before = t.doc().elements.size();
    t.press_at(body_pixel_x(body, 3), body_pixel_y(body, at), input::space::kPixels);
    CHECK(t.doc().elements.size() == before); // the mode had it; nothing was created

    t.toggle_terminal();
    const InfoBodyPlace now = body_place(t);
    t.press_at(body_pixel_x(now, 3),
               body_pixel_y(now, prose_row_of_action(now, kActionCreate)),
               input::space::kPixels);
    CHECK(t.doc().elements.size() == before + 1); // and closing it restores the gesture
}

TEST_CASE("HD-8: growing the panel gives the lists more room and the footer exactly two rows") {
    // A control asks for the room it needs. Nothing here stretches, and the spare room a tall
    // panel has stays spare -- it simply falls between the properties and the footer.
    std::size_t objects = 0;
    std::size_t properties = 0;
    for (const std::int64_t h : std::vector<std::int64_t>{22, 30, 40, 55, 70, 77}) {
        CAPTURE(h);
        Sample p = panel_of(20, 0, 80, h, 8, 18);
        const InfoBodyPlace body = body_of(p.d, p.s);
        REQUIRE(body.present);
        CHECK(body.objects_rows >= objects);       // monotonic...
        CHECK(body.properties_rows >= properties); // ...on both
        CHECK(static_cast<std::int64_t>(body.capacity) - body.action_row ==
              static_cast<std::int64_t>(kActionRows)); // ...and the footer never grows
        objects = body.objects_rows;
        properties = body.properties_rows;
    }

    // AND AT THE TALLEST, THE SPARE ROOM IS REAL AND IT IS SPARE.
    Sample tall = panel_of(2, 0, 240, 77);
    const InfoBodyPlace body = body_of(tall.d, tall.s);
    const std::size_t spent = body.objects_rows + 1 + body.properties_rows;
    REQUIRE(body.capacity > spent + kActionRows + 40);
    const surface::SurfaceCanvas c = paint(tall.d, tall.s);
    const surface::SurfaceTextRegion* shown = body_on(c, body);
    REQUIRE(shown != nullptr);
    for (std::size_t i = spent; i < static_cast<std::size_t>(body.action_row); ++i) {
        CAPTURE(i);
        CHECK(shown->rows[i].text.empty());
    }
}

TEST_CASE("HD-8: a resize moves the footer and changes no document and no draft") {
    // The footer is derived from the capacity every paint, so a new extent moves it with no
    // path of its own -- and a window dragged is still not an edit.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 22, 8, 18}));
    t.key(input::scan::kN);
    t.begin_editing("Name");
    t.text("q");
    const std::string draft = t.row("Name")->draft();
    const std::string document = persist::to_text(t.doc());
    const std::int64_t small = body_place(t).action_row;

    t.publish(loom::to_value(surface::SurfaceExtent{140, 60, 8, 18}));
    const InfoBodyPlace big = body_place(t);
    CHECK(big.action_row > small); // a taller body puts the footer lower
    CHECK(big.action_row + static_cast<std::int64_t>(kActionRows) ==
          static_cast<std::int64_t>(big.capacity));
    CHECK(t.row("Name")->editing());
    CHECK(t.row("Name")->draft() == draft);
    CHECK(persist::to_text(t.doc()) == document);
}

// ============================================================================
// QR-2 — a press is consumed by the layer that owns what it means
// ============================================================================
//
// INT-R0 measured one defect in this chain and one duplication under it, and these cases are
// both. `info_press` answered *the caret MOVED* where its caller asks *did you CONSUME this
// press*, and the two agree for exactly as long as every press that lands on the draft also
// moves it -- which is to say until a maker presses where the caret already is. Measured on
// the pristine tree, that press fell through the property editor, the controls and the object
// list and was answered by the panel with `Info is here -- nothing under it can be taken hold
// of`, written over a notice the maker was still reading.
//
// THE CONTRACT AT EVERY BOOL OF THE PRESS CHAIN IS NOW ONE SENTENCE: true means consumed, stop
// routing; false means not consumed, carry on. Nothing about acceptance, nothing about
// success, and nothing about anything having changed --
//
//     A CONSUMED PRESS DOES NOT HAVE TO CHANGE ANYTHING.
//     IT ONLY HAS TO HAVE REACHED THE LAYER THAT OWNS WHAT THE PRESS MEANS.
//
// `terminal_press` is deliberately NOT part of that contract and is not unified with it: its
// bool is *is a repaint owed*, and consumption there was already decided one layer up by the
// MODE (HD-3's `session_.terminal.open`). Two questions with two answers each are not one
// question.

TEST_CASE("QR-2: a press where the caret already is is CONSUMED, and the panel never answers") {
    // THE REPRODUCTION, AND THE STOP CONDITION. Everything the maker can see is unchanged by
    // this press -- and that is precisely why the old bit got it wrong.
    Live t;
    t.begin_editing("Name");
    const InfoBodyPlace place = body_place(t);
    REQUIRE(place.present);
    const std::int64_t at_row = editing_prose_row(t, place);
    REQUIRE(at_row != kNoProseRow);
    const std::int64_t value_x = place.region_x + kPropertyMarkCols + kPropertyLabelCols;
    const std::int64_t y = place.region_y + at_row + surface::kTuiCanvasTopRow;

    // The caret at a known column, put there by the same gesture this case is about.
    t.press_at(value_x + 2, y, input::space::kCells);
    REQUIRE(t.row("Name")->editor().caret() == 2);

    // A NOTICE WHOSE PRESERVATION IS OBSERVABLE, and one this press did not write: a press on
    // empty workspace is the LAST thing in the chain, so a sentence it left behind is proof
    // that a later press reached nothing further along than the draft.
    t.press(kWorkspaceW - 1, kWorkspaceH - 1);
    REQUIRE(t.notice() == "nothing there");
    REQUIRE(t.row("Name")->editing()); // and it took no hands off the draft
    const std::string document = persist::to_text(t.doc());
    const std::int64_t selected = t.session().selected;

    // THE PRESS THIS PHASE EXISTS FOR: the same column of the same row, with the caret
    // already on it.
    t.press_at(value_x + 2, y, input::space::kCells);

    CHECK(t.row("Name")->editor().caret() == 2);  // the caret did not move...
    CHECK(t.row("Name")->editing());              // ...the draft is still live...
    CHECK(t.row("Name")->draft() == "panel");     // ...and unedited...
    CHECK(persist::to_text(t.doc()) == document); // ...nothing was authored...
    CHECK(t.session().selected == selected);      // ...nothing was selected...
    CHECK_FALSE(t.session().drag.active);         // ...no gesture began...
    CHECK(t.notice() == "nothing there");         // ...and NOTHING was said over the line.

    // Before QR-2 the line above read `Info is here -- nothing under it can be taken hold of`:
    // the press fell past `info_press` because the caret had not moved, past the controls and
    // the object list because it is on neither, and into the panel's occupancy answer.
    CHECK(t.notice().find("nothing under it can be taken hold of") == std::string::npos);
}

TEST_CASE("QR-2: consumed and not-consumed are told apart by WHERE, not by what changed") {
    // The two halves of the contract in one case: a press that moves the caret and a press
    // that does not are the SAME answer to the routing question, and a press one row off the
    // draft is the other answer -- which is the fall-through the repair had to keep.
    Live t;
    t.begin_editing("Name");
    const InfoBodyPlace place = body_place(t);
    REQUIRE(place.present);
    const std::int64_t at_row = editing_prose_row(t, place);
    REQUIRE(at_row != kNoProseRow);
    const std::int64_t value_x = place.region_x + kPropertyMarkCols + kPropertyLabelCols;
    const std::int64_t y = place.region_y + at_row + surface::kTuiCanvasTopRow;

    t.press(kWorkspaceW - 1, kWorkspaceH - 1);
    REQUIRE(t.notice() == "nothing there");

    // MOVING THE CARET IS CONSUMED, and says nothing -- the caret is the statement.
    t.press_at(value_x + 1, y, input::space::kCells);
    CHECK(t.row("Name")->editor().caret() == 1);
    CHECK(t.notice() == "nothing there");

    // NOT MOVING IT IS THE SAME ANSWER.
    t.press_at(value_x + 1, y, input::space::kCells);
    CHECK(t.row("Name")->editor().caret() == 1);
    CHECK(t.notice() == "nothing there");

    // A ROW OF THIS PANEL THAT IS NOT THE DRAFT'S IS NOT CONSUMED, and the panel answers it
    // exactly as it always has. Asserted to be a row no other run of the body claims, so what
    // this measures is the property editor declining rather than the footer or the list
    // taking it.
    const std::int64_t elsewhere = at_row + 3;
    REQUIRE(object_at_prose_row(place, elsewhere) == kNoObject);
    REQUIRE(action_at_prose_row(place, elsewhere) == kNoAction);
    REQUIRE(property_at_prose_row(place, elsewhere) != editing_index(t));
    t.press_at(value_x + 2, place.region_y + elsewhere + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
    CHECK(t.row("Name")->editor().caret() == 1); // and the caret did not move
    CHECK(t.row("Name")->editing());
}

TEST_CASE("QR-2: a press on the ALREADY selected object row is deliberately not consumed") {
    // THE CONTRAST INT-R0 FOUND, PINNED FOR THE FIRST TIME. `objects_press` has the shape
    // `info_press` had -- a press that lands squarely on it and changes nothing -- and here it
    // is a decision: there is nothing for this list to do with the press, so it goes through
    // and the maker gets the panel's sentence rather than silence. Naming the bit did not
    // merge the two sites; it made this one legible as a choice.
    Live t;
    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.present);
    std::size_t at = 0;
    while (at < t.doc().elements.size() && t.doc().elements[at].id != t.session().selected) {
        ++at;
    }
    REQUIRE(at < t.doc().elements.size());
    const std::int64_t row = prose_row_of_object(body, at);
    REQUIRE(row != kNoProseRow);

    t.press(kWorkspaceW - 1, kWorkspaceH - 1);
    REQUIRE(t.notice() == "nothing there");
    const std::int64_t selected = t.session().selected;

    t.press_at(body.region_x + 3, body.region_y + row + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.session().selected == selected); // it selects nothing, because it is already there
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.session().drag.active); // and the panel stopped it before the workspace
}

TEST_CASE("QR-2: the body's resolve-and-locate is ONE answer, and it is the painter's") {
    // `info_body_at` is the six lines `info_press`, `actions_press` and `objects_press` each
    // carried. It answers WHERE and nothing about what that means, and it resolves the body
    // through the same `bounds_of` + `info_body_place` the painter published -- which is what
    // this case measures, against the region actually on the canvas rather than against a
    // second call of the same formula.
    Live t;
    t.press(kWorkspaceW - 1, kWorkspaceH - 1); // any gesture, so there is a canvas to read
    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.present);
    REQUIRE_FALSE(t.canvases.empty());
    const surface::SurfaceTextRegion* shown = body_region(t.canvases.back(), body);
    REQUIRE(shown != nullptr);

    const InfoBodyAt where =
        info_body_at(t.doc(), t.session(), input::space::kCells, body.region_x + 3,
                     body.region_y + 1 + surface::kTuiCanvasTopRow);
    CHECK(where.present);
    CHECK(where.body.region_x == shown->x); // the geometry the maker is looking at
    CHECK(where.body.region_y == shown->y);
    CHECK(where.body.capacity == body.capacity);
    CHECK(where.body.action_row == body.action_row);
    CHECK(where.at.column == 3); // and located in ITS prose, not in cells of the screen
    CHECK(where.at.row == 1);

    // `present` IS THE CONJUNCTION, and it is one bit because it is one fact about the PRESS:
    // it named nothing in this body. A `space` this application does not recognise is a
    // question it did not hear...
    CHECK_FALSE(info_body_at(t.doc(), t.session(), input::space::kUnknown, body.region_x + 3,
                             body.region_y + 1 + surface::kTuiCanvasTopRow)
                    .present);
    // ...and a panel a maker has removed has no body to press. (The third arm -- a panel with
    // no room for a body -- is `info_body_place`'s own refusal, pinned by HD-6.)
    Session closed = t.session();
    (void)close_panel(closed.panels, panel::kInfo);
    CHECK_FALSE(info_body_at(t.doc(), closed, input::space::kCells, body.region_x + 3,
                             body.region_y + 1 + surface::kTuiCanvasTopRow)
                    .present);
}

TEST_CASE("QR-2: no press inside the Info body begins a workspace gesture, on any row") {
    // Every prose row of the body, including the two control rows and the object list, while
    // a draft is live: no drag, no resize, no selection, nothing authored, and the draft still
    // in the maker's hands at the end of it.
    Live t;
    t.begin_editing("Name");
    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.present);
    REQUIRE(body.capacity > 0);
    const std::string document = persist::to_text(t.doc());
    const std::int64_t selected = t.session().selected;

    for (std::size_t row = 0; row < body.capacity; ++row) {
        t.press_at(body.region_x + 3,
                   body.region_y + static_cast<std::int64_t>(row) + surface::kTuiCanvasTopRow,
                   input::space::kCells);
        CHECK_FALSE(t.session().drag.active);
        CHECK(t.session().selected == selected);
        CHECK(persist::to_text(t.doc()) == document);
    }
    CHECK(t.row("Name")->editing()); // and every refusal along the way left the draft alone
    CHECK(t.row("Name")->draft() == "panel");
}

// ---- HD-9: the Info panel's structural rows get a GROUND -----------------------------------
//
// Two consumers of `SurfaceTextRow::background`, one panel: the available action controls and
// the `PROPERTIES` heading. Everything below asserts the SEMANTIC row values a publisher
// produces -- not a renderer's pixels and not a screenshot -- because the ground travels
// unresolved and each medium answers for itself (`project_text_regions` keeps it, the SDL plan
// resolves it against the region's own).

TEST_CASE("HD-9: an available control sits on a ground and an unavailable one does not") {
    // BOTH MEDIA, because the row is the publisher's answer and the publisher has one. The
    // first extent is a cell medium (no metric), the second a graphical one.
    for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
        CAPTURE(line);
        // A document with something to delete: both controls available.
        Sample p = panel_of(3, 0, 80, 38, line == 0 ? 0 : 8, line);
        const InfoBodyPlace body = body_of(p.d, p.s);
        const surface::SurfaceCanvas c = paint(p.d, p.s);
        const surface::SurfaceTextRegion* shown = body_on(c, body);
        REQUIRE(shown != nullptr);
        for (const std::size_t which : {kActionCreate, kActionDelete}) {
            const surface::SurfaceTextRow& row =
                shown->rows[static_cast<std::size_t>(prose_row_of_action(body, which))];
            CHECK(row.text == action_row_text(which, true, body.columns));
            CHECK(row.role == surface::role::kFill);
            CHECK(row.background == surface::role::kMuted);
        }

        // AND WITH NOTHING TO DELETE, the one that cannot run loses the ground rather than
        // being handed a quieter one. That is the whole of what makes a ground mean
        // "actionable": availability is not a matter of degree here.
        Sample empty = panel_of(0, 0, 80, 38, line == 0 ? 0 : 8, line);
        const InfoBodyPlace eb = body_of(empty.d, empty.s);
        const surface::SurfaceCanvas ec = paint(empty.d, empty.s);
        const surface::SurfaceTextRegion* e = body_on(ec, eb);
        REQUIRE(e != nullptr);
        const surface::SurfaceTextRow& create_row =
            e->rows[static_cast<std::size_t>(prose_row_of_action(eb, kActionCreate))];
        const surface::SurfaceTextRow& delete_row =
            e->rows[static_cast<std::size_t>(prose_row_of_action(eb, kActionDelete))];
        CHECK(create_row.background == surface::role::kMuted);
        CHECK(delete_row.background == surface::role::kNone);
        // THE TEXT STILL CARRIES IT ALONE, unchanged by HD-9 and asserted here rather than
        // only next door, because the ground is what could have tempted this to be dropped.
        CHECK(create_row.text == "[ Create ]");
        CHECK(delete_row.text == "( Delete )");
        // AND THE UNAVAILABLE CONTROL IS NOT SIMPLY MISSING: it is a row, in its own ink, in
        // exactly the place the available one would be.
        CHECK(delete_row.role == surface::role::kMuted);
    }
}

TEST_CASE("HD-9: a live draft takes the ground off BOTH controls, and gives it back") {
    // The other unavailability, and the one that moves both controls at once (`kDraftLive`).
    Live t;
    t.begin_editing("Name");
    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.present);
    const surface::SurfaceTextRegion* drafting = body_region(t.canvases.back(), body);
    REQUIRE(drafting != nullptr);
    for (const std::size_t which : {kActionCreate, kActionDelete}) {
        const surface::SurfaceTextRow& row =
            drafting->rows[static_cast<std::size_t>(prose_row_of_action(body, which))];
        CHECK(row.background == surface::role::kNone);
        CHECK(row.role == surface::role::kMuted);
        CHECK(row.text == action_row_text(which, false, body.columns));
    }
    // AND THE ACTIVE EDIT IS STILL THE LOUDEST THING IN THE PANEL. A ground on a control while
    // a draft is live would have put a second bright row beside the one the maker is typing
    // into; the availability rule and the ground rule agree here rather than competing.
    const surface::SurfaceTextRow& editing =
        drafting->rows[static_cast<std::size_t>(editing_prose_row(t, body))];
    CHECK(editing.role == surface::role::kAlert);
    CHECK(editing.background == surface::role::kNone);

    t.key(input::scan::kEscape); // cancel: the controls come back, ground and all
    const InfoBodyPlace after = body_place(t);
    const surface::SurfaceTextRegion* back = body_region(t.canvases.back(), after);
    REQUIRE(back != nullptr);
    CHECK(back->rows[static_cast<std::size_t>(prose_row_of_action(after, kActionCreate))]
              .background == surface::role::kMuted);
}

TEST_CASE("HD-9: `PROPERTIES` is set on a ground, and the row above it is not") {
    // THE HD-7 OBSERVATION, PINNED. The row immediately above the heading is the SELECTED
    // object, which carries accent ink too -- so before HD-9 the boundary and the thing it
    // was not were the same colour on adjacent rows. The ground is what tells them apart, and
    // this case asserts BOTH halves so a later phase cannot buy the heading's distinction by
    // grounding the selection as well.
    for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
        CAPTURE(line);
        Sample p = panel_of(3, 2, 80, 38, line == 0 ? 0 : 8, line);
        const InfoBodyPlace body = body_of(p.d, p.s);
        const surface::SurfaceCanvas c = paint(p.d, p.s);
        const surface::SurfaceTextRegion* shown = body_on(c, body);
        REQUIRE(shown != nullptr);
        const surface::SurfaceTextRow& heading =
            shown->rows[static_cast<std::size_t>(body.heading_row)];
        CHECK(heading.text == "PROPERTIES");
        CHECK(heading.role == surface::role::kAccent); // unchanged: the ink still says what
        CHECK(heading.background == surface::role::kMuted);

        const surface::SurfaceTextRow& selected =
            shown->rows[static_cast<std::size_t>(prose_row_of_object(body, 2))];
        CHECK(selected.role == surface::role::kAccent);
        CHECK(selected.background == surface::role::kNone);
        CHECK(selected.text.rfind("> ", 0) == 0); // and the mark is still what a cell reads
    }
}

TEST_CASE("HD-9: no other row of the body was given a ground") {
    // THE BLAST RADIUS, ASSERTED RATHER THAN INTENDED. Two consumers were earned; every other
    // row of this body -- object rows, property rows, both omission markers, the spare rows,
    // the two empty-state sentences -- shows whatever the region is sitting on, exactly as it
    // did before HD-9.
    for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
        CAPTURE(line);
        // Enough objects that the object list omits on both sides at the graphical minimum,
        // so the marker rows are really in the picture being asserted.
        Sample p = panel_of(20, 10, 78, 22, line == 0 ? 0 : 8, line);
        const InfoBodyPlace body = body_of(p.d, p.s);
        const surface::SurfaceCanvas c = paint(p.d, p.s);
        const surface::SurfaceTextRegion* shown = body_on(c, body);
        REQUIRE(shown != nullptr);
        REQUIRE(shown->rows.size() == body.capacity);
        std::size_t grounded = 0;
        for (std::size_t i = 0; i < shown->rows.size(); ++i) {
            CAPTURE(i);
            const std::size_t which = action_at_prose_row(body, static_cast<std::int64_t>(i));
            const bool structural =
                static_cast<std::int64_t>(i) == body.heading_row ||
                (which != kNoAction && available(action_availability(which, p.d, p.s)));
            if (structural) {
                CHECK(shown->rows[i].background == surface::role::kMuted);
                ++grounded;
            } else {
                CHECK(shown->rows[i].background == surface::role::kNone);
            }
        }
        // THE HEADING AND BOTH AVAILABLE CONTROLS, AND NOTHING ELSE.
        CHECK(grounded == 1 + kActionCount);
        CHECK(body.objects.before + body.objects.after > 0); // the markers were really there
        CHECK(body.properties.after > 0);
    }
}

TEST_CASE("HD-9: the ground reaches the whole row in a CELL medium, not just its characters") {
    // WHAT THE EXISTING SURFACE CONTRACT ACTUALLY BACKGROUNDS, measured through the shared
    // cell projection rather than assumed from the field's name. `project_one_text_region`
    // pads every row to the region's full width and carries the ground on the padding too, so
    // a character medium's answer to "this row, all of it" really is all of it.
    Sample p = panel_of(3, 0, 80, 38);
    const InfoBodyPlace body = body_of(p.d, p.s);
    const surface::SurfaceCanvas c = paint(p.d, p.s);
    const surface::SurfaceTextRegion* shown = body_on(c, body);
    REQUIRE(shown != nullptr);

    surface::SurfaceCanvas only;
    only.width = c.width;
    only.height = c.height;
    only.texts.push_back(*shown);
    const std::vector<surface::ProjectedRow> rows = surface::project_text_regions(only);
    REQUIRE(rows.size() == static_cast<std::size_t>(shown->h));

    const std::size_t heading = static_cast<std::size_t>(body.heading_row);
    CHECK(rows[heading].background == surface::role::kMuted);
    CHECK(rows[heading].label.text.size() == static_cast<std::size_t>(shown->w));
    CHECK(rows[heading].label.text.rfind("PROPERTIES", 0) == 0);
    CHECK(rows[heading].label.text.back() == ' '); // padded, and the ground rides the padding

    const std::size_t create =
        static_cast<std::size_t>(prose_row_of_action(body, kActionCreate));
    CHECK(rows[create].background == surface::role::kMuted);
    CHECK(rows[create].label.text.size() == static_cast<std::size_t>(shown->w));

    // AND THE TERMINAL REALLY EMITS IT. `sgr_bg_for_role(kMuted)` is the bright-black ground,
    // and this is the first Info-panel byte of it -- exactly three runs, one per grounded row,
    // because a grounded row is one role and one ground from the region's first column to its
    // last and the writer opens each run once.
    const std::string bytes = surface::canvas_body(c);
    std::size_t runs = 0;
    for (std::size_t at = bytes.find("\x1b[100m"); at != std::string::npos;
         at = bytes.find("\x1b[100m", at + 1)) {
        ++runs;
    }
    CHECK(runs == 1 + kActionCount);
    // AND NOTHING AFTER A GROUNDED ROW WEARS IT. There is no `\x1b[49m` here and that is not
    // an omission: the ground reaches the region's last column, and the writer restarts every
    // canvas row from no-role/no-ground, so the reset that ends the run is the row itself.
    // (`\x1b[49m` is the branch a ground FOLLOWED by ungrounded cells on the same row takes;
    // the Info body has none, and the Terminal's completion list is the case that does.)
    CHECK(bytes.find("\x1b[49m") == std::string::npos);
}

TEST_CASE("HD-9: the ground resolves to a real ink for a graphical medium, per row") {
    // THE OTHER MEDIUM'S ANSWER TO THE SAME PUBLISHED FACT. `plan_text_regions` resolves
    // `role::kNone` to the REGION's own ground, so "has a ground of its own" is spelled as
    // "differs from the region's" in the renderer -- which is what makes the absence an
    // absence rather than a second flag to keep in step.
    Sample p = panel_of(3, 0, 80, 38, 8, 18);
    const InfoBodyPlace body = body_of(p.d, p.s);
    REQUIRE(body.fit.graphical());
    const surface::SurfaceCanvas c = paint(p.d, p.s);
    const std::vector<surface::PlanTextRegion> plan = surface::plan_text_regions(
        c, surface::SurfaceExtent{80, 38, 8, 18},
        surface::PlanSize{80 * surface::kCanvasCellPx, 38 * surface::kCanvasCellPx});
    const surface::PlanTextRegion* planned = nullptr;
    for (const surface::PlanTextRegion& r : plan) {
        if (r.view.y == body.region_y * surface::kCanvasCellPx) {
            planned = &r;
        }
    }
    REQUIRE(planned != nullptr);
    REQUIRE(planned->rows.size() > static_cast<std::size_t>(body.action_row) + 1);

    const surface::PlanTextRow& heading =
        planned->rows[static_cast<std::size_t>(body.heading_row)];
    const surface::PlanTextRow& create =
        planned->rows[static_cast<std::size_t>(prose_row_of_action(body, kActionCreate))];
    CHECK(heading.background == surface::ink_for_role(surface::role::kMuted));
    CHECK(create.background == surface::ink_for_role(surface::role::kMuted));
    CHECK_FALSE(heading.background == planned->background); // so the strip is really drawn
    CHECK_FALSE(create.background == planned->background);
    // AND THE INK ON TOP IS STILL THE ROW'S OWN ROLE -- two independent fields, which is what
    // lets a grounded heading keep its accent and a grounded control keep its fill.
    CHECK(heading.ink == surface::ink_for_role(surface::role::kAccent));
    CHECK(create.ink == surface::ink_for_role(surface::role::kFill));

    // AN ORDINARY ROW RESOLVES TO THE REGION'S OWN GROUND and costs the renderer no strip.
    const surface::PlanTextRow& ordinary = planned->rows[0];
    CHECK(ordinary.background == planned->background);
}

TEST_CASE("HD-9: the grounded strip is exactly the prose row a press resolves to") {
    // THE HUMAN-FACTOR GUARD. A ground that looked like a larger target than the one that
    // answers would be presentation lying about interaction, so the two are compared as
    // NUMBERS: the strip the renderer fills for prose row `i` is
    // [origin_y + i*line_px, origin_y + (i+1)*line_px) local to the region's viewport, and
    // `prose_row_of_pixel` partitions the identical pixels. Every pixel of the slab, top edge
    // and bottom edge included, names the control it is drawn under.
    Sample p = panel_of(3, 0, 80, 38, 8, 18);
    const InfoBodyPlace body = body_of(p.d, p.s);
    REQUIRE(body.fit.graphical());
    for (const std::size_t which : {kActionCreate, kActionDelete}) {
        const std::int64_t row = prose_row_of_action(body, which);
        const std::int64_t top = body.region_y * surface::kCanvasCellPx + body.fit.origin_y +
                                 row * body.fit.line_px;
        for (const std::int64_t py :
             {top, top + body.fit.line_px / 2, top + body.fit.line_px - 1}) {
            CAPTURE(py);
            CHECK(surface::prose_row_of_pixel(py, body.region_y, body.fit) == row);
            CHECK(action_press_at(
                      body, 0, surface::prose_row_of_pixel(py, body.region_y, body.fit)) ==
                  which);
        }
        // AND ONE PIXEL PAST EITHER END OF THE STRIP IS THE NEIGHBOURING ROW, never this one.
        CHECK(surface::prose_row_of_pixel(top - 1, body.region_y, body.fit) == row - 1);
        CHECK(surface::prose_row_of_pixel(top + body.fit.line_px, body.region_y, body.fit) ==
              row + 1);
    }

    // HORIZONTALLY THE SLAB IS THE VIEWPORT AND THE TARGET IS THE COLUMNS INSIDE IT, and the
    // difference is exactly `kTextInsetPx` at the left end -- the margin `fit_region` has
    // always held back and which no glyph was ever drawn in either. It is asserted rather than
    // waved at, because the ground is the first thing to make it visible.
    const std::int64_t left = body.region_x * surface::kCanvasCellPx;
    CHECK(surface::prose_column_of_pixel(left, body.region_x, body.fit) == -1);
    CHECK(action_press_at(body, -1, body.action_row) == kNoAction);
    CHECK(surface::prose_column_of_pixel(left + surface::kTextInsetPx, body.region_x,
                                         body.fit) == 0);
    CHECK(action_press_at(body, 0, body.action_row) == kActionCreate);
    CHECK(action_press_at(body, body.fit.columns, body.action_row) == kActionCreate);
    CHECK(action_press_at(body, body.fit.columns + 1, body.action_row) == kNoAction);
}

TEST_CASE("HD-9: a ground changed no composition, no row index and no hit mapping") {
    // A STYLING PHASE MUST NOT BECOME A LAYOUT PHASE. Every number HD-7 and HD-8 established
    // is re-measured here across the extents those phases quoted, so a later reader can see
    // that the capacities did not move under the paint.
    struct Case {
        std::int64_t w, h, advance, line;
    };
    for (const Case& k : std::vector<Case>{{78, 22, 8, 18}, {78, 25, 8, 18}, {120, 40, 8, 18},
                                           {240, 80, 8, 18}, {80, 38, 0, 0}, {80, 70, 0, 0}}) {
        CAPTURE(k.w);
        CAPTURE(k.h);
        CAPTURE(k.line);
        Sample p = panel_of(6, 0, k.w, k.h, k.advance, k.line);
        const InfoBodyPlace body = body_of(p.d, p.s);
        REQUIRE(body.present);
        const surface::SurfaceCanvas c = paint(p.d, p.s);
        const surface::SurfaceTextRegion* shown = body_on(c, body);
        REQUIRE(shown != nullptr);

        // THE FOOTER IS STILL EXACTLY TWO ROWS, ANCHORED TO THE FOOT.
        CHECK(body.action_row == static_cast<std::int64_t>(body.capacity - kActionRows));
        CHECK(shown->rows.size() == body.capacity);
        // THE HEADING IS STILL WHERE THE SHARE PUT IT, and the three runs are still disjoint.
        CHECK(body.heading_row == static_cast<std::int64_t>(body.objects_rows));
        CHECK(body.objects_rows + 1 + body.properties_rows <= body.capacity - kActionRows);
        // AND THE INVERSES ARE STILL INVERSES ON EVERY ROW OF THE BODY.
        for (std::size_t which = 0; which < kActionCount; ++which) {
            CHECK(action_at_prose_row(body, prose_row_of_action(body, which)) == which);
        }
        CHECK(action_at_prose_row(body, body.heading_row) == kNoAction);
        CHECK(property_at_prose_row(body, body.heading_row) == kNoProperty);
        CHECK(object_at_prose_row(body, body.heading_row) == kNoObject);
    }
}

// ============================================================================================
// HD-10 — the reserved column is nobody's to spend
// ============================================================================================
//
// HD-9's live run recorded a defect it did not cause: with the Terminal open, the Info panel
// published its lower rows -- properties, and the whole `[ Create ]` / `[ Delete ]` footer --
// and the pane's region, later in `c.texts`, erased them. In canvas-coloured ground, so what a
// maker saw was a panel that STOPPED rather than a panel something was covering, and the three
// answers they could not tell apart were `omitted`, `hidden` and `destroyed`.
//
// WHERE THE OVERLAP CAME FROM, measured before anything was designed:
//
//     the Info panel's REGION is exactly its granted bounds less the heading row
//         (`info_body_place`: region_x = panel.x, region_w = panel.w) -- nothing escaped
//     the two RECTANGLES overlapped, at every extent this composition lays out
//         78x22   pane 56x13 at (22, 9)    side region 28x17 at (50, 0)   overlap 28 x 8
//         98x60   pane 66x32 at (32, 28)   side region 28x55 at (70, 0)   overlap 28 x 27
//         240x80  pane 137x42 at (103, 38) side region 28x75 at (212, 0)  overlap 28 x 37
//
// So it was never an internal presentation exceeding its grant, and never a rule about paint
// order: it was two rectangles claiming the same cells, and publication order was the only
// thing deciding which one a maker got to see.
//
// THE OWNER OF THAT CONFLICT IS `screen_of`, AND IT ALREADY KNEW THE ANSWER. It reserves the
// side column and hands the workspace `room_w = panel_x - kPanelGap` three lines before it
// places the pane; `placement_bounds` puts the overlay stack inside that same number and
// asserts it. The pane was the one placement in the file older than that discipline. HD-10 is
// two expressions: the pane's want is bounded by the room, and its right edge IS the room's.
//
// WHAT THIS IS NOT. It is not a rule that regions may not overlap -- three of them still do,
// every one on purpose and every one inside a single owner's room (the completion list over
// the pane's transcript, the picker over the stack slot beneath it, and the pane itself over
// the workspace and the bottom band). It is the narrower claim that the room reserved BESIDE
// the workspace is nobody's, which is the one overlap that had no owner and no reason.

namespace {

/// A screen and the two rectangles HD-10 is about, so a case can ask about their relationship
/// without either of them computing a rectangle for itself.
struct Places {
    Screen sc{};
    ui::Rect pane{};
    ui::Rect side{};
};
Places places_of(std::int64_t w, std::int64_t h, std::int64_t advance = 0,
                 std::int64_t line = 0) {
    Places p;
    p.sc = screen_of(w, h, advance, line);
    p.pane = ui::Rect{p.sc.terminal_x, p.sc.terminal_y, p.sc.terminal_w, p.sc.terminal_h};
    p.side = placement_bounds(placement::kSideRegion, 0, p.sc);
    return p;
}

/// How many CELLS two rectangles share. Zero is the answer HD-10 requires of the pane and the
/// side region; it is COUNTED rather than compared as edges so a case that gets the arithmetic
/// subtly wrong reports how badly rather than passing.
std::int64_t shared_cells(const ui::Rect& a, const ui::Rect& b) {
    std::int64_t n = 0;
    for (std::int64_t y = b.y; y < b.y + b.h; ++y) {
        for (std::int64_t x = b.x; x < b.x + b.w; ++x) {
            if (a.contains(x, y)) {
                ++n;
            }
        }
    }
    return n;
}

/// A document and a session with Info open and `n` objects, at a stated extent.
struct Room {
    WorkshopDoc d;
    Session s;
};
Room room_of(std::size_t n, std::int64_t w, std::int64_t h, std::int64_t advance = 0,
             std::int64_t line = 0) {
    Room r;
    for (std::size_t i = 0; i < n; ++i) {
        REQUIRE(doc::add_default(r.d) != 0);
    }
    adopt_screen(r.s, w, h, advance, line);
    if (!r.d.elements.empty()) {
        r.s.selected = r.d.elements.back().id;
    }
    refocus(r.d, r.s);
    return r;
}

/// The extents HD-10 measures over: the minimum, the widths where the room is narrower than
/// the pane's want, the width where the two first agree, and up to the largest surface this
/// composition lays out.
const std::vector<std::pair<std::int64_t, std::int64_t>> kHd10Extents = {
    {78, 22},  {79, 22},  {80, 23},  {81, 24},  {94, 30},  {98, 60},
    {100, 33}, {120, 40}, {160, 60}, {240, 80}, {640, 400}};

} // namespace

TEST_CASE("HD-10: the pane and the side region share no cell, at any extent or metric") {
    // THE PHASE'S WHOLE CLAIM, counted in cells rather than compared as edges -- the same
    // question a maker's eye asks, and the one the pristine tree answered with 224 shared
    // cells at the minimum extent and 1,036 at 240x80.
    for (const auto& wh : kHd10Extents) {
        for (const auto& metric : std::vector<std::pair<std::int64_t, std::int64_t>>{
                 {0, 0}, {8, 18}, {11, 23}}) {
            CAPTURE(wh.first);
            CAPTURE(wh.second);
            CAPTURE(metric.first);
            const Places p = places_of(wh.first, wh.second, metric.first, metric.second);
            CHECK(shared_cells(p.pane, p.side) == 0);
            // ...and the two ways of saying it agree, which is what stops a later placement
            // rule from satisfying the arithmetic while breaking the law.
            CHECK(p.pane.x + p.pane.w == p.sc.room_w);
            CHECK(p.pane.x + p.pane.w <= p.side.x - kPanelGap);
            CHECK(p.pane.x >= 0);
            // THE PANE IS NEVER NARROWER THAN THE WORKSPACE'S OWN FLOOR, so the clamp has no
            // degenerate end: the room it is bounded by is the room the workspace has.
            CHECK(p.pane.w >= kWorkspaceMinW);
        }
    }
}

TEST_CASE("HD-10: the want is unchanged and the room is the ceiling") {
    // G-2'S HALF-SHARE RULE SURVIVED. What HD-10 added is a ceiling, and the two agree from 94
    // columns up -- so the rule a maker experiences on any ordinary window is the rule G-2
    // wrote, and the ceiling is what happens on a surface with no half to give away.
    for (std::int64_t w = kScreenMinW; w <= 200; ++w) {
        CAPTURE(w);
        const Screen sc = screen_of(w, kScreenMinH);
        const std::int64_t want = kTerminalWantW + (w - kScreenMinW) / 2;
        CHECK(sc.terminal_w == (want < sc.room_w ? want : sc.room_w));
        CHECK(sc.terminal_w == (w < 94 ? sc.room_w : want));
    }
    // The extents where the ceiling actually binds, named, because "only the narrowest
    // screens" is a claim and these are the whole of it.
    CHECK(screen_of(78, 22).terminal_w == 48);
    CHECK(screen_of(79, 22).terminal_w == 49);
    CHECK(screen_of(80, 22).terminal_w == 50);
    CHECK(screen_of(81, 22).terminal_w == 51); // want 57, room 51
    CHECK(screen_of(94, 22).terminal_w == 64); // want 64, room 64 -- they meet
    CHECK(screen_of(95, 22).terminal_w == 64); // want 64, room 65 -- the want, from here on
}

TEST_CASE("HD-10: the reservation is the SCREEN's, and holds with no panel in it") {
    // THE TEST FOR THE WRONG OWNER, AS A CASE. If the repair had been "the pane must not cover
    // Info", closing Info would hand the pane the column back -- and the column is reserved
    // whether or not anything is standing in it (PNL-0's own decision, and the reason hiding
    // Info moves nothing). Nothing in `screen_of` can see a panel, and this says so.
    Session open_info;
    Session no_info;
    REQUIRE(close_panel(no_info.panels, panel::kInfo));
    REQUIRE(no_info.panels.open.empty());
    for (const auto& wh : kHd10Extents) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        adopt_screen(open_info, wh.first, wh.second, 0, 0);
        adopt_screen(no_info, wh.first, wh.second, 0, 0);
        const Screen a = screen_of(open_info);
        const Screen b = screen_of(no_info);
        CHECK(a.terminal_x == b.terminal_x);
        CHECK(a.terminal_w == b.terminal_w);
        CHECK(b.terminal_x + b.terminal_w == b.room_w);
    }
}

TEST_CASE("HD-10: the Info panel is byte-identical with the pane open") {
    // THE OTHER HALF OF THE OWNERSHIP ANSWER. The repair moved the PANE; it did not make the
    // panel reflow, shrink, or float its footer upwards. So the composition a maker reads on
    // the right is the same composition whether the Terminal is open or closed -- which is
    // what stops opening a mode from moving a target under the hand aiming at it.
    for (const auto& wh : kHd10Extents) {
        for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
            CAPTURE(wh.first);
            CAPTURE(wh.second);
            CAPTURE(line);
            Room closed = room_of(6, wh.first, wh.second, line == 0 ? 0 : 8, line);
            Room opened = room_of(6, wh.first, wh.second, line == 0 ? 0 : 8, line);
            opened.s.terminal.open = true;
            opened.s.terminal.attached = true;

            const InfoBodyPlace a = body_of(closed.d, closed.s);
            const InfoBodyPlace b = body_of(opened.d, opened.s);
            REQUIRE(a.present);
            REQUIRE(b.present);
            CHECK(a.region_x == b.region_x);
            CHECK(a.region_y == b.region_y);
            CHECK(a.region_w == b.region_w);
            CHECK(a.region_h == b.region_h);
            CHECK(a.capacity == b.capacity);
            CHECK(a.objects_rows == b.objects_rows);
            CHECK(a.properties_rows == b.properties_rows);
            CHECK(a.heading_row == b.heading_row);
            CHECK(a.action_row == b.action_row);

            // AND THE ROWS THEMSELVES, one by one, published on both canvases -- HD-9's
            // grounds included, so an occlusion repair cannot quietly regress them.
            const surface::SurfaceCanvas ca = paint(closed.d, closed.s);
            const surface::SurfaceCanvas cb = paint(opened.d, opened.s);
            const surface::SurfaceTextRegion* ra = body_on(ca, a);
            const surface::SurfaceTextRegion* rb = body_on(cb, b);
            REQUIRE(ra != nullptr);
            REQUIRE(rb != nullptr);
            REQUIRE(ra->rows.size() == rb->rows.size());
            for (std::size_t i = 0; i < ra->rows.size(); ++i) {
                CAPTURE(i);
                CHECK(ra->rows[i].text == rb->rows[i].text);
                CHECK(ra->rows[i].role == rb->rows[i].role);
                CHECK(ra->rows[i].background == rb->rows[i].background);
            }
        }
    }
}

TEST_CASE("HD-10: the footer a maker can SEE, with the pane open, on the real rasterizer") {
    // THE DEFECT ITSELF, AS A PICTURE. Everything above is arithmetic; this is the thing HD-9
    // photographed. The screen is rasterized by the terminal medium's own pure function, the
    // Info column is cut out of it, and the rows that used to be erased are read back.
    for (const auto& wh : kHd10Extents) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        Room r = room_of(4, wh.first, wh.second);
        r.s.terminal.open = true;
        r.s.terminal.attached = true;
        r.s.terminal.id = loom::WeaveId{3};
        const InfoBodyPlace body = body_of(r.d, r.s);
        REQUIRE(body.present);
        const surface::SurfaceCanvas c = paint(r.d, r.s);
        const std::vector<std::string> rows = rasterized(c);
        REQUIRE(rows.size() == static_cast<std::size_t>(screen_of(r.s).h));

        const auto column = [&rows, &body](std::int64_t prose_row) {
            const std::int64_t y = body.region_y + prose_row;
            return rows[static_cast<std::size_t>(y)].substr(
                static_cast<std::size_t>(body.region_x),
                static_cast<std::size_t>(body.region_w));
        };
        CHECK(column(prose_row_of_action(body, kActionCreate)).rfind("[ Create ]", 0) == 0);
        CHECK(column(prose_row_of_action(body, kActionDelete)).rfind("[ Delete ]", 0) == 0);
        CHECK(column(body.heading_row).rfind("PROPERTIES", 0) == 0);
        CHECK(column(0).find("panel") != std::string::npos);
        // ...AND NOT ONE ROW OF THE BODY IS BLANKED BY SOMETHING ELSE. Every row the region
        // published is the row the screen shows, which is the sentence the pristine tree
        // could not say for anything below the pane's top edge.
        const surface::SurfaceTextRegion* published = body_on(c, body);
        REQUIRE(published != nullptr);
        for (std::size_t i = 0; i < published->rows.size(); ++i) {
            CAPTURE(i);
            CHECK(column(static_cast<std::int64_t>(i)).rfind(published->rows[i].text, 0) == 0);
        }
    }
}

TEST_CASE("HD-10: what the pane DOES cover is unchanged, and is on purpose") {
    // THE CLASSIFICATION, MEASURED. Forbidding overlap globally would forbid three things this
    // application does deliberately, so the three are asserted as still true rather than left
    // to a reader to notice they survived.
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    t.toggle_terminal();
    const Screen sc = screen_of(t.session());

    // ONE: the pane covers the WORKSPACE, which is what an overlay over the material IS.
    const ui::Rect pane{sc.terminal_x, sc.terminal_y, sc.terminal_w, sc.terminal_h};
    const ui::Rect workspace{kWorkspaceX, kWorkspaceY, sc.room_w, sc.room_h};
    CHECK(shared_cells(pane, workspace) > 0);

    // TWO: the pane covers the bottom band, and the SCREEN answers for that by not painting
    // the notice or the help lines at all while it is open -- an occlusion with an owner, and
    // the precedent HD-10's own answer was modelled on.
    CHECK(sc.terminal_y + sc.terminal_h == sc.h);
    CHECK(label_at(t.canvases.back(), 0, sc.help_y).rfind("n new | d delete", 0) != 0);

    // THREE: the completion list covers the pane's own transcript, inside one owner's room.
    t.text("s");
    const surface::SurfaceCanvas& c = t.canvases.back();
    const surface::SurfaceTextRegion* list = list_of(c, sc);
    REQUIRE(list != nullptr);
    const surface::SurfaceTextRegion* body = pane_of(c, sc);
    REQUIRE(body != nullptr);
    const ui::Rect list_rect{list->x, list->y, list->w, list->h};
    const ui::Rect pane_rect{body->x, body->y, body->w, body->h};
    CHECK(shared_cells(list_rect, pane_rect) > 0);
    CHECK(list->x >= body->x);
    CHECK(list->x + list->w <= body->x + body->w);
    // ...and NONE of them reaches the side region.
    const ui::Rect side = placement_bounds(placement::kSideRegion, 0, sc);
    CHECK(shared_cells(list_rect, side) == 0);
    CHECK(shared_cells(pane_rect, side) == 0);
}

TEST_CASE("HD-10: the pane and the overlay stack meet on one row, at one height, and only there") {
    // THE ONE OVERLAP HD-10 LEAVES BETWEEN TWO INDEPENDENT PRESENTATIONS, measured exactly so
    // that it is a known fact rather than a later discovery. Both are OVERLAYS in the room the
    // workspace has: the stack grows down from the top-left, the pane up from the bottom-right,
    // and at the shortest screen this composition lays out their edges touch for one row.
    //
    // IT IS NOT REPAIRED HERE, and the reason is that repairing it means reserving the stack's
    // rows from the pane -- a SECOND reservation, which `screen_of` does not make and which
    // would tie the pane's height to `kStackRows`. The reserved side column is a subtraction
    // the screen already performs; the overlay stack is not. HD-10's report says the rest.
    for (std::int64_t h = kScreenMinH; h <= 40; ++h) {
        CAPTURE(h);
        const Places p = places_of(kScreenMinW, h);
        const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, p.sc);
        CHECK(shared_cells(p.pane, slot) == (h == kScreenMinH ? slot.w : 0));
    }
    // And the row they share is the stack slot's LAST row, which the Builder spends on
    // `[ Build ]` -- a label naming the key `b` rather than a control, since `[ Build ]` has
    // never been pressable (PNL-2 says so in its own words).
    const Places min = places_of(kScreenMinW, kScreenMinH);
    const ui::Rect slot = placement_bounds(placement::kOverlayStack, 0, min.sc);
    CHECK(min.pane.y == slot.y + slot.h - 1);
}

TEST_CASE("HD-10: the composition is DERIVED, and a resize recomputes all of it") {
    // Nothing is stored: no occlusion state, no remembered rectangle, no cached room. The
    // proof is that walking a live session through seven extents and back gives, at each one,
    // exactly what a freshly computed screen of that extent gives.
    Live t;
    (void)t.mount_skin_seat();
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const auto& wh : std::vector<std::pair<std::int64_t, std::int64_t>>{
             {78, 22}, {240, 80}, {94, 30}, {78, 22}, {160, 45}, {80, 23}, {78, 22}}) {
        CAPTURE(wh.first);
        CAPTURE(wh.second);
        t.publish(loom::to_value(surface::SurfaceExtent{wh.first, wh.second}));
        const Screen live = screen_of(t.session());
        const Places fresh = places_of(wh.first, wh.second);
        CHECK(live.terminal_x == fresh.sc.terminal_x);
        CHECK(live.terminal_w == fresh.sc.terminal_w);
        CHECK(live.terminal_y == fresh.sc.terminal_y);
        CHECK(live.terminal_h == fresh.sc.terminal_h);
        CHECK(shared_cells(fresh.pane, fresh.side) == 0);
        // AND THE PICTURE AGREES WITH THE PLACEMENT at every one of them: the pane's own
        // header is at the pane's corner and the panel's footer is readable beside it.
        const std::vector<std::string> rows = rasterized(t.canvases.back());
        REQUIRE(rows.size() == static_cast<std::size_t>(live.h));
        CHECK(rows[static_cast<std::size_t>(live.terminal_y)].substr(
                  static_cast<std::size_t>(live.terminal_x), 8) == "TERMINAL");
        const InfoBodyPlace body = body_of(t.doc(), t.session());
        REQUIRE(body.present);
        CHECK(rows[static_cast<std::size_t>(body.region_y +
                                            prose_row_of_action(body, kActionCreate))]
                  .substr(static_cast<std::size_t>(body.region_x), 10) == "[ Create ]");
    }
}

TEST_CASE("HD-10: the same composition truth in a medium that sets type") {
    // THE ANSWER IS MEDIUM-INDEPENDENT because it is made ENTIRELY in canvas cells, before any
    // metric is consulted: `screen_of` places the pane against `room_w`, and a metric only ever
    // changes how much PROSE fits inside a placement it did not choose. So the disjointness is
    // the same fact in both media, and the graphical medium's own resolution says it in device
    // pixels without anybody converting a rectangle twice.
    Room r = room_of(6, 120, 40, 8, 18);
    r.s.terminal.open = true;
    r.s.terminal.attached = true;
    const Screen sc = screen_of(r.s);
    const surface::SurfaceCanvas c = paint(r.d, r.s);
    const InfoBodyPlace body = body_of(r.d, r.s);
    REQUIRE(body.present);

    const surface::RegionFit pane_fit =
        surface::fit_region(sc.terminal_x, sc.terminal_y, sc.terminal_w, sc.terminal_h,
                            sc.text_advance_px, sc.text_line_px);
    REQUIRE(pane_fit.graphical());
    REQUIRE(body.fit.graphical());
    CHECK(pane_fit.view.x + pane_fit.view.w <= body.fit.view.x);

    // AND THE CELL PROJECTION SAYS THE SAME THING: every row of the body is on the screen.
    const std::vector<std::string> rows = rasterized(c);
    CHECK(rows[static_cast<std::size_t>(body.region_y +
                                        prose_row_of_action(body, kActionDelete))]
              .substr(static_cast<std::size_t>(body.region_x), 10) == "[ Delete ]");
}

// ============================================================================
// Tier 13 — the SETUP: a maker names the arrangement they are working in (WS-0)
// ============================================================================
//
// The question this tier answers is the one the phase set out to ask:
//
//     what is the smallest thing a maker can NAME, LEAVE, and COME BACK TO,
//     and how does it stay honest when one of the panes it names is one this
//     build has never heard of?
//
// The answer is a human name and an ordered list of two-string references, and
// the cases below are arranged so that every way of getting it wrong is visible:
//
//   PURE       the value, its law, its bounds, and the fallible resolution that
//              is the whole reason a reference is text rather than an ordinal.
//   RECONCILED authored intent onto resolved presentations, with the Builder and
//              Info as deliberately opposite witnesses -- one has a provider to
//              ask and a copy to forget, the other has neither.
//   PERSISTED  the file's own identity, its refusals, and the round trip.
//   LIVE       through the real weave on a real bus, driven by published input,
//              including two processes' worth of it.
//
// WHAT IS DELIBERATELY ABSENT from every case here: a rectangle, a placement, a
// screen extent, a WeaveId, a picker cursor, an integer panel kind, and any
// assertion that a provider exists.

namespace {

/// The reference a built-in kind is spelled with, as a case says it -- through
/// the catalog, never as a literal, so a case cannot agree with a typo.
PaneRef ref_of(std::int64_t kind) { return pane_ref_of(kind); }

/// A reference to a pane no build of this Workshop has ever had. The
/// third-party entry every unresolved case is built on.
PaneRef stranger() { return PaneRef{"third.party.tools", "history"}; }

/// A setup, spelled the way a case reads: a name and the kinds it means.
Setup setup_of(const std::string& name, const std::vector<std::int64_t>& kinds) {
    Setup s;
    s.name = name;
    for (const std::int64_t k : kinds) {
        s.panes.push_back(ref_of(k));
    }
    return s;
}

/// The kinds a session currently has open, in open order -- what the authored
/// order is supposed to have produced.
std::vector<std::int64_t> open_kinds(const Panels& panels) {
    std::vector<std::int64_t> out;
    for (const Panel& p : panels.open) {
        out.push_back(p.kind);
    }
    return out;
}

/// A setup file's text with one substring replaced -- how the refusal cases
/// forge a file the honest writer could never produce. The document tier's own
/// `forged`, asked about the other artifact.
std::string forged_setup(const Setup& s, const std::string& from, const std::string& to) {
    std::string text = setup_persist::to_text(s);
    const std::size_t at = text.find(from);
    INFO("looking for `", from, "` in: ", text);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
    return text;
}

/// Name the setup and save it, the way a maker does: `s`, the character that
/// key produced, clear what is there, type a name, Return.
///
/// IT SENDS THE TRIGGER'S OWN TEXT EVERY TIME, deliberately. The backends report
/// `s` as `KeyPressed{S}` AND `TextEntered{"s"}`, and a fixture that sent only
/// the first would make the swallow untestable from every case that uses it.
void name_setup(Live& t, const std::string& name) {
    t.key(input::scan::kS);
    t.text("s");
    REQUIRE(t.session().setup.naming.open);
    for (int guard = 0; guard < 64 && !t.session().setup.naming.line.empty(); ++guard) {
        t.key(input::scan::kBackspace);
    }
    REQUIRE(t.session().setup.naming.line.empty());
    for (const char c : name) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kReturn);
}

/// The setup line as a maker reads it, off the canvas at the place the painter
/// put it -- never rebuilt here, so a case cannot pass while the screen says
/// something else.
std::string setup_row(const surface::SurfaceCanvas& c, const Screen& sc) {
    return label_at(c, 0, sc.notice_y - 1);
}

/// Make this Workshop paint once, the way a Skin claiming the surface makes it:
/// a weave runs only on message, so a session nobody has spoken to has published
/// no canvas at all.
const surface::SurfaceCanvas& first_frame(Live& t) {
    t.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE_FALSE(t.canvases.empty());
    return t.canvases.back();
}

} // namespace

// ---- The value, its law and its bounds ---------------------------------------

TEST_CASE("a pane reference is two strings, and the pair is the identity") {
    const PaneRef info = ref_of(panel::kInfo);
    CHECK(info.provider == std::string(kWorkshopProvider));
    CHECK(info.pane == std::string(pane_key::kInfo));
    CHECK(info == PaneRef{kWorkshopProvider, pane_key::kInfo});

    // NEITHER HALF ALONE IS THE IDENTITY. The same pane key under another
    // provider is a different pane, and that is the property that lets a later
    // provider have an `info` of its own without colliding with this one.
    CHECK_FALSE(info == PaneRef{"third.party.tools", pane_key::kInfo});
    CHECK_FALSE(info == PaneRef{kWorkshopProvider, "builder"});

    // And nothing is normalised: a reference comes back exactly as it went in.
    const PaneRef odd{"Third.Party.Tools", "History"};
    CHECK_FALSE(odd == stranger());
    CHECK(ref_text(odd) == "Third.Party.Tools/History");
}

TEST_CASE("two setups are the same setup when they name the same panes in the same order") {
    const Setup a = setup_of("Build", {panel::kInfo, panel::kBuilder});
    Setup b = setup_of("Build", {panel::kInfo, panel::kBuilder});
    CHECK(a == b);

    // THE NAME IS PART OF IT: renaming a setup makes it a different setup, which
    // is what makes `saved()` say UNSAVED after a rename that moved no pane.
    b.name = "Analysis";
    CHECK_FALSE(a == b);

    // ...AND SO IS THE ORDER. Today's two built-ins occupy different places so
    // the order changes no rectangle; the value still distinguishes them,
    // because the day a second kind is placed in the stack the order IS which
    // slot each one takes (`bounds_of`).
    const Setup other_way = setup_of("Build", {panel::kBuilder, panel::kInfo});
    CHECK_FALSE(a == other_way);
}

TEST_CASE("a fresh Workshop's setup and its open panels are ONE decision") {
    // The drift this case exists to prevent is silent: `default_setup()` saying
    // Info while `default_panels()` said something else would give a maker a
    // screen that disagreed with the setup line above it from the first frame.
    // Both are derived from `kDefaultPanels`, and this is the proof.
    const Setup fresh = default_setup();
    CHECK(fresh.name == std::string(kDefaultSetupName));
    REQUIRE(fresh.panes.size() == kDefaultPanelCount);

    std::vector<std::int64_t> from_setup;
    for (const PaneRef& p : fresh.panes) {
        const std::optional<std::int64_t> kind = resolve_pane(p);
        REQUIRE(kind.has_value());
        from_setup.push_back(*kind);
    }
    CHECK(from_setup == open_kinds(Panels{}));

    // And the product default itself, named rather than merely derived: Info.
    CHECK(from_setup == std::vector<std::int64_t>{panel::kInfo});

    // A default session agrees with both.
    const Session s;
    CHECK(s.setup.active == fresh);
    CHECK(open_kinds(s.panels) == from_setup);
}

TEST_CASE("every catalog row carries a durable reference that resolves back to it") {
    // Over the POPULATION, not over its size: a third kind is proved by this
    // case without being mentioned in it.
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        const PanelKind& row = kPanelCatalog[i];
        INFO("catalog entry ", i, ": ", std::string(row.name));
        REQUIRE(row.provider != nullptr);
        REQUIRE(row.pane != nullptr);
        CHECK_FALSE(std::string(row.provider).empty());
        CHECK_FALSE(std::string(row.pane).empty());

        // The two directions compose: a kind's reference resolves to that kind.
        const PaneRef ref = pane_ref_of(row.kind);
        CHECK(ref.provider == std::string(row.provider));
        CHECK(ref.pane == std::string(row.pane));
        const std::optional<std::int64_t> back = resolve_pane(ref);
        REQUIRE(back.has_value());
        CHECK(*back == row.kind);

        // And it is legal as a reference, by the same law a file's is judged by.
        CHECK(check_pane_ref(ref).accepted);
    }

    // The built-ins are spelled the way the phase promised a maker could read.
    CHECK(ref_text(ref_of(panel::kInfo)) == "zengine.workshop/info");
    CHECK(ref_text(ref_of(panel::kBuilder)) == "zengine.workshop/builder");
}

TEST_CASE("an unknown reference resolves to NOTHING, and never to the Builder") {
    // THE WHOLE REASON THE FALLIBLE DOOR EXISTS. `panel_kind` answers with the
    // catalog's first row for an unknown kind, which is right for its callers
    // and would be a lie here: an unknown reference routed through it would
    // paint a maker's third-party pane as Workshop's build tool.
    CHECK_FALSE(resolve_pane(stranger()).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{"third.party.tools", pane_key::kInfo}).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{kWorkshopProvider, "history"}).has_value());
    CHECK_FALSE(resolve_pane(PaneRef{"", ""}).has_value());
    CHECK_FALSE(resolvable(stranger()));

    // The negative control, stated as its own claim: the total lookup DOES
    // answer Builder for an unknown kind, and it is still allowed to, because
    // nothing that meets a file goes through it.
    CHECK(panel_kind(9999).kind == panel::kBuilder);
    CHECK(resolve_pane(stranger()).value_or(panel::kInfo) != panel::kBuilder);
}

TEST_CASE("what this application accepts as a setup name") {
    CHECK(check_setup_name("Default").accepted);
    CHECK(check_setup_name("Morning build").accepted);                    // spaces allowed
    CHECK(check_setup_name(std::string(kMaxSetupNameLen, 'x')).accepted); // exactly the bound

    CHECK_FALSE(check_setup_name("").accepted);
    CHECK_FALSE(check_setup_name("   ").accepted); // more than spaces
    CHECK_FALSE(check_setup_name(std::string(kMaxSetupNameLen + 1, 'x')).accepted);
    // A control character can only arrive from a forged file, and what it would
    // do is move a terminal's cursor out of the line it was given.
    CHECK_FALSE(check_setup_name("Build\nmore").accepted);
    CHECK_FALSE(check_setup_name(std::string("Build\x1b[2J")).accepted);
    CHECK_FALSE(check_setup_name(std::string("Build\x7f")).accepted);

    // The refusals name what is wrong, because the reason IS the feature.
    CHECK(check_setup_name("").refusal.find("empty") != std::string::npos);
    CHECK(check_setup_name(std::string(kMaxSetupNameLen + 1, 'x'))
              .refusal.find(std::to_string(kMaxSetupNameLen)) != std::string::npos);
    CHECK(check_setup_name("a\tb").refusal.find("control") != std::string::npos);
}

TEST_CASE("what this application accepts as either half of a reference") {
    CHECK(check_pane_key("zengine.workshop", "provider").accepted);
    CHECK(check_pane_key(std::string(kMaxPaneKeyLen, 'a'), "pane key").accepted);

    CHECK_FALSE(check_pane_key("", "provider").accepted);
    CHECK_FALSE(check_pane_key(std::string(kMaxPaneKeyLen + 1, 'a'), "provider").accepted);
    CHECK_FALSE(check_pane_key("two words", "pane key").accepted);
    CHECK_FALSE(check_pane_key("line\nbreak", "pane key").accepted);

    // The refusal says WHICH half, because `provider` and `pane key` are two
    // fields a maker looking at their own file has to tell apart.
    CHECK(check_pane_ref(PaneRef{"", "info"}).refusal.find("provider") != std::string::npos);
    CHECK(check_pane_ref(PaneRef{"zengine.workshop", ""}).refusal.find("pane key") !=
          std::string::npos);
}

TEST_CASE("the whole-setup law: duplicates, the count bound, and an empty list") {
    CHECK(check_setup(default_setup()).accepted);

    // AN EMPTY PANE LIST IS LEGAL. "I want nothing open" is reachable through
    // the picker already, so refusing to save it would make one arrangement a
    // maker can produce impossible to name.
    Setup empty;
    empty.name = "Nothing";
    CHECK(check_setup(empty).accepted);

    // AN UNRESOLVED REFERENCE IS LEGAL, and this is the case that says the law
    // and the resolution are different questions.
    Setup future;
    future.name = "Later";
    future.panes.push_back(stranger());
    CHECK(check_setup(future).accepted);
    CHECK_FALSE(resolvable(future.panes.front()));

    // A DUPLICATE IS NOT. A kind is open or it is not; a file naming one twice
    // was written by somebody who believed in a policy this application does not
    // have, and silently keeping one of the two would hide that.
    Setup twice = setup_of("Twice", {panel::kInfo});
    twice.panes.push_back(ref_of(panel::kInfo));
    CHECK_FALSE(check_setup(twice).accepted);
    CHECK(check_setup(twice).refusal.find("twice") != std::string::npos);
    CHECK(check_setup(twice).refusal.find("zengine.workshop/info") != std::string::npos);

    // ...and neither is more than a setup may hold. THE BOUND IS NOT THE
    // CATALOG'S POPULATION, deliberately: a setup must be able to retain
    // references to panes this build has never heard of, so a limit cut to
    // `kPanelKinds` would refuse the one case the design exists to allow.
    CHECK(kMaxSetupPanes > kPanelKinds);
    Setup many_panes;
    many_panes.name = "Many";
    for (std::size_t i = 0; i < kMaxSetupPanes; ++i) {
        many_panes.panes.push_back(PaneRef{"third.party.tools", "p" + std::to_string(i)});
    }
    CHECK(check_setup(many_panes).accepted);
    many_panes.panes.push_back(PaneRef{"third.party.tools", "one-too-many"});
    CHECK_FALSE(check_setup(many_panes).accepted);
    CHECK(check_setup(many_panes).refusal.find(std::to_string(kMaxSetupPanes)) !=
          std::string::npos);

    // A bad name and a bad reference are both refused by the one whole-setup
    // law, so a caller cannot check one and forget the other.
    const Setup unnamed = setup_of("", {panel::kInfo});
    CHECK_FALSE(check_setup(unnamed).accepted);
    Setup bad_ref = setup_of("Bad", {});
    bad_ref.panes.push_back(PaneRef{"has space", "info"});
    CHECK_FALSE(check_setup(bad_ref).accepted);
}

TEST_CASE("adding and removing a pane preserves order and never duplicates") {
    Setup s;
    s.name = "Work";
    CHECK(add_pane(s, ref_of(panel::kInfo)));
    CHECK(add_pane(s, ref_of(panel::kBuilder)));
    CHECK_FALSE(add_pane(s, ref_of(panel::kInfo))); // already there, and it says so
    REQUIRE(s.panes.size() == 2);
    CHECK(s.panes[0] == ref_of(panel::kInfo));
    CHECK(s.panes[1] == ref_of(panel::kBuilder));
    CHECK(check_setup(s).accepted);

    // ADDED AT THE END, which is where `open_panel` has always put a newly
    // opened panel -- so the authored order agrees with the resolved order a
    // maker was already watching.
    CHECK(remove_pane(s, ref_of(panel::kInfo)));
    REQUIRE(s.panes.size() == 1);
    CHECK(s.panes[0] == ref_of(panel::kBuilder));
    CHECK_FALSE(remove_pane(s, ref_of(panel::kInfo)));

    // An unresolved reference is an ordinary member: it can be added, found and
    // removed by exactly the same three functions.
    CHECK(add_pane(s, stranger()));
    CHECK(has_pane(s, stranger()));
    CHECK(remove_pane(s, stranger()));
    CHECK_FALSE(has_pane(s, stranger()));
}

TEST_CASE("the unresolved panes are reported in the setup's own order") {
    Setup s;
    s.name = "Mixed";
    s.panes.push_back(ref_of(panel::kInfo));
    s.panes.push_back(stranger());
    s.panes.push_back(PaneRef{"other.tools", "graph"});
    s.panes.push_back(ref_of(panel::kBuilder));

    const std::vector<PaneRef> waiting = unresolved_panes(s);
    REQUIRE(waiting.size() == 2);
    CHECK(waiting[0] == stranger());
    CHECK(waiting[1] == PaneRef{"other.tools", "graph"});
    CHECK(unresolved_panes(default_setup()).empty());
}

// ---- Authored intent, reconciled onto resolved presentations ------------------

TEST_CASE("reconciling opens what the setup names, in the setup's order") {
    Panels panels;
    REQUIRE(open_kinds(panels) == std::vector<std::int64_t>{panel::kInfo});

    const Reconciled done = reconcile(panels, setup_of("Both", {panel::kBuilder, panel::kInfo}));
    CHECK(done.opened == std::vector<std::int64_t>{panel::kBuilder});
    CHECK(done.closed.empty());
    CHECK(done.unresolved == 0);
    // THE ORDER IS THE SETUP'S, not the order things happened to be opened in.
    CHECK(open_kinds(panels) == std::vector<std::int64_t>{panel::kBuilder, panel::kInfo});

    // The other way round, from the same starting point, produces the other order.
    Panels again;
    (void)reconcile(again, setup_of("Both", {panel::kInfo, panel::kBuilder}));
    CHECK(open_kinds(again) == std::vector<std::int64_t>{panel::kInfo, panel::kBuilder});
}

TEST_CASE("reconciling closes what the setup does not name, through the existing door") {
    Panels panels;
    REQUIRE(open_panel(panels, panel::kBuilder));
    panels.builder.heard = true;
    panels.builder.shown.target = "zengine-snake";

    const Reconciled done = reconcile(panels, setup_of("Info only", {panel::kInfo}));
    CHECK(done.closed == std::vector<std::int64_t>{panel::kBuilder});
    CHECK(done.opened.empty());
    CHECK(open_kinds(panels) == std::vector<std::int64_t>{panel::kInfo});

    // THE PANEL'S COPY IS FORGOTTEN BY THE SAME ACT, because the close goes
    // through `close_panel` rather than through a loop that rebuilt the vector.
    // A removed Builder whose copied status outlived it would make a reopened
    // panel look instantly informed about something minutes stale.
    CHECK_FALSE(panels.builder.heard);
    CHECK(panels.builder.shown.target.empty());
}

TEST_CASE("a panel open on both sides of a reconcile keeps what it was showing") {
    // OPEN BEFORE, OPEN AFTER -- the case that says a reconcile is not a rebuild.
    // Restoring the setup you are already in must not be a visible event.
    Panels panels;
    REQUIRE(open_panel(panels, panel::kBuilder));
    panels.builder.heard = true;
    panels.builder.shown.target = "zengine-snake";
    panels.builder.shown.builds = 3;

    const Setup same = setup_of("Both", {panel::kInfo, panel::kBuilder});
    const Reconciled done = reconcile(panels, same);
    CHECK(done.opened.empty());
    CHECK(done.closed.empty());
    CHECK(panels.builder.heard);
    CHECK(panels.builder.shown.target == "zengine-snake");
    CHECK(panels.builder.shown.builds == 3);

    // ...and doing it a second time changes nothing at all.
    const Reconciled twice = reconcile(panels, same);
    CHECK(twice.opened.empty());
    CHECK(twice.closed.empty());
    CHECK(panels.builder.shown.builds == 3);
}

TEST_CASE("an unresolved reference is counted, and produces no panel of any kind") {
    Panels panels;
    Setup s = setup_of("Mixed", {panel::kInfo});
    s.panes.push_back(stranger());
    s.panes.push_back(PaneRef{"other.tools", "graph"});

    const Reconciled done = reconcile(panels, s);
    CHECK(done.unresolved == 2);
    // NO PLACEHOLDER, NO SLOT, NO FALL-THROUGH TO THE BUILDER. The only kind
    // available to paint an unknown pane with is the Builder, which is exactly
    // why the resolution had to be fallible before this line could be written.
    CHECK(open_kinds(panels) == std::vector<std::int64_t>{panel::kInfo});
    CHECK_FALSE(panels.has(panel::kBuilder));
    // And the setup still holds all three: reconciling takes it by const
    // reference and could not drop one if it wanted to.
    CHECK(s.panes.size() == 3);
    CHECK(s.panes[1] == stranger());
}

TEST_CASE("an empty setup closes everything, and is a legal thing to be in") {
    Panels panels;
    REQUIRE(open_panel(panels, panel::kBuilder));
    Setup nothing;
    nothing.name = "Nothing";

    const Reconciled done = reconcile(panels, nothing);
    CHECK(done.closed.size() == 2);
    CHECK(panels.open.empty());
    CHECK(done.unresolved == 0);

    // And back again from empty, which is the case that proves `opened` names
    // every kind rather than only the ones that were never open.
    const Reconciled back = reconcile(panels, setup_of("Both", {panel::kInfo, panel::kBuilder}));
    CHECK(back.opened.size() == 2);
    CHECK(open_kinds(panels) == std::vector<std::int64_t>{panel::kInfo, panel::kBuilder});
}

TEST_CASE("reconciling touches the picker, the document and the screen not at all") {
    Panels panels;
    panels.picker.open = true;
    panels.picker.cursor = 1;
    (void)reconcile(panels, setup_of("Builder", {panel::kBuilder}));
    // The picker is INTERACTION STATE. It is not setup intent, it is not in the
    // file, and reconciling is not a reason to open or close it.
    CHECK(panels.picker.open);
    CHECK(panels.picker.cursor == 1);
}

// ---- The setup's own file ------------------------------------------------------

TEST_CASE("a setup file says what it is, in words a maker can read") {
    const std::string text = setup_persist::to_text(default_setup());
    INFO(text);

    // ITS OWN FORMAT IDENTITY, beside the document's and not equal to it.
    CHECK(text.find("\"format\":\"zengine-workshop-setup\"") != std::string::npos);
    CHECK(text.find("\"format_version\":\"1\"") != std::string::npos);
    CHECK(text.find("\"name\":\"Default\"") != std::string::npos);
    CHECK(text.find("\"provider\":\"zengine.workshop\"") != std::string::npos);
    CHECK(text.find("\"pane\":\"info\"") != std::string::npos);

    // NO INTEGER PANEL KIND ANYWHERE IN IT. This is the assertion to check this
    // format against first: the file names panes the way a person and a future
    // provider would, and never the way this build's own vocabulary does.
    CHECK(text.find("\"kind\"") == std::string::npos);
    CHECK(text.find("placement") == std::string::npos);
    CHECK(text.find("rect") == std::string::npos);
    CHECK(text.find("weave") == std::string::npos);

    // ...and it is not the document's format, which is the whole point of it
    // having one of its own: handing Workshop the wrong one of its own two
    // files is named rather than half-read.
    CHECK(std::string(setup_persist::kFormat) != std::string(persist::kFormat));
}

TEST_CASE("every shape of setup survives a round trip through its file") {
    struct Case {
        const char* what;
        Setup setup;
    };
    std::vector<Case> cases;
    cases.push_back({"the default, Info only", default_setup()});
    cases.push_back({"Builder only", setup_of("Build", {panel::kBuilder})});
    cases.push_back({"both, in a deliberate order",
                     setup_of("Everything", {panel::kBuilder, panel::kInfo})});
    cases.push_back({"both, in the other order",
                     setup_of("Everything", {panel::kInfo, panel::kBuilder})});
    Setup nothing;
    nothing.name = "Nothing at all";
    cases.push_back({"an empty pane list", nothing});
    cases.push_back({"a human name with spaces in it", setup_of("Morning build", {panel::kInfo})});
    cases.push_back({"a name at the bound",
                     setup_of(std::string(kMaxSetupNameLen, 'n'), {panel::kInfo})});
    Setup mixed = setup_of("Mixed", {panel::kInfo});
    mixed.panes.push_back(stranger());
    mixed.panes.push_back(ref_of(panel::kBuilder));
    cases.push_back({"a reference this build cannot resolve, between two it can", mixed});

    for (const Case& c : cases) {
        CAPTURE(c.what);
        const std::string a = setup_persist::to_text(c.setup);
        const setup_persist::LoadedSetup read = setup_persist::from_text(a);
        REQUIRE(read.outcome.accepted);
        CHECK(read.setup == c.setup);

        // SAVE -> LOAD -> SAVE IS BYTE-IDENTICAL. No canonicalisation framework
        // and no sorting: the codec's output is deterministic and nothing here
        // reorders, normalises or resolves a value on its way through.
        const std::string b = setup_persist::to_text(read.setup);
        CHECK(a == b);
    }
}

TEST_CASE("saving never sorts, normalises, resolves or drops a reference") {
    // The strongest version of the claim: a setup whose order is NOT the
    // catalog's, containing an entry this build cannot present, in the middle.
    Setup s;
    s.name = "Deliberate";
    s.panes.push_back(ref_of(panel::kBuilder));
    s.panes.push_back(stranger());
    s.panes.push_back(ref_of(panel::kInfo));

    const setup_persist::LoadedSetup read = setup_persist::from_text(setup_persist::to_text(s));
    REQUIRE(read.outcome.accepted);
    REQUIRE(read.setup.panes.size() == 3);
    CHECK(read.setup.panes[0] == ref_of(panel::kBuilder));
    CHECK(read.setup.panes[1] == stranger());
    CHECK(read.setup.panes[2] == ref_of(panel::kInfo));

    // The unresolved entry's bytes are exactly the bytes that went in.
    const std::string text = setup_persist::to_text(read.setup);
    CHECK(text.find("\"provider\":\"third.party.tools\"") != std::string::npos);
    CHECK(text.find("\"pane\":\"history\"") != std::string::npos);
}

TEST_CASE("a malformed setup file is refused, and the live setup is untouched") {
    // The claim is not "the parser returned an error". It is that the setup a
    // maker is in is exactly what it was.
    const Setup good = setup_of("Everything", {panel::kInfo, panel::kBuilder});
    const std::string valid = setup_persist::to_text(good);

    struct Case {
        const char* what;
        std::string text;
    };
    std::vector<Case> cases;
    cases.push_back({"not JSON at all", "{ this is not a setup"});
    cases.push_back({"an empty file", ""});
    cases.push_back({"a JSON array", "[1,2,3]"});
    cases.push_back({"someone else's value",
                     loom::compat::serialize(loom::to_value(ui::Extent{0, 4}))});
    cases.push_back({"a Workshop DOCUMENT handed to the setup reader",
                     persist::to_text(two_panels())});
    cases.push_back({"a valid setup with a tail after it", valid + "x"});
    cases.push_back({"the wrong format identity",
                     forged_setup(good, "\"zengine-workshop-setup\"", "\"someone-elses-tool\"")});
    cases.push_back({"an unsupported format version",
                     forged_setup(good, "\"format_version\":\"1\"", "\"format_version\":\"2\"")});
    cases.push_back({"a missing required field",
                     forged_setup(good, "\"name\":\"Everything\",", "")});
    cases.push_back({"a field of the wrong kind",
                     forged_setup(good, "\"format_version\":\"1\"", "\"format_version\":1")});
    cases.push_back({"a field the setup does not declare",
                     forged_setup(good, "\"name\":", "\"colour\":\"red\",\"name\":")});
    cases.push_back({"a field a pane reference does not declare",
                     forged_setup(good, "\"provider\":", "\"rect\":\"1\",\"provider\":")});
    cases.push_back({"a name that is not a name",
                     forged_setup(good, "\"name\":\"Everything\"", "\"name\":\"\"")});
    cases.push_back({"a name that is only spaces",
                     forged_setup(good, "\"name\":\"Everything\"", "\"name\":\"   \"")});
    cases.push_back({"a name longer than a name",
                     forged_setup(good, "\"name\":\"Everything\"",
                                  "\"name\":\"" + std::string(kMaxSetupNameLen + 1, 'x') +
                                      "\"")});
    cases.push_back({"a name carrying a control character",
                     forged_setup(good, "\"name\":\"Everything\"", "\"name\":\"Ever\\ttime\"")});
    cases.push_back({"an empty provider key",
                     forged_setup(good, "\"provider\":\"zengine.workshop\"", "\"provider\":\"\"")});
    cases.push_back({"an empty pane key",
                     forged_setup(good, "\"pane\":\"info\"", "\"pane\":\"\"")});
    cases.push_back({"a provider key longer than a key",
                     forged_setup(good, "\"provider\":\"zengine.workshop\"",
                                  "\"provider\":\"" + std::string(kMaxPaneKeyLen + 1, 'p') +
                                      "\"")});
    cases.push_back({"a pane key with a space in it",
                     forged_setup(good, "\"pane\":\"info\"", "\"pane\":\"two words\"")});
    cases.push_back({"the same pane named twice",
                     forged_setup(good, "\"pane\":\"builder\"", "\"pane\":\"info\"")});

    // ...and one that has to be built rather than forged: more panes than a
    // setup may hold.
    Setup crowd;
    crowd.name = "Crowd";
    for (std::size_t i = 0; i <= kMaxSetupPanes; ++i) {
        crowd.panes.push_back(PaneRef{"third.party.tools", "p" + std::to_string(i)});
    }
    cases.push_back({"more panes than a setup may hold", setup_persist::to_text(crowd)});

    for (const Case& c : cases) {
        CAPTURE(c.what);
        const setup_persist::LoadedSetup refused = setup_persist::from_text(c.text);
        CHECK_FALSE(refused.outcome.accepted);
        CHECK_FALSE(refused.outcome.refusal.empty()); // a refusal without a reason is not one
        // And nothing was carried out of it: the candidate a caller would have
        // reconciled is empty, which is what makes "never halfway restored"
        // structural rather than careful.
        CHECK(refused.setup.name.empty());
        CHECK(refused.setup.panes.empty());
    }

    // The control: the file these were all forged from still loads.
    const setup_persist::LoadedSetup ok = setup_persist::from_text(valid);
    CHECK(ok.outcome.accepted);
    CHECK(ok.setup == good);
}

TEST_CASE("a setup file on disk is the setup read back from it") {
    TempDir dir("setup-roundtrip");
    const std::string path = dir.file("setup.json");
    const Setup original = setup_of("Everything", {panel::kBuilder, panel::kInfo});
    REQUIRE(setup_persist::save_file(path, original).accepted);

    const setup_persist::LoadedSetup read = setup_persist::load_file(path);
    REQUIRE(read.outcome.accepted);
    CHECK(read.setup == original);

    // The bytes on disk are the bytes the writer produced -- no wrapper, no
    // trailer, nothing added by the file layer -- and the sibling is gone.
    CHECK(slurp(path) == setup_persist::to_text(original));
    CHECK_FALSE(std::filesystem::exists(persist::pending_path(path)));

    // Saving again over an existing setup replaces it.
    const Setup second = setup_of("Info only", {panel::kInfo});
    REQUIRE(setup_persist::save_file(path, second).accepted);
    CHECK(setup_persist::load_file(path).setup == second);
}

TEST_CASE("a missing setup file is an ordinary refusal, not an empty setup") {
    TempDir dir("setup-missing");
    const setup_persist::LoadedSetup refused = setup_persist::load_file(dir.file("never.json"));
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("never.json") != std::string::npos);
}

TEST_CASE("a file too large to be a setup is refused before it is read") {
    TempDir dir("setup-huge");
    const std::string path = dir.file("setup.json");
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const std::string chunk(1u << 12, 'x');
        for (int i = 0; i < 32; ++i) { // 128 KiB, past the 64 KiB ceiling
            out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        }
    }
    REQUIRE(std::filesystem::file_size(path) > setup_persist::kMaxSetupBytes);

    const setup_persist::LoadedSetup refused = setup_persist::load_file(path);
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal.find("larger") != std::string::npos);
    // It names WHICH of the two artifacts it was measuring against, because a
    // maker with two files needs to know which ceiling they met.
    CHECK(refused.outcome.refusal.find("setup") != std::string::npos);
}

TEST_CASE("a detected setup write failure leaves the last good setup file untouched") {
    // The reason the writer never opens the destination, asked about the second
    // artifact: a save that fails must not be able to turn a maker's saved
    // arrangement into an empty or half-written file.
    TempDir dir("setup-failsave");
    const std::string path = dir.file("setup.json");
    const Setup first = setup_of("Good", {panel::kInfo});
    REQUIRE(setup_persist::save_file(path, first).accepted);
    const std::string good_bytes = slurp(path);
    REQUIRE_FALSE(good_bytes.empty());

    // A controlled, deterministic, non-destructive failure: the sibling path the
    // writer must use is occupied by a DIRECTORY, so the write cannot open.
    std::filesystem::create_directories(persist::pending_path(path));

    const Setup second = setup_of("Better", {panel::kBuilder, panel::kInfo});
    const Written refused = setup_persist::save_file(path, second);
    CHECK_FALSE(refused.accepted);
    CHECK_FALSE(refused.refusal.empty());

    // The last good save is intact, byte for byte, and still loads.
    CHECK(slurp(path) == good_bytes);
    const setup_persist::LoadedSetup reloaded = setup_persist::load_file(path);
    REQUIRE(reloaded.outcome.accepted);
    CHECK(reloaded.setup == first);

    std::error_code ec;
    std::filesystem::remove_all(persist::pending_path(path), ec);
    CHECK(setup_persist::save_file(path, second).accepted); // and it works again
    CHECK(setup_persist::load_file(path).setup == second);
}

TEST_CASE("the document's file and the setup's file cannot be mistaken for each other") {
    TempDir dir("two-files");
    const std::string doc_path = dir.document();
    const std::string setup_path = dir.file("setup.json");
    REQUIRE(persist::save_file(doc_path, rich_document()).accepted);
    REQUIRE(setup_persist::save_file(setup_path, default_setup()).accepted);

    // Each reader refuses the other's file, by name, rather than half-reading it.
    CHECK_FALSE(setup_persist::load_file(doc_path).outcome.accepted);
    WorkshopDoc live = two_panels();
    const WorkshopDoc before = live;
    CHECK_FALSE(persist::load_file(setup_path, live).accepted);
    CHECK(live == before);

    // And writing one leaves the other's bytes alone -- which is what "two
    // artifacts" is worth, said in the only way that could fail.
    const std::string doc_bytes = slurp(doc_path);
    REQUIRE(setup_persist::save_file(setup_path, setup_of("Other", {panel::kBuilder})).accepted);
    CHECK(slurp(doc_path) == doc_bytes);
}

// ---- Through the real weave: the picker moves the intent -------------------------

TEST_CASE("a fresh Workshop's active setup and its open panels agree from the first frame") {
    Live t;
    CHECK(t.session().setup.active == default_setup());
    CHECK(open_kinds(t.session().panels) == std::vector<std::int64_t>{panel::kInfo});
    // UNSAVED, and structurally so: nothing has been written, and the copy the
    // comparison is made against has an empty name no legal setup can equal.
    CHECK_FALSE(t.session().setup.saved());
}

TEST_CASE("opening a panel through the picker moves the setup's intent, not only the screen") {
    // THE COHERENCE CLAIM. Before WS-0 the picker called `open_panel` directly;
    // if it still did, the active setup would go on describing an arrangement
    // the screen had stopped showing the moment a maker pressed `p`.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_builder(t);

    REQUIRE(t.session().panels.has(panel::kBuilder));
    CHECK(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));
    // At the END of the authored order, which is where the screen put it.
    REQUIRE(t.session().setup.active.panes.size() == 2);
    CHECK(t.session().setup.active.panes[1] == ref_of(panel::kBuilder));
    // ...and the resolved open order agrees with the resolved order of the refs.
    CHECK(open_kinds(t.session().panels) ==
          std::vector<std::int64_t>{panel::kInfo, panel::kBuilder});

    // Removing it takes the reference back out.
    pick(t, panel::kBuilder);
    CHECK_FALSE(t.session().panels.has(panel::kBuilder));
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));

    // And so does removing Info, which has no provider to make it a special case.
    pick(t, panel::kInfo);
    CHECK_FALSE(t.session().panels.has(panel::kInfo));
    CHECK(t.session().setup.active.panes.empty());
}

TEST_CASE("a panel change makes the setup UNSAVED, and changing it back makes it saved again") {
    TempDir dir("setup-saved-truth");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    name_setup(t, "Working");
    REQUIRE(t.session().setup.saved());
    CHECK(t.session().setup.active.name == "Working");
    CHECK(t.notice().find("saved setup \"Working\"") == 0);

    open_builder(t);
    CHECK_FALSE(t.session().setup.saved());

    // OPENING THEN CLOSING BACK TO THE SAVED INTENT SAYS SAVED AGAIN, which is
    // what a comparison buys and a dirty flag could not: there is no hand to
    // forget to unset.
    pick(t, panel::kBuilder);
    CHECK(t.session().setup.saved());

    // A rename with no pane moved is still a change, because the name is part of
    // the value.
    Setup renamed = t.session().setup.active;
    renamed.name = "Other";
    CHECK_FALSE(renamed == t.session().setup.on_file);
}

TEST_CASE("the `s` that opens the name editor does not type itself into the name") {
    // The trap WG-0 measured and named: a key transition and the character it
    // produced are two facts that were simultaneously true, and both arrive.
    TempDir dir("setup-swallow");
    Live t;
    t.host.setup_path = dir.file("setup.json");

    t.key(input::scan::kS);
    t.text("s");
    REQUIRE(t.session().setup.naming.open);
    // IT OPENED ON THE NAME THE SETUP ALREADY HAS, with no `s` appended.
    CHECK(t.session().setup.naming.line.text() == "Default");
    CHECK(t.session().setup.naming.line.caret() == std::string("Default").size());

    // A real keystroke arriving straight after IS taken -- the swallow belongs
    // to one moment and cannot eat the next character a maker meant.
    t.text("s");
    CHECK(t.session().setup.naming.line.text() == "Defaults");

    // And the capital the same key produces under Shift is owed too: the
    // trigger said which KEY changed, and the case is the platform's answer.
    Live shifted;
    shifted.host.setup_path = dir.file("other.json");
    shifted.key(input::scan::kS, input::mod::kShift);
    shifted.text("S");
    REQUIRE(shifted.session().setup.naming.open);
    CHECK(shifted.session().setup.naming.line.text() == "Default");
}

TEST_CASE("escape leaves the setup name exactly as it was, and saves nothing") {
    TempDir dir("setup-cancel");
    Live t;
    t.host.setup_path = dir.file("setup.json");

    t.key(input::scan::kS);
    t.text("s");
    t.text("!");
    REQUIRE(t.session().setup.naming.line.text() == "Default!");
    t.key(input::scan::kEscape);

    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == "Default");
    CHECK_FALSE(std::filesystem::exists(t.host.setup_path));
    CHECK(t.notice().find("unchanged") != std::string::npos);
}

TEST_CASE("a name the law refuses leaves the editor open over what was typed") {
    TempDir dir("setup-badname");
    Live t;
    t.host.setup_path = dir.file("setup.json");

    t.key(input::scan::kS);
    t.text("s");
    for (int i = 0; i < 8; ++i) {
        t.key(input::scan::kBackspace);
    }
    REQUIRE(t.session().setup.naming.line.empty());
    t.key(input::scan::kReturn);

    // STILL OPEN, so a maker fixes what they typed rather than retyping it, and
    // nothing was written.
    CHECK(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == "Default");
    CHECK_FALSE(std::filesystem::exists(t.host.setup_path));
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("empty") != std::string::npos);

    // ...and typing a legal one from there works.
    for (const char c : std::string("Fixed")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kReturn);
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == "Fixed");
    CHECK(t.session().setup.saved());
}

TEST_CASE("with no setup file, naming and restoring say so and change nothing") {
    Live t; // no --setup was given
    t.key(input::scan::kS);
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("--setup") != std::string::npos);

    const Setup before = t.session().setup.active;
    t.key(input::scan::kR);
    CHECK(t.session().setup.active == before);
    CHECK(t.notice().find("--setup") != std::string::npos);
    // ...and it is a DIFFERENT sentence from the document's, because a maker
    // with one file and not the other has to know which one they are missing.
    CHECK(t.notice().find("--document") == std::string::npos);
}

TEST_CASE("restoring a setup returns the intent that was saved") {
    TempDir dir("setup-restore");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    open_builder(t);
    pick(t, panel::kInfo); // Builder open, Info removed
    name_setup(t, "Build only");
    REQUIRE(t.session().setup.saved());

    // Wander away from it.
    pick(t, panel::kInfo);
    pick(t, panel::kBuilder);
    REQUIRE_FALSE(t.session().setup.saved());
    REQUIRE(open_kinds(t.session().panels) == std::vector<std::int64_t>{panel::kInfo});

    t.key(input::scan::kR);
    CHECK(t.session().setup.active.name == "Build only");
    CHECK(open_kinds(t.session().panels) == std::vector<std::int64_t>{panel::kBuilder});
    CHECK(t.session().setup.saved());
    CHECK(t.notice().find("restored setup \"Build only\"") == 0);
    CHECK(t.notice().find(t.host.setup_path) != std::string::npos);
}

TEST_CASE("a malformed setup file is refused without closing a single panel") {
    TempDir dir("setup-refuse-live");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    open_builder(t);
    name_setup(t, "Good");
    const Setup saved = t.session().setup.active;
    const std::vector<std::int64_t> panels_before = open_kinds(t.session().panels);
    const BuilderPane pane_before = t.session().panels.builder;
    const WorkshopDoc doc_before = t.doc();
    const std::int64_t asked_before = tool->described;

    // A file whose LAST field is the broken one, so a loader that acted as it
    // read would already have closed something by the time it noticed.
    spillout(t.host.setup_path,
             forged_setup(saved, "\"pane\":\"builder\"", "\"pane\":\"two words\""));
    t.key(input::scan::kR);

    CHECK(t.session().notice_is_bad);
    CHECK(t.session().setup.active == saved);
    CHECK(open_kinds(t.session().panels) == panels_before);
    CHECK(t.session().panels.builder.heard == pane_before.heard);
    CHECK(t.session().panels.builder.shown.target == pane_before.shown.target);
    CHECK(t.doc() == doc_before);
    // NOTHING WAS ASKED OF ANYBODY either: a refused restore is not a reason to
    // send a message on behalf of a panel that did not open.
    CHECK(tool->described == asked_before);
}

// ---- Lifecycle: the Builder and Info as opposite witnesses ------------------------

TEST_CASE("a restore that OPENS the Builder asks the tool what it is") {
    TempDir dir("setup-life-open");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    REQUIRE(setup_persist::save_file(t.host.setup_path,
                                     setup_of("Build", {panel::kBuilder}))
                .accepted);
    REQUIRE_FALSE(t.session().panels.has(panel::kBuilder));
    const std::int64_t before = tool->described;

    t.key(input::scan::kR);
    CHECK(t.session().panels.has(panel::kBuilder));
    // CLOSED BEFORE, OPEN AFTER: it performs the same asking opening it through
    // the picker has always performed, through the same one path.
    CHECK(tool->described == before + 1);
    CHECK(t.session().panels.builder.heard);
    CHECK(t.session().panels.builder.shown.target == "zengine-snake");
}

TEST_CASE("a restore that leaves the Builder open does not ask again or lose its view") {
    TempDir dir("setup-life-same");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    open_builder(t);
    REQUIRE(t.session().panels.builder.heard);
    const std::int64_t asked = tool->described;
    const zengine::builder::BuildStatus shown = t.session().panels.builder.shown;

    REQUIRE(setup_persist::save_file(t.host.setup_path,
                                     setup_of("Same", {panel::kInfo, panel::kBuilder}))
                .accepted);
    t.key(input::scan::kR);

    // OPEN BEFORE, OPEN AFTER: the presentation is left alone. No duplicate
    // refresh ceremony, and the copy it was showing is the copy it is showing.
    CHECK(tool->described == asked);
    CHECK(t.session().panels.builder.heard);
    CHECK(t.session().panels.builder.shown.target == shown.target);
    CHECK(t.session().panels.builder.shown.builds == shown.builds);
}

TEST_CASE("a restore that CLOSES the Builder forgets its copy and leaves the tool alive") {
    TempDir dir("setup-life-close");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    open_builder(t);
    REQUIRE(t.session().panels.builder.heard);

    REQUIRE(setup_persist::save_file(t.host.setup_path, setup_of("Info", {panel::kInfo}))
                .accepted);
    t.key(input::scan::kR);

    CHECK_FALSE(t.session().panels.has(panel::kBuilder));
    // OPEN BEFORE, CLOSED AFTER: the per-kind view is forgotten by the same act,
    // exactly as the picker's removal forgets it.
    CHECK_FALSE(t.session().panels.builder.heard);
    CHECK(t.session().panels.builder.shown.target.empty());

    // THE TOOL IS UNTOUCHED. Nothing was unloaded, nothing was told, and it
    // answers the very next ask with its own running total.
    const std::int64_t asked = tool->described;
    tool->next.builds = 7;
    open_builder(t);
    CHECK(tool->described == asked + 1);
    CHECK(t.session().panels.builder.shown.builds == 7);
}

TEST_CASE("Info opens and closes through a restore with no message and no document change") {
    TempDir dir("setup-life-info");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    const WorkshopDoc doc_before = t.doc();
    const std::int64_t selected = t.session().selected;
    REQUIRE(t.session().panels.has(panel::kInfo));

    // OPEN BEFORE, CLOSED AFTER: the document and the selection are untouched,
    // because Info holds no copy of anything -- what it presents outlives it and
    // belongs to somebody else.
    Setup nothing;
    nothing.name = "Nothing";
    REQUIRE(setup_persist::save_file(t.host.setup_path, nothing).accepted);
    t.key(input::scan::kR);
    CHECK(t.session().panels.open.empty());
    CHECK(t.doc() == doc_before);
    CHECK(t.session().selected == selected);
    CHECK(tool->described == 0);

    // CLOSED BEFORE, OPEN AFTER: it opens, and it asks nobody. A Workshop
    // hosting no tools at all does this and it works.
    REQUIRE(setup_persist::save_file(t.host.setup_path, default_setup()).accepted);
    t.key(input::scan::kR);
    CHECK(t.session().panels.has(panel::kInfo));
    CHECK(tool->described == 0);
    CHECK(t.doc() == doc_before);
    CHECK(t.session().selected == selected);
}

// ---- The unresolved reference, end to end ------------------------------------------

TEST_CASE("a setup naming a pane this build has never heard of loads, keeps it, and says so") {
    // ONE OF THE PHASE'S PRIMARY ACCEPTANCE PROOFS, and not a future-only unit
    // test: the branch is written and exercised while it is still unreachable by
    // any file this build can produce.
    TempDir dir("setup-unresolved");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    ToolSeat* tool = mount_tool(t, "zengine-snake");

    Setup authored;
    authored.name = "Future";
    authored.panes.push_back(ref_of(panel::kInfo));
    authored.panes.push_back(stranger());
    authored.panes.push_back(ref_of(panel::kBuilder));
    const std::string bytes = setup_persist::to_text(authored);
    spillout(t.host.setup_path, bytes);

    t.key(input::scan::kR);

    // IT LOADED. An unresolved reference is not a load failure.
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.session().setup.active == authored);
    // THE UNKNOWN REFERENCE IS STILL THERE, still between the two that resolved,
    // still byte-for-byte what it was.
    REQUIRE(t.session().setup.active.panes.size() == 3);
    CHECK(t.session().setup.active.panes[1] == stranger());

    // The two that resolve are open, in file order relative to each other.
    CHECK(open_kinds(t.session().panels) ==
          std::vector<std::int64_t>{panel::kInfo, panel::kBuilder});

    // AND NOTHING WAS PAINTED ON THE UNKNOWN REFERENCE'S BEHALF. The only kind
    // available to paint an unknown pane with is the Builder, and there is
    // exactly one Builder, in the stack's FIRST slot -- the second slot, where a
    // placeholder would have gone, is empty.
    const Screen sc = screen_of(t.session());
    const PanelBounds builder_at = bounds_of(t.session().panels, panel::kBuilder, sc);
    REQUIRE(builder_at.open);
    CHECK(builder_at.rect == placement_bounds(placement::kOverlayStack, 0, sc));
    CHECK(t.session().panels.open.size() == 2);
    // ...and the slot a placeholder would have taken is not occupied by anything:
    // a hand reaching into it meets the workspace, not a pane painted on behalf
    // of a reference nothing could resolve.
    const ui::Rect second = placement_bounds(placement::kOverlayStack, 1, sc);
    CHECK_FALSE(occupied_at(t.session().panels, sc, second.x + 1, second.y + 1).occupied);

    // The notice says UNRESOLVED and names the reference, and never says
    // unavailable -- Workshop knows it has no catalog row for this, and knows
    // nothing whatever about whoever could present it.
    CHECK(t.notice().find("unresolved") != std::string::npos);
    CHECK(t.notice().find("third.party.tools/history") != std::string::npos);
    CHECK(t.notice().find("unavailable") == std::string::npos);

    // The setup LINE says it too, as a count beside the name.
    CHECK(setup_row(t.canvases.back(), sc).find("1 unresolved") != std::string::npos);

    // NO PROVIDER TRAFFIC WAS CREATED. The one ask that happened is the Builder
    // panel's own, and nothing was sent on the unknown reference's behalf.
    CHECK(tool->described == 1);

    // AND RE-SAVING RETAINS IT EXACTLY. The maker renames the setup and saves;
    // the stranger's entry comes through untouched.
    name_setup(t, "Future kept");
    Setup expected = authored;
    expected.name = "Future kept";
    CHECK(t.session().setup.active == expected);
    CHECK(slurp(t.host.setup_path) == setup_persist::to_text(expected));
    CHECK(slurp(t.host.setup_path).find("third.party.tools") != std::string::npos);

    // ...and it survives a picker gesture on either built-in.
    pick(t, panel::kBuilder);
    CHECK(has_pane(t.session().setup.active, stranger()));
    pick(t, panel::kInfo);
    CHECK(has_pane(t.session().setup.active, stranger()));
    CHECK(t.session().setup.active.panes.front() == stranger());
}

// ---- Authored versus resolved -------------------------------------------------------

TEST_CASE("the same setup resolves to different bounds under a different extent") {
    // THE AUTHORED/RESOLVED PROOF FOR WS-0, and its GREEN CONTROL in one case:
    // change only the surface extent and the setup stays SAVED while every
    // rectangle it resolves to moves.
    TempDir dir("setup-extent");
    Live t;
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    open_builder(t);
    name_setup(t, "Both");
    REQUIRE(t.session().setup.saved());
    const std::string bytes = slurp(t.host.setup_path);
    const Setup authored = t.session().setup.active;

    const Screen small = screen_of(t.session());
    const ui::Rect info_small = bounds_of(t.session().panels, panel::kInfo, small).rect;
    const ui::Rect builder_small = bounds_of(t.session().panels, panel::kBuilder, small).rect;

    t.publish(loom::to_value(surface::SurfaceExtent{140, 44, 0, 0}));

    const Screen large = screen_of(t.session());
    const ui::Rect info_large = bounds_of(t.session().panels, panel::kInfo, large).rect;
    const ui::Rect builder_large = bounds_of(t.session().panels, panel::kBuilder, large).rect;

    // THE RESOLVED GEOMETRY MOVED...
    CHECK(large.w != small.w);
    CHECK_FALSE(info_large == info_small);
    CHECK(info_large.x != info_small.x);
    CHECK(builder_large.h == builder_small.h); // the stack's slot is a fixed size...
    CHECK(large.room_w != small.room_w);       // ...and the room around it is not

    // ...AND NOTHING AUTHORED DID. Same value, same references, same bytes, and
    // still saved -- which is the control that makes this a claim about
    // persisted geometry rather than about recomposition.
    CHECK(t.session().setup.active == authored);
    CHECK(t.session().setup.saved());
    CHECK(slurp(t.host.setup_path) == bytes);

    // Restoring under the new extent produces the same intent and the current
    // rectangles, never the ones the file was written under.
    t.key(input::scan::kR);
    CHECK(t.session().setup.active == authored);
    CHECK(bounds_of(t.session().panels, panel::kInfo, large).rect == info_large);

    // A text metric moves the same picture again, and the setup is untouched.
    t.publish(loom::to_value(surface::SurfaceExtent{140, 44, 8, 18}));
    CHECK(t.session().setup.active == authored);
    CHECK(t.session().setup.saved());
    CHECK(slurp(t.host.setup_path) == bytes);

    // And no rectangle, placement, column, row or metric is in the file at all.
    for (const char* forbidden : {"\"x\"", "\"y\"", "\"w\"", "\"h\"", "rect", "columns", "rows",
                                  "advance", "extent", "placement", "slot"}) {
        CAPTURE(forbidden);
        CHECK(bytes.find(forbidden) == std::string::npos);
    }
}

// ---- Two processes ------------------------------------------------------------------

TEST_CASE("a maker names a setup, leaves, and gets it back in a fresh Workshop") {
    // THE PRODUCT OUTCOME, deterministically: two independent Workshops, two
    // independent buses, one file between them.
    TempDir dir("setup-two-runs");
    const std::string path = dir.file("setup.json");

    std::string bytes;
    {
        // RUN A: a fresh Workshop opens with Info; the maker opens Builder,
        // removes Info, names the setup and saves.
        Live a;
        a.host.setup_path = path;
        (void)mount_tool(a, "zengine-snake");
        REQUIRE(open_kinds(a.session().panels) == std::vector<std::int64_t>{panel::kInfo});

        open_builder(a);
        pick(a, panel::kInfo);
        name_setup(a, "Morning build");

        REQUIRE(a.session().setup.saved());
        REQUIRE(open_kinds(a.session().panels) == std::vector<std::int64_t>{panel::kBuilder});
        bytes = slurp(path);
        REQUIRE_FALSE(bytes.empty());
    }

    {
        // RUN B: a fresh Workshop begins from its ordinary default, and the
        // maker restores.
        Live b;
        b.host.setup_path = path;
        ToolSeat* tool = mount_tool(b, "zengine-snake");
        REQUIRE(b.session().setup.active == default_setup());
        REQUIRE(open_kinds(b.session().panels) == std::vector<std::int64_t>{panel::kInfo});
        const WorkshopDoc opening_document = b.doc();
        const std::int64_t opening_workspace = b.session().workspace_w;

        b.key(input::scan::kR);

        CHECK(b.session().setup.active.name == "Morning build");
        CHECK(b.session().panels.has(panel::kBuilder));
        CHECK_FALSE(b.session().panels.has(panel::kInfo));
        CHECK(b.session().setup.saved());

        // BUILDER ASKS ITS STILL-INDEPENDENT TOOL FOR CURRENT STATUS -- the
        // tool's own answer, not a copy that rode the file.
        CHECK(tool->described == 1);
        CHECK(b.session().panels.builder.shown.target == "zengine-snake");

        // AND NEITHER DOCUMENT CONTENT NOR THE CURRENT SCREEN EXTENT CAME OUT OF
        // THE SETUP. Both are what this run had before the restore.
        CHECK(b.doc() == opening_document);
        CHECK(b.session().workspace_w == opening_workspace);

        // The file is unchanged by having been read.
        CHECK(slurp(path) == bytes);
    }
}

TEST_CASE("saving and restoring a setup does not touch the document or its saved status") {
    TempDir dir("setup-doc-separation");
    Live t;
    t.host.document_path = dir.document();
    t.host.setup_path = dir.file("setup.json");
    (void)mount_tool(t, "zengine-snake");

    // Save the document, then do a whole setup round trip over the top of it.
    t.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE(t.notice().find("saved " + t.host.document_path) == 0);
    const std::string document_bytes = slurp(t.host.document_path);
    const WorkshopDoc document = t.doc();

    open_builder(t);
    name_setup(t, "Build");
    t.key(input::scan::kR);

    CHECK(t.doc() == document);
    CHECK(slurp(t.host.document_path) == document_bytes);
    // The document status line still says the document is saved -- a setup round
    // trip is not a document edit.
    CHECK(t.notes.back().text.find(t.host.document_path + " saved") != std::string::npos);

    // And the two commands stayed apart: `^o` loads the document and leaves the
    // setup exactly where it was.
    const Setup setup_before = t.session().setup.active;
    t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(t.notice().find("loaded " + t.host.document_path) == 0);
    CHECK(t.session().setup.active == setup_before);
    CHECK(t.session().setup.saved());
}

// ---- What a maker reads --------------------------------------------------------------

TEST_CASE("the setup line names the setup, its file, whether it is saved, and its gestures") {
    TempDir dir("setup-line");
    Live t;
    t.host.setup_path = dir.file("s.json");

    const Screen sc = screen_of(t.session());
    const std::string fresh = setup_row(first_frame(t), sc);
    INFO(fresh);
    CHECK(fresh.find("setup \"Default\"") == 0);
    CHECK(fresh.find("UNSAVED") != std::string::npos);

    name_setup(t, "Named");
    const std::string saved = setup_row(t.canvases.back(), sc);
    CHECK(saved.find("setup \"Named\" saved") == 0);

    // AT THE MINIMUM COMPOSITION WITH THE DEFAULT FILE NAME THE WHOLE LINE FITS,
    // and that is the measurement the ORDER of that line was chosen against: the
    // name, the marker, the file and both gestures, in 78 cells with room over.
    Live plain;
    plain.host.setup_path = kDefaultSetupFileName;
    const std::string minimal = setup_row(first_frame(plain), screen_of(plain.session()));
    INFO(minimal);
    CHECK(minimal.find("setup \"Default\" UNSAVED") == 0);
    CHECK(minimal.find(kDefaultSetupFileName) != std::string::npos);
    CHECK(minimal.find("s name/save") != std::string::npos);
    CHECK(minimal.find("r restore") != std::string::npos);
    CHECK(static_cast<std::int64_t>(minimal.size()) <= kMinScreen.w);
    CHECK(minimal.find("...") == std::string::npos);

    // AND A LINE TOO LONG FOR THE ROOM IS CUT WITH A MARK rather than silently --
    // which is what a canvas label buys that the status slot could not, and the
    // reason the identity is at the front: it is never what elides.
    Live wordy;
    wordy.host.setup_path = std::string(90, 'p');
    const std::string cut = setup_row(first_frame(wordy), screen_of(wordy.session()));
    CHECK(static_cast<std::int64_t>(cut.size()) == kMinScreen.w);
    CHECK(cut.substr(cut.size() - 3) == "...");
    CHECK(cut.find("setup \"Default\" UNSAVED") == 0);

    // A wider surface spends the room it gained on the rest of the sentence,
    // which needs nothing from anybody but room.
    wordy.publish(loom::to_value(surface::SurfaceExtent{160, 40, 0, 0}));
    const std::string roomy = setup_row(wordy.canvases.back(), screen_of(wordy.session()));
    CHECK(roomy.find("...") == std::string::npos);
    CHECK(roomy.find(std::string(90, 'p')) != std::string::npos);
    CHECK(roomy.find("r restore") != std::string::npos);
}

TEST_CASE("the setup line becomes the name editor while a maker is typing") {
    TempDir dir("setup-editor-line");
    Live t;
    t.host.setup_path = dir.file("s.json");

    t.key(input::scan::kS);
    t.text("s");
    const Screen sc = screen_of(t.session());
    const std::string row = setup_row(t.canvases.back(), sc);
    INFO(row);
    CHECK(row.find("setup name> Default") == 0);
    // THE CARET IS IN THE TEXT, at the end where `s` left it. A one-cell-tall
    // bounded region would hold zero rows of a real face (HD-6), so this editor
    // says its caret the way the cell projection says a region's.
    CHECK(row.find(std::string("Default") + surface::kCaretGlyph) != std::string::npos);
    CHECK(row.find("enter saves") != std::string::npos);
    CHECK(row.find("esc cancels") != std::string::npos);
    CHECK(static_cast<std::int64_t>(row.size()) <= sc.w);

    // The caret follows the maker's hand, and a character typed at it lands there.
    t.key(input::scan::kLeft);
    t.key(input::scan::kLeft);
    t.text("X");
    CHECK(t.session().setup.naming.line.text() == "DefauXlt");
    CHECK(setup_row(t.canvases.back(), sc).find(std::string("DefauX") + surface::kCaretGlyph) !=
          std::string::npos);
}

TEST_CASE("the name editor takes the keys, and the picker and the document keep theirs") {
    TempDir dir("setup-modes");
    Live t;
    t.host.setup_path = dir.file("s.json");

    t.key(input::scan::kS);
    t.text("s");
    REQUIRE(t.session().setup.naming.open);

    // `p` DOES NOT OPEN THE PICKER while the editor has the keys: it is a
    // character, and the editor is the mode above command mode.
    t.text("p");
    CHECK_FALSE(t.session().panels.picker.open);
    CHECK(t.session().setup.naming.line.text() == "Defaultp");

    // ...and `q` does not quit.
    t.text("q");
    CHECK_FALSE(t.host.quit);
    CHECK(t.session().setup.naming.line.text() == "Defaultpq");

    // The two commands that mean the same thing in every mode still do: `^s`
    // saves the DOCUMENT, from inside the setup-name editor, and says so.
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice().find("no document file") == 0);

    // The picker and the name editor cannot both be open: `s` is a command, and
    // the picker takes the keys before command mode is reached.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.session().setup.naming.open);
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    t.key(input::scan::kS);
    t.text("s");
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().panels.picker.open);
}

TEST_CASE("the picker's own state never reaches the setup file") {
    TempDir dir("setup-no-picker");
    Live t;
    t.host.setup_path = dir.file("s.json");

    // Leave the picker's cursor somewhere deliberate, then save.
    t.key(input::scan::kP);
    t.key(input::scan::kDown);
    REQUIRE(t.session().panels.picker.cursor == 1);
    t.key(input::scan::kEscape);
    name_setup(t, "Clean");

    const std::string bytes = slurp(t.host.setup_path);
    CHECK(bytes.find("cursor") == std::string::npos);
    CHECK(bytes.find("picker") == std::string::npos);
    CHECK(bytes.find("terminal") == std::string::npos);
    CHECK(bytes.find("selected") == std::string::npos);
    CHECK(bytes.find("notice") == std::string::npos);

    // And restoring opens no picker: `r` is a command, so the picker was closed
    // before it could run, and nothing in a restore opens one.
    t.key(input::scan::kR);
    CHECK_FALSE(t.session().panels.picker.open);
}

// ---- WS-0a: the sentence that quotes a name owns the escaping ------------------
//
// WS-0 accepts `"` and `\` in a setup name and persists those bytes exactly, which is
// the right law and stays the law. What it did not own was the PROSE: three callers
// spelled the quoted token by hand, so a legal name could manufacture the delimiter a
// maker uses to tell an identity from the status beside it. These cases pin the
// spelling, all three callers, the authored bytes on both sides of it, and the unit the
// two existing bounds have always been counted in.

namespace {

/// READ ONE QUOTED TOKEN BACK THE WAY A MAKER'S EYE DOES -- and deliberately written
/// against the RULE rather than against `quoted_setup_name`, so that this is a second,
/// independent implementation and not a mirror of the first. An opening quote, then
/// bytes in which a backslash escapes whatever follows it, and the first UNESCAPED
/// quote ends the name.
///
/// It is what makes "the name cannot terminate its own token" a measurable claim
/// instead of a hopeful one: with the token built by raw interpolation this recovers
/// the wrong name, which is exactly the ambiguity WS-0a exists to close.
struct QuotedToken {
    bool well_formed = false;
    std::string name; ///< the bytes the token means
    std::size_t end = 0; ///< one past the token's closing quote
};

QuotedToken read_quoted(const std::string& line, std::size_t at) {
    QuotedToken t;
    if (at >= line.size() || line[at] != '"') {
        return t;
    }
    for (std::size_t i = at + 1; i < line.size(); ++i) {
        if (line[i] == '\\') {
            if (i + 1 >= line.size()) {
                return t; // a trailing escape is not a finished token
            }
            t.name += line[++i];
            continue;
        }
        if (line[i] == '"') {
            t.well_formed = true;
            t.end = i + 1;
            return t;
        }
        t.name += line[i];
    }
    return t;
}

/// A name of `n` repetitions of one byte -- the pathological shapes, said once.
std::string repeated(std::size_t n, char c) { return std::string(n, c); }

/// U+1F680, written as its four UTF-8 bytes rather than as a source character, so
/// nothing here depends on this file's execution encoding or on `char8_t`. Four bytes,
/// one code point, one character a maker would count.
constexpr const char* kFourByteChar = "\xF0\x9F\x9A\x80";

std::string four_byte_chars(std::size_t count) {
    std::string out;
    for (std::size_t i = 0; i < count; ++i) {
        out += kFourByteChar;
    }
    return out;
}

} // namespace

TEST_CASE("WS-0a: a setup name is spelled into prose as one unambiguous quoted token") {
    // ORDINARY NAMES ARE UNCHANGED, exactly, and this is the control the whole cleanup
    // is measured against: the sentence a maker has been reading since WS-0 must come
    // back byte-for-byte.
    CHECK(quoted_setup_name("Default") == "\"Default\"");
    CHECK(quoted_setup_name("Morning build") == "\"Morning build\"");
    CHECK(quoted_setup_name(repeated(kMaxSetupNameLen, 'n')) ==
          "\"" + repeated(kMaxSetupNameLen, 'n') + "\"");

    // A QUOTE CANNOT TERMINATE THE TOKEN. The exact output, because a case that only
    // looked for `\"` somewhere in the string would pass with a second, unescaped quote
    // still sitting further along it.
    CHECK(quoted_setup_name("Ops\" UNSAVED | decoy") == "\"Ops\\\" UNSAVED | decoy\"");

    // A BACKSLASH CANNOT DISGUISE WHETHER THE QUOTE AFTER IT WAS AUTHORED.
    CHECK(quoted_setup_name("A\\B") == "\"A\\\\B\"");
    CHECK(quoted_setup_name("A\\\"B") == "\"A\\\\\\\"B\"");

    // TOTAL, including on input no valid `Setup` can hold: the law refuses an empty
    // name, so this is a helper's boundary rather than a reachable state, and a
    // presentation spelling that had an opinion about which inputs it would answer for
    // would be a second law in a second place.
    CHECK(quoted_setup_name("") == "\"\"");
    CHECK_FALSE(check_setup_name("").accepted);

    // THE SUBSTITUTION IS INJECTIVE, which is the property a lossy repair (rendering a
    // quote as an apostrophe) would give up: two names a maker can tell apart must not
    // present as one.
    CHECK(quoted_setup_name("A\"B") != quoted_setup_name("A\\\"B"));

    // ...and the whole of it, stated once: applying the escape rule in reverse recovers
    // the authored bytes, for every shape this phase is about.
    const std::vector<std::string> names = {
        "Default",       "Morning build", "Ops\" UNSAVED | decoy", "A\\B",
        "A\\\"B",        "\"",            "\\",                    "\\\\\"\"",
        "quote\"in\"it", repeated(kMaxSetupNameLen, '"'),
    };
    for (const std::string& authored : names) {
        CAPTURE(authored);
        const QuotedToken read = read_quoted(quoted_setup_name(authored), 0);
        CHECK(read.well_formed);
        CHECK(read.name == authored);
        CHECK(read.end == quoted_setup_name(authored).size());

        // The raw interpolation this replaced, asked the same question: it is a token a
        // reader recovers the WRONG name from the moment the name carries a quote.
        const QuotedToken naive = read_quoted("\"" + authored + "\"", 0);
        if (authored.find('"') != std::string::npos ||
            authored.find('\\') != std::string::npos) {
            CHECK((!naive.well_formed || naive.name != authored));
        }
    }
}

TEST_CASE("WS-0a: a name that could impersonate the setup line is one token on it") {
    TempDir dir("ws0a-status");
    Live t;
    t.host.setup_path = dir.file("s.json");

    // A NAME BUILT TO LIE. Every byte of it is legal under WS-0's law and stays legal:
    // this case is about the sentence, not about the name.
    const std::string authored = "Ops\" UNSAVED | decoy";
    REQUIRE(check_setup_name(authored).accepted);
    name_setup(t, authored);
    REQUIRE(t.session().setup.active.name == authored);
    REQUIRE(t.session().setup.saved());

    const Screen sc = screen_of(t.session());
    const std::string row = setup_row(t.canvases.back(), sc);
    INFO(row);

    // THE IDENTITY IS ONE TOKEN, and a reader applying the escape rule gets the maker's
    // own bytes back out of it.
    REQUIRE(row.compare(0, 6, "setup ") == 0);
    const QuotedToken read = read_quoted(row, 6);
    REQUIRE(read.well_formed);
    CHECK(read.name == authored);

    // ...AND EXACTLY ONE SAVED MARKER, the real one, OUTSIDE the token. The decoy word
    // inside the name is not it, and the proof is positional rather than a search: the
    // status begins where the token ends.
    CHECK(row.compare(read.end, 6, " saved") == 0);
    CHECK(row.find("UNSAVED", read.end) == std::string::npos);

    // The row is still one bounded row of the band, and the file is still named on it.
    CHECK(static_cast<std::int64_t>(row.size()) <= sc.w);

    // AND THE AUTHORED BYTES NEVER MOVED. The escaped spelling is prose and reaches
    // neither the live setup nor the copy `saved()` compares against.
    CHECK(t.session().setup.active.name == authored);
    CHECK(t.session().setup.on_file.name == authored);
    CHECK(slurp(t.host.setup_path).find("\\\" UNSAVED") != std::string::npos);
}

TEST_CASE("WS-0a: the save notice and the restore notice spell the name the same way") {
    TempDir dir("ws0a-notices");
    Live t;
    t.host.setup_path = dir.file("s.json");

    const std::string authored = "Ops \"A\\B\"";
    REQUIRE(check_setup_name(authored).accepted);

    name_setup(t, authored);
    const std::string saved = t.notice();
    INFO(saved);
    REQUIRE(saved.compare(0, 12, "saved setup ") == 0);
    const QuotedToken after_save = read_quoted(saved, 12);
    REQUIRE(after_save.well_formed);
    CHECK(after_save.name == authored);
    // The PATH is named the ordinary way -- it is not a setup name and gains no
    // escaping from this phase.
    CHECK(saved.compare(after_save.end, 4, " to ") == 0);
    CHECK(saved.find(t.host.setup_path) != std::string::npos);

    t.key(input::scan::kR);
    const std::string restored = t.notice();
    INFO(restored);
    REQUIRE(restored.compare(0, 15, "restored setup ") == 0);
    const QuotedToken after_restore = read_quoted(restored, 15);
    REQUIRE(after_restore.well_formed);
    // NOT REINTERPRETED AND NOT NORMALISED on the way back: the name that comes out of
    // the file is the name that went into it, and the notice says those bytes.
    CHECK(after_restore.name == authored);
    CHECK(restored.compare(after_restore.end, 6, " from ") == 0);
    CHECK(t.session().setup.active.name == authored);

    // ONE OWNER, PROVEN BY AGREEMENT: both notices and the status line carry the
    // identical token, so a caller that resumed improvising its own would be named here
    // rather than only in whichever case happened to cover it.
    const std::string token = quoted_setup_name(authored);
    CHECK(saved.find(token) == 12);
    CHECK(restored.find(token) == 15);
    CHECK(setup_row(t.canvases.back(), screen_of(t.session())).find(token) == 6);
}

TEST_CASE("WS-0a: the name editor edits the authored bytes, never the escaped spelling") {
    TempDir dir("ws0a-editor");
    Live t;
    t.host.setup_path = dir.file("s.json");

    // TYPED FROM SCRATCH, character by character, exactly as a maker produces it: the
    // quote and the backslash arrive as ordinary text and are stored as themselves.
    const std::string authored = "Ops \"A\\B\"";
    name_setup(t, authored);
    REQUIRE(t.session().setup.active.name == authored);

    // REOPENED ON THE NAME IT ALREADY HAS -- and what the editor holds is the ORIGINAL
    // bytes. A maker does not have to type `\"` to mean `"` in their own name.
    t.key(input::scan::kS);
    t.text("s");
    REQUIRE(t.session().setup.naming.open);
    CHECK(t.session().setup.naming.line.text() == authored);
    CHECK(t.session().setup.naming.line.text()[6] == '\\'); // the raw backslash, stored as one

    const Screen sc = screen_of(t.session());
    const std::string row = setup_row(t.canvases.back(), sc);
    INFO(row);
    // The editing row is not a quoted sentence, so it is not an escaped one either: the
    // prompt, the raw name, the caret where the maker's hand left it, and the hint.
    CHECK(row.find(std::string("setup name> ") + authored + surface::kCaretGlyph) == 0);
    CHECK(row.find(quoted_setup_name(authored)) == std::string::npos);

    // ESCAPE CHANGES NOTHING, and the name is still the authored one.
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == authored);
    CHECK(t.session().setup.saved());

    // AND ENTER SAVES THE AUTHORED BYTES, not the spelling they are presented with.
    t.key(input::scan::kS);
    t.text("s");
    t.text("!");
    t.key(input::scan::kReturn);
    const std::string grown = authored + "!";
    CHECK(t.session().setup.active.name == grown);
    const setup_persist::LoadedSetup back = setup_persist::load_file(t.host.setup_path);
    REQUIRE(back.outcome.accepted);
    CHECK(back.setup.name == grown);
    CHECK(back.setup.name.find("\\\"") == std::string::npos); // no escape was stored
}

TEST_CASE("WS-0a: an ordinary setup name presents exactly as it did before") {
    // THE GREEN CONTROL FOR THE WHOLE CLEANUP. Not one byte of the sentence a maker
    // without a quote in their name reads may have moved, and every one of these
    // strings is spelled out here rather than composed, so that a change to the
    // presentation owner cannot quietly agree with itself.
    TempDir dir("ws0a-ordinary");
    Live t;
    t.host.setup_path = dir.file("s.json");

    const Screen sc = screen_of(t.session());
    const std::string fresh = setup_row(first_frame(t), sc);
    CHECK(fresh.find("setup \"Default\" UNSAVED") == 0);

    name_setup(t, "Morning build");
    CHECK(t.notice().find("saved setup \"Morning build\" to ") == 0);
    CHECK(setup_row(t.canvases.back(), sc).find("setup \"Morning build\" saved") == 0);

    t.key(input::scan::kR);
    CHECK(t.notice().find("restored setup \"Morning build\" from ") == 0);

    // ...and the name a fresh Workshop carries is spelled the way the constant is.
    CHECK(quoted_setup_name(kDefaultSetupName) == "\"Default\"");
}

TEST_CASE("WS-0a: an escaped name that outgrows the row is cut with the mark") {
    TempDir dir("ws0a-fit");

    // EXPANSION ALONE IS NOT A PROBLEM. Twelve quotes double to twenty-four and the
    // whole sentence still fits, which is the control that keeps the case below about
    // the BOUND rather than about escaping.
    Live modest;
    modest.host.setup_path = dir.file("m.json");
    name_setup(modest, repeated(12, '"'));
    const std::string easy = setup_row(modest.canvases.back(), screen_of(modest.session()));
    INFO(easy);
    CHECK(read_quoted(easy, 6).name == repeated(12, '"'));
    CHECK(easy.find(" saved") != std::string::npos);

    // THE PATHOLOGICAL LEGAL NAME: thirty-two bytes at the bound, every one of them a
    // quote, so the token is sixty-six cells of a seventy-eight cell row.
    Live t;
    t.host.setup_path = dir.file("p.json");
    const std::string authored = repeated(kMaxSetupNameLen, '"');
    REQUIRE(check_setup_name(authored).accepted);
    name_setup(t, authored);
    REQUIRE(t.session().setup.active.name == authored);
    REQUIRE(t.session().setup.saved());

    const std::string cut = setup_row(t.canvases.back(), screen_of(t.session()));
    INFO(cut);
    CHECK(static_cast<std::int64_t>(cut.size()) == kMinScreen.w);
    CHECK(cut.substr(cut.size() - 3) == "...");
    // IT CANNOT RUN UNMARKED INTO WHAT COMES AFTER IT. The existing `detail::fit` is
    // the whole of the answer -- the sentence is fitted once, at the presentation
    // boundary, after the token is formed -- so nothing downstream of the identity is
    // shown as though it were complete.
    CHECK(cut.find("s name/save") == std::string::npos);
    CHECK(cut.find(t.host.setup_path) == std::string::npos);

    // THE CUT IS PRESENTATION AND NOTHING ELSE: the name, the saved comparison and the
    // file are all untouched by it, and a wider surface spends the room on the rest of
    // the same sentence.
    CHECK(t.session().setup.active.name == authored);
    CHECK(t.session().setup.saved());
    CHECK(setup_persist::load_file(t.host.setup_path).setup.name == authored);

    t.publish(loom::to_value(surface::SurfaceExtent{240, 40, 0, 0}));
    const std::string roomy = setup_row(t.canvases.back(), screen_of(t.session()));
    INFO(roomy);
    CHECK(roomy.find("...") == std::string::npos);
    CHECK(read_quoted(roomy, 6).name == authored);
    CHECK(roomy.find("s name/save") != std::string::npos);
    // ...and the extent changed what fits, never the setup or its saved status.
    CHECK(t.session().setup.active.name == authored);
    CHECK(t.session().setup.saved());
}

TEST_CASE("WS-0a: a name carrying a quote and a backslash survives its file exactly") {
    TempDir dir("ws0a-persist");
    const std::string authored = "Ops \"A\\B\"";
    const Setup s = setup_of(authored, {panel::kInfo, panel::kBuilder});
    REQUIRE(check_setup(s).accepted);

    // THE FILE IS UNCHANGED BY THIS PHASE. Its identity, its version and its writer are
    // the ones accepted WS-0 shipped -- setup_persist.hpp is not touched -- so the bytes
    // this writes ARE a WS-0 file, and reading them back is the compatibility witness.
    CHECK(setup_persist::kFormatVersion == 1);
    CHECK(std::string(setup_persist::kFormat) == "zengine-workshop-setup");

    const std::string path = dir.file("q.json");
    REQUIRE(setup_persist::save_file(path, s).accepted);
    const std::string a = slurp(path);
    INFO(a);
    CHECK(a.find("\"format\":\"zengine-workshop-setup\"") != std::string::npos);
    CHECK(a.find("\"format_version\":\"1\"") != std::string::npos);

    const setup_persist::LoadedSetup read = setup_persist::load_file(path);
    REQUIRE(read.outcome.accepted);
    // EXACT AUTHORED BYTES, and not the spelling any sentence presents them with.
    CHECK(read.setup.name == authored);
    CHECK(read.setup == s);

    REQUIRE(setup_persist::save_file(path, read.setup).accepted);
    const std::string b = slurp(path);
    CHECK(a == b); // save -> load -> save, byte-identical, with a quote in the name

    // THE COMPAT CODEC OWNS THE FILE'S OWN ESCAPING and always did; nothing here
    // hand-authored a second one, and the escaped PROSE spelling was never stored.
    CHECK(a.find("Ops \\\"A\\\\B\\\"") != std::string::npos);
    CHECK(quoted_setup_name(read.setup.name) == "\"Ops \\\"A\\\\B\\\"\"");
}

TEST_CASE("WS-0a: the name and key bounds are BYTES, and the refusal says bytes") {
    // THE BOUNDS THEMSELVES ARE UNCHANGED. WS-0a corrects a false unit in a sentence
    // and nothing about what is accepted.
    CHECK(kMaxSetupNameLen == 32);
    CHECK(kMaxPaneKeyLen == 64);

    // EIGHT FOUR-BYTE CHARACTERS ARE THIRTY-TWO BYTES -- eight characters a maker
    // counts, and exactly the bound `std::string::size()` measures.
    const std::string at_bound = four_byte_chars(8);
    REQUIRE(at_bound.size() == kMaxSetupNameLen);
    CHECK(check_setup_name(at_bound).accepted);

    // ...AND ONE MORE BYTE IS REFUSED, saying the unit it was refused in.
    const Written over = check_setup_name(at_bound + "x");
    CHECK_FALSE(over.accepted);
    CHECK(over.refusal == "a setup name is at most 32 bytes");

    // THE SHARPEST ILLUSTRATION OF THE UNIT: nine characters, thirty-six bytes, refused
    // -- so a refusal saying "at most 32 characters" would be a false sentence about a
    // true refusal. That is the whole of what this correction is.
    const std::string nine = four_byte_chars(9);
    CHECK(nine.size() == 36);
    CHECK_FALSE(check_setup_name(nine).accepted);
    CHECK(check_setup_name(nine).refusal == "a setup name is at most 32 bytes");

    // THE SAME QUESTION OF THE KEY BOUND, both halves of a reference.
    const std::string key_at_bound = four_byte_chars(16);
    REQUIRE(key_at_bound.size() == kMaxPaneKeyLen);
    CHECK(check_pane_key(key_at_bound, "provider").accepted);
    CHECK(check_pane_key(key_at_bound + "x", "provider").refusal ==
          "a pane reference's provider is at most 64 bytes");
    CHECK(check_pane_key(key_at_bound + "x", "pane key").refusal ==
          "a pane reference's pane key is at most 64 bytes");
    CHECK(check_pane_ref(PaneRef{"zengine.workshop", four_byte_chars(17)}).refusal ==
          "a pane reference's pane key is at most 64 bytes");

    // NO UNICODE POLICY WAS BOUGHT WITH THIS, and the absence is asserted rather than
    // merely intended. Workshop counts bytes and nothing else: it has no opinion about
    // how many code points, graphemes or CELLS a name occupies, and a thirty-third byte
    // is refused whatever a character count would have said about it.
    CHECK(check_setup_name(four_byte_chars(1)).accepted);   // 4 bytes, 1 character
    CHECK(check_setup_name(repeated(4, 'a')).accepted);     // 4 bytes, 4 characters
    CHECK_FALSE(check_setup_name(at_bound + " ").accepted); // 9 characters, 33 bytes
}
