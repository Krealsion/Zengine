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
#include "workshop/authoring.hpp"
#include "workshop/load_persist.hpp"
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

// THE THREE DOORS THE FILES WEAVE ASKS. Two of them are this host's own weaves
// (`workshop/pane_doors.hpp`); the third is Workshop's, answered where `open_source` lives.
// The pane is a loaded image now, so what this suite owes is the HOST's half of each seam:
// who may ask, who is answered, and whose words the answer is in.
#include "workshop/pane_doors.hpp"

// ...AND WHICH OF THOSE TWO OWNED FACTS THIS HOST MAKES ROUTABLE (SOURCE-0). The project
// anchor and the current recipe catalog are this suite's two subjects already; the Sources
// over them are asked here, against the real owners, through the real door.
#include "workshop/host_sources.hpp"
#include "operator/source.hpp"

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

/// WHICH ARM MADE A LINKED DIRECTORY, so a case can say what a lane exercised.
enum class LinkArm { none, symbolic_link, junction };

/// A DIRECTORY THAT LEAVES THE TREE, made the strongest way this platform allows.
///
/// POSIX gets an ordinary directory symlink. Windows tries one first, and the attempt fails on
/// both of its standard libraries for different reasons -- MSVC's STL refuses for privilege (a
/// symbolic link needs one, and this session does not hold it); libstdc++ does not implement
/// `create_directory_symlink` at all -- so on either library the JUNCTION arm follows. That is
/// the case that matters there anyway: `mklink /J` needs no privilege, and a junction is the
/// entry whose `is_symlink()` answers FALSE while it is still a directory that leaves the tree.
/// Returns `none` when the platform made neither, so a case can say so rather than passing
/// quietly on a boundary nobody arranged.
inline LinkArm make_linked_directory(const std::filesystem::path& link,
                                     const std::filesystem::path& target) {
    std::error_code ec;
    std::filesystem::create_directory_symlink(target, link, ec);
    if (!ec) {
        return LinkArm::symbolic_link;
    }
#if defined(_WIN32)
    // `mklink /J` is the ordinary way a person makes one, and it is the reparse point this
    // application's `linked` predicate was written against (EDIT-1 measured it).
    const std::string command = "cmd /c mklink /J \"" + link.string() + "\" \"" +
                                target.string() + "\" >nul 2>&1";
    if (std::system(command.c_str()) == 0) {
        std::error_code exists_ec;
        if (std::filesystem::exists(link, exists_ec) && !exists_ec) {
            return LinkArm::junction;
        }
    }
#endif
    return LinkArm::none;
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

/// A PROJECT THIS CASE MADE, WITH A WORKSHOP STANDING IN IT -- what `FilesRig` was, minus
/// the pane the browser used to be.
///
/// `project_dir` IS SET THE WAY THE HOST SETS IT: one string on the `HostContext`, before
/// the weave runs. That is the whole of what a launch directory is to this application.
///
/// THE GESTURES THAT USED TO REACH THE HOST THROUGH THE BROWSER are a weave's now -- `u`
/// and a Return on a row cross as `RecipeUseRequested` and `OpenSourceRequested` -- so a
/// case about the HOST's half of one calls that half, through the very closure the door
/// spends. What the maker's hand does is the pane seam's claim and is proved there.
struct ProjectRig {
    Live t;
    TempDir dir;
    std::filesystem::path root;

    explicit ProjectRig(const char* tag = "project", bool with_project = true) : dir(tag) {
        root = dir.path();
        if (with_project) {
            t.host.project_dir = root.generic_string();
        }
        resize_screen(40);
    }

    void resize_screen(std::int64_t h) {
        t.publish(loom::to_value(surface::SurfaceExtent{78, h, 0, 0}));
    }
    const Session& session() const { return t.w->session(); }
    std::string notice() const { return session().notice; }
    std::string shown() const { return stack_text(t.canvases.back()); }

    /// Put the keys where a maker's ordinary commands land, by pressing a panel that is
    /// always there. Kept from `FilesRig` unchanged -- it never went through the browser.
    void to_command() {
        const Session& s = session();
        // THE LAYOUTS PANE, which is on every desk and takes no keyboard. It was Info until
        // Info became a weave, and a weave's pane takes the keys -- which is the opposite of
        // what this helper is for.
        const ui::Rect band =
            cells_covered(bounds_of(s.panels, s.setup.active, panel::kLayouts, screen_of(s)).rect);
        t.press_canvas(band.x + band.w - 1, band.y);
        REQUIRE(keyboard_context(session()) == KeyContext::kCommand);
    }
};

/// THE PROJECT DOOR WITH THE RECIPE SEAM WIRED, mounted the way `workshop.cpp` mounts it --
/// after `host.recipe_source` is set, because the door captures it by value.
inline void mount_project_door_with_source(ProjectRig& r) {
    const std::string marks = (r.root / "workshop-marks.json").generic_string();
    auto door = std::make_unique<ProjectDoor>(r.t.host.project_dir, marks, ProjectDoor::Frontier{},
                                              ProjectDoor::Names{}, r.t.host.recipe_source);
    ProjectDoor* raw = door.get();
    loom::Grant grant;
    grant.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
    grant.allow_to_any(RecipeSourceSaid::zen_name, RecipeSourceSaid::zen_version);
    const loom::WeaveId id =
        r.t.bus.register_weave(std::move(door), std::move(grant), std::string(kProjectRole));
    raw->zen_set_self(id);
}

/// CHOOSE A CATALOG THE WAY THE RECIPES DOOR DOES: the one install seam
/// (`HostContext::use_recipes`, WL-PROJ-04), then the republish the Files weave asks the
/// Builder for after an accepted one. Two acts, exactly as `RecipesDoor` and the weave
/// perform them between them -- and neither of them is a browser.
inline HostContext::RecipeSwap choose_catalog(Live& t, const std::string& path) {
    const HostContext::RecipeSwap done = t.host.use_recipes(path);
    if (done.accepted) {
        t.publish(loom::to_value(zengine::builder::StatusRequested{}));
    }
    return done;
}

// ============================================================================
// Tier 1 — ENUMERATION, ORDERING AND ADMISSION, as values
// ============================================================================

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

TEST_CASE("the fixture's sweep removes a link and never enters what it leads to") {
    // THE FIXTURE EVERY DISK CASE HERE STANDS ON, asked the one question `remove_all` gets
    // wrong on libstdc++/Windows: a directory a case linked INTO its temporary directory is
    // not the case's to empty. Measured before `remove_tree` existed (MinGW-w64 GCC 13.1):
    // the standard sweep walked the junction, deleted the target's contents through it,
    // and -- when the target had gone first, as `outside` does before `project/away` in the
    // case above -- stopped at the dangling junction and left it standing, so the NEXT
    // process's `mklink /J` on that name failed and the case above took its early return.
    // A green that had witnessed nothing. MSVC's STL and POSIX never followed, so there this
    // case does not tell the old sweep from the new one, and says which arm it ran.
    TempDir kept("kept"); // the targets' home: outside the directory whose sweep is under test
    const std::filesystem::path target = kept.path() / "target";
    std::filesystem::create_directory(target);
    put_file(target / "secret.cpp", "int s;\n");
    const std::filesystem::path doomed = kept.path() / "doomed";
    std::filesystem::create_directory(doomed);

    std::filesystem::path swept_root;
    std::filesystem::path live;
    std::filesystem::path dangling;
    LinkArm arm = LinkArm::none;
    {
        TempDir swept("swept");
        swept_root = swept.path();
        live = swept_root / "away";
        dangling = swept_root / "gone";
        arm = make_linked_directory(live, target);
        if (arm == LinkArm::none) {
            MESSAGE("this platform made no linked directory for this process");
            return;
        }
        // The second link's target goes away BEFORE the sweep runs: the arrangement the old
        // sweep produced by itself, and the shape of what it left behind.
        REQUIRE(make_linked_directory(dangling, doomed) == arm);
        std::error_code doom_ec;
        std::filesystem::remove_all(doomed, doom_ec);
        REQUIRE_FALSE(doom_ec);
        MESSAGE((std::string("both swept links made as a ") +
                 (arm == LinkArm::junction ? "junction" : "symbolic link")));
    } // `swept` is swept here

    // BOTH LINKS ARE GONE, asked UNFOLLOWED -- a dangling link is still an entry, and an
    // entry is exactly what the old sweep left.
    std::error_code live_ec;
    std::error_code dangling_ec;
    std::error_code root_ec;
    CHECK_FALSE(std::filesystem::exists(std::filesystem::symlink_status(live, live_ec)));
    CHECK_FALSE(std::filesystem::exists(std::filesystem::symlink_status(dangling, dangling_ec)));
    CHECK_FALSE(std::filesystem::exists(std::filesystem::symlink_status(swept_root, root_ec)));
    // ...AND THE TARGET WAS NEVER ENTERED: the directory and its contents are as they were.
    CHECK(std::filesystem::is_directory(target));
    CHECK(std::filesystem::is_regular_file(target / "secret.cpp"));
    CHECK(slurp((target / "secret.cpp").string()) == "int s;\n");
}

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

TEST_CASE("EDIT-1: the project door names the file that recipe's build would compile") {
    // THE OTHER END OF THE SAME SENTENCE, driven through the real door: the Builder pane's
    // edit-source gesture asks `zengine.project` which one file a recipe whose source is
    // spelled relatively names, and the PATH it gets is the project's -- not the workspace
    // decoy's. (Opening it is the Editor weave's, one door on: `test_workshop_panes_editor.cpp`
    // and the Builder seam suite drive that half.)
    ProjectRig r("editbuild");
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
    // ⭐ THE GESTURE CROSSES AS A SENTENCE. `builder.edit-source` is the Builder PANE'S row,
    // so what reaches the project office is `RecipeSourceRequested{recipe}` -- a recipe NAME,
    // never a path -- and the door resolves it against the catalog the host owns.
    mount_project_door_with_source(r);
    DoorAsker* asker = mount_door_asker(r.t);
    const RecipeSourceSaid said = edit_source_through_door(r.t, asker, "one");
    CHECK(said.accepted);
    CHECK(said.recipe == "one");
    CHECK(said.source == completed);
    CHECK(said.source == (r.root / "src" / "example.cpp").lexically_normal().generic_string());
    // AND THE GENERATED PROJECT, FROM THE SAME VALUE, NAMES THE SAME FILE.
    CHECK(zengine::builder::generated_project(all[0]).find(said.source) != std::string::npos);
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
    // ...AND A SINGLE-SOURCE RECIPE'S EMPTY DIRECTORY IS ITS WORKSPACE'S `out`, never
    // the host's own (RELOAD-1): the build must not write the file the process maps.
    CHECK(owner.views()[0].path ==
          HostContext::so_in("/install/build-workspace/one/out", "one"));
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

TEST_CASE("PROJ-0: the project door names the file the OWNER's completed recipe names") {
    // EDIT-1'S SENTENCE, RE-PROVEN OVER THE NEW CUSTODY. The two-base decoy is arranged
    // exactly as it is above -- the project and the generated workspace both holding
    // `src/example.cpp` with different bytes -- and the only thing that changed is who
    // holds the completed value the door's answer comes from. A green here with the
    // decoy absent would prove nothing, which is why the decoy is written first.
    ProjectRig r("ownerdoor");
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

    mount_project_door_with_source(r);
    DoorAsker* asker = mount_door_asker(r.t);
    const RecipeSourceSaid said = edit_source_through_door(r.t, asker, "one");
    CHECK(said.accepted);
    CHECK(said.source == (r.root / "src" / "example.cpp").lexically_normal().generic_string());
    // AND THE GENERATED PROJECT, FROM THE OWNER'S OWN ROW, NAMES THE SAME FILE -- which
    // is the whole of "the file you edit is the file the build compiles", now carried by
    // one object instead of by three parties agreeing.
    REQUIRE(owner.all().size() == 1);
    CHECK(zengine::builder::generated_project(owner.all()[0]).find(said.source) !=
          std::string::npos);
    // ...AND THE ARTIFACT LOOKUP READS THE SAME ROW: one view, whose path is the
    // completed artifact directory and the stem, spelled by the host's one rule.
    REQUIRE(owner.views().size() == 1);
    CHECK(owner.views()[0].path ==
          HostContext::so_in(owner.all()[0].artifact_dir, owner.all()[0].artifact));
    // ...AND A RECIPE THE OWNER DOES NOT HOLD IS REFUSED IN THE CATALOG'S OWN WORDS.
    const RecipeSourceSaid unknown = edit_source_through_door(r.t, asker, "two");
    CHECK_FALSE(unknown.accepted);
    CHECK(unknown.refusal.find("do not hold `two`") != std::string::npos);
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
    auto seat =
        std::make_unique<zengine::builder::BuilderWeave>(owner.views(), owner.source());
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

/// ASK THE EDITOR DOOR, from an office of its own. The gesture that used to reach the
/// editor through the browser is a loaded pane's ask now, so a host case that needs a file
/// open makes it happen through the very door the pane spends.
inline DoorAsker* asker_for(ProjectRig& r) { return mount_door_asker(r.t); }

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
    // PROJECT, and an empty artifact directory on a single-source recipe means the
    // workspace's `out` (RELOAD-1) -- off the install, where the loaded file is.
    CHECK(owner.all()[0].single_source->source == (root / "src/one.cpp").generic_string());
    CHECK(owner.all()[0].artifact_dir == "/install/build-workspace/one/out");
    CHECK(owner.views()[0].path == HostContext::so_in("/install/build-workspace/one/out", "one"));
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
    ProjectRig r("uselive");
    const std::string install = r.root.generic_string();
    put_catalog(r.root / "a-recipes.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_catalog(r.root / "b-recipes.json", {authored_recipe("beta", "src/beta.cpp"),
                                            authored_recipe("gamma", "src/gamma.cpp")});
    r.t.host.use_recipes = host_use_recipes(owner, install, r.t.host.project_dir);
    REQUIRE(r.t.host.use_recipes((r.root / "a-recipes.json").generic_string()).accepted);

    // ⭐ THE TOOL IS THE CONSUMER THIS CASE CAN SEE, and since the Builder panel became a
    // weave it is the only one in this process: what a PANE makes of the change is asserted
    // in the pane's own suite, against the real seam. What the host owes is that the owner,
    // the tool and the answer all move together.
    zengine::builder::BuilderWeave* tool = mount_live_tool(r.t, owner);
    REQUIRE(tool->recipes().size() == 1);
    REQUIRE(tool->recipes()[0].id == "alpha");

    const HostContext::RecipeSwap moved =
        choose_catalog(r.t, (r.root / "b-recipes.json").generic_string());
    REQUIRE(moved.accepted);
    // THE ANSWER CARRIES BOTH HALVES A MAKER IS OWED: which file is in force now, and how
    // much it holds. The pane's sentence is composed from exactly these two fields.
    CHECK(moved.path == (r.root / "b-recipes.json").generic_string());
    CHECK(moved.recipes == 2);

    // THE OWNER MOVED, WHOLE.
    CHECK(owner.source() == (r.root / "b-recipes.json").generic_string());
    REQUIRE(owner.all().size() == 2);
    CHECK(owner.all()[0].id == "beta");
    // ...AND THE TOOL ANSWERS THE NEW CATALOG, because it reads the owner rather than a
    // list it kept. It was never destroyed, never recreated, and never told a recipe.
    REQUIRE(tool->recipes().size() == 2);
    CHECK(tool->recipes()[1].id == "gamma");
    // THE MAKER IS TOLD WHAT HAPPENED AND WHAT IS NOW CURRENT -- by the PANE, out of the
    // answer this door gave it, so the sentence is asserted where the pane is
    // (`test_files.cpp`). What the host owes is the answer itself, and it is both halves.
}

TEST_CASE("PROJ-1: the chooser needs no Builder pane loaded at all") {
    // THE ORDERING CLAIM, MADE EXPLICITLY. Choosing what this project can build is not an
    // act on the Builder's presentation, so requiring that presentation to exist first
    // would be a gesture that depended on which panes a maker happened to have open.
    CurrentRecipes owner;
    ProjectRig r("nobuilder");
    put_catalog(r.root / "a.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_catalog(r.root / "b.json", {authored_recipe("beta", "src/beta.cpp")});
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(),
                                            r.t.host.project_dir);
    REQUIRE(r.t.host.use_recipes((r.root / "a.json").generic_string()).accepted);
    zengine::builder::BuilderWeave* tool = mount_live_tool(r.t, owner);

    REQUIRE(choose_catalog(r.t, (r.root / "b.json").generic_string()).accepted);

    CHECK(owner.source() == (r.root / "b.json").generic_string());
    REQUIRE(tool->recipes().size() == 1);
    CHECK(tool->recipes()[0].id == "beta");
    // ⭐ ...AND THE CLAIM IS STRONGER SINCE THE BUILDER BECAME A WEAVE, not weaker: there is
    // no Builder presentation in this process AT ALL, so the ordering this case names --
    // choosing what a project builds is not an act on the Builder's presentation -- is now a
    // property of the arrangement rather than of what a maker happened to have open. A pane
    // that loads later is told by the ordinary ask, and that is the pane's own case.
}

TEST_CASE("PROJ-1: a refused catalog leaves the maker exactly where they were") {
    // ⭐ THE RECOVERY CLAIM. A file that is not a catalog is an ordinary thing to point at
    // -- the browser lists every real file and judges no contents -- so the refusal has to
    // be survivable: the recipes in force are the old ones, the panel still shows them, the
    // browser still works, and the maker is told BOTH halves.
    CurrentRecipes owner;
    ProjectRig r("refused");
    put_catalog(r.root / "a.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_file(r.root / "notes.txt", "not a catalog\n");
    std::filesystem::create_directories(r.root / "somewhere");
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(),
                                            r.t.host.project_dir);
    REQUIRE(r.t.host.use_recipes((r.root / "a.json").generic_string()).accepted);
    zengine::builder::BuilderWeave* tool = mount_live_tool(r.t, owner);
    const Held before = held_by(owner);

    const HostContext::RecipeSwap refused =
        choose_catalog(r.t, (r.root / "notes.txt").generic_string());
    REQUIRE_FALSE(refused.accepted);

    CHECK(held_by(owner) == before);
    REQUIRE(tool->recipes().size() == 1);
    CHECK(tool->recipes()[0].id == "alpha");
    // ⭐ BOTH HALVES OF WHAT A MAKER IS OWED ARE IN THE ANSWER, which is the part the host
    // still owns: the owner's own words for what was wrong, and -- read back from the owner
    // AFTER the attempt rather than echoed from the candidate -- the catalog that is still
    // in force. A door that echoed `notes.txt` into `path` would let any presentation say
    // "still using <the file just refused>" by doing the obvious thing with the obvious
    // field, so the value is asserted here and the sentence built from it is asserted where
    // the pane is.
    CHECK_FALSE(refused.refusal.empty());
    CHECK(refused.path == (r.root / "a.json").generic_string());
    CHECK(refused.recipes == 1);
    // A DIRECTORY NEVER REACHES THE OWNER AT ALL: the pane refuses it in its own words
    // before it asks, which is why there is nothing here to assert but the arrangement --
    // the case for it is the pane's (`test_files.cpp`).
    CHECK(held_by(owner) == before);

    // AND WORKSHOP IS STILL WORKSHOP: a refusal cost the maker the answer and nothing else.
    CHECK(r.session().panels.has(panel::kLayouts));
}

TEST_CASE("PROJ-1: the republish is the ask a presentation already sends, once") {
    // ⭐ THE ROUTE, COUNTED. A live catalog projection needs a push when the catalog moves --
    // so a replacement has to push it, and the only honest question is WHAT it pushes with.
    // The answer is the message a presentation has always sent: one `StatusRequested`, to the
    // office that already answers it, publishing the two shapes it already publishes. The
    // pushER is the host here (it holds the catalog owner); every CONSUMER is a weave now.
    //
    // The stand-in is used here rather than the real tool precisely because it COUNTS: it
    // records how many times it was asked, which is the fact this case is about. An
    // observer graph, a subscription, a second recipe event or a repeated poll would all
    // show up as a different number -- and the baseline is ZERO now, because no built-in
    // asks it anything when a panel opens.
    CurrentRecipes owner;
    ProjectRig r("republish");
    put_catalog(r.root / "b.json", {authored_recipe("beta", "src/beta.cpp")});
    put_file(r.root / "notes.txt", "not a catalog\n");
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(),
                                            r.t.host.project_dir);
    ToolSeat* tool = mount_tool(r.t, "alpha");
    REQUIRE(tool->described == 0); // nothing has asked it anything

    REQUIRE(choose_catalog(r.t, (r.root / "b.json").generic_string()).accepted);

    CHECK(tool->described == 1);
    // NOTHING ELSE WAS SAID. No build was ordered, and the tool heard exactly one more
    // sentence than it had heard before.
    CHECK(tool->asked.empty());

    // A SECOND CHOICE IS A SECOND ASK AND NOT A SECOND MECHANISM -- including the
    // same-file reload, which is a real replacement and says so on the wire.
    REQUIRE(choose_catalog(r.t, (r.root / "b.json").generic_string()).accepted);
    CHECK(tool->described == 2);

    // ...AND A REFUSAL PUSHES NOTHING, because nothing changed for a pane to be told about.
    REQUIRE_FALSE(choose_catalog(r.t, (r.root / "notes.txt").generic_string()).accepted);
    CHECK(tool->described == 2);
}

