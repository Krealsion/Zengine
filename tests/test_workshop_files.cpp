// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop project suite — WHERE SOURCE COMES FROM. Two halves of one subject: the
// Files pane (what a listing is allowed to claim, where the root boundary actually lives,
// which names may be opened at all, and how a row reaches the ONE editor door), and what a
// project-relative source MEANS to the editor and to the build that compiles it.
//
// TWO TIERS. The enumeration, the ordering and the name admission are PURE and asserted as
// values; everything about routing, pressing, refreshing and opening is driven through the
// real weave on a real bus (`Live`), because the interesting half of a browser is not the
// sort but what the application does around it -- what a first press means, what a build
// finishing is allowed to move, and what can never quietly replace an unsaved document.
//
// EVERY CASE THAT TOUCHES DISK USES A TEMPORARY DIRECTORY OF ITS OWN, and the project root
// this suite gives Workshop is that directory: nothing here reads the machine's real
// working directory, and nothing writes into the source tree.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// THE BUILD SIDE OF THE SAME QUESTION. This suite owns "where does source come from",
// and half that answer is what a RELATIVE authored source means to the thing that
// compiles it -- so the generated project and the preflight are read here, against the
// same completed catalog the editor reads.
#include "builder/generate.hpp"
#include "workshop/recipe_persist.hpp"

// ============================================================================
// Support: a project on disk, and a Workshop launched into it
// ============================================================================

inline void put_file(const std::filesystem::path& at, const std::string& bytes) {
    std::ofstream out(at, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}

/// A Workshop standing in a project of this case's own making, with the Files pane open.
///
/// `project_dir` IS SET THE WAY THE HOST SETS IT -- one string on the HostContext, before
/// the weave runs. That is the whole of what a launch directory is to this application, so
/// a rig that supplied it any other way would be proving something about a rig.
struct FilesRig {
    Live t;
    TempDir dir;
    std::filesystem::path root;

    explicit FilesRig(const char* tag = "files", bool with_project = true) : dir(tag) {
        root = dir.path();
        if (with_project) {
            t.host.project_dir = root.generic_string();
        }
        resize_screen(40);
    }

    void resize_screen(std::int64_t h) {
        t.publish(loom::to_value(surface::SurfaceExtent{78, h, 0, 0}));
    }
    void open() { pick(t, panel::kProjectFiles); }
    const Session& session() const { return t.w->session(); }
    const FilesPane& pane() const { return session().panels.files; }
    const Listing& listing() const { return pane().listing; }
    std::string notice() const { return session().notice; }
    std::string shown() const { return stack_text(t.canvases.back()); }

    ui::Rect files_cells() const {
        const Session& s = session();
        return cells_covered(
            bounds_of(s.panels, s.setup.active, panel::kProjectFiles, screen_of(s)).rect);
    }
    /// A press on one BODY row of the pane -- the same header offset the resolution
    /// reserved, so what a case aims at is what the painter drew.
    void press_body(std::int64_t row) {
        const ui::Rect c = files_cells();
        t.press_canvas(c.x + 2, c.y + kFilesHeaderRows + row);
    }
    void wheel_body(double dy, std::int64_t row = 0) {
        const ui::Rect c = files_cells();
        t.publish(loom::to_value(input::PointerWheel{
            0.0, dy, c.x + 2, c.y + kFilesHeaderRows + row + surface::kTuiCanvasTopRow,
            input::space::kCells, input::mod::kNone}));
    }
    /// HAND THE KEYS BACK TO COMMAND MODE the way a maker does -- a press on a pane that
    /// does not take them. The Info panel is the reliable one: it is open by default, it
    /// lives in the side region rather than in the stack the browser and the editor share,
    /// and it clears the keyboard candidate by the same one line every press goes through.
    void to_command() {
        const Session& s = session();
        const ui::Rect info =
            cells_covered(bounds_of(s.panels, s.setup.active, panel::kInfo, screen_of(s)).rect);
        t.press_canvas(info.x + 1, info.y + 1);
        REQUIRE(keyboard_context(session()) == KeyContext::kCommand);
    }
    /// Which row the cursor is on, by NAME -- cases assert about entries, never indices.
    std::string at_cursor() const {
        const FileRow* row = row_at(listing(), pane().cursor);
        return row != nullptr ? row->name : std::string();
    }
    std::vector<std::string> names() const {
        std::vector<std::string> out;
        for (const FileRow& r : listing().rows) {
            out.push_back(r.name);
        }
        return out;
    }
};

// ============================================================================
// Tier 1 — ENUMERATION, ORDERING AND ADMISSION, as values
// ============================================================================

TEST_CASE("EDIT-1: a listing is directories first, then files, bytewise inside each") {
    TempDir dir("order");
    std::filesystem::create_directory(dir.path() / "zeta");
    std::filesystem::create_directory(dir.path() / "Alpha");
    put_file(dir.path() / "beta.cpp", "x");
    put_file(dir.path() / "Aardvark.txt", "x");
    const Listing l = enumerate_directory(dir.path().generic_string());
    REQUIRE(l.known);
    REQUIRE(l.rows.size() == 4);
    // Directories first, whatever their names sort to against the files.
    CHECK(l.rows[0].directory);
    CHECK(l.rows[1].directory);
    CHECK_FALSE(l.rows[2].directory);
    CHECK_FALSE(l.rows[3].directory);
    // BYTEWISE, WHICH IS NOT ALPHABETICAL: `A` is 0x41 and `z` is 0x7A, so an
    // uppercase name sorts before every lowercase one. That is the point -- the order is
    // the same on every machine and in every locale, which a collation would not be.
    CHECK(l.rows[0].name == "Alpha");
    CHECK(l.rows[1].name == "zeta");
    CHECK(l.rows[2].name == "Aardvark.txt");
    CHECK(l.rows[3].name == "beta.cpp");
}

TEST_CASE("EDIT-1: a listing shows what is there -- dotfiles and build trees included") {
    TempDir dir("hidden");
    put_file(dir.path() / ".gitignore", "x");
    std::filesystem::create_directory(dir.path() / ".git");
    std::filesystem::create_directory(dir.path() / "build-workspace");
    put_file(dir.path() / "main.cpp", "int main(){}");
    const Listing l = enumerate_directory(dir.path().generic_string());
    REQUIRE(l.known);
    const auto has = [&](const char* name) {
        for (const FileRow& r : l.rows) {
            if (r.name == name) {
                return true;
            }
        }
        return false;
    };
    // A BROWSER THAT HID REAL ENTRIES WOULD BE LYING ABOUT THE PROJECT, and the entries
    // most worth hiding are exactly the ones a maker most often needs to see: the
    // generated workspace a build wrote, and the dot-files that decide what the project
    // even is.
    CHECK(has(".gitignore"));
    CHECK(has(".git"));
    CHECK(has("build-workspace"));
    CHECK(has("main.cpp"));
    CHECK(l.rows.size() == 4);
}

TEST_CASE("EDIT-1: a name outside printable ASCII keeps its row, marked, and cannot be opened") {
    TempDir dir("names");
    put_file(dir.path() / "plain.cpp", "x");
    // A name this application's narrow path custody cannot carry on both platforms.
    const std::string wide = "caf\xc3\xa9.cpp"; // UTF-8 e-acute
    put_file(dir.path() / std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(wide.data()), wide.size())), "x");
    const Listing l = enumerate_directory(dir.path().generic_string());
    REQUIRE(l.known);
    REQUIRE(l.rows.size() == 2);
    const FileRow* odd = nullptr;
    const FileRow* ok = nullptr;
    for (const FileRow& r : l.rows) {
        (r.openable ? ok : odd) = &r;
    }
    REQUIRE(ok != nullptr);
    REQUIRE(odd != nullptr);
    CHECK(ok->name == "plain.cpp");
    // THE ROW EXISTS, and its projection MARKS the loss at the position it happened
    // rather than tidying it away -- and the projection is never the identity.
    CHECK(odd->name != shown_name(odd->name));
    CHECK(shown_name(odd->name).find('?') != std::string::npos);
    CHECK_FALSE(printable_ascii_name(odd->name));
}

