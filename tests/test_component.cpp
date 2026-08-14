// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Component suite — the first foundational Zen component, on its own.
//
// EVERYTHING HERE IS PURE, AND THAT IS THE CLAIM RATHER THAN A CONVENIENCE. There is no bus,
// no weave, no kernel, no canvas, no Skin, no Workshop and no Screen in this file: the whole
// of `TextBox` is a std::string and two indices, and if any of those had to be
// booted to test it, the component would not be one. This suite links `zengine-component` and
// nothing else — it is the only suite in this repository that does not even reach loom::core.
//
// THE TIERS:
//
//   1. WHAT A CHARACTER IS — the four boundary walks, which are the whole of this
//      application's Unicode position.
//   2. THE CARET — a position in the text, and every operation keeping it in the text. Moved
//      here from the Workshop suite by HD-5: what a TextBox DOES is this suite's claim, and
//      that the Terminal's line and a property draft get their answers from it is Workshop's.
//   3. THE WINDOW — `first_visible`, the four caret-follow rules, and the slice.
//   4. THE POINTER — a column of a visible slice becoming a byte of the whole text.
//   5. WHAT IT IS NOT — a component with no domain, no medium, no identity and no policy.
//
// WHY THE THIRD TIER IS SO LARGE: the viewport is the half a second consumer needed and the
// half a picture cannot check. A case that could only see the window through a painted row
// could not tell a window that is right from one that is right by accident.

// main() and the framework live in doctest_main.cpp -- the shared one that refuses a run
// selecting zero cases (POP-01).
#include "doctest.h"

#include "component/text_box.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

using namespace zengine::component;

namespace {

/// Every position a text has, as a list — the fixture the boundary cases walk.
std::vector<std::size_t> boundaries(const std::string& text) {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || !is_continuation_byte(text[i])) {
            out.push_back(i);
        }
    }
    return out;
}

} // namespace

// ---- 1. What a character is --------------------------------------------------------------

TEST_CASE("component: a character boundary is one rule, walked in four directions") {
    // The four helpers are the whole of what "not in the middle of a character" means in this
    // application, and they moved here from `workshop/property.hpp` with HD-5 because both of
    // the consumers that spend them ARE this component now.
    const std::string line = "a\xC3\xA9z\xE2\x82\xAC"; // a, e-acute (2), z, euro (3)
    REQUIRE(line.size() == 7);

    CHECK_FALSE(is_continuation_byte(line[0]));
    CHECK_FALSE(is_continuation_byte(line[1])); // the lead byte of the accented letter
    CHECK(is_continuation_byte(line[2]));       // ...and its continuation
    CHECK(is_continuation_byte(line[5]));
    CHECK(is_continuation_byte(line[6]));

    // BACKWARDS: one whole character, never one byte.
    CHECK(character_before(line, 7) == 4); // over the euro sign
    CHECK(character_before(line, 4) == 3);
    CHECK(character_before(line, 3) == 1); // over the accented letter
    CHECK(character_before(line, 1) == 0);
    CHECK(character_before(line, 0) == 0); // there is none, and that is an answer
    CHECK(character_before(line, 999) == 4);

    // FORWARDS: the same rule, the other way.
    CHECK(character_after(line, 0) == 1);
    CHECK(character_after(line, 1) == 3);
    CHECK(character_after(line, 3) == 4);
    CHECK(character_after(line, 4) == 7);
    CHECK(character_after(line, 7) == 7);
    CHECK(character_after(line, 999) == 7);

    // AT OR BEFORE — a press: the character it LANDED on, never the one after it.
    CHECK(character_boundary(line, 2) == 1);
    CHECK(character_boundary(line, 5) == 4);
    CHECK(character_boundary(line, 6) == 4);
    CHECK(character_boundary(line, 4) == 4);
    CHECK(character_boundary(line, 999) == 7);

    // AT OR AFTER — a window: hide one more character rather than begin inside one, because
    // snapping a window's START backwards carries its right EDGE back with it (HD-4).
    CHECK(character_boundary_at_or_after(line, 2) == 3);
    CHECK(character_boundary_at_or_after(line, 5) == 7);
    CHECK(character_boundary_at_or_after(line, 6) == 7);
    CHECK(character_boundary_at_or_after(line, 4) == 4);
    CHECK(character_boundary_at_or_after(line, 999) == 7);

    // THE TWO DIRECTIONS COMPOSE IN EITHER ORDER over every position, which is the property
    // that lets a caret rule and a window rule both run without either undoing the other.
    for (std::size_t i = 0; i <= line.size(); ++i) {
        CAPTURE(i);
        const std::size_t back = character_boundary(line, i);
        const std::size_t fwd = character_boundary_at_or_after(line, i);
        CHECK(back <= i);
        CHECK(fwd >= i);
        CHECK(character_boundary(line, back) == back);           // idempotent
        CHECK(character_boundary_at_or_after(line, fwd) == fwd); // ...both ways
    }

    // Plain ASCII is every position, which is why nothing above is visible in ordinary use.
    CHECK(boundaries("hello").size() == 6);
    CHECK(boundaries(line).size() == 5); // four characters and the end
}

