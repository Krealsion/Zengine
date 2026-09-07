// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FILES_HPP
#define ZENGINE_WORKSHOP_FILES_HPP

// BROWSING A FILESYSTEM, FROM WHEREVER THIS WORKSHOP BEGAN.
// Files law: agents/files.md



#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>


// WHETHER A FILESYSTEM PATH CAN BE SAID AT ALL. The browser is one of two places that turn
// something the OS reported into a Workshop path string, and both of them used to be able
// to end the process by asking.
#include "workshop/path_admission.hpp"

// ...AND WHERE A LOCATION MAY BE SEEDED FROM, AND WHAT ITS PARENT IS. The browser is the
// first consumer of the marks, not their owner -- `parent_location` is a lexical fact about
// a path and lives beside the places that path may be one of.
#include "files/marks.hpp"

namespace zengine::workshop {

/// HOW MANY ENTRIES ONE LISTING WILL HOLD.
// WL-FILES-13 -- agents/workshop/files.md
inline constexpr std::size_t kMaxListedEntries = 2000;

/// ONE ROW: a name and a kind, and deliberately nothing else.
struct FileRow {
    /// The filename bytes, UTF-8, as the filesystem gave them.
    std::string name;
    bool directory = false;
    /// A DIRECTORY THAT LEAVES THE TREE.
    // WL-FILES-04 -- agents/workshop/files.md
    bool linked = false;
    /// Can this name be carried through Workshop's narrow path custody at all?
    // WL-FILES-10 -- agents/workshop/files.md
    bool openable = true;
};

/// Is every byte of this name plainly printable ASCII -- the bytes both supported
/// platforms carry identically through a `std::string` path, and the bytes this
/// application's media can place truthfully in a column?
inline bool printable_ascii_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    for (const char c : name) {
        const unsigned char b = static_cast<unsigned char>(c);
        if (b < 0x20 || b > 0x7E) {
            return false;
        }
    }
    return true;
}

/// WHAT A NAME THIS APPLICATION CANNOT CARRY LOOKS LIKE ON SCREEN.
// WL-FILES-10 -- agents/workshop/files.md
inline std::string shown_name(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (const char c : name) {
        const unsigned char b = static_cast<unsigned char>(c);
        out.push_back(b < 0x20 || b > 0x7E ? '?' : c);
    }
    return out;
}

/// DIRECTORIES FIRST, THEN FILES, AND BYTEWISE INSIDE EACH CLASS.
// WL-FILES-13 -- agents/workshop/files.md
inline bool row_before(const FileRow& a, const FileRow& b) {
    if (a.directory != b.directory) {
        return a.directory;
    }
    return a.name < b.name;
}

/// ONE DIRECTORY'S LISTING, AS A SNAPSHOT.
// WL-FILES-14 -- agents/workshop/files.md
struct Listing {
    /// Did an enumeration happen and produce these rows? False with a `refusal` is the
    /// ordinary absence -- no project, a directory that went away, a directory this
    /// process may not read.
    bool known = false;
    std::string refusal; ///< why not, when not; empty when known
    std::vector<FileRow> rows;
    /// Did the walk stop at the ceiling? When true, `rows.size()` is what was OBSERVED and
    /// the directory holds an unknown number more. Nothing here ever learns the total,
    /// which is exactly why nothing here ever prints one.
    bool bounded = false;
};

/// DOES THIS DIRECTORY LEAVE THE TREE? Windows asks the host, of the PATH, never the listing's
/// cached copy: one of its two standard libraries (libstdc++) cannot see a reparse point
/// unfollowed, so there following and not following agree and the mark would be lost.
bool leaves_the_tree(const std::filesystem::directory_entry& entry);

/// ENUMERATE ONE DIRECTORY.
///
/// EVERY FAILURE IS AN ORDINARY REFUSAL. The iterator is constructed and advanced through
/// its `error_code` forms, so a directory that is missing, unreadable or replaced by a file
/// while this runs produces a sentence rather than an exception -- and produces NO rows,
/// because a partial listing presented as a listing is the quiet wrong answer this pane
/// exists to avoid.
///
/// AN ENTRY THAT CANNOT BE CLASSIFIED IS STILL SHOWN, AS A FILE. Asking whether an entry
/// is a directory can fail on its own (a race, a permission on the entry rather than on
/// its parent), and the two available answers are to drop the row or to keep it under the
/// kind that cannot be entered. Dropping it would make this browser lie about what is
/// there; keeping it as a file means the worst case is a row whose activation the editor's
/// door refuses in its own words.
// WL-FILES-14 -- agents/workshop/files.md
inline Listing enumerate_directory(const std::string& dir) {
    Listing out;
    if (dir.empty()) {
        out.refusal = "there is no location to browse";
        return out;
    }
    std::error_code ec;
    std::filesystem::directory_iterator it(std::filesystem::path(dir), ec);
    if (ec) {
        out.refusal = "cannot list " + dir + ": " + ec.message();
        return out;
    }
    const std::filesystem::directory_iterator done;
    while (it != done) {
        if (out.rows.size() >= kMaxListedEntries) {
            out.bounded = true;
            break;
        }
        const std::filesystem::directory_entry entry = *it;
        FileRow row;
        // TAKING THE NAME IS ITSELF A CONVERSION THAT CAN REFUSE, and a refusal here is a
        // row rather than the end of the listing (`path_admission.hpp`). ⚠ `exact` is not
        // redundant beside the byte test: a projection can be entirely printable ASCII, so
        // dropping it would make an unsayable name openable under a spelling that names a
        // different file or no file.
        AdmittedName admitted = admit_filename(entry.path().filename());
        row.name = std::move(admitted.name);
        row.openable = admitted.exact && printable_ascii_name(row.name);
        std::error_code kind_ec;
        row.directory = entry.is_directory(kind_ec);
        if (kind_ec) {
            row.directory = false;
        }
        if (row.directory) {
            row.linked = leaves_the_tree(entry);
        }
        if (!row.name.empty()) {
            out.rows.push_back(std::move(row));
        }
        it.increment(ec);
        if (ec) {
            out.rows.clear();
            out.bounded = false;
            out.refusal = "cannot finish listing " + dir + ": " + ec.message();
            return out;
        }
    }
    std::sort(out.rows.begin(), out.rows.end(), row_before);
    out.known = true;
    return out;
}

