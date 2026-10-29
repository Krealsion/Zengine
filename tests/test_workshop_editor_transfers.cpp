// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The standard Editor in Workshop's typed carry (suite `panes`): a real loaded Workshop with the
// Editor, Inventory and its pane, the menu presenter, the opening manager and an input actor
// whose Loom authority each case chooses. OUT, a selection into a named Inventory folder as an
// owned copy; IN, text dropped as one undoable edit, a saved command as its Terminal line; BACK,
// a saved location reopened -- the refusals that keep each honest, and the prose door's drop on a
// text pane beside them. Pure conversions are the `source_transfer` suite's.

#include "editor_transfer_story.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

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
    /// A RIGHT PRESS on a document row and column.
    void right_doc(std::int64_t row, std::int64_t col) { click(editor, chrome() + row, col, 3); }
};

/// THE EDITOR'S OWN ROWS OF THE MENU ON THE SCREEN, down to the rule above the host's rows: each
/// line's words after its two-column marker, the blanks between words and key squeezed to one, so
/// a row reads `Save source ctrl+s`.
std::vector<std::string> editor_rows(EditorStory& s) {
    std::vector<std::string> out;
    for (const std::string& line : presented_texts(s.r)) {
        const std::string text = line.size() > 2 ? line.substr(2) : std::string();
        if (text.rfind("--", 0) == 0) {
            break; // the rule naming the pane, above the host's own rows
        }
        std::string squeezed;
        for (const char c : text) {
            if (c != ' ' || (!squeezed.empty() && squeezed.back() != ' ')) {
                squeezed += c;
            }
        }
        while (!squeezed.empty() && squeezed.back() == ' ') {
            squeezed.pop_back();
        }
        out.push_back(squeezed);
    }
    return out;
}

/// CHOOSE THE EDITOR'S ROW WHOSE WORDS ARE `words`, as a weaver does by keys: Down to it, Return.
void choose(EditorStory& s, const std::string& words) {
    const std::vector<std::string> rows = editor_rows(s);
    std::size_t at = rows.size();
    for (std::size_t i = 0; i < rows.size() && at == rows.size(); ++i) {
        if (rows[i] == words || rows[i].rfind(words + " ", 0) == 0) {
            at = i;
        }
    }
    std::string all;
    for (const std::string& row : rows) all += "\n  " + row;
    REQUIRE_MESSAGE(at < rows.size(), "no row of the Editor's menu reads " << words << ":" << all);
    for (std::size_t i = 0; i < at; ++i) {
        s.key(input::scan::kDown);
    }
    s.key(input::scan::kReturn);
}

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

TEST_CASE("right-click on the highlight puts Extract first in the Editor's menu and carries by pick-and-place; right-click off it opens the Editor's menu there, its rows above the standard rows") {
    EditorStory s("xfer-menu");
    REQUIRE(s.open(s.write("a.txt", "keep this\nand not this\n")).accepted);
    s.click_doc(0, 0);
    s.key(input::scan::kRight, input::mod::kShift);
    s.key(input::scan::kRight, input::mod::kShift);
    s.key(input::scan::kRight, input::mod::kShift);
    s.key(input::scan::kRight, input::mod::kShift);
    s.right_doc(1, 4); // off the highlight
    // OFF IT, THE EDITOR'S OWN MENU OPENS THERE, the host's standard rows beneath its rows, and the
    // selection stands.
    REQUIRE(s.r.session().presented.open);
    CHECK_FALSE(s.r.session().context.open);
    std::vector<std::string> rows = editor_rows(s);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows.front() == "Cut ctrl+x");
    CHECK(presented_texts(s.r).back().find("hide pane") != std::string::npos);
    CHECK(s.doc().caret_byte == 4);
    s.key(input::scan::kEscape);
    REQUIRE_FALSE(s.r.session().presented.open);
    s.right_doc(0, 1); // on it
    REQUIRE(s.r.session().presented.open);
    // EXTRACT FIRST, THE EDITOR'S OTHER ROWS AND THE STANDARD ROWS BENEATH IT.
    rows = editor_rows(s);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows.front() == "Extract selection to Inventory ctrl+e");
    CHECK(presented_texts(s.r).back().find("hide pane") != std::string::npos);
    s.key(input::scan::kReturn);
    INFO(s.notice() << " / " << s.r.last_notice());
    CHECK(s.r.last_notice().find("Carrying") != std::string::npos);
    s.click(s.inventory, 2, 2);
    const auto kept = s.stored();
    REQUIRE(kept.size() == 1);
    CHECK(loom::from_value<st::SourceText>(kept[0].pair.item).text == "keep");
}

