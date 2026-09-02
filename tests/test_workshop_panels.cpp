// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panels suite — the panels Workshop itself ships, and the surface that says
// what is currently true.
//
// What it holds: what this terminal can say next; the dynamic panels and the Builder
// panel that stopped meaning ONE hard-coded target; Info, the second panel kind; the
// Inspector's property body with its real type, real bounds and real window; the Info
// panel's two lists sharing one bounded body; and attention — the current-condition
// surface, whose statements have a lifetime.
//
// A panel authored OUTSIDE this repository arrives through the external pane seam and is
// `test_workshop_panes_seam.cpp`; the geometry these panels are placed with is
// `test_workshop_screen.cpp`.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

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
    // (SurfaceCanvas is v7 since TEXT-0's selection fields; the list follows the wire.)
    const Completion all = complete_line(me, "send * ");
    CHECK(displays(all) == std::vector<std::string>{"SurfaceText v1", "SurfaceCanvas v8",
                                                    "zen.Ack v1"});
    // ACCEPTANCE WRITES THE VERSION TOO, because a shape without one is never a command
    // this pane can run: the grammar wants four words and the version is the fourth.
    CHECK(all.candidates[0].insert == "SurfaceText 1 ");
    CHECK(all.candidates[0].detail.find("slot:Text") != std::string::npos);

    // CASE FOLLOWS THE WIRE. A schema name is identity; matching `surfacetext` against
    // `SurfaceText` would offer a completion that composes to UnknownShape.
    CHECK(displays(complete_line(me, "send * Surface")) ==
          std::vector<std::string>{"SurfaceText v1", "SurfaceCanvas v8"});
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
    t.bus.drain_until_idle();

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
    t.bus.drain_until_idle();
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
    t.bus.drain_until_idle();
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
    plane(only_pane).texts = {*pane_of(only_pane, kMinScreen)};
    const std::vector<surface::ProjectedRow> shown = projected_of(only_pane);
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
    CHECK(all_texts(without_workspace(c)).back().x == list->x);
    CHECK(all_texts(without_workspace(c)).back().y == list->y);
    CHECK(all_texts(without_workspace(c)).front().y == body_place(t).region_y);
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
    // The workspace fact lives in the band's own row since WUX-1, and it moved with the
    // extent: what a share resolves against is said where the tool speaks.
    CHECK(workspace_row(c, t.session(), sc) == "workspace 70x27 cells");
    // The panel came with the right edge rather than staying at column 50.
    CHECK(inspector_row(c, sc.panel_x + kChromeCells, kSideY + kChromeCells) == "OBJECTS");
    CHECK(properties_heading(c, t.doc(), t.session()) == "PROPERTIES");
    CHECK(sc.panel_x == 72);

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
    CHECK(inspector_row(c, 50 + kChromeCells, kSideY + kChromeCells) == "OBJECTS");
    CHECK(label_at(c, 0, 19).rfind("n new | d delete", 0) == 0);
    CHECK(label_at(c, 0, 20).rfind("enter edit | up/down row", 0) == 0);
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
    t.bus.drain_until_idle();

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

TEST_CASE("every BUILT-IN catalog row reaches the picker, with its summary and its state") {
    // THE COMPILE-TIME CATALOG IS THE PICKER'S BUILT-IN HALF, and this case is asked in the
    // control where it is the whole of what the picker shows: a `Session` no office has
    // offered anything to. Since WP-0 the picker walks `combined_catalog` -- these rows, in
    // this order, and then whatever runtime panes this session admitted -- so "a kind that is
    // not in `kPanelCatalog` cannot be opened" is no longer the sentence. What is still true
    // and is what this case owes: every row that IS here reaches the picker, and the runtime
    // half's own coverage is the WP-0 tier's (`the combined picker lists an offered pane...`).
    //
    // ASSERTED OVER THE WHOLE CATALOG RATHER THAN OVER ITS LENGTH (WG-0). This case used to
    // open `REQUIRE(kPanelKinds == 2)` and then name two entries by hand, which made the
    // CENSUS of the catalog part of the claim: adding a panel kind reddened this case for no
    // reason to do with the kind being added, and it still said nothing about whether a third
    // row reached the picker. What the picker owes is one row per catalog entry carrying that
    // entry's name, its summary and its current state, and that is a claim over `kPanelKinds`
    // entries rather than over the number two. Measured: with the loop below bounded at two
    // instead of `kPanelKinds`, the old spelling passed and this one names every missing row.
    REQUIRE(kPanelKinds >= 2);
    CHECK(kPanelCatalog[0].kind == panel::kBuilder);
    CHECK(std::string(kPanelCatalog[0].name) == "Builder");
    CHECK(kPanelCatalog[1].kind == panel::kInfo);
    CHECK(std::string(kPanelCatalog[1].name) == "Info");

    Session s; // a fresh session: Info open, Builder not
    s.panels.picker.open = true;
    surface::SurfaceCanvas c;
    paint_picker(plane(c), s.panels, s.setup.active, screen_of(s), s.keymap);
    const std::string shown = stack_text(c);
    CHECK(shown.find("+ PANEL") != std::string::npos);
    // EVERY BUILT-IN ENTRY, WITH ITS SUMMARY AND ITS STATE BESIDE IT. The state column is what
    // makes the picker usable as the one owner of presence: Return does one of two opposite
    // things, so the list has to say which one it is about to do. The two `detail::pad` widths
    // are the picker's own, and reading them back here pins the COLUMN rather than merely the
    // word. `closed` rather than `picker_state_word` is right in THIS session: nothing here is
    // waiting, because nothing is authored beyond what the minimum screen seats.
    //
    // ⚠ FITTED TO THE PICKER'S OWN COLUMNS SINCE WUX-13. The name column widened to hold
    // `Pane Manager` whole (thirteen since WUX-14), and at the 78-column minimum that is
    // three cells off every summary
    // -- Info's `objects and properties` is now cut there and MARKED, which is `detail::fit`
    // doing its job. What the picker owes is the row AS IT FITS IT; the case searches for
    // exactly that rather than for a sentence the room cannot hold.
    const std::int64_t columns =
        panel_prose_place(picker_bounds(screen_of(s)), screen_of(s)).columns - 2;
    for (std::size_t i = 0; i < kPanelKinds; ++i) {
        const PanelKind& k = kPanelCatalog[i];
        const std::string state = s.panels.has(k.kind) ? "open" : "closed";
        INFO("catalog entry ", i, ": ", std::string(k.name));
        CHECK(shown.find(detail::fit(detail::pad(k.name, kPickerNameCols) +
                                         detail::pad(state, kPaneStateCols) + k.summary,
                                     columns)) != std::string::npos);
    }
    CHECK(shown.find(detail::pad("Builder", kPickerNameCols) + "closed") != std::string::npos);
    CHECK(shown.find(detail::pad("Info", kPickerNameCols) + "open") != std::string::npos);
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

// ============================================================================
// BLD-1 -- the Builder panel stops meaning ONE hard-coded target
// ============================================================================

TEST_CASE("BLD-1: the Builder panel shows what CAN be built and which one is chosen") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"oven", "zengine-oven"});
    open_builder(t);

    // ONE ASK, TWO ANSWERS. Opening the panel sends `StatusRequested`; the catalog
    // and the status come back as two publications, because they change at
    // completely different rates.
    CHECK(tool->described == 1);
    const std::string shown = stack_text(t.canvases.back());
    CHECK(shown.find("snake -> snake") != std::string::npos);
    CHECK(shown.find("(1/2)") != std::string::npos);
    // ...AND THE HEADER NAMES THE OFFICE AND CLAIMS NO GESTURE (WUX-5). It used to spell
    // `b/B build, c pick, f frontier, p removes` -- four ordinary keymap rows the band's
    // legend and the full hotkey view already say, in the maker's own bindings.
    CHECK(shown.find(std::string("BUILDER @") + zengine::builder::kBuilderRole) !=
          std::string::npos);
    for (const char* gesture : {"b/B build", "c pick", "f frontier", "removes"}) {
        CHECK(shown.find(gesture) == std::string::npos);
    }
}

TEST_CASE("BLD-1: `c` moves the maker's choice, wraps, and asks for nothing") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"oven", "zengine-oven"});
    open_builder(t);
    REQUIRE(t.w->session().panels.builder.chosen == 0);

    t.key(input::scan::kC);
    CHECK(t.w->session().panels.builder.chosen == 1);
    CHECK(t.w->session().notice == "build recipe: oven -> zengine-oven");
    CHECK(stack_text(t.canvases.back()).find("oven -> zengine-oven  (2/2)") != std::string::npos);

    // IT WRAPS, because a list of two a maker is stepping through with one key is a
    // ring and not a scrollbar.
    t.key(input::scan::kC);
    CHECK(t.w->session().panels.builder.chosen == 0);
    // ...and backwards with the modifier, which is the same gesture family spelled
    // two ways.
    t.key(input::scan::kC, input::mod::kShift);
    CHECK(t.w->session().panels.builder.chosen == 1);

    // NOTHING WAS ASKED OF ANYBODY. Choosing is a presentation move: the tool has
    // been asked once, when the panel opened, and not since.
    CHECK(tool->asked.empty());
    CHECK(tool->described == 1);
}

namespace {

/// A CATALOG ARRIVING THE WAY A REPLACEMENT MAKES ONE ARRIVE: the tool is asked what it is
/// and answers with what it now holds, WITHOUT the panel being closed -- which is exactly
/// the shape the live gesture produces (workshop/weave.hpp sends the same
/// `StatusRequested` after a successful replacement). `on(RecipeCatalog)` is the ONE place
/// a new catalog reaches this panel, whichever gesture caused it, so a case driving it
/// through this door is measuring the law at its owner.
///
/// ⚠ REMOVING AND REOPENING THE PANEL WOULD NOT DO. `close_panel` deliberately forgets the
/// Builder pane whole (panel.hpp), selection included, so a round trip through the picker
/// would arrange a case in which every answer below is 0 for a reason that has nothing to
/// do with the law being measured.
void recatalog(Live& t, ToolSeat* tool, const std::vector<const char*>& recipes) {
    tool->catalog.recipes.clear();
    for (const char* id : recipes) {
        tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{id, id});
    }
    (void)t.bus.send_to_role(zengine::builder::kBuilderRole,
                             loom::Message(loom::to_value(zengine::builder::StatusRequested{})));
    t.bus.drain_until_idle();
}

} // namespace

TEST_CASE("PROJ-1: a reordered catalog moves the maker's choice to its recipe, not its row") {
    // THE ORDINAL TRAP, ARRANGED SO THE WRONG ANSWER IS VISIBLE. Until a catalog could
    // change under a running Workshop, `chosen` being an index was harmless: the list it
    // indexed never moved. Now it can, and the same index in a new catalog is a DIFFERENT
    // RECIPE -- so an implementation that kept the number would silently re-aim `b`, `e`
    // and `f` at something the maker never picked, with the panel looking exactly as it
    // did a moment before.
    //
    // The new catalog is the SAME LENGTH, so no clamp can save a row-based answer: row 2
    // still exists and now holds `beta`, while the recipe the maker actually picked has
    // moved to row 1.
    Live t;
    ToolSeat* tool = mount_tool(t, "alpha");
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"beta", "beta"});
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"gamma", "gamma"});
    open_builder(t);

    t.key(input::scan::kC);
    t.key(input::scan::kC);
    REQUIRE(t.w->session().panels.builder.chosen == 2);
    REQUIRE(t.w->session().panels.builder.picked);
    REQUIRE(t.w->session().panels.builder.known.recipes[2].recipe == "gamma");

    recatalog(t, tool, {"alpha", "gamma", "beta"});

    // THE CHOICE FOLLOWED ITS RECIPE. Row 1 now, because that is where `gamma` is.
    CHECK(t.w->session().panels.builder.chosen == 1);
    CHECK(t.w->session().panels.builder.known.recipes[1].recipe == "gamma");
    // ...AND THE PICK IS STILL THE MAKER'S. A reordering is not a reason to forget that
    // they chose; `picked` records HOW a selection was made, and nothing about it moved.
    CHECK(t.w->session().panels.builder.picked);
    // THE BEHAVIOURAL HALF, which is the one that would have hurt: `b` asks for the
    // recipe the maker picked and not for the one now sitting at the old row.
    tool->asked.clear();
    t.key(input::scan::kB);
    REQUIRE(tool->asked.size() == 1);
    CHECK(tool->asked[0] == "gamma");
    CHECK(stack_text(t.canvases.back()).find("gamma -> gamma  (2/3)") != std::string::npos);
}

TEST_CASE("PROJ-1: a choice whose recipe is gone is cleared, not handed to its neighbour") {
    // THE ADJACENT-ROW TRAP. The maker's recipe has been REMOVED and the row number they
    // were on is still perfectly valid -- it holds somebody else's recipe now. A clamp
    // cannot catch this, because there is nothing to clamp: the index is in range. A
    // catalog replacement is allowed to INVALIDATE a standing choice and is not allowed to
    // REINTERPRET one, and this is the case where the difference is the whole answer.
    Live t;
    ToolSeat* tool = mount_tool(t, "alpha");
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"beta", "beta"});
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"gamma", "gamma"});
    open_builder(t);

    t.key(input::scan::kC);
    REQUIRE(t.w->session().panels.builder.chosen == 1);
    REQUIRE(t.w->session().panels.builder.picked);
    REQUIRE(t.w->session().panels.builder.known.recipes[1].recipe == "beta");

    recatalog(t, tool, {"alpha", "gamma"});

    // HOME, AND NOT THE MAKER'S ANY MORE. Row 1 exists and holds `gamma`; the selection
    // does not go there, and the pick is released so the frontier action cannot read
    // `chosen`'s default as an explicit intent (BLD-2's own distinction).
    CHECK(t.w->session().panels.builder.chosen == 0);
    CHECK_FALSE(t.w->session().panels.builder.picked);
    tool->asked.clear();
    t.key(input::scan::kB);
    REQUIRE(tool->asked.size() == 1);
    CHECK(tool->asked[0] == "alpha");
}

TEST_CASE("PROJ-1: a catalog that still holds the chosen recipe loses nothing") {
    // THE THIRD ARM, and it is the one that keeps the two above from being satisfied by an
    // implementation that simply forgets everything on every arrival. A re-ask that returns
    // the SAME catalog -- the ordinary case, and the only one that existed before this
    // phase -- leaves the row and the pick exactly where the maker left them.
    Live t;
    ToolSeat* tool = mount_tool(t, "alpha");
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"beta", "beta"});
    open_builder(t);
    t.key(input::scan::kC);
    REQUIRE(t.w->session().panels.builder.chosen == 1);
    REQUIRE(t.w->session().panels.builder.picked);

    recatalog(t, tool, {"alpha", "beta"});

    CHECK(t.w->session().panels.builder.chosen == 1);
    CHECK(t.w->session().panels.builder.picked);
    CHECK(t.w->session().panels.builder.known.recipes[1].recipe == "beta");
}

TEST_CASE("PROJ-1: an emptied catalog leaves no selection standing") {
    // THE DEGENERATE END OF THE SAME LAW. A valid catalog may name no recipes at all
    // (`builder::check_recipes` admits one deliberately), so a replacement can legitimately
    // leave this panel with nothing to choose between -- and `chosen` must not go on naming
    // a row that no longer exists in any sense.
    Live t;
    ToolSeat* tool = mount_tool(t, "alpha");
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"beta", "beta"});
    open_builder(t);
    t.key(input::scan::kC);
    REQUIRE(t.w->session().panels.builder.chosen == 1);

    recatalog(t, tool, {});

    CHECK(t.w->session().panels.builder.chosen == 0);
    CHECK_FALSE(t.w->session().panels.builder.picked);
    CHECK(t.w->session().panels.builder.known.recipes.empty());
    CHECK(stack_text(t.canvases.back()).find("no build recipes") != std::string::npos);
    tool->asked.clear();
    t.key(input::scan::kB);
    CHECK(tool->asked.empty());
}

TEST_CASE("BLD-1: `b` builds the recipe the maker chose, not the one last built") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"oven", "zengine-oven"});
    open_builder(t);

    t.key(input::scan::kC);
    t.key(input::scan::kB);
    REQUIRE(tool->asked.size() == 1);
    CHECK(tool->asked[0] == "oven");
    REQUIRE(tool->realize_asked.size() == 1);
    CHECK_FALSE(tool->realize_asked[0]);
    CHECK(t.w->session().notice.find("asked the Builder for `oven`") != std::string::npos);
    // A PLAIN BUILD SAYS NOTHING ABOUT REALIZING, and the panel is not waiting for
    // an answer it never asked for.
    CHECK(t.w->session().notice.find("realize") == std::string::npos);
    CHECK_FALSE(t.w->session().panels.builder.awaiting_realization);
}

TEST_CASE("BLD-1: `Shift+b` is BUILD & REALIZE, and the second intention crosses the seam") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    // THE STUB ANSWERS EVERY ASK AT ONCE, so a case that wants to watch a build has to
    // give it a condition a build can be IN -- exactly as the ASYNC-1 cases above do.
    tool->next.outcome = zengine::builder::outcome::kAsked;
    open_builder(t);

    t.key(input::scan::kB, input::mod::kShift);
    REQUIRE(tool->asked.size() == 1);
    CHECK(tool->asked[0] == "snake");
    REQUIRE(tool->realize_asked.size() == 1);
    CHECK(tool->realize_asked[0]);
    CHECK(t.w->session().notice.find("and to realize it") != std::string::npos);
    CHECK(t.w->session().panels.builder.awaiting_realization);

    // WORKSHOP GAINED NO POWER FOR IT. It said one sentence to one office; what
    // happens next belongs to two owners neither of which is here.
    CHECK(t.w->session().panels.builder.awaiting);
}

TEST_CASE("BLD-1: a build outcome and a realization outcome are TWO rows and TWO notices") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    tool->next.outcome = zengine::builder::outcome::kAsked;
    open_builder(t);
    t.key(input::scan::kB, input::mod::kShift);

    // THE BUILD ENDS FIRST, and it is announced first.
    tool->next.outcome = zengine::builder::outcome::kSucceeded;
    tool->next.realize = true;
    tool->next.realization = zengine::builder::realization::kOffered;
    tool->next.realized_detail = "offered to the project";
    t.publish(loom::to_value(tool->next));
    CHECK(t.w->session().notice == "built snake -- exit 0");
    CHECK_FALSE(t.w->session().panels.builder.awaiting);
    // ...and the panel is STILL WATCHING THE SECOND QUESTION, which is why the two
    // latches are two.
    CHECK(t.w->session().panels.builder.awaiting_realization);
    CHECK(stack_text(t.canvases.back()).find("offered") != std::string::npos);

    // THE PROJECT ANSWERS SEVERAL TURNS LATER, and that is news to a panel that
    // watched it begin.
    tool->next.realization = zengine::builder::realization::kRealized;
    tool->next.realized_detail = "weave #9 as zengine.oven";
    t.publish(loom::to_value(tool->next));
    CHECK(t.w->session().notice == "realized snake -- weave #9 as zengine.oven");
    CHECK_FALSE(t.w->session().panels.builder.awaiting_realization);
    const std::string shown = stack_text(t.canvases.back());
    CHECK(shown.find("realized -- weave #9 as zengine.oven") != std::string::npos);
    // BOTH OUTCOMES ARE STILL ON THE SCREEN. A maker never has to derive one from
    // the other.
    CHECK(shown.find("succeeded") != std::string::npos);
}

TEST_CASE("BLD-1: a build that WORKED whose realization was REFUSED says both") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    tool->next.outcome = zengine::builder::outcome::kAsked;
    open_builder(t);
    t.key(input::scan::kB, input::mod::kShift);

    tool->next.outcome = zengine::builder::outcome::kSucceeded;
    tool->next.realize = true;
    tool->next.realization = zengine::builder::realization::kOffered;
    t.publish(loom::to_value(tool->next));
    REQUIRE(t.w->session().notice == "built snake -- exit 0");

    tool->next.realization = zengine::builder::realization::kRefused;
    tool->next.realized_detail = "artifact 'snake': already part of this running project";
    t.publish(loom::to_value(tool->next));
    CHECK(t.w->session().notice.find("NOT REALIZED: snake") != std::string::npos);
    CHECK(t.w->session().notice.find("already part of") != std::string::npos);
    const std::string shown = stack_text(t.canvases.back());
    CHECK(shown.find("succeeded") != std::string::npos);   // the build is untouched
    CHECK(shown.find("REFUSED") != std::string::npos);     // ...and the project said no
}

TEST_CASE("BLD-1: when a failed build refuses realization, the CAUSE is the notice") {
    // ONE NOTICE LINE AND TWO FACTS THAT SETTLED TOGETHER. A maker needs the cause
    // ("BUILD FAILED") and not the consequence ("nothing was offered"), which the
    // panel's own rows carry anyway.
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    tool->next.outcome = zengine::builder::outcome::kAsked;
    open_builder(t);
    t.key(input::scan::kB, input::mod::kShift);

    tool->next.outcome = zengine::builder::outcome::kFailed;
    tool->next.status = 2;
    tool->next.realize = true;
    tool->next.realization = zengine::builder::realization::kRefused;
    tool->next.realized_detail = "the build failed, so nothing was offered to the project";
    t.publish(loom::to_value(tool->next));

    CHECK(t.w->session().notice == "BUILD FAILED: snake -- exit 2");
    // THE LATCH IS STILL RELEASED, so the derivative refusal is not announced later
    // as though it were news.
    CHECK_FALSE(t.w->session().panels.builder.awaiting_realization);
    CHECK(stack_text(t.canvases.back()).find("REFUSED") != std::string::npos);
}

TEST_CASE("BLD-1: a green build with no artifact is announced as neither success nor failure") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    tool->next.outcome = zengine::builder::outcome::kAsked;
    open_builder(t);
    t.key(input::scan::kB);

    tool->next.outcome = zengine::builder::outcome::kNoArtifact;
    tool->next.status = 0;
    tool->next.artifact = "zengine-oven";
    tool->next.detail = "the build succeeded and `zengine-oven` is not at /tmp/zengine-oven.so";
    t.publish(loom::to_value(tool->next));

    CHECK(t.w->session().notice.find("produced no `zengine-oven`") != std::string::npos);
    CHECK(t.w->session().notice.find("built") != 0u);
    CHECK(stack_text(t.canvases.back()).find("NO ARTIFACT") != std::string::npos);
}

TEST_CASE("BLD-1: a project with no recipes says so, and `b` asks for nothing") {
    Live t;
    auto seat = std::make_unique<ToolSeat>();
    ToolSeat* tool = seat.get();
    loom::Grant grant;
    grant.allow_to_any(zengine::builder::BuildStatus::zen_name,
                       zengine::builder::BuildStatus::zen_version);
    grant.allow_to_any(zengine::builder::RecipeCatalog::zen_name,
                       zengine::builder::RecipeCatalog::zen_version);
    const loom::WeaveId id = t.bus.register_weave(std::move(seat), std::move(grant),
                                                  std::string(zengine::builder::kBuilderRole));
    tool->zen_set_self(id);
    open_builder(t);

    CHECK(stack_text(t.canvases.back()).find("no build recipes") != std::string::npos);
    t.key(input::scan::kB);
    CHECK(tool->asked.empty());
    CHECK(t.w->session().notice.find("no build recipes") != std::string::npos);
    t.key(input::scan::kC);
    CHECK(t.w->session().notice.find("no build recipes to choose between") != std::string::npos);
}

TEST_CASE("BLD-1: `c` with no Builder panel open is an unbound key, exactly as `b` is") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    const std::string before = t.w->session().notice;
    t.key(input::scan::kC);
    CHECK(t.w->session().notice == before);
    CHECK(tool->described == 0);
    CHECK(tool->asked.empty());
}

// ---- BLD-2: the frontier is visible, joined, and actionable ------------------------
//
// The project's realization frontier reaches the Builder panel as a VALUE the weave
// derives from the host's live view at every paint and every gesture, and holds for
// exactly that long. The cases below are the phase's focused falsifiers: a copied or
// stale frontier, a silently chosen recipe, and a frontier action that bypasses the
// existing Build & Realize route each turn at least one of them red.

TEST_CASE("BLD-2: the Builder shows the frontier from the LIVING owner, and keeps no copy") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"oven", "zengine-oven"});

    // THE VIEW IS ALIVE: the fixture's lambda reads this local at every spend, exactly
    // as the host's reads the realization owner. Nothing is handed over but a function.
    ProjectFrontier live;
    t.host.frontier = [&live] { return live; };

    open_builder(t);
    // NO FRONTIER, NO ROW. The panel is byte-for-byte the ordinary Builder: absence of
    // a pending frontier is the whole answer, and no "all good" is manufactured.
    CHECK(stack_text(t.canvases.back()).find("project") == std::string::npos);

    // THE PROJECT STOPS AT A ROW. The next repaint -- an ordinary one, caused by an
    // ordinary gesture -- shows it, with the recipe join and the blocked count.
    live.waiting = true;
    live.artifact = "zengine-oven";
    live.blocked = 3;
    t.key(input::scan::kC);
    CHECK(stack_text(t.canvases.back()).find("waiting zengine-oven (oven, blocks 3)") !=
          std::string::npos);

    // THE OWNER MOVES; THE PANEL MOVES WITH IT, WITH NOBODY TOLD. A panel that copied
    // the frontier at the moment it appeared would still say `zengine-oven` here --
    // which is the exact mutation this case exists to redden.
    live.artifact = "zengine-later";
    live.blocked = 0;
    t.key(input::scan::kC);
    // The full row is `waiting zengine-later (no recipe, blocks 0)`; at the minimum
    // extent the panel is 48 cells and `detail::fit` marks the cut, so the assertion
    // holds the prefix that survives every extent.
    const std::string moved = stack_text(t.canvases.back());
    CHECK(moved.find("waiting zengine-later (no recipe") != std::string::npos);
    CHECK(moved.find("zengine-oven (oven") == std::string::npos);

    // ...AND WHEN THE FRONTIER RESOLVES, THE ROW LEAVES WITH IT. The third `said` row
    // comes back: the composition below the recipe row is BLD-1a's again.
    live = ProjectFrontier{};
    t.key(input::scan::kC);
    CHECK(stack_text(t.canvases.back()).find("waiting zengine-later") == std::string::npos);
    CHECK(stack_text(t.canvases.back()).find("project") == std::string::npos);
}

