// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Neovim-backed Editor in Workshop's typed carry (suite `workshop_neovim`, behind the `neovim`
// gate) -- the same loaded Workshop the standard Editor's transfer cases use
// (`editor_transfer_story.hpp`), with the Neovim-backed Editor holding the office and a real Neovim
// under the case's own directory:
//
//   out   a Visual selection dragged from its highlight, the right-click Extract and `ctrl+k`, into
//         Inventory as exactly what Neovim's own yank takes -- characterwise, linewise and block
//   in    text dropped where the hand aimed, as data and one undo step, replacing the Visual
//         highlight only when dropped onto it; a saved command as its Terminal line; in a `cpp`
//         buffer, C++ by choice
//   back  a saved location reopened through the managed opening, with Neovim's unsaved buffer kept
//
// and the refusals Neovim's own state makes: a mode no drop may enter, and `ctrl+k` left to Neovim
// wherever Neovim gives it a meaning. Time is given by hand: this rig mounts no Timer, so a case
// hands the Neovim-backed Editor its beat and looks, within a bound of wall time.

#include "doctest.h"

#include "editor_transfer_story.hpp"
#include "neovim_environment.hpp"

#include "neovim-editor/vocabulary.hpp"

#if defined(NEOVIM_PROGRAM)

#include <chrono>
#include <thread>

namespace {

using namespace editor_transfer_story;
namespace nve = zengine::neovim_editor;

/// WHERE THIS CASE'S NEOVIM KEEPS ITS STATE -- set before the Workshop exists, because Neovim
/// starts when its pane is first given room.
struct NeovimHome {
    TempDir home;
    NeovimEnvironment env;
    explicit NeovimHome(const char* tag) : home(tag), env(home.path(), NEOVIM_PROGRAM) {}
};

struct PokeState { ZEN_SHAPE(PokeState, 1); };

/// A PARTY THAT READS THE EDITOR'S PROBE SURFACE (`zen.PokeRead`): a Neovim-backed Editor refuses a
/// snapshot while its Neovim runs, so its state is read the way a probe reads it.
class NeovimReader : public loom::WeaveBase<NeovimReader, PokeState, loom::Accept<loom::Result, loom::Refused>,
                                            loom::Emit<>> {
public:
    std::vector<std::pair<std::uint64_t, std::string>> answers;
    void on(const loom::Result& r, loom::Mail& mail) { answers.emplace_back(mail.correlation(), r.value); }
    void on(const loom::Refused& r, loom::Mail& mail) { answers.emplace_back(mail.correlation(), "REFUSED: " + r.reason); }
};

struct NeovimStory : NeovimHome, TransferStory {
    NeovimReader* reader = nullptr;
    loom::WeaveId reader_id{};
    std::uint64_t reads = 0;

    explicit NeovimStory(const char* tag, int permissions = kEverything)
        : NeovimHome(tag), TransferStory(tag, permissions, {}, nve::kNeovimEditorStem) {
        auto probe = std::make_unique<NeovimReader>();
        reader = probe.get();
        reader_id = r.bus.register_weave(std::move(probe), loom::Grant{}, std::string());
        reader->zen_set_self(reader_id);
        REQUIRE_MESSAGE(until([&] { return read("ready") == "true"; }), "Neovim never became ready: ", read("failure"));
    }

    loom::WeaveId holder() { return r.bus.role_holder(ed::kEditorPaneRole); }

