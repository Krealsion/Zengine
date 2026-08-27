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

    // SMALL ENOUGH THAT A ROW MAY OWN ONE WITHOUT A MEASUREMENT BEING NEEDED: a std::string,
    // three indices, the two history vectors and the grouping kind, and nothing else (the
    // TEXT-0 growth: the anchor, undo/redo, one enum). The bound is generous on purpose --
    // what would fail it is a cache, a viewport object or a retained view sneaking in.
    CHECK(sizeof(TextBox) <=
          sizeof(std::string) + 2 * sizeof(std::vector<int>) + 6 * sizeof(std::size_t));
}

// ---- 6. The selection (TEXT-0) ------------------------------------------------------------
//
// AN ANCHOR AND THE CARET, and no third fact: `anchor() == caret()` IS "no selection", so an
// empty selection cannot exist as a distinct state and nothing below ever has to test a flag
// against a range. The cases walk the conventional grammar -- extend with Shift, collapse on
// plain movement, replace on type -- because "conventional" is exactly the claim TEXT-0 makes.

TEST_CASE("component: shift-movement extends a selection and plain movement collapses it") {
    TextBox box;
    box.set("hello world", 5); // caret between the words
    CHECK_FALSE(box.has_selection());
    CHECK(box.anchor() == 5);

    // Extending leaves the anchor and walks the caret -- in both directions through zero.
    box.select_left();
    box.select_left();
    CHECK(box.has_selection());
    CHECK(box.anchor() == 5);
    CHECK(box.caret() == 3);
    CHECK(box.selection_begin() == 3);
    CHECK(box.selection_end() == 5);
    CHECK(box.selected_text() == "lo");
    box.select_right(); // shrink from the same anchor
    CHECK(box.selected_text() == "o");
    box.select_right(); // ...to nothing: anchor == caret is the absence
    CHECK_FALSE(box.has_selection());

    // Home/End extend to the line's own boundaries.
    box.select_home();
    CHECK(box.selected_text() == "hello");
    CHECK(box.caret() == 0); // the caret is the ACTIVE end, and it is at the left
    box.select_end();
    box.select_end(); // idempotent at the boundary
    CHECK(box.selection_begin() == 5);
    CHECK(box.selection_end() == 11);

    // PLAIN LEFT/RIGHT WITH A SELECTION COLLAPSE TO ITS ENDS rather than stepping a
    // character: after sweeping a range the arrows mean "put me at this side of it".
    box.left();
    CHECK_FALSE(box.has_selection());
    CHECK(box.caret() == 5); // the selection's begin, not 10
    box.select_end();
    box.right();
    CHECK(box.caret() == 11); // the selection's end, not one past it
}

TEST_CASE("component: select_all takes everything with the caret at the end") {
    TextBox box;
    box.set("abc", 1);
    box.select_all();
    CHECK(box.selection_begin() == 0);
    CHECK(box.selection_end() == 3);
    CHECK(box.caret() == 3);
    // On empty text there is nothing to select, and nothing is: the absence is honest.
    TextBox empty;
    empty.select_all();
    CHECK_FALSE(empty.has_selection());
}

TEST_CASE("component: typing, backspace and delete act on the selection first") {
    TextBox box;
    box.set("hello world", 0);
    box.place(6);
    box.select_end(); // "world"
    box.type("there");
    CHECK(box.text() == "hello there");
    CHECK(box.caret() == 11);
    CHECK_FALSE(box.has_selection());

    box.place(0);
    box.select_word_right(); // "hello " -- through the space to the next word's start
    box.backspace();
    CHECK(box.text() == "there");
    CHECK(box.caret() == 0);

    box.select_end();
    box.erase_forward();
    CHECK(box.text().empty());
}

TEST_CASE("component: a selection is character-whole over multi-byte text") {
    TextBox box;
    box.set("a\xC3\xA9z", 0); // a, e-acute (2 bytes), z
    box.select_right();
    box.select_right(); // over 'a' and the whole accented letter
    CHECK(box.selection_end() == 3);
    CHECK(box.selected_text() == "a\xC3\xA9");
    // place() into the middle of a character snaps, and the anchor snaps with it: no
    // gesture leaves either end of a selection between two bytes of one character.
    box.place(2);
    CHECK(box.caret() == 1);
    CHECK(box.anchor() == 1);
    box.select_left();
    CHECK(box.selected_text() == "a");
}

