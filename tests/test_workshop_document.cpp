// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop document suite — the authored material, and the maker's hands on it.
//
// Everything here is headless and pure, or drives the real weave on a real bus. That is
// not a limitation of the suite, it is a property of the design: the authored objects are
// plain data, the operations on them are functions that can refuse, and a gesture is an
// ordinary message. Nothing in Workshop's own logic needs a terminal, so nothing here
// has one.
//
// What it holds:
//   1. THE VOCABULARY — the document is ordinary content, and identity is not the name.
//   2. THE PROPERTY CONNECTION — read through the semantic surface, commit through it,
//      and the two ways a commit can fail told apart. Includes the reuse pin: two
//      properties of one type share every line of conversion.
//   3. AUTHORED INTENT VERSUS RESOLVED VALUE, as a maker meets it, and selection against
//      the real authored objects.
//   4. THE MAKER'S OWN HANDS — create, move, delete, and the hand on an EXTENT.
//   5. THE GESTURE PATH — input moments becoming maker gestures through the real weave,
//      and which layer consumes a press.
//   6. EDITABLE TEXT AS A COMPONENT — the property draft is a TextBox, end to end through
//      the weave, and clipboard reads follow paste intent.
//   7. THE KEYMAP — one executable binding truth: the context resolver, the authored file,
//      the legend, and the chord.
//
// The screen these cases paint is asserted in `test_workshop_screen.cpp`; the panels they
// open are `test_workshop_panels.cpp`; what survives a process is
// `test_workshop_persistence.cpp`.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

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

TEST_CASE("QR-3: what the authored name bound IS, and what it is not a statement about") {
    // THE POLICY, PINNED WHERE IT IS DECIDED. This bound arrived as 32 with the words
    // `a label, not a document` and no other rationale anywhere in the history or the docs.
    // The only rationale ever written for a 32 in this
    // application is a SCREEN measurement (`setup::kMaxSetupNameLen`), and it was measurably
    // false for this constant -- so the KIND of bound survived and the number moved.
    CHECK(doc::kMaxNameLen == 64);

    // IT IS AN INPUT BOUNDARY AND NOT A CAPACITY, which is what makes it one function rather
    // than two: a maker's rename and a document from a FILE meet the same rule, in the same
    // words. Exactly at the bound is legal; one byte past it is not, in both directions.
    CHECK(doc::check_name(std::string(doc::kMaxNameLen, 'x')).accepted);
    CHECK_FALSE(doc::check_name(std::string(doc::kMaxNameLen + 1, 'x')).accepted);
    CHECK(doc::check_name(std::string(doc::kMaxNameLen + 1, 'x')).refusal ==
          "a name is at most 64 characters");
    CHECK_FALSE(doc::check_name(std::string()).accepted);

    // A NAME THE OLD BOUND REFUSED IS AUTHORED THROUGH THE ORDINARY OPERATION NOW, and the
    // one the suite itself has needed since HD-7 is the fixture: 43 bytes, which HD-7 could
    // only get by writing the element's field directly because no maker could type it.
    WorkshopDoc d = two_panels();
    const std::int64_t id = d.elements[0].id;
    const std::string long_name = "the-quick-brown-fox-jumps-over-the-lazy-dog";
    REQUIRE(long_name.size() == 43);
    CHECK(doc::rename(d, id, long_name).accepted);
    CHECK(doc::find(d, id)->label == long_name);
    // ...and the whole document is still a document by its own law.
    CHECK(doc::check_document(d).accepted);

    // AND IT SURVIVES A SAVE AND A RESTORE, byte for byte. A validation policy is not a wire
    // change: `kFormatVersion` did not move, and a name is an ordinary string in the file.
    const std::string text = persist::to_text(d);
    CHECK(text.find(long_name) != std::string::npos);
    WorkshopDoc back = two_panels();
    const Written loaded = persist::load_into(back, text);
    REQUIRE(loaded.accepted);
    CHECK(doc::find(back, id)->label == long_name);
    CHECK(back == d);
    CHECK(persist::kFormatVersion == 1);

    // NOTHING ABOUT THE SETUP'S OWN NAME MOVED WITH IT. Two bounds, two reasons: the setup
    // line has a width it must fit whole and an object's name no longer has one.
    CHECK(kMaxSetupNameLen == 32);
}

TEST_CASE("QR-3: a name past the authored bound is still refused, from a file as from a hand") {
    // THE OTHER HALF: raising a bound is not removing one. A document from a file meets the
    // same `check_name`, and a refusal leaves the live document exactly as it was.
    WorkshopDoc good = two_panels();
    const std::string text = persist::to_text(good);
    const std::string over(doc::kMaxNameLen + 1, 'x');
    std::string forged = text;
    const std::string was = "\"name\":\"panel\"";
    const std::size_t at = forged.find(was);
    REQUIRE(at != std::string::npos);
    forged.replace(at, was.size(), "\"name\":\"" + over + "\"");

    WorkshopDoc live = two_panels();
    const Written refused = persist::load_into(live, forged);
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal.find("at most 64") != std::string::npos);
    CHECK(live == good); // not one byte of the live document was written

    // And through the maker's own operation.
    CHECK_FALSE(doc::rename(live, live.elements[0].id, over).accepted);
    CHECK(live.elements[0].label == "panel");
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
    // The object's authored top-left (3,2) is terminal (3, 2 + kWorkspaceY + the terminal's
    // own two rows) -- 6 since QR-14 put a band above the workspace, 5 before it.
    const std::int64_t top = 2 + kWorkspaceY + surface::kTuiCanvasTopRow;
    CHECK(begin_drag(d, s, term(3, top).first, term(3, top).second) == id);
    CHECK(s.drag.grab_dx == 0);
    CHECK(s.drag.grab_dy == 0);
    end_drag(s);
    CHECK(begin_drag(d, s, term(3, top - 1).first, term(3, top - 1).second) == 0); // above
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
    CHECK(t.status_note().rfind("[workshop] 2 objects", 0) == 0);
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
        CHECK(t.status_note().rfind("[workshop] 9 objects | selected #" + std::to_string(id), 0) ==
              0);
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
    const std::string& drawn =
        shown->rows[static_cast<std::size_t>(kInfoHeadingRows + at_row)].text;
    const std::string slice = row->editor().visible(place.value_columns);
    CHECK(drawn == property_row_text(*row, true, place.value_columns));
    CHECK(static_cast<std::int64_t>(slice.size()) <= place.value_columns);
    CHECK(kLongValue.substr(kLongValue.size() - slice.size()) == slice);
    CHECK(shown->caret_row == kInfoHeadingRows + at_row);
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
               cells.region_y + kInfoHeadingRows + cell_row + surface::kTuiCanvasTopRow,
               input::space::kCells);
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
        const ui::Rect panel =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc).rect);
        const InfoBodyPlace place = body_place(t);
        REQUIRE(place.present);

        // THE REGION IS THE PANEL'S INTERIOR SINCE WUX-5 -- the whole panel less the one
        // cell of chrome its visible boundary occupies on every side -- the OBJECTS heading
        // its first prose row, and the body's capacity the fit less that reservation.
        const ui::Rect inside = pane_body_cells(panel);
        CHECK(place.region_x == inside.x);
        CHECK(place.region_y == inside.y);
        CHECK(place.region_x + place.region_w == inside.x + inside.w);
        CHECK(place.region_y + place.region_h == inside.y + inside.h);
        CHECK(inside.x == panel.x + kChromeCells);
        CHECK(inside.w == panel.w - 2 * kChromeCells);
        CHECK(place.columns == place.fit.columns);
        CHECK(place.value_columns ==
              place.fit.columns - kPropertyMarkCols - kPropertyLabelCols - kPropertyCaretCols);
        CHECK(place.capacity == static_cast<std::size_t>(place.fit.rows - kInfoHeadingRows));

        // THE PAINTER CUT THE SLICE WITH IT...
        const surface::SurfaceTextRegion* shown = body_region(t.canvases.back(), place);
        REQUIRE(shown != nullptr);
        CHECK(shown->w == place.region_w);
        CHECK(shown->h == place.region_h);
        // §11: no row the body cannot hold -- the heading's own row rides above the body's
        // capacity since WUX-1.
        CHECK(shown->rows.size() <=
              static_cast<std::size_t>(kInfoHeadingRows) + place.capacity);
        const std::int64_t at_row = editing_prose_row(t, place);
        REQUIRE(at_row != kNoProseRow);
        CHECK(shown->rows[static_cast<std::size_t>(kInfoHeadingRows + at_row)].text ==
              property_row_text(*t.row("Name"), true, place.value_columns));
        // ...THE CARET IS ON IT, on both axes...
        CHECK(shown->caret_row == kInfoHeadingRows + at_row);
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
    CHECK(shown->caret_row == kInfoHeadingRows + editing_prose_row(t, wide));

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
    CHECK(inspector_row(c, place.region_x, place.region_y + kInfoHeadingRows + at_row) ==
          ">Name     abcdefg_hij");
    CHECK(label_at(c, place.region_x, place.region_y + kInfoHeadingRows + at_row).size() ==
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
                        place.region_y + kInfoHeadingRows + prose_row_of_property(place, 1)) ==
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
    t.press_at(value_x + 2,
               place.region_y + kInfoHeadingRows + at_row + 3 + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
    CHECK(t.row("Name")->editor().caret() == 5); // and the caret did not move

    // On the value: the caret moves and the notice is NOT overwritten, because the caret is
    // the statement and a sentence repeating it would push a refusal off the line.
    t.press_at(value_x + 2, place.region_y + kInfoHeadingRows + at_row + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.row("Name")->editor().caret() == 2);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");

    // And a row that is NOT being edited is not an editor: a press on its value is the
    // panel's, which is what keeps "Return opens a draft" the only way one opens.
    t.key(input::scan::kEscape);
    t.press_at(value_x + 2, place.region_y + kInfoHeadingRows + at_row + surface::kTuiCanvasTopRow,
               input::space::kCells);
    CHECK(t.notice() == "Info is here -- nothing under it can be taken hold of");
    CHECK_FALSE(t.row("Name")->editing());
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
    const std::int64_t y = place.region_y + kInfoHeadingRows + at_row + surface::kTuiCanvasTopRow;

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
    const std::int64_t y = place.region_y + kInfoHeadingRows + at_row + surface::kTuiCanvasTopRow;

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
                     body.region_y + kInfoHeadingRows + 1 + surface::kTuiCanvasTopRow);
    CHECK(where.present);
    CHECK(where.body.region_x == shown->x); // the geometry the maker is looking at
    CHECK(where.body.region_y == shown->y);
    CHECK(where.body.capacity == body.capacity);
    CHECK(where.body.action_row == body.action_row);
    CHECK(where.at.column == 3); // and located in ITS prose, not in cells of the screen
    CHECK(where.at.row == 1);    // a BODY row: the heading row above the body names none

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
                shown->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(body, which))];
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
            e->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(eb, kActionCreate))];
        const surface::SurfaceTextRow& delete_row =
            e->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(eb, kActionDelete))];
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
            drafting->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(body, which))];
        CHECK(row.background == surface::role::kNone);
        CHECK(row.role == surface::role::kMuted);
        CHECK(row.text == action_row_text(which, false, body.columns));
    }
    // AND THE ACTIVE EDIT IS STILL THE LOUDEST THING IN THE PANEL. A ground on a control while
    // a draft is live would have put a second bright row beside the one the maker is typing
    // into; the availability rule and the ground rule agree here rather than competing.
    const surface::SurfaceTextRow& editing =
        drafting->rows[static_cast<std::size_t>(kInfoHeadingRows + editing_prose_row(t, body))];
    CHECK(editing.role == surface::role::kAlert);
    CHECK(editing.background == surface::role::kNone);

    t.key(input::scan::kEscape); // cancel: the controls come back, ground and all
    const InfoBodyPlace after = body_place(t);
    const surface::SurfaceTextRegion* back = body_region(t.canvases.back(), after);
    REQUIRE(back != nullptr);
    CHECK(back->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(after, kActionCreate))]
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
            shown->rows[static_cast<std::size_t>(kInfoHeadingRows + body.heading_row)];
        CHECK(heading.text == "PROPERTIES");
        CHECK(heading.role == surface::role::kAccent); // unchanged: the ink still says what
        CHECK(heading.background == surface::role::kMuted);

        const surface::SurfaceTextRow& selected =
            shown->rows[static_cast<std::size_t>(kInfoHeadingRows +
                                                 prose_row_of_object(body, 2))];
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
        REQUIRE(shown->rows.size() == static_cast<std::size_t>(kInfoHeadingRows) + body.capacity);
        std::size_t grounded = 0;
        for (std::size_t i = 0; i < shown->rows.size(); ++i) {
            CAPTURE(i);
            // Region row i is body row i - kInfoHeadingRows; the OBJECTS heading itself
            // (body row -1) is deliberately ungrounded -- accent ink alone, like the
            // selected object row.
            const std::int64_t body_row = static_cast<std::int64_t>(i) - kInfoHeadingRows;
            const std::size_t which = action_at_prose_row(body, body_row);
            const bool structural =
                body_row == body.heading_row ||
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
    plane(only).texts.push_back(*shown);
    const std::vector<surface::ProjectedRow> rows = projected_of(only);
    REQUIRE(rows.size() == static_cast<std::size_t>(shown->h));

    const std::size_t heading = static_cast<std::size_t>(kInfoHeadingRows + body.heading_row);
    CHECK(rows[heading].background == surface::role::kMuted);
    CHECK(rows[heading].label.text.size() == static_cast<std::size_t>(shown->w));
    CHECK(rows[heading].label.text.rfind("PROPERTIES", 0) == 0);
    CHECK(rows[heading].label.text.back() == ' '); // padded, and the ground rides the padding

    const std::size_t create =
        static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(body, kActionCreate));
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
    // AND THE GROUND STOPS WHERE THE PANEL'S INTERIOR DOES (WUX-5). Before the panel had a
    // visible boundary its region reached the canvas row's own end, so a grounded row was
    // ended by the row itself and `\x1b[49m` never appeared. The boundary is an ungrounded
    // cell ON the same row now, so the writer closes the ground explicitly -- the branch the
    // Terminal's completion list has always taken, arriving here for the same reason: a
    // ground FOLLOWED by cells that are not part of it.
    CHECK(bytes.find("\x1b[49m") != std::string::npos);
}

