// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Editor buffer suite -- the multiline document as VALUES.
//
// THIS FILE OWNS `editor-pane/editor.hpp`: the buffer's mechanics (motion, selection, the
// clipboard splice, history and the revision), the source byte law (`source_in` /
// `source_text` as an exact inverse over everything admitted, and every refusal), the tab
// geometry both ways, the displayed-column window, the paste flattening, and the agreement
// between the buffer's declared vocabulary and what `consume` eats. None of it touches a
// weave, a bus, a file or a screen: a case constructs a buffer, acts, and reads it back.
//
// ⭐ IT WAS `test_workshop_editor.cpp`'S FIRST TIER, and the file it tests was
// `workshop/editor.hpp`. The Editor is a weave now (`Zengine/editor-pane/`), the header moved
// with it, and this tier came out of the Workshop suite for the reason the component suite has
// its own executable: what the BUFFER does is this suite's claim, and that the Editor pane gets
// its answers from it is the panes suite's (`test_workshop_panes_editor.cpp`). The cases are the
// same cases, unchanged, so a mutation that reddened them there reddens them here.
//
// ⚠ NO WORKSHOP SUPPORT IS INCLUDED, and that is a measurement: this translation unit can name
// nothing of the host's session, screen or keymap, which is exactly what the header it tests
// can name.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "doctest.h"

#include "editor-pane/editor.hpp"

#include "component/text_box.hpp"
#include "input/vocabulary.hpp"

#include <string>
#include <vector>

namespace input = zengine::input;
namespace component = zengine::component;
using namespace zengine::workshop;

// ============================================================================
// Tier 1 -- THE BUFFER: multiline mechanics as values
// ============================================================================

TEST_CASE("EDIT-0: a fresh buffer is one empty line, and every state is ordinary") {
    EditorBuffer b;
    REQUIRE(b.line_count() == 1);
    CHECK(b.line(0).empty());
    CHECK(b.caret_row() == 0);
    CHECK(b.caret_byte() == 0);
    CHECK_FALSE(b.has_selection());
    // Every motion at the empty document's edges is a no-op, not a fault.
    b.left();
    b.up();
    b.right();
    b.down();
    b.backspace();
    b.erase_forward();
    CHECK(b.line_count() == 1);
    CHECK(b.caret_byte() == 0);
}

TEST_CASE("EDIT-0: newline splits, and the joins put it back from either side") {
    EditorBuffer b;
    b.type("onetwo");
    b.place(0, 3);
    b.newline();
    REQUIRE(b.line_count() == 2);
    CHECK(b.line(0) == "one");
    CHECK(b.line(1) == "two");
    CHECK(b.caret_row() == 1);
    CHECK(b.caret_byte() == 0);
    // Backspace at the start of a line joins with the previous one, caret at the seam.
    b.backspace();
    REQUIRE(b.line_count() == 1);
    CHECK(b.line(0) == "onetwo");
    CHECK(b.caret_row() == 0);
    CHECK(b.caret_byte() == 3);
    // Delete at the end of a line joins with the next one, caret unmoved.
    b.newline();
    b.place(0, 3);
    b.erase_forward();
    REQUIRE(b.line_count() == 1);
    CHECK(b.line(0) == "onetwo");
    CHECK(b.caret_byte() == 3);
}

TEST_CASE("EDIT-0: left and right cross line boundaries; home and end stay on the line") {
    EditorBuffer b;
    b.set_lines({"ab", "cd"});
    b.place(1, 0);
    b.left();
    CHECK(b.caret_row() == 0);
    CHECK(b.caret_byte() == 2); // the end of the previous line, not its last character
    b.right();
    CHECK(b.caret_row() == 1);
    CHECK(b.caret_byte() == 0);
    b.end();
    CHECK(b.caret_byte() == 2);
    b.home();
    CHECK(b.caret_byte() == 0);
    b.document_end();
    CHECK(b.caret_row() == 1);
    CHECK(b.caret_byte() == 2);
    b.document_home();
    CHECK(b.caret_row() == 0);
    CHECK(b.caret_byte() == 0);
}

