// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FILESYSTEM_ROOTS_HPP
#define ZENGINE_WORKSHOP_FILESYSTEM_ROOTS_HPP

// THE ONE PLACE THIS REPOSITORY ASKS AN OPERATING SYSTEM WHICH FILESYSTEM ROOTS IT HAS.
//
// It is a header of its own and small on purpose, exactly as `surface/terminal_size.hpp`
// is one floor down: everything above it is platform-neutral, and the `#if defined(_WIN32)`
// below is the only one any of it needs.
//
// WHY IT EXISTS AT ALL, MEASURED. Going up is `parent_path()` until a path is its own
// parent, and on POSIX that reaches `/` and there is nothing else to reach. On Windows
// `C:/`, `G:/` and `//server/` are three unrelated roots with NO lexical relationship
// whatsoever -- no sequence of parents crosses between them -- so a browser that could only
// go up could never leave the volume it started on. This answers the one question that
// makes the other drives reachable, and it answers it with a list rather than with a mode.
//
// WHAT IT IS NOT, and the absences are the whole design:
//
//   NOT A REGISTRY.   Nothing is held. The list is asked for at the gesture and spent
//                     immediately, because a drive is a fact about a machine at a moment --
//                     media is inserted and removed, shares mount and vanish -- and a
//                     remembered list is a list that goes wrong silently.
//   NOT A CLAIM OF    On Windows these are the LOGICAL DRIVES this system reports, and they
//   COMPLETENESS.     are not every path this process could address: a UNC share is
//                     reachable by spelling and appears in no drive list. So the word for
//                     them is "host-reported roots", never "every root".
//   NOT A FILTER.     A drive with no media in it is reported and is not listable, and that
//                     is left exactly as it is: the maker jumps there and meets the
//                     filesystem's own sentence, which is more honest than a browser that
//                     quietly decided which of their drives were worth showing.
//   NOT A FILESYSTEM  Nothing is opened, mounted, enumerated or stat'ed. On POSIX this
//   SERVICE.          function touches no syscall at all.

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

namespace zengine::workshop {

/// THE FILESYSTEM ROOTS THIS HOST REPORTS, ascending, spelled the way every path in this
/// application is spelled: absolute, with forward separators.
///
/// POSIX: the literal `/`. There is one root, it is always there, and asking the system
/// about it would be asking a question whose answer is written in the standard.
///
/// WINDOWS: `GetLogicalDrives()`, which is one call returning a bit per drive letter and
/// needs no string conversion at all -- so the narrowest of the three mechanisms available,
/// and the one that cannot itself fail to be carried (`A:/` through `Z:/` are ASCII by
/// construction). The drive TYPE is deliberately not asked: filtering by it would be this
/// application deciding which of a maker's drives are worth offering, and a drive that
/// cannot be listed already refuses in the filesystem's own words.
inline std::vector<std::string> host_filesystem_roots() {
    std::vector<std::string> out;
#if defined(_WIN32)
    const DWORD mask = ::GetLogicalDrives();
    for (int letter = 0; letter < 26; ++letter) {
        if ((mask & (DWORD{1} << letter)) == 0) {
            continue;
        }
        std::string root;
        root.push_back(static_cast<char>('A' + letter));
        root += ":/";
        out.push_back(std::move(root));
    }
#else
    out.push_back("/");
#endif
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_FILESYSTEM_ROOTS_HPP
