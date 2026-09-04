// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `files.hpp`'s section -- whether a directory leaves the tree, asked of the host
// -- compiled once into `zengine-workshop-logic` and linked by the host and every suite; the
// declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/files.md

// THE ONE ATTRIBUTE THIS BROWSER ASKS THE HOST FOR: whether a directory entry is a reparse
// point. Reached the way `filesystem_roots.hpp` reaches Win32.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "files.hpp"

namespace zengine::workshop {

// WL-FILES-04 -- agents/workshop/files.md
bool leaves_the_tree(const std::filesystem::directory_entry& entry) {
#if defined(_WIN32)
    const DWORD attributes = ::GetFileAttributesW(entry.path().c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return true; // an unfollowed query that fails marks the row, as it always has
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    // A LINK IS A DIRECTORY THAT DISAGREES WITH ITSELF: followed, it is a directory;
    // unfollowed, it is not -- whatever kind of link this library reports it as.
    std::error_code link_ec;
    const std::filesystem::file_status own = entry.symlink_status(link_ec);
    return link_ec ? true : !std::filesystem::is_directory(own);
#endif
}

} // namespace zengine::workshop
