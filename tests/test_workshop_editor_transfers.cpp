// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The standard Editor in Workshop's typed carry (suite `panes`) -- a real loaded Workshop with the
// Editor, Inventory, its pane, the menu presenter, the opening manager and an input actor whose
// Loom authority each case chooses, driven through the real Input weave:
//
//   out   a selection dragged from its highlight, the right-click Extract and the key, into a
//         named Inventory folder as an owned copy of the buffer's text with its observation
//   in    text dropped where it lands, or onto the highlight to replace it, as one undoable edit;
//         a saved command as its Terminal line, never run; in a C++ document, C++ by choice
//   back  a saved file location that reopens through the managed opening
//
// and the refusals that keep each honest: the byte law, a moved picture, the unsaved floor, a
// missing file in another root, and an actor without the authority. Pure conversions are the
// `source_transfer` suite's; this file drives them through the pane.

#include "editor_transfer_story.hpp"

#include <memory>
#include <string>

namespace {

using namespace editor_transfer_story;

/// THE STANDARD EDITOR'S OWN STATE, read where its cases need it.
struct EditorStory : TransferStory {
    using TransferStory::TransferStory;
    ed::EditorPaneState doc() {
        return loom::from_value<ed::EditorPaneState>(r.bus.weave(r.bus.role_holder(ed::kEditorPaneRole))->snapshot());
    }
    /// The Editor's rows above the document: the status row, and a notice row when one stands.
    std::int64_t chrome() { return doc().notice.empty() ? 1 : 2; }
    /// A document row and column, as the pane's own lattice names it.
    void click_doc(std::int64_t row, std::int64_t col) { click(editor, chrome() + row, col); }
    std::string notice() { return doc().notice; }
};

} // namespace

