// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop keys suite — the typed rows a maker edits through, the keymap that says which
// gesture a key is, and the text a maker types into Workshop's own boxes.
//
// ⭐ THE NAME IS THE SUITE'S HISTORY. It held the object document -- the prototype canvas's
// authored rectangles, their identities, the hands that moved and resized them, the file that
// kept them and the seam that showed them to the Info pane -- until that canvas retired, and
// the cases that remained were never about it. They stay here under the name the population
// file, the registers and the build already use.
//
// Everything here is headless and pure, or drives the real weave on a real bus. Nothing in
// Workshop's own logic needs a terminal, so nothing here has one.
//
// What it holds:
//   1. THE TYPED PROPERTY CONNECTION -- read through the semantic surface, commit through it,
//      the two ways a commit can fail told apart, and the reuse pin: two properties of one
//      type share every line of conversion. Proven over a plain subject; the rows a maker
//      edits are a pane's (`pane_subject_rows`), and their owner's cases are the Info suite's.
//   2. THE WEAVE ON A REAL BUS -- the quit policy both doors reach, and a notice longer than
//      its line.
//   3. EDITABLE TEXT AS A COMPONENT -- the naming line is a TextBox, and clipboard reads
//      follow paste intent.
//   4. THE KEYMAP -- one executable binding truth: the context resolver, the authored file,
//      the legend, and the chord.
//
// The screen these cases paint is asserted in `test_workshop_screen.cpp`; the panels they
// open are `test_workshop_panels.cpp`; what survives a process is
// `test_workshop_persistence.cpp`.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// ============================================================================
// Tier 1 — the typed property connection
// ============================================================================
//
// ⭐ PROVEN OVER A PLAIN SUBJECT. These laws are about `Row` and `Property`, not about any
// one owner of rows, so the subject below is nothing but its properties and setters that
// refuse in words. They were proven over the object document's rectangles until that
// document retired; the claims did not change, and neither did a line of `property.hpp`
// they exercise.

namespace {

/// A name and three whole numbers, and nothing else.
struct Probe {
    std::string name = "panel";
    std::int64_t x = 3;
    std::int64_t width = 60;
    std::int64_t height = 6;
};

Written probe_name(Probe& p, std::string v) {
    if (v.empty()) {
        return Written::no("a name cannot be empty");
    }
    p.name = std::move(v);
    return Written::ok();
}

Written probe_x(Probe& p, std::int64_t v) {
    if (v < 0) {
        return Written::no("the room starts at 0");
    }
    p.x = v;
    return Written::ok();
}

/// ONE CHECK FOR BOTH EXTENTS, so the reuse case can prove a refusal is the check's words.
Written probe_extent(std::int64_t& at, std::int64_t v) {
    if (v < 1 || v > 100) {
        return Written::no("an extent is 1 to 100 cells");
    }
    at = v;
    return Written::ok();
}

Property<std::string> name_of(Probe& p) {
    return Property<std::string>([&p] { return p.name; },
                                 [&p](std::string v) { return probe_name(p, std::move(v)); });
}

Property<std::int64_t> x_of(Probe& p) {
    return Property<std::int64_t>([&p] { return p.x; },
                                  [&p](std::int64_t v) { return probe_x(p, v); });
}

Property<std::int64_t> width_of(Probe& p) {
    return Property<std::int64_t>([&p] { return p.width; },
                                  [&p](std::int64_t v) { return probe_extent(p.width, v); });
}

Property<std::int64_t> height_of(Probe& p) {
    return Property<std::int64_t>([&p] { return p.height; },
                                  [&p](std::int64_t v) { return probe_extent(p.height, v); });
}

void clear_draft(Row& row) {
    while (!row.draft().empty()) {
        row.backspace();
    }
}

} // namespace

TEST_CASE("a property reads the current typed value through the semantic surface") {
    Probe p;
    CHECK(name_of(p).read() == "panel");
    CHECK(x_of(p).read() == 3);
    CHECK(width_of(p).read() == 60);

    // A property is a LIVE connection, not a snapshot: a change made anywhere
    // else is what the next read returns. This is why no Row caches a value and
    // why nothing in this package has a "refresh the inspector" call.
    p.name = "renamed";
    CHECK(name_of(p).read() == "renamed");
    const Row row = Row::edit("Name", name_of(p));
    CHECK(row.value() == "renamed");
    p.name = "again";
    CHECK(row.value() == "again"); // the row reads through it too
}

TEST_CASE("a successful commit writes through the semantic setter") {
    Probe p;
    Row row = Row::edit("Width", width_of(p));
    row.begin();
    CHECK(row.editing());
    CHECK(row.draft() == "60");

    // The draft alone changes nothing.
    type_all(row, "x");
    CHECK(p.width == 60);

    row.cancel();
    row.begin();
    clear_draft(row);
    type_all(row, "24");
    CHECK(p.width == 60); // still nothing

    CHECK(row.commit() == Commit::Accepted);
    CHECK(p.width == 24);
    CHECK_FALSE(row.editing());
    CHECK(row.refusal().empty());
    CHECK(row.value() == "24");
}

TEST_CASE("an unparseable draft leaves the property untouched and says so") {
    Probe p;
    Row row = Row::edit("Width", width_of(p));
    row.begin();
    clear_draft(row);
    type_all(row, "banana");

    CHECK(row.commit() == Commit::Unparseable);
    CHECK(p.width == 60);                     // the property never moved
    CHECK(row.editing());                     // still in the draft, so it can be fixed
    CHECK(row.draft() == "banana");           // and the draft was NOT thrown away
    CHECK_FALSE(row.refusal().empty());       // the refusal is observable
    // ...and it is shown AS a draft: the insertion point is the component's, and the caret is
    // where it says the next keystroke lands rather than always at the end.
    CHECK(row.display() == "banana");
    CHECK(row.editor().caret() == 6);         // the end, because that is where typing left it
    CHECK(row.editor().text() == "banana");
    CHECK(row.value() == "60");               // the committed value is still the real one
}

TEST_CASE("a parseable value the property refuses is a DIFFERENT outcome, with its reason") {
    Probe p;
    Row row = Row::edit("Width", width_of(p));
    row.begin();
    clear_draft(row);
    // `500` IS a whole number -- it parses. The setter is what says no, and a maker
    // needs to tell that from "not a number at all": one is fixed by retyping,
    // the other by wanting something else.
    type_all(row, "500");
    CHECK(row.commit() == Commit::Refused);
    CHECK(row.refusal() == "an extent is 1 to 100 cells");
    CHECK(p.width == 60);
    // The draft survives a REFUSAL too, not only an unparseable draft -- both
    // failures leave the maker looking at what they typed, so both are pinned.
    // (A mutation that cleared the draft here was green until this line.)
    CHECK(row.editing());
    CHECK(row.draft() == "500");
    CHECK(row.display() == "500");
    // AND THE COMPONENT SURVIVES WITH IT. A refused commit leaves the maker looking at what
    // they typed AND at where they were typing it, which is the half a caret adds: a draft
    // preserved with its insertion point thrown away would have to be re-navigated.
    CHECK(row.editor().caret() == 3);
    CHECK(row.editor().first_visible() == 0);
    row.left();
    row.left();
    CHECK(row.editor().caret() == 1); // still an editor, not a preserved string
    CHECK(row.value() == "60");

    // Zero is the other edge of the same check, through the same row.
    row.cancel();
    row.begin();
    clear_draft(row);
    type_all(row, "0");
    CHECK(row.commit() == Commit::Refused);
    CHECK(row.refusal() == "an extent is 1 to 100 cells");
    CHECK(p.width == 60);
}

