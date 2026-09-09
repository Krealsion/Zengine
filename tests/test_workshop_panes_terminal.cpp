// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — THE TERMINAL, AS A LOADED WEAVE.
//
// THIS FILE OWNS the transcript a maker reads and the line they compose messages on.
// Everything the terminal overlay did that a maker can see -- reading the record, typing a
// line, running it, being told what could come next, choosing a candidate, and watching the
// answer arrive -- is driven here through the REAL `zengine-terminal-pane` image, over the
// REAL pane protocol, against the REAL participant this host mounts. Nothing in this file
// constructs the weave, reaches into its state, or calls one of its functions.
//
// ---- WHY THIS ONE IS DIFFERENT FROM THE OTHER FOUR -------------------------------
//
// ⚠ THE THING IT PRESENTS DID NOT MIGRATE, AND THAT IS MEASURED. `loom::TerminalSession` is
// a `loom::Weave` whose handler sends nothing by construction, and the only shapes it accepts
// are the three answer doors its host declared -- so no message IT ACCEPTS TODAY makes it
// author a line, and none makes it answer with its transcript. Under this work's constraint
// -- the Loom is fenced -- that settles it; it is not a claim about every possible Loom. It
// stays a host-mounted identity with a grant of one rule. Every case here mounts it on the
// rig's bus, drives the PANE, and then asks THAT OBJECT what it heard: the two identities
// stay two, measured rather than asserted, and the record's oldest sentence is still under
// test.
//
// ⚠ AND IT IS THE FIRST PANE WITH A CARET. `PaneCaret` is this migration's one new protocol
// sentence, and the cases for it are here rather than in the seam suite because this is the
// only image that publishes one -- the seam suite owns what Workshop does with a caret it is
// sent, including the ones it refuses.
//
// ⚠ AND THE COMPLETION IS AN ASK, not a picture. What a maker may say next depends on the
// participant's LIVE composition ladder, so the pane holds the line and the participant's
// holder answers about it. "Browsing candidates authors nothing" is now a claim about which
// SHAPE the pane sent, and it is checked with the participant's own transcript.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "terminal-pane/vocabulary.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {

namespace pane = zengine::terminal_pane;

/// The office and pane, spelled through the package's own header -- the durable names, not
/// literals, so a case cannot agree with a typo.
inline PaneRef pane_terminal_ref() {
    return PaneRef{pane::kTerminalPaneRole, pane::kTerminalPane};
}

/// A LIVE WORKSHOP WITH THE REAL TERMINAL PANE LOADED INTO IT.
struct TerminalRig {
    PaneRig r;
    std::int64_t kind = 0;
    loom::TerminalSession* me = nullptr;

    /// LOAD THE IMAGE, MOUNT THE PARTICIPANT, AND PICK THE PANE.
    ///
    /// ⚠ THERE IS A `pick` HERE AND ITS PRESENCE IS THE CLAIM. Info's reference is what
    /// `default_setup` authors, so its office resolved a row that was already there. No
    /// setup has ever named the Terminal -- it was a bool behind a global chord, and no
    /// `PaneRef` for it exists in any file a maker has written. So it arrives the way Files,
    /// the Builder and Attention did: a stranger a maker opens from the picker.
    void open(std::int64_t width = 160, std::int64_t height = 48, int shapes = 0,
              bool participant = true) {
        r.mount_workshop();
        if (participant) {
            me = r.mount_terminal(shapes);
        }
        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = pane::kTerminalPaneStem;
        seat.weave = load::WeaveIntent{pane::kTerminalPaneRole};
        plan.artifacts.push_back(seat);
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(width, height);
        REQUIRE_MESSAGE(row() != nullptr, "the loaded image offered no `terminal` pane");
        kind = row()->kind;
        r.pick(pane_terminal_ref());
        REQUIRE(r.session().panels.has(kind));
    }

    const RuntimePane* row() {
        return r.session().panels.runtime.find(pane::kTerminalPaneRole, pane::kTerminalPane);
    }

    /// PRESS INTO THE PANE, which is the whole of what VD-22 made necessary: its rows are
    /// active only while it holds the keyboard. The overlay this replaces took the keyboard
    /// WHOLE, from wherever the maker was standing, the moment a global chord fired.
    void focus() {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.press_cell(body.x, body.y); // the header: a row that means nothing
        REQUIRE(r.session().panels.keyboard == kind);
    }

    void unfocus() {
        r.press_cell(0, screen_of(r.session()).h - 1);
        REQUIRE(r.session().panels.keyboard != kind);
    }

    std::vector<std::string> shown() { return pane_rows(r, kind); }

    std::string text() {
        std::string all;
        for (const std::string& one : shown()) {
            all += one;
            all += '\n';
        }
        return all;
    }