TEST_CASE("BLD-2: `f` builds and realizes the ONE recipe that produces the frontier") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"oven", "zengine-oven"});
    ProjectFrontier live;
    live.waiting = true;
    live.artifact = "zengine-oven";
    live.blocked = 2;
    t.host.frontier = [&live] { return live; };
    open_builder(t);

    // THE MAKER'S CURSOR IS SOMEWHERE ELSE, deliberately: the join is by artifact stem
    // against the tool's own catalog, never by whatever row the panel happened to be
    // on. A frontier action that spent `chosen` here would build `snake` -- the
    // wrong-recipe mutation, and this is the case that reddens it.
    REQUIRE(t.w->session().panels.builder.chosen == 0);
    t.key(input::scan::kF);
    REQUIRE(tool->asked.size() == 1);
    CHECK(tool->asked[0] == "oven");
    // ...AND IT IS THE EXISTING BUILD & REALIZE ROUTE, whole: the same
    // `BuildRequested` Shift+b says, with the maker's second intention aboard.
    REQUIRE(tool->realize_asked.size() == 1);
    CHECK(tool->realize_asked[0]);
    CHECK(t.w->session().panels.builder.awaiting_realization);
    CHECK(t.w->session().notice.find("asked the Builder for `oven`") != std::string::npos);
    CHECK(t.w->session().notice.find("and to realize it") != std::string::npos);
    // THE SELECTION MOVED WITH THE GESTURE, VISIBLY: the panel's recipe row now names
    // what was actually asked for, so `b` next does what the screen says.
    CHECK(t.w->session().panels.builder.chosen == 1);
    CHECK(stack_text(t.canvases.back()).find("oven -> zengine-oven") != std::string::npos);
}

TEST_CASE("BLD-2: several recipes produce the frontier -- `f` never chooses for the maker") {
    Live t;
    auto seat = std::make_unique<ToolSeat>();
    ToolSeat* tool = seat.get();
    loom::Grant grant;
    grant.allow_to_any(zengine::builder::BuildStatus::zen_name,
                       zengine::builder::BuildStatus::zen_version);
    grant.allow_to_any(zengine::builder::RecipeCatalog::zen_name,
                       zengine::builder::RecipeCatalog::zen_version);
    const loom::WeaveId id = t.bus.register_weave(std::move(seat), std::move(grant),
                                                  std::string(zengine::builder::kBuilderRole));
    tool->zen_set_self(id);
    // TWO RECIPES, ONE ARTIFACT -- authored law, not an edge case (`check_recipes`
    // deduplicates identities and deliberately not artifacts). The FIRST catalog row
    // matches the frontier, which is what makes this the sharp case: an
    // implementation that quietly spends the first match, or reads `chosen`'s default
    // of 0 as a choice, sends an ask here and goes red.
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"oven-a", "zengine-oven"});
    tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"oven-b", "zengine-oven"});
    tool->next.recipe = "oven-a";
    tool->next.artifact = "zengine-oven";
    ProjectFrontier live;
    live.waiting = true;
    live.artifact = "zengine-oven";
    live.blocked = 1;
    t.host.frontier = [&live] { return live; };
    open_builder(t);

    // THE PANEL COUNTS THE CHOICES rather than silently showing one of them.
    CHECK(stack_text(t.canvases.back()).find("(2 recipes") != std::string::npos);

    // NO PICK YET: `chosen == 0` is an index, not a choice. `f` refuses, names the
    // candidates, and asks nothing of anybody.
    REQUIRE(t.w->session().panels.builder.chosen == 0);
    t.key(input::scan::kF);
    CHECK(tool->asked.empty());
    CHECK(t.w->session().notice.find("2 recipes produce `zengine-oven`") != std::string::npos);
    CHECK(t.w->session().notice.find("`oven-a`, `oven-b`") != std::string::npos);
    CHECK(t.w->session().notice.find("pick one with c") != std::string::npos);

    // THE MAKER PICKS -- the one gesture that makes a selection theirs -- and `f`
    // spends exactly that pick.
    t.key(input::scan::kC);
    REQUIRE(t.w->session().panels.builder.chosen == 1);
    t.key(input::scan::kF);
    REQUIRE(tool->asked.size() == 1);
    CHECK(tool->asked[0] == "oven-b");
    REQUIRE(tool->realize_asked.size() == 1);
    CHECK(tool->realize_asked[0]);

    // ...INCLUDING THE FIRST ROW, once it is genuinely picked: the refusal above was
    // about the default, never about the row.
    t.key(input::scan::kC);
    REQUIRE(t.w->session().panels.builder.chosen == 0);
    t.key(input::scan::kF);
    REQUIRE(tool->asked.size() == 2);
    CHECK(tool->asked[1] == "oven-a");
}

TEST_CASE("BLD-2: `f` refuses in words when nothing is waiting, and when nothing can build "
          "the frontier") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    open_builder(t);

    // NO VIEW WIRED IS "NOT WAITING": the fixture's default, and any host that wired
    // no realization owner. Nothing is asked of the tool.
    t.key(input::scan::kF);
    CHECK(tool->asked.empty());
    CHECK(t.w->session().notice.find("not waiting on any artifact") != std::string::npos);

    // A FRONTIER NOTHING HERE CAN PRODUCE is a different sentence: the join over the
    // tool's own catalog came back empty, and the gesture says so instead of guessing.
    ProjectFrontier live;
    live.waiting = true;
    live.artifact = "zengine-mystery";
    t.host.frontier = [&live] { return live; };
    t.key(input::scan::kF);
    CHECK(tool->asked.empty());
    CHECK(t.w->session().notice.find("no authored recipe produces `zengine-mystery`") !=
          std::string::npos);
}

TEST_CASE("BLD-2: `f` with no Builder panel open is an unbound key, exactly as `b` is") {
    Live t;
    ToolSeat* tool = mount_tool(t, "snake");
    ProjectFrontier live;
    live.waiting = true;
    live.artifact = "snake";
    t.host.frontier = [&live] { return live; };
    const std::string before = t.w->session().notice;
    t.key(input::scan::kF);
    CHECK(t.w->session().notice == before);
    CHECK(tool->described == 0);
    CHECK(tool->asked.empty());
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

    // WITHOUT A PANEL, the screen carries no stacked rows; the picker's own gesture is
    // said by the band's legend and the hotkey view since WUX-1, not by a row-0 hint.
    const surface::SurfaceCanvas bare = t.canvases.back();
    CHECK(stack_text(bare).empty());

    open_builder(t);
    const surface::SurfaceCanvas with = t.canvases.back();
    const Screen sc = screen_of(t.session());
    // THE BOUNDS THE PLACEMENT PATH GIVES IT, and the rows are read against those
    // rather than against a column this case knows independently. The rows are a region's
    // since WUX-1, so they are read through the cell projection every character medium
    // draws with.
    const ui::Rect stack =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc).rect);
    const ui::Rect inside = pane_body_cells(stack);
    std::size_t stacked_rows = 0;
    for (const surface::SurfaceLabel& l : cell_text_of(with)) {
        if (l.x == inside.x && l.y >= inside.y && l.y < inside.y + inside.h) {
            // Every stacked row is padded to the panel's own INTERIOR width, so in a
            // character medium the spaces erase the workspace under it rather
            // than punching holes through to it -- and the boundary around them is the
            // backdrop rect, which erases the rest (WUX-5).
            ++stacked_rows;
            CHECK(l.text.size() == static_cast<std::size_t>(inside.w));
            CHECK(stack.x + stack.w <= sc.panel_x);
        }
    }
    CHECK(stacked_rows == static_cast<std::size_t>(inside.h));
    // The OBJECTS and PROPERTIES columns are untouched by any of this.
    CHECK(inspector_row(with, sc.panel_x + kChromeCells, kSideY + kChromeCells) == "OBJECTS");
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
    // AND THE TWO PLACES PARTITION THE BUILT-IN CATALOG (WG-0). This used to read
    // `kinds_placed_in(kOverlayStack) == 1`, which is a census of the place whose whole
    // purpose is to hold several -- so it reddened for any kind added to the stack while
    // saying nothing about a law. The law is that every COMPILE-TIME kind declares one of the
    // two: a row whose `placed_in` is neither is counted by nobody and silently resolved by
    // `placement_bounds`'s fall-through, which is the stack's rectangle under another name.
    // Measured against current source: a catalog row declaring a third place value reddens
    // this. WG-0 recorded "and nothing else" and that half is no longer true -- since WP-0 a
    // kind outside the stack takes no SLOT in `seat_panes`, so the same mutant also moves the
    // capacity and waiting cases. The law still catches what it names; it is simply no longer
    // the only thing watching `placed_in`.
    //
    // IT IS ABOUT `placed_in` AND THEREFORE ABOUT ROWS, NOT ABOUT EVERY KIND (WP-0). A runtime
    // pane has no catalog row to declare anything in: `placement_of` branches on
    // `is_runtime_kind` BEFORE it reaches `panel_kind`, so an external pane is placed in the
    // stack by Workshop and asks for nothing. That branch is the WP-0 tier's claim (`an
    // unknown runtime reference never becomes the Builder`); `kinds_placed_in` walks
    // `kPanelCatalog` and could not see it.
    CHECK(kinds_placed_in(placement::kSideRegion) + kinds_placed_in(placement::kOverlayStack) +
              kinds_placed_in(placement::kTopBand) ==
          kPanelKinds);

    // THE BOUNDS ARE RESOLVED AGAINST A SCREEN, and they are the two places
    // Workshop has always had: the column beside the workspace, and the top of
    // the workspace itself.
    const ui::Rect side = placement_bounds(placement::kSideRegion, 0, kMinScreen);
    const ui::Rect stack = placement_bounds(placement::kOverlayStack, 0, kMinScreen);
    CHECK(side == ui::Rect{50, 2, 28, 16});
    CHECK(stack == ui::Rect{0, 2, 48, 9});

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

    const PanelBounds info = bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc);
    const PanelBounds builder = bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc);
    REQUIRE(info.open);
    REQUIRE(builder.open);
    CHECK(info.placed_in == placement::kSideRegion);
    CHECK(builder.placed_in == placement::kOverlayStack);

    // INFO'S REGION COMES FROM THE PATH: its two headings are at the rectangle it
    // was handed, on the rows it keeps inside that rectangle. The panels are
    // cell-aligned here, so the covered-cell reading is the exact one.
    // ...AND EACH PANEL'S ROWS ARE INSIDE ITS OWN CHROME (WUX-5): the rectangle the path
    // hands it is unchanged, and the boundary it now draws is subtracted from it once.
    const ui::Rect info_cells = pane_body_cells(info.rect);
    const ui::Rect builder_cells = pane_body_cells(builder.rect);
    CHECK(inspector_row(c, info_cells.x, info_cells.y) == "OBJECTS");
    CHECK(properties_heading(c, t.doc(), t.session()) == "PROPERTIES");
    // BUILDER'S REGION COMES FROM THE PATH, at the first slot of the stack.
    CHECK(label_at(c, builder_cells.x, builder_cells.y).find("BUILDER") == 0);
    CHECK(builder.rect == fine_of_cells(placement_bounds(placement::kOverlayStack, 0, sc)));

    // AND NEITHER PANEL PAINTS OUTSIDE ITS OWN BOUNDS. Every label in a panel's
    // column is on one of that panel's rows, and every row of the stacked panel
    // is padded to the width its bounds gave it -- which is the erasing mechanism
    // following the panel rather than a constant.
    std::size_t seen_info = 0;
    std::size_t seen_stack = 0;
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == info_cells.x) {
            CHECK(l.y >= info_cells.y);
            CHECK(l.y < info_cells.y + info_cells.h);
            ++seen_info;
        }
        if (l.x == builder_cells.x && l.y >= builder_cells.y &&
            l.y < builder_cells.y + builder_cells.h) {
            CHECK(l.text.size() == static_cast<std::size_t>(builder_cells.w));
            ++seen_stack;
        }
    }
    CHECK(seen_info > 0);
    CHECK(seen_stack == static_cast<std::size_t>(builder_cells.h));
}