TEST_CASE("an unparseable draft writes nothing even where a default WOULD be accepted") {
    // The sharp version of the previous case, and the one a mutation found
    // missing. On Width, a commit that wrongly wrote a default-constructed value
    // is INVISIBLE: the default whole number is 0, which the extent check refuses
    // anyway, so the setter masks the bug. X has no such luck -- 0 is a
    // perfectly legal position -- so this is where "an unparseable draft does
    // not write" is actually observable.
    Probe p;
    REQUIRE(p.x == 3);

    Row row = Row::edit("X", x_of(p));
    row.begin();
    clear_draft(row);
    type_all(row, "banana");

    CHECK(row.commit() == Commit::Unparseable);
    CHECK(p.x == 3); // not 0, and not anything else
    CHECK(row.draft() == "banana");
    CHECK(row.value() == "3");

    // And the whole-number form's own edges, on the same row.
    row.cancel();
    row.begin();
    clear_draft(row);
    type_all(row, "-1");
    CHECK(row.commit() == Commit::Refused); // parses; the setter refuses it
    CHECK(row.refusal() == "the room starts at 0");
    CHECK(p.x == 3);

    row.cancel();
    row.begin();
    clear_draft(row);
    type_all(row, "0");
    CHECK(row.commit() == Commit::Accepted); // 0 IS a legal position
    CHECK(p.x == 0);
}

TEST_CASE("cancel abandons the draft and never touched the property") {
    Probe p;
    Row row = Row::edit("Name", name_of(p));
    row.begin();
    type_all(row, "zzz");
    row.cancel();

    CHECK_FALSE(row.editing());
    CHECK(row.display() == "panel");
    CHECK(p.name == "panel");

    // ANY TEXT IS A STRING, so the text form never says "unparseable": an empty name is a
    // value of the type, and the setter is what refuses it.
    row.begin();
    clear_draft(row);
    CHECK(row.commit() == Commit::Refused);
    CHECK(row.refusal() == "a name cannot be empty");
    CHECK(p.name == "panel");
}

TEST_CASE("reuse: two properties of one type share every line of conversion") {
    Probe p;
    // Width and Height are both whole numbers under one check. Building rows for them is one
    // call each, and NEITHER call names a parse, a format, or a refusal wording -- that is
    // what TextForm<std::int64_t> and the shared check already are. The old builder needed a
    // whole row implementation per property; this pins that it no longer does.
    Row width = Row::edit("Width", width_of(p));
    Row height = Row::edit("Height", height_of(p));

    // The same typed draft behaviour on both...
    for (Row* row : {&width, &height}) {
        row->begin();
        clear_draft(*row);
        type_all(*row, "40");
        CHECK(row->commit() == Commit::Accepted);
    }
    CHECK(p.width == 40);
    CHECK(p.height == 40);

    // ...and the same refusal, in the same words, from the shared check.
    for (Row* row : {&width, &height}) {
        row->begin();
        clear_draft(*row);
        type_all(*row, "101");
        CHECK(row->commit() == Commit::Refused);
        CHECK(row->refusal() == "an extent is 1 to 100 cells");
    }
    CHECK(p.width == 40);
    CHECK(p.height == 40);
}

TEST_CASE("a commit of text the row never drafted meets the same conversion and the same refusals") {
    // THE SEAM'S DOOR (`Row::commit_text`): an inspector whose draft lives in another image sends
    // only its finished text, and the row judges it exactly as it judges its own draft -- the same
    // parse, the same setter, the same words -- opening no draft of its own either way.
    Probe p;
    Row row = Row::edit("Width", width_of(p));
    CHECK(row.commit_text("banana") == Commit::Unparseable);
    CHECK(row.refusal() == "not a whole number");
    CHECK(p.width == 60);
    CHECK(row.commit_text("500") == Commit::Refused);
    CHECK(row.refusal() == "an extent is 1 to 100 cells");
    CHECK(p.width == 60);
    CHECK(row.commit_text("24") == Commit::Accepted);
    CHECK(row.refusal().empty());
    CHECK(p.width == 24);
    CHECK_FALSE(row.editing());

    // A shown row has nothing to write to, whoever sent the text.
    Row shown = Row::show("Resolved", [] { return std::string("24 cells"); });
    CHECK(shown.commit_text("12") == Commit::Refused);
    CHECK(shown.refusal() == "not authored");
}

TEST_CASE("the whole-number text form: canonical out, and nothing but a whole number in") {
    using Form = TextForm<std::int64_t>;
    CHECK(Form::format(12) == "12");
    CHECK(Form::format(-3) == "-3");

    CHECK(Form::parse("12") == std::optional<std::int64_t>{12});
    CHECK(Form::parse("-3") == std::optional<std::int64_t>{-3}); // parses; a setter may refuse
    CHECK(Form::parse("9223372036854775807") ==
          std::optional<std::int64_t>{9223372036854775807LL});

    CHECK_FALSE(Form::parse("").has_value());
    CHECK_FALSE(Form::parse("-").has_value());
    CHECK_FALSE(Form::parse("banana").has_value());
    CHECK_FALSE(Form::parse("12x").has_value());
    CHECK_FALSE(Form::parse("70%").has_value());
    CHECK_FALSE(Form::parse("99999999999999999999").has_value()); // too big to be a number
    CHECK(std::string(Form::expected()) == "a whole number");
}

TEST_CASE("a shown row cannot be edited, because it has nothing to write to") {
    // The structural answer to "never present a derived value as though it were a
    // property": a `show` has no setter to reach, so begin() on one does nothing at all,
    // and a section heading is the same with no value either.
    Probe p;
    Row shown = Row::show("Resolved", [&p] { return std::to_string(p.width) + " cells"; });
    CHECK_FALSE(shown.editable());
    shown.begin();
    CHECK_FALSE(shown.editing());
    CHECK(shown.value() == "60 cells");
    p.width = 12;
    CHECK(shown.value() == "12 cells"); // it reads through like every row

    Row heading = Row::section("PLACE");
    CHECK_FALSE(heading.editable());
    CHECK(heading.section());
    heading.begin();
    CHECK_FALSE(heading.editing());

    CHECK(Row::edit("Width", width_of(p)).editable());
}

// ============================================================================
// Tier 2 — the weave, through a real bus
// ============================================================================
//
// This tier exists because `WorkshopWeave` lives in workshop/weave.hpp rather
// than in the host's anonymous namespace, so the chain from a published message to
// what the host does can be walked end to end with no test hook, no framework, and
// no seam that exists only for this file. (It walked the pointer and the keys to the
// object document's operations until that document retired: move, resize, the size
// handle, both media's presses. What is left is the host's own.)

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

TEST_CASE("the status line names the live layout and its panes, and claims no file") {
    // ⭐ IT COUNTED OBJECTS AND COMPARED THE OBJECT DOCUMENT WITH ITS FILE (`saved` / `UNSAVED`)
    // until that document retired. What a medium's own status line shows beside the room now is
    // which desk is live and how many panes it names; whether that desk is saved is the layout
    // band's to say, so this line claims nothing about any file.
    Live t;
    t.key(input::scan::kTab); // any turn: a status note goes out with every frame
    REQUIRE_FALSE(t.notes.empty());
    const std::size_t named = t.session().setup.active.panes.size();
    CHECK(t.status_note() == "[workshop] layout " +
                                 quoted_setup_name(t.session().setup.active.name) + " | " +
                                 std::to_string(named) + (named == 1 ? " pane" : " panes"));
    CHECK(t.status_note().find("saved") == std::string::npos);
    CHECK(t.status_note().find("UNSAVED") == std::string::npos);

    // ...AND IT FOLLOWS THE DESK: a pane taken off it is a pane fewer on the line.
    pick(t, panel::kLayouts);
    const std::size_t now = t.session().setup.active.panes.size();
    REQUIRE(now + 1 == named);
    CHECK(t.status_note() == "[workshop] layout " +
                                 quoted_setup_name(t.session().setup.active.name) + " | " +
                                 std::to_string(now) + (now == 1 ? " pane" : " panes"));
}