TEST_CASE("PROJ-1: a live catalog choice is this session's and is written nowhere") {
    // THE PERSISTENCE POSTURE, MEASURED. PROJ-1 deliberately remembers no catalog choice:
    // the next launch chooses exactly as this one did, from `--recipes` or the shipped
    // default. So the fact lives on the `Session`, which is written to no file, and the
    // durable session record a maker's next launch restores says nothing about it.
    CurrentRecipes owner;
    ProjectRig r("nopersist");
    const std::string session_file = (r.root / "last-session.json").generic_string();
    r.t.host.session_path = session_file;
    put_catalog(r.root / "a.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_catalog(r.root / "b.json", {authored_recipe("beta", "src/beta.cpp")});
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(),
                                            r.t.host.project_dir);
    REQUIRE(r.t.host.use_recipes((r.root / "a.json").generic_string()).accepted);
    REQUIRE(choose_catalog(r.t, (r.root / "b.json").generic_string()).accepted);
    // THE ANSWER IS THE OWNER'S OWN, read back after the attempt rather than recomposed
    // from wherever anybody was looking -- so no two parties can come to name two different
    // files (PROJ-2).
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
// Tier 6b — PROJ-15: where the shipped catalog is
// ============================================================================
//
// THE LAW IS A LAUNCH FACT (WL-PROJ-15): the catalog Workshop ships is staged beside the
// executable under `kDefaultRecipesName`, `--recipes` names a different one, and no registry,
// picker or search path stands between the two. No rig can run `main()` (it claims a
// terminal), so the fact is read in two halves: the staged FILE, from where the executable
// lands, through the one seam the launch itself installs it through; and the launch's
// CHOICE, out of the host's own source with its comments stripped, as the one-seam case
// above reads it.

namespace {

/// A source file's CODE -- `//` comments cut -- so a sentence that explains a rule cannot
/// satisfy a check for the rule. The one-seam case above spells the same reader inline; it
/// is repeated here rather than hoisted, so that case's text stays exactly as its witnesses
/// cite it.
inline std::string host_code_of(const char* path) {
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
}

inline std::size_t count_of(const std::string& text, const std::string& needle) {
    std::size_t at = 0;
    std::size_t seen = 0;
    while ((at = text.find(needle, at)) != std::string::npos) {
        ++seen;
        at += needle.size();
    }
    return seen;
}

} // namespace

TEST_CASE("the shipped catalog is staged beside the executable, under the name the launch resolves") {
    // WHERE THE EXECUTABLE LANDS is asked of the build (`$<TARGET_FILE_DIR:zengine-workshop>`),
    // never spelled by this case; the staging rule writes there at generate time, so the file
    // is beside the host before the host is built.
    const std::filesystem::path beside(WORKSHOP_HOST_DIR);
    // THE CONSTANT IS A NAME, NOT A PATH: it carries no directory of its own, because the
    // directory is the executable's and nothing else's.
    const std::string name(recipe_persist::kDefaultRecipesName);
    REQUIRE_FALSE(name.empty());
    CHECK(name.find_first_of("/\\") == std::string::npos);
    const std::filesystem::path staged = beside / name;
    CAPTURE(staged.generic_string());
    std::error_code ec;
    REQUIRE_MESSAGE(std::filesystem::is_regular_file(staged, ec),
                    "no shipped catalog beside the executable: ", staged.generic_string());

    // ...AND WHAT IS STAGED THERE IS THE CATALOG: read through the ONE seam the launch
    // installs it through, with the host's directory given the way the launch gives it.
    // The owner answers with the path it holds, never with an echo of the argument.
    TempDir project("shipped");
    CurrentRecipes owner;
    const HostContext::RecipeSwap read = host_use_recipes(
        owner, beside.generic_string(), project.path().generic_string())(staged.generic_string());
    REQUIRE_MESSAGE(read.accepted, read.refusal);
    CHECK(read.path == staged.generic_string());
    CHECK(owner.source() == staged.generic_string());
    MESSAGE((std::string("the shipped catalog installs ") + std::to_string(read.recipes) +
             " recipe(s) from " + read.path));
}

TEST_CASE("the launch resolves the catalog by one rule: --recipes, else the project catalog at "
          "the root, else the shipped default beside the executable") {
    // THE LAUNCH IS `main()`'s, AND `main()` CANNOT RUN HERE, so the choice it makes is read
    // out of its code -- comments stripped, expressions rather than words (BLD-0's tripwire
    // rule), because the host EXPLAINS this arrangement at length above the lines that make it.
    const std::string host = host_code_of(WORKSHOP_HOST_CPP);

    // THE HOST'S DIRECTORY IS THE EXECUTABLE'S, resolved once.
    CHECK(count_of(host, "host.dir = exe_dir();") == 1);

    // THE RULE IS SPELLED ONCE, AND IT IS THE PLAN'S TWIN: the host names the function and
    // no second default -- the shipped name is the rule's to spell, not `main()`'s -- and it
    // hands both rules the same existence probe, so the two project files are found alike.
    CHECK(count_of(host, "recipe_persist::recipes_in_force(") == 1);
    CHECK(host.find("recipe_persist::kDefaultRecipesName") == std::string::npos);
    CHECK(count_of(host, "args.recipes, host.project_dir, host.dir, present)") == 1);
    CHECK(count_of(host, "args.load_plan, host.project_dir, host.dir, present)") == 1);

    // ...AND `--recipes` IS A PATH THE MAKER TYPED: parsed as one, refused empty rather than
    // quietly defaulted, and spent nowhere but in that choice -- a second consumer would be
    // the registry or the picker this law says there is not. The third spending is the
    // "nothing to build" sentence, which is only ever said of the shipped default.
    CHECK(host.find("arg == \"--recipes\"") != std::string::npos);
    CHECK(host.find("args.recipes = value;") != std::string::npos);
    CHECK(count_of(host, "args.recipes") == 3);
    CHECK(host.find("if (args.recipes.empty() && !present(recipe_path))") != std::string::npos);
}

TEST_CASE("the project catalog at the captured root is the catalog in force when no --recipes "
          "is given") {
    // THE PLAN'S TWIN, AS A PURE FUNCTION (WL-PROJ-15): explicit wins; else the project
    // catalog when it is there; else the shipped default -- the same three answers, in the
    // same order, on the same kind of probe `plan_in_force` is pinned on.
    const auto present_at = [](const std::string& where) {
        return [where](const std::string& path) { return path == where; };
    };
    const std::string project = "/project/" + std::string(recipe_persist::kProjectRecipesName);
    const std::string shipped = "/install/" + std::string(recipe_persist::kDefaultRecipesName);
    CHECK(recipe_persist::recipes_in_force("/explicit.json", "/project", "/install",
                                           present_at(project)) == "/explicit.json");
    CHECK(recipe_persist::recipes_in_force("", "/project", "/install", present_at(project)) ==
          project);
    CHECK(recipe_persist::recipes_in_force("", "/project", "/install",
                                           present_at("/elsewhere")) == shipped);
    CHECK(recipe_persist::recipes_in_force("", "", "/install", present_at(project)) == shipped);
    // ...AND THE SHIPPED DEFAULT IS NAMED EVEN WHEN NOTHING IS THERE: the rule names a file
    // and the caller judges it, which is what lets an absent default stay "nothing to build".
    CHECK(recipe_persist::recipes_in_force("", "/project", "/install",
                                           [](const std::string&) { return false; }) == shipped);
}

// ============================================================================
// Tier 4 — WHAT THE ANCHOR IS, AND WHAT IT IS NOT (PROJ-2)
// ============================================================================
//
// THE SUBJECT IS FOUR FACTS STAYING APART. The project anchor says what a relative source
// spelling MEANS; the Files location says where somebody is looking; a mark says a place is
// worth coming back to; and the operating system says what may be read at all. They
// coincide at launch and are separate everywhere else.
//
// THREE OF THE FOUR ARE THE PANE'S NOW. Where a maker is looking, the marks they keep and
// what the platform will enumerate all left with the browser (`Zengine/files/`), and the
// cases that walk, mark and enumerate went with them -- they are the pane's own claims and
// are proved in `test_files.cpp`, over the same pure half, unchanged.
//
// WHAT STAYS HERE IS THE ANCHOR: the one of the four the HOST owns. It is captured at
// launch, it is what `install_recipes` completes a relative source against, and the whole
// claim below is that nothing a maker does elsewhere moves it -- which is now stated the
// way it is actually reachable, by choosing a catalog that lives somewhere else entirely.

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
    ProjectRig r("extcatalog");
    const std::string anchor = r.t.host.project_dir;
    std::filesystem::create_directories(r.root / "src");
    put_file(r.root / "src" / "thing.cpp", "the project\n");
    r.t.host.use_recipes = host_use_recipes(owner, r.root.generic_string(), anchor);
    REQUIRE(choose_catalog(r.t, (foreign / "recipes.json").generic_string()).accepted);

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
    // ...AND CHOOSING A CATALOG SOMEWHERE ELSE DID NOT MOVE THE PROJECT.
    CHECK(r.t.host.project_dir == anchor);
    // THE OWNER NAMES THE CHOSEN FILE UNAMBIGUOUSLY (SC-12): its own absolute path, not a
    // based spelling with no stated base. The screen's copy of that sentence is the pane's
    // now and is asserted where the pane is.
    CHECK(owner.source() == (foreign / "recipes.json").generic_string());
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

// ============================================================================
// Tier 6c — THE THREE DOORS, FROM THE HOST'S SIDE
// ============================================================================
//
// WHAT A LOADED PANE CAN ASK THIS HOST FOR, and what it gets. The Files pane left this
// process, so the four facts it used to read off `HostContext` cross as asks to offices;
// the pane's half of that conversation is proved where the pane is, and the HOST's half --
// who may ask, who is answered, and whose words the answer is in -- is proved here.
//
// THE ASKER IS A REAL WEAVE IN A REAL OFFICE (`DoorAsker`, workshop_support.hpp), because
// every one of these claims is about AUTHORSHIP: an ask with no office is refused, and an
// answer goes to the weave that asked and to nobody else. A rig that called the door's
// handler directly could not put either question.

namespace {

/// This host's two doors, mounted the way `workshop.cpp` mounts them: each in its own
/// office, each granted exactly the one answer it gives.
///
/// ⚠ THE STRINGS THE PROJECT DOOR READS LIVE HERE, because the door holds pointers into
/// `main`'s own locals and derives at every ask rather than keeping a copy.
struct DoorRig {
    ProjectRig r;
    std::string marks_path;
    ProjectDoor* project = nullptr;
    RecipesDoor* recipes = nullptr;

    explicit DoorRig(const char* tag) : r(tag) {
        marks_path = (r.root / "workshop-marks.json").generic_string();
    }

    void mount_project() {
        auto door = std::make_unique<ProjectDoor>(r.t.host.project_dir, marks_path);
        project = door.get();
        loom::Grant grant;
        grant.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        const loom::WeaveId id = r.t.bus.register_weave(std::move(door), std::move(grant),
                                                        std::string(kProjectRole));
        project->zen_set_self(id);
    }

    void mount_recipes(RecipesDoor::Use use, RecipesDoor::Author author = nullptr) {
        auto door = std::make_unique<RecipesDoor>(std::move(use), std::move(author));
        recipes = door.get();
        loom::Grant grant;
        grant.allow_to_any(RecipeOutcome::zen_name, RecipeOutcome::zen_version);
        const loom::WeaveId id = r.t.bus.register_weave(std::move(door), std::move(grant),
                                                        std::string(kRecipesRole));
        recipes->zen_set_self(id);
    }
};

} // namespace

TEST_CASE("PANE-DOOR: the project door answers the weave that asked, and nobody else") {
    // ⭐ THE SEAM'S WHOLE SHAPE IN ONE CASE. The two facts the browser used to read off
    // `HostContext` -- where this run began, and which places file it owns -- cross as
    // VALUES, to the asker, through Loom's own answer route. Nothing was published.
    DoorRig d("projectdoor");
    d.mount_project();
    DoorAsker* files = mount_door_asker(d.r.t, "zengine.test.files");
    DoorAsker* nosy = mount_door_asker(d.r.t, "zengine.test.nosy");

    asker_do(d.r.t, files, [](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kProjectRole, ProjectRootRequested{});
    });

    REQUIRE(files->roots.size() == 1);
    CHECK(files->roots[0].project_dir == d.r.root.generic_string());
    CHECK(files->roots[0].marks_path == d.marks_path);
    // ...AND THE SECOND OFFICE HEARD NOTHING AT ALL. An answer is not a broadcast, so a
    // weave that did not ask learns neither path.
    CHECK(nosy->roots.empty());
}