TEST_CASE("the Editor's menu offers the acts the document allows now, each beside the key that does it now") {
    // EACH ROW AS THE DOCUMENT STANDS: a step to undo or redo, a selection to cut, copy or carry,
    // unsaved work to discard -- and beside each, the key that does what it does.
    EditorStory s("menu-rows");
    REQUIRE(s.open(s.write("a.txt", "one\ntwo\n")).accepted);
    s.click_doc(1, 0);
    s.right_doc(1, 2);
    REQUIRE(s.r.session().presented.open);
    CHECK(editor_rows(s) == std::vector<std::string>{"Paste ctrl+v", "Select all ctrl+a",
                                                     "Carry this file's location ctrl+l",
                                                     "Save source ctrl+s", "Close file"});
    s.key(input::scan::kEscape);
    s.text("x");
    s.key(input::scan::kRight, input::mod::kShift);
    s.right_doc(1, 3); // off the highlight
    REQUIRE(s.r.session().presented.open);
    CHECK(editor_rows(s) == std::vector<std::string>{
                                "Undo ctrl+z", "Cut ctrl+x", "Copy ctrl+c", "Paste ctrl+v",
                                "Select all ctrl+a", "Extract selection to Inventory ctrl+e",
                                "Carry this file's location ctrl+l", "Save source ctrl+s",
                                "Discard source edits ctrl+d", "Close file"});
    choose(s, "Undo");
    CHECK(s.doc().text == "one\ntwo\n");
    // ...A STEP TO REDO; AND A PRESS ON THE STATUS ROW PUTS THE LOCATION FIRST, AND ONCE.
    s.click(s.editor, 0, 4, 3);
    REQUIRE(s.r.session().presented.open);
    CHECK(editor_rows(s) == std::vector<std::string>{"Carry this file's location ctrl+l",
                                                     "Redo ctrl+y", "Paste ctrl+v",
                                                     "Select all ctrl+a", "Save source ctrl+s",
                                                     "Close file"});
}

TEST_CASE("each edit row of the Editor's menu does what its key does: select all, cut, paste, undo, redo and copy") {
    EditorStory s("menu-edits");
    REQUIRE(s.open(s.write("a.txt", "keep this\n")).accepted);
    s.click_doc(0, 2);
    s.right_doc(0, 2);
    choose(s, "Select all");
    s.right_doc(0, 2);
    choose(s, "Cut");
    CHECK(s.doc().text.empty());
    // THE TERMINAL'S CLIPBOARD CANNOT BE READ, so a paste lands what this process copied last.
    s.right_doc(0, 0);
    choose(s, "Paste");
    CHECK(s.doc().text == "keep this\n");
    s.right_doc(0, 0);
    choose(s, "Undo");
    CHECK(s.doc().text.empty());
    s.right_doc(0, 0);
    choose(s, "Redo");
    CHECK(s.doc().text == "keep this\n");
    s.right_doc(0, 0);
    choose(s, "Select all");
    s.right_doc(0, 0);
    choose(s, "Copy");
    s.key(input::scan::kEnd, input::mod::kCtrl);
    s.right_doc(0, 0);
    choose(s, "Paste");
    CHECK(s.doc().text == "keep this\nkeep this\n");
}