TEST_CASE("a notice a maker's own path makes too long is marked on screen, not cut in the session") {
    // The overlong notice through the real message path, on one Workshop produces
    // honestly. A setup path is the maker's own input and may be any length the
    // platform allows, so a refusal naming it is a sentence this tool can be
    // asked to say and cannot show. Nothing is forged and nothing is distorted
    // to produce it: one ordinary keystroke, on a path that is simply not there.
    // (The keystroke was the object document's `^o`, until that document retired.)
    TempDir dir("long-notice");
    Live t;
    t.host.setup_path = dir.file(
        "a-workshop-setup-with-a-name-its-maker-chose-and-this-terminal-cannot-show-all-of.json");

    t.key(input::scan::kR);

    // The refusal is whole in the session, and it is genuinely longer than a
    // line -- the path alone overruns the screen, whatever the platform's own
    // wording for a missing file happens to be.
    REQUIRE(t.notice().size() > static_cast<std::size_t>(kMinScreen.w));
    CHECK(t.notice().find(t.host.setup_path) != std::string::npos);

    const std::string shown = label_at(t.canvases.back(), 0, kMinScreen.notice_y);
    CHECK(shown.size() == static_cast<std::size_t>(kMinScreen.w));
    CHECK(shown.compare(shown.size() - 3, 3, "...") == 0);
    CHECK(t.notice().compare(0, shown.size() - 3, shown, 0, shown.size() - 3) == 0);

    // And the refusal cost the maker nothing but the notice: no file became the desk's.
    CHECK(t.session().notice_is_bad);
    CHECK(t.session().setup.active_link.path.empty());
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

// ⭐ THE INFO PANEL'S OWN CASES LEFT THIS SUITE WITH THE PANEL, AND THEY ARE NAMED RATHER THAN
// QUIETLY DROPPED. Thirty-one cases stood here about a presentation this host no longer makes:
// HD-5's property draft (its window, its caret, its press inverse, its commit and cancel, both
// media), HD-9's grounds under the controls and the `PROPERTIES` heading, QR-2's consumed-press
// chain inside the body, TEXT-0's and QR-11's draft vocabulary and clipboard, and CTX-0's
// live-draft hold on a contextual deletion. Every one of them measured `paint_info`,
// `info_body_place` or `info_body_at`, and all three are `Zengine/info-pane/pane.cpp`'s now.
//
// WHERE EACH CLAIM LIVES NOW. The pane's own composition, its draft and its presses are the
// pane's, in `tests/test_workshop_panes_info.cpp`, driven through the real loaded image and the
// pane protocol. The editable line's own laws are `component::TextBox`'s and are pinned by
// `tests/test_component.cpp`, which is where they always were. What this suite kept then was the
// DOCUMENT -- the operations, the identities, the canvas, the selection and the seam the host
// published -- and that retired with the object canvas (see the head of this file).
//
// ⚠ AND ONE CLAIM HAS NO HOME AND IS A NAMED LOSS: `"CTX-0: a live draft holds a contextual
// deletion back"`. The rule rested on this host being able to see a live inspector draft, and
// it cannot: a maker typing a Width while deleting that object from the context menu loses the
// typing. Reported in RB3 as stage 2's debt and paid here in the only currency there was.

TEST_CASE("TEXT-0: the name editor selects with the same keys and says it in characters") {
    TempDir dir("text0-naming");
    Live t;
    (void)t.mount_skin_seat(); // the cross-consumer paste below asks it, like every paste
    t.host.setup_path = dir.file("setup.json");
    // ⚠ THE COPY USED TO HAPPEN IN THE TERMINAL OVERLAY (VD-24), which was a box of this
    // host's. The Terminal is a weave now, so the cross-consumer half of this claim is said
    // the way it is actually true: a PANE copies -- publishing `ClipboardCopy`, which is what
    // every migrated pane does with its own selection -- and this host's name editor pastes
    // it. That is a wider claim than the one it replaces, because the two boxes are now in
    // two images.
    t.publish(loom::to_value(surface::ClipboardCopy{"Morning"}));

    // THE EDITOR IS OPENED BY DOUBLE-CLICKING THE TAB SINCE WUX-11: `s` saves now, and
    // renaming is the layout operation reached from the tab a maker points at.
    open_rename_on_tab(t, t.session().setup.active_at);
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
    CHECK(editor.rows[0].text.find("layout name> Default") == 0);
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
    CHECK(label_at(t.canvases.back(), 0, 0).find("layout name> Default") == 0);

    // Paste replaces the selection: the name a maker copied in a PANE arrives here.
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

TEST_CASE("TEXT-0: ^c still quits exactly where nothing takes text") {
    // Command mode: the rewritten MSG-0 case covers a focused pane and the overlay; this
    // one pins the three keyboard owners that take no text -- the contextual surface, pane
    // management, and plain command mode -- so the narrowing cannot creep. (The first was the
    // `p` picker until it retired.)
    {
        Live t;
        t.key(input::scan::kA); // the contextual surface is open and owns the keyboard
        t.text("a");
        REQUIRE(t.menu().open);
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
    // ⚠ THE BOX IS THE LAYOUT NAME EDITOR NOW (VD-24). It used to be the terminal
    // overlay's line, which was a box of this host's; the Terminal is a weave and its line
    // is its own, so the box this host still owns is the one that stands in for every box
    // this claim is about.
    Live t;
    open_rename_on_tab(t, t.session().setup.active_at);
    REQUIRE(t.session().setup.naming.open);
    (void)t.bus.send(t.workshop_id,
                     loom::Message(loom::to_value(surface::ClipboardText{true, "EVIL"}),
                                   loom::WeaveId{}, loom::WeaveId{}, 1));
    t.bus.drain_until_idle();
    CHECK(t.session().setup.naming.line.text() == "Default");
    CHECK(t.session().clipboard.text.empty());

    // THE SHARPER HALF: an ask genuinely OUTSTANDING (nobody holds the skin role, so the
    // conversation stays open), and a rogue with a real bus-stamped identity guessing the
    // correlation an asker's fresh book mints first -- 1, the settlement law's own example
    // of why a correlation identifies and never authenticates. The book alone would be
    // satisfied; `answers_ask()` is the wall that is not, because Loom stamps the one
    // authorized answer and no send can wear the stamp.
    t.key(input::scan::kA, input::mod::kCtrl);
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
    CHECK(t.session().setup.naming.line.text() == "abc"); // the payload reached no box
    CHECK(t.session().clipboard.text.empty());
}

TEST_CASE("QR-11: paste reads the platform current, not the mirror stale") {
    // SC-2's own sentence: on a readable medium the value used for a paste is the actual
    // platform value current for THAT paste -- never a mirror populated earlier. The
    // platform moves silently between two pastes (no event; nothing here may watch), and
    // each paste gets its own moment's truth.
    Live t;
    SkinSeat* skin = t.mount_skin_seat();
    open_rename_on_tab(t, t.session().setup.active_at); // the host's own box (VD-24)
    REQUIRE(t.session().setup.naming.open);
    t.key(input::scan::kA, input::mod::kCtrl);
    for (const char c : std::string("stale")) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kC, input::mod::kCtrl); // mirror = "stale", platform = "stale"
    REQUIRE(t.session().clipboard.text == "stale");
    skin->platform = "fresh"; // an unrelated application copies; nothing travels
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.session().setup.naming.line.text() == "fresh");
    skin->platform = "newer"; // ...and again, between two pastes
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.session().setup.naming.line.text() == "newer");
    CHECK(skin->clipboard_reads == 2);
}

TEST_CASE("QR-11: with nobody at the skin role, paste inserts nothing and breaks nothing") {
    // The honest degradation: the ask has no respondent (the send is refused as a tap
    // event; no message returns; Loom has no unanswerability notice), so the paste simply
    // does not happen -- consumed, so it cannot fall through to an application binding --
    // and pressing it past the book's capacity stays quiet rather than becoming a crash
    // or a queue.
    Live t;
    open_rename_on_tab(t, t.session().setup.active_at); // the host's own box (VD-24)
    REQUIRE(t.session().setup.naming.open);
    t.key(input::scan::kA, input::mod::kCtrl);
    for (const char c : std::string("abc")) {
        t.text(std::string(1, c));
    }
    for (int i = 0; i < 6; ++i) { // past the book's capacity of 4
        t.key(input::scan::kV, input::mod::kCtrl);
    }
    CHECK(t.session().setup.naming.line.text() == "abc");
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
    // Ctrl+Shift+C is not Ctrl+C: the old test asked only whether Ctrl was among the bits, so
    // the widened chord quit too. Under exact matching it reaches nothing.
    t.key(input::scan::kC, input::mod::kCtrl | input::mod::kShift);
    CHECK_FALSE(t.host.quit);
    // A chord is a different gesture from its bare key: `=` adds a layout, `^=` does not...
    const std::size_t before = layout_count(t.session().setup);
    t.key(input::scan::kEquals, input::mod::kCtrl);
    CHECK(layout_count(t.session().setup) == before);
    // ...and the bare key still does: exact matching narrowed, it did not move.
    t.key(input::scan::kEquals);
    CHECK(layout_count(t.session().setup) == before + 1);
    // Alt+Q used to quit.
    t.key(input::scan::kQ, input::mod::kAlt);
    CHECK_FALSE(t.host.quit);
    // (The witness was `^s`/`^n` on the object document until it retired.)
}