TEST_CASE("HD-9: the ground resolves to a real ink for a graphical medium, per row") {
    // THE OTHER MEDIUM'S ANSWER TO THE SAME PUBLISHED FACT. `plan_layer_regions` resolves
    // `role::kNone` to the REGION's own ground, so "has a ground of its own" is spelled as
    // "differs from the region's" in the renderer -- which is what makes the absence an
    // absence rather than a second flag to keep in step.
    Sample p = panel_of(3, 0, 80, 38, 8, 18);
    const InfoBodyPlace body = body_of(p.d, p.s);
    REQUIRE(body.fit.graphical());
    const surface::SurfaceCanvas c = paint(p.d, p.s);
    const std::vector<surface::PlanTextRegion> plan = plan_regions_of(
        c, surface::SurfaceExtent{80, 38, 8, 18},
        surface::PlanSize{80 * surface::kCanvasCellPx, 38 * surface::kCanvasCellPx});
    const surface::PlanTextRegion* planned = nullptr;
    for (const surface::PlanTextRegion& r : plan) {
        if (r.view.y == body.region_y * surface::kCanvasCellPx) {
            planned = &r;
        }
    }
    REQUIRE(planned != nullptr);
    REQUIRE(planned->rows.size() >
            static_cast<std::size_t>(kInfoHeadingRows + body.action_row) + 1);

    const surface::PlanTextRow& heading =
        planned->rows[static_cast<std::size_t>(kInfoHeadingRows + body.heading_row)];
    const surface::PlanTextRow& create =
        planned->rows[static_cast<std::size_t>(
            kInfoHeadingRows + prose_row_of_action(body, kActionCreate))];
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
        CHECK(shown->rows.size() == static_cast<std::size_t>(kInfoHeadingRows) + body.capacity);
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

// ============================================================================
// TEXT-0: the TextBox contract, driven end to end through the weave
// ============================================================================
//
// The component suite owns what the vocabulary DOES; these cases own that Workshop's four
// editable places actually reach it -- keys in, published selection out, clipboard across
// consumers and across the bus, and the routing that keeps `^c` copy where text is being
// edited and quit where it cannot be.

namespace {

/// An ordinary accepter of a maker's copy -- what the active Skin (and any text-holding
/// pane provider) looks like to the bus when `ClipboardCopy` is said.
struct CopyHeard {
    std::int64_t count = 0;
    ZEN_SHAPE(CopyHeard, 1, ZEN_FIELD(count));
};
class ClipboardEars
    : public loom::WeaveBase<ClipboardEars, CopyHeard,
                             loom::Accept<surface::ClipboardCopy>, loom::Emit<>> {
public:
    explicit ClipboardEars(std::vector<std::string>& heard) : heard_(&heard) {}
    void on(const surface::ClipboardCopy& c, loom::Mail&) { heard_->push_back(c.text); }

private:
    std::vector<std::string>* heard_;
};

} // namespace

TEST_CASE("TEXT-0: the terminal line selects, copies, cuts, pastes and undoes by keys") {
    Live t;
    // A readable medium at the Skin's role (QR-11): copies land on its platform, and each
    // paste ASKS it — the conversation every paste below settles inside its own gesture.
    SkinSeat* skin = t.mount_skin_seat();
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("hello world")) {
        t.text(std::string(1, c));
    }

    // Shift+Left sweeps a selection, and the PUBLISHED pane region says exactly which
    // prose columns are selected -- the same prompt shift the caret has always had.
    for (int i = 0; i < 5; ++i) {
        t.key(input::scan::kLeft, input::mod::kShift);
    }
    CHECK(t.pane().input.selected_text() == "world");
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        const TerminalInputPlace p = terminal_input_place(screen_of(t.session()));
        CHECK(pane.sel_begin_row == p.prose_row);
        CHECK(pane.sel_end_row == p.prose_row);
        CHECK(pane.sel_begin_col == kTerminalPromptCols + 6);
        CHECK(pane.sel_end_col == kTerminalPromptCols + 11);
        CHECK(pane.caret_col == kTerminalPromptCols + 6); // the active end is the caret
    }

    // Typing replaces what was swept.
    t.text("there");
    CHECK(t.pane().input.text() == "hello there");
    // ...and the selection's absence is published as the absence.
    {
        const surface::SurfaceTextRegion& pane =
            *pane_of(t.canvases.back(), screen_of(t.session()));
        CHECK(pane.sel_begin_row == surface::kNoSelection);
    }

    // Ctrl+A, copy, cut, paste, undo, redo -- the chords land on the line while the
    // overlay has the keyboard. A copy fills the mirror AND the medium's clipboard; a
    // paste asks the medium (QR-11) and inserts what it answers.
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK(t.session().clipboard.text == "hello there");
    CHECK(skin->platform == "hello there"); // the copy reached the platform through the Skin
    CHECK(t.pane().input.text() == "hello there"); // a copy erases nothing
    t.key(input::scan::kX, input::mod::kCtrl);
    CHECK(t.pane().input.empty());
    t.key(input::scan::kV, input::mod::kCtrl);
    t.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.pane().input.text() == "hello therehello there");
    CHECK(skin->clipboard_reads == 2); // one read per paste, on request, never before
    t.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(t.pane().input.text() == "hello there");
    t.key(input::scan::kY, input::mod::kCtrl);
    CHECK(t.pane().input.text() == "hello therehello there");
    // Word movement is part of the same vocabulary.
    t.key(input::scan::kHome);
    t.key(input::scan::kRight, input::mod::kCtrl);
    CHECK(t.pane().input.caret() == 6);
}

TEST_CASE("TEXT-0: a copy is said to the process once, and a heard copy fills the mirror") {
    // The medium here CANNOT be read -- the terminal's standing truth -- so every paste
    // below is the fallback road: what this process itself last copied. That is what
    // keeps copy-here-paste-there working on a medium whose platform never answers, with
    // no platform claim anywhere (QR-11; the readable road has its own cases).
    Live t;
    SkinSeat* skin = t.mount_skin_seat();
    skin->readable_medium = false;
    std::vector<std::string> heard;
    (void)loom::mount<ClipboardEars>(t.bus, heard);
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("abc")) {
        t.text(std::string(1, c));
    }

    // A copy with nothing selected publishes nothing -- there was no copy.
    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK(heard.empty());

    // A real copy is one publication, carrying the copied bytes.
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kC, input::mod::kCtrl);
    REQUIRE(heard.size() == 1);
    CHECK(heard[0] == "abc");
    // A cut is a copy too, said the same way.
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kX, input::mod::kCtrl);
    REQUIRE(heard.size() == 2);

    // ANOTHER PARTICIPANT'S ClipboardCopy fills the mirror -- the terminal-media road,
    // where no platform ever answers back -- and paste STILL ASKS first (read follows
    // intent whatever the medium), inserting the mirror only because the medium answered
    // that it cannot say.
    t.publish(loom::to_value(surface::ClipboardCopy{"a pane's copy"}));
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.pane().input.text() == "a pane's copy");
    CHECK(skin->clipboard_reads == 1);
    // ...and MIRRORING is not copying: the ears heard the case's own publication (their
    // third) and Workshop said nothing back -- neither for hearing it nor for pasting it.
    CHECK(heard.size() == 3);
}

TEST_CASE("TEXT-0: the property draft speaks the same vocabulary and keeps its policy keys") {
    Live t;
    (void)t.mount_skin_seat(); // the paste below is a conversation; somebody must answer it
    t.begin_editing("Name");
    const Row* name = t.row("Name");
    REQUIRE(name != nullptr);
    REQUIRE(name->editing());
    REQUIRE(name->draft() == "panel");

    // Select-all + type replaces the whole draft in one gesture...
    t.key(input::scan::kA, input::mod::kCtrl);
    // ...and while the selection is live, the PUBLISHED Info region marks exactly the
    // value's columns of the editing row, through the same offsets the caret uses.
    {
        const InfoBodyPlace body = body_place(t);
        const surface::SurfaceTextRegion* region = body_region(t.canvases.back(), body);
        REQUIRE(region != nullptr);
        CHECK(region->sel_begin_row == region->caret_row);
        CHECK(region->sel_begin_col == kPropertyMarkCols + kPropertyLabelCols);
        CHECK(region->sel_end_col == kPropertyMarkCols + kPropertyLabelCols + 5);
    }
    t.text("frame");
    CHECK(name->draft() == "frame");

    // ^c over a live draft is copy, never quit; the clipboard chords work; undo works.
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK_FALSE(t.host.quit);
    CHECK(t.session().clipboard.text == "frame");
    t.key(input::scan::kEnd);
    t.key(input::scan::kV, input::mod::kCtrl);
    CHECK(name->draft() == "frameframe");
    t.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(name->draft() == "frame");

    // RETURN IS STILL THE OWNER'S: it commits through the property, exactly as before.
    t.key(input::scan::kReturn);
    CHECK_FALSE(name->editing());
    CHECK(t.doc().elements[0].label == "frame");
    CHECK(t.notice() == "committed Name = frame");
}

TEST_CASE("TEXT-0: the name editor selects with the same keys and says it in characters") {
    TempDir dir("text0-naming");
    Live t;
    (void)t.mount_skin_seat(); // the cross-consumer paste below asks it, like every paste
    t.host.setup_path = dir.file("setup.json");
    (void)t.mount_terminal();

    // Copy in the Terminal first -- the cross-consumer half of the claim.
    t.toggle_terminal();
    for (const char c : std::string("Morning")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kC, input::mod::kCtrl);
    t.toggle_terminal();

    t.key(input::scan::kS);
    t.text("s");
    REQUIRE(t.session().setup.naming.open);
    REQUIRE(t.session().setup.naming.line.text() == "Default");

    // Ctrl+A selects the name, and the editor row -- the identity row, which is the TOP
    // band's first since QR-14 -- says so the way every other selection on this screen is
    // said: the REGION's selection (a band under the glyphs where the row is real type,
    // reverse video in a cell medium), with a real region caret at its active end. The old
    // one-cell label had to bracket the span in characters; the region carries both.
    t.key(input::scan::kA, input::mod::kCtrl);
    CHECK(t.session().setup.naming.line.selected_text() == "Default");
    const std::vector<surface::SurfaceTextRegion> at_bands =
        regions_at(t.canvases.back(), 0, 0);
    REQUIRE(at_bands.size() == 1);
    const surface::SurfaceTextRegion& editor = at_bands.front();
    REQUIRE_FALSE(editor.rows.empty());
    CHECK(editor.rows[0].text.find("setup name> Default") == 0);
    const std::int64_t prompt =
        static_cast<std::int64_t>(std::char_traits<char>::length(kSetupNamePrompt));
    const std::int64_t at_caret =
        prompt + static_cast<std::int64_t>(t.session().setup.naming.line.caret_column());
    CHECK(editor.caret_row == 0);
    CHECK(editor.caret_col == at_caret);
    CHECK(editor.sel_begin_row == 0);
    CHECK(editor.sel_begin_col == prompt);
    CHECK(editor.sel_end_row == 0);
    CHECK(editor.sel_end_col == prompt + 7);
    // ...and the cell projection still inserts the caret as a character, so a character
    // medium's row reads exactly as it always did.
    CHECK(label_at(t.canvases.back(), 0, 0).find("setup name> Default") == 0);

    // Paste replaces the selection: the name a maker copied in the Terminal arrives here.
    t.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.session().setup.naming.line.text() == "Morning");
    // ^c with a selection copies rather than quitting, in this mode too.
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kC, input::mod::kCtrl);
    CHECK_FALSE(t.host.quit);
    // Escape is still the owner's: the name is unchanged and nothing was saved.
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().setup.active.name == "Default");
}