    /// THE ROW A GIVEN PREFIX IS ON, in the pane's own lattice.
    std::int64_t row_of(const std::string& prefix) {
        const std::vector<std::string> rows = shown();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].rfind(prefix, 0) == 0) {
                return static_cast<std::int64_t>(i);
            }
        }
        return -1;
    }

    /// THE INPUT ROW -- the last one this pane publishes, which is where it puts its caret.
    std::int64_t input_row() { return static_cast<std::int64_t>(shown().size()) - 1; }

    void press_row(std::int64_t at, std::int64_t column = 0) {
        press_pane(r, kind, at, column);
    }

    /// GIVE THIS PANE EXACTLY `rows` ROWS OF ITS OWN, by authoring the height a maker would
    /// drag -- the pane's own chrome is three cells of the authored box, measured.
    ///
    /// ⚠ AND THEN REPAINT, because an extent identical to the standing one is deduplicated
    /// and re-resolves nothing: the room a case asks for arrives on the next repaint, and
    /// pressing into the pane is the one every case here already makes.
    void give_rows(std::int64_t rows) {
        const Written wrote =
            author_pane_size(r.session().setup.active, pane_terminal_ref(), PaneSize{},
                             PaneSize{pane_unit::kSubcells, subs(rows + 3)});
        REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
        focus();
        REQUIRE(seat() != nullptr);
        REQUIRE_MESSAGE(seat()->rows == rows, "asked for ", rows, " rows and was granted ",
                        seat()->rows);
    }

    // ---- STAGING AN ORDER ON THE REAL BUS ------------------------------------------
    //
    // ⚠ THE ONLY WAY A CASE HERE CAN SAY "WHILE THAT ANSWER WAS IN FLIGHT". Every helper
    // above publishes AND drains, so a request and its answer are both spent before the
    // call returns and no in-flight state survives to the next line. These three enqueue
    // without draining, so a batch of gestures is delivered the way one poll delivers them
    // -- and an answer a pane asked for during the batch lands BEHIND the gestures that
    // were already queued. No sleep, no thread, no timing: the bus is FIFO and the order is
    // the case's own. `test_workshop_document.cpp` stages QR-11's editor race the same way.
    void enqueue_key(std::int64_t sc, std::int64_t mods = input::mod::kNone) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::KeyPressed{sc, "", mods}),
                                          loom::WeaveId{}, loom::WeaveId{}, 0));
    }
    void enqueue_text(const std::string& s) {
        (void)r.bus.publish(loom::Message(loom::to_value(input::TextEntered{s}), loom::WeaveId{},
                                          loom::WeaveId{}, 0));
    }
    void settle() { r.bus.drain_until_idle(); }

    /// THE ROW THE MAKER IS TYPING ON, as the pane last published it.
    std::string input_text() {
        const std::vector<std::string> rows = shown();
        REQUIRE_FALSE(rows.empty());
        return rows[static_cast<std::size_t>(input_row())];
    }

    /// Type a whole line into the pane, a character at a time, as a backend reports it.
    void type(const std::string& line) {
        for (const char c : line) {
            r.text(std::string(1, c));
        }
    }

    void submit() { r.key(input::scan::kReturn); }

    /// THE CARET WORKSHOP IS HOLDING FOR THIS PANE, read off the host's own record -- so a
    /// case asks what was ADMITTED rather than what was sent.
    const ExternalPane* seat() { return r.session().panels.external_pane(kind); }

    /// The region this pane's rows are drawn into, on the last canvas -- found at the exact
    /// corner `external_body_rect` resolves, which is what `external_region_rows` matches on.
    ///
    /// BY VALUE, because `all_texts` answers by value: a pointer into the range of a
    /// range-for over it dies at the semicolon, which is a use-after-free the sanitizer lane
    /// names and an ordinary run does not.
    surface::SurfaceTextRegion region() {
        const ui::Rect body = external_body_rect(r.session(), kind);
        const std::vector<surface::SurfaceTextRegion> texts = all_texts(r.last_canvas());
        for (const surface::SurfaceTextRegion& one : texts) {
            if (one.x == body.x && one.y == body.y) {
                return one;
            }
        }
        FAIL("no region at this pane's own corner");
        return surface::SurfaceTextRegion{};
    }

    /// Every entry of the participant's own record, rendered by the CORE's own facts.
    std::vector<loom::TranscriptEntry> record() {
        REQUIRE(me != nullptr);
        return me->transcript().entries();
    }

    /// The ids this pane declares RIGHT NOW, sorted.
    std::vector<std::string> declared() {
        const RuntimePane* one = row();
        REQUIRE(one != nullptr);
        std::vector<std::string> ids;
        for (const PaneActionRow& a : one->actions) {
            ids.push_back(a.id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }
};

} // namespace

// ============================================================================
// THE OFFER, THE ROWS, AND THE KEYS
// ============================================================================

TEST_CASE("TERM-W1: the Terminal is an ordinary arranged pane, offered by an office") {
    // ⭐ THE SENTENCE THE WHOLE MIGRATION IS FOR. The overlay had no catalog row, no
    // `PaneRef`, no place a maker chose and no boundary; it was `Session::terminal.open`.
    TerminalRig t;
    t.open();
    const RuntimePane* row = t.row();
    REQUIRE(row != nullptr);
    CHECK(row->provider == pane::kTerminalPaneRole);
    CHECK(row->pane == pane::kTerminalPane);
    CHECK(row->name == std::string(pane::kTerminalPaneName));
    CHECK(row->summary == std::string(pane::kTerminalPaneSummary));
    // IT IS A RUNTIME KIND, so it is placed in the overlay stack like every other migrated
    // pane -- moved, sized, fronted and closed by the same gestures.
    CHECK(is_runtime_kind(row->kind));
    CHECK(placement_of(row->kind) == placement::kOverlayStack);
    // AND WORKSHOP COMPILES NOTHING FOR IT: the reference is the OFFICE's, minted from the
    // offer, and no `panel::k*` constant of this host names it.
    const std::optional<std::int64_t> resolved =
        resolve_pane(pane_terminal_ref(), t.r.session().panels);
    REQUIRE(resolved.has_value());
    CHECK(*resolved == row->kind);
}

TEST_CASE("TERM-W2: the five keys are the pane's rows, on the built-in's own spellings") {
    // A MAKER'S AUTHORED OVERRIDE MOVES WITH THE PANE. The overlay's five rows lived in
    // `KeyContext::kTerminal` under exactly these ids and exactly these gestures; a keymap
    // file naming one of them still names it.
    TerminalRig t;
    t.open();
    const std::vector<std::string> ids = t.declared();
    // sorted: back, complete, next, previous, submit
    CHECK(ids == std::vector<std::string>{pane::kActionBack, pane::kActionComplete,
                                          pane::kActionDown, pane::kActionUp,
                                          pane::kActionSubmit});
    const RuntimePane* row = t.row();
    REQUIRE(row != nullptr);
    const auto gesture_of = [row](const char* id) {
        for (const PaneActionRow& a : row->actions) {
            if (a.id == id) {
                return std::pair<std::int64_t, std::int64_t>{a.scancode, a.modifiers};
            }
        }
        FAIL("no such row: ", id);
        return std::pair<std::int64_t, std::int64_t>{0, 0};
    };
    CHECK(gesture_of(pane::kActionSubmit).first == input::scan::kReturn);
    CHECK(gesture_of(pane::kActionComplete).first == input::scan::kTab);
    CHECK(gesture_of(pane::kActionUp).first == input::scan::kUp);
    CHECK(gesture_of(pane::kActionDown).first == input::scan::kDown);
    CHECK(gesture_of(pane::kActionBack).first == input::scan::kEscape);
    for (const PaneActionRow& a : row->actions) {
        CHECK(a.modifiers == input::mod::kNone);
    }
}

TEST_CASE("TERM-W3: nothing global opens it, and no key acts on it from anywhere else") {
    // ⭐ VD-22, ON THE LAST GLOBAL THAT VIOLATED IT. `workshop.terminal` was `Ctrl+t` from
    // anywhere; `Ctrl+a` went the same way with Attention's overlay. The Terminal is opened
    // from the picker, and its keys reach it only after a maker has pressed into it.
    TerminalRig t;
    t.open();
    // No action row of Workshop's own names the terminal any more.
    for (const ActionRow& a : kActionCatalog) {
        CHECK(std::string(a.id).rfind("terminal.", 0) != 0);
        CHECK(std::string(a.id) != "workshop.terminal");
    }
    // ...nor does the contextual catalog.
    for (const ContextRow& c : kContextCatalog) {
        CHECK(std::string(c.action) != "workshop.terminal");
    }
    // AND THE OLD CHORD DOES NOTHING. Ctrl+t while the pane does NOT hold the keyboard is
    // an unbound key: nothing opens, nothing is typed, and the pane's line stays empty.
    t.unfocus();
    const std::string before = t.text();
    t.r.key(input::scan::kT, input::mod::kCtrl);
    CHECK(t.text() == before);
}

