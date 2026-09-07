// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Files package's own suite -- the browser's pure half, proved where it now lives.
//
// The listing, the maker's marks, their durable file and path admission are the same code
// the built-in spent; what this suite proves is that they behave in the FILES PACKAGE's own
// image -- linked against `zengine-files-os` (the two platform bodies) and the Workshop
// vocabulary, and NOT against `zengine-workshop-logic`. Most of these cases came here whole
// from `workshop_files` when the browser became a weave: the claim did not change, the
// office that owns it did.
//
// WHAT IS NOT HERE is the pane's INTERACTION -- a press that selects, a key that enters a
// directory, the header a maker reads. That was the built-in's presentation and is the
// weave's now; it is proved through the real pane seam, where the seam is.

#include "files/vocabulary.hpp"

#include "files/files.hpp"
#include "files/filesystem_roots.hpp"
#include "files/marks_persist.hpp"
#include "workshop/path_admission.hpp"
#include "workshop/prefs_persist.hpp"

#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace zengine::workshop;

namespace {

/// A scratch directory this run owns, removed with the case that made it. The Workshop
/// suites' `TempDir` under a root of their own; this suite links none of that rig, so it
/// carries the three lines it actually needs.
class TempDir {
public:
    explicit TempDir(const char* tag) {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("zen-files-" + std::string(tag) + "-" + std::to_string(++counter));
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_, ec);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const std::filesystem::path& path() const { return path_; }
    std::string dir() const { return path_.generic_string(); }

private:
    std::filesystem::path path_;
};

/// AN ABSOLUTE SPELLING THIS PLATFORM AGREES IS ONE. A rooted path with no drive is not
/// absolute on Windows, so a case that means "absolute" says it in the platform's terms.
inline std::string abs_spelling(const std::string& tail) {
#if defined(_WIN32)
    return "C:" + tail;
#else
    return tail;
#endif
}

inline std::string root_spelling() { return abs_spelling("/"); }

inline void put_file(const std::filesystem::path& at, const std::string& bytes);

/// AN ENTRY WHOSE NAME THIS PLATFORM WILL NOT SPELL BACK. Windows gets a lone surrogate --
/// a name the filesystem accepts and no code page can hold; POSIX gets an invalid UTF-8
/// byte. Answers false where the platform refused to make one.
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
    put_file(dir / std::filesystem::path(std::string("loneÿ.txt")), "x");
    return true;
#endif
}