TEST_CASE("TEXT-0: a drag sweeps a selection on the terminal line, and release keeps it") {
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("send something")) {
        t.text(std::string(1, c));
    }
    const TerminalInputPlace p = terminal_input_place(screen_of(t.session()));

    // The press places the caret AND opens the drag; no selection yet.
    t.press_at(pane_cell_x(p, p.first_column + 5), pane_cell_y(p, p.prose_row),
               input::space::kCells);
    REQUIRE(t.pane().input.caret() == 5);
    CHECK(t.session().text_drag.active);
    CHECK_FALSE(t.pane().input.has_selection());

    // Motion extends from the pressed anchor -- the same geometry the press resolved.
    t.publish(loom::to_value(input::PointerMoved{pane_cell_x(p, p.first_column + 9),
                                                 pane_cell_y(p, p.prose_row), 0, 0,
                                                 input::space::kCells, input::mod::kNone}));
    CHECK(t.pane().input.selected_text() == "some");
    CHECK(t.pane().input.anchor() == 5);

    // ...and sweeping back through the anchor selects the other way. The ROW is
    // deliberately not re-tested mid-drag: a hand that wanders off the line keeps
    // sweeping it by column, so the selection is stable rather than flickering.
    t.publish(loom::to_value(input::PointerMoved{pane_cell_x(p, p.first_column + 1),
                                                 pane_cell_y(p, p.prose_row - 1), 0, 0,
                                                 input::space::kCells, input::mod::kNone}));
    CHECK(t.pane().input.selected_text() == "end ");
    CHECK(t.pane().input.caret() == 1);

    // Release ends the GESTURE and keeps the SELECTION -- ending the sweep is not
    // unselecting -- and a motion after release moves nothing.
    t.publish(loom::to_value(input::PointerButton{1, false, pane_cell_x(p, p.first_column + 1),
                                                  pane_cell_y(p, p.prose_row),
                                                  input::space::kCells, input::mod::kNone}));
    CHECK_FALSE(t.session().text_drag.active);
    CHECK(t.pane().input.selected_text() == "end ");
    t.publish(loom::to_value(input::PointerMoved{pane_cell_x(p, p.first_column + 12),
                                                 pane_cell_y(p, p.prose_row), 0, 0,
                                                 input::space::kCells, input::mod::kNone}));
    CHECK(t.pane().input.selected_text() == "end ");

    // A fresh press collapses the old selection: it is the gesture that STARTS one.
    t.press_at(pane_cell_x(p, p.first_column + 3), pane_cell_y(p, p.prose_row),
               input::space::kCells);
    CHECK_FALSE(t.pane().input.has_selection());
    CHECK(t.pane().input.caret() == 3);
}

TEST_CASE("TEXT-0: a drag sweeps a selection on the property draft through its own row") {
    Live t;
    t.begin_editing("Name"); // the draft is "panel", caret at its end
    const Row* name = t.row("Name");
    REQUIRE(name != nullptr);
    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.present);
    const std::size_t editing = editing_index(t);
    const std::int64_t row = prose_row_of_property(body, editing);
    const std::int64_t value0 = kPropertyMarkCols + kPropertyLabelCols;

    // Press at the value's second character, then sweep right.
    const std::int64_t row_y =
        body.region_y + kInfoHeadingRows + row + surface::kTuiCanvasTopRow;
    t.press_at(body.region_x + value0 + 1, row_y, input::space::kCells);
    REQUIRE(name->editor().caret() == 1);
    CHECK(t.session().text_drag.active);
    t.publish(loom::to_value(input::PointerMoved{
        body.region_x + value0 + 4, row_y, 0, 0,
        input::space::kCells, input::mod::kNone}));
    CHECK(name->editor().selected_text() == "ane");

    // Typing replaces what the hand swept -- the point of sweeping it.
    t.publish(loom::to_value(input::PointerButton{
        1, false, body.region_x + value0 + 4, row_y,
        input::space::kCells, input::mod::kNone}));
    CHECK_FALSE(t.session().text_drag.active);
    t.text("X");
    CHECK(name->draft() == "pXl");
}

TEST_CASE("TEXT-0: ^c still quits exactly where nothing takes text") {
    // Command mode: the rewritten MSG-0 case covers a focused pane and the overlay; this
    // one pins the three keyboard owners that take no text -- the picker, pane management,
    // and plain command mode -- so the narrowing cannot creep.
    {
        Live t;
        t.key(input::scan::kP); // the picker is open and owns the keyboard
        t.text("p");
        REQUIRE(t.session().panels.picker.open);
        t.key(input::scan::kC, input::mod::kCtrl);
        CHECK(t.host.quit);
    }
    {
        Live t;
        t.key(input::scan::kW); // pane management
        t.text("w");
        REQUIRE(t.session().arrange.open);
        t.key(input::scan::kC, input::mod::kCtrl);
        CHECK(t.host.quit);
    }
    {
        Live t; // command mode, nothing open
        t.key(input::scan::kC, input::mod::kCtrl);
        CHECK(t.host.quit);
    }
}

TEST_CASE("TEXT-0: the real Composer's fields speak the vocabulary across the seam") {
    // The fourth consumer, driven as a STRANGER: the real `zengine-composer` library, loaded
    // through the real Kernel, receiving the chords as `PaneKey` across the pane seam -- so
    // what is proven is the whole road: Workshop's routing hands `^c` to the focused pane,
    // the provider's field consumes it, the copy is SAID to the process, and Workshop's own
    // mirror (and therefore every other text box in the application) holds the bytes.
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
        if (row.provider == std::string(kIntroOffice) && row.pane == std::string(kIntroPane)) {
            intro_kind = row.kind;
        }
        if (row.provider == std::string(kComposerOffice)) {
            compose_kind = row.kind;
        }
    }
    REQUIRE(is_runtime_kind(intro_kind));
    REQUIRE(is_runtime_kind(compose_kind));

    // The target: the introspection office itself, selected through the REAL Loaded pane --
    // press the row naming the introspection stem, exactly as a maker would. That office is
    // loaded and answers `zen.DescribeAccepted` in the same turn.
    {
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
    }

    // Focus the pane and open a form: press rows until the form's own notice appears. Which
    // row is a message is the provider's business, so the case walks rather than assumes.
    press_body(r, compose_kind);
    bool form_open = false;
    {
        const ui::Rect body = external_body_rect(r.session(), compose_kind);
        const std::vector<std::string> rows = external_rows(r.last_canvas(), body);
        for (std::size_t i = 0; i < rows.size() && !form_open; ++i) {
            r.press_cell(body.x + 1, body.y + kExternalHeaderRows + static_cast<std::int64_t>(i));
            for (const std::string& row :
                 external_rows(r.last_canvas(), external_body_rect(r.session(), compose_kind))) {
                form_open = form_open || row.find("enter acts") != std::string::npos;
            }
        }
    }
    REQUIRE(form_open);

    // A readable medium at the Skin's role, for the provider's own paste conversation.
    SkinSeat* skin = nullptr;
    {
        auto seat = std::make_unique<SkinSeat>();
        skin = seat.get();
        loom::Grant grant;
        grant.allow_to_any(surface::ClipboardText::zen_name,
                           surface::ClipboardText::zen_version);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(seat), std::move(grant), surface::kSkinRole);
        skin->zen_set_self(id);
    }

    // Type into the field under the cursor (typing makes it present), select all, copy.
    r.text("hello");
    r.key(input::scan::kA, input::mod::kCtrl);
    r.key(input::scan::kC, input::mod::kCtrl);
    CHECK_FALSE(r.host.quit); // ^c crossed the seam instead of quitting
    CHECK(r.session().clipboard.text == "hello"); // ...and the copy reached the mirror
    CHECK(skin->platform == "hello");             // ...and the platform, through the Skin

    // The other direction (QR-11): the platform's clipboard changes SILENTLY -- some
    // unrelated application copied; no event travels, nothing here hears it -- and the
    // maker's paste is what asks. The provider's field gets the platform's CURRENT text,
    // visible in the pane's published rows, which is the only window this case has.
    skin->platform = "pasted-in";
    r.key(input::scan::kA, input::mod::kCtrl);
    r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(skin->clipboard_reads == 1); // the provider asked, once, because of the paste
    bool shown = false;
    for (const std::string& row :
         external_rows(r.last_canvas(), external_body_rect(r.session(), compose_kind))) {
        shown = shown || row.find("pasted-in") != std::string::npos;
    }
    CHECK(shown);
    // ...and undo is local to the field, one chord away.
    r.key(input::scan::kZ, input::mod::kCtrl);
    bool restored = false;
    for (const std::string& row :
         external_rows(r.last_canvas(), external_body_rect(r.session(), compose_kind))) {
        restored = restored || row.find("hello") != std::string::npos;
    }
    CHECK(restored);

    // QR-11's owner binding, across the seam: the paste's answer arrives after the FORM
    // it was asked in was dropped -- the paste and the Escape enqueued in one batch, as
    // one poll delivers them -- and the payload lands nowhere. `esc` is back (the draft
    // is dropped whole; choosing the same shape again gives a fresh form), so the form
    // that asked no longer exists whatever form a maker opens next.
    skin->platform = "SECRET";
    const auto enqueue_key = [&r](std::int64_t sc, std::int64_t mods) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::KeyPressed{sc, "", mods}),
                                          loom::WeaveId{}, loom::WeaveId{}, 0));
    };
    // ...and the sharpest arm: a NEW form stands, typed into, before the answer arrives
    // -- same field index, a fresh box (whose epoch matches a fresh recording), present.
    // Only the form's own identity (the draft generation) tells the two apart, and it
    // must: the old form's paste landing here would be "whichever field owns the cursor
    // later" wearing the same index.
    enqueue_key(input::scan::kA, input::mod::kCtrl);
    enqueue_key(input::scan::kV, input::mod::kCtrl);      // the OLD form's field asks
    enqueue_key(input::scan::kEscape, input::mod::kNone); // the form is dropped whole
    enqueue_key(input::scan::kReturn, input::mod::kNone); // a NEW form opens (cursor row 0)
    (void)r.bus.publish(loom::Message(loom::to_value(input::TextEntered{"z"}),
                                      loom::WeaveId{}, loom::WeaveId{}, 0)); // typed: present
    r.bus.drain_until_idle();
    CHECK(skin->clipboard_reads == 2); // the request was real; the read happened
    bool typed_visible = false;
    for (const std::string& row :
         external_rows(r.last_canvas(), external_body_rect(r.session(), compose_kind))) {
        CAPTURE(row);
        CHECK(row.find("SECRET") == std::string::npos);
        typed_visible = typed_visible || row.find("[z") != std::string::npos;
    }
    // The staging reached the vulnerable state -- the new form's field is present and
    // holds the typed byte -- so the absence above is a measurement, not a vacancy.
    CHECK(typed_visible);
}

// ============================================================================
// QR-11: clipboard reads follow paste intent
// ============================================================================
//
// The product law: permission to use clipboard text when the maker asks to paste it is
// not permission to continuously observe clipboard text. The reader-side half (nothing
// ambient can even be SAID) is pinned in the input suite; the medium half in the surface
// suite. These cases own Workshop's half: a paste is a conversation, the answer belongs
// to the draft that asked, and text asked for by a draft that has ended lands nowhere.

namespace {

/// A participant with a real, bus-stamped identity that speaks a well-formed answer shape
/// at Workshop on command — the forgery the answers_ask wall exists to refuse.
class RogueAnswerer : public loom::WeaveBase<RogueAnswerer, SeenState, loom::Accept<SeatDo>,
                                             loom::Emit<surface::ClipboardText>> {
public:
    void on(const SeatDo&, loom::Mail& mail) {
        (void)mail.send(target, surface::ClipboardText{true, "EVIL"}, correlation);
    }
    loom::WeaveId target{};
    std::uint64_t correlation = 0;
};

} // namespace

