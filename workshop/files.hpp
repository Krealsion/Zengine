// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FILES_HPP
#define ZENGINE_WORKSHOP_FILES_HPP

// BROWSING THE PROJECT A MAKER LAUNCHED INTO.
//
// WHAT THIS IS: one directory's worth of names, at a place inside the project root, held
// as a snapshot until something asks for a new one. That is the whole subject. It is the
// `editor.hpp` split applied to a list instead of to a document -- the machinery and the
// state live here, and the pane that shows them is presentation only.
//
// WHAT THIS IS NOT, and the absences are load-bearing:
//
//   NOT A FILESYSTEM SERVICE.  One enumeration of one directory, on demand. Nothing here
//                              is asked about a path it was not just handed, nothing
//                              caches across directories, and no second consumer exists
//                              to generalize for.
//   NOT AN INDEX.              Nothing walks recursively, nothing is remembered between
//                              directories, and there is no search. A project's shape is
//                              not this application's to hold.
//   NOT A WATCHER.             Nothing polls, nothing subscribes, no timer beats. A
//                              listing is recomputed when a maker acts and when a build
//                              finishes -- both moments somebody already caused.
//   NOT A SECOND SOURCE TRUTH. A row is a NAME and a KIND. It holds no resolved path, no
//                              recipe, no artifact, no build state and no editor state,
//                              so the browser cannot come to disagree with the parties
//                              that own those. What a row DENOTES is derived, at the
//                              moment it is activated, from root + the names walked into
//                              + this row's name.
//
// THE ROOT BOUNDARY IS THE REPRESENTATION, NOT A CHECK. The current directory is a STACK
// OF NAMES entered below the root; parent means pop, and there is no `..` row to press. A
// maker therefore cannot navigate above the root because there is nothing to say that
// with -- which is a stronger promise than a comparison somebody has to remember to make,
// and it needs no canonicalization, no containment resolver and no secure-path machinery.
//
// THE ONE HOLE IN THAT PROMISE IS NAMED AND CLOSED: entering a LINKED DIRECTORY would
// leave the project's tree while every name on the stack still says otherwise. Such a row
// is shown -- this browser does not hide real entries -- and refuses to be entered, in
// words. That is a boundary this file imposes and can therefore describe honestly; it is
// not a claim that the project root is a security perimeter, and nothing here should be
// read as one.
//
// FILENAMES ARE BYTES THIS APPLICATION CAN CARRY, OR THEY ARE MARKED. Every path in
// Workshop is a `std::string`, and on Windows a narrow `path::string()` cannot faithfully
// carry a name outside the active code page. So names are taken as UTF-8 (`u8string`) and
// split once: a name that is entirely printable ASCII is exact, comparable and openable on
// both supported platforms; any other name keeps its row, is shown as a projection that
// makes the loss visible, and refuses activation. The alternative -- narrowing quietly --
// would hand the editor a path that names a different file or no file, which is the one
// outcome worse than a refusal.
//
// ...AND ASKING FOR THOSE BYTES IS ITSELF A CONVERSION THAT CAN REFUSE. MEASURED on
// MSVC/NTFS: a filename holding ill-formed UTF-16 -- an unpaired surrogate, which
// `CreateFileW` accepts and somebody else's program can therefore leave in any directory --
// makes `u8string()` THROW, and an exception out of this walk is not a marked row, it is
// the browser gone. So the name is taken through `path_admission.hpp`, which always answers
// with something: the filesystem's own bytes when it has them, and the same `?` marking one
// layer earlier when it does not. Such a row is shown, is NOT exact, and refuses activation
// like any other name this application cannot say -- one law, one more way to enter it.


//
// THIS IS A FILENAME-CUSTODY LAW AND NOT A CONTENT LAW. What may be INSIDE a file is the
// editor's question, answered at the editor's door in the editor's vocabulary. The browser
// pre-judges nothing: a `.png`, a binary, a file with mixed line endings all reach the door
// and are refused there, by the party whose sentence is true.

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

