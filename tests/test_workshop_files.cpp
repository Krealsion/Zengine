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
// ...AND WHO HOLDS THE COMPLETED ANSWER (PROJ-0), and how a catalog file BECOMES that
// answer (PROJ-1). One owner per running Workshop, read by every recipe consumer; the two
// tiers at the end of this file are its own.
#include "workshop/recipes.hpp"

// THE REAL BUILDER TOOL, because "every consumer moved to the new catalog" is a claim
// about the tool that actually reads the owner. The stand-in next door publishes a catalog
// of its own and could not tell a live replacement from a rig assignment.
#include "builder/weave.hpp"

// WHETHER A FILESYSTEM PATH CAN BE SAID AT ALL (QR-12). The browser's names and the host's
// launch directory are the two paths this application takes from the OS, and the boundary
// they now share is asserted here directly rather than only through its consumers.
#include "workshop/path_admission.hpp"

// `std::system`, for the one Windows arrangement the standard library cannot make: a
// directory JUNCTION. `mklink /J` is how a person makes one and needs no privilege.
#include <cstdlib>

// ...AND THE ONE PLATFORM CALL THIS SUITE MAKES. A filename holding ill-formed UTF-16 is
// the measured condition the admission boundary exists for, and only `CreateFileW` will
// create one; every case that arranges it still runs on both families, because what each
// family can put in a directory is arranged behind one helper and the LAW being asserted is
// the same law (see `put_unsayable_entry`).
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

/// PUT THE HARDEST NAME THIS PLATFORM CAN HOLD IN `dir`, and say whether it went in.
///
/// THE HARDNESS IS A DIFFERENT HARDNESS ON EACH FAMILY, AND THAT IS THE POINT. Windows/NTFS
/// accepts ILL-FORMED UTF-16 -- an unpaired surrogate, which `CreateFileW` takes and any
/// other program can therefore leave in a directory a maker walks into -- and that is the
/// MEASURED case where asking a path for its filename bytes THROWS. POSIX accepts arbitrary
/// BYTES, where the same ask is a passthrough and the name is inert for the ordinary
/// printable-ASCII reason instead.
///
/// So the ARRANGEMENT is per-platform and the LAW is not, and every case below runs on both
/// families rather than one family quietly selecting fewer cases than the other.
inline bool put_unsayable_entry(const std::filesystem::path& dir) {
#if defined(_WIN32)
    std::wstring name = (dir / "lone").wstring();
    name.push_back(static_cast<wchar_t>(0xD800)); // a HIGH surrogate with no low half
    name += L".txt";
    const HANDLE made = ::CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (made == INVALID_HANDLE_VALUE) {
        return false;
    }
    ::CloseHandle(made);
    return true;
#else
    put_file(dir / std::filesystem::path(std::string("lone\xff.txt")), "x");
    return true;
#endif
}

/// AN ABSOLUTE SPELLING THIS PLATFORM AGREES IS ONE.
///
/// ⚠ `/p/root` IS ABSOLUTE ON POSIX AND IS NOT ON WINDOWS -- it carries a root DIRECTORY and
/// no root NAME there, which is `std::filesystem`'s own answer and exactly the distinction
/// `admit_location` refuses on. A case that wrote the POSIX spelling on both families would
/// assert one law on one and a typo on the other, so the ARRANGEMENT is per-platform and the
/// LAW asserted over it is identical.
inline std::string abs_spelling(const std::string& tail) {
#if defined(_WIN32)
    return "C:" + tail;
#else
    return tail;
#endif
}

/// This platform's own filesystem root, spelled the way every path here is spelled.
inline std::string root_spelling() { return abs_spelling("/"); }

/// A DIRECTORY THAT LEAVES THE TREE, made the strongest way this platform allows.
///
/// POSIX gets an ordinary directory symlink. Windows tries one first and falls back to a
/// JUNCTION -- which is the case that actually matters there, because a junction needs no
/// privilege (a symbolic link does, and this session does not hold it), and because a junction
/// is the entry whose `is_symlink()` answers FALSE while it is still a directory that leaves
/// the tree. Returns false when the platform made neither, so a case can say so rather than
/// passing quietly on a boundary nobody arranged.
inline bool make_linked_directory(const std::filesystem::path& link,
                                  const std::filesystem::path& target) {
    std::error_code ec;
    std::filesystem::create_directory_symlink(target, link, ec);
    if (!ec) {
        return true;
    }
#if defined(_WIN32)
    // `mklink /J` is the ordinary way a person makes one, and it is the reparse point this
    // application's `linked` predicate was written against (EDIT-1 measured it).
    const std::string command = "cmd /c mklink /J \"" + link.string() + "\" \"" +
                                target.string() + "\" >nul 2>&1";
    if (std::system(command.c_str()) == 0) {
        std::error_code exists_ec;
        return std::filesystem::exists(link, exists_ec) && !exists_ec;
    }
#endif
    return false;
}

