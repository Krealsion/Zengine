// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LAYOUTS_RUN_HPP
#define ZENGINE_WORKSHOP_LAYOUTS_RUN_HPP

// The layouts a run holds: the run of desks and which one is live, the id the owner minted for
// each, and each desk's link to a setup file.
// Workshop law: agents/workshop/layouts.md (+3 registers; agents/workshop.md routes)

#include "workshop/setup.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

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

/// WHICH OF THE THREE THIS LAYOUT IS. Pure, total, and the ONE place the question
/// is decided.
// WL-LAYOUT-02 -- agents/workshop/layouts.md
inline std::int64_t link_status(const Setup& desk, const SetupLink& link) noexcept {
    if (link.path.empty()) {
        return setup_link::kNone;
    }
    return desk == link.known ? setup_link::kCurrent : setup_link::kModified;
}

/// ONE LAYOUT AS THE SHELF AND THE RUN HOLD IT: the desk, the artifact it is associated with, and
/// the id the owner minted for it (0 for one no run holds yet). Two are equal when they hold the
/// same desk and link: the id says which layout of the run one is, not what it holds.
// WL-LAYOUT-01 -- agents/workshop/layouts.md; WL-SESSION-05 -- agents/workshop/session-restore.md
struct Layout {
    Setup desk;
    SetupLink link;
    std::int64_t id = 0;

    friend bool operator==(const Layout& a, const Layout& b) {
        return a.desk == b.desk && a.link == b.link;
    }
    friend bool operator!=(const Layout& a, const Layout& b) { return !(a == b); }
};

/// WHERE A PANE TAKEN OFF A LAYOUT WAS LEFT: its row's place and size, which a seat in that layout
/// gives back.
// WL-DESK-19 -- agents/workshop/desktop.md
struct LeftPlace {
    PaneRef ref;
    PanePlace place;
    PaneSize width;
    PaneSize height;
};

/// THE LAYOUTS ONE RUN HOLDS, WHICH ONE IS LIVE, AND THE ID THE NEXT NEW ONE TAKES.
// WL-LAYOUT-01, WL-LAYOUT-03 -- agents/workshop/layouts.md
struct SetupState {
    Setup active = default_setup();
    SetupLink active_link;
    std::int64_t active_id = 1;
    std::vector<Layout> shelved;
    std::size_t active_at = 0;
    /// The id the next new or copied layout takes: above every id the run holds, and never one a
    /// layout of this run held before.
    // WL-SESSION-05 -- agents/workshop/session-restore.md
    std::int64_t next_id = 2;
    /// How many times another desk has been put live (a switch, a restored run, a new or removed
    /// live layout, a desk restored from its file); never persisted. How a reader that named "this
    /// pane on this desk" learns the desk is another one (`InspectedPane::desk`).
    // WL-INFO-14 -- agents/workshop/info-body.md
    std::uint64_t put_live = 0;
    /// Where each pane taken off a layout was left, by the layout's id, the oldest first: at most
    /// `kMaxSetupPanes` a layout, held for the run and never persisted.
    // WL-DESK-19 -- agents/workshop/desktop.md
    std::map<std::int64_t, std::vector<LeftPlace>> left;
};

/// THE LIVE LAYOUT LEAVES `row`: where it stood is kept, replacing what was kept for its pane, and
/// past the bound the oldest goes.
// WL-DESK-19 -- agents/workshop/desktop.md
inline void leave_place(SetupState& s, const SetupPane& row) {
    std::vector<LeftPlace>& kept = s.left[s.active_id];
    std::erase_if(kept, [&row](const LeftPlace& l) { return l.ref == row.ref; });
    if (kept.size() >= kMaxSetupPanes) {
        kept.erase(kept.begin());
    }
    kept.push_back(LeftPlace{row.ref, row.place, row.width, row.height});
}

/// WHAT A LAYOUT NO LONGER IN THE RUN LEFT goes with it, so the run keeps places for its own alone.
// WL-DESK-19 -- agents/workshop/desktop.md
inline void forget_gone_places(SetupState& s) {
    std::erase_if(s.left, [&s](const auto& kept) {
        if (kept.first == s.active_id) {
            return false;
        }
        for (const Layout& l : s.shelved) {
            if (l.id == kept.first) {
                return false;
            }
        }
        return true;
    });
}

/// ...AND A SEAT IN THE LIVE LAYOUT TAKES IT BACK: where `ref` was left there, or nothing.
// WL-DESK-19 -- agents/workshop/desktop.md
inline std::optional<LeftPlace> take_left_place(SetupState& s, const PaneRef& ref) {
    const auto found = s.left.find(s.active_id);
    if (found == s.left.end()) {
        return std::nullopt;
    }
    std::vector<LeftPlace>& kept = found->second;
    for (auto it = kept.begin(); it != kept.end(); ++it) {
        if (it->ref == ref) {
            const LeftPlace out = *it;
            kept.erase(it);
            return out;
        }
    }
    return std::nullopt;
}

/// How many layouts this run holds, the active one included: never zero, as `active` is a value.
inline std::size_t layout_count(const SetupState& s) noexcept { return s.shelved.size() + 1; }

/// Where the live layout stands, held to the run.
inline std::size_t live_at(const SetupState& s) noexcept {
    return s.active_at < layout_count(s) ? s.active_at : s.shelved.size();
}