// ---- 2. The caret ------------------------------------------------------------------------

TEST_CASE("component: an empty box has exactly one position, and every operation keeps it") {
    TextBox in;
    CHECK(in.empty());
    CHECK(in.size() == 0);
    CHECK(in.text().empty());
    CHECK(in.caret() == 0);
    CHECK(in.first_visible() == 0);
    CHECK(in.caret_column() == 0);
    CHECK(in.at_end());
    CHECK(in.visible(20).empty());

    // None of these has anywhere to go, and none of them invents somewhere.
    in.left();
    in.right();
    in.home();
    in.end();
    in.backspace();
    in.erase_forward();
    in.place(999);
    in.keep_caret_visible(0);
    in.keep_caret_visible(-4);
    CHECK(in.caret() == 0);
    CHECK(in.first_visible() == 0);
    CHECK(in.empty());
}

TEST_CASE("component: typing goes in AT the caret, not at the end") {
    TextBox in;
    in.type("hello world");
    in.home();
    in.type(">> ");
    CHECK(in.text() == ">> hello world");
    CHECK(in.caret() == 3);

    // A middle insert: the whole reason the second consumer earned this component. Repairing
    // `hellp` costs one move and one keystroke rather than seven deletions and seven retypes.
    TextBox typo;
    typo.type("hellp world");
    typo.place(4);
    typo.erase_forward();
    typo.type("o");
    CHECK(typo.text() == "hello world");
    CHECK(typo.caret() == 5);
}

TEST_CASE("component: backspace and delete are different gestures, aimed the same way") {
    TextBox in;
    in.type("abcd");
    in.place(2);

    in.erase_forward(); // takes what is AT the caret; the caret does not move
    CHECK(in.text() == "abd");
    CHECK(in.caret() == 2);

    in.backspace(); // takes what is BEFORE it, and follows it back
    CHECK(in.text() == "ad");
    CHECK(in.caret() == 1);

    in.home();
    in.backspace(); // nothing before the start
    CHECK(in.text() == "ad");
    CHECK(in.caret() == 0);

    in.end();
    in.erase_forward(); // nothing after the end
    CHECK(in.text() == "ad");
    CHECK(in.caret() == 2);
}

TEST_CASE("component: set and clear are the two doors that replace the whole text") {
    TextBox in;
    in.type("a long value that was already there");
    in.keep_caret_visible(10);
    REQUIRE(in.first_visible() > 0);

    // `set` says where the caret goes, because the alternative is a call site that changes
    // the text and forgets. And the WINDOW starts over: an offset into the text that WAS
    // there is not a fact about the text that is.
    in.set("short", 2);
    CHECK(in.text() == "short");
    CHECK(in.caret() == 2);
    CHECK(in.first_visible() == 0);

    // A caret past the end of the new text lands at its end rather than outside it.
    in.set("ab", 900);
    CHECK(in.caret() == 2);
    // ...and one inside a character snaps, exactly as a press does.
    in.set("a\xC3\xA9z", 2);
    CHECK(in.caret() == 1);

    in.clear();
    CHECK(in.empty());
    CHECK(in.caret() == 0);
    CHECK(in.first_visible() == 0);
}

// The four cases below MOVED HERE from the Workshop suite with HD-5, unchanged apart from
// the type's name. What a TextBox does is this suite's claim; that the Terminal's line and a
// property draft get their answers from it is Workshop's.