TEST_CASE("PANE-DOOR: a door answers an office, and refuses speech with no author") {
    // THE RULE EVERY DOOR KEEPS, AND ITS CANARY. An ask authored personally reaches the
    // door and is refused there -- so the refusal is a measurement rather than a bus
    // accident, and the very same asker gets its answer the moment it speaks as an office.
    DoorRig d("anonymous");
    d.mount_project();
    d.mount_recipes([](const std::string&) {
        HostContext::RecipeSwap done;
        done.accepted = true;
        return done;
    });
    DoorAsker* files = mount_door_asker(d.r.t, "zengine.test.files");

    files->personally = true;
    asker_do(d.r.t, files, [](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kProjectRole, ProjectRootRequested{});
    });
    asker_do(d.r.t, files, [](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kRecipesRole, RecipeUseRequested{"/anywhere.json"});
    });
    CHECK(files->roots.empty());
    CHECK(files->outcomes.empty());
    // THE DOORS SAW BOTH ASKS AND ANSWERED NEITHER, which is the difference between a rule
    // and a delivery that never happened.
    CHECK(d.project->snapshot().get("refused")->as_int() == 1);
    CHECK(d.recipes->snapshot().get("refused")->as_int() == 1);
    CHECK(d.recipes->snapshot().get("used")->as_int() == 0);

    files->personally = false;
    asker_do(d.r.t, files, [](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kProjectRole, ProjectRootRequested{});
    });
    CHECK(files->roots.size() == 1);
    CHECK(d.project->snapshot().get("answers")->as_int() == 1);
}