TEST_CASE("component: word movement is space-delimited runs, walked from either side") {
    const std::string line = "send @a  Ping";
    CHECK(word_before(line, 13) == 9);  // from the end to Ping's start
    CHECK(word_before(line, 9) == 5);   // over the double space to @a's start
    CHECK(word_before(line, 5) == 0);
    CHECK(word_before(line, 0) == 0);
    CHECK(word_after(line, 0) == 5);    // over "send" and its space
    CHECK(word_after(line, 5) == 9);    // over "@a" and both spaces
    CHECK(word_after(line, 9) == 13);
    CHECK(word_after(line, 13) == 13);

    TextBox box;
    box.set(line, 13);
    box.word_left();
    CHECK(box.caret() == 9);
    CHECK_FALSE(box.has_selection());
    box.select_word_left();
    CHECK(box.selected_text() == "@a  ");
    box.word_right(); // collapses and walks
    CHECK(box.caret() == 9);
    // The word erases: one gesture, one word (plus the spaces that ride with it).
    box.end();
    box.erase_word_before();
    CHECK(box.text() == "send @a  ");
    box.home();
    box.erase_word_after();
    CHECK(box.text() == "@a  ");
}

TEST_CASE("component: the visible selection is the span both media may spend") {
    TextBox box;
    box.set("0123456789", 10);
    box.place(2);
    box.select_end();          // [2, 10)
    box.keep_caret_visible(4); // window shows "6789"
    CHECK(box.first_visible() == 6);
    const TextBox::VisibleSpan all = box.visible_selection(4);
    CHECK(all.present());
    CHECK(all.begin == 0); // clamped to the slice's start
    CHECK(all.end == 4);
    // A selection near the start shows whole once the window is back there.
    box.place(0);
    box.select_right(); // [0,1)
    box.keep_caret_visible(4);
    CHECK(box.first_visible() == 0);
    const TextBox::VisibleSpan head = box.visible_selection(4);
    CHECK(head.present());
    CHECK(head.begin == 0);
    CHECK(head.end == 1);
    // Zero columns can show nothing, whatever is selected.
    CHECK_FALSE(box.visible_selection(0).present());
}

TEST_CASE("component: drag_to_column extends from the pressed anchor and can leave the slice") {
    TextBox box;
    box.set("0123456789", 0);
    box.keep_caret_visible(5); // window "01234"
    box.place(box.position_at_column(2));
    CHECK(box.caret() == 2);
    box.drag_to_column(4);
    CHECK(box.selected_text() == "23");
    CHECK(box.anchor() == 2);
    // Dragging past the right edge names bytes past the window -- ordinary arithmetic --
    // and the next reconcile scrolls to them.
    box.drag_to_column(8);
    CHECK(box.caret() == 8);
    box.keep_caret_visible(5);
    CHECK(box.first_visible() == 3);
    // Dragging LEFT of the slice steps one character per motion: deterministic, minimal,
    // and enough to walk a selection out of the window a motion at a time.
    box.place(box.position_at_column(2)); // caret 5 in the scrolled window
    box.drag_to_column(-1);
    CHECK(box.caret() == 2); // one character before first_visible (3)
    box.drag_to_column(-1);
    CHECK(box.caret() == 1); // (the window followed via settle: first <= caret)
}

// ---- 7. The clipboard (TEXT-0) ------------------------------------------------------------

TEST_CASE("component: copy, cut and paste move text through the owner's clipboard") {
    TextBox box;
    Clipboard clip;
    box.set("hello world", 0);
    box.select_word_right();
    box.copy(clip);
    CHECK(clip.text == "hello ");
    CHECK(clip.writes == 1);
    CHECK(box.text() == "hello world"); // a copy erases nothing
    CHECK(box.has_selection());         // ...and keeps the selection

    box.end();
    box.select_word_left();
    box.cut(clip);
    CHECK(clip.text == "world");
    CHECK(clip.writes == 2);
    CHECK(box.text() == "hello ");
    CHECK_FALSE(box.has_selection());

    box.home();
    box.paste(clip);
    CHECK(box.text() == "worldhello ");
    CHECK(box.caret() == 5);
    // Paste replaces a selection when one exists.
    box.select_end();
    box.paste(clip);
    CHECK(box.text() == "worldworld");
}

TEST_CASE("component: copy with nothing selected leaves the clipboard alone") {
    TextBox box;
    Clipboard clip;
    clip.text = "precious";
    clip.writes = 3;
    box.set("abc", 1);
    box.copy(clip);
    box.cut(clip);
    CHECK(clip.text == "precious"); // a stray chord does not overwrite a maker's copy
    CHECK(clip.writes == 3);
    CHECK(box.text() == "abc");
    // And pasting an empty clipboard is nothing, not an empty edit in the history.
    Clipboard empty;
    box.paste(empty);
    CHECK(box.text() == "abc");
    CHECK_FALSE(box.undo()); // set() opened fresh history and nothing has been written to it
}

