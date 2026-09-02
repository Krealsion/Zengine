// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_CONTEXT_HPP
#define ZENGINE_WORKSHOP_CONTEXT_HPP

// WHAT CAN I DO WITH THIS? -- the contextual-action surface.
// Workshop law: agents/workshop/contextual.md (+1 registers; agents/workshop.md routes)

#include "keymap.hpp"
#include "setup.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// THE SUBJECT KINDS a contextual request can truthfully name.
// WL-CTX-01 -- agents/workshop/contextual.md; WL-TAB-12 -- agents/workshop/tab-run.md
namespace context_subject {
inline constexpr std::int64_t kRoot = 0;   ///< the empty room / Workshop itself
inline constexpr std::int64_t kPane = 1;   ///< an arrangeable pane, by durable `PaneRef`
inline constexpr std::int64_t kObject = 2; ///< a document object, by minted identity
inline constexpr std::int64_t kLayout = 3; ///< a painted layout tab, by maker position
} // namespace context_subject

/// One subject kind as a declaration bit, so a row can be meaningful for several kinds
/// without a second row.
inline constexpr std::int64_t context_bit(std::int64_t subject) noexcept {
    return std::int64_t{1} << subject;
}

inline constexpr std::int64_t kOnRoot = context_bit(context_subject::kRoot);
inline constexpr std::int64_t kOnPane = context_bit(context_subject::kPane);
inline constexpr std::int64_t kOnObject = context_bit(context_subject::kObject);
inline constexpr std::int64_t kOnLayout = context_bit(context_subject::kLayout);

/// THE SURFACE'S OWN STATE -- a mode in the picker's family: open, a captured subject,
/// which group level is showing, and a cursor.
// WL-CTX-01, WL-CTX-03 -- agents/workshop/contextual.md
struct ContextMenu {
    bool open = false;
    std::int64_t subject = context_subject::kRoot;
    PaneRef pane;
    std::int64_t object = 0;
    std::size_t layout = 0; ///< read exactly when `subject == kLayout`
    /// The open presentation group, "" for the top level. A group is its name -- it has
    /// no identity, no gesture and no dispatch arm, and an empty one is never shown.
    std::string group;
    std::size_t cursor = 0;
    bool anchored = false;   ///< a pointer opened this, at the cell below
    std::int64_t anchor_x = 0; ///< the opening press's canvas cell
    std::int64_t anchor_y = 0;
};

/// ONE DECLARATION: an action id, the subject kinds it is meaningful for, and the
/// presentation group it is offered under ("" = the menu's own top level). A REFERENCE,
/// never an identity.
struct ContextRow {
    const char* action = "";
    std::int64_t subjects = 0;
    const char* group = "";
};