TEST_CASE("a key the weaver's keymap moved shows moved in the Editor's menu, whose row still saves") {
    // TWO KEYS FOR ONE ROW: the menu shows the first, as the legend spells one, so no number of
    // keys a weaver gives a row can stretch its label past what a presenter shows.
    EditorStory s("menu-moved", kEverything, {}, ed::kEditorPaneStem, false,
                  {{"editor.save", "ctrl+g"}, {"editor.save", "ctrl+b"}});
    const std::string path = s.write("a.txt", "one\n");
    REQUIRE(s.open(path).accepted);
    s.click_doc(0, 0);
    s.text("x");
    s.right_doc(0, 2);
    REQUIRE(s.r.session().presented.open);
    const std::vector<std::string> rows = editor_rows(s);
    CHECK(std::find(rows.begin(), rows.end(), "Save source ctrl+g") != rows.end());
    CHECK(std::find(rows.begin(), rows.end(), "Save source ctrl+s") == rows.end());
    choose(s, "Save source");
    CHECK(s.notice().rfind("saved -- ", 0) == 0);
    CHECK(s.doc().saved_text == "xone\n");
}

TEST_CASE("Close file lets a clean document go, and refuses unsaved work as the Editor's other exits do, leaving the document as it was") {
    EditorStory s("menu-close");
    const std::string path = s.write("a.txt", "one\n");
    REQUIRE(s.open(path).accepted);
    s.click_doc(0, 0);
    s.text("x");
    s.right_doc(0, 2);
    choose(s, "Close file");
    CHECK(s.notice() == "the Editor holds unsaved changes to " + path +
                            " -- save source or discard source edits in the Editor first; nothing "
                            "was closed");
    CHECK(s.doc().path == path);
    CHECK(s.doc().text == "xone\n");
    // ...DISCARD, ONE WAY OUT THE REFUSAL NAMES, IS ON THE SAME MENU; THEN THE CLOSE LETS IT GO.
    s.right_doc(0, 2);
    choose(s, "Discard source edits");
    CHECK(s.doc().text == "one\n");
    s.right_doc(0, 2);
    choose(s, "Close file");
    CHECK(s.doc().path.empty());
    CHECK(s.notice().rfind("closed ", 0) == 0);
    bool none = false;
    for (const std::string& row : s.rows(s.editor)) {
        none = none || row.find("no source open") != std::string::npos;
    }
    CHECK(none);
    // AN EMPTY EDITOR HANDS A RIGHT PRESS BACK, and Workshop's own pane menu opens.
    s.click(s.editor, 0, 2, 3);
    CHECK_FALSE(s.r.session().presented.open);
    CHECK(s.r.session().context.open);
}