TEST_CASE("component: paste flattens foreign bytes into one line") {
    CHECK(pasteable_line("a\r\nb") == "a b");    // a CRLF pair is ONE space
    CHECK(pasteable_line("a\nb\rc") == "a b c"); // lone LF and CR are one each
    CHECK(pasteable_line("a\tb") == "a b");      // a tab would break the byte=column grid
    CHECK(pasteable_line("a\x7F") == "a ");
    CHECK(pasteable_line("caf\xC3\xA9") == "caf\xC3\xA9"); // multi-byte text passes whole

    TextBox box;
    Clipboard clip;
    clip.text = "two\r\nlines";
    box.paste(clip);
    CHECK(box.text() == "two lines");
    // UTF-8 through the round trip: what was copied is what is pasted, byte for byte.
    box.select_all();
    box.type("\xE2\x82\xAC z");
    box.home();
    box.select_word_right();
    box.copy(clip);
    CHECK(clip.text == "\xE2\x82\xAC ");
    box.end();
    box.paste(clip);
    CHECK(box.text() == "\xE2\x82\xAC z\xE2\x82\xAC ");
}

// ---- 8. The history (TEXT-0) --------------------------------------------------------------

TEST_CASE("component: undo restores text, caret and selection; redo replays it") {
    TextBox box;
    Clipboard clip;
    box.set("hello", 5);
    box.type(" world");
    CHECK(box.text() == "hello world");
    CHECK(box.undo());
    CHECK(box.text() == "hello");
    CHECK(box.caret() == 5);
    CHECK(box.redo());
    CHECK(box.text() == "hello world");
    CHECK(box.caret() == 11);

    // A cut is one entry, and undoing it restores the selection that was cut.
    box.place(0);
    box.select_word_right();
    box.cut(clip);
    CHECK(box.text() == "world");
    CHECK(box.undo());
    CHECK(box.text() == "hello world");
    CHECK(box.selected_text() == "hello ");

    // Undo past the bottom is a no-op that says so; so is redo past the top.
    while (box.undo()) {
    }
    CHECK(box.text() == "hello");
    CHECK_FALSE(box.undo());
    while (box.redo()) {
    }
    CHECK_FALSE(box.redo());
}

TEST_CASE("component: contiguous typing coalesces into one undo entry") {
    TextBox box;
    box.type("h");
    box.type("e");
    box.type("y");
    CHECK(box.text() == "hey");
    CHECK(box.undo());
    CHECK(box.text().empty()); // one gesture takes the whole burst

    // Movement breaks the group: type, move, type is two edits in two places.
    box.redo();
    box.left();
    box.type("X");
    CHECK(box.text() == "heXy");
    CHECK(box.undo());
    CHECK(box.text() == "hey");
    CHECK(box.undo());
    CHECK(box.text().empty());

    // A change of KIND breaks it too: typing then backspacing is two entries...
    box.type("abc");
    box.backspace();
    box.backspace();
    CHECK(box.text() == "a");
    CHECK(box.undo());
    CHECK(box.text() == "abc"); // the backspace burst, together
    CHECK(box.undo());
    CHECK(box.text().empty());

    // ...and an edit that replaced a selection stands alone, however it was made.
    box.type("hello");
    box.select_home();
    box.type("X");
    CHECK(box.text() == "X");
    CHECK(box.undo());
    CHECK(box.text() == "hello");
    CHECK(box.selected_text() == "hello"); // the replaced selection comes back selected
}

TEST_CASE("component: a new edit discards the redone future") {
    TextBox box;
    box.type("ab");
    box.left();
    box.type("X"); // "aXb"
    box.undo();    // "ab"
    CHECK(box.can_redo());
    box.type("Y"); // a new road: "aYb"
    CHECK_FALSE(box.can_redo());
    CHECK(box.text() == "aYb");
    CHECK(box.undo());
    CHECK(box.text() == "ab");
}

TEST_CASE("component: set and clear open a fresh draft with no inherited history") {
    TextBox box;
    box.type("first draft");
    CHECK(box.can_undo());
    // `set` is how every consumer opens a draft on a value: an undo surviving it would
    // resurrect a DIFFERENT draft's text under this one's label.
    box.set("second", 6);
    CHECK_FALSE(box.can_undo());
    CHECK_FALSE(box.can_redo());
    CHECK_FALSE(box.undo());
    CHECK(box.text() == "second");
    box.type("!");
    box.select_all(); // and the selection dies with the draft too
    box.clear();
    CHECK_FALSE(box.can_undo());
    CHECK_FALSE(box.has_selection());
    CHECK(box.text().empty());
}