TEST_CASE("PANE-DOOR: the recipes door spends this host's one writer and re-words nothing") {
    // ⭐ THE ACTING DOOR. It composes no recipe, reads no bytes and invents no refusal: it
    // hands the ask to the closure `workshop.cpp` already wired and puts the owner's own
    // answer on the wire, both halves.
    CurrentRecipes owner;
    DoorRig d("recipesdoor");
    put_catalog(d.r.root / "a.json", {authored_recipe("alpha", "src/alpha.cpp")});
    put_file(d.r.root / "notes.txt", "not a catalog\n");
    d.mount_recipes(host_use_recipes(owner, d.r.root.generic_string(), d.r.t.host.project_dir));
    DoorAsker* files = mount_door_asker(d.r.t, "zengine.test.files");

    const std::string good = (d.r.root / "a.json").generic_string();
    asker_do(d.r.t, files, [good](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kRecipesRole, RecipeUseRequested{good});
    });
    REQUIRE(files->outcomes.size() == 1);
    CHECK(files->outcomes[0].accepted);
    CHECK(files->outcomes[0].refusal.empty());
    CHECK(files->outcomes[0].path == good);
    CHECK(files->outcomes[0].recipes == 1);
    CHECK(owner.source() == good); // the act really happened, through the one seam

    // A REFUSAL CARRIES THE OWNER'S WORDS AND THE CATALOG STILL IN FORCE -- read back from
    // the owner AFTER the attempt, never echoed from the candidate. A door that echoed the
    // ask would let a pane say "still using <the file just refused>".
    const std::string bad = (d.r.root / "notes.txt").generic_string();
    asker_do(d.r.t, files, [bad](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kRecipesRole, RecipeUseRequested{bad});
    });
    REQUIRE(files->outcomes.size() == 2);
    CHECK_FALSE(files->outcomes[1].accepted);
    CHECK_FALSE(files->outcomes[1].refusal.empty());
    CHECK(files->outcomes[1].path == good);
    CHECK(owner.source() == good);
    CHECK(d.recipes->snapshot().get("refusals")->as_int() == 1);
}