TEST_CASE("a closed panel is not anywhere") {
    // A PANEL THAT IS NOT OPEN HAS NO BOUNDS, and the empty rectangle is the
    // deliberate answer rather than the first slot's: a caller that forgets to
    // ask gets a rectangle that contains nothing, not one that contains the place
    // this panel WOULD have had.
    Live t;
    const Screen sc = screen_of(t.session());
    const PanelBounds absent = bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, sc);
    CHECK_FALSE(absent.open);
    CHECK(absent.rect == FineRect{});
    CHECK_FALSE(cells_covered(absent.rect).contains(0, 1));
    // Its KIND still has a declared place, because that is a fact about the
    // catalog rather than about this session.
    CHECK(absent.placed_in == placement::kOverlayStack);
    // The place it would have had is perfectly well defined -- what is absent is
    // the PANEL, not the place.
    CHECK(placement_bounds(placement::kOverlayStack, 0, sc).contains(0, kStackY));
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
    CHECK(bounds_of(info_first, setup_for(info_first), panel::kBuilder, sc).rect == fine_of_cells(first_slot));
    CHECK(bounds_of(builder_first, setup_for(builder_first), panel::kBuilder, sc).rect == fine_of_cells(first_slot));
    // And Info is in the same column either way: the side region has no slots.
    CHECK(bounds_of(info_first, setup_for(info_first), panel::kInfo, sc).rect ==
          bounds_of(builder_first, setup_for(builder_first), panel::kInfo, sc).rect);
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
    const ui::Rect moved_body = pane_body_cells(moved_info);
    surface::SurfaceCanvas ic;
    paint_info(plane(ic), d, s, fine_of_cells(moved_info), kMinScreen);
    CHECK(inspector_row(ic, moved_body.x, moved_body.y) == "OBJECTS");
    // The heading is a ROW of the body since HD-7, so it moves with the rectangle too.
    const InfoBodyPlace moved = info_body_place(moved_info, kMinScreen, d, s);
    REQUIRE(moved.present);
    CHECK(inspector_row(ic, moved.region_x,
                        moved.region_y + kInfoHeadingRows + moved.heading_row) == "PROPERTIES");
    // AND ITS BACKDROP GOES WITH IT (PNL-2a). This assertion used to read
    // `ic.rects.empty()` -- Info had no backdrop at all, pinned deliberately by
    // PNL-1 and named by PNL-2 as the case that would have to change if anybody
    // ever gave it one. Somebody did. It is one rect, it is the whole of the
    // rectangle this painter was HANDED, and it is not the rectangle this kind
    // would have resolved to -- so the backdrop is owned by the placement path
    // exactly as much as the labels are.
    REQUIRE(all_rects(ic).size() == 1);
    CHECK(all_rects(ic)[0].x == moved_info.x);
    CHECK(all_rects(ic)[0].y == moved_info.y);
    CHECK(all_rects(ic)[0].w == moved_info.w);
    CHECK(all_rects(ic)[0].h == moved_info.h);
    CHECK(all_rects(ic)[0].role == kPaneChrome);
    for (const surface::SurfaceLabel& l : cell_text_of(ic)) {
        CHECK(l.x == moved_body.x);
        CHECK(l.y >= moved_body.y);
        CHECK(l.y < moved_body.y + moved_body.h);
    }

    BuilderPane pane;
    pane.heard = true;
    pane.shown.recipe = "zengine-snake";
    const ui::Rect moved_stack{4, 6, 30, kStackRows};
    const ui::Rect moved_stack_body = pane_body_cells(moved_stack);
    surface::SurfaceCanvas bc;
    paint_builder(plane(bc), pane, fine_of_cells(moved_stack), kMinScreen);
    CHECK(label_at(bc, moved_stack_body.x, moved_stack_body.y).find("BUILDER") == 0);
    REQUIRE(all_rects(bc).size() == 1);
    CHECK(all_rects(bc)[0].x == moved_stack.x);
    CHECK(all_rects(bc)[0].y == moved_stack.y);
    CHECK(all_rects(bc)[0].w == moved_stack.w);
    CHECK(all_rects(bc)[0].h == moved_stack.h);
    for (const surface::SurfaceLabel& l : cell_text_of(bc)) {
        CHECK(l.x == moved_stack_body.x);
        CHECK(l.y >= moved_stack_body.y);
        CHECK(l.y < moved_stack_body.y + moved_stack_body.h);
        // Fitted and padded to the INTERIOR of the bounds it was handed and not to the
        // stack's own width, so a narrower panel erases exactly what it covers.
        CHECK(l.text.size() == static_cast<std::size_t>(moved_stack_body.w));
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

TEST_CASE("WIND-1: the side region keeps its reservation and the stack takes half the surplus") {
    // THE TWO PLACES ANSWER THE EXTENT QUESTION DIFFERENTLY, and the path is where that
    // difference lives: the region is anchored to the right edge and keeps its width; the
    // stack is anchored to the top-left corner, keeps its rows, and takes HALF of whatever
    // surplus the room has over the composition it was written for (WIND-1).
    //
    // THIS CASE USED TO SAY THE STACK KEPT EVERYTHING, and that sentence is now false. What
    // replaces it is not a bigger number but a LAW -- kStackW + (room_w - kStackW)/2 --
    // stated over the whole clamped width domain, because a table of six extents cannot tell
    // a half-share from any other curve through the same six points.
    const Screen big = screen_of(100, 30);
    const ui::Rect side = placement_bounds(placement::kSideRegion, 0, big);
    const ui::Rect stack = placement_bounds(placement::kOverlayStack, 0, big);
    CHECK(side.x == big.panel_x);
    CHECK(side.w == kPanelCols);
    CHECK(side.x + side.w == big.w);
    CHECK(side.h > placement_bounds(placement::kSideRegion, 0, kMinScreen).h);
    // The side region is byte-identical to the minimum screen's, WIDTH included -- which is
    // the half of the old sentence that stayed true.
    CHECK(side.w == placement_bounds(placement::kSideRegion, 0, kMinScreen).w);
    // The stack is not. Its column, its rows and its height are; its width followed the
    // room: 100 columns of surface is a room of 70, a surplus of 22, and half of that is 11.
    const ui::Rect min_stack = placement_bounds(placement::kOverlayStack, 0, kMinScreen);
    CHECK(stack.x == min_stack.x);
    CHECK(stack.y == min_stack.y);
    CHECK(stack.h == min_stack.h);
    CHECK(stack.w == 59);
    CHECK(stack.w > min_stack.w);
    CHECK(stack.x + stack.w <= side.x);

    // THE LAW, OVER EVERY WIDTH THIS COMPOSITION LAYS OUT, at three heights so that a
    // height creeping into the width would be named here rather than discovered later.
    for (std::int64_t w = kScreenMinW; w <= kScreenMaxW; ++w) {
        for (const std::int64_t h : {kScreenMinH, std::int64_t{60}, kScreenMaxH}) {
            CAPTURE(w);
            CAPTURE(h);
            const Screen sc = screen_of(w, h);
            const ui::Rect b = placement_bounds(placement::kOverlayStack, 0, sc);
            CHECK(b.w == kStackW + (sc.room_w - kStackW) / 2);
            CHECK(b.x == kStackX);
            CHECK(b.h == kStackRows);
            // NEVER PAST THE ROOM, and this is HD-10's reserved-column law asked of the
            // other overlay: what is beside the workspace is nobody's to spend.
            CHECK(b.x + b.w <= sc.room_w);
            CHECK(b.x + b.w <= sc.panel_x - kPanelGap);
            CHECK(b.x + b.w <= placement_bounds(placement::kSideRegion, 0, sc).x - kPanelGap);
            // AND THE MAKER KEEPS THE OTHER HALF. This is the law that would silently die
            // if a later phase simplified the expression to sc.room_w: wherever the room
            // has any surplus at all, a column of the panel's own rows is still the
            // maker's to press.
            if (sc.room_w > kStackW) {
                CHECK(b.x + b.w < sc.room_w);
            }
            // Never narrower than the composition it was written for, either.
            CHECK(b.w >= kStackW);
        }
    }

    // THE EXACT ANSWERS, and the free columns each leaves in the panel's own rows. The
    // 79-column row is the control that tells FLOOR from CEILING: a room of 49 is a surplus
    // of exactly one, and the odd column stays the maker's.
    struct Witness {
        std::int64_t w;
        std::int64_t h;
        std::int64_t room;
        std::int64_t width;
        std::int64_t free;
    };
    for (const Witness& row : std::vector<Witness>{{78, 22, 48, 48, 0},
                                                   {79, 22, 49, 48, 1},
                                                   {96, 22, 66, 57, 9},
                                                   {120, 40, 90, 69, 21},
                                                   {200, 60, 170, 109, 61},
                                                   {640, 400, 610, 329, 281}}) {
        CAPTURE(row.w);
        CAPTURE(row.h);
        const Screen sc = screen_of(row.w, row.h);
        const ui::Rect b = placement_bounds(placement::kOverlayStack, 0, sc);
        CHECK(sc.room_w == row.room);
        CHECK(b.w == row.width);
        CHECK(sc.room_w - (b.x + b.w) == row.free);
    }
    // ...and rounding the half UP would put 49 on the 79-column row and leave the maker
    // nothing. Said as its own comparison so that mutation has a line to red on.
    CHECK(placement_bounds(placement::kOverlayStack, 0, screen_of(79, 22)).w !=
          kStackW + (screen_of(79, 22).room_w - kStackW + 1) / 2);
}

TEST_CASE("WIND-1: the minimum composition is byte-identical, and a width buys no slot") {
    // THE PRICE OF THE HALF-SHARE AT THE BOTTOM OF THE RANGE IS ZERO. At 78x22 the room IS
    // kStackW, the surplus is nothing, and every number the composition was written with is
    // exactly what it was -- which is why nothing else in this suite moved.
    CHECK(kMinStack == ui::Rect{0, 2, 48, 9});
    CHECK(placement_bounds(placement::kOverlayStack, 0, kMinScreen) == ui::Rect{0, 2, 48, 9});
    CHECK(kMinStack.x + kMinStack.w == kMinScreen.room_w);
    CHECK(kMinStack.x + kMinStack.w <= kMinSide.x - kPanelGap);
    CHECK(kMinStack.y + kMinStack.h <= kMinScreen.notice_y);

    // A WIDTH IS NOT A HEIGHT. `stack_slots_that_fit` reads y and h and nothing else, so a
    // panel that grew sideways must not have bought room for a panel underneath it: at
    // every height, the narrowest screen and the widest agree exactly.
    for (std::int64_t h = kScreenMinH; h <= 60; ++h) {
        CAPTURE(h);
        const std::size_t narrow = stack_slots_that_fit(screen_of(kScreenMinW, h));
        for (const std::int64_t w :
             {std::int64_t{79}, std::int64_t{120}, std::int64_t{200}, kScreenMaxW}) {
            CAPTURE(w);
            const Screen wide = screen_of(w, h);
            CHECK(stack_slots_that_fit(wide) == narrow);
            CHECK(stack_capacity(wide).slots == narrow);
        }
    }
    // And the capacity IS still driven by the rows: three more of them is the second slot.
    CHECK(stack_slots_that_fit(screen_of(kScreenMinW, kScreenMinH)) == 1);
    CHECK(stack_slots_that_fit(screen_of(kScreenMaxW, kScreenMinH)) == 1);
    CHECK(stack_slots_that_fit(screen_of(kScreenMaxW, kScreenMinH + 3)) == 2);

    // EVERY SLOT ON ONE SCREEN IS THE SAME WIDTH, because the width is a fact about the
    // room and the slot only names a row. A width that varied by slot would be a layout
    // policy arriving as a side effect.
    const Screen tall = screen_of(200, 60);
    for (std::size_t slot = 0; slot < 4; ++slot) {
        CAPTURE(slot);
        const ui::Rect b = placement_bounds(placement::kOverlayStack, slot, tall);
        CHECK(b.w == placement_bounds(placement::kOverlayStack, 0, tall).w);
        CHECK(b.x == kStackX);
        CHECK(b.y == kStackY + static_cast<std::int64_t>(slot) * (kStackRows + kStackGap));
    }
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

    // A FRESH SESSION HAS IT OPEN, beside the Layouts pane the layout run became (WUX-12)
    // -- and those two are the whole of what a fresh Workshop opens.
    const Panels& panels = t.w->session().panels;
    REQUIRE(panels.open.size() == 2);
    CHECK(panels.open[0].kind == panel::kInfo);
    CHECK(panels.open[1].kind == panel::kLayouts);
    CHECK(panels.has(panel::kInfo));
    CHECK_FALSE(panels.has(panel::kBuilder));

    // ...and the screen a maker boots into is the screen they have always booted
    // into: the two column headings, in the column they have always been in.
    const Screen sc = screen_of(t.session());
    const surface::SurfaceCanvas& c = t.canvases.back();
    CHECK(inspector_row(c, sc.panel_x + kChromeCells, kSideY + kChromeCells) == "OBJECTS");
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
    CHECK(inspector_row(gone, sc.panel_x, kSideY).empty());
    // THE HEADING IS NOWHERE ON THE CANVAS, asked of the whole picture rather than of the
    // row the panel would have put it on: since QR-14 that row is inside the workspace, so
    // a row-addressed question would be asking the document what the panel says.
    for (const std::string& row : rasterized(gone)) {
        CHECK(row.find("PROPERTIES") == std::string::npos);
    }

    // AND NOTHING ELSE MOVED. The screen around the hole is the screen it was:
    // the band's workspace fact and the help lines are exactly where they were, because
    // removing a panel is not a re-layout.
    CHECK(workspace_row(gone, t.session(), sc) == "workspace 48x16 cells");
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
    for (const surface::SurfaceRect& r : all_rects(t.canvases.back())) {
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
    CHECK(workspace_row(t.canvases.back(), t.session(), screen_of(t.session())) ==
          "workspace 48x16 cells");

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
    CHECK(workspace_row(t.canvases.back(), t.session(), screen_of(t.session())) ==
          "workspace 48x16 cells");

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
    CHECK(stack_text(t.canvases.back()).find(detail::pad("Builder", kPickerNameCols) + "closed") != std::string::npos);
    CHECK(stack_text(t.canvases.back()).find(detail::pad("Info", kPickerNameCols) + "open") != std::string::npos);
    t.key(input::scan::kEscape);

    open_builder(t);
    pick(t, panel::kInfo);
    t.key(input::scan::kP);
    CHECK(stack_text(t.canvases.back()).find(detail::pad("Builder", kPickerNameCols) + "open") != std::string::npos);
    CHECK(stack_text(t.canvases.back()).find(detail::pad("Info", kPickerNameCols) + "closed") != std::string::npos);
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
    // THE SLOT'S INTERIOR (WUX-5): the picker's rows are inside its own boundary, and the
    // boundary itself is the backdrop rect -- which is what covers the rest of the slot.
    const ui::Rect slot =
        pane_body_cells(placement_bounds(placement::kOverlayStack, 0, screen_of(t.session())));
    for (std::int64_t row = 0; row < slot.h; ++row) {
        const std::string top = topmost_at(c, slot.x, slot.y + row);
        CHECK(top.size() == static_cast<std::size_t>(slot.w));
        visible += top;
        visible += '\n';
    }
    CHECK(visible.find("+ PANEL") != std::string::npos);
    CHECK(visible.find(detail::pad("Builder", kPickerNameCols) + "open") != std::string::npos);
    // Not one row of the panel underneath survives. (`asks` and not `recipe`: since
    // KEY-0 the picker's own Builder row says `build a chosen recipe`, so that word
    // stopped being panel-unique; `asks N ever` is the exit row's and only the panel's.)
    CHECK(visible.find("asks") == std::string::npos);
    CHECK(visible.find("BUILDER") == std::string::npos);
    CHECK(visible.find("Build ]") == std::string::npos);

    // AND THE CANARY: dismissing the picker gives the panel back whole.
    t.key(input::scan::kEscape);
    CHECK(stack_text(t.canvases.back()).find("recipe") != std::string::npos);
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
    CHECK(cells.capacity == static_cast<std::size_t>(cells.region_h - kInfoHeadingRows));
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
                                   s.text_line_px) -
              static_cast<std::size_t>(kInfoHeadingRows));
    CHECK(type.value_columns > cells.value_columns);

    // AND THE FACE'S OWN LINE IS WHAT DECIDES: a taller line is fewer rows, at the same
    // bounds, with nothing else moving. Half the rows, to within the one row an integer
    // division can leave behind -- the equation is the assertion above and this is its shape.
    s.text_line_px = 36;
    const InfoBodyPlace taller = body_of(d, s);
    CHECK(taller.capacity ==
          static_cast<std::size_t>((taller.region_h * surface::kCanvasCellPx -
                                    2 * surface::kTextInsetPx) /
                                   s.text_line_px) -
              static_cast<std::size_t>(kInfoHeadingRows));
    // Both capacities carry the heading's reservation off the top (WUX-1), so the halving
    // is exact to within the division's own row plus the one heading row counted twice on
    // the doubled side.
    CHECK(taller.capacity * 2 <= type.capacity);
    CHECK(taller.capacity * 2 + 1 + static_cast<std::size_t>(kInfoHeadingRows) >=
          type.capacity);
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
    // WUX-5 took two more rows: the panel's visible boundary is one cell on every side, so
    // both media resolve their budget inside a rectangle two cells shorter.
    // QR-14 took one more: the side region begins under the top band now, so it is exactly
    // as tall as the workspace rather than one row taller.
    REQUIRE(ct.capacity == 13); // thirteen cell rows of body at the minimum screen
    REQUIRE(cs.capacity == 8);  // (15 * 12 - 4) / 18, less the heading
    // HD-8's footer comes off both budgets, so both shares are two rows smaller than HD-6
    // measured them and the CELL body no longer seats all eight properties either. What the
    // case is about is unmoved: two media, two capacities, one set of semantic rows.
    REQUIRE(ct.properties_rows == 5);
    REQUIRE(cs.properties_rows == 3);
    REQUIRE(tui.rows.size() == 8);

    // THE TERMINAL SHOWS FIVE AND SAYS SO. Before HD-8 it showed all eight and said nothing;
    // the rows it lost are HD-8's two pressable ones and QR-14's one cell of column, and
    // `share_body_rows` decides which list pays -- the larger claim does.
    CHECK(ct.properties.count == 4);
    CHECK(ct.properties.before == 0);
    CHECK(ct.properties.after == 4);

    // THE WINDOW SHOWS THREE AND SAYS SO -- the marker spends one of the four rows, which is
    // `list_window`'s rule and not a second one.
    CHECK(cs.properties.count == 2);
    CHECK(cs.properties.before == 0);
    CHECK(cs.properties.after == 6);

    const surface::SurfaceCanvas tui_canvas = paint(d, tui);
    const surface::SurfaceCanvas sdl_canvas = paint(d, sdl);
    const surface::SurfaceTextRegion* tui_body = body_on(tui_canvas, ct);
    const surface::SurfaceTextRegion* sdl_body = body_on(sdl_canvas, cs);
    REQUIRE(tui_body != nullptr);
    REQUIRE(sdl_body != nullptr);
    // The region carries BOTH lists since HD-7, the FOOTER since HD-8 and the `OBJECTS`
    // heading since WUX-1, and the footer is anchored to the foot -- so the body now
    // publishes exactly the heading plus its capacity in every medium, with the spare room
    // written as the blank rows it is.
    const std::size_t heading = static_cast<std::size_t>(kInfoHeadingRows);
    CHECK(tui_body->rows.size() == heading + ct.capacity);
    CHECK(sdl_body->rows.size() == heading + cs.capacity);
    CHECK(tui_body->rows[heading + static_cast<std::size_t>(ct.action_row)].text ==
          "[ Create ]");
    CHECK(sdl_body->rows.back().text == "[ Delete ]");
    // ...and the omission marker is still the last thing the property list says, one row
    // under the properties rather than at the end of the region.
    CHECK(sdl_body->rows[heading + static_cast<std::size_t>(cs.heading_row) +
                         cs.properties_rows]
              .text == "... 6 more");

    // SAME PROPERTY FACTS, in the same order, for the rows both are showing. The value
    // columns differ because the media differ; the property does not.
    for (std::size_t i = 0; i < cs.properties.count; ++i) {
        CAPTURE(i);
        const auto& tr =
            tui_body->rows[heading + static_cast<std::size_t>(prose_row_of_property(ct, i))];
        const auto& sr =
            sdl_body->rows[heading + static_cast<std::size_t>(prose_row_of_property(cs, i))];
        CHECK(tr.text == property_row_text(tui.rows[i], i == tui.cursor, ct.value_columns));
        CHECK(sr.text == property_row_text(sdl.rows[i], i == sdl.cursor, cs.value_columns));
        CHECK(tr.text.substr(0, 10) == sr.text.substr(0, 10));
    }

    // AND NO ROW IS PUBLISHED THAT THE BODY CANNOT HOLD, in either medium (§11).
    CHECK(tui_body->rows.size() <= heading + ct.capacity);
    CHECK(sdl_body->rows.size() <= heading + cs.capacity);
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
    const ui::Rect panel =
cells_covered(bounds_of(s.panels, s.setup.active, panel::kInfo, sc).rect);
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
    CHECK(shown->y + shown->h == panel.y + panel.h - kChromeCells);
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
    REQUIRE(body_of(d, s).properties_rows == 3); // less the footer's two and the boundary's

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
    for (const Want& w : {Want{0, 0, 6, ">Identity #1", "... 6 more"},
                          Want{3, 3, 4, "... 3 earlier", "... 4 more"},
                          Want{4, 4, 3, "... 4 earlier", "... 3 more"},
                          Want{7, 6, 0, "... 6 earlier", ">Resolved 28 x 6 cells"}}) {
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
        // HD-8: the footer is anchored to the foot; WUX-1: the `OBJECTS` heading rides
        // above the body's capacity.
        const std::size_t heading = static_cast<std::size_t>(kInfoHeadingRows);
        CHECK(shown->rows.size() == heading + body.capacity);
        CHECK(shown->rows[heading + static_cast<std::size_t>(body.heading_row) + 1].text ==
              w.first_row);
        // THE LAST ROW OF THE PROPERTY RUN, which is no longer the last row of the region --
        // the footer is under it. Asked for by the run's own arithmetic rather than by
        // `back()`, so this case cannot come to be about the control rows by accident.
        CHECK(shown->rows[heading + static_cast<std::size_t>(body.heading_row) +
                          body.properties_rows]
                  .text == w.last_row);

        // EVERY MARKER IS A COUNT AND A DIRECTION, and it comes out of the row budget -- the
        // PROPERTY list's share of it since HD-7, which is the number the markers are bounded
        // by and the number `list_window` was handed.
        const std::size_t first = heading + static_cast<std::size_t>(body.heading_row) + 1;
        if (w.before > 0) {
            CHECK(shown->rows[first].text == omitted_text(w.before, "earlier"));
            CHECK(shown->rows[first].role == surface::role::kMuted);
        }
        if (w.after > 0) {
            const std::size_t last = heading + static_cast<std::size_t>(body.heading_row) +
                                     body.properties_rows;
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
    CHECK(body_place(t).properties_rows == 3); // less the footer's two and the boundary's

    // DOWN through the whole population, one row at a time.
    std::vector<std::string> reached;
    for (std::size_t step = 0; step < t.session().rows.size(); ++step) {
        const InfoBodyPlace body = body_place(t);
        CAPTURE(t.session().cursor);
        CHECK(prose_row_of_property(body, t.session().cursor) != kNoProseRow);
        reached.push_back(t.session().rows[t.session().cursor].label());
        const surface::SurfaceTextRegion* shown = body_on(t.canvases.back(), body);
        REQUIRE(shown != nullptr);
        CHECK(shown->rows.size() <= static_cast<std::size_t>(kInfoHeadingRows) + body.capacity);
        // THE ROW THE CURSOR IS ON CARRIES THE MARK, wherever the window put it.
        CHECK(shown->rows[static_cast<std::size_t>(
                              kInfoHeadingRows +
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
    CHECK(shown->caret_row == kInfoHeadingRows + editing_prose_row(t, refused));
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

    // AND NO TWO ROWS OVERLAP AT ANY OF THEM: the body publishes one row per prose row,
    // the heading riding above the capacity (WUX-1).
    const surface::SurfaceTextRegion* shown = body_on(t.canvases.back(), back);
    REQUIRE(shown != nullptr);
    CHECK(shown->rows.size() == static_cast<std::size_t>(kInfoHeadingRows) + back.capacity);
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
    // Two columns narrower than before WUX-5: the panel's boundary is one cell on each side.
    REQUIRE(body.value_columns == 15);
    const std::size_t vc = static_cast<std::size_t>(body.value_columns);

    // SHORT: unchanged, and not even fitted -- `detail::fit` returns a value that fits
    // byte-for-byte, marker included.
    CHECK(property_row_text(s.rows[1], false, body.value_columns) == " Name     panel");

    // EXACTLY THE WIDTH: still whole. The mark begins one character later than that.
    d.elements[0].label = std::string(vc, 'a');
    refocus(d, s);
    CHECK(property_row_text(s.rows[1], false, body.value_columns) ==
          " Name     " + std::string(vc, 'a'));
    d.elements[0].label = std::string(vc + 1, 'a');
    refocus(d, s);
    CHECK(property_row_text(s.rows[1], false, body.value_columns) ==
          " Name     " + std::string(vc - 3, 'a') + "...");

    // OVER-WIDTH: the established marker, and the whole row still fits the body.
    d.elements[0].label = "the-quick-brown-fox-jumps-over-5";
    refocus(d, s);
    const std::string row = property_row_text(s.rows[1], false, body.value_columns);
    CHECK(row == " Name     the-quick-br...");
    CHECK(static_cast<std::int64_t>(row.size()) <= body.columns);
    const surface::SurfaceCanvas painted = paint(d, s);
    const surface::SurfaceTextRegion* shown = body_on(painted, body);
    REQUIRE(shown != nullptr);
    // The cursor opens on Name, so the row a maker SEES carries the mark; the fitting is
    // the same either way, which is the half this case is about.
    const std::size_t name_row =
        static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_property(body, 1));
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
    // face, so `plan_layer_regions` sets it in type and `plan_caret` puts a `kCaretWidthPx`
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
    const std::vector<surface::PlanTextRegion> plan = plan_regions_of(
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
    CHECK(drawn->caret.y ==
          drawn->origin_y + (kInfoHeadingRows + editing_prose_row(t, body)) * 18);

    // AND A CHARACTER MEDIUM STILL ANSWERS WITH THE GLYPH, at the same prose position: two
    // projections of one published fact.
    const std::vector<surface::ProjectedRow> cells = projected_of(without_workspace(c));
    const std::string& row =
        cells[static_cast<std::size_t>(kInfoHeadingRows + editing_prose_row(t, body))]
            .label.text;
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
    const ui::Rect panel =
cells_covered(bounds_of(s.panels, s.setup.active, panel::kInfo, screen_of(s)).rect);

    // A LINE TALLER THAN THE WHOLE PANEL: seventeen cells is 204 pixels (the region is the
    // whole panel since WUX-1, heading included), so a 210-pixel line fits no row at all
    // and the body is described in cells instead.
    s.text_advance_px = 8;
    s.text_line_px = 210;
    const InfoBodyPlace squeezed = info_body_place(panel, screen_of(s), d, s);
    CHECK_FALSE(squeezed.fit.graphical());
    CHECK(squeezed.capacity ==
          static_cast<std::size_t>(squeezed.region_h - kInfoHeadingRows));

    Session faceless = s;
    faceless.text_advance_px = 0;
    faceless.text_line_px = 0;
    CHECK(squeezed.fit == info_body_place(panel, screen_of(faceless), d, faceless).fit);

    // AND IT IS STILL DRAWN. Both draw lists partition on `fit_region(...).graphical()`, so
    // the body is in exactly one of them and never in neither. The band is a region since
    // WUX-1 and its five cells hold no 210-pixel row either, so its rows are in the cell
    // list beside the body's.
    const surface::SurfaceCanvas c = paint(d, s);
    const surface::SurfaceExtent metric{s.screen_w * surface::kCanvasCellPx,
                                        s.screen_h * surface::kCanvasCellPx, 8, 210};
    const std::size_t typed =
        plan_regions_of(without_workspace(c), metric, surface::PlanSize{4000, 4000}).size();
    const std::size_t celled = projected_of(without_workspace(c), metric).size();
    CHECK(typed == 0);
    // ...and BOTH bands are in the cell list beside it since QR-14.
    CHECK(celled == static_cast<std::size_t>(squeezed.region_h + kBottomRows + kTopRows));
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
    CHECK_FALSE(info_body_place(ui::Rect{50, 0, 28, kInfoHeadingRows}, sc, d, s).present);
    CHECK_FALSE(info_body_place(ui::Rect{50, 0, kPropertyMarkCols + kPropertyLabelCols, 30}, sc,
                                d, s)
                    .present);
    // HD-7: one cell of body is one prose row here and the body needs three. HD-8: it needs
    // `kActionRows` more, because a panel that shows a maker two lists and no way to act on
    // either is the state this phase exists to end -- so the controls are bought at the same
    // price as the material, and the floor moves rather than the footer being dropped.
    // Unreachable at every screen this composition supports (the shortest body a face
    // resolves to here is ten prose rows); asserted because the metric arrives on the bus.
    // The heights are the panel's OUTER ones, so each carries the chrome the boundary
    // spends (WUX-5) on top of the rows the floor is actually about.
    const auto outer_for = [](std::int64_t rows) {
        return ui::Rect{50, 0, 28 + 2 * kChromeCells, rows + 2 * kChromeCells};
    };
    CHECK_FALSE(info_body_place(outer_for(kInfoHeadingRows + 2), sc, d, s).present);
    CHECK_FALSE(info_body_place(outer_for(kInfoHeadingRows + 3), sc, d, s).present);
    CHECK_FALSE(info_body_place(outer_for(kInfoHeadingRows +
                                          static_cast<std::int64_t>(kInfoBodyMinRows +
                                                                    kActionRows) - 1),
                                sc, d, s)
                    .present);
    CHECK(info_body_place(outer_for(kInfoHeadingRows + static_cast<std::int64_t>(
                                                          kInfoBodyMinRows + kActionRows)),
                          sc, d, s)
              .present);

    // A CLOSED INFO PANEL PAINTS NO BODY, which is what keeps the caret count honest. The
    // band is the screen's own region since WUX-1 and is on the canvas either way, so the
    // claim is about the panel's rectangle: no region is published there.
    Session closed;
    closed.selected = 1;
    refocus(d, closed);
    (void)close_panel(closed.panels, panel::kInfo);
    const Screen closed_sc = screen_of(closed);
    for (const surface::SurfaceTextRegion& r :
         all_texts(without_workspace(paint(d, closed)))) {
        CHECK(r.x != closed_sc.panel_x);
    }
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
        bounds_of(s.panels, s.setup.active, panel::kInfo, screen_of(s)).rect, screen_of(s), d, s);
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
    // the foot. WUX-1: the `OBJECTS` heading is its first row, above the body's capacity --
    // so what it publishes is the heading plus exactly the capacity: the two lists, the
    // `PROPERTIES` heading, the spare room written as blank rows, and the two controls.
    const std::size_t heading = static_cast<std::size_t>(kInfoHeadingRows);
    CHECK(shown->rows.size() == heading + cells.capacity);
    // The MATERIAL is still exactly the object rows, the heading and the property rows, and
    // everything after it to the footer is blank -- which is what "spare room stays spare"
    // looks like once a footer forces the spare rows to be written down.
    for (std::size_t i = heading + cells.objects_rows + 1 + cells.properties.count;
         i < heading + static_cast<std::size_t>(cells.action_row); ++i) {
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

    // AND THE SMALLER ONES COUNT WHAT THEY CANNOT SHOW while the biggest has nothing to
    // count. WUX-5's boundary moved the threshold by two rows: a 120x40 terminal seats
    // eighteen of the twenty now and says so, where it used to seat them all.
    CHECK(seen[0].find("... ") != std::string::npos);
    CHECK(seen[1].find("... ") != std::string::npos);
    CHECK(seen[2].find("... ") == std::string::npos);
    CHECK(seen[1].find("> #1 panel") != std::string::npos);
    CHECK(seen[2].find("  #20 panel") != std::string::npos); // all twenty, at 240x80
}

// ----------------------------------------------------------------------------
// Tier 12 — HD-7: the Info panel's two lists share one bounded body
// ----------------------------------------------------------------------------
//
// The OBJECTS list was the last information-bearing surface in this panel with a capacity a
// constant decided. `kListRows = 5` and `kRowsY = 8` are gone; both lists are rows of one
// `SurfaceTextRegion` and both capacities come from one `fit_region` and one sharing policy.

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
    // WUX-5 TOOK TWO MORE OFF EVERY ONE, for the panel's own visible boundary -- and again
    // the budgets with room to spare did not move.
    // ...AND QR-14 TOOK ONE MORE CELL ROW OFF THE COLUMN, because the side region begins
    // under the top band now. It lands where `share_body_rows` already decides such things
    // land -- on whichever list has the larger claim -- and the budgets with room to spare
    // did not move at all.
    for (const Want& want : {Want{78, 22, 0, 0, 5, 5},   // the minimum TUI screen
                             Want{120, 37, 0, 0, 17, 8}, // a 120x40 terminal: seventeen
                             Want{240, 77, 0, 0, 20, 8}, // and a 240x80 one, with room over
                             Want{78, 22, 8, 18, 2, 3},  // the minimum graphical screen
                             // The ordinary window: the `OBJECTS` heading is a row of the
                             // face since WUX-1, and at this height that row genuinely
                             // costs the object list one (the extra cell row the region
                             // gained does not reach another 18-pixel line here).
                             Want{80, 38, 8, 18, 7, 8},
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
    const std::size_t heading = static_cast<std::size_t>(kInfoHeadingRows);
    CHECK(shown->rows[heading + 0].text == "> #1 panel");
    CHECK(shown->rows[heading + 2].text == "PROPERTIES");
    CHECK(shown->rows[heading + 3].text.rfind(" Identity", 0) == 0);
    // ELEVEN ROWS OF MATERIAL AND NO MORE. HD-7 asserted that as `rows.size() == 11`, which
    // HD-8's foot-anchored footer makes false about the REGION and still true about the
    // material: everything from the eleventh row to the controls is blank, and the controls
    // are the last two rows whatever the document does.
    CHECK(shown->rows.size() == heading + body.capacity);
    for (std::size_t i = heading + spent; i < heading + static_cast<std::size_t>(body.action_row);
         ++i) {
        CAPTURE(i);
        CHECK(shown->rows[i].text.empty());
    }
    CHECK(shown->rows[heading + static_cast<std::size_t>(body.action_row)].text ==
          "[ Create ]");
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
                CHECK(shown->rows.size() <=
                      static_cast<std::size_t>(kInfoHeadingRows) + body.capacity);
                CHECK(shown->rows[static_cast<std::size_t>(kInfoHeadingRows +
                                                           body.heading_row)]
                          .text == "PROPERTIES");
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
    const std::string cut =
        body_on(narrow_canvas, narrow)->rows[static_cast<std::size_t>(kInfoHeadingRows)].text;
    CHECK(static_cast<std::int64_t>(cut.size()) <= narrow.columns);
    CHECK(cut.rfind("> #1 ", 0) == 0);                       // the mark and the identity survive
    CHECK(cut.substr(cut.size() - 3) == "...");              // and the cut is SAID
    CHECK(cut == "> #1 the-quick-brown-fo...");
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
    const std::string more =
        body_on(wide_canvas, roomier)->rows[static_cast<std::size_t>(kInfoHeadingRows)].text;
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

    t.press_at(body.region_x + 3,
               body.region_y + kInfoHeadingRows + at + surface::kTuiCanvasTopRow,
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
    // the object rows are inside it. THREE regions in the type list: the panel's and the two
    // BANDS' (the screen's own chrome is real type now too, and since QR-14 there are two of
    // them); the case reads the panel's by its place, and the claim about the object rows is
    // unchanged.
    const std::vector<surface::PlanTextRegion> typed =
        plan_regions_of(without_workspace(c), metric, surface::PlanSize{4000, 4000});
    REQUIRE(typed.size() == 3);
    const surface::PlanTextRegion* panel_region = nullptr;
    for (const surface::PlanTextRegion& r : typed) {
        if (r.view.y == body.region_y * surface::kCanvasCellPx) {
            panel_region = &r;
        }
    }
    REQUIRE(panel_region != nullptr);
    CHECK(panel_region->line_px == 18);
    bool named = false;
    for (const surface::PlanTextRow& row : panel_region->rows) {
        named = named || row.text == "> #1 panel";
    }
    CHECK(named);
    for (const surface::ProjectedRow& row : projected_of(without_workspace(c), metric)) {
        CHECK(row.label.text.find("#1 panel") == std::string::npos); // not drawn as cells
    }
    // AND THE ROWS ARE POSITIONED OFF THE SAME FIT: the plan carries ONE line pitch for the
    // whole region, which is the face's, so an object name and a property row cannot be set
    // at two different pitches.
    CHECK(panel_region->rows.size() >= 2);
    CHECK(panel_region->line_px == body.fit.line_px);
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
        // §12 WHERE THE SHARE CAN SEAT IT. A list of fewer than three rows cannot show an
        // object AND say what it hid, so `list_window` spends what it has on the omission
        // -- its own rule 3, reachable since HD-7 and one extent sooner since WUX-5 gave
        // the panel a visible boundary. Above that floor the selection is always shown.
        if (body.objects_rows >= 3) {
            CHECK(prose_row_of_object(body, 19) != kNoProseRow);
        } else {
            CHECK(body.objects.count == 0);
            CHECK(body.objects.after == t.doc().elements.size());
        }
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
    const std::size_t heading = static_cast<std::size_t>(kInfoHeadingRows);
    CHECK(shown->rows[heading].text == omitted_text(body.objects.before, "earlier"));
    CHECK(shown->rows[heading].role == surface::role::kMuted);
    const std::size_t last = heading + static_cast<std::size_t>(body.heading_row) - 1;
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
    CHECK(shown->rows[heading + static_cast<std::size_t>(here)].text.rfind("> ", 0) == 0);
    CHECK(shown->rows[heading + static_cast<std::size_t>(here)].role ==
          surface::role::kAccent);
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
                REQUIRE(shown->rows.size() ==
                        static_cast<std::size_t>(kInfoHeadingRows) + body.capacity);
                for (std::size_t which = 0; which < kActionCount; ++which) {
                    const std::int64_t at = prose_row_of_action(body, which);
                    REQUIRE(at != kNoProseRow);
                    CHECK(shown->rows[static_cast<std::size_t>(kInfoHeadingRows + at)]
                              .text.find(action_label(which)) != std::string::npos);
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
            shown->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(body, kActionCreate))];
        const surface::SurfaceTextRow& delete_row =
            shown->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(body, kActionDelete))];
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
    CHECK(shown->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(body, kActionCreate))].text ==
          "[ Create ]");
    CHECK(shown->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(body, kActionDelete))].text ==
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
        CHECK(shown->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(body, which))].text ==
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
    CHECK(after->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(free_now, kActionCreate))]
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
    CHECK(shown->rows[static_cast<std::size_t>(kInfoHeadingRows + prose_row_of_action(body, kActionDelete))].text ==
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
    t.press_at(body.region_x + 3,
               body.region_y + kInfoHeadingRows + at + surface::kTuiCanvasTopRow,
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
    for (std::size_t i = static_cast<std::size_t>(kInfoHeadingRows) + spent;
         i < static_cast<std::size_t>(kInfoHeadingRows + body.action_row); ++i) {
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

// ==========================================================================================
// ATTENTION HAS A PLACE -- the current-condition surface
// ==========================================================================================
//
// The phase's subject is a DISTINCTION, not a feature: Workshop can say a thing that
// HAPPENED and a thing that is TRUE, and both used to go out through one unowned
// string with no lifetime. So the cases below are mostly about what does NOT happen -- a
// condition that does not go stale, a dismissal that does not resolve, a resolution that
// does not need un-saying, a severity that does not open anything, and a presentation that
// does not become history.
//
// EACH OF THE EIGHT NAMED FALSIFIERS HAS A CASE, and each is written so that the defect it
// names would turn it red rather than merely leaving it unproven.

TEST_CASE("WUX-4: a healthy Workshop says nothing on the attention slot at all") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(t.conditions().empty());
    CHECK(t.attention_note().empty()); // EMPTY IS THE RETRACTION, and it is also the floor
    // ...and the view is still reachable, because "is anything wrong?" is a question a
    // maker is entitled to ask when the answer is no.
    t.key(input::scan::kA, input::mod::kCtrl);
    REQUIRE(t.session().attention.open);
    CHECK(stack_text(t.canvases.back()).find("nothing needs your attention") !=
          std::string::npos);
    CHECK(keyboard_context(t.session()) == KeyContext::kAttention);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().attention.open);
}

TEST_CASE("WUX-4: a held condition stands until its owner retracts it") {
    // FALSIFIER 1 -- a stale held condition: the owner resolves and the presentation
    // wrongly remains. And its inverse, which is the defect that was actually measured: a
    // standing truth that only disappears because somebody said something else.
    Session s;
    CHECK(attention_conditions(s).empty());

    s.conditions.establish(Condition{"test.wall", "a wall", "why it is a wall",
                                     surface::role::kAlert, std::string()});
    REQUIRE(attention_conditions(s).size() == 1);
    CHECK(attention_compact(attention_shown(s)) == "a wall");

    // AN UPDATE UNDER THE SAME KEY IS ONE CONDITION, not a second row.
    s.conditions.establish(Condition{"test.wall", "the same wall", "a better reason",
                                     surface::role::kAlert, std::string()});
    REQUIRE(attention_conditions(s).size() == 1);
    CHECK(attention_conditions(s).front().detail == "a better reason");

    // AND IT GOES BECAUSE ITS OWNER SAID SO, by name.
    s.conditions.retract("test.wall");
    CHECK(attention_conditions(s).empty());
    CHECK(attention_compact(attention_shown(s)).empty());
    // Retracting what was never established is silence rather than an error.
    s.conditions.retract("test.wall");
    CHECK(attention_conditions(s).empty());
}

TEST_CASE("WUX-4: a derived condition enters and leaves attention with its subject") {
    // FALSIFIER 4 -- a derived condition that is latched: the underlying state resolves
    // and the copy stays. There is no copy, so there is nothing to go stale: the pane's
    // own state IS the condition, and NOTHING in this case calls a retraction.
    Session s;
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    const Screen sc = screen_of(s);
    const PaneRef builder = ref_of(panel::kBuilder);
    REQUIRE(attention_conditions(s).empty());

    // A PANE THE MAKER AUTHORED, WITH NO CELL OF IT ON THE SCREEN.
    REQUIRE(author_pane_place(s.setup.active, builder, subs(sc.w + 40), subs(sc.h + 40))
                .accepted);
    const std::vector<Condition> off = attention_conditions(s);
    REQUIRE(off.size() == 1);
    CHECK(off.front().key == pane_window_key(builder));
    CHECK(off.front().compact.find("off-room") != std::string::npos);
    CHECK(off.front().compact.find(ref_text(builder)) != std::string::npos);
    // THE REMEDY IS `pane_state`'S OWN COLUMN, said where a maker can read it.
    CHECK(off.front().detail == std::string(pane_state_remedy(pane_state::kOffRoom)));
    CHECK(off.front().role == surface::role::kAccent); // actionable, not an error
    CHECK(off.front().action == "workshop.manage");

    // ...AND IT IS GONE THE MOMENT THE PLACE IS RESET. No retract call exists on this
    // path, and none is needed: the derivation stopped returning it.
    REQUIRE(reset_pane_place(s.setup.active, builder));
    CHECK(attention_conditions(s).empty());
}

TEST_CASE("WUX-4: not every true pane state deserves ambient attention") {
    // The judgement, pinned in both directions. A pane a maker CLOSED is their own
    // choice; an `unresolved` one is already counted on the band's own status row, all
    // day, derived; a `covered` one has something of it on the screen and stacking is
    // what arranging does. What earns the glance is the one thing none of those is:
    // authored, resolvable, and nothing of it to look at.
    Session s;
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    const Screen sc = screen_of(s);
    const PaneRef builder = ref_of(panel::kBuilder);

    // OPEN: nothing is wrong.
    REQUIRE(pane_state_of(s.panels, s.setup.active, sc,
                          CatalogRow{panel::kBuilder, builder, "Builder", ""}) ==
            pane_state::kOpen);
    CHECK(attention_conditions(s).empty());

    // CLOSED: a state with an available action, and deliberately not a warning.
    //
    // ⚠ THE DESK IS NAMED RATHER THAN INHERITED. This used to append a row to whatever
    // `Session`'s own default was, which is a fixture that quietly means "the product
    // default plus one" -- and a product default that grows (WUX-12 added the Layouts pane)
    // then puts an authored-but-unopened pane into a case about a CLOSED one. What the case
    // means is one desk naming Info alone, so it says that.
    Session closed;
    closed.setup.active = setup_of("Info only", {panel::kInfo});
    closed.panels.open = {Panel{panel::kInfo}};
    REQUIRE(pane_state_of(closed.panels, closed.setup.active, screen_of(closed),
                          CatalogRow{panel::kBuilder, builder, "Builder", ""}) ==
            pane_state::kClosed);
    CHECK(attention_conditions(closed).empty());
}

TEST_CASE("WUX-4: the project frontier is a condition while it waits and nothing after") {
    Live t;
    ProjectFrontier live;
    t.host.frontier = [&live] { return live; };
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(t.conditions().empty());
    CHECK(t.attention_note().empty());

    live.waiting = true;
    live.artifact = "zengine-thing";
    live.blocked = 2;
    t.key(input::scan::kTab); // any gesture at all repaints; nothing here says a sentence
    {
        const std::vector<Condition> now = t.conditions();
        const Condition* waiting = condition_by_key(now, kFrontierKey);
        REQUIRE(waiting != nullptr);
        CHECK(waiting->compact.find("zengine-thing") != std::string::npos);
        CHECK(waiting->detail.find("2 authored rows behind it") != std::string::npos);
        // WAITING IS NOT A FAILURE, and the role says so.
        CHECK(waiting->role == surface::role::kAccent);
        CHECK(waiting->action == "builder.frontier");
    }
    CHECK(t.attention_note().find("project waiting") != std::string::npos);

    live = ProjectFrontier{};
    t.key(input::scan::kTab);
    CHECK(condition_by_key(t.conditions(), kFrontierKey) == nullptr);
    CHECK(t.attention_note().empty());
}

TEST_CASE("WUX-4: dismissal hides a presentation and changes nothing that is true") {
    // FALSIFIER 2 -- a dismissal that mutates truth. The condition is a standing wall
    // whose owner is a file on disk, so "did anything change" has an answer outside this
    // process: the file is still refused and is still not overwritten.
    TempDir dir("wux4-dismiss");
    const std::string prefs = dir.file("workshop-prefs.json");
    spillout(prefs, "{ not a prefs file");
    Live t;
    t.host.prefs_path = prefs;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE(condition_by_key(t.conditions(), kPrefsWallKey) != nullptr);
    REQUIRE_FALSE(t.attention_note().empty());

    // OPEN, AND HIDE THE ONE THE CURSOR IS ON.
    t.key(input::scan::kA, input::mod::kCtrl);
    REQUIRE(t.session().attention.open);
    t.key(input::scan::kD);
    CHECK(t.session().attention.dismissed.size() == 1);

    // THE PRESENTATION IS GONE...
    CHECK(attention_shown(t.session()).empty());
    CHECK(t.attention_note().empty());
    // ...AND THE TRUTH IS NOT. The condition is still returned by the projection, its
    // owner still holds it, and the wall it describes is still standing: a toggle changes
    // the live preference and still writes nothing.
    CHECK(condition_by_key(t.conditions(), kPrefsWallKey) != nullptr);
    CHECK(t.session().conditions.holds(kPrefsWallKey));
    t.key(input::scan::kEscape);
    t.key(input::scan::kT);
    t.text("t");
    CHECK_FALSE(t.session().pane_titles);
    CHECK(t.notice().find("will not be overwritten") != std::string::npos);
    CHECK(slurp(prefs) == "{ not a prefs file");
}

TEST_CASE("WUX-4: a dismissed condition comes back when it materially changes") {
    // FALSIFIER 3 -- a dismissal that never re-arms. The dismissal is scoped to the
    // STATEMENT and not to the key alone, which is the Terminal completion's
    // `dismissed_at` rule one layer out.
    Session s;
    s.conditions.establish(Condition{"test.wall", "a wall", "the first reason",
                                     surface::role::kAlert, std::string()});
    const std::vector<Condition> before = attention_conditions(s);
    REQUIRE(before.size() == 1);
    s.attention.dismiss(before.front());
    CHECK(attention_shown(s).empty());

    // THE SAME STATEMENT, SAID AGAIN, IS STILL HIDDEN -- a dismissal a repaint undid
    // would be a gesture with no effect.
    s.conditions.establish(Condition{"test.wall", "a wall", "the first reason",
                                     surface::role::kAlert, std::string()});
    CHECK(attention_shown(s).empty());

    // A MATERIALLY DIFFERENT STATEMENT UNDER THE SAME KEY IS VISIBLE AGAIN, with nobody
    // clearing anything, and each field of the statement is enough on its own.
    const Condition moved[] = {
        Condition{"test.wall", "a WIDER wall", "the first reason", surface::role::kAlert,
                  std::string()},
        Condition{"test.wall", "a wall", "a WORSE reason", surface::role::kAlert,
                  std::string()},
        Condition{"test.wall", "a wall", "the first reason", surface::role::kAccent,
                  std::string()},
        Condition{"test.wall", "a wall", "the first reason", surface::role::kAlert,
                  "workshop.manage"}};
    for (const Condition& changed : moved) {
        Session moved_session = s;
        moved_session.conditions.establish(changed);
        CHECK(attention_shown(moved_session).size() == 1);
    }
}

TEST_CASE("WUX-4: dismiss is not resolve, resolve is not dismiss") {
    // The two halves pinned SEPARATELY, because they are two different mistakes.
    Session s;
    s.conditions.establish(Condition{"test.wall", "a wall", "why", surface::role::kAlert,
                                     std::string()});

    // DISMISS != RESOLVE: hiding leaves the condition true and its owner untouched.
    s.attention.dismiss(attention_conditions(s).front());
    CHECK(attention_shown(s).empty());
    CHECK(attention_conditions(s).size() == 1);
    CHECK(s.conditions.holds("test.wall"));

    // RESOLVE != DISMISS: retracting removes the condition itself, and the dismissal
    // that outlived it is simply irrelevant -- there is nothing for it to hide.
    s.conditions.retract("test.wall");
    CHECK(attention_conditions(s).empty());
    CHECK(attention_shown(s).empty());
    CHECK_FALSE(s.attention.dismissed.empty()); // held, and about nothing

    // ...and a condition that becomes true again under the same key with the same
    // statement is still hidden, which is the honest reading of what the maker said.
    s.conditions.establish(Condition{"test.wall", "a wall", "why", surface::role::kAlert,
                                     std::string()});
    CHECK(attention_shown(s).empty());
}

TEST_CASE("WUX-4: event sentences stay events, and a condition needs no sentence") {
    // FALSIFIER 5 -- event/condition conflation, in both directions.
    TempDir dir("wux4-events");
    const std::string prefs = dir.file("workshop-prefs.json");
    spillout(prefs, "{ not a prefs file");
    Live t;
    t.host.prefs_path = prefs;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    const std::string standing = t.attention_note();
    REQUIRE_FALSE(standing.empty());

    // AN ORDINARY EVENT SENTENCE DOES NOT BECOME A CONDITION.
    t.key(input::scan::kN);
    REQUIRE_FALSE(t.notice().empty());
    const std::size_t conditions_now = t.conditions().size();
    t.key(input::scan::kN);
    t.key(input::scan::kD);
    t.key(input::scan::kLeftBracket);
    CHECK(t.conditions().size() == conditions_now);

    // ...AND THE STANDING CONDITION DOES NOT DEPEND ON A LATER `say()` TO SURVIVE OR TO
    // BE HEARD. Four sentences have been written over the notice row since; the compact
    // attention line is byte-for-byte what it was, because it was never a sentence.
    CHECK(t.attention_note() == standing);
    CHECK(t.session().notice != standing);
}

TEST_CASE("WUX-4: an alert condition opens nothing") {
    // FALSIFIER 6 -- severity causing modality. Nothing in this tree can enter a mode
    // except a maker's gesture, and a condition is not a gesture however loud it is.
    TempDir dir("wux4-modal");
    const std::string prefs = dir.file("workshop-prefs.json");
    spillout(prefs, "{ not a prefs file");
    Live t;
    t.host.prefs_path = prefs;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    const std::vector<Condition> now = t.conditions();
    const Condition* wall = condition_by_key(now, kPrefsWallKey);
    REQUIRE(wall != nullptr);
    REQUIRE(wall->role == surface::role::kAlert);

    for (int beat = 0; beat < 4; ++beat) {
        t.publish(loom::to_value(surface::SurfaceReady{}));
        t.key(input::scan::kTab);
        CHECK_FALSE(t.session().attention.open);
        CHECK_FALSE(t.session().hotkeys.open);
        CHECK_FALSE(t.session().panels.picker.open);
        CHECK_FALSE(t.session().terminal.open);
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.session().setup.naming.open);
        CHECK(keyboard_context(t.session()) == KeyContext::kCommand);
    }
}

TEST_CASE("WUX-4: showing a condition writes no history") {
    // FALSIFIER 7 -- attention implying history. `loom::Recorder` is working memory and
    // `loom::Logger` is a durable selected record; a condition is neither, and showing one
    // must not quietly imply the other. Both are attached to the real bus for
    // the whole lifecycle below, which is the only way "nothing was written" is a
    // measurement rather than an absence of wiring.
    TempDir dir("wux4-history");
    const std::string prefs = dir.file("workshop-prefs.json");
    spillout(prefs, "{ not a prefs file");
    Live t;
    loom::Recorder history(t.bus);
    loom::Logger journal(t.bus);
    t.host.prefs_path = prefs;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE(condition_by_key(t.conditions(), kPrefsWallKey) != nullptr);

    // THE WHOLE LIFECYCLE: establish (above), read, hide, re-read, close.
    t.key(input::scan::kA, input::mod::kCtrl);
    t.key(input::scan::kDown);
    t.key(input::scan::kUp);
    t.key(input::scan::kD);
    t.key(input::scan::kEscape);

    // NOT ONE DIAGNOSTIC. `journal.info(...)` is the one door a host-authored fact has
    // into durable history, and nothing on the condition path goes near it.
    CHECK(journal.counters().diagnostics == 0);
    CHECK(journal.counters().appended == 0);

    // AND NO SHAPE OF ITS OWN. A condition is a value on the session; it has no wire
    // form, so it cannot be observed, recorded, selected, or retained -- and the
    // recorder's per-shape tally is where that would show up if it ever gained one.
    for (const loom::ShapeTally& tally : history.tallies()) {
        CHECK_MESSAGE(tally.shape.find("ondition") == std::string::npos,
                      "a condition reached the bus as shape ", tally.shape);
        CHECK_MESSAGE(tally.shape.find("ttention") == std::string::npos,
                      "attention reached the bus as shape ", tally.shape);
    }
}

TEST_CASE("WUX-4: a condition names an action and cannot execute one") {
    // FALSIFIER 8 -- a displayed action gaining authority. The condition holds an
    // `ActionRow::id` and nothing else; what the view paints is that action's CURRENT
    // gesture, looked up in the effective keymap at the moment it paints, and the view's
    // own vocabulary is four gestures that do not include it.
    TempDir dir("wux4-action");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"workshop.manage", "y"}}));
    Keyed t(path);
    Session& s = const_cast<Session&>(t.session());
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kBuilder}, Panel{panel::kInfo}};
    const Screen sc = screen_of(s);
    REQUIRE(author_pane_place(s.setup.active, ref_of(panel::kBuilder), subs(sc.w + 40),
                              subs(sc.h + 40))
                .accepted);
    t.key(input::scan::kA, input::mod::kCtrl);
    REQUIRE(t.session().attention.open);

    // THE PAINTED GESTURE IS THE MAKER'S, not the default: one truth, projected.
    const std::string view = stack_text(t.canvases.back());
    CHECK(view.find("try: y arrange desk") != std::string::npos);
    CHECK(view.find("try: w arrange desk") == std::string::npos);

    // AND PRESSING IT HERE DOES NOTHING. The action's own context is command mode; the
    // view is a mode of its own, and a condition is not an execution path.
    t.key(input::scan::kY);
    CHECK_FALSE(t.session().arrange.open);
    CHECK(t.session().attention.open); // still reading, still not executing
}