TEST_CASE("component: the history is bounded and forgets its far past first") {
    TextBox box;
    // Alternate kinds so every edit is its own entry, far past any reasonable depth.
    for (int i = 0; i < 130; ++i) {
        box.type("x");
        box.backspace();
    }
    box.type("end");
    int undone = 0;
    while (box.undo()) {
        ++undone;
    }
    CHECK(undone == 100);            // the cap: deep enough for any draft, bounded on purpose
    CHECK_FALSE(box.text().empty()); // the far past is forgotten, the present never refused
}

// ---- 9. The vocabulary (TEXT-0) -----------------------------------------------------------
//
// `consume` is QR-2's bool at the component boundary: true = this vocabulary owned the
// gesture, stop routing; false = not mine, yours. The table below is the WHOLE vocabulary,
// and the declines are as load-bearing as the consumptions -- an owner's policy keys must
// come back false forever, without the component ever learning what they mean.

TEST_CASE("component: consume owns exactly the editing vocabulary and declines the rest") {
    TextBox box;
    Clipboard clip;
    box.set("hello world", 11);

    // The six base gestures, shift-transparent on the erasers and extending on movement.
    CHECK(box.consume(key::kLeft, mod::kNone, clip));
    CHECK(box.caret() == 10);
    CHECK(box.consume(key::kLeft, mod::kShift, clip));
    CHECK(box.selected_text() == "l");
    CHECK(box.consume(key::kHome, mod::kShift, clip));
    CHECK(box.selection_begin() == 0);
    CHECK(box.consume(key::kEnd, mod::kNone, clip));
    CHECK_FALSE(box.has_selection());
    CHECK(box.consume(key::kBackspace, mod::kShift, clip)); // shift mid-word still erases
    CHECK(box.text() == "hello worl");
    CHECK(box.consume(key::kHome, mod::kNone, clip));
    CHECK(box.consume(key::kDelete, mod::kNone, clip));
    CHECK(box.text() == "ello worl");

    // The chords.
    CHECK(box.consume(key::kA, mod::kCtrl, clip));
    CHECK(box.selected_text() == "ello worl");
    CHECK(box.consume(key::kC, mod::kCtrl, clip));
    CHECK(clip.text == "ello worl");
    CHECK(box.consume(key::kX, mod::kCtrl, clip));
    CHECK(box.text().empty());
    // Ctrl+V is a REQUEST since QR-11: consumed, counted, and applied by the OWNER once
    // it holds the value the paste means -- here, the clipboard it already has.
    CHECK(box.consume(key::kV, mod::kCtrl, clip));
    CHECK(clip.paste_requests == 1);
    CHECK(box.text().empty()); // the box did not paste; the owner does
    box.paste(clip);
    CHECK(box.text() == "ello worl");
    CHECK(box.consume(key::kZ, mod::kCtrl, clip));
    CHECK(box.text().empty()); // undo the paste
    CHECK(box.consume(key::kY, mod::kCtrl, clip));
    CHECK(box.text() == "ello worl"); // redo, the Windows spelling
    CHECK(box.consume(key::kZ, mod::kCtrl, clip));
    CHECK(box.consume(key::kZ, mod::kCtrl | mod::kShift, clip));
    CHECK(box.text() == "ello worl"); // redo, the other conventional spelling
    CHECK(box.consume(key::kLeft, mod::kCtrl, clip));
    CHECK(box.caret() == 5); // word left
    CHECK(box.consume(key::kRight, mod::kCtrl | mod::kShift, clip));
    CHECK(box.selected_text() == "worl"); // word extend
    CHECK(box.consume(key::kEnd, mod::kNone, clip));
    CHECK(box.consume(key::kBackspace, mod::kCtrl, clip)); // word backspace
    CHECK(box.text() == "ello ");

    // THE DECLINES. Policy keys and unknown chords are not this vocabulary's, and neither
    // is any chord carrying Alt or Super -- those belong to applications and window systems.
    CHECK_FALSE(box.consume(40 /*Return*/, mod::kNone, clip));
    CHECK_FALSE(box.consume(41 /*Escape*/, mod::kNone, clip));
    CHECK_FALSE(box.consume(43 /*Tab*/, mod::kNone, clip));
    CHECK_FALSE(box.consume(22 /*S*/, mod::kCtrl, clip)); // save is an application's
    CHECK_FALSE(box.consume(18 /*O*/, mod::kCtrl, clip)); // so is open
    CHECK_FALSE(box.consume(20 /*Q*/, mod::kNone, clip)); // a bare printable is text's
    CHECK_FALSE(box.consume(key::kLeft, mod::kAlt, clip));
    CHECK_FALSE(box.consume(key::kC, mod::kCtrl | mod::kSuper, clip));
    CHECK_FALSE(box.consume(key::kC, mod::kCtrl | mod::kShift, clip));
    CHECK(box.text() == "ello "); // and none of the declines touched anything
}