TEST_CASE("KEY-0: the effective keymap lists every place a key is answered, and marks the text "
          "box's keys as nobody's to move") {
    Live t;
    const std::string keys = keymap_text(t.session());
    // THE HOST'S OWN ROWS, each under the place it is answered, spelled as a file would.
    CHECK(keys.find("command mode | = | new layout | layout.new") != std::string::npos);
    CHECK(keys.find("above every mode, unless text has the keys | ctrl+c | quit | workshop.quit") !=
          std::string::npos);
    CHECK(keys.find("naming a layout | return | rename | naming.commit") != std::string::npos);
    // AN ACTION WITH TWO ROWS IN ONE PLACE IS LISTED ONCE: one override moves both.
    std::size_t quits = 0;
    for (std::size_t at = keys.find("| quit | workshop.quit"); at != std::string::npos;
         at = keys.find("| quit | workshop.quit", at + 1)) {
        ++quits;
    }
    CHECK(quits == 2); // the no-text chord and command mode's `q`, two places
    // THE TEXT BOX'S OWN VOCABULARY, listed for discovery and marked.
    CHECK(keys.find("the text box's own keys | ctrl+c | copy |  (not remappable)") !=
          std::string::npos);
    // AND NO APPLICATION ROW WITHOUT AN APPLICATION: a host whose desktop never declared lists none.
    CHECK(keys.find("the application's") == std::string::npos);
    DesktopSeat* desk = mount_desktop(t);
    (void)desk;
    CHECK(keymap_text(t.session()).find(
              "above every mode -- the application's | ctrl+t | terminal | desktop.terminal") !=
          std::string::npos);
}

TEST_CASE("KEY-0: an authored override changes dispatch AND every displayed spelling") {
    TempDir dir("keymap-override");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"layout.new", "g"}}));
    Keyed t(path);
    // The load was announced first, in words, with the override counted -- read it
    // before any gesture writes its own sentence over the one notice line.
    CHECK(t.notice().find("1 override") != std::string::npos);

    // DISPATCH: `g` adds a layout and `=` no longer does -- the override moved the
    // binding, not the action. (`g`, because it is a free letter in command mode.)
    const std::size_t before = layout_count(t.session().setup);
    t.key(input::scan::kEquals);
    CHECK(layout_count(t.session().setup) == before);
    t.key(input::scan::kG);
    CHECK(layout_count(t.session().setup) == before + 1);

    // DISPLAY: the band's help row and the effective keymap both spell the same
    // effective binding, because both project the same value dispatch read.
    // (The legend packs the command rows in catalog order and wraps; this pair is its second
    // row's on the minimum screen.)
    const Screen sc = screen_of(t.session());
    const std::string legend = label_at(t.canvases.back(), 0, sc.help_y) + " | " +
                               label_at(t.canvases.back(), 0, sc.help_y + 1);
    CHECK(legend.find("g new layout") != std::string::npos);
    CHECK(legend.find("= new layout") == std::string::npos);
    CHECK(keymap_text(t.session()).find("command mode | g | new layout | layout.new *") !=
          std::string::npos);
}

TEST_CASE("KEY-0: an override survives restart, and deleting the file restores defaults") {
    TempDir dir("keymap-restart");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"layout.new", "g"}}));
    {
        Keyed first(path);
        const std::size_t before = layout_count(first.session().setup);
        first.key(input::scan::kG);
        REQUIRE(layout_count(first.session().setup) == before + 1);
    }
    // RESTART: a new process reads the same authored file and reaches the same
    // effective truth.
    {
        Keyed again(path);
        const std::size_t before = layout_count(again.session().setup);
        again.key(input::scan::kG);
        CHECK(layout_count(again.session().setup) == before + 1);
        again.key(input::scan::kEquals);
        CHECK(layout_count(again.session().setup) == before + 1);
    }
    // RESET: deleting the file IS returning to the defaults -- nothing else to
    // clear, nothing rewritten.
    std::error_code drop;
    std::filesystem::remove(path, drop);
    Keyed defaults(path);
    CHECK(defaults.notice().empty()); // an absent file is not a complaint
    const std::size_t before = layout_count(defaults.session().setup);
    defaults.key(input::scan::kEquals);
    CHECK(layout_count(defaults.session().setup) == before + 1);
    defaults.key(input::scan::kG);
    CHECK(layout_count(defaults.session().setup) == before + 1);
}

TEST_CASE("KEY-0: a same-context collision is refused naming both actions and the gesture") {
    // `s` is setup.name's default; authoring layout.new onto it would put two
    // actions on one gesture in one context, which is a lockout and must not be
    // savable -- the whole candidate is refused and the defaults stand.
    const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(
        keymap_file_text("default", {{"layout.new", "s"}}));
    CHECK_FALSE(loaded.outcome.accepted);
    CHECK(loaded.outcome.refusal.find("layout.new") != std::string::npos);
    CHECK(loaded.outcome.refusal.find("setup.name") != std::string::npos);
    CHECK(loaded.outcome.refusal.find("`s`") != std::string::npos);

    // ...and the refused file leaves a live Workshop on its defaults, out loud.
    TempDir dir("keymap-collide");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"layout.new", "s"}}));
    Keyed t(path);
    // ...OUT LOUD, AND AS A CONDITION. The file is still refused an hour
    // later and at the next launch, so it is not a sentence about a moment: it stands
    // under its own key, in the loader's own words, and it disappears from attention only
    // if it stops being true.
    const std::vector<Condition> now = attention_conditions(t.session());
    const Condition* wall = condition_by_key(now, kKeymapWallKey);
    REQUIRE(wall != nullptr);
    CHECK(wall->detail.find("setup.name") != std::string::npos);
    CHECK(wall->compact.find("default bindings stand") != std::string::npos);
    CHECK(wall->role == surface::role::kAlert);
    const std::size_t before = layout_count(t.session().setup);
    t.key(input::scan::kEquals);
    CHECK(layout_count(t.session().setup) == before + 1); // the default still adds one
}

TEST_CASE("KEY-0: reusing one gesture across mutually exclusive contexts is legal") {
    // `t` toggles pane titles in command mode; the contextual menu cannot be open while
    // command mode resolves a key, so authoring context.up onto `t` collides with nothing --
    // the defaults already live this way (`w` arranges the desk and is also an arrangement
    // scope's own key). (The witness was `h`, the object canvas's move, until it retired.)
    const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(
        keymap_file_text("default", {{"context.up", "t"}}));
    REQUIRE(loaded.outcome.accepted);

    TempDir dir("keymap-reuse");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"context.up", "t"}}));
    Keyed t(path);
    const bool titles = t.session().pane_titles;
    t.key(input::scan::kT);
    CHECK(t.session().pane_titles != titles); // command context: `t` still toggles titles
    t.key(input::scan::kA);
    t.text("a");
    REQUIRE(t.menu().open);
    t.key(input::scan::kDown);
    REQUIRE(t.menu().cursor == 1);
    t.key(input::scan::kT); // menu context: the authored `t` steps up
    CHECK(t.menu().cursor == 0);
    CHECK(t.session().pane_titles != titles); // ...and toggled nothing
}

