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
    CHECK(q1->rect.w == 46); // 60% of the minimum screen's 78-cell room
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

TEST_CASE("`]` reaches the room a bigger surface gave, and `[` still narrows") {
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{100, 33}));
    CHECK(t.session().workspace_w == 100);

    // The ceiling moved with the screen: before G-2 this stopped at 48, and it stopped thirty
    // columns short of the surface until the right column stopped being a reservation.
    for (int i = 0; i < 60; ++i) {
        t.key(input::scan::kRightBracket);
    }
    CHECK(t.session().workspace_w == 100);
    CHECK(t.notice().find("100 cells wide") != std::string::npos);
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
    CHECK(has_rect(c, kWorkspaceX, kWorkspaceY, 78, 16, surface::role::kMuted));
    // THE OBJECTS COLUMN IS NOT IN THIS PICTURE ANY MORE, and its absence is the fallback
    // being honest rather than the fallback shrinking: a default `Session` opens the panels
    // `kDefaultPanels` names, the Info weave is not one of them, and a run with no medium is
    // also a run with no load plan. What this case owes is the COMPOSITION -- 78 by 22, the
    // workspace at its full extent, the bands where they belong -- and every row of it is
    // still asked for below.
    CHECK(label_at(c, 0, 19).rfind("n new | d delete", 0) == 0);
    // ⭐ THE SECOND HELP ROW LOST THREE KEYS AND KEPT ITS SHAPE. `enter edit` and
    // `up/down row` were the Info panel's command-mode rows and left with it; what stands
    // here is the row the keymap composes from what remains.
    CHECK(label_at(c, 0, 20).rfind("[ ] workspace | p + panel", 0) == 0);
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
    CHECK(kPanelCatalog[0].kind == panel::kEditor);
    CHECK(std::string(kPanelCatalog[0].name) == "Editor");
    CHECK(kPanelCatalog[1].kind == panel::kLayouts);
    CHECK(std::string(kPanelCatalog[1].name) == "Layouts");

    Session s; // a fresh session: the Layouts band open, the other two not
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
    // three cells off every summary -- a long one is cut there and MARKED, which is
    // `detail::fit` doing its job. What the picker owes is the row AS IT FITS IT; the case
    // searches for exactly that rather than for a sentence the room cannot hold.
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
    CHECK(shown.find(detail::pad("Editor", kPickerNameCols) + "closed") != std::string::npos);
    CHECK(shown.find(detail::pad("Layouts", kPickerNameCols) + "open") != std::string::npos);
}

TEST_CASE("a panel opens from the picker, is removed, and opens again") {
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    REQUIRE_FALSE(t.w->session().panels.has(panel::kEditor));

    t.key(input::scan::kP);
    CHECK(t.w->session().panels.picker.open);
    // The picker is a question, not a panel: opening it opens nothing.
    CHECK_FALSE(t.w->session().panels.has(panel::kEditor));
    t.key(input::scan::kEscape);

    open_editor_pane(t);
    CHECK_FALSE(t.w->session().panels.picker.open);
    CHECK(t.w->session().panels.has(panel::kEditor));
    CHECK(stack_text(t.canvases.back()).find("Editor") != std::string::npos);

    // THE SAME DOOR REMOVES IT (PNL-0). There is no close key; selecting a kind
    // that is open is what takes it away.
    open_editor_pane(t);
    CHECK_FALSE(t.w->session().panels.has(panel::kEditor));
    CHECK(stack_text(t.canvases.back()).find("Editor") == std::string::npos);

    open_editor_pane(t);
    CHECK(t.w->session().panels.has(panel::kEditor));
    CHECK(stack_text(t.canvases.back()).find("Editor") != std::string::npos);
    // ⭐ ...AND NOT ONE OF THOSE OPENS ASKED THE TOOL ANYTHING. That half used to be the
    // point -- a reopened Builder panel showed a live tool rather than a remembered one --
    // and no built-in asks a participant anything when it opens now, because the built-in
    // that did is a weave and asks in its own image, on its own room grant.
    CHECK(tool->described == 0);
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
        // now that one gesture does both, and the Layouts band was open when the picker was.
        CHECK_FALSE(t.w->session().panels.has(panel::kEditor));
        CHECK_FALSE(t.w->session().panels.has(panel::kPaneEditor));
        CHECK(t.w->session().panels.has(panel::kLayouts));
    }
}

TEST_CASE("selecting an open kind REMOVES it, and says what was not touched") {
    // BLD-0 refused this selection (`Editor is already open -- x closes it`)
    // because `x` was the removal and the picker had nothing to add. PNL-0 gave
    // the picker both directions, so the refusal became the removal and `x` went
    // back to being unbound. Both kinds, because the whole point is that the
    // gesture does not know which kind it is operating on.
    Live t;
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    open_editor_pane(t);
    REQUIRE(t.w->session().panels.has(panel::kEditor));

    open_editor_pane(t);
    CHECK_FALSE(t.w->session().panels.has(panel::kEditor));
    CHECK(t.w->session().notice ==
          "removed Editor -- p brings it back; nothing behind it was touched");
    CHECK_FALSE(t.w->session().notice_is_bad); // a removal a maker asked for is not a refusal

    // ...AND THE THING BEHIND IT REALLY WAS NOT TOUCHED. The tool never heard
    // about the removal: no message reached the office, and reopening finds it
    // exactly where it was.
    const std::int64_t asked_of_tool = tool->described;
    pick(t, panel::kPaneEditor);
    REQUIRE(t.w->session().panels.has(panel::kPaneEditor)); // opened, so it can be removed
    pick(t, panel::kPaneEditor);
    CHECK_FALSE(t.w->session().panels.has(panel::kPaneEditor));
    CHECK(t.w->session().notice ==
          "removed Pane Manager -- p brings it back; nothing behind it was touched");
    CHECK(tool->described == asked_of_tool);
}