TEST_CASE("TERM-W4: a maker presses in, types a line, and the participant runs it") {
    // ⭐ THE WHOLE LOOP, THROUGH THE REAL IMAGE. Press in; type; Return; and the line is on
    // the participant's own record, recorded by the participant and by nothing else.
    TerminalRig t;
    t.open();
    t.focus();
    t.type("hello there");
    CHECK(t.text().find("> hello there") != std::string::npos);
    // NOTHING WAS AUTHORED BY TYPING. The record is still empty: composing is not sending.
    CHECK(t.record().empty());
    t.submit();
    const std::vector<loom::TranscriptEntry> after = t.record();
    REQUIRE_FALSE(after.empty());
    CHECK(after.front().kind == loom::TranscriptKind::LocalCommand);
    CHECK(after.front().text == "hello there");
    // ...AND THE PARTICIPANT'S ANSWER CAME BACK AS A PICTURE, not as a reply to the act:
    // the grammar notice it recorded is on the pane's rows.
    CHECK(t.text().find("this pane speaks two verbs") != std::string::npos);
    // AND THE LINE IS CLEAR, ready for the next one.
    CHECK(t.row_of(">    ") >= 0);
}

TEST_CASE("TERM-W5: a typed send leaves through the PARTICIPANT's door, not the pane's") {
    // ⭐ THE CLAIM THE PARTICIPANT EXISTS FOR, and the reason it did not migrate. What
    // reaches the skin was authored by the TERMINAL's identity -- the bus stamp says so, and
    // a payload cannot write a bus stamp. The pane's own identity is a third weave that said
    // nothing to anybody.
    TerminalRig t;
    t.open();
    SkinSeat* skin = t.r.mount_skin_seat();
    t.focus();
    t.type("send @zengine.skin SurfaceText 1 slot=hello text=there");
    t.submit();
    // THE SLOT THE MAKER AUTHORED, out of everything this skin heard -- Workshop publishes
    // its own status text to the same office constantly, so the case names the message
    // rather than taking the last one.
    REQUIRE(skin->heard.size() == skin->from.size());
    std::size_t at = skin->heard.size();
    for (std::size_t i = 0; i < skin->heard.size(); ++i) {
        if (skin->heard[i].slot == "hello") {
            at = i;
        }
    }
    REQUIRE_MESSAGE(at < skin->heard.size(), "the authored SurfaceText never arrived");
    CHECK(skin->heard[at].text == "there");
    // ⭐ THE BUS STAMP: it cannot be written by a payload and cannot be chosen by whoever
    // composed the message. It says the TERMINAL authored this, not Workshop and not the
    // pane -- which is the whole reason the participant did not migrate.
    CHECK(skin->from[at] == t.r.terminal_id);
    CHECK(skin->from[at] != t.r.workshop_id);
    // The participant recorded a SUBMITTED entry, which is what the pane draws.
    const std::vector<loom::TranscriptEntry> record = t.record();
    bool submitted = false;
    for (const loom::TranscriptEntry& e : record) {
        submitted = submitted || e.kind == loom::TranscriptKind::Submitted;
    }
    CHECK(submitted);
    CHECK(t.text().find("SUBMITTED") != std::string::npos);
}

// ============================================================================
// THE PICTURE
// ============================================================================

TEST_CASE("TERM-W6: the record crosses as a picture, said only when the reading changed") {
    // THE `DocumentShown` DISCIPLINE, one owner over. A repaint that changed nothing about
    // the participant says nothing, which is what makes this seam terminate: a pane answers
    // a publication with its rows, and a repaint follows that.
    TerminalRig t;
    t.open();
    const std::size_t said_before = t.r.said_transcripts.size();
    REQUIRE(said_before > 0); // the first reading is news
    // ⚠ THE REPAINTS HERE ARE RESIZES AND NOT SKIN HELLOS, and the difference is real: a
    // fresh skin's hello makes this host ask the catalog again, every office re-offers, and
    // an OFFER deliberately un-says the pictures so a listener that has just arrived hears
    // them (`on(PaneOffered)`; TERM-W6b). A resize is an ordinary repaint with no new
    // listener in it, which is the beat this law is about.
    t.r.extent(160, 48);
    t.r.extent(160, 48);
    CHECK(t.r.said_transcripts.size() == said_before);
    // A NEW ENTRY IS NEWS.
    t.focus();
    t.type("x");
    t.submit();
    CHECK(t.r.said_transcripts.size() > said_before);
    const TranscriptShown last = t.r.said_transcripts.back();
    CHECK(last.attached);
    CHECK(last.participant == static_cast<std::int64_t>(t.r.terminal_id.value));
    REQUIRE_FALSE(last.entries.empty());
    CHECK(last.entries.front().kind == ws::kEntryCommand);
    CHECK(last.entries.front().text == "x");
}

TEST_CASE("TERM-W6b: a pane that loaded after the last publication still hears the reading") {
    // ⭐ THE DEFECT THE WITNESS FOUND, AND IT WAS MINE. "Said only when the reading changed" is
    // what makes this seam terminate -- and it means a pane that arrives AFTER this host last
    // spoke hears nothing, and shows a permanent "no participant was mounted on this bus" over
    // a process that mounted one. The shipped plan loads the Skin before the Terminal pane, so
    // the first repaint happens with the pane not yet live: in a real Workshop the header read
    // the wrong sentence, and the rig did not because it loads the pane before it paints.
    //
    // AN OFFER IS THE ONE MOMENT A NEW LISTENER CERTAINLY EXISTS. The host already re-says the
    // conditions and the document there (`on(PaneOffered)`); the record was simply not on that
    // list. Driven here as it happens: paint FIRST, load SECOND.
    TerminalRig t;
    t.r.mount_workshop();
    t.me = t.r.mount_terminal();
    t.r.ready();
    t.r.extent(160, 48);
    REQUIRE_FALSE(t.r.said_transcripts.empty()); // the host has spoken, to nobody
    REQUIRE(t.r.said_transcripts.back().attached);

    load::LoadPlan plan;
    load::ArtifactIntent seat;
    seat.stem = pane::kTerminalPaneStem;
    seat.weave = load::WeaveIntent{pane::kTerminalPaneRole};
    plan.artifacts.push_back(seat);
    const load::Executed done = t.r.run_plan(plan);
    REQUIRE_MESSAGE(done.ok, done.refusal);
    t.r.ready();
    REQUIRE(t.row() != nullptr);
    t.kind = t.row()->kind;
    t.r.pick(pane_terminal_ref());
    REQUIRE(t.r.session().panels.has(t.kind));

    // ⚔ MUTATION: dropping `transcript_said_ = false` from `on(PaneOffered)` puts
    // "no participant was mounted on this bus" here, which is the sentence a maker read.
    CHECK(t.text().find("TERMINAL -- weave #") != std::string::npos);
    CHECK(t.text().find("no participant was mounted") == std::string::npos);
}

