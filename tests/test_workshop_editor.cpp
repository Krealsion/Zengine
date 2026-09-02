// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop editor suite — the built-in source editor: the multiline buffer and its
// mechanics, the source-byte law, the tab geometry, the Builder's edit-source door, save
// and dirty truth, the viewport, and the no-silent-loss floor.
//
// THREE TIERS, DELIBERATELY. The buffer and the byte/geometry laws are PURE and asserted
// as values; the routing and presentation are driven through the real weave on a real bus
// (`Live`), because the interesting half of an editor is not the splice arithmetic but
// what the application does around it -- where `^s` goes, what a press means, what a late
// clipboard answer may touch, and what can NEVER lose a dirty buffer. Every case that
// needs a file uses a temporary directory of its own; nothing here writes into the source
// tree.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// ============================================================================
// Support: raw bytes on disk, and a Workshop with a Builder that names sources
// ============================================================================

inline void raw_write(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}

inline std::string raw_read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/// A LIVE WORKSHOP WITH A BUILDER THAT NAMES REAL SOURCES: a mounted stand-in tool whose
/// catalog holds a single-source recipe (`hello`), a source-less one (`block`), and a
/// second single-source one (`world`); a host seam resolving each exactly as the real
/// host does (the authored string, verbatim); and a screen tall enough that the overlay
/// stack seats the Builder AND the Editor -- the minimum screen holds one slot, which is
/// itself a case below.
struct EditorRig {
    Live t;
    ToolSeat* tool = nullptr;
    TempDir dir;
    std::string src;
    std::string second;

    explicit EditorRig(const char* tag = "editor",
                       const std::string& bytes = "one\ntwo\nthree\n")
        : dir(tag) {
        tool = mount_tool(t, "hello");
        tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"block", "zen-block"});
        tool->catalog.recipes.push_back(zengine::builder::RecipeSummary{"world", "zen-world"});
        // THE IDENTITY IS THE DOOR'S NORMALIZED SPELLING, not the platform's native one
        // (EDIT-1). `open_source` resolves every entrant through `persist::resolved_against`
        // -- absolute, lexically normal, one separator convention -- so on Windows the stored
        // path uses forward slashes while `TempDir::file()` hands back backslashes. Spelling
        // the rig's own expectation through the SAME function is what keeps these cases
        // asserting the law rather than a platform's punctuation.
        src = persist::resolved_against(std::string(), dir.file("hello.cpp"));
        second = persist::resolved_against(std::string(), dir.file("world.cpp"));
        raw_write(src, bytes);
        raw_write(second, "alpha\nbeta\n");
        t.host.recipe_source = [this](const std::string& id) {
            HostContext::RecipeSource out;
            if (id == "hello") {
                out.known = true;
                out.kind = "single_source";
                out.source = src;
            } else if (id == "world") {
                out.known = true;
                out.kind = "single_source";
                out.source = second;
            } else if (id == "block") {
                out.known = true;
                out.kind = "cmake_target";
            }
            return out;
        };
        resize_screen(40);
        open_builder(t);
    }

    void resize_screen(std::int64_t h) {
        t.publish(loom::to_value(surface::SurfaceExtent{78, h, 0, 0}));
    }
    const Session& session() const { return t.w->session(); }
    const EditorState& ed() const { return session().editor; }
    const EditorBuffer& buf() const { return ed().buffer; }
    std::string notice() const { return session().notice; }

    /// `e` and the character its own keystroke produced, in the order the backends
    /// report them -- the trigger's `e` is owed to the gesture and must not land in
    /// the source, which a case below pins. COMMAND MODE'S GESTURE: a rig about to
    /// spend it while the editor holds the keys goes through `to_command()` first,
    /// exactly as a maker's hand would.
    void press_e() {
        t.key(input::scan::kE);
        t.text("e");
    }
    /// Hand the keyboard back to command mode by the maker's own gesture: a press on
    /// the empty workspace.
    void to_command() {
        t.press(0, 0);
        t.release(0, 0);
        REQUIRE(keyboard_context(session()) == KeyContext::kCommand);
    }
    void open_hello() {
        press_e();
        REQUIRE(ed().open_document());
    }
    /// Step the Builder's recipe choice, discharging each trigger's own character.
    void choose(int steps) {
        for (int i = 0; i < steps; ++i) {
            t.key(input::scan::kC);
            t.text("c");
        }
    }

    /// THE PANE'S BODY, INSIDE ITS OWN CHROME (WUX-5) -- the rectangle the painter
    /// resolves and the press inverse reads back, so a case aiming at row n hits row n.
    ui::Rect editor_cells() const {
        const Session& s = session();
        return pane_body_cells(
            bounds_of(s.panels, s.setup.active, panel::kEditor, screen_of(s)).rect);
    }
    void press_body(std::int64_t row, std::int64_t col) {
        const ui::Rect c = editor_cells();
        t.press_canvas(c.x + col, c.y + kEditorHeaderRows + row);
    }
    void motion_body(std::int64_t row, std::int64_t col) {
        const ui::Rect c = editor_cells();
        t.motion_canvas(c.x + col, c.y + kEditorHeaderRows + row);
    }
    void release_body(std::int64_t row, std::int64_t col) {
        const ui::Rect c = editor_cells();
        t.release_canvas(c.x + col, c.y + kEditorHeaderRows + row);
    }
    void wheel_body(double dy) {
        const ui::Rect c = editor_cells();
        t.wheel_canvas(dy, c.x + 2, c.y + kEditorHeaderRows + 1);
    }
    std::string shown() const { return panel_text(t.canvases.back(), editor_cells()); }
    surface::SurfaceTextRegion editor_region() const {
        const ui::Rect c = editor_cells();
        const std::vector<surface::SurfaceTextRegion> at =
            regions_at(t.canvases.back(), c.x, c.y);
        REQUIRE(at.size() == 1);
        return at.front();
    }
};