TEST_SUITE("panes") {

TEST_CASE("a selection dragged from its highlight lands in a named Inventory folder as an owned copy of the buffer's text, unsaved edits included, and nothing of the document moves") {
    EditorStory s("xfer-out");
    const std::string path = s.write("notes.txt", "alpha\n\tbeta gamma\ndelta\n");
    REQUIRE(s.open(path).accepted);
    s.click_doc(0, 5);
    s.text("!"); // an unsaved edit the copy must carry
    s.click_doc(0, 2);
    s.key(input::scan::kDown, input::mod::kShift);
    s.key(input::scan::kEnd, input::mod::kShift);
    const auto before = s.doc();
    REQUIRE(before.anchor_row == 0);
    REQUIRE(before.caret_row == 1);
    const std::string folder = s.make_folder("Snippets");
    const std::int64_t target = s.row_of(s.inventory, "Snippets");
    s.drag(s.editor, s.chrome() + 0, 3, s.inventory, target, 2);
    INFO(s.notice() << " / " << s.r.last_notice());
    const auto kept = s.stored();
    REQUIRE(kept.size() == 1);
    CHECK(kept[0].folder == folder);
    REQUIRE(loom::same_identity(kept[0].pair.item.schema(), *loom::schema_of<st::SourceText>()));
    CHECK(loom::from_value<st::SourceText>(kept[0].pair.item).text == "pha!\n\tbeta gamma");
    REQUIRE(kept[0].pair.metadata.size() == 1);
    const auto seen = loom::from_value<st::SourceSelection>(kept[0].pair.metadata[0]);
    CHECK(seen.path == path);
    CHECK(seen.project_root == s.root.lexically_normal().generic_string());
    CHECK(seen.kind == "characters");
    CHECK(seen.first_line == 1);
    CHECK(seen.first_column == 3);
    CHECK(seen.end_line == 2);
    CHECK(seen.end_column == 12);
    CHECK(seen.line_ending == "LF");
    CHECK(seen.unsaved);
    // THE SOURCE IS UNTOUCHED: the bytes, the selection put back as it stood, and the file on disk.
    const auto after = s.doc();
    CHECK(after.text == before.text);
    CHECK(after.anchor_row == before.anchor_row);
    CHECK(after.anchor_byte == before.anchor_byte);
    CHECK(after.caret_row == before.caret_row);
    CHECK(after.caret_byte == before.caret_byte);
    CHECK(slurp(path) == "alpha\n\tbeta gamma\ndelta\n");
}

TEST_CASE("a press on the highlight that never moves is an ordinary press, and a drag begun off the highlight sweeps and carries nothing, even across the pane's edge") {
    EditorStory s("xfer-click");
    REQUIRE(s.open(s.write("a.txt", "one two\nthree\nfour\n")).accepted);
    s.click_doc(0, 0);
    s.key(input::scan::kEnd, input::mod::kShift);
    REQUIRE(s.doc().caret_byte == 7);
    s.click_doc(0, 2); // on the highlight, and no motion: a click
    auto d = s.doc();
    CHECK(d.caret_row == 0);
    CHECK(d.caret_byte == 2);
    CHECK(d.anchor_byte == 2); // the selection collapsed, as any click collapses it
    CHECK(s.stored().empty());
    // A sweep that starts off the highlight and leaves the pane is still a sweep.
    s.click_doc(0, 0);
    s.key(input::scan::kRight, input::mod::kShift);
    s.drag(s.editor, s.chrome() + 1, 1, s.inventory, 3, 3);
    CHECK(s.stored().empty());
    d = s.doc();
    CHECK(d.anchor_row == 1);
    CHECK(d.anchor_byte == 1);
    CHECK(d.text == "one two\nthree\nfour\n");
}

TEST_CASE("right-click on the highlight offers Extract, which carries by pick-and-place; right-click off it offers nothing") {
    EditorStory s("xfer-menu");
    REQUIRE(s.open(s.write("a.txt", "keep this\nand not this\n")).accepted);
    s.click_doc(0, 0);
    s.key(input::scan::kRight, input::mod::kShift);
    s.key(input::scan::kRight, input::mod::kShift);
    s.key(input::scan::kRight, input::mod::kShift);
    s.key(input::scan::kRight, input::mod::kShift);
    s.click(s.editor, s.chrome() + 1, 4, 3); // off the highlight
    CHECK_FALSE(s.r.session().presented.open);
    s.click(s.editor, s.chrome() + 0, 1, 3); // on it
    REQUIRE(s.r.session().presented.open);
    bool offered = false;
    for (const auto& line : s.r.session().presented.lines) {
        offered = offered || line.text.find("Extract selection to Inventory") != std::string::npos;
    }
    CHECK(offered);
    s.key(input::scan::kReturn);
    INFO(s.notice() << " / " << s.r.last_notice());
    CHECK(s.r.last_notice().find("Carrying") != std::string::npos);
    s.click(s.inventory, 2, 2);
    const auto kept = s.stored();
    REQUIRE(kept.size() == 1);
    CHECK(loom::from_value<st::SourceText>(kept[0].pair.item).text == "keep");
}

TEST_CASE("the keyboard carries the selection by pick-and-place, and with nothing selected it carries nothing in its place") {
    EditorStory s("xfer-key");
    REQUIRE(s.open(s.write("a.txt", "line one\nline two\n")).accepted);
    s.click_doc(1, 0);
    s.key(input::scan::kE, input::mod::kCtrl);
    CHECK(s.notice().find("select text first") != std::string::npos);
    CHECK(s.r.last_notice().find("Carrying") == std::string::npos);
    s.key(input::scan::kEnd, input::mod::kShift);
    s.key(input::scan::kE, input::mod::kCtrl);
    CHECK(s.r.last_notice().find("Carrying") != std::string::npos);
    s.click(s.inventory, 2, 2);
    const auto kept = s.stored();
    REQUIRE(kept.size() == 1);
    CHECK(loom::from_value<st::SourceText>(kept[0].pair.item).text == "line two");
}

TEST_CASE("dropped text is inserted at the painted landing character as one undoable edit, replaces the highlight only when dropped onto it, and saves nothing") {
    EditorStory s("xfer-in");
    const std::string path = s.write("a.txt", "abc\ndef\n");
    REQUIRE(s.open(path).accepted);
    s.add(text_pair("one\ntwo"), "snippet");
    const std::int64_t entry = s.row_of(s.inventory, "snippet");
    s.drag(s.inventory, entry, 2, s.editor, s.chrome() + 1, 1);
    INFO(s.notice());
    auto d = s.doc();
    CHECK(d.text == "abc\ndone\ntwoef\n");
    CHECK(d.caret_row == 2);
    CHECK(d.caret_byte == 3);
    CHECK(s.notice().find("inserted 2 lines at L2:C2") != std::string::npos);
    CHECK(slurp(path) == "abc\ndef\n"); // nothing was saved
    CHECK(s.rows(s.editor)[0].rfind("UNSAVED", 0) == 0);
    s.key(input::scan::kZ, input::mod::kCtrl); // one undo takes the whole drop back
    CHECK(s.doc().text == "abc\ndef\n");
    s.key(input::scan::kY, input::mod::kCtrl);
    CHECK(s.doc().text == "abc\ndone\ntwoef\n");
    s.key(input::scan::kZ, input::mod::kCtrl);
    // Onto the highlight: the highlight is replaced, and undo puts it back selected.
    s.click_doc(0, 0);
    s.key(input::scan::kEnd, input::mod::kShift);
    s.drag(s.inventory, s.row_of(s.inventory, "snippet"), 2, s.editor, s.chrome() + 0, 1);
    d = s.doc();
    CHECK(d.text == "one\ntwo\ndef\n");
    CHECK(s.notice().find("replaced the highlighted selection") != std::string::npos);
    s.key(input::scan::kZ, input::mod::kCtrl);
    d = s.doc();
    CHECK(d.text == "abc\ndef\n");
    CHECK(d.anchor_byte == 0);
    CHECK(d.caret_byte == 3);
    // Beside a selection, not on it: inserted where it landed, the selected text left alone.
    s.drag(s.inventory, s.row_of(s.inventory, "snippet"), 2, s.editor, s.chrome() + 1, 3);
    CHECK(s.doc().text == "abc\ndefone\ntwo\n");
    CHECK(slurp(path) == "abc\ndef\n");
}

TEST_CASE("text the standard Editor cannot hold is refused whole, and the document, its selection and its history stay as they were") {
    EditorStory s("xfer-refuse");
    REQUIRE(s.open(s.write("a.txt", "abc\n")).accepted);
    s.click_doc(0, 3);
    s.text("d");
    s.add(text_pair("red \x1b[31m"), "escape");
    s.add(text_pair("caf\xc3\xa9"), "accent");
    const auto before = s.doc();
    s.drag(s.inventory, s.row_of(s.inventory, "escape"), 2, s.editor, s.chrome() + 0, 1);
    CHECK(s.notice().find("control byte (0x1b)") != std::string::npos);
    s.drag(s.inventory, s.row_of(s.inventory, "accent"), 2, s.editor, s.chrome() + 0, 1);
    CHECK(s.notice().find("outside plain ASCII") != std::string::npos);
    const auto after = s.doc();
    CHECK(after.text == before.text);
    CHECK(after.caret_byte == before.caret_byte);
    s.key(input::scan::kZ, input::mod::kCtrl); // the history is the typing's, untouched
    CHECK(s.doc().text == "abc\n");
}

TEST_CASE("a saved Terminal command dropped on a text document becomes its editable Terminal line and is never sent; a preset's missing fields stay missing") {
    EditorStory s("xfer-command");
    REQUIRE(s.open(s.write("notes.txt", "\n")).accepted);
    int sent = 0;
    const auto watch = s.r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.schema_name == zengine::timer::EnsureTimer::zen_name) ++sent;
    });
    s.add(command_pair(), "beat command");
    s.drag(s.inventory, s.row_of(s.inventory, "beat command"), 2, s.editor, s.chrome() + 0, 0);
    INFO(s.notice());
    CHECK(s.doc().text == "send @zengine.timer EnsureTimer 1 id=\"editor-materials.beat\" delay_ms=250 "
                          "repeat=true preferred=\"keep-remaining\" fallback=\"restart\"\n");
    CHECK(s.notice().find("nothing was sent") != std::string::npos);
    CHECK_FALSE(s.r.session().presented.open); // a text document offers no C++
    // A preset whose required fields were never authored.
    loom::Value partial(loom::schema_of<zengine::timer::EnsureTimer>());
    partial.set("id", loom::Cell::text("half"));
    s.add(zengine::inventory::encode_pair(md::store_draft("half timer", md::Draft(partial)), {}), "half preset");
    s.key(input::scan::kEnd, input::mod::kCtrl);
    s.drag(s.inventory, s.row_of(s.inventory, "half preset"), 2, s.editor, s.chrome() + 1, 0);
    CHECK(s.doc().text.find("\nsend <address> EnsureTimer 1 id=\"half\"") != std::string::npos);
    CHECK(s.notice().find("INCOMPLETE: delay_ms, repeat, preferred, fallback are not set") != std::string::npos);
    s.r.bus.remove_observer(watch);
    CHECK(sent == 0);
}