TEST_CASE("TERM-W7: the image that presents a participant cannot reach one") {
    // ⚠ A SOURCE READ, because it is the only instrument that can keep this claim. Every
    // runtime case below drives an image that HAPPENS not to touch a participant; this one
    // says it cannot -- the headers that declare `loom::TerminalSession`, its transcript and
    // its vocabulary are not included, and the build file links nothing that would bring
    // them in. Prose about the participant is expected and is not searched for: what is
    // checked is what the translation unit can NAME.
    std::ifstream in(TERMINAL_PANE_SOURCE);
    REQUIRE(in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string source = buffer.str();
    REQUIRE_FALSE(source.empty());
    for (const char* forbidden : {"#include <zen/terminal", "#include \"workshop/complete.hpp\"",
                                  "#include <zen/host/terminal_wiring.hpp>",
                                  "#include \"workshop/weave.hpp\"",
                                  "#include \"workshop/screen.hpp\""}) {
        CAPTURE(forbidden);
        CHECK(source.find(forbidden) == std::string::npos);
    }
    // ...and the package links nothing that could bring one in: not the host's compiled
    // logic, and not Loom's terminal.
    std::ifstream cmake(TERMINAL_PANE_CMAKE);
    REQUIRE(cmake.good());
    std::stringstream cbuf;
    cbuf << cmake.rdbuf();
    const std::string build = cbuf.str();
    CHECK(build.find("zengine-workshop-logic") == std::string::npos);
    CHECK(build.find("loom::terminal") == std::string::npos);
    // The one Workshop target it DOES link is the vocabulary -- shapes and no behaviour.
    CHECK(build.find("zengine-workshop-vocabulary") != std::string::npos);
}

TEST_CASE("TERM-W8: a Workshop with no participant says so, and authors nothing") {
    // THE READING THAT IS NOT AN ABSENCE. A host may mount none; the built-in said so in its
    // header and refused to author, and the pane says the same two sentences from one bool.
    TerminalRig t;
    t.open(160, 48, 0, /*participant=*/false);
    CHECK(t.text().find("no participant was mounted on this bus") != std::string::npos);
    t.focus();
    t.type("send @zengine.skin SurfaceText 1 slot=a text=b");
    t.submit();
    // The refusal is the DOOR's -- there is nowhere to record the attempt, because the
    // participant IS the transcript.
    CHECK(t.text().find("no terminal participant is mounted") != std::string::npos);
    REQUIRE_FALSE(t.r.said_transcripts.empty());
    CHECK_FALSE(t.r.said_transcripts.back().attached);
    CHECK(t.r.said_transcripts.back().entries.empty());
}

TEST_CASE("TERM-W9: the pane says what it is not showing, in the two senses that differ") {
    // ENTRIES ABOVE THE TOP OF THIS PANE, and entries the participant evicted for good, are
    // two different facts. ⚠ `earlier` IS THE PANE'S ARITHMETIC NOW: the host publishes the
    // record and the eviction count, and only the party that decided how many rows it could
    // spend can say how many are above the top.
    TerminalRig t;
    t.open(80, 14);
    t.focus();
    CHECK(t.text().find("[the whole of this session's record is on screen]") !=
          std::string::npos);
    for (int i = 0; i < 40; ++i) {
        t.type("line " + std::to_string(i));
        t.submit();
    }
    const std::int64_t marker = t.row_of("... ");
    REQUIRE(marker >= 0);
    CHECK(t.shown()[static_cast<std::size_t>(marker)].find("earlier") != std::string::npos);
    // The whole record is still the participant's; the pane showed a tail of it.
    CHECK(t.record().size() > 40);
}

// ============================================================================
// THE CARET — this migration's one new protocol sentence
// ============================================================================

TEST_CASE("TERM-W10: the pane publishes a caret, and Workshop draws it into the region") {
    // ⭐ `PaneCaret`. The lattice is `PanePressed`'s -- row 0 is the first prose row of the
    // BODY -- and Workshop adds its own header offset when it merges, which is exactly the
    // offset it subtracts to locate a press. One measurer, both directions.
    TerminalRig t;
    t.open();
    t.focus();
    t.type("abc");
    const ExternalPane* seat = t.seat();
    REQUIRE(seat != nullptr);
    CHECK(seat->caret_row == t.input_row());
    CHECK(seat->caret_col == 2 + 3); // `> ` and three characters
    const surface::SurfaceTextRegion region = t.region();
    // The region's caret is the pane's, plus Workshop's own header row.
    CHECK(region.caret_row == seat->caret_row + external_title_rows(
                                                    t.r.session().panels, t.kind,
                                                    t.r.session().pane_titles));
    CHECK(region.caret_col == seat->caret_col);
    // ...and a cell projection inserts it as a character, so a character medium reads as it
    // always did.
    const std::vector<std::string> rows = t.shown();
    CHECK(rows[static_cast<std::size_t>(t.input_row())].rfind("> abc", 0) == 0);
}

TEST_CASE("TERM-W11: the caret carries a selection, and both ends or neither") {
    // THE DECISION, ANSWERED: the shape carries SELECTION as well as the cursor, because the
    // built-in had one and dropping it would have been a regression this migration caused.
    TerminalRig t;
    t.open();
    t.focus();
    t.type("hello");
    t.r.key(input::scan::kA, input::mod::kCtrl); // select all
    const ExternalPane* seat = t.seat();
    REQUIRE(seat != nullptr);
    CHECK(seat->sel_begin_row == t.input_row());
    CHECK(seat->sel_end_row == t.input_row());
    CHECK(seat->sel_begin_col == 2);
    CHECK(seat->sel_end_col == 2 + 5);
    const surface::SurfaceTextRegion region = t.region();
    CHECK(region.sel_begin_row != surface::kNoSelection);
    CHECK(region.sel_end_col - region.sel_begin_col == 5);
    // COLLAPSING IT UN-SAYS IT: a caret with no selection publishes none.
    t.r.key(input::scan::kRight);
    const ExternalPane* after = t.seat();
    REQUIRE(after != nullptr);
    CHECK(after->sel_begin_row == surface::kNoSelection);
    CHECK(after->sel_end_row == surface::kNoSelection);
    CHECK(after->caret_row == t.input_row());
}

TEST_CASE("TERM-W12: a press on the input row places the caret where the maker aimed") {
    // THE INVERSE PAIR, SPENT LIVE: a press names a prose column of the room the pane was
    // granted, and the caret the pane publishes lands at that column.
    TerminalRig t;
    t.open();
    t.focus();
    t.type("abcdefgh");
    t.press_row(t.input_row(), 2 + 3); // before the `d`
    const ExternalPane* seat = t.seat();
    REQUIRE(seat != nullptr);
    CHECK(seat->caret_row == t.input_row());
    CHECK(seat->caret_col == 2 + 3);
    // ...and typing lands there rather than at the end.
    t.type("X");
    CHECK(t.shown()[static_cast<std::size_t>(t.input_row())].rfind("> abcXdefgh", 0) == 0);
}

// ============================================================================
// THE COMPLETION — an ask, and the pane's own cursor over the answer
// ============================================================================

TEST_CASE("TERM-W13: what could be said next is an ASK, and browsing authors nothing") {
    // ⚠ THE CLAIM IS NOW ABOUT WHICH SHAPE THE PANE SENT. `TerminalCompletionRequested` is
    // answered by a path on which every call is const; the only path that authors is
    // `TerminalActRequested`. The instrument is the participant's own record: after a whole
    // session of typing and browsing, it holds nothing at all.
    TerminalRig t;
    t.open();
    t.focus();
    t.type("se");
    CHECK(t.text().find("send") != std::string::npos);
    t.r.key(input::scan::kTab);   // ask for the list
    t.r.key(input::scan::kDown);  // browse it
    t.r.key(input::scan::kUp);
    t.r.key(input::scan::kEscape); // dismiss it
    CHECK(t.record().empty());     // NOTHING was authored by any of that
}

TEST_CASE("TERM-W14: the list is rows INSIDE the pane, above the line it belongs to") {
    // ⚠ A VISIBLE CHANGE, AND IT IS THE PROTOCOL'S. The overlay drew the list as a SECOND
    // bounded region on top of its own; a pane publishes ONE list of rows (`PaneContent`)
    // and Workshop assembles ONE region from it. So the list takes room from the transcript
    // rather than covering it, and it is still never over the input line.
    TerminalRig t;
    t.open();
    t.focus();
    t.type("s");
    const std::vector<std::string> rows = t.shown();
    REQUIRE(rows.size() > 2);
    // The heading is on a row of this pane, and the input row is still the last one.
    const std::int64_t input = t.input_row();
    CHECK(rows[static_cast<std::size_t>(input)].rfind("> s", 0) == 0);
    bool listed = false;
    for (std::size_t i = 0; i + 1 < rows.size(); ++i) {
        listed = listed || rows[i].find("send") != std::string::npos;
    }
    CHECK(listed);
    // AND THERE IS EXACTLY ONE REGION for this pane on the canvas: no second one floats.
    const ui::Rect body = external_body_rect(t.r.session(), t.kind);
    std::size_t regions = 0;
    for (const surface::SurfaceLayer& layer : t.r.canvases.back().layers) {
        for (const surface::SurfaceTextRegion& one : layer.texts) {
            if (one.x == body.x && one.y <= body.y && one.y + one.h >= body.y + body.h) {
                ++regions;
            }
        }
    }
    CHECK(regions == 1);
}

TEST_CASE("TERM-W15: the selection survives a recomputation and not a change of question") {
    // THE DEFECT THE BUILT-IN MET AND THIS PANE INHERITS THE REPAIR FOR: the answer arrives
    // fresh every time, so a naive pane would reset the cursor after every keystroke and the
    // arrow keys would appear to do nothing. The question is the SLOT and the PARTIAL
    // together, which is why `selected` is not on the wire.
    TerminalRig t;
    t.open(240, 100, /*shapes=*/6);
    // THE PANE NEEDS ROOM FOR A LIST WITH TWO ROWS IN IT, and a maker gives a pane room by
    // arranging it -- which is the whole difference from an overlay that sized itself.
    const Written taller = author_pane_size(t.r.session().setup.active, pane_terminal_ref(),
                                           PaneSize{}, PaneSize{pane_unit::kSubcells, subs(24)});
    REQUIRE_MESSAGE(taller.accepted, taller.refusal);
    t.r.extent(240, 100);
    t.focus();
    // TYPING IS THE COMPLETION GESTURE: the list is already open, so there is no Tab here --
    // Tab with a list on screen ACCEPTS, which is the other half of "one key, one meaning".
    t.type("send #1 ");
    const std::int64_t first_chosen = t.row_of("> ");
    REQUIRE(first_chosen >= 0);
    REQUIRE(first_chosen != t.input_row()); // the mark is in the LIST, not on the line

    t.r.key(input::scan::kDown);
    const std::int64_t moved = t.row_of("> ");
    REQUIRE(moved >= 0);
    // THE MOVE SURVIVED THE RECOMPUTATION THAT FOLLOWED IT. Without the same-question rule
    // the answer would arrive fresh with `selected` at zero and this would be `first_chosen`
    // again -- which is exactly what the built-in did before it learned this.
    CHECK(moved == first_chosen + 1);

    // A DIFFERENT WORD IS A NEW LIST, AND IT STARTS AT THE TOP -- said with a partial that
    // keeps several candidates, so "the top" is discriminating: had the selection survived a
    // CHANGE of question, the mark would be on the third of these and not the first.
    t.r.key(input::scan::kDown); // selected = 2
    t.type("E");
    CHECK(t.shown()[static_cast<std::size_t>(t.input_row())].find("send #1 E") !=
          std::string::npos);
    const std::int64_t fresh = t.row_of("> ");
    REQUIRE(fresh >= 0);
    REQUIRE(fresh != t.input_row());
    CHECK(t.shown()[static_cast<std::size_t>(fresh)].find("Extra0") != std::string::npos);
}

TEST_CASE("TERM-W16: accepting a candidate edits the line, and the grammar's separators hold") {
    TerminalRig t;
    t.open();
    t.focus();
    t.type("se");
    t.r.key(input::scan::kTab); // accepts the selected candidate
    const std::vector<std::string> rows = t.shown();
    CHECK(rows[static_cast<std::size_t>(t.input_row())].rfind("> send ", 0) == 0);
    // ...and the caret is at the end of what was written, which is where the completer
    // requires it to be.
    const ExternalPane* seat = t.seat();
    REQUIRE(seat != nullptr);
    CHECK(seat->caret_col == 2 + static_cast<std::int64_t>(std::string("send ").size()));
}

TEST_CASE("TERM-W17: completion follows the END of the line, and says so when it cannot") {
    // THREE DIFFERENT SILENCES WOULD RENDER IDENTICALLY, so the pane says which one this is.
    TerminalRig t;
    t.open();
    t.focus();
    t.type("send ");
    t.press_row(t.input_row(), 2 + 1); // a caret inside the line
    CHECK(t.text().find("completion follows the END of the line") != std::string::npos);
    // Back at the end, the list comes back.
    t.r.key(input::scan::kEnd);
    CHECK(t.text().find("completion follows the END of the line") == std::string::npos);
}

// ============================================================================
// THE STATE A RELOAD KEEPS
// ============================================================================

TEST_CASE("TERM-W18: the state a same-shape reload keeps is the LINE, and only the line") {
    // ⭐ THE DECISION, WRITTEN DOWN AND PINNED AS A SHAPE. Info kept a cursor and dropped its
    // property draft; the Builder dropped its role line; this pane keeps its line.
    //
    // AND THE REASON IS FOUR QUESTIONS, not the word "composition" (`vocabulary.hpp`) --
    // recoverability, user effort, target identity, replacement semantics. TWO OF THEM
    // SEPARATE THESE DRAFTS AND TWO DO NOT: neither draft's edits survive being dropped, and
    // both name a target that can change under them. What carries the decision is the effort
    // in the line and the fact that keeping it risks nothing until an explicit submit. A
    // draft whose answers are MIXED is left undecided on purpose.
    //
    // ⚠ AND WHAT A RELOAD DOES WITH IT IS NOT WITNESSED HERE, which is Attention's own
    // posture one pane over: RELOAD-1's machinery is driven end to end over a real Kernel, a
    // real Manager and a staged image in `tests/test_workshop_load.cpp`, and doing it again
    // needs that rig rather than this one. What IS pinned here is the shape -- because the
    // shape is the decision, and a later edit that added a field to it would be adding a
    // second owner of a fact this pane does not own.
    const std::shared_ptr<const loom::Schema> shape = loom::schema_of<pane::TerminalPaneState>();
    REQUIRE(shape != nullptr);
    REQUIRE(shape->fields().size() == 1);
    CHECK(shape->fields()[0].name == "line");
    CHECK(shape->fields()[0].type.kind == loom::Kind::Text);
    // THE CARET IS NOT IN IT. A caret index carried into an image that may resolve the line
    // differently is a position without the thing that made it mean something; the new image
    // places it at the end, which is where the completer requires it and where a maker about
    // to keep typing wants it.
    //
    // AND NEITHER IS THE RECORD, THE COMPLETION, THE DISMISSAL OR THE ASKED FLAG. The first
    // is the host's reading, re-said the moment it changes; the second is derived from the
    // line by an ask; the last two are about a keystroke made against a word the maker is no
    // longer typing.

    // ...AND THE LINE REALLY IS THE ONE THING THE PANE PUTS THERE, driven live: a maker who
    // has typed half a command has that half on the pane's own row, and nothing this pane
    // shows besides it came from the maker at all.
    TerminalRig t;
    t.open();
    t.focus();
    t.type("send @zengine.skin SurfaceText 1 slot=a");
    const std::vector<std::string> rows = t.shown();
    CHECK(rows[static_cast<std::size_t>(t.input_row())].rfind("> send @zengine.skin", 0) == 0);
}

// ============================================================================
// THE THREE THINGS THAT CROSS A TURN BOUNDARY
// ============================================================================
//
// ⚠ EVERY CASE BELOW IS ABOUT ONE SHAPE OF DEFECT, and it is the shape this extraction
// introduced: an operation that used to be a FUNCTION CALL is now a request whose answer
// arrives later, and between the asking and the answering a maker keeps typing. Three
// operations cross that boundary here -- the act, the completion and the paste -- and each
// of them is answered against a line that may no longer be the line it was asked about.
//
// They are staged with `enqueue_*` and `settle`, never with a sleep: the bus is FIFO, a
// batch of gestures is delivered the way one poll delivers them, and an answer asked for
// during the batch lands behind the gestures already queued.

TEST_CASE("TERM-W19: a refusal is said BESIDE the line it is about, never in place of it") {
    // ⭐ THE PANE'S OWN PRIORITY ORDER, MADE TRUE. `say` spends its row budget input-row
    // first -- "a Terminal with no line is not a Terminal" -- and the refusal used to be
    // added AFTER that budget was spent, by truncating the composed rows to make room. The
    // row it truncated was the last one, which is the input row: the notice appeared and the
    // line it was about disappeared, taking the caret with it (`caret_row` back to
    // `kNoCaret`), so a maker read "nothing was authored" with nowhere to type again.
    TerminalRig t;
    t.open(160, 48, /*shapes=*/0, /*participant=*/false);
    t.focus();
    t.type("send #1 SurfaceText 1");
    t.submit();

    // THE DOOR'S OWN WORDS ARE ON THE PANE...
    CHECK(t.text().find("nothing was authored") != std::string::npos);
    // ...AND SO IS THE LINE THEY ARE ABOUT. The input row is still the last row the pane
    // published, which is the whole of what "beside" means here.
    const std::vector<std::string> rows = t.shown();
    REQUIRE_FALSE(rows.empty());
    CHECK(rows.front().find("nothing was authored") != std::string::npos);
    CHECK(rows.back().rfind(">", 0) == 0);
    // ...AND THE CARET IS ON IT, said to Workshop rather than inferred from the picture.
    const ExternalPane* seat = t.seat();
    REQUIRE(seat != nullptr);
    CHECK(seat->caret_row == static_cast<std::int64_t>(rows.size()) - 1);
    CHECK(seat->caret_row != surface::kNoCaret);

    // AND THE MAKER TYPES AGAIN, which is the thing the lost row made impossible.
    t.type("send");
    CHECK(t.input_text().rfind("> send", 0) == 0);
    CHECK(t.seat()->caret_col == 2 + 4); // the prompt's two columns, then four typed
    // A PRESS STILL MEANS WHAT THE PICTURE SAYS IT MEANS: the row the caret was published
    // on is the row a press places the caret in, so the two never disagree about what is
    // where -- which is the second thing the truncation broke.
    t.press_row(t.input_row(), 2 + 2);
    CHECK(t.seat()->caret_col == 2 + 2);
}

TEST_CASE("TERM-W19b: in a room too small for both, the LINE is what survives") {
    // THE RESULT FOR THE SMALL ROOMS, SAID OUT LOUD rather than left to arithmetic. Two rows
    // is the smallest room that holds both, and it holds them in that order.
    {
        TerminalRig t;
        t.open(160, 48, /*shapes=*/0, /*participant=*/false);
        t.give_rows(2);
        t.type("send #1 SurfaceText 1");
        t.submit();
        const std::vector<std::string> rows = t.shown();
        REQUIRE(rows.size() == 2);
        CHECK(rows[0].find("nothing was authored") != std::string::npos);
        CHECK(rows[1].rfind(">", 0) == 0);
        CHECK(t.seat()->caret_row == 1);
    }
    // ONE ROW IS THE ROOM THAT CANNOT HOLD BOTH, and the line wins it. The refusal is not
    // shown at all -- there is no row for it that is not the maker's own line -- and the
    // caret stays where a maker can keep typing.
    {
        TerminalRig t;
        t.open(160, 48, /*shapes=*/0, /*participant=*/false);
        t.give_rows(1);
        t.type("send #1 SurfaceText 1");
        t.submit();
        const std::vector<std::string> rows = t.shown();
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].find("nothing was authored") == std::string::npos);
        CHECK(rows[0].rfind(">", 0) == 0);
        CHECK(t.seat()->caret_row == 0);
    }
}

