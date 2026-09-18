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

// ============================================================================
// THE COMPLETER — what the participant can be asked about a line it has not run
//
// ⚠ THESE CASES OUTLIVED THE OVERLAY THEY WERE WRITTEN FOR (VD-24). `complete_line` is
// still this host's: it reads the participant's vocabulary, its shape descriptions and its
// live composition ladder, none of which cross the pane seam (see
// `workshop/terminal_seam_vocabulary.hpp` for why the last of those cannot). What retired
// with the overlay is everything about how the answer is DRAWN -- the list's rows, its
// window and its place -- which is the Terminal pane's, and is pinned in the seam suite.
// These are the completer itself, over a participant and a string, and they are unchanged.
// ============================================================================

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

TEST_CASE("with the bus read the address offers everyone then each office held now then each weave registered now with what it is") {
    // ⭐ REAL DESTINATIONS, BY IDENTITY. The host reads the bus at the ask and hands the completer
    // a value; the completer orders it (offices by name, weaves by id) and says what each one is,
    // so a maker chooses `#7` because it is the timer's office and not because 7 looked right.
    loom::TerminalSession me("t", workshop_vocab());
    const std::vector<Destination> reachable{
        Destination{7, "zengine.timer", {"TimerRequested", "TimerCancelled", "Drive", "Hello"}, true, false},
        Destination{3, "", {"Ack", "Refused"}, true, true},
        Destination{12, "zengine.skin", {"SurfaceText"}, false, false},
    };
    const Completion at = complete_line(me, "send ", &reachable);
    CHECK(displays(at) == std::vector<std::string>{"*", "@zengine.skin", "@zengine.timer", "#3", "#7",
                                                   "#12"});
    CHECK(at.candidates[1].insert == "@zengine.skin ");
    CHECK(at.candidates[3].insert == "#3 ");
    CHECK(at.candidates[1].detail == "held by #12 now; reaches whoever holds it when sent");
    CHECK(at.candidates[3].detail == "no office; accepts Ack, Refused (this terminal)");
    CHECK(at.candidates[4].detail == "@zengine.timer; accepts TimerRequested, TimerCancelled, Drive +1");
    CHECK(at.candidates[5].detail == "@zengine.skin; accepts SurfaceText (dead now)");
    // KNOWING ONE IS NEITHER PERMISSION NOR A PROMISE, and the list says so.
    CHECK(at.heading.find("not permission, nor a promise at send") != std::string::npos);

    // WHAT HAS BEEN TYPED FILTERS BY THE SPELLING, SIGIL INCLUDED.
    CHECK(displays(complete_line(me, "send @zengine.t", &reachable)) ==
          std::vector<std::string>{"@zengine.timer"});
    CHECK(displays(complete_line(me, "send #1", &reachable)) == std::vector<std::string>{"#12"});
    CHECK(displays(complete_line(me, "send #", &reachable)) ==
          std::vector<std::string>{"#3", "#7", "#12"});
    const Completion nobody = complete_line(me, "send #99", &reachable);
    CHECK(nobody.candidates.empty());
    CHECK(nobody.heading == "'#99' is an address; nothing on this bus answers to it now");
    CHECK(complete_line(me, "send @", &reachable).heading.find("where it goes") != std::string::npos);
    CHECK(complete_line(me, "send 12", &reachable).heading.find("#12, @office or *") !=
          std::string::npos);

    // AN EMPTY BUS IS STILL A READING: everyone, and nothing else.
    const std::vector<Destination> none;
    CHECK(displays(complete_line(me, "send ", &none)) == std::vector<std::string>{"*"});
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


TEST_CASE("taking the room says whether anything moved, and a room below the minimum is the minimum") {
    // (It refit the object canvas's workspace to the room as well, until that canvas retired.)
    Session s;
    CHECK(s.screen_w == kScreenMinW);

    CHECK(adopt_screen(s, 100, 33));
    CHECK(s.screen_w == 100);
    CHECK(screen_of(s).room_w == screen_of(100, 33).room_w);
    CHECK(screen_of(s).room_h == screen_of(100, 33).room_h);

    // THE SAME EXTENT AGAIN IS NOT A CHANGE. It is what lets a caller decline to repaint a
    // screen nothing happened to -- and it is what makes the clamps safe to state, because
    // two different extents that clamp to one screen are one screen.
    CHECK_FALSE(adopt_screen(s, 100, 33));
    CHECK_FALSE(adopt_screen(s, 100, 33));

    // Below the minimum is not a smaller screen; it is the minimum.
    CHECK(adopt_screen(s, 4, 4));
    CHECK(s.screen_w == kScreenMinW);
    CHECK(s.screen_h == kScreenMinH);
    CHECK(screen_of(s).room_w == kMinScreen.room_w);
    CHECK_FALSE(adopt_screen(s, 0, 0));
    CHECK_FALSE(adopt_screen(s, -9, -9));
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
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, 100, 27, surface::role::kMuted));
    CHECK(sc.room_w == 100);
    // The workspace fact lives in the band's own row since WUX-1, and it moved with the
    // extent: what a share resolves against is said where the tool speaks. It is the whole
    // surface since the right column stopped being subtracted from it, so the room runs under
    // the panel rather than stopping thirty columns short of the edge.
    CHECK(workspace_row(c, t.session(), sc) == "workspace 100x27 cells");
    // (WHAT STANDS IN THE RIGHT COLUMN USED TO BE ASSERTED HERE, by reading the Info panel's
    // own two headings off the canvas. The pane composes those rows now; what this case is
    // about is where the COLUMN is.)
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
    Session s;
    const surface::SurfaceCanvas c = paint(s);
    CHECK(c.width == 78);
    CHECK(c.height == 22);
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, 78, 16, surface::role::kMuted));
    // THE OBJECTS COLUMN IS NOT IN THIS PICTURE ANY MORE, and its absence is the fallback
    // being honest rather than the fallback shrinking: a default `Session` opens the panels
    // `kDefaultPanels` names, the Info weave is not one of them, and a run with no medium is
    // also a run with no load plan. What this case owes is the COMPOSITION -- 78 by 22, the
    // workspace at its full extent, the bands where they belong -- and every row of it is
    // still asked for below.
    // ⭐ THE TWO HELP ROWS LOST THE OBJECT CANVAS'S KEYS AND KEPT THEIR SHAPE. `n new`,
    // `d delete`, `hjkl move` and `[ ] workspace` retired with the canvas, as `enter edit` and
    // `up/down row` left with the Info panel; what stands is what the keymap composes from
    // what remains.
    CHECK(label_at(c, 0, 19).find("n new") == std::string::npos);
    CHECK(label_at(c, 0, 19).find("q quit") != std::string::npos);
    CHECK(label_at(c, 0, 20).find("[ ] workspace") == std::string::npos);
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

// ============================================================================
// Tier 8 — the dynamic panels (BLD-0)
// ============================================================================
//
// A WEAVE MAY PROVIDE A TOOL; A PANEL IS ITS PRESENTATION. Everything in this
// tier is about that sentence, and the cases are arranged so that the split
// would be visible if it broke:
//
//   the CATALOG is Workshop's own furniture, and pure. (The `p` picker that listed it was too,
//     until it retired: its four cases were here.)
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

TEST_CASE("a stacked panel covers the workspace, and may reach the right column") {
    Live t;
    (void)mount_tool(t, "zengine-snake");
    t.key(input::scan::kEscape); // an unbound key: it repaints and changes nothing

    // WITHOUT A PANEL, the screen carries no stacked rows; the picker's own gesture is
    // said by the band's legend and the hotkey view since WUX-1, not by a row-0 hint.
    const surface::SurfaceCanvas bare = t.canvases.back();
    CHECK(stack_text(bare).empty());

    open_stock_pane(t);
    const surface::SurfaceCanvas with = t.canvases.back();
    const Screen sc = screen_of(t.session());
    // THE BOUNDS THE PLACEMENT PATH GIVES IT, and the rows are read against those
    // rather than against a column this case knows independently. The rows are a region's
    // since WUX-1, so they are read through the cell projection every character medium
    // draws with.
    const ui::Rect stack =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
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
            // ...and the slot reaches INTO the right column's place at this extent, which is
            // what a room that is the whole surface buys the stack (`the-room-is-the-screen`).
            // It is still inside the room, which is the wall that remains.
            CHECK(stack.x + stack.w <= sc.room_w);
        }
    }
    CHECK(stacked_rows == static_cast<std::size_t>(inside.h));
    // AND THE BAND ABOVE IT IS UNTOUCHED BY ANY OF THIS. The OBJECTS and PROPERTIES columns
    // used to be what this line checked; they are a weave's rows now, in a place this run
    // seats nobody in. The Layouts band is the neighbour a stacked panel still has.
    CHECK_FALSE(stack_text(with).empty());
    CHECK(t.session().panels.has(panel::kLayouts));
}

// ---- PNL-1: the places, said once -----------------------------------------------------
//
// WHAT THESE CASES ARE ABOUT is not where the panels are -- the case above and the
// 186 that came before it already pin that, cell by cell. It is WHERE THAT ANSWER COMES
// FROM: a kind declares a place in the catalog, `placement_bounds` turns a place into a
// rectangle on a screen, and each painter is handed the rectangle. Before PNL-1 the same
// answer was arrived at twice, by two painters that each knew a column of their own.
//
// THERE ARE THREE PLACES AND THE BUILT-INS DECLARE TWO OF THEM. The side region is the
// third, and no compile-time kind is placed there any more: it survives as a PLACE a DESK
// ROW can name (`pane_unit::kRightColumn`), which is how the Info weave reaches the right
// edge. A place with no kind in it is exactly the case that would have gone untested when
// the count was "one kind, over there".