TEST_CASE("a stacked panel covers the workspace, and may reach the right column") {
    Live t;
    (void)mount_tool(t, "zengine-snake");
    t.key(input::scan::kEscape); // an unbound key: it repaints and changes nothing

    // WITHOUT A PANEL, the screen carries no stacked rows; the picker's own gesture is
    // said by the band's legend and the hotkey view since WUX-1, not by a row-0 hint.
    const surface::SurfaceCanvas bare = t.canvases.back();
    CHECK(stack_text(bare).empty());

    open_editor_pane(t);
    const surface::SurfaceCanvas with = t.canvases.back();
    const Screen sc = screen_of(t.session());
    // THE BOUNDS THE PLACEMENT PATH GIVES IT, and the rows are read against those
    // rather than against a column this case knows independently. The rows are a region's
    // since WUX-1, so they are read through the cell projection every character medium
    // draws with.
    const ui::Rect stack =
cells_covered(bounds_of(t.session().panels, t.session().setup.active, panel::kEditor, sc).rect);
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
    CHECK(placement_of(panel::kEditor) == placement::kOverlayStack);
    CHECK(placement_of(panel::kPaneEditor) == placement::kOverlayStack);
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
    open_editor_pane(t);
    const Screen sc = screen_of(t.session());
    const surface::SurfaceCanvas c = t.canvases.back();

    const PanelBounds band =
        bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc);
    const PanelBounds builder =
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor, sc);
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
    const PanelBounds absent = bounds_of(t.session().panels, t.session().setup.active, panel::kEditor, sc);
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
    band_first.open = {Panel{panel::kLayouts}, Panel{panel::kEditor}};
    Panels builder_first;
    builder_first.open = {Panel{panel::kEditor}, Panel{panel::kLayouts}};

    const ui::Rect first_slot = placement_bounds(placement::kOverlayStack, 0, sc);
    CHECK(bounds_of(band_first, setup_for(band_first), panel::kEditor, sc).rect ==
          fine_of_cells(first_slot));
    CHECK(bounds_of(builder_first, setup_for(builder_first), panel::kEditor, sc).rect ==
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
// THIS TIER WAS INFO'S, AND ITS SUBJECT OUTLIVED INFO. What it always measured is the
// difference a SECOND built-in kind makes: that a panel is a presentation with no state of
// its own, that removing one destroys nothing behind it, and that none of it needs a weave.
// Info was the instance; the Pane Manager is the instance now, and the OBJECTS and
// PROPERTIES columns those cases read are `Zengine/info-pane/`'s rows, measured across the
// seam in `test_workshop_panes_info.cpp`.
//
// WHAT MAKES THE PANE MANAGER THE INTERESTING SECOND KIND is what it is NOT:
//
//   it has no weave.        Opening it sends nothing and asks nobody. A Workshop
//                           hosting no tools at all opens it and it works.
//   it has no state
//   that outlives it.       Removing it destroys a presentation. Its subject, its
//                           cursor and its rows are re-derived from the setup and the
//                           catalog, both of which outlive the panel by a lot.
//   it is in the dock.      AND THIS IS WHERE THE TIER CHANGED. Info was in the
//                           right-hand column, a different place with different rules,
//                           and several cases below were really about THAT difference.
//                           The room is the screen now (WL-GEO-03) and the second built-in
//                           shares the stack -- so the cases that compared a reserved
//                           column before and after a removal are gone, and what is left is
//                           the part that was never about the column: a presentation that
//                           leaves whole and comes back whole.
//
// The cases that would notice if any of those quietly stopped being true are the ones that
// remove the panel with no tool on the bus, that compare its rows before and after a
// removal, and that measure the document while it is absent.

TEST_CASE("a panel can be removed, and takes its whole presentation with it") {
    Live t;
    t.key(input::scan::kN); // something to look at, so an empty panel is not the empty case
    pick(t, panel::kPaneEditor);
    const Screen sc = screen_of(t.session());
    REQUIRE_FALSE(panel_shown(t.canvases.back(), t.session(), panel::kPaneEditor).empty());

    pick(t, panel::kPaneEditor);
    CHECK_FALSE(t.w->session().panels.has(panel::kPaneEditor));

    // NOT ONE LABEL IS LEFT WHERE IT STOOD. Not the heading, not the inventory, not the
    // subject's rows -- the panel is gone rather than emptied.
    const surface::SurfaceCanvas& gone = t.canvases.back();
    CHECK(panel_shown(gone, t.session(), panel::kPaneEditor).empty());
    // THE HEADING IS NOWHERE ON THE CANVAS, asked of the whole picture rather than of the
    // row the panel would have put it on: since QR-14 that row is inside the workspace, so
    // a row-addressed question would be asking the document what the panel says.
    for (const std::string& row : rasterized(gone)) {
        CHECK(row.find("PANE MANAGER") == std::string::npos);
    }

    // AND NOTHING ELSE MOVED. The screen around the hole is the screen it was:
    // the band's workspace fact and the help lines are exactly where they were, because
    // removing a panel is not a re-layout.
    CHECK(workspace_row(gone, t.session(), sc) == "workspace 78x16 cells");
    CHECK(label_at(gone, 0, sc.help_y).find("n new | d delete") == 0);
}

TEST_CASE("reopening a panel brings back what it had, cell for cell") {
    Live t;
    t.key(input::scan::kN);
    t.key(input::scan::kN);
    pick(t, panel::kPaneEditor);
    const std::string before =
        panel_shown(t.canvases.back(), t.session(), panel::kPaneEditor);
    REQUIRE(before.find("PANE MANAGER") != std::string::npos);

    pick(t, panel::kPaneEditor);
    REQUIRE(panel_shown(t.canvases.back(), t.session(), panel::kPaneEditor).empty());

    pick(t, panel::kPaneEditor);
    CHECK(t.w->session().panels.has(panel::kPaneEditor));
    // THE SAME ROWS, not reconstructed ones. They are byte-identical because there was
    // never a copy to go stale: the panel reads the setup and the catalog, both of which
    // went on being true while it was absent.
    CHECK(panel_shown(t.canvases.back(), t.session(), panel::kPaneEditor) == before);
}

TEST_CASE("removing a panel changes nothing about the document, not even its picture") {
    // THE SHARPEST CLAIM IN THIS TIER: the workspace's extent is what a share resolves
    // against, so a workspace that grew or shrank when a panel closed would make every
    // %-width object on the screen change size because a maker hid a list. A panel's
    // presence must not be visible in the picture of the document.
    //
    // AND ITS OTHER HALF IS RETIRED WITH THE RESERVATION. This case also asserted that the
    // vacated 28 columns STAYED vacant -- `sc.panel_x` and `sc.room_w` unmoved -- which was
    // a claim about a reserved column. The room is the screen now, a panel covers room
    // rather than owning it, and covering nothing is exactly what a closed panel does. What
    // survives is the part that was always the point: the DOCUMENT's own extent.
    // The document Workshop boots on already carries the case this needs: object
    // #1 is authored as a SHARE, so its resolved width is a function of the
    // workspace and nothing else.
    Live t;
    pick(t, panel::kPaneEditor);
    REQUIRE(t.w->document().elements.size() == 2);
    REQUIRE(t.w->document().elements[0].width.mode == ui::kExtentPercent);

    const std::int64_t authored_w = t.w->document().elements[0].width.amount;
    const std::int64_t workspace_w = t.session().workspace_w;
    const ui::Scene before = workspace_scene(t.w->document(), t.session());
    REQUIRE(before.items.size() == 2);

    pick(t, panel::kPaneEditor);
    REQUIRE_FALSE(t.w->session().panels.has(panel::kPaneEditor));

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

    // ...and the same rectangle is still on the canvas, at the same cells.
    bool found = false;
    for (const surface::SurfaceRect& r : all_rects(t.canvases.back())) {
        if (r.role == surface::role::kFill && r.w == before.items[0].rect.w &&
            r.h == before.items[0].rect.h) {
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("The Editor and the Pane Manager are present independently -- all four states") {
    Live t;
    // ON A SCREEN THAT CAN SEAT BOTH. Both built-ins are overlay panes now, so "both open"
    // is a state the minimum screen cannot hold at all -- the picker would refuse the
    // second for room and this case would be measuring a refusal. When the second kind was
    // Info the two never competed, because Info was not in the stack.
    t.publish(loom::to_value(surface::SurfaceExtent{120, 44, 0, 0}));
    ToolSeat* tool = mount_tool(t, "zengine-snake");
    const auto shows_manager = [&t]() {
        return !panel_shown(t.canvases.back(), t.session(), panel::kPaneEditor).empty();
    };
    const auto shows_editor = [&t]() {
        return !panel_shown(t.canvases.back(), t.session(), panel::kEditor).empty();
    };

    // Neither -- how Workshop boots, now that the desk's other row is a weave with no
    // office on this bus.
    t.key(input::scan::kEscape);
    CHECK_FALSE(shows_manager());
    CHECK_FALSE(shows_editor());

    // The Pane Manager alone.
    pick(t, panel::kPaneEditor);
    CHECK(shows_manager());
    CHECK_FALSE(shows_editor());

    // Both, and neither knows the other exists.
    open_editor_pane(t);
    CHECK(shows_manager());
    CHECK(shows_editor());

    // The Editor alone -- and it MOVES UP, which is the honest answer and not the one this
    // case used to give. Both built-ins share the stack now, so the second one seated takes
    // the second slot and removing the first promotes it. When the other kind was Info that
    // could not happen: Info took no slot, so its removal moved nothing. The independence
    // this case is about is PRESENCE, not address -- each panel opens and closes without
    // reference to the other -- and the slot arithmetic is `seat_panes`' own tier.
    pick(t, panel::kPaneEditor);
    CHECK_FALSE(shows_manager());
    CHECK(shows_editor());
    CHECK(cells_covered(bounds_of(t.session().panels, t.session().setup.active,
                                  panel::kEditor, screen_of(t.session()))
                            .rect) ==
          placement_bounds(placement::kOverlayStack, 0, screen_of(t.session())));

    // Neither again. An empty screen around a live document is a legitimate state, and
    // the document is still all there.
    open_editor_pane(t);
    CHECK_FALSE(shows_manager());
    CHECK_FALSE(shows_editor());
    CHECK(t.w->document().elements.size() == 2);

    // And back to both, in the other order.
    open_editor_pane(t);
    pick(t, panel::kPaneEditor);
    CHECK(shows_manager());
    CHECK(shows_editor());
    // AND NOTHING HERE ASKED THE TOOL ANYTHING. Opening the Builder panel used to, twice;
    // no built-in speaks to a participant on open now.
    CHECK(tool->described == 0);
}

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
    t.key(input::scan::kN);
    pick(t, panel::kPaneEditor);
    CHECK(t.w->session().panels.has(panel::kPaneEditor));
    CHECK(panel_shown(t.canvases.back(), t.session(), panel::kPaneEditor)
              .find("PANE MANAGER") != std::string::npos);

    pick(t, panel::kPaneEditor);
    CHECK_FALSE(t.w->session().panels.has(panel::kPaneEditor));
    pick(t, panel::kPaneEditor);
    CHECK(t.w->session().panels.has(panel::kPaneEditor));
    CHECK(panel_shown(t.canvases.back(), t.session(), panel::kPaneEditor)
              .find("PANE MANAGER") != std::string::npos);

    // AND NO MESSAGE WENT TO THE ONE OFFICE WORKSHOP KNOWS ABOUT. With the
    // stand-in mounted, opening and removing the panel leaves its counters at zero --
    // the sends in `choose_panel` belong to the Builder kind and to no other.
    Live u;
    ToolSeat* tool = mount_tool(u, "zengine-snake");
    pick(u, panel::kPaneEditor);
    pick(u, panel::kPaneEditor);
    pick(u, panel::kPaneEditor);
    CHECK(tool->described == 0);
    CHECK(tool->asked.empty());
}

TEST_CASE("the document is still a document with the panel removed") {
    Live t;
    pick(t, panel::kPaneEditor);
    const std::size_t born = t.w->document().elements.size();
    REQUIRE(born == 2);
    pick(t, panel::kPaneEditor);
    REQUIRE_FALSE(t.w->session().panels.has(panel::kPaneEditor));

    // Every gesture that authors: they are the WORKSPACE's, not the panel's, and
    // removing a panel does not remove the results.
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
          "workspace 78x16 cells");

    // Reopening finds a panel whose rows are re-derived, not restored: what it inventories
    // was authored while nobody was showing it.
    // WHAT IT USED TO FIND WAS THE OBJECT `#N` THIS CASE MADE, in Info's OBJECTS list. That
    // row is `Zengine/info-pane/`'s now and is measured across the seam; what a
    // Workshop-side case can still ask is that the reopened panel is reading the CURRENT
    // desk, which is the same claim one list over.
    pick(t, panel::kPaneEditor);
    CHECK(panel_shown(t.canvases.back(), t.session(), panel::kPaneEditor)
              .find("Pane Manager") != std::string::npos);
}

TEST_CASE("x is an unbound key again") {
    // BLD-0 bound it to "close the Builder"; the second kind made that a choice
    // the key could not make, so presence moved to the picker and this went back
    // to meaning nothing. A key that still half-worked would be the worst of the
    // three available outcomes.
    Live t;
    (void)mount_tool(t, "zengine-snake");
    open_editor_pane(t);
    REQUIRE(t.w->session().panels.has(panel::kEditor));
    const std::string notice = t.w->session().notice;

    t.key(input::scan::kX);
    CHECK(t.w->session().panels.has(panel::kEditor));
    CHECK(t.w->session().panels.has(panel::kLayouts)); // and the desk's other panel stands
    CHECK(t.w->session().notice == notice); // it said nothing, because it means nothing
}

TEST_CASE("the picker's state column follows the panels, not a memory of them") {
    Live t;
    (void)mount_tool(t, "zengine-snake");

    t.key(input::scan::kP);
    CHECK(stack_text(t.canvases.back())
              .find(detail::pad("Editor", kPickerNameCols) + "closed") != std::string::npos);
    CHECK(stack_text(t.canvases.back())
              .find(detail::pad("Pane Manager", kPickerNameCols) + "closed") !=
          std::string::npos);
    t.key(input::scan::kEscape);

    open_editor_pane(t);
    t.key(input::scan::kP);
    CHECK(stack_text(t.canvases.back())
              .find(detail::pad("Editor", kPickerNameCols) + "open") != std::string::npos);
    CHECK(stack_text(t.canvases.back())
              .find(detail::pad("Pane Manager", kPickerNameCols) + "closed") !=
          std::string::npos);
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
    open_editor_pane(t);
    REQUIRE(stack_text(t.canvases.back()).find("Editor") != std::string::npos);

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
    CHECK(visible.find(detail::pad("Editor", kPickerNameCols) + "open") != std::string::npos);
    // Not one row of the panel underneath survives. The word is chosen to be PANEL-UNIQUE:
    // the picker's own row says `Editor` too, and `edit a source file` shares most of its
    // words with the pane's header -- `no source open` is the pane's sentence and nothing
    // else on this screen says it.
    CHECK(visible.find("no source open") == std::string::npos);

    // AND THE CANARY: dismissing the picker gives the panel back whole.
    t.key(input::scan::kEscape);
    CHECK(stack_text(t.canvases.back()).find("no source open") != std::string::npos);
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
    s.setup.active = two_overlays();
    s.panels.open = {Panel{panel::kEditor}, Panel{panel::kPaneEditor}};
    const Screen sc = screen_of(s);
    const PaneRef builder = ref_of(panel::kEditor);
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
    s.panels.open = {Panel{panel::kEditor}, Panel{panel::kPaneEditor}};
    const Screen sc = screen_of(s);
    const PaneRef builder = ref_of(panel::kEditor);

    // OPEN: nothing is wrong.
    REQUIRE(pane_state_of(s.panels, s.setup.active, sc,
                          CatalogRow{panel::kEditor, builder, "Editor", ""}) ==
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
    closed.setup.active = setup_of("Info only", {panel::kPaneEditor});
    closed.panels.open = {Panel{panel::kPaneEditor}};
    REQUIRE(pane_state_of(closed.panels, closed.setup.active, screen_of(closed),
                          CatalogRow{panel::kEditor, builder, "Editor", ""}) ==
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
        CHECK_FALSE(t.session().hotkeys.open);
        CHECK_FALSE(t.session().panels.picker.open);
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
    const std::int64_t selected_before = t.session().selected;
    const std::int64_t keyboard_before = t.session().panels.keyboard;
    REQUIRE_FALSE(t.session().arrange.addressed());

    SUBCASE("on a pane: the durable reference, and no selection of any kind") {
        open_pane(t, ref_of(panel::kEditor));
        const ui::Rect slot = cells_covered(
            bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
                      screen_of(t.session()))
                .rect);
        t.right_press_canvas(slot.x + 1, slot.y + 1);
        CHECK(t.menu().open);
        CHECK(t.menu().subject == context_subject::kPane);
        CHECK(t.menu().pane == ref_of(panel::kEditor));
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

    // The room: NINE zero-target doors, no groups. It was eleven until the two overlays
    // became panes -- `workshop.attention` and then `workshop.terminal` each opened one
    // particular overlay from the empty room, and what is left in their place is
    // `workshop.picker`, which was already on this list and opens the CHOICE rather than
    // any one pane (VD-22, VD-24).
    const std::vector<ContextEntry> root = context_population(context_subject::kRoot, "");
    REQUIRE(root.size() == 9);
    for (const ContextEntry& e : root) {
        CHECK_FALSE(e.is_group);
    }
    CHECK(root[0].row->act == Act::kObjectNew);
    CHECK(root[8].row->act == Act::kManageResetOrder);

    // EVERY DECLARATION RESOLVES AND OWNS NO POWER: an id `row_of_id` answers and three
    // plain fields -- the compile-time cross-check, restated where a reader looks.
    for (const ContextRow& row : kContextCatalog) {
        CHECK(row_of_id(row.action) != nullptr);
    }
}

TEST_CASE("CTX-0: a contextual action acts on the pointed pane, not the selection") {
    Live t;
    open_pane(t, ref_of(panel::kEditor));
    const std::int64_t doc_selected = t.session().selected;
    // Info, the Layouts pane, and the Builder this case just opened -- the identity
    // permutation `add_pane` assigns, in list order.
    REQUIRE(ranks_of(t.session().setup.active) == std::vector<std::int64_t>{0, 1, 2});

    // Point at the BUILDER and send it to the back through the Order group.
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().subject == context_subject::kPane);
    REQUIRE(t.menu().pane == ref_of(panel::kEditor));
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
    CHECK(t.notice().find(ref_text(ref_of(panel::kEditor))) != std::string::npos);
}

TEST_CASE("CTX-0/ARR-0: contextual Arrange admission precedes binding") {
    Live t;
    // TWO PANES, ON A SCREEN THAT SEATS TWO. The first subcase captures a pane and then
    // pushes THAT pane off the screen, so it needs a second pane to point at -- and both
    // built-ins are in the stack now, which the minimum screen has one slot of.
    t.publish(loom::to_value(surface::SurfaceExtent{120, 44, 0, 0}));
    open_pane(t, ref_of(panel::kPaneEditor));
    open_pane(t, ref_of(panel::kEditor));

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
            bounds_of(t.session().panels, t.session().setup.active, panel::kPaneEditor,
                      screen_of(t.session()))
                .rect);
        t.right_press_canvas(side.x + side.w - 1, side.y + 1);
        REQUIRE(t.menu().subject == context_subject::kPane);
        REQUIRE(t.menu().pane == ref_of(panel::kPaneEditor));
        // ...and the pane the menu is holding goes off this screen before the key lands.
        REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kPaneEditor),
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
            bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
                      screen_of(t.session()))
                .rect);
        t.right_press_canvas(slot.x + 1, slot.y + 1);
        t.key(input::scan::kReturn); // Arrange
        CHECK_FALSE(t.menu().open);
        CHECK(t.session().arrange.open);
        CHECK_FALSE(t.session().arrange.desk); // the ONE-PANE scope, not the old selector
        CHECK(t.session().arrange.pane == ref_of(panel::kEditor));
        // MOVING AND RESIZING THE SAME PANE NEED NO STATE CHANGE IN BETWEEN (ARR-0):
        // an arrow places it and a shifted arrow resizes it, in the state already open.
        t.key(input::scan::kRight);
        const SetupPane* placed = pane_of(t.session().setup.active, ref_of(panel::kEditor));
        REQUIRE(placed != nullptr);
        CHECK(placed->place.mode == pane_unit::kSubcells);
        t.key(input::scan::kRight, input::mod::kShift);
        const SetupPane* sized = pane_of(t.session().setup.active, ref_of(panel::kEditor));
        CHECK(sized->width.mode == pane_unit::kSubcells);
        CHECK(t.session().arrange.open); // still the one state, nothing was left or entered
        CHECK_FALSE(t.session().arrange.desk);
    }
}

