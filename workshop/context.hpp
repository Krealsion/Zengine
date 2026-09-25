// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_CONTEXT_HPP
#define ZENGINE_WORKSHOP_CONTEXT_HPP

// WHAT CAN I DO WITH THIS? -- the contextual-action surface.
// Workshop law: agents/workshop/contextual.md (+1 registers; agents/workshop.md routes)

#include "keymap.hpp"
#include "pane_vocabulary.hpp"
#include "presenter_vocabulary.hpp"
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
// 2 is not reused: a subject kind is a bit in every row's declaration.
inline constexpr std::int64_t kLayout = 3; ///< a painted layout tab, by maker position
} // namespace context_subject

/// One subject kind as a declaration bit, so a row can be meaningful for several kinds
/// without a second row.
inline constexpr std::int64_t context_bit(std::int64_t subject) noexcept {
    return std::int64_t{1} << subject;
}

inline constexpr std::int64_t kOnRoot = context_bit(context_subject::kRoot);
inline constexpr std::int64_t kOnPane = context_bit(context_subject::kPane);
inline constexpr std::int64_t kOnLayout = context_bit(context_subject::kLayout);

/// The surface's own state: open, a captured subject, which group level is showing, and a cursor.
// WL-CTX-01, WL-CTX-03 -- agents/workshop/contextual.md
struct ContextMenu {
    bool open = false;
    std::int64_t subject = context_subject::kRoot;
    PaneRef pane;
    std::size_t layout = 0; ///< read exactly when `subject == kLayout`
    /// The open presentation group, "" for the top level. A group is its name -- it has
    /// no identity, no gesture and no dispatch arm, and an empty one is never shown.
    std::string group;
    std::size_t cursor = 0;
    bool anchored = false;   ///< a pointer opened this, at the cell below
    std::int64_t anchor_x = 0; ///< the opening press's canvas cell
    std::int64_t anchor_y = 0;
};

/// A pane's own menu, presented by the presenter participant on a popup this host granted
/// (`workshop/presenter_vocabulary.hpp`). The host keeps what custody and display need, and
/// nothing of the offer's meaning: no rows, no ids, no cursor.
// WL-CTX-09 -- agents/workshop/pane-menu.md
struct PresentedMenu {
    bool open = false;
    std::int64_t menu = 0;         ///< the grant's number, this host's
    std::string office;            ///< the requester: the office Loom authenticated on the ask
    std::string pane;
    std::string subject;           ///< the ask's subject, for an answer the host gives itself
    std::uint64_t correlation = 0; ///< the ask's number, which every answer echoes
    bool anchored = false;         ///< beside a place in the pane, at the cell below
    std::int64_t anchor_x = 0;
    std::int64_t anchor_y = 0;
    std::int64_t room_rows = 0;    ///< the room granted with it
    std::int64_t room_columns = 0;
    std::vector<surface::SurfaceTextRow> lines; ///< as the presenter last showed them
    std::int64_t picture = 0;      ///< the presenter's number for those lines, as admitted
    PictureStamp stamp;            ///< ...and the one a press names (the host's fence)
    std::uint64_t first_input = 0; ///< the maker's act count when it was granted
    std::uint64_t last_input = 0;  ///< the newest act forwarded to it
    /// THE GRANT'S QUEUED ATTEMPT (Loom's `Ticket::seq`). Every sentence this host queues to the
    /// presenter's office from the grant until the menu ends is about this menu -- one menu at a
    /// time -- and Loom numbers attempts in the order they are queued, so a refusal naming an
    /// attempt at or after this one is about this menu, and one before it about an older one.
    std::uint64_t first_attempt = 0;
};

/// A menu the host took off the screen whose requester may still be owed an answer: kept until
/// Loom has had its say about the menu's sentences, and answered by this host, unchosen, once, if
/// one of them was refused. Forgotten when its fence (`WithdrawalFence`) has come round twice.
// WL-CTX-10 -- agents/workshop/pane-menu.md
struct WithdrawnMenu {
    std::int64_t menu = 0;           ///< the grant's number
    std::string office;              ///< the requester, as Loom authenticated it on the ask
    std::string pane;
    std::string subject;
    std::uint64_t correlation = 0;   ///< the ask's number, which the answer echoes
    std::string why;                 ///< the host's words for ending it
    std::uint64_t first_attempt = 0; ///< its grant's queued attempt...
    std::uint64_t last_attempt = 0;  ///< ...through its withdrawal's: every sentence about it
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
    {"workshop.manage", kOnRoot, ""},
    {"setup.name", kOnRoot, ""},
    {"setup.restore", kOnRoot, ""},
    // Reset order is a fact about the WHOLE setup, not about one pane -- which is why it
    // is a room action here and not a pane one.
    {"manage.reset-order", kOnRoot, ""},
    // -- a pane: the arrangement vocabulary, on the pointed pane -----------------------
    // Arrange is one row because it is one intent; the ordering verbs live under `Order`.
    {"manage.arrange", kOnPane, ""},
    {"manage.front", kOnPane, "Order"},
    {"manage.back", kOnPane, "Order"},
    {"manage.raise", kOnPane, "Order"},
    {"manage.lower", kOnPane, "Order"},
    {"manage.reset-place", kOnPane, "Reset"},
    {"manage.reset-width", kOnPane, "Reset"},
    {"manage.reset-height", kOnPane, "Reset"},
    // Edit Code: the pointed pane's running code, followed to its source. After the arrangement
    // rows, a different intent, and before `remove`, which stays last. A row, not a group: what it
    // reaches is the host's to say at spend (WL-CTX-07).
    {"pane.edit-code", kOnPane, ""},
    {"manage.remove", kOnPane, ""},
    // -- a layout tab ------------------------------------------------------------------
    // Rename first, a double-click's slower twin; the two reorder steps are a group, one intent
    // twice. They act on the captured position, not the live layout: closing an inactive tab
    // leaves the live desk where it was, unlike `^w`.
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
                                                    std::string_view open_group);

/// THE POPULATION OF THE SURFACE AS IT IS OPEN -- the host's catalog, at the open level.
inline std::vector<ContextEntry> context_population(const ContextMenu& menu) {
    return context_population(menu.subject, menu.group);
}

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