TEST_CASE("QR-11: an unsolicited ClipboardText enters no box and no mirror") {
    // The wall the whole road rests on, measured at Workshop: a well-formed payload,
    // directed at the weave, wearing the guessable first correlation -- and it settles
    // nothing, mutates nothing, pastes nothing, because it answers no ask this weave
    // opened and Loom did not stamp it as an answer at all.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    (void)t.bus.send(t.workshop_id,
                     loom::Message(loom::to_value(surface::ClipboardText{true, "EVIL"}),
                                   loom::WeaveId{}, loom::WeaveId{}, 1));
    t.bus.drain_until_idle();
    CHECK(t.pane().input.empty());
    CHECK(t.session().clipboard.text.empty());

    // THE SHARPER HALF: an ask genuinely OUTSTANDING (nobody holds the skin role, so the
    // conversation stays open), and a rogue with a real bus-stamped identity guessing the
    // correlation an asker's fresh book mints first -- 1, the settlement law's own example
    // of why a correlation identifies and never authenticates. The book alone would be
    // satisfied; `answers_ask()` is the wall that is not, because Loom stamps the one
    // authorized answer and no send can wear the stamp.
    for (const char c : std::string("abc")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kV, input::mod::kCtrl); // the ask opens; no answer will ever come
    loom::WeaveId rogue_id{};
    {
        auto weave = std::make_unique<RogueAnswerer>();
        RogueAnswerer* rogue = weave.get();
        loom::Grant grant;
        grant.allow_to_any(surface::ClipboardText::zen_name,
                           surface::ClipboardText::zen_version);
        rogue_id = t.bus.register_weave(std::move(weave), std::move(grant));
        rogue->zen_set_self(rogue_id);
        rogue->target = t.workshop_id;
        rogue->correlation = 1;
    }
    (void)t.bus.send(rogue_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                             loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    CHECK(t.pane().input.text() == "abc"); // the payload reached no box
    CHECK(t.session().clipboard.text.empty());
}

TEST_CASE("QR-11: paste reads the platform current, not the mirror stale") {
    // SC-2's own sentence: on a readable medium the value used for a paste is the actual
    // platform value current for THAT paste -- never a mirror populated earlier. The
    // platform moves silently between two pastes (no event; nothing here may watch), and
    // each paste gets its own moment's truth.
    Live t;
    SkinSeat* skin = t.mount_skin_seat();
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("stale")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kC, input::mod::kCtrl); // mirror = "stale", platform = "stale"
    REQUIRE(t.session().clipboard.text == "stale");
    skin->platform = "fresh"; // an unrelated application copies; nothing travels
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.pane().input.text() == "fresh");
    skin->platform = "newer"; // ...and again, between two pastes
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.pane().input.text() == "newer");
    CHECK(skin->clipboard_reads == 2);
}

TEST_CASE("QR-11: an answer crossing a draft boundary lands nowhere, and the payload dies") {
    // SC-3, the discard half. The acquisition crosses a turn, so a second gesture can be
    // queued behind the paste before the answer arrives; the draft that asked ends, and
    // the text must not land in whichever draft stands afterwards. The enqueue helper
    // exists because Live::key drains to idle per gesture -- the race needs both
    // messages on the queue before either runs, exactly as one poll batch delivers them.
    Live t;
    SkinSeat* skin = t.mount_skin_seat();
    skin->platform = "SECRET";
    const auto enqueue_key = [&t](std::int64_t sc, std::int64_t mods) {
        (void)t.bus.publish(loom::Message(loom::to_value(input::KeyPressed{sc, "", mods}),
                                          loom::WeaveId{}, loom::WeaveId{}, 0));
    };

    // The terminal line: paste requested, then the line SUBMITTED before the answer.
    // clear() gave the successor draft a new epoch, so the in-flight paste is nobody's.
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("help")) {
        t.text(std::string(1, c));
    }
    enqueue_key(input::scan::kV, input::mod::kCtrl);
    enqueue_key(input::scan::kReturn, input::mod::kNone);
    t.bus.drain_until_idle();
    CHECK(t.pane().input.empty());          // the fresh line took no foreign text
    CHECK(skin->clipboard_reads == 1);      // the read did happen -- intent was real
    CHECK(t.session().clipboard.text.empty()); // ...and the discarded payload died whole:
    // a readable answer updates the mirror ONLY when its paste applies.

    // A property draft: paste requested, then the draft CANCELLED before the answer.
    t.toggle_terminal();
    t.begin_editing("Name");
    enqueue_key(input::scan::kV, input::mod::kCtrl);
    enqueue_key(input::scan::kEscape, input::mod::kNone);
    t.bus.drain_until_idle();
    const Row* name = t.row("Name");
    REQUIRE(name != nullptr);
    CHECK_FALSE(name->editing());
    CHECK(name->value() == "panel"); // the committed value never met the payload
    CHECK(t.session().clipboard.text.empty());
    CHECK(skin->clipboard_reads == 2);

    // THE SHARPEST STAGING: the draft that asked ends AND a new draft opens on the SAME
    // row before the answer arrives -- same object, same label, same box, a fresh draft.
    // The epoch is the only fact that tells them apart, and it must: this is exactly
    // "whichever field happens to own the keyboard later", wearing the old field's name.
    t.begin_editing("Name");
    enqueue_key(input::scan::kV, input::mod::kCtrl);      // the OLD draft asks
    enqueue_key(input::scan::kEscape, input::mod::kNone); // ...and ends
    enqueue_key(input::scan::kReturn, input::mod::kNone); // a NEW draft opens, same row
    t.bus.drain_until_idle();
    const Row* reopened = t.row("Name");
    REQUIRE(reopened != nullptr);
    REQUIRE(reopened->editing());
    CHECK(reopened->draft() == "panel"); // the new draft never met the old draft's paste
    CHECK(t.session().clipboard.text.empty());
    CHECK(skin->clipboard_reads == 3); // the old draft's request was real, and it read
}

TEST_CASE("QR-11: the draft that asked keeps its paste across a rebuild in flight") {
    // SC-3, the belongs-to half. An extent change between request and answer rebuilds
    // the inspector rows; the draft rides Row::resume into the new row -- the SAME draft,
    // draft_epoch and all -- so the paste it asked for still belongs to it and lands.
    Live t;
    SkinSeat* skin = t.mount_skin_seat();
    skin->platform = "carried";
    t.begin_editing("Name");
    const auto enqueue_key = [&t](std::int64_t sc, std::int64_t mods) {
        (void)t.bus.publish(loom::Message(loom::to_value(input::KeyPressed{sc, "", mods}),
                                          loom::WeaveId{}, loom::WeaveId{}, 0));
    };
    enqueue_key(input::scan::kA, input::mod::kCtrl);
    enqueue_key(input::scan::kV, input::mod::kCtrl);
    (void)t.bus.publish(loom::Message(
        loom::to_value(surface::SurfaceExtent{kScreenMinW + 8, kScreenMinH + 4, 0, 0}),
        loom::WeaveId{}, loom::WeaveId{}, 0));
    t.bus.drain_until_idle();
    const Row* name = t.row("Name");
    REQUIRE(name != nullptr);
    REQUIRE(name->editing());
    CHECK(name->draft() == "carried"); // select-all + paste, applied after the rebuild
    CHECK(skin->clipboard_reads == 1);
}

TEST_CASE("QR-11: with nobody at the skin role, paste inserts nothing and breaks nothing") {
    // The honest degradation: the ask has no respondent (the send is refused as a tap
    // event; no message returns; Loom has no unanswerability notice), so the paste simply
    // does not happen -- consumed, so it cannot fall through to an application binding --
    // and pressing it past the book's capacity stays quiet rather than becoming a crash
    // or a queue.
    Live t;
    (void)t.mount_terminal();
    t.toggle_terminal();
    for (const char c : std::string("abc")) {
        t.text(std::string(1, c));
    }
    for (int i = 0; i < 6; ++i) { // past the book's capacity of 4
        t.key(input::scan::kV, input::mod::kCtrl);
    }
    CHECK(t.pane().input.text() == "abc");
    CHECK_FALSE(t.host.quit);
}

// ============================================================================
// KEY-0 -- one executable binding truth: the keymap, the context resolver, the
// authored keymap file, the legend, and the full hotkey view.
//
// The regression half of this phase is the 780 cases above: every default
// gesture they drive now travels declaration -> effective binding -> owner, and
// they pass unchanged. What is pinned HERE is what did not exist before: exact
// matching, remapping, admission, preservation, projection, and the view.
// ============================================================================

TEST_CASE("KEY-0: exact modifier matching -- the accidental subset aliases no longer fire") {
    Live t;
    // Ctrl+Shift+S used to save (the old test asked only whether Ctrl was among the
    // bits); the honest witness is the sentence save speaks with no document file --
    // the widened chord no longer reaches it, and the exact one still does.
    t.key(input::scan::kS, input::mod::kCtrl | input::mod::kShift);
    CHECK(t.notice().empty());
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "no document file -- start Workshop with --document <path>");
    // Ctrl+N used to create; under exact matching a chord is a different gesture
    // from its bare key.
    const std::size_t before = t.doc().elements.size();
    t.key(input::scan::kN, input::mod::kCtrl);
    CHECK(t.doc().elements.size() == before);
    // ...and the bare key still creates: exact matching narrowed, it did not move.
    t.key(input::scan::kN);
    CHECK(t.doc().elements.size() == before + 1);
    // Alt+Q used to quit.
    t.key(input::scan::kQ, input::mod::kAlt);
    CHECK_FALSE(t.host.quit);
}

TEST_CASE("KEY-0: shift+space is gone -- not a binding, not an invisible alias") {
    // The old Terminal opener could not arrive from the POSIX backend at all
    // (`ground_byte(' ')` infers Shift only on letters), and KEY-0 removed it rather
    // than keeping a default one backend advertises and cannot deliver.
    Live t;
    t.mount_terminal();
    t.key(input::scan::kSpace, input::mod::kShift);
    t.text(" ");
    CHECK_FALSE(t.pane().open);
    // ...and the toggle both backends can honestly produce is ctrl+t.
    t.key(input::scan::kT, input::mod::kCtrl);
    CHECK(t.pane().open);
}

TEST_CASE("KEY-0: ctrl+k opens the hotkey view, esc and ctrl+k close it") {
    Live t;
    t.key(input::scan::kK, input::mod::kCtrl);
    CHECK(t.session().hotkeys.open);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().hotkeys.open);
    t.key(input::scan::kK, input::mod::kCtrl);
    CHECK(t.session().hotkeys.open);
    t.key(input::scan::kK, input::mod::kCtrl);
    CHECK_FALSE(t.session().hotkeys.open);
}

TEST_CASE("KEY-0: the view is keys-modal -- a maker reading a binding is not executing it") {
    Live t;
    t.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(t.session().hotkeys.open);
    const std::size_t before = t.doc().elements.size();
    t.key(input::scan::kN);
    CHECK(t.doc().elements.size() == before); // `n` swallowed, nothing created
    t.key(input::scan::kP);
    CHECK_FALSE(t.session().panels.picker.open); // `p` swallowed, no picker
    t.key(input::scan::kQ);
    CHECK_FALSE(t.host.quit); // `q` swallowed, no quit through the view
    t.text("x");
    CHECK_FALSE(t.host.quit); // text swallowed too, and lands nowhere
    // ...and the other above-mode actions still answer over it, the terminal
    // overlay's own rule for `^s`/`^o`.
    t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(t.notice() == "no document file -- start Workshop with --document <path>");
    // The context BENEATH is untouched when it closes.
    t.key(input::scan::kEscape);
    t.key(input::scan::kN);
    CHECK(t.doc().elements.size() == before + 1);
}

TEST_CASE("KEY-0: the view lists the context beneath it, and three contexts differ") {
    Live t;
    t.mount_terminal();
    // A TALL SCREEN, so the whole grouped list fits: at the minimum extent the slot
    // elides the deeper groups behind `... N more`, which is its own pinned behavior;
    // this case is about what the groups SAY when there is room to say it. The view is
    // read at the slot the RESOLVED screen grants it (`stack_text` reads the minimum
    // composition's rectangle, which this screen has outgrown).
    t.publish(loom::to_value(surface::SurfaceExtent{120, 70, 0, 0}));
    const auto view_text = [&t]() {
        return panel_text(
            t.canvases.back(),
            pane_body_cells(hotkeys_bounds(t.session(), screen_of(t.session()))));
    };

    // COMMAND MODE BENEATH: the command vocabulary, its layer named.
    t.key(input::scan::kK, input::mod::kCtrl);
    const std::string command_view = view_text();
    CHECK(command_view.find("HOTKEYS") != std::string::npos);
    CHECK(command_view.find("command mode") != std::string::npos);
    CHECK(command_view.find("new") != std::string::npos);
    CHECK(command_view.find("answered above every mode") != std::string::npos);
    CHECK(command_view.find("^t") != std::string::npos);
    t.key(input::scan::kEscape);

    // EDITABLE TEXT BENEATH (the Terminal line): its own controls, and the
    // component's editing vocabulary shown from the component's own rows,
    // marked as not this keymap's to move.
    t.toggle_terminal();
    REQUIRE(t.pane().open);
    t.key(input::scan::kK, input::mod::kCtrl);
    const std::string text_view = view_text();
    CHECK(text_view.find("the terminal line") != std::string::npos);
    CHECK(text_view.find("run the line") != std::string::npos);
    CHECK(text_view.find("not remappable") != std::string::npos);
    CHECK(text_view.find("copy") != std::string::npos);
    // ^c means copy there, so quit's row is not in this context's list.
    CHECK(text_view.find("quit") == std::string::npos);
    CHECK(text_view != command_view);
}