    /// ONE BEAT, AS THE TIMER WOULD GIVE IT, then drain.
    void tick() {
        (void)r.bus.send(holder(), loom::Message(loom::to_value(zengine::timer::TimerFired{"zengine.neovim-editor.beat"}),
                                                 loom::WeaveId{}, loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }
    template <class Done>
    bool until(Done done, int ms = 20000) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        for (;;) {
            tick();
            if (done()) {
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    /// A FEW MORE BEATS, so what Neovim drew last is the picture the pane was handed.
    void settle() {
        for (int i = 0; i < 6; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            tick();
        }
    }
    std::string read(const char* field) {
        const std::uint64_t corr = ++reads;
        (void)r.bus.send(holder(), loom::Message(loom::to_value(loom::PokeRead{field}), loom::WeaveId{}, reader_id, corr));
        r.bus.drain_until_idle();
        for (const auto& one : reader->answers) {
            if (one.first == corr) {
                return one.second;
            }
        }
        return "(unanswered)";
    }
    std::string notice() { return read("notice"); }
    bool shows(const std::string& text) {
        for (const std::string& row : rows(editor)) {
            if (row.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    std::string row(std::int64_t at) {
        const auto rs = rows(editor);
        return at >= 0 && at < static_cast<std::int64_t>(rs.size()) ? rs[static_cast<std::size_t>(at)] : std::string();
    }
    /// THE KEYS TO NEOVIM: a press on the status row, which moves nothing in Neovim.
    void focus() { click(editor, 0, 0); }
    /// NEOVIM'S KEYS, AS TEXT A MAKER TYPES, then beats until Neovim has drawn them.
    void keys(const std::string& typed) {
        text(typed);
        settle();
    }
    void escape() {
        key(input::scan::kEscape);
        settle();
    }
    bool offered(const char* label) {
        for (const auto& line : r.session().presented.lines) {
            if (line.text.find(label) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

std::string all_rows(NeovimStory& s) {
    std::string out;
    for (const std::string& row : s.rows(s.editor)) {
        out += "  | " + row + "\n";
    }
    return out;
}

} // namespace

TEST_SUITE("workshop_neovim") {

TEST_CASE("a Visual selection dragged from its highlight lands in a named Inventory folder as exactly what Neovim's yank takes, unsaved edits included, and Neovim keeps its selection and its buffer") {
    NeovimStory s("nvim-xfer-out");
    const std::string path = s.write("notes.txt", "alpha beta\ngamma delta\nepsilon\n");
    REQUIRE(s.open(path).accepted);
    REQUIRE(s.until([&] { return s.shows("gamma delta"); }));
    s.focus();
    s.keys("A!"); // an unsaved edit the copy must carry
    s.escape();
    s.keys("0wvj"); // from `b` of `beta!` to `d` of `delta`, characterwise
    REQUIRE(s.until([&] { return s.read("mode") == "v" && s.read("modified") == "true"; }));
    s.settle();
    const std::string folder = s.make_folder("Snippets");
    const std::int64_t target = s.row_of(s.inventory, "Snippets");
    s.drag(s.editor, 1, 7, s.inventory, target, 2); // pressed on `e` of `beta!`, on the highlight
    INFO(s.notice() << " / " << s.r.last_notice() << "\n" << all_rows(s));
    auto kept = s.stored();
    REQUIRE(kept.size() == 1);
    CHECK(kept[0].folder == folder);
    REQUIRE(loom::same_identity(kept[0].pair.item.schema(), *loom::schema_of<st::SourceText>()));
    CHECK(loom::from_value<st::SourceText>(kept[0].pair.item).text == "beta!\ngamma d");
    REQUIRE(kept[0].pair.metadata.size() == 1);
    const auto seen = loom::from_value<st::SourceSelection>(kept[0].pair.metadata[0]);
    CHECK(seen.path == path);
    CHECK(seen.project_root == s.root.lexically_normal().generic_string());
    CHECK(seen.kind == "characters");
    CHECK(seen.first_line == 1);
    CHECK(seen.first_column == 7);
    CHECK(seen.end_line == 2);
    CHECK(seen.end_column == 8);
    CHECK(seen.line_ending == "LF");
    CHECK(seen.unsaved);
    CHECK(seen.editor.find("Neovim") != std::string::npos);
    // NEOVIM KEEPS WHAT IT HELD: the selection put back (`gv`), the buffer as it was, nothing written.
    CHECK(s.until([&] { return s.read("mode") == "v"; }));
    CHECK(s.read("modified") == "true");
    CHECK(s.shows("alpha beta!"));
    CHECK(slurp(path) == "alpha beta\ngamma delta\nepsilon\n");
    // ...AND IT IS THE SAME SELECTION: `ctrl+k` in Visual mode carries it again, and it reads the same.
    s.name("the beta");
    s.focus();
    s.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(s.r.last_notice().find("Carrying") != std::string::npos);
    s.click(s.inventory, 2, 2);
    kept = s.stored();
    REQUIRE(kept.size() == 2);
    CHECK(loom::from_value<st::SourceText>(kept[1].pair.item).text == "beta!\ngamma d");
}

TEST_CASE("a press on the Visual highlight that never moves is Neovim's own click, and a drag begun off the highlight is Neovim's own sweep and carries nothing") {
    NeovimStory s("nvim-xfer-press");
    const std::string path = s.write("a.txt", "one two three\nfour\n");
    REQUIRE(s.open(path).accepted);
    REQUIRE(s.until([&] { return s.shows("one two three"); }));
    s.focus();
    s.keys("ve"); // `one`
    REQUIRE(s.until([&] { return s.read("mode") == "v"; }));
    s.settle();
    s.click(s.editor, 1, 1); // on the highlight, no motion: Neovim's click ends Visual mode
    CHECK(s.until([&] { return s.read("mode") == "n"; }));
    CHECK(s.stored().empty());
    s.focus();
    s.keys("ve");
    REQUIRE(s.until([&] { return s.read("mode") == "v"; }));
    s.settle();
    s.drag(s.editor, 1, 9, s.inventory, 3, 3); // begun on `three`, off the highlight
    CHECK(s.stored().empty());
    CHECK(slurp(path) == "one two three\nfour\n");
}

TEST_CASE("ctrl+k carries a linewise selection as lines and a block as a block, and in Insert mode ctrl+k stays Neovim's digraph key") {
    NeovimStory s("nvim-xfer-kinds");
    const std::string path = s.write("kinds.txt", "one\ttwo\nthree four\nfive\n");
    REQUIRE(s.open(path).accepted);
    REQUIRE(s.until([&] { return s.shows("three four"); }));
    s.focus();
    s.keys("jVj");
    REQUIRE(s.until([&] { return s.read("mode") == "V"; }));
    s.settle();
    s.key(input::scan::kK, input::mod::kCtrl);
    INFO(s.notice() << " / " << s.r.last_notice());
    REQUIRE(s.r.last_notice().find("Carrying") != std::string::npos);
    s.click(s.inventory, 2, 2);
    s.name("lines");
    auto kept = s.stored();
    REQUIRE(kept.size() == 1);
    CHECK(loom::from_value<st::SourceText>(kept[0].pair.item).text == "three four\nfive\n");
    auto seen = loom::from_value<st::SourceSelection>(kept[0].pair.metadata[0]);
    CHECK(seen.kind == "lines");
    CHECK(seen.first_line == 2);
    CHECK(seen.first_column == 1);
    CHECK(seen.end_line == 4);
    CHECK(seen.end_column == 1);
    CHECK_FALSE(seen.unsaved);

    // A BLOCK: columns 2..3 of lines 1 and 2, the tab on line 1 standing right of it.
    s.focus();
    s.escape();
    s.keys("gg0l");
    s.key(input::scan::kV, input::mod::kCtrl);
    s.keys("jl");
    REQUIRE(s.until([&] { return s.shows("V-BLOCK"); }));
    s.settle();
    s.key(input::scan::kK, input::mod::kCtrl);
    REQUIRE(s.r.last_notice().find("Carrying") != std::string::npos);
    s.click(s.inventory, 2, 2);
    s.name("block");
    kept = s.stored();
    REQUIRE(kept.size() == 2);
    CHECK(loom::from_value<st::SourceText>(kept[1].pair.item).text == "ne\nhr");
    seen = loom::from_value<st::SourceSelection>(kept[1].pair.metadata[0]);
    CHECK(seen.kind == "block");

    // INSERT MODE: `ctrl+k` is not declared, so it is Neovim's -- a digraph, `e:` making U+00EB.
    s.focus();
    s.escape();
    s.keys("A");
    REQUIRE(s.until([&] { return s.read("mode") == "i"; }));
    s.settle();
    s.key(input::scan::kK, input::mod::kCtrl);
    s.keys("e:");
    s.escape();
    REQUIRE(s.until([&] { return s.read("modified") == "true"; }));
    CHECK(s.stored().size() == 2); // nothing more was carried
    s.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE(s.until([&] { return s.read("modified") == "false"; }));
    CHECK(slurp(path) == "one\ttwo\nthree four\xC3\xAB\nfive\n");
}

TEST_CASE("right-click on the Visual highlight offers Extract and Neovim's own menu; off the highlight the right press is Neovim's alone") {
    NeovimStory s("nvim-xfer-menu");
    REQUIRE(s.open(s.write("a.txt", "keep this\nand not this\n")).accepted);
    REQUIRE(s.until([&] { return s.shows("and not this"); }));
    s.focus();
    s.keys("ve");
    REQUIRE(s.until([&] { return s.read("mode") == "v"; }));
    s.settle();
    s.click(s.editor, 1, 2, 3); // on the highlight
    REQUIRE(s.r.session().presented.open);
    CHECK(s.offered("Extract selection to Inventory"));
    CHECK(s.offered("Neovim's own menu"));
    s.key(input::scan::kReturn);
    INFO(s.notice() << " / " << s.r.last_notice());
    CHECK(s.r.last_notice().find("Carrying") != std::string::npos);
    s.click(s.inventory, 2, 2);
    const auto kept = s.stored();
    REQUIRE(kept.size() == 1);
    CHECK(loom::from_value<st::SourceText>(kept[0].pair.item).text == "keep");
    CHECK(s.read("mode") == "v"); // the menu moved nothing in Neovim
    // OFF THE HIGHLIGHT the right press crosses to Neovim as its own, and no Workshop menu opens.
    s.name("kept");
    s.settle();
    s.click(s.editor, 2, 4, 3);
    CHECK_FALSE(s.r.session().presented.open);
    CHECK(s.stored().size() == 1);
}

TEST_CASE("dropped text lands in Neovim as data where the hand aimed, as one undo step, replaces the Visual highlight only when dropped onto it, and writes nothing") {
    NeovimStory s("nvim-xfer-in");
    const std::string path = s.write("in.txt", "abc\ndef\n");
    REQUIRE(s.open(path).accepted);
    REQUIRE(s.until([&] { return s.shows("def"); }));
    s.add(text_pair("one\ntwo"), "snippet");
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "snippet"), 2, s.editor, 2, 1); // onto `e` of `def`
    INFO(s.notice() << "\n" << all_rows(s));
    REQUIRE(s.until([&] { return s.shows("twoef"); }));
    CHECK(s.row(1) == "abc");
    CHECK(s.row(2) == "done");
    CHECK(s.row(3) == "twoef");
    CHECK(s.notice().find("inserted 2 lines at line 2, byte 2") != std::string::npos);
    CHECK(s.read("modified") == "true");
    CHECK(slurp(path) == "abc\ndef\n");
    // ONE UNDO STEP takes the whole drop back.
    s.focus();
    s.keys("u");
    REQUIRE(s.until([&] { return s.row(2) == "def"; }));
    CHECK_FALSE(s.shows("twoef"));

    // KEYS IN THE TEXT ARE TEXT: Escape, `:qa!` and a carriage return arrive as bytes, never as input.
    s.add(text_pair("\x1b:qa!\r"), "escape");
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "escape"), 2, s.editor, 1, 0);
    REQUIRE(s.until([&] { return s.shows("^[:qa!^M"); }));
    CHECK(s.read("running") == "true");
    s.focus();
    s.keys("u");
    REQUIRE(s.until([&] { return !s.shows("^[:qa!^M"); }));

    // IN VISUAL MODE a drop onto the highlight replaces it; one beside it is refused, whole.
    s.keys("vl"); // `ab`
    REQUIRE(s.until([&] { return s.read("mode") == "v"; }));
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "snippet"), 2, s.editor, 2, 0);
    CHECK(s.notice().find("replaces the highlight only when dropped onto it") != std::string::npos);
    CHECK(s.row(1) == "abc");
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "snippet"), 2, s.editor, 1, 1);
    REQUIRE(s.until([&] { return s.shows("twoc"); }));
    CHECK(s.row(1) == "one");
    CHECK(s.row(2) == "twoc");
    CHECK(s.notice().find("replaced the Visual selection") != std::string::npos);
    CHECK(s.until([&] { return s.read("mode") == "n"; }));
    CHECK(slurp(path) == "abc\ndef\n");
}

TEST_CASE("a drop into a mode Neovim is still in the middle of is refused in Neovim's words and changes nothing") {
    NeovimStory s("nvim-xfer-mode");
    REQUIRE(s.open(s.write("m.txt", "abc\n")).accepted);
    REQUIRE(s.until([&] { return s.shows("abc"); }));
    s.add(text_pair("xyz"), "snippet");
    s.focus();
    s.keys(":let g:typed = 1");
    REQUIRE(s.until([&] { return s.read("mode") == "c"; }));
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "snippet"), 2, s.editor, 1, 1);
    INFO(s.notice());
    CHECK(s.notice().find("nothing was inserted") != std::string::npos);
    CHECK(s.notice().find("mode c") != std::string::npos);
    CHECK(s.read("modified") == "false");
    CHECK(s.row(1) == "abc");
    s.escape();
}

TEST_CASE("a saved command dropped on a text buffer becomes its Terminal line and is never sent; in a cpp buffer a choice offers generated C++, which one undo removes, and a pending choice refuses a switch") {
    NeovimStory s("nvim-xfer-command");
    REQUIRE(s.open(s.write("notes.txt", "first\n")).accepted);
    REQUIRE(s.until([&] { return s.shows("first"); }));
    s.add(command_pair(), "beat command");
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "beat command"), 2, s.editor, 1, 0);
    INFO(s.notice() << "\n" << all_rows(s));
    REQUIRE(s.until([&] { return s.shows("send @zengine.timer EnsureTimer 1 id=\"editor-materials.beat\""); }));
    CHECK(s.notice().find("nothing was sent") != std::string::npos);

