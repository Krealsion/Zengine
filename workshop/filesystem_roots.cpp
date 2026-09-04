// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `filesystem_roots.hpp`'s section -- the filesystem roots this host reports --
// compiled once into `zengine-workshop-logic` and linked by the host and every suite; the
// declarations, the constants and the constexpr functions stay in the header.
// Workshop law: agents/workshop/files.md

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "filesystem_roots.hpp"

namespace zengine::workshop {

// WL-FILES-06, WL-FILES-07 -- agents/workshop/files.md
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