TEST_CASE("a panel kind declares its place, and the place resolves to bounds") {
    // THE INTENT IS AUTHORED IN THE CATALOG. It is a fact about the kind, known
    // before anything is open and readable without a screen anywhere near it.
    CHECK(placement_of(stock::kKind) == placement::kOverlayStack);
    CHECK(placement_of(panel::kLayouts) == placement::kTopBand);
    // AND NO BUILT-IN DECLARES THE SIDE REGION AT ALL. Zero is ASSERTED rather than the line
    // deleted: "at most one" would still be true of zero and would go on being true if a row
    // were added back, and the place itself is not retired -- a desk row still names it.
    CHECK(kinds_placed_in(placement::kSideRegion) == 0); // asserted at compile time too
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

    // THE BOUNDS ARE RESOLVED AGAINST A SCREEN, and the side region resolves to a rectangle
    // WITH NO KIND DECLARING IT -- which is the whole of what makes it a place rather than a
    // panel's private constant. A desk row spelled `right-column` lands here.
    const ui::Rect side = placement_bounds(placement::kSideRegion, 0, kMinScreen);
    const ui::Rect stack = placement_bounds(placement::kOverlayStack, 0, kMinScreen);
    CHECK(side == ui::Rect{50, 2, 28, 16});
    CHECK(stack == ui::Rect{0, 2, 63, 9}); // 48 + (78 - 48)/2, over a room that is the surface

    // AND THEY OVERLAP AT THE SMALLEST SCREEN, which is the change: the stack's half-share is
    // measured against a room that no longer stops short of the right column, so its slot runs
    // to 62 and the column begins at 50. One comparison of two rectangles is still what the
    // model buys -- it was a hand-checked relation between four separate constants before,
    // and nothing would have noticed one of them moving.
    CHECK(stack.x + stack.w > side.x);
    CHECK(stack.x + stack.w == 63);
    CHECK(side.x == 50);
    CHECK(side.x + side.w == kMinScreen.w);          // the region reaches the right edge
    CHECK(stack.y + stack.h <= kMinScreen.notice_y); // neither reaches the bottom band
    CHECK(side.y + side.h <= kMinScreen.notice_y);
    // The right column is a PLACE made into a rectangle, and no longer a reservation taken out
    // of the room: the room is the surface, and this column stands on it.
    CHECK(side.x == kMinScreen.panel_x);
    CHECK(side.w == kPanelCols);
    CHECK(kMinScreen.room_w == kMinScreen.w);
    CHECK(side.x + side.w == kMinScreen.room_w);
}

TEST_CASE("each panel is painted where the placement path says it is") {
    // TWO PANELS IN TWO DIFFERENT PLACES, which is what this case needs and what the
    // built-ins still give it: the Layouts band at the top, the Editor in the stack. It was
    // Info and the Editor, in the side region and the stack, until the side region stopped
    // being any kind's.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    const Screen sc = screen_of(t.session());
    const surface::SurfaceCanvas c = t.canvases.back();

    const PanelBounds band =
        bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc);
    const PanelBounds builder =
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc);
    REQUIRE(band.open);
    REQUIRE(builder.open);
    CHECK(band.placed_in == placement::kTopBand);
    CHECK(builder.placed_in == placement::kOverlayStack);
    CHECK(band.rect == fine_of_cells(placement_bounds(placement::kTopBand, 0, sc)));

    // BUILDER'S REGION COMES FROM THE PATH, at the first slot of the stack.
    // ...AND ITS ROWS ARE INSIDE ITS OWN CHROME (WUX-5): the rectangle the path hands it is
    // unchanged, and the boundary it now draws is subtracted from it once.
    const ui::Rect builder_cells = pane_body_cells(builder.rect);
    CHECK(label_at(c, builder_cells.x, builder_cells.y).find("Editor") > 0);
    CHECK(builder.rect == fine_of_cells(placement_bounds(placement::kOverlayStack, 0, sc)));

    // AND THE TWO PLACES DO NOT OVERLAP: the band is above the stack's first slot, which is
    // the relation `placement_bounds` owns and no painter re-derives.
    CHECK(cells_covered(band.rect).y + cells_covered(band.rect).h <=
          cells_covered(builder.rect).y);

    // AND THE STACKED PANEL PAINTS NO ROW OUTSIDE ITS OWN BOUNDS. Every row of it is padded
    // to the width its bounds gave it -- the erasing mechanism following the panel rather
    // than a constant.
    std::size_t seen_stack = 0;
    for (const surface::SurfaceLabel& l : cell_text_of(c)) {
        if (l.x == builder_cells.x && l.y >= builder_cells.y &&
            l.y < builder_cells.y + builder_cells.h) {
            CHECK(l.text.size() == static_cast<std::size_t>(builder_cells.w));
            ++seen_stack;
        }
    }
    CHECK(seen_stack == static_cast<std::size_t>(builder_cells.h));
}

TEST_CASE("a closed panel is not anywhere") {
    // A PANEL THAT IS NOT OPEN HAS NO BOUNDS, and the empty rectangle is the
    // deliberate answer rather than the first slot's: a caller that forgets to
    // ask gets a rectangle that contains nothing, not one that contains the place
    // this panel WOULD have had.
    Live t;
    const Screen sc = screen_of(t.session());
    const PanelBounds absent = bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc);
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
    //
    // THE UNALIKE PANEL IS THE BAND NOW. It was Info in the side region; the pair that still
    // proves the rule is a stacked kind and a kind placed somewhere else, and the Layouts
    // band is the somewhere else this build compiles.
    const Screen sc = kMinScreen;
    Panels band_first;
    admit_stock(band_first); // the stand-in, first (stock)
    band_first.open = {Panel{panel::kLayouts}, Panel{stock::kKind}};
    Panels builder_first;
    admit_stock(builder_first); // the stand-in, first (stock)
    builder_first.open = {Panel{stock::kKind}, Panel{panel::kLayouts}};

    const ui::Rect first_slot = placement_bounds(placement::kOverlayStack, 0, sc);
    CHECK(bounds_of(band_first, setup_for(band_first), stock::kKind, sc).rect ==
          fine_of_cells(first_slot));
    CHECK(bounds_of(builder_first, setup_for(builder_first), stock::kKind, sc).rect ==
          fine_of_cells(first_slot));
    // And the band is in the same rows either way: the top band has no slots.
    CHECK(bounds_of(band_first, setup_for(band_first), panel::kLayouts, sc).rect ==
          bounds_of(builder_first, setup_for(builder_first), panel::kLayouts, sc).rect);
}

// (...and four more: `"a painter goes where its bounds say, not where a constant says"` and
// `"Info is open at boot, and it is a panel rather than furniture"` were about `paint_info` and
// about a built-in that is no longer one; `"the inspector's keys say so when Info is not
// showing, and open no draft"` was about three command-mode rows that left with the pane; and
// `"TUI-0: a terminal resize is presentation context, never an authored act"` proved it by
// watching the inspector's cursor and draft survive a resize. That a resize authors nothing is
// still pinned, by the document suite's own resize cases and by `"the surface says how much
// room it has"` directly above.)

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