TEST_CASE("TERM-W20: a completion answer about a line that is gone is neither shown nor taken") {
    // ⭐ CORRELATION SAYS WHICH QUESTION AN ANSWER IS TO; IT DOES NOT SAY THE QUESTION STILL
    // STANDS. The pane asks about the line as it is at that instant, and every path that
    // ends the question early -- Escape emptying the line, the caret leaving the end, a
    // submit -- used to return without saying so, leaving the outstanding answer usable. It
    // then arrived, matched its own correlation, and became a list about a line nobody was
    // typing any more.
    TerminalRig t;
    t.open();
    // ROOM FOR CANDIDATE ROWS, so "no list" and "a list" are different pictures: the list
    // takes at most half of what the chrome leaves, and in the default room that half is one
    // row -- a heading with nothing under it, which both states would show.
    t.give_rows(10);

    // ONE POLL: the `s` is typed (which asks), and the Escape empties the line -- so the
    // answer offering `send ` arrives to a pane whose line is empty.
    t.enqueue_text("s");
    t.enqueue_key(input::scan::kEscape);
    t.settle();

    // NOT SHOWN: the pane offers no candidate at all, because the only answer it has is
    // about a line that no longer exists. (The rows are named by their marker rather than by
    // the word `send`, which is also inside the standing legend's `a sender`.)
    CHECK(t.row_of("> send") < 0);
    CHECK(t.row_of("  send") < 0);
    CHECK(t.input_text().find("Tab: what can this terminal say?") != std::string::npos);

    // NOT TAKEN: the completion key with nothing on screen means "ask for a list", and it
    // must not mean "accept the answer to the question I cancelled". The list that comes
    // back is about the EMPTY line -- both verbs, one of them marked -- and the maker's own
    // row is still empty.
    t.r.key(input::scan::kTab);
    CHECK(t.input_text().find("send") == std::string::npos);
    const std::int64_t chosen = t.row_of("> send");
    CHECK(chosen >= 0);
    CHECK(chosen != t.input_row());
    CHECK(t.row_of("  ask") >= 0);
}