TEST_CASE("KEY-0: an authored override changes dispatch AND every displayed spelling") {
    TempDir dir("keymap-override");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"object.new", "g"}}));
    Keyed t(path);
    // The load was announced first, in words, with the override counted -- read it
    // before any gesture writes its own sentence over the one notice line.
    CHECK(t.notice().find("1 override") != std::string::npos);

    // DISPATCH: `g` creates and `n` no longer does -- the override moved the
    // binding, not the action. (`g`, because it is a still-free letter: `e` stopped
    // being one when the Builder's edit-source door took it.)
    const std::size_t before = t.doc().elements.size();
    t.key(input::scan::kN);
    CHECK(t.doc().elements.size() == before);
    t.key(input::scan::kG);
    CHECK(t.doc().elements.size() == before + 1);

    // DISPLAY: the band's first help row and the hotkey view both spell the
    // same effective binding, because both project the same value dispatch read.
    const Screen sc = screen_of(t.session());
    CHECK(label_at(t.canvases.back(), 0, sc.help_y).rfind("g new", 0) == 0);
    t.key(input::scan::kK, input::mod::kCtrl);
    const std::string view = stack_text(t.canvases.back());
    CHECK(view.find("g             new") != std::string::npos);

}

TEST_CASE("KEY-0: an override survives restart, and deleting the file restores defaults") {
    TempDir dir("keymap-restart");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"object.new", "g"}}));
    {
        Keyed first(path);
        const std::size_t before = first.doc().elements.size();
        first.key(input::scan::kG);
        REQUIRE(first.doc().elements.size() == before + 1);
    }
    // RESTART: a new process reads the same authored file and reaches the same
    // effective truth.
    {
        Keyed again(path);
        const std::size_t before = again.doc().elements.size();
        again.key(input::scan::kG);
        CHECK(again.doc().elements.size() == before + 1);
        again.key(input::scan::kN);
        CHECK(again.doc().elements.size() == before + 1);
    }
    // RESET: deleting the file IS returning to the defaults -- nothing else to
    // clear, nothing rewritten.
    std::error_code drop;
    std::filesystem::remove(path, drop);
    Keyed defaults(path);
    CHECK(defaults.notice().empty()); // an absent file is not a complaint
    const std::size_t before = defaults.doc().elements.size();
    defaults.key(input::scan::kN);
    CHECK(defaults.doc().elements.size() == before + 1);
    defaults.key(input::scan::kG);
    CHECK(defaults.doc().elements.size() == before + 1);
}

TEST_CASE("KEY-0: a same-context collision is refused naming both actions and the gesture") {
    // `d` is object.delete's default; authoring object.new onto it would put two
    // actions on one gesture in one context, which is a lockout and must not be
    // savable -- the whole candidate is refused and the defaults stand.
    const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(
        keymap_file_text("default", {{"object.new", "d"}}));
    CHECK_FALSE(loaded.outcome.accepted);
    CHECK(loaded.outcome.refusal.find("object.new") != std::string::npos);
    CHECK(loaded.outcome.refusal.find("object.delete") != std::string::npos);
    CHECK(loaded.outcome.refusal.find("`d`") != std::string::npos);

    // ...and the refused file leaves a live Workshop on its defaults, out loud.
    TempDir dir("keymap-collide");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"object.new", "d"}}));
    Keyed t(path);
    // ...OUT LOUD, AND AS A CONDITION. The file is still refused an hour
    // later and at the next launch, so it is not a sentence about a moment: it stands
    // under its own key, in the loader's own words, and it disappears from attention only
    // if it stops being true.
    const std::vector<Condition> now = attention_conditions(t.session());
    const Condition* wall = condition_by_key(now, kKeymapWallKey);
    REQUIRE(wall != nullptr);
    CHECK(wall->detail.find("object.delete") != std::string::npos);
    CHECK(wall->compact.find("default bindings stand") != std::string::npos);
    CHECK(wall->role == surface::role::kAlert);
    const std::size_t before = t.doc().elements.size();
    t.key(input::scan::kN);
    CHECK(t.doc().elements.size() == before + 1); // the default still creates
}

TEST_CASE("KEY-0: reusing one gesture across mutually exclusive contexts is legal") {
    // `h` moves an object in command mode; the picker cannot be open at the same
    // moment, so authoring picker.up onto `h` collides with nothing -- the
    // defaults already live this way (`s` names a setup and sizes a pane).
    const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(
        keymap_file_text("default", {{"picker.up", "h"}}));
    REQUIRE(loaded.outcome.accepted);

    TempDir dir("keymap-reuse");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"picker.up", "h"}}));
    Keyed t(path);
    const std::int64_t x_before = t.first()->x;
    t.key(input::scan::kH);
    CHECK(t.first()->x == x_before - 1); // command context: `h` still moves left
    t.key(input::scan::kP);
    t.text("p");
    REQUIRE(t.session().panels.picker.open);
    t.key(input::scan::kDown);
    REQUIRE(t.session().panels.picker.cursor == 1);
    t.key(input::scan::kH); // picker context: the authored `h` steps up
    CHECK(t.session().panels.picker.cursor == 0);
}

TEST_CASE("KEY-0: an override for an unknown action survives with its intent whole") {
    // The setup law's ACCEPTED clause, applied to the sixth file: a well-formed
    // row this build cannot resolve is not an error and must never become one.
    const std::string text = keymap_file_text(
        "default", {{"object.new", "g"}, {"future.action", "hyper+z"}});
    const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(text);
    REQUIRE(loaded.outcome.accepted);
    // The known row was applied...
    REQUIRE(loaded.keymap.overrides.size() == 1);
    CHECK(loaded.keymap.overrides[0].first == Act::kObjectNew);
    // ...the unknown row is preserved byte-for-byte, its gesture unjudged (that
    // spelling is outside THIS build's grammar, and it is not this build's to
    // normalise)...
    REQUIRE(loaded.keymap.authored.size() == 2);
    CHECK(loaded.keymap.authored[1].action == "future.action");
    CHECK(loaded.keymap.authored[1].gesture == "hyper+z");
    // ...and a save writes back exactly the bytes that were read: authored
    // order, authored spellings, nothing tidied.
    CHECK(keymap_persist::to_text(loaded.keymap) == text);
}

TEST_CASE("KEY-0: a gesture outside the grammar on a KNOWN action is refused in words") {
    const keymap_persist::LoadedKeymap bad_key = keymap_persist::from_text(
        keymap_file_text("default", {{"object.new", "f13"}}));
    CHECK_FALSE(bad_key.outcome.accepted);
    CHECK(bad_key.outcome.refusal.find("`f13` is not a key") != std::string::npos);

    const keymap_persist::LoadedKeymap bad_mod = keymap_persist::from_text(
        keymap_file_text("default", {{"object.new", "meta+n"}}));
    CHECK_FALSE(bad_mod.outcome.accepted);
    CHECK(bad_mod.outcome.refusal.find("`meta` is not a modifier") != std::string::npos);

    const keymap_persist::LoadedKeymap twice = keymap_persist::from_text(
        keymap_file_text("default", {{"object.new", "g"}, {"object.new", "i"}}));
    CHECK_FALSE(twice.outcome.accepted);
    CHECK(twice.outcome.refusal.find("authored twice") != std::string::npos);

    const keymap_persist::LoadedKeymap legend = keymap_persist::from_text(
        keymap_file_text("sometimes", {}));
    CHECK_FALSE(legend.outcome.accepted);
    CHECK(legend.outcome.refusal.find("`sometimes` is not a legend mode") !=
          std::string::npos);
}

TEST_CASE("KEY-0: a global action cannot take a bare printable or the editing vocabulary") {
    // A bare printable cannot be global once anything on the screen can take
    // text -- the standing law the old chain kept as a comment, enforced at the
    // door now that there is a door.
    const keymap_persist::LoadedKeymap bare = keymap_persist::from_text(
        keymap_file_text("default", {{"workshop.hotkeys", "t"}}));
    CHECK_FALSE(bare.outcome.accepted);
    CHECK(bare.outcome.refusal.find("bare printable") != std::string::npos);

    // ...and the TextBox vocabulary would consume a chord it owns before any
    // global could mean it, in every text context. The two vocabularies used to
    // avoid collision by discipline in two files, checkable nowhere; the
    // component's declaration rows make the wall real.
    const keymap_persist::LoadedKeymap owned = keymap_persist::from_text(
        keymap_file_text("default", {{"workshop.hotkeys", "ctrl+v"}}));
    CHECK_FALSE(owned.outcome.accepted);
    CHECK(owned.outcome.refusal.find("editing vocabulary") != std::string::npos);
}

TEST_CASE("KEY-0: a known backend gap is accepted and said, never silently rewritten") {
    TempDir dir("keymap-gap");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path,
                      keymap_file_text("default", {{"workshop.terminal", "shift+space"}}));
    Keyed t(path);
    t.mount_terminal();
    // Accepted: the authored gesture works where the wire can carry it...
    t.key(input::scan::kSpace, input::mod::kShift);
    CHECK(t.pane().open);
    // ...its own keystroke's space is swallowed, derived from the binding...
    t.text(" ");
    CHECK(t.pane().input.text().empty());
    // ...and the note said the honest half out loud: a POSIX terminal cannot
    // produce it. Nothing in the file was rewritten.
    CHECK(t.notice().find("shift is not observable") != std::string::npos);
    // The default it replaced no longer fires -- an override moves a binding,
    // it does not leave the old one behind as an invisible alias.
    t.key(input::scan::kSpace, input::mod::kShift);
    REQUIRE_FALSE(t.pane().open);
    t.key(input::scan::kT, input::mod::kCtrl);
    CHECK_FALSE(t.pane().open);
}

TEST_CASE("KEY-0: a printable trigger's own character is swallowed, wherever it is authored") {
    TempDir dir("keymap-swallow");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"setup.name", "g"}}));
    Keyed t(path);
    t.host.setup_path = dir.file("setup.json");

    t.key(input::scan::kG);
    t.text("g");
    REQUIRE(t.session().setup.naming.open);
    CHECK(t.session().setup.naming.line.text() == "Default");
    // The swallow belongs to one moment: the next real character is taken.
    t.text("g");
    CHECK(t.session().setup.naming.line.text() == "Defaultg");
    // ...and the default `s` now types an ordinary s, because the binding moved.
    t.key(input::scan::kEscape);
    t.key(input::scan::kS);
    t.text("s");
    CHECK_FALSE(t.session().setup.naming.open);
}

TEST_CASE("KEY-0: the swallow eats only the trigger's own character, never a different one") {
    // The correspondence is the law: the owed character is derived from the consumed
    // binding, and a character that does not match it is a maker's real keystroke -- a
    // layout can make a key produce something other than its face, and an unconditional
    // eat-the-next-text rule would silently delete that character. Here the trigger's key
    // arrives with a DIFFERENT character than its face: the swallow must let it through.
    TempDir dir("keymap-mismatch");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"setup.name", "g"}}));
    Keyed t(path);
    t.host.setup_path = dir.file("setup.json");
    t.key(input::scan::kG);
    t.text("!");
    REQUIRE(t.session().setup.naming.open);
    CHECK(t.session().setup.naming.line.text() == "Default!");
}

TEST_CASE("KEY-0: a shift+letter binding swallows the capital its keystroke produced") {
    TempDir dir("keymap-capital");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"setup.name", "shift+s"}}));
    Keyed t(path);
    t.host.setup_path = dir.file("setup.json");
    t.key(input::scan::kS, input::mod::kShift);
    t.text("S");
    REQUIRE(t.session().setup.naming.open);
    CHECK(t.session().setup.naming.line.text() == "Default");
    t.text("S");
    CHECK(t.session().setup.naming.line.text() == "DefaultS");
}

