// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FILESYSTEM_ROOTS_HPP
#define ZENGINE_WORKSHOP_FILESYSTEM_ROOTS_HPP

// THE ONE PLACE THIS REPOSITORY ASKS AN OPERATING SYSTEM WHICH FILESYSTEM ROOTS IT HAS.
// Workshop law: agents/workshop/files.md

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
// WL-FILES-06, WL-FILES-07 -- agents/workshop/files.md
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
