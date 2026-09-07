// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// THE TWO OUT-OF-LINE BODIES THE FILES TOOL'S PURE HALF NEEDS -- whether a directory entry
// leaves the tree, and which roots this operating system reports. Everything else the
// browser and the marks spend is header-only (`files.hpp`, `marks.hpp`, `marks_persist.hpp`,
// `path_admission.hpp`); these two ask the platform, so they are the two places
// `<windows.h>` may appear and they are compiled here, in the tool's own image.
//
// They are the bodies `workshop/files.cpp` and `workshop/filesystem_roots.cpp` hold for the
// host today, in `namespace zengine::workshop` because the headers that declare them are
// the same ones -- the pure half moved offices, not namespaces, so every WL-FILES law names
// the same identifiers. When the built-in retires, the host's copies leave and these are the
// only ones.

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "workshop/files.hpp"
#include "workshop/filesystem_roots.hpp"

namespace zengine::workshop {

// Whether an entry leaves the tree (the WL-FILES-04 body). Carried from
// `workshop/files.cpp` unchanged; its `// WL-FILES-04` pointer moves here when the built-in
// retires and this becomes the only copy -- until then the canonical body is the host's.
bool leaves_the_tree(const std::filesystem::directory_entry& entry) {
#if defined(_WIN32)
    const DWORD attributes = ::GetFileAttributesW(entry.path().c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return true; // an unfollowed query that fails marks the row, as it always has
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code link_ec;
    const std::filesystem::file_status own = entry.symlink_status(link_ec);
    return link_ec ? true : !std::filesystem::is_directory(own);
#endif
}

// Which roots this operating system reports (the WL-FILES-06 / WL-FILES-07 body). Carried
// from `workshop/filesystem_roots.cpp` unchanged; the pointer moves here at retirement.
std::vector<std::string> host_filesystem_roots() {
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