TEST_CASE("the caret is a position in the line, and every operation keeps it in the line") {
    // §2's invariant, exercised as operations rather than asserted as a comment: it holds
    // because the operations are the only door, so this case walks through all of them.
    TextBox in;
    CHECK(in.caret() == 0);
    CHECK(in.at_end());

    in.type("abc");
    CHECK(in.text() == "abc");
    CHECK(in.caret() == 3);
    CHECK(in.at_end());

    in.left();
    CHECK(in.caret() == 2);
    CHECK_FALSE(in.at_end());
    in.type("X");
    CHECK(in.text() == "abXc"); // §24: type abc, Left, type X
    CHECK(in.caret() == 3);

    in.backspace();
    CHECK(in.text() == "abc");
    CHECK(in.caret() == 2);
    in.erase_forward();
    CHECK(in.text() == "ab");
    CHECK(in.caret() == 2); // Delete does not move the caret; the text comes to meet it

    in.home();
    CHECK(in.caret() == 0);
    in.backspace();          // at the start, and there is nothing before it
    CHECK(in.text() == "ab");
    CHECK(in.caret() == 0);
    in.left();               // ...and stepping off the start stops at the start
    CHECK(in.caret() == 0);
    in.end();
    CHECK(in.caret() == 2);
    in.right();              // ...as does stepping off the end
    CHECK(in.caret() == 2);
    in.erase_forward();      // at the end, and there is nothing at it
    CHECK(in.text() == "ab");

    // A POSITION FROM OUTSIDE THE TEXT is clamped, never held.
    in.place(999);
    CHECK(in.caret() == 2);
    in.place(0);
    CHECK(in.caret() == 0);

    // REPLACEMENT CANNOT LEAVE A STALE CARET, because the door takes both.
    in.set("send * SurfaceText 1 ", 21);
    CHECK(in.caret() == 21);
    CHECK(in.at_end());
    in.set("hi", 900);
    CHECK(in.caret() == 2); // clamped by the same rule
    in.clear();
    CHECK(in.caret() == 0);
    CHECK(in.empty());

    // A SHORTER LINE CANNOT STRAND THE CARET PAST ITS END -- the property every one of the
    // above rests on, checked directly.
    in.set("abcdef", 6);
    for (int i = 0; i < 10; ++i) {
        in.backspace();
        CHECK(in.caret() <= in.size());
    }
    CHECK(in.empty());
}

TEST_CASE("the caret steps over a character, never into the middle of one") {
    // Workshop already decided what a character is -- `erase_one_character` walks UTF-8
    // continuation bytes so that a backspace over an accented letter does not leave half of
    // it behind. HD-3 spends the SAME two functions, so a caret cannot land somewhere a
    // backspace would refuse to.
    TextBox in;
    in.type("a\xC3\xA9z"); // a, e-acute (two bytes), z
    CHECK(in.size() == 4);
    CHECK(in.caret() == 4);

    in.left();
    CHECK(in.caret() == 3); // before 'z'
    in.left();
    CHECK(in.caret() == 1); // before the accented letter, NOT between its two bytes
    in.left();
    CHECK(in.caret() == 0);
    in.right();
    CHECK(in.caret() == 1);
    in.right();
    CHECK(in.caret() == 3); // over the whole character, not one byte of it

    // A PRESS THAT LANDS ON A CONTINUATION BYTE SNAPS BACK to the character it hit.
    in.place(2);
    CHECK(in.caret() == 1);

    // ...and erasing from there takes the whole character.
    in.end();
    in.backspace();
    in.backspace();
    CHECK(in.text() == "a");

    // WHAT IS NOT CLAIMED, said out loud: the accented letter occupies TWO columns in this
    // presentation, because the projection is one cell per byte and the publisher's `fit`
    // cuts at a byte. The caret agrees with what is drawn, which is the property that
    // matters; codepoint and grapheme correctness are not claimed anywhere in Workshop.
    // WHAT IS NOT CLAIMED, said out loud: the accented letter occupies TWO columns in every
    // presentation of this component, because each of them is one column per BYTE. The caret
    // agrees with what is drawn, which is the property that matters; codepoint and grapheme
    // correctness are not claimed here and are not claimed anywhere above it.
    in.set("a\xC3\xA9z", 3);
    CHECK(in.caret_column() == 3); // three COLUMNS for two characters, and that is the truth
}