// WHAT THE MAKER IS CURRENTLY BROWSING USED TO BE A STRUCT HERE -- `FilesPane`, a field on the
// host's `Panels`. The browser is a loaded weave now and its state is its own: two durable
// fields in a `ZEN_SHAPE` (`files/vocabulary.hpp` `FilesState`, what a same-shape reload keeps)
// and the rest -- the listing, the granted room, the wheel remainder -- private members of the
// weave that never leave its image. So there is no pane object here to hand around, which is
// the point: nothing outside the weave can read or write where a maker is looking.

/// The row the cursor is on, or null when the listing is empty or the cursor outlived it.
/// Bounded AT USE, never at write: rows are replaced wholesale by every refresh, and a
/// cursor trusted across one of those would be one enumeration away from reading past the
/// end (`BuilderPane::chosen`'s own rule).
inline const FileRow* row_at(const Listing& l, std::size_t cursor) {
    if (cursor >= l.rows.size()) {
        return nullptr;
    }
    return &l.rows[cursor];
}

// ---- WHAT THE MAKER IS TOLD, AS PURE FUNCTIONS -------------------------------------
//
// THE SENTENCES ARE THE BUILT-IN'S, AND THEY ARE HERE SO THEY STAY MEASURED. Every one of
// them was carried out of `workshop/weave_editor.cpp` and `workshop/weave_recipes.cpp`
// unchanged, and every one was witnessed by a case that read the notice row off a live
// Workshop's canvas. That rig went with the built-in: the pane is a loaded image now and
// only the whole-loop witness can read its rows. So the COMPOSITION is a value here, asked
// of it directly, and the pane spends nothing else -- which keeps the evidence the
// migration would otherwise have quietly dropped.
//
// ⚠ THE ORDER INSIDE A REFUSAL IS THE CLAIM, not the wording. The notice row is cut at the
// band's width, so the two SHORT fixed statements go first and the two long variable ones --
// the owner's own sentence, then the path in force -- take the tail. MEASURED (the live
// witness, before the migration): with the reason first, the reassuring half was exactly
// the half that elided.

/// WHY THIS ROW CANNOT BE A RECIPE CATALOG, or empty when it can be asked about at all.
/// Every arm says what is wrong AND that nothing moved, because the second half is the one
/// a maker needs most and the one a bare reason leaves them guessing about.
inline std::string catalog_row_refusal(const FileRow* row, bool run_began_somewhere) {
    if (row == nullptr) {
        return "no row is selected -- the recipes in force are unchanged";
    }
    if (!row->openable) {
        return "`" + shown_name(row->name) +
               "` has bytes this Workshop cannot carry in a path -- the recipes in force "
               "are unchanged";
    }
    if (row->directory) {
        return "`" + shown_name(row->name) + "` is a directory -- a recipe catalog is one "
                                             "authored file";
    }
    if (!run_began_somewhere) {
        return "this run began nowhere -- the recipes in force are unchanged";
    }
    return std::string();
}

/// THE PATH THE ANSWER SAYS IS IN FORCE, said in words when there is none.
inline std::string catalog_in_force(const std::string& path) {
    return path.empty() ? std::string("no catalog") : path;
}

/// A CHOSEN CATALOG THE OWNER REFUSED. `in_force` is what the owner answered AFTER the
/// attempt, so this sentence can never name the file that was just refused.
inline std::string catalog_refused_words(const std::string& reason, const std::string& in_force) {
    return "not a recipe catalog -- the recipes in force are unchanged: " + reason +
           "; still using " + catalog_in_force(in_force);
}

/// AN AUTHORED ROW THE OWNER REFUSED. Nothing was written, and what is running is unchanged.
inline std::string authoring_refused_words(const std::string& reason,
                                           const std::string& in_force) {
    return "no recipe was written: " + reason + "; still using " + catalog_in_force(in_force);
}

/// A CHOSEN CATALOG THE OWNER TOOK -- which file, and how much it holds.
inline std::string catalog_taken_words(const std::string& path, std::int64_t recipes) {
    return "build recipes: " + path + " (" + std::to_string(recipes) +
           (recipes == 1 ? " recipe)" : " recipes)");
}

/// A ROW THE OWNER WROTE: what the maker called it, what it produces, and where it landed.
inline std::string authored_words(const std::string& id, const std::string& artifact,
                                  const std::string& path, std::int64_t recipes) {
    return "authored recipe `" + id + "` -> " + artifact + " in " + path + " (" +
           std::to_string(recipes) + " recipes)";
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_FILES_HPP