TEST_CASE("TERM-W20b: an answer for a caret that has since moved does not reopen the list") {
    // THE SAME LAW ON THE OTHER GESTURE. Completion follows the END of the line, and the
    // pane says so out loud when the caret is inside it (TERM-W17). An answer asked for
    // while the caret WAS at the end used to overwrite that sentence with a real list -- and
    // a real list is a list the completion key accepts, which would delete everything after
    // the caret.
    TerminalRig t;
    t.open();
    t.focus();
    t.type("send ");

    // ONE POLL: the `x` is typed at the end (which asks), and the caret then steps back into
    // the line, so the answer arrives about a caret position the maker has left.
    t.enqueue_text("x");
    t.enqueue_key(input::scan::kLeft);
    t.settle();

    CHECK(t.text().find("completion follows the END of the line") != std::string::npos);
    CHECK(t.input_text().rfind("> send x", 0) == 0);
    // AND THE COMPLETION KEY IS STILL THE SILENCE IT SAYS IT IS: nothing is accepted into
    // the line from a list that is not open.
    t.r.key(input::scan::kTab);
    CHECK(t.input_text().rfind("> send x", 0) == 0);
}

TEST_CASE("TERM-W21: clipboard text lands in the draft that asked for it, or nowhere") {
    // ⭐ THE PROTECTION THE HOST USED TO SUPPLY, RESTORED AT ITS NEW OWNER (QR-11 and
    // the text-box register's paste law). While the terminal line was a box of this HOST's,
    // Workshop recorded its `draft_epoch` at the paste and applied the answer only if the
    // same draft still stood. The extracted pane kept the correlation check and lost the
    // epoch, so a maker who abandoned a command and typed a different one had the first
    // command's clipboard text spliced into the second.
    TerminalRig t;
    t.open();
    SkinSeat* skin = t.r.mount_skin_seat();
    REQUIRE(skin != nullptr);
    skin->platform = "OLD";
    t.focus();
    t.type("abc");

    // ONE POLL: the paste is asked for on `abc`, the line is then abandoned whole, and a
    // different command is typed on the fresh draft that follows.
    t.enqueue_key(input::scan::kV, input::mod::kCtrl);
    t.enqueue_key(input::scan::kEscape);
    t.enqueue_text("n");
    t.enqueue_text("e");
    t.enqueue_text("w");
    t.settle();

    // THE READ REALLY HAPPENED, so the absence below is a measurement and not a vacancy:
    // the staging reached the state where the payload was in the pane's hands.
    CHECK(skin->clipboard_reads == 1);
    CHECK(t.input_text().rfind("> new", 0) == 0);
    CHECK(t.text().find("OLD") == std::string::npos);

    // AND A PASTE INTO THE DRAFT THAT ASKED STILL LANDS, which is the half a check that
    // discarded everything would also pass.
    skin->platform = "OK";
    t.r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(skin->clipboard_reads == 2);
    CHECK(t.input_text().rfind("> newOK", 0) == 0);
}

