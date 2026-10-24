// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_LIVE_DESK_HPP
#define ZENGINE_WORKSHOP_LIVE_DESK_HPP

// The live layout as Workshop presents it: the copy the layouts' owner last sent, every layout as
// a tab, and the desk a gesture in flight proposes until the owner answers it. Workshop reads it
// for every geometry, hit test, picture and desk read, and writes no layout: each `LiveLayout`
// replaces it whole.
// Workshop law: agents/workshop/layouts.md

#include "component/text_box.hpp"
#include "setup.hpp"
#include "workshop/layouts/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

/// THE ONE-LINE LAYOUT-NAME EDITOR: open or not, which layout it is naming, and the line being
/// typed. Workshop's input: a committed name is asked of the owner.
// WL-LAYOUT-10 -- agents/workshop/layouts.md; WL-TEXT-01 -- agents/workshop/text-box.md
struct LayoutNaming {
    bool open = false;
    std::size_t at = 0;
    component::TextBox line;
};

/// THE LIVE LAYOUT AS THE OWNER LAST SENT IT, and the desk presented from it. `active` is what
/// every reader reads: the owner's desk, or, while a gesture's edit is in flight, the desk that
/// edit proposes; `owned` is the owner's own, never written here.
// WL-LAYOUT-01 -- agents/workshop/layouts.md
struct LiveDesk {
    Setup active = default_setup();
    Setup owned = default_setup();
    std::int64_t id = 0;            ///< the live layout's id; 0 until the owner's first copy
    std::size_t active_at = 0;      ///< where it stands in the weaver's order
    std::vector<LiveTab> tabs{LiveTab{0, kDefaultSetupName, setup_link::kNone}};
    std::string link_path;          ///< the live layout's setup file, or empty
    std::int64_t link = setup_link::kNone;
    /// How many times the owner put another desk live; `InspectedPane::desk` reads it.
    // WL-INFO-14 -- agents/workshop/info-body.md
    std::uint64_t put_live = 0;
    std::int64_t revision = 0;      ///< the `LiveLayout` it is
    std::vector<RowRevision> revisions;
    LayoutNaming naming;
};

/// How many layouts the owner holds, the live one included: never zero.
inline std::size_t layout_count(const LiveDesk& d) noexcept {
    return d.tabs.empty() ? 1 : d.tabs.size();
}

/// The name of the layout at position `at`; out of range answers the live one's.
inline const std::string& layout_name_at(const LiveDesk& d, std::size_t at) noexcept {
    if (at < d.tabs.size()) {
        return d.tabs[at].name;
    }
    return d.active.name;
}

/// The id of the layout at position `at`; out of range, and before the owner's first copy, 0, the
/// live one.
inline std::int64_t layout_id_at(const LiveDesk& d, std::size_t at) noexcept {
    return at < d.tabs.size() ? d.tabs[at].id : 0;
}

/// One row's window as an edit of the live layout: its place and size, whole.
inline LayoutEdit window_edit(const SetupPane& row) {
    LayoutEdit e;
    e.op = layout_op::kWindow;
    e.provider = row.ref.provider;
    e.pane = row.ref.pane;
    e.place = setup_persist::place_out(row.place);
    e.width = setup_persist::size_out(row.width);
    e.height = setup_persist::size_out(row.height);
    return e;
}

/// The settings revision the owner minted for the live row of `ref`, or 0 for none.
inline std::int64_t row_revision(const LiveDesk& d, const PaneRef& ref) noexcept {
    for (const RowRevision& r : d.revisions) {
        if (r.provider == ref.provider && r.pane == ref.pane) {
            return r.revision;
        }
    }
    return 0;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_LIVE_DESK_HPP