TEST_CASE("KEY-0: an override for an unknown action survives with its intent whole") {
    // The setup law's ACCEPTED clause, applied to the sixth file: a well-formed
    // row this build cannot resolve is not an error and must never become one.
    const std::string text = keymap_file_text(
        "default", {{"layout.new", "g"}, {"future.action", "hyper+z"}});
    const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(text);
    REQUIRE(loaded.outcome.accepted);
    // The known row was applied...
    REQUIRE(loaded.keymap.overrides.size() == 1);
    CHECK(loaded.keymap.overrides[0].first == Act::kLayoutNew);
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
        keymap_file_text("default", {{"layout.new", "f13"}}));
    CHECK_FALSE(bad_key.outcome.accepted);
    CHECK(bad_key.outcome.refusal.find("`f13` is not a key") != std::string::npos);

    const keymap_persist::LoadedKeymap bad_mod = keymap_persist::from_text(
        keymap_file_text("default", {{"layout.new", "meta+n"}}));
    CHECK_FALSE(bad_mod.outcome.accepted);
    CHECK(bad_mod.outcome.refusal.find("`meta` is not a modifier") != std::string::npos);

    const keymap_persist::LoadedKeymap twice = keymap_persist::from_text(
        keymap_file_text("default", {{"layout.new", "g"}, {"layout.new", "i"}}));
    CHECK_FALSE(twice.outcome.accepted);
    CHECK(twice.outcome.refusal.find("authored twice") != std::string::npos);

    const keymap_persist::LoadedKeymap legend = keymap_persist::from_text(
        keymap_file_text("sometimes", {}));
    CHECK_FALSE(legend.outcome.accepted);
    CHECK(legend.outcome.refusal.find("`sometimes` is not a legend mode") !=
          std::string::npos);
}

TEST_CASE("KEY-0: a retired id in a maker's file is kept and said, and nothing answers it") {
    // ⭐ THE OBJECT CANVAS'S KEYS AND THE DOCUMENT'S TWO GLOBALS RETIRED (`kRetiredActions`), and
    // a maker's file may still name them. Such a row is a well-formed row this build cannot
    // resolve -- the accepted clause holds for it exactly as for a future id -- and the old
    // global's bare printable is not judged, because no row of this build answers it.
    const std::string text =
        keymap_file_text("default", {{"object.new", "g"}, {"document.save", "t"}});
    const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(text);
    REQUIRE(loaded.outcome.accepted);
    CHECK(loaded.keymap.overrides.empty());
    REQUIRE(loaded.keymap.authored.size() == 2);
    CHECK(keymap_persist::to_text(loaded.keymap) == text); // nothing tidied, nothing dropped

    // SAID AT THE LOAD, each by what took it...
    TempDir dir("keymap-retired");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, text);
    Keyed t(path);
    CHECK(t.notice().find("`object.new` retired with the object canvas -- kept, and nothing "
                          "answers it") != std::string::npos);
    CHECK(t.notice().find("`document.save` retired with the object document") !=
          std::string::npos);
    // ...AND ANSWERED BY NOTHING: `g` is nobody's, and `t` is still the titles toggle it was.
    const std::size_t layouts = layout_count(t.session().setup);
    const bool titles = t.session().pane_titles;
    t.key(input::scan::kG);
    CHECK(layout_count(t.session().setup) == layouts);
    t.key(input::scan::kT);
    CHECK(t.session().pane_titles != titles);
    CHECK(retired_with("object.new") != nullptr);
    CHECK(retired_with("layout.new") == nullptr);

    // ⚠ AND THE HOST DECLARES NO ROW ABOVE EVERY MODE BUT QUIT'S. The two walls a global row
    // meets at the file -- no bare printable, no chord the text box owns -- guarded the
    // document's `^s`/`^o`; the rows answered above every mode now are the application's, and
    // they meet the same walls when the desktop declares them (`join_app_rows`, witnessed in
    // `tests/test_workshop_panes_actions.cpp`). A host row declared global again must bring
    // its own witness for the file's walls, and this line is where it will find out.
    for (const ActionRow& row : kActionCatalog) {
        CAPTURE(row.id);
        CHECK(row.context != KeyContext::kGlobal);
        CHECK(row.context != KeyContext::kUnlessOwned);
    }
}

TEST_CASE("KEY-0: the picker's and the host Pane Manager's ids are kept, said with where the act "
          "went, and answered by nothing") {
    // ⭐ THE PICKER AND THE HOST'S PANE MANAGER RETIRED WITH THEIR ROWS, and a maker's file may
    // still name them. Each is kept byte for byte and said at the load WITH WHERE ITS ACT WENT, so
    // a maker who moved one is told where to move it next rather than left with a silent key.
    const std::string text = keymap_file_text(
        "default", {{"workshop.picker", "g"}, {"pane-editor.front", "y"}, {"draft.commit", "j"}});
    const keymap_persist::LoadedKeymap loaded = keymap_persist::from_text(text);
    REQUIRE(loaded.outcome.accepted);
    CHECK(loaded.keymap.overrides.empty());
    CHECK(keymap_persist::to_text(loaded.keymap) == text); // nothing tidied, nothing dropped

    TempDir dir("keymap-retired-manager");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, text);
    Keyed t(path);
    CHECK(t.notice().find("`workshop.picker` retired with the `p` picker -- kept, and nothing "
                          "answers it (" + std::string(kToPaneManager) + ")") !=
          std::string::npos);
    CHECK(t.notice().find("`pane-editor.front` retired with the host's Pane Manager -- kept, and "
                          "nothing answers it (" + std::string(kToArranging) + ")") !=
          std::string::npos);
    CHECK(t.notice().find("`draft.commit` retired with the host's Pane Manager -- kept, and "
                          "nothing answers it (" + std::string(kToInfoRows) + ")") !=
          std::string::npos);
    // ...AND ANSWERED BY NOTHING: `g` opens no list, and neither does `p`, which the picker held.
    const std::string before = t.notice();
    for (const std::int64_t key : {input::scan::kG, input::scan::kP}) {
        t.key(key);
        CHECK_FALSE(t.menu().open);
        CHECK(t.session().panels.open.size() == 1); // the shipped desk's Layouts, and nothing new
    }
    CHECK(t.notice() == before);
    // THE PANE MANAGER'S KEY IS THE DESKTOP'S TO DECLARE, so this host names no row for it.
    CHECK(row_of_id("desktop.panes") == nullptr);
}

TEST_CASE("KEY-0: a known backend gap is accepted and said, never silently rewritten") {
    TempDir dir("keymap-gap");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path,
    // ⚠ THE ROW IS THE CONTEXTUAL SURFACE'S NOW. It was `workshop.terminal`, the global chord
    // that opened the terminal overlay, and then the picker's `p`; both retired, so the claim
    // -- an authored gesture a backend cannot produce is ACCEPTED, said, and not rewritten --
    // is made over another row a maker can author.
                      keymap_file_text("default", {{"workshop.context", "shift+space"}}));
    Keyed t(path);
    // The note said the honest half out loud at load: a POSIX terminal cannot produce it.
    // Nothing in the file was rewritten. It is read HERE, before any gesture, because the
    // notice line has one occupant and the next act writes its own sentence over this one.
    CHECK(t.notice().find("shift is not observable") != std::string::npos);
    // Accepted: the authored gesture works where the wire can carry it...
    t.key(input::scan::kSpace, input::mod::kShift);
    CHECK(t.menu().open);
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.menu().open);
    // The default it replaced no longer fires -- an override moves a binding,
    // it does not leave the old one behind as an invisible alias.
    t.key(input::scan::kA);
    t.text("a");
    CHECK_FALSE(t.menu().open);
}