TEST_CASE("WIND-1: the right column keeps its width and the stack takes half the surplus") {
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
    // room: 100 columns of surface IS a room of 100 since the right column stopped being
    // subtracted from it, a surplus of 52 over the slot's floor, and half of that is 26.
    const ui::Rect min_stack = placement_bounds(placement::kOverlayStack, 0, kMinScreen);
    CHECK(stack.x == min_stack.x);
    CHECK(stack.y == min_stack.y);
    CHECK(stack.h == min_stack.h);
    CHECK(stack.w == 74);
    CHECK(stack.w > min_stack.w);
    // ...and it stops inside the ROOM rather than short of the right column, which is the one
    // wall that survived the reservation's retirement. At this extent the slot's last two
    // columns are over the right column's place, which is what a room that is the whole
    // surface buys the stack -- and is legible, because a panel wears a boundary. The screen
    // suite measures the same overlap for the other overlay, which does not
    // (`"HD-10: the terminal pane now covers the right column, measured"`).
    CHECK(stack.x + stack.w <= big.room_w);
    CHECK(stack.x + stack.w == side.x + 2);

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
            // NEVER PAST THE ROOM -- and the room is the surface now, so this is the only
            // wall left. It used to be said three times, twice against the reserved column;
            // the column reserves nothing, so a slot may reach it and the two extra
            // comparisons said something that is no longer true.
            CHECK(b.x + b.w <= sc.room_w);
            // AND THE MAKER KEEPS THE OTHER HALF -- unconditionally now. The guard was here
            // because at the minimum screen the room WAS `kStackW` and the slot was the whole
            // of it; the room is thirty columns wider than the slot's floor at every extent,
            // so a column of the panel's own rows is always the maker's to press.
            CHECK(sc.room_w > kStackW);
            CHECK(b.x + b.w < sc.room_w);
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
    for (const Witness& row : std::vector<Witness>{{78, 22, 78, 63, 15},
                                                   {79, 22, 79, 63, 16},
                                                   {96, 22, 96, 72, 24},
                                                   {120, 40, 120, 84, 36},
                                                   {200, 60, 200, 124, 76},
                                                   {640, 400, 640, 344, 296}}) {
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

TEST_CASE("WIND-1: the half-share pays at the bottom of the range too, and buys no slot") {
    // THE PRICE OF THE HALF-SHARE AT THE BOTTOM OF THE RANGE WAS ZERO, AND IS NOT ANY MORE.
    // At 78x22 the room WAS kStackW, the surplus was nothing, and the slot was the whole of
    // the room -- the one extent where the rule's own promise, that a column of the panel's
    // rows stays reachable, bought the maker nothing. The room is the surface now, so the
    // surplus at the smallest screen is thirty and the slot takes fifteen of it.
    CHECK(kMinStack == ui::Rect{0, 2, 63, 9});
    CHECK(placement_bounds(placement::kOverlayStack, 0, kMinScreen) == ui::Rect{0, 2, 63, 9});
    CHECK(kMinStack.x + kMinStack.w < kMinScreen.room_w);
    CHECK(kMinScreen.room_w - (kMinStack.x + kMinStack.w) == 15);
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

// ============================================================================
// Tier 9 -- a panel that is nobody's weave (PNL-0)
// ============================================================================
//
// THIS TIER WAS INFO'S, AND THEN THE HOST PANE MANAGER'S, AND ITS SUBJECT OUTLIVED BOTH. What
// it measured was the difference a SECOND built-in kind makes -- a presentation with no state of
// its own, removed and reopened whole -- and both second kinds became weaves (Info) or the
// desktop's pane (the Pane Manager). Layouts is the one built-in left; what stays true of it is
// the category claim below: a built-in needs no weave, and opening one speaks to no office.

TEST_CASE("a built-in panel needs no weave, and opening one speaks to no office") {
    // NOTHING IS MOUNTED IN THE BUILDER OFFICE, and nothing else is either. If
    // being a panel required a tool behind it, this is the case that could not
    // pass.
    //
    // THE CLAIM MOVED FROM A KIND TO A CATEGORY. It used to read "Info needs no weave to be
    // a panel", and Info needs one now -- it IS one. What was never about Info is that a
    // BUILT-IN needs none, which is the whole reason the catalog and the runtime catalog are
    // two lists (`combined_catalog`) rather than one.
    Live t;
    REQUIRE(t.w->session().panels.has(panel::kLayouts)); // the shipped desk opens it
    pick(t, panel::kLayouts); // the close door
    CHECK_FALSE(t.w->session().panels.has(panel::kLayouts));
    pick(t, panel::kLayouts); // the launch door
    CHECK(t.w->session().panels.has(panel::kLayouts));
    CHECK(panel_shown(t.canvases.back(), t.session(), panel::kLayouts).find("Default") !=
          std::string::npos);

    // AND NO MESSAGE WENT TO THE ONE OFFICE WORKSHOP KNOWS ABOUT. With the
    // stand-in mounted, closing and opening the panel leaves its counters at zero.
    Live u;
    ToolSeat* tool = mount_tool(u, "zengine-snake");
    pick(u, panel::kLayouts);
    pick(u, panel::kLayouts);
    pick(u, panel::kLayouts);
    CHECK(tool->described == 0);
    CHECK(tool->asked.empty());
}

TEST_CASE("x is an unbound key again") {
    // BLD-0 bound it to "close the Builder"; the second kind made that a choice
    // the key could not make, so presence moved to the picker and this went back
    // to meaning nothing. A key that still half-worked would be the worst of the
    // three available outcomes. (Presence is the desktop Pane Manager's now, where `x` closes the
    // pane ITS list is on -- that pane's own row, and command mode's `x` still means nothing.)
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_stock_pane(t);
    REQUIRE(t.w->session().panels.has(stock::kKind));
    const std::string notice = t.w->session().notice;

    t.key(input::scan::kX);
    CHECK(t.w->session().panels.has(stock::kKind));
    CHECK(t.w->session().panels.has(panel::kLayouts)); // and the desk's other panel stands
    CHECK(t.w->session().notice == notice); // it said nothing, because it means nothing
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



} // namespace

// ⭐ THE INFO PANEL'S OWN SUITE LEFT THIS FILE WITH THE PANEL, AND IT WAS THE LARGEST SET IN
// THE ARC. Fifty-odd cases stood here about a presentation this host no longer makes:
//
//   HD-6  the property body under a real face -- its rows, its fallback to cells, its
//         press-to-property inverse, and a panel with no room for a body at all;
//   HD-7  the OBJECTS list -- its share of the body, its window, its omission marker, its two
//         row maps, long names, duplicate names, an empty document, presses on rows, and both
//         media's spellings of the same row;
//   HD-8  the two bracketed controls -- where they sit, their inverses, availability said in
//         characters, Create and Delete as the same operations the keys perform, and what a
//         live draft does to both;
//   TUI-0 more terminal is more Inspector, and the marker still tells the truth;
//   WUX-7 hovering a clipped object row to read past its ellipsis, and a double-click in a
//         property draft.
//
// WHERE EACH CLAIM LIVES NOW. All of it is `Zengine/info-pane/pane.cpp`'s composition, and the
// pane's own cases are in `tests/test_workshop_panes_info.cpp`, driven through the real loaded
// image and the pane protocol -- which is a stronger place for them than here, because they now
// cross a seam a maker's own weave could cross.
//
// ⚠ AND ONE FAMILY HAS NO HOME, WHICH IS THE HONEST PART. The WUX-7 hover cases measured
// reading past an ellipsis, and that feature retired with the panel: the pane protocol has no
// hover, and adding one so this host could keep the feature is the host-mapped route VD-22
// refuses. `screen_reveal.cpp` carries the same sentence beside the code that left.

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

TEST_CASE("WUX-4: a healthy Workshop says nothing on the attention slot at all") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    CHECK(t.conditions().empty());
    CHECK(t.attention_note().empty()); // EMPTY IS THE RETRACTION, and it is also the floor
    // ...AND THE ANSWER IS STILL SAID OUT LOUD. "Is anything wrong?" is a question a maker
    // is entitled to ask when the answer is no, and the empty chip is not an answer -- so
    // the seam carries one publication with no rows in it, which is what the pane turns
    // into `nothing needs your attention right now`. The view that used to say it here is
    // a weave now (`tests/test_workshop_panes_attention.cpp`).
    REQUIRE_FALSE(t.said_conditions.empty());
    CHECK(t.said_conditions.back().rows.empty());
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
    CHECK(attention_compact(attention_conditions(s)) == "a wall");

    // AN UPDATE UNDER THE SAME KEY IS ONE CONDITION, not a second row.
    s.conditions.establish(Condition{"test.wall", "the same wall", "a better reason",
                                     surface::role::kAlert, std::string()});
    REQUIRE(attention_conditions(s).size() == 1);
    CHECK(attention_conditions(s).front().detail == "a better reason");

    // AND IT GOES BECAUSE ITS OWNER SAID SO, by name.
    s.conditions.retract("test.wall");
    CHECK(attention_conditions(s).empty());
    CHECK(attention_compact(attention_conditions(s)).empty());
    // Retracting what was never established is silence rather than an error.
    s.conditions.retract("test.wall");
    CHECK(attention_conditions(s).empty());
}

TEST_CASE("WUX-4: a derived condition enters and leaves attention with its subject") {
    // FALSIFIER 4 -- a derived condition that is latched: the underlying state resolves
    // and the copy stays. There is no copy, so there is nothing to go stale: the pane's
    // own state IS the condition, and NOTHING in this case calls a retraction.
    Session s;
    admit_stock(s.panels); // the stand-in, first (stock)
    admit_second(s.panels); // ...and the second
    s.setup.active = two_overlays();
    s.panels.open = {Panel{stock::kKind}, Panel{second::kKind}};
    const Screen sc = screen_of(s);
    const PaneRef builder = ref_of(stock::kKind);
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
    admit_stock(s.panels); // the stand-in, first (stock)
    admit_second(s.panels); // ...and the second
    s.setup.active = two_overlays();
    s.panels.open = {Panel{stock::kKind}, Panel{second::kKind}};
    const Screen sc = screen_of(s);
    const PaneRef builder = ref_of(stock::kKind);

    // OPEN: nothing is wrong.
    REQUIRE(pane_state_of(s.panels, s.setup.active, sc,
                          CatalogRow{stock::kKind, builder, "Editor", ""}) ==
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
    admit_stock(closed.panels); // the stand-in, first (stock)
    admit_second(closed.panels); // ...and the second
    closed.setup.active = setup_of("Second only", {second::kKind});
    closed.panels.open = {Panel{second::kKind}};
    REQUIRE(pane_state_of(closed.panels, closed.setup.active, screen_of(closed),
                          CatalogRow{stock::kKind, builder, "Editor", ""}) ==
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

    // AN ORDINARY EVENT SENTENCE DOES NOT BECOME A CONDITION. (The sentences were the object
    // canvas's `created`, `deleted` and the workspace's width until it retired; these are the
    // layout run's.)
    t.key(input::scan::kEquals);
    REQUIRE_FALSE(t.notice().empty());
    const std::size_t conditions_now = t.conditions().size();
    t.key(input::scan::kEquals);
    t.key(input::scan::kComma);
    t.key(input::scan::kPeriod);
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
        CHECK_FALSE(t.session().context.open);
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

    // AND EXACTLY ONE SHAPE, WHICH IS THE SEAM'S (WL-ATTN-11, WL-ATTN-12). The internal
    // `Condition` is still a value on the session with no wire form; what a Recorder in this
    // process can see is the SENTENCE the host says about what is true, and it can see it
    // because saying it is the whole point. This half of the case used to assert that
    // nothing condition-shaped reached the bus at all, and the Attention pane's migration
    // made that false rather than weaker: the claim is now that the seam's shape is the ONLY
    // one, so nothing has quietly gained a second wire form beside it.
    std::size_t said = 0;
    for (const loom::ShapeTally& tally : history.tallies()) {
        if (tally.shape == StandingConditions::zen_name) {
            ++said;
            continue;
        }
        CHECK_MESSAGE(tally.shape.find("ondition") == std::string::npos,
                      "a condition reached the bus as shape ", tally.shape);
        CHECK_MESSAGE(tally.shape.find("ttention") == std::string::npos,
                      "attention reached the bus as shape ", tally.shape);
    }
    CHECK(said == 1); // the seam's own, and nothing else wearing the word
}

TEST_CASE("WUX-4: a condition names an action and what crosses is the maker's own gesture") {
    // FALSIFIER 8 -- a displayed action gaining authority. The condition holds an
    // `ActionRow::id` and nothing else; what CROSSES is that action's current gesture,
    // resolved against the effective keymap at the moment the host says it -- so an id
    // never leaves this process and there is nothing on the far side to execute.
    //
    // ⚠ THE RESOLUTION MOVED, AND THAT IS THE WHOLE CHANGE. The built-in's painter looked
    // the gesture up per paint, inside the host, and drew it. The pane cannot: a keymap is
    // the host's and a maker may have moved the key. So the host resolves it once at the
    // seam and sends prose -- which makes this claim STRONGER than it was, because before,
    // the id was one lookup away from the thing that drew it, and now it never crosses.
    TempDir dir("wux4-action");
    const std::string path = dir.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"workshop.manage", "y"}}));
    Keyed t(path);
    Session& s = const_cast<Session&>(t.session());
    s.conditions.establish(Condition{"test.thing", "a thing", "why it is a thing",
                                     surface::role::kAlert, "workshop.manage"});
    t.publish(loom::to_value(surface::SurfaceReady{}));

    // THE SENTENCE THAT CROSSES CARRIES THE MAKER'S GESTURE, not the default: one truth,
    // projected once.
    REQUIRE_FALSE(t.said_conditions.empty());
    REQUIRE(t.said_conditions.back().rows.size() == 1);
    const StandingCondition& said = t.said_conditions.back().rows[0];
    CHECK(said.suggestion == "try: y arrange desk");
    CHECK(said.suggestion.find("try: w") == std::string::npos);
    CHECK(said.suggestion.find("workshop.manage") == std::string::npos);

    // AND PRESSING IT SOMEWHERE ELSE IS AN ORDINARY PRESS OF AN ORDINARY ROW, which is what
    // the condition said it would be. Nothing about the condition made it happen and
    // nothing about it could have.
    CHECK_FALSE(t.session().arrange.open);
    t.key(input::scan::kY);
    CHECK(t.session().arrange.open);
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
    const std::vector<Condition> shown = attention_conditions(s);
    REQUIRE(shown.size() == 3);
    CHECK(shown[0].key == "z.loud");  // loudest first, whatever its key
    CHECK(shown[1].key == "a.quiet"); // then the key, so the order cannot wobble
    CHECK(shown[2].key == "b.quiet");
    CHECK(attention_compact(shown) == "a loud thing (+2 more)");

    // ONE CONDITION SAYS NO COUNT AT ALL -- a bound that announces itself when there is
    // nothing to bound is noise.
    s.conditions.retract("a.quiet");
    s.conditions.retract("b.quiet");
    CHECK(attention_compact(attention_conditions(s)) == "a loud thing");

    // AND RANKING IS TOTAL OVER A ROLE THIS VOCABULARY DOES NOT HAVE YET.
    CHECK(attention_rank(surface::role::kAlert) < attention_rank(surface::role::kAccent));
    CHECK(attention_rank(surface::role::kAccent) < attention_rank(surface::role::kFill));
    CHECK(attention_rank(surface::role::kFill) < attention_rank(9999));
}