TEST_CASE("KEY-0: the legend's three modes project the band, and hidden unbinds nothing") {
    TempDir dir("keymap-legend");
    const std::string path = dir.file("keymap.json");

    write_keymap_file(path, keymap_file_text("compact", {}));
    Keyed compact(path);
    const Screen sc = screen_of(compact.session());
    // The legend rows are rows of the band's one region since WUX-1, so they are read
    // through the cell projection with the region's padding trimmed.
    CHECK(inspector_row(compact.canvases.back(), 0, sc.help_y) == "^k hotkeys");
    CHECK(inspector_row(compact.canvases.back(), 0, sc.help_y + 1).empty());

    write_keymap_file(path, keymap_file_text("hidden", {}));
    Keyed hidden(path);
    // Blank rows -- and ONLY blank rows: the band's geometry is `screen_of`'s
    // and the notice and setup line are untouched.
    CHECK(inspector_row(hidden.canvases.back(), 0, sc.help_y).empty());
    CHECK(inspector_row(hidden.canvases.back(), 0, sc.help_y + 1).empty());
    // Hidden never makes the full list unreachable: the binding is dispatch's,
    // and the legend is read by nothing but the band's painter.
    hidden.key(input::scan::kK, input::mod::kCtrl);
    CHECK(hidden.session().hotkeys.open);
    CHECK(stack_text(hidden.canvases.back()).find("HOTKEYS") != std::string::npos);

    write_keymap_file(path, keymap_file_text("full", {}));
    Keyed full(path);
    CHECK(label_at(full.canvases.back(), 0, sc.help_y).rfind("n new | d delete", 0) == 0);
}

TEST_CASE("KEY-0: the picker still closes on the key that opened it, wherever it moved") {
    TempDir dir("keymap-opener");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"workshop.picker", "u"}}));
    Keyed t(path);
    t.key(input::scan::kU);
    t.text("u");
    REQUIRE(t.session().panels.picker.open);
    t.key(input::scan::kU); // the opener's own binding closes it
    CHECK_FALSE(t.session().panels.picker.open);
    t.key(input::scan::kP); // ...and the retired default does neither
    t.text("p");
    CHECK_FALSE(t.session().panels.picker.open);
}

TEST_CASE("KEY-0: the terminal header and hints spell the effective toggle") {
    TempDir dir("keymap-header");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"workshop.terminal", "ctrl+g"}}));
    Keyed t(path);
    t.mount_terminal();
    t.key(input::scan::kG, input::mod::kCtrl);
    REQUIRE(t.pane().open);
    const std::string pane_rows = terminal_text(t.canvases.back(), screen_of(t.session()));
    CHECK(pane_rows.find("(^g closes)") != std::string::npos);
    CHECK(pane_rows.find("shift+space") == std::string::npos);
    t.key(input::scan::kG, input::mod::kCtrl);
    REQUIRE_FALSE(t.pane().open);
    // ...and the band's legend claim moved with it -- the top-row hint is retired (WUX-1),
    // so the toggle's one remaining compact claim surface is the pairs the band packs.
    const std::vector<std::string> pairs =
        help_pairs(t.session().keymap, KeyContext::kCommand);
    bool spelled = false;
    for (const std::string& pair : pairs) {
        CHECK(pair.find("^t terminal") == std::string::npos); // the old spelling is gone
        spelled = spelled || pair == "^g terminal";
    }
    CHECK(spelled);
    CHECK(t.notice() == "terminal closed -- ^g reopens it");
}

// ============================================================================
// ---- WUX-1: the graphical voice -------------------------------------------
//
// The phase's own contract: Workshop-owned prose stops being cell-voiced merely
// because its composition was given one cell of height. The band is one
// budget-composed region, the shared top row is retired, the Builder composes by
// priority, the `OBJECTS` heading joined its panel's region, and pane titles are
// a presentation preference with a KEY-0 action -- with keyboard identity never
// hidden. The TUI's cell budgets select the composition every prior phase pinned,
// which is what the untouched cases above this section keep proving.

namespace {

/// The band region a canvas actually published, or nullptr -- by its place.
const surface::SurfaceTextRegion* band_on(const surface::SurfaceCanvas& c, const Screen& sc) {
    const ui::Rect b = band_bounds(sc);
    for (const surface::SurfaceLayer& layer : c.layers) {
        for (const surface::SurfaceTextRegion& r : layer.texts) {
            if (r.x == b.x && r.y == b.y && r.h == b.h) {
                return &r;
            }
        }
    }
    return nullptr;
}

/// The TOP band region a canvas published, or nullptr -- by its place (QR-14).
const surface::SurfaceTextRegion* top_band_on(const surface::SurfaceCanvas& c,
                                              const Screen& sc) {
    const ui::Rect b = top_band_bounds(sc);
    for (const surface::SurfaceLayer& layer : c.layers) {
        for (const surface::SurfaceTextRegion& r : layer.texts) {
            if (r.x == b.x && r.y == b.y && r.h == b.h) {
                return &r;
            }
        }
    }
    return nullptr;
}

/// A region row's text -- "" for a row the composition left unsaid.
std::string band_row(const surface::SurfaceTextRegion* band, std::size_t i) {
    if (band == nullptr || i >= band->rows.size()) {
        return {};
    }
    return band->rows[i].text;
}

} // namespace

TEST_CASE("WUX-1/SC-1: the shipped face reads every Workshop-owned sentence as real type") {
    // THE PHASE'S TARGET LAW, as one sweep: at the shipped metric, a full screen --
    // Info open, Builder open, an object selected -- publishes its prose as regions the
    // graphical medium sets in type, and the only `SurfaceLabel`s left are the ones whose
    // CELL is the meaning (the size handle; management's edge glyphs when arranging).
    WorkshopDoc d = two_panels();
    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    s.selected = d.elements[0].id;
    refocus(d, s);
    (void)open_panel(s.panels, panel::kBuilder);
    const surface::SurfaceCanvas c = paint(d, s);

    for (const surface::SurfaceLabel& l : all_labels(c)) {
        CAPTURE(l.text);
        CHECK(l.text == std::string(kHandleGlyph));
    }

    // ...and every published region's bounds hold at least one row of the face, so none
    // of them falls back to the cell voice: the band, the Builder, the whole Info panel
    // (heading included), and each object's name over material at least two cells tall.
    const Screen sc = screen_of(s);
    for (const surface::SurfaceTextRegion& r : all_texts(c)) {
        CAPTURE(r.x);
        CAPTURE(r.y);
        CHECK(surface::fit_region(r, surface::SurfaceExtent{0, 0, 8, 18}).graphical());
    }
    CHECK(band_on(c, sc) != nullptr);
}

TEST_CASE("QR-14/SC-2+SC-7: two bands compose their budgets, and the selector is row 0") {
    WorkshopDoc d;
    doc::add_default(d);

    // A CHARACTER MEDIUM: two rows at the top (the layout selector with the setup's status,
    // then the workspace fact) and four at the foot (the notice, then the legend takes what
    // the notice leaves). Five facts in six reserved rows, which is what the screen has
    // always reserved -- WUX-1 left one of them blank at row 0 and QR-14 spends it.
    Session cells = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    cells.notice = "created #1";
    refocus(d, cells);
    const Screen csc = screen_of(cells);
    const surface::SurfaceCanvas cell_canvas = paint(d, cells);

    const surface::SurfaceTextRegion* ctop = top_band_on(cell_canvas, csc);
    REQUIRE(ctop != nullptr);
    CHECK(ctop->y == 0); // THE FIRST WORKSHOP ROW IS THE LAYOUT SELECTOR
    CHECK(top_band_fit(csc).rows == kTopRows);
    REQUIRE(ctop->rows.size() == 2);
    CHECK(band_row(ctop, 0).rfind("> \"Default\"", 0) == 0); // the live layout tab (WUX-9)
    CHECK(band_row(ctop, 0).find("| UNSAVED") != std::string::npos);
    CHECK(band_row(ctop, 1) == "workspace 48x16 cells");

    const surface::SurfaceTextRegion* cband = band_on(cell_canvas, csc);
    REQUIRE(cband != nullptr);
    CHECK(band_fit(csc).rows == kBottomRows);
    REQUIRE(cband->rows.size() == 4);
    CHECK(band_row(cband, 0) == "created #1");
    CHECK(band_row(cband, 1).rfind("n new | d delete", 0) == 0);
    CHECK_FALSE(band_row(cband, 2).empty());
    CHECK_FALSE(band_row(cband, 3).empty()); // no reserved row is left spare
    // ...AND NO ROW SAYS ANYTHING TWICE. The identity is the top band's and the notice is
    // the bottom band's, and neither writes in the other's rectangle.
    CHECK(band_row(cband, 0).find("\"Default\"") == std::string::npos);
    CHECK(band_row(ctop, 0).find("created #1") == std::string::npos);
    CHECK(ctop->y + ctop->h == cells_covered(fine_of_cells(ui::Rect{0, kWorkspaceY, 1, 1})).y);
    CHECK(cband->y == kWorkspaceY + csc.room_h); // the body ends where the band begins

    // THE SHIPPED FACE: one row at the top (the identity with the workspace fact folded in,
    // WUX-1's own fold) and two at the foot (the notice and one packed legend row). THREE
    // face rows of chrome, exactly as many as the single five-cell band held.
    Session sdl = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    sdl.notice = "created #1";
    refocus(d, sdl);
    const Screen ssc = screen_of(sdl);
    const surface::SurfaceCanvas sdl_canvas = paint(d, sdl);
    const surface::SurfaceTextRegion* stop = top_band_on(sdl_canvas, ssc);
    const surface::SurfaceTextRegion* sband = band_on(sdl_canvas, ssc);
    REQUIRE(stop != nullptr);
    REQUIRE(sband != nullptr);
    CHECK(top_band_fit(ssc).rows == 1);
    CHECK(band_fit(ssc).rows == 2);
    REQUIRE(stop->rows.size() == 1);
    REQUIRE(sband->rows.size() == 2);
    CHECK(band_row(stop, 0).rfind("> \"Default\"", 0) == 0);
    CHECK(band_row(stop, 0).find("| workspace 48x16 cells") != std::string::npos);
    CHECK(band_row(sband, 0) == "created #1");
    CHECK(band_row(sband, 1).rfind("n new | d delete", 0) == 0);

    // DEGRADE HONESTLY BELOW THAT, and each band degrades in its own rectangle: the legend
    // gives way first at the foot, and the workspace fact folds into the identity at the top.
    Session three = screen_session(kScreenMinW, kScreenMinH, 8, 14); // 44/14 = 3 band rows
    three.notice = "created #1";
    refocus(d, three);
    const surface::SurfaceCanvas three_canvas = paint(d, three);
    const surface::SurfaceTextRegion* tband = band_on(three_canvas, screen_of(three));
    REQUIRE(tband != nullptr);
    REQUIRE(tband->rows.size() == 3);
    CHECK(band_row(tband, 0) == "created #1");
    CHECK(band_row(tband, 1).rfind("n new | d delete", 0) == 0);

    Session one = screen_session(kScreenMinW, kScreenMinH, 8, 30); // 44/30 = 1 band row
    one.notice = "created #1";
    refocus(d, one);
    const surface::SurfaceCanvas one_canvas = paint(d, one);
    const surface::SurfaceTextRegion* oband = band_on(one_canvas, screen_of(one));
    const surface::SurfaceTextRegion* otop = top_band_on(one_canvas, screen_of(one));
    REQUIRE(oband != nullptr);
    REQUIRE(oband->rows.size() == 1);
    CHECK(band_row(oband, 0) == "created #1"); // the tool's voice wins the one row
    // ⚠ AND THE IDENTITY IS NOT A CANDIDATE FOR THAT ROW ANY MORE. It has a band of its own
    // that no budget down here can take, which is the whole point of the move: a maker never
    // loses sight of which desk they are in because the tool had something to say.
    REQUIRE(otop != nullptr);
    REQUIRE_FALSE(otop->rows.empty());
    CHECK(band_row(otop, 0).rfind("> \"Default\"", 0) == 0);

    one.notice.clear();
    const surface::SurfaceCanvas quiet_canvas = paint(d, one);
    const surface::SurfaceTextRegion* qband = band_on(quiet_canvas, screen_of(one));
    REQUIRE(qband != nullptr);
    REQUIRE(qband->rows.size() == 1);
    CHECK(qband->rows[0].text.rfind("n new | d delete", 0) == 0); // the legend, with nothing said
}