TEST_CASE("HD-4: the window is state, and every operation leaves the caret inside it") {
    // §2 and §24's viewport matrix, over the class rather than through a painted row: what is
    // being pinned is the invariant itself, and a case that could only see it through a
    // picture could not tell a window that is right from one that is right by accident.
    constexpr std::int64_t kRoom = 10;
    const auto inside = [](const TextBox& in, std::int64_t room) {
        return in.first_visible() <= in.caret() &&
               in.caret() - in.first_visible() <= static_cast<std::size_t>(room);
    };

    SUBCASE("a line that fits does not move the window") {
        TextBox in;
        in.type("abcdefghi"); // nine, one short of the room
        in.keep_caret_visible(kRoom);
        CHECK(in.first_visible() == 0);
        CHECK(in.visible(kRoom) == "abcdefghi");
        CHECK(inside(in, kRoom));
    }

    SUBCASE("an exact fit still shows the whole line") {
        TextBox in;
        in.type("abcdefghij"); // exactly the room, with the caret after the last character
        in.keep_caret_visible(kRoom);
        CHECK(in.first_visible() == 0);
        CHECK(in.visible(kRoom) == "abcdefghij");
        CHECK(inside(in, kRoom));
    }

    SUBCASE("one byte beyond the fit moves the window by exactly one") {
        TextBox in;
        in.type("abcdefghijk");
        in.keep_caret_visible(kRoom);
        CHECK(in.first_visible() == 1);
        CHECK(in.visible(kRoom) == "bcdefghijk");
        CHECK(in.text() == "abcdefghijk"); // the authored line is untouched by the scroll
    }

    SUBCASE("many beyond the fit, and Home and End reach both ends") {
        TextBox in;
        in.type("abcdefghijklmnopqrstuvwxyz"); // 26
        in.keep_caret_visible(kRoom);
        CHECK(in.first_visible() == 16);
        CHECK(in.visible(kRoom) == "qrstuvwxyz");

        in.home();
        in.keep_caret_visible(kRoom);
        CHECK(in.first_visible() == 0);
        CHECK(in.visible(kRoom) == "abcdefghij");
        CHECK(inside(in, kRoom));

        in.end();
        in.keep_caret_visible(kRoom);
        CHECK(in.first_visible() == 16);
        CHECK(in.visible(kRoom) == "qrstuvwxyz");
        CHECK(inside(in, kRoom));
    }

    SUBCASE("repeated Left walks the window one character at a time, and Right walks it back") {
        TextBox in;
        in.type("abcdefghijklmnopqrstuvwxyz");
        in.keep_caret_visible(kRoom);
        REQUIRE(in.first_visible() == 16);

        // INSIDE THE WINDOW NOTHING MOVES. Ten Lefts take the caret from 26 to 16, which is
        // the window's own start -- minimal movement means the window sits still for all of
        // them (§3).
        for (int i = 0; i < 10; ++i) {
            in.left();
            in.keep_caret_visible(kRoom);
            CHECK(in.first_visible() == 16);
        }
        REQUIRE(in.caret() == 16);

        // ...AND THEN IT FOLLOWS, one character per keystroke.
        for (std::size_t want = 15; want > 0; --want) {
            in.left();
            in.keep_caret_visible(kRoom);
            CHECK(in.caret() == want);
            CHECK(in.first_visible() == want); // the caret is at the left edge
        }

        // RIGHT IS THE MIRROR: ten free, then one per keystroke.
        in.home();
        in.keep_caret_visible(kRoom);
        REQUIRE(in.first_visible() == 0);
        for (int i = 0; i < 10; ++i) {
            in.right();
            in.keep_caret_visible(kRoom);
            CHECK(in.first_visible() == 0);
        }
        for (std::size_t step = 1; step <= 16; ++step) {
            in.right();
            in.keep_caret_visible(kRoom);
            CHECK(in.caret() == 10 + step);
            CHECK(in.first_visible() == step); // the caret is at the right edge
        }
    }

    SUBCASE("Backspace at the left edge scrolls, and Delete at the right edge does not") {
        TextBox in;
        in.type("abcdefghijklmnopqrstuvwxyz");
        in.home();
        for (int i = 0; i < 16; ++i) {
            in.right();
        }
        in.keep_caret_visible(kRoom);
        REQUIRE(in.caret() == 16);
        REQUIRE(in.first_visible() == 6); // the caret is at the right edge of the window

        // DELETE takes the character AT the caret. The caret does not move, and neither does
        // the window -- until the line is short enough that the window is holding blank room.
        in.erase_forward();
        in.keep_caret_visible(kRoom);
        CHECK(in.caret() == 16);
        CHECK(in.first_visible() == 6);
        CHECK(in.text() == "abcdefghijklmnoprstuvwxyz");

        // BACKSPACE at the window's left edge follows the caret out of it.
        in.home();
        for (int i = 0; i < 6; ++i) {
            in.right();
        }
        in.keep_caret_visible(kRoom);
        REQUIRE(in.caret() == 6);
        REQUIRE(in.first_visible() == 0);
        in.end();
        in.keep_caret_visible(kRoom);
        REQUIRE(in.size() == 25);
        REQUIRE(in.first_visible() == 15);
        in.place(15);
        in.keep_caret_visible(kRoom);
        REQUIRE(in.caret() == 15);
        REQUIRE(in.first_visible() == 15); // the caret is ON the window's first byte
        in.backspace();
        in.keep_caret_visible(kRoom);
        CHECK(in.caret() == 14);
        CHECK(in.first_visible() == 14);
    }

    SUBCASE("deleting back to a short line gives the room back") {
        // §3's last bullet, and the one that decides whether a long line can be repaired: a
        // window left where a long line put it shows an EMPTY row with the whole command
        // hidden away to the left, which reads exactly like a tool that lost the text.
        TextBox in;
        in.type("abcdefghijklmnopqrstuvwxyz");
        in.keep_caret_visible(kRoom);
        REQUIRE(in.first_visible() == 16);
        for (int i = 0; i < 20; ++i) {
            in.backspace();
            in.keep_caret_visible(kRoom);
        }
        CHECK(in.text() == "abcdef");
        CHECK(in.first_visible() == 0);
        CHECK(in.visible(kRoom) == "abcdef");
    }

    SUBCASE("clear and set start the window over") {
        TextBox in;
        in.type("abcdefghijklmnopqrstuvwxyz");
        in.keep_caret_visible(kRoom);
        REQUIRE(in.first_visible() == 16);

        in.clear();
        CHECK(in.first_visible() == 0);
        CHECK(in.caret() == 0);
        in.keep_caret_visible(kRoom);
        CHECK(in.first_visible() == 0);

        // A WHOLESALE REPLACEMENT (accepting a completion candidate) arrives with its caret
        // at the end of the inserted result, so the window follows to the TAIL (§10).
        in.set("0123456789abcdefghij", 20);
        CHECK(in.first_visible() == 0); // ...before the reconcile
        in.keep_caret_visible(kRoom);
        CHECK(in.first_visible() == 10);
        CHECK(in.visible(kRoom) == "abcdefghij");
    }

    SUBCASE("no blank room on the right while there is text hidden on the left") {
        // The property §18 turns on: after a reconcile the window never sits further right
        // than the last full screenful, so blank room at the right of the input row means
        // the authored line really did end there.
        TextBox in;
        in.type("abcdefghijklmnopqrstuvwxyz");
        for (std::size_t at = 0; at <= in.size(); ++at) {
            in.place(at);
            in.keep_caret_visible(kRoom);
            CAPTURE(at);
            CHECK(in.first_visible() <= in.size() - static_cast<std::size_t>(kRoom));
            CHECK(inside(in, kRoom));
        }
    }

    SUBCASE("the capacity-free half of the invariant holds before any reconcile") {
        // The invariant is kept in two halves and this is the one the OPERATIONS owe: a
        // reader between an edit and the next repaint must not find a window beginning after
        // the caret or past the end of the line. It is also the whole of the leftward scroll,
        // which is why it costs no capacity -- Left, Home, a press and a backspace at the
        // window's edge all move it here rather than in `keep_caret_visible`.
        TextBox in;
        in.type("abcdefghijklmnopqrstuvwxyz");
        in.keep_caret_visible(kRoom);
        REQUIRE(in.first_visible() == 16);

        in.left();
        CHECK(in.first_visible() == 16); // still inside the window: nothing moved
        in.home();
        CHECK(in.first_visible() == 0); // ...and no reconcile has run

        in.end();
        in.keep_caret_visible(kRoom);
        REQUIRE(in.first_visible() == 16);
        in.place(4);
        CHECK(in.first_visible() == 4);

        in.end();
        in.keep_caret_visible(kRoom);
        REQUIRE(in.first_visible() == 16);
        in.backspace();
        CHECK(in.first_visible() == 16); // the caret is still inside it
        for (int i = 0; i < 10; ++i) {
            in.backspace();
        }
        CHECK(in.caret() == 15);
        CHECK(in.first_visible() == 15); // followed the caret out, told no capacity at all

        // ...AND A REPLACEMENT SHORTER THAN THE WINDOW cannot leave it past the end.
        in.set("ab", 2);
        CHECK(in.first_visible() == 0);
        CHECK(in.caret() == 2);
    }

    SUBCASE("a row with no room at all still holds the invariant") {
        // Total over the capacity, because the capacity comes from a metric that arrived on
        // the bus: a pane too narrow for its own prompt reports zero columns.
        TextBox in;
        in.type("abcdef");
        in.keep_caret_visible(0);
        CHECK(in.first_visible() == in.caret());
        CHECK(in.visible(0).empty());
        in.home();
        in.keep_caret_visible(0);
        CHECK(in.first_visible() == 0);
        CHECK(in.visible(-4).empty());
    }
}