TEST_CASE("WUX-4: what is true is said across the seam, in the host's own order and words") {
    // ⭐ THE ARC'S ONE NEW HOST-TO-PANE SENTENCE (WL-ATTN-12). The pane that shows these
    // rows derives none of them and could not: they are this host's reading of this host's
    // own state. So the host says them -- ranked, `to_any`, with the action already resolved
    // into the words a maker reads, because resolving it needs the effective keymap and a
    // loaded image cannot see one.
    //
    // ⚔ MUTATION, MEASURED: drop the `say_conditions` call from `repaint`. Nothing is ever
    //   said, so the case stops at its first line -- `REQUIRE_FALSE(said_conditions.empty())`
    //   is fatal and the rest never runs, which is the honest shape of "the seam is silent".
    Live t;
    Session& s = const_cast<Session&>(t.session());
    s.conditions.establish(Condition{"b.quiet", "a quiet thing", "why it is quiet",
                                     surface::role::kAccent, std::string()});
    s.conditions.establish(Condition{"z.loud", "a loud thing", "why it is loud",
                                     surface::role::kAlert, "workshop.manage"});
    t.publish(loom::to_value(surface::SurfaceReady{}));
    REQUIRE_FALSE(t.said_conditions.empty());
    const StandingConditions& said = t.said_conditions.back();
    REQUIRE(said.rows.size() == 2);

    // THE ORDER IS THE HOST'S AND IT CROSSES ALREADY APPLIED (WL-ATTN-07): loudest first,
    // then the key. A pane that had to rank would be a second place this application decides
    // what is urgent.
    CHECK(said.rows[0].key == "z.loud");
    CHECK(said.rows[1].key == "b.quiet");
    CHECK(said.rows[0].compact == "a loud thing");
    CHECK(said.rows[0].detail == "why it is loud");
    CHECK(said.rows[0].role == surface::role::kAlert);

    // ...AND THE ACTION CROSSES AS PROSE, NOT AS A NAME. What the pane is handed is the
    // sentence the built-in's painter composed, resolved through the keymap in force -- so
    // an id never reaches the far side and nothing over there could press one if it did.
    CHECK(said.rows[0].suggestion.rfind("try: ", 0) == 0);
    CHECK(said.rows[0].suggestion.find("workshop.manage") == std::string::npos);
    CHECK(said.rows[1].suggestion.empty()); // a condition that names no action suggests none
}

TEST_CASE("WUX-4: nothing new is nothing said, which is what stops the seam looping") {
    // ⭐ THE SILENCE IS LOAD-BEARING, and it is measured rather than assumed. A pane that
    // hears a publication says its rows; `on(PaneContent)` ends in a repaint; a repaint that
    // published unconditionally would say it again, and this process would have no quiet
    // state. So the host compares what it is about to say against its own last utterance.
    //
    // ⚔ MUTATION, MEASURED: drop the `same_conditions` arm from `say_conditions`. Two
    //   assertions go red -- the count climbs across three repaints with no news in them, and
    //   the one that follows real news is then off by the difference.
    Live t;
    t.publish(loom::to_value(surface::SurfaceReady{}));
    const std::size_t after_first = t.said_conditions.size();
    REQUIRE(after_first >= 1); // even "nothing is wrong" is said once, and it is an answer

    // REPAINTS WITH NO NEWS IN THEM, and there are several: a keystroke that moves a cursor
    // repaints, and nothing about what is TRUE changed.
    t.key(input::scan::kDown);
    t.key(input::scan::kUp);
    t.key(input::scan::kDown);
    CHECK(t.said_conditions.size() == after_first);

    // ...AND NEWS IS SAID THE ONCE. One condition arrives, one publication follows it.
    Session& s = const_cast<Session&>(t.session());
    s.conditions.establish(Condition{"test.wall", "a wall", "why it is a wall",
                                     surface::role::kAlert, std::string()});
    t.key(input::scan::kDown);
    REQUIRE(t.said_conditions.size() == after_first + 1);
    CHECK(t.said_conditions.back().rows.size() == 1);
    t.key(input::scan::kUp);
    CHECK(t.said_conditions.size() == after_first + 1);

    // AND SO IS ITS RESOLUTION: the last condition retracting is one publication with no
    // rows in it, which is the retraction the compact chip makes with an empty string.
    s.conditions.retract("test.wall");
    t.key(input::scan::kDown);
    REQUIRE(t.said_conditions.size() == after_first + 2);
    CHECK(t.said_conditions.back().rows.empty());
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
    const std::int64_t keyboard_before = t.session().panels.keyboard;
    REQUIRE_FALSE(t.session().arrange.addressed());

    SUBCASE("on a pane: the durable reference, and no selection of any kind") {
        open_pane(t, ref_of(stock::kKind));
        const ui::Rect slot = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                      screen_of(t.session()))
                .rect);
        const std::int64_t selected_before = t.session().panels.selected;
        t.right_press_canvas(slot.x + 1, slot.y + 1);
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kPane);
        CHECK(t.menu().pane == ref_of(stock::kKind));
        CHECK(t.session().panels.selected == selected_before);
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.session().arrange.addressed());
        CHECK(t.session().panels.keyboard == keyboard_before);
    }
    // (A DOCUMENT OBJECT WAS A SUBJECT HERE, by its identity, until the object canvas retired:
    // what stood where #2 was is the room now.)
    SUBCASE("on the empty room: a real subject with no identity") {
        t.right_press(40, 0);
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kRoot);
    }
    SUBCASE("a further right press re-targets instead of toggling") {
        open_pane(t, ref_of(stock::kKind));
        const Screen sc = screen_of(t.session());
        const ui::Rect slot = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, stock::kKind, sc).rect);
        // A ROOM CELL THE PANE DOES NOT COVER: right of it and below it, inside the room.
        const std::int64_t bare_x = slot.x + slot.w + 1;
        const std::int64_t bare_y = slot.y + slot.h + 1;
        REQUIRE(bare_x < kWorkspaceX + sc.room_w);
        REQUIRE(bare_y < kWorkspaceY + sc.room_h);
        REQUIRE_FALSE(
            occupied_at(t.session().panels, t.session().setup.active, sc, bare_x, bare_y).occupied);
        t.right_press_canvas(bare_x, bare_y);
        REQUIRE(t.menu().subject == context_subject::kRoot);
        t.right_press_canvas(slot.x + 1, slot.y + 1);
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kPane);
        CHECK(t.menu().pane == ref_of(stock::kKind));
    }
}

TEST_CASE("CTX-0: the declared populations are the researched ones, keyed by id") {
    // The pane's top level since ARR-0: ONE arrangement entry -- moving and resizing are
    // one maker intent -- then two groups at their first members' positions, then Edit Code
    // (a different intent: what the pane is, not where it sits), and remove, last.
    // Groups appear ONCE, and an empty group is structurally impossible (a group entry
    // exists only where a member declared it).
    const std::vector<ContextEntry> pane = context_population(context_subject::kPane, "");
    REQUIRE(pane.size() == 5);
    CHECK_FALSE(pane[0].is_group);
    CHECK(pane[0].row->act == Act::kArrange);
    CHECK(pane[1].is_group);
    CHECK(std::string(pane[1].group) == "Order");
    CHECK(pane[2].is_group);
    CHECK(std::string(pane[2].group) == "Reset");
    CHECK_FALSE(pane[3].is_group);
    CHECK(pane[3].row->act == Act::kEditCode);
    CHECK(pane[4].row->act == Act::kManageRemove);

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

    // (AN OBJECT'S POPULATION WAS HERE -- deletion, its one row -- until the object canvas
    // retired; no subject kind names an object now.)

    // The room: FIVE zero-target doors, no groups. It was eleven until the two overlays became
    // panes -- `workshop.attention` and then `workshop.terminal` each opened one particular
    // overlay from the empty room -- nine until the key list became the desktop's pane, and
    // eight until `object.new`, `document.save` and `document.open` retired with the canvas, and
    // five until `workshop.picker` ("+ panel") retired with the picker.
    const std::vector<ContextEntry> root = context_population(context_subject::kRoot, "");
    REQUIRE(root.size() == 4);
    for (const ContextEntry& e : root) {
        CHECK_FALSE(e.is_group);
    }
    CHECK(root[0].row->act == Act::kArrangeDesk);
    CHECK(root[3].row->act == Act::kManageResetOrder);

    // EVERY DECLARATION RESOLVES AND OWNS NO POWER: an id `row_of_id` answers and three
    // plain fields -- the compile-time cross-check, restated where a reader looks.
    for (const ContextRow& row : kContextCatalog) {
        CHECK(row_of_id(row.action) != nullptr);
    }
}

TEST_CASE("CTX-0: a contextual action acts on the pointed pane, not the selection") {
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const std::int64_t selected_before = t.session().panels.selected;
    // Info, the Layouts pane, and the Builder this case just opened -- the identity
    // permutation `add_pane` assigns, in list order.
    REQUIRE(ranks_of(t.session().setup.active) == std::vector<std::int64_t>{0, 1, 2});

    // Point at the BUILDER and send it to the back through the Order group.
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);
    REQUIRE(t.menu().pane == ref_of(stock::kKind));
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
    CHECK(t.session().panels.selected == selected_before);
    CHECK(t.notice().find("back-most") != std::string::npos);
    CHECK(t.notice().find(ref_text(ref_of(stock::kKind))) != std::string::npos);
}