TEST_CASE("in a C++ document a dropped command offers its Terminal line or generated C++; the C++ lands selected with its includes named, and one undo removes it") {
    EditorStory s("xfer-cpp");
    const std::string path = s.write("tool.cpp", "#include <cstdio>\nint main() {}\n");
    REQUIRE(s.open(path).accepted);
    s.add(command_pair(), "beat command");
    s.drag(s.inventory, s.row_of(s.inventory, "beat command"), 2, s.editor, s.chrome() + 1, 13);
    REQUIRE(s.r.session().presented.open);
    CHECK(s.doc().text == "#include <cstdio>\nint main() {}\n"); // the drop itself chose nothing
    s.key(input::scan::kDown);
    s.key(input::scan::kReturn);
    INFO(s.notice());
    const auto d = s.doc();
    std::ifstream golden(SOURCE_TRANSFER_GOLDEN_FOR_PANES, std::ios::binary);
    const std::string generated((std::istreambuf_iterator<char>(golden)), std::istreambuf_iterator<char>());
    // WHOLE LINES, before the line the drop landed on, so the code joins no text on either side.
    CHECK(d.text == "#include <cstdio>\n" + generated + "int main() {}\n");
    CHECK(d.anchor_row == 1); // selected for review: from the start of the line it landed on...
    CHECK(d.anchor_byte == 0);
    CHECK(d.caret_row == 1 + 23); // ...to where that line now begins
    CHECK(d.caret_byte == 0);
    CHECK(s.notice().find("add #include <zen/schema.hpp> and <zen/value.hpp>") != std::string::npos);
    s.key(input::scan::kZ, input::mod::kCtrl);
    CHECK(s.doc().text == "#include <cstdio>\nint main() {}\n");
    CHECK(slurp(path) == "#include <cstdio>\nint main() {}\n");
    // A header whose language is not known asks for the choice to say so.
    REQUIRE(s.open(s.write("tool.h", "\n")).accepted);
    s.drag(s.inventory, s.row_of(s.inventory, "beat command"), 2, s.editor, s.chrome() + 0, 0);
    REQUIRE(s.r.session().presented.open);
    bool asks = false;
    for (const auto& line : s.r.session().presented.lines) asks = asks || line.text.find("this .h is C++") != std::string::npos;
    CHECK(asks);
    // ...and a pending choice refuses a switch until it is made or dismissed.
    auto sw = std::make_unique<SwitchAsker>();
    SwitchAsker* raw = sw.get();
    loom::Grant g;
    g.allow_to_role(EditorHandoffJudgeRequested::zen_name, 1, ed::kEditorPaneRole);
    const loom::WeaveId id = s.r.bus.register_weave(std::move(sw), g, std::string(kEditorSwitchRole));
    raw->zen_set_self(id);
    s.r.bus.send(id, loom::Message(loom::to_value(SeatDo{})));
    s.r.bus.drain_until_idle();
    REQUIRE(raw->judged.size() == 1);
    CHECK_FALSE(raw->judged[0].ok);
    CHECK(raw->judged[0].refusal.find("waiting for your choice") != std::string::npos);
}