/// The live layout's desk.
inline const Setup& live_desk(const SetupState& s) noexcept { return s.active; }
inline Setup& live_desk(SetupState& s) noexcept { return s.active; }

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

/// ...AND THE SAME DESK TO WRITE: what a layout keeps for itself is written where it is kept.
inline Setup& layout_at(SetupState& s, std::size_t at) noexcept {
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

/// ...AND ITS ID, by the same rule.
inline std::int64_t id_at(const SetupState& s, std::size_t at) noexcept {
    if (at == s.active_at || at >= layout_count(s)) {
        return s.active_id;
    }
    return s.shelved[shelf_index(s, at)].id;
}

/// WHERE THE LAYOUT WITH ID `id` STANDS -- 0 is the live one -- or nothing when no layout of
/// the run holds it: a layout gone is never another one.
// WL-SESSION-05 -- agents/workshop/session-restore.md
inline std::optional<std::size_t> position_of(const SetupState& s, std::int64_t id) noexcept {
    if (id == 0 || id == s.active_id) {
        return s.active_at;
    }
    for (std::size_t at = 0; at < layout_count(s); ++at) {
        if (at != s.active_at && s.shelved[shelf_index(s, at)].id == id) {
            return at;
        }
    }
    return std::nullopt;
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
                     Layout{std::move(s.active), std::move(s.active_link), s.active_id});
    s.active = std::move(s.shelved[to].desk);
    s.active_link = std::move(s.shelved[to].link);
    s.active_id = s.shelved[to].id;
    s.shelved.erase(s.shelved.begin() + static_cast<std::ptrdiff_t>(to));
    s.active_at = to;
    ++s.put_live;
    return true;
}

/// THE WEAVER'S ORDERED RUN, WITH THE LIVE ONE PUT BACK WHERE IT SITS, each with its id.
// WL-LAYOUT-04, WL-LAYOUT-12 -- agents/workshop/layouts.md
inline std::vector<Layout> layout_run(const SetupState& s) {
    std::vector<Layout> run = s.shelved;
    // Total over `active_at`, as `layout_at` is.
    const std::size_t at = s.active_at <= s.shelved.size() ? s.active_at : s.shelved.size();
    run.insert(run.begin() + static_cast<std::ptrdiff_t>(at),
               Layout{s.active, s.active_link, s.active_id});
    return run;
}

/// TAKE AN ORDERED RUN AND LIFT ONE OF IT LIVE -- the direction a restored session travels, and
/// the only other place the run's spelling is made. Its ids are kept when each is positive and
/// none is held twice, else each layout is given one in run order; `next_id` is then at least
/// one above every id, never lower than it was.
// WL-LAYOUT-04, WL-LAYOUT-12 -- agents/workshop/layouts.md
inline bool install_layout_run(SetupState& s, std::vector<Layout> run, std::size_t active) {
    if (run.empty() || active >= run.size()) {
        return false;
    }
    bool kept = true;
    for (std::size_t i = 0; i < run.size() && kept; ++i) {
        kept = run[i].id > 0;
        for (std::size_t j = 0; j < i && kept; ++j) {
            kept = run[j].id != run[i].id;
        }
    }
    std::int64_t top = 0;
    for (std::size_t i = 0; i < run.size(); ++i) {
        if (!kept) {
            run[i].id = static_cast<std::int64_t>(i) + 1;
        }
        top = run[i].id > top ? run[i].id : top;
    }
    s.next_id = top + 1 > s.next_id ? top + 1 : s.next_id;
    s.active = std::move(run[active].desk);
    s.active_link = std::move(run[active].link);
    s.active_id = run[active].id;
    run.erase(run.begin() + static_cast<std::ptrdiff_t>(active));
    s.shelved = std::move(run);
    s.active_at = active;
    ++s.put_live;
    return true;
}

/// ONE MORE LAYOUT: A FRESH BLANK DESK WITH A NEW ID, APPENDED, AND LIVE.
// WL-LAYOUT-03 -- agents/workshop/layouts.md
inline bool add_layout(SetupState& s, std::size_t ceiling = kMaxLayouts) {
    if (layout_count(s) >= ceiling) {
        return false;
    }
    s.shelved.insert(s.shelved.begin() + static_cast<std::ptrdiff_t>(s.active_at),
                     Layout{std::move(s.active), std::move(s.active_link), s.active_id});
    s.active = default_setup();
    s.active_link = SetupLink{};
    s.active_id = s.next_id++;
    s.active_at = s.shelved.size();
    ++s.put_live;
    return true;
}

/// COPY THE LAYOUT AT `at` UNDER A NEW ID, PUT THE COPY DIRECTLY AFTER IT, AND STAND ON THE COPY.
// WL-LAYOUT-04 -- agents/workshop/layouts.md
inline bool duplicate_layout(SetupState& s, std::size_t at, std::size_t ceiling = kMaxLayouts) {
    if (at >= layout_count(s) || layout_count(s) >= ceiling) {
        return false;
    }
    std::vector<Layout> run = layout_run(s);
    const std::size_t copy_at = at + 1;
    run.insert(run.begin() + static_cast<std::ptrdiff_t>(copy_at),
               Layout{run[at].desk, SetupLink{}, s.next_id++});
    return install_layout_run(s, std::move(run), copy_at);
}

/// DISCARD THE LAYOUT AT `at`. Its id is never minted again.
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
    s.active_id = s.shelved[take].id;
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

#endif // ZENGINE_WORKSHOP_LAYOUTS_RUN_HPP