TEST_CASE("CTX-0/ARR-0: contextual Arrange admission precedes binding") {
    Live t;
    // TWO PANES, ON A SCREEN THAT SEATS TWO. The first subcase captures a pane and then
    // pushes THAT pane off the screen, so it needs a second pane to point at -- and both
    // built-ins are in the stack now, which the minimum screen has one slot of.
    t.publish(loom::to_value(surface::SurfaceExtent{120, 44, 0, 0}));
    open_pane(t, second_ref());
    open_pane(t, ref_of(stock::kKind));

    SUBCASE("a refused entry establishes nothing") {
        // ⚠ THE REFUSAL THIS SUBCASE DROVE IS RETIRED, AND THE ONES THAT REMAIN ARE BLIND.
        // It right-pressed Info and read back "is in the reserved side column -- the screen
        // owns its place"; nothing is reserved now (`the-room-is-the-screen`) and Info is
        // arranged by the same keys as every other pane. Every refusal
        // `arrange_geometry_ready` still makes belongs to a pane with NO RECTANGLE -- absent,
        // unresolved, sized in pixels, or off the screen -- so none of them can be reached by
        // pointing at all. The subject has to be captured and only then made unreachable,
        // which is the very hazard the captured subject exists for and is a better witness
        // than the one it replaces.
        const ui::Rect side = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, second::kKind,
                      screen_of(t.session()))
                .rect);
        t.right_press_canvas(side.x + side.w - 1, side.y + 1);
        REQUIRE(t.menu().subject == context_subject::kPane);
        REQUIRE(t.menu().pane == second_ref());
        // ...and the pane the menu is holding goes off this screen before the key lands.
        REQUIRE(author_pane_place(live(t).setup.active, second_ref(),
                                  surface::subs_of_cells(kScreenMaxW),
                                  surface::subs_of_cells(kScreenMaxH))
                    .accepted);
        t.key(input::scan::kReturn); // the top row is Arrange
        CHECK_FALSE(t.menu().open);
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.session().arrange.addressed());
        CHECK(t.session().notice_is_bad);
        CHECK(t.notice().find("off this screen") != std::string::npos);
    }
    SUBCASE("an accepted entry binds exactly the pointed pane, and one state carries "
            "both manipulations") {
        const ui::Rect slot = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                      screen_of(t.session()))
                .rect);
        t.right_press_canvas(slot.x + 1, slot.y + 1);
        t.key(input::scan::kReturn); // Arrange
        CHECK_FALSE(t.menu().open);
        CHECK(t.session().arrange.open);
        CHECK_FALSE(t.session().arrange.desk); // the ONE-PANE scope, not the old selector
        CHECK(t.session().arrange.pane == ref_of(stock::kKind));
        // MOVING AND RESIZING THE SAME PANE NEED NO STATE CHANGE IN BETWEEN (ARR-0):
        // an arrow places it and a shifted arrow resizes it, in the state already open.
        t.key(input::scan::kRight);
        const SetupPane* placed = pane_of(t.session().setup.active, ref_of(stock::kKind));
        REQUIRE(placed != nullptr);
        CHECK(placed->place.mode == pane_unit::kSubcells);
        t.key(input::scan::kRight, input::mod::kShift);
        const SetupPane* sized = pane_of(t.session().setup.active, ref_of(stock::kKind));
        CHECK(sized->width.mode == pane_unit::kSubcells);
        CHECK(t.session().arrange.open); // still the one state, nothing was left or entered
        CHECK_FALSE(t.session().arrange.desk);
    }
}

TEST_CASE("CTX-0: a captured pane that left the setup is refused truthfully") {
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().pane == ref_of(stock::kKind));
    // The reference leaves the setup UNDER the open surface -- the clearing that keeps
    // the mode's own selection fresh does not know this subject exists.
    REQUIRE(remove_pane(live(t).setup.active, ref_of(stock::kKind)));

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
    open_pane(t, ref_of(stock::kKind));
    enter_arrange_desk(t);
    select_pane(t, ref_of(stock::kKind));
    t.key(input::scan::kD);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(stock::kKind)));
    // The presentation followed the intent through the one door, and the removed
    // reference cleared the keyboard's address on membership -- the DESK stays open,
    // because its subject is the desk and the desk is still there (ARR-0).
    for (const Panel& p : t.session().panels.open) {
        CHECK(p.kind != stock::kKind);
    }
    CHECK(t.session().arrange.open);
    CHECK(t.session().arrange.desk);
    CHECK_FALSE(t.session().arrange.addressed());
    CHECK(t.notice().find("removed") != std::string::npos);
    CHECK(t.notice().find("nothing behind it was touched") != std::string::npos);
}

TEST_CASE("CTX-0: a contextual remove removes the pointed pane") {
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    t.key(input::scan::kUp); // the cursor bound keeps it at the top; up is a no-op
    t.key(input::scan::kDown);
    t.key(input::scan::kDown);
    t.key(input::scan::kDown); // edit code
    t.key(input::scan::kDown); // remove, the last top-level row
    t.key(input::scan::kReturn);
    CHECK_FALSE(t.menu().open);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(stock::kKind)));
    for (const Panel& p : t.session().panels.open) {
        CHECK(p.kind != stock::kKind);
    }
    CHECK(t.notice().find("removed") != std::string::npos);
}

TEST_CASE("CTX-0: navigation backtracks cleanly and every way out closes") {
    Live t;
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
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
    SUBCASE("the keyboard door opens on what command mode can name: the room") {
        // (It opened on the selected object while one resolved, until the object canvas
        // retired.)
        t.key(input::scan::kA);
        t.text("a");
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kRoot);
    }
}

TEST_CASE("CTX-0: input spent on the open surface does not leak through it") {
    Live t;
    t.right_press(40, 0); // the room's menu
    REQUIRE(t.menu().open);
    REQUIRE(t.menu().subject == context_subject::kRoot);

    SUBCASE("navigation keys move the surface's cursor and nothing beneath") {
        t.key(input::scan::kDown);
        t.key(input::scan::kDown);
        CHECK(t.menu().cursor == 2);
        // (THE INSPECTOR'S CURSOR USED TO BE ASKED HERE TOO. It is the Info weave's own now, and
        // the surface's keys cannot reach a pane that does not hold the keyboard -- which is a
        // stronger statement of the same claim and is the pane's own case.)
        CHECK_FALSE(t.session().arrange.open);
    }
    SUBCASE("a press outside dismisses, is consumed, and operates nothing") {
        const std::int64_t selected_before = t.session().panels.selected;
        const std::string notice_before = t.notice();
        // A cell left of the popup's own derived rectangle (the bounds are the press
        // resolver's too, so reading them here is the one geometry, not a second guess);
        // without the surface this press would be answered by whatever occupies it, or
        // by the room -- with it open, the press is spent whole on dismissal.
        const FineRect b = context_bounds(t.session(), screen_of(t.session()));
        REQUIRE(surface::cell_of_subs(b.x) >= 2); // the anchored popup sits right of here
        t.press_canvas(surface::cell_of_subs(b.x) - 2, surface::cell_of_subs(b.y));
        CHECK_FALSE(t.menu().open);
        CHECK(t.session().panels.selected == selected_before); // nothing was selected
        CHECK_FALSE(t.session().pane_drag.active);             // nothing was taken hold of
        CHECK(t.notice() == notice_before);                    // nothing was said
    }
    SUBCASE("a press on the surface's own furniture is consumed silently") {
        const std::string notice_before = t.notice();
        // The heading row: inside the rectangle, on no population row.
        t.press_canvas(
            context_cell_x(t.session()),
            surface::cell_of_subs(context_bounds(t.session(), screen_of(t.session())).y));
        CHECK(t.menu().open); // not a dismissal
        CHECK(t.notice() == notice_before);
    }
    SUBCASE("a press on a row is the pointer's choose") {
        // Row 0 of the room's population is `arrange desk` -- the press lands exactly where the
        // painter drew the row (the inverse-pair claim, spent live). (It was the picker's door,
        // under the object canvas's `new`, until both retired.)
        t.press_canvas(context_cell_x(t.session()),
                       context_entry_cell_y(t.session(), 0));
        CHECK_FALSE(t.menu().open);
        CHECK(t.session().arrange.open);
    }
}

TEST_CASE("CTX-0/ARR-0: a mode that owns the pointer answers a right press its own way") {
    Live t;
    // ⚠ THE TERMINAL OVERLAY WAS THE FIRST SUBCASE HERE (VD-24) -- "a second button still
    // means nothing there", proved by opening the mode and pressing right inside it. It was
    // the only mode in this application that owned the pointer ANYWHERE on the screen, which
    // is exactly what a pane does not do. What is left is the mode that still owns one: an
    // arrangement scope.
    SUBCASE("an arrangement scope: the press LEAVES it, consumed whole (SC-6)") {
        open_pane(t, ref_of(stock::kKind));
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
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
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
    CHECK(t.menu().pane == ref_of(stock::kKind));
}

TEST_CASE("ARR-0/SC-6: every arrangement level claims the press; the menu keeps its own") {
    Live t;
    open_pane(t, ref_of(stock::kKind));

    SUBCASE("the desk") {
        enter_arrange_desk(t);
        t.right_press(40, 0);
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.menu().open);
    }
    SUBCASE("the reset prompt leaves the whole interaction") {
        enter_arrange_desk(t);
        select_pane(t, ref_of(stock::kKind));
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
            bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
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

namespace {

// ⭐ RESTORED AFTER THE INFO PANEL'S CASES LEFT: these two read the LAYOUT run and are about
// the tab run rather than about any panel.
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

TEST_CASE("WUX-9/SC-4: a switch returns membership, geometry and front order as authored") {
    Live t;
    const LayoutKeys k = layout_keys(t);
    // LAYOUT ONE: the second stand-in alone, moved somewhere a maker chose. It was Info and then
    // the host's Pane Manager; a layout that needs a pane on it opens one.
    pick(t, second::kKind);
    REQUIRE(t.session().panels.has(second::kKind));
    REQUIRE(author_pane_place(live(t).setup.active, second_ref(),
                              surface::subs_of_cells(3), surface::subs_of_cells(4))
                .accepted);
    const Setup first = t.session().setup.active;

    press_gesture(t, k.make);
    // LAYOUT TWO: a different membership, a different place, a different front order.
    open_stock_pane(t);
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind),
                              surface::subs_of_cells(9), surface::subs_of_cells(2))
                .accepted);
    REQUIRE(send_to_back(live(t).setup.active, ref_of(stock::kKind)));
    const Setup second = t.session().setup.active;
    REQUIRE(first != second);

    // BACK AND FORTH, AND EACH ONE COMES BACK BYTE FOR BYTE.
    for (int round = 0; round < 3; ++round) {
        CAPTURE(round);
        press_gesture(t, k.previous);
        CHECK(t.session().setup.active == first);
        CHECK(t.session().panels.has(second::kKind));
        CHECK_FALSE(t.session().panels.has(stock::kKind));
        press_gesture(t, k.next);
        CHECK(t.session().setup.active == second);
        CHECK(t.session().panels.has(stock::kKind));
        // ...AND THE AUTHORED FRONT ORDER WITH IT: the Builder was put BEHIND Info in
        // this layout, so it is the first thing painted and the last thing pressed.
        CHECK(painted_order(t.session()).front() == stock::kKind);
    }

    // THE PRESENTATIONS ARE RECONCILED THROUGH THE ONE DOOR, so the panels a switch left
    // open are exactly the ones the destination names -- in its own list order.
    CHECK(authored_order(t.session()) == presentation_order(second, t.session().panels));
}