TEST_CASE("CTX-0: a captured pane that left the setup is refused truthfully") {
    Live t;
    open_pane(t, ref_of(panel::kEditor));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    REQUIRE(t.menu().pane == ref_of(panel::kEditor));
    // The reference leaves the setup UNDER the open surface -- the clearing that keeps
    // the mode's own selection fresh does not know this subject exists.
    REQUIRE(remove_pane(live(t).setup.active, ref_of(panel::kEditor)));

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
    open_pane(t, ref_of(panel::kEditor));
    enter_arrange_desk(t);
    select_pane(t, ref_of(panel::kEditor));
    t.key(input::scan::kD);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(panel::kEditor)));
    // The presentation followed the intent through the one door, and the removed
    // reference cleared the keyboard's address on membership -- the DESK stays open,
    // because its subject is the desk and the desk is still there (ARR-0).
    for (const Panel& p : t.session().panels.open) {
        CHECK(p.kind != panel::kEditor);
    }
    CHECK(t.session().arrange.open);
    CHECK(t.session().arrange.desk);
    CHECK_FALSE(t.session().arrange.addressed());
    CHECK(t.notice().find("removed") != std::string::npos);
    CHECK(t.notice().find("nothing behind it was touched") != std::string::npos);
}

TEST_CASE("CTX-0: a contextual remove removes the pointed pane") {
    Live t;
    open_pane(t, ref_of(panel::kEditor));
    const std::int64_t selected_before = t.session().selected;
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
                  screen_of(t.session()))
            .rect);
    t.right_press_canvas(slot.x + 1, slot.y + 1);
    t.key(input::scan::kUp); // the cursor bound keeps it at the top; up is a no-op
    t.key(input::scan::kDown);
    t.key(input::scan::kDown);
    t.key(input::scan::kDown); // remove, the last top-level row
    t.key(input::scan::kReturn);
    CHECK_FALSE(t.menu().open);
    CHECK_FALSE(has_pane(t.session().setup.active, ref_of(panel::kEditor)));
    for (const Panel& p : t.session().panels.open) {
        CHECK(p.kind != panel::kEditor);
    }
    CHECK(t.session().selected == selected_before);
    CHECK(t.notice().find("removed") != std::string::npos);
}