TEST_CASE("HD-4: the window never begins inside a character") {
    // §6. The caret already refuses to sit inside a character (HD-3); the window has to obey
    // the same rule through the same machinery, and it snaps the other way -- forwards --
    // because snapping backwards would carry the window's right edge back with it and push
    // the caret off the row it is drawn on.
    TextBox in;
    std::string accented;
    for (int i = 0; i < 12; ++i) {
        accented += "\xC3\xA9"; // é, two bytes each: every odd index is a continuation byte
    }
    in.type(accented);
    REQUIRE(in.size() == 24);

    for (std::int64_t room = 1; room <= 9; ++room) {
        for (std::size_t at = 0; at <= in.size(); ++at) {
            in.place(at); // clamped and snapped by the component, as a press would be
            in.keep_caret_visible(room);
            CAPTURE(room);
            CAPTURE(at);
            // NEITHER END OF THE WINDOW IS HALF A CHARACTER.
            CHECK(in.first_visible() % 2 == 0);
            CHECK(in.caret() % 2 == 0);
            CHECK_FALSE(is_continuation_byte(in.text()[in.first_visible()]));
            // ...AND THE CARET IS STILL IN IT.
            CHECK(in.first_visible() <= in.caret());
            CHECK(in.caret() - in.first_visible() <= static_cast<std::size_t>(room));
            // THE SLICE BEGINS ON A WHOLE CHARACTER.
            const std::string shown = in.visible(room);
            if (!shown.empty()) {
                CHECK_FALSE(is_continuation_byte(shown[0]));
            }
        }
    }

    // AND THE FORWARD SNAP COSTS AT MOST ONE CHARACTER OF TEXT, never the caret: a window
    // that wanted to begin at byte 11 begins at 12, which is the é the maker can actually
    // read rather than its second half.
    in.end();
    in.keep_caret_visible(13);
    CHECK(in.first_visible() == 12);
    CHECK(in.visible(13) == "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9");
}

