// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LAYOUT_SHELF_HPP
#define ZENGINE_WORKSHOP_LAYOUT_SHELF_HPP

// The layouts a session holds: the run of desks and which one is live, each desk's link to a
// setup file, and the editor over a layout's name. A part of `workshop/setup.hpp`, which
// includes it after the setup value; nothing else includes it.
// Workshop law: agents/workshop/layouts.md (+3 registers; agents/workshop.md routes)

#include "component/text_box.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

// ---- The session's side of it -------------------------------------------------

/// THE ONE-LINE LAYOUT-NAME EDITOR: open or not, which layout it is naming, and
/// the line being typed.
// WL-LAYOUT-10 -- agents/workshop/layouts.md; WL-TEXT-01 -- agents/workshop/text-box.md
struct LayoutNaming {
    bool open = false;
    std::size_t at = 0;
    component::TextBox line;
};

/// A LAYOUT'S OPTIONAL RELATIONSHIP TO ONE STANDALONE SETUP ARTIFACT.
// WL-LAYOUT-01, WL-LAYOUT-02, WL-LAYOUT-11 -- agents/workshop/layouts.md
struct SetupLink {
    std::string path;
    Setup known;

    friend bool operator==(const SetupLink& a, const SetupLink& b) {
        return a.path == b.path && a.known == b.known;
    }
    friend bool operator!=(const SetupLink& a, const SetupLink& b) { return !(a == b); }
};

/// What the active layout's status says: derived at every composition, stored nowhere.
namespace setup_link {
inline constexpr std::int64_t kNone = 0;     ///< no artifact is associated
inline constexpr std::int64_t kCurrent = 1;  ///< the desk equals the last known value
inline constexpr std::int64_t kModified = 2; ///< it is associated and differs from it
} // namespace setup_link

/// WHICH OF THE THREE THIS LAYOUT IS. Pure, total, and the ONE place the question
/// is decided.
// WL-LAYOUT-02 -- agents/workshop/layouts.md
inline std::int64_t link_status(const Setup& desk, const SetupLink& link) noexcept {
    if (link.path.empty()) {
        return setup_link::kNone;
    }
    return desk == link.known ? setup_link::kCurrent : setup_link::kModified;
}

/// ONE LAYOUT AS THE SHELF AND THE RUN HOLD IT: the desk, and the artifact it is
/// associated with.
// WL-LAYOUT-01 -- agents/workshop/layouts.md
struct Layout {
    Setup desk;
    SetupLink link;

    friend bool operator==(const Layout& a, const Layout& b) {
        return a.desk == b.desk && a.link == b.link;
    }
    friend bool operator!=(const Layout& a, const Layout& b) { return !(a == b); }
};

/// THE LAYOUTS THIS WORKSHOP IS HOLDING, WHICH ONE IS LIVE, AND THE EDITOR OVER
/// ITS NAME.
// WL-LAYOUT-01, WL-LAYOUT-03 -- agents/workshop/layouts.md
struct SetupState {
    Setup active = default_setup();
    SetupLink active_link;
    LayoutNaming naming;
    std::vector<Layout> shelved;
    std::size_t active_at = 0;
    /// How many times another desk has been put live (a switch, a restored run, a new or removed
    /// live layout, a desk restored from its file); never persisted. How a reader that named "this
    /// pane on this desk" learns the desk is another one (`InspectedPane::desk`).
    // WL-INFO-14 -- agents/workshop/info-body.md
    std::uint64_t put_live = 0;
};

/// How many layouts this Workshop holds, the active one included: never zero, as `active` is a
/// value.
inline std::size_t layout_count(const SetupState& s) noexcept { return s.shelved.size() + 1; }

/// Where position `at` sits on the shelf, for a position that is not the live one: the one place a
/// run index becomes a shelf index.
inline std::size_t shelf_index(const SetupState& s, std::size_t at) noexcept {
    return at < s.active_at ? at : at - 1;
}