/// A SKIN THAT TAKES THE CLIPBOARD ANSWER AWAY WITH IT -- `defer_answer()` moves the
/// answer out of the queue and into a capability this seat holds, so a case can put real
/// turns between a maker's paste and the platform's answer. `AnswerNow` is the unrelated
/// delivery that lets the held answer be spent; the correlation the ask was delivered
/// with is restored by the bus, never chosen here.
struct AnswerNow {
    ZEN_SHAPE(AnswerNow, 1);
};

class SlowSkin : public loom::WeaveBase<SlowSkin, SeenState,
                                        loom::Accept<surface::ClipboardTextRequested, AnswerNow>,
                                        loom::Emit<surface::ClipboardText>> {
public:
    std::string text;
    bool held = false;
    loom::DeferredAnswer answer;

    void on(const surface::ClipboardTextRequested&, loom::Mail& mail) {
        answer = mail.defer_answer();
        held = answer.valid();
    }
    void on(const AnswerNow&, loom::Mail& mail) {
        if (!held) {
            return;
        }
        held = false;
        (void)loom::answer_deferred(answer, mail, surface::ClipboardText{true, text});
    }
};

inline SlowSkin* mount_slow_skin(Live& t) {
    auto seat = std::make_unique<SlowSkin>();
    SlowSkin* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_any(surface::ClipboardText::zen_name, surface::ClipboardText::zen_version);
    const loom::WeaveId id =
        t.bus.register_weave(std::move(seat), std::move(grant), surface::kSkinRole);
    raw->zen_set_self(id);
    return raw;
}

// ============================================================================
// Tier 1 — THE BUFFER: multiline mechanics as values
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
// Tier 3 — THE KEYMAP: two save identities, one physical chord
// ============================================================================

TEST_CASE("EDIT-0: the editor context takes text, and its class algebra is exact") {
    CHECK(context_takes_text(KeyContext::kEditor));
    CHECK(active_in(KeyContext::kNoEditor, KeyContext::kCommand));
    CHECK(active_in(KeyContext::kNoEditor, KeyContext::kTerminal));
    CHECK_FALSE(active_in(KeyContext::kNoEditor, KeyContext::kEditor));
    CHECK_FALSE(active_in(KeyContext::kNoText, KeyContext::kEditor));
    // The two save rows can never both be active: their contexts do not intersect.
    CHECK_FALSE(contexts_intersect(KeyContext::kNoEditor, KeyContext::kEditor));
    CHECK(contexts_intersect(KeyContext::kNoEditor, KeyContext::kNoText));
    CHECK(contexts_intersect(KeyContext::kNoEditor, KeyContext::kGlobal));
}