TEST_CASE("CTX-0: navigation backtracks cleanly and every way out closes") {
    Live t;
    open_pane(t, ref_of(panel::kEditor));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
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
    // ⚠ THE TERMINAL OVERLAY WAS THE FIRST SUBCASE HERE (VD-24) -- "a second button still
    // means nothing there", proved by opening the mode and pressing right inside it. It was
    // the only mode in this application that owned the pointer ANYWHERE on the screen, which
    // is exactly what a pane does not do. What is left is the mode that still owns one: an
    // arrangement scope.
    SUBCASE("an arrangement scope: the press LEAVES it, consumed whole (SC-6)") {
        open_pane(t, ref_of(panel::kEditor));
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
    open_pane(t, ref_of(panel::kEditor));
    const ui::Rect slot = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
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
    CHECK(t.menu().pane == ref_of(panel::kEditor));
}

TEST_CASE("ARR-0/SC-6: every arrangement level claims the press; the menu keeps its own") {
    Live t;
    open_pane(t, ref_of(panel::kEditor));

    SUBCASE("the desk") {
        enter_arrange_desk(t);
        t.right_press(40, 0);
        CHECK_FALSE(t.session().arrange.open);
        CHECK_FALSE(t.menu().open);
    }
    SUBCASE("the reset prompt leaves the whole interaction") {
        enter_arrange_desk(t);
        select_pane(t, ref_of(panel::kEditor));
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
            bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
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
    // LAYOUT ONE: the Pane Manager alone, moved somewhere a maker chose. It was Info, which
    // a fresh desk had open; Info is a weave and arrives with its office, so a layout that
    // needs a pane on it opens one.
    pick(t, panel::kPaneEditor);
    REQUIRE(t.session().panels.has(panel::kPaneEditor));
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kPaneEditor),
                              surface::subs_of_cells(3), surface::subs_of_cells(4))
                .accepted);
    const Setup first = t.session().setup.active;

    press_gesture(t, k.make);
    // LAYOUT TWO: a different membership, a different place, a different front order.
    open_editor_pane(t);
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kEditor),
                              surface::subs_of_cells(9), surface::subs_of_cells(2))
                .accepted);
    REQUIRE(send_to_back(live(t).setup.active, ref_of(panel::kEditor)));
    const Setup second = t.session().setup.active;
    REQUIRE(first != second);

    // BACK AND FORTH, AND EACH ONE COMES BACK BYTE FOR BYTE.
    for (int round = 0; round < 3; ++round) {
        CAPTURE(round);
        press_gesture(t, k.previous);
        CHECK(t.session().setup.active == first);
        CHECK(t.session().panels.has(panel::kPaneEditor));
        CHECK_FALSE(t.session().panels.has(panel::kEditor));
        press_gesture(t, k.next);
        CHECK(t.session().setup.active == second);
        CHECK(t.session().panels.has(panel::kEditor));
        // ...AND THE AUTHORED FRONT ORDER WITH IT: the Builder was put BEHIND Info in
        // this layout, so it is the first thing painted and the last thing pressed.
        CHECK(painted_order(t.session()).front() == panel::kEditor);
    }

    // THE PRESENTATIONS ARE RECONCILED THROUGH THE ONE DOOR, so the panels a switch left
    // open are exactly the ones the destination names -- in its own list order.
    CHECK(authored_order(t.session()) == presentation_order(second, t.session().panels));
}