inline void put_file(const std::filesystem::path& at, const std::string& bytes) {
    std::ofstream out(at, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}

/// A whole source file as bytes -- for the two claims here that are about the BUILD GRAPH
/// and can only be asked of the files that state it.
inline std::string file_bytes(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

// ---- The package's own durable names ------------------------------------------------

TEST_CASE("the durable names are what a saved setup will hold") {
    CHECK(std::string(zengine::files::kFilesRole) == "zengine.files");
    CHECK(std::string(zengine::files::kProjectFilesPane) == "project-files");
    CHECK(std::string(zengine::files::kFilesStem) == "zengine-files");
    // The action ids are the built-in's own, so a maker's authored keymap keeps working.
    CHECK(std::string(zengine::files::kActionUp) == "files.up");
    CHECK(std::string(zengine::files::kActionPickBuildable) == "files.pick-buildable");
}

TEST_CASE("a FilesState is a shape a same-shape reload keeps") {
    const auto shape = loom::schema_of<zengine::files::FilesState>();
    REQUIRE(shape != nullptr);
    CHECK(shape->name() == "FilesState");
    CHECK(shape->version() == 1u);
    REQUIRE(shape->fields().size() == 2);
    CHECK(shape->fields()[0].name == "current_dir");
    CHECK(shape->fields()[1].name == "cursor");
}

TEST_CASE("row_at is bounded at use") {
    TempDir dir("rowat");
    put_file(dir.path() / "one.txt", "x");
    const Listing listing = enumerate_directory(dir.dir());
    REQUIRE(listing.rows.size() == 1);
    CHECK(row_at(listing, 0) != nullptr);
    CHECK(row_at(listing, 1) == nullptr);
    CHECK(row_at(listing, 999) == nullptr);
}

// ---- The build graph, read as the two files that are it -------------------------------

TEST_CASE("FILES-WEAVE: the image links no host target, and the host builds no browser") {
    // ⭐ THE STRUCTURAL HALF OF "IT IS A WEAVE NOW", and it is a claim about the BUILD
    // GRAPH rather than about behaviour -- so it is asked of the two files that ARE the
    // graph. A link edge to the host's compiled logic would still leave every case in this
    // repository green for a long while, and would quietly make this package a second copy
    // of Workshop rather than a stranger to it.
    //
    // WHAT IT MAY LINK is the portable half every loaded tool links: the vocabulary
    // interface target (include paths and shapes), the editable line, the key numbers, the
    // activation cursor, and its own two platform bodies. What it may NOT link is anything
    // that carries a `Session`, a screen, a keymap or a host.
    const std::string files = file_bytes(FILES_CMAKE);
    REQUIRE_FALSE(files.empty());
    // THE EDGES, READ OUT OF THE ONE CALL THAT STATES THEM. Reading the whole file would
    // make prose about a target into a link to it -- and this file's prose says out loud
    // which targets it deliberately does NOT take, which is the sentence worth keeping.
    const std::size_t call = files.find("target_link_libraries(zengine-files ");
    REQUIRE(call != std::string::npos);
    const std::size_t end = files.find(")", call);
    REQUIRE(end != std::string::npos);
    const std::string edges = files.substr(call, end - call);
    for (const char* forbidden : {"zengine-workshop-logic", "zengine-workshop-load",
                                  "zengine-workshop-session-history", "loom::kernel"}) {
        CHECK_MESSAGE(edges.find(forbidden) == std::string::npos, "files/CMakeLists.txt links `",
                      forbidden, "`");
    }
    // ...AND WHAT IT DOES LINK IS SAID, so a green here is a measurement of a real edge set
    // rather than of a file that happens to mention nothing.
    CHECK(files.find("zengine_weave(zengine-files files.cpp)") != std::string::npos);
    CHECK(edges.find("zengine-workshop-vocabulary") != std::string::npos);

    // THE HOST COMPILES NO BROWSER, said from the other side. The two platform bodies the
    // pure half needs are compiled in THIS package (`files/os.cpp`), and the host's own
    // source list must not name the files they came out of.
    const std::string host = file_bytes(WORKSHOP_CMAKE);
    REQUIRE_FALSE(host.empty());
    for (const char* forbidden : {"files.cpp", "filesystem_roots.cpp", "screen_browser.cpp", "FilesPane"}) {
        CHECK_MESSAGE(host.find(forbidden) == std::string::npos,
                      "workshop/CMakeLists.txt still compiles `", forbidden, "`");
    }
    // ...AND IT STAGES THE IMAGE BESIDE ITSELF, because a plan row naming an artifact a
    // deployed binary cannot find is a pane that silently never arrives.
    CHECK(host.find("zengine-files") != std::string::npos);
}

// ---- WHAT THE MAKER IS TOLD ----------------------------------------------------------

TEST_CASE("PROJ-1: a refusal says what went wrong AND what is still running, in that order") {
    // ⭐ THE SENTENCE THE LIVE WITNESS CORRECTED, carried out of the built-in and kept
    // measured. The notice row is cut at the band's width, so the half a maker most needs
    // -- "you did not just lose your recipes" -- must not be the half that elides. The
    // ORDER is the claim; the wording is only how it is said.
    const std::string said =
        catalog_refused_words("no `recipes` array", "/project/a.json");
    CHECK(said.rfind("not a recipe catalog", 0) == 0u);
    CHECK(said.find("the recipes in force are unchanged") != std::string::npos);
    CHECK(said.find("the recipes in force are unchanged") < said.find("no `recipes` array"));
    CHECK(said.find("no `recipes` array") < said.find("still using /project/a.json"));
    // ...AND THE FIXED HALF SURVIVES A CUT THE VARIABLE HALVES DO NOT. The pane's own row
    // is fitted at the band's width; a plain truncation is the same question asked of the
    // ORDER, which is the property, without borrowing the host's fitter to ask it.
    CHECK(said.substr(0, 60).find("the recipes in force are unchanged") != std::string::npos);
    CHECK(said.substr(0, 60).find("still using") == std::string::npos);

    // AN OWNER HOLDING NOTHING IS SAID IN WORDS, never as an empty tail a maker has to
    // read a missing path out of.
    CHECK(catalog_refused_words("unreadable", std::string()).find("still using no catalog") !=
          std::string::npos);
    // AND THE AUTHORING HALF SAYS THE SAME TWO THINGS FOR ITS OWN ACT.
    const std::string wrote = authoring_refused_words("id already taken", "/project/a.json");
    CHECK(wrote.rfind("no recipe was written", 0) == 0u);
    CHECK(wrote.find("still using /project/a.json") != std::string::npos);
}

TEST_CASE("PROJ-1: an accepted catalog names the file in force and how much it holds") {
    CHECK(catalog_taken_words("/project/b.json", 2) ==
          "build recipes: /project/b.json (2 recipes)");
    // ONE IS SAID IN THE SINGULAR, because a maker reads this row and not a counter.
    CHECK(catalog_taken_words("/project/b.json", 1) ==
          "build recipes: /project/b.json (1 recipe)");
    CHECK(catalog_taken_words("/project/b.json", 0) ==
          "build recipes: /project/b.json (0 recipes)");
    // AND AN AUTHORED ROW SAYS WHAT THE MAKER CALLED IT AND WHAT IT PRODUCES -- the two
    // halves they just typed, so the row they wrote is the row they can see.
    CHECK(authored_words("oven", "zengine-oven", "/project/b.json", 3) ==
          "authored recipe `oven` -> zengine-oven in /project/b.json (3 recipes)");
}

TEST_CASE("PROJ-1: a row that cannot be a catalog is refused before the owner is troubled") {
    // FOUR ARMS, AND EVERY ONE OF THEM SAYS NOTHING MOVED. A bare reason would leave a
    // maker guessing whether the gesture had already cost them the catalog in force.
    FileRow file;
    file.name = "recipes.json";
    FileRow dir;
    dir.name = "somewhere";
    dir.directory = true;
    FileRow unsayable;
    unsayable.name = "cafÃ©";
    unsayable.openable = false;

    CHECK(catalog_row_refusal(nullptr, true) ==
          "no row is selected -- the recipes in force are unchanged");
    CHECK(catalog_row_refusal(&dir, true).find("is a directory") != std::string::npos);
    CHECK(catalog_row_refusal(&unsayable, true).find("cannot carry in a path") !=
          std::string::npos);
    CHECK(catalog_row_refusal(&unsayable, true).find("the recipes in force are unchanged") !=
          std::string::npos);
    CHECK(catalog_row_refusal(&file, false) ==
          "this run began nowhere -- the recipes in force are unchanged");
    // ...AND AN ORDINARY FILE IN A RUN THAT BEGAN SOMEWHERE IS NOT REFUSED AT ALL, which
    // is the arm that makes the other four a measurement.
    CHECK(catalog_row_refusal(&file, true).empty());
    // THE UNSAYABLE NAME IS SHOWN THE WAY EVERY OTHER ROW SHOWS IT, and never raw.
    CHECK(catalog_row_refusal(&unsayable, true).find(shown_name(unsayable.name)) !=
          std::string::npos);
}

// ---- The pure half, moved from `workshop_files` with the office that owns it ----------

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
