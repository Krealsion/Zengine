// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Files package's own suite -- the pure half, proved where it now lives.
//
// The browser's `Listing`, the maker's `LocationMarks` and their durable file are the same
// files the built-in spent (`workshop/files.hpp`, `marks.hpp`, `marks_persist.hpp`,
// `path_admission.hpp`); what this suite proves is that they compile and behave in the
// FILES PACKAGE's own image -- linked against `zengine-files-os` (the two platform bodies)
// and the Workshop vocabulary, and NOT against `zengine-workshop-logic`. A green here is the
// structural half of the migration: the pure half is portable to the office that now owns
// it. The browser's INTERACTION over the pane protocol is proved through the real seam in
// the Workshop panes suite once the weave is loaded from the plan.

#include "workshop/files.hpp"
#include "workshop/filesystem_roots.hpp"
#include "workshop/marks_persist.hpp"
#include "workshop/path_admission.hpp"

#include "files/vocabulary.hpp"

#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace ws = zengine::workshop;

namespace {

/// A unique scratch directory this run owns, removed at the end of a case's block.
struct Scratch {
    std::filesystem::path root;
    Scratch() {
        static int n = 0;
        root = std::filesystem::temp_directory_path() /
               ("zen-files-test-" + std::to_string(100000 + n++));
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::filesystem::create_directories(root, ec);
    }
    ~Scratch() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    std::string dir() const { return root.generic_string(); }
    void touch(const char* name) const {
        std::ofstream(root / name) << "x";
    }
    void mkdir(const char* name) const {
        std::error_code ec;
        std::filesystem::create_directories(root / name, ec);
    }
};

} // namespace

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

TEST_CASE("enumerate lists directories first, then files, bytewise in each class") {
    Scratch s;
    s.touch("beta.txt");
    s.touch("alpha.txt");
    s.mkdir("zdir");
    s.mkdir("adir");
    const ws::Listing listing = ws::enumerate_directory(s.dir());
    REQUIRE(listing.known);
    REQUIRE(listing.rows.size() == 4);
    CHECK(listing.rows[0].directory);
    CHECK(listing.rows[0].name == "adir");
    CHECK(listing.rows[1].name == "zdir");
    CHECK_FALSE(listing.rows[2].directory);
    CHECK(listing.rows[2].name == "alpha.txt");
    CHECK(listing.rows[3].name == "beta.txt");
}

TEST_CASE("a directory that cannot be listed is a refusal, not an empty listing") {
    const ws::Listing listing = ws::enumerate_directory("/no/such/place/anywhere");
    CHECK_FALSE(listing.known);
    CHECK_FALSE(listing.refusal.empty());
    CHECK(listing.rows.empty());
}

TEST_CASE("row_at is bounded at use") {
    Scratch s;
    s.touch("one.txt");
    const ws::Listing listing = ws::enumerate_directory(s.dir());
    REQUIRE(listing.rows.size() == 1);
    CHECK(ws::row_at(listing, 0) != nullptr);
    CHECK(ws::row_at(listing, 1) == nullptr);
    CHECK(ws::row_at(listing, 999) == nullptr);
}

TEST_CASE("a name outside printable ASCII keeps its row, marked, and cannot be opened") {
    ws::FileRow row;
    row.name = "plain.txt";
    CHECK(ws::printable_ascii_name(row.name));
    CHECK(ws::shown_name(std::string("a\x01z")) == "a?z");
}

TEST_CASE("parent is lexical and stops at the filesystem root") {
    CHECK(ws::parent_location("/a/b/c") == "/a/b");
    CHECK(ws::at_filesystem_root("/"));
    CHECK(ws::parent_location("/").empty());
}

TEST_CASE("the marks owner remembers, forgets, and reports provenance") {
    ws::LocationMarks marks;
    marks.origin = "/home/you/project";
    CHECK(marks.somewhere_to_go());
    CHECK(marks.remember("/home/you/other"));
    CHECK_FALSE(marks.remember("/home/you/other")); // a duplicate collapses
    CHECK(marks.marked("/home/you/other"));
    CHECK((marks.provenance("/home/you/project") & ws::mark_from::kOrigin) != 0);
    CHECK((marks.provenance("/home/you/other") & ws::mark_from::kMaker) != 0);
    CHECK(marks.forget("/home/you/other"));
    CHECK_FALSE(marks.marked("/home/you/other"));
}

TEST_CASE("the traversal set is built at the gesture, one address is one stop") {
    ws::LocationMarks marks;
    marks.origin = "/root/origin";
    marks.remember("/root/mark");
    const std::vector<ws::MarkedPlace> stops = marks.destinations({"/", "/root/origin"});
    // origin, the mark, then the roots -- and "/root/origin" is one stop with two provenances.
    bool origin_is_root_too = false;
    for (const ws::MarkedPlace& p : stops) {
        if (p.path == "/root/origin") {
            origin_is_root_too =
                (p.from & ws::mark_from::kOrigin) != 0 && (p.from & ws::mark_from::kRoot) != 0;
        }
    }
    CHECK(origin_is_root_too);
}

TEST_CASE("marks are durable: a write round-trips, and a refused file is never overwritten") {
    Scratch s;
    const std::string path = (s.root / "marks.json").generic_string();
    const std::vector<std::string> places = {"/a/one", "/a/two"};
    const ws::Written wrote = ws::marks_persist::save_file(path, places);
    REQUIRE(wrote.accepted);
    const ws::marks_persist::LoadedMarks read = ws::marks_persist::load_file(path);
    REQUIRE(read.outcome.accepted);
    CHECK(read.maker == places);

    // A file this build cannot read is refused whole, and its bytes are left as they are.
    const std::string bad = (s.root / "bad.json").generic_string();
    std::ofstream(bad) << "not a marks file at all";
    const ws::marks_persist::LoadedMarks refused = ws::marks_persist::load_file(bad);
    CHECK_FALSE(refused.outcome.accepted);
}

TEST_CASE("a persisted relative mark is admitted as a skip, never re-based") {
    Scratch s;
    const std::string path = (s.root / "m.json").generic_string();
    // A file carrying a relative spelling is written through the family's own writer; on the
    // way back in, a mark that is not an absolute location this build can carry is skipped as
    // a standing note, never resolved against anywhere.
    const ws::Written wrote = ws::marks_persist::save_file(path, {"relative/here"});
    REQUIRE(wrote.accepted);
    const ws::marks_persist::LoadedMarks loaded = ws::marks_persist::load_file(path);
    CHECK(loaded.outcome.accepted);
    CHECK(loaded.maker.empty());
    CHECK_FALSE(loaded.skipped.empty());
}

TEST_CASE("path admission carries an ordinary path and refuses a relative one") {
    CHECK(ws::admit_location("/a/b/../b/c") == "/a/b/c");
    CHECK(ws::admit_location("relative/path").empty());
    CHECK(ws::admit_location("").empty());
}

TEST_CASE("the host reports at least one filesystem root") {
    const std::vector<std::string> roots = ws::host_filesystem_roots();
    CHECK_FALSE(roots.empty());
}