TEST_CASE("WUX-9/SC-5: a switch touches no Workshop-global fact") {
    Live t;
    const LayoutKeys k = layout_keys(t);
    open_editor_pane(t);
    // A SELECTION AND A KEYBOARD CANDIDATE, made by a press exactly as a maker makes them.
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.session().panels.selected == panel::kEditor);
    const std::int64_t selected = t.session().panels.selected;
    const std::int64_t keyboard = t.session().panels.keyboard;
    const WorkshopDoc document = t.doc();
    const std::uint64_t doc_epoch = t.session().editor.doc_epoch;
    const std::string source = t.session().editor.path;

    // A LAYOUT WITHOUT THE BUILDER IN IT -- and since WUX-11 that is what `layout.new`
    // MAKES: a fresh blank desk, whose membership `apply_setup` reconciles to through the
    // one door membership changes through.
    press_gesture(t, k.make);
    live(t).setup.active.name = "Inspect";
    REQUIRE_FALSE(t.session().panels.has(panel::kEditor));

    // THE SELECTION IS NOT DESTROYED BY THE SWITCH -- it simply resolves to nothing while
    // its pane is absent, which is `selected_pane`'s own discipline (WUX-5).
    CHECK(t.session().panels.selected == selected);
    CHECK(t.session().panels.keyboard == keyboard);
    CHECK(selected_pane(t.session().panels) == kNoPaneKind);
    CHECK(keyboard_pane(t.session().panels) == kNoPaneKind);
    // ...AND IT LIFTS NOTHING: no ghost foreground for a pane that is not on this desk.
    for (const std::int64_t kind : painted_order(t.session())) {
        CHECK(kind != panel::kEditor);
    }
    // THE DOCUMENT AND THE SOURCE EDITOR ARE ONE TRUTH EACH, AND A SWITCH IS NOT A DOOR
    // TO EITHER.
    CHECK(t.doc() == document);
    CHECK(t.session().editor.doc_epoch == doc_epoch);
    CHECK(t.session().editor.path == source);

    // AND COMING BACK MAKES THE RETAINED SELECTION MEAN SOMETHING AGAIN.
    press_gesture(t, k.previous);
    CHECK(t.session().panels.selected == selected);
    CHECK(selected_pane(t.session().panels) == panel::kEditor);
    CHECK(painted_order(t.session()).back() == panel::kEditor);
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
    open_editor_pane(t);
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.session().panels.selected == panel::kEditor);

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
    open_editor_pane(t);
    const std::size_t runtime_before = t.session().panels.runtime.entries.size();
    const std::size_t external_before = t.session().panels.external.size();
    const WorkshopDoc document = t.doc();
    const Setup was = t.session().setup.active;

    press_gesture(t, k.make);
    // A FRESH DESK, NOT A COPY (WUX-11). What a blank layout changes is the PRESENTATION:
    // it names no Builder, so `apply_setup` withdraws that presentation exactly as any
    // other whole-desk replacement does.
    CHECK(t.session().setup.active == default_setup());
    CHECK_FALSE(t.session().panels.has(panel::kEditor));
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
    CHECK(t.session().panels.has(panel::kEditor));
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
            s.setup.active = setup_of(std::string(len, 'z'), {panel::kPaneEditor});
            for (std::size_t more = 1; more < count; ++more) {
                s.setup.shelved.push_back(Layout{
                    setup_of(std::string(len, static_cast<char>('a' + more)), {panel::kPaneEditor}),
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
    // ⭐ `f0`, NOT `f1`. Layouts is the FIRST row `default_setup` authors now -- the Info
    // row that used to precede it is a weave the desk names after it -- so it is back-most
    // rather than one up from it. The count is still three: the two the desk boots with and
    // the Pane Manager this case opened.
    CHECK(editor_value(t, "Front").find("f0 of 3") == 0);
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
    const PaneRef builder = ref_of(panel::kEditor);
    REQUIRE(add_pane(live(t).setup.active, builder));
    t.publish(loom::to_value(surface::SurfaceExtent{80, 22, 0, 0})); // a reconcile
    REQUIRE(has_pane(t.session().setup.active, builder));
    REQUIRE_FALSE(t.session().panels.has(panel::kEditor)); // authored, and waiting
    press_into_editor(t);
    choose_by_keys(t, ref_of(panel::kPaneEditor));
    type_value(t, "X", "2");
    CHECK_FALSE(t.session().notice_is_bad);
    // THE EDITOR LEFT THE REACTIVE STACK, AND THE WAITING PANE WAS SEATED IN THE SLOT IT
    // VACATED -- which only a reconcile does.
    CHECK(t.session().panels.has(panel::kEditor));
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
    const PaneRef closed = ref_of(panel::kEditor);
    REQUIRE_FALSE(t.session().panels.has(panel::kEditor));
    choose_by_keys(t, closed);
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
    CHECK(t.notice() == "opened Editor -- o removes it");
    CHECK(t.session().panels.has(panel::kEditor));
    CHECK(t.session().pane_editor.subject == closed);
    CHECK(editor_value(t, "State") == "open");
    CHECK(editor_value(t, "Open") == "yes -- o removes it");
    CHECK(editor_value(t, "X") == "-");
    // ...AND REMOVE IT AGAIN: the subject STANDS, the row is still in the list.
    t.key(input::scan::kO);
    CHECK(t.notice().find("removed Editor -- o brings it back") == 0);
    CHECK_FALSE(t.session().panels.has(panel::kEditor));
    CHECK(t.session().pane_editor.subject == closed);
    CHECK(editor_value(t, "State") == "closed -- open it from the picker");
    CHECK(inventory_index(t, closed) < 99);

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
    open_pane(t, ref_of(panel::kEditor));
    const ui::Rect builder = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor,
                  screen_of(t.session()))
            .rect);
    t.press_canvas(builder.x + 1, builder.y + 1);
    REQUIRE(t.session().panels.selected == panel::kEditor);
    REQUIRE(keyboard_context(t.session()) == KeyContext::kCommand); // the Builder takes no keys
    const std::vector<std::int64_t> order_before = presentation_order(t.session().setup.active, t.session().panels);
    const Setup setup_before = t.session().setup.active;
    const std::size_t panes_before = t.session().panels.open.size();

    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.session().panels.keyboard == kNoPaneKind);
    CHECK(t.session().pane_editor.subject == layouts); // SC-3: the subject is a different fact
    CHECK(t.notice().find("unselected Editor") != std::string::npos);
    // NOTHING ELSE MOVED (SC-10): no pane closed, no rank, no geometry, no file.
    CHECK(t.session().panels.open.size() == panes_before);
    CHECK(presentation_order(t.session().setup.active, t.session().panels) == order_before);
    CHECK(t.session().setup.active == setup_before);
    CHECK(t.session().panels.has(panel::kEditor));
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
    // reaches command mode -- the Layouts band, which the catalog says does not take the
    // keyboard. It was Info, which took none either and is not on this desk.
    const Screen sc = screen_of(t.session());
    const ui::Rect band = cells_covered(
        bounds_of(t.session().panels, t.session().setup.active, panel::kLayouts, sc).rect);
    t.press_canvas(band.x + 1, band.y);
    REQUIRE(t.session().panels.selected == panel::kLayouts);
    t.key(input::scan::kP);
    REQUIRE(t.session().panels.picker.open);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().panels.picker.open);          // `picker.close` answered...
    CHECK(t.session().panels.selected == panel::kLayouts); // ...and the selection stood
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
    t.press_canvas(band.x + 1, band.y);
    REQUIRE(t.session().panels.selected == panel::kLayouts);
    t.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(t.session().hotkeys.open);
    t.key(input::scan::kEscape);
    CHECK_FALSE(t.session().hotkeys.open);
    CHECK(t.session().panels.selected == panel::kLayouts);
}