TEST_CASE("a row of the Editor's menu chosen after another document took the Editor does nothing to it, and says so") {
    // THE CHOICE IS ABOUT THE DOCUMENT THE MENU OPENED ON, as it stood: a source opened while the
    // menu is up takes the Editor, and a row chosen then acts on nothing, as a carry chosen then.
    EditorStory s("menu-stale");
    const std::string first = s.write("a.txt", "one\n");
    const std::string second = s.write("b.txt", "two\n");
    REQUIRE(s.open(first).accepted);
    s.right_doc(0, 2);
    REQUIRE(s.r.session().presented.open);
    REQUIRE(s.open(second).accepted);
    REQUIRE(s.doc().path == second);
    REQUIRE(s.r.session().presented.open);
    choose(s, "Close file");
    CHECK(s.notice() == "nothing was done -- the document changed after you pointed at it");
    CHECK(s.doc().path == second);
    CHECK(s.doc().text == "two\n");
    // ...AND A MENU OPENED ON THE DOCUMENT THERE NOW CLOSES IT.
    s.right_doc(0, 2);
    choose(s, "Close file");
    CHECK(s.doc().path.empty());
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

TEST_CASE("a drop whose pane leaves before it is delivered is said not delivered: a value on a text pane's rows") {
    EditorStory s("xfer-gone");
    s.add(text_pair("one"), "snippet");
    // A TEXT PANE TAKING A VALUE ON ITS ROWS, beside the Inventory the value is carried from.
    const std::string office = "zengine.test.rows";
    ProviderSeat* seat = s.r.mount_provider(office);
    const std::int64_t rows = seat_pane_open(s.r, seat, office.c_str(), "rows");
    REQUIRE(rows != kNoPaneKind);
    s.r.drive(seat, [](ProviderSeat& p, loom::Mail& m) {
        p.say(m, SeatRows{"rows", {TextRow{"drop here", gfx::role::kFill}}});
    });
    const loom::WeaveId seat_id = s.r.bus.role_holder(office);
    // The drop is queued and said sent; the pane leaves before the bus dispatches it.
    bool removed = false;
    RemoveObserver watch{s.r.bus, s.r.bus.add_observer([&s, &removed, seat_id](const loom::BusEvent& e) {
        if (!removed && e.kind == loom::EventKind::Delivered && e.target == s.r.workshop_id &&
            s.r.last_notice().find(" -> ") != std::string::npos)
            removed = s.r.bus.unregister_weave(seat_id) != nullptr;
    })};
    s.drag(s.inventory, s.row_of(s.inventory, "snippet"), 2, rows, 0, 1);
    REQUIRE_MESSAGE(removed, s.r.last_notice());
    CHECK_MESSAGE(s.r.last_notice().find("Value not delivered to " + office) != std::string::npos,
                  s.r.last_notice());
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
    for (const std::string& line : presented_texts(s.r)) asks = asks || line.find("this .h is C++") != std::string::npos;
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

// THE WINDOW SAYS WHAT IT DREW UNDER THE HAND, the line its part names, and the drop names it: the
// Editor refuses a drop the window landed on a line its rows have since left.
TEST_CASE("a drop aimed at a picture the text has since left is refused and changes nothing") {
    EditorStory s("xfer-stale");
    std::string lines;
    for (int i = 0; i < 80; ++i) lines += "line " + std::to_string(i) + "\n";
    REQUIRE(s.open(s.write("long.txt", lines)).accepted);
    s.add(text_pair("DROPPED"), "word");
    s.click(s.inventory, s.row_of(s.inventory, "word"), 2);
    s.key(input::scan::kReturn); // pick up a copy
    REQUIRE(s.r.last_notice().find("Carrying") != std::string::npos);
    // THE PICTURE MOVES UNDER THE DROP: once the window has landed the press that places the copy,
    // and before Workshop has taken it and sent its drop, the window gives the Editor a wheel, which
    // the Editor takes first: the drop is aimed at the picture the wheel replaces.
    const RuntimePane* row = s.r.session().panes.runtime.of_kind(s.editor);
    REQUIRE(row != nullptr);
    const std::string area = pane_window_area(s.r.session().panes, *row);
    const std::string office = std::string(gfx::kWindowOfficePrefix) + gfx::kTerminalWindow;
    const loom::WeaveId window = s.r.bus.role_holder(office);
    REQUIRE(window.valid());
    bool wheeled = false;
    const loom::ObserverId tap = s.r.bus.add_observer([&](const loom::BusEvent& event) {
        if (!wheeled && event.kind == loom::EventKind::Delivered &&
            event.schema_name == input::AttributedInput::zen_name && event.target == window) {
            wheeled = true;
            (void)s.r.bus.office_send_to_role_as(
                window, office, ed::kEditorPaneRole,
                loom::Message(loom::to_value(gfx::AreaWheelTurned{area, 0, -3.0, 10, 10, 0, true, 0})));
        }
    });
    s.batch({s.at(s.editor, 5, 3, "PointerButton", true), s.at(s.editor, 5, 3, "PointerButton", false)});
    s.r.bus.remove_observer(tap);
    REQUIRE(wheeled);
    INFO(s.notice());
    CHECK(s.notice().find("moved under the drop") != std::string::npos);
    CHECK(s.doc().text == lines);
}

TEST_CASE("a press the window landed on a line the Editor's rows have since left is refused in words and places no caret, and one landed on the line drawn there now places it") {
    EditorStory s("xfer-press-line");
    std::string lines;
    for (int i = 0; i < 80; ++i) lines += "line " + std::to_string(i) + "\n";
    REQUIRE(s.open(s.write("long.txt", lines)).accepted);
    const std::int64_t row = s.chrome() + 3;
    const std::string drawn = part_drawn_at(s.editor, row, 2);
    REQUIRE(drawn == "line:4");
    // THE TEXT SCROLLS UNDER IT: the window gives the Editor a wheel.
    const RuntimePane* pane = s.r.session().panes.runtime.of_kind(s.editor);
    REQUIRE(pane != nullptr);
    const std::string area = pane_window_area(s.r.session().panes, *pane);
    REQUIRE(s.r.bus.office_send_to_role_as(s.r.windows.id(), s.r.windows.office(), ed::kEditorPaneRole,
        loom::Message(loom::to_value(gfx::AreaWheelTurned{area, 0, -3.0, 10, 10, 0, true, 0}))).valid());
    s.r.bus.drain_until_idle();
    REQUIRE(part_drawn_at(s.editor, row, 2) != drawn);
    const auto before = s.doc();
    press_as_drawn(s.r, s.editor, row, 2, drawn);
    CHECK_MESSAGE(s.notice().find("the text moved under the press") != std::string::npos, s.notice());
    CHECK(s.doc().caret_row == before.caret_row);
    // ...AND A PRESS LANDED ON THE LINE DRAWN THERE NOW, wherever the notice put it, places the caret.
    const std::int64_t again = s.chrome() + 3;
    const std::string now = part_drawn_at(s.editor, again, 2);
    REQUIRE(now.rfind("line:", 0) == 0);
    press_as_drawn(s.r, s.editor, again, 2, now);
    CHECK(s.doc().caret_row + 1 == std::stoll(now.substr(5)));
}

TEST_CASE("a press aimed at a picture where the text it lands on was not yet selected begins no carry; the drag sweeps") {
    EditorStory s("xfer-stale-press");
    REQUIRE(s.open(s.write("a.txt", "abcdef\nghijkl\nmnopqr\n")).accepted);
    s.make_folder("Snippets");
    const std::int64_t target = s.row_of(s.inventory, "Snippets");
    s.click_doc(0, 0);
    s.key(input::scan::kEnd, input::mod::kShift);
    REQUIRE(s.doc().caret_byte == 6);
    // ONE POLL: a press on the second line, and Shift+Down after it, which grows the selection over
    // that line: the window lands the press first, on the picture where the line was not yet
    // highlighted, and the key waits behind it.
    input::InjectedEvent grow;
    grow.kind = "KeyPressed";
    grow.scancode = input::scan::kDown;
    grow.modifiers = input::mod::kShift;
    input::InjectedEvent grown = grow;
    grown.kind = "KeyReleased";
    auto press = s.at(s.editor, s.chrome() + 1, 2, "PointerButton", true);
    auto release = s.at(s.inventory, target, 2, "PointerButton", false);
    auto move = release;
    move.kind = "PointerMoved";
    move.dx = release.x - press.x;
    move.dy = release.y - press.y;
    s.batch({press, grow, grown, move, release});
    CHECK(s.stored().empty());
    const auto d = s.doc();
    CHECK(d.anchor_row == 1);
    CHECK(d.anchor_byte == 2);
    CHECK_FALSE((d.caret_row == 1 && d.caret_byte == 2)); // the motion swept from the press
    CHECK(d.text == "abcdef\nghijkl\nmnopqr\n");
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
    s.name("a.txt at 3"); // the new entry's name line, as a weaver answers it
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
    s.name("a.txt at 3"); // the new entry's name line, as a weaver answers it
    REQUIRE(s.stored().size() == 1);
    REQUIRE(s.open(s.write("b.txt", "two\n")).accepted);
    s.drag(s.inventory, s.row_of(s.inventory, "a.txt at 3"), 2, s.editor, s.chrome() + 0, 1);
    CHECK(s.doc().path.find("b.txt") != std::string::npos);
    CHECK(s.notice().find("no authority") != std::string::npos);
}


// THE EDITOR ANSWERS ONLY THE WEAVER'S HAND ON A WEAVER'S HOST, at a drop and a wheel too: a guest's
// location dragged from Inventory toward the Editor carries nothing -- what a guest would carry
// rests in no pane there -- moving neither the document nor the keys, and its wheel over the Editor
// scrolls nothing. The guest stored that location on a development host, where its hand reaches the
// Editor as the weaver's.
TEST_CASE("on a weaver's host a guest's wheel and value drop toward the Editor are refused, and the Editor hears neither") {
    EditorStory s("xfer-guest-host");
    scope::GuestRowFacts row;
    row.admitted = true;
    row.name = "agent";
    row.powers = {"input", "inventory", "open"};
    row.version = 2;
    const loom::WeaveId guest = s.hand_id;
    s.r.host.guest_row = [guest, row](loom::WeaveId who) {
        return who == guest ? row : scope::GuestRowFacts{};
    };
    s.r.host.host_fact.development = true;
    const std::string a = s.write("a.txt", "one\n");
    REQUIRE(s.open(a).accepted);
    s.key(input::scan::kL, input::mod::kCtrl);
    s.click(s.inventory, 2, 2);
    s.name("a.txt at 3");
    REQUIRE(s.stored().size() == 1);
    std::string lines;
    for (int i = 0; i < 80; ++i) lines += "line " + std::to_string(i) + "\n";
    REQUIRE(s.open(s.write("b.txt", lines)).accepted);
    REQUIRE(s.doc().first_row == 0);
    const auto wheel = [&s] {
        input::InjectedEvent e = s.at(s.editor, s.chrome() + 2, 1, "PointerWheel");
        e.wheel_dy = -3.0;
        s.batch({e});
    };
    const std::string refused =
        "this is a weaver's host: the editor answers only the weaver's hand, and guest 'agent' is not the weaver";

    s.r.host.host_fact.development = false;
    s.drag(s.inventory, s.row_of(s.inventory, "a.txt at 3"), 2, s.editor, s.chrome() + 0, 1);
    INFO(s.r.last_notice());
    CHECK(s.r.last_notice().find("a guest's carried item rests in no pane") != std::string::npos);
    CHECK(s.doc().path.find("b.txt") != std::string::npos);
    CHECK(s.r.session().panes.keyboard == s.inventory); // the drag's press took them; the drop did not
    wheel();
    CHECK(s.r.last_notice() == refused);
    CHECK(s.doc().first_row == 0);

    // THE TWIN, ON A DEVELOPMENT HOST: the same wheel scrolls, and the same drop reopens the location.
    // (A host is one or the other for a whole run: the desk is laid out again here, so the window
    // places the Editor's area as the host now says, for the weaver's hand alone or for any.)
    s.r.host.host_fact.development = true;
    s.r.extent(181, 60);
    s.r.extent(180, 60);
    wheel();
    CHECK(s.doc().first_row > 0);
    s.drag(s.inventory, s.row_of(s.inventory, "a.txt at 3"), 2, s.editor, s.chrome() + 0, 1);
    CHECK(s.doc().path.find("a.txt") != std::string::npos);
}
} // TEST_SUITE