TEST_CASE("WUX-1/SC-3: the legend modes move only the legend rows, in both budgets") {
    WorkshopDoc d;
    doc::add_default(d);
    for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
        CAPTURE(line);
        Session s = screen_session(kScreenMinW, kScreenMinH, line == 0 ? 0 : 8, line);
        s.notice = "a notice";
        refocus(d, s);
        const Screen sc = screen_of(s);
        // THE LEGEND IS THE BOTTOM BAND'S SECOND ROW ON EVERY MEDIUM SINCE QR-14: the
        // notice leads that band and the legend takes what it leaves.
        const std::size_t legend_at = 1;

        s.keymap.legend = legend_mode::kFull;
        const surface::SurfaceCanvas full_c = paint(d, s);
        const surface::SurfaceTextRegion* full_b = band_on(full_c, sc);
        REQUIRE(full_b != nullptr);
        CHECK(band_row(full_b, legend_at).rfind("n new | d delete", 0) == 0);

        s.keymap.legend = legend_mode::kCompact;
        const surface::SurfaceCanvas compact_c = paint(d, s);
        const surface::SurfaceTextRegion* compact_b = band_on(compact_c, sc);
        REQUIRE(compact_b != nullptr);
        CHECK(band_row(compact_b, legend_at) == "^k hotkeys");

        s.keymap.legend = legend_mode::kHidden;
        const surface::SurfaceCanvas hidden_c = paint(d, s);
        const surface::SurfaceTextRegion* hidden_b = band_on(hidden_c, sc);
        REQUIRE(hidden_b != nullptr);
        CHECK(band_row(hidden_b, legend_at).empty());

        // THE OTHER ROWS NEVER MOVE WITH THE PREFERENCE: a maker toggling the legend
        // watches the legend, not a reflowing band -- and since QR-14 that includes the
        // identity row, which is a whole band away and cannot be reached from here.
        for (std::size_t i = 0; i < legend_at; ++i) {
            CAPTURE(i);
            CHECK(band_row(full_b, i) == band_row(compact_b, i));
            CHECK(band_row(full_b, i) == band_row(hidden_b, i));
        }
        CHECK(band_row(top_band_on(full_c, sc), 0) ==
              band_row(top_band_on(hidden_c, sc), 0));
    }
}

TEST_CASE("WUX-1/SC-2: the hotkey view remains the full claim surface for the moved hints") {
    // The three gestures the retired row advertised are ordinary keymap rows, so the
    // authoritative surface -- the full hotkey view -- lists all three by label. A tall
    // screen, so the whole grouped list fits (the minimum extent's `... N more` elision
    // is the view's own pinned behavior, not this claim's).
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{120, 70, 0, 0}));
    t.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(t.session().hotkeys.open);
    // The view is the stack COLUMN, floor to ceiling -- taller than the first slot, so it
    // is read at its own bounds rather than through the slot accessor.
    const std::string view = panel_text(
        t.canvases.back(),
        pane_body_cells(hotkeys_bounds(t.session(), screen_of(t.session()))));
    CHECK(view.find("terminal") != std::string::npos);
    CHECK(view.find("arrange desk") != std::string::npos);
    CHECK(view.find("+ panel") != std::string::npos);
    CHECK(view.find("titles") != std::string::npos); // the new action is discoverable too
}

TEST_CASE("WUX-1/SC-4: the Builder keeps the facts a maker acts on, by explicit priority") {
    BuilderPane pane;
    pane.heard = true;
    pane.known.recipes.push_back(
        zengine::builder::RecipeSummary{"zengine-snake", "libzengine-snake"});
    pane.shown.outcome = zengine::builder::outcome::kSucceeded;
    pane.shown.status = 0;
    pane.shown.command = "cmake --build build";
    pane.shown.detail = "a compiler sentence long enough to wrap across several rows of "
                        "the panel so the tail is genuinely elided at every budget";
    pane.shown.realization = zengine::builder::realization::kNotAsked;
    // THE RECTANGLE IS SIZED SO THE INTERIOR IS THE 48x9 THIS CASE IS ABOUT (WUX-5). Every
    // budget below is a fact about the Builder's composition PRIORITY, not about how much
    // room a stack slot happens to leave once its visible boundary is subtracted -- so the
    // chrome is added to the ask here and the pinned compositions are untouched.
    const ui::Rect slot{0, 1, 48 + 2 * kChromeCells, 9 + 2 * kChromeCells};
    ProjectFrontier waiting;
    waiting.waiting = true;
    waiting.artifact = "libzengine-snake";
    waiting.blocked = 3;

    const auto rows_at = [&](std::int64_t line, const ProjectFrontier& f) {
        surface::SurfaceCanvas c;
        paint_builder(plane(c), pane, fine_of_cells(slot),
                      screen_of(kScreenMinW, kScreenMinH, line == 0 ? 0 : 8, line),
                      f);
        std::vector<std::string> out;
        for (const surface::SurfaceTextRegion& r : all_texts(c)) {
            for (const surface::SurfaceTextRow& row : r.rows) {
                out.push_back(row.text);
            }
        }
        return out;
    };

    // A CELL MEDIUM'S NINE ROWS ARE THE COMPOSITION EVERY PRIOR PHASE PINNED, in the
    // display order that never changes: header, recipe, [project], last, exit, ran,
    // realize, said...
    const std::vector<std::string> nine = rows_at(0, ProjectFrontier{});
    REQUIRE(nine.size() == 9);
    CHECK(nine[0].rfind("BUILDER @", 0) == 0);
    CHECK(nine[1].rfind("recipe", 0) == 0);
    CHECK(nine[2].rfind("last", 0) == 0);
    CHECK(nine[3].rfind("exit", 0) == 0);
    CHECK(nine[4].rfind("ran", 0) == 0);
    CHECK(nine[5].rfind("realize", 0) == 0);
    CHECK(nine[6].rfind("said", 0) == 0);

    // THE SHIPPED FACE'S FIVE keep the header, what `b` does next, the live result, the
    // second outcome, and the first row of the compiler's own words -- the static
    // metadata (exit, ran) yields, WHOLE, with nothing substituted in its place.
    const std::vector<std::string> five = rows_at(18, ProjectFrontier{});
    REQUIRE(five.size() == 5);
    CHECK(five[0].rfind("BUILDER @", 0) == 0);
    CHECK(five[1].rfind("recipe", 0) == 0);
    CHECK(five[2].rfind("last", 0) == 0);
    CHECK(five[3].rfind("realize", 0) == 0);
    CHECK(five[4].rfind("said", 0) == 0);
    CHECK(five[4].find(detail::kElided) != std::string::npos); // the cut is THIS budget's
    for (const std::string& row : five) {
        CHECK(row.rfind("exit", 0) != 0);
        CHECK(row.rfind("ran", 0) != 0);
    }

    // ...AND THE FRONTIER STAYS LEGIBLE WHEN PRESENT, at five rows and at three.
    const std::vector<std::string> five_waiting = rows_at(18, waiting);
    REQUIRE(five_waiting.size() == 5);
    CHECK(five_waiting[0].rfind("BUILDER @", 0) == 0);
    CHECK(five_waiting[1].rfind("recipe", 0) == 0);
    CHECK(five_waiting[2].rfind("project", 0) == 0);
    CHECK(five_waiting[2].find("blocks 3") != std::string::npos);
    CHECK(five_waiting[3].rfind("last", 0) == 0);
    CHECK(five_waiting[4].rfind("realize", 0) == 0);

    const std::vector<std::string> three_waiting = rows_at(28, waiting); // (104)/28 = 3
    REQUIRE(three_waiting.size() == 3);
    CHECK(three_waiting[0].rfind("BUILDER @", 0) == 0);
    CHECK(three_waiting[1].rfind("project", 0) == 0);
    CHECK(three_waiting[2].rfind("last", 0) == 0);

    // TWO ROWS: the office's identity and the live result -- the two facts that survive
    // longest, still in display order.
    const std::vector<std::string> two = rows_at(40, ProjectFrontier{}); // (104)/40 = 2
    REQUIRE(two.size() == 2);
    CHECK(two[0].rfind("BUILDER @", 0) == 0);
    CHECK(two[1].rfind("last", 0) == 0);
}

TEST_CASE("PROJ-1: the catalog row costs one `said` row, and only where it is present") {
    // ⭐ THE MEASUREMENT THAT DECIDED THE PRESENTATION. This panel seats NINE facts in the
    // nine rows a character medium answers -- exactly full -- so a tenth UNCONDITIONAL row
    // would spend the third `said` row of every session that ever opens this panel, to
    // restate a fact the host's own banner already said correctly at launch. The row
    // therefore exists exactly while the fact has MOVED, which is the `project` row's own
    // rule and the same one-`said`-row trade.
    //
    // ⚠ WHAT IS ASSERTED HERE IS THE COST, NOT THE STRING. Before any replacement the
    // composition must be byte-for-byte the one WUX-1 pinned above -- at the nine-row cell
    // budget AND at the shipped face's five -- and after one it must have taken one row
    // and no more, from the end of the `said` block and from nowhere else.
    BuilderPane pane;
    pane.heard = true;
    pane.known.recipes.push_back(
        zengine::builder::RecipeSummary{"zengine-snake", "libzengine-snake"});
    pane.shown.outcome = zengine::builder::outcome::kSucceeded;
    pane.shown.command = "cmake --build build";
    pane.shown.detail = "a compiler sentence long enough to wrap across several rows of "
                        "the panel so the tail is genuinely elided at every budget";
    pane.shown.realization = zengine::builder::realization::kNotAsked;
    const ui::Rect slot{0, 1, 48 + 2 * kChromeCells, 9 + 2 * kChromeCells}; // interior 48x9

    const auto rows_at = [&](std::int64_t line, const std::string& catalog) {
        surface::SurfaceCanvas c;
        paint_builder(plane(c), pane, fine_of_cells(slot),
                      screen_of(kScreenMinW, kScreenMinH, line == 0 ? 0 : 8, line),
                      ProjectFrontier{}, catalog);
        std::vector<std::string> out;
        for (const surface::SurfaceTextRegion& r : all_texts(c)) {
            for (const surface::SurfaceTextRow& row : r.rows) {
                out.push_back(row.text);
            }
        }
        return out;
    };

    // NOTHING MOVED: the panel this repository has painted since BLD-2, unchanged.
    const std::vector<std::string> before = rows_at(0, std::string());
    REQUIRE(before.size() == 9);
    CHECK(before[6].rfind("said", 0) == 0);
    for (const std::string& row : before) {
        CHECK(row.rfind("catalog", 0) != 0);
    }
    // ...and the shipped face's five, likewise untouched.
    const std::vector<std::string> face_before = rows_at(18, std::string());
    REQUIRE(face_before.size() == 5);
    CHECK(face_before[4].rfind("said", 0) == 0);

    // A SESSION THAT HAS MOVED: nine rows still, the catalog row in DISPLAY order right
    // after the recipe it is about, and the cost taken from the tail of `said`.
    const std::vector<std::string> after = rows_at(0, "catalogs/other-recipes.json");
    REQUIRE(after.size() == 9);
    CHECK(after[0].rfind("BUILDER @", 0) == 0);
    CHECK(after[1].rfind("recipe", 0) == 0);
    CHECK(after[2].rfind("catalog", 0) == 0);
    CHECK(after[2].find("catalogs/other-recipes.json") != std::string::npos);
    CHECK(after[3].rfind("last", 0) == 0);
    CHECK(after[4].rfind("exit", 0) == 0);
    CHECK(after[5].rfind("ran", 0) == 0);
    CHECK(after[6].rfind("realize", 0) == 0);
    CHECK(after[7].rfind("said", 0) == 0);
    // THE COST, AS ARITHMETIC. `panel_block` labels only the FIRST row of the said block
    // and indents its wraps, so the block is "everything from the `said` row to the end":
    // three rows before, two after, and the row it lost is the LAST one.
    CHECK(before.size() - 6 == 3);
    CHECK(after.size() - 7 == 2);
    // ...and the elision mark moves with it, so what a maker reads is honest about THIS
    // budget rather than about the one the panel had a moment ago.
    CHECK(after.back().find(detail::kElided) != std::string::npos);

    // ⭐ AND THE SHIPPED FACE PAYS NOTHING AT ALL. Its five rows are the five it always
    // had: the catalog row's priority puts it last, so a budget that could not seat ten
    // facts before cannot be made to drop one for this.
    const std::vector<std::string> face_after = rows_at(18, "catalogs/other-recipes.json");
    CHECK(face_after == face_before);
}