TEST_CASE("WUX-4: the compact line is ranked by truth, and says how many it is not saying") {
    Session s;
    // Established in the OPPOSITE order to the one they rank in, so a projection that
    // ordered by arrival would pick the wrong winner.
    s.conditions.establish(Condition{"b.quiet", "a quiet thing", "why", surface::role::kAccent,
                                     std::string()});
    s.conditions.establish(Condition{"a.quiet", "another quiet thing", "why",
                                     surface::role::kAccent, std::string()});
    s.conditions.establish(Condition{"z.loud", "a loud thing", "why", surface::role::kAlert,
                                     std::string()});
    const std::vector<Condition> shown = attention_shown(s);
    REQUIRE(shown.size() == 3);
    CHECK(shown[0].key == "z.loud");  // loudest first, whatever its key
    CHECK(shown[1].key == "a.quiet"); // then the key, so the order cannot wobble
    CHECK(shown[2].key == "b.quiet");
    CHECK(attention_compact(shown) == "a loud thing (+2 more)");

    // ONE CONDITION SAYS NO COUNT AT ALL -- a bound that announces itself when there is
    // nothing to bound is noise.
    s.conditions.retract("a.quiet");
    s.conditions.retract("b.quiet");
    CHECK(attention_compact(attention_shown(s)) == "a loud thing");

    // AND RANKING IS TOTAL OVER A ROLE THIS VOCABULARY DOES NOT HAVE YET.
    CHECK(attention_rank(surface::role::kAlert) < attention_rank(surface::role::kAccent));
    CHECK(attention_rank(surface::role::kAccent) < attention_rank(surface::role::kFill));
    CHECK(attention_rank(surface::role::kFill) < attention_rank(9999));
}

TEST_CASE("WUX-4: the view shows every current condition in its owner's own words") {
    Live t;
    Session& s = const_cast<Session&>(t.session());
    s.conditions.establish(Condition{"a.one", "the first thing",
                                     "a sentence its owner already had", surface::role::kAlert,
                                     std::string()});
    s.conditions.establish(Condition{"b.two", "the second thing", "and one for the second",
                                     surface::role::kAccent, std::string()});
    t.key(input::scan::kA, input::mod::kCtrl);
    const std::string view = stack_text(t.canvases.back());

    // ALL OF THEM, not only the compact winner.
    CHECK(view.find("ATTENTION -- 2 conditions") != std::string::npos);
    CHECK(view.find("the first thing") != std::string::npos);
    CHECK(view.find("the second thing") != std::string::npos);
    // ...and the owner's own explanation for the one being read.
    CHECK(view.find("a sentence its owner already had") != std::string::npos);

    // MOVING THE CURSOR MOVES WHICH EXPLANATION IS SPENT, and changes nothing else.
    t.key(input::scan::kDown);
    const std::string second = stack_text(t.canvases.back());
    CHECK(second.find("and one for the second") != std::string::npos);
    CHECK(t.conditions().size() == 2);

    // CLOSING CHANGES NO CONDITION.
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().attention.open);
    CHECK(t.conditions().size() == 2);
    CHECK(t.session().attention.dismissed.empty());
}

TEST_CASE("WUX-4: the view's gestures are the keymap's, and every help surface says so") {
    Live t;
    Session& s = const_cast<Session&>(t.session());
    s.conditions.establish(Condition{"a.one", "a thing", "why", surface::role::kAlert,
                                     std::string()});
    t.key(input::scan::kA, input::mod::kCtrl);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kAttention);

    // THE FULL HOTKEY VIEW DESCRIBES THE SURFACE BENEATH IT, this one included -- the
    // view's four gestures are ordinary catalog rows, not a parallel vocabulary.
    t.key(input::scan::kK, input::mod::kCtrl);
    const std::string hotkeys = stack_text(t.canvases.back());
    CHECK(hotkeys.find("what needs attention") != std::string::npos);
    CHECK(hotkeys.find("hide this one") != std::string::npos);
    CHECK(hotkeys.find("row up") != std::string::npos);
    t.key(input::scan::kK, input::mod::kCtrl);

    // ...AND THE OPENER IS ADVERTISED WHEREVER THE ABOVE-MODE CHORDS ARE.
    const std::vector<std::string> pairs = help_pairs(t.session().keymap, KeyContext::kCommand);
    bool advertised = false;
    for (const std::string& pair : pairs) {
        advertised = advertised || pair == "^a attention";
    }
    CHECK(advertised);

    // THE OPENER CLOSES IT, wherever a maker moved it -- and it is a gesture every
    // supported backend can actually produce.
    t.key(input::scan::kA, input::mod::kCtrl);
    CHECK_FALSE(t.session().attention.open);
    CHECK(posix_gap(t.session().keymap.gesture_of(Act::kAttention)) == nullptr);
}

TEST_CASE("WUX-4: the view never publishes more rows than its region can show") {
    // A REGION PADS WHAT IT WAS NOT GIVEN AND SILENTLY DROPS WHAT WILL NOT FIT, in BOTH
    // media -- so a painter that over-spends its budget loses whatever it wrote last, which
    // here is the omission marker: the one row that exists to say something was dropped. A
    // bound that grows when it is exceeded is not a bound, so this sweeps the population
    // against the room and asserts the published row count against the room's own answer.
    Live t;
    Session& s = const_cast<Session&>(t.session());
    for (int i = 0; i < 12; ++i) {
        // Long explanations on purpose: the cursor's block is what makes the naive window
        // arithmetic wrong, and a one-line detail would never reach the defect.
        s.conditions.establish(Condition{
            "k." + std::to_string(i), "condition number " + std::to_string(i),
            std::string("a long explanation that will certainly have to be wrapped across "
                        "several rows of any column this view is ever given, ") +
                std::to_string(i),
            i % 2 == 0 ? surface::role::kAlert : surface::role::kAccent, "workshop.manage"});
    }
    t.key(input::scan::kA, input::mod::kCtrl);
    REQUIRE(t.session().attention.open);

    // EVERY EXTENT THIS COMPOSITION IS HONEST AT, and every cursor position in it.
    for (const std::int64_t height : {kScreenMinH, kScreenMinH + 7, kScreenMinH + 20}) {
        t.publish(loom::to_value(surface::tui_canvas_extent(
            surface::TerminalSize{static_cast<int>(kScreenMinW), static_cast<int>(height)})));
        const Screen sc = screen_of(t.session());
        const PanelProsePlace place = panel_prose_place(attention_bounds(sc), sc);
        REQUIRE(place.present);
        for (std::size_t at = 0; at < 12; ++at) {
            const_cast<Session&>(t.session()).attention.cursor = at;
            t.key(input::scan::kUp); // any key at all repaints; the cursor is set above
            const_cast<Session&>(t.session()).attention.cursor = at;
            surface::SurfaceLayer layer;
            paint_attention(layer, t.session(), sc, ProjectFrontier{});
            REQUIRE(layer.texts.size() == 1);
            CAPTURE(height);
            CAPTURE(at);
            CHECK(static_cast<std::int64_t>(layer.texts[0].rows.size()) <= place.rows);
        }
    }
}

TEST_CASE("WUX-4: the condition path carries no timer, no callback and no history") {
    // The mechanical gate beside the behavioural cases, and it is here for the reason
    // every source probe in this repository is: a property that is true because of what
    // the code does NOT contain cannot be proved by running the code.
    //
    // THE PROSE GOES FIRST, this repository's own source-probe discipline. The header
    // EXPLAINS what it
    // refuses to be -- it names the Recorder, the Logger and every timed lifetime out loud
    // in order to say that none of them is here -- and a probe that could not tell a
    // sentence from a statement would forbid the explanation.
    std::ifstream in(WORKSHOP_ATTENTION_HPP);
    REQUIRE_MESSAGE(in.good(), "cannot read the condition model at ", WORKSHOP_ATTENTION_HPP);
    std::ostringstream all;
    all << in.rdbuf();
    const std::string whole = all.str();
    std::string text;
    text.reserve(whole.size());
    for (std::size_t i = 0; i < whole.size();) {
        if (whole.compare(i, 2, "//") == 0) {
            while (i < whole.size() && whole[i] != '\n') {
                ++i;
            }
            continue;
        }
        text.push_back(whole[i]);
        ++i;
    }

    // NO EXPIRY OF ANY KIND. Nothing in this path has a time axis, and nothing here may
    // invent one.
    for (const char* forbidden : {"Timer", "timeout", "expire", "expiry", "fuse", "elapsed",
                                  "deadline", "chrono", "TimerFired"}) {
        CHECK_MESSAGE(text.find(forbidden) == std::string::npos, "attention.hpp names ",
                      forbidden, ", which is a lifetime nobody asked for");
    }
    // NO CALLBACK, NO AUTHORITY, NO REGISTRY.
    for (const char* forbidden : {"std::function", "Manager", "register", "subscribe",
                                  "dispatch", "invoke", "callback"}) {
        CHECK_MESSAGE(text.find(forbidden) == std::string::npos, "attention.hpp names ",
                      forbidden, ", which is a power a condition may not hold");
    }
    // NO HISTORY, AND NO DOMAIN TRUTH.
    for (const char* forbidden : {"Recorder", "Logger", "journal", "BuildStatus",
                                  "pane_state", "Keymap", "prefs"}) {
        CHECK_MESSAGE(text.find(forbidden) == std::string::npos, "attention.hpp names ",
                      forbidden, ", which belongs to somebody else");
    }
    // ...and the one package include it has is the ONE vocabulary it spends: no Workshop
    // header, and nothing of the substrate at all. A condition has no wire form, so it
    // cannot be sent, recorded, logged or persisted, and this is where that is enforced.
    CHECK(text.find("#include \"surface/vocabulary.hpp\"") != std::string::npos);
    CHECK(text.find("#include \"screen.hpp\"") == std::string::npos);
    CHECK(text.find("#include \"panel.hpp\"") == std::string::npos);
    CHECK(text.find("<zen/") == std::string::npos);
    CHECK(text.find("ZEN_SHAPE") == std::string::npos);
}