TEST_CASE("a ctrl+shift+letter binding is accepted, and its collapse on the POSIX wire is said") {
    // THE GAP THE BACKLOG NAMED (its entry BL-DEF-03). The POSIX terminal sends ctrl+letter
    // as one control byte and Shift leaves no mark on it, so `ctrl+shift+g` and `ctrl+g` are
    // one byte there -- a fact the keymap's prose stated in five places while `posix_gap`
    // said nothing. Over the value first:
    CHECK(posix_gap(Gesture{input::scan::kG, input::mod::kCtrl | input::mod::kShift}) != nullptr);
    CHECK(std::string(posix_gap(Gesture{input::scan::kG, input::mod::kCtrl | input::mod::kShift}))
              .find("collapses to plain ctrl+letter") != std::string::npos);
    CHECK(posix_gap(Gesture{input::scan::kG, input::mod::kCtrl}) == nullptr);
    CHECK(posix_gap(Gesture{input::scan::kG, input::mod::kShift}) == nullptr); // a capital
    // AND ON A DIGIT THE OLDER RULE STILL ANSWERS: shift on a non-letter is not observable.
    CHECK(std::string(posix_gap(Gesture{input::scan::k1, input::mod::kCtrl | input::mod::kShift}))
              .find("shift is not observable") != std::string::npos);

    // Then through the file, `shift+space`'s own road: accepted, working where the wire can
    // carry it, and the gap said once at the load. Nothing in the file is rewritten.
    TempDir dir("keymap-ctrl-shift-gap");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"layout.new", "ctrl+shift+g"}}));
    Keyed t(path);
    CHECK(t.notice().find("collapses to plain ctrl+letter") != std::string::npos);
    const std::size_t before = layout_count(t.session().setup);
    t.key(input::scan::kG, input::mod::kCtrl | input::mod::kShift);
    CHECK(layout_count(t.session().setup) == before + 1);
    t.key(input::scan::kEquals);
    CHECK(layout_count(t.session().setup) == before + 1); // the default it replaced no longer fires
}

TEST_CASE("WUX-11: an action with no default gesture answers to no key, and says so") {
    // ⭐ THE HAZARD THE GUARD EXISTS FOR. `input::scan::kUnknown` is what the wire reports
    // for a key this build has no name for, so it is the one scancode that can never be a
    // binding -- and a row declaring `kNoGesture` wears exactly that value. Without the
    // guard, ONE unnamed key would match every unbound row at once and the first in
    // declaration order would run: a press with no name performing an operation.
    Keymap k;
    for (const KeyContext ctx : {KeyContext::kCommand, KeyContext::kGlobal, KeyContext::kNoText,
                                 KeyContext::kNaming, KeyContext::kContext}) {
        CHECK(k.action_for(ctx, input::scan::kUnknown, input::mod::kNone) == Act::kNone);
        CHECK(k.above_mode_action(ctx, input::scan::kUnknown, input::mod::kNone) == Act::kNone);
    }
    CHECK_FALSE(k.matches(Act::kLayoutRename, input::scan::kUnknown, input::mod::kNone));
    CHECK_FALSE(k.matches(Act::kLayoutDuplicate, input::scan::kUnknown, input::mod::kNone));

    // ...AND THE FOUR ROWS THAT DECLARE ONE REALLY DO SHIP UNBOUND, which is what makes
    // this case about a live shape rather than about a hypothetical.
    for (const Act unbound : {Act::kLayoutRename, Act::kLayoutDuplicate, Act::kLayoutMoveLeft,
                              Act::kLayoutMoveRight}) {
        CHECK_FALSE(is_bound(k.gesture_of(unbound)));
        // A SURFACE THAT SPELLS BINDINGS MUST NOT SPELL ONE THAT DOES NOT EXIST.
        CHECK(gesture_text(k.gesture_of(unbound)) == "unbound");
    }
    // The band's legend carries no pair for them: its scarcest resource is columns, and a
    // pair whose gesture half is a non-key spends them saying nothing.
    for (const std::string& pair : help_pairs(k, KeyContext::kCommand)) {
        CAPTURE(pair);
        CHECK(pair.find("unbound") == std::string::npos);
        CHECK(pair.find("rename layout") == std::string::npos);
    }

    // ⚠ AND A PRESS OF THAT KEY THROUGH THE REAL WEAVE DOES NOTHING AT ALL.
    Live t;
    const std::size_t layouts = layout_count(t.session().setup);
    const Setup desk = t.session().setup.active;
    t.key(input::scan::kUnknown);
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(layout_count(t.session().setup) == layouts);
    CHECK(t.session().setup.active == desk);

    // ...WHILE A MAKER WHO BINDS ONE GETS IT, because unbound is a default and not a
    // refusal. Two of them may be authored at once, which the collision check must not
    // read as one gesture held twice.
    Keymap bound;
    const Written applied = apply_overrides(
        {{"layout.duplicate", "g"}, {"layout.move-left", "y"}}, legend_mode::kDefault, bound);
    REQUIRE_MESSAGE(applied.accepted, applied.refusal);
    CHECK(bound.action_for(KeyContext::kCommand, input::scan::kG, input::mod::kNone) ==
          Act::kLayoutDuplicate);
    CHECK(bound.action_for(KeyContext::kCommand, input::scan::kY, input::mod::kNone) ==
          Act::kLayoutMoveLeft);
    // ...and the ones still unbound are still unreachable by an unnamed key.
    CHECK(bound.action_for(KeyContext::kCommand, input::scan::kUnknown, input::mod::kNone) ==
          Act::kNone);
}

TEST_CASE("KEY-0: a printable trigger's own character is swallowed, wherever it is authored") {
    TempDir dir("keymap-swallow");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"layout.rename", "g"}}));
    Keyed t(path);
    t.host.setup_path = dir.file("setup.json");

    t.key(input::scan::kG);
    t.text("g");
    REQUIRE(t.session().setup.naming.open);
    CHECK(t.session().setup.naming.line.text() == "Default");
    // The swallow belongs to one moment: the next real character is taken.
    t.text("g");
    CHECK(t.session().setup.naming.line.text() == "Defaultg");
    // ⭐ AND THE ACTION THIS BINDS SHIPS WITH NO GESTURE AT ALL (WUX-11), which is the
    // second half of what this case now proves: `layout.rename` is reachable from a tab's
    // menu and from a maker's own keymap, and the two roads are the same action. `s` is
    // still `setup.name`'s -- it saves, and opens no editor.
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
    write_keymap_file(path, keymap_file_text("default", {{"layout.rename", "g"}}));
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
    write_keymap_file(path, keymap_file_text("default", {{"layout.rename", "shift+s"}}));
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
    // THE COMPACT LEGEND IS THE APPLICATION'S ROWS ABOVE EVERY MODE, as they are in force --
    // the launches and the key list, read off the keymap and named by no one here.
    DesktopSeat* launches = mount_desktop(compact);
    (void)launches;
    compact.key(input::scan::kTab); // any gesture: the band is repainted
    const Screen sc = screen_of(compact.session());
    // The legend rows are rows of the band's one region since WUX-1, so they are read
    // through the cell projection with the region's padding trimmed.
    CHECK(inspector_row(compact.canvases.back(), 0, sc.help_y) ==
          "^t terminal | ^p panes | ^k hotkeys");
    CHECK(inspector_row(compact.canvases.back(), 0, sc.help_y + 1).empty());

    write_keymap_file(path, keymap_file_text("hidden", {}));
    Keyed hidden(path);
    // Blank rows -- and ONLY blank rows: the band's geometry is `screen_of`'s
    // and the notice and setup line are untouched.
    CHECK(inspector_row(hidden.canvases.back(), 0, sc.help_y).empty());
    CHECK(inspector_row(hidden.canvases.back(), 0, sc.help_y + 1).empty());
    // Hidden never makes the full list unreachable: the binding is dispatch's,
    // and the legend is read by nothing but the band's painter.
    DesktopSeat* desk = mount_desktop(hidden);
    hidden.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE_FALSE(desk->asked().empty());
    CHECK(desk->asked().back() == DesktopSeat::kHotkeysId);

    write_keymap_file(path, keymap_file_text("full", {}));
    Keyed full(path);
    CHECK(label_at(full.canvases.back(), 0, sc.help_y).rfind("q quit", 0) == 0);
}