TEST_CASE("EDIT-0: vertical movement keeps the preferred DISPLAYED column across short lines") {
    EditorBuffer b;
    b.set_lines({"alpha beta", "xy", "longer line"});
    b.place(0, 10); // visual column 10
    b.down();
    CHECK(b.caret_row() == 1);
    CHECK(b.caret_byte() == 2); // clamped to the short line's end...
    b.down();
    CHECK(b.caret_row() == 2);
    CHECK(b.caret_byte() == 10); // ...and BACK at the preferred column, not ratcheted left
    b.up();
    b.up();
    CHECK(b.caret_row() == 0);
    CHECK(b.caret_byte() == 10);
    // Horizontal movement forgets the run's column: the next vertical run starts fresh.
    b.left();
    b.down();
    CHECK(b.caret_byte() == 2); // the short line clamps against the NEW column (9)
    b.down();
    CHECK(b.caret_row() == 2);
    CHECK(b.caret_byte() == 9);
    // A preferred column through a TAB line re-aims by display, not by byte.
    b.set_lines({"abcdefgh", "\tx", "abcdefgh"});
    b.place(0, 5); // visual 5
    b.down();
    CHECK(b.caret_row() == 1);
    CHECK(b.caret_byte() == 2); // visual 5 is `x`'s right edge: after it (tab covers 0-3)
    b.down();
    CHECK(b.caret_row() == 2);
    CHECK(b.caret_byte() == 5);
}

TEST_CASE("EDIT-0: up from the first line goes to the start; down from the last goes to the end") {
    EditorBuffer b;
    b.set_lines({"abc", "def"});
    b.place(0, 2);
    b.up();
    CHECK(b.caret_row() == 0);
    CHECK(b.caret_byte() == 0);
    b.place(1, 1);
    b.down();
    CHECK(b.caret_row() == 1);
    CHECK(b.caret_byte() == 3);
}

TEST_CASE("EDIT-0: word movement crosses a line edge to the neighbouring line's edge") {
    EditorBuffer b;
    b.set_lines({"one two", "three four"});
    b.place(1, 0);
    b.word_left();
    CHECK(b.caret_row() == 0);
    CHECK(b.caret_byte() == 7); // the previous line's end...
    b.word_left();
    CHECK(b.caret_byte() == 4); // ...then the word inside it
    b.end();
    b.word_right();
    CHECK(b.caret_row() == 1);
    CHECK(b.caret_byte() == 0);
    b.word_right();
    CHECK(b.caret_byte() == 6);
}

TEST_CASE("EDIT-0: a selection spans lines, erases whole, and replacement typing replaces it") {
    EditorBuffer b;
    b.set_lines({"one", "two", "three"});
    b.place(0, 1);
    b.select_down();
    b.select_right();
    b.select_right();
    CHECK(b.has_selection());
    // The range is (0,1)..(1,3): row 0 from byte 1, the break, and the whole of `two`
    // (select_down lands at the preferred column, then two rightward extensions).
    CHECK(b.selected_text() == "ne\ntwo");
    CHECK(b.selection_begin() == EditorPos{0, 1});
    CHECK(b.selection_end() == EditorPos{1, 3});
    b.type("X");
    REQUIRE(b.line_count() == 2);
    CHECK(b.line(0) == "oX");
    CHECK(b.line(1) == "three");
    CHECK(b.caret_row() == 0);
    CHECK(b.caret_byte() == 2);
}

TEST_CASE("EDIT-0: select_all takes the whole document with the caret at its end") {
    EditorBuffer b;
    b.set_lines({"ab", "cd"});
    b.select_all();
    CHECK(b.selection_begin() == EditorPos{0, 0});
    CHECK(b.selection_end() == EditorPos{1, 2});
    CHECK(b.caret_row() == 1);
    CHECK(b.caret_byte() == 2);
    CHECK(b.selected_text() == "ab\ncd");
}

TEST_CASE("EDIT-0: copy, cut and paste carry newlines, and paste splices at the caret") {
    EditorBuffer b;
    component::Clipboard clip;
    b.set_lines({"one", "two", "three"});
    b.place(0, 1);
    b.select_down();
    b.copy(clip);
    CHECK(clip.text == "ne\nt");
    CHECK(clip.writes == 1);
    b.cut(clip);
    CHECK(clip.writes == 2);
    REQUIRE(b.line_count() == 2);
    CHECK(b.line(0) == "owo");
    // Paste back at a fresh position: one structural splice, caret after the last
    // pasted fragment, the line's tail carried behind it.
    b.place(1, 5);
    b.paste_lines({"ne", "t"});
    REQUIRE(b.line_count() == 3);
    CHECK(b.line(1) == "threene");
    CHECK(b.line(2) == "t");
    CHECK(b.caret_row() == 2);
    CHECK(b.caret_byte() == 1);
}