// ============================================================================
// CTX-0 — What can I do with this? The contextual-action surface
// ============================================================================
//
// Two laws, and every case below is one of their falsifiers. POINTING NAMES A SUBJECT
// FOR ONE REQUEST; SELECTION IS A STATE A MAKER ENTERED: opening the surface captures a
// temporary subject and changes no persistent selection, no management selection and no
// keyboard candidate -- Move and Size alone may select, and only after their explicit
// target passes admission. OPEN REMEMBERS AN IDENTITY; SPEND RE-ASKS ITS OWNER: the
// surface holds a `PaneRef`, an object id, or nothing, and the owner operations answer
// for a subject that has since disappeared.

TEST_CASE("CTX-0: a right press captures a subject and selects nothing") {
    Live t;
    const std::int64_t selected_before = t.session().selected;
    const std::int64_t keyboard_before = t.session().panels.keyboard;
    REQUIRE_FALSE(t.session().arrange.addressed());

    SUBCASE("on a pane: the durable reference, and no selection of any kind") {
        open_pane(t, ref_of(panel::kBuilder));
        const ui::Rect slot = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                      screen_of(t.session()))
                .rect);
        t.right_press_canvas(slot.x + 1, slot.y + 1);
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kPane);
        CHECK(t.menu().pane == ref_of(panel::kBuilder));
        CHECK(t.session().selected == selected_before);
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.session().arrange.addressed());
        CHECK(t.session().panels.keyboard == keyboard_before);
    }
    SUBCASE("on a document object: the identity, and the selection untouched") {
        REQUIRE(t.session().selected == 1); // a fresh Workshop opens on #1
        t.right_press(7, 11);              // #2's body
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kObject);
        CHECK(t.menu().object == 2);
        CHECK(t.session().selected == 1); // pointing at #2 did not select it
    }
    SUBCASE("on the empty room: a real subject with no identity") {
        t.right_press(40, 0);
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kRoot);
    }
    SUBCASE("a further right press re-targets instead of toggling") {
        t.right_press(40, 0);
        REQUIRE(t.menu().subject == context_subject::kRoot);
        t.right_press(7, 11);
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kObject);
        CHECK(t.menu().object == 2);
    }
}

TEST_CASE("CTX-0: the declared populations are the researched ones, keyed by id") {
    // The pane's top level since ARR-0: ONE arrangement entry -- moving and resizing are
    // one maker intent -- then two groups at their first members' positions, and remove.
    // Groups appear ONCE, and an empty group is structurally impossible (a group entry
    // exists only where a member declared it).
    const std::vector<ContextEntry> pane = context_population(context_subject::kPane, "");
    REQUIRE(pane.size() == 4);
    CHECK_FALSE(pane[0].is_group);
    CHECK(pane[0].row->act == Act::kArrange);
    CHECK(pane[1].is_group);
    CHECK(std::string(pane[1].group) == "Order");
    CHECK(pane[2].is_group);
    CHECK(std::string(pane[2].group) == "Reset");
    CHECK(pane[3].row->act == Act::kManageRemove);

    const std::vector<ContextEntry> order =
        context_population(context_subject::kPane, "Order");
    REQUIRE(order.size() == 4);
    CHECK(order[0].row->act == Act::kManageFront);
    CHECK(order[1].row->act == Act::kManageBack);
    CHECK(order[2].row->act == Act::kManageRaise);
    CHECK(order[3].row->act == Act::kManageLower);

    const std::vector<ContextEntry> reset =
        context_population(context_subject::kPane, "Reset");
    REQUIRE(reset.size() == 3);
    CHECK(reset[0].row->act == Act::kManageResetPlace);
    CHECK(reset[1].row->act == Act::kManageResetWidth);
    CHECK(reset[2].row->act == Act::kManageResetHeight);

    // The object's whole first population is deletion -- Inspect is deferred until Info
    // has an honest pane-subject model, and nothing pads a menu to look fuller.
    const std::vector<ContextEntry> object =
        context_population(context_subject::kObject, "");
    REQUIRE(object.size() == 1);
    CHECK(object[0].row->act == Act::kObjectDelete);

    // The room: eleven zero-target doors, no groups.
    const std::vector<ContextEntry> root = context_population(context_subject::kRoot, "");
    REQUIRE(root.size() == 11);
    for (const ContextEntry& e : root) {
        CHECK_FALSE(e.is_group);
    }
    CHECK(root[0].row->act == Act::kObjectNew);
    CHECK(root[10].row->act == Act::kManageResetOrder);

    // EVERY DECLARATION RESOLVES AND OWNS NO POWER: an id `row_of_id` answers and three
    // plain fields -- the compile-time cross-check, restated where a reader looks.
    for (const ContextRow& row : kContextCatalog) {
        CHECK(row_of_id(row.action) != nullptr);
    }
}

TEST_CASE("CTX-0: a contextual action acts on the pointed pane, not the selection") {
    Live t;
    open_pane(t, ref_of(panel::kBuilder));
    const std::int64_t doc_selected = t.session().selected;
    // Info, the Layouts pane, and the Builder this case just opened -- the identity
    // permutation `add_pane` assigns, in list order.
    REQUIRE(ranks_of(t.session().setup.active) == std::vector<std::int64_t>{0, 1, 2});

    // Point at the BUILDER and send it to the back through the Order group.
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);
    REQUIRE(t.menu().pane == ref_of(panel::kBuilder));
    t.key(input::scan::kDown);
    t.key(input::scan::kReturn); // descend into Order
    REQUIRE(t.menu().group == "Order");
    t.key(input::scan::kDown);   // back
    t.key(input::scan::kReturn); // choose: send the POINTED pane to the back
    CHECK_FALSE(t.menu().open);  // a choice closes the surface

    // The pointed pane moved; the maker's own selections stayed exactly where they were,
    // and no arrangement state opened -- an ordering verb is one request, not a mode.
    CHECK(ranks_of(t.session().setup.active) == std::vector<std::int64_t>{1, 2, 0});
    CHECK_FALSE(t.session().arrange.open);
    CHECK_FALSE(t.session().arrange.addressed());
    CHECK(t.session().selected == doc_selected);
    CHECK(t.notice().find("back-most") != std::string::npos);
    CHECK(t.notice().find(ref_text(ref_of(panel::kBuilder))) != std::string::npos);
}

TEST_CASE("CTX-0/ARR-0: contextual Arrange admission precedes binding") {
    Live t;
    open_pane(t, ref_of(panel::kBuilder));

    SUBCASE("a refused entry establishes nothing") {
        // Info sits in the reserved side column -- the screen owns its place, and the
        // owner's own sentence says so. No arrangement scope, no binding.
        const ui::Rect side = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, panel::kInfo,
                      screen_of(t.session()))
                .rect);
        t.right_press_canvas(side.x + 1, side.y + 1);
        REQUIRE(t.menu().subject == context_subject::kPane);
        REQUIRE(t.menu().pane == ref_of(panel::kInfo));
        t.key(input::scan::kReturn); // the top row is Arrange
        CHECK_FALSE(t.menu().open);
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.session().arrange.addressed());
        CHECK(t.session().notice_is_bad);
        CHECK(t.notice().find("reserved side column") != std::string::npos);
    }
    SUBCASE("an accepted entry binds exactly the pointed pane, and one state carries "
            "both manipulations") {
        const ui::Rect slot = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                      screen_of(t.session()))
                .rect);
        t.right_press_canvas(slot.x + 1, slot.y + 1);
        t.key(input::scan::kReturn); // Arrange
        CHECK_FALSE(t.menu().open);
        CHECK(t.session().arrange.open);
        CHECK_FALSE(t.session().arrange.desk); // the ONE-PANE scope, not the old selector
        CHECK(t.session().arrange.pane == ref_of(panel::kBuilder));
        // MOVING AND RESIZING THE SAME PANE NEED NO STATE CHANGE IN BETWEEN (ARR-0):
        // an arrow places it and a shifted arrow resizes it, in the state already open.
        t.key(input::scan::kRight);
        const SetupPane* placed = pane_of(t.session().setup.active, ref_of(panel::kBuilder));
        REQUIRE(placed != nullptr);
        CHECK(placed->place.mode == pane_unit::kSubcells);
        t.key(input::scan::kRight, input::mod::kShift);
        const SetupPane* sized = pane_of(t.session().setup.active, ref_of(panel::kBuilder));
        CHECK(sized->width.mode == pane_unit::kSubcells);
        CHECK(t.session().arrange.open); // still the one state, nothing was left or entered
        CHECK_FALSE(t.session().arrange.desk);
    }
}

TEST_CASE("CTX-0: a captured pane that left the setup is refused truthfully") {
    Live t;
    open_pane(t, ref_of(panel::kBuilder));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().pane == ref_of(panel::kBuilder));
    // The reference leaves the setup UNDER the open surface -- the clearing that keeps
    // the mode's own selection fresh does not know this subject exists.
    REQUIRE(remove_pane(live(t).setup.active, ref_of(panel::kBuilder)));

    SUBCASE("Arrange refuses with absence, not with an unrelated geometry sentence") {
        t.key(input::scan::kReturn); // Arrange
        CHECK(t.session().notice_is_bad);
        CHECK(t.notice().find("no longer in this setup") != std::string::npos);
        CHECK(t.notice().find("no room") == std::string::npos);
        // ...and no dead arrangement state was left behind.
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.session().arrange.addressed());
    }
    SUBCASE("a targeted operation answers the same absence") {
        t.key(input::scan::kDown);
        t.key(input::scan::kReturn); // Order
        t.key(input::scan::kReturn); // front
        CHECK(t.session().notice_is_bad);
        CHECK(t.notice().find("no longer in this setup") != std::string::npos);
        CHECK(t.notice().find("already where") == std::string::npos);
    }
}

TEST_CASE("CTX-0: manage.remove removes the addressed pane by its own key") {
    Live t;
    open_pane(t, ref_of(panel::kBuilder));
    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kBuilder));
    t.key(input::scan::kD);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));
    // The presentation followed the intent through the one door, and the removed
    // reference cleared the keyboard's address on membership -- the DESK stays open,
    // because its subject is the desk and the desk is still there (ARR-0).
    for (const Panel& p : t.session().panels.open) {
        CHECK(p.kind != panel::kBuilder);
    }
    CHECK(t.session().arrange.open);
    CHECK(t.session().arrange.desk);
    CHECK_FALSE(t.session().arrange.addressed());
    CHECK(t.notice().find("removed") != std::string::npos);
    CHECK(t.notice().find("nothing behind it was touched") != std::string::npos);
}

TEST_CASE("CTX-0: a contextual remove removes the pointed pane") {
    Live t;
    open_pane(t, ref_of(panel::kBuilder));
    const std::int64_t selected_before = t.session().selected;
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    t.key(input::scan::kUp); // the cursor bound keeps it at the top; up is a no-op
    t.key(input::scan::kDown);
    t.key(input::scan::kDown);
    t.key(input::scan::kDown); // remove, the last top-level row
    t.key(input::scan::kReturn);
    CHECK_FALSE(t.menu().open);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(panel::kBuilder)));
    for (const Panel& p : t.session().panels.open) {
        CHECK(p.kind != panel::kBuilder);
    }
    CHECK(t.session().selected == selected_before);
    CHECK(t.notice().find("removed") != std::string::npos);
}

TEST_CASE("CTX-0: navigation backtracks cleanly and every way out closes") {
    Live t;
    open_pane(t, ref_of(panel::kBuilder));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);

    SUBCASE("descend, back out onto the group row, escape closes from the top") {
        t.right_press_canvas(slot.x + 1, slot.y + 1);
        t.key(input::scan::kDown);
        t.key(input::scan::kDown); // Reset
        t.key(input::scan::kReturn);
        REQUIRE(t.menu().group == "Reset");
        REQUIRE(t.menu().cursor == 0);
        t.key(input::scan::kEscape);
        CHECK(t.menu().open);
        CHECK(t.menu().group.empty());
        CHECK(t.menu().cursor == 2); // back on the Reset row the maker came from
        t.key(input::scan::kEscape);
        CHECK_FALSE(t.menu().open);
    }
    SUBCASE("the key that opened it closes it") {
        t.key(input::scan::kA);
        t.text("a"); // the printable trigger pays the swallow rule
        REQUIRE(t.menu().open);
        t.key(input::scan::kA);
        CHECK_FALSE(t.menu().open);
    }
    SUBCASE("the keyboard door opens on what command mode can name") {
        REQUIRE(t.session().selected == 1);
        t.key(input::scan::kA);
        t.text("a");
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kObject);
        CHECK(t.menu().object == 1);
        t.key(input::scan::kEscape);
        // ...and with nothing selected, the room.
        live(t).selected = 0;
        t.key(input::scan::kA);
        t.text("a");
        CHECK(t.menu().subject == context_subject::kRoot);
    }
}

TEST_CASE("CTX-0: input spent on the open surface does not leak through it") {
    Live t;
    const std::size_t inspector_before = t.session().cursor;
    t.right_press(40, 0); // the room's menu
    REQUIRE(t.menu().open);
    REQUIRE(t.menu().subject == context_subject::kRoot);

    SUBCASE("navigation keys move the surface's cursor and nothing beneath") {
        t.key(input::scan::kDown);
        t.key(input::scan::kDown);
        CHECK(t.menu().cursor == 2);
        CHECK(t.session().cursor == inspector_before); // the Inspector did not move
        CHECK(t.session().selected == 1);
    }
    SUBCASE("a press outside dismisses, is consumed, and operates nothing") {
        const std::int64_t selected_before = t.session().selected;
        const std::string notice_before = t.notice();
        // A cell left of the popup's own derived rectangle (the bounds are the press
        // resolver's too, so reading them here is the one geometry, not a second guess);
        // without the surface this press would be answered by whatever occupies it, or
        // by the document -- with it open, the press is spent whole on dismissal.
        const FineRect b = context_bounds(t.session(), screen_of(t.session()));
        REQUIRE(surface::cell_of_subs(b.x) >= 2); // the anchored popup sits right of here
        t.press_canvas(surface::cell_of_subs(b.x) - 2, surface::cell_of_subs(b.y));
        CHECK_FALSE(t.menu().open);
        CHECK(t.session().selected == selected_before); // nothing was selected
        CHECK_FALSE(t.session().drag.active);           // nothing was taken hold of
        CHECK(t.notice() == notice_before);             // nothing was said
    }
    SUBCASE("a press on the surface's own furniture is consumed silently") {
        const std::string notice_before = t.notice();
        // The heading row: inside the rectangle, on no population row.
        t.press_canvas(
            context_cell_x(t.session()),
            surface::cell_of_subs(context_bounds(t.session(), screen_of(t.session())).y));
        CHECK(t.menu().open); // not a dismissal
        CHECK(t.notice() == notice_before);
        CHECK(t.session().selected == 1);
    }
    SUBCASE("a press on a row is the pointer's choose") {
        // Row 1 of the room's population is the picker door -- the press lands exactly
        // where the painter drew the row (the inverse-pair claim, spent live).
        t.press_canvas(context_cell_x(t.session()),
                       context_entry_cell_y(t.session(), 1));
        CHECK_FALSE(t.menu().open);
        CHECK(t.session().panels.picker.open);
    }
}

TEST_CASE("CTX-0/ARR-0: a mode that owns the pointer answers a right press its own way") {
    Live t;
    SUBCASE("the terminal overlay: a second button still means nothing there") {
        t.toggle_terminal();
        REQUIRE(t.session().terminal.open);
        t.right_press(7, 11);
        CHECK_FALSE(t.menu().open);
        CHECK(t.session().terminal.open);
    }
    SUBCASE("an arrangement scope: the press LEAVES it, consumed whole (SC-6)") {
        open_pane(t, ref_of(panel::kBuilder));
        enter_arrange_desk(t);
        t.right_press(7, 11);
        // The state-local first refusal: leaving is what this interaction truthfully
        // means by a secondary press -- and ONE consumed gesture performs ONE
        // transition, so no context menu opens from the same press (SC-7).
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.menu().open);
    }
}

// ============================================================================
// ARR-0 — the secondary press's routing law, end to end
// ============================================================================
//
// THE ACTIVE INTERACTION THAT CAN TRUTHFULLY INTERPRET A SECONDARY PRESS RECEIVES FIRST
// REFUSAL; ONLY AN UNCLAIMED SECONDARY PRESS REACHES THE ORDINARY CONTEXTUAL OPENER.
// And one consumed gesture performs one interaction transition: the press that closes a
// state does not then operate the state it revealed.

TEST_CASE("ARR-0/SC-7: one right press exits Arrange; only the NEXT one opens context") {
    Live t;
    open_pane(t, ref_of(panel::kBuilder));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);

    // Enter pane-local Arrange through the menu, the road a maker actually takes.
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);
    t.key(input::scan::kReturn); // Arrange
    REQUIRE(t.session().arrange.open);
    REQUIRE_FALSE(t.session().arrange.desk);

    // RIGHT-CLICK ONCE: Arrange exits, and the context menu is NOT open.
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    CHECK_FALSE(t.session().arrange.open);
    CHECK_FALSE(t.menu().open);

    // THE RELEASE OF THAT SAME PRESS is a non-primary release on the ordinary path, and
    // it is dropped exactly as every second-button release always was -- nothing opens.
    t.publish(loom::to_value(input::PointerButton{3, false, slot.x + 1,
                                                  slot.y + 1 + surface::kTuiCanvasTopRow,
                                                  input::space::kCells, input::mod::kNone}));
    CHECK_FALSE(t.menu().open);
    CHECK_FALSE(t.session().arrange.open);

    // RIGHT-CLICK AGAIN: ordinary Workshop receives this NEW press, and context opens.
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    CHECK(t.menu().open);
    CHECK(t.menu().subject == context_subject::kPane);
    CHECK(t.menu().pane == ref_of(panel::kBuilder));
}

TEST_CASE("ARR-0/SC-6: every arrangement level claims the press; the menu keeps its own") {
    Live t;
    open_pane(t, ref_of(panel::kBuilder));

    SUBCASE("the desk") {
        enter_arrange_desk(t);
        t.right_press(40, 0);
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.menu().open);
    }
    SUBCASE("the reset prompt leaves the whole interaction") {
        enter_arrange_desk(t);
        select_pane(t, ref_of(panel::kBuilder));
        t.key(input::scan::k0);
        REQUIRE(t.session().arrange.resetting);
        t.right_press(40, 0);
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.session().arrange.resetting);
        CHECK_FALSE(t.menu().open);
    }
    SUBCASE("the open menu keeps its established meaning: re-targeting, not a toggle") {
        t.right_press(40, 15); // empty workspace, below the slot the Builder occupies
        REQUIRE(t.menu().open);
        REQUIRE(t.menu().subject == context_subject::kRoot);
        const ui::Rect slot = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                      screen_of(t.session()))
                .rect);
        t.right_press_canvas(slot.x + 1, slot.y + 1);
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kPane);
    }
    SUBCASE("no keymap row was minted for it") {
        // The routing is each state's own local reading, not a rebindable global Back:
        // no catalog identity names a pointer gesture (the binding grammar cannot even
        // spell one -- a Gesture is a scancode), and no `*click*` action exists.
        for (std::size_t i = 0; i < kActionCatalogCount; ++i) {
            CHECK(std::string(kActionCatalog[i].id).find("click") == std::string::npos);
        }
    }
}

// ---- WUX-7: the Inspector's draft, and reading past its ellipsis ---------------------------

TEST_CASE("WUX-7: a double-click in a property draft selects the word under it") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{80, 38, 8, 18}));
    t.begin_editing("Name");
    while (!t.row("Name")->draft().empty()) {
        t.key(input::scan::kBackspace);
    }
    t.text("alpha beta gamma");
    REQUIRE(t.row("Name")->draft() == "alpha beta gamma");
    const InfoBodyPlace body = body_place(t);
    const std::int64_t row = editing_prose_row(t, body);
    const auto press_col = [&](std::int64_t column, std::int64_t mods = input::mod::kNone) {
        t.press_at(value_pixel_x(body, column), value_pixel_y(body, row),
                   input::space::kPixels, mods);
    };

    SUBCASE("the second press selects it; the first still places the caret") {
        t.clock.together();
        press_col(7); // inside `beta`, which begins at byte 6
        CHECK(t.row("Name")->editor().caret() == 7);
        CHECK_FALSE(t.row("Name")->editor().has_selection());
        press_col(8);
        CHECK(t.row("Name")->editor().selected_text() == "beta");
        // ...and it is the draft's ordinary selection: typing replaces it.
        t.text("B");
        CHECK(t.row("Name")->draft() == "alpha B gamma");
    }
    SUBCASE("too slow is two presses, and a different word is two presses") {
        t.clock.apart();
        press_col(7);
        press_col(8);
        CHECK_FALSE(t.row("Name")->editor().has_selection());
        t.clock.together();
        press_col(7);
        press_col(2); // inside `alpha`
        CHECK_FALSE(t.row("Name")->editor().has_selection());
    }
    SUBCASE("a press in one box and a press in another are never one gesture") {
        // ⚔ MUTATION: an arming that carries no place. The Terminal's line and this draft
        // hold the same bytes at the same offsets, so only the identity tells them apart.
        (void)t.mount_terminal();
        t.clock.together();
        t.toggle_terminal();
        const Screen sc = screen_of(t.session());
        const TerminalInputPlace p = terminal_input_place(sc);
        for (const char c : std::string("alpha beta gamma")) {
            t.text(std::string(1, c));
        }
        // The inverse of `terminal_input_place`'s own resolution, spelled here because the
        // pane's press helper belongs to the suite that owns the pane.
        t.press_at(p.region_x * surface::kCanvasCellPx + p.fit.origin_x +
                       (kTerminalPromptCols + 7) * p.fit.advance_px + p.fit.advance_px / 2,
                   p.region_y * surface::kCanvasCellPx + p.fit.origin_y +
                       p.prose_row * p.fit.line_px + p.fit.line_px / 2,
                   input::space::kPixels);
        REQUIRE(t.session().click.place == text_drag_place::kTerminalLine);
        t.toggle_terminal();
        press_col(7);
        CHECK_FALSE(t.row("Name")->editor().has_selection());
        CHECK(t.pane().input.has_selection() == false);
    }
    SUBCASE("a modifier-bearing press keeps its ordinary meaning") {
        t.clock.together();
        press_col(7, input::mod::kCtrl);
        press_col(8, input::mod::kCtrl);
        CHECK_FALSE(t.row("Name")->editor().has_selection());
        CHECK(t.row("Name")->editor().caret() == 8);
    }
}

TEST_CASE("WUX-7: hovering a clipped object row reads past its ellipsis, and nothing else") {
    // A LONG NON-PATH IDENTITY -- a document object's own name, which is exactly the kind of
    // value the OBJECTS column has always cut and never been able to show.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
    const std::string name = "a-considerably-longer-object-name-than-this-column-can-hold";
    t.begin_editing("Name");
    while (!t.row("Name")->draft().empty()) {
        t.key(input::scan::kBackspace);
    }
    t.text(name);
    t.key(input::scan::kReturn);
    REQUIRE(t.doc().elements.front().label == name);

    const InfoBodyPlace body = body_place(t);
    REQUIRE(body.present);
    const std::int64_t first_row = prose_row_of_object(body, 0);
    REQUIRE(first_row != kNoProseRow);
    const std::string full = object_row_full(t.doc().elements.front(),
                                             t.doc().elements.front().id == t.session().selected);
    REQUIRE(full.size() > static_cast<std::size_t>(body.columns)); // it really is cut
    const auto row_text = [&](std::int64_t n) {
        return object_row(t.canvases.back(), t.doc(), t.session(), n);
    };
    const auto hover = [&](std::int64_t prose_row, std::int64_t column) {
        t.motion_canvas(body.region_x + column,
                        body.region_y + kInfoHeadingRows + prose_row);
    };
    const std::string at_rest = row_text(first_row);
    REQUIRE(at_rest.find(detail::kElided) != std::string::npos);

    SUBCASE("the left edge is the row a maker was already looking at") {
        // ⚔ MUTATION: an offset that does not start at zero -- the row would jump the
        // instant a pointer touched it.
        hover(first_row, 0);
        CHECK(row_text(first_row) == at_rest);
        CHECK(t.session().reveal.present());
        CHECK(t.session().reveal.offset == 0);
    }
    SUBCASE("the right edge shows the end of the name, inside the same row") {
        hover(first_row, body.columns - 1);
        const std::string revealed = row_text(first_row);
        CHECK(revealed != at_rest);
        CHECK(revealed.rfind(detail::kElided, 0) == 0); // marked where the head went
        CHECK(revealed.find("column-can-hold") != std::string::npos);
        CHECK(static_cast<std::int64_t>(revealed.size()) <= body.columns);
        // ...AND NOTHING UNDERNEATH IT MOVED: not the value, not the geometry, not the desk.
        CHECK(t.doc().elements.front().label == name);
        CHECK(body_place(t).region_x == body.region_x);
        CHECK(body_place(t).columns == body.columns);
    }
    SUBCASE("leaving the row puts the ordinary presentation back") {
        hover(first_row, body.columns - 1);
        REQUIRE(row_text(first_row) != at_rest);
        t.motion_canvas(kWorkspaceX + 2, kWorkspaceY + 2);
        CHECK_FALSE(t.session().reveal.present());
        CHECK(row_text(first_row) == at_rest);
    }
    SUBCASE("a row that fits does not move, however long the pointer rests on it") {
        // ⚔ MUTATION: revealing on length rather than on what the projection omitted.
        t.key(input::scan::kN); // a second object, called `panel` -- comfortably short
        const InfoBodyPlace now = body_place(t);
        const std::int64_t second = prose_row_of_object(now, 1);
        REQUIRE(second != kNoProseRow);
        const std::string short_row = object_row(t.canvases.back(), t.doc(), t.session(), second);
        REQUIRE(short_row.find(detail::kElided) == std::string::npos);
        for (std::int64_t column = 0; column < now.columns; ++column) {
            t.motion_canvas(now.region_x + column,
                            now.region_y + kInfoHeadingRows + second);
            CAPTURE(column);
            CHECK(object_row(t.canvases.back(), t.doc(), t.session(), second) == short_row);
            CHECK_FALSE(t.session().reveal.present());
        }
    }
    SUBCASE("the neighbouring rows, the heading and the blank body are not this row") {
        // ⚔ MUTATION: approximate row arithmetic. Every prose row of the body except the
        // one showing this object is swept, and none of them may reveal it.
        const InfoBodyPlace now = body_place(t);
        for (std::int64_t row = -kInfoHeadingRows; row < static_cast<std::int64_t>(now.capacity);
             ++row) {
            if (row == first_row) {
                continue;
            }
            CAPTURE(row);
            hover(row, now.columns - 1);
            CHECK(row_text(first_row) == at_rest);
            CHECK(t.session().reveal.place != reveal_place::kInfoObject);
        }
    }
    SUBCASE("a pane drawn OVER the panel owns those cells, and the row underneath is still") {
        // ⚔ MUTATION: resolving the hover from the panel's own bounds without asking the
        // occupancy walk first -- a maker pointing at the Builder would scroll a row
        // underneath it that they cannot even see.
        open_pane(t, ref_of(panel::kBuilder));
        const Screen sc = screen_of(t.session());
        const ui::Rect side = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc).rect);
        REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder),
                                  surface::subs_of_cells(side.x),
                                  surface::subs_of_cells(side.y))
                    .accepted);
        t.publish(loom::to_value(surface::SurfaceExtent{160, 44, 0, 0}));
        const InfoBodyPlace covered = body_place(t);
        const std::int64_t row = prose_row_of_object(covered, 0);
        REQUIRE(row != kNoProseRow);
        const std::int64_t cx = covered.region_x + covered.columns - 1;
        const std::int64_t cy = covered.region_y + kInfoHeadingRows + row;
        REQUIRE(occupied_at(t.session().panels, t.session().setup.active, screen_of(t.session()),
                            cx, cy)
                    .kind == panel::kBuilder);
        t.motion_canvas(cx, cy);
        CHECK(t.session().reveal.place != reveal_place::kInfoObject);
    }
    SUBCASE("hover is never authorship: no press, no draft, no rebuild, no notice") {
        const std::string said = t.notice();
        const std::string desk = setup_persist::to_text(t.session().setup.active);
        const std::size_t cursor = t.session().cursor;
        const std::int64_t chosen = t.session().selected;
        for (std::int64_t column = 0; column < body.columns; ++column) {
            hover(first_row, column);
        }
        CHECK(t.notice() == said);
        CHECK(setup_persist::to_text(t.session().setup.active) == desk);
        CHECK(t.session().cursor == cursor);
        CHECK(t.session().selected == chosen);
        CHECK(t.doc().elements.front().label == name);
        CHECK_FALSE(t.row("Name")->editing());
    }
}