    const std::string cpp = s.write("main.cpp", "#include <cstdio>\nint main() {}\n");
    REQUIRE(s.open(cpp).accepted);
    REQUIRE(s.until([&] { return s.shows("int main() {}"); }));
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "beat command"), 2, s.editor, 2, 13);
    REQUIRE(s.r.session().presented.open);
    CHECK(s.offered("Insert its Terminal line"));
    CHECK(s.offered("Generate C++ that builds it"));
    CHECK(s.read("modified") == "false"); // the drop itself chose nothing
    // ...AND A PENDING CHOICE REFUSES A SWITCH until it is made or dismissed.
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

    s.key(input::scan::kDown);
    s.key(input::scan::kReturn);
    REQUIRE(s.until([&] { return s.shows("make_ensure_timer_v1"); }));
    CHECK(s.notice().find("add #include <zen/schema.hpp> and <zen/value.hpp>") != std::string::npos);
    CHECK(s.read("modified") == "true");
    CHECK(slurp(cpp) == "#include <cstdio>\nint main() {}\n");
    s.focus();
    s.keys("u");
    REQUIRE(s.until([&] { return !s.shows("make_ensure_timer_v1"); }));
    CHECK(s.until([&] { return s.read("modified") == "false"; }));
}

TEST_CASE("ctrl+k in Normal mode carries this file's location, which reopens the file through the managed opening at its line; Neovim's unsaved buffer is kept, and a changed line or a missing file is refused in words") {
    NeovimStory s("nvim-xfer-locate");
    const std::string a = s.write("a.txt", "first\nsecond\nthird line\nfourth\n");
    const std::string b = s.write("b.txt", "other\n");
    REQUIRE(s.open(a).accepted);
    REQUIRE(s.until([&] { return s.shows("third line"); }));
    s.focus();
    s.keys("2j3l"); // line 3, on `r`
    s.settle();
    s.key(input::scan::kK, input::mod::kCtrl);
    INFO(s.notice() << " / " << s.r.last_notice());
    REQUIRE(s.r.last_notice().find("Carrying") != std::string::npos);
    s.click(s.inventory, 2, 2);
    s.name("a.txt at 3");
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
    REQUIRE(s.until([&] { return s.read("path") == b; }));
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "a.txt at 3"), 2, s.editor, 1, 1);
    REQUIRE(s.until([&] { return s.read("path") == a; }));
    CHECK(s.notice().find("at line 3") != std::string::npos);
    // THE CURSOR IS WHERE THE LOCATION SAID: an `X` typed now lands before `r`.
    s.focus();
    s.keys("iX");
    s.escape();
    REQUIRE(s.until([&] { return s.shows("thiXrd line"); }));

    // UNSAVED WORK IS KEPT: b opens beside a's modified buffer, and the location brings a back as
    // Neovim holds it -- and since line 3 no longer reads as it did, the cursor is left alone.
    REQUIRE(s.open(b).accepted);
    REQUIRE(s.until([&] { return s.read("path") == b; }));
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "a.txt at 3"), 2, s.editor, 1, 1);
    REQUIRE(s.until([&] { return s.read("path") == a; }));
    CHECK(s.shows("thiXrd line"));
    CHECK(s.read("modified") == "true");
    CHECK(s.notice().find("no longer reads as it did") != std::string::npos);
    CHECK(slurp(a) == "first\nsecond\nthird line\nfourth\n");

    // A MISSING FILE IS REFUSED, and Neovim is never asked to create it.
    std::filesystem::remove(b);
    s.add(zengine::inventory::encode_pair(loom::to_value(st::SourceLocation{b, 1, 1}), {}), "gone b");
    s.settle();
    s.drag(s.inventory, s.row_of(s.inventory, "gone b"), 2, s.editor, 1, 1);
    CHECK(s.notice().find("is not there") != std::string::npos);
    CHECK(s.read("path") == a);
    CHECK_FALSE(std::filesystem::exists(b));
}

} // TEST_SUITE

#endif // NEOVIM_PROGRAM