TEST_CASE("WUX-1/SC-5: pane titles are one action, one binding truth, one dispatch") {
    // THE DEFAULT: bare `t` in command mode, declared in the catalog like every gesture.
    Live t;
    REQUIRE(t.session().pane_titles);
    t.key(input::scan::kT);
    t.text("t");
    CHECK_FALSE(t.session().pane_titles);
    CHECK(t.notice() ==
          "pane titles hidden -- a pane holding the keyboard still shows its own");
    t.key(input::scan::kT);
    t.text("t");
    CHECK(t.session().pane_titles);
    CHECK(t.notice() == "pane titles shown");

    // THE BAND'S PAIR IS THE CATALOG'S, so the claim follows the effective binding.
    const std::vector<std::string> pairs = help_pairs(t.session().keymap, KeyContext::kCommand);
    bool spelled = false;
    for (const std::string& pair : pairs) {
        spelled = spelled || pair == "t titles";
    }
    CHECK(spelled);
}

TEST_CASE("WUX-1/SC-5+SC-9: the titles action remaps and collides like every other row") {
    // AN OVERRIDE MOVES DISPATCH AND HELP TOGETHER -- one truth, no hard-coded second path.
    TempDir dir("wux1-titles-remap");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"workshop.pane-titles", "ctrl+e"}}));
    Keyed t(path);
    REQUIRE(t.session().keymap.overrides.size() == 1);
    t.key(input::scan::kT); // the retired default does nothing now
    t.text("t");
    CHECK(t.session().pane_titles);
    t.key(input::scan::kE, input::mod::kCtrl);
    CHECK_FALSE(t.session().pane_titles);
    const std::vector<std::string> pairs = help_pairs(t.session().keymap, KeyContext::kCommand);
    bool old_spelling = false;
    bool new_spelling = false;
    for (const std::string& pair : pairs) {
        old_spelling = old_spelling || pair == "t titles";
        new_spelling = new_spelling || pair == "^e titles";
    }
    CHECK_FALSE(old_spelling);
    CHECK(new_spelling);

    // AND THE ADMISSION WALLS APPLY: a same-context collision is refused WHOLE, so the
    // new action cannot be authored onto another command gesture.
    Keymap refused;
    const Written verdict =
        apply_overrides({{"workshop.pane-titles", "n"}}, legend_mode::kDefault, refused);
    CHECK_FALSE(verdict.accepted);
    CHECK(verdict.refusal.find("workshop.pane-titles") != std::string::npos);
    CHECK(verdict.refusal.find("object.new") != std::string::npos);
}

TEST_CASE("WUX-1/SC-5+SC-6: hiding titles returns the row; the keyboard's pane keeps its own") {
    PaneRig r;
    r.mount_workshop();
    r.ready();
    r.extent(100, 44); // two overlay slots, so both panes are genuinely presented
    ProviderSeat* first = r.mount_provider(kHelloOffice);
    ProviderSeat* second = r.mount_provider("zengine.other");
    const std::int64_t a = seat_pane_open(r, first, kHelloOffice, kHelloPane);
    const std::int64_t b = seat_pane_open(r, second, "zengine.other", "pane");
    REQUIRE(a != kNoPaneKind);
    REQUIRE(b != kNoPaneKind);
    REQUIRE(first->rooms.size() == 1);
    const std::int64_t titled_rows = first->rooms.back().rows;

    // NO PANE FOCUSED: hiding titles returns the header row to BOTH providers, and the
    // published panels carry no header row.
    r.key(input::scan::kT);
    r.text("t");
    REQUIRE_FALSE(r.session().pane_titles);
    REQUIRE(first->rooms.size() == 2);
    CHECK(first->rooms.back().rows == titled_rows + kExternalHeaderRows);
    REQUIRE(second->rooms.size() == 2);
    const auto shown_rows = [&](std::int64_t kind) {
        return external_region_rows(r.last_canvas(), external_body_rect(r.session(), kind));
    };
    CHECK(shown_rows(a).at(0).find("Seat @") == std::string::npos);

    // AN EXTERNAL PANE FOCUSED: its title auto-shows, mark and all, and ITS room shrinks
    // back -- the other pane stays bare (SC-6: presentation may hide ordinary chrome; it
    // may not hide where typing goes).
    press_body(r, a);
    REQUIRE(keyboard_pane(r.session().panels) == a);
    CHECK(first->rooms.size() == 3);
    CHECK(first->rooms.back().rows == titled_rows);
    CHECK(shown_rows(a).at(0).rfind(std::string(kTypingHere) + "Seat @", 0) == 0);
    CHECK(shown_rows(b).at(0).find("@zengine.other") == std::string::npos);

    // FOCUS MOVED BETWEEN PANES WHILE HIDDEN: the title follows the keyboard.
    press_body(r, b);
    REQUIRE(keyboard_pane(r.session().panels) == b);
    CHECK(shown_rows(b).at(0).rfind(std::string(kTypingHere) + "Seat @", 0) == 0);
    CHECK(shown_rows(a).at(0).find("Seat @" + std::string(kHelloOffice)) ==
          std::string::npos);

    // FOCUS RELEASED by pressing outside every pane: the keyboard is Workshop's again,
    // so no runtime pane shows a title.
    press_outside(r, b);
    CHECK(keyboard_pane(r.session().panels) == kNoPaneKind);
    CHECK(shown_rows(a).at(0).find("Seat @") == std::string::npos);
    CHECK(shown_rows(b).at(0).find("@zengine.other") == std::string::npos);

    // TITLES BACK: every pane says whose it is again, and the rooms shrink to make room.
    r.key(input::scan::kT);
    r.text("t");
    REQUIRE(r.session().pane_titles);
    CHECK(first->rooms.back().rows == titled_rows);
    CHECK(shown_rows(a).at(0).find("Seat @") != std::string::npos);

    // AND THE TOGGLE TOUCHED NO IDENTITY: the runtime catalog rows and the setup's
    // references are exactly what they were through the whole conversation.
    CHECK(r.session().panels.runtime.of_kind(a) != nullptr);
    CHECK(r.session().panels.runtime.of_kind(b) != nullptr);
    CHECK(r.session().setup.active.panes.size() >= 2);
}

TEST_CASE("WUX-1/SC-6: the press lattice follows the reserved rows, titles hidden or shown") {
    // HD-3's law through the preference: the row a provider means by 0 is the row under
    // whatever header rows this presentation actually reserved -- resolved once, spent by
    // the painter, the press path and the grant alike.
    PaneRig r;
    r.mount_workshop();
    r.ready();
    ProviderSeat* seat = r.mount_provider(kHelloOffice);
    const std::int64_t kind = seat_pane_open(r, seat, kHelloOffice, kHelloPane);
    const ui::Rect body = external_body_rect(r.session(), kind);

    // Titles shown: the first body row is one cell under the panel's top.
    const std::size_t presses_before = seat->presses.size();
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows);
    REQUIRE(seat->presses.size() == presses_before + 1);
    CHECK(seat->presses.back().row == 0);

    // Titles hidden AND the pane unfocused... except a press INTO the pane focuses it,
    // which auto-shows its title -- so the lattice a press lands in is the titled one,
    // and row 0 is still the row under the header. The un-titled lattice is on screen
    // only while the pane does not hold the keyboard, which no press into it can be
    // true of. That asymmetry is SC-6's invariant working, not an off-by-one.
    press_outside(r, kind);
    r.key(input::scan::kT);
    r.text("t");
    REQUIRE_FALSE(r.session().pane_titles);
    const std::size_t hidden_before = seat->presses.size();
    r.press_cell(body.x + 1, body.y); // the panel's top row: bare, no header reserved
    // The press focuses the pane; the focused pane reserves its title again, so this
    // cell is the header's and names no body row.
    CHECK(seat->presses.size() == hidden_before);
    CHECK(keyboard_pane(r.session().panels) == kind);
    const std::size_t focused_before = seat->presses.size();
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows);
    REQUIRE(seat->presses.size() == focused_before + 1);
    CHECK(seat->presses.back().row == 0);
}

// ============================================================================
// CTX-0 — deleting the pointed object: the explicit-id door
// ============================================================================
//
// `delete_object_at(id)` is `delete_selected`'s target-taking sibling, and the pin is the
// branch: the neighbour/selection repair runs EXACTLY when the deleted id is the selected
// one, and a deletion that touched no selection perturbs none.

TEST_CASE("CTX-0: contextually deleting the selected object uses the existing repair") {
    Live t;
    REQUIRE(t.session().selected == 1);
    t.right_press(4, 3); // #1's body -- the selected one
    REQUIRE(t.menu().subject == context_subject::kObject);
    REQUIRE(t.menu().object == 1);
    t.key(input::scan::kReturn); // the object's one row: delete
    CHECK_FALSE(t.menu().open);
    CHECK(doc::find(t.doc(), 1) == nullptr);
    // The post-delete selection rule, byte for byte the keyboard's: the object that took
    // the deleted one's place in authored order.
    CHECK(t.session().selected == 2);
    CHECK(t.notice() == "deleted #1 -- now on #2");
}

TEST_CASE("CTX-0: contextually deleting a pointed object preserves an unrelated selection") {
    Live t;
    REQUIRE(t.session().selected == 1);
    t.right_press(7, 11); // #2's body -- NOT the selection
    REQUIRE(t.menu().object == 2);
    t.key(input::scan::kReturn);
    CHECK(doc::find(t.doc(), 2) == nullptr);
    CHECK(t.session().selected == 1); // the maker's selection was never transport
    CHECK(t.notice() == "deleted #2");
    // ...and the inspector still describes the selected object, rebuilt, never patched.
    REQUIRE_FALSE(t.session().rows.empty());
}

TEST_CASE("CTX-0: a live draft holds a contextual deletion back") {
    Live t;
    t.begin_editing("Name");
    REQUIRE(editing_index(t) < t.session().rows.size());
    t.right_press(7, 11);
    REQUIRE(t.menu().object == 2);
    t.key(input::scan::kReturn); // choose delete -- and the application holds it back
    CHECK_FALSE(t.menu().open);
    CHECK(doc::find(t.doc(), 2) != nullptr); // nothing was deleted
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("finish the draft first") != std::string::npos);
    CHECK(editing_index(t) < t.session().rows.size()); // the draft survived whole
}

TEST_CASE("CTX-0: replacing the document drops a captured object subject") {
    Live t;
    TempDir dir("ctx-doc");
    t.host.document_path = dir.document();
    t.key(input::scan::kS, input::mod::kCtrl); // save the two-object document
    REQUIRE_FALSE(t.session().notice_is_bad);

    SUBCASE("an object subject cannot alias into the replacement") {
        t.right_press(7, 11);
        REQUIRE(t.menu().subject == context_subject::kObject);
        // `document.open` is a global, answered above the open surface -- which is
        // exactly why the surface must notice the ground moving underneath it.
        t.key(input::scan::kO, input::mod::kCtrl);
        REQUIRE_FALSE(t.session().notice_is_bad);
        CHECK_FALSE(t.menu().open);
    }
    SUBCASE("a room subject names nothing the replacement touched, and stands") {
        t.right_press(40, 0);
        REQUIRE(t.menu().subject == context_subject::kRoot);
        t.key(input::scan::kO, input::mod::kCtrl);
        REQUIRE_FALSE(t.session().notice_is_bad);
        CHECK(t.menu().open);
    }
}

TEST_CASE("CTX-0: the shipped catalog stays admissible with the new rows") {
    // `apply_overrides` over an empty authored set runs the same-gesture collision sweep
    // across the EFFECTIVE map -- the defaults themselves. A new declaration colliding
    // with an existing one in an intersecting context would refuse right here.
    Keymap out;
    const Written admitted = apply_overrides({}, legend_mode::kDefault, out);
    REQUIRE(admitted.accepted);
    // The two CTX-0 identities hold their researched defaults.
    CHECK(out.gesture_of(Act::kManageRemove) ==
          Gesture{input::scan::kD, input::mod::kNone});
    CHECK(out.gesture_of(Act::kContextOpen) ==
          Gesture{input::scan::kA, input::mod::kNone});
    // ...and both remap like any other action, all rows moving together.
    Keymap moved;
    const Written re = apply_overrides(
        {{"manage.remove", "x"}, {"workshop.context", ";"}}, legend_mode::kDefault, moved);
    REQUIRE(re.accepted);
    CHECK(moved.gesture_of(Act::kManageRemove) ==
          Gesture{input::scan::kX, input::mod::kNone});
    CHECK(moved.action_for(KeyContext::kArrangeDesk, input::scan::kX, input::mod::kNone) ==
          Act::kManageRemove);
    CHECK(moved.action_for(KeyContext::kArrangeDesk, input::scan::kD, input::mod::kNone) ==
          Act::kNone);
    // ...and the one override moved BOTH scopes' rows -- one action, one authored gesture.
    CHECK(moved.action_for(KeyContext::kArrangePane, input::scan::kX, input::mod::kNone) ==
          Act::kManageRemove);
}