/// The desk at position `at` in the weaver's order. Out of range answers the active layout: every
/// caller holds a position from this same run.
inline const Setup& layout_at(const SetupState& s, std::size_t at) noexcept {
    if (at == s.active_at || at >= layout_count(s)) {
        return s.active;
    }
    return s.shelved[shelf_index(s, at)].desk;
}

/// ...AND ITS SETUP ASSOCIATION, by the same rule and for the same reason.
inline const SetupLink& link_at(const SetupState& s, std::size_t at) noexcept {
    if (at == s.active_at || at >= layout_count(s)) {
        return s.active_link;
    }
    return s.shelved[shelf_index(s, at)].link;
}

/// MAKE THE LAYOUT AT POSITION `to` THE LIVE ONE -- the whole of a switch's value
/// half, and the only thing in this application that changes which layout is
/// active.
// WL-LAYOUT-03, WL-LAYOUT-05 -- agents/workshop/layouts.md
inline bool activate_layout(SetupState& s, std::size_t to) {
    if (to >= layout_count(s) || to == s.active_at) {
        return false;
    }
    s.shelved.insert(s.shelved.begin() + static_cast<std::ptrdiff_t>(s.active_at),
                     Layout{std::move(s.active), std::move(s.active_link)});
    s.active = std::move(s.shelved[to].desk);
    s.active_link = std::move(s.shelved[to].link);
    s.shelved.erase(s.shelved.begin() + static_cast<std::ptrdiff_t>(to));
    s.active_at = to;
    ++s.put_live;
    return true;
}

/// THE WEAVER'S ORDERED RUN, WITH THE LIVE ONE PUT BACK WHERE IT SITS.
// WL-LAYOUT-04, WL-LAYOUT-12 -- agents/workshop/layouts.md
inline std::vector<Layout> layout_run(const SetupState& s) {
    std::vector<Layout> run = s.shelved;
    // Total over `active_at`, as `layout_at` is.
    const std::size_t at = s.active_at <= s.shelved.size() ? s.active_at : s.shelved.size();
    run.insert(run.begin() + static_cast<std::ptrdiff_t>(at), Layout{s.active, s.active_link});
    return run;
}

/// TAKE AN ORDERED RUN AND LIFT ONE OF IT LIVE -- the direction a
/// restored session travels, and the only other place the run's spelling is made.
// WL-LAYOUT-04, WL-LAYOUT-12 -- agents/workshop/layouts.md
inline bool install_layout_run(SetupState& s, std::vector<Layout> run, std::size_t active) {
    if (run.empty() || active >= run.size()) {
        return false;
    }
    s.active = std::move(run[active].desk);
    s.active_link = std::move(run[active].link);
    run.erase(run.begin() + static_cast<std::ptrdiff_t>(active));
    s.shelved = std::move(run);
    s.active_at = active;
    ++s.put_live;
    return true;
}

/// ONE MORE LAYOUT: A FRESH BLANK DESK, APPENDED, AND LIVE.
// WL-LAYOUT-03 -- agents/workshop/layouts.md
inline bool add_layout(SetupState& s, std::size_t ceiling = kMaxLayouts) {
    if (layout_count(s) >= ceiling) {
        return false;
    }
    s.shelved.insert(s.shelved.begin() + static_cast<std::ptrdiff_t>(s.active_at),
                     Layout{std::move(s.active), std::move(s.active_link)});
    s.active = default_setup();
    s.active_link = SetupLink{};
    s.active_at = s.shelved.size();
    ++s.put_live;
    return true;
}

/// COPY THE LAYOUT AT `at`, PUT THE COPY DIRECTLY AFTER IT, AND STAND ON THE COPY.
// WL-LAYOUT-04 -- agents/workshop/layouts.md
inline bool duplicate_layout(SetupState& s, std::size_t at, std::size_t ceiling = kMaxLayouts) {
    if (at >= layout_count(s) || layout_count(s) >= ceiling) {
        return false;
    }
    std::vector<Layout> run = layout_run(s);
    const std::size_t copy_at = at + 1;
    run.insert(run.begin() + static_cast<std::ptrdiff_t>(copy_at),
               Layout{run[at].desk, SetupLink{}});
    return install_layout_run(s, std::move(run), copy_at);
}