/// THE DECLARED FIRST POPULATIONS.
// WL-CTX-05 -- agents/workshop/contextual.md
inline constexpr ContextRow kContextCatalog[] = {
    // -- the empty room: Workshop's own zero-target doors ------------------------------
    {"object.new", kOnRoot, ""},
    {"workshop.picker", kOnRoot, ""},
    {"workshop.manage", kOnRoot, ""},
    {"workshop.terminal", kOnRoot, ""},
    {"workshop.attention", kOnRoot, ""},
    {"workshop.hotkeys", kOnRoot, ""},
    {"document.save", kOnRoot, ""},
    {"document.open", kOnRoot, ""},
    {"setup.name", kOnRoot, ""},
    {"setup.restore", kOnRoot, ""},
    // Reset order is a fact about the WHOLE setup, not about one pane -- which is why it
    // is a room action here and not a pane one.
    {"manage.reset-order", kOnRoot, ""},
    // -- a pane: the arrangement vocabulary, on the pointed pane -----------------------
    //
    // ARRANGE IS ONE ROW BECAUSE IT IS ONE INTENT: moving and resizing the
    // pointed pane are one interaction state, entered here on the captured reference.
    // The ordering verbs live under `Order` -- the group's old name was `Arrange`,
    // which this row's arrival made a lie one indentation deep.
    {"manage.arrange", kOnPane, ""},
    {"manage.front", kOnPane, "Order"},
    {"manage.back", kOnPane, "Order"},
    {"manage.raise", kOnPane, "Order"},
    {"manage.lower", kOnPane, "Order"},
    {"manage.reset-place", kOnPane, "Reset"},
    {"manage.reset-width", kOnPane, "Reset"},
    {"manage.reset-height", kOnPane, "Reset"},
    {"manage.remove", kOnPane, ""},
    // -- a document object -------------------------------------------------------------
    {"object.delete", kOnObject, ""},
    // -- a layout tab ------------------------------------------------------------------
    //
    // THE FIVE OPERATIONS A MAKER CAN DO TO A TAB, on the tab they pointed at. Rename is
    // first because it is the one a double-click already performs, so the menu names the
    // gesture's slower twin at the top; the two reorder steps are a group for the reason
    // `Order` is one -- they are the same intent twice, and a maker reads them together.
    //
    // ⚠ THEY ACT ON THE CAPTURED POSITION AND NOT ON THE LIVE LAYOUT. `manage.remove`'s
    // pane rows established the shape: the subject is what the press named, and the owner
    // re-asks the run about it at spend. Closing an inactive tab therefore leaves the live
    // desk exactly where it was, which is the whole difference between this and `^w`.
    {"layout.rename", kOnLayout, ""},
    {"layout.duplicate", kOnLayout, ""},
    {"layout.move-left", kOnLayout, "Order"},
    {"layout.move-right", kOnLayout, "Order"},
    {"layout.remove", kOnLayout, ""},
};

inline constexpr std::size_t kContextCatalogCount =
    sizeof(kContextCatalog) / sizeof(kContextCatalog[0]);

// ---- The drift guard: every reference resolves, at compile time --------------------------
// WL-CTX-05 -- agents/workshop/contextual.md

inline constexpr bool context_same_id(const char* a, const char* b) noexcept {
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

inline constexpr bool context_actions_resolve() noexcept {
    for (const ContextRow& row : kContextCatalog) {
        bool found = false;
        for (const ActionRow& declared : kActionCatalog) {
            if (context_same_id(row.action, declared.id)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

static_assert(context_actions_resolve(),
              "every contextual declaration references a kActionCatalog id");

inline constexpr bool context_rows_distinct() noexcept {
    for (std::size_t i = 0; i < kContextCatalogCount; ++i) {
        for (std::size_t j = i + 1; j < kContextCatalogCount; ++j) {
            if (context_same_id(kContextCatalog[i].action, kContextCatalog[j].action) &&
                (kContextCatalog[i].subjects & kContextCatalog[j].subjects) != 0) {
                return false;
            }
        }
    }
    return true;
}

static_assert(context_rows_distinct(),
              "one action appears once per subject kind -- a menu keyed by id must not "
              "show a row twice");

/// ONE RENDERED ENTRY of the surface at one level: a group to descend into, or an action
/// to request. An action entry carries its resolved declaration row (for the label); a
/// group entry carries only its name -- folders are not actions.
struct ContextEntry {
    bool is_group = false;
    const char* group = "";
    const ActionRow* row = nullptr;
};

/// THE POPULATION AT ONE LEVEL.
// WL-CTX-05 -- agents/workshop/contextual.md
inline std::vector<ContextEntry> context_population(std::int64_t subject,
                                                    std::string_view open_group) {
    const std::int64_t bit = context_bit(subject);
    std::vector<ContextEntry> out;
    for (const ContextRow& row : kContextCatalog) {
        if ((row.subjects & bit) == 0) {
            continue;
        }
        const bool grouped = row.group[0] != '\0';
        if (open_group.empty()) {
            if (!grouped) {
                out.push_back(ContextEntry{false, row.group, row_of_id(row.action)});
                continue;
            }
            bool seen = false;
            for (const ContextEntry& already : out) {
                if (already.is_group && context_same_id(already.group, row.group)) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                out.push_back(ContextEntry{true, row.group, nullptr});
            }
            continue;
        }
        if (grouped && open_group == row.group) {
            out.push_back(ContextEntry{false, row.group, row_of_id(row.action)});
        }
    }
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_CONTEXT_HPP