TEST_CASE("TERM-W21b: an edit is not a new draft, and a submit is") {
    // THE POLICY, BOTH WAYS -- because "discard whenever anything changed" is the wrong
    // repair and would pass the case above. A keystroke is an EDIT to the draft that asked;
    // `set` and `clear` are the two doors that end one (`component::TextBox::draft_epoch`),
    // and they are what a paste is bound to.
    TerminalRig t;
    t.open();
    SkinSeat* skin = t.r.mount_skin_seat();
    REQUIRE(skin != nullptr);
    skin->platform = "OLD";
    t.focus();
    t.type("ab");

    // TYPED INTO WHILE THE ANSWER WAS IN FLIGHT: the same draft, so the text arrives at the
    // caret the maker has moved it to.
    t.enqueue_key(input::scan::kV, input::mod::kCtrl);
    t.enqueue_text("c");
    t.settle();
    CHECK(t.input_text().rfind("> abcOLD", 0) == 0);

    // SUBMITTED WHILE THE ANSWER WAS IN FLIGHT: the line was cleared to author it, so the
    // draft that asked is over and the payload lands nowhere -- on the fresh line least of
    // all, where a maker would have found bytes they never pasted anywhere.
    t.enqueue_key(input::scan::kV, input::mod::kCtrl);
    t.enqueue_key(input::scan::kReturn);
    t.settle();
    // The line that WAS authored is the one the paste had already landed in, asked of the
    // participant's own record rather than of the pane -- what a six-row pane has room to
    // show of a transcript is a different question from what is on it.
    bool authored = false;
    for (const loom::TranscriptEntry& e : t.record()) {
        authored = authored || e.text == "abcOLD";
    }
    CHECK(authored);
    CHECK(t.input_text().find("OLD") == std::string::npos);
}