TEST_CASE("a drop aimed at a picture the text has since left is refused and changes nothing") {
    EditorStory s("xfer-stale");
    std::string lines;
    for (int i = 0; i < 80; ++i) lines += "line " + std::to_string(i) + "\n";
    REQUIRE(s.open(s.write("long.txt", lines)).accepted);
    s.add(text_pair("DROPPED"), "word");
    s.click(s.inventory, s.row_of(s.inventory, "word"), 2);
    s.key(input::scan::kReturn); // pick up a copy
    REQUIRE(s.r.last_notice().find("Carrying") != std::string::npos);
    // ONE POLL: the wheel scrolls the Editor, and the press that places the copy was read with it
    // -- aimed at the picture the wheel is about to replace.
    input::InjectedEvent wheel = s.at(s.editor, 5, 3, "PointerWheel");
    wheel.wheel_dy = -1.0;
    s.batch({wheel, s.at(s.editor, 5, 3, "PointerButton", true), s.at(s.editor, 5, 3, "PointerButton", false)});
    INFO(s.notice());
    CHECK(s.notice().find("moved under the drop") != std::string::npos);
    CHECK(s.doc().text == lines);
}

TEST_CASE("a saved location reopens its file through the managed opening at its line, and never over unsaved work") {
    EditorStory s("xfer-locate");
    const std::string a = s.write("a.txt", "first\nsecond\nthird line\nfourth\n");
    const std::string b = s.write("b.txt", "other\n");
    REQUIRE(s.open(a).accepted);
    s.click_doc(2, 3);
    s.key(input::scan::kL, input::mod::kCtrl);
    REQUIRE(s.r.last_notice().find("Carrying") != std::string::npos);
    s.click(s.inventory, 2, 2);
    s.name("a.txt at 3"); // the new entry's name line, as a maker answers it
    auto kept = s.stored();
    REQUIRE(kept.size() == 1);
    const auto loc = loom::from_value<st::SourceLocation>(kept[0].pair.item);
    CHECK(loc.path == a);
    CHECK(loc.line == 3);
    CHECK(loc.column == 4);
    const auto ctx = loom::from_value<st::SourceLocationContext>(kept[0].pair.metadata[0]);
    CHECK(ctx.relative == "a.txt");
    CHECK(ctx.line_text == "third line");
    REQUIRE(s.open(b).accepted);
    CHECK(s.doc().path == b);
    s.drag(s.inventory, s.row_of(s.inventory, "a.txt at 3"), 2, s.editor, s.chrome() + 0, 1);
    INFO(s.notice());
    auto d = s.doc();
    CHECK(d.path == a);
    CHECK(d.caret_row == 2);
    CHECK(d.caret_byte == 3);
    CHECK(s.notice().find("at line 3") != std::string::npos);
    CHECK(d.text == "first\nsecond\nthird line\nfourth\n"); // a location is never inserted as text
    // Unsaved work refuses another source: the location is refused in the Editor's own words.
    REQUIRE(s.open(b).accepted);
    s.click_doc(0, 0);
    s.text("dirty ");
    s.drag(s.inventory, s.row_of(s.inventory, "a.txt at 3"), 2, s.editor, s.chrome() + 0, 1);
    d = s.doc();
    CHECK(d.path == b);
    CHECK(d.text == "dirty other\n");
    CHECK(s.notice().find("unsaved changes") != std::string::npos);
}