// ============================================================================
// ---- WUX-9: several layouts in one running Workshop ------------------------
//
// The live half: the four gestures, the switch transaction through `apply_setup`, the
// pointer's own door on the band, and the Workshop-global facts a switch may not touch.

namespace {

/// The names of the layouts this Workshop is holding, in the maker's order.
std::vector<std::string> layout_names(const Live& t) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < layout_count(t.session().setup); ++i) {
        out.push_back(layout_at(t.session().setup, i).name);
    }
    return out;
}

/// The canvas cell a painted tab's first byte sits on, for a press.
std::int64_t tab_cell(const Live& t, std::size_t at) {
    const Screen sc = screen_of(t.session());
    const BandStatus row = band_status(t.session(), sc);
    for (const LayoutTab& tab : row.tabs) {
        if (tab.at == at) {
            return top_band_bounds(sc).x + tab.column + 1; // inside the mark, on the name
        }
    }
    return -1;
}

} // namespace

TEST_CASE("WUX-9/SC-10: four ordinary command-mode actions reach the layout shelf") {
    Live t;
    const LayoutKeys k = layout_keys(t);
    // THE DEFAULTS THE PHASE CHOSE, and every one of them is a gesture BOTH backends can
    // deliver: three unshifted printables and one plain ctrl chord.
    CHECK(k.next == Gesture{input::scan::kPeriod, input::mod::kNone});
    CHECK(k.previous == Gesture{input::scan::kComma, input::mod::kNone});
    CHECK(k.make == Gesture{input::scan::kEquals, input::mod::kNone});
    CHECK(k.drop == Gesture{input::scan::kW, input::mod::kCtrl});
    for (const Gesture& g : {k.next, k.previous, k.make, k.drop}) {
        CHECK(posix_gap(g) == nullptr);
    }

    // ONE LAYOUT: stepping says so and makes nothing.
    REQUIRE(layout_count(t.session().setup) == 1);
    press_gesture(t, k.next);
    CHECK(t.notice().find("only layout") != std::string::npos);
    CHECK(layout_count(t.session().setup) == 1);

    // ...AND THE ONLY LAYOUT CANNOT BE REMOVED.
    press_gesture(t, k.drop);
    CHECK(t.notice().find("only layout") != std::string::npos);
    CHECK(t.session().notice_is_bad);
    CHECK(layout_count(t.session().setup) == 1);

    // A NEW ONE IS A COPY OF THE ONE YOU WERE ON, appended and live.
    press_gesture(t, k.make);
    CHECK(layout_count(t.session().setup) == 2);
    CHECK(t.session().setup.active_at == 1);
    CHECK(t.notice().find("2 of 2") != std::string::npos);

    // STEPPING WRAPS, IN BOTH DIRECTIONS.
    press_gesture(t, k.next);
    CHECK(t.session().setup.active_at == 0);
    press_gesture(t, k.previous);
    CHECK(t.session().setup.active_at == 1);
    press_gesture(t, k.previous);
    CHECK(t.session().setup.active_at == 0);

    // AND REMOVING STANDS ON THE NEIGHBOUR RATHER THAN ON NOTHING.
    press_gesture(t, k.drop);
    CHECK(layout_count(t.session().setup) == 1);
    CHECK(t.session().setup.active_at == 0);
    CHECK(t.notice().find("removed layout") == 0);
}

TEST_CASE("WUX-9/SC-4: a switch returns membership, geometry and front order as authored") {
    Live t;
    const LayoutKeys k = layout_keys(t);
    // LAYOUT ONE: Info alone, moved somewhere a maker chose.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kInfo),
                              surface::subs_of_cells(3), surface::subs_of_cells(4))
                .accepted);
    const Setup first = t.session().setup.active;

    press_gesture(t, k.make);
    // LAYOUT TWO: a different membership, a different place, a different front order.
    open_builder(t);
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder),
                              surface::subs_of_cells(9), surface::subs_of_cells(2))
                .accepted);
    REQUIRE(send_to_back(live(t).setup.active, ref_of(panel::kBuilder)));
    const Setup second = t.session().setup.active;
    REQUIRE(first != second);

    // BACK AND FORTH, AND EACH ONE COMES BACK BYTE FOR BYTE.
    for (int round = 0; round < 3; ++round) {
        CAPTURE(round);
        press_gesture(t, k.previous);
        CHECK(t.session().setup.active == first);
        CHECK(t.session().panels.has(panel::kInfo));
        CHECK_FALSE(t.session().panels.has(panel::kBuilder));
        press_gesture(t, k.next);
        CHECK(t.session().setup.active == second);
        CHECK(t.session().panels.has(panel::kBuilder));
        // ...AND THE AUTHORED FRONT ORDER WITH IT: the Builder was put BEHIND Info in
        // this layout, so it is the first thing painted and the last thing pressed.
        CHECK(painted_order(t.session()).front() == panel::kBuilder);
    }

    // THE PRESENTATIONS ARE RECONCILED THROUGH THE ONE DOOR, so the panels a switch left
    // open are exactly the ones the destination names -- in its own list order.
    CHECK(authored_order(t.session()) == presentation_order(second, t.session().panels));
}

TEST_CASE("WUX-9/SC-5: a switch touches no Workshop-global fact") {
    Live t;
    const LayoutKeys k = layout_keys(t);
    open_builder(t);
    // A SELECTION AND A KEYBOARD CANDIDATE, made by a press exactly as a maker makes them.
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.session().panels.selected == panel::kBuilder);
    const std::int64_t selected = t.session().panels.selected;
    const std::int64_t keyboard = t.session().panels.keyboard;
    const WorkshopDoc document = t.doc();
    const std::string location = t.session().panels.files.current_dir;
    const std::uint64_t doc_epoch = t.session().editor.doc_epoch;
    const std::string source = t.session().editor.path;

    // A LAYOUT WITHOUT THE BUILDER IN IT -- and since WUX-11 that is what `layout.new`
    // MAKES: a fresh blank desk, whose membership `apply_setup` reconciles to through the
    // one door membership changes through.
    press_gesture(t, k.make);
    live(t).setup.active.name = "Inspect";
    REQUIRE_FALSE(t.session().panels.has(panel::kBuilder));

    // THE SELECTION IS NOT DESTROYED BY THE SWITCH -- it simply resolves to nothing while
    // its pane is absent, which is `selected_pane`'s own discipline (WUX-5).
    CHECK(t.session().panels.selected == selected);
    CHECK(t.session().panels.keyboard == keyboard);
    CHECK(selected_pane(t.session().panels) == kNoPaneKind);
    CHECK(keyboard_pane(t.session().panels) == kNoPaneKind);
    // ...AND IT LIFTS NOTHING: no ghost foreground for a pane that is not on this desk.
    for (const std::int64_t kind : painted_order(t.session())) {
        CHECK(kind != panel::kBuilder);
    }
    // THE DOCUMENT, THE SOURCE EDITOR AND THE BROWSER'S PLACE ARE ONE TRUTH EACH, AND
    // A SWITCH IS NOT A DOOR TO ANY OF THEM.
    CHECK(t.doc() == document);
    CHECK(t.session().panels.files.current_dir == location);
    CHECK(t.session().editor.doc_epoch == doc_epoch);
    CHECK(t.session().editor.path == source);

    // AND COMING BACK MAKES THE RETAINED SELECTION MEAN SOMETHING AGAIN.
    press_gesture(t, k.previous);
    CHECK(t.session().panels.selected == selected);
    CHECK(selected_pane(t.session().panels) == panel::kBuilder);
    CHECK(painted_order(t.session()).back() == panel::kBuilder);
}

TEST_CASE("WUX-9/SC-9: pressing a painted tab switches, and the rest of the row does not") {
    Live t;
    t.host.setup_path = "workshop-setup.json";
    const LayoutKeys k = layout_keys(t);
    press_gesture(t, k.make);
    live(t).setup.active.name = "Second";
    press_gesture(t, k.make);
    live(t).setup.active.name = "Third";
    REQUIRE(layout_names(t) == std::vector<std::string>{"Default", "Second", "Third"});
    REQUIRE(t.session().setup.active_at == 2);

    // A PRESS ON A TAB IS THE SAME SWITCH THE KEYBOARD PERFORMS.
    // THE ROW THE TABS ARE PAINTED ON, which since QR-14 is the FIRST row of Workshop.
    const std::int64_t band_row = top_band_bounds(screen_of(t.session())).y;
    t.press_canvas(tab_cell(t, 0), band_row);
    CHECK(t.session().setup.active_at == 0);
    CHECK(t.session().setup.active.name == "Default");
    CHECK(layout_names(t) == std::vector<std::string>{"Default", "Second", "Third"});

    t.press_canvas(tab_cell(t, 1), band_row);
    CHECK(t.session().setup.active_at == 1);

    // THE STATUS TO THE RIGHT OF THE RUN IS NOT A TAB, and pressing it selects no layout.
    // PAST THE CREATE AFFORDANCE TOO (WUX-11): `+` is the one other span the run owns, and
    // a case that landed on it would be measuring a new layout rather than a dead cell.
    const BandStatus row = band_status(t.session(), screen_of(t.session()));
    const std::int64_t past =
        top_band_bounds(screen_of(t.session())).x +
        (row.create_columns > 0 ? row.create_column + row.create_columns
                                : row.tabs.back().column + row.tabs.back().columns);
    t.press_canvas(past + 2, band_row);
    CHECK(t.session().setup.active_at == 1);
    CHECK(layout_count(t.session().setup) == 3);
    // ...and neither does the workspace-fact row beneath it.
    t.press_canvas(tab_cell(t, 0), band_row + 1);
    CHECK(t.session().setup.active_at == 1);
    // ⚠ NOR THE ROW THE RUN USED TO BE PAINTED ON. A press at the old footer coordinate is
    // a press on the bottom band, which owns no tab and never did answer one -- the stale
    // vertical hit map QR-14 must not leave behind.
    t.press_canvas(tab_cell(t, 0), band_bounds(screen_of(t.session())).y);
    CHECK(t.session().setup.active_at == 1);
}

TEST_CASE("WUX-12/SC-4+SC-8: a tab press IS a press on the Layouts pane, and still switches") {
    // ⭐ THE LAW THIS CASE STATES WAS REVERSED BY WUX-12, DELIBERATELY, AND THE REVERSAL IS
    // THE CONVERSION. It used to say *a press on the band is not a press on a pane, so the
    // line that records which pane the maker is pointing at must not run for it* -- which
    // was true only because the tab run lived in a rectangle no pane could own. It is a
    // pane's interior now, so pointing at a tab is pointing at the Layouts pane and the desk
    // says so with selected chrome, exactly as pointing at Files or the Editor does. An
    // exemption here would have been the one place the conversion stopped short: a surface
    // that owns the point but does not become the thing you are pointing at.
    //
    // WHAT DID NOT CHANGE is everything the press MEANS: the tab under the hand becomes the
    // live layout, through the same door the key spends.
    Live t;
    t.host.setup_path = "workshop-setup.json";
    const LayoutKeys k = layout_keys(t);
    open_builder(t);
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.session().panels.selected == panel::kBuilder);

    press_gesture(t, k.make);
    live(t).setup.active.name = "Other";
    // THE ROW THE TABS ARE PAINTED ON, which is the Layouts pane's first interior row.
    const std::int64_t band_row = top_band_bounds(screen_of(t.session())).y;
    t.press_canvas(tab_cell(t, 0), band_row);

    // THE SWITCH HAPPENED...
    CHECK(t.session().setup.active_at == 0);
    // ...AND THE MAKER IS NOW POINTING AT THE PANE THEY PRESSED. One press, one reading,
    // one pane -- `Panels::selected` is written by the same line that writes it for every
    // other pane, off the same occupancy walk.
    CHECK(t.session().panels.selected == panel::kLayouts);
}

TEST_CASE("WUX-9/SC-10: the layout gestures stay in command mode") {
    Live t;
    const LayoutKeys k = layout_keys(t);
    press_gesture(t, k.make); // two layouts to switch between
    REQUIRE(layout_count(t.session().setup) == 2);
    const std::size_t at = t.session().setup.active_at;

    // THE PICKER SWALLOWS BARE KEYS, so a layout gesture inside it is the picker's.
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    press_gesture(t, k.next);
    CHECK(t.session().setup.active_at == at);
    t.key(input::scan::kEscape);

    // SO DOES THE NAME EDITOR, where the same key is a character in a name.
    t.host.setup_path = "workshop-setup.json";
    open_rename_on_tab(t, t.session().setup.active_at);
    REQUIRE(t.session().setup.naming.open);
    press_gesture(t, k.next);
    t.text(".");
    CHECK(t.session().setup.active_at == at);
    CHECK(t.session().setup.naming.line.text().find(".") != std::string::npos);
    t.key(input::scan::kEscape);

    // AND SO DOES THE ARRANGEMENT DESK, whose own vocabulary owns every bare key it binds.
    t.key(input::scan::kW);
    t.text("w");
    REQUIRE(t.session().arrange.open);
    press_gesture(t, k.next);
    CHECK(t.session().setup.active_at == at);
    t.key(input::scan::kEscape);
    CHECK(t.session().setup.active_at == at);
}

TEST_CASE("WUX-11/SC-1: a new layout is blank and duplicates no Workshop-global state") {
    Live t;
    const LayoutKeys k = layout_keys(t);
    open_builder(t);
    const std::size_t runtime_before = t.session().panels.runtime.entries.size();
    const std::size_t external_before = t.session().panels.external.size();
    const WorkshopDoc document = t.doc();
    const Setup was = t.session().setup.active;

    press_gesture(t, k.make);
    // A FRESH DESK, NOT A COPY (WUX-11). What a blank layout changes is the PRESENTATION:
    // it names no Builder, so `apply_setup` withdraws that presentation exactly as any
    // other whole-desk replacement does.
    CHECK(t.session().setup.active == default_setup());
    CHECK_FALSE(t.session().panels.has(panel::kBuilder));
    CHECK(live_status(t.session().setup) == setup_link::kNone);
    // ...AND NOTHING WORKSHOP-GLOBAL WAS COPIED, CLEARED OR REVALIDATED. The catalog, the
    // external instances and the document are one truth each, and a desk is not a door to
    // any of them -- which is the half a blank layout must keep as exactly as a copy did.
    CHECK(t.session().panels.runtime.entries.size() == runtime_before);
    CHECK(t.session().panels.external.size() == external_before);
    CHECK(t.doc() == document);
    // AND THE LAYOUT IT WAS MADE FROM IS UNTOUCHED, waiting where it was.
    CHECK(layout_at(t.session().setup, 0) == was);
    press_gesture(t, k.previous);
    CHECK(t.session().panels.has(panel::kBuilder));
}

// ---- WUX-11: the gestures a maker actually makes on a tab ------------------------------

namespace {

/// Three named layouts, standing on the middle one -- the shape most of the cases below
/// want, built through the shipped gestures rather than by reaching into the value.
void three_named_layouts(Live& t) {
    rename_live_layout(t, "Home");
    press_gesture(t, layout_keys(t).make);
    rename_live_layout(t, "Code");
    press_gesture(t, layout_keys(t).make);
    rename_live_layout(t, "Art");
    press_gesture(t, layout_keys(t).previous);
    REQUIRE(layout_names(t) == std::vector<std::string>{"Home", "Code", "Art"});
    REQUIRE(t.session().setup.active_at == 1);
}

} // namespace

TEST_CASE("WUX-11/SC-3: a double-click on a tab renames THAT layout, and writes no file") {
    TempDir dir("wux11-dblclick");
    Live t;
    t.host.setup_path = dir.file("s.json");
    three_named_layouts(t);

    // ⭐ THE FIRST PRESS ACTIVATES AND THE SECOND OPENS THE EDITOR, which is why the
    // editor's subject and the live layout cannot disagree. A single press is the ordinary
    // switch it always was.
    press_tab(t, 0);
    CHECK(t.session().setup.active_at == 0);
    CHECK_FALSE(t.session().setup.naming.open);

    open_rename_on_tab(t, 2);
    REQUIRE(t.session().setup.naming.open);
    CHECK(t.session().setup.naming.at == 2);
    CHECK(t.session().setup.active_at == 2); // the first press stood on it
    CHECK(t.session().setup.naming.line.text() == "Art");
    type_name(t, "Gallery");

    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(layout_names(t) == std::vector<std::string>{"Home", "Code", "Gallery"});
    // ⭐ AND NO SETUP ARTIFACT WAS WRITTEN. Renaming is a layout operation.
    CHECK_FALSE(std::filesystem::exists(t.host.setup_path));
    CHECK(live_status(t.session().setup) == setup_link::kNone);

    // A THIRD PRESS IS AN ORDINARY PRESS AGAIN: the arming is SPENT, so there is no
    // triple-click and no editor re-opening under a maker's hand.
    press_tab(t, 2);
    CHECK_FALSE(t.session().setup.naming.open);
    // ...AND TWO PRESSES ON DIFFERENT TABS ARE TWO AIMS, never one gesture.
    t.clock.together();
    press_tab(t, 0);
    press_tab(t, 1);
    t.clock.apart();
    CHECK_FALSE(t.session().setup.naming.open);
    CHECK(t.session().setup.active_at == 1);
}

TEST_CASE("WUX-11/SC-2+SC-5: a tab's context menu acts on THAT tab") {
    Live t;
    three_named_layouts(t);

    // THE SUBJECT IS THE TAB THE PRESS NAMED, and asking about it does not stand on it.
    right_press_tab(t, 2);
    REQUIRE(t.menu().open);
    CHECK(t.menu().subject == context_subject::kLayout);
    CHECK(t.menu().layout == 2);
    CHECK(t.session().setup.active_at == 1); // NOT switched

    // ...AND THE FIVE OPERATIONS A MAKER CAN DO TO A TAB ARE THE ROWS IT OFFERS.
    std::vector<std::string> offered;
    for (const ContextEntry& row : context_population(t.menu().subject, t.menu().group)) {
        offered.push_back(row.is_group ? std::string("[") + row.group + "]"
                                       : std::string(row.row->id));
    }
    CHECK(offered == std::vector<std::string>{"layout.rename", "layout.duplicate", "[Order]",
                                              "layout.remove"});

    // CLOSE, ON THE TAB THAT WAS POINTED AT: the live desk does not move.
    const Setup live = t.session().setup.active;
    REQUIRE(choose_context_action(t, "layout.remove"));
    CHECK_FALSE(t.menu().open);
    CHECK(layout_names(t) == std::vector<std::string>{"Home", "Code"});
    CHECK(t.session().setup.active == live);
    CHECK(t.session().setup.active_at == 1);

    // DUPLICATE, ON AN INACTIVE TAB: the copy lands directly after its source and is live.
    right_press_tab(t, 0);
    REQUIRE(t.menu().layout == 0);
    REQUIRE(choose_context_action(t, "layout.duplicate"));
    CHECK(layout_names(t) == std::vector<std::string>{"Home", "Home", "Code"});
    CHECK(t.session().setup.active_at == 1);
    CHECK(t.session().setup.active == layout_at(t.session().setup, 0));

    // ⚠ AND `^w` IS NOT TAUGHT BESIDE A ROW THAT CLOSES A DIFFERENT LAYOUT. Found by the
    // live TUI witness: `layout.remove` IS bound and IS requestable in command mode, so the
    // annotation appeared beside a Close row acting on a tab the maker was not standing on.
    // `object.delete`'s own refinement, one subject over.
    right_press_tab(t, 0);
    REQUIRE(t.menu().open);
    REQUIRE(t.menu().layout != t.session().setup.active_at);
    for (const ContextEntry& row : context_population(t.menu().subject, t.menu().group)) {
        if (!row.is_group && row.row != nullptr &&
            std::string(row.row->id) == "layout.remove") {
            CHECK(context_annotation(t.session(), row).empty());
        }
    }
    // ...and it IS taught when the captured tab is the one a maker is standing on, because
    // then the key and the row are the same act. The surface is dismissed first: a left
    // press while it is open is spent on the dismissal and reaches no tab.
    t.key(input::scan::kEscape);
    REQUIRE_FALSE(t.menu().open);
    press_tab(t, 0);
    REQUIRE(t.session().setup.active_at == 0);
    right_press_tab(t, 0);
    REQUIRE(t.menu().layout == t.session().setup.active_at);
    for (const ContextEntry& row : context_population(t.menu().subject, t.menu().group)) {
        if (!row.is_group && row.row != nullptr &&
            std::string(row.row->id) == "layout.remove") {
            CHECK(context_annotation(t.session(), row) ==
                  hotkey_text(t.session().keymap, Act::kLayoutRemove));
        }
    }
    t.key(input::scan::kEscape);

    // RENAME, FROM THE MENU: the discoverable twin of the double-click.
    right_press_tab(t, 2);
    REQUIRE(t.menu().layout == 2);
    REQUIRE(choose_context_action(t, "layout.rename"));
    REQUIRE(t.session().setup.naming.open);
    CHECK(t.session().setup.naming.at == 2);
    type_name(t, "Renamed");
    CHECK(layout_names(t) == std::vector<std::string>{"Home", "Home", "Renamed"});
}

TEST_CASE("WUX-11/SC-4: Move Left and Move Right reorder from the tab that was pointed at") {
    Live t;
    three_named_layouts(t);

    // AN INACTIVE TAB MOVES AND THE LIVE DESK STAYS LIVE.
    const Setup live = t.session().setup.active;
    right_press_tab(t, 2);
    REQUIRE(choose_context_action(t, "layout.move-left"));
    CHECK(layout_names(t) == std::vector<std::string>{"Home", "Art", "Code"});
    CHECK(t.session().setup.active == live);
    CHECK(t.session().setup.active_at == 2);

    // THE LIVE TAB MOVES AND TAKES ITS POSITION WITH IT.
    right_press_tab(t, 2);
    REQUIRE(choose_context_action(t, "layout.move-left"));
    CHECK(layout_names(t) == std::vector<std::string>{"Home", "Code", "Art"});
    CHECK(t.session().setup.active == live);
    CHECK(t.session().setup.active_at == 1);

    // THE END OF THE RUN IS SAID RATHER THAN SILENTLY IGNORED.
    right_press_tab(t, 0);
    REQUIRE(choose_context_action(t, "layout.move-left"));
    CHECK(layout_names(t) == std::vector<std::string>{"Home", "Code", "Art"});
    CHECK(t.notice().find("already at the start") != std::string::npos);

    // ...AND THE PAINTED SPANS FOLLOW THE NEW ORDER IMMEDIATELY, which is what makes the
    // next press land where a maker is looking (HD-3, spent on a run that just moved).
    right_press_tab(t, 0);
    REQUIRE(choose_context_action(t, "layout.move-right"));
    REQUIRE(layout_names(t) == std::vector<std::string>{"Code", "Home", "Art"});
    const BandStatus row = band_status(t.session(), screen_of(t.session()));
    REQUIRE(row.tabs.size() == 3);
    for (const LayoutTab& tab : row.tabs) {
        CAPTURE(tab.at);
        const std::string span = row.text.substr(static_cast<std::size_t>(tab.column),
                                                 static_cast<std::size_t>(tab.columns));
        CHECK(span.find(layout_at(t.session().setup, tab.at).name) != std::string::npos);
    }
}