// ---- 3. The window -----------------------------------------------------------------------

TEST_CASE("component: the capacity is an argument, so one box serves two different widths") {
    // The reason `columns` is not a member. A Terminal row and an Inspector row are different
    // widths in the same running application, and a component that remembered one of them
    // would be remembering the wrong one for the other.
    TextBox in;
    in.type("0123456789abcdefghij"); // 20
    CHECK(in.caret() == 20);

    in.keep_caret_visible(8);
    CHECK(in.first_visible() == 12);
    CHECK(in.visible(8) == "cdefghij");

    in.keep_caret_visible(20); // a wider row: the whole thing, with nothing hidden left
    CHECK(in.first_visible() == 0);
    CHECK(in.visible(20) == "0123456789abcdefghij");

    in.keep_caret_visible(4); // and a narrower one, from the same state
    CHECK(in.first_visible() == 16);
    CHECK(in.visible(4) == "ghij");

    // ASKING FOR THE SLICE DOES NOT MOVE THE WINDOW. `visible` is a read; the window moves
    // only in `keep_caret_visible` and in the operations. So a consumer painting at one width
    // and hit-testing at another gets two consistent answers about ONE window rather than two
    // windows.
    const std::size_t before = in.first_visible();
    CHECK(in.visible(100) == "ghij");
    CHECK(in.visible(1) == "g");
    CHECK(in.first_visible() == before);
}