TEST_CASE("EDIT-1: a bound claims what it read, and never a total it never reached") {
    TempDir dir("bound");
    for (std::size_t i = 0; i < kMaxListedEntries + 25; ++i) {
        put_file(dir.path() / ("f" + std::to_string(i) + ".txt"), "x");
    }
    const Listing l = enumerate_directory(dir.path().generic_string());
    REQUIRE(l.known);
    CHECK(l.bounded);
    CHECK(l.rows.size() == kMaxListedEntries);
    FilesPane pane;
    pane.listing = l;
    const std::string head = files_header(pane, false);
    // QR-4: "stopped counting" is the honest sentence. A fraction would be a claim about a
    // total this walk never reached.
    CHECK(head.find("stopped counting") != std::string::npos);
    CHECK(head.find(std::to_string(kMaxListedEntries + 25)) == std::string::npos);
}

TEST_CASE("EDIT-1: a directory that cannot be listed is a refusal, not an empty listing") {
    const Listing gone = enumerate_directory("/definitely/not/a/directory/here");
    CHECK_FALSE(gone.known);
    CHECK_FALSE(gone.refusal.empty());
    CHECK(gone.rows.empty());
    // AN ABSENT PROJECT IS ITS OWN REFUSAL and is never resolved into some other place.
    const Listing none = enumerate_directory("");
    CHECK_FALSE(none.known);
    CHECK(none.rows.empty());
}

TEST_CASE("EDIT-1: the current directory is the root plus the names entered, and nothing else") {
    const std::vector<std::string> stack{"src", "deep"};
    CHECK(current_dir("/p/root", stack) == "/p/root/src/deep");
    CHECK(relative_dir(stack) == "src/deep");
    CHECK(relative_dir({}).empty());
    // NO ROOT, NO PATH. The stack alone denotes nothing, which is why an absent project
    // refuses rather than browsing something relative to wherever the process stands.
    CHECK(current_dir("", stack).empty());
}

// ============================================================================
// Tier 2 — THE PANE, ON A REAL BUS
// ============================================================================