TEST_CASE("EDIT-0: one physical ^s resolves to the document's save or the editor's, by context") {
    const Keymap k;
    CHECK(k.action_for(KeyContext::kCommand, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    CHECK(k.action_for(KeyContext::kTerminal, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    CHECK(k.action_for(KeyContext::kEditor, input::scan::kS, input::mod::kCtrl) ==
          Act::kEditorSave);
    // The head answers the document's row everywhere BUT the editor; the editor's own
    // row travels the chain.
    CHECK(k.above_mode_action(KeyContext::kCommand, input::scan::kS, input::mod::kCtrl) ==
          Act::kSaveDocument);
    CHECK(k.above_mode_action(KeyContext::kEditor, input::scan::kS, input::mod::kCtrl) ==
          Act::kNone);
    // `^o` stays global -- the editor context included.
    CHECK(k.above_mode_action(KeyContext::kEditor, input::scan::kO, input::mod::kCtrl) ==
          Act::kOpenDocument);
}

TEST_CASE("EDIT-0: a kNoEditor override meets the global guards at admission") {
    // A bare printable cannot be above-the-modes once anything can take text...
    const keymap_persist::LoadedKeymap bare = keymap_persist::from_text(
        keymap_file_text("default", {{"document.save", "g"}}));
    CHECK_FALSE(bare.outcome.accepted);
    CHECK(bare.outcome.refusal.find("bare printable") != std::string::npos);
    // ...and the editing vocabulary's own chords would be consumed first in every
    // text context.
    const keymap_persist::LoadedKeymap owned = keymap_persist::from_text(
        keymap_file_text("default", {{"document.save", "ctrl+c"}}));
    CHECK_FALSE(owned.outcome.accepted);
    CHECK(owned.outcome.refusal.find("editing vocabulary") != std::string::npos);
    // A chord outside both guards is admitted and moves the binding.
    const keymap_persist::LoadedKeymap moved = keymap_persist::from_text(
        keymap_file_text("default", {{"document.save", "ctrl+g"}}));
    REQUIRE(moved.outcome.accepted);
}

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

// ============================================================================
// Tier 4 — THE DOOR: Builder names the source, and the refusals speak
// ============================================================================

TEST_CASE("EDIT-0: `e` opens the chosen recipe's source, focuses the editor, and says so") {
    EditorRig r("open");
    r.open_hello();
    CHECK(r.ed().path == r.src);
    CHECK(r.notice() == "editing " + r.src);
    REQUIRE(r.buf().line_count() == 4); // one, two, three, and the final empty line
    CHECK(r.buf().line(0) == "one");
    CHECK(r.session().panels.has(panel::kEditor));
    CHECK(r.session().panels.keyboard == panel::kEditor);
    CHECK(keyboard_context(r.session()) == KeyContext::kEditor);
    // The trigger's own character was owed to the gesture: no `e` landed in the source.
    CHECK(r.buf().line(0) == "one");
    CHECK_FALSE(r.ed().dirty());
    // The pane paints the document with its header naming the file.
    const std::string body = r.shown();
    CHECK(body.find("Editor saved") != std::string::npos);
    CHECK(body.find("L1:C1/4") != std::string::npos);
    // The path follows the separator, elidable at its tail -- and spelled by the
    // platform, so the assertion stops at the separator itself.
    CHECK(body.find(" -- ") != std::string::npos);
    CHECK(body.find("one") != std::string::npos);
    CHECK(body.find("three") != std::string::npos);
}

TEST_CASE("EDIT-0: `e` with no Builder panel is an unbound key, exactly as `b` is") {
    EditorRig r("nodoor");
    pick(r.t, panel::kBuilder); // remove the panel the rig opened
    REQUIRE_FALSE(r.session().panels.has(panel::kBuilder));
    const std::string before = r.notice();
    r.press_e();
    CHECK(r.notice() == before);
    CHECK_FALSE(r.ed().open_document());
}

TEST_CASE("EDIT-0: a Builder that has not answered yet refuses the door in its own words") {
    Live t;
    TempDir dir("silent");
    (void)dir;
    t.publish(loom::to_value(surface::SurfaceExtent{78, 40, 0, 0}));
    pick(t, panel::kBuilder); // no tool mounted: StatusRequested reaches nobody
    REQUIRE(t.w->session().panels.has(panel::kBuilder));
    t.key(input::scan::kE);
    t.text("e");
    CHECK(t.w->session().notice.find("has not said what it builds yet") != std::string::npos);
    CHECK_FALSE(t.w->session().editor.open_document());
}

TEST_CASE("EDIT-0: a source-less recipe kind refuses in the recipe owner's vocabulary") {
    EditorRig r("cmake");
    r.choose(1); // hello -> block
    r.press_e();
    CHECK(r.notice().find("`block` is a cmake_target recipe") != std::string::npos);
    CHECK(r.notice().find("names no single source") != std::string::npos);
    CHECK_FALSE(r.ed().open_document());
}

TEST_CASE("EDIT-0: a host with no recipe-source seam refuses rather than guessing") {
    EditorRig r("unwired");
    r.t.host.recipe_source = {};
    r.press_e();
    CHECK(r.notice().find("resolves no recipe sources") != std::string::npos);
    CHECK_FALSE(r.ed().open_document());
}

TEST_CASE("EDIT-0: a catalog/recipes disagreement is named, and nothing opens") {
    EditorRig r("disagree");
    r.t.host.recipe_source = [](const std::string&) {
        return HostContext::RecipeSource{};
    };
    r.press_e();
    CHECK(r.notice().find("disagree about `hello`") != std::string::npos);
    CHECK_FALSE(r.ed().open_document());
}

TEST_CASE("EDIT-0: a file the byte law refuses costs the notice and nothing else") {
    EditorRig r("badbytes");
    raw_write(r.src, "one\r\ntwo\n"); // mixed endings
    r.press_e();
    CHECK(r.notice().find("mixes CRLF and LF") != std::string::npos);
    CHECK_FALSE(r.ed().open_document());
    CHECK_FALSE(r.session().panels.has(panel::kEditor)); // no pane was authored either
    CHECK(raw_read(r.src) == "one\r\ntwo\n");             // and the file is untouched
}

TEST_CASE("EDIT-0: at the minimum screen there is no room, and the refusal names the remedy") {
    EditorRig r("noroom");
    r.resize_screen(22); // back to the minimum: the stack holds one slot, Builder has it
    r.press_e();
    CHECK(r.notice().find("no room for the Editor") != std::string::npos);
    CHECK_FALSE(r.ed().open_document());
    CHECK_FALSE(has_pane(r.session().setup.active, pane_ref_of(panel::kEditor)));
    // Growing the window is the named remedy, and then the same gesture works.
    r.resize_screen(40);
    r.press_e();
    CHECK(r.ed().open_document());
}

TEST_CASE("EDIT-0: re-requesting the open source reveals it and destroys nothing") {
    EditorRig r("rerequest");
    r.open_hello();
    r.t.text("zz");
    r.t.key(input::scan::kDown);
    const std::uint64_t rev = r.buf().revision();
    REQUIRE(r.ed().dirty());
    // Remove the pane (presentation only), then ask for the same source again. The
    // picker and `e` are command mode's, so the keys go back there first.
    r.to_command();
    pick(r.t, panel::kEditor);
    REQUIRE_FALSE(r.session().panels.has(panel::kEditor));
    r.press_e();
    CHECK(r.session().panels.has(panel::kEditor));
    CHECK(r.ed().dirty());
    CHECK(r.buf().revision() == rev); // buffer, caret, selection, history: untouched
    CHECK(r.buf().line(0) == "zzone");
    CHECK(r.notice().find("UNSAVED edits stand") != std::string::npos);
}

TEST_CASE("EDIT-0: a dirty buffer refuses a different source, and save or discard opens the way") {
    EditorRig r("replace");
    r.open_hello();
    r.t.text("x");
    REQUIRE(r.ed().dirty());
    r.to_command();
    r.choose(2); // hello -> block -> world
    r.press_e();
    CHECK(r.notice().find("has unsaved changes") != std::string::npos);
    CHECK(r.ed().path == r.src); // still the first document
    CHECK(r.buf().line(0) == "xone");
    // Discard deliberately, then the replacement proceeds.
    r.t.key(input::scan::kD, input::mod::kCtrl);
    CHECK_FALSE(r.ed().dirty());
    r.press_e();
    CHECK(r.ed().path == r.second);
    CHECK(r.buf().line(0) == "alpha");
}

// ============================================================================
// Tier 5 — SAVE AND DIRTY TRUTH: ^s follows the keyboard
// ============================================================================

TEST_CASE("EDIT-0: ^s in the editor saves the SOURCE; elsewhere it keeps the document's meaning") {
    EditorRig r("saveroute");
    r.open_hello();
    r.t.text("// note");
    r.t.key(input::scan::kReturn);
    REQUIRE(r.ed().dirty());
    r.t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(r.notice() == "saved " + r.src);
    CHECK_FALSE(r.ed().dirty());
    CHECK(raw_read(r.src) == "// note\none\ntwo\nthree\n");
    // Point the keyboard elsewhere: the same physical chord is the document's again --
    // and this fixture chose no document file, so the document's own sentence answers.
    r.t.press(0, 0);
    r.t.release(0, 0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kCommand);
    r.t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(r.notice().find("no document file") != std::string::npos);
    CHECK(raw_read(r.src) == "// note\none\ntwo\nthree\n"); // the source was not re-written
}

TEST_CASE("EDIT-0: dirty derives by comparison -- editing back to the saved text is clean") {
    EditorRig r("derive");
    r.open_hello();
    CHECK_FALSE(r.ed().dirty());
    r.t.text("x");
    CHECK(r.ed().dirty());
    r.t.key(input::scan::kBackspace);
    CHECK_FALSE(r.ed().dirty()); // compared, never flagged
    CHECK(r.shown().find("Editor saved") != std::string::npos);
    r.t.text("y");
    CHECK(r.shown().find("Editor UNSAVED") != std::string::npos);
}

TEST_CASE("EDIT-0: a failed save keeps the buffer, keeps dirty, and speaks the writer's refusal") {
    EditorRig r("savefail");
    r.open_hello();
    r.t.text("x");
    REQUIRE(r.ed().dirty());
    // Take the directory away: the atomic writer cannot even open its sibling file.
    std::error_code drop;
    std::filesystem::remove_all(std::filesystem::path(r.src).parent_path(), drop);
    r.t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(r.notice().find("cannot write") != std::string::npos);
    CHECK(r.ed().dirty());
    CHECK(r.buf().line(0) == "xone"); // the buffer is exactly as it was
}

TEST_CASE("EDIT-0: CRLF and the final-newline state round-trip through open, edit, save") {
    EditorRig r("crlf");
    raw_write(r.src, "one\r\ntwo");
    r.open_hello();
    CHECK(r.ed().convention == line_ending::kCRLF);
    REQUIRE(r.buf().line_count() == 2); // no trailing newline: no final empty line
    // An inserted newline uses THIS document's convention at save.
    r.t.key(input::scan::kEnd, input::mod::kCtrl);
    r.t.key(input::scan::kReturn);
    r.t.text("three");
    r.t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(raw_read(r.src) == "one\r\ntwo\r\nthree");
    // And the LF twin, with its final newline preserved.
    EditorRig lf("lfround", "a\nb\n");
    lf.open_hello();
    CHECK(lf.ed().convention == line_ending::kLF);
    lf.t.text("x");
    lf.t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(raw_read(lf.src) == "xa\nb\n");
}

TEST_CASE("EDIT-0: tabs round-trip untouched, and Tab inserts a tab byte") {
    EditorRig r("tabs", "\tone\n\ttwo\n");
    r.open_hello();
    r.t.key(input::scan::kEnd);
    r.t.key(input::scan::kReturn);
    r.t.key(input::scan::kTab);
    r.t.text("mid");
    r.t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(raw_read(r.src) == "\tone\n\tmid\n\ttwo\n");
}

TEST_CASE("EDIT-0: saving an unchanged file is allowed and says so plainly") {
    EditorRig r("resave");
    r.open_hello();
    r.t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(r.notice() == "saved " + r.src);
    CHECK(raw_read(r.src) == "one\ntwo\nthree\n");
}

// ============================================================================
// Tier 6 — NO SILENT LOSS: hiding, replacing, quitting
// ============================================================================

TEST_CASE("EDIT-0: removing and reopening the pane cannot lose a byte of dirty source") {
    EditorRig r("hide");
    r.open_hello();
    r.t.text("precious ");
    REQUIRE(r.ed().dirty());
    r.to_command();
    pick(r.t, panel::kEditor); // remove the presentation
    REQUIRE_FALSE(r.session().panels.has(panel::kEditor));
    CHECK(r.ed().dirty()); // the document is session state, not pane state
    CHECK(r.buf().line(0) == "precious one");
    pick(r.t, panel::kEditor); // and back
    REQUIRE(r.session().panels.has(panel::kEditor));
    // The cell projection inserts the caret glyph inside the word the caret sits in
    // (`precious _one`), so the row is asserted around it rather than through it.
    CHECK(r.shown().find("precious ") != std::string::npos);
    CHECK(r.shown().find("one") != std::string::npos);
    CHECK(r.shown().find("UNSAVED") != std::string::npos);
    CHECK(r.editor_region().caret_row == kEditorHeaderRows); // the caret came back too
}

TEST_CASE("EDIT-0: an orderly close refuses while source is unsaved, and proceeds once it is not") {
    EditorRig r("quit");
    r.open_hello();
    r.t.text("x");
    REQUIRE(r.ed().dirty());
    r.t.publish(loom::to_value(surface::SurfaceCloseRequested{}));
    CHECK_FALSE(r.t.host.quit);
    CHECK(r.notice().find("unsaved changes") != std::string::npos);
    CHECK(r.notice().find("discards them") != std::string::npos);
    // The named discard door works from command mode too, so the refusal's remedy is
    // reachable exactly where the maker is standing.
    r.t.press(0, 0);
    r.t.release(0, 0);
    r.t.key(input::scan::kD, input::mod::kCtrl);
    CHECK_FALSE(r.ed().dirty());
    r.t.publish(loom::to_value(surface::SurfaceCloseRequested{}));
    CHECK(r.t.host.quit);
}

TEST_CASE("EDIT-0: a saved buffer quits without ceremony, and ^s is the other way out") {
    EditorRig r("quitsave");
    r.open_hello();
    r.t.text("x");
    r.t.publish(loom::to_value(surface::SurfaceCloseRequested{}));
    CHECK_FALSE(r.t.host.quit);
    r.t.key(input::scan::kS, input::mod::kCtrl);
    r.t.publish(loom::to_value(surface::SurfaceCloseRequested{}));
    CHECK(r.t.host.quit);
}

TEST_CASE("EDIT-0: discard is deliberate, scoped, undoable, and honest about nothing to do") {
    EditorRig r("discard");
    r.open_hello();
    r.t.key(input::scan::kD, input::mod::kCtrl);
    CHECK(r.notice().find("nothing to discard") != std::string::npos);
    r.t.text("x");
    r.t.key(input::scan::kD, input::mod::kCtrl);
    CHECK(r.notice().find("discarded unsaved edits") != std::string::npos);
    CHECK(r.notice().find("undo takes them back") != std::string::npos);
    CHECK_FALSE(r.ed().dirty());
    CHECK(r.buf().line(0) == "one");
    // THE DISCARD IS ITSELF AN UNDOABLE EDIT -- what makes its soft, POSIX-reachable
    // chord safe to bind: one slip of ctrl+d is one undo away from the edits standing.
    r.t.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(r.buf().line(0) == "xone");
    CHECK(r.ed().dirty());
    // With no document at all, the same gesture says so rather than doing nothing.
    EditorRig none("discardnone");
    none.t.key(input::scan::kD, input::mod::kCtrl);
    CHECK(none.notice().find("no source is open") != std::string::npos);
}

// ============================================================================
// Tier 7 — THE KEYBOARD PLACE: routing, chords, and the screen's statements
// ============================================================================

TEST_CASE("EDIT-0: printable text edits the source, and command letters stop being commands") {
    EditorRig r("typing");
    r.open_hello();
    const std::size_t objects = r.t.w->document().elements.size();
    r.t.key(input::scan::kN); // `n` is object.new in command mode -- not here
    r.t.text("n");
    CHECK(r.t.w->document().elements.size() == objects);
    CHECK(r.buf().line(0) == "none");
    // Pressing into the workspace hands command mode back.
    r.t.press(0, 0);
    r.t.release(0, 0);
    r.t.key(input::scan::kN);
    r.t.text("n");
    CHECK(r.t.w->document().elements.size() == objects + 1);
    CHECK(r.buf().line(0) == "none"); // and the editor's text was not touched by it
}

TEST_CASE("EDIT-0: ^c in the editor copies -- it does not quit -- and quit stays a press away") {
    EditorRig r("ctrlc");
    r.open_hello();
    r.t.key(input::scan::kRight, input::mod::kShift);
    r.t.key(input::scan::kC, input::mod::kCtrl);
    CHECK_FALSE(r.t.host.quit);
    CHECK(r.session().clipboard.text == "o");
    // A copy with nothing selected is still consumed: "copy nothing" must never quit.
    r.t.key(input::scan::kRight);
    r.t.key(input::scan::kC, input::mod::kCtrl);
    CHECK_FALSE(r.t.host.quit);
    // ^a selects all here (the component's own chord), rather than opening attention.
    r.t.key(input::scan::kA, input::mod::kCtrl);
    CHECK_FALSE(r.session().attention.open);
    CHECK(r.buf().has_selection());
    CHECK(r.buf().selected_text() == "one\ntwo\nthree\n");
}

TEST_CASE("EDIT-0: ^o keeps its global object-document meaning while the editor has the keys") {
    EditorRig r("ctrlo");
    r.open_hello();
    r.t.key(input::scan::kO, input::mod::kCtrl);
    CHECK(r.notice().find("no document file") != std::string::npos);
    CHECK(r.ed().open_document()); // the source document is untouched by it
}

TEST_CASE("EDIT-0: Escape means nothing in the editor -- no mode closes, no text moves") {
    EditorRig r("escape");
    r.open_hello();
    const std::uint64_t rev = r.buf().revision();
    r.t.key(input::scan::kEscape);
    CHECK(keyboard_context(r.session()) == KeyContext::kEditor);
    CHECK(r.buf().revision() == rev);
}

TEST_CASE("EDIT-0: the band and the header both say where typing goes") {
    EditorRig r("band");
    r.open_hello();
    const Screen sc = screen_of(r.session());
    CHECK(label_at(r.t.canvases.back(), 0, sc.help_y)
              .rfind("typing goes to the source editor", 0) == 0);
    CHECK(r.shown().find("> Editor") != std::string::npos);
    // Hand the keys back: both statements retract on their own.
    r.t.press(0, 0);
    r.t.release(0, 0);
    CHECK(label_at(r.t.canvases.back(), 0, sc.help_y)
              .rfind("typing goes to", 0) != 0);
    CHECK(r.shown().find("> Editor") == std::string::npos);
    CHECK(r.shown().find("  Editor") != std::string::npos);
}

TEST_CASE("EDIT-0: the hotkey view answers for the editor with its own unremappable keys") {
    EditorRig r("hotkeys");
    r.open_hello();
    r.t.key(input::scan::kK, input::mod::kCtrl);
    // THE VIEW IS ANCHORED AT THE SELECTED PANE SINCE WUX-5, so it is read at its own
    // derived bounds rather than at the overlay column it used to open in.
    const std::string view = panel_text(
        r.t.canvases.back(),
        pane_body_cells(hotkeys_bounds(r.session(), screen_of(r.session()))));
    CHECK(view.find("the source editor") != std::string::npos);
    CHECK(view.find("save source") != std::string::npos);
    r.t.key(input::scan::kEscape); // the view closes; the editor still has the keys
    CHECK(keyboard_context(r.session()) == KeyContext::kEditor);
}

TEST_CASE("EDIT-0: an empty editor pane takes no keys and says how to fill itself") {
    EditorRig r("empty");
    open_pane(r.t, pane_ref_of(panel::kEditor)); // opened by hand, no document
    CHECK(r.shown().find("no source open") != std::string::npos);
    const ui::Rect c = r.editor_cells();
    r.t.press_canvas(c.x + 2, c.y + 1); // a press into the empty body
    CHECK(r.session().panels.keyboard == panel::kEditor);
    CHECK(keyboard_context(r.session()) == KeyContext::kCommand); // no document: no place
    const std::size_t objects = r.t.w->document().elements.size();
    r.t.key(input::scan::kN);
    r.t.text("n");
    CHECK(r.t.w->document().elements.size() == objects + 1); // bare keys are commands still
}

// ============================================================================
// Tier 8 — THE POINTER: what you see is what you press
// ============================================================================

TEST_CASE("EDIT-0: a press places the caret through the same tab geometry the paint used") {
    EditorRig r("presstab", "\tX\nplain\n");
    r.open_hello();
    const surface::SurfaceTextRegion painted = r.editor_region();
    REQUIRE(painted.rows.size() >= 2);
    CHECK(painted.rows[1].text == "    X"); // the tab expanded at the fixed stop
    r.press_body(0, 2); // inside the tab's span: the caret lands before the tab
    CHECK(r.buf().caret_row() == 0);
    CHECK(r.buf().caret_byte() == 0);
    r.press_body(0, 4); // the tab's right edge: after it, before the X
    CHECK(r.buf().caret_byte() == 1);
    r.press_body(0, 5);
    CHECK(r.buf().caret_byte() == 2);
    // A press far past a line's end names the line's end, not an imaginary column.
    r.press_body(1, 40);
    CHECK(r.buf().caret_row() == 1);
    CHECK(r.buf().caret_byte() == 5);
    // ...and the caret the paint publishes is at the pressed place, header counted.
    const surface::SurfaceTextRegion after = r.editor_region();
    CHECK(after.caret_row == kEditorHeaderRows + 1);
    CHECK(after.caret_col == 5);
}

TEST_CASE("EDIT-0: a drag sweeps a multiline selection, and the selection survives release") {
    EditorRig r("drag");
    r.open_hello();
    r.press_body(0, 1);
    r.motion_body(1, 2);
    CHECK(r.buf().has_selection());
    CHECK(r.buf().selection_begin() == EditorPos{0, 1});
    CHECK(r.buf().selection_end() == EditorPos{1, 2});
    r.motion_body(2, 1); // the sweep follows the hand, row by row
    CHECK(r.buf().selection_end() == EditorPos{2, 1});
    r.release_body(2, 1);
    CHECK(r.buf().has_selection()); // release ends the gesture, not the selection
    const surface::SurfaceTextRegion painted = r.editor_region();
    CHECK(painted.sel_begin_row == kEditorHeaderRows + 0);
    CHECK(painted.sel_begin_col == 1);
    CHECK(painted.sel_end_row == kEditorHeaderRows + 2);
    CHECK(painted.sel_end_col == 1);
}

TEST_CASE("EDIT-0: a press on the editor's header focuses without moving the caret") {
    EditorRig r("header");
    r.open_hello();
    r.press_body(1, 2);
    REQUIRE(r.buf().caret_row() == 1);
    r.t.press(0, 0); // keys away...
    r.t.release(0, 0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kCommand);
    const ui::Rect c = r.editor_cells();
    r.t.press_canvas(c.x + 3, c.y); // ...and back, via the header row
    CHECK(keyboard_context(r.session()) == KeyContext::kEditor);
    CHECK(r.buf().caret_row() == 1); // the caret did not move
    CHECK(r.buf().caret_byte() == 2);
}

// ============================================================================
// Tier 9 — THE VIEWPORT: the caret stays visible, the wheel looks elsewhere
// ============================================================================

inline std::string thirty_lines() {
    std::string bytes;
    for (int i = 1; i <= 30; ++i) {
        bytes += "line " + std::to_string(i) + "\n";
    }
    return bytes;
}

TEST_CASE("EDIT-0: keyboard navigation scrolls the window and the caret never leaves it") {
    EditorRig r("follow", thirty_lines());
    r.open_hello();
    const ExternalBodyPlace body = editor_body(r.session(), screen_of(r.session()));
    REQUIRE(body.present);
    for (int i = 0; i < 12; ++i) {
        r.t.key(input::scan::kDown);
    }
    CHECK(r.buf().caret_row() == 12);
    const std::size_t rows = static_cast<std::size_t>(body.rows);
    CHECK(r.ed().first_row == 12 + 1 - rows); // the window followed, minimally
    const surface::SurfaceTextRegion painted = r.editor_region();
    CHECK(painted.caret_row ==
          kEditorHeaderRows + static_cast<std::int64_t>(12 - r.ed().first_row));
    // Ctrl+End: the document's end is visible; Ctrl+Home: back to the top.
    r.t.key(input::scan::kEnd, input::mod::kCtrl);
    CHECK(r.ed().first_row == r.buf().line_count() - rows);
    r.t.key(input::scan::kHome, input::mod::kCtrl);
    CHECK(r.ed().first_row == 0);
}

TEST_CASE("EDIT-0: the wheel scrolls the editor's body, moves no caret, and is consumed there") {
    EditorRig r("wheel", thirty_lines());
    r.open_hello();
    REQUIRE(r.ed().first_row == 0);
    r.wheel_body(-1.0); // a notch toward the maker: later lines
    CHECK(r.ed().first_row == 3);
    CHECK(r.buf().caret_row() == 0); // the caret stayed put...
    CHECK(r.editor_region().caret_row == surface::kNoCaret); // ...and left the window
    r.wheel_body(-1.0);
    CHECK(r.ed().first_row == 6);
    r.wheel_body(+1.0);
    CHECK(r.ed().first_row == 3);
    // Fractional notches accumulate until they are worth whole lines.
    r.wheel_body(+0.5);
    CHECK(r.ed().first_row == 2); // 0.5 * 3 = 1.5 -> one line, 0.5 carried
    r.wheel_body(+0.5);
    CHECK(r.ed().first_row == 0); // the carry makes the second half worth two
    // The next caret gesture brings the view back to the caret.
    r.wheel_body(-2.0);
    REQUIRE(r.ed().first_row > 0);
    r.t.key(input::scan::kRight);
    CHECK(r.ed().first_row == 0);
}

TEST_CASE("EDIT-0: the wheel elsewhere scrolls nothing, and a covered editor is not reached") {
    EditorRig r("wheelmiss", thirty_lines());
    r.open_hello();
    // Over the workspace: nothing.
    r.t.wheel_canvas(-1.0, 60, 20);
    CHECK(r.ed().first_row == 0);
    // Over the Builder pane (a different pane's cells): nothing.
    const Session& s = r.session();
    const ui::Rect builder = cells_covered(
        bounds_of(s.panels, s.setup.active, panel::kBuilder, screen_of(s)).rect);
    r.t.wheel_canvas(-1.0, builder.x + 2, builder.y + 2);
    CHECK(r.ed().first_row == 0);
    // Over the editor's HEADER row: the body's own boundary holds.
    const ui::Rect c = r.editor_cells();
    r.t.wheel_canvas(-1.0, c.x + 2, c.y);
    CHECK(r.ed().first_row == 0);
}

TEST_CASE("EDIT-0: a horizontal window follows the caret and recovers the room an erase frees") {
    EditorRig r("horizontal", "short\n");
    r.open_hello();
    const ExternalBodyPlace body = editor_body(r.session(), screen_of(r.session()));
    const std::int64_t text_cols = editor_text_columns(body);
    for (std::int64_t i = 0; i < text_cols + 10; ++i) {
        r.t.text("a");
    }
    CHECK(r.ed().first_col > 0); // the window slid to keep the caret visible
    const surface::SurfaceTextRegion painted = r.editor_region();
    CHECK(painted.caret_col == text_cols); // at the window's caret column, on screen
    for (std::int64_t i = 0; i < text_cols + 10; ++i) {
        r.t.key(input::scan::kBackspace);
    }
    CHECK(r.ed().first_col == 0); // rule 1: erasing recovers the room it freed
}

TEST_CASE("EDIT-0: a resize reconciles the viewport and does not strand the caret") {
    EditorRig r("resize", thirty_lines());
    r.open_hello();
    r.t.key(input::scan::kEnd, input::mod::kCtrl);
    const std::size_t at_forty = r.ed().first_row;
    REQUIRE(at_forty > 0);
    r.resize_screen(50); // more body rows: the window may relax, the caret stays visible
    const ExternalBodyPlace body = editor_body(r.session(), screen_of(r.session()));
    REQUIRE(body.present);
    const std::size_t cr = r.buf().caret_row();
    CHECK(cr >= r.ed().first_row);
    CHECK(cr < r.ed().first_row + static_cast<std::size_t>(body.rows));
}

// ============================================================================
// Tier 10 — THE CLIPBOARD CONVERSATION: the answer lands where the maker asked
// ============================================================================

TEST_CASE("EDIT-0: copy here, paste there -- multiline, through the medium's own answer") {
    EditorRig r("paste");
    SkinSeat* skin = r.t.mount_skin_seat();
    r.open_hello();
    r.t.key(input::scan::kRight, input::mod::kShift);
    r.t.key(input::scan::kDown, input::mod::kShift);
    r.t.key(input::scan::kC, input::mod::kCtrl);
    CHECK(skin->platform == "one\nt"); // the copy reached the platform's clipboard
    r.t.key(input::scan::kEnd, input::mod::kCtrl);
    r.t.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(r.buf().line_count() == 5);
    CHECK(r.buf().line(3) == "one");
    CHECK(r.buf().line(4) == "t");
    // One undo returns the whole paste.
    r.t.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(r.buf().line_count() == 4);
}

TEST_CASE("EDIT-0: a late paste answer may not land at a caret that has since moved") {
    EditorRig r("latepaste");
    SlowSkin* skin = mount_slow_skin(r.t);
    skin->text = "LATE";
    r.open_hello();
    r.t.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(skin->held); // the answer is owed, and deliberately not yet given
    r.t.text("moved "); // the document moves while the answer is in flight
    r.t.publish(loom::to_value(AnswerNow{}));
    CHECK(r.buf().line(0) == "moved one"); // no LATE anywhere
    CHECK(r.notice().find("arrived after the source moved") != std::string::npos);
    CHECK(r.notice().find("paste again") != std::string::npos);
    // Asked again with the document at rest, the same answer lands at the caret.
    r.t.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(skin->held);
    r.t.publish(loom::to_value(AnswerNow{}));
    CHECK(r.buf().line(0) == "moved LATEone");
}

TEST_CASE("EDIT-0: a late answer for a replaced document is discarded whole") {
    EditorRig r("stalepaste");
    SlowSkin* skin = mount_slow_skin(r.t);
    skin->text = "GHOST";
    r.open_hello();
    r.t.key(input::scan::kV, input::mod::kCtrl);
    REQUIRE(skin->held);
    r.to_command();
    r.choose(2); // to `world`, with a clean buffer: the replacement proceeds
    r.press_e();
    REQUIRE(r.ed().path == r.second);
    r.t.publish(loom::to_value(AnswerNow{}));
    CHECK(r.buf().line(0) == "alpha"); // the old document's paste touched nothing
    CHECK(r.buf().line(1) == "beta");
    for (std::size_t i = 0; i < r.buf().line_count(); ++i) {
        CHECK(r.buf().line(i).find("GHOST") == std::string::npos);
    }
}

TEST_CASE("EDIT-0: a clipboard holding non-ASCII refuses the paste rather than corrupting it") {
    EditorRig r("pastebytes");
    SkinSeat* skin = r.t.mount_skin_seat();
    skin->platform = "caf\xC3\xA9";
    r.open_hello();
    r.t.key(input::scan::kV, input::mod::kCtrl);
    CHECK(r.notice().find("outside plain ASCII") != std::string::npos);
    CHECK(r.buf().line(0) == "one");
    CHECK_FALSE(r.ed().dirty());
}

TEST_CASE("EDIT-0: typed non-ASCII is refused with a sentence, and the keystroke costs nothing") {
    EditorRig r("typebytes");
    r.open_hello();
    r.t.text("caf\xC3\xA9");
    CHECK(r.notice().find("outside plain ASCII") != std::string::npos);
    CHECK(r.buf().line(0) == "one");
    r.t.text("cafe"); // the plain spelling lands
    CHECK(r.buf().line(0) == "cafeone");
}

// ============================================================================
// Tier 11 — THE PANE FAMILY: identity, arrangement, and the loop's return
// ============================================================================

TEST_CASE("EDIT-0: the Editor is an ordinary catalog pane with a durable reference") {
    const PaneRef ref = pane_ref_of(panel::kEditor);
    CHECK(ref.provider == std::string(kWorkshopProvider));
    CHECK(ref.pane == std::string(pane_key::kEditor));
    Panels empty;
    const std::optional<std::int64_t> back = resolve_pane(ref, empty);
    REQUIRE(back.has_value());
    CHECK(*back == panel::kEditor);
    CHECK(placement_of(panel::kEditor) == placement::kOverlayStack);
}

TEST_CASE("EDIT-0: arranging the editor pane moves its window and not one byte of its source") {
    EditorRig r("arrange");
    r.open_hello();
    r.t.text("held ");
    REQUIRE(r.ed().dirty());
    const std::uint64_t rev = r.buf().revision();
    r.to_command();
    enter_arrange_desk(r.t);
    select_pane(r.t, pane_ref_of(panel::kEditor));
    r.t.key(input::scan::kRight); // place it one cell right
    r.t.key(input::scan::kEscape);
    REQUIRE_FALSE(r.session().arrange.open);
    const SetupPane* row = pane_of(r.session().setup.active, pane_ref_of(panel::kEditor));
    REQUIRE(row != nullptr);
    CHECK(row->place.mode == pane_unit::kSubcells); // the window moved...
    CHECK(r.buf().revision() == rev);               // ...the source did not
    CHECK(r.buf().line(0) == "held one");
    CHECK(r.ed().dirty());
}

TEST_CASE("EDIT-0: build and realize stay the Builder's, reached from the editor by one press") {
    EditorRig r("loop");
    r.open_hello();
    r.t.text("// changed");
    r.t.key(input::scan::kReturn);
    r.t.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE_FALSE(r.ed().dirty());
    // Back to command mode, ask for the build -- the same `b` route as ever.
    r.t.press(0, 0);
    r.t.release(0, 0);
    r.t.key(input::scan::kB);
    REQUIRE(r.tool->asked.size() == 1);
    CHECK(r.tool->asked[0] == "hello");
    CHECK(r.tool->realize_asked[0] == false);
    r.t.key(input::scan::kB, input::mod::kShift);
    REQUIRE(r.tool->asked.size() == 2);
    CHECK(r.tool->realize_asked[1] == true);
    // ...and the editor still holds the document, caret coherent, for the next edit.
    CHECK(r.ed().path == r.src);
    CHECK(r.buf().line(0) == "// changed");
}

// The seventh suite's own membership claim, the family's standing rule: a temporary
// directory belongs to the suite that made it, and this binary knows which one it is.
TEST_CASE("EDIT-0: this suite's temporary directories carry its own name") {
    const std::string root = workshop_temp_root().string();
    CHECK(root.find("editor") != std::string::npos);
}