TEST_CASE("component: the window moves as little as it must, and never recentres") {
    TextBox in;
    in.type(std::string("0123456789") + "ABCDEFGHIJ"); // 20, caret at the end
    in.keep_caret_visible(10);
    REQUIRE(in.first_visible() == 10);

    // TEN LEFTS WALK THE CARET ACROSS THE VISIBLE WIDTH AND MOVE NOTHING. That is minimal,
    // and it is what a maker sees as "the line stays still while I move through it".
    for (int i = 0; i < 10; ++i) {
        in.left();
        in.keep_caret_visible(10);
        CHECK(in.first_visible() == 10);
    }
    CHECK(in.caret() == 10);

    // The eleventh moves it exactly one character.
    in.left();
    in.keep_caret_visible(10);
    CHECK(in.caret() == 9);
    CHECK(in.first_visible() == 9);

    // Right is the mirror, and it does not jump back to where it started either.
    for (int i = 0; i < 11; ++i) {
        in.right();
        in.keep_caret_visible(10);
    }
    CHECK(in.caret() == 20);
    CHECK(in.first_visible() == 10);

    // HOME AND END ARE THE TWO THAT MOVE IT FURTHEST, and they still move it only as far as
    // the caret went.
    in.home();
    in.keep_caret_visible(10);
    CHECK(in.first_visible() == 0);
    CHECK(in.visible(10) == "0123456789");
    in.end();
    in.keep_caret_visible(10);
    CHECK(in.first_visible() == 10);
    CHECK(in.visible(10) == "ABCDEFGHIJ");
}

TEST_CASE("component: no blank room on the right while text is hidden on the left") {
    // Rule 1, and it is not cosmetic. Without it, backspacing a long text down to a short one
    // leaves the window where the long one put it: an apparently EMPTY row with the whole
    // value hidden away to the left, which reads exactly like a tool that lost it.
    TextBox in;
    in.type("0123456789abcdefghij");
    in.keep_caret_visible(10);
    REQUIRE(in.first_visible() == 10);

    for (int i = 0; i < 15; ++i) {
        in.backspace();
        in.keep_caret_visible(10);
        CAPTURE(in.text());
        CHECK(in.first_visible() <= (in.size() > 10 ? in.size() - 10 : 0));
    }
    CHECK(in.text() == "01234");
    CHECK(in.first_visible() == 0);
    CHECK(in.visible(10) == "01234");

    // AND IT BUYS THE PROPERTY A HIT TEST TURNS ON: after a reconcile there is blank room at
    // the right only when the text ends inside the row, so a press in that room is a press
    // after the last character there is.
    for (const std::int64_t room : {1, 2, 5, 10, 40}) {
        TextBox b;
        b.type("0123456789abcdefghij");
        for (const std::size_t at : {std::size_t{0}, std::size_t{7}, std::size_t{20}}) {
            b.place(at);
            b.keep_caret_visible(room);
            CAPTURE(room);
            CAPTURE(at);
            const bool blank_at_right =
                static_cast<std::int64_t>(b.visible(room).size()) < room;
            CHECK((!blank_at_right || b.first_visible() + b.visible(room).size() == b.size()));
        }
    }
}

TEST_CASE("component: the window never begins inside a character, at any capacity") {
    const std::string accented(12, 'x');
    TextBox in;
    std::string twelve;
    for (int i = 0; i < 12; ++i) {
        twelve += "\xC3\xA9"; // twelve e-acutes: 24 bytes, every odd index a continuation
    }
    in.set(twelve, twelve.size());
    (void)accented;

    for (std::int64_t room = 1; room <= 9; ++room) {
        for (std::size_t at = 0; at <= twelve.size(); at += 2) {
            in.place(at);
            in.keep_caret_visible(room);
            CAPTURE(room);
            CAPTURE(at);
            CHECK(in.first_visible() % 2 == 0); // never on a continuation byte
            CHECK(in.caret() % 2 == 0);
            const std::string shown = in.visible(room);
            if (!shown.empty()) {
                CHECK_FALSE(is_continuation_byte(shown[0]));
            }
        }
    }
}