TEST_CASE("EDIT-0: undo groups typing, treats joins and pastes as one edit, and redo returns") {
    EditorBuffer b;
    b.type("abc");
    b.newline();
    b.type("def");
    // Three groups: the typed run, the newline, the second run.
    REQUIRE(b.undo());
    CHECK(b.line_count() == 2);
    CHECK(b.line(1).empty());
    REQUIRE(b.undo());
    CHECK(b.line_count() == 1);
    CHECK(b.line(0) == "abc");
    REQUIRE(b.undo());
    CHECK(b.line(0).empty());
    CHECK_FALSE(b.can_undo());
    REQUIRE(b.redo());
    REQUIRE(b.redo());
    REQUIRE(b.redo());
    CHECK(b.line(0) == "abc");
    CHECK(b.line(1) == "def");
    // A join is one undo step, and undoing it restores both lines AND the caret.
    b.place(1, 0);
    b.backspace();
    CHECK(b.line_count() == 1);
    REQUIRE(b.undo());
    REQUIRE(b.line_count() == 2);
    CHECK(b.line(0) == "abc");
    CHECK(b.line(1) == "def");
    // A multiline paste is one undo step.
    b.place(0, 0);
    b.paste_lines({"x", "y"});
    REQUIRE(b.line_count() == 3);
    REQUIRE(b.undo());
    CHECK(b.line_count() == 2);
    CHECK(b.line(0) == "abc");
    // An edit after an undo makes the undone future a road not taken.
    b.type("Z");
    CHECK_FALSE(b.can_redo());
}

TEST_CASE("EDIT-0: set_lines wipes the history -- undo cannot resurrect another document") {
    EditorBuffer b;
    b.type("secret");
    b.set_lines({"fresh"});
    CHECK_FALSE(b.can_undo());
    CHECK_FALSE(b.undo());
    CHECK(b.line(0) == "fresh");
}

TEST_CASE("EDIT-0: the revision moves with text, caret and selection, and with nothing else") {
    EditorBuffer b;
    b.set_lines({"abcd"});
    const std::uint64_t at_open = b.revision();
    b.right();
    CHECK(b.revision() > at_open); // caret movement is a position change
    const std::uint64_t moved = b.revision();
    b.type("x");
    CHECK(b.revision() > moved);
    const std::uint64_t typed = b.revision();
    component::Clipboard clip;
    b.copy(clip); // nothing selected: nothing moved
    CHECK(b.revision() == typed);
}

TEST_CASE("EDIT-0: word-grain erases work, and at a line edge they mean the join") {
    EditorBuffer b;
    b.set_lines({"one two", "three"});
    b.place(0, 7);
    b.erase_word_before();
    CHECK(b.line(0) == "one ");
    b.place(1, 0);
    b.erase_word_before(); // at the line's start: the join, exactly as backspace
    REQUIRE(b.line_count() == 1);
    CHECK(b.line(0) == "one three");
    b.place(0, 4);
    b.erase_word_after();
    CHECK(b.line(0) == "one ");
}

// ============================================================================
// Tier 2 — THE BYTE LAW AND THE TAB GEOMETRY: pure, and exact
// ============================================================================

TEST_CASE("EDIT-0: source_in and source_text are inverse over everything admitted") {
    const std::vector<std::string> admitted = {
        "one\ntwo\nthree\n", "no trailing newline", "", "\n", "a\n\nb\n",
        "\tindent\nplain\n"};
    for (const std::string& bytes : admitted) {
        const SourceIn in = source_in(bytes);
        REQUIRE(in.outcome.accepted);
        CHECK(in.convention == line_ending::kLF);
        CHECK(source_text(in.lines, in.convention) == bytes);
    }
    const std::string crlf = "one\r\ntwo\r\n";
    const SourceIn win = source_in(crlf);
    REQUIRE(win.outcome.accepted);
    CHECK(win.convention == line_ending::kCRLF);
    CHECK(source_text(win.lines, win.convention) == crlf);
    // A trailing newline IS a final empty line -- the representation's identity.
    CHECK(source_in("a\n").lines.size() == 2);
    CHECK(source_in("a").lines.size() == 1);
    CHECK(source_in("").lines.size() == 1);
}

TEST_CASE("EDIT-0: mixed endings, bare CR, control bytes and non-ASCII are refused whole") {
    const SourceIn mixed = source_in("one\r\ntwo\n");
    CHECK_FALSE(mixed.outcome.accepted);
    CHECK(mixed.outcome.refusal.find("mixes CRLF and LF") != std::string::npos);
    const SourceIn bare = source_in("one\rtwo");
    CHECK_FALSE(bare.outcome.accepted);
    CHECK(bare.outcome.refusal.find("bare carriage return") != std::string::npos);
    const SourceIn control = source_in("ok\nbad\x07line\n");
    CHECK_FALSE(control.outcome.accepted);
    CHECK(control.outcome.refusal.find("line 2") != std::string::npos);
    CHECK(control.outcome.refusal.find("0x07") != std::string::npos);
    const SourceIn utf8 = source_in("caf\xC3\xA9\n");
    CHECK_FALSE(utf8.outcome.accepted);
    CHECK(utf8.outcome.refusal.find("outside plain ASCII") != std::string::npos);
    CHECK(utf8.outcome.refusal.find("line 1") != std::string::npos);
}