TEST_CASE("a location whose saved line has since gained text at its end opens its file and leaves the caret and the text alone, and one still as saved takes the caret") {
    EditorStory s("xfer-locate-whole");
    const std::string a = s.write("a.txt", "first\nsecond line now changed\nthird\n");
    const std::string b = s.write("b.txt", "other\n");
    st::SourceLocationContext ctx;
    ctx.project_root = s.root.lexically_normal().generic_string();
    ctx.relative = "a.txt";
    ctx.line_text = "second line"; // saved whole -- it was shorter than the bound -- before text was added
    s.add(zengine::inventory::encode_pair(loom::to_value(st::SourceLocation{a, 2, 3}), {loom::to_value(ctx)}), "a at 2");
    ctx.line_text = "third";
    s.add(zengine::inventory::encode_pair(loom::to_value(st::SourceLocation{a, 3, 2}), {loom::to_value(ctx)}), "a at 3");
    REQUIRE(s.open(b).accepted);
    s.drag(s.inventory, s.row_of(s.inventory, "a at 2"), 2, s.editor, s.chrome() + 0, 1);
    INFO(s.notice());
    auto d = s.doc();
    CHECK(d.path == a); // the file opens: only its stale caret is declined
    CHECK(s.notice().find("line 2 no longer reads as it did") != std::string::npos);
    CHECK(d.caret_row == 0);
    CHECK(d.caret_byte == 0);
    CHECK(d.text == "first\nsecond line now changed\nthird\n");
    REQUIRE(s.open(b).accepted);
    s.drag(s.inventory, s.row_of(s.inventory, "a at 3"), 2, s.editor, s.chrome() + 0, 1);
    d = s.doc();
    CHECK(d.path == a);
    CHECK(d.caret_row == 2);
    CHECK(d.caret_byte == 1);
    CHECK(s.notice().find("at line 3") != std::string::npos);
}