TEST_CASE("WUX-11/SC-4: dragging a tab along the run reorders it and nothing else") {
    Live t;
    three_named_layouts(t);
    const Setup live = t.session().setup.active;
    const std::size_t panels_before = t.session().panels.open.size();

    // A PRESS TAKES HOLD OF THE TAB IT LANDS ON -- which the press has just made live, so
    // the hand is always carrying `active_at` and there is no captured position to stale.
    const std::int64_t from = tab_column(t, 1);
    REQUIRE(from >= 0);
    t.press_canvas(from, 0);
    REQUIRE(t.session().tab_drag.active);
    REQUIRE(t.session().setup.active_at == 1);

    // A MOTION OVER ANOTHER TAB MOVES THE CARRIED LAYOUT THERE.
    t.motion_canvas(tab_column(t, 2), 0);
    CHECK(layout_names(t) == std::vector<std::string>{"Home", "Art", "Code"});
    CHECK(t.session().setup.active == live);
    CHECK(t.session().setup.active_at == 2);

    // ...AND THE RUN RE-DERIVES UNDER THE HAND, so a second motion is answered against the
    // order that is painted now rather than against the one the press began on.
    t.motion_canvas(tab_column(t, 0), 0);
    CHECK(layout_names(t) == std::vector<std::string>{"Code", "Home", "Art"});
    CHECK(t.session().setup.active_at == 0);

    // A RELEASE ENDS THE GESTURE, WHEREVER THE HAND IS.
    t.release_canvas(tab_column(t, 0), 0);
    CHECK_FALSE(t.session().tab_drag.active);
    t.motion_canvas(tab_column(t, 2), 0);
    CHECK(layout_names(t) == std::vector<std::string>{"Code", "Home", "Art"});

    // AND NOTHING BUT ORDER MOVED: the same desk is live, the same panes are presented,
    // and no association was created by any of it.
    CHECK(t.session().setup.active == live);
    CHECK(t.session().panels.open.size() == panels_before);
    CHECK(live_status(t.session().setup) == setup_link::kNone);
}

TEST_CASE("WUX-11/SC-1: the `+` affordance is the pointer's spelling of `layout.new`") {
    Live t;
    three_named_layouts(t);
    const Screen sc = screen_of(t.session());
    const BandStatus row = band_status(t.session(), sc);
    REQUIRE(row.create_columns == 1);
    CHECK(row.text.substr(static_cast<std::size_t>(row.create_column), 1) == "+");

    // PRESSING IT MAKES A BLANK LAYOUT, exactly as the key does -- and it is not a tab: no
    // layout was activated, and the run's count grew by one.
    t.press_canvas(row.create_column, 0);
    CHECK(layout_count(t.session().setup) == 4);
    CHECK(t.session().setup.active_at == 3);
    CHECK(t.session().setup.active == default_setup());
    CHECK(live_status(t.session().setup) == setup_link::kNone);

    // ...AND IT REFUSES A NINTH IN THE SAME WORDS THE KEY DOES.
    while (layout_count(t.session().setup) < kMaxLayouts) {
        press_gesture(t, layout_keys(t).make);
    }
    REQUIRE(layout_count(t.session().setup) == kMaxLayouts);
    const std::vector<std::string> before = layout_names(t);
    const BandStatus full = band_status(t.session(), screen_of(t.session()));
    if (full.create_columns > 0) {
        t.press_canvas(full.create_column, 0);
        CHECK(t.session().notice_is_bad);
        CHECK(t.notice().find("most layouts") != std::string::npos);
    }
    CHECK(layout_names(t) == before);
    CHECK(layout_count(t.session().setup) == kMaxLayouts);
}

TEST_CASE("WUX-11/SC-8: at the minimum width the `+` yields to the tab and the status") {
    // ⭐ THE AFFORDANCE IS THE FIRST THING TO GO. A row too narrow for everything must go on
    // saying WHICH layout is live and WHAT its association is; a create button is a
    // convenience whose keyboard route is unaffected by not painting it.
    //
    // SWEPT over name lengths and counts, because the yield happens at exactly the widths
    // where the run reaches its budget -- and the sweep also proves the two things that must
    // hold at EVERY width, which is what makes the yield meaningful rather than incidental.
    bool ever_omitted = false;
    bool ever_painted = false;
    for (std::size_t count = 1; count <= kMaxLayouts; ++count) {
        for (std::size_t len = 1; len <= kMaxSetupNameLen; ++len) {
            CAPTURE(count);
            CAPTURE(len);
            Session s = screen_session(kScreenMinW, kScreenMinH, 0, 0);
            s.setup.active = setup_of(std::string(len, 'z'), {panel::kInfo});
            for (std::size_t more = 1; more < count; ++more) {
                s.setup.shelved.push_back(Layout{
                    setup_of(std::string(len, static_cast<char>('a' + more)), {panel::kInfo}),
                    SetupLink{}});
            }
            s.setup.active_link =
                SetupLink{"/a/very/long/path/to/an/artifact.json", s.setup.active};

            const BandStatus row = band_status(s, screen_of(s));
            INFO(row.text);
            REQUIRE(static_cast<std::int64_t>(row.text.size()) <= kScreenMinW);
            // THE ACTIVE TAB IS PAINTED...
            bool live_painted = false;
            for (const LayoutTab& tab : row.tabs) {
                live_painted = live_painted || tab.active;
            }
            REQUIRE(live_painted);
            // ...AND THE VERDICT SURVIVES.
            REQUIRE(row.text.find("setup: ") != std::string::npos);
            REQUIRE(row.text.find("| current") != std::string::npos);
            // ...AND WHERE THE AFFORDANCE IS PAINTED IT TOOK NO CELL OF EITHER.
            if (row.create_columns > 0) {
                ever_painted = true;
                for (const LayoutTab& tab : row.tabs) {
                    REQUIRE((row.create_column >= tab.column + tab.columns ||
                             row.create_column + row.create_columns <= tab.column));
                }
                REQUIRE(row.create_column + row.create_columns <=
                        static_cast<std::int64_t>(row.text.find("setup: ")));
            } else {
                ever_omitted = true;
            }
        }
    }
    // ⭐ AND BOTH OUTCOMES ARE REACHABLE, which is what makes "it yields" a fact rather than
    // a sentence: a run with room gets its `+`, and a run that has used the budget does not.
    CHECK(ever_painted);
    CHECK(ever_omitted);
}

// ---- WUX-13: the Pane Editor -- a Workshop pane as a SUBJECT -------------------------------
//
// THE OLD PROOF OF CONCEPT EDITED DOCUMENT OBJECTS NAMED `panel`; this editor's subject is an
// ordinary Workshop pane, held by durable identity (`PaneRef`) and never derived from
// `Panels::selected`. Every case below drives the real weave through the doors a maker has
// (the picker, a press, the keys) and reads what came out through the placement path.

namespace {

/// The Pane Editor's INTERIOR, in cells, through `bounds_of` -- never a constant.
ui::Rect editor_cells(const Live& t) {
    const Screen sc = screen_of(t.session());
    const PanelBounds at =
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc);
    REQUIRE(at.open);
    return pane_body_cells(at.rect, sc);
}

/// PRESS INTO THE PANE EDITOR -- its heading row, which names nothing and is consumed as a
/// focus statement -- so the keys are pointed at it exactly as a maker points them.
void press_into_editor(Live& t) {
    const ui::Rect b = editor_cells(t);
    t.press_canvas(b.x, b.y);
    REQUIRE(t.session().panels.selected == panel::kPaneEditor);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneEditor);
}

/// Open the Pane Editor from the picker and point the keys at it.
void open_editor(Live& t) {
    open_pane(t, ref_of(panel::kPaneEditor));
    REQUIRE(t.session().panels.has(panel::kPaneEditor));
    press_into_editor(t);
}

std::size_t inventory_index(const Live& t, const PaneRef& ref) {
    const std::vector<CatalogRow> rows =
        inventory_rows(t.session().setup.active, t.session().panels);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].ref == ref) {
            return i;
        }
    }
    FAIL("not in the inventory: ", ref_text(ref));
    return rows.size();
}

/// CHOOSE A SUBJECT BY KEYS: on the PANES list, step to the pane, Return.
void choose_by_keys(Live& t, const PaneRef& ref) {
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    if (t.session().pane_editor.on_rows) {
        t.key(input::scan::kTab);
    }
    REQUIRE_FALSE(t.session().pane_editor.on_rows);
    const std::size_t want = inventory_index(t, ref);
    for (int guard = 0; guard < 64; ++guard) {
        const std::size_t at = t.session().pane_editor.cursor;
        if (at == want) {
            break;
        }
        t.key(at < want ? input::scan::kDown : input::scan::kUp);
    }
    REQUIRE(t.session().pane_editor.cursor == want);
    t.key(input::scan::kReturn);
    REQUIRE(t.session().pane_editor.subject == ref);
}

const Row* editor_row(const Live& t, const std::string& label) {
    for (const Row& r : t.session().pane_editor.rows) {
        if (r.label() == label) {
            return &r;
        }
    }
    return nullptr;
}

std::string editor_value(const Live& t, const std::string& label) {
    const Row* row = editor_row(t, label);
    REQUIRE_MESSAGE(row != nullptr, "no Pane Editor row labelled ", label);
    return row->value();
}

/// PUT THE ROW CURSOR ON A LABELLED ROW, by keys only -- Tab into the rows, then step.
void go_to_row(Live& t, const std::string& label) {
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    if (!t.session().pane_editor.on_rows) {
        t.key(input::scan::kTab);
    }
    REQUIRE(t.session().pane_editor.on_rows);
    for (int guard = 0; guard < 64; ++guard) {
        const PaneEditor& ed = t.session().pane_editor;
        REQUIRE(ed.row_cursor < ed.rows.size());
        if (ed.rows[ed.row_cursor].label() == label) {
            return;
        }
        // walk down, then wrap to the top and walk down again
        const std::size_t was = ed.row_cursor;
        t.key(input::scan::kDown);
        if (t.session().pane_editor.row_cursor == was) {
            for (int up = 0; up < 32; ++up) {
                t.key(input::scan::kUp);
            }
        }
    }
    FAIL("no Pane Editor row labelled ", label);
}

/// TYPE A VALUE INTO A LABELLED ROW AND COMMIT IT -- the ordinary draft vocabulary.
void type_value(Live& t, const std::string& label, const std::string& text) {
    go_to_row(t, label);
    t.key(input::scan::kReturn);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kDraft);
    const Row* row = editor_row(t, label);
    REQUIRE(row != nullptr);
    REQUIRE(row->editing());
    // an opened draft holds the current value; replace it whole
    for (std::size_t i = 0; i < 64; ++i) {
        t.key(input::scan::kBackspace);
    }
    for (const char c : text) {
        t.text(std::string(1, c));
    }
    t.key(input::scan::kReturn);
}

/// The Pane Editor's painted rows, read back off the last frame's canvas.
std::string editor_text(Live& t) {
    return panel_text(t.canvases.back(), editor_cells(t));
}

} // namespace

TEST_CASE("WUX-13/SC-2: the Pane Editor is a built-in, and its list is the picker's population") {
    // A CATALOG ROW LIKE ANY OTHER: in the overlay stack, so it can be its own subject, and
    // a keyboard-taking pane, so its list has a cursor. Not in the default setup -- a new
    // kind never is (make-a-workshop-tool's law).
    const PanelKind& k = panel_kind(panel::kPaneEditor);
    CHECK(std::string(k.name) == "Pane Manager");
    CHECK(std::string(k.pane) == "pane-editor");
    CHECK(k.placed_in == placement::kOverlayStack);
    CHECK(k.takes_keyboard);
    Live t;
    CHECK_FALSE(t.session().panels.has(panel::kPaneEditor));
    // ...AND THE PICKER'S NAME COLUMN HOLDS ITS WHOLE NAME (WUX-13 widened it).
    CHECK(std::string(k.name).size() < kPickerNameCols);

    // THE PANES LIST IS `inventory_rows` -- catalog, admitted runtime panes, and every
    // reference the setup names -- and NOT a copy: an authored reference no office resolves
    // has a row here because it has one there (F6: filtering unresolved refs out is caught).
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    REQUIRE(add_pane(live(t).setup.active, stranger()));
    open_editor(t);
    // give the editor room for the whole list, through the authored size door
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(panel::kPaneEditor), PaneSize{},
                             PaneSize{pane_unit::kSubcells, subs(30)})
                .accepted);
    t.key(input::scan::kDown); // any gesture repaints
    const std::vector<CatalogRow> rows =
        inventory_rows(t.session().setup.active, t.session().panels);
    const std::string shown = editor_text(t);
    CHECK(shown.find("PANE MANAGER *") != std::string::npos);
    for (const CatalogRow& row : rows) {
        INFO(row.name);
        CHECK(shown.find(detail::pad(row.name, kPickerNameCols)) != std::string::npos);
    }
    CHECK(shown.find(detail::pad("history", kPickerNameCols) + "unresolved") !=
          std::string::npos);
    CHECK(has_pane(t.session().setup.active, stranger()));
}

TEST_CASE("WUX-13/SC-1: the subject is chosen, and interacting inside the editor does not "
          "retarget it") {
    // ⭐ THE SUBJECT MODEL. `Panels::selected` says which pane the maker is interacting WITH
    // -- and pressing into the Pane Editor makes that the Pane Editor -- while the subject
    // says which pane they asked it to DESCRIBE. The two are related and not the same.
    //
    // ⚔ MUTATION (F1): deriving the subject from `Panels::selected`. Every press and key
    // below lands in the Pane Editor, so a derived subject would become the Pane Editor
    // the moment the maker touched a row; `subject == layouts` goes red.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    const PaneRef layouts = ref_of(panel::kLayouts);
    choose_by_keys(t, layouts);
    // CHOOSING DID NOT SELECT: the desk's selection is still the pane under the hand.
    CHECK(t.session().panels.selected == panel::kPaneEditor);
    CHECK(t.session().pane_editor.subject == layouts);

    // A PRESS ON ONE OF ITS OWN ROWS...
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(panel::kPaneEditor), PaneSize{},
                             PaneSize{pane_unit::kSubcells, subs(30)})
                .accepted);
    t.key(input::scan::kTab); // repaint at the new size, and step into the rows
    REQUIRE(t.session().pane_editor.on_rows);
    const ui::Rect b = editor_cells(t);
    const Screen sc = screen_of(t.session());
    const PaneEditorBodyPlace body = pane_editor_body(
        t.session(), sc,
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc).rect);
    REQUIRE(body.present);
    const std::int64_t x_row = prose_row_of_field(body, 5); // X, after four facts and AUTHORED
    REQUIRE(x_row != kNoProseRow);
    t.press_canvas(b.x + 3, b.y + kPaneEditorHeadingRows + x_row);
    CHECK(t.session().panels.selected == panel::kPaneEditor);
    CHECK(t.session().pane_editor.subject == layouts);
    CHECK(t.session().pane_editor.on_rows);
    CHECK(t.session().pane_editor.rows[t.session().pane_editor.row_cursor].label() == "X");
    // ...AND TYPING IN IT: the keys are the editor's, not command mode's, and the subject
    // stands.
    const std::size_t objects = t.doc().elements.size();
    t.key(input::scan::kN); // command mode's `new object`; here it is the Pane Creator's prompt
    CHECK(t.doc().elements.size() == objects);
    CHECK(keyboard_context(t.session()) == KeyContext::kPaneNaming);
    t.key(input::scan::kEscape); // ...cancelled: no pane was made, and the subject stands
    CHECK_FALSE(t.session().panels.maker.open());
    CHECK(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    t.key(input::scan::kDown);
    t.key(input::scan::kUp);
    CHECK(t.session().pane_editor.subject == layouts);
    CHECK(t.session().panels.selected == panel::kPaneEditor);

    // AND PRESSING ELSEWHERE TAKES THE KEYS AWAY AND LEAVES THE SUBJECT: selection moved,
    // the editor still describes Layouts.
    t.press(90, 35); // the workspace, clear of the stack
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(keyboard_context(t.session()) == KeyContext::kCommand);
    CHECK(t.session().pane_editor.subject == layouts);
}

TEST_CASE("WUX-13/SC-4+SC-5: the subject's rows say identity, then AUTHORED, then RESOLVED") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    choose_by_keys(t, ref_of(panel::kLayouts));
    const std::vector<Row>& rows = t.session().pane_editor.rows;
    // ...AND THEN INTERIOR (WUX-14): for a code-backed subject, one read-only capture row.
    const char* const expected[] = {"Name",   "Identity", "Provider", "Summary", "AUTHORED",
                                    "X",      "Y",        "Width",    "Height",  "Front",
                                    "Open",   "RESOLVED", "Window",   "State",   "INTERIOR",
                                    "Interior"};
    REQUIRE(rows.size() == sizeof(expected) / sizeof(expected[0]));
    for (std::size_t i = 0; i < rows.size(); ++i) {
        INFO(i);
        CHECK(rows[i].label() == expected[i]);
    }
    // IDENTITY: catalog facts, none invented.
    CHECK(editor_value(t, "Name") == "Layouts");
    CHECK(editor_value(t, "Identity") == "zengine.workshop/layouts");
    CHECK(editor_value(t, "Provider") == "zengine.workshop (built in)");
    CHECK(editor_value(t, "Summary") == "layout tabs and setup");
    // AUTHORED: nothing yet -- a fresh desk is the developer's answer on every axis.
    CHECK(rows[4].section());
    CHECK(editor_value(t, "X") == "-");
    CHECK(editor_value(t, "Y") == "-");
    CHECK(editor_value(t, "Width") == "-");
    CHECK(editor_value(t, "Height") == "-");
    CHECK(editor_value(t, "Front").find("f1 of 3") == 0);
    CHECK(editor_value(t, "Open").find("yes") == 0);
    // RESOLVED: the rectangle the pane path answers RIGHT NOW, in the face's unit.
    CHECK(rows[11].section());
    const Screen sc = screen_of(t.session());
    const PanelBounds where =
        bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc);
    CHECK(editor_value(t, "Window") == fine_rect_text(where.resolved, 0));
    CHECK(editor_value(t, "Window") == "@0,0 132x2 cells");
    CHECK(editor_value(t, "State") == "open");
    // WHICH ROWS ARE THE MAKER'S TO TOUCH says which truth is which.
    for (const char* authored : {"X", "Y", "Width", "Height"}) {
        CHECK(editor_row(t, authored)->editable());
    }
    for (const char* derived : {"Name", "Identity", "Provider", "Summary", "Front", "Open",
                                "Window", "State", "Interior"}) {
        INFO(derived);
        CHECK_FALSE(editor_row(t, derived)->editable());
    }
    // ...AND THE SECTIONS ARE PAINTED AS BOUNDARIES, on the one ground every ink reads on.
    REQUIRE(author_pane_size(live(t).setup.active, ref_of(panel::kPaneEditor), PaneSize{},
                             PaneSize{pane_unit::kSubcells, subs(30)})
                .accepted);
    t.key(input::scan::kTab);
    const ui::Rect b = editor_cells(t);
    const std::vector<surface::SurfaceTextRegion> found = regions_at(t.canvases.back(), b.x, b.y);
    REQUIRE(found.size() == 1);
    bool authored_ground = false;
    bool resolved_ground = false;
    for (const surface::SurfaceTextRow& row : found.front().rows) {
        if (row.text == "AUTHORED") {
            authored_ground = row.background == surface::role::kMuted;
        }
        if (row.text == "RESOLVED") {
            resolved_ground = row.background == surface::role::kMuted;
        }
    }
    CHECK(authored_ground);
    CHECK(resolved_ground);
    // ...AND THE SUBJECT'S OWN ROW IN THE LIST WEARS ITS MARK.
    CHECK(editor_text(t).find("*" + detail::pad("Layouts", kPickerNameCols)) !=
          std::string::npos);
}

TEST_CASE("WUX-13/SC-6+SC-11: a typed place moves Layouts through the gesture door, and its "
          "tabs follow") {
    // ⭐ THE SELF-APPLICATION PROOF, in the suite. Layouts is Workshop's own presentation
    // of itself; the Pane Editor changes its authored Y; the pane path -- paint, occupancy,
    // the tab press inverse -- follows with nothing added.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    const PaneRef layouts = ref_of(panel::kLayouts);
    choose_by_keys(t, layouts);
    const Screen sc = screen_of(t.session());
    REQUIRE(layouts_body(t.session(), sc).region_y == 0);
    type_value(t, "Y", "20");
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.notice() == "committed Y = 20 cells");
    // THE AUTHORED ROW MOVED, THROUGH `author_pane_window`: the untyped axis kept what it
    // stood at (the resolved 0), the typed axis is the maker's, and the extents were not
    // touched.
    const SetupPane* row = pane_of(t.session().setup.active, layouts);
    REQUIRE(row != nullptr);
    CHECK(row->place == PanePlace{pane_unit::kSubcells, 0, subs(20)});
    CHECK(row->width == PaneSize{});
    CHECK(row->height == PaneSize{});
    CHECK(editor_value(t, "Y") == "20 cells");
    CHECK(editor_value(t, "X") == "0 cells");
    // THE RESOLVED ROW FOLLOWED, FRESH...
    const PanelBounds where =
        bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc);
    CHECK(where.rect.y == subs(20));
    CHECK(editor_value(t, "Window") == "@0,20 132x2 cells");
    // ...AND SO DID THE TABS: the run's body is at the new row, and the press inverse
    // answers there and not at the old one.
    CHECK(layouts_body(t.session(), sc).region_y == 20);
    CHECK(band_tab_at(t.session(), sc, input::space::kCells, 2,
                      20 + surface::kTuiCanvasTopRow)
              .hit);
    CHECK_FALSE(band_tab_at(t.session(), sc, input::space::kCells, 2,
                            surface::kTuiCanvasTopRow)
                    .hit);
    CHECK(occupied_at(t.session().panels, t.session().setup.active, sc, 2, 20).kind ==
          panel::kLayouts);
    CHECK_FALSE(occupied_at(t.session().panels, t.session().setup.active, sc, 2, 0).occupied);
    // A TYPED WIDTH IS THE SAME DOOR, ONE AXIS: the place and the height stand.
    type_value(t, "Width", "40");
    CHECK(pane_of(t.session().setup.active, layouts)->width ==
          PaneSize{pane_unit::kSubcells, subs(40)});
    CHECK(pane_of(t.session().setup.active, layouts)->place ==
          PanePlace{pane_unit::kSubcells, 0, subs(20)});
    CHECK(editor_value(t, "Window") == "@0,20 40x2 cells");
    // `-` IS THE RESET DOOR, ONE AXIS AT A TIME.
    type_value(t, "Width", "-");
    CHECK(pane_of(t.session().setup.active, layouts)->width == PaneSize{});
    CHECK(pane_of(t.session().setup.active, layouts)->place ==
          PanePlace{pane_unit::kSubcells, 0, subs(20)});
    type_value(t, "X", "-");
    CHECK(pane_of(t.session().setup.active, layouts)->place == PanePlace{});
    CHECK(layouts_body(t.session(), sc).region_y == 0);
}

TEST_CASE("WUX-13/SC-6: a typed place reseats the stack through `apply_setup`") {
    // ⚔ MUTATION (F4): a write that lands in the `SetupPane` without going through the
    // commit path's reseat. `bounds_of` reads the setup live, so a moved pane MOVES either
    // way -- what a bypass leaves behind is a pane the picker refused for want of room,
    // still waiting after the room appeared. The minimum screen seats one stacked pane.
    Live t;
    open_editor(t);
    REQUIRE(t.session().panels.has(panel::kPaneEditor));
    const PaneRef builder = ref_of(panel::kBuilder);
    REQUIRE(add_pane(live(t).setup.active, builder));
    t.publish(loom::to_value(surface::SurfaceExtent{80, 22, 0, 0})); // a reconcile
    REQUIRE(has_pane(t.session().setup.active, builder));
    REQUIRE_FALSE(t.session().panels.has(panel::kBuilder)); // authored, and waiting
    press_into_editor(t);
    choose_by_keys(t, ref_of(panel::kPaneEditor));
    type_value(t, "X", "2");
    CHECK_FALSE(t.session().notice_is_bad);
    // THE EDITOR LEFT THE REACTIVE STACK, AND THE WAITING PANE WAS SEATED IN THE SLOT IT
    // VACATED -- which only a reconcile does.
    CHECK(t.session().panels.has(panel::kBuilder));
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kPaneEditor))->place.mode ==
          pane_unit::kSubcells);
}

TEST_CASE("WUX-13/SC-7: a typed value that is not admissible is refused, and the authored row "
          "is untouched") {
    // ⚔ MUTATION (F3): clamping a typed value to the room or to the lattice. Every
    // comparison against `before` below is value identity over the whole desk.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    const PaneRef layouts = ref_of(panel::kLayouts);
    choose_by_keys(t, layouts);
    type_value(t, "X", "10");
    const Setup before = t.session().setup.active;
    REQUIRE(pane_of(before, layouts)->place.x == subs(10));

    type_value(t, "X", "abc");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("X: not a whole number of cells") == 0);
    CHECK(t.session().setup.active == before);
    // THE DRAFT IS STILL OPEN WITH THE MAKER'S TEXT IN IT, so they fix what they typed.
    REQUIRE(editor_row(t, "X")->editing());
    t.key(input::scan::kEscape);
    CHECK_FALSE(editor_row(t, "X")->editing());
    CHECK(t.session().setup.active == before);

    type_value(t, "Width", "99999");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("at most 4096 cells") != std::string::npos);
    CHECK(t.session().setup.active == before);
    t.key(input::scan::kEscape);

    type_value(t, "Height", "0");
    CHECK(t.session().notice_is_bad);
    CHECK(t.session().setup.active == before);
    t.key(input::scan::kEscape);

    type_value(t, "Y", "-7");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("cannot be negative") != std::string::npos);
    CHECK(t.session().setup.active == before);
    t.key(input::scan::kEscape);

    // AN OFF-ROOM VALUE IS NOT CLAMPED EITHER: it is legal authored intent, and the
    // resolved row says what this screen makes of it.
    type_value(t, "X", "500");
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(pane_of(t.session().setup.active, layouts)->place.x == subs(500));
    CHECK(editor_value(t, "State").find("off-room") == 0);
    CHECK(editor_value(t, "Window") == "@500,0 132x2 cells");

    // A RESET OF AN AXIS ALREADY AT THE DEVELOPER'S ANSWER IS REFUSED IN THE OWNER'S WORDS.
    const Setup placed = t.session().setup.active;
    type_value(t, "Width", "-");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("already takes the developer's width") != std::string::npos);
    CHECK(t.session().setup.active == placed);
    t.key(input::scan::kEscape);
}

