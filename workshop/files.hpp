// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FILES_HPP
#define ZENGINE_WORKSHOP_FILES_HPP

// BROWSING A FILESYSTEM, FROM WHEREVER THIS WORKSHOP BEGAN.
// Workshop law: agents/workshop/files.md



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
#include "path_admission.hpp"

// ...AND WHERE A LOCATION MAY BE SEEDED FROM, AND WHAT ITS PARENT IS. The browser is the
// first consumer of the marks, not their owner -- `parent_location` is a lexical fact about
// a path and lives beside the places that path may be one of.
#include "marks.hpp"

namespace zengine::workshop {

/// HOW MANY ENTRIES ONE LISTING WILL HOLD.
// WL-FILES-13 -- agents/workshop/files.md
inline constexpr std::size_t kMaxListedEntries = 2000;

/// ONE ROW: a name and a kind, and deliberately nothing else.
struct FileRow {
    /// The filename bytes, UTF-8, as the filesystem gave them.
    // WL-FILES-10 -- agents/workshop/files.md
    std::string name;
    bool directory = false;
    /// A DIRECTORY THAT LEAVES THE TREE.
    // WL-FILES-04 -- agents/workshop/files.md
    bool linked = false;
    /// Can this name be carried through Workshop's narrow path custody at all?
    // WL-FILES-10, WL-FILES-11 -- agents/workshop/files.md
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
            // A LINK IS A DIRECTORY THAT DISAGREES WITH ITSELF: followed, it is a
            // directory; unfollowed, it is not. That comparison is standard C++ and needs
            // no per-platform predicate -- and it catches whatever this platform's
            // standard library reports a reparse point as, rather than only the one kind
            // named `symlink`.
            std::error_code link_ec;
            const std::filesystem::file_status own = entry.symlink_status(link_ec);
            row.linked = link_ec ? true : !std::filesystem::is_directory(own);
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

/// WHAT THE MAKER IS CURRENTLY BROWSING -- session state, work in progress rather than desk.
// WL-FILES-01, WL-FILES-02 -- agents/workshop/files.md
struct FilesPane {
    /// THE ONE ABSOLUTE LOCATION THIS BROWSER IS SHOWING. Lexically normal, forward
    /// separators, and empty exactly while this run has nowhere to begin.
    // WL-FILES-01, WL-FILES-02, WL-FILES-09 -- agents/workshop/files.md
    std::string current_dir;
    Listing listing;
    std::size_t cursor = 0;    ///< which row the maker is on; bounded at use, never at write
    double wheel_accum = 0.0;  ///< fractional wheel notches not yet worth a row
};

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

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_FILES_HPP