TEST_CASE("PANE-DOOR: a host that holds no such office answers nothing, and that is the answer") {
    // THE DESIGNED ABSENCE. A host with no project mounts no project door, so the ask
    // reaches nobody -- which is not an error and must not be one: the pane's own answer to
    // silence is to go on browsing from wherever it is.
    DoorRig d("nodoor");
    DoorAsker* files = mount_door_asker(d.r.t, "zengine.test.files");
    asker_do(d.r.t, files, [](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kProjectRole, ProjectRootRequested{});
    });
    CHECK(files->roots.empty());
}

// ============================================================================
// Tier 7 — SOURCE-0: the two facts this host makes routable
// ============================================================================
//
// THE HOST MAY DESCRIBE ITSELF; IT MAY NOT INVENT PROVIDER POWER. PROV-0's law -- a host
// authors no operator -- was written against a host manufacturing SEMANTICS for itself,
// and as written it also forbade a host exposing state it already owns. SOURCE-0 refines
// exactly that: zero-input SOURCES over owned facts are allowed, parameterized power is
// not, and `mount_host_sources` is where the second half is enforced rather than promised.
//
// THIS SUITE OWNS THE OWNERS. `HostContext::project_dir` is what a project-relative source
// is relative to, and `CurrentRecipes` is who holds the completed catalog -- the two
// subjects the tiers above are already about. So the Sources over them are asked here,
// against the real owners, through the real seam, rather than against a rig's copy.

namespace {

/// The host's own exposure, arranged the way `workshop.cpp` arranges it: the owners
/// first, then the catalog, then the one door.
struct HostSourceRig {
    std::string project_dir;
    CurrentRecipes recipes;
    op::Catalog catalog;

    explicit HostSourceRig(std::string anchor = std::string()) : project_dir(std::move(anchor)) {}

    op::MountReport expose() {
        return mount_host_sources(catalog, host_sources(project_dir, recipes));
    }

    /// What `zengine.project.anchor` says right now.
    std::string anchor() {
        const op::Evaluation said = op::sample(catalog, kProjectAnchorSource);
        REQUIRE_MESSAGE(said.ok(), said.reason());
        return said.value().at(0)->as_text();
    }