TEST_CASE("WUX-9/SC-5: a switch touches no Workshop-global fact") {
    Live t;
    const LayoutKeys k = layout_keys(t);
    open_stock_pane(t);
    // A SELECTION AND A KEYBOARD CANDIDATE, made by a press exactly as a maker makes them.
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.session().panels.selected == stock::kKind);
    // The press pointed the keys at the stand-in, which would take the switch key below as
    // every runtime pane does; the keys are put down without a gesture and the selection --
    // the fact this case is about -- stands.
    release_keys(t);
    const std::int64_t selected = t.session().panels.selected;
    const std::int64_t keyboard = t.session().panels.keyboard;

    // A LAYOUT WITHOUT THE BUILDER IN IT -- and since WUX-11 that is what `layout.new`
    // MAKES: a fresh blank desk, whose membership `apply_setup` reconciles to through the
    // one door membership changes through.
    press_gesture(t, k.make);
    live(t).setup.active.name = "Inspect";
    REQUIRE_FALSE(t.session().panels.has(stock::kKind));

    // THE SELECTION IS NOT DESTROYED BY THE SWITCH -- it simply resolves to nothing while
    // its pane is absent, which is `selected_pane`'s own discipline (WUX-5).
    CHECK(t.session().panels.selected == selected);
    CHECK(t.session().panels.keyboard == keyboard);
    CHECK(selected_pane(t.session().panels) == kNoPaneKind);
    CHECK(keyboard_pane(t.session().panels) == kNoPaneKind);
    // ...AND IT LIFTS NOTHING: no ghost foreground for a pane that is not on this desk.
    for (const std::int64_t kind : painted_order(t.session())) {
        CHECK(kind != stock::kKind);
    }
    // (THE OBJECT DOCUMENT WAS CHECKED HERE -- one truth, and a switch not a door to it -- until
    // it retired with its canvas; the source editor's document is the Editor weave's, and a
    // switch reaches no weave: `test_workshop_panes_editor.cpp` proves that from the pane's side.)

    // AND COMING BACK MAKES THE RETAINED SELECTION MEAN SOMETHING AGAIN.
    press_gesture(t, k.previous);
    CHECK(t.session().panels.selected == selected);
    CHECK(selected_pane(t.session().panels) == stock::kKind);
    CHECK(painted_order(t.session()).back() == stock::kKind);
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
    open_stock_pane(t);
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.session().panels.selected == stock::kKind);

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

    // THE CONTEXTUAL SURFACE SWALLOWS BARE KEYS, so a layout gesture inside it is the surface's.
    // (It was the `p` picker until the picker retired.)
    t.key(input::scan::kA);
    REQUIRE(t.menu().open);
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
    open_stock_pane(t);
    const std::size_t runtime_before = t.session().panels.runtime.entries.size();
    const std::size_t external_before = t.session().panels.external.size();
    const Setup was = t.session().setup.active;
    REQUIRE(external_before == 1); // the stand-in's view, which is a PRESENTATION's copy

    press_gesture(t, k.make);
    // A FRESH DESK, NOT A COPY (WUX-11). What a blank layout changes is the PRESENTATION:
    // it names no Builder, so `apply_setup` withdraws that presentation exactly as any
    // other whole-desk replacement does.
    CHECK(t.session().setup.active == default_setup());
    CHECK_FALSE(t.session().panels.has(stock::kKind));
    CHECK(live_status(t.session().setup) == setup_link::kNone);
    // ...AND NOTHING WORKSHOP-GLOBAL WAS COPIED, CLEARED OR REVALIDATED. The catalog and the
    // external instances are one truth each, and a desk is not a door to either -- which is the
    // half a blank layout must keep as exactly as a copy did.
    CHECK(t.session().panels.runtime.entries.size() == runtime_before);
    // ...WHILE A PRESENTATION'S OWN COPY LEAVES WITH THE PRESENTATION: the stand-in's view is
    // forgotten by the close that withdrew it (WL-LAYOUT-07), which is the opposite of a
    // Workshop-global fact and is why it is counted here rather than assumed.
    CHECK(t.session().panels.external.size() == external_before - 1);
    // AND THE LAYOUT IT WAS MADE FROM IS UNTOUCHED, waiting where it was.
    CHECK(layout_at(t.session().setup, 0) == was);
    press_gesture(t, k.previous);
    CHECK(t.session().panels.has(stock::kKind));
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
            admit_stock(s.panels);
            admit_second(s.panels);
            s.setup.active = setup_of(std::string(len, 'z'), {second::kKind});
            for (std::size_t more = 1; more < count; ++more) {
                s.setup.shelved.push_back(Layout{
                    setup_of(std::string(len, static_cast<char>('a' + more)), {second::kKind}),
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

// ---- WUX-13: a pane's rows, written through their owners --------------------------------------
//
// ⭐ THE HOST'S PANE MANAGER WAS THIS TIER'S SUBJECT -- a built-in whose subject was a pane, with
// two lists, their cursors, a draft and keys of its own -- and it retired: its list is the
// desktop's Pane Manager, and a pane as a subject is Info's, named through the host's inspection
// door. What the tier proved about the ROWS is unchanged, and is proved here through the door
// Info spends: a subject named by an inspector office (`hand_inspect`), and each write a
// `PaneCommitRequested` judged by the row's own setter -- the gesture door, a reset door, a
// definition's region door (`hand_commit`). The cases about the manager's own list, cursor,
// keys, wheel and its own subject retired with it; Info's subject is witnessed in
// `test_workshop_panes_info.cpp`.

TEST_CASE("WUX-13/SC-4+SC-5: the subject's rows say identity, then AUTHORED, then RESOLVED") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    REQUIRE(hand_inspect(t, ref_of(panel::kLayouts)).accepted);
    const Session& s = t.session();
    const std::vector<Row>& rows = s.inspected.rows;
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
    CHECK(subject_value(s, "Name") == "Layouts");
    CHECK(subject_value(s, "Identity") == "zengine.workshop/layouts");
    CHECK(subject_value(s, "Provider") == "zengine.workshop (built in)");
    CHECK(subject_value(s, "Summary") == "layout tabs and setup");
    // AUTHORED: nothing yet -- a fresh desk is the developer's answer on every axis.
    CHECK(rows[4].section());
    CHECK(subject_value(s, "X") == "-");
    CHECK(subject_value(s, "Y") == "-");
    CHECK(subject_value(s, "Width") == "-");
    CHECK(subject_value(s, "Height") == "-");
    // `f0`: Layouts is the FIRST row `default_setup` authors, and the desk boots with two rows --
    // Layouts and Info's. The facts name no key: the reader of these rows holds its own.
    CHECK(subject_value(s, "Front") == "f0 of 2");
    CHECK(subject_value(s, "Open") == "yes");
    // RESOLVED: the rectangle the pane path answers RIGHT NOW, in the face's unit.
    CHECK(rows[11].section());
    const Screen sc = screen_of(s);
    const PanelBounds where = bounds_of(s.panels, s.setup.active, panel::kLayouts, sc);
    CHECK(subject_value(s, "Window") == fine_rect_text(where.resolved, 0));
    CHECK(subject_value(s, "Window") == "@0,0 132x2 cells");
    CHECK(subject_value(s, "State") == "open");
    // WHICH ROWS ARE THE MAKER'S TO TOUCH says which truth is which.
    for (const char* authored : {"X", "Y", "Width", "Height"}) {
        CHECK(subject_row(s, authored)->editable());
    }
    for (const char* derived : {"Name", "Identity", "Provider", "Summary", "Front", "Open",
                                "Window", "State", "Interior"}) {
        INFO(derived);
        CHECK_FALSE(subject_row(s, derived)->editable());
    }
}

TEST_CASE("WUX-13/SC-6+SC-11: a typed place moves Layouts through the gesture door, and its "
          "tabs follow") {
    // ⭐ THE SELF-APPLICATION PROOF, in the suite. Layouts is Workshop's own presentation of
    // itself; a commit through the inspector's door changes its authored Y; the pane path --
    // paint, occupancy, the tab press inverse -- follows with nothing added.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    const PaneRef layouts = ref_of(panel::kLayouts);
    REQUIRE(hand_inspect(t, layouts).accepted);
    const Screen sc = screen_of(t.session());
    REQUIRE(layouts_body(t.session(), sc).region_y == 0);
    CHECK(hand_commit(t, "Y", "20").accepted);
    CHECK_FALSE(t.session().notice_is_bad);
    CHECK(t.notice() == "committed Y of Layouts = 20 cells");
    // THE AUTHORED ROW MOVED, THROUGH `author_pane_window`: the untyped axis kept what it
    // stood at (the resolved 0), the typed axis is the maker's, and the extents were not
    // touched.
    const SetupPane* row = pane_of(t.session().setup.active, layouts);
    REQUIRE(row != nullptr);
    CHECK(row->place == PanePlace{pane_unit::kSubcells, 0, subs(20)});
    CHECK(row->width == PaneSize{});
    CHECK(row->height == PaneSize{});
    CHECK(subject_value(t.session(), "Y") == "20 cells");
    CHECK(subject_value(t.session(), "X") == "0 cells");
    // THE RESOLVED ROW FOLLOWED, FRESH...
    const PanelBounds where =
        bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc);
    CHECK(where.rect.y == subs(20));
    CHECK(subject_value(t.session(), "Window") == "@0,20 132x2 cells");
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
    CHECK(hand_commit(t, "Width", "40").accepted);
    CHECK(pane_of(t.session().setup.active, layouts)->width ==
          PaneSize{pane_unit::kSubcells, subs(40)});
    CHECK(pane_of(t.session().setup.active, layouts)->place ==
          PanePlace{pane_unit::kSubcells, 0, subs(20)});
    CHECK(subject_value(t.session(), "Window") == "@0,20 40x2 cells");
    // `-` IS THE RESET DOOR, ONE AXIS AT A TIME.
    CHECK(hand_commit(t, "Width", "-").accepted);
    CHECK(pane_of(t.session().setup.active, layouts)->width == PaneSize{});
    CHECK(pane_of(t.session().setup.active, layouts)->place ==
          PanePlace{pane_unit::kSubcells, 0, subs(20)});
    CHECK(hand_commit(t, "X", "-").accepted);
    CHECK(pane_of(t.session().setup.active, layouts)->place == PanePlace{});
    CHECK(layouts_body(t.session(), sc).region_y == 0);
}

TEST_CASE("WUX-13/SC-6: a typed place reseats the stack through `apply_setup`") {
    // ⚔ MUTATION (F4): a write that lands in the `SetupPane` without going through the
    // commit path's reseat. `bounds_of` reads the setup live, so a moved pane MOVES either
    // way -- what a bypass leaves behind is a pane refused for want of room, still waiting
    // after the room appeared. The minimum screen seats one stacked pane.
    Live t;
    open_stock_pane(t);
    REQUIRE(t.session().panels.has(stock::kKind));
    REQUIRE(add_pane(live(t).setup.active, second_ref()));
    t.publish(loom::to_value(surface::SurfaceExtent{80, 22, 0, 0})); // a reconcile
    REQUIRE(has_pane(t.session().setup.active, second_ref()));
    REQUIRE_FALSE(t.session().panels.has(second::kKind)); // authored, and waiting
    REQUIRE(hand_inspect(t, stock_ref()).accepted);
    CHECK(hand_commit(t, "X", "2").accepted);
    CHECK_FALSE(t.session().notice_is_bad);
    // THE STAND-IN LEFT THE REACTIVE STACK, AND THE WAITING PANE WAS SEATED IN THE SLOT IT
    // VACATED -- which only a reconcile does.
    CHECK(t.session().panels.has(second::kKind));
    CHECK(pane_of(t.session().setup.active, stock_ref())->place.mode == pane_unit::kSubcells);
}

TEST_CASE("WUX-13/SC-7: a typed value that is not admissible is refused, and the authored row "
          "is untouched") {
    // ⚔ MUTATION (F3): clamping a typed value to the room or to the lattice. Every
    // comparison against `before` below is value identity over the whole desk.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    const PaneRef layouts = ref_of(panel::kLayouts);
    REQUIRE(hand_inspect(t, layouts).accepted);
    REQUIRE(hand_commit(t, "X", "10").accepted);
    const Setup before = t.session().setup.active;
    REQUIRE(pane_of(before, layouts)->place.x == subs(10));

    // THE REFUSAL IS THE OWNER'S OWN WORDS, answered to the inspector that asked; keeping what
    // was typed so it can be fixed is the inspector's draft, not the host's.
    const PaneSubjectActed abc = hand_commit(t, "X", "abc");
    CHECK_FALSE(abc.accepted);
    CHECK(abc.refusal.find("X: not a whole number of cells") == 0);
    CHECK(t.session().setup.active == before);

    const PaneSubjectActed wide = hand_commit(t, "Width", "99999");
    CHECK_FALSE(wide.accepted);
    CHECK(wide.refusal.find("at most 4096 cells") != std::string::npos);
    CHECK(t.session().setup.active == before);

    CHECK_FALSE(hand_commit(t, "Height", "0").accepted);
    CHECK(t.session().setup.active == before);

    const PaneSubjectActed up = hand_commit(t, "Y", "-7");
    CHECK_FALSE(up.accepted);
    CHECK(up.refusal.find("cannot be negative") != std::string::npos);
    CHECK(t.session().setup.active == before);

    // AN OFF-ROOM VALUE IS NOT CLAMPED EITHER: it is legal authored intent, and the
    // resolved row says what this screen makes of it.
    CHECK(hand_commit(t, "X", "500").accepted);
    CHECK(pane_of(t.session().setup.active, layouts)->place.x == subs(500));
    CHECK(subject_value(t.session(), "State").find("off-room") == 0);
    CHECK(subject_value(t.session(), "Window") == "@500,0 132x2 cells");

    // A RESET OF AN AXIS ALREADY AT THE DEVELOPER'S ANSWER IS REFUSED IN THE OWNER'S WORDS.
    const Setup placed = t.session().setup.active;
    const PaneSubjectActed reset = hand_commit(t, "Width", "-");
    CHECK_FALSE(reset.accepted);
    CHECK(reset.refusal.find("already takes the developer's width") != std::string::npos);
    CHECK(t.session().setup.active == placed);
}

TEST_CASE("WUX-13/SC-8: looking never authors") {
    // ⚔ MUTATION (F2): an inspection that writes a resolved rectangle back into the setup.
    // Every read below is followed by value identity over the whole desk.
    Live t;
    const Setup born = t.session().setup.active;
    // NAMING A SUBJECT AUTHORS NOTHING. (Opening the host's Pane Manager authored its own
    // participation, the one thing it wrote, until it retired.)
    REQUIRE(hand_inspect(t, ref_of(panel::kLayouts)).accepted);
    CHECK(t.session().setup.active == born);
    for (const Row& r : t.session().inspected.rows) {
        (void)r.value(); // every row, read
    }
    CHECK(t.session().setup.active == born);
    // THE SCREEN CHANGES; THE RESOLVED ROW CHANGES; THE AUTHORED ROWS DO NOT.
    const std::string small = subject_value(t.session(), "Window");
    t.publish(loom::to_value(surface::SurfaceExtent{160, 60, 0, 0}));
    CHECK(subject_value(t.session(), "Window") != small);
    CHECK(subject_value(t.session(), "Window") == "@0,0 160x2 cells");
    CHECK(subject_value(t.session(), "X") == "-");
    CHECK(t.session().setup.active == born);
    // THE FACE CHANGES: the same value, spelled in pixels, and nothing written.
    t.publish(loom::to_value(surface::SurfaceExtent{160, 60, 8, 18, surface::kCanvasCellPx}));
    CHECK(subject_value(t.session(), "Window") == "@0,0 1920x24 px");
    CHECK(subject_value(t.session(), "X") == "-");
    CHECK(t.session().setup.active == born);
    // SELECTING PANES, PRESSING AROUND: still nothing.
    t.press(90, 35);
    t.key(input::scan::kTab);
    t.key(input::scan::kDown);
    t.key(input::scan::kUp);
    CHECK(t.session().setup.active == born);
}

TEST_CASE("WUX-13/SC-9: a closed pane and an unresolved row are subjects with honest facts") {
    // ⚔ MUTATION (F5): dropping the subject when its pane leaves the layout. The subject
    // below is closed and launched again through the doors and stands throughout.
    // ⚔ MUTATION (F6): filtering unresolved refs out of the inventory, or out of the setup.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    const PaneRef closed = stock_ref();
    REQUIRE_FALSE(t.session().panels.has(stock::kKind));
    REQUIRE(hand_inspect(t, closed).accepted);
    CHECK(subject_value(t.session(), "State") == "closed -- open it from the Pane Manager");
    CHECK(subject_value(t.session(), "Open") == "no");
    CHECK(subject_value(t.session(), "X") == "--");
    CHECK(subject_value(t.session(), "Window") == "-");
    // A CLOSED PANE'S GEOMETRY IS NOT TYPEABLE, and the refusal says what to do.
    const PaneSubjectActed x = hand_commit(t, "X", "3");
    CHECK_FALSE(x.accepted);
    CHECK(x.refusal.find("is not in this layout -- open it first") != std::string::npos);
    // OPEN IT THROUGH THE LAUNCH DOOR -- the subject stands.
    open_stock_pane(t);
    CHECK(t.session().panels.has(stock::kKind));
    CHECK(t.session().inspected.ref == closed);
    CHECK(subject_value(t.session(), "State") == "open");
    CHECK(subject_value(t.session(), "Open") == "yes");
    CHECK(subject_value(t.session(), "X") == "-");
    // ...AND CLOSE IT AGAIN: the subject STANDS.
    pick(t, stock::kKind);
    CHECK_FALSE(t.session().panels.has(stock::kKind));
    CHECK(t.session().inspected.ref == closed);
    CHECK(subject_value(t.session(), "State") == "closed -- open it from the Pane Manager");

    // AN AUTHORED REFERENCE NO OFFICE RESOLVES keeps its identity and its geometry.
    REQUIRE(add_pane(live(t).setup.active, stranger()));
    REQUIRE(author_pane_place(live(t).setup.active, stranger(), subs(7), subs(9)).accepted);
    REQUIRE(author_pane_size(live(t).setup.active, stranger(),
                             PaneSize{pane_unit::kSubcells, subs(30)}, PaneSize{})
                .accepted);
    REQUIRE(hand_inspect(t, stranger()).accepted);
    CHECK(subject_value(t.session(), "Name") == "history");
    CHECK(subject_value(t.session(), "Identity") == "third.party.tools/history");
    CHECK(subject_value(t.session(), "Provider") ==
          "third.party.tools (unresolved -- no office here offers it)");
    CHECK(subject_value(t.session(), "State").find("unresolved") == 0);
    CHECK(subject_value(t.session(), "X") == "7 cells");
    CHECK(subject_value(t.session(), "Y") == "9 cells");
    CHECK(subject_value(t.session(), "Width") == "30 cells");
    CHECK(subject_value(t.session(), "Height") == "-");
    CHECK(subject_value(t.session(), "Window") == "-");
    // ITS GEOMETRY CANNOT BE TYPED (no base to measure the other axis from)...
    const PaneSubjectActed y = hand_commit(t, "Y", "1");
    CHECK_FALSE(y.accepted);
    CHECK(y.refusal.find("is unresolved") != std::string::npos);
    // ...AND `-` RESETS AN UNRESOLVED PANE'S AXIS, through the reset door. (Its order was the
    // host Pane Manager's `b` here too, until that retired; ordering is the arrangement's.)
    CHECK(hand_commit(t, "Width", "-").accepted);
    CHECK(pane_of(t.session().setup.active, stranger())->width == PaneSize{});
    CHECK(pane_of(t.session().setup.active, stranger())->place ==
          PanePlace{pane_unit::kSubcells, subs(7), subs(9)});
    CHECK(has_pane(t.session().setup.active, stranger()));
}

TEST_CASE("WUX-13/SC-10: editing a pane in a layout related to a current Setup makes it "
          "modified") {
    // ⚔ MUTATION (F8): an editor-local dirty bit, or a comparison that stays `current`.
    // The verdict below is `link_status`, derived by comparing the desk to the known value,
    // and the inspection holds no flag of its own.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    link_live_setup(live(t).setup, "somewhere.json");
    REQUIRE(live_status(t.session().setup) == setup_link::kCurrent);
    REQUIRE(hand_inspect(t, ref_of(panel::kLayouts)).accepted);
    // reading is not editing
    for (const Row& r : t.session().inspected.rows) {
        (void)r.value();
    }
    CHECK(live_status(t.session().setup) == setup_link::kCurrent);
    REQUIRE(hand_commit(t, "Y", "20").accepted);
    CHECK(live_status(t.session().setup) == setup_link::kModified);
    // ...and undoing it by hand makes it current again, because there is no flag.
    REQUIRE(hand_commit(t, "Y", "-").accepted);
    CHECK(live_status(t.session().setup) == setup_link::kCurrent);
}

TEST_CASE("WUX-13/SC-12: moving, resizing and closing Layouts through the doors leaves the "
          "reservation alone") {
    // ⚔ MUTATION (F7): coupling `screen_of`'s reservation to the Layouts pane. Every
    // comparison below moves.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    const Screen before = screen_of(t.session());
    REQUIRE(hand_inspect(t, ref_of(panel::kLayouts)).accepted);
    REQUIRE(hand_commit(t, "Y", "30").accepted);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    CHECK(screen_of(t.session()).room_w == before.room_w);
    REQUIRE(hand_commit(t, "Height", "9").accepted);
    REQUIRE(hand_commit(t, "Width", "9").accepted);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    CHECK(screen_of(t.session()).room_w == before.room_w);
    pick(t, panel::kLayouts); // closed altogether
    REQUIRE_FALSE(t.session().panels.has(panel::kLayouts));
    CHECK(screen_of(t.session()).room_w == before.room_w);
    CHECK(screen_of(t.session()).room_h == before.room_h);
    CHECK(screen_of(t.session()).notice_y == before.notice_y);
    // ...and the launch door brings it back, at the developer's default.
    pick(t, panel::kLayouts);
    CHECK(t.session().panels.has(panel::kLayouts));
}

TEST_CASE("WUX-13/SC-13: a pane edit survives a restart through the session, and the subject "
          "does not") {
    TempDir dir("wux13-restart");
    const std::string session = dir.file("session.json");
    {
        Live t;
        t.host.session_path = session;
        t.host.setup_path = dir.file("s.json");
        t.publish(loom::to_value(surface::SurfaceReady{}));
        t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
        REQUIRE(hand_inspect(t, ref_of(panel::kLayouts)).accepted);
        REQUIRE(hand_commit(t, "Y", "20").accepted);
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
    CHECK(layouts_body(back.session(), screen_of(back.session())).region_y == 20);
    // THE SUBJECT IS INTERACTION STATE AND IS NOT PERSISTED.
    CHECK_FALSE(back.session().inspected.addressed());
    CHECK(back.session().inspected.rows.empty());
}

TEST_CASE("WUX-13: a typed amount is read and written in the face's own unit") {
    // THE WUX-6 GRAMMAR, READ BACKWARDS: a graphical face spells a pane in pixels and takes
    // pixels back; a cell face does the same in cells; a unit the face did not report is
    // refused rather than converted.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{160, 60, 8, 18, surface::kCanvasCellPx}));
    const PaneRef layouts = ref_of(panel::kLayouts);
    REQUIRE(hand_inspect(t, layouts).accepted);
    CHECK(hand_commit(t, "X", "120").accepted);
    CHECK(t.notice() == "committed X of Layouts = 120 px");
    CHECK(pane_of(t.session().setup.active, layouts)->place.x == subs(10));
    CHECK(subject_value(t.session(), "X") == "120 px");
    const PaneSubjectActed cells = hand_commit(t, "Y", "10 cells");
    CHECK_FALSE(cells.accepted);
    CHECK(cells.refusal == "Y: this face reads px, not cells");
    CHECK(hand_commit(t, "Y", "24px").accepted);
    CHECK(pane_of(t.session().setup.active, layouts)->place.y == subs(2));
    // A PIXEL-GRAIN VALUE READ ON A CELL FACE IS MARKED AS A PROJECTION, and typing on that
    // face authors cells.
    CHECK(hand_commit(t, "X", "126").accepted);
    CHECK(subject_value(t.session(), "X") == "126 px");
    t.publish(loom::to_value(surface::SurfaceExtent{160, 60, 0, 0, 0}));
    CHECK(subject_value(t.session(), "X") == "~10 cells (~ projected)");
    CHECK(hand_commit(t, "X", "11 cells").accepted);
    CHECK(pane_of(t.session().setup.active, layouts)->place.x == subs(11));
    CHECK(subject_value(t.session(), "X") == "11 cells");
    const PaneSubjectActed px = hand_commit(t, "X", "3 px");
    CHECK_FALSE(px.accepted);
    CHECK(px.refusal == "X: this face reads cells, not px");
}

// ---- QR-18: Escape puts the selected pane down, last ---------------------------------------

TEST_CASE("QR-18/SC-1+SC-3: Escape clears the ordinary selection last, and the inspected subject "
          "stands") {
    // ⭐ THE PARTY THAT NOW OWNS THIS MEANING (WL-DESK-02). Escape-to-deselect is a
    // DECLARED application row, so this case supplies the declarer -- and what it proves
    // is the whole relocated path: the key resolves to the row, the host asks its owner,
    // the owner answers under the number the ask went out on, and only then is the
    // selection put down. A Workshop with no desktop has no such row and Escape does
    // nothing, which is the case beside this one.
    // MUTATION (F1): removing the final Escape branch -- `selected == kNoPaneKind` below
    // goes red. MUTATION (F2): clearing the subject beside the selection -- the subject
    // check goes red.
    Live t;
    mount_desktop(t);
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    const PaneRef layouts = ref_of(panel::kLayouts);
    REQUIRE(hand_inspect(t, layouts).accepted); // an inspector names Layouts

    // THE PROMPT'S OWN CASE: the subject is Layouts, the ordinary selection is another pane.
    open_pane(t, ref_of(stock::kKind));
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.session().panels.selected == stock::kKind);
    // The stand-in takes the keys as every runtime pane does; the Escape below is command
    // mode's, so the keys are put down without a gesture and the selection stands.
    release_keys(t);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kCommand);
    const std::vector<std::int64_t> order_before = presentation_order(t.session().setup.active, t.session().panels);
    const Setup setup_before = t.session().setup.active;
    const std::size_t panes_before = t.session().panels.open.size();

    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.session().panels.keyboard == kNoPaneKind);
    CHECK(t.session().inspected.ref == layouts); // SC-3: the subject is a different fact
    CHECK(t.notice().find(std::string("unselected ") + stock::kName) != std::string::npos);
    // NOTHING ELSE MOVED (SC-10): no pane closed, no rank, no geometry, no file.
    CHECK(t.session().panels.open.size() == panes_before);
    CHECK(presentation_order(t.session().setup.active, t.session().panels) == order_before);
    CHECK(t.session().setup.active == setup_before);
    CHECK(t.session().panels.has(stock::kKind));

    // THE SAME WITH THE SUBJECT ITSELF SELECTED: Layouts takes no keys, so the selection is
    // what Escape sheds -- and the subject stands. (It was the host's Pane Manager selected
    // and holding the keys here, until that retired.)
    const ui::Rect band = cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                                  panel::kLayouts, screen_of(t.session()))
                                            .rect);
    t.press_canvas(band.x + band.w - 1, band.y);
    REQUIRE(t.session().panels.selected == panel::kLayouts);
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(keyboard_context(t.session()) == KeyContext::kCommand);
    CHECK(t.session().inspected.ref == layouts);

    // WITH NOTHING SELECTED, ESCAPE IS THE NO-OP IT ALWAYS WAS, and says nothing new.
    const std::string notice = t.notice();
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.notice() == notice);
}