TEST_CASE("a written gesture is modifier words in one order out, any order in, and never twice") {
    // THE LAW (WL-KEY-14) AT THE TWO SEAMS the KEY-0 cases above do not pin: the ORDER the
    // writer spells and the parser tolerates, and the DUPLICATE the parser refuses. Pure
    // values, asked of the writer and the parser directly.
    const std::int64_t four =
        input::mod::kCtrl | input::mod::kShift | input::mod::kAlt | input::mod::kSuper;
    const Gesture all{input::scan::kZ, four};

    // THE WRITER SPELLS ONE ORDER -- ctrl, shift, alt, super -- whatever order the bits
    // were set in, and a punctuation key by its own character.
    CHECK(gesture_word(all) == "ctrl+shift+alt+super+z");
    CHECK(gesture_word(Gesture{input::scan::kZ, input::mod::kSuper | input::mod::kCtrl}) ==
          "ctrl+super+z");
    CHECK(gesture_word(Gesture{input::scan::kA, input::mod::kAlt | input::mod::kShift}) ==
          "shift+alt+a");
    CHECK(gesture_word(Gesture{input::scan::kLeftBracket, input::mod::kNone}) == "[");
    CHECK(gesture_word(Gesture{input::scan::kMinus, input::mod::kCtrl}) == "ctrl+-");

    // THE PARSER TAKES ANY ORDER: all twenty-four spellings of the four words mean the one
    // gesture, and the writer puts each of them back into the one order.
    const char* words[4] = {"ctrl", "shift", "alt", "super"};
    int order[4] = {0, 1, 2, 3};
    std::size_t spellings = 0;
    do {
        std::string text;
        for (const int i : order) {
            text += words[i];
            text += '+';
        }
        text += 'z';
        CAPTURE(text);
        const ParsedGesture parsed = parse_gesture(text);
        REQUIRE_MESSAGE(parsed.accepted, parsed.refusal);
        CHECK(parsed.gesture == all);
        CHECK(gesture_word(parsed.gesture) == "ctrl+shift+alt+super+z");
        ++spellings;
    } while (std::next_permutation(order, order + 4));
    CHECK(spellings == 24);
    CHECK(parse_gesture("shift+ctrl+a").gesture == parse_gesture("ctrl+shift+a").gesture);

    // A DUPLICATE IS REFUSED, BY THE NAME THAT REPEATED -- however far apart the two are and
    // whatever the key -- and a refusal is a refusal: no gesture comes with it.
    struct Twice {
        const char* text;
        const char* word;
    };
    for (const Twice t : {Twice{"ctrl+ctrl+z", "ctrl"}, Twice{"shift+ctrl+shift+a", "shift"},
                          Twice{"alt+super+alt+home", "alt"}, Twice{"super+super+[", "super"}}) {
        CAPTURE(t.text);
        const ParsedGesture parsed = parse_gesture(t.text);
        CHECK_FALSE(parsed.accepted);
        CHECK(parsed.refusal.find("appears twice") != std::string::npos);
        CHECK(parsed.refusal.find(std::string("`") + t.word + "`") != std::string::npos);
        CHECK(parsed.gesture == Gesture{});
    }

    // AN UNKNOWN MODIFIER OR KEY IS REFUSED BY NAME, NEVER GUESSED: `meta` is not `alt`,
    // `f13` is not a key, and a modifier standing where the key should be is a key name
    // this keymap does not have.
    CHECK(parse_gesture("meta+z").refusal.find("`meta` is not a modifier") != std::string::npos);
    CHECK(parse_gesture("ctrl+f13").refusal.find("`f13` is not a key") != std::string::npos);
    CHECK(parse_gesture("ctrl+shift").refusal.find("`shift` is not a key") != std::string::npos);
    CHECK_FALSE(parse_gesture("").accepted);

    // THE ROUND TRIP OVER THE WHOLE NAMED SCAN SET, under every modifier set: what the
    // writer spells, the parser reads back as the same gesture, and each name means its one
    // scancode. The named set is the grammar; nothing outside it has a spelling.
    std::size_t named = 0;
    for (std::int64_t sc = 1; sc < 128; ++sc) {
        const char* name = key_name_of(sc);
        if (name == nullptr) {
            continue;
        }
        ++named;
        CHECK(scancode_of_name(name) == sc);
        for (std::int64_t mods = 0; mods < 16; ++mods) {
            const Gesture g{sc, mods};
            const std::string word = gesture_word(g);
            CAPTURE(word);
            const ParsedGesture back = parse_gesture(word);
            REQUIRE_MESSAGE(back.accepted, back.refusal);
            CHECK(back.gesture == g);
        }
    }
    REQUIRE_MESSAGE(named > 0, "the named scan set is empty -- nothing was witnessed");
    MESSAGE((std::to_string(named) + " named keys, 16 modifier sets each, round-tripped"));
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
                                              const Session& s, const Screen& sc) {
    return layouts_region_on(c, s, sc);
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
    // THE PHASE'S TARGET LAW, as one sweep: at the shipped metric, a full screen -- Info open
    // and the stand-in open -- publishes its prose as regions the graphical medium sets in
    // type, and no `SurfaceLabel` is left: a label is kept only where its CELL is the meaning,
    // and the one such glyph off the arrangement (the object canvas's size handle) retired
    // with the canvas.
    Session s = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    (void)open_panel(s.panels, stock::kKind);
    const surface::SurfaceCanvas c = paint(s);

    for (const surface::SurfaceLabel& l : all_labels(c)) {
        CAPTURE(l.text);
        CHECK(l.text == std::string(kHandleGlyph));
    }
    CHECK(all_labels(c).empty());

    // ...and every published region's bounds hold at least one row of the face, so none
    // of them falls back to the cell voice: the band and every pane's region.
    const Screen sc = screen_of(s);
    for (const surface::SurfaceTextRegion& r : all_texts(c)) {
        CAPTURE(r.x);
        CAPTURE(r.y);
        CHECK(surface::fit_region(r, surface::SurfaceExtent{0, 0, 8, 18}).graphical());
    }
    CHECK(band_on(c, sc) != nullptr);
}

