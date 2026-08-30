// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_MARKS_HPP
#define ZENGINE_WORKSHOP_MARKS_HPP

// PLACES A MAKER MAY WANT TO COME BACK TO.
//
// WHAT A LOCATION MARK SAYS, and it is the whole of what it says: *this filesystem
// location matters as a place I may want to return to*. It confers no authority, states no
// trust, names no project membership, changes no recipe base, and makes nothing buildable,
// editable, loadable or conformant. A mark is a DESTINATION.
//
// WHY IT IS NOT THE BROWSER'S. The Files pane is the first consumer of these and is
// deliberately not their owner: a later consumer -- a surface that says what this session
// is made of, a path shown relative to a place a maker named -- must be able to ask about
// remembered places without reaching inside a presentation. So the marks sit beside
// `panels` on the session, exactly where the source document and the current catalog's
// projection already sit, and the browser reads them like everybody else will.
//
// THREE PROVENANCES, AND THEY ARE FLAGS RATHER THAN A KIND, because one place is often
// known two ways at once -- a maker may durably mark the very directory this run began in:
//
//   ORIGIN   generated once for this run: the admitted filesystem location from which this
//            Workshop's Files navigation began. Not persisted, not moved by browsing, not
//            renamed "the project" -- it coincides with the project anchor today and the
//            two are different facts (`HostContext::project_dir` owns what a project-relative
//            spelling MEANS, and nothing here can move it).
//   MAKER    the durable half, and the only half written to a file: a place somebody
//            deliberately said they wanted back.
//   ROOT     generated from the host at the gesture, never held: what this system reports
//            as a filesystem root. Asked freshly because a drive is a fact about a machine
//            at a moment, and a remembered list of them is a list that goes wrong quietly.
//
// WHAT THIS IS NOT, and the absences are load-bearing:
//
//   NOT A PROJECT MODEL.   A mark is a directory and a reason it is known. There is no
//                          membership, no manifest, no set of files, no configuration and
//                          no second answer to "what does a relative path mean here".
//   NOT A TAG SYSTEM.      There are no names, no categories, no colours and no arbitrary
//                          annotations. An absolute path is identity enough until a real
//                          consumer needs a human name for one.
//   NOT A HISTORY.         Nothing records where a maker has been. A mark is an explicit
//                          act; walking somewhere is not.
//   NOT A FILE REGISTRY.   Directories only. A file mark would be a different fact with a
//                          different lifetime, and inventing one here to keep the shape
//                          symmetrical would be inventing a consumer.
//   NOT AUTHORITY.         The OS decides what this process may read. Nothing here models,
//                          simulates or claims a filesystem boundary, and a mark to a
//                          place that cannot be listed is still a mark -- the refusal a
//                          maker meets there is the filesystem's own.

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
///
/// The MEASURED root test on both families, and the reason it is spelled here rather than
/// asked of `has_parent_path()`: that predicate answers TRUE at `/` and at `C:\` (measured,
/// both platforms), so a boundary built on it would be a boundary that never fires. It is
/// purely lexical, touches no disk and asks no platform, which is what lets a painter spend
/// it -- a listing is somebody else's population and a header is not the place to walk one.
inline bool at_filesystem_root(const std::string& dir) {
    if (dir.empty()) {
        return false;
    }
    const std::filesystem::path p(dir);
    return p.parent_path() == p;
}

/// THE LEXICAL PARENT OF `dir`, or EMPTY when `dir` is its own parent.
///
/// LEXICAL, AND NEVER CANONICAL. Entering a linked directory and going back up returns a
/// maker to where they came from rather than to wherever the link pointed -- measured on
/// both families, and the reason `canonical`/`weakly_canonical` are not called anywhere in
/// this application's navigation: correcting a path to look tidy relocates the maker to a
/// place they never walked to and makes `parent` mean something they did not do.
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
    /// Has this run generated its origin and read its durable marks yet? ONE guard for
    /// both, and it is what makes "generated once for the run" a fact rather than a hope:
    /// origin is the location navigation BEGAN from, so it is fixed the first time anything
    /// asks, and nothing afterwards can move it.
    bool settled = false;

    /// WHERE THIS RUN'S NAVIGATION BEGAN, or empty when it had nowhere to begin.
    ///
    /// EMPTY IS THE DESIGNED ABSENCE and is never invented: a run whose launch location
    /// this build could not carry has no origin, and Files says so rather than substituting
    /// a neighbouring directory. A maker can still reach a durable mark or a filesystem
    /// root from there, which is the honest way back onto the machine.
    std::string origin;

    /// THE DURABLE HALF: places somebody deliberately asked to keep. Sorted and unique by
    /// their normalized absolute spelling, so the traversal order is the same on every run
    /// and the written file's bytes are deterministic.
    std::vector<std::string> maker;

    /// DOES THIS RUN KNOW ANYWHERE AT ALL, without asking a platform?
    ///
    /// The in-memory half of "is there anything to do here" -- the generated origin and the
    /// maker's own places. It deliberately does NOT consult the host's filesystem roots:
    /// this is asked at every paint and every keystroke, and asking an operating system
    /// which drives exist at that rate is the per-paint population `files.hpp` forbids.
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
    ///
    /// The root arm is the lexical fixed point rather than a host root list, deliberately:
    /// a presentation runs at every paint and a drive enumeration is somebody else's
    /// population, so what a header may state is what a path can say about itself.
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
    ///
    /// Origin first because it is where the maker started; then their own marks, in the one
    /// order that is the same on every machine (the browser's own bytewise rule); then the
    /// roots the host reported for THIS gesture, which the caller passes in because asking
    /// the platform is a gesture-time act and this function is pure.
    ///
    /// ONE ADDRESS IS ONE STOP. A directory that is the origin AND a maker mark AND a
    /// filesystem root appears once, wearing all three provenances -- traversal is about
    /// where a maker lands, and landing on the same place twice in a cycle is a cycle with
    /// a stutter in it.
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