TEST_CASE("QR-18/SC-2: every more-specific Escape meaning answers first, and deselection waits") {
    // ⭐ THE PARTY THAT NOW OWNS THIS MEANING (WL-DESK-02). Escape-to-deselect is a
    // DECLARED application row, so this case supplies the declarer -- and what it proves
    // is the whole relocated path: the key resolves to the row, the host asks its owner,
    // the owner answers under the number the ask went out on, and only then is the
    // selection put down. A Workshop with no desktop has no such row and Escape does
    // nothing, which is the case beside this one.
    // MUTATION (F3): asking the final fallthrough BEFORE the resolved context -- the
    // surface would still be open with the selection already gone.
    Live t;
    mount_desktop(t);
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));

    // THE CONTEXTUAL SURFACE: a mode above every pane. Select a pane that takes no keys so `a`
    // still reaches command mode -- the Layouts band, which the catalog says does not take the
    // keyboard. (The mode was the `p` picker, and a live draft in the host's Pane Manager the
    // second meaning here, until both retired.)
    const Screen sc = screen_of(t.session());
    const ui::Rect band = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc).rect);
    t.press_canvas(band.x + band.w - 1, band.y);
    REQUIRE(t.session().panels.selected == panel::kLayouts);
    t.key(input::scan::kA);
    REQUIRE(t.menu().open);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.menu().open);                            // the surface answered...
    CHECK(t.session().panels.selected == panel::kLayouts); // ...and the selection stood
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind); // the next Escape sheds it
    // ⭐ THE HOTKEY VIEW WAS THE LAST MEANING HERE, keys-modal above everything. It is the
    // desktop's Hotkeys pane now, a pane like any other, and its Escape is its own.
}

