// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_MARKS_HPP
#define ZENGINE_WORKSHOP_MARKS_HPP

// PLACES A MAKER MAY WANT TO COME BACK TO.
// Workshop law: agents/workshop/files.md

#include "path_admission.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHY A LOCATION IS KNOWN. Flags rather than an enumeration: one place can be the origin,
/// a maker's own mark and a filesystem root simultaneously, and a presentation that had to
/// pick one of the three would have to pick wrongly.
namespace mark_from {
inline constexpr std::uint8_t kOrigin = 1u << 0;
inline constexpr std::uint8_t kMaker = 1u << 1;
inline constexpr std::uint8_t kRoot = 1u << 2;
} // namespace mark_from

/// ONE TRAVERSAL DESTINATION: where it is, and how it came to be known.
struct MarkedPlace {
    std::string path;      ///< absolute, lexically normal, generic slashes
    std::uint8_t from = 0; ///< one or more `mark_from` flags
};

/// IS THIS LOCATION ITS OWN LEXICAL PARENT -- the top of what a path can say?
// WL-FILES-03 -- agents/workshop/files.md
inline bool at_filesystem_root(const std::string& dir) {
    if (dir.empty()) {
        return false;
    }
    const std::filesystem::path p(dir);
    return p.parent_path() == p;
}

/// THE LEXICAL PARENT OF `dir`, or EMPTY when `dir` is its own parent.
// WL-FILES-03 -- agents/workshop/files.md
inline std::string parent_location(const std::string& dir) {
    if (dir.empty()) {
        return std::string();
    }
    const std::filesystem::path p(dir);
    const std::filesystem::path up = p.parent_path();
    if (up == p) {
        return std::string();
    }
    return admit_location(admit_path(up).spelling);
}

/// THE MARKS THIS RUN KNOWS -- one owner, session-level, outside every pane.
struct LocationMarks {
    /// Has this run generated its origin and read its durable marks yet?
    // WL-FILES-02 -- agents/workshop/files.md
    bool settled = false;

    /// WHERE THIS RUN'S NAVIGATION BEGAN, or empty when it had nowhere to begin.
    // WL-FILES-02 -- agents/workshop/files.md
    std::string origin;

    /// THE DURABLE HALF: places somebody deliberately asked to keep. Sorted and unique by
    /// their normalized absolute spelling, so the traversal order is the same on every run
    /// and the written file's bytes are deterministic.
    std::vector<std::string> maker;

    /// DOES THIS RUN KNOW ANYWHERE AT ALL, without asking a platform?
    // WL-FILES-06, WL-FILES-07 -- agents/workshop/files.md
    bool somewhere_to_go() const { return !origin.empty() || !maker.empty(); }

    /// Is this exact location one of the maker's own marks?
    bool marked(const std::string& path) const {
        return std::binary_search(maker.begin(), maker.end(), path);
    }

    /// KEEP THIS PLACE. False when it was already kept -- a duplicate collapses to the one
    /// maker fact rather than becoming a second stop with the same address.
    bool remember(std::string path) {
        if (path.empty()) {
            return false;
        }
        const std::vector<std::string>::iterator at =
            std::lower_bound(maker.begin(), maker.end(), path);
        if (at != maker.end() && *at == path) {
            return false;
        }
        maker.insert(at, std::move(path));
        return true;
    }

    /// FORGET THIS PLACE -- the maker fact only. Whether the location is also this run's
    /// origin or a filesystem root is not the maker's to revoke and is untouched.
    bool forget(const std::string& path) {
        const std::vector<std::string>::iterator at =
            std::lower_bound(maker.begin(), maker.end(), path);
        if (at == maker.end() || *at != path) {
            return false;
        }
        maker.erase(at);
        return true;
    }

    /// HOW THIS RUN KNOWS A LOCATION, without asking the platform anything.
    // WL-FILES-07 -- agents/workshop/files.md
    std::uint8_t provenance(const std::string& path) const {
        std::uint8_t from = 0;
        if (path.empty()) {
            return from;
        }
        if (!origin.empty() && path == origin) {
            from |= mark_from::kOrigin;
        }
        if (marked(path)) {
            from |= mark_from::kMaker;
        }
        if (at_filesystem_root(path)) {
            from |= mark_from::kRoot;
        }
        return from;
    }

    /// EVERY PLACE THIS RUN CAN JUMP TO, IN ONE DETERMINISTIC ORDER.
    // WL-FILES-06 -- agents/workshop/files.md
    std::vector<MarkedPlace> destinations(const std::vector<std::string>& roots) const {
        std::vector<MarkedPlace> out;
        const auto join = [&out](const std::string& path, std::uint8_t from) {
            if (path.empty()) {
                return;
            }
            for (MarkedPlace& seen : out) {
                if (seen.path == path) {
                    seen.from |= from;
                    return;
                }
            }
            out.push_back(MarkedPlace{path, from});
        };
        join(origin, mark_from::kOrigin);
        for (const std::string& path : maker) {
            join(path, mark_from::kMaker);
        }
        for (const std::string& path : roots) {
            join(path, mark_from::kRoot);
        }
        return out;
    }
};

/// WHAT A MAKER IS TOLD ABOUT WHY A PLACE IS KNOWN, in the order a sentence wants them.
/// Empty when a location is merely somewhere they walked to, which is the ordinary case
/// and deserves no word at all.
inline std::string provenance_words(std::uint8_t from) {
    std::string out;
    const auto add = [&out](const char* word) {
        if (!out.empty()) {
            out += ", ";
        }
        out += word;
    };
    if ((from & mark_from::kOrigin) != 0) {
        add("origin");
    }
    if ((from & mark_from::kMaker) != 0) {
        add("marked");
    }
    if ((from & mark_from::kRoot) != 0) {
        add("filesystem root");
    }
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_MARKS_HPP