TEST_CASE("component: a consumed gesture that changes nothing is still consumed") {
    // QR-2's whole sentence: a consumed press does not have to change anything, it only has
    // to have reached the layer that owns what it means. A copy with nothing selected, an
    // undo with no history and a Home at 0 are the boundary's own no-ops -- and if any of
    // them answered false, the chord would fall through to an application binding the moment
    // it happened to be idle, which for ^c means "copy nothing" quitting the program.
    TextBox box;
    Clipboard clip;
    box.set("abc", 0);
    CHECK(box.consume(key::kC, mod::kCtrl, clip)); // nothing selected: consumed, no copy
    CHECK(clip.writes == 0);
    CHECK(box.consume(key::kZ, mod::kCtrl, clip)); // no history: consumed, nothing restored
    CHECK(box.consume(key::kY, mod::kCtrl, clip));
    CHECK(box.consume(key::kHome, mod::kNone, clip)); // already at 0: consumed
    CHECK(box.consume(key::kV, mod::kCtrl, clip));    // a paste request is ALWAYS consumed
    CHECK(clip.paste_requests == 1); // ...and always counted: the box cannot know whether
    box.paste(clip);                 // the clipboard holds anything -- that is the owner's
    CHECK(box.text() == "abc");      // read. An empty one applies as nothing.
}

TEST_CASE("QR-11: paste is a request the owner applies, and set/clear name the draft") {
    // The two halves of the intent seam, pinned at the component: Ctrl+V mutates NOTHING
    // in the box -- not text, not selection, not history -- because the value it means is
    // the clipboard's current one and only the owner can obtain it; and `draft_epoch`
    // moves at exactly the two doors a draft begins and ends at, so an owner whose
    // acquisition crosses a turn can tell "the draft that asked" from every other.
    TextBox box;
    Clipboard clip;
    clip.text = "stale";
    box.set("keep|this", 4);
    box.consume(key::kA, mod::kCtrl, clip); // a selection a paste would replace
    const std::string before = box.text();
    CHECK(box.consume(key::kV, mod::kCtrl, clip));
    CHECK(box.text() == before);           // nothing pasted...
    CHECK(box.selected_text() == before);  // ...nothing deselected...
    CHECK(clip.writes == 0);               // ...nothing copied...
    CHECK(clip.paste_requests == 1);       // ...one request recorded.

    // The epoch: 0 at birth, bumped by set and by clear, untouched by editing.
    TextBox fresh;
    CHECK(fresh.draft_epoch() == 0);
    fresh.set("draft one", 0);
    const std::uint64_t opened = fresh.draft_epoch();
    CHECK(opened == 1);
    fresh.type("x");
    fresh.consume(key::kLeft, mod::kNone, clip);
    CHECK(fresh.draft_epoch() == opened); // edits move text, never the draft's identity
    fresh.clear();
    CHECK(fresh.draft_epoch() == 2);
    fresh.set("draft two", 0);
    CHECK(fresh.draft_epoch() == 3);

    // A COPY of the box carries the epoch -- what keeps a draft carried across a rebuild
    // (Row::resume) the SAME draft to a paste already in flight.
    TextBox carried = fresh;
    CHECK(carried.draft_epoch() == fresh.draft_epoch());
}

TEST_CASE("component: ctrl+Home and ctrl+End are the line's own ends") {
    // On one line the document's ends and the line's ends are the same two places, so the
    // chorded spellings collapse to the plain ones rather than being declined -- a maker
    // who holds Ctrl out of multiline habit still lands where they meant.
    TextBox box;
    Clipboard clip;
    box.set("abc", 1);
    CHECK(box.consume(key::kEnd, mod::kCtrl, clip));
    CHECK(box.caret() == 3);
    CHECK(box.consume(key::kHome, mod::kCtrl | mod::kShift, clip));
    CHECK(box.selected_text() == "abc");
}