/// DISCARD THE LAYOUT AT `at`.
// WL-LAYOUT-03 -- agents/workshop/layouts.md
inline bool remove_layout(SetupState& s, std::size_t at) {
    if (s.shelved.empty() || at >= layout_count(s)) {
        return false;
    }
    if (at != s.active_at) {
        s.shelved.erase(s.shelved.begin() + static_cast<std::ptrdiff_t>(shelf_index(s, at)));
        if (at < s.active_at) {
            --s.active_at;
        }
        return true;
    }
    const std::size_t take =
        s.active_at < s.shelved.size() ? s.active_at : s.shelved.size() - 1;
    s.active = std::move(s.shelved[take].desk);
    s.active_link = std::move(s.shelved[take].link);
    s.shelved.erase(s.shelved.begin() + static_cast<std::ptrdiff_t>(take));
    s.active_at = take;
    ++s.put_live;
    return true;
}

/// MOVE THE LAYOUT AT `from` TO POSITION `to` IN THE WEAVER'S ORDER.
// WL-LAYOUT-04 -- agents/workshop/layouts.md; WL-TAB-11 -- agents/workshop/tab-run.md
inline bool move_layout(SetupState& s, std::size_t from, std::size_t to) {
    const std::size_t n = layout_count(s);
    if (from >= n || to >= n || from == to) {
        return false;
    }
    std::vector<Layout> run = layout_run(s);
    Layout moved = std::move(run[from]);
    run.erase(run.begin() + static_cast<std::ptrdiff_t>(from));
    run.insert(run.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    // Where the live layout landed, computed from the erase and the insert: two layouts may be
    // equal, so a search could find the wrong one.
    std::size_t live = s.active_at;
    if (live == from) {
        live = to;
    } else {
        if (live > from) {
            --live;
        }
        if (live >= to) {
            ++live;
        }
    }
    return install_layout_run(s, std::move(run), live);
}

/// GIVE THE LAYOUT AT `at` A NEW NAME -- the whole of a rename's value
/// half, and the only thing in this application that writes a layout's name
/// without writing a file.
// WL-LAYOUT-04, WL-LAYOUT-10 -- agents/workshop/layouts.md
inline bool rename_layout(SetupState& s, std::size_t at, std::string name) {
    if (at >= layout_count(s)) {
        return false;
    }
    if (at == s.active_at) {
        s.active.name = std::move(name);
        return true;
    }
    s.shelved[shelf_index(s, at)].desk.name = std::move(name);
    return true;
}

/// TEACH EVERY LAYOUT ASSOCIATED WITH `path` WHAT THAT ARTIFACT NOW HOLDS.
// WL-LAYOUT-11 -- agents/workshop/layouts.md
inline void adopt_known_setup(SetupState& s, const std::string& path, const Setup& known) {
    if (path.empty()) {
        return;
    }
    if (s.active_link.path == path) {
        s.active_link.known = known;
    }
    for (Layout& shelved : s.shelved) {
        if (shelved.link.path == path) {
            shelved.link.known = known;
        }
    }
}

/// THE POSITION ONE STEP ALONG THE RUN, WRAPPING -- the keyboard's whole
/// traversal law, over the ENTIRE population rather than over whatever the band
/// had room to paint. `by` is +1 or -1.
inline std::size_t layout_step(const SetupState& s, std::int64_t by) noexcept {
    const std::size_t n = layout_count(s);
    const std::size_t at = s.active_at < n ? s.active_at : 0;
    if (n <= 1) {
        return at;
    }
    return by < 0 ? (at == 0 ? n - 1 : at - 1) : (at + 1 == n ? 0 : at + 1);
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_LAYOUT_SHELF_HPP