// ---- 4. The pointer ----------------------------------------------------------------------

TEST_CASE("component: a column of the visible slice names a byte of the WHOLE text") {
    // The arithmetic a pointer needs, and the one subtraction a horizontal window adds to a
    // hit test. A consumer resolves a press to a column of its own prose and subtracts
    // whatever its row begins with; everything after that is here.
    TextBox in;
    in.type("0123456789abcdefghij"); // 20
    in.keep_caret_visible(8);
    REQUIRE(in.first_visible() == 12);

    CHECK(in.position_at_column(0) == 12);
    CHECK(in.position_at_column(3) == 15);
    CHECK(in.position_at_column(8) == 20);

    // THE BOUNDARIES. Left of the slice is the first byte the maker can SEE -- not byte zero,
    // which is a screenful away from where they pressed.
    CHECK(in.position_at_column(-1) == 12);
    CHECK(in.position_at_column(-400) == 12);
    CHECK(in.position_at_column(400) == 20); // past the end of the WHOLE text

    // ...AND IT IS THE INVERSE OF `caret_column` over every position the window shows, which
    // is the property a click-then-type depends on once the text is longer than the row.
    for (std::size_t at = 12; at <= 20; ++at) {
        in.place(at);
        CAPTURE(at);
        CHECK(in.position_at_column(static_cast<std::int64_t>(in.caret_column())) == at);
    }

    // TOTAL AT BOTH ENDS OF THE NUMBER LINE, because a column arrives from a pointer.
    constexpr std::int64_t kMin = (std::numeric_limits<std::int64_t>::min)();
    constexpr std::int64_t kMax = (std::numeric_limits<std::int64_t>::max)();
    CHECK(in.position_at_column(kMin) == in.first_visible());
    CHECK(in.position_at_column(kMax) == in.size());

    // An empty text has exactly one position, and every column names it.
    TextBox empty;
    for (std::int64_t col = -3; col < 20; ++col) {
        CHECK(empty.position_at_column(col) == 0);
    }

    // A press that lands on a continuation byte is snapped by `place`, not by this: the answer
    // here is a byte index and makes no claim to be a boundary.
    TextBox multi;
    multi.set("a\xC3\xA9z", 0);
    CHECK(multi.position_at_column(2) == 2); // the middle of the accented letter...
    multi.place(multi.position_at_column(2));
    CHECK(multi.caret() == 1); // ...and the caret lands on the character it hit
}

// ---- 5. What it is not --------------------------------------------------------------------

TEST_CASE("component: a TextBox is a value with no identity and no policy") {
    // A COMPONENT IS NOT AN ENTITY. It has no id, nothing registers it, nothing persists it,
    // and it dies with whatever holds it -- which is why two of them are simply two values
    // that know nothing about each other. This is the first production proof that internal
    // composition does not imply independent entityhood.
    static_assert(std::is_copy_constructible_v<TextBox>);
    static_assert(std::is_move_constructible_v<TextBox>);
    static_assert(std::is_default_constructible_v<TextBox>);
    static_assert(std::is_nothrow_destructible_v<TextBox>);

    struct TwoEditors { // a Terminal pane and a property row, in miniature
        TextBox terminal;
        TextBox property;
    };
    TwoEditors t;
    t.terminal.type("send @a Ping 1");
    t.property.type("60%");
    t.terminal.keep_caret_visible(8); // two different widths, at the same moment
    t.property.keep_caret_visible(17);
    CHECK(t.terminal.text() == "send @a Ping 1");
    CHECK(t.property.text() == "60%");
    CHECK(t.terminal.first_visible() == 6);
    CHECK(t.property.first_visible() == 0);

    // Copying takes the whole state and nothing is shared afterwards.
    TextBox copy = t.property;
    copy.type("!");
    CHECK(copy.text() == "60%!");
    CHECK(t.property.text() == "60%");

    // SMALL ENOUGH THAT A ROW MAY OWN ONE WITHOUT A MEASUREMENT BEING NEEDED: a std::string
    // and two indices, and nothing else. The bound is generous on purpose -- what would fail
    // it is a cache, a viewport object or a retained view sneaking in.
    CHECK(sizeof(TextBox) <= sizeof(std::string) + 4 * sizeof(std::size_t));
}