TEST_CASE("EDIT-1: the pane lists the project it was launched into") {
    FilesRig r("open");
    std::filesystem::create_directory(r.root / "src");
    put_file(r.root / "main.cpp", "int main(){}\n");
    r.open();
    REQUIRE(r.listing().known);
    const std::vector<std::string> want{"src", "main.cpp"};
    CHECK(r.names() == want);
    // AND IT IS THE PROJECT ROOT, not the directory this binary happens to live in.
    CHECK(r.shown().find("main.cpp") != std::string::npos);
}

TEST_CASE("EDIT-1: with no project the pane refuses in words and guesses nothing") {
    FilesRig r("noroot", /*with_project=*/false);
    r.open();
    CHECK_FALSE(r.listing().known);
    CHECK_FALSE(r.listing().refusal.empty());
    CHECK(r.listing().rows.empty());
    // AND IT DOES NOT TAKE THE KEYS. A pane with nothing to navigate resolves to command
    // mode, exactly as an Editor with no document does -- so the maker's next gesture
    // means what it means everywhere else instead of vanishing.
    r.press_body(0);
    CHECK(keyboard_context(r.session()) == KeyContext::kCommand);
    CHECK(r.shown().find("no project directory") != std::string::npos);
}

TEST_CASE("EDIT-1: entering a directory walks in, and parent walks back to where you were") {
    FilesRig r("walk");
    std::filesystem::create_directory(r.root / "src");
    std::filesystem::create_directory(r.root / "docs");
    put_file(r.root / "src" / "a.cpp", "int a;\n");
    r.open();
    r.press_body(0); // select `docs` -- the first row, directories first
    CHECK(r.at_cursor() == "docs");
    CHECK(keyboard_context(r.session()) == KeyContext::kFiles);
    r.t.key(input::scan::kDown);
    CHECK(r.at_cursor() == "src");
    r.t.key(input::scan::kReturn);
    const std::vector<std::string> in_src{"src"};
    CHECK(r.pane().entered == in_src);
    const std::vector<std::string> only_a{"a.cpp"};
    CHECK(r.names() == only_a);
    CHECK(r.notice() == "in src");
    // GOING UP PUTS THE MAKER BACK ON THE ROW THEY CAME FROM, which is the one refresh
    // that does not send the cursor home: the answer to "where was I" exists here.
    r.t.key(input::scan::kBackspace);
    CHECK(r.pane().entered.empty());
    CHECK(r.at_cursor() == "src");
    CHECK(r.notice() == "in the project root");
}