/// A DIRECTORY NAME OF THE SAME KIND, for the launch-capture case. On Windows it is spelled
/// with universal-character-names on purpose -- what these characters ARE is decided by the
/// C++ standard rather than by whatever encoding a compiler guesses this file is in -- and
/// nothing in a single-byte code page can hold them.
inline std::filesystem::path unsayable_dir_name() {
#if defined(_WIN32)
    return std::filesystem::path(std::wstring(L"caf\u00E9-\u65E5\u672C"));
#else
    return std::filesystem::path(std::string("caf\xc3\xa9-\xff"));
#endif
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

TEST_CASE("QR-12: an ordinary path and an ordinary name are carried exactly as they were") {
    TempDir dir("admit");
    // THE ADMISSION BOUNDARY CHANGES NOTHING IT CAN CARRY. Everything below this line is
    // what every friendly path in this application got before the boundary existed, and
    // the case is here so that making failure explicit cannot quietly re-spell success.
    const AdmittedPath carried = admit_path(dir.path());
    CHECK(carried.carried);
    CHECK(carried.spelling == dir.path().generic_string());

    const AdmittedName plain = admit_filename(std::filesystem::path("hello.cpp"));
    CHECK(plain.exact);
    CHECK(plain.name == "hello.cpp");
    CHECK(printable_ascii_name(plain.name));
}

TEST_CASE("QR-12: the launch capture is the working directory, when it can be said") {
    // THE CAPTURE IS ONE FUNCTION so that the thing proved is the thing `main` runs. This
    // lane's own working directory is an ordinary path, so the ordinary arm is what this
    // case pins; the arm where the platform refuses is Windows' own, below.
    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    REQUIRE_FALSE(ec);
    const AdmittedPath carried = admit_path(cwd);
    REQUIRE(carried.carried);
    std::string captured;
    REQUIRE_NOTHROW(captured = launch_project_dir());
    CHECK(captured == carried.spelling);
    CHECK_FALSE(captured.empty());
}

TEST_CASE("QR-12: a name this platform will not spell is one inert row, not the end of it") {
    TempDir dir("unsayable");
    put_file(dir.path() / "plain.cpp", "int x;\n");
    // A case that cannot arrange its own condition must say so rather than pass.
    REQUIRE(put_unsayable_entry(dir.path()));

    Listing l;
    // THE REPAIR, STATED AS THE FALSIFIER. On Windows this call THREW before the admission
    // boundary existed -- measured, out of the browser, with no handler above it.
    REQUIRE_NOTHROW(l = enumerate_directory(dir.path().generic_string()));
    REQUIRE(l.known);
    // NEITHER ROW WAS DROPPED. Hiding the hostile entry would make this browser lie about
    // what is in the directory; losing the neighbour would let one name cost the listing.
    REQUIRE(l.rows.size() == 2);

    const FileRow* ordinary = nullptr;
    const FileRow* unsayable = nullptr;
    for (const FileRow& r : l.rows) {
        (r.openable ? ordinary : unsayable) = &r;
    }
    REQUIRE(ordinary != nullptr);
    REQUIRE(unsayable != nullptr);
    // THE NEIGHBOUR IS UNTOUCHED -- exact bytes, still openable.
    CHECK(ordinary->name == "plain.cpp");
    // ...AND THE UNSAYABLE ROW IS VISIBLE, MARKED AND INERT, which is the law it joins
    // rather than a new one.
    CHECK_FALSE(unsayable->openable);
    CHECK(shown_name(unsayable->name).find('?') != std::string::npos);
#if defined(_WIN32)
    // ⚠ WINDOWS-ONLY, AND IT IS THE WHOLE REASON `exact` EXISTS. Here the platform refused
    // to hand over any bytes, so this row's name is a PROJECTION -- and the projection is
    // entirely printable ASCII. A byte test alone would call it openable and hand a door a
    // path that names a different file or no file.
    CHECK(printable_ascii_name(unsayable->name));
#endif
}

TEST_CASE("QR-12: a name this platform will not spell refuses at the browser's door") {
    FilesRig r("unsayable-live");
    REQUIRE(put_unsayable_entry(r.root));

    r.open();
    REQUIRE(r.listing().rows.size() == 1);
    REQUIRE_FALSE(r.listing().rows.front().openable);
    r.press_body(0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    r.t.key(input::scan::kReturn);
    // NO DOCUMENT, and the browser's EXISTING sentence about a path it cannot carry. The
    // new way of failing to say a name arrives at the refusal that already knows why.
    CHECK_FALSE(r.session().editor.open_document());
    CHECK(r.notice().find("cannot carry in a path") != std::string::npos);
}

TEST_CASE("QR-12: a launch directory this Workshop cannot say is an absence, not an exit") {
    TempDir dir("launch-dir");
    const std::filesystem::path standing = dir.path() / unsayable_dir_name();
    std::error_code ec;
    std::filesystem::create_directory(standing, ec);
    REQUIRE_FALSE(ec);

    // THE PROCESS'S OWN DIRECTORY IS RESTORED WHATEVER THIS CASE DOES -- including through
    // a failed REQUIRE, and before `TempDir` tries to remove the tree it is standing in.
    struct Standing {
        std::filesystem::path was;
        Standing() {
            std::error_code e;
            was = std::filesystem::current_path(e);
        }
        ~Standing() {
            std::error_code e;
            std::filesystem::current_path(was, e);
        }
    } restore;

    // ARRANGE THE DEFECT, DO NOT INJECT ONE BELOW IT. The process really stands there, and
    // the narrowing question is really put to the platform -- exactly the call the capture
    // used to make unguarded.
    std::filesystem::current_path(standing, ec);
    REQUIRE_FALSE(ec);
    bool platform_refuses = false;
    try {
        std::error_code where_ec;
        (void)std::filesystem::current_path(where_ec).generic_string();
    } catch (const std::exception&) {
        platform_refuses = true;
    }

    std::string captured;
    // THE REPAIR, STATED AS THE FALSIFIER: on Windows this composition THREW out of `main`.
    REQUIRE_NOTHROW(captured = launch_project_dir());
    if (platform_refuses) {
        // THE DESIGNED ABSENCE -- the same empty every consumer already refuses in words.
        // Nothing adjacent was substituted for the directory that could not be said.
        CHECK(captured.empty());
    } else {
        // The platform CAN say this name: POSIX, where narrowing is a byte passthrough, or
        // a Windows whose active code page carries it. Then the capture owes the ordinary
        // truth -- a hostile-looking name is not a reason to invent an absence either.
        std::error_code where_ec;
        CHECK(captured == std::filesystem::current_path(where_ec).generic_string());
        CHECK_FALSE(captured.empty());
    }
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
    const std::string head = files_header(pane, std::string(), false, 200);
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

TEST_CASE("PROJ-2: a location is one absolute spelling, admitted the same way every time") {
    // THE INVARIANT THE WHOLE REPRESENTATION RESTS ON: absolute, lexically normal, forward
    // separators -- and one function enforces it, so a seed, an enter, a parent and a mark
    // jump cannot each remember it differently.
    CHECK(admit_location(abs_spelling("/p/root/src/deep")) == abs_spelling("/p/root/src/deep"));
    CHECK(admit_location(abs_spelling("/p/root/src/../src/./deep")) ==
          abs_spelling("/p/root/src/deep"));
    // ⚠ ONE LOCATION, ONE SPELLING -- and `lexically_normal` alone does not give it. MEASURED
    // on both families: a path ending in `..` normalizes WITH a trailing separator, so the
    // same directory would arrive as two different byte strings and a mark would silently
    // stop being the place it is. A root keeps its separator, because there it IS the path.
    CHECK(admit_location(abs_spelling("/p/root/sub/..")) == abs_spelling("/p/root"));
    CHECK(admit_location(abs_spelling("/p/root/")) == abs_spelling("/p/root"));
    CHECK(admit_location(root_spelling()) == root_spelling());
    // A RELATIVE SPELLING IS NOT A LOCATION AT ALL, and is emphatically not re-based
    // against wherever this process happens to be standing.
    CHECK(admit_location("src/deep").empty());
    CHECK(admit_location("").empty());
#if defined(_WIN32)
    CHECK(admit_location("C:\\work\\game") == "C:/work/game");
    CHECK(admit_location("C:/work/../work/game") == "C:/work/game");
#endif
}

TEST_CASE("PROJ-2: parent is lexical and stops where a path stops, not where a project does") {
    // ⭐ THE MEASURED FIXED POINT, and the reason `has_parent_path()` is never the test:
    // it answers TRUE at every root below, so a boundary built on it would never fire.
    CHECK(parent_location(abs_spelling("/p/root/src/deep")) == abs_spelling("/p/root/src"));
    CHECK(parent_location(abs_spelling("/p/root")) == abs_spelling("/p"));
    CHECK(parent_location(abs_spelling("/p")) == root_spelling());
    CHECK(parent_location(root_spelling()).empty()); // the top -- and it HAS a parent_path by
    CHECK(std::filesystem::path(root_spelling()).has_parent_path()); // the standard's answer
    CHECK(parent_location("").empty());
#if defined(_WIN32)
    CHECK(parent_location("C:/work/game") == "C:/work");
    CHECK(parent_location("C:/work") == "C:/");
    CHECK(parent_location("C:/").empty());
    CHECK(std::filesystem::path("C:/").has_parent_path());
#endif
    // AND AN ESCAPE BELOW A ROOT IS UNSAYABLE, by `lexically_normal` alone -- no
    // containment resolver, no comparison anybody has to remember to make.
    CHECK(admit_location(abs_spelling("/..")) == root_spelling());
    CHECK(admit_location(abs_spelling("/home/../..")) == root_spelling());
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

TEST_CASE("EDIT-1: with no origin the pane refuses in words and guesses nothing") {
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
    CHECK(r.shown().find("this run began nowhere") != std::string::npos);
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
    CHECK(r.pane().current_dir == (r.root / "src").generic_string());
    const std::vector<std::string> only_a{"a.cpp"};
    CHECK(r.names() == only_a);
    CHECK(r.notice() == "in " + (r.root / "src").generic_string());
    // GOING UP PUTS THE MAKER BACK ON THE ROW THEY CAME FROM, which is the one refresh
    // that does not send the cursor home: the answer to "where was I" exists here.
    r.t.key(input::scan::kBackspace);
    CHECK(r.pane().current_dir == r.root.generic_string());
    CHECK(r.at_cursor() == "src");
    // ...AND THE PLACE THIS RUN BEGAN SAYS SO. Origin is a MARK, not a wall.
    CHECK(r.notice() == "in " + r.root.generic_string() + " (origin)");
}

TEST_CASE("PROJ-2: parent walks straight past the project and stops at the filesystem") {
    // ⭐⭐ THE PHASE'S FIRST CENTRAL CLAIM, THROUGH THE REAL GESTURE. A maker standing in
    // their project presses Backspace until a path has no parent left -- and what they walk
    // past on the way is the project anchor, which does not move, notice, or refuse.
    FilesRig r("root");
    std::filesystem::create_directories(r.root / "deep" / "deeper");
    r.t.host.project_dir = (r.root / "deep").generic_string();
    const std::string anchor = r.t.host.project_dir;
    r.open();
    REQUIRE(r.pane().current_dir == anchor);

    r.press_body(0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    r.t.key(input::scan::kBackspace);
    // ABOVE THE PROJECT, and the anchor is exactly where it was.
    CHECK(r.pane().current_dir == r.root.generic_string());
    CHECK(r.t.host.project_dir == anchor);
    CHECK(r.listing().known);

    // ...ALL THE WAY UP, and the top refuses in the FILESYSTEM's words rather than the
    // project's, and does not move.
    for (int i = 0; i < 64; ++i) {
        r.t.key(input::scan::kBackspace);
    }
    CHECK(r.pane().current_dir == std::filesystem::path(r.root).root_path().generic_string());
    CHECK(r.notice().find("is the top of this filesystem") != std::string::npos);
    CHECK(r.notice().find("project") == std::string::npos);
    const std::string at_top = r.pane().current_dir;
    r.t.key(input::scan::kBackspace);
    CHECK(r.pane().current_dir == at_top);
    // ...AND THE ANCHOR NEVER MOVED, through any of it.
    CHECK(r.t.host.project_dir == anchor);
    // AND NO ROW SAYS `..`, so there is nothing to press that would construct one.
    for (const std::string& name : r.names()) {
        CHECK(name != "..");
        CHECK(name != ".");
    }
}

TEST_CASE("PROJ-2: a linked directory is marked, entered, and left again LEXICALLY") {
    // ⭐ THE REFUSAL THAT IS NOT INHERITED. EDIT-1 refused entry to keep the entered-name
    // stack honest; there is no stack, so there is no property left to protect -- and the
    // measured cost of keeping the refusal anyway was six of the twenty-three directories
    // at POSIX `/`.
    FilesRig r("link");
    const std::filesystem::path outside = r.root / "outside";
    std::filesystem::create_directory(outside);
    put_file(outside / "secret.cpp", "int s;\n");
    std::filesystem::create_directory(r.root / "project");
    if (!make_linked_directory(r.root / "project" / "away", outside)) {
        // A PLATFORM THAT WILL MAKE NEITHER KIND FOR THIS PROCESS cannot answer this
        // question, and a case that quietly passed there would be claiming behaviour nobody
        // exercised. On Windows this is now the junction arm, which needs no privilege --
        // and a junction is the entry that matters there, because `is_symlink()` answers
        // FALSE for one while it is still a directory that leaves the tree.
        MESSAGE("this platform made no linked directory for this process");
        return;
    }
    const std::filesystem::path project = r.root / "project";
    r.t.host.project_dir = project.generic_string();
    r.open();
    REQUIRE(r.listing().rows.size() == 1);
    const FileRow& row = r.listing().rows.front();
    CHECK(row.name == "away");
    CHECK(row.directory);
    CHECK(row.linked); // the row is still MARKED -- that mark is what explains parent
#if defined(_WIN32)
    // ⚠⚠ AND ON WINDOWS THE MARK IS EARNED BY THE DISAGREEMENT TEST, not by `is_symlink()`.
    // MEASURED: a junction answers `is_symlink() == false` while being exactly the kind of
    // entry this row is marking, so an `is_symlink` predicate would have shown it as an
    // ordinary directory and the maker would have no idea why parent behaves as it does.
    const std::filesystem::path made = project / "away";
    std::error_code sym_ec;
    if (!std::filesystem::is_symlink(made, sym_ec) && !sym_ec) {
        MESSAGE("witnessed a Windows reparse point that is not a symbolic link (a junction)");
    }
#endif
    r.press_body(0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    r.t.key(input::scan::kReturn);

    // IT TRAVELLED, AND THE LOCATION IS THE ONE THE MAKER WALKED TO -- not the one the
    // link points at. Nothing canonicalized.
    CHECK(r.pane().current_dir == (project / "away").generic_string());
    CHECK(r.pane().current_dir.find("outside") == std::string::npos);
    const std::vector<std::string> through{"secret.cpp"};
    CHECK(r.names() == through); // the target's contents, through the link's own spelling

    // ...AND PARENT UNDOES EXACTLY THE ENTER. A canonicalizing browser would come back out
    // into the link's TARGET's parent (the temp root) instead of where the maker came from.
    r.t.key(input::scan::kBackspace);
    CHECK(r.pane().current_dir == project.generic_string());
    CHECK(r.at_cursor() == "away");
    // The row still says what it is, and no longer says it is refused.
    CHECK(files_row_text(r.listing().rows.front()).find("(link)") != std::string::npos);
    CHECK(files_row_text(r.listing().rows.front()).find("not entered") == std::string::npos);
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
    CHECK(r.notice() == "listed " + r.root.generic_string() + " again");
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
    REQUIRE(r.pane().current_dir == (r.root / "src").generic_string());
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

// ============================================================================
// Tier 5 — PROJ-0: one owner for the completed catalog this session means
// ============================================================================

namespace {

/// The host's own wiring of `HostContext::recipe_source`, said again here so a case
/// measures the shape `workshop.cpp` actually installs rather than a shape a rig
/// invented. It captures the OWNER and asks it at the moment of the gesture; that is
/// the whole of what this tier is about.
inline std::function<HostContext::RecipeSource(const std::string&)>
host_recipe_source(const CurrentRecipes& owner) {
    return [&owner](const std::string& id) {
        HostContext::RecipeSource out;
        const zengine::builder::Recipe* found = zengine::builder::recipe_named(owner.all(), id);
        if (found != nullptr) {
            out.known = true;
            out.kind = found->single_source.has_value() ? "single_source" : "cmake_target";
            if (found->single_source.has_value()) {
                out.source = found->single_source->source;
            }
        }
        return out;
    };
}

/// One authored single-source recipe, completed against a project the way the host
/// completes one -- the same function, the same two directories, in the same order.
inline std::vector<zengine::builder::Recipe> completed_catalog(const std::string& id,
                                                               const std::string& source,
                                                               const std::string& install,
                                                               const std::string& project) {
    zengine::builder::SingleSourceRecipe one;
    one.source = source;
    zengine::builder::Recipe authored;
    authored.id = id;
    authored.artifact = id;
    authored.single_source = one;
    std::vector<zengine::builder::Recipe> all{authored};
    recipe_persist::complete_recipes(all, install, project);
    return all;
}

} // namespace

TEST_CASE("PROJ-0: the owner derives the tool's view from the recipes it is holding") {
    // THE TWO HALVES COME OUT OF ONE VALUE, which is why nobody has to keep them in
    // step. `views()` is not a second catalog a host assembles beside the first; it is
    // the same rows with the build procedure subtracted (BLD-1) -- an identity, an
    // artifact stem, and the one file that stem means on this platform.
    CurrentRecipes owner;
    CHECK(owner.all().empty());
    CHECK(owner.views().empty()); // the "this Workshop can build nothing" state, held

    std::vector<zengine::builder::Recipe> completed =
        completed_catalog("one", "/abs/one.cpp", "/install", "/project");
    zengine::builder::Recipe second;
    second.id = "two";
    second.artifact = "zengine-two";
    second.artifact_dir = "/elsewhere";
    second.cmake_target = zengine::builder::CMakeTargetRecipe{"/tree", "two", std::string()};
    completed.push_back(second);
    owner.hold("/project/recipes.json", completed, &HostContext::so_in);

    REQUIRE(owner.all().size() == 2);
    REQUIRE(owner.views().size() == 2);
    // ROW FOR ROW, IN THE CATALOG'S OWN ORDER, so an index into one is an index into
    // the other -- which is what the Builder panel's `chosen` has always assumed.
    for (std::size_t i = 0; i < owner.all().size(); ++i) {
        CHECK(owner.views()[i].id == owner.all()[i].id);
        CHECK(owner.views()[i].artifact == owner.all()[i].artifact);
        CHECK(owner.views()[i].path ==
              HostContext::so_in(owner.all()[i].artifact_dir, owner.all()[i].artifact));
    }
    // ...AND THE COMPLETED ARTIFACT DIRECTORY IS WHAT THE VIEW SPELLS, not the host's
    // own: a recipe whose product lands in somebody else's tree is the reason `so_in`
    // takes a directory at all.
    CHECK(owner.views()[0].path == HostContext::so_in("/install", "one"));
    CHECK(owner.views()[1].path == HostContext::so_in("/elsewhere", "zengine-two"));
    // AND NOTHING IN THE VIEW CARRIES A BUILD PROCEDURE. The subtraction is the split,
    // and it survived being derived by the owner instead of by `main`.
    CHECK(owner.views()[0].path.find("one.cpp") == std::string::npos);
}

TEST_CASE("PROJ-0: holding a new catalog replaces the contents, never the object") {
    // ⭐ THE PROPERTY THAT MAKES ONE OWNER WORTH HAVING. Every consumer binds to these
    // two vectors once, at construction, and keeps that binding for the life of the
    // process -- so a replacement has to leave the OBJECTS alone and change only what is
    // in them. If `hold` ever replaces the vectors themselves, every consumer in the
    // process is reading freed memory, and this is where that is caught.
    CurrentRecipes owner;
    const std::vector<zengine::builder::Recipe>* recipes = &owner.all();
    const std::vector<zengine::builder::RecipeView>* views = &owner.views();

    owner.hold("/project/a.json", completed_catalog("one", "/abs/one.cpp", "/install", "/project"),
               &HostContext::so_in);
    CHECK(&owner.all() == recipes);
    CHECK(&owner.views() == views);
    REQUIRE(recipes->size() == 1);
    CHECK((*recipes)[0].id == "one");

    // A DIFFERENT CATALOG, THROUGH THE SAME DOOR: the bindings still name the live
    // answer, and the answer is the new one.
    owner.hold("/project/b.json",
               completed_catalog("later", "/abs/later.cpp", "/install", "/project"),
               &HostContext::so_in);
    CHECK(&owner.all() == recipes);
    CHECK(&owner.views() == views);
    REQUIRE(recipes->size() == 1);
    CHECK((*recipes)[0].id == "later");
    REQUIRE((*recipes)[0].single_source.has_value());
    CHECK((*recipes)[0].single_source->source == "/abs/later.cpp");
    REQUIRE(views->size() == 1);
    CHECK((*views)[0].id == "later");

    // ...AND AN EMPTY ONE EMPTIES BOTH, rather than leaving the previous views standing
    // beside no recipes -- the half-installed catalog this phase must not create.
    owner.hold("/project/empty.json", {}, &HostContext::so_in);
    CHECK(recipes->empty());
    CHECK(views->empty());
}

TEST_CASE("PROJ-0: the host's edit-source answer is asked of the owner, not of a copy") {
    // ⭐ THE CLOSURE FALSIFIER. `HostContext::recipe_source` used to capture its own list
    // of three fields per recipe -- a third session-long store of completed truth, taken
    // because the catalog was about to be handed onward by value. It captures the owner
    // now and reads it when asked, and this is the case that can tell the two apart: the
    // answer has to follow a catalog the closure never saw.
    CurrentRecipes owner;
    const std::function<HostContext::RecipeSource(const std::string&)> answer =
        host_recipe_source(owner);

    // WITH NOTHING HELD, NOTHING IS KNOWN -- and it is a refusal in the caller's hands
    // rather than a guess: `known` false is the whole statement.
    CHECK_FALSE(answer("one").known);

    owner.hold("/project/a.json", completed_catalog("one", "src/a.cpp", "/install", "/project"),
               &HostContext::so_in);
    const HostContext::RecipeSource first = answer("one");
    CHECK(first.known);
    CHECK(first.kind == "single_source"); // the recipe FILE's own word for the kind
    CHECK(first.source == "/project/src/a.cpp");
    CHECK_FALSE(answer("two").known);

    // THE ONE PLACE CHANGES, AND THE SAME CLOSURE ANSWERS THE NEW CATALOG.
    owner.hold("/elsewhere/b.json",
               completed_catalog("two", "src/b.cpp", "/install", "/elsewhere"),
               &HostContext::so_in);
    CHECK_FALSE(answer("one").known);
    const HostContext::RecipeSource second = answer("two");
    CHECK(second.known);
    CHECK(second.source == "/elsewhere/src/b.cpp");

    // ...AND A KIND WITH NO SOURCE STILL SAYS SO IN THE FILE'S WORDS, so a refusal
    // downstream speaks the vocabulary the maker authored in.
    zengine::builder::Recipe target;
    target.id = "built";
    target.artifact = "built";
    target.cmake_target = zengine::builder::CMakeTargetRecipe{"/tree", "t", std::string()};
    owner.hold("/project/t.json", {target}, &HostContext::so_in);
    const HostContext::RecipeSource named = answer("built");
    CHECK(named.known);
    CHECK(named.kind == "cmake_target");
    CHECK(named.source.empty());
}

TEST_CASE("PROJ-0: the editor opens the file the OWNER's completed recipe names") {
    // EDIT-1'S SENTENCE, RE-PROVEN OVER THE NEW CUSTODY. The two-base decoy is arranged
    // exactly as it is above -- the project and the generated workspace both holding
    // `src/example.cpp` with different bytes -- and the only thing that changed is who
    // holds the completed value the editor's answer comes from. A green here with the
    // decoy absent would prove nothing, which is why the decoy is written first.
    FilesRig r("ownerdoor");
    const std::filesystem::path workspace = r.root / "build-workspace" / "one";
    std::filesystem::create_directories(r.root / "src");
    std::filesystem::create_directories(workspace / "src");
    put_file(r.root / "src" / "example.cpp", "the project\n");
    put_file(workspace / "src" / "example.cpp", "the decoy\n");

    zengine::builder::SingleSourceRecipe one;
    one.source = "src/example.cpp"; // relative, exactly as a maker writes it
    one.workspace = workspace.generic_string();
    zengine::builder::Recipe authored;
    authored.id = "one";
    authored.artifact = "one";
    authored.single_source = one;
    std::vector<zengine::builder::Recipe> all{authored};
    recipe_persist::complete_recipes(all, r.root.generic_string(), r.t.host.project_dir);

    // THE OWNER TAKES IT, AND THE HOST'S SEAM IS WIRED OVER THE OWNER -- `workshop.cpp`,
    // in the order `workshop.cpp` does it.
    CurrentRecipes owner;
    owner.hold("/project/recipes.json", std::move(all), &HostContext::so_in);
    r.t.host.recipe_source = host_recipe_source(owner);

    ToolSeat* tool = mount_tool(r.t, "one");
    (void)tool;
    open_builder(r.t);
    r.t.key(input::scan::kE);
    r.t.text("e");
    REQUIRE(r.session().editor.open_document());
    CHECK(r.session().editor.path ==
          (r.root / "src" / "example.cpp").lexically_normal().generic_string());
    CHECK(r.session().editor.buffer.line(0) == "the project");
    // AND THE GENERATED PROJECT, FROM THE OWNER'S OWN ROW, NAMES THE SAME FILE -- which
    // is the whole of "the file you edit is the file the build compiles", now carried by
    // one object instead of by three parties agreeing.
    REQUIRE(owner.all().size() == 1);
    CHECK(zengine::builder::generated_project(owner.all()[0]).find(r.session().editor.path) !=
          std::string::npos);
    // ...AND THE ARTIFACT LOOKUP READS THE SAME ROW: one view, whose path is the
    // completed artifact directory and the stem, spelled by the host's one rule.
    REQUIRE(owner.views().size() == 1);
    CHECK(owner.views()[0].path ==
          HostContext::so_in(owner.all()[0].artifact_dir, owner.all()[0].artifact));
}

TEST_CASE("PROJ-0/PROJ-1: one completed catalog, installed through one seam") {
    // DEFENCE IN DEPTH, AND SAID TO BE. Every case above drives a seam; what a source
    // read adds is that "there is ONE completed catalog in this process, installed by ONE
    // function" cannot quietly stop being true while all of them stay green -- and no rig
    // can run `main()`, which claims a terminal.
    //
    // ⚠ THE FORBIDDEN FORMS ARE EXPRESSIONS, never bare words, and the prose goes first
    // (BLD-0's tripwire rule): both files EXPLAIN the ownership they hold, and a check
    // that could not tell a sentence from a statement would read the explanation as the
    // defect.
    const auto code_of = [](const char* path) {
        std::string out;
        std::ifstream in(path);
        REQUIRE_MESSAGE(in.good(), "cannot read ", path);
        std::string line;
        while (std::getline(in, line)) {
            const std::size_t comment = line.find("//");
            out += comment == std::string::npos ? line : line.substr(0, comment);
            out += '\n';
        }
        return out;
    };
    const std::string host = code_of(WORKSHOP_HOST_CPP);
    const std::string owner_file = code_of(WORKSHOP_RECIPES_HPP);

    // ONE OWNER, DECLARED BEFORE THE BUS -- and the ORDER is the lifetime proof, because
    // everything that reads it is destroyed with the Kernel.
    const std::size_t owner = host.find("CurrentRecipes current_recipes;");
    const std::size_t bus = host.find("loom::Switchboard bus;");
    REQUIRE(owner != std::string::npos);
    REQUIRE(bus != std::string::npos);
    CHECK(owner < bus);

    // ⭐ THE COMPLETION AND THE CUSTODY MOVED INTO ONE SEAM (PROJ-1), and the whole point
    // of moving them is that the launch and a maker's live choice cannot come to complete
    // an authored recipe differently. So the host spells NEITHER any more: it wires one
    // closure over `install_recipes` and everything -- including its own startup catalog
    // -- goes through it.
    CHECK(host.find("install_recipes(") != std::string::npos);
    CHECK_MESSAGE(host.find("recipe_persist::complete_recipes(") == std::string::npos,
                  "workshop.cpp completes recipes itself, which is a second recipe policy "
                  "beside the one `install_recipes` holds");
    CHECK_MESSAGE(host.find("current_recipes.hold(") == std::string::npos,
                  "workshop.cpp installs a catalog directly, bypassing the one transaction "
                  "every catalog change is supposed to be");
    // ...AND THE SEAM ITSELF COMPLETES ONCE AND HOLDS ONCE. A second call to either inside
    // the owner's own header would be the same drift one file over.
    const auto occurrences = [](const std::string& text, const std::string& needle) {
        std::size_t at = 0;
        std::size_t seen = 0;
        while ((at = text.find(needle, at)) != std::string::npos) {
            ++seen;
            at += needle.size();
        }
        return seen;
    };
    CHECK(occurrences(owner_file, "recipe_persist::complete_recipes(") == 1);
    CHECK(occurrences(owner_file, "owner.hold(") == 1);
    // ⭐ AND THE SOURCE PATH HAS EXACTLY ONE WRITER, WHICH IS `hold`. `hold` takes the
    // path as a parameter precisely so that "the path moved and the recipes did not" has
    // no spelling in this program; one assignment, in the same call that moves the rows,
    // is what makes that a property of the type rather than of a caller's care.
    CHECK(occurrences(owner_file, "source_ =") == 1);
    CHECK_MESSAGE(owner_file.find("set_source") == std::string::npos,
                  "workshop/recipes.hpp has a second writer for the catalog's source path");

    // EVERY CONSUMER READS THE OWNER. The runner, the tool, the edit-source answer and
    // the waiting-row predicate are the four, and each is spelled as a read.
    CHECK(host.find("kBuildRunnerRole, current_recipes.all()") != std::string::npos);
    CHECK(host.find("kBuilderRole, current_recipes.views()") != std::string::npos);
    CHECK(host.find("[&current_recipes](const std::string& id)") != std::string::npos);
    CHECK(host.find("[&host, &current_recipes](const std::string& stem)") != std::string::npos);

    // ...AND NOTHING ELSE KEEPS ONE. The two locals this host used to carry past the read
    // -- a completed catalog and a derived view list -- are gone, and a `main` that grew
    // either back would be a `main` with two answers again.
    for (const char* forbidden : {"std::vector<builder::RecipeView> recipe_views",
                                  "recipe_views.reserve", "recipe_views.push_back"}) {
        CHECK_MESSAGE(host.find(forbidden) == std::string::npos, "workshop.cpp declares '",
                      forbidden, "', which is a second long-lived catalog of views");
    }
    // The host no longer spells a `std::vector<builder::Recipe>` at all: the candidate
    // lives and dies inside `install_recipes`.
    CHECK(occurrences(host, "std::vector<builder::Recipe> ") == 0);
}


// ============================================================================
// Tier 6 — PROJ-1: choosing a recipe catalog while Workshop is running
// ============================================================================

namespace {

/// ONE AUTHORED SINGLE-SOURCE RECIPE, as a maker writes one -- legal against
/// `builder::check_recipes`, which is the law every file goes through.
inline zengine::builder::Recipe authored_recipe(const std::string& id,
                                                const std::string& source) {
    zengine::builder::SingleSourceRecipe one;
    one.source = source;
    one.links.push_back("loom::kernel");
    zengine::builder::Recipe r;
    r.id = id;
    r.artifact = id;
    r.single_source = one;
    return r;
}

/// A CATALOG FILE ON DISK, written through the codec that reads it -- never hand-rolled
/// bytes, so a case cannot pass by agreeing with itself about a format.
inline void put_catalog(const std::filesystem::path& at,
                        const std::vector<zengine::builder::Recipe>& authored) {
    put_file(at, recipe_persist::to_text(authored));
}

inline std::string bytes_of(const std::filesystem::path& at) {
    std::ifstream in(at, std::ios::binary);
    REQUIRE(in.good());
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/// EVERYTHING THE OWNER IS HOLDING, AS ONE VALUE -- so a case can say "nothing moved"
/// about the whole of it rather than about whichever half it remembered to check. The
/// half-replacements this phase forbids are exactly the states in which two of these three
/// agree with a previous reading and the third does not.
struct Held {
    std::string source;
    std::vector<std::string> ids;
    std::vector<std::string> paths;

    friend bool operator==(const Held&, const Held&) = default;
};

inline Held held_by(const CurrentRecipes& owner) {
    Held out;
    out.source = owner.source();
    for (const zengine::builder::Recipe& r : owner.all()) {
        out.ids.push_back(r.id);
    }
    for (const zengine::builder::RecipeView& v : owner.views()) {
        out.paths.push_back(v.path);
    }
    return out;
}

/// The host's own wiring of `HostContext::use_recipes`, said again here so a case measures
/// the shape `workshop.cpp` actually installs rather than a shape a rig invented. The
/// source-read case above is what keeps the two from drifting.
inline std::function<HostContext::RecipeSwap(const std::string&)>
host_use_recipes(CurrentRecipes& owner, std::string host_dir, std::string project_dir) {
    return [&owner, host_dir, project_dir](const std::string& path) {
        HostContext::RecipeSwap done;
        const Written read = install_recipes(owner, path, host_dir, project_dir,
                                             &HostContext::so_in);
        done.accepted = read.accepted;
        done.refusal = read.refusal;
        done.path = owner.source();
        done.recipes = owner.all().size();
        return done;
    };
}

/// THE REAL BUILDER TOOL, mounted over the owner's views the way the host mounts it. It is
/// the participant a live replacement has to reach, and it holds no catalog of its own --
/// which is the whole reason a replacement can reach it at all.
inline zengine::builder::BuilderWeave* mount_live_tool(Live& t, const CurrentRecipes& owner) {
    auto seat = std::make_unique<zengine::builder::BuilderWeave>(owner.views());
    zengine::builder::BuilderWeave* raw = seat.get();
    loom::Grant grant;
    grant.allow_to_any(zengine::builder::BuildStatus::zen_name,
                       zengine::builder::BuildStatus::zen_version);
    grant.allow_to_any(zengine::builder::RecipeCatalog::zen_name,
                       zengine::builder::RecipeCatalog::zen_version);
    grant.allow_to_any(zengine::builder::OfferArtifact::zen_name,
                       zengine::builder::OfferArtifact::zen_version);
    grant.allow_to_role(zengine::builder::RunBuild::zen_name,
                        zengine::builder::RunBuild::zen_version,
                        zengine::builder::kBuildRunnerRole);
    const loom::WeaveId id = t.bus.register_weave(std::move(seat), std::move(grant),
                                                  std::string(zengine::builder::kBuilderRole));
    raw->zen_set_self(id);
    return raw;
}

/// Put the browser's cursor on a named row, the way a maker does: point the keys at the
/// pane if they are elsewhere, then step. Fails loudly rather than acting on whatever row
/// it ended on.
///
/// ⚠ IT PRESSES ONLY WHEN THE PANE DOES NOT ALREADY HOLD THE KEYS, because a press on the
/// selected row of a pane that DOES hold them is the browser's activation gesture -- which
/// would open the row in the editor instead of pointing at it. That asymmetry is the
/// browser's own two-press promise, and a helper that ignored it would arrange a different
/// case from the one it claims to.
inline void point_at(FilesRig& r, const std::string& name) {
    if (keyboard_context(r.session()) != KeyContext::kFiles) {
        r.press_body(0);
    }
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    for (std::size_t i = 0; i < r.listing().rows.size(); ++i) {
        r.t.key(input::scan::kUp); // home, so the walk below starts from a known row
    }
    for (std::size_t i = 0; i < r.listing().rows.size(); ++i) {
        if (r.at_cursor() == name) {
            return;
        }
        r.t.key(input::scan::kDown);
    }
    REQUIRE_MESSAGE(r.at_cursor() == name, "no row called ", name);
}

/// The gesture itself: `u` in the browser.
inline void use_as_recipes(FilesRig& r) {
    r.t.key(input::scan::kU);
    r.t.text("u");
}

} // namespace

TEST_CASE("PROJ-1: installing a catalog moves its source, its rows and its views together") {
    TempDir dir("install");
    const std::filesystem::path root = dir.path();
    put_catalog(root / "recipes.json", {authored_recipe("one", "src/one.cpp"),
                                        authored_recipe("two", "src/two.cpp")});

    CurrentRecipes owner;
    const Written done = install_recipes(owner, (root / "recipes.json").generic_string(),
                                         "/install", root.generic_string(),
                                         &HostContext::so_in);
    REQUIRE(done.accepted);
    // THE THREE ANSWERS, AND THEY ARE ONE ANSWER. The file it came from, what it holds,
    // and the tool's reduced view of what it holds -- all installed by the one call.
    CHECK(owner.source() == (root / "recipes.json").generic_string());
    REQUIRE(owner.all().size() == 2);
    CHECK(owner.all()[0].id == "one");
    REQUIRE(owner.views().size() == 2);
    CHECK(owner.views()[1].id == "two");
    // ...AND THE COMPLETION LAW RAN, ONCE, ON THE WAY IN: a relative source means the
    // PROJECT, and an empty artifact directory means the install.
    CHECK(owner.all()[0].single_source->source == (root / "src/one.cpp").generic_string());
    CHECK(owner.all()[0].artifact_dir == "/install");
    CHECK(owner.views()[0].path == HostContext::so_in("/install", "one"));
}

TEST_CASE("PROJ-1: a candidate that cannot be read installs nothing at all") {
    // ⭐ THE TRANSACTION, AT ITS FIRST STAGE. The interesting claim is not that the call
    // said no -- it is that the session is holding exactly what it was holding, in all
    // three of its answers. A implementation that cleared the owner before reading, or
    // that wrote the path before the rows, passes a "was it refused" check and fails this.
    TempDir dir("unreadable");
    const std::filesystem::path root = dir.path();
    put_catalog(root / "good.json", {authored_recipe("one", "src/one.cpp")});

    CurrentRecipes owner;
    REQUIRE(install_recipes(owner, (root / "good.json").generic_string(), "/install",
                            root.generic_string(), &HostContext::so_in)
                .accepted);
    const Held before = held_by(owner);

    const Written no = install_recipes(owner, (root / "not-here.json").generic_string(),
                                       "/install", root.generic_string(),
                                       &HostContext::so_in);
    CHECK_FALSE(no.accepted);
    // THE OWNER'S OWN WORDS, unwrapped: `persist::read_file` names the file and says why.
    CHECK(no.refusal.find("not-here.json") != std::string::npos);
    CHECK(held_by(owner) == before);
}

TEST_CASE("PROJ-1: bytes that are not a catalog install nothing at all") {
    // THE SECOND STAGE, AND THREE DIFFERENT WAYS TO FAIL IT: bytes that are not the wire
    // format at all, a well-formed file that says it is something else, and a file whose
    // RECIPE LAW is broken. Each is refused in its own owner's words and each leaves the
    // session holding what it held.
    TempDir dir("malformed");
    const std::filesystem::path root = dir.path();
    put_catalog(root / "good.json", {authored_recipe("one", "src/one.cpp")});

    CurrentRecipes owner;
    REQUIRE(install_recipes(owner, (root / "good.json").generic_string(), "/install",
                            root.generic_string(), &HostContext::so_in)
                .accepted);
    const Held before = held_by(owner);

    put_file(root / "notes.txt", "these are notes, not recipes\n");
    const Written garbage = install_recipes(owner, (root / "notes.txt").generic_string(),
                                            "/install", root.generic_string(),
                                            &HostContext::so_in);
    CHECK_FALSE(garbage.accepted);
    CHECK_FALSE(garbage.refusal.empty());
    CHECK(held_by(owner) == before);

    // A DIFFERENT WORKSHOP FILE, which is the honest confusion a maker actually makes:
    // four durable artifacts sit beside each other and this one says which it is.
    put_file(root / "plan.json", load_persist::to_text(load::LoadPlan{}));
    const Written wrong_kind = install_recipes(owner, (root / "plan.json").generic_string(),
                                               "/install", root.generic_string(),
                                               &HostContext::so_in);
    CHECK_FALSE(wrong_kind.accepted);
    CHECK(held_by(owner) == before);

    // AND A CATALOG THE RECIPE LAW REFUSES: one name, twice. The file parses; what fails
    // is `builder::check_recipes`, which is the same function a typed catalog goes
    // through, so a maker meets one sentence rather than two.
    put_catalog(root / "twice.json", {authored_recipe("one", "src/a.cpp")});
    {
        std::string text = bytes_of(root / "twice.json");
        // Two rows with one name cannot be produced by the writer, so the file is forged
        // the way this suite forges every other refusal: by editing what the writer wrote.
        const std::size_t at = text.find("\"recipes\"");
        REQUIRE(at != std::string::npos);
        const std::size_t open = text.find('[', at);
        const std::size_t shut = text.rfind(']');
        REQUIRE(open != std::string::npos);
        REQUIRE(shut != std::string::npos);
        const std::string row = text.substr(open + 1, shut - open - 1);
        text = text.substr(0, open + 1) + row + "," + row + text.substr(shut);
        put_file(root / "twice.json", text);
    }
    const Written twice = install_recipes(owner, (root / "twice.json").generic_string(),
                                          "/install", root.generic_string(),
                                          &HostContext::so_in);
    CHECK_FALSE(twice.accepted);
    CHECK(twice.refusal.find("declared twice") != std::string::npos);
    CHECK(held_by(owner) == before);
}

TEST_CASE("PROJ-1: a valid EMPTY catalog is a replacement, not a failure") {
    // ⭐ THE DISTINCTION A FAILURE-SHAPED IMPLEMENTATION LOSES. `builder::check_recipes`
    // admits an empty catalog deliberately -- a project with nothing to build is a project
    // -- so a maker who authors one MEANT it, and installing it must leave the session
    // holding no recipes AND holding that file as its source. That is the opposite of a
    // parse failure, which leaves the previous catalog untouched.
    TempDir dir("emptycat");
    const std::filesystem::path root = dir.path();
    put_catalog(root / "full.json", {authored_recipe("one", "src/one.cpp")});
    put_catalog(root / "empty.json", {});

    CurrentRecipes owner;
    REQUIRE(install_recipes(owner, (root / "full.json").generic_string(), "/install",
                            root.generic_string(), &HostContext::so_in)
                .accepted);
    REQUIRE(owner.all().size() == 1);

    REQUIRE(install_recipes(owner, (root / "empty.json").generic_string(), "/install",
                            root.generic_string(), &HostContext::so_in)
                .accepted);
    CHECK(owner.all().empty());
    CHECK(owner.views().empty());
    CHECK(owner.source() == (root / "empty.json").generic_string());
}

TEST_CASE("PROJ-1: selecting the catalog already in force is a reload, not a no-op") {
    // ⭐ THE OPTIMIZATION THAT WOULD COST THE FEATURE. The authored file is DURABLE truth
    // and a maker edits it; if installing the same path short-circuited, the one explicit
    // way to pick up that edit would silently do nothing -- and this application would
    // then need a watcher, a poll or a timer to be honest, which is exactly what it has
    // refused to grow.
    TempDir dir("reload");
    const std::filesystem::path root = dir.path();
    const std::string path = (root / "recipes.json").generic_string();
    put_catalog(root / "recipes.json", {authored_recipe("before", "src/a.cpp")});

    CurrentRecipes owner;
    REQUIRE(install_recipes(owner, path, "/install", root.generic_string(),
                            &HostContext::so_in)
                .accepted);
    REQUIRE(owner.all()[0].id == "before");

    put_catalog(root / "recipes.json", {authored_recipe("after", "src/b.cpp")});
    REQUIRE(install_recipes(owner, path, "/install", root.generic_string(),
                            &HostContext::so_in)
                .accepted);
    CHECK(owner.source() == path); // the same file...
    REQUIRE(owner.all().size() == 1);
    CHECK(owner.all()[0].id == "after"); // ...read again
    CHECK(owner.all()[0].single_source->source == (root / "src/b.cpp").generic_string());
}

TEST_CASE("PROJ-1: a catalog's own directory is not a source base") {
    // ⭐⭐ THE COMPLETION FALSIFIER, and it is arranged so the wrong base names a REAL FILE
    // WITH DIFFERENT BYTES. EDIT-1's law is that a relative authored `single_source` means
    // the PROJECT -- where this Workshop was launched -- and PROJ-1 must not let picking a
    // catalog somewhere else quietly move that. So both candidate bases hold
    // `src/thing.cpp`: the project's, and the directory the catalog file itself sits in. A
    // green with only one of them present would prove nothing at all.
    TempDir dir("anchor");
    const std::filesystem::path root = dir.path();
    const std::filesystem::path elsewhere = root / "catalogs";
    std::filesystem::create_directories(root / "src");
    std::filesystem::create_directories(elsewhere / "src");
    put_file(root / "src" / "thing.cpp", "the project\n");
    put_file(elsewhere / "src" / "thing.cpp", "the decoy\n");
    put_catalog(elsewhere / "recipes.json", {authored_recipe("thing", "src/thing.cpp")});

    CurrentRecipes owner;
    REQUIRE(install_recipes(owner, (elsewhere / "recipes.json").generic_string(), "/install",
                            root.generic_string(), &HostContext::so_in)
                .accepted);
    REQUIRE(owner.all().size() == 1);
    // THE PROJECT'S FILE, NOT THE CATALOG'S NEIGHBOUR.
    CHECK(owner.all()[0].single_source->source ==
          (root / "src" / "thing.cpp").generic_string());
    CHECK(owner.all()[0].single_source->source.find("catalogs") == std::string::npos);
    // ...AND THE GENERATED PROJECT NAMES THE SAME ONE, which is where the two files would
    // actually have diverged: "the file you edit is the file the build compiles".
    CHECK(zengine::builder::generated_project(owner.all()[0])
              .find((root / "src" / "thing.cpp").generic_string()) != std::string::npos);
}

TEST_CASE("PROJ-1: a maker chooses a catalog in Files and every consumer moves with it") {
    // ⭐ THE PHASE'S CENTRAL CLAIM, END TO END AND THROUGH THE REAL PARTICIPANTS. A maker
    // points at an ordinary file, invokes one ordinary action, and the recipes this
    // session means are the ones that file authored -- with no process restart, no second
    // browser, no modal chooser and no extension test anywhere on the path.
    //
    // ⚠ THE OWNER IS DECLARED FIRST, exactly as `main` declares it above the bus: the tool
    // below reads it for as long as it lives.
    CurrentRecipes owner;
    FilesRig r("uselive");
    const std::string install = r.root.generic_string();
    put_catalog(r.root / "a-recipes.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_catalog(r.root / "b-recipes.json", {authored_recipe("beta", "src/beta.cpp"),
                                            authored_recipe("gamma", "src/gamma.cpp")});
    r.t.host.use_recipes = host_use_recipes(owner, install, r.t.host.project_dir);
    REQUIRE(r.t.host.use_recipes((r.root / "a-recipes.json").generic_string()).accepted);

    zengine::builder::BuilderWeave* tool = mount_live_tool(r.t, owner);
    open_builder(r.t);
    r.open();
    REQUIRE(r.session().panels.builder.known.recipes.size() == 1);
    REQUIRE(r.session().panels.builder.known.recipes[0].recipe == "alpha");
    REQUIRE(r.shown().find("alpha -> alpha  (1/1)") != std::string::npos);
    // NOTHING NAMES A CATALOG YET: the session has not moved, and the host's banner said
    // the launch catalog correctly.
    REQUIRE(r.shown().find("catalog") == std::string::npos);

    point_at(r, "b-recipes.json");
    use_as_recipes(r);

    // THE OWNER MOVED, WHOLE.
    CHECK(owner.source() == (r.root / "b-recipes.json").generic_string());
    REQUIRE(owner.all().size() == 2);
    CHECK(owner.all()[0].id == "beta");
    // ...AND THE TOOL ANSWERS THE NEW CATALOG, because it reads the owner rather than a
    // list it kept. It was never destroyed, never recreated, and never told a recipe.
    REQUIRE(tool->recipes().size() == 2);
    CHECK(tool->recipes()[1].id == "gamma");
    // ...AND THE PANEL SHOWS IT, republished through the same `StatusRequested` an opening
    // panel has always sent. No observer, no subscription, no second recipe event.
    REQUIRE(r.session().panels.builder.known.recipes.size() == 2);
    CHECK(r.session().panels.builder.known.recipes[0].recipe == "beta");
    CHECK(r.shown().find("beta -> beta  (1/2)") != std::string::npos);
    // THE MAKER IS TOLD WHAT HAPPENED AND WHAT IS NOW CURRENT.
    CHECK(r.notice().find("b-recipes.json") != std::string::npos);
    CHECK(r.notice().find("2 recipes") != std::string::npos);
    // ...AND THE PANEL CAN STILL ANSWER IT LATER, because the banner has stopped being
    // true and this is the row that says so.
    //
    // ⭐ IT IS THE ABSOLUTE PATH, FITTED BY THE MEASURER THAT KEEPS ITS TAIL (PROJ-2). The
    // project-relative spelling PROJ-1 shipped here was unambiguous only while the browser
    // could not leave the project, and the file's own NAME is the half a maker needs. This
    // temp path is longer than the panel's measured column, so what is asserted is exactly
    // the property: the leaf survives, the cut marks itself, and the row still fits.
    const std::string panel = r.shown();
    CHECK(panel.find("catalog  ") != std::string::npos);
    CHECK(panel.find("b-recipes.json") != std::string::npos);
    CHECK(panel.find(detail::kElided) != std::string::npos);
    // ...AND IT COST ONE `said` ROW AND NOTHING ELSE. Every fact a maker acts on is still
    // on the panel, in the same order: this row yields first under any smaller budget, and
    // a session that never moves its catalog pays nothing at all.
    for (const char* kept : {"BUILDER @", "recipe   ", "last     ", "realize  ", "exit     ",
                             "ran      ", "said     "}) {
        CHECK_MESSAGE(panel.find(kept) != std::string::npos, "the catalog row displaced `",
                      kept, "`");
    }
}

TEST_CASE("PROJ-1: the chooser does not need the Builder panel to be open") {
    // THE ORDERING CLAIM, MADE EXPLICITLY. Choosing what this project can build is not an
    // act on the Builder's presentation, so requiring that presentation to exist first
    // would be a gesture that depended on which panes a maker happened to have open.
    CurrentRecipes owner;
    FilesRig r("nobuilder");
    put_catalog(r.root / "a.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_catalog(r.root / "b.json", {authored_recipe("beta", "src/beta.cpp")});
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(),
                                            r.t.host.project_dir);
    REQUIRE(r.t.host.use_recipes((r.root / "a.json").generic_string()).accepted);
    zengine::builder::BuilderWeave* tool = mount_live_tool(r.t, owner);
    r.open();
    REQUIRE_FALSE(r.session().panels.has(panel::kBuilder));

    point_at(r, "b.json");
    use_as_recipes(r);

    CHECK(owner.source() == (r.root / "b.json").generic_string());
    REQUIRE(tool->recipes().size() == 1);
    CHECK(tool->recipes()[0].id == "beta");
    // ...AND A PANEL OPENED AFTERWARDS IS TOLD THE TRUTH BY THE ORDINARY ASK. The keys
    // are in the browser, so they are handed back first -- the picker is command mode's.
    r.to_command();
    open_builder(r.t);
    REQUIRE(r.session().panels.builder.known.recipes.size() == 1);
    CHECK(r.session().panels.builder.known.recipes[0].recipe == "beta");
}

TEST_CASE("PROJ-1: a refused catalog leaves the maker exactly where they were") {
    // ⭐ THE RECOVERY CLAIM. A file that is not a catalog is an ordinary thing to point at
    // -- the browser lists every real file and judges no contents -- so the refusal has to
    // be survivable: the recipes in force are the old ones, the panel still shows them, the
    // browser still works, and the maker is told BOTH halves.
    CurrentRecipes owner;
    FilesRig r("refused");
    put_catalog(r.root / "a.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_file(r.root / "notes.txt", "not a catalog\n");
    std::filesystem::create_directories(r.root / "somewhere");
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(),
                                            r.t.host.project_dir);
    REQUIRE(r.t.host.use_recipes((r.root / "a.json").generic_string()).accepted);
    zengine::builder::BuilderWeave* tool = mount_live_tool(r.t, owner);
    open_builder(r.t);
    r.open();
    const Held before = held_by(owner);
    const std::size_t catalogs_before = r.session().panels.builder.known.recipes.size();

    point_at(r, "notes.txt");
    use_as_recipes(r);

    CHECK(held_by(owner) == before);
    CHECK(r.session().panels.builder.known.recipes.size() == catalogs_before);
    CHECK(r.session().panels.builder.known.recipes[0].recipe == "alpha");
    REQUIRE(tool->recipes().size() == 1);
    CHECK(tool->recipes()[0].id == "alpha");
    // BOTH HALVES IN ONE SENTENCE, AND THE ORDER IS PART OF THE CLAIM: the two short fixed
    // statements come first because the notice row is cut at the band's width, and the
    // half a maker most needs -- "you did not just lose your recipes" -- must not be the
    // half that elides. The live witness measured exactly that failure with the reason
    // first, which is why the order is asserted here and not merely the content.
    const std::string said = r.notice();
    CHECK(said.find("not a recipe catalog") == 0u);
    CHECK(said.find("the recipes in force are unchanged") != std::string::npos);
    CHECK(said.find("the recipes in force are unchanged") < said.find("not a Workshop"));
    CHECK(said.find("still using " + (r.root / "a.json").generic_string()) !=
          std::string::npos);
    // ...AND NO CATALOG ROW APPEARED, because this session has not moved.
    CHECK(r.shown().find("catalog  ") == std::string::npos);

    // A DIRECTORY IS REFUSED IN THE BROWSER'S OWN WORDS, before the owner is troubled.
    point_at(r, "somewhere");
    use_as_recipes(r);
    CHECK(r.notice().find("is a directory") != std::string::npos);
    CHECK(held_by(owner) == before);

    // AND WORKSHOP IS STILL WORKSHOP: the browser answers the next gesture.
    r.t.key(input::scan::kR);
    CHECK(r.notice() == "listed " + r.root.generic_string() + " again");
}

TEST_CASE("PROJ-1: recipes come from the saved file, never from an unsaved editor buffer") {
    // ⭐ THE DURABLE-AUTHORSHIP CLAIM. The same path can be open in the editor and pointed
    // at in the browser, and the two are answering different questions: what a maker is
    // WRITING, and what this session currently MEANS. Joining them -- consuming the buffer,
    // or saving it first -- would make an unsaved draft into build procedure.
    CurrentRecipes owner;
    FilesRig r("dirtycat");
    put_catalog(r.root / "b.json", {authored_recipe("beta", "src/beta.cpp")});
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(),
                                            r.t.host.project_dir);
    REQUIRE(r.t.host.use_recipes((r.root / "b.json").generic_string()).accepted);
    const std::string durable = bytes_of(r.root / "b.json");
    r.open();

    // OPEN THE CATALOG IN THE EDITOR AND MAKE THE BUFFER DIFFER. One character at the
    // caret is enough, and it is deliberately one that makes the BUFFER unreadable as a
    // catalog: if the buffer were ever consumed, the install below could not succeed.
    point_at(r, "b.json");
    r.t.key(input::scan::kReturn);
    REQUIRE(r.session().editor.open_document());
    REQUIRE(r.session().editor.path == (r.root / "b.json").lexically_normal().generic_string());
    REQUIRE(keyboard_context(r.session()) == KeyContext::kEditor);
    r.t.text("x");
    REQUIRE(r.session().editor.dirty());

    // THE DURABLE FILE IS WHAT IS READ.
    point_at(r, "b.json");
    use_as_recipes(r);
    REQUIRE(owner.all().size() == 1);
    CHECK(owner.all()[0].id == "beta");
    // NOTHING WAS SAVED ON THE MAKER'S BEHALF, and the draft is still theirs.
    CHECK(bytes_of(r.root / "b.json") == durable);
    CHECK(r.session().editor.dirty());

    // AND WHEN THEY DO SAVE, THE RELOAD READS WHAT THEY SAVED. The saved bytes are the
    // buffer's, which is not a catalog -- so the refusal here IS the proof that the file on
    // disk is the input, and that a same-path reload is a real read rather than a cached
    // answer. `editor.save` is the editor's own row, so the keys go back to the editor the
    // way they came: a press on the row this pane already has open reveals it.
    point_at(r, "b.json");
    r.t.key(input::scan::kReturn);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kEditor);
    r.t.key(input::scan::kS, input::mod::kCtrl);
    REQUIRE_FALSE(r.session().editor.dirty());
    CHECK(bytes_of(r.root / "b.json") != durable);

    const Held before = held_by(owner);
    point_at(r, "b.json");
    use_as_recipes(r);
    CHECK(r.notice().find("not a recipe catalog") != std::string::npos);
    CHECK(held_by(owner) == before);
}

TEST_CASE("PROJ-1: the republish is the ask the panel already sends, once") {
    // ⭐ THE ROUTE, COUNTED. `Panels::builder.known` is a derived presentation copy and the
    // one live catalog projection that still needs a push -- so a replacement has to push
    // it, and the only honest question is WHAT it pushes with. The answer is the message an
    // opening panel has always sent: one `StatusRequested`, to the office that already
    // answers it, publishing the two shapes it already publishes.
    //
    // The stand-in is used here rather than the real tool precisely because it COUNTS: it
    // records how many times it was asked, which is the fact this case is about. An
    // observer graph, a subscription, a second recipe event or a repeated poll would all
    // show up as a different number.
    CurrentRecipes owner;
    FilesRig r("republish");
    put_catalog(r.root / "b.json", {authored_recipe("beta", "src/beta.cpp")});
    put_file(r.root / "notes.txt", "not a catalog\n");
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(),
                                            r.t.host.project_dir);
    ToolSeat* tool = mount_tool(r.t, "alpha");
    open_builder(r.t);
    REQUIRE(tool->described == 1); // the panel opening
    r.open();
    REQUIRE(tool->described == 1); // ...and nothing since

    point_at(r, "b.json");
    use_as_recipes(r);

    CHECK(tool->described == 2);
    // NOTHING ELSE WAS SAID. No build was ordered, and the tool heard exactly one more
    // sentence than it had heard before.
    CHECK(tool->asked.empty());

    // A SECOND CHOICE IS A SECOND ASK AND NOT A SECOND MECHANISM -- including the
    // same-file reload, which is a real replacement and says so on the wire.
    point_at(r, "b.json");
    use_as_recipes(r);
    CHECK(tool->described == 3);

    // ...AND A REFUSAL PUSHES NOTHING, because nothing changed for a panel to be told about.
    point_at(r, "notes.txt");
    use_as_recipes(r);
    CHECK(tool->described == 3);
}

TEST_CASE("PROJ-1: the chooser is an ordinary action on the ordinary binding truth") {
    // NO PRIVATE SHORTCUT. The gesture is a `kActionCatalog` row like every other, so a
    // maker's keymap file can move it, the help surfaces list it, and dispatch reads the
    // same one fact all three read. A hard-coded key here would be the drift KEY-R0
    // measured in six places, reintroduced for one action.
    const ActionRow* row = row_of_id("files.use-recipes");
    REQUIRE(row != nullptr);
    CHECK(row->context == KeyContext::kFiles);
    CHECK(std::string(row->label) == "use as recipes");

    Keymap map;
    CHECK(map.action_for(KeyContext::kFiles, input::scan::kU, input::mod::kNone) ==
          Act::kFilesUseRecipes);
    // ...AND A REMAP MOVES IT, which is what makes the claim above worth anything.
    Keymap remapped;
    REQUIRE(apply_overrides({{"files.use-recipes", "g"}}, legend_mode::kDefault, remapped)
                .accepted);
    CHECK(remapped.action_for(KeyContext::kFiles, input::scan::kG, input::mod::kNone) ==
          Act::kFilesUseRecipes);
    CHECK(remapped.action_for(KeyContext::kFiles, input::scan::kU, input::mod::kNone) ==
          Act::kNone);
}

TEST_CASE("PROJ-1: a live catalog choice is this session's and is written nowhere") {
    // THE PERSISTENCE POSTURE, MEASURED. PROJ-1 deliberately remembers no catalog choice:
    // the next launch chooses exactly as this one did, from `--recipes` or the shipped
    // default. So the fact lives on the `Session`, which is written to no file, and the
    // durable session record a maker's next launch restores says nothing about it.
    CurrentRecipes owner;
    FilesRig r("nopersist");
    const std::string session_file = (r.root / "last-session.json").generic_string();
    r.t.host.session_path = session_file;
    put_catalog(r.root / "a.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_catalog(r.root / "b.json", {authored_recipe("beta", "src/beta.cpp")});
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(),
                                            r.t.host.project_dir);
    REQUIRE(r.t.host.use_recipes((r.root / "a.json").generic_string()).accepted);
    r.open();
    point_at(r, "b.json");
    use_as_recipes(r);
    // THE PROJECTION IS THE OWNER'S OWN ANSWER, read back after the attempt rather than
    // recomposed from the browser's location -- so the screen and the owner cannot come to
    // name two different files (PROJ-2).
    REQUIRE(r.session().recipes_moved_to == (r.root / "b.json").generic_string());
    REQUIRE(owner.source() == (r.root / "b.json").generic_string());

    // WHAT A SAVE ACTUALLY CARRIES, read off the file this Workshop writes on its way
    // out. No catalog path is in it -- not the one in force, not the one that was
    // replaced, not a list of either -- and no new durable artifact appeared beside it.
    r.to_command();
    r.t.key(input::scan::kQ);
    REQUIRE(std::filesystem::exists(session_file));
    const std::string text = bytes_of(session_file);
    CHECK(text.find("a.json") == std::string::npos);
    CHECK(text.find("b.json") == std::string::npos);
    CHECK(text.find("recipe") == std::string::npos);
}

// ============================================================================
// Tier 4 — LOCATION MARKS, AND A BROWSER THAT MAY LEAVE (PROJ-2)
// ============================================================================
//
// THE SUBJECT IS FOUR FACTS STAYING APART. The project anchor says what a relative source
// spelling MEANS; the Files location says where somebody is looking; a mark says a place is
// worth coming back to; and the operating system says what may be read at all. They
// coincide at launch and are separate everywhere else, and almost every case below is a
// falsifier for one of them quietly becoming another.
//
// NOTHING HERE REACHES INSIDE THE PANE. Every location this tier arranges is walked to
// through the browser's own gestures, because the whole subject IS the navigation: a helper
// that assigned `current_dir` would arrange a case the product cannot reach and prove
// nothing about the verbs a maker actually has.

namespace {

/// The three new gestures, as a maker makes them: a bare letter arrives as a key transition
/// AND the character it produced, and the browser's swallow rule expects both.
inline void mark_here(FilesRig& r) {
    r.t.key(input::scan::kM);
    r.t.text("m");
}
inline void next_mark(FilesRig& r) {
    r.t.key(input::scan::kN);
    r.t.text("n");
}
inline void previous_mark(FilesRig& r) {
    r.t.key(input::scan::kN, input::mod::kShift);
    r.t.text("N");
}

/// Point the keys at the browser without activating anything -- the two-press promise, so a
/// case that only wants the pane focused does not open a row by accident.
inline void aim_at_files(FilesRig& r) {
    if (keyboard_context(r.session()) != KeyContext::kFiles) {
        r.press_body(0);
    }
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
}

/// Is `dir` an ancestor of (or equal to) `of`, over the one spelling both are in?
inline bool contains_location(const std::string& dir, const std::string& of) {
    if (dir.empty() || of.rfind(dir, 0) != 0) {
        return false;
    }
    return of.size() == dir.size() || dir.back() == '/' || of[dir.size()] == '/';
}

/// WALK THE BROWSER TO AN ABSOLUTE LOCATION THE WAY A MAKER WOULD: Backspace up to a common
/// ancestor, then Return down through the rows. Fails loudly rather than arriving somewhere
/// else, and never touches the pane's own state.
inline void walk_to(FilesRig& r, const std::filesystem::path& to) {
    const std::string want = to.generic_string();
    aim_at_files(r);
    for (int guard = 0; guard < 64 && !contains_location(r.pane().current_dir, want);
         ++guard) {
        const std::string was = r.pane().current_dir;
        r.t.key(input::scan::kBackspace);
        REQUIRE_MESSAGE(r.pane().current_dir != was, "cannot go up out of ", was);
    }
    REQUIRE(contains_location(r.pane().current_dir, want));
    while (r.pane().current_dir != want) {
        const std::string rest = want.substr(r.pane().current_dir.size());
        const std::size_t at = rest.find('/', 1);
        const std::string step =
            rest.substr(1, at == std::string::npos ? std::string::npos : at - 1);
        point_at(r, step);
        r.t.key(input::scan::kReturn);
    }
    REQUIRE(r.pane().current_dir == want);
}

} // namespace

TEST_CASE("PROJ-2: the marks owner is session truth, and Files is only its first reader") {
    // SC-4, AS A VALUE. The owner is `LocationMarks` on the `Session`, beside `panels` --
    // not inside `FilesPane` -- so a later consumer can ask about remembered places without
    // reaching into a presentation, and `close_panel` cannot destroy one.
    LocationMarks marks;
    marks.origin = "/work/game";
    CHECK(marks.provenance("/work/game") == mark_from::kOrigin);
    CHECK(marks.provenance("/work") == 0);

    // A DUPLICATE COLLAPSES TO ONE MAKER FACT.
    CHECK(marks.remember("/elsewhere/lib"));
    CHECK_FALSE(marks.remember("/elsewhere/lib"));
    CHECK(marks.maker.size() == 1);
    CHECK(marks.marked("/elsewhere/lib"));

    // ONE PLACE, TWO PROVENANCES, AND THEY STAY DISTINCT: a maker may durably mark the very
    // directory this run began in, and forgetting the maker fact leaves origin alone.
    CHECK(marks.remember("/work/game"));
    CHECK(marks.provenance("/work/game") == (mark_from::kOrigin | mark_from::kMaker));
    CHECK(marks.forget("/work/game"));
    CHECK(marks.provenance("/work/game") == mark_from::kOrigin);
    CHECK_FALSE(marks.forget("/work/game")); // forgetting twice is a no-op, not an error

    // THE MARKS ARE SORTED, so the traversal order and the written bytes are the same on
    // every run rather than following the order somebody happened to press `m` in.
    CHECK(marks.remember("/a"));
    CHECK(marks.remember("/z"));
    CHECK(marks.remember("/m"));
    const std::vector<std::string> want{"/a", "/elsewhere/lib", "/m", "/z"};
    CHECK(marks.maker == want);

    // A MARK CARRIES NO OTHER MEANING. The owner's whole surface is places and provenance:
    // there is nowhere here to put a recipe, a project, a grant or a build intent.
    CHECK(marks.provenance("/nowhere/anybody/mentioned") == 0);
}

TEST_CASE("PROJ-2: one address is one traversal stop, however many ways it is known") {
    // FALSIFIER 9. Origin, a maker mark and a filesystem root can all name one directory; a
    // cycle that stopped there three times would stutter, and it is the PROVENANCE that must
    // survive the dedup rather than the duplicate.
    LocationMarks marks;
    marks.origin = "/";
    marks.remember("/");
    marks.remember("/work");
    const std::vector<MarkedPlace> stops = marks.destinations({"/"});
    REQUIRE(stops.size() == 2);
    CHECK(stops[0].path == "/");
    CHECK(stops[0].from == (mark_from::kOrigin | mark_from::kMaker | mark_from::kRoot));
    CHECK(stops[1].path == "/work");
    CHECK(stops[1].from == mark_from::kMaker);
    // THE ORDER IS ORIGIN, THEN THE MAKER'S OWN, THEN THE HOST'S ROOTS -- deterministic, and
    // it does not shuffle when the host reports something different.
    const std::vector<MarkedPlace> more = marks.destinations({"/", "/mnt/x"});
    REQUIRE(more.size() == 3);
    CHECK(more[2].path == "/mnt/x");
    CHECK(more[2].from == mark_from::kRoot);
    // FALSIFIER 10: the roots are an ARGUMENT and are held nowhere. The same owner, asked
    // again with a different answer from the host, gives the different answer.
    CHECK(marks.destinations({}).size() == 2);
    CHECK(provenance_words(more[0].from) == "origin, marked, filesystem root");
    CHECK(provenance_words(0).empty());
}

TEST_CASE("PROJ-2: the host's filesystem roots are asked for, never invented") {
    // SC-7. What this asserts on each family is what that family actually has, and the claim
    // deliberately stops short of "every reachable path".
    const std::vector<std::string> roots = host_filesystem_roots();
    REQUIRE_FALSE(roots.empty());
    for (const std::string& root : roots) {
        // Every reported root is a spelling this application can carry AND is its own
        // lexical parent -- which is what makes it a place `files.parent` stops at.
        CHECK(admit_location(root) == root);
        CHECK(at_filesystem_root(root));
        CHECK(parent_location(root).empty());
    }
#if defined(_WIN32)
    // The mechanism is the logical-drive mask, so every entry is `X:/` -- ASCII by
    // construction, which is why no new narrow conversion becomes load-bearing here -- and
    // the drive this suite is running from is one of them.
    for (const std::string& root : roots) {
        REQUIRE(root.size() == 3);
        CHECK(root[1] == ':');
        CHECK(root[2] == '/');
    }
    const std::string here = std::filesystem::current_path().root_path().generic_string();
    CHECK(std::find(roots.begin(), roots.end(), here) != roots.end());
#else
    const std::vector<std::string> only_slash{"/"};
    CHECK(roots == only_slash);
#endif
}

TEST_CASE("PROJ-2: a maker marks a place, leaves, and comes back to it") {
    // ⭐⭐ THE PHASE'S OTHER CENTRAL CLAIM, THROUGH THE REAL GESTURES. Mark, walk away,
    // traverse back -- and the project anchor is untouched at every step.
    FilesRig r("marklive");
    const std::filesystem::path away = r.root / "away";
    std::filesystem::create_directories(away / "inner");
    put_file(away / "there.cpp", "int t;\n");
    r.t.host.marks_path = (r.root / "marks.json").generic_string();
    const std::string anchor = r.t.host.project_dir;
    r.open();
    REQUIRE(r.pane().current_dir == r.root.generic_string());

    walk_to(r, away);
    mark_here(r);
    CHECK(r.notice() == "marked: " + away.generic_string());
    CHECK(r.session().marks.marked(away.generic_string()));
    // ...AND IT REACHED THE DURABLE FILE IMMEDIATELY, because a mark a maker made and a
    // crash lost would be a promise this application did not keep.
    REQUIRE(std::filesystem::exists(r.t.host.marks_path));
    CHECK(bytes_of(r.t.host.marks_path).find(away.generic_string()) != std::string::npos);

    // WALK SOMEWHERE ELSE ENTIRELY -- above the project, which is now allowed.
    r.t.key(input::scan::kBackspace);
    r.t.key(input::scan::kBackspace);
    REQUIRE(r.pane().current_dir != away.generic_string());
    REQUIRE(r.pane().current_dir != anchor);

    // ...AND TRAVERSE BACK. The destinations are origin, the mark, and this system's roots;
    // whichever the cycle reaches first, `away` is reachable and says why it is known.
    bool reached = false;
    for (int i = 0; i < 8 && !reached; ++i) {
        next_mark(r);
        reached = r.pane().current_dir == away.generic_string();
    }
    CHECK(reached);
    CHECK(r.notice() == "at " + away.generic_string() + " (marked)");
    const std::vector<std::string> there{"inner", "there.cpp"};
    CHECK(r.names() == there);

    // ⭐ A JUMP CHANGED WHERE THE MAKER IS LOOKING AND NOTHING ELSE (falsifier 4).
    CHECK(r.t.host.project_dir == anchor);
    CHECK(r.session().recipes_moved_to.empty());
    CHECK_FALSE(r.session().editor.open_document());
    CHECK(r.session().marks.origin == anchor);

    // AND THE TOGGLE IS A TOGGLE.
    mark_here(r);
    CHECK(r.notice() == "no longer marked: " + away.generic_string());
    CHECK_FALSE(r.session().marks.marked(away.generic_string()));
    CHECK(bytes_of(r.t.host.marks_path).find("away") == std::string::npos);
}

TEST_CASE("PROJ-2: traversal is cyclic, has no standing selection, and both directions work") {
    // SC-6, end to end. The cycle is found from where the browser IS at the gesture, so
    // there is no remembered index to drift out of agreement with the screen.
    FilesRig r("traverse");
    std::filesystem::create_directories(r.root / "one");
    std::filesystem::create_directories(r.root / "two");
    r.t.host.marks_path = (r.root / "marks.json").generic_string();
    r.open();
    walk_to(r, r.root / "one");
    mark_here(r);
    walk_to(r, r.root / "two");
    mark_here(r);
    walk_to(r, r.root);

    const std::vector<MarkedPlace> stops =
        r.session().marks.destinations(host_filesystem_roots());
    REQUIRE(stops.size() >= 3);
    const std::size_t total = stops.size();
    std::size_t at = total;
    for (std::size_t i = 0; i < total; ++i) {
        if (stops[i].path == r.pane().current_dir) {
            at = i;
        }
    }
    REQUIRE(at < total);

    // FORWARD FROM WHERE THE MAKER IS, WRAPPING AT THE END -- a full cycle is the identity.
    for (std::size_t step = 1; step <= total; ++step) {
        next_mark(r);
        CHECK(r.pane().current_dir == stops[(at + step) % total].path);
    }
    CHECK(r.pane().current_dir == stops[at].path);

    // ...AND BACKWARD IS ITS INVERSE.
    previous_mark(r);
    CHECK(r.pane().current_dir == stops[(at + total - 1) % total].path);
    next_mark(r);
    CHECK(r.pane().current_dir == stops[at].path);

    // THERE IS NO STANDING "SELECTED MARK": walking somewhere by hand re-anchors the cycle
    // on where the maker actually IS, and the next jump lands on a known place rather than
    // on the neighbour of a stale index.
    r.t.key(input::scan::kBackspace);
    const std::string walked = r.pane().current_dir;
    REQUIRE(r.session().marks.provenance(walked) == 0);
    next_mark(r);
    CHECK(r.pane().current_dir != walked);
    CHECK(r.pane().current_dir == stops.front().path);
}

TEST_CASE("PROJ-2: origin is generated for the run, is a mark, and is not the project") {
    // SC-3. It coincides with the anchor today and is a DIFFERENT FACT: it is not written
    // down, it does not move when the browser does, and it is not renamed "project".
    FilesRig r("seedmark");
    std::filesystem::create_directory(r.root / "sub");
    r.t.host.marks_path = (r.root / "marks.json").generic_string();
    r.open();
    REQUIRE(r.session().marks.settled);
    CHECK(r.session().marks.origin == r.root.generic_string());
    CHECK(r.session().marks.origin == r.t.host.project_dir);
    // ...AND IT IS VISIBLY A MARK, on the pane's own header.
    CHECK(r.shown().find("origin") != std::string::npos);

    // BROWSING DOES NOT MOVE IT, and the badge follows the LOCATION rather than the pane.
    walk_to(r, r.root / "sub");
    CHECK(r.session().marks.origin == r.root.generic_string());
    CHECK(r.shown().find("origin") == std::string::npos);

    // FALSIFIER 5: ORIGIN IS NOT PERSISTED. Marking somewhere writes the maker's own places
    // and nothing else -- a generated fact in a durable file would come back as a maker's
    // fact on the next run and could never be unmarked.
    mark_here(r);
    REQUIRE(std::filesystem::exists(r.t.host.marks_path));
    const std::string written = bytes_of(r.t.host.marks_path);
    CHECK(written.find((r.root / "sub").generic_string()) != std::string::npos);
    CHECK(written.find("origin") == std::string::npos);
    CHECK(written.find(r.root.generic_string() + "\"") == std::string::npos);
}

TEST_CASE("PROJ-2: origin is the ADMITTED spelling of the launch location, not the raw one") {
    // ⭐ THE ONE ADMISSION WHOSE ABSENCE NOTHING ELSE CATCHES. `HostContext::project_dir` is a
    // plain field a host fills in, and origin is compared to the browser's location BY BYTES
    // -- so an origin taken raw would be an origin the maker can never get back to: walking
    // away and back produces the NORMALIZED spelling, and the two would never match again.
    FilesRig r("seedadmit");
    std::filesystem::create_directory(r.root / "sub");
    r.t.host.project_dir = (r.root / "sub" / "..").generic_string(); // ".../sub/.."
    REQUIRE(r.t.host.project_dir != r.root.generic_string());
    r.open();
    CHECK(r.session().marks.origin == r.root.generic_string());
    CHECK(r.pane().current_dir == r.root.generic_string());
    CHECK(r.shown().find("origin") != std::string::npos);
    // AND THE BADGE COMES BACK, which is the whole value of one spelling: walk down, walk up,
    // and this run still knows where it began.
    walk_to(r, r.root / "sub");
    CHECK(r.shown().find("origin") == std::string::npos);
    r.t.key(input::scan::kBackspace);
    CHECK(r.pane().current_dir == r.root.generic_string());
    CHECK(r.shown().find("origin") != std::string::npos);
}

TEST_CASE("PROJ-2: a run with no origin can still reach a place it remembers") {
    // SC-3's absence arm, and §1.3's "seeded later by jumping". No launch location this
    // build can carry means no origin -- and nothing is invented in its place. What a maker
    // gets instead is the durable places they already kept.
    TempDir dir("noorigin");
    const std::filesystem::path root = dir.path();
    const std::filesystem::path kept = root / "kept";
    std::filesystem::create_directories(kept);
    const std::string marks_file = (root / "marks.json").generic_string();
    put_file(marks_file, marks_persist::to_text({kept.generic_string()}));

    FilesRig r("noorigin", /*with_project=*/false);
    r.t.host.marks_path = marks_file;
    r.open();
    REQUIRE(r.session().marks.settled);
    CHECK(r.session().marks.origin.empty());
    CHECK(r.pane().current_dir.empty());
    CHECK_FALSE(r.listing().known);
    // THE PANE TAKES THE KEYS, because there is now something to do in it: this run knows a
    // place. (With no origin AND no marks it still declines, which is the one residual
    // `docs/workshop/limitations.md` names.)
    r.press_body(0);
    REQUIRE(keyboard_context(r.session()) == KeyContext::kFiles);
    next_mark(r);
    CHECK(r.pane().current_dir == kept.generic_string());
    CHECK(r.listing().known);
    CHECK(r.notice() == "at " + kept.generic_string() + " (marked)");
    // ...AND STILL NO ORIGIN WAS INVENTED FOR IT.
    CHECK(r.session().marks.origin.empty());
    CHECK(r.t.host.project_dir.empty());
}

TEST_CASE("PROJ-2: marks survive a restart, and the browsing location deliberately does not") {
    // ⭐ SC-14, THE PERSISTENCE BOUNDARY, PROVED IN BOTH DIRECTIONS AT ONCE. Falsifiers 6
    // and 7 are its two halves: a mark that did not come back, and a browsing location that
    // did.
    TempDir dir("restart");
    const std::filesystem::path root = dir.path();
    std::filesystem::create_directories(root / "keep");
    std::filesystem::create_directories(root / "elsewhere");
    const std::string marks_file = (root / "marks.json").generic_string();

    {
        FilesRig r("restart-a");
        r.root = root; // the SAME project across both runs, which is what a restart is
        r.t.host.project_dir = root.generic_string();
        r.t.host.marks_path = marks_file;
        r.open();
        walk_to(r, root / "keep");
        mark_here(r);
        REQUIRE(r.session().marks.marked((root / "keep").generic_string()));
        // ...and then wander off somewhere that is NOT a mark, and leave.
        walk_to(r, root / "elsewhere");
        REQUIRE_FALSE(r.session().marks.marked((root / "elsewhere").generic_string()));
    }

    REQUIRE(std::filesystem::exists(marks_file));
    {
        FilesRig r("restart-b");
        r.root = root;
        r.t.host.project_dir = root.generic_string();
        r.t.host.marks_path = marks_file;
        r.open();
        // THE MARK CAME BACK.
        CHECK(r.session().marks.marked((root / "keep").generic_string()));
        // THE BROWSING LOCATION DID NOT: a new run begins at a freshly generated origin,
        // which is WUX-0's split -- a remembered reference is not unfinished work.
        CHECK(r.pane().current_dir == root.generic_string());
        CHECK(r.session().marks.origin == root.generic_string());
        CHECK_FALSE(r.session().marks.marked((root / "elsewhere").generic_string()));
        // ...AND NO TRAVERSAL POSITION CAME BACK EITHER: the first jump is found from where
        // the browser IS, so there was never an index for a run to have written down.
        CHECK(bytes_of(marks_file).find("elsewhere") == std::string::npos);
    }
}

TEST_CASE("PROJ-2: a persisted mark is admitted, never re-based, and never quietly dropped") {
    // SC-5's refusal law, and falsifiers 8 and 22 in one arrangement.
    TempDir dir("markfile");
    const std::filesystem::path root = dir.path();
    const std::string good = (root / "kept").generic_string();

    // A HAND-EDITED FILE with three kinds of row: one usable, one RELATIVE, one empty.
    put_file(root / "marks.json", marks_persist::to_text({good, "relative/place", ""}));
    const marks_persist::LoadedMarks loaded =
        marks_persist::load_file((root / "marks.json").generic_string());
    REQUIRE(loaded.outcome.accepted);
    // ⭐ THE RELATIVE ROW IS REFUSED AND IS NOT RESOLVED AGAINST ANYTHING. A mark re-based
    // against the process's own footing would mean a different directory on every launch,
    // which is the two-bases defect `persist::resolved_against` exists to end.
    const std::vector<std::string> only_good{good};
    CHECK(loaded.maker == only_good);
    CHECK(loaded.skipped.find("relative/place") != std::string::npos);
    CHECK(loaded.skipped.find("absolute") != std::string::npos);

    // ...AND A PLACE THAT IS SIMPLY NOT THERE IS KEPT. Existence is never tested: an
    // unplugged drive or a tree not checked out yet is a temporary answer, and deleting a
    // maker's durable fact on the strength of one is the silent loss this law forbids.
    CHECK_FALSE(std::filesystem::exists(good));
    const std::string gone = abs_spelling("/definitely/not/here/at/all");
    put_file(root / "absent.json", marks_persist::to_text({gone}));
    const marks_persist::LoadedMarks kept =
        marks_persist::load_file((root / "absent.json").generic_string());
    REQUIRE(kept.outcome.accepted);
    const std::vector<std::string> want{gone};
    CHECK(kept.maker == want);
    CHECK(kept.skipped.empty());

    // A MISSING FILE IS NO MARKS -- deleting it is how a maker forgets everywhere at once.
    CHECK_FALSE(
        marks_persist::load_file((root / "nope.json").generic_string()).outcome.accepted);

    // A FILE THAT SAYS IT IS SOMETHING ELSE IS REFUSED WHOLE, by its own claim, before its
    // rows are judged -- the family's law, and a different answer from a bad row.
    put_file(root / "wrong.json", prefs_persist::to_text(true));
    CHECK_FALSE(
        marks_persist::load_file((root / "wrong.json").generic_string()).outcome.accepted);

    // ...AND WRITING IS OBSERVATION: a saved list read back is the same list, in the same
    // order, so a second save of a loaded file is the same bytes.
    CHECK(marks_persist::from_text(marks_persist::to_text(kept.maker)).maker == want);
}

TEST_CASE("PROJ-2: a marks file this run could not read is never overwritten") {
    // ⭐ THE PREFS FILE'S LAW, ONE FILE OVER. A refusal a maker can act on must not become a
    // file a maker has lost: the run says it could not read their places, holds none, and
    // the next `m` writes nothing.
    FilesRig r("markrefused");
    const std::string marks_file = (r.root / "marks.json").generic_string();
    put_file(marks_file, "these are notes, not marks\n");
    r.t.host.marks_path = marks_file;
    r.open();
    CHECK(r.session().marks.maker.empty());
    // ⚠ IT IS A STANDING CONDITION AND NOT A NOTICE (WUX-4): an unreadable file is still
    // unreadable an hour later and has a maker action, and the gesture that opens the pane
    // says "opened Files" immediately after this load -- so a sentence on the notice row
    // would be replaced in the same turn, every time.
    const Condition* wall = r.session().conditions.find(kMarksWallKey);
    REQUIRE(wall != nullptr);
    CHECK(wall->role == surface::role::kAlert);
    CHECK(wall->detail.find("left exactly as it is") != std::string::npos);

    aim_at_files(r);
    mark_here(r);
    CHECK(bytes_of(marks_file) == "these are notes, not marks\n");
    // The LIVE set still moved, which is what makes this a refusal rather than a lockout.
    CHECK(r.session().marks.marked(r.root.generic_string()));
}

TEST_CASE("PROJ-2: a location that cannot be listed is a refusal, not a substitution") {
    // SC-2 and §2.2: the maker is told the truth about where they are, keeps the lexical
    // location, and can navigate back out -- nothing falls back to the project or to origin.
    FilesRig r("unlistable");
    const std::filesystem::path vanishes = r.root / "vanishes";
    std::filesystem::create_directory(vanishes);
    r.open();
    walk_to(r, vanishes);
    REQUIRE(r.listing().known);

    std::filesystem::remove(vanishes);
    r.t.key(input::scan::kR); // look again
    r.t.text("r");
    CHECK(r.pane().current_dir == vanishes.generic_string()); // it did NOT move
    CHECK_FALSE(r.listing().known);
    CHECK_FALSE(r.listing().refusal.empty());
    CHECK(r.listing().rows.empty());
    CHECK(r.listing().refusal.find(vanishes.generic_string()) != std::string::npos);
    // ...AND PARENT STILL WORKS, because the location is lexical truth rather than a
    // successful enumeration. A browser that had bounced back to origin would have taken
    // the maker's own place away to hide a refusal.
    r.t.key(input::scan::kBackspace);
    CHECK(r.pane().current_dir == r.root.generic_string());
    CHECK(r.listing().known);
}

TEST_CASE("PROJ-2: an external file opens and saves through the ONE editor door") {
    // ⭐ SC-10 AND FALSIFIER 17. The maker walks out of the project entirely and edits a file
    // there; the document identity is that file's own absolute path, the save reaches that
    // file, and the anchor is exactly where it was.
    TempDir outside("external");
    const std::filesystem::path foreign = outside.path();
    put_file(foreign / "foo.cpp", "one\ntwo\n");

    FilesRig r("extopen");
    const std::string anchor = r.t.host.project_dir;
    r.open();
    walk_to(r, foreign);
    point_at(r, "foo.cpp");
    r.t.key(input::scan::kReturn);

    const EditorState& e = r.session().editor;
    REQUIRE(e.open_document());
    // THE IDENTITY IS THE EXTERNAL FILE'S OWN ABSOLUTE PATH -- the project was NOT applied
    // to it, and could not have been: `resolved_against` returns early for an absolute
    // spelling, which is what makes the door referrer-blind.
    CHECK(e.path == (foreign / "foo.cpp").generic_string());
    CHECK(e.path.find(anchor) == std::string::npos);
    CHECK(e.buffer.line(0) == "one");

    // EDIT AND SAVE: the bytes reach THAT file, and nothing is created under the project.
    r.t.key(input::scan::kEnd);
    r.t.text("!");
    r.t.key(input::scan::kS, input::mod::kCtrl);
    CHECK(bytes_of((foreign / "foo.cpp").generic_string()) == "one!\ntwo\n");
    CHECK_FALSE(std::filesystem::exists(std::filesystem::path(anchor) / "foo.cpp"));
    // ...AND OPENING FOREIGN SOURCE MADE NOTHING ABOUT IT PART OF THE PROJECT.
    CHECK(r.t.host.project_dir == anchor);
    CHECK(r.session().marks.origin == anchor);
    CHECK_FALSE(r.session().marks.marked(foreign.generic_string()));
}

TEST_CASE("PROJ-2: an external catalog is chosen live, and the project still owns relative sources") {
    // ⭐⭐ FALSIFIERS 18 AND 19, THROUGH THE MAKER'S ACTUAL DOOR. PROJ-1 pinned the two-base
    // decoy at `install_recipes`; this repeats it at the live gesture from OUTSIDE the
    // project, which is the arrangement that only became reachable now.
    TempDir outside("foreign-catalog");
    const std::filesystem::path foreign = outside.path();
    std::filesystem::create_directories(foreign / "src");
    put_file(foreign / "src" / "thing.cpp", "the decoy\n");
    put_catalog(foreign / "recipes.json", {authored_recipe("thing", "src/thing.cpp")});

    CurrentRecipes owner;
    FilesRig r("extcatalog");
    const std::string anchor = r.t.host.project_dir;
    std::filesystem::create_directories(r.root / "src");
    put_file(r.root / "src" / "thing.cpp", "the project\n");
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(), anchor);
    r.open();
    walk_to(r, foreign);
    point_at(r, "recipes.json");
    use_as_recipes(r);

    REQUIRE(owner.source() == (foreign / "recipes.json").generic_string());
    REQUIRE(owner.all().size() == 1);
    // ⭐ THE PROJECT'S FILE WINS, NOT THE CATALOG'S NEIGHBOUR -- in the completed recipe AND
    // in the generated project, which is where the two would actually have diverged.
    CHECK(owner.all()[0].single_source->source ==
          (r.root / "src" / "thing.cpp").generic_string());
    CHECK(owner.all()[0].single_source->source.find(foreign.generic_string()) ==
          std::string::npos);
    CHECK(zengine::builder::generated_project(owner.all()[0])
              .find((r.root / "src" / "thing.cpp").generic_string()) != std::string::npos);
    // ...AND CHOOSING A CATALOG SOMEWHERE ELSE DID NOT MOVE THE PROJECT, or the origin.
    CHECK(r.t.host.project_dir == anchor);
    CHECK(r.session().marks.origin == anchor);
    // THE STANDING PROJECTION NAMES THE CHOSEN FILE UNAMBIGUOUSLY (SC-12): the owner's own
    // absolute path, not a based spelling with no stated base.
    CHECK(r.session().recipes_moved_to == (foreign / "recipes.json").generic_string());
}

TEST_CASE("PROJ-2: fitting a path keeps the end that says which file it is") {
    // SC-13 AND FALSIFIER 21, as a value. The property, stated once: enough root to say
    // WHICH filesystem, a mark where something was removed, and the useful tail.
    const std::string p = "/home/me/code/very/long/project/src/foo.cpp";
    CHECK(detail::fit_path(p, 200) == p); // it fits: nothing changes, not even a mark
    const std::string cut = detail::fit_path(p, 26);
    CHECK(cut.size() <= 26);
    CHECK(cut.rfind("/", 0) == 0);                         // the root cue survives
    CHECK(cut.find(detail::kElided) != std::string::npos); // the cut marks itself
    CHECK(cut.find("foo.cpp") != std::string::npos);       // the leaf survives
    // ...AND THE ORDINARY MEASURER IS THE FALSIFIER: it removes exactly the half a path
    // carries its meaning in.
    CHECK(detail::fit(p, 26).find("foo.cpp") == std::string::npos);

    // A WINDOWS DRIVE IS A CUE TOO, and a UNC name is the whole `//server/`.
    const std::string w = "C:/Users/me/code/very/long/project/src/foo.cpp";
    CHECK(detail::fit_path(w, 26).rfind("C:/", 0) == 0);
    CHECK(detail::fit_path(w, 26).find("foo.cpp") != std::string::npos);
    const std::string unc = "//server/share/deep/deeper/deepest/foo.cpp";
    CHECK(detail::fit_path(unc, 28).rfind("//server/", 0) == 0);
    CHECK(detail::fit_path(unc, 28).find("foo.cpp") != std::string::npos);

    // TOTAL AT EVERY WIDTH, including ones no pane has: below the shape's own cost it falls
    // back to the mark-only answer rather than underflowing.
    for (std::int64_t width = -2; width < 12; ++width) {
        const std::string got = detail::fit_path(p, width);
        CHECK(got.size() <= static_cast<std::size_t>(width < 0 ? 0 : width));
    }
    // AND IT CHANGES NO IDENTITY: this is a projection of a string, and the string is the
    // caller's own.
    CHECK(p == "/home/me/code/very/long/project/src/foo.cpp");
}

TEST_CASE("PROJ-2: the browser's header says where you are and why that place is known") {
    // SC-2's presentation half: the LOCATION is absolute and takes the tail, the provenance
    // is a word rather than a badge that costs the path, and neither displaces the position.
    FilesRig r("header");
    put_file(r.root / "a.cpp", "int a;\n");
    r.open();
    const std::string head = files_header(
        r.pane(), provenance_words(r.session().marks.provenance(r.pane().current_dir)),
        false, 200);
    CHECK(head.rfind("Files 1/1", 0) == 0);
    CHECK(head.find("origin") != std::string::npos);
    CHECK(head.find(r.root.generic_string()) != std::string::npos);
    // UNDER A REAL BUDGET the location is fitted by the PATH measurer, and the row fits.
    const std::string tight = files_header(r.pane(), std::string("origin"), false, 30);
    CHECK(tight.size() <= 30);
    CHECK(tight.rfind("Files 1/1", 0) == 0);
    // AND A LOCATION NOBODY MARKED SAYS NOTHING EXTRA -- absence is the ordinary case.
    FilesPane plain;
    plain.current_dir = "/somewhere/plain";
    plain.listing.known = true;
    CHECK(files_header(plain, std::string(), false, 200) == "Files empty  /somewhere/plain");
}

TEST_CASE("PROJ-2: the browser's location is never written to a maker's durable files") {
    // SC-14's negative, widened: the setup, the session and the marks file each say nothing
    // about where somebody was standing.
    FilesRig r("noleak");
    std::filesystem::create_directory(r.root / "wandered");
    const std::string session_file = (r.root / "last-session.json").generic_string();
    r.t.host.session_path = session_file;
    r.t.host.marks_path = (r.root / "marks.json").generic_string();
    r.open();
    walk_to(r, r.root / "wandered");

    CHECK(setup_persist::to_text(r.session().setup.active).find("wandered") ==
          std::string::npos);
    r.to_command();
    r.t.key(input::scan::kQ);
    REQUIRE(std::filesystem::exists(session_file));
    CHECK(bytes_of(session_file).find("wandered") == std::string::npos);
    CHECK(bytes_of(session_file).find("marks") == std::string::npos);
    // ...AND NO MARKS FILE WAS EVEN CREATED, because nobody marked anything.
    CHECK_FALSE(std::filesystem::exists(r.t.host.marks_path));
}

TEST_CASE("PROJ-2: the three mark gestures are ordinary rows on the one binding truth") {
    // SC-5's "through ordinary action/keymap truth", and KEY-0's law: no private shortcut
    // system, no gesture a keymap file cannot move, and none shipped inside a POSIX gap.
    for (const char* id : {"files.mark", "files.next-mark", "files.previous-mark"}) {
        const ActionRow* row = row_of_id(id);
        REQUIRE_MESSAGE(row != nullptr, "no action row declares `", id, "`");
        CHECK(row->context == KeyContext::kFiles);
        CHECK(posix_gap(row->gesture) == nullptr);
    }
    // THE DEFAULTS COLLIDE WITH NOTHING, which is the admission law's own check run over the
    // whole effective map rather than asserted by hand.
    Keymap fresh;
    CHECK(apply_overrides({}, legend_mode::kDefault, fresh).accepted);
    // ...AND A MAKER WHO MOVES ONE GETS THEIR OWN BINDING, here and on every help surface.
    Keymap moved;
    REQUIRE(apply_overrides({{"files.mark", "k"}}, legend_mode::kDefault, moved).accepted);
    CHECK(moved.action_for(KeyContext::kFiles, input::scan::kK, input::mod::kNone) ==
          Act::kFilesMark);
    CHECK(moved.action_for(KeyContext::kFiles, input::scan::kM, input::mod::kNone) ==
          Act::kNone);
}