TEST_CASE("QR-14/SC-2+SC-7: two bands compose their budgets, and the selector is row 0") {
    // A CHARACTER MEDIUM: two rows at the top (the layout selector with the setup's status,
    // then the workspace fact) and four at the foot (the notice, then the legend takes what
    // the notice leaves). Five facts in six reserved rows, which is what the screen has
    // always reserved -- WUX-1 left one of them blank at row 0 and QR-14 spends it.
    Session cells = screen_session(kScreenMinW, kScreenMinH, 0, 0);
    cells.notice = "a notice";
    const Screen csc = screen_of(cells);
    const surface::SurfaceCanvas cell_canvas = paint(cells);

    const surface::SurfaceTextRegion* ctop = top_band_on(cell_canvas, cells, csc);
    REQUIRE(ctop != nullptr);
    CHECK(ctop->y == 0); // THE FIRST WORKSHOP ROW IS THE LAYOUT SELECTOR
    CHECK(layouts_body(cells, csc).rows == kTopRows);
    REQUIRE(ctop->rows.size() == 2);
    CHECK(band_row(ctop, 0).rfind(">Default<", 0) == 0); // the live layout tab (WUX-9)
    CHECK(band_row(ctop, 0).find("setup: none") != std::string::npos);
    CHECK(band_row(ctop, 1) == "workspace 78x16 cells");

    const surface::SurfaceTextRegion* cband = band_on(cell_canvas, csc);
    REQUIRE(cband != nullptr);
    CHECK(band_fit(csc).rows == kBottomRows);
    REQUIRE(cband->rows.size() == 4);
    CHECK(band_row(cband, 0) == "a notice");
    CHECK(band_row(cband, 1).rfind("q quit", 0) == 0);
    CHECK_FALSE(band_row(cband, 2).empty());
    CHECK_FALSE(band_row(cband, 3).empty()); // no reserved row is left spare
    // ...AND NO ROW SAYS ANYTHING TWICE. The identity is the top band's and the notice is
    // the bottom band's, and neither writes in the other's rectangle.
    CHECK(band_row(cband, 0).find("\"Default\"") == std::string::npos);
    CHECK(band_row(ctop, 0).find("a notice") == std::string::npos);
    CHECK(ctop->y + ctop->h == cells_covered(fine_of_cells(ui::Rect{0, kWorkspaceY, 1, 1})).y);
    CHECK(cband->y == kWorkspaceY + csc.room_h); // the body ends where the band begins

    // THE SHIPPED FACE: one row at the top (the identity with the workspace fact folded in,
    // WUX-1's own fold) and two at the foot (the notice and one packed legend row). THREE
    // face rows of chrome, exactly as many as the single five-cell band held.
    Session sdl = screen_session(kScreenMinW, kScreenMinH, 8, 18);
    sdl.notice = "a notice";
    const Screen ssc = screen_of(sdl);
    const surface::SurfaceCanvas sdl_canvas = paint(sdl);
    const surface::SurfaceTextRegion* stop = top_band_on(sdl_canvas, sdl, ssc);
    const surface::SurfaceTextRegion* sband = band_on(sdl_canvas, ssc);
    REQUIRE(stop != nullptr);
    REQUIRE(sband != nullptr);
    CHECK(layouts_body(sdl, ssc).rows == 1);
    CHECK(band_fit(ssc).rows == 2);
    REQUIRE(stop->rows.size() == 1);
    REQUIRE(sband->rows.size() == 2);
    CHECK(band_row(stop, 0).rfind(">Default<", 0) == 0);
    CHECK(band_row(stop, 0).find("| workspace 78x16 cells") != std::string::npos);
    CHECK(band_row(sband, 0) == "a notice");
    CHECK(band_row(sband, 1).rfind("q quit", 0) == 0);

    // DEGRADE HONESTLY BELOW THAT, and each band degrades in its own rectangle: the legend
    // gives way first at the foot, and the workspace fact folds into the identity at the top.
    Session three = screen_session(kScreenMinW, kScreenMinH, 8, 14); // 44/14 = 3 band rows
    three.notice = "a notice";
    const surface::SurfaceCanvas three_canvas = paint(three);
    const surface::SurfaceTextRegion* tband = band_on(three_canvas, screen_of(three));
    REQUIRE(tband != nullptr);
    REQUIRE(tband->rows.size() == 3);
    CHECK(band_row(tband, 0) == "a notice");
    CHECK(band_row(tband, 1).rfind("q quit", 0) == 0);

    Session one = screen_session(kScreenMinW, kScreenMinH, 8, 30); // 44/30 = 1 band row
    one.notice = "a notice";
    const surface::SurfaceCanvas one_canvas = paint(one);
    const surface::SurfaceTextRegion* oband = band_on(one_canvas, screen_of(one));
    const surface::SurfaceTextRegion* otop = top_band_on(one_canvas, one, screen_of(one));
    REQUIRE(oband != nullptr);
    REQUIRE(oband->rows.size() == 1);
    CHECK(band_row(oband, 0) == "a notice"); // the tool's voice wins the one row
    // ⚠ AND THE IDENTITY IS NOT A CANDIDATE FOR THAT ROW ANY MORE. It has a band of its own
    // that no budget down here can take, which is the whole point of the move: a maker never
    // loses sight of which desk they are in because the tool had something to say.
    REQUIRE(otop != nullptr);
    REQUIRE_FALSE(otop->rows.empty());
    CHECK(band_row(otop, 0).rfind(">Default<", 0) == 0);

    one.notice.clear();
    const surface::SurfaceCanvas quiet_canvas = paint(one);
    const surface::SurfaceTextRegion* qband = band_on(quiet_canvas, screen_of(one));
    REQUIRE(qband != nullptr);
    REQUIRE(qband->rows.size() == 1);
    CHECK(qband->rows[0].text.rfind("q quit", 0) == 0); // the legend, with nothing said
}

TEST_CASE("WUX-1/SC-3: the legend modes move only the legend rows, in both budgets") {
    for (const std::int64_t line : std::vector<std::int64_t>{0, 18}) {
        CAPTURE(line);
        Session s = screen_session(kScreenMinW, kScreenMinH, line == 0 ? 0 : 8, line);
        s.notice = "a notice";
        const Screen sc = screen_of(s);
        // THE LEGEND IS THE BOTTOM BAND'S SECOND ROW ON EVERY MEDIUM SINCE QR-14: the
        // notice leads that band and the legend takes what it leaves.
        const std::size_t legend_at = 1;

        s.keymap.legend = legend_mode::kFull;
        const surface::SurfaceCanvas full_c = paint(s);
        const surface::SurfaceTextRegion* full_b = band_on(full_c, sc);
        REQUIRE(full_b != nullptr);
        CHECK(band_row(full_b, legend_at).rfind("q quit", 0) == 0);

        s.keymap.legend = legend_mode::kCompact;
        const surface::SurfaceCanvas compact_c = paint(s);
        const surface::SurfaceTextRegion* compact_b = band_on(compact_c, sc);
        REQUIRE(compact_b != nullptr);
        // COMPACT IS THE APPLICATION'S ROWS ABOVE EVERY MODE; with no desktop, there are none.
        CHECK(band_row(compact_b, legend_at).empty());
        Session with_desktop = s;
        REQUIRE(join_app_rows(with_desktop.keymap,
                              std::vector<AppRow>{AppRow{"desktop.hotkeys", "hotkeys",
                                                         Gesture{input::scan::kK,
                                                                 input::mod::kCtrl},
                                                         app_precedence::kAboveModes}})
                    .accepted);
        const surface::SurfaceCanvas compact_d = paint(with_desktop);
        CHECK(band_row(band_on(compact_d, sc), legend_at) == "^k hotkeys");

        s.keymap.legend = legend_mode::kHidden;
        const surface::SurfaceCanvas hidden_c = paint(s);
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
        CHECK(band_row(top_band_on(full_c, s, sc), 0) ==
              band_row(top_band_on(hidden_c, s, sc), 0));
    }
}

TEST_CASE("WUX-1/SC-2: the effective keymap remains the full claim surface for the moved hints") {
    // The gestures the retired row advertised are ordinary keymap rows, so the authoritative
    // surface -- the effective keymap a Hotkeys pane lists -- has them. (The third was the
    // picker's `+ panel`, and it retired with the picker: its successor is the desktop's
    // `desktop.panes`, an application row taught while the desktop declares it.)
    Live t;
    const std::string view = keymap_text(t.session());
    CHECK(view.find("arrange desk") != std::string::npos);
    CHECK(view.find("+ panel") == std::string::npos);
    CHECK(view.find("titles") != std::string::npos); // the new action is discoverable too
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
        apply_overrides({{"workshop.pane-titles", "a"}}, legend_mode::kDefault, refused);
    CHECK_FALSE(verdict.accepted);
    CHECK(verdict.refusal.find("workshop.pane-titles") != std::string::npos);
    CHECK(verdict.refusal.find("workshop.context") != std::string::npos);
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

    // Titles hidden AND the pane unfocused: the lattice ON SCREEN reserves no header, so the
    // panel's top prose row is the provider's row 0 -- and a press there names row 0. It is
    // read against that picture, the one the maker aimed at, BEFORE the press focuses the
    // pane; the focus then brings the title back (SC-6's invariant), and from then on the
    // titled lattice is the one on screen, where that same cell is the header's.
    press_outside(r, kind);
    r.key(input::scan::kT);
    r.text("t");
    REQUIRE_FALSE(r.session().pane_titles);
    REQUIRE(external_title_rows(r.session().panels, kind, r.session().pane_titles) == 0);
    const std::size_t hidden_before = seat->presses.size();
    r.press_cell(body.x + 1, body.y); // the panel's top row: bare, no header reserved
    REQUIRE(seat->presses.size() == hidden_before + 1);
    CHECK(seat->presses.back().row == 0);
    CHECK(keyboard_pane(r.session().panels) == kind);
    REQUIRE(external_title_rows(r.session().panels, kind, r.session().pane_titles) == 1);
    const std::size_t focused_before = seat->presses.size();
    r.press_cell(body.x + 1, body.y); // the same cell, now the focused pane's header
    CHECK(seat->presses.size() == focused_before);
    r.press_cell(body.x + 1, body.y + kExternalHeaderRows);
    REQUIRE(seat->presses.size() == focused_before + 1);
    CHECK(seat->presses.back().row == 0);
}

// ============================================================================
// CTX-0 — the catalog the contextual rows reference
// ============================================================================
//
// ⭐ THE OBJECT DOCUMENT'S CASES WERE HERE: CTX-0's explicit-id deletion and its selection
// repair, the document crossing the pane seam as a picture (WL-DOC-20) and a commit written
// only to the subject the host named (WL-DOC-21). They retired with the object canvas. The
// Info pane inspects a PANE now, through the subject the host names for it -- the same
// discipline over a real subject, in `tests/test_workshop_panes_info.cpp` (WL-INFO).

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