TEST_CASE("QR-18/SC-4: a desk with no unoccupied cell still reaches selection = none") {
    // ⭐ THE PARTY THAT NOW OWNS THIS MEANING (WL-DESK-02). Escape-to-deselect is a
    // DECLARED application row, so this case supplies the declarer -- and what it proves
    // is the whole relocated path: the key resolves to the row, the host asks its owner,
    // the owner answers under the number the ask went out on, and only then is the
    // selection put down. A Workshop with no desktop has no such row and Escape does
    // nothing, which is the case beside this one.
    // THE RECOVERY CLAIM. Every cell between the two bands is some pane's, so there is no
    // blank pixel to press; Escape is the way down.
    Live t;
    mount_desktop(t);
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_pane(t, ref_of(stock::kKind));
    const Screen sc = screen_of(t.session());
    // The Builder over the whole room, the side column included -- an authored window is
    // canvas-absolute (WUX-2), and the room is what a pane may cover.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(stock::kKind),
                              surface::subs_of_cells(0), surface::subs_of_cells(kTopRows))
                .accepted);
    const Written sized =
        author_pane_size(live(t).setup.active, ref_of(stock::kKind),
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
        bounds_of(t.session().panels, t.session().setup.active, stock::kKind, now).rect);
    t.press_canvas(builder.x + 3, builder.y + 3);
    REQUIRE(t.session().panels.selected == stock::kKind);
    release_keys(t); // a runtime pane keeps Escape while it holds the keys (WL-ARR-14)
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.session().panels.keyboard == kNoPaneKind);
    CHECK(t.session().panels.has(stock::kKind)); // still there, still that big
}