namespace zengine::workshop {

/// HOW MANY ENTRIES ONE LISTING WILL HOLD.
///
/// A BOUND ON WORK THIS APPLICATION DID NOT CHOOSE. Every other population Workshop paints
/// is one it built; a directory is somebody else's, and a build output tree or a home
/// directory can hold more names than a pane could ever show. So the walk stops, and what
/// it says afterwards is exactly what it observed -- it stopped counting here -- and never
/// a total it never reached (QR-4: a bound claims what it read).
///
/// Two thousand is generous against any directory a person authors by hand and tight
/// against a machine-generated one. It bounds the memory a hostile directory can make this
/// session hold to a few hundred kilobytes of names.
inline constexpr std::size_t kMaxListedEntries = 2000;

/// ONE ROW: a name and a kind, and deliberately nothing else.
struct FileRow {
    /// The filename bytes, UTF-8, as the filesystem gave them -- and IDENTITY exactly when
    /// `openable` says so: these are the bytes joined to the current directory when the row
    /// is activated, and they are never the projection a painter shows. When `openable` is
    /// false they may not even BE the filesystem's bytes (a name the platform would not
    /// spell at all is projected here); they name nothing, and no door spends them.
    std::string name;
    bool directory = false;
    /// A DIRECTORY THAT LEAVES THE TREE. True only for a directory row whose own
    /// (unfollowed) status is not a directory -- a symbolic link, or the platform's
    /// equivalent where the standard library reports one. Shown, never entered.
    bool linked = false;
    /// Can this name be carried through Workshop's narrow path custody at all? False makes
    /// the row a marked projection that refuses activation, and there are TWO ways to earn
    /// it: bytes outside printable ASCII, and a name this platform would not spell in this
    /// application's vocabulary at all. This field, never the bytes, is what a door asks --
    /// the second kind's `name` is already a projection and can look perfectly ordinary.
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

/// WHAT A NAME THIS APPLICATION CANNOT CARRY LOOKS LIKE ON SCREEN -- and it is a
/// PROJECTION, never an identity. Each byte outside printable ASCII becomes one `?`, so
/// the loss is visible at the position it happened rather than hidden by a tidy
/// substitution. Nothing ever opens this string, compares it, or writes it to a file.
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
///
/// Directories first because entering is the browser's other verb and a maker looking for
/// somewhere to go should not read past the files to find it. Bytewise because it is the
/// one order that is the same on every machine: a locale-aware collation would sort one
/// maker's project differently from another's, and this pane's whole job is to say what is
/// there. It compares the ADMITTED NAME BYTES, so what a row sorts by is what a row is.
inline bool row_before(const FileRow& a, const FileRow& b) {
    if (a.directory != b.directory) {
        return a.directory;
    }
    return a.name < b.name;
}

/// ONE DIRECTORY'S LISTING, AS A SNAPSHOT.
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
inline Listing enumerate_directory(const std::string& dir) {
    Listing out;
    if (dir.empty()) {
        out.refusal = "no project directory to browse";
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

/// WHAT THE MAKER IS CURRENTLY BROWSING -- session state, and work in progress rather than
/// desk: nothing here is written to a file, restored at launch, or carried between runs.
/// Where the pane IS lives in the setup like every pane's does; where a maker had walked to
/// inside it does not, for WUX-0's reason (the desk, never the work in progress).
struct FilesPane {
    /// The names walked into, below the root, outermost first. Empty = at the root. This
    /// IS the current directory, and it is why there is no path to validate: every element
    /// is a name this browser itself listed.
    std::vector<std::string> entered;
    Listing listing;
    std::size_t cursor = 0;    ///< which row the maker is on; bounded at use, never at write
    double wheel_accum = 0.0;  ///< fractional wheel notches not yet worth a row
};

/// The directory the pane is currently showing, spelled from the root and the stack. It is
/// derived at every spend and stored nowhere -- the same discipline a pane's rectangle is
/// under -- so a stack that changes cannot leave a stale path behind it.
inline std::string current_dir(const std::string& root, const std::vector<std::string>& entered) {
    if (root.empty()) {
        return std::string();
    }
    std::filesystem::path p(root);
    for (const std::string& name : entered) {
        p /= name;
    }
    return p.lexically_normal().generic_string();
}

/// THE PROJECT-RELATIVE PROJECTION OF WHERE THE MAKER IS -- for a header to show, and for
/// nothing else. The root is usually the longest and least interesting part of an absolute
/// path, and a pane header is the one place in this application where the maker already
/// knows what it would say. Identity stays absolute (`current_dir`); this is what that
/// looks like from the project's own doorstep.
inline std::string relative_dir(const std::vector<std::string>& entered) {
    std::string out;
    for (const std::string& name : entered) {
        if (!out.empty()) {
            out += "/";
        }
        out += shown_name(name);
    }
    return out;
}

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