    /// What `zengine.recipes.catalog` says right now, both halves in one read -- because
    /// the owner holds them as one fact and a case that read them separately could not
    /// see a half-replacement at all.
    std::pair<std::string, std::int64_t> catalog_said() {
        const op::Evaluation said = op::sample(catalog, kRecipeCatalogSource);
        REQUIRE_MESSAGE(said.ok(), said.reason());
        const loom::Value& facts = *said.value().at(0)->as_message();
        return {facts.get("source")->as_text(), facts.get("recipes")->as_int()};
    }
};

/// AN ORDINARY PARAMETERIZED OPERATOR, hand-built, because this suite deliberately names
/// no semantic vocabulary: what makes a definition an Operator rather than a Source is its
/// SHAPE, so any definition that asks for something is the case.
op::OperatorDef parameterized_def(const std::string& identity) {
    auto in = loom::make_schema(
        identity + ".in", 1,
        std::vector<loom::Field>{loom::Field{"n", loom::type_of(loom::Kind::Int), true}});
    auto out = loom::make_schema(
        identity + ".out", 1,
        std::vector<loom::Field>{loom::Field{"value", loom::type_of(loom::Kind::Int), true}});
    return op::OperatorDef(identity, std::move(in), std::move(out),
                           [](const loom::Value& args) {
                               return loom::Cell::integer(args.get("n")->as_int());
                           });
}

/// The ACTIVE contribution to one power in a projection, or nothing.
const ws::PowerContribution* contribution_of(const ws::ResolvedPowers& said,
                                             const std::string& power) {
    for (const ws::PowerStack& p : said.powers) {
        if (p.power == power && !p.contributions.empty()) {
            return &p.contributions.back(); // active last, the catalog's own order
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("SOURCE-0: the host exposes exactly two Sources, under its own honest provenance") {
    HostSourceRig r;
    REQUIRE(r.expose());

    // TWO IDENTITIES AND NOT A THIRD. No field, getter, pane, preference or schema became
    // routable because it exists; the list is written out by hand and this is what it says.
    CHECK(r.catalog.identities() ==
          std::vector<std::string>{kProjectAnchorSource, kRecipeCatalogSource});

    // BOTH ARE SOURCES, by the one predicate, because they take nothing.
    CHECK(op::is_source(*r.catalog.find(kProjectAnchorSource)));
    CHECK(op::is_source(*r.catalog.find(kRecipeCatalogSource)));

    // ...AND THE PROVENANCE IS THE HOST'S OWN NAME, not an erased one and not a borrowed
    // artifact's. `Catalog::mount` takes a null custody for a provider that is not an
    // image at all, which is exactly what an in-process host is.
    CHECK(r.catalog.providers() == std::vector<std::string>{kHostProvider});
    CHECK(r.catalog.contributions(kProjectAnchorSource).front().provider == kHostProvider);
    CHECK(r.catalog.contributions(kRecipeCatalogSource).front().provider == kHostProvider);

    SUBCASE("and the batch is unmountable like any other provider's") {
        REQUIRE(r.catalog.unmount(kHostProvider));
        CHECK(r.catalog.identities().empty());
    }
}

TEST_CASE("SOURCE-0: the host's own door refuses anything that would take an argument") {
    // ⭐ THE REFINED PROV-0 BOUNDARY, AS A MECHANISM. "The host may describe itself" is
    // not a licence to author application power, and the difference between the two is
    // exactly whether a definition would ask a maker for anything. A prose rule here
    // would be a rule nothing enforces; this one cannot be walked past.
    HostSourceRig r("/project");

    SUBCASE("on its own") {
        std::vector<op::OperatorDef> batch;
        batch.push_back(parameterized_def("zengine.host.parameterized"));
        const op::MountReport refused = mount_host_sources(r.catalog, std::move(batch));
        CHECK_FALSE(refused.ok);
        CHECK(refused.reason.find("zengine.host.parameterized") != std::string::npos);
        CHECK(refused.reason.find("never parameterized power") != std::string::npos);
        CHECK(r.catalog.identities().empty());
    }

    SUBCASE("and smuggled in beside two legitimate Sources") {
        // THE JUDGEMENT IS OVER THE WHOLE BATCH, before any of it is installed -- the
        // same all-or-nothing shape `Catalog::mount` already keeps. A door that judged
        // as it installed would leave the two Sources behind and refuse the third.
        std::vector<op::OperatorDef> batch = host_sources(r.project_dir, r.recipes);
        batch.push_back(parameterized_def("zengine.host.parameterized"));
        const op::MountReport refused = mount_host_sources(r.catalog, std::move(batch));
        CHECK_FALSE(refused.ok);
        CHECK_MESSAGE(r.catalog.identities().empty(),
                      "a refused host batch left something behind");
        CHECK_FALSE(r.catalog.mounted(kHostProvider));
    }
}

TEST_CASE("SOURCE-0: zengine.project.anchor answers the owner's anchor, absence included") {
    SUBCASE("a project the host was launched into") {
        HostSourceRig r("/somewhere/project");
        REQUIRE(r.expose());
        CHECK(r.anchor() == "/somewhere/project");
    }

    SUBCASE("...and the designed ABSENCE is preserved, never a manufactured path") {
        // EMPTY IS WHAT THE OWNER SAYS when the platform will not report a working
        // directory, and it is what every consumer already refuses in words. A Source
        // that substituted the install directory, the Files location or `.` here would
        // be inventing a project.
        HostSourceRig r;
        REQUIRE(r.expose());
        CHECK(r.anchor().empty());
    }

    SUBCASE("it reads the OWNER, not a copy taken at registration") {
        HostSourceRig r("/first");
        REQUIRE(r.expose());
        CHECK(r.anchor() == "/first");
        r.project_dir = "/second"; // the owner changed; nobody re-registered anything
        CHECK(r.anchor() == "/second");
        CHECK(r.catalog.contributions(kProjectAnchorSource).size() == 1);
    }

    SUBCASE("its output schema says PROJECT ANCHOR, not `a path-shaped thing`") {
        // TWO NAMESPACES, KEPT APART. The identity answers which source is meant; the
        // schema answers what the answer MEANS -- and `content_id` hashes the NAME, so a
        // structurally identical `zengine.FilesLocation v1` would be a different door
        // rather than an interchangeable one.
        HostSourceRig r("/p");
        REQUIRE(r.expose());
        const loom::Schema& out = *r.catalog.find(kProjectAnchorSource)->outputs();
        CHECK(out.name() == "zengine.ProjectAnchor");
        CHECK(out.version() == 1);

        const auto look_alike = loom::make_schema(
            "zengine.FilesLocation", 1,
            std::vector<loom::Field>{loom::Field{"anchor", loom::type_of(loom::Kind::Text), true}});
        CHECK(look_alike->fields().size() == out.fields().size());
        CHECK_FALSE(loom::same_identity(*look_alike, out));
        CHECK(look_alike->content_id() != out.content_id());
    }
}

TEST_CASE("SOURCE-0: zengine.recipes.catalog answers which catalog is in force, and how much") {
    HostSourceRig r("/project");
    REQUIRE(r.expose());

    SUBCASE("with nothing held, the owner's own absence is what is said") {
        // A PROJECT WITH NOTHING TO BUILD IS A PROJECT -- `CurrentRecipes`' own rule,
        // carried rather than translated into an error.
        const std::pair<std::string, std::int64_t> said = r.catalog_said();
        CHECK(said.first.empty());
        CHECK(said.second == 0);
    }

    SUBCASE("with a catalog held, both halves come from the one owner") {
        TempDir dir("srccat");
        const std::filesystem::path root = dir.path();
        put_catalog(root / "two.json",
                    {authored_recipe("one", "src/a.cpp"), authored_recipe("two", "src/b.cpp")});
        REQUIRE(install_recipes(r.recipes, (root / "two.json").generic_string(), "/install",
                                r.project_dir, &HostContext::so_in)
                    .accepted);

        const std::pair<std::string, std::int64_t> said = r.catalog_said();
        CHECK(said.first == (root / "two.json").generic_string());
        CHECK(said.second == 2);
        // ...AND IT IS THE OWNER'S ANSWER, not a second join of the same bytes.
        CHECK(said.first == r.recipes.source());
        CHECK(said.second == static_cast<std::int64_t>(r.recipes.all().size()));
    }

    SUBCASE("the catalog itself is NOT exposed, only the standing description") {
        TempDir dir("srcnarrow");
        const std::filesystem::path root = dir.path();
        put_catalog(root / "one.json", {authored_recipe("secret", "src/secret.cpp")});
        REQUIRE(install_recipes(r.recipes, (root / "one.json").generic_string(), "/install",
                                r.project_dir, &HostContext::so_in)
                    .accepted);
        // BEING IN MEMORY IS NOT A REASON TO BE ROUTABLE. The rows, their ids, their
        // sources, their build procedures and their artifacts are all right there in the
        // owner and none of them crosses.
        const loom::Schema& facts =
            *r.catalog.find(kRecipeCatalogSource)->outputs()->fields()[0].type.message;
        CHECK(facts.fields().size() == 2);
        CHECK(facts.find("source") != nullptr);
        CHECK(facts.find("recipes") != nullptr);
        const op::Evaluation said = op::sample(r.catalog, kRecipeCatalogSource);
        REQUIRE(said.ok());
        CHECK(said.value().at(0)->as_message()->get("source")->as_text().find("secret") ==
              std::string::npos);
    }
}

TEST_CASE("SOURCE-0: the recipe Source follows a live swap, and a REFUSED one moves nothing") {
    // ⭐ THE LOAD-BEARING WITNESS for the difference between a registered ROUTE and a
    // sampled ANSWER. PROJ-1 made catalog selection live; if this Source had captured the
    // startup catalog, every case above would still pass and this one could not.
    TempDir dir("srcswap");
    const std::filesystem::path root = dir.path();
    put_catalog(root / "first.json", {authored_recipe("one", "src/a.cpp")});
    put_catalog(root / "second.json",
                {authored_recipe("two", "src/b.cpp"), authored_recipe("three", "src/c.cpp")});
    put_file(root / "notes.txt", "this is not a recipe catalog\n");

    HostSourceRig r(root.generic_string());
    const auto use = host_use_recipes(r.recipes, "/install", r.project_dir);
    REQUIRE(use((root / "first.json").generic_string()).accepted);
    REQUIRE(r.expose());

    const std::pair<std::string, std::int64_t> started{(root / "first.json").generic_string(), 1};
    CHECK(r.catalog_said() == started);

    SUBCASE("a successful replacement is reported by the NEXT sample, with no re-registration") {
        REQUIRE(use((root / "second.json").generic_string()).accepted);
        CHECK(r.catalog_said() ==
              std::pair<std::string, std::int64_t>{(root / "second.json").generic_string(), 2});
        // NOTHING WAS RE-REGISTERED. One contribution, from one mount, made before the swap.
        CHECK(r.catalog.contributions(kRecipeCatalogSource).size() == 1);
        CHECK(r.catalog.providers() == std::vector<std::string>{kHostProvider});
    }

    SUBCASE("a REFUSED replacement leaves the sampled answer exactly where it was") {
        // ...because it leaves the OWNER exactly where it was. The Source has no opinion
        // about a failed swap and needs none: it reports whoever the owner currently is.
        const HostContext::RecipeSwap no = use((root / "notes.txt").generic_string());
        CHECK_FALSE(no.accepted);
        CHECK(r.catalog_said() == started);
    }

    SUBCASE("a file that is not there is the same story") {
        CHECK_FALSE(use((root / "absent.json").generic_string()).accepted);
        CHECK(r.catalog_said() == started);
    }
}

TEST_CASE("SOURCE-0: enumeration says what a sample would yield, and samples nothing") {
    HostSourceRig r("/project");
    REQUIRE(r.expose());

    const std::uint64_t before = op::invocations();
    const ws::ResolvedPowers said = describe_powers(r.catalog);
    const std::uint64_t after = op::invocations();

    // ⭐ NOT ONE BODY RAN. Running one to find out what it yields would be a side effect
    // in a view, and it is also unnecessary: a definition has carried both its schemas
    // since it was authored.
    CHECK(after == before);

    const ws::PowerContribution* anchor = contribution_of(said, kProjectAnchorSource);
    const ws::PowerContribution* recipes = contribution_of(said, kRecipeCatalogSource);
    REQUIRE(anchor != nullptr);
    REQUIRE(recipes != nullptr);

    // IDENTITY, PROVENANCE, COMPOSITE, SOURCE, AND WHAT A SAMPLE WOULD RETURN -- the five
    // facts a Sources surface needs, in ONE projection, with no per-identity describe.
    CHECK(anchor->provider == kHostProvider);
    CHECK(anchor->source);
    CHECK_FALSE(anchor->composite);
    CHECK(anchor->output.name == "zengine.ProjectAnchor");
    CHECK(anchor->output.version == 1);
    CHECK(recipes->source);
    CHECK(recipes->output.name == "zengine.RecipeCatalog");

    // ...AND THE IDENTITY IS THE DEFINITION'S OWN, not a number this view computed from a
    // structure it happened to walk.
    CHECK(anchor->output.content_id ==
          static_cast<std::int64_t>(
              r.catalog.find(kProjectAnchorSource)->outputs()->content_id()));
    CHECK(recipes->output.content_id ==
          static_cast<std::int64_t>(
              r.catalog.find(kRecipeCatalogSource)->outputs()->content_id()));

    SUBCASE("an ordinary Operator in the same projection is NOT a Source") {
        // The projection reads shape off the store and branches on no identity: a power
        // this file never heard of would classify the same way.
        r.catalog.publish(parameterized_def("test.needs.an.argument"));
        const ws::ResolvedPowers again = describe_powers(r.catalog);
        const ws::PowerContribution* asks = contribution_of(again, "test.needs.an.argument");
        REQUIRE(asks != nullptr);
        CHECK_FALSE(asks->source);
        CHECK(asks->output.name == "test.needs.an.argument.out");
        CHECK(asks->provider.empty()); // published by this rig itself, exactly as the wire says
    }
}

TEST_CASE("SOURCE-0: the flow -- expose, enumerate without evaluating, sample, swap, sample") {
    // THE INTEGRATION WITNESS, THROUGH THE PRODUCTION OWNERS AND THE PRODUCTION SEAMS.
    // Every step below is the thing `workshop.cpp` actually calls: `mount_host_sources`,
    // `describe_powers`, `op::sample`, `install_recipes`. There is no test-only Source
    // store anywhere in it.
    TempDir dir("srcflow");
    const std::filesystem::path root = dir.path();
    put_catalog(root / "start.json", {authored_recipe("one", "src/a.cpp")});
    put_catalog(root / "later.json",
                {authored_recipe("two", "src/b.cpp"), authored_recipe("three", "src/c.cpp")});

    HostSourceRig r(root.generic_string());
    const auto use = host_use_recipes(r.recipes, "/install", r.project_dir);
    REQUIRE(use((root / "start.json").generic_string()).accepted);

    //  1  the host publishes two Sources
    REQUIRE(r.expose());

    //  2  enumerate without evaluating
    const std::uint64_t quiet = op::invocations();
    const ws::ResolvedPowers seen = describe_powers(r.catalog);
    CHECK(op::invocations() == quiet);
    CHECK(seen.powers.size() == 2);
    CHECK(contribution_of(seen, kProjectAnchorSource)->source);
    CHECK(contribution_of(seen, kRecipeCatalogSource)->source);

    //  3  sample the project anchor
    CHECK(r.anchor() == root.generic_string());

    //  4  sample the recipe catalog
    CHECK(r.catalog_said() ==
          std::pair<std::string, std::int64_t>{(root / "start.json").generic_string(), 1});

    //  5  replace the recipe catalog through the existing live seam
    REQUIRE(use((root / "later.json").generic_string()).accepted);

    //  6  sample again -- the NEW catalog, from the SAME registration
    CHECK(r.catalog_said() ==
          std::pair<std::string, std::int64_t>{(root / "later.json").generic_string(), 2});
    CHECK(r.anchor() == root.generic_string()); // and the anchor did not move with it
    CHECK(r.catalog.contributions(kRecipeCatalogSource).size() == 1);
}

TEST_CASE("SOURCE-0: exposure stays deliberate -- the host did not become reflectable") {
    // SC-14. A fact being true is not a reason to publish it, and the catalog describes
    // what this composition CHOSE to make addressable. The list below is the one SOURCE-0
    // explicitly declines: each is a real fact this process could reach and none is a
    // door.
    TempDir dir("srcsecret");
    HostSourceRig r(dir.path().generic_string());
    r.recipes.hold((dir.path() / "r.json").generic_string(), {}, &HostContext::so_in);
    REQUIRE(r.expose());

    for (const char* absent : {"zengine.clipboard", "zengine.editor.document",
                               "zengine.editor.buffer", "zengine.prefs", "zengine.keymap",
                               "zengine.session", "zengine.grants", "zengine.marks",
                               "zengine.files.location", "zengine.navigation.origin",
                               "time.now"}) {
        CHECK_MESSAGE(r.catalog.find(absent) == nullptr, "'", absent,
                      "' became routable without anybody deciding it should");
    }
    // ...and asking for one is an ordinary refusal rather than a hole.
    const op::Evaluation said = op::sample(r.catalog, "zengine.clipboard");
    CHECK_FALSE(said.ok());
    CHECK(said.reason() == "unresolved operator reference 'zengine.clipboard'");
}

TEST_CASE("SOURCE-0: the host reaches its own facts through one door, and the owners outlive it") {
    // DEFENCE IN DEPTH, and the same shape the PROJ-0/PROJ-1 read above has: no rig can
    // run `main()`, so what a source read adds is that this arrangement cannot quietly
    // stop being written this way while every case here stays green.
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

    // ONE DOOR, SPELLED ONCE, over the two owners BY NAME.
    const std::size_t door =
        host.find("mount_host_sources(operators, host_sources(host.project_dir, current_recipes))");
    CHECK_MESSAGE(door != std::string::npos,
                  "workshop.cpp no longer exposes its facts through the one host-sources door");

    // THE OWNERS ARE DECLARED BEFORE THE CATALOG THAT HOLDS THE CLOSURES READING THEM,
    // so reverse-order destruction drops the readers first. This is the lifetime claim and
    // there is nothing else that could make it.
    const std::size_t recipes = host.find("CurrentRecipes current_recipes;");
    const std::size_t context = host.find("HostContext host;");
    const std::size_t catalog = host.find("op::Catalog operators;");
    REQUIRE(recipes != std::string::npos);
    REQUIRE(context != std::string::npos);
    REQUIRE(catalog != std::string::npos);
    CHECK(recipes < catalog);
    CHECK(context < catalog);
    CHECK(catalog < door);

    // ...AND THE HOST STILL AUTHORS NOTHING. It builds no schema, mints no definition and
    // names neither Source identity: what it decides is THAT the exposure happens, and
    // `workshop/host_sources.hpp` decides everything else.
    for (const char* forbidden :
         {"zengine.project.anchor", "zengine.recipes.catalog", "loom::make_schema",
          "loom::SchemaBuilder", "op::OperatorDef", "op::is_source", "project_anchor_source",
          "recipe_catalog_source"}) {
        CHECK_MESSAGE(host.find(forbidden) == std::string::npos, "workshop.cpp names '", forbidden,
                      "', which is authorship the one door owns");
    }
}

TEST_CASE("RELOAD-1: a single-source recipe's product lands in its workspace, never on the "
          "loaded path") {
    // THE PATH RULE'S FIRST HALF: a build of an artifact this process has loaded must
    // not write the file the process has mapped -- Windows refuses the link on it and
    // Linux changes code under the program. So an empty `artifact_dir` on a
    // single-source recipe completes to the WORKSPACE's `out`, after the workspace
    // itself is completed, and the tool judges the file the build actually wrote. A
    // CMake-target recipe's empty directory still means the host's, and an explicit one
    // is honoured as written for both kinds.
    zengine::builder::SingleSourceRecipe one;
    one.source = "/abs/one.cpp";
    zengine::builder::Recipe single;
    single.id = "one";
    single.artifact = "zengine-one";
    single.single_source = one;
    zengine::builder::Recipe target;
    target.id = "two";
    target.artifact = "zengine-two";
    target.cmake_target = zengine::builder::CMakeTargetRecipe{"/tree", "two", std::string()};
    zengine::builder::Recipe aimed;
    aimed.id = "three";
    aimed.artifact = "zengine-three";
    aimed.artifact_dir = "/elsewhere";
    aimed.single_source = one;
    std::vector<zengine::builder::Recipe> all{single, target, aimed};
    recipe_persist::complete_recipes(all, "/install", "/project");
    REQUIRE(all.size() == 3);
    CHECK(all[0].single_source->workspace == "/install/build-workspace/one");
    CHECK(all[0].artifact_dir == "/install/build-workspace/one/out");
    CHECK(all[1].artifact_dir == "/install");
    CHECK(all[2].artifact_dir == "/elsewhere");
    // THE VIEW FOLLOWS, so the tool looks where CMake was told to write...
    CurrentRecipes owner;
    owner.hold("/project/recipes.json", all, &HostContext::so_in);
    REQUIRE(owner.views().size() == 3);
    CHECK(owner.views()[0].path ==
          HostContext::so_in("/install/build-workspace/one/out", "zengine-one"));
    CHECK(owner.views()[0].path.find("/install/zengine-one") == std::string::npos);
    // ...AND THE GENERATED PROJECT AIMS THE SAME DIRECTORY.
    CHECK(zengine::builder::generated_project(owner.all()[0])
              .find("/install/build-workspace/one/out") != std::string::npos);
    // ...WHILE THE FILE'S OWN ROW STAYS AS AUTHORED: the completion is never written back.
    const std::vector<zengine::builder::Recipe> authored{single};
    CHECK(recipe_persist::to_text(authored).find("build-workspace") == std::string::npos);
}

// ============================================================================
// LOAD-IT -- WHICH PLAN A LAUNCH MEANS
// ============================================================================
//
// PICK-1's own cases went with the browser: picking a buildable place, the chooser that
// enumerates it and the fields a maker types are all the Files pane's gestures now, and
// they are driven through the pane's own seam where the pane is. What stays here is the
// launch rule -- which load plan a run means -- because it is decided before any pane
// exists, by a pure function over two directories and a probe.

TEST_CASE("LOAD-IT: the project plan at the captured root is the plan in force when no "
          "--load-plan is given") {
    // THE LAUNCH RULE, AS A PURE FUNCTION: explicit wins; else the project plan when it is
    // there; else the shipped default. The probe is the caller's, so the rule is decided
    // by an answer and never by a disk.
    const auto present_at = [](const std::string& where) {
        return [where](const std::string& path) { return path == where; };
    };
    const std::string project = "/project/" + std::string(load_persist::kProjectLoadPlanName);
    const std::string shipped = "/install/" + std::string(load_persist::kDefaultLoadPlanName);
    CHECK(load_persist::plan_in_force("/explicit.json", "/project", "/install",
                                      present_at(project)) == "/explicit.json");
    CHECK(load_persist::plan_in_force("", "/project", "/install", present_at(project)) ==
          project);
    CHECK(load_persist::plan_in_force("", "/project", "/install", present_at("/elsewhere")) ==
          shipped);
    CHECK(load_persist::plan_in_force("", "", "/install", present_at(project)) == shipped);
    // ...AND THE HOST SPELLS THE RULE ONCE: it names the function and no second default.
    const std::string host = file_source(WORKSHOP_HOST_CPP);
    CHECK(host.find("load_persist::plan_in_force(") != std::string::npos);
    CHECK(host.find("kDefaultLoadPlanName") == std::string::npos);
}