TEST_CASE("EDIT-0: tab geometry maps bytes and displayed columns both ways, exactly") {
    const std::string line = "\tab\tc";
    // Spans: tab [0,4), a [4,5), b [5,6), tab [6,8), c [8,9).
    CHECK(visual_col_of(line, 0) == 0);
    CHECK(visual_col_of(line, 1) == 4);
    CHECK(visual_col_of(line, 2) == 5);
    CHECK(visual_col_of(line, 3) == 6);
    CHECK(visual_col_of(line, 4) == 8);
    CHECK(visual_len(line) == 9);
    // The inverse: inside a tab's span the caret lands BEFORE the tab; at a right
    // edge, after the byte whose edge it is.
    CHECK(byte_of_visual_col(line, 0) == 0);
    CHECK(byte_of_visual_col(line, 1) == 0);
    CHECK(byte_of_visual_col(line, 3) == 0);
    CHECK(byte_of_visual_col(line, 4) == 1);
    CHECK(byte_of_visual_col(line, 5) == 2);
    CHECK(byte_of_visual_col(line, 6) == 3);
    CHECK(byte_of_visual_col(line, 7) == 3);
    CHECK(byte_of_visual_col(line, 8) == 4);
    CHECK(byte_of_visual_col(line, 9) == 5);
    CHECK(byte_of_visual_col(line, 99) == 5);
    // Every boundary the forward map produces comes back through the inverse.
    for (std::size_t at = 0; at <= line.size(); ++at) {
        CHECK(byte_of_visual_col(line, visual_col_of(line, at)) == at);
    }
}

TEST_CASE("EDIT-0: expanded_slice shows tabs as spaces and windows by displayed columns") {
    const std::string line = "\tab";
    CHECK(expanded_slice(line, 0, 8) == "    ab");
    CHECK(expanded_slice(line, 2, 8) == "  ab"); // a window into the tab's own span
    CHECK(expanded_slice(line, 4, 8) == "ab");
    CHECK(expanded_slice(line, 5, 8) == "b");
    CHECK(expanded_slice(line, 0, 3) == "   ");
    CHECK(expanded_slice("abc", 1, 2) == "bc");
    CHECK(expanded_slice("abc", 9, 4).empty());
}

TEST_CASE("EDIT-0: pasteable_source flattens breaks and controls, and declines non-ASCII") {
    const PasteableSource ok = pasteable_source("a\r\nb\rc\nd\te");
    REQUIRE(ok.representable);
    REQUIRE(ok.lines.size() == 4);
    CHECK(ok.lines[0] == "a");
    CHECK(ok.lines[1] == "b");
    CHECK(ok.lines[2] == "c");
    CHECK(ok.lines[3] == "d\te"); // the tab survives: source holds tabs
    const PasteableSource bell = pasteable_source("a\x07" "b");
    REQUIRE(bell.representable);
    CHECK(bell.lines[0] == "a b"); // a control byte becomes one space, pasteable_line's law
    CHECK_FALSE(pasteable_source("caf\xC3\xA9").representable);
}

// ============================================================================
// Tier 2 -- THE VOCABULARY: what the buffer declares is what it consumes
// ============================================================================

TEST_CASE("EDIT-0: the editor's declared vocabulary and consume agree, both directions") {
    component::Clipboard clip;
    // Every declared row is consumed...
    for (std::size_t i = 0; i < kEditorVocabularyCount; ++i) {
        const component::EditingGesture& g = kEditorVocabulary[i];
        EditorBuffer b;
        b.set_lines({"one two", "three"});
        b.place(0, 3);
        INFO("vocabulary row ", i, ": ", g.label);
        CHECK(b.consume(g.scancode, g.modifiers, clip));
    }
    // ...and everything consumed is declared: sweep the named-scancode space under
    // every modifier combination the wire can say with the four semantic bits.
    for (std::int64_t sc = 1; sc < 128; ++sc) {
        for (std::int64_t mods = 0; mods < 16; ++mods) {
            EditorBuffer b;
            b.set_lines({"one two", "three"});
            b.place(0, 3);
            const bool eaten = b.consume(sc, mods, clip);
            bool declared = false;
            for (std::size_t i = 0; i < kEditorVocabularyCount; ++i) {
                if (kEditorVocabulary[i].scancode == sc &&
                    kEditorVocabulary[i].modifiers == mods) {
                    declared = true;
                }
            }
            INFO("scancode ", sc, " modifiers ", mods);
            CHECK(eaten == declared);
        }
    }
}