TEST_CASE("WUX-13/SC-8: looking never authors") {
    // ⚔ MUTATION (F2): an inspection that writes a resolved rectangle back into the setup.
    // Every read below is followed by value identity over the whole desk.
    Live t;
    const Setup born = t.session().setup.active;
    open_editor(t);
    // opening the editor authored one thing -- its own participation -- and nothing else
    Setup expect = born;
    REQUIRE(add_pane(expect, ref_of(panel::kPaneEditor)));
    REQUIRE(t.session().setup.active == expect);
    choose_by_keys(t, ref_of(panel::kLayouts));
    for (const Row& r : t.session().pane_editor.rows) {
        (void)r.value(); // every row, read
    }
    CHECK(t.session().setup.active == expect);
    // THE SCREEN CHANGES; THE RESOLVED ROW CHANGES; THE AUTHORED ROWS DO NOT.
    const std::string small = editor_value(t, "Window");
    t.publish(loom::to_value(surface::SurfaceExtent{160, 60, 0, 0}));
    CHECK(editor_value(t, "Window") != small);
    CHECK(editor_value(t, "Window") == "@0,0 160x2 cells");
    CHECK(editor_value(t, "X") == "-");
    CHECK(t.session().setup.active == expect);
    // THE FACE CHANGES: the same value, spelled in pixels, and nothing written.
    t.publish(loom::to_value(surface::SurfaceExtent{160, 60, 8, 18, surface::kCanvasCellPx}));
    CHECK(editor_value(t, "Window") == "@0,0 1920x24 px");
    CHECK(editor_value(t, "X") == "-");
    CHECK(t.session().setup.active == expect);
    // SELECTING PANES, PRESSING AROUND: still nothing.
    t.press(90, 35);
    press_into_editor(t);
    t.key(input::scan::kTab);
    t.key(input::scan::kDown);
    t.key(input::scan::kUp);
    CHECK(t.session().setup.active == expect);
}

TEST_CASE("WUX-13/SC-9: a closed pane and an unresolved row are subjects with honest facts") {
    // ⚔ MUTATION (F5): dropping the subject when its pane leaves the layout. The subject
    // below is removed and reopened THROUGH THE EDITOR and stands throughout.
    // ⚔ MUTATION (F6): filtering unresolved refs out of the list, or out of the setup.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    const PaneRef files = ref_of(panel::kProjectFiles);
    REQUIRE_FALSE(t.session().panels.has(panel::kProjectFiles));
    choose_by_keys(t, files);
    CHECK(editor_value(t, "State") == "closed -- open it from the picker");
    CHECK(editor_value(t, "Open") == "no -- o opens it");
    CHECK(editor_value(t, "X") == "--");
    CHECK(editor_value(t, "Window") == "-");
    // A CLOSED PANE'S GEOMETRY IS NOT TYPEABLE, and the refusal says what to do.
    type_value(t, "X", "3");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("is not in this layout -- open it first") != std::string::npos);
    t.key(input::scan::kEscape);
    // OPEN IT THROUGH THE EDITOR -- the picker's own door, the editor's own gesture word.
    t.key(input::scan::kO);
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.notice() == "opened Files -- o removes it");
    CHECK(t.session().panels.has(panel::kProjectFiles));
    CHECK(t.session().pane_editor.subject == files);
    CHECK(editor_value(t, "State") == "open");
    CHECK(editor_value(t, "Open") == "yes -- o removes it");
    CHECK(editor_value(t, "X") == "-");
    // ...AND REMOVE IT AGAIN: the subject STANDS, the row is still in the list.
    t.key(input::scan::kO);
    CHECK(t.notice().find("removed Files -- o brings it back") == 0);
    CHECK_FALSE(t.session().panels.has(panel::kProjectFiles));
    CHECK(t.session().pane_editor.subject == files);
    CHECK(editor_value(t, "State") == "closed -- open it from the picker");
    CHECK(inventory_index(t, files) < 99);

    // AN AUTHORED REFERENCE NO OFFICE RESOLVES keeps its identity and its geometry.
    REQUIRE(add_pane(live(t).setup.active, stranger()));
    REQUIRE(author_pane_place(live(t).setup.active, stranger(), subs(7), subs(9)).accepted);
    REQUIRE(author_pane_size(live(t).setup.active, stranger(),
                             PaneSize{pane_unit::kSubcells, subs(30)}, PaneSize{})
                .accepted);
    choose_by_keys(t, stranger());
    CHECK(editor_value(t, "Name") == "history");
    CHECK(editor_value(t, "Identity") == "third.party.tools/history");
    CHECK(editor_value(t, "Provider") ==
          "third.party.tools (unresolved -- no office here offers it)");
    CHECK(editor_value(t, "State").find("unresolved") == 0);
    CHECK(editor_value(t, "X") == "7 cells");
    CHECK(editor_value(t, "Y") == "9 cells");
    CHECK(editor_value(t, "Width") == "30 cells");
    CHECK(editor_value(t, "Height") == "-");
    CHECK(editor_value(t, "Window") == "-");
    // ITS GEOMETRY CANNOT BE TYPED (no base to measure the other axis from)...
    type_value(t, "Y", "1");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("is unresolved") != std::string::npos);
    t.key(input::scan::kEscape);
    // ...BUT ITS ORDER CAN, and the row was never touched. It was added LAST, so it is
    // front-most already; `b` is the order gesture with somewhere to go.
    t.key(input::scan::kB);
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.notice().find("third.party.tools/history back-most") == 0);
    CHECK(has_pane(t.session().setup.active, stranger()));
    CHECK(pane_of(t.session().setup.active, stranger())->place ==
          PanePlace{pane_unit::kSubcells, subs(7), subs(9)});
    CHECK(pane_of(t.session().setup.active, stranger())->front == 0);
    // AND `-` RESETS AN UNRESOLVED PANE'S AXIS, through the reset door.
    type_value(t, "Width", "-");
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(pane_of(t.session().setup.active, stranger())->width == PaneSize{});
    CHECK(has_pane(t.session().setup.active, stranger()));
}

TEST_CASE("WUX-13/SC-10: editing a pane in a layout related to a current Setup makes it "
          "modified") {
    // ⚔ MUTATION (F8): an editor-local dirty bit, or a comparison that stays `current`.
    // The verdict below is `link_status`, derived by comparing the desk to the known value,
    // and the editor holds no flag of its own.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    link_live_setup(live(t).setup, "somewhere.json");
    REQUIRE(live_status(t.session().setup) == setup_link::kCurrent);
    choose_by_keys(t, ref_of(panel::kLayouts));
    // reading is not editing
    for (const Row& r : t.session().pane_editor.rows) {
        (void)r.value();
    }
    CHECK(live_status(t.session().setup) == setup_link::kCurrent);
    type_value(t, "Y", "20");
    CHECK(live_status(t.session().setup) == setup_link::kModified);
    // ...and undoing it by hand makes it current again, because there is no flag.
    type_value(t, "Y", "-");
    CHECK(live_status(t.session().setup) == setup_link::kCurrent);
}

TEST_CASE("WUX-13/SC-12: moving, resizing and closing Layouts through the editor leaves the "
          "reservation alone") {
    // ⚔ MUTATION (F7): coupling `screen_of`'s reservation to the Layouts pane. Every
    // comparison below moves, and so does the document's share basis.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    const Screen before = screen_of(t.session());
    const std::int64_t doc_w = t.session().workspace_w;
    const std::int64_t doc_h = t.session().workspace_h;
    choose_by_keys(t, ref_of(panel::kLayouts));
    type_value(t, "Y", "30");
    CHECK(screen_of(t.session()).room_h == before.room_h);
    CHECK(screen_of(t.session()).room_w == before.room_w);
    type_value(t, "Height", "9");
    type_value(t, "Width", "9");
    CHECK(screen_of(t.session()).room_h == before.room_h);
    CHECK(screen_of(t.session()).room_w == before.room_w);
    t.key(input::scan::kO); // remove it altogether
    REQUIRE_FALSE(t.session().panels.has(panel::kLayouts));
    CHECK(screen_of(t.session()).room_w == before.room_w);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    CHECK(screen_of(t.session()).notice_y == before.notice_y);
    CHECK(t.session().workspace_w == doc_w);
    CHECK(t.session().workspace_h == doc_h);
    // ...and the picker still brings it back, at the developer's default.
    t.key(input::scan::kO);
    CHECK(t.session().panels.has(panel::kLayouts));
}

TEST_CASE("WUX-13/SC-13: a Pane Editor edit survives a restart through the session, and the "
          "subject does not") {
    TempDir dir("wux13-restart");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.host.setup_path = dir.file("s.json");
        t.publish(loom::to_value(surface::SurfaceReady{}));
        t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
        open_editor(t);
        choose_by_keys(t, ref_of(panel::kLayouts));
        type_value(t, "Y", "20");
        REQUIRE_FALSE(t.session().notice_is_bad);
        t.press(90, 35); // the workspace, clear of the stack
        t.key(input::scan::kQ);
        REQUIRE(t.host.quit);
    }
    REQUIRE(std::filesystem::exists(session));
    Live back;
    back.host.session_path = session;
    back.host.setup_path = dir.file("elsewhere.json");
    back.publish(loom::to_value(surface::SurfaceReady{}));
    back.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    const SetupPane* row = pane_of(back.session().setup.active, ref_of(panel::kLayouts));
    REQUIRE(row != nullptr);
    CHECK(row->place == PanePlace{pane_unit::kSubcells, 0, subs(20)});
    CHECK(back.session().panels.has(panel::kPaneEditor));
    CHECK(layouts_body(back.session(), screen_of(back.session())).region_y == 20);
    // THE SUBJECT IS INTERACTION STATE AND IS NOT PERSISTED.
    CHECK_FALSE(back.session().pane_editor.addressed());
    CHECK(back.session().pane_editor.rows.empty());
}

TEST_CASE("WUX-13/SC-15: the Pane Editor can be its own subject, and its own rows do not "
          "retarget it") {
    // ⚔ MUTATION (F9): a subject that follows selection. Every gesture below selects the
    // Pane Editor; the subject is the Pane Editor because it was CHOSEN, and typing into
    // its own X moves the pane the rows are painted in.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    const PaneRef me = ref_of(panel::kPaneEditor);
    choose_by_keys(t, me);
    CHECK(editor_value(t, "Name") == "Pane Manager");
    CHECK(editor_value(t, "Identity") == "zengine.workshop/pane-editor");
    CHECK(editor_value(t, "State") == "open");
    // THE COINCIDENCE IS BROKEN ON PURPOSE: select something else (the workspace: nothing),
    // and the subject is still the Pane Editor -- because it was chosen, not because it
    // was selected. A subject derived from the selection would be nothing here.
    t.press(90, 35);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.session().pane_editor.subject == me);
    press_into_editor(t);
    CHECK(t.session().pane_editor.subject == me);
    const Screen sc = screen_of(t.session());
    const FineRect was =
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc).rect;
    // a press on one of its own rows, then a typed edit of its own place
    REQUIRE(author_pane_size(live(t).setup.active, me, PaneSize{},
                             PaneSize{pane_unit::kSubcells, subs(30)})
                .accepted);
    t.key(input::scan::kTab);
    const ui::Rect b = editor_cells(t);
    t.press_canvas(b.x + 2, b.y); // its own heading: consumed, and pointing nowhere new
    CHECK(t.session().panels.selected == panel::kPaneEditor);
    CHECK(t.session().pane_editor.subject == me);
    type_value(t, "X", "10");
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.session().pane_editor.subject == me);
    const FineRect now =
        bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc).rect;
    CHECK(now.x == subs(10));
    CHECK(now.x != was.x);
    CHECK(editor_value(t, "X") == "10 cells");
    CHECK(editor_value(t, "Window").find("@10,") == 0);
    // the keys are still here, at the new rectangle
    CHECK(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    // ...and it can take itself off the layout; the picker brings it back, subject intact.
    t.key(input::scan::kO);
    CHECK_FALSE(t.session().panels.has(panel::kPaneEditor));
    CHECK(t.session().pane_editor.subject == me);
    CHECK(keyboard_context(t.session()) == KeyContext::kCommand);
    open_pane(t, me);
    CHECK(t.session().panels.has(panel::kPaneEditor));
    CHECK(t.session().pane_editor.subject == me);
    // ...AT THE DEVELOPER'S DEFAULT: a pane taken off a layout loses the row that held its
    // place (P-WORK-08, unchanged by this phase), and the editor says so honestly.
    CHECK(pane_of(t.session().setup.active, me)->place == PanePlace{});
    CHECK(editor_value(t, "X") == "-");
}

TEST_CASE("WUX-13: a typed amount is read and written in the face's own unit") {
    // THE WUX-6 GRAMMAR, READ BACKWARDS: a graphical face spells a pane in pixels and takes
    // pixels back; a cell face does the same in cells; a unit the face did not report is
    // refused rather than converted.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 60, 8, 18, surface::kCanvasCellPx}));
    open_editor(t);
    choose_by_keys(t, ref_of(panel::kLayouts));
    type_value(t, "X", "120");
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.notice() == "committed X = 120 px");
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kLayouts))->place.x == subs(10));
    CHECK(editor_value(t, "X") == "120 px");
    type_value(t, "Y", "10 cells");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice() == "Y: this face reads px, not cells");
    t.key(input::scan::kEscape);
    type_value(t, "Y", "24px");
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kLayouts))->place.y == subs(2));
    // A PIXEL-GRAIN VALUE READ ON A CELL FACE IS MARKED AS A PROJECTION, and typing on that
    // face authors cells.
    type_value(t, "X", "126");
    CHECK(editor_value(t, "X") == "126 px");
    t.publish(loom::to_value(surface::SurfaceExtent{160, 60, 0, 0, 0}));
    CHECK(editor_value(t, "X") == "~10 cells (~ projected)");
    type_value(t, "X", "11 cells");
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(pane_of(t.session().setup.active, ref_of(panel::kLayouts))->place.x == subs(11));
    CHECK(editor_value(t, "X") == "11 cells");
    type_value(t, "X", "3 px");
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice() == "X: this face reads cells, not px");
    t.key(input::scan::kEscape);
}

TEST_CASE("WUX-13: the subject stands across a layout switch, and clears only when nothing "
          "names it") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    REQUIRE(add_pane(live(t).setup.active, stranger()));
    choose_by_keys(t, stranger());
    // A NEW, EMPTY LAYOUT: the stranger is in no setup here and in no catalog -- the one
    // state that clears the subject -- and it clears at the next gesture, saying so.
    t.press(90, 35); // the workspace, clear of the stack
    t.key(input::scan::kEquals); // `layout.new`
    REQUIRE(layout_count(t.session().setup) == 2);
    CHECK(t.session().pane_editor.subject == stranger()); // paint clears nothing
    open_pane(t, ref_of(panel::kPaneEditor));
    press_into_editor(t);
    t.key(input::scan::kDown);
    CHECK_FALSE(t.session().pane_editor.addressed());
    CHECK(t.session().notice_is_bad);
    CHECK(t.notice().find("subject cleared") != std::string::npos);
    // A CATALOG PANE STANDS ACROSS THE SAME SWITCH, as a closed subject on the layout it
    // was taken off and an open one on the layout it is still on.
    choose_by_keys(t, ref_of(panel::kLayouts));
    CHECK(editor_value(t, "State") == "open");
    t.key(input::scan::kO); // off THIS layout only
    CHECK(editor_value(t, "State").find("closed") == 0);
    CHECK(t.session().pane_editor.subject == ref_of(panel::kLayouts));
    t.press(90, 35);
    t.key(input::scan::kComma); // back to the first layout
    REQUIRE(t.session().setup.active_at == 0);
    CHECK(t.session().pane_editor.subject == ref_of(panel::kLayouts));
    press_into_editor(t);
    CHECK(editor_value(t, "State") == "open");
    t.press(90, 35);
    t.key(input::scan::kPeriod); // and forward again: still closed there, still the subject
    REQUIRE(t.session().setup.active_at == 1);
    press_into_editor(t);
    CHECK(t.session().pane_editor.subject == ref_of(panel::kLayouts));
    CHECK(editor_value(t, "State").find("closed") == 0);
}

// ---- QR-18: Escape puts the selected pane down, last; the wheel reaches the editor's lists --

TEST_CASE("QR-18/SC-1+SC-3: Escape clears the ordinary selection last, and the Pane Editor's "
          "subject stands") {
    // MUTATION (F1): removing the final Escape branch -- `selected == kNoPaneKind` below
    // goes red. MUTATION (F2): clearing the subject beside the selection -- the subject
    // check goes red.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_editor(t);
    const PaneRef layouts = ref_of(panel::kLayouts);
    choose_by_keys(t, layouts);
    REQUIRE(t.session().panels.selected == panel::kPaneEditor);
    REQUIRE(t.session().pane_editor.subject == layouts);

    // THE PROMPT'S OWN CASE: the subject is Layouts, the ordinary selection is another pane.
    t.press(90, 35); // the workspace: the picker is command mode's
    open_pane(t, ref_of(panel::kBuilder));
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.session().panels.selected == panel::kBuilder);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kCommand); // the Builder takes no keys
    const std::vector<std::int64_t> order_before = presentation_order(t.session().setup.active, t.session().panels);
    const Setup setup_before = t.session().setup.active;
    const std::size_t panes_before = t.session().panels.open.size();

    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.session().panels.keyboard == kNoPaneKind);
    CHECK(t.session().pane_editor.subject == layouts); // SC-3: the subject is a different fact
    CHECK(t.notice().find("unselected Builder") != std::string::npos);
    // NOTHING ELSE MOVED (SC-10): no pane closed, no rank, no geometry, no file.
    CHECK(t.session().panels.open.size() == panes_before);
    CHECK(presentation_order(t.session().setup.active, t.session().panels) == order_before);
    CHECK(t.session().setup.active == setup_before);
    CHECK(t.session().panels.has(panel::kBuilder));
    CHECK(t.session().panels.has(panel::kPaneEditor));

    // THE SAME WITH THE EDITOR ITSELF SELECTED AND HOLDING THE KEYS: its context binds
    // nothing to Escape, so the selection is what Escape sheds -- and the subject stands.
    press_into_editor(t);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kPaneEditor);
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(keyboard_context(t.session()) == KeyContext::kCommand);
    CHECK(t.session().pane_editor.subject == layouts);

    // WITH NOTHING SELECTED, ESCAPE IS THE NO-OP IT ALWAYS WAS, and says nothing new.
    const std::string notice = t.notice();
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.notice() == notice);
}

TEST_CASE("QR-18/SC-2: every more-specific Escape meaning answers first, and deselection waits") {
    // MUTATION (F3): asking the final fallthrough BEFORE the resolved context -- the
    // picker would still be open, or the draft still live, with the selection already gone.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));

    // THE PICKER: a mode above every pane. Select a pane that takes no keys so `p` still
    // reaches command mode.
    const Screen sc = screen_of(t.session());
    const ui::Rect info =
        cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kInfo, sc).rect);
    t.press_canvas(info.x + 1, info.y + 1);
    REQUIRE(t.session().panels.selected == panel::kInfo);
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().panels.picker.open);      // `picker.close` answered...
    CHECK(t.session().panels.selected == panel::kInfo); // ...and the selection stood
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind); // the next Escape sheds it

    // A LIVE DRAFT in the Pane Editor: `draft.cancel` answers, then the pane, then nothing.
    open_editor(t);
    choose_by_keys(t, ref_of(panel::kLayouts));
    go_to_row(t, "X");
    t.key(input::scan::kReturn);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kDraft);
    REQUIRE(t.session().panels.selected == panel::kPaneEditor);
    t.key(input::scan::kEscape);
    CHECK(keyboard_context(t.session()) == KeyContext::kPaneEditor); // the draft is gone...
    CHECK(t.session().panels.selected == panel::kPaneEditor);        // ...the selection is not
    CHECK(t.session().pane_editor.subject == ref_of(panel::kLayouts));
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.session().pane_editor.subject == ref_of(panel::kLayouts));

    // THE HOTKEY VIEW, keys-modal above everything: Escape closes it and nothing else.
    t.press_canvas(info.x + 1, info.y + 1);
    REQUIRE(t.session().panels.selected == panel::kInfo);
    t.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(t.session().hotkeys.open);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().hotkeys.open);
    CHECK(t.session().panels.selected == panel::kInfo);
}

TEST_CASE("QR-18/SC-4: a desk with no unoccupied cell still reaches selection = none") {
    // THE RECOVERY CLAIM. Every cell between the two bands is some pane's, so there is no
    // blank pixel to press; Escape is the way down.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_pane(t, ref_of(panel::kBuilder));
    const Screen sc = screen_of(t.session());
    // The Builder over the whole room, the side column included -- an authored window is
    // canvas-absolute (WUX-2), and the room is what a pane may cover.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kBuilder),
                              surface::subs_of_cells(0), surface::subs_of_cells(kTopRows))
                .accepted);
    const Written sized =
        author_pane_size(live(t).setup.active, ref_of(panel::kBuilder),
                         PaneSize{pane_unit::kSubcells, surface::subs_of_cells(sc.w)},
                         PaneSize{pane_unit::kSubcells,
                                  surface::subs_of_cells(sc.h - kTopRows - kBottomRows)});
    REQUIRE_MESSAGE(sized.accepted, sized.refusal);
    t.publish(loom::to_value(surface::SurfaceExtent{132, 47, 0, 0})); // reseat
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    const Screen now = screen_of(t.session());
    std::int64_t unoccupied = 0;
    for (std::int64_t y = 0; y < now.h - kBottomRows; ++y) {
        for (std::int64_t x = 0; x < now.w; ++x) {
            if (!occupied_at(t.session().panels, t.session().setup.active, now, x, y).occupied) {
                ++unoccupied;
            }
        }
    }
    REQUIRE(unoccupied == 0); // the desk is covered: top band's Layouts pane, then the Builder

    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kBuilder, now).rect);
    t.press_canvas(builder.x + 3, builder.y + 3);
    REQUIRE(t.session().panels.selected == panel::kBuilder);
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.session().panels.keyboard == kNoPaneKind);
    CHECK(t.session().panels.has(panel::kBuilder)); // still there, still that big
}

TEST_CASE("QR-18/SC-5: the Pane Editor's two lists are reached by the wheel past their windows") {
    // MUTATION (F5): dropping the Pane Editor's wheel arm while the `... N more` row stays
    // -- the hidden name below never appears.
    Live t; // the minimum screen: the sixth built-in makes the PANES list window (WUX-13)
    open_editor(t);
    const std::vector<CatalogRow> inventory =
        inventory_rows(t.session().setup.active, t.session().panels);
    const ui::Rect b = editor_cells(t);
    const Screen sc = screen_of(t.session());
    const auto body = [&] {
        return pane_editor_body(
            t.session(), sc,
            bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor, sc).rect);
    };
    REQUIRE(body().present);
    REQUIRE(body().panes.after > 0); // the list is windowed: `... N more` is painted
    const std::string last_name = inventory.back().name;
    CHECK(editor_text(t).find(last_name) == std::string::npos);
    CHECK(editor_text(t).find(" more") != std::string::npos);

    // THE WHEEL OVER THE PANES LIST WALKS ITS CURSOR, the window follows, the hidden row
    // arrives -- and the keys stay where they were (in the PANES list), the subject unchosen.
    const std::int64_t panes_row = b.y + kPaneEditorHeadingRows; // the list's first row
    for (int i = 0; i < 16; ++i) {
        const std::size_t at = t.session().pane_editor.cursor;
        t.wheel_canvas(-1.0, b.x + 2, panes_row); // toward the maker: later rows
        if (t.session().pane_editor.cursor == at) {
            break; // the tail: the window reached it a step before the cursor did
        }
    }
    CHECK(body().panes.after == 0);
    CHECK(t.session().pane_editor.cursor == inventory.size() - 1);
    CHECK(editor_text(t).find(last_name) != std::string::npos);
    CHECK_FALSE(t.session().pane_editor.on_rows);
    CHECK_FALSE(t.session().pane_editor.addressed());
    // AND BACK TO THE HEAD.
    for (int i = 0; i < 8; ++i) {
        t.wheel_canvas(+1.0, b.x + 2, panes_row);
    }
    CHECK(t.session().pane_editor.cursor == 0);
    CHECK(body().panes.before == 0);

    // THE SUBJECT'S ROWS: choose a subject, and the rows list windows too.
    choose_by_keys(t, ref_of(panel::kLayouts));
    REQUIRE(body().fields.after > 0);
    const std::size_t rows_total = t.session().pane_editor.rows.size();
    const std::string last_row = t.session().pane_editor.rows.back().label();
    CHECK(editor_text(t).find(last_row) == std::string::npos);
    const std::int64_t fields_row =
        b.y + kPaneEditorHeadingRows + static_cast<std::int64_t>(body().panes_rows);
    const std::size_t row_cursor_before = t.session().pane_editor.row_cursor;
    const std::size_t panes_cursor = t.session().pane_editor.cursor; // choosing moved it here
    // Wheel until the row cursor stops: the window reaches the tail a row or two BEFORE the
    // cursor does (`list_window` keeps the cursor inside, not at the edge).
    for (int i = 0; i < 24; ++i) {
        const std::size_t at = t.session().pane_editor.row_cursor;
        t.wheel_canvas(-1.0, b.x + 2, fields_row);
        if (t.session().pane_editor.row_cursor == at) {
            break;
        }
    }
    CHECK(body().fields.after == 0);
    CHECK(t.session().pane_editor.row_cursor == rows_total - 1);
    CHECK(t.session().pane_editor.row_cursor != row_cursor_before);
    CHECK(editor_text(t).find(last_row) != std::string::npos);
    // THE KEYS DID NOT MOVE BETWEEN THE LISTS, and the PANES cursor did not move either.
    CHECK_FALSE(t.session().pane_editor.on_rows);
    CHECK(t.session().pane_editor.cursor == panes_cursor);
    CHECK(t.session().pane_editor.subject == ref_of(panel::kLayouts));

    // THE HEADING SPENDS NOTHING.
    const std::size_t at = t.session().pane_editor.row_cursor;
    t.wheel_canvas(+1.0, b.x + 2, b.y);
    CHECK(t.session().pane_editor.row_cursor == at);
    CHECK(t.session().pane_editor.cursor == panes_cursor);
}