TEST_CASE("TERM-W22: a paste is one gesture, and undo gives back the line it landed in") {
    // ⭐ THE REVIEWER'S OWN REPRODUCTION, and the defect is one word wide: the migration
    // reached for `TextBox::type` where the built-in used `TextBox::paste`. `type` with no
    // selection is a TYPING edit and coalesces into the burst before it, so the paste joined
    // the word the maker had typed and one undo took both. `paste` is `kStructural` -- one
    // gesture, one entry -- exactly as a cut is.
    TerminalRig t;
    t.open();
    SkinSeat* skin = t.r.mount_skin_seat();
    REQUIRE(skin != nullptr);
    skin->platform = "OLD";
    t.focus();
    t.type("keep");
    t.r.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(t.input_text().rfind("> keepOLD", 0) == 0);

    t.r.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(t.input_text().rfind("> keep", 0) == 0);
    CHECK(t.input_text().find("OLD") == std::string::npos);
    // ...and the typing that preceded it is still one entry of its own, so a second undo
    // takes the word and not half of it.
    t.r.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(t.input_text().find("keep") == std::string::npos);
    // ...and redo replays the two in order, which is the other half of "one entry".
    t.r.key(input::scan::kZ, input::mod::kCtrl | input::mod::kShift);
    CHECK(t.input_text().rfind("> keep", 0) == 0);
    t.r.key(input::scan::kZ, input::mod::kCtrl | input::mod::kShift);
    CHECK(t.input_text().rfind("> keepOLD", 0) == 0);
}

TEST_CASE("TERM-W23: what the clipboard holds is normalized to fit a line, or refused aloud") {
    // THE DOOR'S OWN JUDGEMENT, and it asks about the text that would LAND rather than the
    // text that arrived. `pasteable_line` is what a single-line field can hold -- a tab, an
    // LF, a CR and a CRLF pair are one space apiece -- and gating on `admissible` before that
    // normalization refused two copied lines WHOLE, silently. The component owns the
    // normalization; this case owns the pane's door spending it.
    TerminalRig t;
    t.open();
    SkinSeat* skin = t.r.mount_skin_seat();
    REQUIRE(skin != nullptr);
    t.focus();

    skin->platform = "two\r\nlines";
    t.r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.input_text().rfind("> two lines", 0) == 0); // the CRLF pair is ONE space

    // TWICE, because the first Escape dismisses a LIST if one is open and only then
    // clears: a case that leaned on which branch it took would change meaning under
    // the completer.
    t.r.key(input::scan::kEscape);
    t.r.key(input::scan::kEscape);
    REQUIRE(t.input_text().find("Tab: what can this terminal say?") != std::string::npos);
    skin->platform = "a\tb\nc";
    t.r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.input_text().rfind("> a b c", 0) == 0);

    // ⚠ AND WHAT NORMALIZATION CANNOT REPAIR IS REFUSED WHOLE AND SAID OUT LOUD. A byte
    // outside printable ASCII survives `pasteable_line` and would sit in a line whose own row
    // draws it as a space -- so a maker would submit something other than what they read.
    // This pane's typed door already refuses one; the difference here is that the door speaks.
    t.r.key(input::scan::kEscape);
    t.r.key(input::scan::kEscape);
    t.type("hold");
    skin->platform = "na\xC3\xAFve";
    t.r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.text().find("outside plain ASCII") != std::string::npos);
    CHECK(t.input_text().rfind("> hold", 0) == 0); // ...and the line is untouched
    CHECK(t.input_text().find("na") == std::string::npos);

    // A READABLE BUT EMPTY CLIPBOARD PASTES NOTHING -- and in particular not the mirror's
    // last value, which is the stale answer the ask exists to avoid.
    const int reads = skin->clipboard_reads;
    skin->platform = "";
    t.r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(skin->clipboard_reads == reads + 1); // the read really happened
    CHECK(t.input_text().rfind("> hold", 0) == 0);

    // A PASTE OVER A SELECTION REPLACES IT, which is the behaviour that moved with the door
    // and would have gone quietly if `paste` had been reached for without it.
    skin->platform = "gone";
    t.r.key(input::scan::kA, input::mod::kCtrl);
    t.r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.input_text().rfind("> gone", 0) == 0);
    CHECK(t.input_text().find("hold") == std::string::npos);
    t.r.key(input::scan::kEscape);
    t.r.key(input::scan::kEscape);
    t.type("hold");

    // AND A MEDIUM THAT CANNOT BE READ FALLS BACK TO THE MIRROR (WL-TEXT-10) -- the terminal
    // medium's own standing truth, which is what keeps copy-here paste-there working there.
    t.r.key(input::scan::kA, input::mod::kCtrl);
    t.r.key(input::scan::kC, input::mod::kCtrl);
    skin->readable_medium = false;
    t.r.key(input::scan::kEnd);
    t.r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(t.input_text().rfind("> holdhold", 0) == 0);
}