TEST_CASE("EDIT-1: the root boundary is the empty stack -- there is nothing to go up to") {
    FilesRig r("root");
    put_file(r.root / "a.cpp", "int a;\n");
    r.open();
    r.press_body(0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    r.t.key(input::scan::kBackspace);
    CHECK(r.pane().entered.empty());
    CHECK(r.notice().find("does not go above it") != std::string::npos);
    // AND NO ROW SAYS `..`, so there is nothing to press that would construct one.
    for (const std::string& name : r.names()) {
        CHECK(name != "..");
        CHECK(name != ".");
    }
}

TEST_CASE("EDIT-1: a linked directory is shown and refuses to be entered") {
    FilesRig r("link");
    const std::filesystem::path outside = r.root / "outside";
    std::filesystem::create_directory(outside);
    put_file(outside / "secret.cpp", "int s;\n");
    std::filesystem::create_directory(r.root / "project");
    std::error_code ec;
    std::filesystem::create_directory_symlink(outside, r.root / "project" / "away", ec);
    if (ec) {
        // A PLATFORM THAT WILL NOT MAKE ONE FOR THIS PROCESS (Windows without the
        // privilege) cannot answer this question, and a case that quietly passed there
        // would be claiming a boundary nobody imposed.
        MESSAGE("this platform refused to create a directory symlink: ", ec.message());
        return;
    }
    r.t.host.project_dir = (r.root / "project").generic_string();
    r.open();
    REQUIRE(r.listing().rows.size() == 1);
    const FileRow& row = r.listing().rows.front();
    CHECK(row.name == "away");
    CHECK(row.directory);
    CHECK(row.linked); // the row is SHOWN -- hiding a real entry is the other way to be wrong
    r.press_body(0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    r.t.key(input::scan::kReturn);
    CHECK(r.pane().entered.empty()); // it did not travel
    CHECK(r.notice().find("does not follow links out of it") != std::string::npos);
}

TEST_CASE("EDIT-1: opening a row hands the path to the ONE editor door") {
    FilesRig r("door");
    std::filesystem::create_directory(r.root / "src");
    put_file(r.root / "src" / "hello.cpp", "one\ntwo\n");
    r.open();
    r.press_body(0);
    r.t.key(input::scan::kReturn); // into `src`
    REQUIRE(r.at_cursor() == "hello.cpp");
    r.t.key(input::scan::kReturn); // open it
    const EditorState& e = r.session().editor;
    CHECK(e.open_document());
    // THE IDENTITY IS THE NORMALIZED ABSOLUTE SPELLING, derived from root + stack + name.
    CHECK(e.path == (r.root / "src" / "hello.cpp").lexically_normal().generic_string());
    CHECK(e.buffer.line(0) == "one");
    CHECK(r.session().panels.keyboard == panel::kEditor);
    CHECK(keyboard_context(r.session()) == KeyContext::kEditor);
    CHECK(r.notice() == "editing " + e.path);
}

TEST_CASE("EDIT-1: the browser does not judge contents -- the editor's door does") {
    FilesRig r("content");
    // A file whose BYTES the editor refuses. The browser has no opinion about it: it is a
    // row, it is openable as a PATH, and the refusal that arrives speaks the editor's
    // vocabulary rather than a file-type list this pane does not have.
    put_file(r.root / "picture.png", std::string("\x89PNG\r\n\x1a\n", 8));
    r.open();
    const std::vector<std::string> only_png{"picture.png"};
    REQUIRE(r.names() == only_png);
    CHECK(r.listing().rows.front().openable); // the PATH is fine
    r.press_body(0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    r.t.key(input::scan::kReturn);
    CHECK_FALSE(r.session().editor.open_document());
    CHECK(r.notice().find("picture.png") != std::string::npos);
    CHECK(r.session().notice_is_bad);
}

TEST_CASE("EDIT-1: a name the path custody cannot carry refuses at the browser, in its words") {
    FilesRig r("badname");
    const std::string wide = "caf\xc3\xa9.cpp";
    put_file(r.root / std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(wide.data()), wide.size())), "int x;\n");
    r.open();
    REQUIRE(r.listing().rows.size() == 1);
    REQUIRE_FALSE(r.listing().rows.front().openable);
    r.press_body(0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    r.t.key(input::scan::kReturn);
    CHECK_FALSE(r.session().editor.open_document());
    // THE BROWSER'S OWN SENTENCE, about the PATH and not about the contents.
    CHECK(r.notice().find("cannot carry in a path") != std::string::npos);
}

TEST_CASE("EDIT-1: a dirty document is never replaced through the browser") {
    FilesRig r("dirty");
    put_file(r.root / "a.cpp", "alpha\n");
    put_file(r.root / "b.cpp", "beta\n");
    r.open();
    r.press_body(0);
    REQUIRE(r.at_cursor() == "a.cpp");
    r.t.key(input::scan::kReturn);
    REQUIRE(r.session().editor.open_document());
    const std::string opened = r.session().editor.path;
    const std::uint64_t epoch = r.session().editor.doc_epoch;
    r.t.text("X"); // the editor holds the keys: type into the source
    REQUIRE(r.session().editor.dirty());
    // BACK TO THE BROWSER, and every browser gesture is harmless: selecting and navigating
    // touch nothing, and the one gesture that WOULD replace the document is refused.
    r.press_body(1);
    CHECK(r.at_cursor() == "b.cpp");
    CHECK(r.session().editor.dirty());
    r.press_body(1); // the selected row, in a pane that now holds the keys: activate
    CHECK(r.session().editor.path == opened);
    CHECK(r.session().editor.doc_epoch == epoch);
    CHECK(r.session().editor.dirty());
    CHECK(r.notice().find("unsaved changes") != std::string::npos);
    CHECK(r.session().editor.buffer.line(0).find('X') != std::string::npos);
}

TEST_CASE("EDIT-1: the first press into a cold pane selects and never activates") {
    FilesRig r("firstpress");
    put_file(r.root / "a.cpp", "alpha\n");
    put_file(r.root / "b.cpp", "beta\n");
    r.open();
    // The cursor starts on row 0, so a press on row 0 in a pane that does not hold the
    // keys is EXACTLY the collision this guard exists for: the press that points the
    // keyboard here must not also be the press that opens a file.
    REQUIRE(r.pane().cursor == 0);
    REQUIRE_FALSE(files_has_keyboard(r.session()));
    r.press_body(0);
    CHECK_FALSE(r.session().editor.open_document());
    CHECK(files_has_keyboard(r.session()));
    CHECK(r.pane().cursor == 0);
    // The SECOND press on that same row is a maker acting in a pane they are already in.
    r.press_body(0);
    CHECK(r.session().editor.open_document());
    CHECK(r.session().editor.path ==
          (r.root / "a.cpp").lexically_normal().generic_string());
}

TEST_CASE("EDIT-1: a press selects the row the paint put under the pointer") {
    FilesRig r("inverse");
    for (int i = 0; i < 5; ++i) {
        put_file(r.root / ("f" + std::to_string(i) + ".cpp"), "x\n");
    }
    r.open();
    r.press_body(0);
    REQUIRE(files_has_keyboard(r.session()));
    // THE INVERSE AGREES WITH THE PAINT, asserted by reading the painted row back rather
    // than by trusting the arithmetic twice.
    for (std::int64_t row = 0; row < 4; ++row) {
        r.press_body(row);
        const ui::Rect c = r.files_cells();
        const std::string painted =
            label_at(r.t.canvases.back(), c.x, c.y + kFilesHeaderRows + row);
        INFO("body row ", row, " painted as: ", painted);
        CHECK(painted.find(r.at_cursor()) != std::string::npos);
    }
}

TEST_CASE("EDIT-1: a press below the last row invents no entry") {
    FilesRig r("blank");
    put_file(r.root / "only.cpp", "x\n");
    r.open();
    r.press_body(0);
    REQUIRE(files_has_keyboard(r.session()));
    const std::size_t was = r.pane().cursor;
    r.press_body(6); // well past the one row this listing has
    CHECK(r.pane().cursor == was);
    CHECK_FALSE(r.session().editor.open_document());
}

TEST_CASE("EDIT-1: the wheel moves the browser's cursor and leaves the editor's alone") {
    FilesRig r("wheel");
    for (int i = 0; i < 40; ++i) {
        put_file(r.root / ("f" + std::to_string(i) + ".cpp"), "x\n");
    }
    r.open();
    r.press_body(0);
    REQUIRE(files_has_keyboard(r.session()));
    const std::string top = r.at_cursor();
    r.wheel_body(-2.0); // away from the top
    CHECK(r.at_cursor() != top);
    const std::string moved = r.at_cursor();
    r.wheel_body(2.0);
    CHECK(r.at_cursor() != moved);
    // AND IT IS BOUNDED AT BOTH ENDS: spinning past the top stops at the top rather than
    // wrapping or running off.
    r.wheel_body(50.0);
    CHECK(r.at_cursor() == top);
}

TEST_CASE("EDIT-1: the browser's keys do not reach command mode, and command mode's do not reach it") {
    FilesRig r("keys");
    std::filesystem::create_directory(r.root / "src");
    put_file(r.root / "a.cpp", "x\n");
    r.open();
    const std::size_t objects_before = r.session().rows.size();
    r.press_body(0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    // `r` is refresh HERE and is not command mode's anything; `n` is command mode's and is
    // unbound here, so it does nothing rather than creating an object behind the pane.
    r.t.key(input::scan::kR);
    r.t.text("r");
    CHECK(r.notice().find("listed the project root again") != std::string::npos);
    r.t.key(input::scan::kN);
    r.t.text("n");
    CHECK(r.session().rows.size() == objects_before);
    // Pressing onto a pane that does not take keys hands them back, whole.
    r.to_command();
    CHECK(keyboard_context(r.session()) == KeyContext::kCommand);
}

TEST_CASE("EDIT-1: the listing is a snapshot -- painting does not re-walk the directory") {
    FilesRig r("snapshot");
    put_file(r.root / "a.cpp", "x\n");
    r.open();
    const std::vector<std::string> only_a{"a.cpp"};
    REQUIRE(r.names() == only_a);
    // A file appears behind Workshop's back. NOTHING about painting may notice it: a
    // listing recomputed per paint would be an OS walk per frame, which is the one cost
    // this pane's whole refresh design exists to refuse.
    put_file(r.root / "b.cpp", "y\n");
    for (int i = 0; i < 3; ++i) {
        r.resize_screen(40 + i); // three more paints, each through the real repaint path
    }
    const std::vector<std::string> still_a{"a.cpp"};
    CHECK(r.names() == still_a);
    // The maker asks, and now it is there.
    r.press_body(0);
    r.t.key(input::scan::kR);
    const std::vector<std::string> both{"a.cpp", "b.cpp"};
    CHECK(r.names() == both);
}

TEST_CASE("EDIT-1: reopening the pane takes a fresh listing") {
    FilesRig r("reopen");
    put_file(r.root / "a.cpp", "x\n");
    r.open();
    REQUIRE(r.names().size() == 1);
    put_file(r.root / "b.cpp", "y\n");
    pick(r.t, panel::kProjectFiles); // the same gesture removes it
    REQUIRE_FALSE(r.session().panels.has(panel::kProjectFiles));
    r.open();
    const std::vector<std::string> expected_1{"a.cpp", "b.cpp"};
    CHECK(r.names() == expected_1);
}

TEST_CASE("EDIT-1: nothing about the browser is written to a maker's files") {
    FilesRig r("persist");
    std::filesystem::create_directory(r.root / "src");
    r.open();
    r.press_body(0);
    r.t.key(input::scan::kReturn);
    const std::vector<std::string> in_src{"src"};
    REQUIRE(r.pane().entered == in_src);
    // THE PANE RIDES THE SETUP LIKE EVERY PANE. What a setup names is the pane's presence
    // and geometry -- and the browser's own place inside it is work in progress, which
    // WUX-0 keeps out of every durable file.
    const Setup saved = r.session().setup.active;
    bool named = false;
    for (const SetupPane& row : saved.panes) {
        if (row.ref.pane == pane_key::kProjectFiles) {
            named = true;
        }
    }
    CHECK(named);
    const std::string text = setup_persist::to_text(saved);
    CHECK(text.find("src") == std::string::npos);
    CHECK(text.find("cursor") == std::string::npos);
}

// ============================================================================
// Tier 3 — THE TWO REFERRERS, AND THE ONE DOOR THEY SHARE
// ============================================================================

TEST_CASE("EDIT-1: a finished build gives the browser a fresh listing, and nothing else does") {
    FilesRig r("build");
    put_file(r.root / "a.cpp", "int a;\n");
    ToolSeat* tool = mount_tool(r.t, "one");
    open_builder(r.t);
    r.open();
    const std::vector<std::string> only_a{"a.cpp"};
    REQUIRE(r.names() == only_a);
    REQUIRE(r.at_cursor() == "a.cpp");

    // A BUILD BEGINS, and while it runs nothing is rescanned: an intermediate status is
    // not a finished build and must not cost a directory walk.
    tool->answers_builds = false;
    r.t.key(input::scan::kB);
    r.t.text("b");
    REQUIRE(r.session().panels.builder.awaiting);
    put_file(r.root / "made-by-the-build.so", "artifact\n");
    tool->next.outcome = zengine::builder::outcome::kRunning;
    r.t.publish(loom::to_value(tool->next));
    CHECK(r.names() == only_a);

    // ...AND WHEN IT ENDS, THE LISTING IS TRUTHFUL AGAIN. No watcher, no timer, no poll,
    // and not one byte added to the Builder's protocol: the message was already arriving.
    tool->next.outcome = zengine::builder::outcome::kSucceeded;
    tool->next.status = 0;
    r.t.publish(loom::to_value(tool->next));
    const std::vector<std::string> after{"a.cpp", "made-by-the-build.so"};
    CHECK(r.names() == after);
    // AND THE MAKER DID NOT MOVE. They did not ask for this refresh, so it may not take
    // their place away from them.
    CHECK(r.at_cursor() == "a.cpp");
}

TEST_CASE("EDIT-1: Builder and the browser open ONE document, however the path is spelled") {
    FilesRig r("shared");
    put_file(r.root / "hello.cpp", "one\ntwo\n");
    // THE RECIPE SPELLS IT THE AWKWARD WAY a maker would: relative, with a `./` in front.
    // The host resolves a relative source against the project before anything sees it, and
    // the door normalizes whatever it is handed -- so neither spelling can mint a document
    // of its own.
    ToolSeat* tool = mount_tool(r.t, "hello");
    (void)tool;
    const std::string resolved =
        persist::resolved_against(r.t.host.project_dir, "./hello.cpp");
    r.t.host.recipe_source = [resolved](const std::string& id) {
        HostContext::RecipeSource out;
        if (id == "hello") {
            out.known = true;
            out.kind = "single_source";
            out.source = resolved;
        }
        return out;
    };
    open_builder(r.t);
    r.open();

    // THROUGH THE BROWSER FIRST.
    r.press_body(0);
    REQUIRE(r.at_cursor() == "hello.cpp");
    r.press_body(0);
    REQUIRE(r.session().editor.open_document());
    const std::string by_browser = r.session().editor.path;
    CHECK(by_browser == (r.root / "hello.cpp").lexically_normal().generic_string());

    // Put the caret somewhere and leave a mark on the view, so "the same document" is a
    // claim about STATE and not merely about a matching string.
    r.t.key(input::scan::kDown);
    const std::size_t caret_row = r.session().editor.buffer.caret_row();
    const std::uint64_t epoch = r.session().editor.doc_epoch;

    // AND NOW THROUGH BUILDER'S OWN GESTURE, from command mode.
    r.to_command();
    r.t.key(input::scan::kE);
    r.t.text("e");
    CHECK(r.session().editor.path == by_browser);
    // THE SAME DOCUMENT, REVEALED AND NOT REOPENED: the epoch did not move and the caret
    // is where the maker left it.
    CHECK(r.session().editor.doc_epoch == epoch);
    CHECK(r.session().editor.buffer.caret_row() == caret_row);
    CHECK(r.session().panels.keyboard == panel::kEditor);
}

TEST_CASE("EDIT-1: equivalent spellings of one file are one document") {
    FilesRig r("spelling");
    put_file(r.root / "a.cpp", "alpha\n");
    const std::string plain = persist::resolved_against(r.t.host.project_dir, "a.cpp");
    const std::string dotted = persist::resolved_against(r.t.host.project_dir, "./a.cpp");
    const std::string winding =
        persist::resolved_against(r.t.host.project_dir, "sub/../a.cpp");
    // ONE NORMALIZATION, APPLIED TO EVERY ENTRANT, is what makes these one identity --
    // and identity is compared as a STRING, so without it they would be three documents.
    CHECK(plain == dotted);
    CHECK(plain == winding);
    CHECK(plain == (r.root / "a.cpp").lexically_normal().generic_string());
    // AN ABSOLUTE AUTHORED SPELLING KEEPS ITS OWN MEANING and is not joined to anything.
    const std::string absolute =
        persist::resolved_against(r.t.host.project_dir, (r.root / "a.cpp").generic_string());
    CHECK(absolute == plain);
    // AND WITH NO PROJECT THERE IS NO GUESS: a relative spelling comes back as authored,
    // to be refused downstream rather than joined to somewhere this process happened to be.
    CHECK(persist::resolved_against("", "a.cpp") == "a.cpp");
}

// ============================================================================
// Tier 4 — THE TWO-BASE FALSIFIER: one relative source, one file
// ============================================================================

TEST_CASE("EDIT-1: a relative recipe source is the PROJECT's file, in the editor and in the build") {
    // THE CONDITION THAT MADE THE OLD CLAIM FALSE, ARRANGED ON PURPOSE. Two directories
    // both hold `src/example.cpp`, with different bytes: the project the maker launched
    // into, and the generated workspace the build is written in. Before this phase the
    // editor and the runner's preflight resolved the authored spelling against the
    // PROCESS's working directory while CMake resolved the very same string against the
    // WORKSPACE -- so a recipe naming `src/example.cpp` named two files and the
    // documentation's promise that they were one was simply untrue for relative spellings.
    //
    // A GREEN BUILD THAT NEVER ARRANGED THIS PROVES NOTHING, which is why the decoy exists
    // and why it holds different bytes.
    TempDir dir("twobase");
    const std::filesystem::path install = dir.path() / "install";
    const std::filesystem::path root = dir.path() / "project";
    const std::filesystem::path workspace = install / "build-workspace" / "one";
    std::filesystem::create_directories(root / "src");
    std::filesystem::create_directories(workspace / "src");
    put_file(root / "src" / "example.cpp", "// the project's own file\n");
    put_file(workspace / "src" / "example.cpp", "// the decoy in the workspace\n");

    zengine::builder::SingleSourceRecipe one;
    one.source = "src/example.cpp"; // relative, exactly as a maker writes it
    one.workspace = workspace.generic_string();
    zengine::builder::Recipe authored;
    authored.id = "one";
    authored.artifact = "one";
    authored.single_source = one;

    const std::string resolved =
        (root / "src" / "example.cpp").lexically_normal().generic_string();

    std::vector<zengine::builder::Recipe> all{authored};
    recipe_persist::complete_recipes(all, install.generic_string(), root.generic_string());
    REQUIRE(all.size() == 1);
    REQUIRE(all[0].single_source.has_value());
    CHECK(all[0].single_source->source == resolved);

    // THE GENERATED PROJECT COMPILES THE PROJECT'S FILE...
    const std::string project = zengine::builder::generated_project(all[0]);
    CHECK(project.find(resolved) != std::string::npos);
    // ...AND NO LONGER CARRIES THE AMBIGUOUS SPELLING AT ALL. This is the assertion that
    // actually kills the defect: while `add_library` held a relative string, CMake -- not
    // Zengine -- decided what it meant, and it decided the workspace.
    CHECK(project.find("\"src/example.cpp\"") == std::string::npos);

    // THE PREFLIGHT CHECKS THE PROJECT'S FILE...
    const zengine::builder::PreparedBuild ready =
        zengine::builder::prepare(all[0], "/usr/bin/cmake");
    CHECK(ready.ok);

    // ...AND HERE IS THE SHARP HALF. Take the project's file away and leave the decoy
    // standing. A build that still found "something" would be a build reading the
    // workspace copy; this one refuses, and names the file the project actually meant.
    std::filesystem::remove(root / "src" / "example.cpp");
    std::vector<zengine::builder::Recipe> again{authored};
    recipe_persist::complete_recipes(again, install.generic_string(), root.generic_string());
    const zengine::builder::PreparedBuild refused =
        zengine::builder::prepare(again[0], "/usr/bin/cmake");
    CHECK_FALSE(refused.ok);
    CHECK(refused.trouble.find(resolved) != std::string::npos);
}

TEST_CASE("EDIT-1: the editor opens the file that recipe's build would compile") {
    // THE OTHER END OF THE SAME SENTENCE, driven through the real weave: the maker presses
    // Builder's edit-source gesture on a recipe whose source is spelled relatively, and the
    // BYTES they get are the project's -- not the workspace decoy's.
    FilesRig r("editbuild");
    const std::filesystem::path workspace = r.root / "build-workspace" / "one";
    std::filesystem::create_directories(r.root / "src");
    std::filesystem::create_directories(workspace / "src");
    put_file(r.root / "src" / "example.cpp", "the project\n");
    put_file(workspace / "src" / "example.cpp", "the decoy\n");

    zengine::builder::SingleSourceRecipe one;
    one.source = "src/example.cpp";
    one.workspace = workspace.generic_string();
    zengine::builder::Recipe authored;
    authored.id = "one";
    authored.artifact = "one";
    authored.single_source = one;
    std::vector<zengine::builder::Recipe> all{authored};
    recipe_persist::complete_recipes(all, r.root.generic_string(), r.t.host.project_dir);

    // THE HOST ANSWERS OVER THE COMPLETED CATALOG, exactly as the real host does -- which
    // is the whole mechanism: one completion, and every reader downstream reads it.
    const std::string completed = all[0].single_source->source;
    r.t.host.recipe_source = [completed](const std::string& id) {
        HostContext::RecipeSource out;
        if (id == "one") {
            out.known = true;
            out.kind = "single_source";
            out.source = completed;
        }
        return out;
    };
    ToolSeat* tool = mount_tool(r.t, "one");
    (void)tool;
    open_builder(r.t);
    r.t.key(input::scan::kE);
    r.t.text("e");
    REQUIRE(r.session().editor.open_document());
    CHECK(r.session().editor.path ==
          (r.root / "src" / "example.cpp").lexically_normal().generic_string());
    CHECK(r.session().editor.buffer.line(0) == "the project");
    // AND THE GENERATED PROJECT, FROM THE SAME VALUE, NAMES THE SAME FILE.
    CHECK(zengine::builder::generated_project(all[0]).find(r.session().editor.path) !=
          std::string::npos);
}

TEST_CASE("EDIT-1: the door normalizes what a referrer hands it, not what a browser happened to build") {
    // MUTATION-DRIVEN. Dropping the normalization inside `open_source` left this suite green,
    // because both callers happened to hand it an already-absolute spelling -- the browser
    // builds one from the root, and a completed recipe carries one. So nothing measured the
    // DOOR's own promise. This does: the referrer hands over exactly what a maker might have
    // authored, relative and with a `./` in it, and the door is the only thing that could
    // turn it into the identity the browser also arrives at.
    FilesRig r("doornorm");
    put_file(r.root / "a.cpp", "alpha\n");
    ToolSeat* tool = mount_tool(r.t, "raw");
    (void)tool;
    r.t.host.recipe_source = [](const std::string& id) {
        HostContext::RecipeSource out;
        if (id == "raw") {
            out.known = true;
            out.kind = "single_source";
            out.source = "./a.cpp"; // AUTHORED, relative, not completed by anybody
        }
        return out;
    };
    open_builder(r.t);
    r.t.key(input::scan::kE);
    r.t.text("e");
    REQUIRE(r.session().editor.open_document());
    const std::string want = (r.root / "a.cpp").lexically_normal().generic_string();
    CHECK(r.session().editor.path == want);

    // AND THE BROWSER LANDS ON THAT SAME DOCUMENT, revealing rather than replacing -- which
    // is the property two referrers spelling one file differently would otherwise break.
    const std::uint64_t epoch = r.session().editor.doc_epoch;
    r.resize_screen(60); // room for a third stacked pane beside the Builder and the Editor
    r.to_command();      // the editor holds the keys, so `p` would type rather than pick
    r.open();
    REQUIRE(r.session().panels.has(panel::kProjectFiles));
    r.press_body(0);
    REQUIRE(files_has_keyboard(r.session()));
    r.press_body(0);
    CHECK(r.session().editor.path == want);
    CHECK(r.session().editor.doc_epoch == epoch);
}

TEST_CASE("EDIT-1: the press inverse counts the omission marker the paint spent a row on") {
    // MUTATION-DRIVEN. The inverse's marker offset survived a mutation because every earlier
    // case had a listing that FIT, so `list_window` never spent a row on `... n earlier` and
    // the offset was always zero. A scrolled window is where the two arithmetics can disagree.
    FilesRig r("scrolled");
    for (int i = 0; i < 40; ++i) {
        put_file(r.root / ("f" + std::to_string(i < 10 ? 0 : 1) + std::to_string(i) + ".cpp"),
                 "x\n");
    }
    r.open();
    r.press_body(0);
    REQUIRE(files_has_keyboard(r.session()));
    // Walk far enough down that the window can no longer start at the top.
    for (int i = 0; i < 20; ++i) {
        r.t.key(input::scan::kDown);
    }
    const ExternalBodyPlace body = files_body(r.session(), screen_of(r.session()));
    REQUIRE(body.present);
    const ListWindow win =
        list_window(r.listing().rows.size(), r.pane().cursor, static_cast<std::size_t>(body.rows));
    REQUIRE(win.before > 0); // the paint really is spending a row on the marker
    const ui::Rect c = r.files_cells();
    CHECK(label_at(r.t.canvases.back(), c.x, c.y + kFilesHeaderRows).find("earlier") !=
          std::string::npos);
    // EVERY BODY ROW: what a press selects is what that row PAINTS -- marker row included,
    // which must name no entry at all.
    for (std::int64_t row = 0; row < body.rows; ++row) {
        const std::string painted = label_at(r.t.canvases.back(), c.x, c.y + kFilesHeaderRows + row);
        const std::size_t was = r.pane().cursor;
        r.press_body(row);
        INFO("body row ", row, " painted as: ", painted);
        if (painted.find("earlier") != std::string::npos ||
            painted.find(" more") != std::string::npos) {
            CHECK(r.pane().cursor == was); // a marker names no entry and invents none
        } else if (!painted.empty()) {
            CHECK(painted.find(r.at_cursor()) != std::string::npos);
        }
    }
}

TEST_CASE("EDIT-1: the press inverse answers the graphical medium's geometry too") {
    // THE PANE IS PROVEN ON A REAL WINDOW BY A PHOTOGRAPH, and a photograph cannot press.
    // This is the gesture half of the same claim at the same METRICS: a medium whose text is
    // real type rather than cells resolves a different number of rows into the same
    // rectangle, and the press inverse must follow the paint there exactly as it does in
    // cells -- one resolution (`external_body_place`), asked by both.
    FilesRig r("graphical");
    for (int i = 0; i < 12; ++i) {
        put_file(r.root / ("f" + std::to_string(i) + ".cpp"), "x\n");
    }
    r.open();
    // A GRAPHICAL SCREEN: the same extent, with a medium that has told Workshop how big one
    // character of its own type is (HD-1) -- which is the whole difference.
    r.t.publish(loom::to_value(surface::SurfaceExtent{78, 40, 8, 18}));
    const Screen sc = screen_of(r.session());
    REQUIRE(sc.text_advance_px == 8);
    const ExternalBodyPlace body = files_body(r.session(), sc);
    REQUIRE(body.present);
    r.press_body(0);
    REQUIRE(files_has_keyboard(r.session()));
    for (std::int64_t row = 0; row < body.rows; ++row) {
        const std::size_t was = r.pane().cursor;
        r.press_body(row);
        std::size_t which = 0;
        const bool named = files_row_of_body_row(r.pane(), body.rows, row, which);
        INFO("graphical body row ", row, ", names an entry: ", named);
        if (named) {
            CHECK(r.pane().cursor == which);
        } else {
            CHECK(r.pane().cursor == was); // a marker row invents nothing here either
        }
    }
}