TEST_CASE("QR-18/SC-4: a desk with no unoccupied cell still reaches selection = none") {
    // THE RECOVERY CLAIM. Every cell between the two bands is some pane's, so there is no
    // blank pixel to press; Escape is the way down.
    Live t;
    t.publish(loom::to_value(surface::SurfaceExtent{132, 46, 0, 0}));
    open_pane(t, ref_of(panel::kEditor));
    const Screen sc = screen_of(t.session());
    // The Builder over the whole room, the side column included -- an authored window is
    // canvas-absolute (WUX-2), and the room is what a pane may cover.
    REQUIRE(author_pane_place(live(t).setup.active, ref_of(panel::kEditor),
                              surface::subs_of_cells(0), surface::subs_of_cells(kTopRows))
                .accepted);
    const Written sized =
        author_pane_size(live(t).setup.active, ref_of(panel::kEditor),
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
        bounds_of(t.session().panels, t.session().setup.active, panel::kEditor, now).rect);
    t.press_canvas(builder.x + 3, builder.y + 3);
    REQUIRE(t.session().panels.selected == panel::kEditor);
    t.key(input::scan::kEscape);
    CHECK(t.session().panels.selected == kNoPaneKind);
    CHECK(t.session().panels.keyboard == kNoPaneKind);
    CHECK(t.session().panels.has(panel::kEditor)); // still there, still that big
}

TEST_CASE("QR-18/SC-5: the Pane Editor's two lists are reached by the wheel past their windows") {
    // MUTATION (F5): dropping the Pane Editor's wheel arm while the `... N more` row stays
    // -- the hidden name below never appears.
    Live t;
    // THE INVENTORY ENTRIES THAT WINDOW THE PANES LIST (WUX-13). They used to be BUILT-INS;
    // the project browser and the Builder panel are loaded weaves now, so the entries come
    // from offices that offered them -- which is the same fact the list is about and is a
    // stronger fixture, because the window has nothing to do with where a pane came from.
    REQUIRE(live_offer_pane(t, "zengine.test.seated", "seated", "Seated") != kNoPaneKind);
    REQUIRE(live_offer_pane(t, "zengine.test.second", "second", "Second") != kNoPaneKind);
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