TEST_CASE("a location saved under another root opens that exact file and says so, and when it is gone it is refused and never replaced by this root's same-named file") {
    TempDir other("xfer-other-root");
    EditorStory s("xfer-worktree");
    const std::string mine = s.write("src/main.cpp", "// this run's copy\n");
    const std::string theirs = s.write("src/main.cpp", "// the other root's copy\n", other.path());
    st::SourceLocationContext ctx;
    ctx.project_root = other.path().lexically_normal().generic_string();
    ctx.relative = "src/main.cpp";
    ctx.line_text = "// the other root's copy";
    s.add(zengine::inventory::encode_pair(loom::to_value(st::SourceLocation{theirs, 1, 4}), {loom::to_value(ctx)}),
          "their main");
    REQUIRE(s.open(s.write("notes.txt", "notes\n")).accepted);
    s.drag(s.inventory, s.row_of(s.inventory, "their main"), 2, s.editor, s.chrome() + 0, 1);
    INFO(s.notice());
    CHECK(s.doc().path == theirs);
    CHECK(s.notice().find("saved under another project root") != std::string::npos);
    REQUIRE(s.open(s.write("notes.txt", "notes\n")).accepted);
    std::filesystem::remove(theirs);
    s.drag(s.inventory, s.row_of(s.inventory, "their main"), 2, s.editor, s.chrome() + 0, 1);
    CHECK(s.doc().path != mine);
    CHECK(s.doc().path.find("notes.txt") != std::string::npos);
    CHECK(s.notice().find("could not open " + theirs) != std::string::npos);
    CHECK(s.notice().find("never follows its name to another root") != std::string::npos);
}

TEST_CASE("an actor without the carry cannot extract, and one without the open may carry a location but not open it") {
    {
        EditorStory s("xfer-no-carry", kEverything & ~kCarry);
        REQUIRE(s.open(s.write("a.txt", "text\n")).accepted);
        s.click_doc(0, 0);
        s.key(input::scan::kEnd, input::mod::kShift);
        s.key(input::scan::kE, input::mod::kCtrl);
        CHECK(s.notice().find("no authority") != std::string::npos);
        CHECK(s.r.last_notice().find("Carrying") == std::string::npos);
    }
    EditorStory s("xfer-no-open", kEverything & ~kOpen);
    const std::string a = s.write("a.txt", "one\n");
    REQUIRE(s.open(a).accepted);
    s.key(input::scan::kL, input::mod::kCtrl);
    s.click(s.inventory, 2, 2);
    s.name("a.txt at 3"); // the new entry's name line, as a maker answers it
    REQUIRE(s.stored().size() == 1);
    REQUIRE(s.open(s.write("b.txt", "two\n")).accepted);
    s.drag(s.inventory, s.row_of(s.inventory, "a.txt at 3"), 2, s.editor, s.chrome() + 0, 1);
    CHECK(s.doc().path.find("b.txt") != std::string::npos);
    CHECK(s.notice().find("no authority") != std::string::npos);
}

} // TEST_SUITE
